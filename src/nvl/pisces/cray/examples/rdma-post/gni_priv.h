/* -*- c-basic-offset: 8; indent-tabs-mode: nil -*- */
/*
        Contains data structures and constants used internally by the GNI.

        Copyright 2007 Cray Inc. All Rights Reserved.
        Written by Igor Gorodetsky <igorodet@cray.com>

        This program is free software; you can redistribute it and/or modify it 
        under the terms of the GNU General Public License as published by the 
        Free Software Foundation; either version 2 of the License, 
        or (at your option) any later version.

        This program is distributed in the hope that it will be useful, 
        but WITHOUT ANY WARRANTY; without even the implied warranty of 
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
        See the GNU General Public License for more details.

        You should have received a copy of the GNU General Public License 
        along with this program; if not, write to the Free Software Foundation, Inc., 
        51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _GNI_PRIV_H_
#define _GNI_PRIV_H_

#include "gni_pub.h"
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __KERNEL__
#include <sys/ioctl.h>
#include <pthread.h>
#else
#include <linux/ioctl.h>
#include <linux/mmu_notifier.h>
#include <linux/mutex.h>
#endif
#include "ghal_pub.h"
#include "gni_atomic.h"
#include "gni_bitops.h"
#include "gni_list.h"

/** 
 * GNI driver version control macros and values 
 * Example: 0x00400062
 */
#if defined CRAY_CONFIG_GHAL_GEMINI
/* Reflects major releases of GNI driver (e.g. support for new HW) */
#define GNI_DRV_MAJOR_REV 0x00
/* MINOR revisions:
 * Reflects any interface changes, e.g. IOCTL and kernel level GNI API.
 * 0x44 - revision as of 20120202 before table was started.
 */
#define GNI_DRV_MINOR_REV 0x44

#elif defined CRAY_CONFIG_GHAL_ARIES
/* Reflects major releases of GNI driver (e.g. support for new HW) */
#define GNI_DRV_MAJOR_REV 0x00
/* MINOR revisions:
 * Reflects any interface changes, e.g. IOCTL and kernel level GNI API.
 * 0x44 - revision as of 20120202 before table was started.
 * 0x45 - GNI_IOC_NIC_SETATTR: CQ cacheline revisit mode 1 made default.
 */
#define GNI_DRV_MINOR_REV 0x45

#else
#error "Add new GHAL target to gni_priv.h"
#endif

#ifdef __KERNEL__
/* Reflects any kgni driver code changes, sometimes */
#define GNI_DRV_CODE_REV  0x00b9
#endif
#define GNI_DRV_VERSION ((GNI_DRV_MAJOR_REV << 24) | (GNI_DRV_MINOR_REV << 16) | GNI_DRV_CODE_REV)

/* Cascade PTAG ranges.  PTAGs are dynamically assigned by the driver on
 * Cascade nodes.  Given an attached KNC, host and KNC drivers assign PTAGs
 * from separate ranges to prevent concurrent read-modify-write of the same PTT
 * entries. */
  enum
  {
    GNI_PTAG_AUTO_START = 4,
    GNI_PTAG_AUTO_HOST_START = GNI_PTAG_AUTO_START,
    GNI_PTAG_AUTO_HOST_END = 127,
    /* PTAG 128 saved for GNI_PTAG_LND_KNC (defined in gni_pub.h) */
    GNI_PTAG_AUTO_KNC_START = 129,
    GNI_PTAG_AUTO_KNC_END = 251,
    GNI_PTAG_AUTO_END = GNI_PTAG_AUTO_KNC_END
  };

/* h/w alignment requirement for CQs */
#define GNI_CQ_ALIGNMENT            4096
#define GNI_CQ_MAX_NUMBER           GHAL_CQ_MAX_NUMBER
#define GNI_CQ_EMULATED_MAX_NUMBER  (GNI_CQ_MAX_NUMBER/2)	/* should be more than ever needed */
/* rd_index resides after the CQ queue */
#define GNI_CQ_EMULATED_RD_INDEX_PTR(cq_hndl, addr) \
	((uint64_t *)((char *)(addr) + (((cq_hndl)->entry_count + 1) * sizeof(uint64_t))))

/* h/w alignment requirements for FMA and RDMA operations */
#define GNI_LOCAL_ADDR_ALIGNMENT    GHAL_LOCAL_ADDR_ALIGNMENT
#define GNI_ADDR_ALIGNMENT          GHAL_ADDR_ALIGNMENT
#define GNI_AMO_ALIGNMENT           GHAL_AMO_ALIGNMENT
#define GNI_AAX_ALIGNMENT           GHAL_AAX_ALIGNMENT

#define GNI_AMO_32_ALIGNMENT        3	/* 4 byte alignment */
#define GNI_AMO_32_OPERAND_SIZE     0xffffffff

#define GNI_CE_RES_ALIGNMENT        0x1f

/* Size of a cacheline is 64 bytes */
#define GNI_CACHELINE_SIZE          64
#define GNI_CQES_PER_CACHELINE      8	/* GNI_CACHELINE_SIZE / sizeof(gni_cq_entry_t) */
#define GNI_CQES_PER_PAGE           512	/* PAGE_SIZE / sizeof(gni_cq_entry_t) */

/* FMA Get or Fetching AMO Flagged Response */
#define GNI_FMA_FR_XFER_MAX_SIZE    (GNI_CACHELINE_SIZE - GNI_FMA_FLAGGED_RESPONSE_SIZE)

#define GNI_ERR_FATAL 1
#define GNI_ERR_WARN  2

/* gni_pub.h provides 4 private CDM mode bits, defined here */
#define GNI_CDM_MODE_DLA_DISABLE    GNI_CDM_MODE_PRIV_RESERVED_1
#define GNI_CDM_MODE_PRIV_UNUSED_2  GNI_CDM_MODE_PRIV_RESERVED_2
#define GNI_CDM_MODE_PRIV_UNUSED_3  GNI_CDM_MODE_PRIV_RESERVED_3
#define GNI_CDM_MODE_PRIV_UNUSED_4  GNI_CDM_MODE_PRIV_RESERVED_4

/* gni_pub.h provides 4 private MEM option bits, defined here */
#define GNI_MEM_COUNT_NON_VMDH        GNI_MEM_PRIV_RESERVED_1
#define GNI_MEM_NO_COUNTING_NON_VMDH  GNI_MEM_PRIV_RESERVED_2	/* this is used when uGNI is calling GNI_MemRegister */
#define GNI_MEM_PRIV_UNUSED_3         GNI_MEM_PRIV_RESERVED_3
#define GNI_MEM_PRIV_UNUSED_4         GNI_MEM_PRIV_RESERVED_4

#ifndef __KERNEL__
/* Error Handling */
#define GNI_ERROR(type, message, args...)  {                           \
    if ((type) & GNI_ERR_FATAL) fprintf(stderr, "Fatal Error: ");          \
    if ((type) & GNI_ERR_WARN)  fprintf(stderr, "Warning: ");              \
    fprintf(stderr, message, ##args);                                  \
    fprintf(stderr, " at line %d in file %s\n", __LINE__, __FILE__);   \
    if (type == GNI_ERR_FATAL) abort();                                 \
    }
#else
#define GNI_ERROR(type, message, args...)  {                           \
    if ((type) & GNI_ERR_FATAL) printk(KERN_ERR);          \
    if ((type) & GNI_ERR_WARN)  printk(KERN_INFO);              \
    printk(message, ##args);                                  \
    printk(" at line %d in file %s\n", __LINE__, __FILE__);   \
    if (type == GNI_ERR_FATAL) BUG();                          \
    }

#endif

#ifndef __KERNEL__
#define GNII_STAT_GET(nic_hndl, idx)         ((nic_hndl)->stats[idx])
#define GNII_STAT_INC(nic_hndl, idx)         ((nic_hndl)->stats[idx]++)
#define GNII_STAT_ADD(nic_hndl, idx, count)  ((nic_hndl)->stats[idx] += (count))
#define GNII_STAT_MAX(nic_hndl, idx, cmp)  \
	((nic_hndl)->stats[idx] = MAX((nic_hndl)->stats[idx], (cmp)))
#endif

/* IOCTL Arguments */
#define GNI_INVALID_CQ_DESCR    (-1L)
/* Set private attributes */
  typedef struct gni_nic_setattr_args
  {
    uint32_t ptag;		/* IN protection tag */
    uint32_t cookie;		/* IN cookie - used for EPs, etc. */
    uint32_t rank;		/* IN unique address of the instance within the upper layer protocol domain */
    struct
    {
      uint32_t modes:28;	/* IN modes, supplied in GNI_CdmCreate */
      uint32_t unused:2;
      uint32_t cq_revisit_mode:2;	/* OUT CQ revisit mode (Aries) */
    };
    uint32_t nic_pe;		/* OUT physical address of nic */
    uint64_t fma_window;	/* OUT address of fma_window (write combining) in process address space */
    uint64_t fma_window_nwc;	/* OUT address of fma_window (non write combining) in process address space */
    uint64_t fma_window_get;	/* OUT address of fma get window in process address space */
    uint64_t fma_ctrl;		/* OUT address of fma control window in process address space */
    uint64_t datagram_counter;	/* OUT address of completed datagram counter in process address space */
  } gni_nic_setattr_args_t;

/* Set FMA Privilege Mask */
  typedef struct gni_fma_setpriv_args
  {
    uint32_t priv_mask;		/* IN FMA Privileged mask; set to (-1) or GHAL_FMA_MASK_DEFAULT as default setting */
  } gni_fma_setpriv_args_t;

/* Configure vMDH */
  typedef struct gni_nic_vmdhconfig_args
  {
    uint16_t blk_base;		/* OUT base MDH of the vMDH block */
    uint16_t blk_size;		/* IN size of the vMDH block */
  } gni_nic_vmdhconfig_args_t;

/* Configure NTT table */
#define GNI_NTT_FLAG_CLEAR              0x01	/* clear entries in NTT table */
#define GNI_NTT_FLAG_SET_GRANULARITY    0x02	/* set granularity */
  typedef struct gni_nic_nttconfig_args
  {
    uint8_t granularity;	/* IN NTT table granularity; 0 - to disable NTT */
    /* app. must set gran. to 0, before changing it */
    uint8_t flags;		/* IN (see flags above) */
    uint32_t base;		/* INOUT Base NTT entry */
    uint32_t num_entries;	/* IN Number of entries to configure */
    union
    {
      uint32_t *entries;	/* IN ignored if FLAG_CLEAR is set */
      gni_ntt_entry_t *entries_v2;	/* IN for aries */
    };
  } gni_nic_nttconfig_args_t;

/* Configure parameters for job */
  typedef struct gni_nic_jobconfig_args
  {
    uint64_t job_id;		/* IN job ID */
    uint32_t ptag;		/* IN protection tag for the given job */
    uint32_t cookie;		/* IN cookie for the given job */
    gni_job_limits_t *limits;	/* IN pointer to the limit's structure */
  } gni_nic_jobconfig_args_t;

/* Configure parameters fot NTT and job in a single ioctl command */
  typedef struct gni_nic_nttjob_config_args
  {
    gni_nic_nttconfig_args_t ntt_config;	/* NTT configuration parameters */
    gni_nic_jobconfig_args_t job_config;	/* Job configuration parameters */
  } gni_nic_nttjob_config_args_t;

/* Configure FMA parameters */
  typedef struct gni_nic_fmaconfig_args
  {
    uint16_t dlvr_mode;		/* IN delivery mode (see GNI_DLVMODE_xxx) */
    union
    {
      uint8_t pe_mask_mode;	/* Deprecated: FMA_CONFIG uses pe_mask_mode=0 regardless of the value specified here  */
      uint8_t fmad_id;		/* FMA sharing: ID of FMA descriptor to configure */
    };
    uint64_t kern_cq_descr;	/* IN kernel agent CQ descriptor */
  } gni_nic_fmaconfig_args_t;

