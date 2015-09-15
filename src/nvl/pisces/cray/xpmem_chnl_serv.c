/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

/* User level test procedures */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
//
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#include <dbapi.h>
#include <dballoc.h>

//
#include "gni_priv.h"
#include "gni_pub.h"
#include <alps/libalpslli.h>

#include "hobbes_cmd_queue.h"
#include "xemem.h"
#include "xpmem.h"
#include "pmi.h"
#include "pmi_cray_ext.h"
#include "pmi_util.h"


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

static void
sfence (void)
{
  asm volatile ("sfence":::"memory");
}

static void
get_alps_info (alpsAppGni_t * alps_info)
{
  int alps_rc = 0;
  int req_rc = 0;
  size_t rep_size = 0;

  uint64_t apid = 0;
  alpsAppLLIGni_t *alps_info_list;
  char buf[1024];

  alps_info_list = (alpsAppLLIGni_t *) & buf[0];

  alps_app_lli_lock ();

  fprintf (stderr, "sending ALPS request\n");
  alps_rc = alps_app_lli_put_request (ALPS_APP_LLI_ALPS_REQ_GNI, NULL, 0);
  if (alps_rc != 0)
    fprintf (stderr, "alps_app_lli_put_request failed: %d", alps_rc);
  fprintf (stderr, "waiting for ALPS reply\n");
  alps_rc = alps_app_lli_get_response (&req_rc, &rep_size);
  if (alps_rc != 0)
    fprintf (stderr, "alps_app_lli_get_response failed: alps_rc=%d\n",
	     alps_rc);
  if (req_rc != 0)
    fprintf (stderr, "alps_app_lli_get_response failed: req_rc=%d\n", req_rc);
  if (rep_size != 0)
    {
      alps_rc = alps_app_lli_get_response_bytes (alps_info_list, rep_size);
      if (alps_rc != 0)
	fprintf (stderr, "alps_app_lli_get_response_bytes failed: %d",
		 alps_rc);
    }

  fprintf (stderr, "sending ALPS request\n");
  alps_rc = alps_app_lli_put_request (ALPS_APP_LLI_ALPS_REQ_APID, NULL, 0);
  if (alps_rc != 0)
    fprintf (stderr, "alps_app_lli_put_request failed: %d\n", alps_rc);
  fprintf (stderr, "waiting for ALPS reply");
  alps_rc = alps_app_lli_get_response (&req_rc, &rep_size);
  if (alps_rc != 0)
    fprintf (stderr, "alps_app_lli_get_response failed: alps_rc=%d\n",
	     alps_rc);
  if (req_rc != 0)
    fprintf (stderr, "alps_app_lli_get_response failed: req_rc=%d\n", req_rc);
  if (rep_size != 0)
    {
      alps_rc = alps_app_lli_get_response_bytes (&apid, rep_size);
      if (alps_rc != 0)
	fprintf (stderr, "alps_app_lli_get_response_bytes failed: %d\n",
		 alps_rc);
    }

  alps_app_lli_unlock ();

  memcpy (alps_info, (alpsAppGni_t *) alps_info_list->u.buf,
	  sizeof (alpsAppGni_t));
  return;
}


static uint32_t
get_cpunum (void)
{
  int i, j;
  uint32_t cpu_num;

  cpu_set_t coremask;

  (void) sched_getaffinity (0, sizeof (coremask), &coremask);

  for (i = 0; i < CPU_SETSIZE; i++)
    {
      if (CPU_ISSET (i, &coremask))
	{
	  int run = 0;
	  for (j = i + 1; j < CPU_SETSIZE; j++)
	    {
	      if (CPU_ISSET (j, &coremask))
		run++;
	      else
		break;
	    }
	  if (!run)
	    {
	      cpu_num = i;
	    }
	  else
	    {
	      fprintf (stdout,
		       "This thread is bound to multiple CPUs(%d).  Using lowest numbered CPU(%d).",
		       run + 1, cpu_num);
	      cpu_num = i;
	    }
	}
    }
  return (cpu_num);
}


