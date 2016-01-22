/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

/* User level test procedures */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#include "gni_pub.h"
#include "gni_priv.h"
#include <alps/libalpslli.h>
#include <sys/utsname.h>

#include "hobbes_cmd_queue.h"
#include "xemem.h"
#include "xpmem.h"
#include <hobbes_cmd_queue.h>
#include <xemem.h>
#include <hobbes_enclave.h>
#include <dlfcn.h>		/* For dlsym */
#include "pmi.h"		/* Client should have just include pmi.h but not link libpmi, server might */
#include <assert.h>		/* Client should have just include pmi.h but not link libpmi, server might */
#include "xmem_list.h"
#include "pmi_util.h"

/*  For utilities of PMI etc */
int my_rank, comm_size;

struct utsname uts_info;

/*  This is to implement utility functions
 * allgather gather the requested information from all of the ranks.
 */

int client_fd, pmi_fd;
xemem_segid_t cmdq_segid;
hcq_handle_t hcq = HCQ_INVALID_HANDLE;
struct memseg_list *mt = NULL;


void
allgather (void *in, void *out, int len)
{
  uint32_t retlen;
  void *out_ptr;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  //fprintf(stdout, "client library allgather in pointer  %p, \n", in);
  cmd = hcq_cmd_issue (hcq, PMI_IOC_ALLGATHER, len, in);
  out_ptr = hcq_get_ret_data (hcq, cmd, &retlen);
  memcpy (out, out_ptr, (comm_size * len));
  hcq_cmd_complete (hcq, cmd);
}

/* Define bare minimum PMI calls to forward to other side */

int
pmi_finalize (void)
{
  int retlen;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  cmd = hcq_cmd_issue (hcq, PMI_IOC_FINALIZE, 0, NULL);
  hcq_cmd_complete (hcq, cmd);
  return PMI_SUCCESS;
}

int
pmi_barrier (void)
{
  int retlen;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  cmd = hcq_cmd_issue (hcq, PMI_IOC_BARRIER, 0, NULL);
  hcq_cmd_complete (hcq, cmd);
  return PMI_SUCCESS;
}

/*  End of For utilities of PMI etc */

#define MAXIMUM_CQ_RETRY_COUNT 500

#define PAGE_SHIFT	12
#define PAGE_SIZE (0x1UL << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))


#define NIC_ADDR_BITS    22
#define NIC_ADDR_SHIFT   (32-NIC_ADDR_BITS)
#define NIC_ADDR_MASK    0x3FFFFF
#define CPU_NUM_BITS     7
#define CPU_NUM_SHIFT    (NIC_ADDR_SHIFT-CPU_NUM_BITS)
#define CPU_NUM_MASK     0x7F
#define THREAD_NUM_BITS  3
#define THREAD_NUM_SHIFT (CPU_NUM_SHIFT-THREAD_NUM_BITS)
#define THREAD_NUM_MASK  0x7

#define FMA_WINDOW_SIZE    (1024 * 1024 * 1024L)
#define GHAL_FMA_CTRL_SIZE      0x1000UL


int __real_open(const char *pathname, int flags);

void
kgni_init (const char *pathname, int flags)
{
  static int already_init = 0;

  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  if (!already_init)
    {
      mt = list_new ();
      hobbes_client_init ();
      cmdq_segid = xemem_lookup_segid ("GEMINI-NEW");

      hcq = hcq_connect (cmdq_segid);
      if (hcq == HCQ_INVALID_HANDLE)
	{
	  printf ("connect failed\n");
	  return -1;
	}
      already_init = 1;
    }
}

/* We need to start cmd queue for PMI calls or kgni ioctls */

int
__wrap_open (const char *pathname, int flags, ...)
{
  /* Some evil injected code goes here. */
  if (strncmp (pathname, "/dev/kgni0", 10) == 0)
    {
      client_fd = __real_open ("/home/smukher/temp", flags);
      kgni_init (pathname, flags);
      return client_fd;
    }
  else if (strncmp (pathname, "/dev/pmi", 8) == 0)
    {
      pmi_fd = __real_open ("/home/smukher/temp1", flags);
      kgni_init (pathname, flags);
      return pmi_fd;
    }
  else
    {
      return __real_open (pathname, flags);
    }
}

int __real_ioctl (int fd, int request, void *data);

