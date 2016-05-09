/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

/*
 * RDMA Put test example - this test only uses PMI
 *
 * Note: this test should not be run oversubscribed on nodes, i.e. more
 * instances on a given node than cpus, owing to the busy wait for
 * incoming data.
 */

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <malloc.h>
#include <sched.h>
#include <sys/utsname.h>
#include <errno.h>
#include "gni_pub.h"
#include "pmi.h"

#define BIND_ID_MULTIPLIER       100
#define CACHELINE_MASK           0x3F	/* 64 byte cacheline */
#define CDM_ID_MULTIPLIER        1000
#define LOCAL_EVENT_ID_BASE      10000000
#define NUMBER_OF_TRANSFERS      100
#define POST_ID_MULTIPLIER       1000
#define REMOTE_EVENT_ID_BASE     11000000
#define SEND_DATA                0xdddd000000000000
#define TRANSFER_LENGTH          (1<<22)

typedef struct
{
  gni_mem_handle_t mdh;
  uint64_t addr;
} mdh_addr_t;

int compare_data_failed = 0;
int rank_id;
struct utsname uts_info;
int v_option = 0;

#include "utility_functions.h"

uint64_t
now (void)
{
  int rc;
  struct timeval tv;

  rc = gettimeofday (&tv, NULL);
  if (rc != 0)
    abort ();

  return (tv.tv_sec * 1e6) + tv.tv_usec;
}