/* Post datagram */
  typedef struct gni_ep_postdata_args
  {
    uint32_t remote_pe;		/* IN address of remote NIC
				   (virtual if NTT was configured for the Job) */
    uint32_t remote_id;		/* IN remote identifier */
    void *in_data;		/* IN data to post */
    uint16_t in_data_len;	/* IN size of the data to post */
    uint64_t post_id;		/* IN id supplied by application identifying this datagram,
				   meaningful only  if use_post_id field is set to 1 */
    uint8_t use_post_id;	/* IN if set to 1, use post_id for matching 
				   with postdata_test/wait, otherwise igore post_id field */
  } gni_ep_postdata_args_t;

#define GNI_EP_PDT_MODE_USE_POST_ID     0x01	/* use post_id for matching with session */
#define GNI_EP_PDT_MODE_PROBE           0x02	/* probe for session but only return post id
						   (if GNI_EP_PDT_MODE_USE_POST_ID is set) or
						   remote_pe and remote_id */
/* Test or Wait for postdata transaction to complete */
  typedef struct gni_ep_postdata_test_args
  {
    uint32_t timeout;		/* IN time(in millisecs) to wait for transaction to complete */
    uint32_t remote_pe;		/* INOUT address of remote NIC
				   (virtual if GNI_CDM_MODE_NTT_ENABLE) */
    uint32_t remote_id;		/* INOUT remote identifier */
    void *out_buff;		/* INOUT buffer for incoming datagram */
    uint16_t buff_size;		/* IN size of the out_buff */
    gni_post_state_t state;	/* OUT current state of the transaction */
    uint8_t modes;		/* IN modes to control how to use kgni_sm_get_state */
    uint64_t post_id;		/* IN id supplied by application identifying this datagram,
				   meaningful only  if use_post_id field is set to 1 */
  } gni_ep_postdata_test_args_t;

/* Terminate postdata transaction */
  typedef struct gni_ep_postdata_term_args
  {
    uint32_t remote_pe;		/* IN address of remote NIC
				   (virtual if GNI_CDM_MODE_NTT_ENABLE) */
    uint32_t remote_id;		/* IN remote identifier */
    uint8_t use_post_id;	/* IN if set to 1, use post_id for matching
				   with session */
    uint64_t post_id;		/* IN id supplied by application identifying this datagram,
				   meaningful only  if use_post_id field is set to 1 */
  } gni_ep_postdata_term_args_t;

/* External memory registration information, for when exmem.h does not
 * exist. This is because the features are developed in parallel. */
#ifndef _exmem_h_
/* cuda-specific arguments for bar1 mapping */
  typedef struct exmem_cuda_bar1_args
  {
    uint64_t p2p_token;
    uint32_t va_token;
  } exmem_cuda_bar1_args_t;

  typedef union exmem_xargs
  {
    exmem_cuda_bar1_args_t cuda;
  } exmem_xargs_t;

  typedef struct exmem_page
  {
    uint64_t phys_addr;
  } exmem_page_t;

  typedef uint64_t exmem_handle_t;
#endif

  typedef exmem_xargs_t gni_mem_xargs_t;

/* Memory registration */
  typedef struct gni_mem_register_args
  {
    uint32_t segments_cnt;	/* IN serves as discriminator for the kernel */
    uint64_t address;		/* IN used if one segment is registered */
    uint64_t length;		/* IN used if one segment is registered */
    gni_mem_segment_t *mem_segments;	/* IN used if several segments are registered */
    uint64_t kern_cq_descr;	/* IN opaque kernel internal reference */
    uint32_t flags;		/* IN mem protection and mem handle virtualization flags */
    uint32_t vmdh_index;	/* IN MDD block index */
    gni_mem_handle_t mem_hndl;	/* OUT memory domain handle */
    gni_mem_xargs_t xargs;	/* IN Extra arguments based on type */
  } gni_mem_register_args_t;

/* Memory registration, backwards compatibility */
  typedef struct gni_mem_register_noxargs
  {
    uint32_t segments_cnt;	/* IN serves as discriminator for the kernel */
    uint64_t address;		/* IN used if one segment is registered */
    uint64_t length;		/* IN used if one segment is registered */
    gni_mem_segment_t *mem_segments;	/* IN used if several segments are registered */
    uint64_t kern_cq_descr;	/* IN opaque kernel internal reference */
    uint32_t flags;		/* IN mem protection and mem handle virtualization flags */
    uint32_t vmdh_index;	/* IN MDD block index */
    gni_mem_handle_t mem_hndl;	/* OUT memory domain handle */
  } gni_mem_register_noxargs_t;

/* Allocate MDD resources */
  typedef struct gni_mem_mddalloc_args
  {
    uint32_t num_entries;	/* IN number MDD entries to be allocated */
  } gni_mem_mddalloc_args_t;

/* Memory de-registration */
  typedef struct gni_mem_deregister_args
  {
    gni_mem_handle_t mem_hndl;	/* IN memory domain handle to be deregistered */
  } gni_mem_deregister_args_t;

/* The GNI_IOC_MEM_QUERY_HNDLS ioctl call information */

/*
 * The GNI_IOC_MEM_QUERY_HNDLS ioctl call can return these errors:
 *      EFAULT  (In uGNI this is mapped to GNI_RC_INVALID_PARAM):
 *              The ioctl argument list is invalid.
 *              The parameters could not be copy from or to user space.
 *      EINVAL  (In uGNI this is mapped to GNI_RC_INVALID_PARAM):
 *              One of the parameters is invalid.
 *      ENOENT  (In uGNI this is mapped to GNI_RC_INVALID_STATE):
 *              The supplied memory descriptor could not be found.
 *      ENODATA (In uGNI this is mapped to GNI_RC_NO_MATCH):
 *              There are no more memory descriptors available.
 *      ENOTTY  (In uGNI this aborts the application):
 *              This is an invalid ioctl request.
 *              
 */

/* The GNI_IOC_MEM_QUERY_HNDLS ioctl parameter list */

  typedef struct gni_mem_query_hndls_args
  {
    gni_mem_handle_t mem_hndl;	/* INOUT memory domain handle */
    uint64_t address;		/* OUT memory address */
    uint64_t length;		/* OUT memory address length */
  } gni_mem_query_hndls_args_t;

/* CQ Create */
  typedef struct gni_cq_create_no_interrupt_args
  {
    gni_cq_entry_t *queue;	/* IN pointer to 4kB aligned space for cq */
    uint32_t entry_count;	/* IN nr of entries fitting into the cq */
    gni_mem_handle_t mem_hndl;	/* IN memory domain handle of cq */
    uint32_t delay_count;	/* IN nr of events until interrupt (block only) */
    gni_cq_mode_t mode;		/* IN mode of operation of CQ (block, non-block) */
    volatile uint64_t *hw_rd_index_ptr;	/* OUT pointer to CqRdIndex in Gemini */
    uint64_t kern_cq_descr;	/* OUT opaque kernel internal reference */
  } gni_cq_create_no_interrupt_args_t;

  typedef struct gni_cq_create_with_interrupt_args
  {
    gni_cq_entry_t *queue;	/* IN pointer to 4kB aligned space for cq */
    uint32_t entry_count;	/* IN nr of entries fitting into the cq */
    gni_mem_handle_t mem_hndl;	/* IN memory domain handle of cq */
    uint32_t delay_count;	/* IN nr of events until interrupt (block only) */
    gni_cq_mode_t mode;		/* IN mode of operation of CQ (block, non-block) */
    volatile uint64_t *hw_rd_index_ptr;	/* OUT pointer to CqRdIndex in Gemini */
    uint64_t kern_cq_descr;	/* OUT opaque kernel internal reference */
    uint8_t allow_user_interrupt_mask;	/* IN allow the creation of the interrupt mask (block only) */
    atomic_t *interrupt_mask;	/* OUT pointer to user interrupt mask atomic */
  } gni_cq_create_with_interrupt_args_t;

  typedef struct gni_cq_create_args
  {
    gni_cq_entry_t *queue;	/* IN pointer to 4kB aligned space for cq */
    uint32_t entry_count;	/* IN nr of entries fitting into the cq */
    gni_mem_handle_t mem_hndl;	/* IN memory domain handle of cq */
    uint32_t delay_count;	/* IN nr of events until interrupt (block only) */
    gni_cq_mode_t mode;		/* IN mode of operation of CQ (block, non-block) */
    volatile uint64_t *hw_rd_index_ptr;	/* OUT pointer to CqRdIndex in Gemini */
    uint64_t kern_cq_descr;	/* OUT opaque kernel internal reference */
    uint8_t allow_user_interrupt_mask;	/* IN allow the creation of the interrupt mask (block only) */
    atomic_t *interrupt_mask;	/* OUT pointer to user interrupt mask atomic */
    size_t cq_size;		/* IN requested size of the CQ in bytes */
    uint64_t cq_user_memory;	/* OUT pointer to cq user memory */
  } gni_cq_create_args_t;

/* CQ Destroy */
  typedef struct gni_cq_destroy_args
  {
    uint64_t kern_cq_descr;	/* IN opaque kernel internal reference */
  } gni_cq_destroy_args_t;

/* CQ Wait Event */
  typedef struct gni_cq_single_wait_event_args
  {
    uint64_t kern_cq_descr;	/* IN opaque kernel internal references */
    uint64_t timeout;		/* INOUT milliseconds to block before returning */
    uint64_t *next_event_ptr;	/* IN pointer to the next event in the CQ */
  } gni_cq_single_wait_event_args_t;

  typedef struct gni_cq_wait_event_args
  {
    uint64_t *kern_cq_descr;	/* IN array: opaque kernel internal references */
    uint64_t timeout;		/* INOUT milliseconds to block before returning */
    uint64_t **next_event_ptr;	/* IN array: pointer to the next event in the CQ */
    uint32_t num_cqs;		/* IN size of above arrays */
    uint32_t which;		/* OUT index of CQ with event */
  } gni_cq_wait_event_args_t;

/* Post RDMA */
  typedef struct gni_post_rdma_args
  {
    uint32_t remote_pe;		/* IN address of the remote NIC
				   (virtual if GNI_CDM_MODE_NTT_ENABLE) */
    gni_post_descriptor_t *post_desc;	/* IN pointer to the post descriptor */
    uint64_t kern_cq_descr;	/* IN opaque specifier for src CQ for transaction */
    uint64_t src_cq_data;	/* IN src CQ data which will make up most of the LOCAL event */
    uint64_t usr_event_data;	/* IN data which will make up most of the GLOBAL & REMOTE event */
  } gni_post_rdma_args_t;

/* Subscribe errors */
  typedef struct gni_subscribe_errors
  {
    gni_error_mask_t mask;	/* IN mask for subscribing to error classes */
    uint32_t EEQ_size;		/* IN size of event queue, zero means reasonable default */
    uint32_t id;		/* OUT EEQ id for err_handle */
  } gni_subscribe_errors_t;

/* Release errors */
  typedef struct gni_release_errors
  {
    uint32_t id;		/* IN EEQ id from err_handle */
  } gni_release_errors_t;

/* Set a new error mask */
  typedef struct gni_set_error_mask
  {
    gni_error_mask_t mask_in;	/* IN mask that will be set */
    gni_error_mask_t mask_out;	/* IN mask that was set */
    uint32_t id;		/* IN EEQ id from err_handle */
  } gni_set_error_mask_t;

/* Check for error event in queue */
  typedef struct gni_get_error_event
  {
    gni_error_event_t event;	/* OUT error event which was at head of queue */
    uint32_t id;		/* IN EEQ id from err_handle */
  } gni_get_error_event_t;

/* Wait for error events to arrive in queue */
  typedef struct gni_wait_error_events_t
  {
    gni_error_event_t *events;	/* IN  array of events */
    uint32_t size;		/* IN  size of users array of events */
    uint32_t timeout;		/* IN  time to wait until events arrive */
    uint32_t num_events;	/* OUT number of events received before timeout */
    uint32_t id;		/* IN EEQ id from err_handle */
  } gni_wait_error_events_t;

/* Set a new error ptag */
  typedef struct gni_set_error_ptag
  {
    gni_error_mask_t ptag;	/* IN ptag that will be set */
    uint32_t id;		/* IN EEQ id from err_handle */
  } gni_set_error_ptag_t;

