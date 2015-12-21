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
#include <linux/ioctl.h>
#ifdef CONFIG_MMU_NOTIFIER_FORK
#include <linux/mmu_notifier.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __KERNEL__
#include <sys/ioctl.h>
#else
#include <linux/ioctl.h>
#endif

/**
 * Atomic type.
 */
  typedef struct
  {
    volatile int counter;
  } atomic_t;

  typedef struct
  {
    volatile long counter;
  } atomic64_t;

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
#define GNI_DLA_MODE_PERSIST            1
#define GNI_DLA_MODE_DISCARD_USER       2
#define GNI_DLA_MODE_DISCARD_KERN       3
#define GNI_DLA_MODE_PERSIST_SHARED     4

/* set NIC's DLA attributes */
  typedef struct gni_dla_setattr_args
  {
    uint32_t mode;		/* IN specify DLA mode */
    uint64_t kern_cq_descr;	/* IN opaque specifier for DLA CQ */
    volatile void *fifo_fill_status;	/* OUT address mapped to FIFO fill status */
    uint32_t max_alloc_credits;	/* OUT maximum credits per DLA block */
    uint32_t max_mode_credits;	/* OUT maximum credits for DLA mode */
    uint32_t max_credits;	/* OUT maximum total credits */
    uint32_t reserved[5];
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

/* Collective engine child type */
  typedef enum
  {
    GNI_CE_CHILD_UNUSED,
    GNI_CE_CHILD_VCE,
    GNI_CE_CHILD_PE
  } gni_ce_child_t;

/* Collective engine modes */
#define GNI_CE_MODE_ROUND_UP            0x00000001	/* Rounding mode, specify 1 */
#define GNI_CE_MODE_ROUND_DOWN          0x00000002
#define GNI_CE_MODE_ROUND_NEAR          0x00000004
#define GNI_CE_MODE_ROUND_ZERO          0x00000008
#define GNI_CE_MODE_CQE_ONCOMP          0x00000010	/* CQE delivery mode, specify 1 */
#define GNI_CE_MODE_CQE_ONERR           0x00000040
#define GNI_CE_MODE_RC_NMIN_HASH        0x00000080	/* Routing mode, specify 1 */
#define GNI_CE_MODE_RC_MIN_HASH         0x00000100
#define GNI_CE_MODE_RC_MNON_HASH        0x00000200
#define GNI_CE_MODE_RC_ADAPT            0x00000400

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

  typedef struct gni_fma_win_addrs
  {
    uint64_t put;
    uint64_t put_nwc;
    uint64_t get;
    uint64_t ctrl;
  } gni_fma_win_addrs_t;

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
#define GNI_IOC_MEM_DEREGISTER          _IOWR(GNI_IOC_MAGIC, 10, gni_mem_deregister_args_t)

#define GNI_IOC_CQ_CREATE               _IOWR(GNI_IOC_MAGIC, 11, gni_cq_create_args_t)
#define GNI_IOC_CQ_DESTROY              _IOWR(GNI_IOC_MAGIC, 12, gni_cq_destroy_args_t)
#define GNI_IOC_CQ_WAIT_EVENT           _IOWR(GNI_IOC_MAGIC, 13, gni_cq_wait_event_args_t)

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
#define GNI_IOC_MEM_QUERY_HNDLS         _IOWR(GNI_IOC_MAGIC, 36, gni_mem_query_hndls_args_t)

/* These are just some cmd code for hobbes to send PMI calls */

#define GNI_IOC_PMI_ALLGATHER      51000
#define GNI_IOC_PMI_GETSIZE        51001
#define GNI_IOC_PMI_GETRANK        51002
#define GNI_IOC_PMI_FINALIZE        51003
#define GNI_IOC_PMI_BARRIER        51004


#define GNI_IOC_MAXNR  36

#ifdef __cplusplus
}				/* extern "C" */
#endif

#endif /*_GNI_PRIV_H_*/