#define GNI_INSTID(nic_addr, cpu_num, thr_num) (((nic_addr&NIC_ADDR_MASK)<<NIC_ADDR_SHIFT)|((cpu_num&CPU_NUM_MASK)<<CPU_NUM_SHIFT)|(thr_num&THREAD_NUM_MASK))

#define FMA_WINDOW_SIZE    (1024 * 1024 * 1024L)

/*
 * allgather gather the requested information from all of the ranks.
 */

static void
allgather (void *in, void *out, int len)
{
  static int already_called = 0;
  int i;
  static int *ivec_ptr = NULL;
  static int job_size = 0;
  int my_rank;
  char *out_ptr;
  int rc;
  char *tmp_buf;

  if (!already_called)
    {
      rc = PMI_Get_size (&job_size);
      assert (rc == PMI_SUCCESS);

      rc = PMI_Get_rank (&my_rank);
      assert (rc == PMI_SUCCESS);

      ivec_ptr = (int *) malloc (sizeof (int) * job_size);
      assert (ivec_ptr != NULL);

      rc = PMI_Allgather (&my_rank, ivec_ptr, sizeof (int));
      assert (rc == PMI_SUCCESS);

      already_called = 1;
    }

  tmp_buf = (char *) malloc (job_size * len);
  assert (tmp_buf);

  rc = PMI_Allgather (in, tmp_buf, len);
  assert (rc == PMI_SUCCESS);

  out_ptr = out;

  for (i = 0; i < job_size; i++)
    {
      memcpy (&out_ptr[len * ivec_ptr[i]], &tmp_buf[i * len], len);
    }

  free (tmp_buf);
}


