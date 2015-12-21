/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

/*
 * CQWrite test example - this test only uses PMI
 */

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/utsname.h>
#include <errno.h>
#include "gni_pub.h"
#include "pmi.h"
#include "pmi_util.h"
#include "utility_functions.h"


int aborted = 0;
char *command_name;
int expected_passed = 0;
int failed = 0;
int passed = 0;

#define CACHELINE_MASK            0x3F	/* 64 byte cacheline */
#define NUMBER_OF_TRANSFERS       10
#define SEND_DATA                 0xee0000000000
#define TRANSFER_LENGTH           512
#define TRANSFER_LENGTH_IN_BYTES  4096

typedef struct
{
  gni_mem_handle_t mdh;
  uint64_t addr;
} mdh_addr_t;

int rank_id;
struct utsname uts_info;
int v_option = 0;


void
print_help (void)
{
  fprintf (stdout,
	   "\n"
	   "CQ_WRITE_PMI_EXAMPLE\n"
	   "  Purpose:\n"
	   "    The purpose of this example is to demonstrate the writing of a\n"
	   "    transaction to a remote completion queue.\n"
	   "\n"
	   "  APIs:\n"
	   "    This example will concentrate on using the following uGNI APIs:\n"
	   "      - GNI_PostCqWrite() is used to write an event to a completion queue.\n"
	   "      - GNI_CqGetEvent() is used to receive and process an event from a\n"
	   "        completion queue.\n"
	   "\n"
	   "  Parameters:\n"
	   "    Additional parameters for this example are:\n"
	   "      1.  '-h' prints the help information for this example.\n"
	   "      2.  '-n' specifies the number of messages that will be sent.\n"
	   "          The default value is 10 messages to transfer.\n"
	   "      3.  '-v', '-vv' or '-vvv' allows various levels of output or debug\n"
	   "          messages to be displayed.  With each additional 'v' more\n"
	   "          information will be displayed.\n"
	   "          The default value is no output or debug messages will be\n"
	   "          displayed.\n"
	   "\n"
	   "  Execution:\n"
	   "    The following is a list of suggested example executions with various\n"
	   "    options:\n" "      cq_write_pmi_example\n" "\n");
}

