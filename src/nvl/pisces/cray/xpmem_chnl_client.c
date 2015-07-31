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

#include "hobbes_cmd_queue.h"
#include "xemem.h"
#include "xpmem.h"
#include <hobbes_cmd_queue.h>
#include <xemem.h>
#include <hobbes_enclave.h>
#include <dlfcn.h>		/* For dlsym */


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

int client_fd;
xemem_segid_t cmdq_segid;
hcq_handle_t hcq = HCQ_INVALID_HANDLE;


typedef int (*orig_open_f_type) (const char *pathname, int flags);

int
kgni_open (const char *pathname, int flags)
{

  hcq_cmd_t cmd = HCQ_INVALID_CMD;

  hobbes_client_init ();
  cmdq_segid = xemem_lookup_segid ("GEMINI-NEW");

  printf ("client lookup for seg to connect\n");
  hcq = hcq_connect (cmdq_segid);
  if (hcq == HCQ_INVALID_HANDLE)
    {
      printf ("connect failed\n");
      return -1;
    }
  printf
    (" gemini proxy client  called open: will foward to xpmem server '%s'!!!\n",
     pathname);
  orig_open_f_type orig_open;
  orig_open = (orig_open_f_type) dlsym (RTLD_NEXT, "open");
  client_fd = orig_open ("/tmp/temp", O_RDWR);
  return client_fd;
}

int
open (const char *pathname, int flags, ...)
{
  /* Some evil injected code goes here. */
  printf ("xpmem open wrapper open(...) to access '%s'!!!\n", pathname);
  orig_open_f_type orig_open;
  orig_open = (orig_open_f_type) dlsym (RTLD_NEXT, "open");
  if (strncmp (pathname, "/dev/kgni0", 10) == 0)
    {
      kgni_open (pathname, flags);
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

  fprintf (stderr, "ioctl : wrapping ioctl\n");
  fflush (stderr);
  next_ioctl_f_type next_ioctl;
  next_ioctl = dlsym (RTLD_NEXT, "ioctl");
  fprintf (stderr, "next_ioctl = %p\n", next_ioctl);
  fflush (stderr);
  if (fd == client_fd)
    {
      fprintf (stderr, "ioctl: on kgni device\n");
      fflush (stderr);
      handle_ioctl (client_fd, request, argp);
      return 0;
    }
  else
    {
      fprintf (stderr, "ioctl: call actual ioctl\n");
      fflush (stderr);
      return next_ioctl (fd, request, argp);
    }
}

int
pack_args (int request, void *args)
{
  gni_nic_setattr_args_t *nic_set_attr1;
  gni_mem_register_args_t *mem_reg_attr;
  void *reg_buf;
  hcq_cmd_t cmd = HCQ_INVALID_CMD;
  xemem_segid_t reg_mem_seg;
  gni_mem_segment_t *segment;	/*comes from gni_pub.h */
  int i;
  uint32_t seg_cnt;


  switch (request)
    {
    case GNI_IOC_NIC_SETATTR:
      /* IOCTL  1. handle NIC_SETATTR ioctl for client */
      nic_set_attr1 = (gni_nic_setattr_args_t *) args;
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_NIC_SETATTR, sizeof (nic_set_attr1),
		       nic_set_attr1);

      printf ("cmd = %llu data size %d\n", cmd, sizeof (nic_set_attr1));
      uint32_t len = 0;
      nic_set_attr1 = hcq_get_ret_data (hcq, cmd, &len);
      printf ("from server return cookie : %d ptag :%d  rank :%d\n",
	      nic_set_attr1->cookie, nic_set_attr1->ptag,
	      nic_set_attr1->rank);
      hcq_cmd_complete (hcq, cmd);
      memcpy (args, (void *) nic_set_attr1, sizeof (nic_set_attr1));
      break;

    case GNI_IOC_MEM_REGISTER:
      /* If memory is segmented we just loop through the list 
         segments[0].address && segments[0].length
       */
      mem_reg_attr = (gni_mem_register_args_t *) args;
      // check to see if memory is segmented
      if (mem_reg_attr->mem_segments == NULL)
	{
	  reg_mem_seg =
	    xemem_make (mem_reg_attr->address, mem_reg_attr->length, "");
	  // Now segid actually is 64 bit so push it there
	  mem_reg_attr->address = (uint64_t) reg_mem_seg;
	}
      else
	{
	  segment = mem_reg_attr->mem_segments;	/*i Look kgni_mem.c, kgni_mem_register_count */
	  seg_cnt = mem_reg_attr->segments_cnt;
	  for (i = 0; i < seg_cnt; i++)
	    {
	      reg_mem_seg =
		xemem_make (segment[i].address, segment[0].length, "");
	      segment[i].address = reg_mem_seg;
	    }

	}
      cmd =
	hcq_cmd_issue (hcq, GNI_IOC_MEM_REGISTER, sizeof (mem_reg_attr),
		       mem_reg_attr);

      break;
    default:
      break;
    }
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
  ghal_fma_desc_t fma_desc_cpu = GHAL_FMA_INIT;
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
}