/* perform a CDM barrier */
  typedef struct gni_cdm_barr_args
  {
    uint32_t count;
    uint32_t timeout;
  } gni_cdm_barr_args_t;

/* DLA modes */
#define GNI_DLA_MODE_NONE               0
#define GNI_DLA_MODE_PERSIST            1
#define GNI_DLA_MODE_DISCARD_USER_OLD   2
#define GNI_DLA_MODE_DISCARD_KERN       3
#define GNI_DLA_MODE_PERSIST_SHARED     4
#define GNI_DLA_MODE_DISCARD_USER_W_MARKERS  5	/* designates support for FORCED_MARKERS */
#define GNI_DLA_MODE_DISCARD_USER_W_SUSPEND  6	/* designates support for SUSPENDING */
#define GNI_DLA_MODE_DISCARD_USER       GNI_DLA_MODE_DISCARD_USER_W_SUSPEND

/* set NIC's DLA attributes */
  typedef struct gni_dla_setattr_args
  {
    uint32_t mode;		/* IN specify DLA mode */
    uint64_t kern_cq_descr;	/* IN opaque specifier for DLA CQ */
    volatile void *fifo_fill_status;	/* OUT address mapped to FIFO fill status */
    uint32_t max_alloc_credits;	/* OUT maximum credits per DLA block */
    uint32_t max_mode_credits;	/* OUT maximum credits for DLA mode */
    uint32_t max_credits;	/* OUT maximum total credits */
    uint32_t alloc_type;	/* OUT configured DLA events (FMA sharing) */
    uint32_t reserved[4];
  } gni_dla_setattr_args_t;

  typedef struct gni_flbte_setattr_args
  {
    atomic_t *tx_desc_free;	/* OUT address mapped to TX counter */
    uint32_t tx_desc_total;	/* OUT total TX descriptors */
    uint32_t bte_chan;		/* OUT BTE channel number */
    uint32_t max_len;		/* OUT maximum FLBTE length allowed */
    uint32_t reserved[3];
  } gni_flbte_setattr_args_t;

/* Manage collective engine resources */
  typedef struct gni_vce_mgmt_args
  {
    uint32_t vce_id;
    uint32_t modes;		/* not used in 1st impl. */
  } gni_vce_mgmt_args_t;

/* CE child types */
  typedef enum
  {
    GNI_CE_CHILD_UNUSED,
    GNI_CE_CHILD_VCE,
    GNI_CE_CHILD_PE
  } gni_ce_child_t;

#define GNI_CE_MAX_CHILDREN             32
/* Configure collective engine resources */
  typedef struct gni_vce_config_args
  {
    uint8_t vce_id;
    uint32_t modes;
    uint8_t parent_vce;
    uint32_t parent_pe;
    uint8_t parent_chid;
    gni_ce_child_t child_types[GNI_CE_MAX_CHILDREN];
    uint32_t child_pes[GNI_CE_MAX_CHILDREN];
    uint32_t child_vces[GNI_CE_MAX_CHILDREN];
    uint64_t kern_cq_descr;
  } gni_vce_config_args_t;

#if defined CRAY_CONFIG_GHAL_GEMINI
  typedef struct gni_fma_win_addrs
  {
    uint64_t put;
    uint64_t put_nwc;
    uint64_t get;
    uint64_t ctrl;
  } gni_fma_win_addrs_t;

#define GNI_FMA_WIN_GET_DLA_HNDL(addrs_ptr)     (NULL)
#define GNI_FMA_WIN_SET_DLA_HNDL(addrs_ptr, p)

#else
  typedef struct gni_fma_win_addrs
  {
    uint64_t put;
    uint64_t put_nwc;
    uint64_t get;
    uint64_t ctrl;
    struct gni_dla_state *dla_hndl;
  } gni_fma_win_addrs_t;

#define GNI_FMA_WIN_GET_DLA_HNDL(addrs_ptr)     (addrs_ptr)->dla_hndl
#define GNI_FMA_WIN_SET_DLA_HNDL(addrs_ptr, p)  ((addrs_ptr)->dla_hndl = (p))
#endif

  typedef struct gni_fma_assign_args
  {
    uint64_t kern_cq_desc;	/* IN ID of CQ to configure into new FMAD */
    uint64_t timeout_ms;	/* IN timeout */
    uint32_t use_inc;		/* IN use count increment */
    uint32_t fmad_id;		/* OUT ID of the FMA descriptor we got assigned */
    uint64_t fma_state;		/* OUT address of FMA state array */
    uint64_t fma_shared;	/* OUT address of FMA shared array */
    uint32_t fma_pid;		/* OUT my PID for FMA sharing */
    uint32_t fma_res_id;	/* OUT my unique ID for FMA sharing */
    uint32_t fma_cpu;		/* OUT the CPU I'm running on */
    uint32_t fma_tsc_khz;	/* OUT tsc_khz from the kernel for timing in FMA sharing */
    gni_fma_win_addrs_t addrs;	/* OUT FMA window addresses */
  } gni_fma_assign_args_t;

  typedef struct gni_fma_umap_args
  {
    uint32_t fmad_id;
    gni_fma_win_addrs_t addrs;	/* OUT FMA window addresses */
  } gni_fma_umap_args_t;

  typedef enum gni_suspend_state
  {
    GNII_SUSPEND_INVALID = 0,
    GNII_SUSPEND_INIT,
    GNII_SUSPEND_PENDING,
    GNII_SUSPEND_READY,
    GNII_SUSPEND_CANCEL,
    GNII_SUSPEND_QUERY
  } gni_suspend_state_t;

  typedef struct gni_sr_wait_event_args
  {
    gni_suspend_state_t state;
    uint32_t timeout_ms;
    uint64_t unused0;
    uint64_t unused1;
  } gni_sr_wait_event_args_t;

  typedef struct gni_sr_suspend_args
  {
    uint8_t ptag;
    uint32_t timeout_ms;
    uint64_t unused0;
    uint64_t unused1;
  } gni_sr_suspend_args_t;

  typedef struct gni_sr_resume_args
  {
    uint8_t ptag;
    uint64_t unused0;
    uint64_t unused1;
  } gni_sr_resume_args_t;

  typedef struct gni_comp_chan_create_args
  {
    int comp_chan_fd;
  } gni_comp_chan_create_args_t;

  typedef struct gni_comp_chan_attach_args
  {
    int comp_chan_fd;
    uint64_t event_data;
    uint64_t cq_id;
  } gni_comp_chan_attach_args_t;

  typedef struct gni_comp_chan_cqarm_args
  {
    uint64_t *cq_ids;
    uint64_t ncq_ids;
  } gni_comp_chan_cqarm_args_t;

/* IOCTL commands */
#define GNI_IOC_MAGIC   'G'

/* Privileged command */
#define GNI_IOC_NIC_SETATTR             _IOWR(GNI_IOC_MAGIC, 1, gni_nic_setattr_args_t)

#define GNI_IOC_NIC_FMA_CONFIG          _IOWR(GNI_IOC_MAGIC, 2, gni_nic_fmaconfig_args_t)

#define GNI_IOC_EP_POSTDATA             _IOWR(GNI_IOC_MAGIC, 4, gni_ep_postdata_args_t)
#define GNI_IOC_EP_POSTDATA_TEST        _IOR(GNI_IOC_MAGIC,  5, gni_ep_postdata_test_args_t)
#define GNI_IOC_EP_POSTDATA_WAIT        _IOWR(GNI_IOC_MAGIC, 6, gni_ep_postdata_test_args_t)
#define GNI_IOC_EP_POSTDATA_TERMINATE   _IOWR(GNI_IOC_MAGIC, 7, gni_ep_postdata_term_args_t)

#define GNI_IOC_MEM_REGISTER            _IOWR(GNI_IOC_MAGIC, 8, gni_mem_register_args_t)
#define GNI_IOC_MEM_OLD_REGISTER        _IOWR(GNI_IOC_MAGIC, 8, gni_mem_register_noxargs_t)

#define GNI_IOC_MEM_DEREGISTER          _IOWR(GNI_IOC_MAGIC, 10, gni_mem_deregister_args_t)

#define GNI_IOC_CQ_CREATE               _IOWR(GNI_IOC_MAGIC, 11, gni_cq_create_args_t)
#define GNI_IOC_CQ_NO_INTR_CREATE       _IOWR(GNI_IOC_MAGIC, 11, gni_cq_create_no_interrupt_args_t)
#define GNI_IOC_CQ_WITH_INTR_CREATE     _IOWR(GNI_IOC_MAGIC, 11, gni_cq_create_with_interrupt_args_t)

#define GNI_IOC_CQ_DESTROY              _IOWR(GNI_IOC_MAGIC, 12, gni_cq_destroy_args_t)

#define GNI_IOC_CQ_WAIT_EVENT           _IOWR(GNI_IOC_MAGIC, 13, gni_cq_wait_event_args_t)
#define GNI_IOC_CQ_OLD_WAIT_EVENT       _IOWR(GNI_IOC_MAGIC, 13, gni_cq_single_wait_event_args_t)

#define GNI_IOC_POST_RDMA               _IOWR(GNI_IOC_MAGIC, 14, gni_post_rdma_args_t)

#define GNI_IOC_NIC_VMDH_CONFIG         _IOWR(GNI_IOC_MAGIC, 15, gni_nic_vmdhconfig_args_t)
#define GNI_IOC_NIC_NTT_CONFIG          _IOWR(GNI_IOC_MAGIC, 16, gni_nic_nttconfig_args_t)

#define GNI_IOC_NIC_JOBCONFIG           _IOWR(GNI_IOC_MAGIC, 17, gni_nic_jobconfig_args_t)

#define GNI_IOC_FMA_SET_PRIVMASK        _IOWR(GNI_IOC_MAGIC, 18, gni_fma_setpriv_args_t)

/* 19 is unused */

#define GNI_IOC_SUBSCRIBE_ERR           _IOWR(GNI_IOC_MAGIC, 20, gni_subscribe_errors_t)
#define GNI_IOC_RELEASE_ERR             _IOWR(GNI_IOC_MAGIC, 21, gni_release_errors_t)
#define GNI_IOC_SET_ERR_MASK            _IOWR(GNI_IOC_MAGIC, 22, gni_set_error_mask_t)
#define GNI_IOC_GET_ERR_EVENT           _IOWR(GNI_IOC_MAGIC, 23, gni_get_error_event_t)
#define GNI_IOC_WAIT_ERR_EVENTS         _IOWR(GNI_IOC_MAGIC, 24, gni_wait_error_events_t)
#define GNI_IOC_SET_ERR_PTAG            _IOWR(GNI_IOC_MAGIC, 25, gni_set_error_ptag_t)
#define GNI_IOC_CDM_BARR                _IOWR(GNI_IOC_MAGIC, 26, gni_cdm_barr_args_t)
#define GNI_IOC_NIC_NTTJOB_CONFIG       _IOWR(GNI_IOC_MAGIC, 27, gni_nic_nttjob_config_args_t)
#define GNI_IOC_DLA_SETATTR             _IOWR(GNI_IOC_MAGIC, 28, gni_dla_setattr_args_t)
#define GNI_IOC_VCE_ALLOC               _IOWR(GNI_IOC_MAGIC, 29, gni_vce_mgmt_args_t)
#define GNI_IOC_VCE_FREE                _IOWR(GNI_IOC_MAGIC, 30, gni_vce_mgmt_args_t)
#define GNI_IOC_VCE_CONFIG              _IOWR(GNI_IOC_MAGIC, 31, gni_vce_config_args_t)
#define GNI_IOC_FLBTE_SETATTR           _IOWR(GNI_IOC_MAGIC, 32, gni_flbte_setattr_args_t)
#define GNI_IOC_FMA_ASSIGN              _IOWR(GNI_IOC_MAGIC, 33, gni_fma_assign_args_t)
#define GNI_IOC_FMA_UMAP                _IOWR(GNI_IOC_MAGIC, 34, gni_fma_umap_args_t)
#define GNI_IOC_GET_PTAG                _IOWR(GNI_IOC_MAGIC, 35, gni_nic_jobconfig_args_t)
#define GNI_IOC_MEM_QUERY_HNDLS         _IOWR(GNI_IOC_MAGIC, 36, gni_mem_query_hndls_args_t)
#define GNI_IOC_SR_WAIT_EVENT           _IOWR(GNI_IOC_MAGIC, 37, gni_sr_wait_event_args_t)
#define GNI_IOC_SR_SUSPEND              _IOWR(GNI_IOC_MAGIC, 38, gni_sr_wait_event_args_t)
#define GNI_IOC_SR_RESUME               _IOWR(GNI_IOC_MAGIC, 39, gni_sr_wait_event_args_t)
#define GNI_IOC_COMP_CHAN_CREATE        _IOWR(GNI_IOC_MAGIC, 40, gni_comp_chan_create_args_t)
#define GNI_IOC_COMP_CHAN_ATTACH        _IOWR(GNI_IOC_MAGIC, 41, gni_comp_chan_attach_args_t)
#define GNI_IOC_COMP_CHAN_CQARM         _IOWR(GNI_IOC_MAGIC, 42, gni_comp_chan_cqarm_args_t)

