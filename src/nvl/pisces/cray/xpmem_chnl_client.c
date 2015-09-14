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

#include "gni_priv.h"
#include "gni_pub.h"
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
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  cmd = hcq_cmd_issue (hcq, PMI_IOC_ALLGATHER, len, in);
  out = hcq_get_ret_data (hcq, cmd, &retlen);
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


typedef int (*orig_open_f_type) (const char *pathname, int flags);

void
kgni_open (const char *pathname, int flags)
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
open (const char *pathname, int flags, ...)
{
  /* Some evil injected code goes here. */
  orig_open_f_type orig_open;
  orig_open = (orig_open_f_type) dlsym (RTLD_NEXT, "open");
  if (strncmp (pathname, "/dev/kgni0", 10) == 0)
    {
      client_fd = orig_open ("/home/smukher/temp", flags);
      printf
	(" gemini proxy client  called open: fd is = %d!!!\n", client_fd);
      kgni_open (pathname, flags);
      return client_fd;
    }
  else if (strncmp (pathname, "/dev/pmi", 8) == 0)
    {
      pmi_fd = orig_open ("/home/smukher/temp1", flags);
      printf ("  client  called PMI open: fd is = %d!!!\n", pmi_fd);
      kgni_open (pathname, flags);
      return pmi_fd;
    }
  else
    {
      return orig_open (pathname, flags);
    }
}

typedef int (*next_ioctl_f_type) (int fd, int request, void *data);

int
ioctl (int fd, unsigned long int request, ...)
{
  char *msg;
  va_list args;
  void *argp;

  /* extract argp from varargs */
  va_start (args, request);
  argp = va_arg (args, void *);
  va_end (args);

  next_ioctl_f_type next_ioctl;
  next_ioctl = dlsym (RTLD_NEXT, "ioctl");
  if ((fd == client_fd) || (fd == pmi_fd))
    {
      fprintf (stderr, "ioctl: on kgni device\n");
      fflush (stderr);
      handle_ioctl (client_fd, request, argp);
      return 0;
    }
  else
    {
      return next_ioctl (fd, request, argp);
    }
}

int
pack_args (int request, void *args)
{
  gni_nic_setattr_args_t *nic_set_attr1;
  gni_mem_register_args_t *mem_reg_attr;
  gni_cq_destroy_args_t *cq_destroy_args;
  gni_cq_create_args_t *cq_create_args;
  gni_cq_wait_event_args_t *cq_wait_event_args;
  void *reg_buf;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  xemem_segid_t reg_mem_seg;
  xemem_segid_t *cq_seg;
  gni_mem_segment_t *segment;	/*comes from gni_pub.h */
  int i;
  uint32_t seg_cnt;
  xemem_segid_t *cq_index_seg;

  /* PMI ioctls args */
  pmi_allgather_args_t *gather_arg;
  pmi_getsize_args_t *size_arg;
  pmi_getrank_args_t *rank_arg;
  int *size = malloc (sizeof (int));
  int *rank = malloc (sizeof (int));


  switch (request)
    {
    case GNI_IOC_NIC_SETATTR:
      /* IOCTL  1. handle NIC_SETATTR ioctl for client */
      nic_set_attr1 = (gni_nic_setattr_args_t *) args;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_NIC_SETATTR,
		       sizeof (gni_nic_setattr_args_t), nic_set_attr1);

      //printf ("cmd = %llu data size %d\n", cmd, sizeof (nic_set_attr1));
      uint32_t len = 0;
      nic_set_attr1 = hcq_get_ret_data (hcq, cmd, &len);
      printf ("from server return cookie : %d ptag :%d  rank :%d\n",
	      nic_set_attr1->cookie, nic_set_attr1->ptag,
	      nic_set_attr1->rank);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) nic_set_attr1, sizeof (nic_set_attr1));
      break;
    case GNI_IOC_CQ_CREATE:
      cq_create_args = args;	/* create xememseg of queue */
      /* Following is what we get in args for this ioctl from client side 
         cq_create_args.queue = (gni_cq_entry_t *) cq->queue;
         cq_create_args.entry_count = entry_count;
         cq_create_args.mem_hndl = cq->mem_hndl;
         cq_create_args.mode = mode;
         cq_create_args.delay_count = delay_count;
         cq_create_args.kern_cq_descr = GNI_INVALID_CQ_DESCR;
         cq_create_args.interrupt_mask = NULL;
       */
      cq_seg = list_find (mt, cq_create_args->queue);
      cq_create_args->queue = (gni_cq_entry_t *) cq_seg;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_CQ_CREATE, sizeof (cq_create_args),
		       cq_create_args);
      cq_create_args = hcq_get_ret_data (hcq, cmd, &len);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) cq_create_args, sizeof (cq_create_args));
      break;

    case GNI_IOC_MEM_REGISTER:
      /* If memory is segmented we just loop through the list 
         segments[0].address && segments[0].length
       */
      mem_reg_attr = (gni_mem_register_args_t *) args;
      printf ("In mem register attr address %p, len %d\n",
	      mem_reg_attr->address, mem_reg_attr->length);
      // check to see if memory is segmented
      if (mem_reg_attr->mem_segments == NULL)
	{
	  reg_mem_seg =
	    xemem_make (mem_reg_attr->address, mem_reg_attr->length, "");
	  if (reg_mem_seg == NULL)
	    {
	      printf ("clinet could not create a segment for registering\n");
	    }
	  else
	    {

	      printf ("sucessfully created xemem segid %llu\n", reg_mem_seg);
	    }
	  // Now segid actually is 64 bit so push it there
	  list_add_element (mt, (uint64_t) reg_mem_seg, mem_reg_attr->address,
			    mem_reg_attr->length);
	  mem_reg_attr->address = (uint64_t) reg_mem_seg;
	}
      else
	{
	  segment = mem_reg_attr->mem_segments;	/*i Look kgni_mem.c, kgni_mem_register_count */
	  seg_cnt = mem_reg_attr->segments_cnt;
	  for (i = 0; i < seg_cnt; i++)
	    {
	      reg_mem_seg =
		xemem_make (segment[i].address, segment[i].length, "");
	      list_add_element (mt, (uint64_t) reg_mem_seg,
				segment[i].address, segment[i].length);
	      segment[i].address = reg_mem_seg;
	    }

	}
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
      cq_index_seg = list_find (mt, cq_wait_event_args->next_event_ptr);
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
    case PMI_IOC_ALLGATHER:
      gather_arg = args;
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
    default:
      break;
    }
  return 0;
}