int
__wrap_ioctl (int fd, unsigned long int request, ...)
{
  char *msg;
  va_list args;
  void *argp;

  /* extract argp from varargs */
  va_start (args, request);
  argp = va_arg (args, void *);
  va_end (args);

  if (fd == client_fd)
    {
      //fprintf (stderr, "ioctl: on kgni device\n");
      //fflush (stderr);
      handle_ioctl (client_fd, request, argp);
      return 0;
    }
  else if (fd == pmi_fd)
    {
      //fprintf (stderr, "ioctl: on PMI device\n");
      //fflush (stderr);
      handle_ioctl (client_fd, request, argp);
      return 0;
    }
  else
    {
      return __real_ioctl (fd, request, argp);
    }
}


int
pack_args (unsigned long int request, void *args)
{
  gni_nic_setattr_args_t *nic_set_attr1;
  gni_mem_register_args_t *mem_reg_attr;
  gni_mem_register_args_t *mem_reg_attr1;
  gni_cq_destroy_args_t *cq_destroy_args;
  gni_cq_create_args_t *cq_create_args;
  gni_cq_wait_event_args_t *cq_wait_event_args;
  gni_post_rdma_args_t *post_rdma_args;
  void *reg_buf;
  void *rdma_post_tmpbuf;
  void *cq_tmpbuf;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  gni_mem_segment_t *segment;	/*comes from gni_pub.h */
  int i;
  uint32_t seg_cnt;
  xemem_segid_t cq_index_seg;
  xemem_segid_t reg_mem_seg;
  xemem_segid_t my_mem_seg;
  xemem_segid_t cq_seg;
  xemem_segid_t rdma_post_seg;
  uint32_t len = 0;
  int *rc;

  /* PMI ioctls args */
  pmi_allgather_args_t *gather_arg;
  pmi_getsize_args_t *size_arg;
  pmi_getrank_args_t *rank_arg;
  int *size = malloc (sizeof (int));
  int *rank = malloc (sizeof (int));
  mdh_addr_t *clnt_gather_hdl;

  switch (request)
    {
    case GNI_IOC_NIC_SETATTR:
      /* IOCTL  1. handle NIC_SETATTR ioctl for client */
      nic_set_attr1 = (gni_nic_setattr_args_t *) args;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_NIC_SETATTR,
		       sizeof (gni_nic_setattr_args_t), nic_set_attr1);

      fprintf (stderr, "cmd = %0x data size %d\n", cmd,
	       sizeof (nic_set_attr1));
      nic_set_attr1 = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) nic_set_attr1, sizeof (nic_set_attr1));
      printf ("after NIC attach  cookie : %d ptag :%d  rank :%d\n",
	      nic_set_attr1->cookie, nic_set_attr1->ptag,
	      nic_set_attr1->rank);
      break;
    case GNI_IOC_CQ_CREATE:
      cq_create_args =
	(gni_cq_create_args_t *) malloc (sizeof (gni_cq_create_args_t));
      memcpy (cq_create_args, args, sizeof (gni_cq_create_args_t));
      /* Following is what we get in args for this ioctl from client side 
         cq_create_args.queue = (gni_cq_entry_t *) cq->queue;
         cq_create_args.entry_count = entry_count;
         cq_create_args.mem_hndl = cq->mem_hndl;
         cq_create_args.mode = mode;
         cq_create_args.delay_count = delay_count;
         cq_create_args.kern_cq_descr = GNI_INVALID_CQ_DESCR;
         cq_create_args.interrupt_mask = NULL;
         fprintf (stderr, "client ioctl for cq create %p, entry count %d\n",
         cq_create_args->queue, cq_create_args->entry_count);
         fprintf (stderr, " cLIENT CQ_CREATE mem hndl word1=0x%16lx, word2 = 0x%16lx\n",
         cq_create_args->mem_hndl.qword1, cq_create_args->mem_hndl.qword2);
       */
      cq_tmpbuf = cq_create_args->queue;	/* preserve cq space */
	fprintf(stderr, "print seglist in CQ CREATE for vaddr %p\n", cq_create_args->queue);
      cq_seg = list_find_segid_by_vaddr (mt, cq_create_args->queue);
      cq_create_args->queue = (gni_cq_entry_t *) cq_seg;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_CQ_CREATE, sizeof (gni_cq_create_args_t),
		       cq_create_args);
      cq_create_args = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      cq_create_args->queue = (gni_cq_entry_t *) cq_tmpbuf;	/* restore cq space */
      memcpy (args, (void *) cq_create_args, sizeof (gni_cq_create_args_t));
      break;

    case GNI_IOC_MEM_REGISTER:
      /* If memory is segmented we just loop through the list 
         segments[0].address && segments[0].length
       */
      mem_reg_attr = (gni_mem_register_args_t *)args;
      printf ("client  mem register vaddr before xemem_make: %p, len %d\n",
	      mem_reg_attr->address, mem_reg_attr->length);
      reg_mem_seg =
	xemem_make (mem_reg_attr->address, mem_reg_attr->length, NULL);
      fprintf (stderr, " mem register after xemem make client register  segment %llu address %p\n",
	       reg_mem_seg, mem_reg_attr->address);
	list_add_element (mt, &reg_mem_seg, mem_reg_attr->address, mem_reg_attr->length);
	fprintf(stderr, "print list in MEM REGISTER\n");
	//list_print(mt);
      mem_reg_attr->address = (uint64_t) reg_mem_seg;
      // Now segid actually is 64 bit so push it there
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_MEM_REGISTER,
		       sizeof (gni_mem_register_args_t),
		       (char *) mem_reg_attr);

      mem_reg_attr = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) mem_reg_attr, sizeof (gni_mem_register_args_t));
      break;

    case GNI_IOC_CQ_WAIT_EVENT:
      /*  cq_hndl->queue is a user /client address space area. i.e 
         During cq_create iocti, one needs to create a xemem seg of 4KB size
         cq_idx = GNII_CQ_REMAP_INDEX(cq_hndl, cq_hndl->read_index);

         cq_wait_event_args.kern_cq_descr =
         (uint64_t *)cq_hndl->kern_cq_descr;
         cq_wait_event_args.next_event_ptr =
         (uint64_t **)&cq_hndl->queue[cq_idx];
         cq_wait_event_args.num_cqs = 0;
       */
      cq_wait_event_args = args;
      cq_index_seg =
	list_find_segid_by_vaddr (mt, cq_wait_event_args->next_event_ptr);
      cq_wait_event_args->next_event_ptr = cq_index_seg;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_CQ_WAIT_EVENT,
		       sizeof (gni_cq_wait_event_args_t), cq_wait_event_args);
      cq_wait_event_args = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) cq_wait_event_args, sizeof (cq_wait_event_args));
      break;
    case GNI_IOC_CQ_DESTROY:
      cq_destroy_args = args;	/* ONLY need cq_destroy_args.kern_cq_descr, a opaque pointer */
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_CQ_DESTROY,
		       sizeof (gni_cq_destroy_args_t), cq_destroy_args);
      cq_destroy_args = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) cq_destroy_args, sizeof (cq_destroy_args));
      break;
    case PMI_IOC_ALLGATHER:
      gather_arg = args;	// Now check to see if we are gathering mem hnlds
      if (gather_arg->in_data_len == sizeof (mdh_addr_t))
	{
	  clnt_gather_hdl = gather_arg->in_data;
	  /*
	     fprintf(stdout, "Client casting :  0x%lx    0x%016lx    0x%016lx\n",
	     clnt_gather_hdl->addr,
	     clnt_gather_hdl->mdh.qword1,
	     clnt_gather_hdl->mdh.qword2);
	   */
	  my_mem_seg = list_find_segid_by_vaddr (mt, clnt_gather_hdl->addr);
	  clnt_gather_hdl->addr = (uint64_t) my_mem_seg;
	  //fprintf(stdout, "client  gather after segid found :  %llu\n", clnt_gather_hdl->addr);
		//list_print(mt);
	}
      allgather (gather_arg->in_data, gather_arg->out_data,
		 gather_arg->in_data_len);
      break;
    case PMI_IOC_GETSIZE:
      size_arg = args;
      cmd = hcq_cmd_issue (hcq, PMI_IOC_GETSIZE, sizeof (int), size);
      size = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      comm_size = *size;
      size_arg->comm_size = *size;	/* Store it in a global */
      break;
    case PMI_IOC_GETRANK:
      rank_arg = args;
      cmd = hcq_cmd_issue (hcq, PMI_IOC_GETRANK, sizeof (int), rank);
      rank = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      rank_arg->myrank = *rank;	/* Store it in a global */
      my_rank = *rank;
      break;
    case PMI_IOC_FINALIZE:
      pmi_finalize ();
      break;
    case PMI_IOC_BARRIER:
      pmi_barrier ();
      break;

    case GNI_IOC_NIC_FMA_CONFIG:
      fprintf (stderr, "FMA config client case\n");
      break;
    case GNI_IOC_EP_POSTDATA:
      fprintf (stderr, "EP POSTDATA client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_TEST:
      fprintf (stderr, "EP POSTDATA test client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_WAIT:
      fprintf (stderr, "EP POSTDATA WAIT client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_TERMINATE:
      fprintf (stderr, "EP POSTDATA TERMINATE client case\n");
      break;

    case GNI_IOC_MEM_DEREGISTER:
      fprintf (stderr, "mem deregister client case\n");
      break;
    case GNI_IOC_POST_RDMA:
      post_rdma_args = args;
/*
      fprintf (stderr,
	       "client side POST RDMA  local addr 0x%lx    word1 0x%016lx   word2  0x%016lx\n",
	       post_rdma_args->post_desc->local_addr,
	       post_rdma_args->post_desc->local_mem_hndl.qword1,
	       post_rdma_args->post_desc->local_mem_hndl.qword2);
*/
      rdma_post_seg =
	list_find_segid_by_vaddr (mt, post_rdma_args->post_desc->local_addr);
      post_rdma_args->post_desc->local_addr = (uint64_t) rdma_post_seg;
      // Get a big buffer and pack post desc and rdma-args
      //fprintf (stderr, "post RDMA  client segid %llu\n", rdma_post_seg);
      rdma_post_tmpbuf =
	malloc (sizeof (gni_post_rdma_args_t) +
		sizeof (gni_post_descriptor_t));
      memcpy ((void *) rdma_post_tmpbuf,
	      post_rdma_args->post_desc, sizeof (gni_post_descriptor_t));
      memcpy (rdma_post_tmpbuf + sizeof (gni_post_descriptor_t),
	      post_rdma_args, sizeof (gni_post_rdma_args_t));
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_POST_RDMA,
		       sizeof (gni_post_rdma_args_t) +
		       sizeof (gni_post_descriptor_t), rdma_post_tmpbuf);

      rc = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      free (rdma_post_tmpbuf);
      break;

    case GNI_IOC_NIC_VMDH_CONFIG:
      fprintf (stderr, "VMDH config  client case\n");
      break;
    case GNI_IOC_NIC_NTT_CONFIG:
      fprintf (stderr, "NTT config client case\n");
      break;

    case GNI_IOC_NIC_JOBCONFIG:
      fprintf (stderr, "NIC jobconfig client case\n");
      break;

    case GNI_IOC_FMA_SET_PRIVMASK:
      fprintf (stderr, "FMA set PRIVMASK client case\n");
      break;

    case GNI_IOC_SUBSCRIBE_ERR:
      fprintf (stderr, "subscribe error client case\n");
      break;
    case GNI_IOC_RELEASE_ERR:
      fprintf (stderr, "Release error client case\n");
      break;
    case GNI_IOC_SET_ERR_MASK:
      fprintf (stderr, "Set Err Mask client case\n");
      break;
    case GNI_IOC_GET_ERR_EVENT:
      fprintf (stderr, "Get Err event client case\n");
      break;
    case GNI_IOC_WAIT_ERR_EVENTS:
      fprintf (stderr, "Wait err event client case\n");
      break;
    case GNI_IOC_SET_ERR_PTAG:
      fprintf (stderr, "EP POSTDATA client case\n");
      break;
    case GNI_IOC_CDM_BARR:
      fprintf (stderr, "CDM BARR client case\n");
      break;
    case GNI_IOC_NIC_NTTJOB_CONFIG:
      fprintf (stderr, "NTT JOBCONFIG client case\n");
      break;
    case GNI_IOC_DLA_SETATTR:
      fprintf (stderr, "DLA SETATTR client case\n");
      break;
    case GNI_IOC_VCE_ALLOC:
      fprintf (stderr, "VCE ALLOC client case\n");
      break;
    case GNI_IOC_VCE_FREE:
      fprintf (stderr, "VCE FREE client case\n");
      break;
    case GNI_IOC_VCE_CONFIG:
      fprintf (stderr, "VCE CONFIG client case\n");
      break;
    case GNI_IOC_FLBTE_SETATTR:
      fprintf (stderr, "FLBTE SETATTR client case\n");
      break;
    case GNI_IOC_FMA_ASSIGN:
      fprintf (stderr, "FMA ASSIGN  client case\n");
      break;
    case GNI_IOC_FMA_UMAP:
      fprintf (stderr, "fma unmap client case\n");
      break;
    case GNI_IOC_MEM_QUERY_HNDLS:
      fprintf (stderr, "mem query hndlclient case\n");
      break;
    default:
      fprintf (stderr,
	       "Called default case in client side for kgni ioctl %llu\n",
	       request);
      break;
    }
  return 0;
}


