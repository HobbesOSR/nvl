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

/*Shyamali CQ event hack */

/* CQ event hack ****************************************************/

#define CQE_SOURCE_SHIFT           56UL
#define CQE_SOURCE_MASK            0x7UL

int
cq_event_ready (gni_cq_entry_t event)
{
  return (((event) >> CQE_SOURCE_SHIFT) & CQE_SOURCE_MASK);
}

static int
gni_cq_remove_event (IN gni_cq_handle_t cq_hndl,
		     OUT gni_cq_entry_t * event_data)
{
  gni_cq_entry_t entry;
  gni_return_t status = GNI_RC_SUCCESS;
  uint32_t cq_idx;

  /* avoid locking if no event is ready */
  if (!GNII_CQ_HAS_EVENT (cq_hndl))
    {
      return GNI_RC_NOT_DONE;
    }

  gni_slock (&cq_hndl->api_lock);

  cq_idx = GNII_CQ_REMAP_INDEX (cq_hndl, cq_hndl->read_index);
  entry = cq_hndl->queue[cq_idx];

  /*
   * quick return if no event
   */
  if (!GHAL_CQ_IS_EVENT_READY (entry))
    {
      /* No new entry has arrived on the queue. */
      printf ("no event ready so return second case\n");
      gni_sunlock (&cq_hndl->api_lock);
      return GNI_RC_NOT_DONE;
    }

  /* A new entry has arrived successfully. This event may or
   * may not have an error state. Copy CQE into *event_data.
   */
  *event_data = entry;
  /*
     printf("got event 0x%016"PRIx64" read_index = (%u, %u)",
     entry, cq_hndl->read_index, cq_idx);

     Increment the CQ read index and write it to the Gemini. */
  GNII_CQ_ADVANCE (cq_hndl, entry);

  if (unlikely (GNI_CQ_OVERRUN (entry)))
    {
      /* CQ is in overrun state and CQ events may have been
       * lost; this is a fatal condition for RDMA and FMA 
       * operations; the entry has been removed from the
       * queue above.
       */
      status = GNI_RC_ERROR_RESOURCE;
    }
  else if (likely (GNI_CQ_STATUS_OK (entry)))
    {
      /* CQE has no error state */
      status = GNI_RC_SUCCESS;
    }
  else
    {
      /* CQE has error state */
      status = GNI_RC_TRANSACTION_ERROR;
    }

  gni_sunlock (&cq_hndl->api_lock);

  return status;
}

static int64_t
gni_reqid (gni_cq_entry_t entry)
{
  uint64_t ret;
  ret = GNI_CQ_GET_TID (entry);
  return ret;
}

static gni_return_t
gni_reqfind (IN gni_cq_handle_t cq_hndl, IN uint32_t req_id,
	     OUT gnii_req_t ** req_ptr)
{
  gnii_req_t *req;
  gnii_req_list_t *req_list;

  req_list = (gnii_req_list_t *) cq_hndl->req_list;
  if (unlikely ((req_id < GNII_REQ_ID_OFFSET) ||
		(req_id > req_list->nreqs_max + GNII_REQ_ID_OFFSET - 1)))
    {
      return GNI_RC_INVALID_PARAM;
    }
  req = &req_list->base[req_id - GNII_REQ_ID_OFFSET];
  *req_ptr = req;

  return GNI_RC_SUCCESS;
}


static inline void
gni_reqfree (IN gni_cq_handle_t cq_hndl, IN gnii_req_t * request)
{
  gnii_req_list_t *req_list;

  request->type = GNII_REQ_TYPE_INACTIVE;
  req_list = (gnii_req_list_t *) cq_hndl->req_list;
  req_list->freelist[--(req_list->freehead)] = request;
}