int
handle_ioctl (int device, int request, void *arg)
{

  int status;
  gni_nic_fmaconfig_args_t fma_attr;
  gni_mem_register_args_t mem_reg_attr;
  gni_mem_deregister_args_t mem_dereg_attr;
  gni_post_rdma_args_t post_attr;
  gni_post_descriptor_t post_desc;
  gni_mem_handle_t send_mhndl;
  uint8_t *rcv_data;
  gni_mem_handle_t rcv_mhndl;
  gni_mem_handle_t peer_rcv_mhndl;
  gni_cq_entry_t *event_data;
  void *fma_window;
  uint64_t *get_window;
  uint64_t gcw, get_window_offset;
  int i, max_rank, j;
  FILE *hf;
  gni_post_state_t *state_array;
  int connected = 0;

  int rc;
  int ret = -1;
  pack_args (request, arg);
  unpack_args (request, arg);
  return 0;
}

int
unpack_args (int request, void *args)
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

      fma_win = xemem_attach (win_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_win == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_win_seg);
	  return HCQ_INVALID_HANDLE;
	}
      //printf ("after xemem attch of fma window %llu\n", fma_win);
      list_add_element (mt, (uint64_t) fma_win_seg, fma_win, FMA_WINDOW_SIZE);
/*********************************/

      fma_put_seg = xemem_lookup_segid ("fma_win_put");
      apid = xemem_get (fma_put_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_put_seg);
	  return HCQ_INVALID_HANDLE;
	}

      put_addr.apid = apid;
      put_addr.offset = 0;

      fma_put = xemem_attach (put_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_put == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_win_seg);
	  return HCQ_INVALID_HANDLE;
	}
      //printf ("after xemem attach of fma PUT window %llu\n", fma_put);
/***************************/
      list_add_element (mt, (uint64_t) fma_put_seg, fma_put, FMA_WINDOW_SIZE);
      fma_get_seg = xemem_lookup_segid ("fma_win_get");
      apid = xemem_get (fma_get_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_get_seg);
	  return HCQ_INVALID_HANDLE;
	}

      get_addr.apid = apid;
      get_addr.offset = 0;

      fma_get = xemem_attach (get_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_get == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_get_seg);
	  return HCQ_INVALID_HANDLE;
	}
      //printf ("after xemem attach of fma GET window %llu\n", fma_get);
      list_add_element (mt, (uint64_t) fma_get_seg, fma_get, FMA_WINDOW_SIZE);
      fma_ctrl_seg = xemem_lookup_segid ("fma_win_ctrl");
      apid = xemem_get (fma_ctrl_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_ctrl_seg);
	  return HCQ_INVALID_HANDLE;
	}

      ctrl_addr.apid = apid;
      ctrl_addr.offset = 0;

      fma_ctrl = xemem_attach (ctrl_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_ctrl == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_ctrl_seg);
	  return HCQ_INVALID_HANDLE;
	}
      printf ("after xemem attach of fma CTRL window %p\n", fma_ctrl);
      list_add_element (mt, (uint64_t) fma_ctrl_seg, fma_ctrl,
			FMA_WINDOW_SIZE);

      nic_set_attr->fma_window = fma_win;
      nic_set_attr->fma_window_nwc = fma_put;
      nic_set_attr->fma_window_get = fma_get;
      nic_set_attr->fma_ctrl = fma_ctrl;
      break;
    case GNI_IOC_CQ_CREATE:
      break;
    case GNI_IOC_CQ_DESTROY:
      break;
    case GNI_IOC_MEM_REGISTER:
      break;
    case GNI_IOC_CQ_WAIT_EVENT:
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
    default:
      fprintf (stderr, "called disconnect in client default\n");
      hcq_disconnect (hcq);
      hobbes_client_deinit ();
      break;
    }
  //let's print something what we got back in nic_set_attr struct 
  /* End of NIC SET ATTR */
  return 0;

}
