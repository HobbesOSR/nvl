/**
//
//
// *************************************************************************
//@HEADER
 */
/**
 */

#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
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

#include <sched.h>
#include <alps/libalpslli.h>
#include <gni_pub.h>


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

/*
# pwd
/scratch1/smukher/vnet/palacios_debug_test/palacios/linux_usr/v3_debug.c
# ./v3_debug /dev/v3-vm0 0 3
Debug Virtual Core 0 with Command 3
# ./v3_debug /dev/v3-vm0 0 1
Debug Virtual Core 0 with Command 1
This is done via  ioctl to /dev/v3-vm0
# ./v3_debug /dev/v3-vm0 0 2
Debug Virtual Core 0 with Command 2
# ./v3_debug /dev/v3-vm0 0 4
Debug Virtual Core 0 with Command 4
# ./v3_debug /dev/v3-vm0 0 5
Debug Virtual Core 0 with Command 5

V3_VM_DEBUG ioctl == 31:
*/



typedef struct {
    gni_cq_handle_t cq_hdl;
    gni_ep_handle_t ep_hdl;
} conn_ep;


typedef struct {
    char             *peer_name;
    uint32_t          peer_cookie;
    uint32_t          peer_ptag;
    int			  peer_instance;

    alpsAppGni_t      peer_alps_info;


    gni_cdm_handle_t     cdm_hdl;  /* a client creates this comm domain with params from the server */
    gni_nic_handle_t     nic_hdl;
    gni_ep_handle_t      ep_hdl;

} gni_connection;



static uint32_t get_cpunum(void);
static void get_alps_info(alpsAppGni_t *alps_info);




/**
 */

static void get_alps_info(
        alpsAppGni_t *alps_info)
{
    int alps_rc=0;
    int req_rc=0;
    size_t rep_size=0;

    uint64_t apid=0;
    alpsAppLLIGni_t *alps_info_list;
    char buf[1024];

    alps_info_list=(alpsAppLLIGni_t *)&buf[0];

    alps_app_lli_lock();

    fprintf(stderr, "sending ALPS request\n");
    alps_rc = alps_app_lli_put_request(ALPS_APP_LLI_ALPS_REQ_GNI, NULL, 0);
    if (alps_rc != 0) fprintf(stderr, "alps_app_lli_put_request failed: %d", alps_rc);
    fprintf(stderr, "waiting for ALPS reply\n");
    alps_rc = alps_app_lli_get_response(&req_rc, &rep_size);
    if (alps_rc != 0) fprintf(stderr, "alps_app_lli_get_response failed: alps_rc=%d\n", alps_rc);
    if (req_rc != 0) fprintf(stderr, "alps_app_lli_get_response failed: req_rc=%d\n", req_rc);
    if (rep_size != 0) {
        fprintf(stderr, "waiting for ALPS reply bytes (%d) ; sizeof(alps_info)==%d ; sizeof(alps_info_list)==%d\n", rep_size, sizeof(alps_info), sizeof(alps_info_list));
        alps_rc = alps_app_lli_get_response_bytes(alps_info_list, rep_size);
        if (alps_rc != 0) fprintf(stderr, "alps_app_lli_get_response_bytes failed: %d", alps_rc);
    }

    fprintf(stderr, "sending ALPS request\n");
    alps_rc = alps_app_lli_put_request(ALPS_APP_LLI_ALPS_REQ_APID, NULL, 0);
    if (alps_rc != 0) fprintf(stderr, "alps_app_lli_put_request failed: %d\n", alps_rc);
    fprintf(stderr, "waiting for ALPS reply");
    alps_rc = alps_app_lli_get_response(&req_rc, &rep_size);
    if (alps_rc != 0) fprintf(stderr, "alps_app_lli_get_response failed: alps_rc=%d\n", alps_rc);
    if (req_rc != 0) fprintf(stderr, "alps_app_lli_get_response failed: req_rc=%d\n", req_rc);
    if (rep_size != 0) {
        fprintf(stderr, "waiting for ALPS reply bytes (%d) ; sizeof(apid)==%d\n", rep_size, sizeof(apid));
        alps_rc = alps_app_lli_get_response_bytes(&apid, rep_size);
        if (alps_rc != 0) fprintf(stderr, "alps_app_lli_get_response_bytes failed: %d\n", alps_rc);
    }

    alps_app_lli_unlock();

    memcpy(alps_info, (alpsAppGni_t*)alps_info_list->u.buf, sizeof(alpsAppGni_t));

    fprintf(stderr, "apid                 =%llu\n", (unsigned long long)apid);
    fprintf(stderr, "alps_info->device_id =%llu\n", (unsigned long long)alps_info->device_id);
    fprintf(stderr, "alps_info->local_addr=%lld\n", (long long)alps_info->local_addr);
    fprintf(stderr, "alps_info->cookie    =%llu\n", (unsigned long long)alps_info->cookie);
    fprintf(stderr, "alps_info->ptag      =%llu\n", (unsigned long long)alps_info->ptag);

    fprintf(stderr, "ALPS response - apid(%llu) alps_info->device_id(%llu) alps_info->local_addr(%llu) \n"
            "alps_info->cookie(%llu) alps_info->ptag(%llu)\n",
            (unsigned long long)apid,
            (unsigned long long)alps_info->device_id,
            (long long)alps_info->local_addr,
            (unsigned long long)alps_info->cookie,
            (unsigned long long)alps_info->ptag);

        return 0;

    return;
}