gni_return_t
gni_getcompleted (IN gni_cq_handle_t cq_hndl,
		  IN gni_cq_entry_t event_data,
		  OUT gni_post_descriptor_t ** post_descr)
{
  gni_post_descriptor_t *desc;
  int req_id;
  gnii_req_t *gnii_req;
  gni_return_t rc = GNI_RC_SUCCESS;

#ifdef GNI_DEBUG
  if (unlikely
      ((cq_hndl == NULL) || (cq_hndl->type_id != GNI_TYPE_CQ_HANDLE)
       || (cq_hndl->mdh_attached)))
    {
      return GNI_RC_INVALID_PARAM;
    }
#endif

  if (unlikely (GNI_CQ_GET_TYPE (event_data) != GNI_CQ_EVENT_TYPE_POST))
    {
      *post_descr = NULL;
      printf ("failed here first \n");
      return GNI_RC_DESCRIPTOR_ERROR;
    }

  /* Pull the request id out of the event data. */

  req_id = gni_reqid (event_data);

  /* Find the request specified by req_id in the vector of requests kept 
     internally for bookkeeping. If no valid request is found, return 
     an error. */

  rc = gni_reqfind (cq_hndl, req_id, &gnii_req);
  if (unlikely (rc != GNI_RC_SUCCESS))
    {
      return rc;
    }

  if (unlikely (gnii_req->type != GNII_REQ_TYPE_POST))
    {
      *post_descr = NULL;
      printf ("failed here second \n");
      return GNI_RC_DESCRIPTOR_ERROR;
    }

  /* Retrieve the original post descriptor and check its error status. */

  desc = gnii_req->u.post.desc;
  rc = desc->status;
  if (rc != GNI_RC_SUCCESS && rc != GNI_RC_TRANSACTION_ERROR)
    {
      printf ("failed here third %d\n", rc);
      rc = GNI_RC_DESCRIPTOR_ERROR;
    }

  *post_descr = desc;

  /* If we have gotten all cqes requested for
     this descriptor, remove the descriptor from
     the linked list and release the associated
     request. Note that in the case of a non-okay status,
     and in the case that both GNI_CQMODE_LOCAL_EVENT and
     GNI_CQMODE_GLOBAL_EVENT were requested, both events may not
     be returned by hardware.  For more details consult section
     1.6.3 of bte_logic_spec.doc */

  //printf("desc->cq_mode_complete = %d desc->cq_mode = %d req_id = %d\n",
  //      desc->cq_mode_complete,desc->cq_mode,gnii_req->id);

  if (!gnii_req->cqes_pending)
    {
      gni_reqfree (cq_hndl, gnii_req);	/* free the request */
    }

  return rc;
}

/*
 * GNII_ProcessLocalCQEntry - process local cq entry
 */

gni_return_t
gni_processlocalentry (gni_cq_handle_t cq_hndl, gni_cq_entry_t * event_data)
{
  uint32_t req_id, recoverable;
  gnii_req_t *req = NULL;
  gni_post_descriptor_t *desc;
  gni_return_t status = GNI_RC_SUCCESS;
  gni_ep_smsg_mbox_t *smsg;


  /* we special case FLBTE CQEs for several reasons:
   *   - FLBTE implementation is intended to avoid BTE_UNAVAILABLE errors
   *     and any enqueue failure means the atomic counter scheme is broken;
   *     so we return GNI_RC_ERROR_RESOURCE on error
   *   - only bottom 20 bits of SW_DATA were specified by software via SRC_CQDATA
   *   - these 20 bits (req_id) don't currently allow us to lookup gnii_req_t
   *   - it isn't enough to check for non-zero STATUS because STATUS is zero for
   *     FLBTE enqueue errors (BTE_UNAVAILABLE and INSUFFICIENT_PRIV).
   */
  if (unlikely (GNI_CQ_BTE_ENQ_STATUS (*event_data)))
    {
      if (GNI_CQ_GET_FAILED_ENQUEUE_CNT (*event_data))
	{
	  char buffer[1024];
	  GNI_CqErrorStr (*event_data, buffer, sizeof (buffer));
	  /* FLBTE enqueue failure */
	  status = GNI_RC_ERROR_RESOURCE;
	}
      else
	{
	  /* FLBTE enqueue success (note, enqueue success events not enabled) */
	  status = GNI_RC_SUCCESS;
	}
      return status;
    }

  /* quick return if DMAPP managed CQE */

  if (GNI_CQ_GET_TYPE (*event_data) == GNI_CQ_EVENT_TYPE_DMAPP)
    return status;

  /* Handle local cq events. */

  req_id = gni_reqid (*event_data);
  status = gni_reqfind (cq_hndl, req_id, &req);
  if (unlikely (status != GNI_RC_SUCCESS))
    {
      GNI_ERROR (GNI_ERR_FATAL,
		 "GNII_ProcessLocalCQEntry: find request failed %s",
		 gni_err_str[status]);
      return status;
    }


  switch (req->type)
    {
    case GNII_REQ_TYPE_POST:
      gcc_atomic_dec (&req->ep_hndl->outstanding_tx_reqs);
      desc = req->u.post.desc;

      /* We know if we get to here that the CQ is not overrun,
         so either the transaction completed okay, or there was
         was some kind of transaction error */
      if (likely (GNI_CQ_STATUS_OK (*event_data)))
	{
	  desc->status = GNI_RC_SUCCESS;
	}
      else
	{
	  desc->status = GNI_RC_TRANSACTION_ERROR;
	}

      if (GHAL_CQ_SOURCE_BTE (*event_data) &&
	  ((desc->type == GNI_POST_RDMA_PUT) ||
	   (desc->type == GNI_POST_RDMA_GET)))
	{
	  req->cqes_pending &= ~GNI_CQMODE_LOCAL_EVENT;

	  /* Swap in the local_event bits into the event_data */
	  GNI_CQ_SET_INST_ID (*event_data, req->local_event);
	}
      if (GHAL_CQ_SOURCE_SSID (*event_data))
	{
	  req->cqes_pending &= ~GNI_CQMODE_GLOBAL_EVENT;

	  /* Swap in the remote_event bits into the event_data */
	  GNI_CQ_SET_INST_ID (*event_data, req->local_event);

	  /* FMA DLA processing */
	  //GNII_ReqDlaGlobalEvent(cq_hndl, req);
	}

      /* Or in the GNII_REQ_TYPE_POST at bits 54,55 */
      GNI_CQ_SET_TYPE (*event_data, GNI_CQ_EVENT_TYPE_POST);

      /* swap back in the user's post_id */
      desc->post_id = req->u.post.user_post_id;

      status = desc->status;
      break;
    default:
      GNI_ERROR (GNI_ERR_FATAL, "unknown request type: %d", req->type);
      break;
    }
  return status;
}