int
unpack_args (int request, void *args)
{
  gni_nic_setattr_args_t *nic_set_attr;

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
      printf ("apid = %llu window segid for FMA from server %llu\n",
	      win_addr.apid, fma_win_seg);

      fma_win = xemem_attach (win_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_win == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_win_seg);
	  return HCQ_INVALID_HANDLE;
	}
      printf ("after xemem attch of fma window %llu\n", fma_win);
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
      printf ("apid = %llu put segid for FMA from server %llu\n",
	      put_addr.apid, fma_put_seg);

      fma_put = xemem_attach (put_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_put == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_win_seg);
	  return HCQ_INVALID_HANDLE;
	}
      printf ("after xemem attach of fma PUT window %llu\n", fma_put);
/***************************/
      fma_get_seg = xemem_lookup_segid ("fma_win_get");
      apid = xemem_get (fma_get_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_get_seg);
	  return HCQ_INVALID_HANDLE;
	}

      get_addr.apid = apid;
      get_addr.offset = 0;
      printf ("apid = %llu GET segid for FMA from server %llu\n",
	      get_addr.apid, fma_get_seg);

      fma_get = xemem_attach (get_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_get == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_get_seg);
	  return HCQ_INVALID_HANDLE;
	}
      printf ("after xemem attach of fma GET window %llu\n", fma_get);
      fma_ctrl_seg = xemem_lookup_segid ("fma_win_ctrl");
      apid = xemem_get (fma_ctrl_seg, XEMEM_RDWR);
      if (apid <= 0)
	{
	  xemem_remove (fma_ctrl_seg);
	  return HCQ_INVALID_HANDLE;
	}

      ctrl_addr.apid = apid;
      ctrl_addr.offset = 0;
      printf ("apid = %llu CTRL segid for FMA from server %llu\n",
	      ctrl_addr.apid, fma_ctrl_seg);

      fma_ctrl = xemem_attach (ctrl_addr, FMA_WINDOW_SIZE, NULL);

      if (fma_ctrl == NULL)
	{
	  xemem_release (apid);
	  xemem_remove (fma_ctrl_seg);
	  return HCQ_INVALID_HANDLE;
	}
      printf ("after xemem attach of fma CTRL window %llu\n", fma_ctrl);

      nic_set_attr->fma_window = fma_win;
      nic_set_attr->fma_window_nwc = fma_put;
      nic_set_attr->fma_window_get = fma_get;
      nic_set_attr->fma_ctrl = fma_ctrl;
      break;
    default:
      hcq_disconnect (hcq);
      hobbes_client_deinit ();
      break;
    }
  //let's print something what we got back in nic_set_attr struct 
  /* End of NIC SET ATTR */
  return 0;

}