#define GNI_IOC_MAXNR  42

#ifdef __KERNEL__

#define gni_lock_t              spinlock_t
#define gni_lock(lock)          spin_lock(lock)
#define gni_unlock(lock)        spin_unlock(lock)
#define gni_lock_init(lock)     spin_lock_init(lock)

#define gni_slock_t             spinlock_t
#define gni_slock(lock)         spin_lock(lock)
#define gni_sunlock(lock)       spin_unlock(lock)
#define gni_slock_init(lock)    spin_lock_init(lock)

#else

#if 0
#define gni_lock_t              pthread_mutex_t
#define gni_lock(lock)          pthread_mutex_lock(lock)
#define gni_unlock(lock)        pthread_mutex_unlock(lock)
#define gni_lock_init(lock)     pthread_mutex_init(lock, NULL)

#define gni_slock_t             pthread_spinlock_t
#define gni_slock(lock)         pthread_spin_lock(lock)
#define gni_sunlock(lock)       pthread_spin_unlock(lock)
#define gni_slock_init(lock)    pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE)
#else
#define gni_lock_t              pthread_mutex_t
#define gni_lock(lock)
#define gni_unlock(lock)
#define gni_lock_init(lock)

#define gni_slock_t             pthread_spinlock_t
#define gni_slock(lock)
#define gni_sunlock(lock)
#define gni_slock_init(lock)
#endif

#endif

  inline static uint8_t gni_crc_bits (uint8_t data)
  {
    uint8_t lcrc = 0;

    if (data & 1)
        lcrc ^= 0x5e;
    if (data & 2)
        lcrc ^= 0xbc;
    if (data & 4)
        lcrc ^= 0x61;
    if (data & 8)
        lcrc ^= 0xc2;
    if (data & 0x10)
        lcrc ^= 0x9d;
    if (data & 0x20)
        lcrc ^= 0x23;
    if (data & 0x40)
        lcrc ^= 0x46;
    if (data & 0x80)
        lcrc ^= 0x8c;

      return lcrc;
  }


  inline static uint8_t gni_memhndl_calc_crc (gni_mem_handle_t * memhndl)
  {
    uint64_t qw1 = memhndl->qword1;
    uint64_t qw2 = memhndl->qword2;
    uint8_t crc = 0;
    crc = gni_crc_bits ((qw1 ^ crc) & 0xff);
    crc = gni_crc_bits (((qw1 >> 8) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw1 >> 16) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw1 >> 24) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw1 >> 32) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw1 >> 40) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw1 >> 48) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw1 >> 56) ^ crc) & 0xff);
    crc = gni_crc_bits ((qw2 ^ crc) & 0xff);
    crc = gni_crc_bits (((qw2 >> 8) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw2 >> 16) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw2 >> 24) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw2 >> 32) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw2 >> 40) ^ crc) & 0xff);
    crc = gni_crc_bits (((qw2 >> 48) ^ crc) & 0xff);

    return crc;
  }

  inline static uint8_t gni_memhndl_cookie_crc (uint64_t value)
  {
    uint8_t crc = 0;
    crc = gni_crc_bits ((value ^ crc) & 0xff);
    crc = gni_crc_bits (((value >> 8) ^ crc) & 0xff);
    crc = gni_crc_bits (((value >> 16) ^ crc) & 0xff);
    crc = gni_crc_bits (((value >> 24) ^ crc) & 0xff);
    crc = gni_crc_bits (((value >> 32) ^ crc) & 0xff);
    crc = gni_crc_bits (((value >> 40) ^ crc) & 0xff);
    crc = gni_crc_bits (((value >> 48) ^ crc) & 0xff);
    crc = gni_crc_bits (((value >> 56) ^ crc) & 0xff);

    return crc;
  }

#ifndef __KERNEL__
/**
 * GNII_RemoteMemHndl - Setup remote memory handle
 *
 * Parameters:
 * IN
 * pa        The physical address to start the memory handle at
 * mdh       The destination mdh to use
 * npages    The number of pages assigned to the mdh
 *
 * INOUT
 * mem_hndl  The new memory handle for the region.
 *
 * Returns:
 * void
 *
 * Description:
 *
 * This is a private function available to Cray specific tools which
 * need to reference memory on a remote node, which may not have a
 * functioning OS. For example, two consumers are BND, Boot Node
 * Daemon, and hsn-proxy-gni. This function is declared in gni_priv.h,
 * and exist so library versions maintain control over the memory
 * handle. The library can verify versioning with the running kernel
 * better than individual apps, regardless of dynamic/static
 * libraries.
 **/
  void
    GNII_RemoteMemHndl (INOUT gni_mem_handle_t * mem_hndl,
			IN uint64_t pa, IN int mdh, IN int npages);
#endif

/**
 * Run-time type information for GNI handles.
 *
 * A gni_type_id_t field is added to each GNI
 * handle in order to perform run-time type checking.
 */
  typedef unsigned int gni_type_id_t;

#define GNI_TYPE_INVALID     0x00000000
#define GNI_TYPE_CDM_HANDLE  0xABCD0000
#define GNI_TYPE_NIC_HANDLE  0xABCD0001
#define GNI_TYPE_EP_HANDLE   0xABCD0002
#define GNI_TYPE_CHNL_HANDLE 0xABCD0003
#define GNI_TYPE_CQ_HANDLE   0xABCD0004
#define GNI_TYPE_ERR_HANDLE  0xABCD0005
#define GNI_TYPE_MSGQ_HANDLE 0xABCD0006
#define GNI_TYPE_CE_HANDLE   0xABCD0007
#define GNI_TYPE_COMPCHAN_HANDLE 0xABCD0008

  typedef enum
  {
    GNI_CDM_TYPE_KERNEL = 0,
    GNI_CDM_TYPE_USER,
    GNI_CDM_TYPE_MAX		/* must be the last one */
  } gni_cdm_type_t;

  typedef struct gni_mdd_block
  {
    uint16_t blk_base;		/* base MDH if the MDD block */
    uint16_t blk_size;		/* size of the MDD block */
    uint16_t is_allocated;	/* flag indicating whether block has been allocd */
  } gni_mdd_block_t;

#ifdef __KERNEL__
  typedef struct gni_child_desc
  {
    struct mm_struct *mm;
    gni_list_t umap_list;
    gni_list_t list;
  } gni_child_desc_t;

  typedef struct gni_umap_desc
  {
    volatile void *address;
    uint64_t size;
    gni_list_t list;
    struct
    {
      uint32_t no_copy:1;
      uint32_t type:31;
    };
  } gni_umap_desc_t;

#define GNI_UMAP_TYPE_UNSPECIFIED	0
#define GNI_UMAP_TYPE_FMA		1

  typedef struct gni_eeq
  {
    uint32_t id;
    uint32_t read;
    uint32_t write;
    uint32_t size;
    gni_error_event_t *events;
    wait_queue_head_t waitaddr;
    void (*new_ev_f) (gni_err_handle_t);
    void (*app_crit_err) (gni_err_handle_t);
    struct list_head list;
    gni_error_mask_t mask;
    uint8_t ptag;
    gni_err_handle_t err_handle;
  } gni_eeq_t;

  typedef struct gni_err
  {
    gni_type_id_t type_id;
    gni_nic_handle_t nic_handle;
    gni_eeq_t *eeq;
  } gni_err_t;

#else /* __KERNEL__ */

  typedef struct gni_err
  {
    gni_type_id_t type_id;
    gni_error_mask_t error_mask;
    uint32_t device_hndl;
    uint32_t privileged;
    uint32_t eeq_id;
  } gni_err_t;

#endif /* __KERNEL__ */

#define GNI_DLA_NUM_CONFIG_PARAMS 16
  typedef union gni_dla_config_params
  {
    struct
    {
      int max_fma_size;
      int bytes_per_block;
      int extra_credits_per_block;
      int min_total_blocks;
      int status_interval;
      int pr_credits_percent;
      int alloc_status_retry;
      int alloc_status_multiple;
      int alloc_status_threshold;
      int overflow_max_retries;
      int overflow_percent;
      int min_credits_per_block;
      int put_wc_threshold;
      int fmad_num_blocks;
      int unused1;
      int unused2;
    };
    int params[GNI_DLA_NUM_CONFIG_PARAMS];
  } gni_dla_config_params_t;

/* DLA structures */
  typedef struct gni_dla_block
  {
    struct
    {
      uint32_t fmad_id:8;
      uint32_t state:2;
      uint32_t reissued:1;
      uint32_t last_block:1;
      uint32_t fma_block_id:20;
    };
    union
    {
      int32_t fma_req_id;
      int32_t local_block_id;
    };
    union
    {
      gni_cq_handle_t cq_hndl;
      uint64_t cq_id;
    };
  } gni_dla_block_t;

/* for FMA sharing, access to these is using a 64-bit atomic
 * as a critical section.
 * This structure helps so the code isn't a complete mess where
 * the atomics are used.
 */
  typedef union gni_dla_cs
  {
    volatile struct
    {
      uint32_t next_free_blk;
      int32_t discard_reset_id;
    };
    atomic64_t atomic_discard_reset_next_free_blk;
    uint64_t qw;
  } gni_dla_cs_t;

  typedef struct gni_dla_state
  {
    gni_dla_block_t *blocks;
    struct gni_dla_state *fmad_dla_hndl;
    volatile void *alloc_status;
    int32_t num_blocks;
    union
    {
      int32_t num_free;
      atomic_t atomic_num_free;
    };
    uint32_t first_assigned_blk;
    int32_t first_discarded_block;
    gni_dla_cs_t cs;		/* next_free_blk, discard_reset_id */
    uint32_t dla_bytes_per_block;
    uint16_t status_interval;
    uint16_t oversubscribed_status_interval;
    uint16_t mode;
    uint16_t alloc_type;
    uint32_t allocatable_credits;
    uint32_t max_credits;
  } gni_dla_state_t;

/* FMA Sharing structures */
  typedef union gni_fma_state
  {
    struct
    {
      uint64_t pid:32;
      uint64_t res_id:8;
      uint64_t assigned:1;
      uint64_t use_cnt:1;
      uint64_t restricted:1;
      uint64_t suspended:1;
      uint64_t sr_pending:1;
      uint64_t unused:11;
      uint64_t fmad_id:8;
    };
    uint64_t qw;
    atomic64_t atomic_qw;
  } gni_fma_state_t;

#ifndef BITS_TO_LONGS
#define BITS_TO_LONGS(b)  (((b) + 63) / 64)
#endif

  typedef struct gni_fma_shared
  {
    unsigned long desc_bitmap[BITS_TO_LONGS (GHAL_FMA_TOTAL_NUMBER)];
    uint32_t generation;
  } gni_fma_shared_t;
#define GNI_FMA_SHARED_GENERATION_OFFSET	(sizeof(unsigned long) * BITS_TO_LONGS(GHAL_FMA_TOTAL_NUMBER))

#ifdef __KERNEL__
#define GNI_FMA_STATE_PTR(base,i)		\
	((gni_fma_state_t *)(((void *)((base))) + (i * GNI_CACHELINE_SIZE)))
