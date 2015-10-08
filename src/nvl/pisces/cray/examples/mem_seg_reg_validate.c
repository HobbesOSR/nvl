#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <alps/libalpslli.h>
#include "gni_pub.h"
#define DATA_VAL 0xbeefbeefbeef
#define FLAG_VAL 0xf1a9
#define GARBAGE_VAL 0xbadbadbad
#define FORK_MASK 0xFFF0

#define IS_PAGE_BOUNDARY(addr, pagesize) !((pagesize - 1) & (unsigned long)addr)


#define NIC_ADDR_BITS    22
#define NIC_ADDR_SHIFT   (32-NIC_ADDR_BITS)
#define NIC_ADDR_MASK    0x3FFFFF
#define CPU_NUM_BITS     7
#define CPU_NUM_SHIFT    (NIC_ADDR_SHIFT-CPU_NUM_BITS)
#define CPU_NUM_MASK     0x7F
#define THREAD_NUM_BITS  3
#define THREAD_NUM_SHIFT (CPU_NUM_SHIFT-THREAD_NUM_BITS)
#define THREAD_NUM_MASK  0x7

#define GNI_INSTID(nic_addr, cpu_num, thr_num) (((nic_addr&NIC_ADDR_MASK)<<NIC_ADDR_SHIFT)|((cpu_num&CPU_NUM_MASK)<<CPU_NUM_SHIFT)|(thr_num&THREAD_NUM_MASK))
#define GNI_NIC_ADDRESS(inst_id)               ((inst_id>>NIC_ADDR_SHIFT)&NIC_ADDR_MASK)
#define GNI_CPU_NUMBER(inst_id)                ((inst_id>>CPU_NUM_SHIFT)&CPU_NUM_MASK)
#define GNI_THREAD_NUMBER(inst_id)             (inst_id&THREAD_NUM_MASK)


static uint32_t get_cpunum (void);
static void get_alps_info (alpsAppGni_t * alps_info);

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
      fprintf (stderr,
	       "waiting for ALPS reply bytes (%d) ; sizeof(alps_info)==%d ; sizeof(alps_info_list)==%d\n",
	       rep_size, sizeof (alps_info), sizeof (alps_info_list));
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
      fprintf (stderr,
	       "waiting for ALPS reply bytes (%d) ; sizeof(apid)==%d\n",
	       rep_size, sizeof (apid));
      alps_rc = alps_app_lli_get_response_bytes (&apid, rep_size);
      if (alps_rc != 0)
	fprintf (stderr, "alps_app_lli_get_response_bytes failed: %d\n",
		 alps_rc);
    }

  alps_app_lli_unlock ();

  memcpy (alps_info, (alpsAppGni_t *) alps_info_list->u.buf,
	  sizeof (alpsAppGni_t));

  fprintf (stderr, "apid                 =%llu\n", (unsigned long long) apid);
  fprintf (stderr, "alps_info->device_id =%llu\n",
	   (unsigned long long) alps_info->device_id);
  fprintf (stderr, "alps_info->local_addr=%lld\n",
	   (long long) alps_info->local_addr);
  fprintf (stderr, "alps_info->cookie    =%llu\n",
	   (unsigned long long) alps_info->cookie);
  fprintf (stderr, "alps_info->ptag      =%llu\n",
	   (unsigned long long) alps_info->ptag);

  fprintf (stderr,
	   "ALPS response - apid(%llu) alps_info->device_id(%llu) alps_info->local_addr(%llu) \n"
	   "alps_info->cookie(%llu) alps_info->ptag(%llu)\n",
	   (unsigned long long) apid,
	   (unsigned long long) alps_info->device_id,
	   (long long) alps_info->local_addr,
	   (unsigned long long) alps_info->cookie,
	   (unsigned long long) alps_info->ptag);

  return 0;

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

typedef struct
{
  gni_mem_handle_t mdh;
  uint64_t addr;
} mdh_addr_t;

typedef struct
{
  uint64_t **data_segs;
  gni_mem_handle_t mem_handle;
  gni_mem_segment_t *mem_segs;
  unsigned int seg_count;
  unsigned int reg_len;		/* in qwords */
  unsigned int dest_offset;	/* in qwords */
} seg_reg_t;


/* returns length as qwords */
unsigned int
recv_buff_len (seg_reg_t * regs, int num_regs)
{
  int i;
  uint64_t len = 0;

  for (i = 0; i < num_regs; i++)
    {
      len += regs[i].reg_len;
    }

  /* add room for flags */
  len += num_regs;

  return len;
}

/* seg_len is an integer representing a multiple of pagesize to allocate for segment blocks */
void
populate_registrations (seg_reg_t * regs, int num_regs, int seg_len,
			int num_segs)
{
  int i, j, k, alignment;
  unsigned int off = 1;
  unsigned int seg_len_int64;

  alignment = getpagesize ();

  seg_len_int64 = seg_len * alignment / sizeof (uint64_t);
  for (i = 0; i < num_regs; i++)
    {
      regs[i].seg_count = num_segs;
      regs[i].data_segs =
	(uint64_t **) malloc (num_segs * sizeof (uint64_t *));
      regs[i].mem_segs =
	(gni_mem_segment_t *) malloc (num_segs * sizeof (gni_mem_segment_t));
      regs[i].reg_len = num_segs * seg_len_int64;
      regs[i].dest_offset = off;
      for (j = 0; j < num_segs; j++)
	{
	  regs[i].mem_segs[j].length = seg_len * alignment;

	  assert (!posix_memalign
		  ((void *) &(regs[i].data_segs[j]), alignment,
		   regs[i].mem_segs[j].length));

	  assert (IS_PAGE_BOUNDARY (regs[i].data_segs[j], alignment));
	  regs[i].mem_segs[j].address = (uint64_t) regs[i].data_segs[j];

	  for (k = 0; k < seg_len_int64; k++)
	    regs[i].data_segs[j][k] = DATA_VAL + i + j + k;
	}
      off += regs[i].reg_len + 1;
    }

}