int
main (int argc, char **argv)
{
  unsigned int *all_nic_addresses;
  gni_cdm_handle_t cdm_handle;
  int cookie;
  gni_cq_handle_t cq_handle;
  gni_cq_entry_t current_event;
  uint64_t data;
  gni_post_descriptor_t *data_desc;
  gni_cq_handle_t destination_cq_handle;
  int device_id = 0;
  gni_ep_handle_t *endpoint_handles_array;
  uint32_t event_inst_id;
  gni_post_descriptor_t *event_post_desc_ptr;
  int events_returned;
  register int i;
  unsigned int local_address;
  mdh_addr_t memory_handle;
  int modes = 0;
  int my_id;
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
  uint64_t *receive_buffer;
  uint64_t receive_data;
  int receive_from;
  int first_spawned;
  unsigned int remote_address;
  gni_mem_handle_t remote_memory_handle;
  mdh_addr_t *remote_memory_handle_array;
  int send_to;
  gni_return_t status = GNI_RC_SUCCESS;
  char *text_pointer;
  uint32_t transfers = NUMBER_OF_TRANSFERS;
  uint32_t vmdh_index = -1;


  if ((i = uname (&uts_info)) != 0)
    {
      fprintf (stderr, "uname(2) failed, errno=%d\n", errno);
      exit (1);
    }

  /*
   * Get job attributes from PMI.
   */
  rc = PMI_Init (&first_spawned);
  assert (rc == PMI_SUCCESS);


  rc = PMI_Get_rank (&rank_id);
  assert (rc == PMI_SUCCESS);

  rc = PMI_Get_size (&number_of_ranks);
  assert (rc == PMI_SUCCESS);
  my_rank = rank_id;
  comm_size = number_of_ranks;;

  while ((opt = getopt (argc, argv, "hn:v")) != -1)
    {
      switch (opt)
	{
	case 'h':
	  if (rank_id == 0)
	    {
	      print_help ();
	    }

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

	case 'v':
	  v_option++;
	  break;

	case '?':
	  break;
	}
    }

  /*
   * Get job attributes from PMI. on server, now hardcode these
   */

  ptag = 10;
  cookie = 100;
  modes = 0;

  /*
   * Determine the number of passes required for this test to be successful.
   */

  expected_passed = transfers * 3;

  /*
   * Allocate the data_desc array.
   */

  data_desc = (gni_post_descriptor_t *) calloc (transfers,
						sizeof
						(gni_post_descriptor_t));
  assert (data_desc != NULL);

  /*
   * Create a handle to the communication domain.
   *    rank_id is the rank of the instance of the job.
   *    ptag is the protection tab for the job.
   *    cookie is a unique identifier created by the system.
   *    modes is a bit mask used to enable various flags.
   *    cdm_handle is the handle that is returned pointing to the
   *        communication domain.
   */

  status = GNI_CdmCreate (rank_id, ptag, cookie, modes, &cdm_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CdmCreate     ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      goto EXIT_TEST;
    }

  if (v_option > 1)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CdmCreate     with ptag %u cookie 0x%x\n",
	       uts_info.nodename, rank_id, ptag, cookie);
    }

  /*
   * Attach the communication domain handle to the NIC.
   *    cdm_handle is the handle pointing to the communication domain.
   *    device_id is the device identifier of the NIC that be attached to.
   *    local_address is the PE address that is returned for the
   *        communication domain that this NIC is attached to.
   *    nic_handle is the handle that is returned pointing to the NIC.
   */

  status = GNI_CdmAttach (cdm_handle, device_id, &local_address, &nic_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CdmAttach     ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      goto EXIT_DOMAIN;
    }

  if (v_option > 1)
    {
      fprintf (stdout, "[%s] Rank: %4i GNI_CdmAttach     to NIC\n",
	       uts_info.nodename, rank_id);
    }

  /*
   * Determine the minimum number of completion queue entries, which
   * is the number of outstanding transactions at one time.  For this
   * test, transfers entries are needed, since all the events
   * are sent before the completion queue is checked.
   */

  number_of_cq_entries = transfers;

  /*
   * Create the completion queue.
   *     nic_handle is the NIC handle that this completion queue will be
   *          associated with.
   *     number_of_cq_entries is the size of the completion queue.
   *     zero is the delay count is the number of allowed events before an
   *          interrupt is generated.
   *     GNI_CQ_NOBLOCK states that the operation mode is non-blocking.
   *     NULL states that no user supplied callback function is defined.
   *     NULL states that no user supplied pointer is passed to the callback
   *          function.
   *     cq_handle is the handle that is returned pointing to this newly
   *          created completion queue.
   */

  status =
    GNI_CqCreate (nic_handle, number_of_cq_entries, 0, GNI_CQ_NOBLOCK,
		  NULL, NULL, &cq_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CqCreate      sourceERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      goto EXIT_DOMAIN;
    }

  if (v_option > 1)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CqCreate      source with %i entries\n",
	       uts_info.nodename, rank_id, number_of_cq_entries);
    }

  /*
   * Determine the minimum number of completion queue entries, which is
   * the number of transactions outstanding at one time.  For this test,
   * since there are no barriers between transfers, the number of
   * outstanding transfers could be up to the transfers.
   */

  number_of_dest_cq_entries = transfers;

  status =
    GNI_CqCreate (nic_handle, number_of_dest_cq_entries, 0,
		  GNI_CQ_NOBLOCK, NULL, NULL, &destination_cq_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CqCreate      destination ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      goto EXIT_CQ;
    }

  if (v_option > 1)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CqCreate      destination with %i entries\n",
	       uts_info.nodename, rank_id, number_of_dest_cq_entries);
    }

  /*
   * Allocate the endpoint handles array.
   */

  endpoint_handles_array = (gni_ep_handle_t *) calloc (number_of_ranks,
						       sizeof
						       (gni_ep_handle_t));
  assert (endpoint_handles_array != NULL);

  /*
   * Get all of the NIC address for all of the ranks.
   */

  all_nic_addresses = (unsigned int *) gather_nic_addresses ();

  /*
   * Create the endpoints to all of the ranks.
   */

  for (i = 0; i < number_of_ranks; i++)
    {
      if (i == rank_id)
	{
	  continue;
	}

      /*
       * You must do an EpCreate for each endpoint pair.
       * That is for each remote node that you will want to communicate with.
       * The EpBind request updates some fields in the endpoint_handle so
       * this is the reason that all pairs of endpoints need to be created.
       *
       * Create the logical endpoint for each rank.
       *     nic_handle is our NIC handle.
       *     cq_handle is our completion queue handle.
       *     endpoint_handles_array will contain the handle that is returned
       *         for this endpoint instance.
       */

      status =
	GNI_EpCreate (nic_handle, cq_handle, &endpoint_handles_array[i]);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpCreate      ERROR remote rank: %4i status: %s (%d)\n",
		   uts_info.nodename, rank_id, i, gni_err_str[status],
		   status);
	  goto EXIT_ENDPOINT;
	}

      if (v_option > 1)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpCreate      remote rank: %i NIC: %p, CQ: %p, EP: %p\n",
		   uts_info.nodename, rank_id, i, nic_handle, cq_handle,
		   endpoint_handles_array[i]);
	}

      /*
       * Get the remote address to bind to.
       */

      remote_address = all_nic_addresses[i];

      /*
       * Bind the remote address to the endpoint handler.
       *     endpoint_handles_array is the endpoint handle that is being bound
       *     remote_address is the address that is being bound to this endpoint
       *         handler
       *     i is an unique user specified identifier for this bind.  In this
       *         test i refers to the instance id of the remote communication
       *         domain that we are binding to.
       */

      status = GNI_EpBind (endpoint_handles_array[i], remote_address, i);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpBind        ERROR remote rank: %4i status: %s (%d)\n",
		   uts_info.nodename, rank_id, i, gni_err_str[status],
		   status);
	  goto EXIT_ENDPOINT;
	}

      if (v_option > 1)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpBind        remote rank: %4i EP:  %p remote_address: %u, remote_id: %u\n",
		   uts_info.nodename, rank_id, i,
		   endpoint_handles_array[i], remote_address, i);
	}
    }

  /*
   * Allocate the buffer that will receive the data.
   */

  rc = posix_memalign ((void **) &receive_buffer, 4096, 4096);
  assert (rc == 0);

  /*
   * Initialize the buffer to all zeros.
   */

  memset (receive_buffer, 0, TRANSFER_LENGTH_IN_BYTES);

  /*
   * Register the memory associated for the receive buffer with the NIC.
   *     nic_handle is our NIC handle.
   *     receive_buffer is the memory location of the receive buffer.
   *     TRANSFER_LENGTH_IN_BYTES is the size of the memory allocated to the
   *         receive buffer.
   *     destination_cq_handle is the destination completion queue handle.
   *     GNI_MEM_READWRITE is the read/write attribute for the receive buffer's
   *         memory region.
   *     vmdh_index specifies the index within the allocated memory region,
   *         a value of -1 means that the GNI library will determine this index.
   *     remote_memory_handle is the handle for this memory region.
   */

  status = GNI_MemRegister (nic_handle, (uint64_t) receive_buffer,
			    TRANSFER_LENGTH_IN_BYTES,
			    destination_cq_handle,
			    GNI_MEM_READWRITE,
			    vmdh_index, &remote_memory_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_MemRegister   receive_buffer ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
      goto EXIT_ENDPOINT;
    }

  if (v_option > 1)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_MemRegister   receive_buffer  size: %u address: %p\n",
	       uts_info.nodename, rank_id,
	       (unsigned int) TRANSFER_LENGTH_IN_BYTES, receive_buffer);
    }

  /*
   * Allocate a buffer to contain all of the remote memory handle's.
   */


  remote_memory_handle_array =
    (mdh_addr_t *) malloc (number_of_ranks * sizeof (mdh_addr_t));
  assert (remote_memory_handle_array);

  memory_handle.addr = (uint64_t) receive_buffer;
  memory_handle.mdh = remote_memory_handle;

  /*
   * Gather up all of the remote memory handle's.
   * This also acts as a barrier to get all of the ranks to sync up.
   */

  gather (&memory_handle, remote_memory_handle_array, sizeof (mdh_addr_t));

  if ((v_option > 1) && (rank_id == 0))
    {
      fprintf (stdout,
	       "[%s] rank address     mdh.qword1            mdn.qword2\n",
	       uts_info.nodename);
      for (i = 0; i < number_of_ranks; i++)
	{
	  fprintf (stdout, "[%s] %4i 0x%lx    0x%016lx    0x%016lx\n",
		   uts_info.nodename, i,
		   remote_memory_handle_array[i].addr,
		   remote_memory_handle_array[i].mdh.qword1,
		   remote_memory_handle_array[i].mdh.qword2);
	}
    }

  if (v_option > 1)
    {

      /*
       * Write out all of the output messages.
       */

      fflush (stdout);
    }

  /*
   * Determine who we are going to send our data to and
   * who we are going to receive data from.
   */

  send_to = (rank_id + 1) % number_of_ranks;
  receive_from = (number_of_ranks + rank_id - 1) % number_of_ranks;
  my_receive_from = (receive_from & 0xffffff) << 20;
  my_id = (rank_id & 0xfffff) << 20;

  if (v_option > 2)
    {
      fprintf (stdout, "[%s] Rank: %4i sending destination CQ writes\n",
	       uts_info.nodename, rank_id);
    }

  for (i = 0; i < transfers; i++)
    {
      /*
       * Initialize the event to be sent.
       * The source data will look like: 0xeelllllttttt
       *     where: ee is the actual value
       *            lllll is the rank for this process
       *            ttttt is the transfer number
       */

      data = SEND_DATA + my_id + i + 1;

      /*
       * Detemine what the received event will look like.
       * The received data will look like: 0xeerrrrrttttt
       *     where: ee is the actual value
       *            rrrrr is the rank of the remote process,
       *                   that is sending to this process
       *            ttttt is the transfer number
       */

      receive_data = SEND_DATA + my_receive_from + i + 1;

      /*
       * Setup the data request.
       * Send with the request with hashing adaption disabled so that the
       * complete queue data will arrive in order at target node.
       */

      data_desc[i].type = GNI_POST_CQWRITE;
      data_desc[i].cq_mode = GNI_CQMODE_GLOBAL_EVENT;
      data_desc[i].dlvr_mode = GNI_DLVMODE_NO_ADAPT;
      data_desc[i].cqwrite_value = data;
      data_desc[i].remote_mem_hndl = remote_memory_handle_array[send_to].mdh;

      if (v_option)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_PostCqWrite   transfer: %4i send_to:   %4i message: 0x%16.16lx\n",
		   uts_info.nodename, rank_id, (i + 1), send_to, data);
	}

      /*
       * Send the transaction.
       */

      status =
	GNI_PostCqWrite (endpoint_handles_array[send_to], &data_desc[i]);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_PostCqWrite   ERROR status: %s (%d)\n",
		   uts_info.nodename, rank_id, gni_err_str[status], status);
	  goto EXIT_MEMORY_REMOTE;
	}


      if (v_option > 2)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_PostCqWrite   transfer: %4i successful\n",
		   uts_info.nodename, rank_id, (i + 1));
	}

      if (v_option > 2)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i Wait for destination completion queue messages recv from: %4i\n",
		   uts_info.nodename, rank_id, receive_from);
	}

      /*
       * Check the completion queue to verify that the data has
       * been received.  The destination completion queue needs to be
       * checked and events to be removed so that it does not become full
       * and cause succeeding events to be lost.
       */

      rc =
	get_cq_event (destination_cq_handle, uts_info, rank_id, 0, 1,
		      &current_event);
      if (rc == 0)
	{

	  /*
	   * An event was received.
	   */

	  if (GNI_CQ_GET_DATA (current_event) != receive_data)
	    {

	      /*
	       * The event's data was not the expected data value.
	       */

	      fprintf (stdout,
		       "[%s] Rank: %4i CQ Event ERROR erroneous CQ value detected, recv_data: 0x%16.16lx, expected_data: 0x%16.16lx\n",
		       uts_info.nodename, rank_id,
		       GNI_CQ_GET_DATA (current_event), receive_data);
	    }
	  else
	    {
	    }
	}
      else
	{

	  /*
	   * An error occurred while receiving the event.
	   */

	}
    }				/* end of for loop for transfers */

  if (v_option > 1)
    {

      /*
       * Write out all of the output messages.
       */

      fflush (stdout);
    }

  /*
   * Get all of the completion queue events back from the transfers.
   */

  events_returned = 0;

  if (v_option > 2)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i message transfers complete, checking local CQ events\n",
	       uts_info.nodename, rank_id);
    }

  while (events_returned < transfers)
    {

      /*
       * Check the completion queue to verify that the message request has
       * been sent.  The source completion queue needs to be checked and
       * events to be removed so that it does not become full and cause
       * succeeding calls to PostCqWrite to fail.
       */

      rc = get_cq_event (cq_handle, uts_info, rank_id, 1, 1, &current_event);
      if (rc == 0)
	{

	  /*
	   * An event was received.
	   *
	   * Complete the event, which removes the current event's post
	   * descriptor from the event queue.
	   */

	  status =
	    GNI_GetCompleted (cq_handle, current_event, &event_post_desc_ptr);
	  if (status != GNI_RC_SUCCESS)
	    {
	      fprintf (stdout,
		       "[%s] Rank: %4i GNI_GetCompleted  ERROR status: %s (%d)\n",
		       uts_info.nodename, rank_id, gni_err_str[status],
		       status);

	      break;
	    }

	  /*
	   * Validate the current event's instance id with the expected id.
	   */

	  event_inst_id = GNI_CQ_GET_INST_ID (current_event);
	  if (event_inst_id != send_to)
	    {

	      /*
	       * The event's inst_id was not the expected inst_id
	       * value.
	       */

	      fprintf (stdout,
		       "[%s] Rank: %4i CQ Event ERROR received inst_id: %d, expected inst_id: %d in event_data\n",
		       uts_info.nodename, rank_id, event_inst_id, send_to);

	    }
	  else
	    {

	      events_returned++;
	    }
	}
      else
	{

	  /*
	   * An error occurred while receiving the event.
	   */

	  break;
	}
    }

  if (v_option > 2)
    {
      if (events_returned == transfers)
	{
	  fprintf (stdout, "[%s] Rank: %4i All CQ events received\n",
		   uts_info.nodename, rank_id);
	  fflush (stdout);
	}
    }

  /*
   * Wait for all the processes to finish before we clean up and exit.
   */

  rc = PMI_Barrier ();
  assert (rc == PMI_SUCCESS);

  /*
   * Free allocated memory.
   */

  free (remote_memory_handle_array);