int
handle_ioctl (int device, unsigned long int request, void *arg)
{

  pack_args (request, arg);
  unpack_args (request, arg);
  return 0;
}

int
unpack_args (unsigned long int request, void *args)
{
  gni_nic_setattr_args_t *nic_set_attr;
  gni_cq_wait_event_args_t *cq_wait_event_args;
  switch (request)
    {
    case GNI_IOC_NIC_SETATTR:
      nic_set_attr = args;

      struct xemem_addr win_addr;
      struct xemem_addr put_addr;
      struct xemem_addr get_addr;
      struct xemem_addr ctrl_addr;


      void *fma_win = NULL;
      void *fma_put = NULL;
      void *fma_get = NULL;
      void *fma_ctrl = NULL;
      void *fma_nc = NULL;

      xemem_apid_t apid;

      xemem_segid_t fma_win_seg;
      xemem_segid_t fma_put_seg;
      xemem_segid_t fma_get_seg;
      xemem_segid_t fma_ctrl_seg;
      hcq_cmd_t cmd = HCQ_INVALID_CMD;


      fma_win_seg = xemem_lookup_segid ("fma_win_seg");
      apid = xemem_get (fma_win_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_win_seg);
	  return HCQ_INVALID_HANDLE;
	}

      win_addr.apid = apid;
      win_addr.offset = 0;

      fma_win = xemem_attach_nocache (win_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_win == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_win_seg);
	  return HCQ_INVALID_HANDLE;
	}
      //printf ("after xemem attch of fma window %llu\n", fma_win);
      list_add_element (mt, &fma_win_seg, fma_win, FMA_WINDOW_SIZE);