int
main (int argc, char **argv)
{
  unsigned int *all_nic_addresses;
  uint32_t bind_id;
  gni_cdm_handle_t cdm_handle;
  uint32_t cdm_id;
  int cookie;
  gni_cq_handle_t cq_handle;
  int create_destination_cq = 1;
  int create_destination_overrun = 0;
  gni_cq_entry_t current_event;
  uint64_t data = SEND_DATA;
  int data_transfers_sent = 0;
  int data_transfers_recvd = 0;
  gni_cq_handle_t destination_cq_handle = NULL;
  int device_id = 0;
  gni_ep_handle_t *endpoint_handles_array;
  uint32_t event_inst_id;
  gni_post_descriptor_t *event_post_desc_ptr;
  uint32_t expected_local_event_id;
  uint32_t expected_remote_event_id;
  int first_spawned;
  int i;
  int j;
  unsigned int local_address;
  uint32_t local_event_id;
  int modes = GNI_CDM_MODE_BTE_SINGLE_CHANNEL;
  int my_id;
  mdh_addr_t my_memory_handle;
  int my_receive_from;
  gni_nic_handle_t nic_handle;
  int number_of_cq_entries;
  int number_of_dest_cq_entries;
  int number_of_ranks;
  char opt;
  extern char *optarg;
  extern int optopt;
  uint8_t ptag;
  int rc;
  gni_post_descriptor_t *rdma_data_desc;
  uint64_t *receive_buffer;
  uint64_t receive_data = SEND_DATA;
  int receive_from;
  gni_mem_handle_t receive_memory_handle;
  unsigned int remote_address;
  uint32_t remote_event_id;
  mdh_addr_t *remote_memory_handle_array;
  uint64_t *send_buffer;
  uint64_t send_post_id;
  int send_to;
  gni_mem_handle_t source_memory_handle;
  gni_return_t status = GNI_RC_SUCCESS;
  char *text_pointer;
  uint32_t transfers = NUMBER_OF_TRANSFERS;
  uint64_t transfer_size;
  int use_event_id = 0;
  uint64_t t0, t1, elapsed;
  double speed;
  int size;


  /*
   * Get job attributes from PMI.
   */

  rc = PMI_Init (&first_spawned);
  assert (rc == PMI_SUCCESS);

  rc = PMI_Get_size (&number_of_ranks);
  assert (rc == PMI_SUCCESS);

  rc = PMI_Get_rank (&rank_id);
  assert (rc == PMI_SUCCESS);

  local_event_id = rank_id;

  while ((opt = getopt (argc, argv, "Dehn:Ov")) != -1)
    {
      switch (opt)
	{
	case 'D':
	  /* Do not create a destination completion queue. */

	  if (create_destination_overrun == 0)
	    {
	      create_destination_cq = 0;
	    }
	  break;

	case 'e':
	  use_event_id = 1;
	  break;

	case 'h':

	  /*
	   * Clean up the PMI information.
	   */

	  PMI_Finalize ();

	  exit (0);

	case 'n':

	  /*
	   * Set the number of messages that will be sent to the
	   * shared message queue.
	   */

	  transfers = atoi (optarg);
	  if (transfers < 1)
	    {
	      transfers = NUMBER_OF_TRANSFERS;
	    }

	  break;

	case 'O':
	  /*
	   * Create the destination completion queue with a very
	   * small number of entries to create an overrun condition.
	   */

	  create_destination_overrun = 1;
	  create_destination_cq = 1;
	  break;

	case 'v':
	  v_option++;
	  break;

	case '?':
	  break;
	}
    }

  ptag = get_ptag ();
  cookie = get_cookie ();

  rdma_data_desc = (gni_post_descriptor_t *) calloc (transfers,
						     sizeof
						     (gni_post_descriptor_t));
  assert (rdma_data_desc != NULL);
  cdm_id = rank_id * CDM_ID_MULTIPLIER;
  status = GNI_CdmCreate (cdm_id, ptag, cookie, modes, &cdm_handle);

  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CdmCreate     ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      INCREMENT_ABORTED;
      goto EXIT_TEST;
    }

  status = GNI_CdmAttach (cdm_handle, device_id, &local_address, &nic_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CdmAttach     ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      INCREMENT_ABORTED;
      goto EXIT_DOMAIN;
    }

  number_of_cq_entries = transfers;

  status =
    GNI_CqCreate (nic_handle, number_of_cq_entries, 0, GNI_CQ_NOBLOCK,
		  NULL, NULL, &cq_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CqCreate      source ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      INCREMENT_ABORTED;
      goto EXIT_DOMAIN;
    }

  if (create_destination_cq != 0)
    {
      if (create_destination_overrun == 0)
	{
	  number_of_dest_cq_entries = transfers * 2;
	}
      else
	{
	  number_of_dest_cq_entries = 1;
	}

      status =
	GNI_CqCreate (nic_handle, number_of_dest_cq_entries, 0,
		      GNI_CQ_NOBLOCK, NULL, NULL, &destination_cq_handle);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_CqCreate      destination ERROR status: %s (%d)\n",
		   uts_info.nodename, rank_id, gni_err_str[status], status);
	  INCREMENT_ABORTED;
	  goto EXIT_CQ;
	}

    }

  endpoint_handles_array = (gni_ep_handle_t *) calloc (number_of_ranks,
						       sizeof
						       (gni_ep_handle_t));
  assert (endpoint_handles_array != NULL);

  all_nic_addresses = (unsigned int *) gather_nic_addresses ();

  for (i = 0; i < number_of_ranks; i++)
    {
      if (i == rank_id)
	{
	  continue;
	}

      status =
	GNI_EpCreate (nic_handle, cq_handle, &endpoint_handles_array[i]);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpCreate      ERROR status: %s (%d)\n",
		   uts_info.nodename, rank_id, gni_err_str[status], status);
	  INCREMENT_ABORTED;
	  goto EXIT_ENDPOINT;
	}

      remote_address = all_nic_addresses[i];
      bind_id = (rank_id * BIND_ID_MULTIPLIER) + i;

      status =
	GNI_EpBind (endpoint_handles_array[i], remote_address, bind_id);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpBind        ERROR status: %s (%d)\n",
		   uts_info.nodename, rank_id, gni_err_str[status], status);
	  INCREMENT_ABORTED;
	  goto EXIT_ENDPOINT;
	}

      if (use_event_id == 1)
	{
	  local_event_id = LOCAL_EVENT_ID_BASE + cdm_id + bind_id;
	  remote_event_id = REMOTE_EVENT_ID_BASE + cdm_id + bind_id;

	  status =
	    GNI_EpSetEventData (endpoint_handles_array[i], local_event_id,
				remote_event_id);
	  if (status != GNI_RC_SUCCESS)
	    {
	      fprintf (stdout,
		       "[%s] Rank: %4i GNI_EpSetEventData ERROR remote rank: %4i status: %d\n",
		       uts_info.nodename, rank_id, i, status);
	      INCREMENT_ABORTED;
	      goto EXIT_ENDPOINT;
	    }

	}
    }

  rc = posix_memalign ((void **) &send_buffer, 4096, TRANSFER_LENGTH);
  assert (rc == 0);

  memset (send_buffer, 0, TRANSFER_LENGTH);

  status = GNI_MemRegister (nic_handle, (uint64_t) send_buffer,
			    TRANSFER_LENGTH, NULL,
			    GNI_MEM_READWRITE, -1, &source_memory_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_MemRegister  send_buffer ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      INCREMENT_ABORTED;
    }

  rc = posix_memalign ((void **) &receive_buffer, 4096, TRANSFER_LENGTH);
  assert (rc == 0);

  memset (receive_buffer, 0, TRANSFER_LENGTH);

  status = GNI_MemRegister (nic_handle, (uint64_t) receive_buffer,
			    TRANSFER_LENGTH,
			    destination_cq_handle,
			    GNI_MEM_READWRITE, -1, &receive_memory_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_MemRegister   receive_buffer ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      INCREMENT_ABORTED;
      goto EXIT_MEMORY_SOURCE;
    }

  remote_memory_handle_array =
    (mdh_addr_t *) calloc (number_of_ranks, sizeof (mdh_addr_t));
  assert (remote_memory_handle_array);

  my_memory_handle.addr = (uint64_t) receive_buffer;
  my_memory_handle.mdh = receive_memory_handle;

  allgather (&my_memory_handle, remote_memory_handle_array,
	     sizeof (mdh_addr_t));


  send_to = (rank_id + 1) % number_of_ranks;
  receive_from = (number_of_ranks + rank_id - 1) % number_of_ranks;
  my_receive_from = (receive_from & 0xffffff) << 24;
  my_id = (rank_id & 0xffffff) << 24;

  if (use_event_id == 1)
    {
      expected_local_event_id = LOCAL_EVENT_ID_BASE + cdm_id
	+ (BIND_ID_MULTIPLIER * rank_id) + send_to;
      expected_remote_event_id = REMOTE_EVENT_ID_BASE
	+ (CDM_ID_MULTIPLIER * receive_from)
	+ (BIND_ID_MULTIPLIER * receive_from) + rank_id;
    }
  else
    {
      expected_local_event_id = (rank_id * BIND_ID_MULTIPLIER) + send_to;
      expected_remote_event_id = CDM_ID_MULTIPLIER * receive_from;
    }
  rdma_data_desc[0].type = GNI_POST_RDMA_PUT;
  if (create_destination_cq != 0)
    {
      rdma_data_desc[0].cq_mode = GNI_CQMODE_GLOBAL_EVENT |
	GNI_CQMODE_REMOTE_EVENT;
    }
  else
    {
      rdma_data_desc[0].cq_mode = GNI_CQMODE_GLOBAL_EVENT;
    }
  rdma_data_desc[0].dlvr_mode = GNI_DLVMODE_PERFORMANCE;
  rdma_data_desc[0].local_addr = (uint64_t) send_buffer;
  rdma_data_desc[0].local_mem_hndl = source_memory_handle;
  rdma_data_desc[0].remote_addr = remote_memory_handle_array[send_to].addr;
  rdma_data_desc[0].remote_mem_hndl = remote_memory_handle_array[send_to].mdh;
  rdma_data_desc[0].rdma_mode = 0;
  rdma_data_desc[0].src_cq_hndl = cq_handle;
  for (size = 8; size <= TRANSFER_LENGTH; size *= 2)
    {
      if (rank_id == 0)
	{
	  transfer_size = size;
	  elapsed = t1 = t0 = 0;
	  t0 = now ();
	  for (i = 0; i < transfers; i++)
	    {
	      send_post_id =
		((uint64_t) expected_local_event_id * POST_ID_MULTIPLIER) +
		i + 1;
	      rdma_data_desc[0].post_id = send_post_id;
	      rdma_data_desc[0].length = size;

	      status =
		GNI_PostRdma (endpoint_handles_array[send_to],
			      &rdma_data_desc[0]);
	      if (status != GNI_RC_SUCCESS)
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_PostRdma      data ERROR status: %s (%d)\n",
			   uts_info.nodename, rank_id, gni_err_str[status],
			   status);
		  INCREMENT_FAILED;
		  continue;
		}
	      data_transfers_sent++;
	      rc =
		get_cq_event (cq_handle, uts_info, rank_id, 1, 1,
			      &current_event);
	      if (rc == 0)
		{
		  status =
		    GNI_GetCompleted (cq_handle, current_event,
				      &event_post_desc_ptr);
		  if (status != GNI_RC_SUCCESS)
		    {
		      fprintf (stdout,
			       "[%s] Rank: %4i GNI_GetCompleted  data ERROR status: %s (%d)\n",
			       uts_info.nodename, rank_id,
			       gni_err_str[status], status);

		      INCREMENT_FAILED;
		      fflush (stdout);
		      return -1;
		    }
		}
	    }			/* sent 100 RDMA PUT, get pong */
	  if (create_destination_cq != 0)
	    {

	      rc = get_cq_event (destination_cq_handle, uts_info,
				 rank_id, 0, 1, &current_event);
	      if (rc == 0)
		{
		  INCREMENT_PASSED;
		}
	      else
		{

		  INCREMENT_FAILED;
		  return -1;
		}

	      fflush (stdout);

	    }
	  /* got pong */

	  t1 = now ();
	  elapsed = t1 - t0;
	  speed = (transfer_size * NUMBER_OF_TRANSFERS) * 1e6 / elapsed;	/* send & receive */
	  fprintf (stdout,
		   "rank: %d  bytes :%llu time delta(microsec) =%llu bandwidth : %6.2lf  \n",
		   rank_id, transfer_size, elapsed / NUMBER_OF_TRANSFERS,
		   speed);
	}
      else if (rank_id == 1)
	{
	  data_transfers_recvd = 0;
	  for (i = 0; i < transfers; i++)
	    {

	      rc = get_cq_event (destination_cq_handle, uts_info,
				 rank_id, 0, 1, &current_event);
	      if (rc == 0)
		{
		  data_transfers_recvd++;
		  INCREMENT_PASSED;
		}
	      else
		{
		  INCREMENT_FAILED;
		  return (-1);
		}

	      fflush (stdout);

	    }
	  /* Send a pong after loop cnt recv */
	  if (data_transfers_recvd == transfers)
	    {
	      send_post_id =
		((uint64_t) expected_local_event_id * POST_ID_MULTIPLIER) +
		0 + 1;

	      rdma_data_desc[0].length = 4;
	      rdma_data_desc[0].post_id = send_post_id;
	      status =
		GNI_PostRdma (endpoint_handles_array[send_to],
			      &rdma_data_desc[0]);
	      if (status != GNI_RC_SUCCESS)
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_PostRdma      data ERROR status: %s (%d)\n",
			   uts_info.nodename, rank_id, gni_err_str[status],
			   status);
		  INCREMENT_FAILED;
		  return -1;
		}

	    }
	}
    }

