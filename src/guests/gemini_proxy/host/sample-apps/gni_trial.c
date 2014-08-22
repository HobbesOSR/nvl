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
#include "libalpslli.h"
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




typedef struct {
    gni_cq_handle_t cq_hdl;
    gni_ep_handle_t ep_hdl;
} conn_ep;




static uint32_t get_cpunum(void);




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

int main (int argc, char *argv[])
{



    uint32_t nic_addr  =0;
    uint32_t cpu_num   =0;
    uint32_t thread_num=0;
    uint32_t gni_cpu_id=0;
    int rc;

     gni_cdm_handle_t     cdm_hdl;  /* a client creates this comm domain with params from the server */
    gni_nic_handle_t     nic_hdl;
    gni_cq_handle_t cq_hdl;
    uint32_t gnu_cpu_id;
    uint32_t ptag, cookie, instance, local_addr;



        rc=GNI_CdmGetNicAddress (0, &nic_addr, &gni_cpu_id);

        cpu_num = get_cpunum();
	printf("Got address = 0x%x and core_id = %d\n", nic_addr, gnu_cpu_id);

        instance=GNI_INSTID(nic_addr, cpu_num, thread_num);
	if (argc < 4) {
                fprintf(stderr, " PTag, Cookie,  Instances  ,local addr are required\n");
                fprintf(stderr, "gni_trial1 <ptag> <cookie> <instance> <local_addr>]\n");
                return 0;
        }

        ptag = atoi(argv[1]);
        cookie = atoi(argv[2]);
        instance = atoi(argv[3]);
        local_addr = atoi(argv[4]);
        rc=GNI_CdmCreate(instance,
                ptag,
                cookie,
                GNI_CDM_MODE_ERR_NO_KILL,
//                GNI_CDM_MODE_ERR_NO_KILL|GNI_CDM_MODE_BTE_SINGLE_CHANNEL,
                &cdm_hdl);
        if (rc!=GNI_RC_SUCCESS) {
            fprintf(stderr, "CdmCreate() failed: %d", rc);
            rc= -1;
            goto cleanup;
        }


       fprintf(stderr, " after cdmcreate\n");
        rc=GNI_CdmAttach(cdm_hdl,
                0, //device_id == 0
                (uint32_t*)&local_addr, /* ALPS and GNI disagree about the type of local_addr.  cast here. */
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
         while(1)
		sleep(100);

cleanup:

    return(rc);
}