int
getRandInRange (int min, int max)
{
  return rand () % (max - min + 1) + min;
}

int
main (int argc, char **argv)
{
  int i, j, k, l, v,
    inst_id,
    test_id,
    hpid,
    nranks,
    device_id = 0,
    ptag,
    cookie, send_to, recv_from, num_scans, cdm_fork_mode = 0, fd = -1, rc;
  unsigned int local_addr,
    num_segs = 2,
    num_iters = 2,
    num_entries = 1024,
    rand_min,
    rand_max,
    num_regs = 2,
    recv_buffer_len, rand_seed, tmp_offset, seg_len = 2, events_returned;
  char *argPtr, *cqErrorStr;
  unsigned char unique_ptag = 0,
    fork_procs_pre_trans = 0,
    fork_procs_post_trans = 0, rand_registrations = 0;
  volatile uint64_t *flag_ptr = NULL;

  gni_mem_handle_t *mdh_data_flags;
  gni_mem_handle_t mdh_recv_buffer;

  uint64_t *flags;
  uint64_t *recv_buffer;
  uint32_t instance;

  gni_cdm_handle_t cdm_handle;
  gni_nic_handle_t nic_handle;
  gni_cq_handle_t cq_handle;
  gni_ep_handle_t *ep_handle_ary;
  gni_return_t status;

  gni_post_descriptor_t *data_descrips;
  gni_post_descriptor_t *flag_descrips;

  mdh_addr_t *rem_mdh_addr_ary;
  mdh_addr_t loc_mdh_addr;

  gni_post_descriptor_t *post_desc_ptr = NULL;
  gni_cq_entry_t event_data;

  seg_reg_t *registrations;


  device_id = 0;


  if (argc > 1)
    {
      alpsAppGni_t alps_info;

      uint32_t nic_addr = 0;
      uint32_t cpu_num = 0;
      uint32_t thread_num = 0;
      uint32_t gni_cpu_id = 0;
      int rc;
      char file_name[1024];
      FILE *fd;
      int retval;

      gni_cdm_handle_t cdm_hdl;	/* a client creates this comm domain with params from the server */
      gni_nic_handle_t nic_hdl;
      gni_ep_handle_t ep_hdl;
      gni_cq_handle_t cq_hdl;


      fprintf (stderr, "Just beginning  processing...\n");
      get_alps_info (&alps_info);



      rc = GNI_CdmGetNicAddress (alps_info.device_id, &nic_addr, &gni_cpu_id);

      cpu_num = get_cpunum ();

      instance = GNI_INSTID (nic_addr, cpu_num, thread_num);
      fprintf (stderr,
	       "nic_addr(%llu), cpu_num(%llu), thread_num(%llu), inst_id(%llu)\n",
	       (uint64_t) nic_addr, (uint64_t) cpu_num, (uint64_t) thread_num,
	       (uint64_t) instance);
      cpu_num = get_cpunum ();

      retval =
	fprintf (stderr,
		 "device %d  cookie %llu ptag %llu instance %llu local_addr %lld pid %d",
		 alps_info.device_id, (unsigned long long) alps_info.cookie,
		 (unsigned long long) alps_info.ptag,
		 (unsigned long long) instance,
		 (long long) alps_info.local_addr, getpid ());
      ptag = alps_info.ptag;
      cookie = alps_info.cookie;
    }
  else
    {
      fprintf (stderr, "Lib UGNI mem registration example\n");

      cookie = 100;
      ptag = 200;
      instance = 100;
    }

  fprintf (stderr, "after arg processing...\n");
  /* create and attach communication domain */
  status = GNI_CdmCreate (instance, ptag, cookie, cdm_fork_mode, &cdm_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stderr, "FAIL: GNI_CdmCreate returned error %d\n", status);
    }
  else
    {
      fprintf (stderr, "created CDM\n");
    }
  fprintf (stderr, "after cdm create ...\n");

  status = GNI_CdmAttach (cdm_handle, device_id, &local_addr, &nic_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stderr, "FAIL: GNI_CdmAttach returned error %d\n", status);
    }
  else
    {
      fprintf (stderr, "attached CDM to NIC\n");
    }


  /* main test loop */


  /* create data buffers */

  /* create recv buffer */
  size_t size = 10;
  size_t alignment = 4096;
  int error;

  error = posix_memalign (&recv_buffer, alignment, size);
  if (error != 0)
    {
      perror ("posix_memalign");
      exit (EXIT_FAILURE);
    }
  printf ("posix_memalign(%d, %d) = %p\n", alignment, size, recv_buffer);

  fprintf (stderr, "memory registration address %p, length %d\n",
	    recv_buffer, size * 4096);
  status =
    GNI_MemRegister (nic_handle, (uint64_t) recv_buffer, 4096 * size, NULL,
		     GNI_MEM_READWRITE, -1, &mdh_recv_buffer);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stderr,
	       "GNI_MemRegister returned error %d for recv_buffer\n", status);
    }
  else
    {
      fprintf (stderr,
	       "Rank %d: Plain Memregister registered recv_buffer\n",
	       inst_id);
    }

  fprintf (stderr, "test passed\n");

  return 0;
}
