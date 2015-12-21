/*
 * This header file contains the common utility functions.
 */

/*
 * allgather gather the requested information from all of the ranks.
 */
int pmi_fd, my_rank, comm_size;

void gather (void *in, void *out, int len);

/*
 * get_gni_nic_address get the nic address for the specified device.
 *
 *   Returns: the nic address for the specified device.
 */

unsigned int get_gni_nic_address (int device_id);

/*
 * gather_nic_addresses gather all of the nic addresses for all of the
 *                      other ranks.
 *
 *   Returns: an array of addresses for all of the nics from all of the
 *            other ranks.
 */

void *gather_nic_addresses (void);


/*
 * get_cq_event will process events from the completion queue.
 *
 *   cq_handle is the completion queue handle.
 *   uts_info contains the node name.
 *   rank_id is the rank of this process.
 *   source_cq determines if the CQ is a source or a
 *       destination completion queue. 
 *   retry determines if GNI_CqGetEvent should be called multiple
 *       times or only once to get an event.
 *
 *   Returns:  gni_cq_entry_t for success
 *             0 on success
 *             1 on an error
 *             2 on an OVERRUN error
 *             3 on no event found error
 */

int
get_cq_event (gni_cq_handle_t cq_handle, struct utsname uts_info,
	      int rank_id, unsigned int source_cq, unsigned int retry,
	      gni_cq_entry_t * next_event);

inline int print_results (void);



unsigned int
get_gni_nic_address (int device_id)
{
  return my_rank;		/* We know that actual GNI_CdmGetNicAddress  returns from file GHAL_CLDEV_ADDR_FMT */

}


/*
 * gather_nic_addresses gather all of the nic addresses for all of the
 *                      other ranks.
 *
 *   Returns: an array of addresses for all of the nics from all of the
 *            other ranks.
 */

void *
gather_nic_addresses (void)
{
  size_t addr_len;
  unsigned int *all_addrs;
  unsigned int local_addr;
  int rc;

  /*
   * Get the size of the process group.  We will have it in a global
   */


  local_addr = get_gni_nic_address (0);

  addr_len = sizeof (unsigned int);

  /*
   * Allocate a buffer to hold the nic address from all of the other
   * ranks.
   */

  all_addrs = (unsigned int *) malloc (addr_len * comm_size);
  assert (all_addrs != NULL);

  /*
   * Get the nic addresses from all of the other ranks.
   */

  gather (&local_addr, all_addrs, sizeof (int));

  return (void *) all_addrs;
}

/*
 * get_cq_event will process events from the completion queue.
 *
 *   cq_handle is the completion queue handle.
 *   uts_info contains the node name.
 *   rank_id is the rank of this process.
 *   source_cq determines if the CQ is a source or a
 *       destination completion queue. 
 *   retry determines if GNI_CqGetEvent should be called multiple
 *       times or only once to get an event.
 *
 *   Returns:  gni_cq_entry_t for success
 *             0 on success
 *             1 on an error
 *             2 on an OVERRUN error
 *             3 on no event found error
 */