EXIT_MEMORY_REMOTE:

  /*
   * Deregister the memory associated for the receive buffer with the NIC.
   *     nic_handle is our NIC handle.
   *     remote_memory_handle is the handle for this memory region.
   */

  status = GNI_MemDeregister (nic_handle, &remote_memory_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_MemDeregister receive_buffer ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
    }
  else
    {
      if (v_option > 1)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_MemDeregister receive_buffer    NIC: %p\n",
		   uts_info.nodename, rank_id, nic_handle);
	}

      /*
       * Free allocated memory.
       */

      free (receive_buffer);
    }

EXIT_ENDPOINT:

  /*
   * Remove the endpoints to all of the ranks.
   *
   * Note: if there are outstanding events in the completion queue,
   *       the endpoint can not be unbound.
   */

  for (i = 0; i < number_of_ranks; i++)
    {
      if (i == rank_id)
	{
	  continue;
	}

      if (endpoint_handles_array[i] == 0)
	{

	  /*
	   * This endpoint does not exist.
	   */

	  continue;
	}

      /*
       * Unbind the remote address from the endpoint handler.
       *     endpoint_handles_array is the endpoint handle that is being unbound
       */

      status = GNI_EpUnbind (endpoint_handles_array[i]);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpUnbind      ERROR remote rank: %4i status: %s (%d)\n",
		   uts_info.nodename, rank_id, i, gni_err_str[status],
		   status);
	  continue;
	}

      if (v_option > 1)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpUnbind      remote rank: %4i EP:  %p\n",
		   uts_info.nodename, rank_id, i, endpoint_handles_array[i]);
	}

      /*
       * You must do an EpDestroy for each endpoint pair.
       *
       * Destroy the logical endpoint for each rank.
       *     endpoint_handles_array is the endpoint handle that is being
       *         destroyed.
       */

      status = GNI_EpDestroy (endpoint_handles_array[i]);
      if (status != GNI_RC_SUCCESS)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpDestroy     ERROR remote rank: %4i status: %s (%d)\n",
		   uts_info.nodename, rank_id, i, gni_err_str[status],
		   status);
	  continue;
	}

      if (v_option > 1)
	{
	  fprintf (stdout,
		   "[%s] Rank: %4i GNI_EpDestroy     remote rank: %4i EP:  %p\n",
		   uts_info.nodename, rank_id, i, endpoint_handles_array[i]);
	}
    }

  /*
   * Free allocated memory.
   */

  free (endpoint_handles_array);

  /*
   * Destroy the destination completion queue.
   *     destination_cq_handle is the handle that is being destroyed.
   */

  status = GNI_CqDestroy (destination_cq_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CqDestroy     destination ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
    }
  else if (v_option > 1)
    {
      fprintf (stdout, "[%s] Rank: %4i GNI_CqDestroy     destination\n",
	       uts_info.nodename, rank_id);
    }

EXIT_CQ:

  /*
   * Destroy the completion queue.
   *     cq_handle is the handle that is being destroyed.
   */

  status = GNI_CqDestroy (cq_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CqDestroy     source ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
    }
  else if (v_option > 1)
    {
      fprintf (stdout, "[%s] Rank: %4i GNI_CqDestroy     source\n",
	       uts_info.nodename, rank_id);
    }

EXIT_DOMAIN:

  /*
   * Clean up the communication domain handle.
   */

  status = GNI_CdmDestroy (cdm_handle);
  if (status != GNI_RC_SUCCESS)
    {
      fprintf (stdout,
	       "[%s] Rank: %4i GNI_CdmDestroy     ERROR status: %s (%d)\n",
	       uts_info.nodename, rank_id, gni_err_str[status], status);
    }
  else if (v_option > 1)
    {
      fprintf (stdout, "[%s] Rank: %4i GNI_CdmDestroy\n",
	       uts_info.nodename, rank_id);
    }

EXIT_TEST:

  /*
   * Free allocated memory.
   */

  free (data_desc);

  /*
   * Display the results from this test.
   */


  /*
   * Clean up the PMI information.
   */

  PMI_Finalize ();

  return rc;
}