int
main (int argc, char *argv[])
{
  int device;
  int status;
  gni_nic_setattr_args_t nic_set_attr;
  gni_nic_nttconfig_args_t ntt_conf_attr;
  gni_nic_vmdhconfig_args_t vmdh_conf_attr;
  gni_nic_fmaconfig_args_t fma_attr;
  gni_ep_postdata_args_t ep_post_attr;
  gni_ep_postdata_test_args_t ep_posttest_attr;
  gni_ep_postdata_term_args_t ep_postterm_attr;
  gni_mem_register_args_t *mem_register_attr;
  gni_mem_register_args_t mem_reg_attr;
  gni_mem_deregister_args_t mem_dereg_attr;
  gni_cq_create_args_t cq_create_attr;
  gni_cq_wait_event_args_t cq_wait_attr;
  gni_cq_destroy_args_t cq_destroy_attr;
  gni_post_rdma_args_t post_attr;
  gni_post_descriptor_t post_desc;
  uint8_t *send_data;
  gni_mem_handle_t send_mhndl;
  uint8_t *rcv_data;
  gni_mem_handle_t rcv_mhndl;
  gni_mem_handle_t peer_rcv_mhndl;
  uint8_t *peer_rcv_data;
  gni_cq_entry_t *event_data;
  char send_buff[256];
  char rcv_buff[256];
  void *fma_window;
  void *reg_addr = NULL;
  uint64_t *get_window;
  uint64_t gcw, get_window_offset;
  int i, max_rank, j;
  FILE *hf;
  int *pe_array;
  gni_post_state_t *state_array;
  int connected = 0;
  void *my_mem;
  int next_event_idx = 0;
  gni_mem_segment_t mem_segments[3];

  alpsAppGni_t alps_info;
  uint32_t nic_addr = 0;
  uint32_t cpu_num = 0;
  uint32_t thread_num = 0;
  uint32_t gni_cpu_id = 0;
  uint32_t instance;
  int rc;
  hcq_handle_t hcq = HCQ_INVALID_HANDLE;
  int fd = 0;
  fd_set rset;
  int max_fds = 0;
  int fd_cnt = 0;
  int ret = -1;
  xemem_segid_t fma_win, fma_put, fma_nc, fma_get, fma_ctrl;
  xemem_segid_t clean_seg, reg_mem_seg;
  struct xemem_addr r_addr;
/* for PMI */
  int my_rank = malloc (4);
  int job_size = malloc (4);
  int first_spawned;
  void *outbuf;
  void *mem_mem;

  rc = PMI_Init (&first_spawned);
  assert (rc == PMI_SUCCESS);
  rc = PMI_Get_size (&job_size);
  assert (rc == PMI_SUCCESS);

  rc = PMI_Get_rank (&my_rank);
  assert (rc == PMI_SUCCESS);

  hobbes_client_init ();


  hcq = hcq_create_queue ("GEMINI-NEW");

  if (hcq == HCQ_INVALID_HANDLE)
    {
      fprintf (stderr, "Could not create command queue\n");
      hcq_free_queue (hcq);
      return -1;
    }

  fprintf (stderr, "server segid: %llu\n", hcq_get_segid (hcq));

  fd = hcq_get_fd (hcq);


  get_alps_info (&alps_info);

  rc = GNI_CdmGetNicAddress (alps_info.device_id, &nic_addr, &gni_cpu_id);

  cpu_num = get_cpunum ();

  instance = GNI_INSTID (nic_addr, cpu_num, thread_num);
  fprintf (stderr, "after alps info get cookie %lu  ptag %lu  instance %d\n",
	   alps_info.cookie, alps_info.ptag, instance);

  device = open ("/dev/kgni0", O_RDWR);
  if (device < 0)
    {
      fprintf (stderr, "Failed to open device\n");
      return 0;
    }

  fprintf (stderr, "Opened device\n");
  FD_ZERO (&rset);
  FD_SET (fd, &rset);
  max_fds = fd + 1;

  while (1)
    {
      int ret = 0;

      FD_ZERO (&rset);
      FD_SET (fd, &rset);
      max_fds = fd + 1;

      ret = select (max_fds, &rset, NULL, NULL, NULL);

      if (ret == -1)
	{
	  perror ("Select Error");
	  break;
	}


      if (FD_ISSET (fd, &rset))
	{
	  hcq_cmd_t cmd = hcq_get_next_cmd (hcq);
	  uint64_t cmd_code = hcq_get_cmd_code (hcq, cmd);
	  uint32_t data_len = 0;
	  char *data_buf = hcq_get_cmd_data (hcq, cmd, &data_len);


	  printf ("cmd code=%llu\n", cmd_code);
	  if (data_len > 0)
	    {
	      printf ("len (%d) received\n", data_len);
	      //memcpy((void *)&nic_set_attr, sizeof(nic_set_attr), (void *)data_buf);        
	    }

	  switch (cmd_code)
	    {
	    case GNI_IOC_NIC_SETATTR:

	      nic_set_attr.modes = 0;

	      /* Configure NIC with ptag and other attributes */
	      nic_set_attr.cookie = alps_info.cookie;
	      nic_set_attr.ptag = alps_info.ptag;
	      nic_set_attr.rank = instance;

	      status = ioctl (device, GNI_IOC_NIC_SETATTR, &nic_set_attr);
	      if (status < 0)
		{
		  fprintf (stderr, "Failed to set NIC attributes (%d)\n",
			   status);
		  return 0;
		}

	      printf
		("Ioctl call GNI_IOC_NIC_SETATTR returned with nic_pe = 0x%x\n",
		 nic_set_attr.nic_pe);


	      /* configure FMA segments in XEMEM 
	         client_segid = xemem_make_signalled(NULL, 0, XPMEM_PERMIT_MODE, (void *)0600, NULL, &client_fd);
	         GNI_NIC_FMA_SET_DEDICATED(nic);
	         nic->fma_window = (void *) nic_attrs.fma_window;
	         nic->fma_window_nwc = (void *) nic_attrs.fma_window_nwc;
	         nic->fma_window_get = (void *) nic_attrs.fma_window_get;
	         nic->fma_ctrl = (void *) nic_attrs.fma_ctrl;
	       */

	      fma_win =
		xemem_make (nic_set_attr.fma_window, FMA_WINDOW_SIZE,
			    "fma_win_seg");
	      fma_put =
		xemem_make (nic_set_attr.fma_window_nwc, FMA_WINDOW_SIZE,
			    "fma_win_put");
	      fma_get =
		xemem_make (nic_set_attr.fma_window_get, FMA_WINDOW_SIZE,
			    "fma_win_get");
	      fma_ctrl =
		xemem_make (nic_set_attr.fma_ctrl, FMA_WINDOW_SIZE,
			    "fma_win_ctrl");

	      fprintf (stderr,
		       "created xemem segment for FMA window segid %llu\n",
		       fma_win);
	      fprintf (stderr,
		       "created xemem segment for FMA put segid %llu\n",
		       fma_put);
	      fprintf (stderr,
		       "created xemem segment for FMA get segid %llu\n",
		       fma_get);
	      fprintf (stderr,
		       "created xemem segment for FMA ctrl segid %llu\n",
		       fma_ctrl);
	      hcq_cmd_return (hcq, cmd, ret, sizeof (nic_set_attr),
			      &nic_set_attr);
	      break;
	    case GNI_IOC_MEM_REGISTER:

	      mem_register_attr = (gni_mem_register_args_t *) data_buf;
	      xemem_apid_t apid;
	      gni_mem_segment_t *segment;
	      int i;
	      if (mem_register_attr->segments_cnt == 1)
		{
		  /* one segment to be registered */
		  apid = xemem_get (mem_register_attr->address, XEMEM_RDWR);
		  if (apid <= 0)
		    {
		      printf ("could not attach user provided memreg \n");
		      return HCQ_INVALID_HANDLE;
		    }

		  r_addr.apid = apid;
		  r_addr.offset = 0;

		  reg_addr =
		    xemem_attach (r_addr, mem_register_attr->length, NULL);
		  if (reg_addr == MAP_FAILED)
		    {
		      printf ("xpmem attach for register failed\n");
		      xemem_release (apid);
		      return -1;
		    }

		  mem_register_attr->address = (uint64_t) reg_addr;
		}
	      else
		{
		  segment = mem_register_attr->mem_segments;
		}
//
	      printf ("mem register got address %p length %d \n",
		      reg_addr, mem_register_attr->length);
	      rc = ioctl (device, GNI_IOC_MEM_REGISTER, mem_register_attr);
	      if (rc < 0)
		{
		  fprintf (stderr, "Failed calling GNI_IOC_MEM_REGISTER\n");
		  return 0;
		}

	      printf ("Memory is registered\n");
	      printf
		("server after registration ioctl : qword1 = 0x%16lx qword2 = 0x%16lx\n",
		 mem_register_attr->mem_hndl.qword1, mem_register_attr->mem_hndl.qword2);
//
	      hcq_cmd_return (hcq, cmd, ret, sizeof (gni_mem_register_args_t),
			      mem_register_attr);
	      break;
	    case PMI_IOC_ALLGATHER:
	      outbuf = (char *) malloc (job_size * data_len);
	      assert (outbuf);
	      allgather (data_buf, outbuf, data_len);
	      hcq_cmd_return (hcq, cmd, ret, (job_size * data_len), outbuf);
	      break;
	    case PMI_IOC_GETRANK:
	      rc = PMI_Get_rank (&my_rank);
	      hcq_cmd_return (hcq, cmd, ret, 4, &my_rank);
	      break;
	    case PMI_IOC_GETSIZE:
	      rc = PMI_Get_size (&job_size);
	      hcq_cmd_return (hcq, cmd, ret, 4, &job_size);
	      fprintf (stderr, "server side after  get size return \n");
	      break;
	    case PMI_IOC_FINALIZE:
	      PMI_Finalize ();
	      hcq_cmd_return (hcq, cmd, ret, 0, NULL);
	      break;

	    default:
	      break;
	    }
	}

    }
  close (device);

  printf ("Deviced is closed\n");
  hcq_free_queue (hcq);
  hobbes_client_deinit ();

  return 0;

}