int
gni_getevent (IN gni_cq_handle_t cq_hndl, OUT gni_cq_entry_t * event_data)
{
  gni_return_t status = GNI_RC_SUCCESS;

#ifdef GNI_DEBUG
  if (unlikely
      ((cq_hndl == NULL) || (cq_hndl->type_id != GNI_TYPE_CQ_HANDLE)))
    {
      return GNI_RC_INVALID_PARAM;
    }
#endif

  do
    {
      status = gni_cq_remove_event (cq_hndl, event_data);
      if ((status == GNI_RC_NOT_DONE) || (status == GNI_RC_ERROR_RESOURCE))
	{
	  /* done if no events or OVERRUN */
	  break;
	}
      else if (status == GNI_RC_TRANSACTION_ERROR)
	{
	  char buffer[1024];
	  /* CQE has error state */
	  GNI_CqErrorStr (*event_data, buffer, sizeof (buffer));
	}
      if (!cq_hndl->mdh_attached && cq_hndl->req_list)
	{
	  status = gni_processlocalentry (cq_hndl, event_data);

	  /* Process Remote CQ Entry errors */
	}
      else if (unlikely (status == GNI_RC_TRANSACTION_ERROR))
	{
	  /* ignore DLA overflow errors (sender will resend) */
	  if (GNI_CQ_STATUS_DLA_OVERFLOW (*event_data))
	    {
	      status = GNI_RC_NO_MATCH;
	    }
	}


      /* If this is a cq for source-terms, i.e. FMA or BTE, 
       * do internal processing of event and return. If this 
       * cq is used for remote notification, the user needs
       * to look for the data herself.
       */

      if (unlikely (cq_hndl->handler != NULL) &&
	  (status == GNI_RC_SUCCESS || status == GNI_RC_TRANSACTION_ERROR))
	{
	  (*cq_hndl->handler) (event_data, cq_hndl->context);
	}
    }
  while (status == GNI_RC_NO_MATCH);

  return status;
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

      //status = GNI_CqGetEvent (cq_handle, &event_data);
      status = gni_getevent (cq_handle, &event_data);
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
		  status = GNI_RC_SUCCESS;
		  return 0;
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


#ifndef HEXDUMP_COLS
#define HEXDUMP_COLS 8
#endif

void
hexdump (void *mem, unsigned int len)
{
  unsigned int i, j;

  for (i = 0;
       i <
       len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0);
       i++)
    {
      /* print offset */
      if (i % HEXDUMP_COLS == 0)
	{
	  printf ("0x%06x: ", i);
	}

      /* print hex data */
      if (i < len)
	{
	  printf ("%02x ", 0xFF & ((char *) mem)[i]);
	}
      else			/* end of block, just aligning for ASCII dump */
	{
	  printf ("   ");
	}

      /* print ASCII dump */
      if (i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
	{
	  for (j = i - (HEXDUMP_COLS - 1); j <= i; j++)
	    {
	      if (j >= len)	/* end of block, not really printing */
		{
		  putchar (' ');
		}
	      else if (isprint (((char *) mem)[j]))	/* printable char */
		{
		  putchar (0xFF & ((char *) mem)[j]);
		}
	      else		/* other char */
		{
		  putchar ('.');
		}
	    }
	  putchar ('\n');
	}
    }
}

void
gather (void *in, void *out, int len)
{
  int rc;
  pmi_allgather_args_t gather_arg;
  gather_arg.in_data = in;
  gather_arg.out_data = out;
  gather_arg.in_data_len = len;
  //fprintf(stdout, "here pointer in %p, len %d, pointer pack %p\n", in, len, gather_arg.in_data);
  //hexdump(gather_arg.in_data, len);
  rc =
    ioctl (pmi_fd, PMI_IOC_ALLGATHER, &gather_arg,
	   sizeof (pmi_allgather_args_t));
  return PMI_SUCCESS;
}
