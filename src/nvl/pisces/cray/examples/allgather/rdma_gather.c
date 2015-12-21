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
#include "pmi_util.h"
#include "pmi.h"

#define BIND_ID_MULTIPLIER       100
#define CACHELINE_MASK           0x3F   /* 64 byte cacheline */
#define CDM_ID_MULTIPLIER        1000
#define FLAG_DATA                0xffff000000000000
#define LOCAL_EVENT_ID_BASE      10000000
#define NUMBER_OF_TRANSFERS      10
#define POST_ID_MULTIPLIER       1000
#define REMOTE_EVENT_ID_BASE     11000000
#define SEND_DATA                0xdddd000000000000
#define TRANSFER_LENGTH          4096
#define TRANSFER_LENGTH_IN_BYTES ((TRANSFER_LENGTH)*sizeof(uint64_t))

typedef struct {
    gni_mem_handle_t mdh;
    uint64_t        addr;
} mdh_addr_t;

int             rank_id;
struct utsname  uts_info;
int             v_option = 0;

#include "utility_functions.h"

void print_help(void)
{
    fprintf(stdout,
"\n"
"RDMA_PUT_PMI_EXAMPLE\n"
"  Purpose:\n"
"    The purpose of this example is to demonstrate the sending of a data\n"
"    transaction to a remote communication endpoint using a RDMA Put request.\n"
"\n"
"  APIs:\n"
"    This example will concentrate on using the following uGNI APIs:\n"
"      - GNI_PostRdma() is used to with the 'PUT' type to send a data\n"
"        transaction to a remote location.\n"
"\n"
    );
}

int
main(int argc, char **argv)
{
    unsigned int   *all_nic_addresses;
    uint32_t        bind_id;
    gni_cdm_handle_t cdm_handle;
    uint32_t        cdm_id;
    int             cookie;
    gni_cq_handle_t cq_handle;
    int             create_destination_cq = 1;
    int             create_destination_overrun = 0;
    gni_cq_entry_t  current_event;
    uint64_t        data = SEND_DATA;
    int             data_transfers_sent = 0;
    gni_cq_handle_t destination_cq_handle = NULL;
    int             device_id = 0;
    gni_ep_handle_t *endpoint_handles_array;
    uint32_t        event_inst_id;
    gni_post_descriptor_t *event_post_desc_ptr;
    uint32_t        expected_local_event_id;
    uint32_t        expected_remote_event_id;
    int             first_spawned;
    uint64_t        *flag;
    volatile uint64_t *flag_ptr;
    int             flag_transfers_sent = 0;
    int             i;
    int             j;
    unsigned int    local_address;
    uint32_t        local_event_id;
    int             modes = GNI_CDM_MODE_BTE_SINGLE_CHANNEL;
    gni_mem_handle_t my_flag_memory_handle;
    int             my_id;
    mdh_addr_t      my_memory_handle;
    int             my_receive_from;
    gni_nic_handle_t nic_handle;
    int             number_of_cq_entries;
    int             number_of_dest_cq_entries;
    int             number_of_ranks;
    char            opt;
    extern char    *optarg;
    extern int      optopt;
    uint8_t         ptag;
    int             rc;
    gni_post_descriptor_t *rdma_data_desc;
    gni_post_descriptor_t *rdma_flag_desc;
    uint64_t       *receive_buffer;
    uint64_t        receive_data = SEND_DATA;
    uint64_t        receive_flag = FLAG_DATA;
    int             receive_from;
    gni_mem_handle_t receive_memory_handle;
    unsigned int    remote_address;
    uint32_t        remote_event_id;
    mdh_addr_t     *remote_memory_handle_array;
    uint64_t       *send_buffer;
    uint64_t        send_post_id;
    int             send_to;
    gni_mem_handle_t source_memory_handle;
    gni_return_t    status = GNI_RC_SUCCESS;
    char           *text_pointer;
    uint32_t        transfers = NUMBER_OF_TRANSFERS;
    int             use_event_id = 0;


    if ((i = uname(&uts_info)) != 0) {
        fprintf(stderr, "uname(2) failed, errno=%d\n", errno);
        exit(1);
    }

    /*
     * Get job attributes from PMI.
     */

    rc = PMI_Init(&first_spawned);
    assert(rc == PMI_SUCCESS);

    rc = PMI_Get_size(&number_of_ranks);
    assert(rc == PMI_SUCCESS);

    rc = PMI_Get_rank(&rank_id);
    assert(rc == PMI_SUCCESS);
    my_rank = rank_id;
  comm_size = number_of_ranks;



    /*
     * Allocate a buffer to contain all of the remote memory handle's.
     */

    remote_memory_handle_array =
        (mdh_addr_t *) calloc(number_of_ranks, sizeof(mdh_addr_t));
    assert(remote_memory_handle_array);
    if (rank_id == 0){
    receive_buffer = 0x7ffef1b45000;
    my_memory_handle.addr = (uint64_t) receive_buffer;
    my_memory_handle.mdh.qword1 = (uint64_t) 0x290000000000634;
    my_memory_handle.mdh.qword2 = (uint64_t) 0x9a000020c0000001;
    } else if (rank_id == 1){
    receive_buffer = 0x7ffef1b46000;
    my_memory_handle.addr = (uint64_t) receive_buffer;
    my_memory_handle.mdh.qword1 = (uint64_t) 0x280000000000632;
    my_memory_handle.mdh.qword2 = (uint64_t) 0xb000020c0000001;
	}

    /*
     * Gather up all of the remote memory handle's.
     * This also acts as a barrier to get all of the ranks to sync up.
     */

    //hexdump(&my_memory_handle, sizeof(mdh_addr_t));
    gather(&my_memory_handle, remote_memory_handle_array,
              sizeof(mdh_addr_t));

        fprintf(stderr,
                "[%s] rank address     mdh.qword1            mdn.qword2\n",
                uts_info.nodename);
        for (i = 0; i < number_of_ranks; i++) {
            fprintf(stderr, "[%s] %4i 0x%lx    0x%016lx    0x%016lx\n",
                    uts_info.nodename, i,
                    remote_memory_handle_array[i].addr,
                    remote_memory_handle_array[i].mdh.qword1,
                    remote_memory_handle_array[i].mdh.qword2);
        }


    PMI_Finalize();
    return rc;
}