#define GNI_NIC_FMA_STATE_PTR(nic,i)		GNI_FMA_STATE_PTR((nic)->kern_fma_state,i)
#define GNI_FMA_SHARED_PTR(nic)			((nic)->kern_fma_shared)
#else
#define GNI_NIC_FMA_STATE_PTR(nic,i)		\
	((gni_fma_state_t *)(((void *)((nic)->fma_state)) + (i * GNI_CACHELINE_SIZE)))
#define GNI_FMA_SHARED_PTR(nic)			((nic)->fma_shared)
#endif

#define GNI_FMA_STATE_PTR_IS_OWNER(s, nic_hndl)	\
	((s)->use_cnt && (s)->assigned && !(s)->suspended && (s)->pid == (nic_hndl)->fma_pid)
#define GNI_FMA_STATE_PTR_IS_RESTRICTED(s)	\
	((s)->restricted)
#define GNI_FMA_STATE_PTR_IS_SUSPENDED(s)	\
	((s)->suspended)
#define GNI_FMA_STATE_PTR_IS_SR_PENDING(s)	\
	((s)->sr_pending)

#define GNI_FMAD_MAP_QWS			\
	((GHAL_FMA_TOTAL_NUMBER + BITS_PER_LONG) / BITS_PER_LONG)

  typedef struct gni_fmad_map
  {
    unsigned long ary[GNI_FMAD_MAP_QWS];
  } gni_fmad_map_t;

#define GNI_MAX_NICS_ATTACHED   32
#define GNI_CDM_INVALID_ID      (-1)
#define GNI_NIC_INVALID_ID      (-1)
#define GNI_VMDH_INVALID_ID     (-1)
#define GNI_FLBTE_INVALID_CHAN  (-1)
/* NIC instance descriptor, fields with FIXED should not be moved */
  typedef struct gni_nic
  {
    gni_type_id_t type_id;	/* Run-time type information */
    gni_cdm_handle_t cdm_hndl;	/* handle of cdm to which this nic handle is attached */
    uint32_t device_hndl;	/* handle returned by open() */
    uint32_t nic_pe;		/* physical address of the Gemini NIC */
    uint32_t inst_id;
    uint32_t cookie;
    uint32_t modes;		/* supplied in GNI_CdmCreate (GNI_CDM_MODE_xxxxxx) */
    uint8_t ptag;
    volatile void *fma_window;	/* virtual address mapped to FMA window */
    volatile void *fma_window_nwc;	/* virtual address mapped to FMA PUT non-WC window */
    volatile void *fma_window_get;	/* virtual address mapped to FMA GET window */
    volatile void *datagram_counter;	/* virtual address mapped to datagram_counter  */
    volatile void *fma_ctrl;	/* virtual address mapped to FMA control page */
    volatile void *fma_desc_cpu;	/* place holder right now for copy of fma desc in host memory */
    uint64_t fma_window_size;	/* size of the FMA window */

    /* TODO Currently a work queue is associated with NIC to save space since there could
       be 10s of thousands of EPs; associating it with EP would make searches faster; 
       the current choice may need to be reviewed later. */
    gni_cq_handle_t current_src_cq_hndl;	/* cq that is currently stored in the fma desc */
    gni_mdd_block_t *mdd_blk;	/* info about MDD block */
    uint16_t smsg_max_retrans;	/* max retransmit count for SMSG transactions */
    uint32_t dla_last_reissued_block;
    uint32_t dla_last_forced_marker;
    gni_cq_handle_t non_active_dla_cq_hndl;
    gni_cq_handle_t dla_cq_hndl;	/* dla event queue (FIXED) */

    /* DON'T MOVE ANYTHING ABOVE, as will break DMAPP/UGNI compatibility */

    uint16_t pkey;
    gni_dla_state_t *dla_hndl;	/* dla state machine handle (uGNI only) */
    uint64_t fma_threshold;	/* threshold to start FMA throttling */
    uint32_t fma_segment_size;	/* FMA max single transaction size */
    uint32_t fma_delay_value;	/* usec to use for FMA throttling */
    uint8_t cq_revisit_mode;
    gni_list_t cq_list;
    atomic_t *flbte_tx_desc_free;	/* FLBTE TXD counter */
    uint32_t flbte_tx_desc_total;	/* total FLBTE TX descriptors */
    uint32_t flbte_chan;	/* assigned FLBTE channel (VCID) */
    uint32_t flbte_max_len;

    /* FMA sharing fields */
    uint32_t fma_mode;		/* FMA allocation mode */
    int32_t fmad_id;		/* FMA descriptor ID, only valid between FmaGet/Put calls */
    uint32_t fma_use_cnt;	/* use_cnt reflected in the last or currently assigned FMA state entry */
    uint32_t fma_pid;		/* PID used in fma state entry */
    uint32_t fma_res_id;	/* instance unique ID used in fma state entry */
    gni_fma_win_addrs_t *fma_windows;	/* FMA window addresses for all FMA descriptors configured for use in the process owning this NIC */
    gni_fma_state_t *fma_state;	/* pointer to the base of the FMA state array */
    void *fma_shared;		/* pointer to the array of 'shared FMA available' flags */
    uint32_t *fma_generation;
    uint32_t last_fma_generation;
    uint32_t fma_cpu;		/* last read CPU that the process using this NIC was running on */
    uint32_t fma_numa;		/* the NUMA node ID corresponding to fma_cpu */
    uint32_t fma_cpu_cnt;	/* the number of CPUs on this machine */
    uint32_t fma_numa_cnt;	/* the number of NUMA nodes on this machine */
    uint32_t fma_tsc_khz;
    struct
    {
      uint16_t fma_desc_cmd:8;
      uint16_t fma_desc_rc:7;
      uint16_t fma_desc_wc:1;
    };
    uint16_t fma_desc_lmdh;
    uint16_t fma_desc_rmdh;

    uint16_t smsg_dlvr_mode;	/* SMSG delivery mode */
    uint32_t my_id;
    int fma_nopriv_masked;
    gni_lock_t fma_lock;
#ifndef __KERNEL__
    uint32_t stats[GNI_NUM_STATS];
    void *cuda_context;
    uint64_t cuda_p2pToken;
    uint32_t cuda_vaToken;
    int ordered_tail;
#else
    void *kdev_hndl;
    struct pci_dev *pdev;
    void *resources;
    gni_cdm_type_t cdm_type;
    uint32_t cdm_id;
    uint32_t ntt_grp_size;	/* NTT group size (if GNI_CDM_MODE_NTT_ENABLE) */
    uint32_t ntt_base;		/* base entry in NTT to use for (if GNI_CDM_MODE_NTT_ENABLE) */
    uint32_t dla_mode;
    uint64_t dla_hw_cq_hndl;
    struct gni_dla_state *fmad_dla_hndl;
    uint64_t ph_fma_win_addr;
    uint64_t ph_fma_ctrl_addr;
    void *sm_chnl_hndl;		/* session management channel */
    void **sm_pkts;
    uint32_t sm_pkt_id;
    void *gart_domain_hndl;
    void *gart_domain_hndl_kern;
    wait_queue_head_t sm_wait_queue;
    struct mmu_notifier mmu_not;
    struct mmu_notifier mdh_mmu_not;	/* For Mem Segment Registration */
    struct mm_struct *mm;
    pid_t pid;
    uint32_t eeq_generation;
    spinlock_t eeqs_lock;
    struct list_head eeqs;
    struct list_head error_list;
    gni_err_handle_t err_handle;	/* For killing apps */
    struct list_head qsce_list;
    void (*qsce_func) (gni_nic_handle_t, uint64_t);
    uint64_t *flsh_page;
    uint8_t vces[GHAL_VCE_MAX_NUMBER];
    uint32_t vmdh_entry_id;
    struct mutex mmu_not_lock;	/* For MMU notifier setup */
    atomic_t mmu_mdhs;		/* For MMU notifier initialization (doesn't have to be an atomic) */
    gni_list_t child_list;
    gni_list_t umap_list;
    gni_umap_desc_t *rw_page_desc;	/* address of PAGE_SIZE shared page */
    gni_umap_desc_t *dla_page_desc;	/* address mapped to FIFO Fill status */
    gni_umap_desc_t *cq_interrupt_mask_desc;	/* user space address for cq interrupt mask */
    gni_fma_state_t *kern_fma_state;
    void *kern_fma_shared;
    int ghal_part;
    int sr_init;
    uint32_t sr_gen;
#endif				/* __KERNEL__ */
  } gni_nic_t;

#define GNI_NIC_FMA_MODE_INVAL          0
#define GNI_NIC_FMA_MODE_SHARED         1
#define GNI_NIC_FMA_MODE_DEDICATED      2
#define GNI_NIC_FMA_MODE_MASK           (GNI_NIC_FMA_MODE_SHARED | GNI_NIC_FMA_MODE_DEDICATED)
#define GNI_NIC_FMA_FLAG_INIT           4

#define _GNI_NIC_FMA_SET_MODE(nic,mode) \
        ((nic)->fma_mode = ((nic->fma_mode & ~GNI_NIC_FMA_MODE_MASK) | ((mode) & GNI_NIC_FMA_MODE_MASK)))
#define _GNI_NIC_FMA_GET_MODE(nic)      ((nic)->fma_mode & GNI_NIC_FMA_MODE_MASK)

#define GNI_NIC_FMA_SET_INVAL(nic)      (_GNI_NIC_FMA_SET_MODE(nic, GNI_NIC_FMA_MODE_INVAL))
#define GNI_NIC_FMA_INVAL(nic)          (_GNI_NIC_FMA_GET_MODE(nic) == GNI_NIC_FMA_MODE_INVAL)
#define GNI_NIC_FMA_SET_SHARED(nic)     (_GNI_NIC_FMA_SET_MODE(nic, GNI_NIC_FMA_MODE_SHARED))
#define GNI_NIC_FMA_SHARED(nic)         (_GNI_NIC_FMA_GET_MODE(nic) == GNI_NIC_FMA_MODE_SHARED)
#define GNI_NIC_FMA_SET_DEDICATED(nic)  (_GNI_NIC_FMA_SET_MODE(nic, GNI_NIC_FMA_MODE_DEDICATED))
#define GNI_NIC_FMA_DEDICATED(nic)      (_GNI_NIC_FMA_GET_MODE(nic) == GNI_NIC_FMA_MODE_DEDICATED)

#define GNI_NIC_FMA_SET_GENERATION(nic) ((nic)->last_fma_generation = *((nic)->fma_generation))
#define GNI_NIC_FMA_SET_INIT(nic)       ((nic)->fma_mode |= GNI_NIC_FMA_FLAG_INIT)
#define GNI_NIC_FMA_CLEAR_INIT(nic)     ((nic)->fma_mode &= ~GNI_NIC_FMA_FLAG_INIT)
#define GNI_NIC_FMA_INIT(nic)           ((nic)->fma_mode & GNI_NIC_FMA_FLAG_INIT)

/* If this line causes a compilation error, don't fix this line! The
 * problem is with the above gni_nic_t structure definition. The
 * structure was modified above the DON'T ALTER ANYTHING ABOVE
 * line. This is here to ensure correctness programmatically.
 * To repeat, never edit the line below for compatibility reasons. */
  extern char gni_nic_assert[1 -
			     2 *
			     !(__builtin_offsetof
			       (struct gni_nic, dla_cq_hndl) == 136)];

#ifdef __KERNEL__
#ifndef CONFIG_MMU_NOTIFIER
/* Our driver now requires MMU notifiers to function with all
 * features. If for any reason, we are on a kernel with this disabled,
 * throw a compilation error. */
#error "kgni requires CONFIG_MMU_NOTIFIER"
#endif
#endif

/* Communication domain descriptor */
  typedef struct gni_cdm
  {
    gni_type_id_t type_id;	/* Run-time type information */
    gni_nic_handle_t nic_hndls[GNI_MAX_NICS_ATTACHED];
    uint32_t inst_id;
    uint32_t cookie;
    uint32_t modes;
    uint8_t ptag;

#ifdef __KERNEL__
    spinlock_t lock;
    struct list_head sessions;	/* list of sessions in progress */
    int my_id;
#else
    uint8_t forked;
    gni_list_t cdm_list;	/* allows ugni to have list of CDMs */
#endif				/* __KERNEL__ */
  } gni_cdm_t;