int
get_cq_event (gni_cq_handle_t cq_handle, struct utsname uts_info,
	      int rank_id, unsigned int source_cq, unsigned int retry,
	      gni_cq_entry_t * next_event)
{
  gni_cq_entry_t event_data = 0;
  uint64_t event_type;
  gni_return_t status = GNI_RC_SUCCESS;
  int wait_count = 0;

  status = GNI_RC_NOT_DONE;
  while (status == GNI_RC_NOT_DONE)
    {

      /*
       * Get the next event from the specified completion queue handle.
       */

      status = GNI_CqGetEvent (cq_handle, &event_data);
      if (status == GNI_RC_SUCCESS)
	{
	  *next_event = event_data;

	  /*
	   * Processed event succesfully.
	   */

	  event_type = GNI_CQ_GET_TYPE (event_data);

	  if (event_type == GNI_CQ_EVENT_TYPE_POST)
	    {
	      if (source_cq == 1)
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    source      type: POST(%lu) inst_id: %lu tid: %lu event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id,
			   event_type,
			   GNI_CQ_GET_INST_ID (event_data),
			   GNI_CQ_GET_TID (event_data), event_data);
		}
	      else
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    destination type: POST(%lu) inst_id: %lu event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id,
			   event_type,
			   GNI_CQ_GET_INST_ID (event_data), event_data);
		}
	    }
	  else if (event_type == GNI_CQ_EVENT_TYPE_SMSG)
	    {
	      if (source_cq == 1)
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    source      type: SMSG(%lu) msg_id: 0x%8.8x event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id,
			   event_type,
			   (unsigned int) GNI_CQ_GET_MSG_ID (event_data),
			   event_data);
		}
	      else
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    destination type: SMSG(%lu) data: 0x%16.16lx event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id,
			   event_type,
			   GNI_CQ_GET_DATA (event_data), event_data);
		}
	    }
	  else if (event_type == GNI_CQ_EVENT_TYPE_MSGQ)
	    {
	      if (source_cq == 1)
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    source      type: MSGQ(%lu) msg_id: 0x%8.8x event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id,
			   event_type,
			   (unsigned int) GNI_CQ_GET_MSG_ID (event_data),
			   event_data);
		}
	      else
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    destination type: MSGQ(%lu) data: 0x%16.16lx event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id,
			   event_type,
			   GNI_CQ_GET_DATA (event_data), event_data);
		}
	    }
	  else
	    {
	      if (source_cq == 1)
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    source      type: %lu inst_id: %lu event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id,
			   event_type,
			   GNI_CQ_GET_DATA (event_data), event_data);
		}
	      else
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    destination type: %lu data: 0x%16.16lx event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id,
			   event_type,
			   GNI_CQ_GET_DATA (event_data), event_data);
		}
	    }

	  return 0;
	}
      else if (status != GNI_RC_NOT_DONE)
	{
	  int error_code = 1;
/*
             * An error occurred getting the event.
             */

	  char *cqErrorStr;
	  char *cqOverrunErrorStr = "";
	  gni_return_t tmp_status = GNI_RC_SUCCESS;
#ifdef CRAY_CONFIG_GHAL_ARIES
	  uint32_t status_code;

	  status_code = GNI_CQ_GET_STATUS (event_data);
	  if (status_code == A_STATUS_AT_PROTECTION_ERR)
	    {
	      return 1;
	    }
#endif

	  /*
	   * Did the event queue overrun condition occurred?
	   * This means that all of the event queue entries were used up
	   * and another event occurred, i.e. there was no entry available
	   * to put the new event into.
	   */

	  if (GNI_CQ_OVERRUN (event_data))
	    {
	      cqOverrunErrorStr = "CQ_OVERRUN detected ";
	      error_code = 2;

	      fprintf (stdout,
		       "[%s] Rank: %4i ERROR CQ_OVERRUN detected\n",
		       uts_info.nodename, rank_id);
	    }

	  cqErrorStr = (char *) malloc (256);
	  if (cqErrorStr != NULL)
	    {

	      /*
	       * Print a user understandable error message.
	       */

	      tmp_status = GNI_CqErrorStr (event_data, cqErrorStr, 256);
	      if (tmp_status == GNI_RC_SUCCESS)
		{
		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    ERROR %sstatus: %s (%d) inst_id: %lu event: 0x%16.16lx GNI_CqErrorStr: %s\n",
			   uts_info.nodename, rank_id, cqOverrunErrorStr,
			   gni_err_str[status], status,
			   GNI_CQ_GET_INST_ID (event_data), event_data,
			   cqErrorStr);
		}
	      else
		{

		  /*
		   * Print the error number.
		   */

		  fprintf (stdout,
			   "[%s] Rank: %4i GNI_CqGetEvent    ERROR %sstatus: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
			   uts_info.nodename, rank_id, cqOverrunErrorStr,
			   gni_err_str[status], status,
			   GNI_CQ_GET_INST_ID (event_data), event_data);
		}

	      free (cqErrorStr);
	    }
	  else
	    {
	      /*
	       * Print the error number.
	       */

	      fprintf (stdout,
		       "[%s] Rank: %4i GNI_CqGetEvent    ERROR %sstatus: %s (%d) inst_id: %lu event: 0x%16.16lx\n",
		       uts_info.nodename, rank_id, cqOverrunErrorStr,
		       gni_err_str[status], status,
		       GNI_CQ_GET_INST_ID (event_data), event_data);
	    }
	  return error_code;
	}
      else if (retry == 0)
	{
	  return 3;
	}
      else
	{

	  /*
	   * An event has not been received yet.
	   */

	  wait_count++;

	  if (wait_count >= MAXIMUM_CQ_RETRY_COUNT)
	    {
	      /*
	       * This prevents an indefinite retry, which could hang the
	       * application.
	       */

	      fprintf (stdout,
		       "[%s] Rank: %4i GNI_CqGetEvent    ERROR no event was received status: %d retry count: %d\n",
		       uts_info.nodename, rank_id, status, wait_count);
	      return 3;
	    }

	  /*
	   * Release the cpu to allow the event to be received.
	   * This is basically a sleep, if other processes need to do some work.
	   */

	  if ((wait_count % (MAXIMUM_CQ_RETRY_COUNT / 10)) == 0)
	    {
	      /*
	       * Sometimes it takes a little longer for
	       * the datagram to arrive.
	       */

	      sleep (1);
	    }
	  else
	    {
	      sched_yield ();
	    }
	}
    }

  return 1;
}

int
PMI_Init (int *var)
{

  pmi_fd = open ("/dev/pmi", O_RDWR);
  return PMI_SUCCESS;
}

int
PMI_Get_size (int *size)
{

  int rc;
  pmi_getsize_args_t size_arg;
  rc =
    ioctl (pmi_fd, PMI_IOC_GETSIZE, &size_arg, sizeof (pmi_getsize_args_t));
  *size = size_arg.comm_size;
  return PMI_SUCCESS;
}

int
PMI_Get_rank (int *rank)
{

  int rc;
  pmi_getrank_args_t rank_arg;
  rc =
    ioctl (pmi_fd, PMI_IOC_GETRANK, &rank_arg, sizeof (pmi_getrank_args_t));
  *rank = rank_arg.myrank;
  return PMI_SUCCESS;
}

int
PMI_Finalize (void)
{
  int rc;
  rc = ioctl (pmi_fd, PMI_IOC_FINALIZE, NULL, 0);
  return PMI_SUCCESS;
}

int
PMI_Barrier (void)
{
  int rc;
  rc = ioctl (pmi_fd, PMI_IOC_BARRIER, NULL, 0);
  return PMI_SUCCESS;
}

void
gather (void *in, void *out, int len)
{
  int rc;
  pmi_allgather_args_t gather_arg;
  gather_arg.in_data = in;
  gather_arg.out_data = out;
  rc =
    ioctl (pmi_fd, PMI_IOC_ALLGATHER, &gather_arg,
	   sizeof (pmi_allgather_args_t));
  return PMI_SUCCESS;
}