static uint32_t get_cpunum(void)
{
  int i, j;
  uint32_t cpu_num;

  cpu_set_t coremask;

  (void)sched_getaffinity(0, sizeof(coremask), &coremask);

  for (i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, &coremask)) {
      int run = 0;
      for (j = i + 1; j < CPU_SETSIZE; j++) {
        if (CPU_ISSET(j, &coremask)) run++;
        else break;
      }
      if (!run) {
        cpu_num=i;
      } else {
        fprintf(stdout, "This thread is bound to multiple CPUs(%d).  Using lowest numbered CPU(%d).", run+1, cpu_num);
        cpu_num=i;
      }
    }
  }
  return(cpu_num);
}

#define GNI_INSTID(nic_addr, cpu_num, thr_num) (((nic_addr&NIC_ADDR_MASK)<<NIC_ADDR_SHIFT)|((cpu_num&CPU_NUM_MASK)<<CPU_NUM_SHIFT)|(thr_num&THREAD_NUM_MASK))

#define GNI_ALPS_FILE "/scratch1/smukher/alps-info"

int main (int argc, char *argv[])
{


    alpsAppGni_t     alps_info;

    uint32_t nic_addr  =0;
    uint32_t cpu_num   =0;
    uint32_t thread_num=0;
    uint32_t gni_cpu_id=0;
    uint32_t  instance;
    int rc;
    char    file_name[1024];
    FILE    *fd;
    int retval;

     gni_cdm_handle_t     cdm_hdl;  /* a client creates this comm domain with params from the server */
    gni_nic_handle_t     nic_hdl;
    gni_ep_handle_t      ep_hdl;
    gni_cq_handle_t cq_hdl;


        get_alps_info(&alps_info);
 


        rc=GNI_CdmGetNicAddress (alps_info.device_id, &nic_addr, &gni_cpu_id);

        cpu_num = get_cpunum();

        instance=GNI_INSTID(nic_addr, cpu_num, thread_num);
        fprintf(stderr, "nic_addr(%llu), cpu_num(%llu), thread_num(%llu), inst_id(%llu)\n",
                (uint64_t)nic_addr, (uint64_t)cpu_num, (uint64_t)thread_num,
                (uint64_t)instance);
         cpu_num = get_cpunum();

        sprintf(file_name, GNI_ALPS_FILE);
        fd = fopen(file_name, "w+");
        if (fd < 0) {
                return -1;
        }
        retval = fprintf(fd, "%d  %llu %llu %llu %lld", alps_info.device_id, (unsigned long long)alps_info.cookie, (unsigned long long)alps_info.ptag, (unsigned long long)instance, (long long)alps_info.local_addr);

        fclose(fd);
 

        rc=GNI_CdmCreate(instance,
                alps_info.ptag,
                alps_info.cookie,
                GNI_CDM_MODE_ERR_NO_KILL,
                &cdm_hdl);
        if (rc!=GNI_RC_SUCCESS) {
            fprintf(stderr, "CdmCreate() failed: %d", rc);
            rc= -1;
            goto cleanup;
        }
/*

        rc=GNI_CdmAttach(cdm_hdl,
                alps_info.device_id,
                (uint32_t*)&alps_info.local_addr,
                &nic_hdl);
        if (rc!=GNI_RC_SUCCESS) {
            fprintf(stderr, "CdmAttach() failed: %d\n", rc);
            if (rc==GNI_RC_PERMISSION_ERROR)
                rc= -2;
            else
                rc= -3;

            goto cleanup;
        }

        rc=GNI_CqCreate (nic_hdl, 1000, 0, GNI_CQ_BLOCKING, NULL, NULL, &cq_hdl);
        if (rc!=GNI_RC_SUCCESS) {
            fprintf(stderr, "CqCreate(ep_cq_hdl) failed: %d\n", rc);
            goto cleanup;
        }
*/
         while(1)
		sleep(100);

cleanup:

    return(rc);
}