#ifndef FLOOR
#define FLOOR(a,b)      ((a) - ((a)%(b)))
#endif
#ifndef CEILING
#define CEILING(a,b)    ((a) <= 0 ? 0 :  (FLOOR((a)-1,b) + (b)))
#endif

/*
 * wildcards for EP's remote_pe and remote_id fields
 */
#define GNII_REMOTE_PE_ANY -1
#define GNII_REMOTE_ID_ANY -1

/* Returns true if the PE and ID are non wildcard (the EP is bound) */
#define GNI_EP_BOUND(rem_pe, rem_id) \
		((rem_pe) != GNII_REMOTE_PE_ANY && (rem_id) != GNII_REMOTE_ID_ANY)

#define GNII_REQ_ID_OFFSET 1

  enum
  {
    GNII_REQ_TYPE_INACTIVE = 0,
    GNII_REQ_TYPE_POST = 1,
    GNII_REQ_TYPE_SMSG = 2,
    GNII_REQ_TYPE_BACK_CREDIT = 3,
    GNII_REQ_TYPE_IGNORE = 4,
    GNII_REQ_TYPE_CANCEL = 5,
    GNII_REQ_TYPE_MSGQ = 6,
    GNII_REQ_TYPE_MSGQ_BACK_CREDIT = 7,
    GNII_REQ_TYPE_CT_POST = 8
  };

/* message queue send header */
  typedef struct gni_msgq_snd_hdr
  {
    uint32_t snder_msgq_id;
    uint32_t rcver_msgq_id;
  } gni_msgq_snd_hdr_t;

  typedef struct gnii_smsg_req
  {
    void *header;
    void *data;
    uint64_t mbox_write_generation;
    uint64_t buffer_generation;
    uint32_t header_length;
    uint32_t data_length;
    uint32_t msg_id;
    uint32_t buffer_write_index;
    int32_t retransmit_count;
    uint32_t buffer_credits_needed;
    uint16_t mbox_write_index;
    uint16_t back_mbox_credits;
    uint16_t back_buffer_credits;
    uint16_t s_seqno;
#ifndef __KERNEL__
    uint8_t tag;
#endif

  } gnii_smsg_req_t;

  typedef struct gnii_post_req
  {
    gni_post_descriptor_t *desc;
    uint64_t user_post_id;
  } gnii_post_req_t;

  typedef struct gnii_back_credit_req
  {
    uint16_t mbox_credits_sent;
    uint16_t buffer_credits_sent;
    uint16_t mb_seqno;
    uint16_t buffer_seqno;
    uint32_t retransmit_count;
#ifndef __KERNEL__
    /* msgq fields */
    void *msgq_hndl;
    void *msgq_conn;
    gni_msgq_snd_hdr_t msgq_hdr;
#endif
  } gnii_back_credit_req_t;

/* request descriptor */
  typedef struct gnii_req
  {
    union
    {
      gnii_smsg_req_t smsg;
      gnii_post_req_t post;
      gnii_back_credit_req_t bc;
    } u;
    gni_dla_state_t *dla_hndl;
    int32_t needed_fmad_id;
    uint32_t num_dla_blocks;
    uint32_t pending_dla_blocks;
    uint32_t reissued_dla_blocks;
    uint32_t dla_last_block_id;
    uint32_t fmad_dla_last_block_id;
    uint16_t dla_last_block_credits;
    uint16_t dla_credits_per_block;
    uint32_t dla_retransmit_count;
    uint8_t dla_overflow;
    uint8_t cqes_pending;
    uint8_t type;		/* type of request, i.e. post or smsg */
    uint8_t err_inject;
    uint32_t id;		/* index of request */
    uint32_t local_event;
    uint32_t remote_event;
    gni_ep_handle_t ep_hndl;	/* ep handle associated with this ref */
  } gnii_req_t;

#define GNII_REQ_CLEAR_DLA_STATE(req)        \
	(req)->needed_fmad_id = -1;          \
	(req)->num_dla_blocks = 0;           \
	(req)->pending_dla_blocks = 0;       \
	(req)->reissued_dla_blocks = 0;      \
	(req)->dla_hndl = NULL;              \
	(req)->dla_last_block_credits = 0;   \
	(req)->dla_retransmit_count = 0;     \
	(req)->dla_overflow = 0;
#define GNII_REQ_IS_REISSUED(req)         ((req) && ((req)->reissued_dla_blocks > 0))
#define GNII_REQ_PENDING_BLOCKS(req)		                 \
	GNII_REQ_IS_REISSUED(req) ? req->reissued_dla_blocks :   \
	((req) ? req->num_dla_blocks : 0)

  typedef struct gnii_req_list
  {
    uint32_t nreqs_max;		/* maximum number of active requests allowed */
    uint32_t freehead;		/* current head of req free list */
    gnii_req_t *base;		/* base of vector of requests */
    gnii_req_t **freelist;	/* the freelist */
    gni_slock_t api_lock;
  } gnii_req_list_t;

/* Endpoint short message descriptor */

  typedef struct gni_ep_smsg_mbox
  {
    gni_mem_handle_t remote_mem_hndl;	/* memory domain handle remote process' mailbox */
    uint64_t remote_mdh_offset;	/* remote mail box offset within memory domain of remote mailbox  */
    uint64_t mbox_write_generation;	/* cycle count through remote mailbox, used for retransmit */
    uint64_t mbox_read_generation;	/* cycle count through local mailbox, used for retransmit */
    uint64_t buffer_generation;	/* cycle count through the buffer, used for retransmit */
    void *mailbox;		/* pointer to base of local mailbox */
    uint32_t buffer_credits;	/* number of buffer send credits */
    uint32_t buffer_write_index;	/* index of current write mailbox */
    uint32_t buffer_max_write_index;	/* maximum  buffer write index */
    uint32_t back_buffer_credits_thresh;	/* threshold to return buffer credits directly from receiver */
    uint32_t back_buffer_credits;	/* number of back buffer credits from remote process */
    atomic_t retransmit_count;	/* count of retransmitted message and back credit requests in flight */
    uint16_t back_mbox_credits;	/* number of back mbox credits from remote process */
    uint16_t msg_maxsize;	/* maximum size of short message */
    uint16_t mbox_credits;	/* number of mbox send credits */
    uint16_t mbox_write_index;	/* mbox write index  */
    uint16_t mbox_read_index;	/* mbox write index  */
    uint16_t mbox_maxcredit;	/* maximum number of mbox send credits */
    uint16_t mbox_allocated;	/* number of mboxes physically allocated */
    uint16_t mbox_mc_offset;	/* offset into local mailbox where mbox credits are delivered */
    uint16_t mbox_bc_offset;	/* offset into local mailbox where buffer credits are delivered */
    uint16_t s_seqno;		/* send sequence number */
    uint16_t r_seqno;		/* expected recv sequence number */
    uint16_t s_seqno_back_mbox_credits;	/*send seqno for back mbox credit updates */
    uint16_t r_seqno_back_mbox_credits;	/*expected send seqno for back mbox credit updates */
    uint16_t s_seqno_back_buffer_credits;	/*send seqno for back buffer credit updates */
    uint16_t r_seqno_back_buffer_credits;	/*expected send seqno for back buffer credit updates */
    gnii_req_t *mbox_update_pending;	/* pending mbox credit update */
    gnii_req_t *buffer_update_pending;	/* pending buffere credit update */

    /* TODO: the ipc_shared field is deprecated and will be removed on the
     * next GNI minor version update */
    uint8_t ipc_shared;		/* = 1 if mbox is ipc shared */
    gni_lock_t api_lock;
  } gni_ep_smsg_mbox_t;

/* Endpoint descriptor */
  typedef struct gni_ep
  {
    gni_type_id_t type_id;	/* Run-time type information */
    gni_nic_handle_t nic_hndl;	/* nic handle this EP is associated with */
    uint32_t remote_pe;		/* address of the remote NIC 
				   (virtual if GNI_CDM_MODE_NTT_ENABLE) */
    uint32_t remote_id;		/* remote identifier */
    uint32_t remote_event;	/* remote event data, by default = inst_id of endpoint.  see GNI_EpSetEventData */
    uint32_t local_event;	/* local event data, by default remote_id of endpoint.  see GNI_EpSetEventData */
    gni_cq_handle_t src_cq_hndl;	/* cq hndl specified when this ep was created */
    atomic_t outstanding_tx_reqs;	/* number of outstanding tx reqs associated with this ep */
    void *datagram_out_buf;	/* pointer to outbuf for current datagram on this ep */
    uint16_t datagram_buf_size;	/* size of outbuf for current datagram on this ep (in bytes) */
    uint8_t pending_datagram;	/* 1 if pending datagram, otherwise 0 */
    void *smsg;			/*  pointer to short message struct */
    gni_smsg_type_t smsg_type;	/* short messaging type */
    gni_list_t blocked_list;
    uint8_t err_inject_rate;

#ifndef __KERNEL__
    void *msgq_conn;		/*  pointer to a MSGQ connection */
    gni_msgq_snd_hdr_t msgq_hdr;
    uint8_t vce_id;
    uint8_t vce_child_id;
    gni_ce_child_t vce_child_type;
#endif
  } gni_ep_t;

  struct gni_comp_chan
  {
    uint32_t type_id;
    int comp_chan_fd;
  };

#define CQ_INTERRUPT_MASK_NOT_ASSIGNED 0
#define CQ_INTERRUPT_MASK_INTERRUPTIBLE  1
#define CQ_INTERRUPT_MASK_INVALID_INDEX  -1

/* Definitions for a CQ with blocking mode timeout delay intervals. */
#define GNI_DEFAULT_CQ_BLOCKED_DELAY_INTERVAL 100	/* in milliseconds, .1  seconds */
#define GNI_MAX_CQ_BLOCKED_DELAY_INTERVAL     10000	/* in milliseconds, 10  seconds */
#define GNI_MIN_CQ_BLOCKED_DELAY_INTERVAL     10	/* in milliseconds, .01 seconds */

/* Maximum allowed page order for a CQ using physical pages. */
#define GNI_CQ_MAX_PHYS_PAGE_ORDER  4

/* Completion queue descriptor, fields with FIXED should not be moved */
  typedef struct gni_cq
  {
    gni_type_id_t type_id;	/* Run-time type information */
    gni_nic_handle_t nic_hndl;	/* nic handle this CQ is associated with */
    volatile gni_cq_entry_t *queue;	/* circular queue of completion entries */
    gni_mem_handle_t mem_hndl;	/* mem. handle of the queue buffer */
    volatile uint64_t *hw_rd_index_ptr;	/* pointer to CqRdIndex in Gemini */
    void *req_list;		/* pointer to req list structure tracking active requests */
    void (*handler) (gni_cq_entry_t *, void *);
    /* pointer to user supplied callback funcion */
    void *context;		/* pointer to context provided as second argument to handler
				   callback function */
    uint32_t entry_count;	/* number of entries in the queue */
    uint32_t ep_ref_count;	/* count of associated ep's */
    uint32_t read_index;	/* index of next entry in the queue to be read */
    gni_cq_mode_t mode;		/* mode of operation of CQ (block, non-block) */
    uint64_t blocked_delay_interval;	/* for a blocked mode CQ this is the timeout delay interval */
    gni_list_t blocked_eps;
    uint8_t blocked_eps_locked;
    uint8_t mdh_attached;	/* 1 if CQ associated with mdh */
    uint64_t kern_cq_descr;	/* kernel agent CQ descriptor (FIXED) */
    void (*queue_dtor) (void *, void *);	/* pointer to queue space destructor,
						   invoked in GNI_CqDestroy */
    void *queue_dtor_data;	/* pointer to auxiliary data needed by queue space
				   destructor */
    gni_list_t nic_list;
    uint8_t cache_revisit_shift;	/* for Aries strided CQEs (FIXED) */
    /* DON'T ALTER ANYTHING ABOVE, as will break DMAPP/UGNI compatibilty */
    atomic_t *interrupt_mask;	/* pointer to kernel interrupt mask atomic */
    uint32_t iommu_tcr;		/* the iommu tcr value associated with the allocated memory */
    int request_count;		/* size of CQ request list */
    int update_thresh;		/* CQ read index update threshold */
    int update_count;		/* number of skipped CQ read index updates */
    gni_slock_t api_lock;
#ifdef __KERNEL__
    void *mem_ptr;		/* pointer to allocated memory */
    uint64_t mem_size;		/* size of the allocated memory */
    uint32_t hw_cq_hndl;	/* CQ descr. number inside the NIC */
    uint32_t irq_index;		/* IRQ index this CQ is registered with */
    gni_cq_event_hndlr_f *event_handler;	/* event handler */
    uint64_t usr_event_data;	/* data provided by user along with handler */
    uint8_t uspace_mapped;	/* used by user level appl. */
    uint32_t write_index;	/* write index (EMULATED CQ) */
    uint32_t last_overrun;	/* OVERRUN set (EMULATED CQ) */
    spinlock_t cq_lock;		/* lock for write_index updates (EMULATED CQ) */
    struct page **mem_pages;	/* pages array (EMULATED CQ) */
    volatile void *cq_window_addr;	/* mapped CQ desc. alias */
    wait_queue_head_t wait_queue;
    atomic64_t active_wait_queue;
    struct mm_struct *mm;
    gni_umap_desc_t *umap_desc;
    gni_umap_desc_t *phys_pages_umap_desc;	/* pointer to user memory descriptor for CQ entries */
    atomic_t outstanding_wait_requests;	/* the number of outstanding wait event requests atomic */
    atomic_t destroy_in_progress;	/* a cq destroy request has been issued. */
    int comp_chan_fd;
    void *comp_chan_file;
    uint64_t comp_chan_event_data;
    atomic_t comp_chan_armed;
    struct list_head comp_chan_list;
#endif
  } gni_cq_t;