EXIT_WAIT_BARRIER:

  rc = PMI_Barrier ();
  assert (rc == PMI_SUCCESS);
  free (remote_memory_handle_array);
  status = GNI_MemDeregister (nic_handle, &receive_memory_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_MemDeregister receive_buffer ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      INCREMENT_ABORTED;
    }
  else
    {
      free (receive_buffer);
    }

EXIT_MEMORY_SOURCE:


  status = GNI_MemDeregister (nic_handle, &source_memory_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_MemDeregister send_buffer ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      INCREMENT_ABORTED;
    }
  else
    {
      free (send_buffer);
    }

EXIT_ENDPOINT:

  for (i = 0; i < number_of_ranks; i++)
    {
      if (i == rank_id)
	{
	  continue;
	}

      if (endpoint_handles_array[i] == 0)
	{
	  continue;
	}


      status = GNI_EpUnbind (endpoint_handles_array[i]);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpUnbind      ERROR remote rank: %4i status: %s (%d)\n",
		   uts_info.nodename, rank_id, i, gni_err_str[status],
		   status);
	  continue;
	}

      status = GNI_EpDestroy (endpoint_handles_array[i]);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpDestroy     ERROR remote rank: %4i status: %s (%d)\n",
		   uts_info.nodename, rank_id, i, gni_err_str[status],
		   status);
	  continue;
	}

    }

  free (endpoint_handles_array);

  if (create_destination_cq != 0)
    {

      status = GNI_CqDestroy (destination_cq_handle);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_CqDestroy     destination ERROR status: %s (%d)\n",
		   uts_info.nodename, rank_id, gni_err_str[status], status);
	}
    }

EXIT_CQ:


  status = GNI_CqDestroy (cq_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CqDestroy     source ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
    }

EXIT_DOMAIN:


  status = GNI_CdmDestroy (cdm_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CdmDestroy    ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
    }

EXIT_TEST:



  free (rdma_data_desc);

  PMI_Finalize ();

  return rc;
}