/*********************************/

      fma_put_seg = xemem_lookup_segid ("fma_win_put");
      apid = xemem_get (fma_put_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_put_seg);
	  return -1;		//invalid handle
	}

      put_addr.apid = apid;
      put_addr.offset = 0;

      fma_put = xemem_attach_nocache (put_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_put == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_win_seg);
	  return -1;		// invalid HCQ handle
	}
      //printf ("after xemem attach of fma PUT window %llu\n", fma_put);
/***************************/
      list_add_element (mt, &fma_put_seg, fma_put, FMA_WINDOW_SIZE);
      fma_get_seg = xemem_lookup_segid ("fma_win_get");
      apid = xemem_get (fma_get_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_get_seg);
	  return HCQ_INVALID_HANDLE;
	}

      get_addr.apid = apid;
      get_addr.offset = 0;

      fma_get = xemem_attach_nocache (get_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_get == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_get_seg);
	  return HCQ_INVALID_HANDLE;
	}
      //printf ("after xemem attach of fma GET window %llu\n", fma_get);
      list_add_element (mt, &fma_get_seg, fma_get, FMA_WINDOW_SIZE);
      fma_ctrl_seg = xemem_lookup_segid ("fma_win_ctrl");
      apid = xemem_get (fma_ctrl_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_ctrl_seg);
	  return HCQ_INVALID_HANDLE;
	}

      ctrl_addr.apid = apid;
      ctrl_addr.offset = 0;

      fma_ctrl = xemem_attach_nocache (ctrl_addr, GHAL_FMA_CTRL_SIZE, NULL);

      if (fma_ctrl == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_ctrl_seg);
	  return HCQ_INVALID_HANDLE;
	}
      //fprintf (stderr, "after xemem attach of 4 FMA window and segments: seglist below \n");
      list_add_element (mt, &fma_ctrl_seg, fma_ctrl, FMA_WINDOW_SIZE);
	//list_print(mt);

      nic_set_attr->fma_window = fma_win;
      nic_set_attr->fma_window_nwc = fma_put;
      nic_set_attr->fma_window_get = fma_get;
      nic_set_attr->fma_ctrl = fma_ctrl;
      break;
    case GNI_IOC_NIC_FMA_CONFIG:
      fprintf (stderr, "FMA config client case\n");
      break;
    case GNI_IOC_EP_POSTDATA:
      fprintf (stderr, "unpack EP POSTDATA client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_TEST:
      fprintf (stderr, "unpack EP POSTDATA test client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_WAIT:
      fprintf (stderr, "unpack EP POSTDATA WAIT client case\n");
      break;
    case GNI_IOC_EP_POSTDATA_TERMINATE:
      fprintf (stderr, "unpack EP POSTDATA TERMINATE client case\n");
      break;

    case GNI_IOC_MEM_DEREGISTER:
      fprintf (stderr, "unpack mem deregister client case\n");
      break;
    case GNI_IOC_POST_RDMA:
      break;

    case GNI_IOC_NIC_VMDH_CONFIG:
      fprintf (stderr, "unpack VMDH config  client case\n");
      break;
    case GNI_IOC_NIC_NTT_CONFIG:
      fprintf (stderr, "unpack NTT config client case\n");
      break;

    case GNI_IOC_NIC_JOBCONFIG:
      fprintf (stderr, "unpack NIC jobconfig client case\n");
      break;

    case GNI_IOC_FMA_SET_PRIVMASK:
      fprintf (stderr, "unpack FMA set PRIVMASK client case\n");
      break;

    case GNI_IOC_SUBSCRIBE_ERR:
      fprintf (stderr, "unpack subscribe error client case\n");
      break;
    case GNI_IOC_RELEASE_ERR:
      fprintf (stderr, "unpack Release error client case\n");
      break;
    case GNI_IOC_SET_ERR_MASK:
      fprintf (stderr, "unpack Set Err Mask client case\n");
      break;
    case GNI_IOC_GET_ERR_EVENT:
      fprintf (stderr, "unpack Get Err event client case\n");
      break;
    case GNI_IOC_WAIT_ERR_EVENTS:
      fprintf (stderr, "unpack Wait err event client case\n");
      break;
    case GNI_IOC_SET_ERR_PTAG:
      fprintf (stderr, "unpack EP POSTDATA client case\n");
      break;
    case GNI_IOC_CDM_BARR:
      fprintf (stderr, "unpack CDM BARR client case\n");
      break;
    case GNI_IOC_NIC_NTTJOB_CONFIG:
      fprintf (stderr, "unpack NTT JOBCONFIG client case\n");
      break;
    case GNI_IOC_DLA_SETATTR:
      fprintf (stderr, "unpack DLA SETATTR client case\n");
      break;
    case GNI_IOC_VCE_ALLOC:
      fprintf (stderr, "unpack VCE ALLOC client case\n");
      break;
    case GNI_IOC_VCE_FREE:
      fprintf (stderr, "unpack VCE FREE client case\n");
      break;
    case GNI_IOC_VCE_CONFIG:
      fprintf (stderr, "unpack VCE CONFIG client case\n");
      break;
    case GNI_IOC_FLBTE_SETATTR:
      fprintf (stderr, "unpack FLBTE SETATTR client case\n");
      break;
    case GNI_IOC_FMA_ASSIGN:
      fprintf (stderr, "unpack FMA ASSIGN  client case\n");
      break;
    case GNI_IOC_FMA_UMAP:
      fprintf (stderr, "unpack fma unmap client case\n");
      break;
    case GNI_IOC_MEM_QUERY_HNDLS:
      fprintf (stderr, "unpack mem query hndlclient case\n");
      break;
    case GNI_IOC_CQ_CREATE:
      break;
    case GNI_IOC_CQ_DESTROY:
      fprintf (stderr, "unpack CQ destroy  client case\n");
      break;
    case GNI_IOC_MEM_REGISTER:
      break;
    case GNI_IOC_CQ_WAIT_EVENT:
      fprintf (stderr, "unpack CQ wait event client case\n");
      break;
    case PMI_IOC_ALLGATHER:
      break;
    case PMI_IOC_GETRANK:
      break;
    case PMI_IOC_GETSIZE:
      break;
    case PMI_IOC_FINALIZE:
      break;
    case PMI_IOC_BARRIER:
      break;
    case PMI_IOC_MALLOC:
      break;
    default:
      fprintf (stderr, "called disconnect in client default\n");
      //hcq_disconnect (hcq);
      //hobbes_client_deinit ();
      break;
    }
  //let's print something what we got back in nic_set_attr struct 
  /* End of NIC SET ATTR */
  return 0;

}