/* If this line causes a compilation error, don't fix this line! The
 * problem is with the above gni_cq_t structure definition. The
 * structure was modified above the DON'T ALTER ANYTHING ABOVE
 * line. This is here to ensure correctness programmatically. 
 * To repeat, never edit the line below for compatibility reasons,
 * these 2 offsets are set in stone. */
  extern char gni_cq_assert1[1 -
			     2 *
			     !(__builtin_offsetof
			       (struct gni_cq, kern_cq_descr) == 120)];
  extern char gni_cq_assert2[1 -
			     2 *
			     !(__builtin_offsetof
			       (struct gni_cq, cache_revisit_shift) == 160)];

/* VCE channel descriptor */
  typedef struct gni_ce
  {
    gni_type_id_t type_id;
    gni_nic_handle_t nic_hndl;
    uint32_t vce_id;
  } gni_ce_t;

  typedef struct gni_mem_hndl_v1
  {
    struct
    {
      uint64_t va:52;
      uint64_t mdh:12;
    };
    struct
    {
      uint64_t npages:28;
      uint64_t pgsize:6;
      uint64_t flags:8;
      uint64_t unused:14;
      uint64_t crc:8;
    };
  } gni_mem_hndl_v1_t;
  typedef struct gni_mem_hndl_v2
  {
    union
    {
      struct
      {
	uint64_t va:52;
	uint64_t entropy:12;
      };
      uint64_t id;
    };
    struct
    {
      uint64_t npages:28;
      uint64_t pgsize:6;
      uint64_t flags:8;
      uint64_t mdh:12;
      uint64_t unused:2;
      uint64_t crc:8;
    };
  } gni_mem_hndl_v2_t;

/*************** Memory Handle ****************/
/* Flags (8 bits)*/
#define GNI_MEMHNDL_FLAG_READONLY       0x01UL	/* Memory is not writable */
#define GNI_MEMHNDL_FLAG_VMDH           0x02UL	/* Mapped via virtual MDH table */
#define GNI_MEMHNDL_FLAG_MRT            0x04UL	/* MRT was used for mapping */
#define GNI_MEMHNDL_FLAG_GART           0x08UL	/* GART was used for mapping */
#define GNI_MEMHNDL_FLAG_IOMMU          0x10UL	/* IOMMU was used for mapping */
#define GNI_MEMHNDL_FLAG_PCI_IOMMU      0x20UL	/* PCI IOMMU was used for mapping */
#define GNI_MEMHNDL_FLAG_CLONE          0x40UL	/* Registration cloned from a master MDD */
#define GNI_MEMHNDL_FLAG_NEW_FRMT       0x80UL	/* Used to support MDD sharing */
/* Memory Handle manipulations  */
#define GNI_MEMHNDL_INIT(memhndl) do {memhndl.qword1 = 0; memhndl.qword2 = 0;} while(0)
/* Support macros, 34 is the offset of the flags value */
#define GNI_MEMHNDL_NEW_FRMT(memhndl) ((memhndl.qword2 >> 34) & GNI_MEMHNDL_FLAG_NEW_FRMT)
#define GNI_MEMHNDL_FRMT_SET(memhndl, val, value)           \
        if (GNI_MEMHNDL_NEW_FRMT(memhndl)) {                \
                uint64_t tmp = value;                       \
                ((gni_mem_hndl_v2_t *)&memhndl)->val = tmp; \
        } else {                                            \
                uint64_t tmp = value;                       \
                ((gni_mem_hndl_v1_t *)&memhndl)->val = tmp; \
        }
#define GNI_MEMHNDL_FRMT_GET(memhndl, val) \
        ((uint64_t)(GNI_MEMHNDL_NEW_FRMT(memhndl) ? ((gni_mem_hndl_v2_t *)&memhndl)->val : ((gni_mem_hndl_v1_t *)&memhndl)->val))

/* Differing locations for V1 and V2 mem handles */
#define GNI_MEMHNDL_SET_VA(memhndl, value)  GNI_MEMHNDL_FRMT_SET(memhndl, va, (value) >> 12)
#define GNI_MEMHNDL_GET_VA(memhndl)         (GNI_MEMHNDL_FRMT_GET(memhndl, va) << 12)
#define GNI_MEMHNDL_CLEAR_VA(memhndl)       GNI_MEMHNDL_FRMT_SET(memhndl, va, 0)
#define GNI_MEMHNDL_SET_MDH(memhndl, value) GNI_MEMHNDL_FRMT_SET(memhndl, mdh, value)
#define GNI_MEMHNDL_GET_MDH(memhndl)        GNI_MEMHNDL_FRMT_GET(memhndl, mdh)
#define GNI_MEMHNDL_CLEAR_MDH(memhndl)      GNI_MEMHNDL_FRMT_SET(memhndl, mdh, 0)

/* The MDH field size is the same, and there is no other define to
 * limit max MDHs in uGNI. */

#define GNI_MEMHNDL_MDH_MASK    0xFFFUL

/* From this point forward, there is no difference. We don't need the
 * inlined conditionals */

/* Number of Registered pages (1TB for 4kB pages): QWORD2[27:0] */
#define GNI_MEMHNDL_NPGS_MASK   0xFFFFFFFUL
#define GNI_MEMHNDL_SET_NPAGES(memhndl, value) memhndl.qword2 |= (value & GNI_MEMHNDL_NPGS_MASK)
#define GNI_MEMHNDL_GET_NPAGES(memhndl) (memhndl.qword2 & GNI_MEMHNDL_NPGS_MASK)
#define GNI_MEMHNDL_CLEAR_NPAGES(memhndl) (memhndl.qword2 &= ~(GNI_MEMHNDL_NPGS_MASK))
/* Page size that was used to calculate the total number of pages : QWORD2[33:28] */
#define GNI_MEMHNDL_PSIZE_MASK  0x3FUL
#define GNI_MEMHNDL_SET_PAGESIZE(memhndl, value) memhndl.qword2 |= (((uint64_t)value & GNI_MEMHNDL_PSIZE_MASK) << 28)
#define GNI_MEMHNDL_GET_PAGESIZE(memhndl) (1UL << ((memhndl.qword2 >> 28) & GNI_MEMHNDL_PSIZE_MASK))
#define GNI_MEMHNDL_GET_PAGESHIFT(memhndl) ((memhndl.qword2 >> 28) & GNI_MEMHNDL_PSIZE_MASK)
#define GNI_MEMHNDL_GET_PAGEMASK(memhndl) (~(GNI_MEMHNDL_GET_PAGESIZE(memhndl) - 1))
#define GNI_MEMHNDL_CLEAR_PAGESIZE(memhndl) (memhndl.qword2 &= ~(GNI_MEMHNDL_PSIZE_MASK<<28))
/* Flags: QWORD2[41:34] */
#define GNI_MEMHNDL_FLAGS_MASK  0xFFUL
#define GNI_MEMHNDL_SET_FLAGS(memhndl, value) memhndl.qword2 |= ((value & GNI_MEMHNDL_FLAGS_MASK) << 34)
#define GNI_MEMHNDL_GET_FLAGS(memhndl) ((memhndl.qword2 >> 34) & GNI_MEMHNDL_FLAGS_MASK)
/* QWORD2[55:54] left blank */
/* CRC to verify integrity of the handle: QWORD2[63:56] ( Call this only after all other field are set!)*/
#define GNI_MEMHNDL_CRC_MASK 0xFFUL
#define GNI_MEMHNDL_SET_CRC(memhndl) (memhndl.qword2 |= ((uint64_t)gni_memhndl_calc_crc(&memhndl)<<56))
#define GNI_MEMHNDL_CLEAR_CRC(memhndl) (memhndl.qword2 &= ~(GNI_MEMHNDL_CRC_MASK<<56))
/* Verify memory handle. Returns logical one if correct. */
#define GNI_MEMHNDL_VERIFY(memhndl) \
	((memhndl.qword2 != 0) &&   \
	 (((memhndl.qword2 >> 56) & 0xFF) == gni_memhndl_calc_crc(&memhndl)))

/* Start and Length of registered region, no longer can use VA and NPAGES with MDD_Sharing */
#define GNI_MEMHNDL_GET_START(memhndl, addr) ((uint64_t)addr & GNI_MEMHNDL_GET_PAGEMASK(memhndl))
#define GNI_MEMHNDL_GET_LEN(memhndl, addr, len) \
	((((uint64_t)addr & ~GNI_MEMHNDL_GET_PAGEMASK(memhndl)) + (len) + \
	  GNI_MEMHNDL_GET_PAGESIZE(memhndl) - 1) & GNI_MEMHNDL_GET_PAGEMASK(memhndl))

#define GNI_FMA_GET             GHAL_FMA_GET
#define GNI_FMA_PUT             GHAL_FMA_PUT
#define GNI_FMA_PUT_MSG         GHAL_FMA_PUT_MSG
#define GNI_FMA_PUT_S           GHAL_FMA_PUT_S

#if defined(CRAY_CONFIG_GHAL_GEMINI)
#define GNII_FMA_CLEAR_LAST_USED(nic_hndl)
#else
#define GNII_FMA_CLEAR_LAST_USED(nic_hndl)	\
	do {					\
	nic_hndl->fma_desc_cmd = -1;		\
	nic_hndl->fma_desc_lmdh = 0;		\
	nic_hndl->fma_desc_rmdh = 0;		\
	nic_hndl->fma_desc_rc = 0;		\
	nic_hndl->fma_desc_wc = 0;		\
	} while(0)
#endif

/* Use this mask to identify Second generation commands */
#define GNI_FMA_ATOMIC2_GEN_MASK        0x40
/* Use this mask to clear _S flag in AMO command before using FMA_LAUNCH method */
#define GNI_FMA_ATOMIC2_S_MASK          0x01
/* Use to identify NON-FETCHING (PUT) semantics (used for both GEN1 and GEN2 opcodes) */
#define GNI_FMA_ATOMIC_NO_FETCH         0x100
/* Use to identify 32 bit AMO requests (GEN2 only) */
#define GNI_FMA_ATOMIC_32_MASK          0x200
/* First generation CACHED flag */
#define GNI_FMA_ATOMIC_CACHED           0x10
/* First generation Command code mask */
#define GNI_FMA_ATOMIC_CMD_MASK         0xF
/* Second generation CACHED flag */
#define GNI_FMA_ATOMIC2_CACHED          0x20

