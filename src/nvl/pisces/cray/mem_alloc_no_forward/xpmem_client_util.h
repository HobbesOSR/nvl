/*
 * Copyright 2011 Cray Inc.  All Rights Reserved.
 */

/*
 * This header file contains the common utility functions.
 */

/*
 * allgather gather the requested information from all of the ranks.
 */

void allgather(void *in, void *out, int len);

/*
 * get_gni_nic_address get the nic address for the specified device.
 *
 *   Returns: the nic address for the specified device.
 */

unsigned int get_gni_nic_address(int device_id);

/*
 * gather_nic_addresses gather all of the nic addresses for all of the
 *                      other ranks.
 *
 *   Returns: an array of addresses for all of the nics from all of the
 *            other ranks.
 */

void*    gather_nic_addresses(void);


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
get_cq_event(gni_cq_handle_t cq_handle, struct utsname uts_info,
             int rank_id, unsigned int source_cq, unsigned int retry,
             gni_cq_entry_t *next_event);

inline int
	print_results(void);