#define GNI_CHECK_FMA_ATOMIC_CACHED(amo_cmd) ((amo_cmd & GNI_FMA_ATOMIC2_GEN_MASK)?(amo_cmd & GNI_FMA_ATOMIC2_CACHED):(amo_cmd & GNI_FMA_ATOMIC_CACHED))

#if defined CRAY_CONFIG_GHAL_GEMINI
/* On Gemini the FMA LOCAL CQ entry macro is in charge of the contents
 * of the global event and the remote event */
#define GNI_FMA_LOCAL_CQ_ENTRY(func, entry, inst_id, req_id) entry = func(inst_id,req_id)
/* On Gemini the FMA REMOTE CQ entry macro is in charge of the
 * contents of the remote event */
#define GNI_FMA_REMOTE_CQ_ENTRY(func, entry, inst_id) entry = func(inst_id, 0)
#endif

#if defined CRAY_CONFIG_GHAL_ARIES
/* On Aries the FMA LOCAL CQ entry macro is in charge of the contents
 * of the global event */
#define GNI_FMA_LOCAL_CQ_ENTRY(func, entry, inst_id, req_id) entry = func(0,req_id)
/* On Aries the FMA REMOTE CQ entry macro is a NOP, as dest_cq_data is
 * used for remote events */
#define GNI_FMA_REMOTE_CQ_ENTRY(func, entry, inst_id) entry = 0ULL
#endif

#define GNI_COOKIE_PKEY(cookie) ((uint16_t)((cookie) >> 16))

#ifdef __KERNEL__

#define GASSERT(expression) { \
        if (unlikely(!(expression))) { \
                (void)printk(KERN_ERR \
                 "Assertion \"%s\" failed: file \"%s\", function \"%s\", line %d\n", \
                 #expression, __FILE__, __FUNCTION__, __LINE__); \
                BUG(); \
        } \
}

#else
#include <assert.h>

#ifndef likely
#define likely(x)       __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif

#define GASSERT(expression) {          \
	if (unlikely(!(expression))) { \
		assert(expression);    \
	}                              \
}
#endif /* __KERNEL__ */

#ifdef CRAY_CONFIG_GHAL_ARIES
#define ARIES_LIKELY(x) likely(x)
#else
#define ARIES_LIKELY(x) unlikely(x)
#endif

/* sysfs defs */

/* strings */
#define GNI_CLDEV_MDD_STR	GHAL_CLDEV_MDD_STR
#define GNI_CLDEV_MRT_STR	GHAL_CLDEV_MRT_STR
#define GNI_CLDEV_IOMMU_STR	GHAL_CLDEV_IOMMU_STR
#define GNI_CLDEV_GART_STR	GHAL_CLDEV_GART_STR
#define GNI_CLDEV_CQ_STR	GHAL_CLDEV_CQ_STR
#define GNI_CLDEV_FMA_STR	GHAL_CLDEV_FMA_STR
#define GNI_CLDEV_RDMA_STR	GHAL_CLDEV_RDMA_STR
#define GNI_CLDEV_CE_STR	GHAL_CLDEV_CE_STR
#define GNI_CLDEV_DLA_STR	GHAL_CLDEV_DLA_STR
#define GNI_CLDEV_SFMA_STR	GHAL_CLDEV_SFMA_STR
#define GNI_CLDEV_DIRECT_STR	GHAL_CLDEV_DIRECT_STR
#define GNI_CLDEV_PCI_IOMMU_STR	GHAL_CLDEV_PCI_IOMMU_STR
#define GNI_CLDEV_NONVMDH_STR	GHAL_CLDEV_NONVMDH_STR
#define GNI_CLDEV_TCR_STR	GHAL_CLDEV_TCR_STR
#define GNI_CLDEV_DVA_STR	GHAL_CLDEV_DVA_STR

/* indices in this array must match values of gni_dev_res_t in gni_pub.h */
#define GNII_DEV_RES_AVAILABLE { \
	0,				/* GNI_DEV_RES_FIRST = 0, */ \
	GHAL_CLDEV_MDD_AVAILABLE,	/* GNI_DEV_RES_MDD, */ \
	GHAL_CLDEV_MRT_AVAILABLE,	/* GNI_DEV_RES_MRT, */ \
	GHAL_CLDEV_CQ_AVAILABLE,	/* GNI_DEV_RES_CQ, */ \
	GHAL_CLDEV_FMA_AVAILABLE,	/* GNI_DEV_RES_FMA, */ \
	GHAL_CLDEV_CE_AVAILABLE,	/* GNI_DEV_RES_CE, */ \
	GHAL_CLDEV_DLA_AVAILABLE,	/* GNI_DEV_RES_DLA, */ \
	GHAL_CLDEV_TCR_AVAILABLE,	/* GNI_DEV_RES_TCR, */ \
	GHAL_CLDEV_DVA_AVAILABLE,	/* GNI_DEV_RES_DVA, */ \
	0				/* GNI_DEV_RES_LAST */ \
};

/* indices in this array must match values of gni_dev_res_t in gni_pub.h */
#define GNII_DEV_RES_STRS { \
	"",				/* GNI_DEV_RES_FIRST = 0, */ \
	GHAL_CLDEV_MDD_STR,		/* GNI_DEV_RES_MDD, */ \
	GHAL_CLDEV_MRT_STR,		/* GNI_DEV_RES_MRT, */ \
	GHAL_CLDEV_CQ_STR,		/* GNI_DEV_RES_CQ, */ \
	GHAL_CLDEV_FMA_STR,		/* GNI_DEV_RES_FMA, */ \
	GHAL_CLDEV_CE_STR,		/* GNI_DEV_RES_CE, */ \
	GHAL_CLDEV_DLA_STR,		/* GNI_DEV_RES_DLA, */ \
	GHAL_CLDEV_TCR_STR,		/* GNI_DEV_RES_TCR, */ \
	GHAL_CLDEV_DVA_STR,		/* GNI_DEV_RES_DVA, */ \
	""				/* GNI_DEV_RES_LAST */ \
};

/* indices in this array must match values of gni_job_res_t in gni_pub.h */
#define GNII_JOB_RES_AVAILABLE { \
	0,				/* GNI_JOB_RES_FIRST = 0, */ \
	GHAL_CLDEV_MDD_AVAILABLE,	/* GNI_JOB_RES_MDD, */ \
	GHAL_CLDEV_MRT_AVAILABLE,	/* GNI_JOB_RES_MRT, */ \
	GHAL_CLDEV_IOMMU_AVAILABLE,	/* GNI_JOB_RES_IOMMU, */ \
	GHAL_CLDEV_GART_AVAILABLE,	/* GNI_JOB_RES_GART, */ \
	GHAL_CLDEV_CQ_AVAILABLE,	/* GNI_JOB_RES_CQ, */ \
	GHAL_CLDEV_FMA_AVAILABLE,	/* GNI_JOB_RES_FMA, */ \
	GHAL_CLDEV_RDMA_AVAILABLE,	/* GNI_JOB_RES_RDMA, */ \
	GHAL_CLDEV_CE_AVAILABLE,	/* GNI_JOB_RES_CE, */ \
	GHAL_CLDEV_DLA_AVAILABLE,	/* GNI_JOB_RES_DLA, */ \
	GHAL_CLDEV_SFMA_AVAILABLE,	/* GNI_JOB_RES_SFMA, */ \
	GHAL_CLDEV_DIRECT_AVAILABLE,	/* GNI_JOB_RES_DIRECT, */ \
	GHAL_CLDEV_PCI_IOMMU_AVAILABLE,	/* GNI_JOB_RES_PCI_IOMMU, */ \
	GHAL_CLDEV_NONVMDH_AVAILABLE,	/* GNI_JOB_RES_NONVMDH, */ \
	0				/* GNI_JOB_RES_LAST */ \
};

/* indices in this array must match values of gni_job_res_t in gni_pub.h */
#define GNII_JOB_RES_STRS { \
	"",				/* GNI_JOB_RES_FIRST = 0, */ \
	GHAL_CLDEV_MDD_STR,		/* GNI_JOB_RES_MDD, */ \
	GHAL_CLDEV_MRT_STR,		/* GNI_JOB_RES_MRT, */ \
	GHAL_CLDEV_IOMMU_STR,		/* GNI_JOB_RES_IOMMU, */ \
	GHAL_CLDEV_GART_STR,		/* GNI_JOB_RES_GART, */ \
	GHAL_CLDEV_CQ_STR,		/* GNI_JOB_RES_CQ, */ \
	GHAL_CLDEV_FMA_STR,		/* GNI_JOB_RES_FMA, */ \
	GHAL_CLDEV_RDMA_STR,		/* GNI_JOB_RES_RDMA, */ \
	GHAL_CLDEV_CE_STR,		/* GNI_JOB_RES_CE, */ \
	GHAL_CLDEV_DLA_STR,		/* GNI_JOB_RES_DLA, */ \
	GHAL_CLDEV_SFMA_STR,		/* GNI_JOB_RES_SFMA, */ \
	GHAL_CLDEV_DIRECT_STR,		/* GNI_JOB_RES_DIRECT, */ \
	GHAL_CLDEV_PCI_IOMMU_STR,	/* GNI_JOB_RES_PCI_IOMMU, */ \
	GHAL_CLDEV_NONVMDH_STR,		/* GNI_JOB_RES_NONVMDH, */ \
	""				/* GNI_JOB_RES_LAST */ \
};

#define GNII_DEV_RES_VALID(res) \
{ \
	if ((res > GNI_DEV_RES_FIRST) && (res < GNI_DEV_RES_LAST)) { \
		return(gni_dev_res_available[res]); \
	} \
	return 0; \
}

#define GNII_DEV_RES_TO_STR(res) \
{ \
	if ((res > GNI_DEV_RES_FIRST) && (res < GNI_DEV_RES_LAST)) { \
		return(gni_dev_res_strs[res]); \
	} \
	return ""; \
}

#define GNII_JOB_RES_VALID(res) \
{ \
	if ((res > GNI_JOB_RES_FIRST) && (res < GNI_JOB_RES_LAST)) { \
		return(gni_job_res_available[res]); \
	} \
	return 0; \
}

#define GNII_JOB_RES_TO_STR(res) \
{ \
	if ((res > GNI_JOB_RES_FIRST) && (res < GNI_JOB_RES_LAST)) { \
		return gni_job_res_strs[res]; \
	} \
	return ""; \
}

#define GNII_FMA_BTE_AVAILABLE(nic_hndl, post_descr) \
	(((nic_hndl)->flbte_tx_desc_free != NULL) && ((nic_hndl)->flbte_max_len >= (post_descr)->length))

/* Support function to verify addr ranges in memory handles. We want
 * to check if the supplied addr/len wholly fits into the memory
 * handle. However, due to MDD Sharing, the memory handle may have a
 * negative value for the start address. So, we test if addr/len ever
 * crosses the complement of the memory handle. It seems the least
 * amount of conditionals. The only ambigious case is if the memory
 * handle starts and ends on the same address. This is impossible for
 * memory handles, as there are not enough bits to describe it. */
  static inline int gni_memhndl_addr_rng_chk (gni_mem_handle_t * mem_hndl,
					      uint64_t addr, uint64_t len)
  {
    uint64_t mem_va = GNI_MEMHNDL_GET_VA ((*mem_hndl));
    uint64_t mem_va_end =
      mem_va +
      GNI_MEMHNDL_GET_PAGESIZE ((*mem_hndl)) *
      GNI_MEMHNDL_GET_NPAGES ((*mem_hndl));
    uint64_t addr_end = addr + len;

    if (mem_va_end < mem_va)
      {
	/* Test if range ever touches memory handle's complement */
	if ((mem_va_end < addr_end) && (mem_va > addr))
	  {
	    return 0;
	  }
      }
    else if (mem_va > addr)
      {
	return 0;
      }
    else if (mem_va_end < addr_end)
      {
	return 0;
      }
    return 1;
  }

#ifdef __cplusplus
}				/* extern "C" */
#endif

#endif /*_GNI_PRIV_H_*/
