/* -*- c-basic-offset: 8; indent-tabs-mode: t -*- */
/**
 *	The Gemini Network Interface (GNI) driver.
 *
 *	Copyright (C) 2007,2008 Cray Inc. All Rights Reserved.
 *	Written by Igor Gorodetsky <igorodet@cray.com>
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation; either version 2 of the License,
 *	or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *	See the GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * 	TODO
 * 	1. Handle Loss of RX Descriptor, by re-setting RX ring (may involve termination of all active sessions):
 * 		If an uncorrectable parity errors is encountered while reading a
 * 		sequence table entry, that entry is evicted.  If this was a BTE_SEND
 *		sequence, then the RX Descriptor allocated for it will be lost.  No RX
 *		Descriptor write-back is performed since the information stored in the
 *		table is considered corrupt.
 *
 **/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/net.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/delay.h>
#include <linux/job_acct.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "ghal.h"
#include "ghal_pub.h"
#include "gni_priv.h"
#include "kgni.h"
#include "kgni_version.h"
#include "gni_stats.h"


MODULE_AUTHOR("Cray Inc.");
MODULE_DESCRIPTION("Cray(R) Gemini Network Interface");
MODULE_LICENSE("GPL");
char kgni_driver_string[] = "Cray(R) Gemini Network Interface";
char kgni_driver_copyright[] = "Copyright (C) 2007,2008 Cray Inc.";
char kgni_driver_name[] = "kgni";
uint32_t kgni_driver_version = GNI_DRV_VERSION;
uint32_t kgni_driver_buildrev = KGNI_DRV_BUILDREV;
char kgni_driver_buildstring[] = KGNI_DRV_BUILDSTRING;

int kgni_craytrace_idx;

extern int simnow_boot;
extern int softsdv_boot;
int kgni_unprotected_ops = 0;

struct kmem_cache *kgni_sm_mem_cache = NULL;
struct kmem_cache *kgni_mem_page_cache = NULL;
struct kmem_cache *kgni_rdma_qelm_cache = NULL;
struct kmem_cache *kgni_eeq_cache = NULL;
struct kmem_cache *kgni_umap_desc_cache = NULL;
struct workqueue_struct *kgni_sm_wq = NULL;

static int	kgni_major = 0;
module_param(kgni_major, int, S_IRUGO);
static int	bte_chnls = KGNI_BTE_DIRECT_CHNLS;
module_param(bte_chnls, int, S_IRUGO);
static int	qsce_delay = 1000;
module_param(qsce_delay, int, S_IRUGO);
static uint	kgni_bte_workq_freq_ms = KGNI_BTE_WORKQ_FREQ_MS_DEFAULT;
module_param(kgni_bte_workq_freq_ms, uint, S_IRUGO | S_IWUSR);

static int 	kgni_driver_numdev = GHAL_MAX_DEVICE_NUMBER;
static dev_t	kgni_dev_id;

/* subsystem debug levels */
static char *kgni_subsys_dbg_str = "enable";
int kgni_subsys_dbg = 0;
static char *kgni_dgram_dbg_str = "dgram";
int kgni_dgram_dbg_lvl = 0;
static char *kgni_smsg_dbg_str = "smsg";
int kgni_smsg_dbg_lvl = 0;

static void kgni_bte_chnl_release(kgni_bte_ctrl_t *bte_ctrl);
static void kgni_bte_chnl_reset(kgni_bte_ctrl_t *bte_ctrl, int mode);
static void kgni_bte_release(kgni_device_t *kgni_dev);

static inline atomic_t *kgni_bte_get_tx_counter_addr(kgni_bte_ctrl_t *bte_ctrl)
{
	kgni_device_t *kgni_dev = bte_ctrl->kgni_dev;
	atomic_t *tx_counters;

	tx_counters = (atomic_t *) &(kgni_dev->rw_page[KGNI_RW_PAGE_FLBTE]);
	return &(tx_counters[bte_ctrl->vcid]);
}

/*
 * For libugni backwards compatibility, we define these IOCTL cmds
 * using the old structure sizes here. They match the code in
 * gni_priv.h, but only change the sizing.
 */
#define GNI_IOC_CQ_OLD_CREATE        _IOWR(GNI_IOC_MAGIC, 11, gni_cq_create_no_interrupt_args_t)
#define GNI_IOC_CQ_OLD_WAIT_EVENT    _IOWR(GNI_IOC_MAGIC, 13, gni_cq_single_wait_event_args_t)
#define GNI_IOC_MEM_OLD_REGISTER     _IOWR(GNI_IOC_MAGIC,  8, gni_mem_register_noxargs_t)

#define   IOCTL_CMD_PALACIOS  -1000000000  /* For identifying ioctl from palacios */
#define   IOCTL_CMD_RANGE  -1200000000  /* For identifying ioctl from palacios */
/* !!! Let application to set the limit on MDD and MRT resources per ptag */

/**
 * kgni_ioctl - The ioctl() implementation
 **/
static long kgni_ioctl(struct file *filp,
		       unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	static int call_from_guest;
	gni_dla_setattr_args_t		dla_attr;
	gni_flbte_setattr_args_t	flbte_attr;
	gni_nic_setattr_args_t		nic_attr;
	gni_nic_vmdhconfig_args_t	nic_vmdh_attr;
	gni_nic_nttconfig_args_t	nic_ntt_attr;
	gni_nic_jobconfig_args_t	nic_jobcfg_attr;
	gni_nic_nttjob_config_args_t	nic_nttjob_attr;
	gni_nic_fmaconfig_args_t	*nic_fma_attr_ptr;
	gni_ep_postdata_args_t		ep_postdata_attr;
	gni_ep_postdata_test_args_t  	ep_posttest_attr;
	gni_ep_postdata_term_args_t	ep_postterm_attr;
	gni_cq_create_args_t		cq_create_attr;
	gni_cq_destroy_args_t		cq_destroy_attr;
	gni_cq_wait_event_args_t	cq_wait_attr;
	gni_cdm_barr_args_t		barr_args;
	gni_mem_register_args_t		*mem_reg_attr_ptr;
	gni_mem_deregister_args_t	*mem_dereg_attr_ptr;
	gni_mem_query_hndls_args_t	mem_query_hndls_attr;
	gni_post_rdma_args_t		*post_attr_ptr;
	gni_fma_assign_args_t		fma_assign_args;
	gni_fma_umap_args_t		fma_umap_args;
	gni_mem_handle_t  		mem_hndl;
	kgni_mem_reg_priv_t		mem_ops = {0};
	gni_mem_segment_t		segment;
	kgni_file_t			*file_inst = filp->private_data;
	gni_nic_handle_t		nic_handle = file_inst->nic_hndl;
	kgni_device_t			*kgni_dev;
	struct inode			*inode = filp->f_dentry->d_inode;
	int				vrt_pe;

	kgni_dev = container_of(inode->i_cdev, kgni_device_t, cdev);
	if (cmd < IOCTL_CMD_RANGE ){
		GPRINTK_RL(1, KERN_INFO, "(ioctl cmd before from palacios = %d)", cmd);
		cmd = cmd - IOCTL_CMD_PALACIOS;
		call_from_guest =1;
		GPRINTK_RL(1, KERN_INFO, "(ioctl cmd AFTER = %d), call_from_guest %d", cmd, call_from_guest);
	} 

	if (_IOC_TYPE(cmd) != GNI_IOC_MAGIC) return (-ENOTTY);

	if (nic_handle->ptag == 0) {
		if(!kgni_acceptable_cmd(cmd)) {
			GPRINTK_RL(1, KERN_INFO, "NIC handle was not properly configured. PTAG is still zero (ioctl cmd = %d)",
			           _IOC_NR(cmd));
			return (-EFAULT);
		}
	}

	switch (cmd) {
	case GNI_IOC_DLA_SETATTR:
		if (_IOC_SIZE(cmd) != sizeof(dla_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}

		if (copy_from_user(&dla_attr, (void *)arg, sizeof(dla_attr))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_dla_setup(nic_handle, &dla_attr);
		if (retval) {
			break;
		}
		if (copy_to_user((void *)arg, &dla_attr, sizeof(dla_attr))) {
			retval = -EFAULT;
		}
		break;
	case GNI_IOC_FLBTE_SETATTR:
		if (_IOC_SIZE(cmd) != sizeof(flbte_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}

		if (copy_from_user(&flbte_attr, (void *)arg, sizeof(flbte_attr))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_flbte_setup(nic_handle, &flbte_attr);
		if (retval) {
			break;
		}
		if (copy_to_user((void *)arg, &flbte_attr, sizeof(flbte_attr))) {
			retval = -EFAULT;
		}
		break;
	case GNI_IOC_NIC_SETATTR:
		if (_IOC_SIZE(cmd) != sizeof(nic_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
 		GPRINTK_RL(0, KERN_INFO, "Shyamali nic set ioctl command from palacios on host ");
		/* set PTag and the rest of parameters */
		if (call_from_guest){
			memcpy((void *)&nic_attr, (void *)arg, sizeof(nic_attr));
		} else {
			if (copy_from_user(&nic_attr, (void *)arg, sizeof(nic_attr))) {
				retval = -EFAULT;
				break;
			}
		}

		retval = kgni_nic_attach(nic_handle, &nic_attr);
		if (retval) {
			break;
		}

		if (GNI_NIC_FMA_DEDICATED(nic_handle)) {
			nic_attr.fma_window = (uint64_t)nic_handle->fma_window;
			nic_attr.fma_ctrl = (uint64_t)nic_handle->fma_ctrl;
			nic_attr.fma_window_nwc = (uint64_t)nic_handle->fma_window_nwc;
			nic_attr.fma_window_get = (uint64_t)nic_handle->fma_window_get;
		}

		/* if NTT is used, we want this to point to the virtual address,
		 * as the physical address is useless for communication */
		if (nic_handle->ntt_grp_size) {
			/* NTT is used, translate nic_handle->nic_pe into the virtual address*/
			vrt_pe = kgni_ntt_physical_to_logical(nic_handle, nic_handle->nic_pe);
			if(vrt_pe < 0) {
				GPRINTK_RL(1, KERN_INFO, "Failed lookup PE 0x%x vrt_pe=0x%x", nic_handle->nic_pe, vrt_pe);
				return -EFAULT;
			}
			nic_attr.nic_pe = vrt_pe;
		} else {
			/* NTT is disabled, use physical address */
			nic_attr.nic_pe = nic_handle->nic_pe;
		}

		nic_attr.cq_revisit_mode = nic_handle->cq_revisit_mode;
		GASSERT(((kgni_chnl_t *)nic_handle->sm_chnl_hndl)->datagram_cnt_desc != NULL);
		nic_attr.datagram_counter = (uint64_t)
				((kgni_chnl_t *)nic_handle->sm_chnl_hndl)->datagram_cnt_desc->address;
		if (call_from_guest) {
			memcpy((void *) arg, &nic_attr, sizeof(nic_attr));
		} else {
			if (copy_to_user((void *) arg, &nic_attr, sizeof(nic_attr))) {
				retval = -EFAULT;
			}
		}
		 call_from_guest = 0;
		break;
	case GNI_IOC_NIC_NTT_CONFIG:
		if (_IOC_SIZE(cmd) != sizeof(nic_ntt_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
#ifndef GNI_UNPROTECTED_OPS
		if (!kgni_unprotected_ops && !capable(CAP_SYS_ADMIN)) {
			retval = - EACCES;
			break;
		}
#endif
		if (copy_from_user(&nic_ntt_attr, (void *)arg, sizeof(gni_nic_nttconfig_args_t))) {
			retval = -EFAULT;
			break;
		}
		if (nic_ntt_attr.entries == NULL && !(nic_ntt_attr.flags & GNI_NTT_FLAG_CLEAR)) {
			retval = -EINVAL;
			break;
		}
		if (!(nic_ntt_attr.flags & GNI_NTT_FLAG_CLEAR) && !access_ok(VERIFY_READ, nic_ntt_attr.entries, nic_ntt_attr.num_entries*kgni_ntt_entries_size())) {
			retval = -EFAULT;
			break;
		}

		retval = kgni_ntt_config(nic_handle, &nic_ntt_attr);
		if (copy_to_user((void *) arg, &nic_ntt_attr, sizeof(gni_nic_nttconfig_args_t))) {
			retval = -EINVAL;
		}
		break;
	case GNI_IOC_NIC_JOBCONFIG:
		if (_IOC_SIZE(cmd) != sizeof(nic_jobcfg_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
#ifndef GNI_UNPROTECTED_OPS
		if (!kgni_unprotected_ops && !capable(CAP_SYS_ADMIN)) {
			retval = -EACCES;
			break;
		}
#endif
		if (copy_from_user(&nic_jobcfg_attr, (void *)arg, sizeof(gni_nic_jobconfig_args_t))) {
			retval = -EFAULT;
			break;
		}
		if (nic_jobcfg_attr.limits != NULL &&
		    !access_ok(VERIFY_READ, nic_jobcfg_attr.limits, sizeof(gni_job_limits_t))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_job_config(nic_handle, &nic_jobcfg_attr);
		break;
	case GNI_IOC_NIC_NTTJOB_CONFIG:
		if (_IOC_SIZE(cmd) != sizeof(nic_nttjob_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
#ifndef GNI_UNPROTECTED_OPS
		if (!capable(CAP_SYS_ADMIN)) {
			retval = -EACCES;
			break;
		}
#endif
		if (copy_from_user(&nic_nttjob_attr, (void *)arg, sizeof(gni_nic_nttjob_config_args_t))) {
			retval = -EFAULT;
			break;
		}
		/* this command can't be used to clear NTT region */
		if (nic_nttjob_attr.ntt_config.flags & GNI_NTT_FLAG_CLEAR) {
			retval = -EFAULT;
			break;
		}

		if (nic_nttjob_attr.ntt_config.entries != NULL) {
			retval = kgni_ntt_config(nic_handle, &nic_nttjob_attr.ntt_config);
			if (retval) break;
			/* prepare ntt parameters for the job configuration;
			 * user may not want to configure NTT, in such case he needs to provide
			 * ntt_base and ntt_size in limits descriptor */
			nic_nttjob_attr.job_config.limits->ntt_base = nic_nttjob_attr.ntt_config.base;
			nic_nttjob_attr.job_config.limits->ntt_size = nic_nttjob_attr.ntt_config.num_entries;
		}

		if (nic_nttjob_attr.job_config.limits == NULL ||
		    !access_ok(VERIFY_WRITE, nic_nttjob_attr.job_config.limits, sizeof(gni_job_limits_t))) {
			retval = -EFAULT;
		}

		if (!retval) {
			retval = kgni_job_config(nic_handle, &nic_nttjob_attr.job_config);
		}

		if (!retval) {
			if (copy_to_user((void *) arg, &nic_nttjob_attr, sizeof(gni_nic_nttjob_config_args_t))) {
				retval = -EINVAL;
			}
		}

		if (retval && nic_nttjob_attr.ntt_config.entries != NULL) {
			/* cleanup NTT if job configuration has failed */
			nic_nttjob_attr.ntt_config.flags |= GNI_NTT_FLAG_CLEAR;
			if (kgni_ntt_config(nic_handle, &nic_nttjob_attr.ntt_config)) {
				/* we should never get here */
				GPRINTK_RL(1, KERN_WARNING, "Failed to clean NTT region in the error path of GNI_IOC_NIC_NTTJOB_CONFIG command");
			}
		}
		break;
	case GNI_IOC_NIC_VMDH_CONFIG:
		if (_IOC_SIZE(cmd) != sizeof(nic_vmdh_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		if (copy_from_user(&nic_vmdh_attr, (void *)arg, sizeof(gni_nic_vmdhconfig_args_t))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_vmdh_config(nic_handle, &nic_vmdh_attr);

		/* ugni expects we return nic_vmdh_attr->blk_base */
		if (copy_to_user((void *) arg, &nic_vmdh_attr, sizeof(gni_nic_vmdhconfig_args_t))) {
			retval = -EFAULT;
		}
		break;
	case GNI_IOC_NIC_FMA_CONFIG:
		if (_IOC_SIZE(cmd) != sizeof(gni_nic_fmaconfig_args_t)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		nic_fma_attr_ptr = (gni_nic_fmaconfig_args_t *)arg;
		if (!access_ok(VERIFY_READ, nic_fma_attr_ptr, sizeof(gni_nic_fmaconfig_args_t))) {
			retval = -EFAULT;
			break;
		}
		if (GNI_NIC_FMA_SHARED(nic_handle)) {
			/* If sharing, take fmad_id specified by user */
			nic_handle->fmad_id = nic_fma_attr_ptr->fmad_id;
		}
		retval = kgni_fma_validate_fmad(nic_handle, nic_handle->fmad_id);
		if (!retval) {
			retval = kgni_fma_config(KGNI_NIC_FMA_INFO(nic_handle), nic_handle,
						 nic_fma_attr_ptr->kern_cq_descr, 1);
		}
		break;
	case GNI_IOC_EP_POSTDATA:
		if (_IOC_SIZE(cmd) != sizeof(ep_postdata_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		if (copy_from_user(&ep_postdata_attr, (void *)arg, sizeof(ep_postdata_attr))) {
			retval = -EFAULT;
			break;
		}
		if (ep_postdata_attr.in_data != NULL) {
			if (!access_ok(VERIFY_READ, ep_postdata_attr.in_data, ep_postdata_attr.in_data_len)) {
				retval = -EFAULT;
				break;
			}
		}
		retval = kgni_sm_add_session(nic_handle, &ep_postdata_attr);
		break;
	case GNI_IOC_EP_POSTDATA_TEST:
	case GNI_IOC_EP_POSTDATA_WAIT:
		if (_IOC_SIZE(cmd) != sizeof(ep_posttest_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		if (copy_from_user(&ep_posttest_attr, (void *)arg, sizeof(ep_posttest_attr))) {
			retval = -EFAULT;
			break;
		}
		if (ep_posttest_attr.out_buff != NULL) {
			if (!access_ok(VERIFY_WRITE, ep_posttest_attr.out_buff, ep_posttest_attr.buff_size)) {
				retval = -EFAULT;
				break;
			}
		}
		if (cmd == GNI_IOC_EP_POSTDATA_TEST) {
			retval = kgni_sm_get_state(nic_handle, &ep_posttest_attr);
		} else {
			retval = kgni_sm_wait_state(nic_handle, &ep_posttest_attr);
		}

		if (copy_to_user((void *) arg, &ep_posttest_attr, sizeof(ep_posttest_attr))) {
			retval = -EFAULT;
		}
		break;
	case GNI_IOC_EP_POSTDATA_TERMINATE:
		if (_IOC_SIZE(cmd) != sizeof(ep_postterm_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		if (copy_from_user(&ep_postterm_attr, (void *)arg, sizeof(ep_postterm_attr))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_sm_terminate(nic_handle, &ep_postterm_attr);
		break;
	case GNI_IOC_MEM_OLD_REGISTER:
	case GNI_IOC_MEM_REGISTER:
		mem_reg_attr_ptr = (gni_mem_register_args_t *)arg;
		if (!access_ok(VERIFY_WRITE, mem_reg_attr_ptr, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			break;
		}

		if (mem_reg_attr_ptr->mem_segments == NULL) {
			segment.address    = mem_reg_attr_ptr->address;
			segment.length     = mem_reg_attr_ptr->length;
			mem_ops.segments   = &segment;
			mem_ops.sgmnt_cnt  = 1;
		} else {
			if (!access_ok(VERIFY_READ, mem_reg_attr_ptr->mem_segments,
				       mem_reg_attr_ptr->segments_cnt*sizeof(gni_mem_segment_t))) {
				retval = -EFAULT;
				break;
			}
			if (mem_reg_attr_ptr->length != 0) {
				GPRINTK_RL(1, KERN_INFO, "gni_mem_register_args_t:length must"
				           " be zero for the segmented memory registration");
				retval = -EFAULT;
				break;
			}
			mem_ops.segments  = mem_reg_attr_ptr->mem_segments;
			mem_ops.sgmnt_cnt = mem_reg_attr_ptr->segments_cnt;
			mem_ops.mem_flags = KGNI_MEM_REG_SEGMENTED;
		}
		mem_ops.hw_cq_hndl = mem_reg_attr_ptr->kern_cq_descr;
		mem_ops.flags      = mem_reg_attr_ptr->flags;
		mem_ops.vmdh_index = mem_reg_attr_ptr->vmdh_index;
		if (mem_ops.flags & GNI_MEM_MDD_CLONE) {
			mem_hndl.qword1 = mem_reg_attr_ptr->mem_hndl.qword1;
			mem_hndl.qword2 = mem_reg_attr_ptr->mem_hndl.qword2;
		}
		if (GNI_MEM_IS_EXTERNAL(mem_ops.flags)) {
			if (cmd == GNI_IOC_MEM_OLD_REGISTER) {
				GPRINTK_RL(1, KERN_INFO, "gni_mem_register_args_t: External"
				           " memory flags set, but uGNI library is out of date");
				retval = -EFAULT;
				break;
			}
			mem_ops.xmem_args = mem_reg_attr_ptr->xargs;
		}

		retval = kgni_mem_register(nic_handle, &mem_ops, &mem_hndl);

		if (copy_to_user(&mem_reg_attr_ptr->mem_hndl, &mem_hndl, sizeof(gni_mem_handle_t))) {
			retval = -EFAULT;
		}
		break;
	case GNI_IOC_MEM_DEREGISTER:
		if (_IOC_SIZE(cmd) != sizeof(gni_mem_deregister_args_t)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		mem_dereg_attr_ptr = (gni_mem_deregister_args_t *)arg;
		if (!access_ok(VERIFY_READ, mem_dereg_attr_ptr, sizeof(gni_mem_deregister_args_t))) {
			retval = -EFAULT;
			break;
		}

		retval = kgni_mem_deregister(nic_handle, &mem_dereg_attr_ptr->mem_hndl, 0);
		break;
	case GNI_IOC_CQ_OLD_CREATE:
	case GNI_IOC_CQ_CREATE:
		if (copy_from_user(&cq_create_attr, (void *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			break;
		}

		if (_IOC_SIZE(cmd) == sizeof(gni_cq_create_no_interrupt_args_t)) {
			/* 
			 * For backwards compatibility with older libugni,
			 * we need to initialize the added fields.
			 */
			cq_create_attr.allow_user_interrupt_mask = 0;
			cq_create_attr.interrupt_mask = NULL;
		}

		retval = kgni_cq_create(nic_handle, &cq_create_attr, 1, NULL, 0, NULL);
		if (copy_to_user((void *) arg, &cq_create_attr, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
		}
		break;
	case GNI_IOC_CQ_DESTROY:
		if (_IOC_SIZE(cmd) != sizeof(cq_destroy_attr)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		if (copy_from_user(&cq_destroy_attr, (void *)arg, sizeof(cq_destroy_attr))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_cq_destroy(nic_handle, cq_destroy_attr.kern_cq_descr);
		break;
	case GNI_IOC_CQ_OLD_WAIT_EVENT:
	case GNI_IOC_CQ_WAIT_EVENT:
		if (_IOC_SIZE(cmd) != sizeof(cq_wait_attr) &&
		    _IOC_SIZE(cmd) != sizeof(gni_cq_single_wait_event_args_t)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		} else if (_IOC_SIZE(cmd) == sizeof(gni_cq_single_wait_event_args_t)) {
			/* 
			 * We allow the old size for backwards compatibility
			 * with older libugni.
			 */
			cq_wait_attr.num_cqs = 0;
		}

		if (copy_from_user(&cq_wait_attr, (void *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_cq_wait(nic_handle, &cq_wait_attr);
		if (copy_to_user((void *) arg, &cq_wait_attr, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
		}
		break;
	case GNI_IOC_POST_RDMA:
		if (_IOC_SIZE(cmd) != sizeof(gni_post_rdma_args_t)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		post_attr_ptr = (gni_post_rdma_args_t *) arg;
		if (!access_ok(VERIFY_READ, post_attr_ptr, sizeof(gni_post_rdma_args_t)) ||
		    !access_ok(VERIFY_READ, post_attr_ptr->post_desc, sizeof(gni_post_descriptor_t))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_rdma_post(nic_handle, post_attr_ptr->remote_pe, post_attr_ptr->post_desc,
					post_attr_ptr->kern_cq_descr, post_attr_ptr->src_cq_data,
					post_attr_ptr->usr_event_data);
		break;
	case GNI_IOC_CDM_BARR:
		if (_IOC_SIZE(cmd) != sizeof(gni_cdm_barr_args_t)) {
			GPRINTK_RL(1, KERN_INFO, "Bad ioctl argument size for cmd = %d", _IOC_NR(cmd));
			retval = -EFAULT;
			break;
		}
		if (copy_from_user(&barr_args, (void *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_cdm_barr(nic_handle, barr_args.count, barr_args.timeout);
		break;
	case GNI_IOC_FMA_ASSIGN:
		if (!GNI_NIC_FMA_SHARED(nic_handle)) {
			GPRINTK_RL(1, KERN_INFO, "Called FMA_ASSIGN ioctl for NIC with dedicated FMAD");
			retval = -EINVAL;
			break;
		}
		if (copy_from_user(&fma_assign_args, (void *)arg, sizeof(gni_fma_assign_args_t))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_fma_assign_new(nic_handle, fma_assign_args.kern_cq_desc,
					     nic_handle->pid, fma_assign_args.use_inc, 1,
					     nic_handle->cdm_type);
		if (!retval || retval == (-ENOSPC)) {
			/* return control fields */
			fma_assign_args.fma_pid = nic_handle->fma_pid;
			fma_assign_args.fma_res_id = nic_handle->fma_res_id;
			fma_assign_args.fma_cpu = nic_handle->fma_cpu;
			fma_assign_args.fma_tsc_khz = nic_handle->fma_tsc_khz;
			fma_assign_args.fma_state = (uint64_t)nic_handle->fma_state;
			fma_assign_args.fma_shared = (uint64_t)nic_handle->fma_shared;

			if (!retval) {
				/* If success, return info. about the assigned FMAD */
				fma_assign_args.fmad_id = nic_handle->fmad_id;
				fma_assign_args.addrs.put = (uint64_t)nic_handle->fma_window;
				fma_assign_args.addrs.put_nwc = (uint64_t)nic_handle->fma_window_nwc;
				fma_assign_args.addrs.get = (uint64_t)nic_handle->fma_window_get;
				fma_assign_args.addrs.ctrl = (uint64_t)nic_handle->fma_ctrl;
			}

			if (copy_to_user((void *) arg, &fma_assign_args, sizeof(fma_assign_args))) {
				retval = -EFAULT;
			}
		}
		break;
	case GNI_IOC_FMA_UMAP:
		if (!GNI_NIC_FMA_SHARED(nic_handle)) {
			GPRINTK_RL(1, KERN_INFO, "Called FMA_UMAP ioctl for NIC with dedicated FMAD");
			retval = -EINVAL;
			break;
		}
		if (copy_from_user(&fma_umap_args, (void *)arg, sizeof(gni_fma_umap_args_t))) {
			retval = -EFAULT;
			break;
		}
		retval = kgni_fma_validate_fmad(nic_handle, fma_umap_args.fmad_id);
		if (!retval) {
			retval = kgni_fma_umap_fmad(nic_handle, fma_umap_args.fmad_id);
			if (!retval) {
				fma_umap_args.addrs.put = (uint64_t)nic_handle->fma_window;
				fma_umap_args.addrs.put_nwc = (uint64_t)nic_handle->fma_window_nwc;
				fma_umap_args.addrs.get = (uint64_t)nic_handle->fma_window_get;
				fma_umap_args.addrs.ctrl = (uint64_t)nic_handle->fma_ctrl;
				if (copy_to_user((void *) arg, &fma_umap_args, sizeof(fma_umap_args))) {
					retval = -EFAULT;
				}
			}
		}
		break;
	case GNI_IOC_MEM_QUERY_HNDLS:
		if (copy_from_user(&mem_query_hndls_attr, (void *)arg, sizeof(gni_mem_query_hndls_args_t))) {
			retval = -EFAULT;
			break;
		}

		retval = kgni_mem_query_hndls(nic_handle, &mem_query_hndls_attr.mem_hndl,
					     &mem_query_hndls_attr.address,
					     &mem_query_hndls_attr.length);
		if (retval) {
			break;
		}

		if (copy_to_user((void *) arg, &mem_query_hndls_attr, sizeof(mem_query_hndls_attr))) {
			retval = -EFAULT;
		}
		break;
	default:
		retval = kgni_error_ioctl(kgni_dev,nic_handle,cmd,arg);
		if(retval == -ENOTTY) {
			retval = kgni_ce_ioctl(kgni_dev,nic_handle,cmd,arg);
		}
		break;
	}
	call_from_guest = 0;

	return retval;
}
EXPORT_SYMBOL(kgni_ioctl);
/**
 * kgni_open -  File open
 *
 * Notes:
 * - uGNI should be able to get proximity of NIC to the CPU core via sysfs (TBD)
 * - uGNI opens device. We create instance of gni_nic_t and store it in kgni_file_t;
 *   nic is created with cdm_type set to KERNEL and will get changed to USER
 *   only after uGNI successful completion of SET_NIC_ATTR command.
 *   No other ioctl command would work with the cdm_type set to KERNEL.
 * - when device gets closed, all resources associated with nic instance
 *   are released.
 * - kernel level application must call cdm_create() and then cdm_attach()
 * - kernel application must call cdm_destroy() to release resources.
 *
 **/
int kgni_open(struct inode *inode, struct file *filp)
{
	kgni_device_t	*kgni_dev;
	kgni_file_t	*file_inst;
	int 		err;

	kgni_dev = container_of(inode->i_cdev, kgni_device_t, cdev);

	file_inst = kzalloc(sizeof(kgni_file_t), GFP_KERNEL);
	if (file_inst == NULL) {
		return (-ENOMEM);
	}

	err = kgni_nic_create(kgni_dev, &file_inst->nic_hndl);
	if (err) {
		goto err_nic_create;
	}

	file_inst->nic_hndl->cdm_type = GNI_CDM_TYPE_USER;
	file_inst->nic_hndl->gart_domain_hndl = ghal_get_gart_user_domain(0);
	filp->private_data = file_inst;

	return 0;

	err_nic_create:
		kfree(file_inst);
	return err;
}

/**
 * kgni_close - File close
 **/
int kgni_close(struct inode *inode, struct file *filp)
{
	kgni_file_t	*file_inst;
	kgni_device_t	*kgni_dev;
	int		i;

	kgni_dev = container_of(inode->i_cdev, kgni_device_t, cdev);

	file_inst = filp->private_data;

	/* Detach NIC instance from CDM */
	kgni_nic_detach(file_inst->nic_hndl);

	GDEBUG(1,"NIC detached (from %d: mm %p)", current->pid, current->mm);

	/* clean associated resources */

	/* clean any left behind CQs */
	for (i = 0; i < GHAL_CQ_MAX_NUMBER; i++) {
		if (kgni_dev->cq_hdnls[i] != NULL &&
		    kgni_dev->cq_hdnls[i]->nic_hndl == file_inst->nic_hndl) {
			kgni_cq_destroy(file_inst->nic_hndl, kgni_dev->cq_hdnls[i]->hw_cq_hndl);
		}
	}

	/*
	 * The unmapping of the cq_interrupt_mask_desc must be done after
	 * the kgni_cq_destroy().  The kgni_cq_destroy() needs to clear out
	 * the cq interrupt mask reservation in the memory that is allocated
	 * on the kgni_device.
	 */
	if ((file_inst->nic_hndl->cdm_type != GNI_CDM_TYPE_KERNEL) &&
	    (file_inst->nic_hndl->mm != NULL) &&
	    (file_inst->nic_hndl->cq_interrupt_mask_desc != NULL)) {
		kgni_unmap_area(file_inst->nic_hndl->mm, file_inst->nic_hndl->cq_interrupt_mask_desc);
		file_inst->nic_hndl->cq_interrupt_mask_desc = NULL;
	}

	kgni_fma_put_all(file_inst->nic_hndl);

	kgni_mem_cleanup(file_inst->nic_hndl);

	/* vmdh_release must come after mem_cleanup */
	kgni_vmdh_release(file_inst->nic_hndl);

	kgni_nic_destroy(file_inst->nic_hndl);

	GDEBUG(1,"NIC destroyed");

	/* last thing we call app_cleanup.
	app should not get any async. events. */
	if (file_inst->app_cleanup) {
		file_inst->app_cleanup(file_inst->app_data);
	}

	filp->private_data = NULL;

	kfree(file_inst);

	return 0;
}

/**
 * kgni_nic_create - create and configure instance of the gni_nic
 **/
int kgni_nic_create(kgni_device_t	*kgni_dev,
		    gni_nic_handle_t 	*nic_hndl)
{
	gni_nic_handle_t nic_handle;
	int err;

	nic_handle = kzalloc(sizeof(gni_nic_t), GFP_KERNEL);
	if (nic_handle == NULL) {
		return (-ENOMEM);
	}

	nic_handle->flsh_page = (uint64_t *) __get_free_page(GFP_KERNEL);
	if (nic_handle->flsh_page == NULL) {
		kfree(nic_handle);
		return (-ENOMEM);
	}

	nic_handle->type_id = GNI_TYPE_NIC_HANDLE;
	nic_handle->kdev_hndl = kgni_dev;
	nic_handle->nic_pe = kgni_dev->nic_pe;
	nic_handle->fma_window_size = GHAL_FMA_WINDOW_SIZE;
	nic_handle->cdm_type = GNI_CDM_TYPE_KERNEL;
	nic_handle->cdm_id = GNI_CDM_INVALID_ID;
	nic_handle->my_id = GNI_NIC_INVALID_ID;
	nic_handle->vmdh_entry_id = GNI_VMDH_INVALID_ID;
	nic_handle->smsg_max_retrans = FMA_SMSG_MAX_RETRANS_DEFAULT;
	nic_handle->flbte_chan = GNI_FLBTE_INVALID_CHAN;
	/* it will get mode from user later, we need to make sure ALPS
	does not get killed, as it opens NIC to configure job, but never calls attach */
	nic_handle->modes = GNI_CDM_MODE_ERR_NO_KILL;
	nic_handle->pid = task_pid_vnr(current);
	init_waitqueue_head(&nic_handle->sm_wait_queue);
	spin_lock_init(&nic_handle->eeqs_lock);
	INIT_LIST_HEAD(&nic_handle->eeqs);
	INIT_GNI_LIST(&nic_handle->cq_list);
	INIT_GNI_LIST(&nic_handle->child_list);
	INIT_GNI_LIST(&nic_handle->umap_list);
	ghal_cq_get_revisit_mode(&nic_handle->cq_revisit_mode);
	GNI_NIC_FMA_SET_INVAL(nic_handle);

	err = kgni_subscribe_errors(nic_handle);
	if (err) {
		goto subscribe_error;
	}

	*nic_hndl = nic_handle;

	GDEBUG(1,"NIC is created[pe=0x%x]", nic_handle->nic_pe);

	return 0;

	subscribe_error:
		free_page((unsigned long) nic_handle->flsh_page);
		kfree(nic_handle);

	return err;
}

/**
 * kgni_nic_destroy - free instance of the gni_nic and associated resources
 **/
int kgni_nic_destroy(gni_nic_handle_t nic_hndl)
{
	if (nic_hndl->cdm_id != GNI_CDM_INVALID_ID) {
		GPRINTK_RL(0, KERN_ERR, "kgni_nic_destroy: NIC is still attached to CDM. Can NOT be destroyed");
		return (-EBUSY);
	}

	kgni_error_cleanup(nic_hndl);
	kgni_ce_cleanup(nic_hndl);
	kgni_dla_cleanup(nic_hndl);

	/* fma_fini would be more appropriate in nic_detach (fma_init is in
	 * nic_attach) but mem_cleanup requires use of an FMAD (and occurs
	 * after nic_detach). Note, FMA window mmaps are not unmapped here,
	 * but done below.
	 */
	kgni_fma_fini(nic_hndl);

	/* drop references on mm structs, cleanup child copied mmaps.
	 * This also removes all remaining mmaps, including FMA window
	 * mmaps.
	 */
	kgni_mm_fini(nic_hndl);

	kgni_resource_decref(nic_hndl);

	free_page((unsigned long) nic_hndl->flsh_page);
	kfree(nic_hndl);

	return 0;
}

/**
 * kgni_get_device_by_num - finds device by number
 **/
kgni_device_t *kgni_get_device_by_num(uint32_t devnum)
{
	int i;
	kgni_device_t *kgni_dev;

	for (i = 0; i < GHAL_MAX_DEVICE_NUMBER; i++) {
		kgni_dev = ghal_get_subsys_by_index(i, GHAL_SUBSYS_GNI);
		if (kgni_dev != NULL && kgni_dev->devnum == devnum) {
			return kgni_dev;
		}
	}
	return NULL;
}

struct file_operations kgni_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = kgni_ioctl,
	.open           = kgni_open,
	.release        = kgni_close,
};

/**
 * kgni_show_version - output version of the GNI driver
 **/
static ssize_t kgni_show_version(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	int total = 0;
	total += scnprintf(buf + total, PAGE_SIZE - total,
		       	   "0x%08x\n", kgni_driver_version);
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "%s\n", kgni_driver_buildstring);
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "%d\n", kgni_driver_buildrev);

	return total;
}

/**
 * kgni_show_resources - output kgni device resources per ptag
 *
 **/
static ssize_t kgni_show_resources(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	kgni_device_t *kgni_dev = dev_get_drvdata(class_dev);
	int total = 0;
	uint32_t ptag = kgni_dev->last_shown_ptag;
	unsigned long flags;

	do {
		if(++ptag >= GHAL_PTAG_MAX_NUMBER ) {
			ptag = 0;
		}
		read_lock_irqsave(&kgni_dev->rsrc_lock, flags);
		if (kgni_dev->resources[ptag]) {
			total += kgni_print_arch_resources_header(kgni_dev, buf + total, PAGE_SIZE - total, ptag);

			if (kgni_dev->resources[ptag]->ntt_block) {
				total += scnprintf(buf + total, PAGE_SIZE - total, " NTT(base/size): %d/%d",
						   kgni_dev->resources[ptag]->ntt_block->base,
						   kgni_dev->resources[ptag]->ntt_block->size);
			}

			total += scnprintf(buf + total, PAGE_SIZE - total, " ---\n");
			total += scnprintf(buf + total, PAGE_SIZE - total, "%-10s %-15s %-15s\n", "Name", "Used", "Limit");

			total += scnprintf(buf + total, PAGE_SIZE - total, "%-10s %-15d %-15d\n",
					   GNI_CLDEV_MDD_STR, atomic_read(&kgni_dev->resources[ptag]->mdd.count), kgni_dev->resources[ptag]->mdd.limit);
			total += scnprintf(buf + total, PAGE_SIZE - total, "%-10s %-15d %-15d\n",
					   GNI_CLDEV_CQ_STR, atomic_read(&kgni_dev->resources[ptag]->cq.count), kgni_dev->resources[ptag]->cq.limit);
			total += scnprintf(buf + total, PAGE_SIZE - total, "%-10s %-15d %-15d\n",
					   GNI_CLDEV_FMA_STR, atomic_read(&kgni_dev->resources[ptag]->fma.count), kgni_dev->resources[ptag]->fma.limit);
			total += scnprintf(buf + total, PAGE_SIZE - total, "%-10s %-15d %-15d\n",
					   GNI_CLDEV_RDMA_STR, atomic_read(&kgni_dev->resources[ptag]->rdma.count), kgni_dev->resources[ptag]->rdma.limit);
			total += scnprintf(buf + total, PAGE_SIZE - total, "%-10s %-15d %-15d\n",
					   GNI_CLDEV_DIRECT_STR, atomic_read(&kgni_dev->resources[ptag]->direct_rdma.count), GHAL_RSRC_NO_LIMIT);

			total += kgni_print_arch_resources(kgni_dev->resources[ptag], buf + total, PAGE_SIZE - total);
		}
		read_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
	} while ((ptag != kgni_dev->last_shown_ptag) && (total < (PAGE_SIZE - 1)));

	if (total == (PAGE_SIZE-1)) {
		/* we run out of space, will show last ptag info again next time */
		kgni_dev->last_shown_ptag = (ptag == 0)? (GHAL_PTAG_MAX_NUMBER - 1):(ptag - 1);
	} else {
		kgni_dev->last_shown_ptag = ptag;
	}

	return total;
}

/**
 * kgni_show_stats - print statistics
 **/
static ssize_t kgni_show_stats(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	kgni_device_t	*kgni_dev = dev_get_drvdata(class_dev);
	int i = 0, total = 0;

	for (i = 0; i < GNI_NUM_STATS; i++) {
		total += scnprintf(buf + total, PAGE_SIZE - total,
			   "%26s = %u\n",
			   gni_statistic_str[i], kgni_dev->stats[i]);
	}

	return total;
}

/**
 * kgni_clear_stats - clear kGNI statistics
 **/
static ssize_t kgni_clear_stats(struct device *class_dev, struct device_attribute *attr, const char *buf, size_t len)
{
	kgni_device_t	*kgni_dev = dev_get_drvdata(class_dev);
	int i;
	int rc;
	int stat = GNI_NUM_STATS;

	rc = sscanf(buf, "%d", &stat);

	if (rc == 0) {
		stat = GNI_NUM_STATS;
	}

	if (stat == GNI_NUM_STATS) {
		/* Clear all kGNI statistics. */
		for (i = 0; i < GNI_NUM_STATS; i++) {
			kgni_dev->stats[i] = 0;
			GDEBUG(1,"Clearing KGNI statistic: %d", i);
		}
	} else if ((stat >= 0) && (stat < GNI_NUM_STATS)) {
		/* Clear only specified kGNI statistic. */
		kgni_dev->stats[stat] = 0;
		GDEBUG(1,"Clearing KGNI statistic: %d", stat);
	} else {
		GDEBUG(1,"Invalid KGNI statistic: %d", stat);
	}

	return len;
}

/**
 * kgni_show_ptagrange - output range of PTAGs available to user applications
 **/
static ssize_t kgni_show_ptagrange(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\t%d\n", GNI_PTAG_USER_START, GNI_PTAG_USER_END);
}

/**
 * kgni_show_ntt - output allocated NTT blocks
 **/
static ssize_t kgni_show_ntt(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	kgni_device_t *kgni_dev = dev_get_drvdata(class_dev);
	struct list_head	*ptr;
	kgni_ntt_block_t	*ntt_ptr;
	int total = 0;

	total += scnprintf(buf + total, PAGE_SIZE - total, "%-10s %-10s %-10s %-10s\n", "Base", "Size", "RefCount", "SD Flag");

	spin_lock(&kgni_dev->ntt_list_lock);
	list_for_each(ptr, &kgni_dev->active_ntts) {
		ntt_ptr = list_entry(ptr, kgni_ntt_block_t, dev_list);
		total += scnprintf(buf + total, PAGE_SIZE - total, "%-10d %-10d %-10d %-10d\n", ntt_ptr->base, ntt_ptr->size,
				   ntt_ptr->ref_cnt, ntt_ptr->selfdestruct);
		if (total >= PAGE_SIZE) {
			break;
		}
	}
	spin_unlock(&kgni_dev->ntt_list_lock);

	return total;
}

/**
 * kgni_show_subsys_debug - output kGNI subsystem debug levels
 **/
static ssize_t kgni_show_subsys_debug(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	int total = 0;
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "%s\t: %d\n", kgni_subsys_dbg_str, kgni_subsys_dbg);
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "%s\t: %d\n", kgni_dgram_dbg_str, kgni_dgram_dbg_lvl);
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "%s\t: %d\n", kgni_smsg_dbg_str, kgni_smsg_dbg_lvl);
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "\nTo edit, echo field name followed by the new field value into this file.\n"
			     "example:\n   echo enable 1 > subsys_debug\n");

	return total;
}

/**
 * kgni_set_subsys_debug - set kGNI subsystem debug levels
 **/
static ssize_t kgni_set_subsys_debug(struct device *class_dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	char subsys_str[10];
	int dbg_lvl;

	if (sscanf(buf, "%9s %d", subsys_str, &dbg_lvl) == 2) {
		if (!strcmp(subsys_str, kgni_subsys_dbg_str)) {
			kgni_subsys_dbg = dbg_lvl ? 1 : 0;
		} else if (!strcmp(subsys_str, kgni_dgram_dbg_str)) {
			kgni_dgram_dbg_lvl = dbg_lvl;
		} else if (!strcmp(subsys_str, kgni_smsg_dbg_str)) {
			kgni_smsg_dbg_lvl = dbg_lvl;
		}
	}

	return count;
}

/**
 * kgni_show_unprotected_ops - output current unprotected ops value
 **/
static ssize_t kgni_show_unprotected_ops(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	int total = 0;

	total += scnprintf(buf + total, PAGE_SIZE - total, "%d\n", kgni_unprotected_ops);

	return total;
}

/**
 * kgni_set_unprotected_ops - set unprotected ops value
 **/
static ssize_t kgni_set_unprotected_ops(struct device *class_dev, struct device_attribute *attr, const char *buf, size_t len)
{
	int new = 0;

	sscanf(buf, "%d", &new);

	if (new && !kgni_unprotected_ops) {
		GDEBUG(1,"Turning on KGNI unprotected ops");
		kgni_unprotected_ops = new;

	} else if (!new && kgni_unprotected_ops) {
		GDEBUG(1,"Turning off KGNI unprotected ops");
		kgni_unprotected_ops = new;
	}

	return len;
}

#define BTE_CHNL_PROC_HDR_FMT_STR	\
	"%-5s %-9s %-6s %-6s %-5s %-6s\n"
#define BTE_CHNL_PROC_FMT_STR		\
	"%-5u %-9s %-6u %-6u %-5u %-6u\n"
#define BTE_CHNL_PROC_TXIDV_STR		"TXIDV"
#define BTE_CHNL_PROC_RXIDV_STR		"RXIDV"
#define BTE_CHNL_PROC_MODE_STR		"MODE"
#define BTE_CHNL_PROC_HELP	"\n" \
	"To edit, echo <VCID> <COL_NAME> <NEW_VALUE> into file, example:\n" \
	"	echo 1 TXIDV 50 > bte_chnl_proc\n"

/* corresponds to ghal_vc_mode_t types */
const char *gni_vc_mode_strs[GHAL_VC_MODE_MAX] = {
	"0:NONE",
	"1:DIRECT",
	"2:FLBTE"
};

static unsigned kgni_bte_chnl_get_free(kgni_bte_ctrl_t *bte_ctrl)
{
	kgni_ring_desc_t  *tx_ring = &bte_ctrl->tx_ring;
	unsigned long     flags;
	unsigned          free;

	switch (bte_ctrl->mode) {
	case GHAL_VC_MODE_DIRECT_BTE:
		spin_lock_irqsave(&bte_ctrl->tx_lock, flags);
		free = (tx_ring->next_to_clean > tx_ring->next_to_use) ?
			tx_ring->next_to_clean - tx_ring->next_to_use :
			tx_ring->count - (tx_ring->next_to_use - tx_ring->next_to_clean);
		spin_unlock_irqrestore(&bte_ctrl->tx_lock, flags);
		break;
	case GHAL_VC_MODE_FLBTE:
		free = (unsigned)atomic_read(tx_ring->desc_free);
		break;
	default:
		free = tx_ring->count;
		break;
	}

	return free;
}

/**
 * kgni_show_bte_chnl_proc - output configurable BTE control channel attributes
 **/
static ssize_t kgni_show_bte_chnl_proc(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	kgni_device_t     *kgni_dev = dev_get_drvdata(class_dev);
	kgni_bte_ctrl_t   *bte_ctrl;
	kgni_bte_queue_t  *txq;
	struct list_head  *ptr;
	kgni_pkt_queue_t  *queue_elm;
	unsigned long     flags;
	int               i, vcid, count, total = 0;

	total += scnprintf(buf + total, PAGE_SIZE - total, BTE_CHNL_PROC_HDR_FMT_STR,
			   "VCID", BTE_CHNL_PROC_MODE_STR,
			   BTE_CHNL_PROC_TXIDV_STR, BTE_CHNL_PROC_RXIDV_STR,
			   "FREE", "TOTAL");

	for (i = 0; i < GHAL_BTE_CHNL_MAX_NUMBER; i++) {
		if (kgni_dev->bte_ctrl[i] != NULL) {
			bte_ctrl = kgni_dev->bte_ctrl[i];
			total += scnprintf(buf + total, PAGE_SIZE - total, BTE_CHNL_PROC_FMT_STR,
					   bte_ctrl->vcid, gni_vc_mode_strs[bte_ctrl->mode],
					   bte_ctrl->tx_idv, bte_ctrl->rx_idv,
					   kgni_bte_chnl_get_free(bte_ctrl), bte_ctrl->tx_ring.count);
		}
	}

	for (i = 0; i < GNI_CDM_TYPE_MAX; i++) {
		count = 0;
		vcid = KGNI_BTE_VCID_ANY;
		txq = &kgni_dev->domain_bte_txq[i];
		spin_lock_irqsave(&txq->pkt_lock, flags);
		if (!list_empty(&txq->pkt_queue)) {
			queue_elm = list_first_entry(&txq->pkt_queue, kgni_pkt_queue_t, list);
			vcid = queue_elm->buf_info.vcid;
			list_for_each(ptr, &txq->pkt_queue) {
				count++;
			}
		}
		spin_unlock_irqrestore(&txq->pkt_lock, flags);
		total += scnprintf(buf + total, PAGE_SIZE - total, "%sBacklog (%s): has %d queued requests (VCID = %d).\n",
				   (i == 0) ? "\n" : "",
				   (i == GNI_CDM_TYPE_KERNEL) ? "KRN" : "USR", count, vcid);
	}

	total += scnprintf(buf + total, PAGE_SIZE - total, BTE_CHNL_PROC_HELP);

	return total;
}

/**
 * kgni_set_bte_chnl_proc - set configurable BTE control channel attributes
 **/
static ssize_t kgni_set_bte_chnl_proc(struct device *class_dev, struct device_attribute *attr, const char *buf, size_t len)
{
	kgni_device_t	*kgni_dev = dev_get_drvdata(class_dev);
	kgni_bte_ctrl_t	*bte_ctrl;
	uint32_t	vcid;
	char		field[20];
	int32_t		val;
	int		rc;

	if (sscanf(buf, "%d %19s %d", &vcid, field, &val) == 3 && (vcid < GHAL_BTE_CHNL_MAX_NUMBER)) {
		bte_ctrl = kgni_dev->bte_ctrl[vcid];
		if (bte_ctrl == NULL) {
			return -EINVAL;
		}

		if (!strncmp(field, BTE_CHNL_PROC_TXIDV_STR, 19)) {
			if (val < 0) {
				return -EINVAL;
			}

			if (!down_timeout(&bte_ctrl->desc_sem, HZ)) {
				rc = ghal_vc_set_idv(kgni_dev->pdev, bte_ctrl->vcid, val, -1);
				up(&bte_ctrl->desc_sem);

				if (rc) {
					return rc;
				} else {
					bte_ctrl->tx_idv = val;
				}
			} else {
				GPRINTK_RL(0, KERN_INFO, "Timed out waiting for BTETC desc. sem.");
				return -ETIME;
			}

		} else if (!strncmp(field, BTE_CHNL_PROC_RXIDV_STR, 19)) {

			if (val < 0) {
				return -EINVAL;
			}

			if (!down_timeout(&bte_ctrl->desc_sem, HZ)) {
				rc = ghal_vc_set_idv(kgni_dev->pdev, bte_ctrl->vcid, -1, val);
				up(&bte_ctrl->desc_sem);

				if (rc) {
					return rc;
				} else {
					bte_ctrl->rx_idv = val;
				}
			} else {
				GPRINTK_RL(0, KERN_INFO, "Timed out waiting for BTERC desc. sem.");
				return -ETIME;
			}
		}
		else if (!strncmp(field, BTE_CHNL_PROC_MODE_STR, 19)) {
			if (val > 0 && val < GHAL_VC_MODE_MAX &&
			    (bte_ctrl->mode != val)) {
				rc = kgni_bte_chnl_reset_prepare(bte_ctrl, val);
				if (rc) {
					return rc;
				} else {
					/* Reset channel, setting to new mode */
					kgni_bte_chnl_reset(bte_ctrl, val);
				}
			} else {
				return -EINVAL;
			}
		}
	} else {
		return -EINVAL;
	}

	return len;
}

/**
 * kgni_show_dla_config - print DLA config parameters
 **/
static ssize_t kgni_show_dla_config(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	int i = 0, total = 0;
	extern gni_dla_config_params_t GNII_dla_config;

	/* these need to be in same order as array definition in gni_priv.h */ 
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "MAX_FMA_SIZE", GNII_dla_config.max_fma_size);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "BYTES_PER_BLOCK", GNII_dla_config.bytes_per_block);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "EXTRA_CREDITS_PER_BLOCK", GNII_dla_config.extra_credits_per_block);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "MIN_TOTAL_BLOCKS", GNII_dla_config.min_total_blocks);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "STATUS_INTERVAL", GNII_dla_config.status_interval);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "PR_CREDITS_PERCENT", GNII_dla_config.pr_credits_percent);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "ALLOC_STATUS_RETRY", GNII_dla_config.alloc_status_retry);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "ALLOC_STATUS_MULTIPLE", GNII_dla_config.alloc_status_multiple);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "ALLOC_STATUS_THRESHOLD", GNII_dla_config.alloc_status_threshold);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "OVERFLOW_MAX_RETRIES", GNII_dla_config.overflow_max_retries);
	total += scnprintf(buf + total, PAGE_SIZE - total,
		   "[%2d]: %24s = %u\n", i++,
		   "OVERFLOW_CREDITS_PERCENT", GNII_dla_config.overflow_percent);

	return total;
}

/**
 * kgni_set_dla_config - change DLA config parameters
 **/
static ssize_t kgni_set_dla_config(struct device *class_dev, struct device_attribute *attr, const char *buf, size_t len)
{
	int      rc;
	uint32_t idx, new_value;
	extern gni_dla_config_params_t GNII_dla_config;

	rc = sscanf(buf, "%u %u", &idx, &new_value);
	if (rc == 2 && idx < GNI_DLA_NUM_CONFIG_PARAMS) {
		GNII_dla_config.params[idx] = new_value;
	}

	return len;
}

/**
 * kgni_show_numa_map - print kGNI NUMA map
 **/
static ssize_t kgni_show_numa_map(struct device *class_dev, struct device_attribute *attr, char *buf)
{
	kgni_device_t *kgni_dev = dev_get_drvdata(class_dev);
	kgni_numa_desc_t *nd = &kgni_dev->numa_desc;
	int total = 0;
	int i;

	total += scnprintf(buf + total, PAGE_SIZE - total, "%u\n%u\n",
			   nd->cpu_cnt, nd->node_cnt);

	for (i = 0; i < nd->cpu_cnt; i++) {
		total += scnprintf(buf + total, PAGE_SIZE - total, "%u %u\n",
				   nd->cpus[i].cpu, nd->cpus[i].node);
	}

	return total;
}

/**
 * kgni_set_numa_map - change kGNI NUMA map
 **/
static ssize_t kgni_set_numa_map(struct device *class_dev, struct device_attribute *attr, const char *buf, size_t len)
{
	GPRINTK_RL(0, KERN_ERR, "unimplemented");

	return len;
}

static DEVICE_ATTR(version, S_IRUGO, kgni_show_version, NULL);
static DEVICE_ATTR(resources, S_IRUGO, kgni_show_resources, NULL);
static DEVICE_ATTR(stats, S_IRUGO | S_IWUSR, kgni_show_stats, kgni_clear_stats);
static DEVICE_ATTR(ptag_range, S_IRUGO, kgni_show_ptagrange, NULL);
static DEVICE_ATTR(ntt, S_IRUGO, kgni_show_ntt, NULL);
static DEVICE_ATTR(subsys_debug, S_IRUGO | S_IWUSR, kgni_show_subsys_debug, kgni_set_subsys_debug);
static DEVICE_ATTR(unprotected_ops, S_IRUGO | S_IWUSR,
		   kgni_show_unprotected_ops, kgni_set_unprotected_ops);
static DEVICE_ATTR(bte_chnl_proc, S_IRUGO | S_IWUSR,
		   kgni_show_bte_chnl_proc, kgni_set_bte_chnl_proc);
static DEVICE_ATTR(dla_config, S_IRUGO | S_IWUSR, kgni_show_dla_config, kgni_set_dla_config);
static DEVICE_ATTR(numa_map, S_IRUGO | S_IWUSR, kgni_show_numa_map, kgni_set_numa_map);


/* Use of '/sys/class/gni/kgni<*>/version' is deprecated.  Use
 * '/sys/class/gni/version' instead. */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,33))
static ssize_t kgni_show_version_cl(struct class *class, char *buf)
#else
static ssize_t kgni_show_version_cl(struct class *class, struct class_attribute *attr, char *buf)
#endif
{
	int total = 0;
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "0x%08x\n", kgni_driver_version);
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "%s\n", kgni_driver_buildstring);
	total += scnprintf(buf + total, PAGE_SIZE - total,
			   "%d\n", kgni_driver_buildrev);

	return total;
}
static CLASS_ATTR(version, S_IRUGO, kgni_show_version_cl, NULL);

/**
 * kgni_create_cldev_files - create all class device files
 **/
static int kgni_create_cldev_files(kgni_device_t *kgni_dev)
{
	int err;

	err = device_create_file(kgni_dev->class_dev, &dev_attr_version);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create version attribute");
		goto err_attr_version;
	}
	err = device_create_file(kgni_dev->class_dev, &dev_attr_resources);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create resources attribute");
		goto err_attr_resources;
	}

	err = device_create_file(kgni_dev->class_dev, &dev_attr_stats);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create stats attribute");
		goto err_attr_stats;
	}

	err = device_create_file(kgni_dev->class_dev, &dev_attr_ptag_range);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create ptag_range attribute");
		goto err_attr_ptag;
	}

	err = device_create_file(kgni_dev->class_dev, &dev_attr_ntt);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create ntt attribute");
		goto err_attr_ntt;
	}

	err = device_create_file(kgni_dev->class_dev, &dev_attr_subsys_debug);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create subsys_debug attribute");
		goto err_attr_subsys_debug;
	}

	err = device_create_file(kgni_dev->class_dev,&dev_attr_unprotected_ops);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create unprotected_ops attribute");
		goto err_attr_unprotected_ops;
	}

	err = device_create_file(kgni_dev->class_dev,&dev_attr_bte_chnl_proc);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create bte_chnl_proc attribute");
		goto err_attr_bte_chnl_proc;
	}

	err = device_create_file(kgni_dev->class_dev,&dev_attr_dla_config);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create dla_config attribute");
		goto err_attr_dla_config;
	}

	err = device_create_file(kgni_dev->class_dev, &dev_attr_numa_map);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create numa_map attribute");
		goto err_attr_numa_map;
	}

	return 0;

	err_attr_numa_map:
		device_remove_file(kgni_dev->class_dev, &dev_attr_dla_config);
	err_attr_dla_config:
		device_remove_file(kgni_dev->class_dev, &dev_attr_bte_chnl_proc);
	err_attr_bte_chnl_proc:
		device_remove_file(kgni_dev->class_dev, &dev_attr_unprotected_ops);
	err_attr_unprotected_ops:
		device_remove_file(kgni_dev->class_dev, &dev_attr_subsys_debug);
	err_attr_subsys_debug:
		device_remove_file(kgni_dev->class_dev, &dev_attr_ntt);
	err_attr_ntt:
		device_remove_file(kgni_dev->class_dev, &dev_attr_ptag_range);
	err_attr_ptag:
		device_remove_file(kgni_dev->class_dev, &dev_attr_stats);
	err_attr_stats:
		device_remove_file(kgni_dev->class_dev, &dev_attr_resources);
	err_attr_resources:
		device_remove_file(kgni_dev->class_dev, &dev_attr_version);
	err_attr_version:
	return err;
}

/**
 * kgni_remove_cldev_files - remove all class device files
 **/
static void kgni_remove_cldev_files(kgni_device_t *kgni_dev)
{
	device_remove_file(kgni_dev->class_dev, &dev_attr_version);
	device_remove_file(kgni_dev->class_dev, &dev_attr_resources);
	device_remove_file(kgni_dev->class_dev, &dev_attr_stats);
	device_remove_file(kgni_dev->class_dev, &dev_attr_ptag_range);
	device_remove_file(kgni_dev->class_dev, &dev_attr_ntt);
	device_remove_file(kgni_dev->class_dev, &dev_attr_subsys_debug);
	device_remove_file(kgni_dev->class_dev, &dev_attr_unprotected_ops);
	device_remove_file(kgni_dev->class_dev, &dev_attr_bte_chnl_proc);
	device_remove_file(kgni_dev->class_dev, &dev_attr_dla_config);
	device_remove_file(kgni_dev->class_dev, &dev_attr_numa_map);
}

static void* kgni_copy_rx_buff(kgni_bte_ctrl_t *bte_ctrl, void *buf_ptr, int hdr_len, int data_len, void **data_ptr)
{
	uint32_t buff_size;
	void *new_buff;
	buff_size = hdr_len + data_len;

	new_buff = kmalloc(buff_size, GFP_ATOMIC);
	if (new_buff != NULL) {
		memcpy(new_buff, buf_ptr, buff_size);
		*data_ptr = new_buff + hdr_len;
	}
	memset(buf_ptr, 0xFF, buff_size);

	return new_buff;
}
/**
 * kgni_alloc_rx_buffs - Allocate RX buffers
 **/
static int kgni_alloc_rx_buffs(kgni_bte_ctrl_t *bte_ctrl)
{
	int i, check_num;
	kgni_ring_desc_t *rx_ring = &bte_ctrl->rx_ring;
	ghal_vc_desc_t *vc_desc = &bte_ctrl->bte_vc_desc;
	ghal_rx_desc_t	rx_desc = GHAL_RXD_INIT;
	ghal_rx_desc_t	cpy_rx_desc = GHAL_RXD_INIT;
	uint16_t last_entry = 0xFFFF;
	char *data_ptr;
	uint32_t buff_size;
	uint32_t count = rx_ring->count;
	uint8_t do_rx_readback = bte_ctrl->kgni_dev->do_rx_readback;
	gfp_t kmalloc_flags;

	kmalloc_flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;

	buff_size = bte_ctrl->rx_buff_size + sizeof(kgni_rx_pktdsc_t) + GHAL_BTE_RX_ALLIGMENT;
	i = rx_ring->next_to_use;

	while (!rx_ring->buffer_info[i].buff_ptr) {

		data_ptr = kmalloc(buff_size, kmalloc_flags);
		if (data_ptr == NULL) {
			break;
		}

		memset(data_ptr, 0xFF, buff_size);

		rx_ring->buffer_info[i].buff_ptr = data_ptr;
		data_ptr += sizeof(kgni_rx_pktdsc_t);
		rx_ring->buffer_info[i].data_ptr = (void *)ALIGN((uint64_t)data_ptr,
							 GHAL_BTE_RX_ALLIGMENT);
		GHAL_RXD_SET_PASSPW(rx_desc);
		GHAL_RXD_SET_COHERENT(rx_desc);
		GHAL_RXD_SET_IRQ(rx_desc);
		GHAL_RXD_SET_IRQ_DELAY(rx_desc);
		GHAL_RXD_SET_BUFF_ADDR(rx_desc, virt_to_phys(rx_ring->buffer_info[i].data_ptr));
		GHAL_VC_CLEAR_RXD_STATUS((*vc_desc), i);

		for (check_num = 0; check_num < 11; check_num++) {
			/* Write RX desc. to Gemini */
			GHAL_VC_WRITE_RXD((*vc_desc), i, rx_desc);
			if (do_rx_readback) {
				wmb();
				memcpy64_fromio(&cpy_rx_desc, GHAL_RX_DESC((*vc_desc), i), sizeof(cpy_rx_desc));
				if (rx_desc.base_addr != cpy_rx_desc.base_addr) {
					if (check_num == 10) {
						GPRINTK_RL(0, KERN_ERR, "Readback returned different value[%d]: 0x%llx 0x%llx ", 
						           check_num, (uint64_t) rx_desc.base_addr, (uint64_t) cpy_rx_desc.base_addr);
						GASSERT(0);
					}
				} else {
					break;
				}
			} else {
				break;
			}
		}

		last_entry = i;

		if (++i == count) i = 0;
	}
	/* Write WrIndex into RX VC channel*/
	if (last_entry != 0xFFFF) {
		GHAL_VC_WRITE_RX_WRINDEX((*vc_desc), last_entry);
	} else {
		return (-ENOMEM);
	}
	rx_ring->next_to_use = i;

	return 0;
}

/**
 * kgni_free_rx_buffs - Free RX buffers
 **/
static void kgni_free_rx_buffs(kgni_bte_ctrl_t *bte_ctrl)
{
	int i;
	kgni_ring_desc_t *rx_ring = &bte_ctrl->rx_ring;
	uint32_t count = rx_ring->count;

	for (i = 0; i < count; i++) {
		if (rx_ring->buffer_info[i].buff_ptr) {
			kfree(rx_ring->buffer_info[i].buff_ptr);
			rx_ring->buffer_info[i].buff_ptr = NULL;
		}
	}

	return;
}

/**
 * kgni_conf_backlog - Setup backlog packet queue
 **/
static void kgni_conf_backlog(kgni_bte_queue_t *backlog)
{
	INIT_LIST_HEAD(&backlog->pkt_queue);
	spin_lock_init(&backlog->pkt_lock);

	return;
}

/**
 * kgni_conf_rings - Configure RX/TX descriptor rings
 **/
static int kgni_conf_rings(kgni_bte_ctrl_t *bte_ctrl)
{
	int size;

	bte_ctrl->tx_ring.count = bte_ctrl->bte_vc_desc.tx_ring_size;
	size = bte_ctrl->tx_ring.count * sizeof(kgni_buffer_t);
	bte_ctrl->tx_ring.buffer_info = kzalloc(size, GFP_KERNEL);
	if (bte_ctrl->tx_ring.buffer_info == NULL) {
		return (-ENOMEM);
	}
	/* TX descriptor counter (FLBTE) */
	bte_ctrl->tx_ring.desc_free = kgni_bte_get_tx_counter_addr(bte_ctrl);
	atomic_set(bte_ctrl->tx_ring.desc_free, bte_ctrl->tx_ring.count);
	spin_lock_init(&bte_ctrl->tx_lock);

	bte_ctrl->rx_ring.count = bte_ctrl->bte_vc_desc.rx_ring_size;
	size = bte_ctrl->rx_ring.count * sizeof(kgni_buffer_t);
	bte_ctrl->rx_ring.buffer_info = kzalloc(size, GFP_KERNEL);
	if (bte_ctrl->rx_ring.buffer_info == NULL) {
		kfree(bte_ctrl->tx_ring.buffer_info);
		return (-ENOMEM);
	}
	bte_ctrl->rx_buff_size = KGNI_CHNL_BUFFER_SIZE;
	spin_lock_init(&bte_ctrl->rx_lock);

	return 0;
}

/**
 * kgni_release_rings - Free resources allocated for RX/TX rings
 **/
static void kgni_release_rings(kgni_bte_ctrl_t *bte_ctrl)
{
	kfree(bte_ctrl->tx_ring.buffer_info);
	kfree(bte_ctrl->rx_ring.buffer_info);

	return;
}

/**
 * kgni_push_tx_init -  initialize state machine for BTE TX tasklet
 **/
static void
kgni_push_tx_init(kgni_device_t *kgni_dev, kgni_bte_push_tx_t *push_tx)
{
	memset(push_tx, 0, sizeof(kgni_bte_push_tx_t));
	push_tx->kgni_dev = kgni_dev;
	/* always start with KERNEL domain backlog */
	push_tx->next_txq_id = GNI_CDM_TYPE_KERNEL;
	push_tx->blocked_txq_id = KGNI_INVALID_BTE_TXQ;
	/* give some randomness to the BTE channel we start with */
	push_tx->next_chnl_id = (unsigned)atomic_read(&kgni_dev->cur_bte_chnl) % kgni_dev->num_bte_chnls;
	return;
}

/**
 * kgni_push_tx_get_next_chnl - round robin to next non-FULL channel
 **/
static kgni_bte_ctrl_t *
kgni_push_tx_get_chnl(kgni_bte_push_tx_t *push_tx)
{
	kgni_device_t   *kgni_dev = push_tx->kgni_dev;
	kgni_bte_ctrl_t *bte_ctrl = NULL;
	int             num_chnls;

	/* we can't already have a locked BTE chnl set in state machine */
	GASSERT(push_tx->locked_bte_ctrl == NULL);

	/* get next channel that is both active and non-FULL */
	for (num_chnls = kgni_dev->num_bte_chnls; num_chnls > 0; num_chnls--) {
		bte_ctrl = kgni_bte_next_chnl(kgni_dev, GHAL_VC_MODE_DIRECT_BTE, push_tx->next_chnl_id);
		push_tx->next_chnl_id = (push_tx->next_chnl_id + 1) % kgni_dev->num_bte_chnls;

		spin_lock_irqsave(&bte_ctrl->tx_lock, push_tx->tx_flags);
		if (!(bte_ctrl->tx_ring.status & KGNI_RING_FULL)) {
			/* we found a non-FULL BTE channel to use;
			 * advance state machine to next chnl and return
			 */
			GASSERT(bte_ctrl->mode == GHAL_VC_MODE_DIRECT_BTE);
			push_tx->locked_bte_ctrl = bte_ctrl;
			break;
		}
		/* else chnl is FULL, try next channel */
		spin_unlock_irqrestore(&bte_ctrl->tx_lock, push_tx->tx_flags);
	}

	return push_tx->locked_bte_ctrl;
}

/**
 * kgni_push_tx_get_queue - round robin to next TX backlog queue
 **/
static kgni_bte_queue_t *
kgni_push_tx_get_queue(kgni_bte_push_tx_t *push_tx, kgni_bte_ctrl_t *bte_ctrl)
{
	kgni_device_t		*kgni_dev = push_tx->kgni_dev;
	kgni_bte_queue_t	*txq;
	kgni_pkt_queue_t	*queue_elm = NULL;
	int			num_txqs;
	int			curr_txq_id;

	GASSERT(push_tx->locked_txq == NULL);
	push_tx->blocked_txq_id = KGNI_INVALID_BTE_TXQ;

	for (num_txqs = GNI_CDM_TYPE_MAX; num_txqs > 0; num_txqs--) {
		curr_txq_id = push_tx->next_txq_id;
		txq = &kgni_dev->domain_bte_txq[curr_txq_id];
		push_tx->next_txq_id = (push_tx->next_txq_id + 1) % GNI_CDM_TYPE_MAX;

		spin_lock_irqsave(&txq->pkt_lock, push_tx->pkt_flags);
		if (!list_empty(&txq->pkt_queue)) {
			/* inspect element to see if can send on current channel */
			queue_elm = list_first_entry(&txq->pkt_queue, kgni_pkt_queue_t, list);
			GASSERT(queue_elm != NULL);
			if ((queue_elm->buf_info.vcid == KGNI_BTE_VCID_ANY) ||
			    (queue_elm->buf_info.vcid == bte_ctrl->vcid)) {
				/* we can send, return with TXQ locked to dequeue in caller */
				push_tx->locked_txq = txq;
				break;
			} else {
				/* TXQ is blocked, mark so we start here first next time;
				 * continue with next TXQ.
				 */
				GASSERT(kgni_dev->num_bte_chnls > 1);
				push_tx->blocked_txq_id = curr_txq_id;
			}
		}
		spin_unlock_irqrestore(&txq->pkt_lock, push_tx->pkt_flags);
	}

	if (push_tx->blocked_txq_id != KGNI_INVALID_BTE_TXQ) {
		push_tx->next_txq_id = push_tx->blocked_txq_id;
	}

	return push_tx->locked_txq;
}

/**
 * kgni_push_tx_get_queued_entry --
 *    - first acquires next non-full BTE channel to send request
 *    - get next TX backlog queued request for current channel (or any channel)
 *    - if request requires a different BTE channel, we retry using next channel
 *    - if we have BTE channel to send with, dequeue and return the BTE request
 *    - if returning a BTE request, the BTE channel's tx_lock remains locked
 **/
static kgni_pkt_queue_t *
kgni_push_tx_get_queued_entry(kgni_bte_push_tx_t *push_tx)
{
	kgni_device_t		*kgni_dev = push_tx->kgni_dev;
	kgni_bte_ctrl_t		*bte_ctrl;
	kgni_bte_queue_t	*txq;
	kgni_pkt_queue_t	*queue_elm = NULL;
	int 			chnls_left;

	/* single chnl requests will retry the loop below for each channel */
	chnls_left = kgni_dev->num_bte_chnls;

	/* get a chnl for sending (non-FULL) */
	bte_ctrl = kgni_push_tx_get_chnl(push_tx);
	while (bte_ctrl) {
		/* get a TXQ and then dequeue a request for our chnl */
		txq = kgni_push_tx_get_queue(push_tx, bte_ctrl);
		if (txq != NULL) {
			/* We can send request from queue on current chnl, dequeue and unlock TXQ.
			 * Return queue_elm and return with locked bte_ctrl->tx_lock;
			 */
			queue_elm = list_first_entry(&txq->pkt_queue, kgni_pkt_queue_t, list);
			GASSERT(queue_elm != NULL);
			list_del(&queue_elm->list);
			spin_unlock_irqrestore(&txq->pkt_lock, push_tx->pkt_flags);
			push_tx->locked_txq = NULL;
			break;
		} else {
			/* else backlogs are empty (and we are done) or blocked with single chnl request */
			queue_elm = NULL;
			spin_unlock_irqrestore(&bte_ctrl->tx_lock, push_tx->tx_flags);
			bte_ctrl = push_tx->locked_bte_ctrl = NULL;

			if ((push_tx->blocked_txq_id != KGNI_INVALID_BTE_TXQ) && (--chnls_left > 0))  {
				/* we have a request that can't be sent on current chnl; try next chnl. */
				bte_ctrl = kgni_push_tx_get_chnl(push_tx);
			}
		}
	}

	return queue_elm;
}

/**
 * kgni_push_tx_queue - tasklet to handle multiple backlog packet queues and BTE channels
 **/
static void kgni_push_tx_queue(unsigned long data)
{
	kgni_device_t		*kgni_dev = (void *)data;
	kgni_bte_ctrl_t		*bte_ctrl = NULL;
	kgni_pkt_queue_t	*queue_elm;
	kgni_ring_desc_t	*tx_ring;
	ghal_vc_desc_t 		*vc_desc;
	unsigned long           flags_rsrc;
	int 			entry;
	unsigned long		start = jiffies;
	kgni_bte_push_tx_t	push_tx_state;

	kgni_push_tx_init(kgni_dev, &push_tx_state);

	queue_elm = kgni_push_tx_get_queued_entry(&push_tx_state);
	while (queue_elm) {
		bte_ctrl = push_tx_state.locked_bte_ctrl;
		GASSERT(bte_ctrl != NULL);
		/* note, bte_ctrl->tx_lock is locked */
		tx_ring = &bte_ctrl->tx_ring;
		vc_desc = &bte_ctrl->bte_vc_desc;

		if (queue_elm->buf_info.type == KGNI_RDMA) {
			/* if ptag is gone or reallocated - do not launch RDMA */
			read_lock_irqsave(&kgni_dev->rsrc_lock, flags_rsrc);
			if (kgni_dev->resources[queue_elm->buf_info.ptag] &&
			    (kgni_dev->resources[queue_elm->buf_info.ptag]->generation ==
			    queue_elm->buf_info.rsrc_gen)) {
				KGNI_NEXT_RING_ENTRY(tx_ring, entry);
				tx_ring->buffer_info[entry].type = queue_elm->buf_info.type;
				tx_ring->buffer_info[entry].ptag = queue_elm->buf_info.ptag;
				tx_ring->buffer_info[entry].rsrc_gen = queue_elm->buf_info.rsrc_gen;
				GHAL_VC_WRITE_TXD((*vc_desc), entry, queue_elm->tx_desc);
				GHAL_VC_WRITE_TX_WRINDEX((*vc_desc), tx_ring->next_to_use);
			}
			read_unlock_irqrestore(&kgni_dev->rsrc_lock, flags_rsrc);
		} else {
			KGNI_NEXT_RING_ENTRY(tx_ring, entry);
			tx_ring->buffer_info[entry].type = queue_elm->buf_info.type;
			GHAL_VC_WRITE_TXD((*vc_desc), entry, queue_elm->tx_desc);
			GHAL_VC_WRITE_TX_WRINDEX((*vc_desc), tx_ring->next_to_use);
		}
		spin_unlock_irqrestore(&bte_ctrl->tx_lock, push_tx_state.tx_flags);
		push_tx_state.locked_bte_ctrl = NULL;
		kmem_cache_free(kgni_rdma_qelm_cache, queue_elm);

		if (jiffies - start > 1) {
			tasklet_schedule(&kgni_dev->bte_tx_tasklet);
			/* no locks are held, we can return */
			return;
		}

		/* get next queued TX request */
		queue_elm = kgni_push_tx_get_queued_entry(&push_tx_state);
	}

	return;
}

/**
 * kgni_proc_rx_queue - tasklet to handle incoming channel packets
 **/
static void kgni_proc_rx_queue(unsigned long data)
{
	kgni_bte_ctrl_t		*bte_ctrl = (void *) data;
	kgni_device_t		*kgni_dev = bte_ctrl->kgni_dev;
	kgni_bte_queue_t	*rxq = &bte_ctrl->rxq;
	kgni_rx_pktdsc_t	*pkt_desc;
	unsigned long           flags = 0;
	uint64_t		*bptr;
	uint64_t		immediate;
	int			i, rcv_rc = -1;
	unsigned long		start = jiffies;

	while (!list_empty(&rxq->pkt_queue)) {
		spin_lock_irqsave(&bte_ctrl->rx_lock, flags);
		pkt_desc = list_first_entry(&rxq->pkt_queue, kgni_rx_pktdsc_t, list);
		list_del(&pkt_desc->list);
		spin_unlock_irqrestore(&bte_ctrl->rx_lock, flags);

		/* use it to lookup up upper layer protocol handler */
		immediate = pkt_desc->immediate;
		/* kgni_rx_pktdsc_t structure resides in front of data_ptr,
		so we store offset of the payload within the buffer in the 64 bits
		immediately before the start of the payload in the pad[] zone*/
		bptr = ((uint64_t *)pkt_desc->buf_info.data_ptr) - 1;
		*bptr = (uint64_t)pkt_desc->buf_info.data_ptr - (uint64_t)pkt_desc;
		/* move ptr. to next qword in pad[] for channel */
		bptr  -= 1;

		/* !!! Implement lookaside cache where we would remember last
		 * channel number that we have found for the given dest_id
		 * (ptag+inst(8:0)) before and call this channel first. We
		 * should invalidate cache entry and go through the full lookup
		 * process in case rcv_handler() returns GNI_CHNL_RCV_NOTMINE */
		GDEBUG(5, "Got packet: ptag=%d pkey=%#x inst_id=%d 0x%llx",
			(uint8_t)KGNI_CHNL_GET_PTAG(immediate),
			(uint16_t)KGNI_CHNL_GET_PKEY(immediate),
			(uint8_t)KGNI_CHNL_GET_INST(immediate),
			immediate);

		/* use lock to make sure channel does not go away while
		we are in progress of giving it a packet */
		read_lock(&kgni_dev->chnl_lock);
		for (i = 0; i < KGNI_MAX_CHNL_NUMBER; i++) {
			if (kgni_dev->chnls[i] && 
			    kgni_dev->chnls[i]->rcv_handler &&
			    KGNI_CHNL_VERIFY_DEST(kgni_dev->chnls[i]->nic_hndl->ptag,
						  kgni_dev->chnls[i]->nic_hndl->pkey,
						  kgni_dev->chnls[i]->nic_hndl->inst_id,
						  immediate)) {

				/* chnl goes into the pad zone too for the future verification */
				*bptr = (uint64_t) kgni_dev->chnls[i];
				rcv_rc = kgni_dev->chnls[i]->rcv_handler(kgni_dev->chnls[i],
									 kgni_dev->chnls[i]->usr_cookie,
									 pkt_desc->buf_info.data_ptr,
									 pkt_desc->pkt_len);
				if (rcv_rc == GNI_CHNL_RCV_HANDLED) {
					break;
				}
			}
		}
		read_unlock(&kgni_dev->chnl_lock);

		if (i == KGNI_MAX_CHNL_NUMBER) {
			/* destination channel not found */
			kfree(pkt_desc);
			if (net_ratelimit()) {
			 	GDEBUG(1, "Unknown dest channel with ptag=%d inst=%d immediate=0x%llx rc=%d",
					(uint8_t)KGNI_CHNL_GET_PTAG(immediate),
					(uint8_t)KGNI_CHNL_GET_INST(immediate), immediate, rcv_rc);
	
			}
		}

		if (jiffies - start > 1) {
			tasklet_schedule(&bte_ctrl->rx_tasklet);
			break;
		}
	}
}

/**
 * kgni_irq_rx_hndlr - handle interrupts generated by BTE RX
 **/
static irqreturn_t kgni_irq_rx_hndlr(int irq, void *data)
{
	kgni_bte_ctrl_t         *bte_ctrl = (void *) data;
	kgni_device_t           *kgni_dev = bte_ctrl->kgni_dev;
	kgni_ring_desc_t        *rx_ring = &bte_ctrl->rx_ring;
	ghal_vc_desc_t          *vc_desc = &bte_ctrl->bte_vc_desc;
	kgni_bte_queue_t        *rxq = &bte_ctrl->rxq;
	uint16_t                entry;
	kgni_rx_pktdsc_t        *pkt_desc;
	irqreturn_t             irq_rc = IRQ_NONE;
	uint64_t                status;
	void                    *data_ptr;
	int                     last_entry = 0xFFFF;
	uint8_t                 copyout_data_on = kgni_dev->copyout_data_on;

	/* Only control channel packets comes here */
	entry = rx_ring->next_to_clean;
	status = GHAL_VC_GET_RXD_STATUS((*vc_desc), entry);
	while (GHAL_VC_IS_RXD_COMPLETE_SET(status)) {
		GHAL_VC_CLEAR_RXD_STATUS((*vc_desc), entry);
		irq_rc = IRQ_HANDLED;
		last_entry = entry;
		/* check for errors; log and discard bad packets */
		if (unlikely(GHAL_VC_IS_RXD_ERROR_SET(status) ||
			     !GHAL_VC_IS_RXD_IMMED_SET(status))) {
			if (ghal_rmt_rx_cam_miss(GHAL_VC_RX_ERR_LOCATION(status), GHAL_VC_RX_ERR_STATUS(status))) {
				/* This is just shortage of RX resources and we expect this to happen */
				GDEBUG(5, "Bad RX packet error: status 0x%llx location 0x%llx entry %d",
				       GHAL_VC_RX_ERR_STATUS(status), 
				       GHAL_VC_RX_ERR_LOCATION(status), entry);
			} else {
				if (net_ratelimit()) {
					GPRINTK_RL(0, KERN_INFO, "Bad RX packet error: status 0x%llx location 0x%llx entry %d",
					           GHAL_VC_RX_ERR_STATUS(status), 
					           GHAL_VC_RX_ERR_LOCATION(status), entry);
				}
			}

			if (!copyout_data_on) {
				kfree(rx_ring->buffer_info[entry].buff_ptr);
				rx_ring->buffer_info[entry].buff_ptr = NULL;
			}
			entry++;
			if (entry == rx_ring->count) {
				entry = 0;
			}
			status = GHAL_VC_GET_RXD_STATUS((*vc_desc), entry);

			continue;
		}
		/* We have a good packet with immediate data;
		queue it for processing */

		if (copyout_data_on) {
			pkt_desc = kgni_copy_rx_buff(bte_ctrl, rx_ring->buffer_info[entry].buff_ptr,
						     (rx_ring->buffer_info[entry].data_ptr - rx_ring->buffer_info[entry].buff_ptr),
						     GHAL_VC_GET_RXD_LENGTH((*vc_desc), entry),  
						     &data_ptr);
			if (pkt_desc == NULL) {
				GPRINTK_RL(0, KERN_ERR, "Packet got dropped due to kernel OOM. Datagram may not be able to complete!!!");
				entry++;
				if (entry == rx_ring->count) {
					entry = 0;
				}
				status = GHAL_VC_GET_RXD_STATUS((*vc_desc), entry);
				continue;
			}
			pkt_desc->buf_info.data_ptr = data_ptr;
		} else {
			pkt_desc = (kgni_rx_pktdsc_t *) rx_ring->buffer_info[entry].buff_ptr;
			pkt_desc->buf_info.data_ptr = rx_ring->buffer_info[entry].data_ptr;
			rx_ring->buffer_info[entry].buff_ptr = NULL;
		}
		pkt_desc->pkt_len = GHAL_VC_GET_RXD_LENGTH((*vc_desc), entry);
		pkt_desc->immediate = GHAL_VC_GET_RXD_IMMED((*vc_desc), entry);

		spin_lock(&bte_ctrl->rx_lock);
		list_add_tail(&pkt_desc->list, &rxq->pkt_queue);
		spin_unlock(&bte_ctrl->rx_lock);

		GDEBUG(5,"RX IRQ occured. We got packet len=%d imm=0x%llx",
		       pkt_desc->pkt_len, pkt_desc->immediate);

		entry++;
		if (entry == rx_ring->count) {
			entry = 0;
		}
		status = GHAL_VC_GET_RXD_STATUS((*vc_desc), entry);
	}


	if (irq_rc == IRQ_HANDLED) {
		rx_ring->next_to_clean = entry;
		tasklet_schedule(&bte_ctrl->rx_tasklet);
	}

	if (copyout_data_on) {
		if (last_entry != 0xFFFF) {
			GHAL_VC_WRITE_RX_WRINDEX((*vc_desc), last_entry);
		}
	} else {
		kgni_alloc_rx_buffs(bte_ctrl);
	}

	return IRQ_HANDLED;
}

static inline void
kgni_bte_tx_status_check(kgni_device_t *kgni_dev, uint8_t err_status, uint32_t ptag)
{
	ghal_error_event_t event = {0};
	struct timeval     tv;

	switch(err_status) {
	case GHAL_TXD_STATUS_UNCORRECTABLE_ERR:
	case GHAL_TXD_STATUS_UNCORRECTABLE_TRANSLATION_ERR:
		event.error_category = GHAL_TRANSACTION_ERRORS;
		break;
	case GHAL_TXD_STATUS_VMDH_INV:
	case GHAL_TXD_STATUS_MDD_INV:
	case GHAL_TXD_STATUS_PROTECTION_VIOLATION:
	case GHAL_TXD_STATUS_MEM_BOUNDS_ERR:
		event.error_category = GHAL_ADDRESS_TRANSLATION_ERRORS;
		break;
	case GHAL_TXD_STATUS_REQUEST_TIMEOUT:
		event.error_category = GHAL_TRANSIENT_ERRORS;
		break;
	default:
		return;
	}

	do_gettimeofday(&tv);

	event.error_code     = (GHAL_ERROR_MASK_BTE << 8) | (err_status + 64);
	event.serial_number  = -1;
	event.timestamp      = (tv.tv_sec << 32) | (tv.tv_usec & ((uint32_t)-1));
	event.ptag           = ptag;

	kgni_error_callback(&event,kgni_dev);
}

/**
 * kgni_irq_tx_flbte_hndlr - handle interrupts generated by BTE TX for FLBTE channels
 **/
static irqreturn_t kgni_irq_tx_flbte_hndlr(int irq, void *data)
{
	kgni_bte_ctrl_t		*bte_ctrl = (void *)data;
	kgni_ring_desc_t	*tx_ring = &bte_ctrl->tx_ring;
	ghal_vc_desc_t		*vc_desc = &bte_ctrl->bte_vc_desc;
	uint16_t		entry, first_cleaned;
	int			processed = 0;
	int			free = -1;
	irqreturn_t		irq_rc = IRQ_NONE;
	unsigned long 		flags;

	// GDEBUG(1,"TX IRQ occured");
	first_cleaned = entry = tx_ring->next_to_clean;

	GASSERT(bte_ctrl->mode == GHAL_VC_MODE_FLBTE);
	while (GHAL_VC_IS_TXD_COMPLETE((*vc_desc), entry)) {
		GHAL_VC_CLEAR_TXD_COMPLETE((*vc_desc), entry);
		irq_rc = IRQ_HANDLED;
		entry++;
		processed++;
		if (entry == tx_ring->count) {
			entry = 0;
		}
	}

	if (irq_rc == IRQ_HANDLED) {
		spin_lock_irqsave(&bte_ctrl->tx_lock, flags);
		if (tx_ring->status != KGNI_RING_LOCKED) {
			/* return TXDs for FLBTE use 
			 * - KGNI_RING_LOCKED prevents updating counter while tx_reclaim_tasklet is active.
			 * - in addition, tx_lock avoids collision with TX counter reset
			 */
			free = gcc_atomic_add(processed, bte_ctrl->tx_ring.desc_free);
			/* TODO - may want to detect counter corruption here, and do FLBTE TX reset */
		}
		/* we have more space now */
		tx_ring->next_to_clean = entry;
		spin_unlock_irqrestore(&bte_ctrl->tx_lock, flags);

		GDEBUG(5, "%d: TX IRQ occured (FLBTE), next %u -> %u, completed %d, free %d",
		       bte_ctrl->vcid, first_cleaned, entry, processed, free);
	}

	return IRQ_HANDLED;
}

/**
 * kgni_irq_tx_hndlr - handle interrupts generated by BTE TX (direct mode channels)
 **/
static irqreturn_t kgni_irq_tx_hndlr(int irq, void *data)
{
	kgni_bte_ctrl_t		*bte_ctrl = (void *)data;
	kgni_device_t		*kgni_dev = bte_ctrl->kgni_dev;
	kgni_ring_desc_t	*tx_ring = &bte_ctrl->tx_ring;
	ghal_vc_desc_t		*vc_desc = &bte_ctrl->bte_vc_desc;
	uint16_t		entry, first_cleaned;
	int			processed = 0;
	irqreturn_t		irq_rc = IRQ_NONE;
	unsigned long 		flags;

	// GDEBUG(1,"TX IRQ occured");
	first_cleaned = entry = tx_ring->next_to_clean;

	GASSERT(bte_ctrl->mode == GHAL_VC_MODE_DIRECT_BTE);
	while (GHAL_VC_IS_TXD_COMPLETE((*vc_desc), entry)) {
		GHAL_VC_CLEAR_TXD_COMPLETE((*vc_desc), entry);
		irq_rc = IRQ_HANDLED;

		switch (tx_ring->buffer_info[entry].type) {
		case KGNI_CHNL:
			/* 
			 * Packet is managed by the upper protocol.
			 * CQ event will be generated by the NIC to confirm
			 * the delivery.
			 */
			break;
		case KGNI_CQ:
			break;
		case KGNI_RDMA:
			read_lock_irqsave(&kgni_dev->rsrc_lock, flags);
			if (kgni_dev->resources[tx_ring->buffer_info[entry].ptag] &&
			    (kgni_dev->resources[tx_ring->buffer_info[entry].ptag]->generation ==
			    tx_ring->buffer_info[entry].rsrc_gen)) {
				/* bug if rdma count already zero */
				GASSERT(atomic_read(&kgni_dev->resources[tx_ring->buffer_info[entry].ptag]->rdma.count) != 0);
				atomic_dec(&kgni_dev->resources[tx_ring->buffer_info[entry].ptag]->rdma.count);
				/* counter is intended to provide count of direct BTE fallback when using FLBTE */ 
				atomic_inc(&kgni_dev->resources[tx_ring->buffer_info[entry].ptag]->direct_rdma.count);
			}
			read_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);

			kgni_bte_tx_status_check(kgni_dev,GHAL_VC_GET_TXD_ERROR((*vc_desc), entry),tx_ring->buffer_info[entry].ptag);
			break;
		default:
			GPRINTK_RL(0, KERN_ERR, "Unknown transaction type (%d)",
			           tx_ring->buffer_info[entry].type);
			GASSERT(0);
		}

		entry++;
		processed++;
		if (entry == tx_ring->count) {
			entry = 0;
		}
	}

	if (irq_rc == IRQ_HANDLED) {
		spin_lock_irqsave(&bte_ctrl->tx_lock, flags);
		/* we have more space now */
		tx_ring->next_to_clean = entry;
		tx_ring->status = (tx_ring->status == KGNI_RING_LOCKED) ? KGNI_RING_LOCKED : KGNI_RING_EMPTY;
		spin_unlock_irqrestore(&bte_ctrl->tx_lock, flags);
		if (!list_empty(&kgni_dev->domain_bte_txq[GNI_CDM_TYPE_USER].pkt_queue) ||
		    !list_empty(&kgni_dev->domain_bte_txq[GNI_CDM_TYPE_KERNEL].pkt_queue)) {
			tasklet_schedule(&kgni_dev->bte_tx_tasklet);
		}

		GDEBUG(5, "%d: TX IRQ occured (DIRECT) next %u -> %u, completed %d",
		       bte_ctrl->vcid, first_cleaned, entry, processed);
	}

	return IRQ_HANDLED;
}

/**
 * kgni_check_tx - Check TX ring for unprocessed descriptors. Workaround for bug #745510
 */
static void kgni_check_tx(struct work_struct *workq)
{
        kgni_bte_ctrl_t *bte_ctrl = container_of((struct delayed_work*) workq, kgni_bte_ctrl_t, tx_workq);
	int tx_irq = GHAL_IRQ_GNI_TX(bte_ctrl->kgni_dev->pdev, bte_ctrl->vcid);
	int rx_irq = GHAL_IRQ_GNI_RX(bte_ctrl->kgni_dev->pdev, bte_ctrl->vcid);
        int handled;

	/* kick TX handler */
	disable_irq(tx_irq);

	handled = kgni_irq_tx_hndlr(tx_irq, bte_ctrl);

	enable_irq(tx_irq);

	/* Check the RX ring for uninitialized descriptors.  This is required
	 * for when atomic kmalloc fails during the RX IRQ. */
	disable_irq(rx_irq);

	kgni_alloc_rx_buffs(bte_ctrl);

	enable_irq(rx_irq);

	/* kicking it from the send/post when we need to queue the request may not be enough. 
	In some circumstances we may end up stuck without periodic TX check. */
	schedule_delayed_work(&bte_ctrl->tx_workq, msecs_to_jiffies(1000));
}

/**
 * kgni_bte_tx_reclaim - tasklet to verify TX ring is no longer in use and reset FLBTE counter
 */
static void kgni_bte_tx_reclaim(unsigned long data)
{
	kgni_bte_ctrl_t		*bte_ctrl = (kgni_bte_ctrl_t *)data;
	kgni_ring_desc_t	*tx_ring = &bte_ctrl->tx_ring;
	ghal_vc_desc_t		*vc_desc = &bte_ctrl->bte_vc_desc;
	unsigned long		flags;
	unsigned		rd_idx = 0, wr_idx = 0;

	/* BTE critical error handler may have reset chnl and cleared LOCKED; in which case we are done. */
	if (tx_ring->status != KGNI_RING_LOCKED) {
		return;
	}

	/* if hardware has outstanding writes to COMPLETE or waiting for TX_IRQ to mark 
	 * them as CLEARED, then reschedule
	 */
	GHAL_VC_TXC_GET_RW_IDX((*vc_desc), rd_idx, wr_idx);
	if ((rd_idx != wr_idx) || (rd_idx != tx_ring->next_to_clean)) {
		if (bte_ctrl->tx_reclaim_count < KGNI_MAX_BTE_TX_RECLAIM_RESCHED) {
			tasklet_schedule(&bte_ctrl->tx_reclaim_tasklet);
			bte_ctrl->tx_reclaim_count++;
		} else {
			spin_lock_irqsave(&bte_ctrl->tx_lock, flags);
			tx_ring->status = KGNI_RING_EMPTY;
			spin_unlock_irqrestore(&bte_ctrl->tx_lock, flags);
			bte_ctrl->tx_reclaim_count = 0;

			GPRINTK_RL(0, KERN_INFO, "TX reclaim for vcid %d timed-out.", (int)bte_ctrl->vcid);
		}
		return;
	}

	/* we can safely reset TX counter */
	spin_lock_irqsave(&bte_ctrl->tx_lock, flags);
	tx_ring->status = KGNI_RING_EMPTY;
	atomic_set(tx_ring->desc_free, tx_ring->count);
	spin_unlock_irqrestore(&bte_ctrl->tx_lock, flags);
	bte_ctrl->tx_reclaim_count = 0;

	GDEBUG(1, "TX reclaim for vcid %d complete.", (int)bte_ctrl->vcid);
	return;
}

static void inline kgni_bte_chnl_del(kgni_bte_ctrl_t *bte_ctrl)
{
	kgni_device_t *kgni_dev = bte_ctrl->kgni_dev;

	switch (bte_ctrl->mode) {
	case GHAL_VC_MODE_FLBTE:
		kgni_dev->num_flbte_chnls--;
		break;
	case GHAL_VC_MODE_DIRECT_BTE:
		kgni_dev->num_bte_chnls--;
		break;
	default:
		break;
	}
}

static inline void kgni_bte_chnl_add(kgni_bte_ctrl_t *bte_ctrl)
{
	kgni_device_t *kgni_dev = bte_ctrl->kgni_dev;

	kgni_dev->bte_ctrl[bte_ctrl->vcid] = bte_ctrl;
	switch (bte_ctrl->mode) {
	case GHAL_VC_MODE_FLBTE:
		kgni_dev->num_flbte_chnls++;
		break;
	case GHAL_VC_MODE_DIRECT_BTE:
		kgni_dev->num_bte_chnls++;
		break;
	default:
		break;
	}
}

static void kgni_bte_chnl_reset(kgni_bte_ctrl_t *bte_ctrl, int new_mode)
{
	unsigned long flags;

	down(&bte_ctrl->desc_sem);
	kgni_bte_down(bte_ctrl, new_mode);
	if (bte_ctrl->mode != new_mode) {
		GPRINTK_RL(0, KERN_WARNING, "changing BTE chnl for vcid %d to mode %s",
		           bte_ctrl->vcid, gni_vc_mode_strs[new_mode]);
		kgni_bte_chnl_del(bte_ctrl);
		bte_ctrl->mode = new_mode;
		kgni_bte_chnl_add(bte_ctrl);
	}
	kgni_bte_up(bte_ctrl);
	up(&bte_ctrl->desc_sem);

	/* unlock access to chnl */
	spin_lock_irqsave(&bte_ctrl->tx_lock, flags);
	bte_ctrl->tx_ring.status = KGNI_RING_EMPTY;
	spin_unlock_irqrestore(&bte_ctrl->tx_lock, flags);
}

static void kgni_bte_reset(struct work_struct *workq)
{
	kgni_bte_ctrl_t *bte_ctrl = container_of(workq, kgni_bte_ctrl_t, rst_workq);
	kgni_device_t	*kgni_dev = bte_ctrl->kgni_dev;

	kgni_bte_chnl_reset(bte_ctrl, bte_ctrl->mode);
	GDEBUG(1," Marked [bte %d] TX fifo EMPTY", bte_ctrl->vcid);

	/* re-enable the FLBTE counter reset tasklet */
	tasklet_enable(&bte_ctrl->tx_reclaim_tasklet);

	/* packet queue is now able to drain */
	if (bte_ctrl->mode != GHAL_VC_MODE_FLBTE) {
		if (!list_empty(&kgni_dev->domain_bte_txq[GNI_CDM_TYPE_USER].pkt_queue) ||
		    !list_empty(&kgni_dev->domain_bte_txq[GNI_CDM_TYPE_KERNEL].pkt_queue)) {
			tasklet_schedule(&kgni_dev->bte_tx_tasklet);
		}
	}
}

static void kgni_error_handler(const ghal_error_event_t *event, void *in)
{
	kgni_bte_ctrl_t *bte_ctrl = (kgni_bte_ctrl_t *)in;
	unsigned long flags = 0;

	GDEBUG(1," BTE channel error detected.  All channels will be "
			"restarted. Event description:");
	GDEBUG(1," sub-block: %u bit: %u cat: %u ptag: %u serial: %u "
			"time: 0x%llX info0: 0x%llX info1: 0x%llX "
			"info2: 0x%llX info3: 0x%llX",
			READ_ERR_TYPE(event->error_code),
			READ_ERR_BITS_SET(event->error_code),
			event->error_category,
			event->ptag,event->serial_number,
			event->timestamp,
			event->info_mmrs[0], event->info_mmrs[1],
			event->info_mmrs[2], event->info_mmrs[3]);
	GDEBUG(1," Marking [bte %d] TX fifo LOCKED", bte_ctrl->vcid);

	/* disable FLBTE counter reset tasklet during reset of BTE channel */
	tasklet_disable(&bte_ctrl->tx_reclaim_tasklet);

	/* Force all future sends to go on packet queue */
	spin_lock_irqsave(&bte_ctrl->tx_lock, flags);
	bte_ctrl->tx_ring.status = KGNI_RING_LOCKED;
	spin_unlock_irqrestore(&bte_ctrl->tx_lock, flags);

	GPRINTK_RL(0, KERN_INFO, "Restarting BTE vcid %d",
			    bte_ctrl->vcid);
	schedule_work(&bte_ctrl->rst_workq);
}

/**
 * kgni_get_vc - Get the GHAL VC
 **/
static int kgni_get_vc(kgni_bte_ctrl_t *bte_ctrl)
{
	ghal_vc_reg_t vc_reg;
	int err;

	vc_reg.mode = bte_ctrl->mode;
	vc_reg.vcid = bte_ctrl->vcid;
	vc_reg.max_len = bte_ctrl->max_len;
	vc_reg.tx_idv = bte_ctrl->tx_idv;
	vc_reg.rx_idv = bte_ctrl->rx_idv;
	vc_reg.tx_timeout = KGNI_BTE_TX_TIMEOUT;

	err = ghal_get_vc(bte_ctrl->kgni_dev->pdev, &vc_reg);
	if (err) return err;

	bte_ctrl->bte_vc_desc = vc_reg.vc_desc;
	bte_ctrl->kgni_dev->nic_pe = vc_reg.nic_pe;
	return 0;
}

/**
 * kgni_setup_isr - Setup the interrupts
 **/
static int kgni_setup_isr(kgni_bte_ctrl_t *bte_ctrl)
{
	int err;

	tasklet_enable(&bte_ctrl->kgni_dev->bte_tx_tasklet);
	tasklet_enable(&bte_ctrl->tx_reclaim_tasklet);
	tasklet_enable(&bte_ctrl->rx_tasklet);

	err = request_irq(GHAL_IRQ_GNI_RX(bte_ctrl->kgni_dev->pdev, bte_ctrl->vcid), kgni_irq_rx_hndlr, 0, kgni_driver_name, bte_ctrl);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "request_irq() returned error %d for GNI_RX, vcid %d", err, bte_ctrl->vcid);
		goto err_irq_gni_rx;
	}
	err = request_irq(GHAL_IRQ_GNI_TX(bte_ctrl->kgni_dev->pdev, bte_ctrl->vcid),
			  (bte_ctrl->mode == GHAL_VC_MODE_FLBTE) ? kgni_irq_tx_flbte_hndlr : kgni_irq_tx_hndlr,
			  0, kgni_driver_name, bte_ctrl);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "request_irq() returned error %d for GNI_TX, vcid %d", err, bte_ctrl->vcid);
		goto err_irq_gni_tx;
	}

	return 0;

 err_irq_gni_tx:
	free_irq(GHAL_IRQ_GNI_RX(bte_ctrl->kgni_dev->pdev, bte_ctrl->vcid), bte_ctrl);
 err_irq_gni_rx:
	tasklet_disable(&bte_ctrl->kgni_dev->bte_tx_tasklet);
	tasklet_disable(&bte_ctrl->tx_reclaim_tasklet);
	tasklet_disable(&bte_ctrl->rx_tasklet);
	return err;
}

/**
 * kgni_remove_isr - Clean the interrupts up
 **/
static void kgni_remove_isr(kgni_bte_ctrl_t *bte_ctrl)
{
	struct pci_dev *pdev = bte_ctrl->kgni_dev->pdev;
	int vcid = bte_ctrl->vcid;

	synchronize_irq(GHAL_IRQ_GNI_RX(pdev, vcid));
	synchronize_irq(GHAL_IRQ_GNI_TX(pdev, vcid));
	free_irq(GHAL_IRQ_GNI_TX(pdev, vcid), bte_ctrl);
	free_irq(GHAL_IRQ_GNI_RX(pdev, vcid), bte_ctrl);
	tasklet_disable(&bte_ctrl->kgni_dev->bte_tx_tasklet);
	tasklet_disable(&bte_ctrl->tx_reclaim_tasklet);
	tasklet_disable(&bte_ctrl->rx_tasklet);
}

static ghal_error_mask_t
kgni_get_error_mask(kgni_device_t *kgni_dev, int vcid)
{
	ghal_error_mask_t mask;
	struct pci_dev *pdev = kgni_dev->pdev;

	switch (vcid) {
	case GHAL_VC_GNI0:
		GHAL_BUILD_BTE_ERR_MASK(mask, GHAL_VC_GNI0);
		break;
	case GHAL_VC_GNI1:
		GHAL_BUILD_BTE_ERR_MASK(mask, GHAL_VC_GNI1);
		break;
	case GHAL_VC_GNI2:
		GHAL_BUILD_BTE_ERR_MASK(mask, GHAL_VC_GNI2);
		break;
	default:
		break;
	}

	/* prevent warning for gemini which doesn't use pdev in GHAL_BUILD_BTE_ERR_MASK() */
	pdev = NULL;

	return mask;
}

/**
 * kgni_bte_configure - configure BTE channel for the virtual channel
 **/
int kgni_bte_chnl_configure(kgni_device_t *kgni_dev, int vcid, int mode)
{
	int err = 0;
	kgni_bte_ctrl_t *bte_ctrl;
	ghal_error_mask_t mask;

	GASSERT(vcid < GHAL_BTE_CHNL_MAX_NUMBER);
	GASSERT(kgni_dev->bte_ctrl[vcid] == NULL);
	bte_ctrl = kzalloc(sizeof(kgni_bte_ctrl_t), GFP_KERNEL);
	if (bte_ctrl == NULL) {
		GPRINTK_RL(0, KERN_ERR, "Failed to allocate bte_ctrl for vcid %d", vcid);
		err = -ENOMEM;
		goto err_kzalloc;
	}
	bte_ctrl->kgni_dev = kgni_dev;
	bte_ctrl->mode = mode;
	bte_ctrl->vcid = vcid;
	bte_ctrl->tx_idv = KGNI_BTE_TX_IDV;
	bte_ctrl->rx_idv = KGNI_BTE_RX_IDV;
	bte_ctrl->max_len = GHAL_BTE_MAX_LENGTH;
	sema_init(&(bte_ctrl->desc_sem), 1); // 1, for unlocked

	/* setup TX/RX queues */
	kgni_conf_backlog(&bte_ctrl->rxq);

	/* Get BTE Virtual channel */
	err = kgni_get_vc(bte_ctrl);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "kgni_get_vc() returned error %d", err);
		goto err_get_vc;
	}

	err = kgni_conf_rings(bte_ctrl);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "kgni_conf_rings() returned error %d", err);
		goto err_cfg_ring;
	}

	err = kgni_alloc_rx_buffs(bte_ctrl);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "kgni_alloc_rx_buffs() returned error %d", err);
		goto err_alloc_buff;
	}

	/* call before kgni_setup_isr() */
	tasklet_init(&bte_ctrl->tx_reclaim_tasklet, kgni_bte_tx_reclaim, (uint64_t)bte_ctrl);
	tasklet_init(&bte_ctrl->rx_tasklet, kgni_proc_rx_queue, (uint64_t)bte_ctrl);
	tasklet_disable(&bte_ctrl->kgni_dev->bte_tx_tasklet);
	tasklet_disable(&bte_ctrl->tx_reclaim_tasklet);
	tasklet_disable(&bte_ctrl->rx_tasklet);

	INIT_WORK(&bte_ctrl->rst_workq,kgni_bte_reset);

	err = kgni_setup_isr(bte_ctrl);
	if (err) {
		goto err_setup_isr;
	}

        INIT_DELAYED_WORK(&bte_ctrl->tx_workq, kgni_check_tx);

	mask = kgni_get_error_mask(kgni_dev, vcid);
	bte_ctrl->err_handle = ghal_subscribe_errors(kgni_dev->pdev, &mask,
						     kgni_error_handler, bte_ctrl);
	if (!bte_ctrl->err_handle) {
		GPRINTK_RL(0, KERN_ERR, "Failed to subscribe for user errors");
		err = -ENOMEM;
		goto err_subscribe_out;
	}
	
	kgni_bte_chnl_add(bte_ctrl);

	return 0;

 err_subscribe_out:
	kgni_remove_isr(bte_ctrl);
	tasklet_kill(&bte_ctrl->tx_reclaim_tasklet);
	tasklet_kill(&bte_ctrl->rx_tasklet);
 err_setup_isr:
	kgni_free_rx_buffs(bte_ctrl);
 err_alloc_buff:
	kgni_release_rings(bte_ctrl);
 err_cfg_ring:
	ghal_put_vc(kgni_dev->pdev, vcid);
 err_get_vc:
	kfree(bte_ctrl);
 err_kzalloc:
	return err;
}

static int kgni_bte_configure(kgni_device_t *kgni_dev, unsigned chnls)
{
	int err, i;
	unsigned available_chnls;

	kgni_dev->num_bte_chnls = 0;
	kgni_dev->num_flbte_chnls = 0;
	atomic_set(&kgni_dev->cur_bte_chnl, 0);
	atomic_set(&kgni_dev->cur_flbte_chnl, 0);

	/* setup BTE TX backlog processing */
	kgni_conf_backlog(&kgni_dev->domain_bte_txq[GNI_CDM_TYPE_USER]);
	kgni_conf_backlog(&kgni_dev->domain_bte_txq[GNI_CDM_TYPE_KERNEL]);
	tasklet_init(&kgni_dev->bte_tx_tasklet, kgni_push_tx_queue, (uint64_t)kgni_dev);

	available_chnls = 1 + (GHAL_VC_GNI_LAST - GHAL_VC_GNI_FIRST);
	if (chnls == 0 || chnls > available_chnls) {
		GPRINTK_RL(0, KERN_INFO, "Only valid values for 'bte_chnls' parameter are [1 - %u] !!!", available_chnls);
		err = (-EINVAL);
		goto err_bte_conf;
	}

	/* setup direct mode BTE channels */
	for (i = 0; i < chnls; i++) {
		err = kgni_bte_chnl_configure(kgni_dev, GHAL_VC_GNI_FIRST + i, GHAL_VC_MODE_DIRECT_BTE);
		if (err) {
			GPRINTK_RL(0, KERN_ERR, "kgni_bte_chnl_configure(%d) (DIRECT) returned error %d",
			           GHAL_VC_GNI_FIRST + i, err);
			goto err_bte_conf;
		}
	}

	/* set pointers for 'single BTE' and datagram BTE users to appropriate direct BTE chnl */
	GASSERT(kgni_dev->bte_ctrl[GHAL_VC_GNI_USR] != NULL);
	kgni_dev->domain_bte_ctrl[GNI_CDM_TYPE_USER] = kgni_dev->bte_ctrl[GHAL_VC_GNI_USR];
	if (chnls > 1) {
		if (chnls > 2) {
			GPRINTK_RL(0, KERN_INFO, "Using distinct BTE channels for kernel and user ranks");
		} else {
			GPRINTK_RL(0, KERN_INFO, "Using separate BTE channels for kernel and user domains");
		}
		GASSERT(kgni_dev->bte_ctrl[GHAL_VC_GNI_KRN] != NULL);
		kgni_dev->domain_bte_ctrl[GNI_CDM_TYPE_KERNEL] = kgni_dev->bte_ctrl[GHAL_VC_GNI_KRN];
	} else {
		GPRINTK_RL(0, KERN_INFO, "Using single BTE channel for kernel and user domains");
		kgni_dev->domain_bte_ctrl[GNI_CDM_TYPE_KERNEL] = kgni_dev->bte_ctrl[GHAL_VC_GNI_USR];
	}

	/* setup any unused channels for FLBTE (Aries) */
	err = kgni_flbte_configure(kgni_dev, (available_chnls - chnls));
	if (err) {
		goto err_bte_conf;
	}

	return 0;

err_bte_conf:
	kgni_bte_release(kgni_dev);
	return err;
}

/**
 * kgni_bte_release - release BTE channel for the domain
 **/
static void kgni_bte_chnl_release(kgni_bte_ctrl_t *bte_ctrl)
{
	if (bte_ctrl->err_handle != NULL) {
		ghal_release_errors(bte_ctrl->err_handle);
	}

	cancel_delayed_work(&bte_ctrl->tx_workq);
	flush_scheduled_work();

	kgni_remove_isr(bte_ctrl);
	tasklet_kill(&bte_ctrl->tx_reclaim_tasklet);
	tasklet_kill(&bte_ctrl->rx_tasklet);
	ghal_reset_vc(bte_ctrl->kgni_dev->pdev, bte_ctrl->vcid, GHAL_VC_MODE_NONE);

	kgni_free_rx_buffs(bte_ctrl);
	kgni_release_rings(bte_ctrl);
	ghal_put_vc(bte_ctrl->kgni_dev->pdev, bte_ctrl->vcid);
	kfree(bte_ctrl);
}

static void kgni_bte_release(kgni_device_t *kgni_dev)
{
	int id;

	for (id = 0; id < GHAL_BTE_CHNL_MAX_NUMBER; id ++) {
		if (kgni_dev->bte_ctrl[id] != NULL) {
			kgni_bte_chnl_release(kgni_dev->bte_ctrl[id]);
			kgni_dev->bte_ctrl[id] = NULL;
		}
	}
	tasklet_kill(&kgni_dev->bte_tx_tasklet);

	return;
}

/**
 * kgni_bte_restart - Restart BTE transactions
 **/
static int kgni_bte_restart(kgni_bte_ctrl_t *bte_ctrl)
{
	int err;

	err = kgni_alloc_rx_buffs(bte_ctrl);
	if (err) {
		/* kgni_alloc_rx_buffs() should not fail here.  If it does, we
		 * can recover later when kgni_check_rx() runs. */
		GPRINTK_RL(1, KERN_ERR, "kgni_alloc_rx_buffs() returned error %d", err);
	}

	err = kgni_setup_isr(bte_ctrl);
	if (err) {
		goto err_setup_isr;
	}

	return 0;
 err_setup_isr:
	kgni_free_rx_buffs(bte_ctrl);
	return err;
}

/**
 * kgni_job_info_next - add one more entry to the list
 **/
static void kgni_job_info_add(kgni_device_t *kgni_dev, kgni_job_infomsg_t *job_info)
{
	spin_lock(&kgni_dev->job_info_lock);
	list_add_tail(&job_info->list,&kgni_dev->job_info_list);
	spin_unlock(&kgni_dev->job_info_lock);

	schedule_work(&kgni_dev->job_info_workq);
}

/**
 * kgni_job_info_next - get next entry in the list
 **/
static kgni_job_infomsg_t *kgni_job_info_next(kgni_device_t *kgni_dev)
{
	kgni_job_infomsg_t *job_info = NULL;

	spin_lock(&kgni_dev->job_info_lock);
	if(!list_empty(&kgni_dev->job_info_list)) {
		job_info = list_entry(kgni_dev->job_info_list.next, kgni_job_infomsg_t, list);
		list_del(&job_info->list);
	}
	spin_unlock(&kgni_dev->job_info_lock);
	return job_info;
}

static inline struct task_struct *kgni_task_from_pid(pid_t pid)
{
	struct task_struct *task;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
	task = find_task_by_vpid(pid);
#else
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
#endif
	return task;
}

/**
 * kgni_job_info - submit Job abort info and send the signal
 **/
static void kgni_job_info(struct work_struct *workq)
{
	kgni_device_t *kgni_dev = container_of(workq, kgni_device_t, job_info_workq);
	kgni_job_infomsg_t  *job_info;
	struct task_struct *task;
	int rc;

	while((job_info = kgni_job_info_next(kgni_dev))) {
		/* Up until this workq, we were sure that the
		 * gni_nic_handle was still around. Meaning the
		 * application hadn't exited yet. However, because the
		 * nic_handle is separated from the task struct, we
		 * need to verify the task/pid combo are still equal
		 * to what our nic_handle reported.
		*/
		rcu_read_lock();
		task = kgni_task_from_pid(job_info->pid);

		if(task && (task == job_info->task)) {
#ifdef CONFIG_CRAY_ABORT_INFO
			rcu_read_unlock();
			/* This function sleeps. Only call without holding
			 * locks, and from a kernel thread. */
			rc = job_set_abort_info(task_pid_nr(task), job_info->info);
			if (rc) {
				GDEBUG(1, " job_set_abort_info() returned error %d", rc);
			}
			rcu_read_lock();

			task = kgni_task_from_pid(job_info->pid);
			if((!task) || (task != job_info->task)) {
				GDEBUG(1, " Process '%s' (pid %d, err_code %d) exited after job_set_abort_info",
				       job_info->task->comm, job_info->task->tgid, job_info->err_code);
				goto skip_signal;
			}
#endif /* CONFIG_CRAY_ABORT_INFO */
		} else {
			GDEBUG(1, " Process '%s' (pid %d, err_code %d) exited before job_set_abort_info",
			       job_info->task->comm, job_info->task->tgid, job_info->err_code);
			goto skip_signal;
		}

		rc = send_sig(SIGKILL, task, 1);
		/* Print out information explaining SIGKILL. */
		GDEBUG(1, " Sending SIGKILL to process '%s' (task %p, pid %d, err_code 0x%x) (send_sig returned: %d)",
		       task->comm, task, task->tgid, job_info->err_code, rc);

	skip_signal:
		rcu_read_unlock();
		put_task_struct(job_info->task);
		kfree(job_info);
	}
}

/**
 * kgni_kill_app - send a sig_kill to applicable applications.
 * Linux kernel threads are not applicable.
 **/
void kgni_kill_app(gni_nic_handle_t nic_hndl, const char *message, uint16_t err_code, uint16_t err_ptag)
{
	pid_t pid = nic_hndl->pid;
	struct task_struct *task;
	kgni_job_infomsg_t  *job_info;
	int rc;

	/* Serialize schedule, or rather lock the tasklist so a process doesn't get
	 * destroyed while we are walking the list. */
	rcu_read_lock();

	/* To prevent sending a signal to a bad task pointer,
	 * we check the pid hash for every entry in the
	 * fifo. If it returns a task, we can send it a signal.
	 */
	task = kgni_task_from_pid(pid);

	if(!task) {
		goto out;
	}

	if(task->flags & PF_KTHREAD) {
		goto out;
	}

	/* will only issue a single message per nic_handle */
	job_info = kmalloc(sizeof(kgni_job_infomsg_t), GFP_ATOMIC);
	if (job_info != NULL) {
		get_task_struct(task); /* Ensure the task doesn't vanish on us */
		job_info->pid = pid;
		job_info->task = task;
		job_info->err_code = err_code;
		if (message == NULL) {
			scnprintf(job_info->info, KGNI_JOB_INFO_SIZE, "Cray HSN detected critical error 0x%x[ptag %d]. Please contact admin for details."
					   " Killing pid %d(%s)", err_code, err_ptag, pid, task->comm);
		} else {
			scnprintf(job_info->info, KGNI_JOB_INFO_SIZE,"%s Killing pid %d(%s)", message, pid, task->comm);
		}
		kgni_job_info_add(nic_hndl->kdev_hndl, job_info);
	} else {
		rc = send_sig(SIGKILL, task, 1);
		/* Print out information explaining SIGKILL. */
		GDEBUG(1, "Sending SIGKILL to process '%s' "
		       "(task %p, pid %d, err_code 0x%x) (send_sig returned: %d)",
		       task->comm, task, task->tgid, err_code, rc);
	}

 out:
	rcu_read_unlock();
}

void kgni_bte_notify_kern(gni_nic_handle_t nic_handle)
{
	gni_eeq_t *eeq;

	rcu_read_lock();
	list_for_each_entry_rcu(eeq, &nic_handle->eeqs, list) {
		if (eeq->app_crit_err) {
			GDEBUG(1," Notifying kern instance via app_crit_err for BTE reset");
			eeq->app_crit_err(eeq->err_handle);
		}
	}
	rcu_read_unlock();
}

/**
 * kgni_kill_tx_apps - While bringing down interface, kill
 * applications that are in the process of sending data.
 **/
static void kgni_kill_tx_apps(kgni_bte_ctrl_t *bte_ctrl)
{
	kgni_ring_desc_t *tx_ring = &bte_ctrl->tx_ring;
	uint32_t count = tx_ring->count;
	uint32_t clean = tx_ring->next_to_clean;
	uint32_t use   = tx_ring->next_to_use;
	char str[256];

	scnprintf(str, 256, "Cray HSN detected critical error and needs to re-start BTE TX interface.");
	GDEBUG(1," BTE kill_tx_apps (mode %d rd_idx %d wr_idx %d)", bte_ctrl->mode, (int)clean, (int)use);

	if (bte_ctrl->mode == GHAL_VC_MODE_FLBTE) {
		/* In FLBTE mode, we have no knowledge of whom (ptag, cdm_hnl) is using
		 * channel; so instead we perform kgni_kill_app for all cdm_handles.
		 */
		kgni_cdm_kill_all(str);
		return;
	}

	while(clean != use) {
		uint32_t cdm_index = 0;
		gni_cdm_handle_t cdm_handle;
		int flag = 0;

		while(cdm_index < KGNI_MAX_CDM_NUMBER) {
			cdm_handle =
				kgni_cdm_lookup_next(tx_ring->buffer_info[clean].ptag,&cdm_index);
			if(cdm_handle) {
				flag |= kgni_cdm_kill_apps(cdm_handle, str);
			}
		}
		if(!flag) {
			GPRINTK_RL(0, KERN_WARNING, "Potential lost message on ptag '%u'",
			           tx_ring->buffer_info[clean].ptag);
		}

		if(++clean == count) clean = 0;
	}
}

static void kgni_qsce_timer(struct work_struct *workq)
{
	kgni_device_t *kgni_dev = container_of((struct delayed_work *)workq, kgni_device_t, qsce_workq);
	uint64_t		duration_time = 0;
	struct list_head	*ptr;
	gni_nic_handle_t	nic_handle;

	if (ghal_quiesce_completed(kgni_dev->pdev, &duration_time)) {
		GDEBUG(1," Quiesce completed %lldms", duration_time);
		spin_lock(&kgni_dev->qsce_lock);
		list_for_each(ptr, &kgni_dev->qsce_list) {
			nic_handle = list_entry(ptr, struct gni_nic, qsce_list);
			nic_handle->qsce_func(nic_handle, duration_time);
		}
		spin_unlock(&kgni_dev->qsce_lock);
		ghal_quiesce_complete(kgni_dev->pdev);
	}

	schedule_delayed_work(&kgni_dev->qsce_workq, msecs_to_jiffies(qsce_delay));
}

/**
 * kgni_reset_rings - Reset ring pointers to zero
 **/
static void kgni_reset_rings(kgni_bte_ctrl_t *bte_ctrl)
{
	bte_ctrl->tx_ring.next_to_clean = 0;
	bte_ctrl->tx_ring.next_to_use   = 0;
	atomic_set(bte_ctrl->tx_ring.desc_free, bte_ctrl->tx_ring.count);

	bte_ctrl->rx_ring.next_to_clean = 0;
	bte_ctrl->rx_ring.next_to_use   = 0;
}

/**
 * kgni_bte_stop - Stop all BTE transactions and clean up state for outstanding desc
 *
 **/
static void kgni_bte_stop(kgni_bte_ctrl_t *bte_ctrl, int new_mode)
{
	struct pci_dev *pdev = bte_ctrl->kgni_dev->pdev;

	/* disable IRQs, then reset channel and descriptors */
	kgni_remove_isr(bte_ctrl);
	ghal_reset_vc(pdev, bte_ctrl->vcid, new_mode);

	kgni_kill_tx_apps(bte_ctrl);
	kgni_free_rx_buffs(bte_ctrl);
	kgni_reset_rings(bte_ctrl);
}

/**
 * kgni_bte_up - BTE reinitialization after a subsequent kgni_bte_down
 *
 **/
int kgni_bte_up(kgni_bte_ctrl_t *bte_ctrl)
{
	ghal_error_mask_t mask;
	int err;

	err = kgni_bte_restart(bte_ctrl);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "kgni_bte_restart returned error %d", err);
		return err;
	}

	mask = kgni_get_error_mask(bte_ctrl->kgni_dev, bte_ctrl->vcid);
	ghal_set_error_mask(bte_ctrl->err_handle,&mask);

	/* Enable channels */
	GHAL_VC_TXC_ENABLE(bte_ctrl->bte_vc_desc, 1);
	GHAL_VC_RXC_ENABLE(bte_ctrl->bte_vc_desc, 1);
	GHAL_VC_WR_RXC_BUFFSIZE(bte_ctrl->bte_vc_desc, bte_ctrl->rx_buff_size);

	GDEBUG(1," kGNI BTE interface to Gemini is up");
	return 0;
}

/**
 * kgni_bte_down - BTE finialization
 *
 **/
void kgni_bte_down(kgni_bte_ctrl_t *bte_ctrl, int mode)
{
	ghal_error_mask_t mask = {{0}};

	GHAL_VC_TXC_DISABLE(bte_ctrl->bte_vc_desc);
	GHAL_VC_RXC_DISABLE(bte_ctrl->bte_vc_desc);
	ghal_set_error_mask(bte_ctrl->err_handle,&mask);
	kgni_bte_stop(bte_ctrl, mode);
	GDEBUG(1," kGNI BTE interface to Gemini is down");
}

static void kgni_numa_desc_init(kgni_device_t *kdev)
{
	kgni_numa_desc_t *nd = &kdev->numa_desc;
	uint8_t nodes[MAX_NUMNODES] = {0};
	int node_cnt = 0;
	int node;
	int cpu;

	/* Find NUMA mapping for all cpus that could ever be online */
	nd->cpu_cnt = num_possible_cpus();

	for_each_cpu(cpu, cpu_possible_mask) {
		nd->cpus[cpu].cpu = cpu;

		node = cpu_to_node(cpu);
		nd->cpus[cpu].node = node;

		if (!nodes[node]) {
			node_cnt++;
			nodes[node] = 1;
		}
	}

	nd->node_cnt = node_cnt;
}

static void kgni_numa_desc_fini(kgni_device_t *kdev)
{
	return;
}

/**
 * kgni_probe - device initialization routine
 *
 * Called by ghal_probe().
 * Returns 0 on success.
 **/
static int kgni_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int err, i, j;
	kgni_device_t *kgni_dev;
	dev_t devno;

	kgni_dev = kzalloc(sizeof(kgni_device_t), GFP_KERNEL);
	if (kgni_dev == NULL) {
		GPRINTK_RL(0, KERN_ERR, "Failed to allocate kgni_dev");
		err = (-ENOMEM);
		goto err_alloc_kgni;
	}

#ifdef CRAY_CONFIG_GHAL_GEMINI
	/* Pre 2.2 Gemini requires a workaround for Bug 755622. */
	if(pdev->revision == GEM_PCI_REV_1_0 || pdev->revision == GEM_PCI_REV_1_1) {
		kgni_dev->do_rx_readback = 1;
		kgni_dev->copyout_data_on = 1;
		GPRINTK_RL(0, KERN_INFO, "Gemini version < 1.2 (pci revision: 0x%x)",
		           pdev->revision);
	} else {
		kgni_dev->do_rx_readback = 0;
		kgni_dev->copyout_data_on = 0;
		GPRINTK_RL(0, KERN_INFO, "Gemini version >= 1.2 (pci revision: 0x%x)",
		           pdev->revision);
	}
#else
	kgni_dev->do_rx_readback = 0;
	kgni_dev->copyout_data_on = 0;
#endif

	/* can not have one big table because of kmalloc size limitation */
	for (i = 0; i < GNI_MDD_DIR_MAX_NUMBER; i++) {
		kgni_dev->mdd_registry[i] = kzalloc(GNI_MDD_MAX_ENTRIES*sizeof(kgni_mdd_info_t), GFP_KERNEL);
		if (kgni_dev->mdd_registry[i] == NULL) {
			for (j = 0; j < i; j++) {
				kfree(kgni_dev->mdd_registry[j]);
			}
			GPRINTK_RL(0, KERN_ERR, "Failed to allocate mdd_registry");
			err = (-ENOMEM);
			goto err_alloc_mdd_reg;
		} else {
			for (j = 0; j < GNI_MDD_MAX_ENTRIES; j++) {
				sema_init(&kgni_dev->mdd_registry[i][j].mdd_lock, 1); // 1, for unlocked
			}
		}
	}
	kgni_dev->mrt_debug_info = NULL;

	kgni_dev->devnum = ghal_get_devnum(pdev);
	devno = MKDEV(kgni_major, kgni_dev->devnum);
	kgni_dev->pdev = pdev;

	cdev_init(&kgni_dev->cdev, &kgni_fops);
	kgni_dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&kgni_dev->cdev, devno, 1);
	if (err) {
		goto err_cdev_add;
	}

	kgni_dev->class_dev = device_create(gemini_class, NULL,
					    kgni_dev->cdev.dev,
					    &kgni_dev->pdev->dev,
					    GHAL_CLDEV_KGNI_FMT,
					    kgni_dev->devnum);
	if (IS_ERR(kgni_dev->class_dev)) {
		err = PTR_ERR(kgni_dev->class_dev);
		GPRINTK_RL(0, KERN_ERR, "class_device_create() returned error %d", err);
		goto err_class_dev;
	}

	dev_set_drvdata(kgni_dev->class_dev, kgni_dev);

	ghal_set_subsys_data(pdev, GHAL_SUBSYS_GNI, kgni_dev);

	atomic_set(&kgni_dev->cur_kern_cqirq, 0);
	atomic_set(&kgni_dev->cur_user_cqirq, 0);

	INIT_WORK(&kgni_dev->job_info_workq,kgni_job_info);
        INIT_DELAYED_WORK(&kgni_dev->qsce_workq, kgni_qsce_timer);

	INIT_LIST_HEAD(&kgni_dev->qsce_list);
	INIT_LIST_HEAD(&kgni_dev->job_info_list);
	INIT_LIST_HEAD(&kgni_dev->active_fmas);
	INIT_LIST_HEAD(&kgni_dev->active_ntts);
	sema_init(&kgni_dev->fma_lock, 1); // 1, for unlocked
	sema_init(&kgni_dev->vmdh_lock, 1); // 1, for unlocked
	rwlock_init(&kgni_dev->chnl_lock);
	rwlock_init(&kgni_dev->rsrc_lock);
	rwlock_init(&kgni_dev->irq_hndlrs_lock);
	spin_lock_init(&kgni_dev->ntt_list_lock);
	spin_lock_init(&kgni_dev->job_info_lock);
	spin_lock_init(&kgni_dev->qsce_lock);
	for ( i = 0; i < GHAL_IRQ_CQ_MAX_INDEX; i++) {
		INIT_LIST_HEAD(&kgni_dev->irq_hndlrs[i]);
	}

	kgni_dev->percpu_errno = alloc_percpu(gni_errno_t);
	if (ZERO_OR_NULL_PTR(kgni_dev->percpu_errno)) {
		GPRINTK_RL(0, KERN_ERR, "alloc_percpu failed to allocate percpu_errno");
		goto err_percpu_errno_alloc;
	}

	kgni_dev->rw_page = (uint64_t *)__get_free_page(GFP_KERNEL|__GFP_ZERO);
	if (kgni_dev->rw_page == NULL) {
		GPRINTK_RL(0, KERN_ERR, "__get_free_page failed");
		goto err_rw_page;
	}

	/* Get 2 pages to hold the maximum number of allowed completion queues. */
	kgni_dev->cq_interrupt_mask_page = (uint64_t *)__get_free_pages(GFP_KERNEL|__GFP_ZERO, KGNI_CQ_INTERRUPT_PAGE_ORDER);
	if (kgni_dev->cq_interrupt_mask_page == NULL) {
		GPRINTK_RL(0, KERN_ERR, "__get_free_page cq_interrupt_mask_page failed");
		goto err_cq_interrupt_mask_page;
	}

        kgni_dev->irq_status_reg = ghal_get_irq_status_reg(pdev);
	err = kgni_cq_irq_init(kgni_dev);
	if (err) {
		goto err_cq_irq_init;
	}

	/* allocate and configure BTE channels for user and kernel domains */
	err = kgni_bte_configure(kgni_dev, bte_chnls);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "kgni_bte_configure returned error %d", err);
		goto err_bte_cfg;
	}

	err = kgni_error_init(kgni_dev);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "kgni_error_init() returned error %d",
		           err);
		goto err_error_init;
	}

	kgni_dla_init_params();

	kgni_numa_desc_init(kgni_dev);

	err = kgni_create_cldev_files(kgni_dev);
	if (err) {
		goto err_resources_file;
	}

	/* Enable channels */
	for (i = 0; i < GHAL_BTE_CHNL_MAX_NUMBER; i ++) {
		if (kgni_dev->bte_ctrl[i]) {
			GHAL_VC_TXC_ENABLE(kgni_dev->bte_ctrl[i]->bte_vc_desc, 1);
			GHAL_VC_RXC_ENABLE(kgni_dev->bte_ctrl[i]->bte_vc_desc, 1);
			GHAL_VC_WR_RXC_BUFFSIZE(kgni_dev->bte_ctrl[i]->bte_vc_desc, kgni_dev->bte_ctrl[i]->rx_buff_size);

			/* Kick off the BTE maintenance workq.  This is used to
			 * work around issues where the TX and RX IRQ handlers
			 * cannot be relied on in all situations. */
			schedule_delayed_work(&kgni_dev->bte_ctrl[i]->tx_workq,
					      kgni_bte_workq_freq_ms);
		}
	}

	schedule_delayed_work(&kgni_dev->qsce_workq, msecs_to_jiffies(qsce_delay));

	GDEBUG(1,"Done");

	return 0;

	err_resources_file:
		kgni_error_fini(kgni_dev);
	err_error_init:
		kgni_bte_release(kgni_dev);
	err_bte_cfg:
		kgni_cq_irq_fini(kgni_dev);
	err_cq_irq_init:
		free_pages((unsigned long)kgni_dev->cq_interrupt_mask_page, 1);
	err_cq_interrupt_mask_page:
		free_page((unsigned long)kgni_dev->rw_page);
	err_rw_page:
		free_percpu(kgni_dev->percpu_errno);
	err_percpu_errno_alloc:
		ghal_set_subsys_data(pdev, GHAL_SUBSYS_GNI, NULL);
		device_destroy(gemini_class, kgni_dev->cdev.dev);
	err_class_dev:
		cdev_del(&kgni_dev->cdev);
	err_cdev_add:
		for (i = 0; i < GNI_MDD_DIR_MAX_NUMBER; i++) {
			kfree(kgni_dev->mdd_registry[i]);
		}
	err_alloc_mdd_reg:
		kfree(kgni_dev);
	err_alloc_kgni:
	return err;
}

/**
 * kgni_remove - device removal routine
 *
 * Called by ghal_remove().
 **/
static void kgni_remove(struct pci_dev *pdev)
{
	kgni_device_t *kgni_dev = ghal_get_subsys_data(pdev, GHAL_SUBSYS_GNI);
	int i;

	if (kgni_dev) {
		kgni_ntt_cleanup(kgni_dev, -1, 0);

		for (i = 0; i < GHAL_BTE_CHNL_MAX_NUMBER; i ++) {
			if (kgni_dev->bte_ctrl[i]) {
				GHAL_VC_TXC_DISABLE(kgni_dev->bte_ctrl[i]->bte_vc_desc);
				GHAL_VC_RXC_DISABLE(kgni_dev->bte_ctrl[i]->bte_vc_desc);
			}
		}

		cancel_delayed_work(&kgni_dev->qsce_workq);
		flush_scheduled_work();

		kgni_numa_desc_fini(kgni_dev);
		kgni_remove_cldev_files(kgni_dev);
		kgni_error_fini(kgni_dev);
		kgni_cq_irq_fini(kgni_dev);
		kgni_bte_release(kgni_dev);
		device_destroy(gemini_class, kgni_dev->cdev.dev);
		cdev_del(&kgni_dev->cdev);
		for (i = 0; i < GNI_MDD_DIR_MAX_NUMBER; i++) {
			kfree(kgni_dev->mdd_registry[i]);
		}
		free_page((unsigned long)kgni_dev->rw_page);
		free_pages((unsigned long)kgni_dev->cq_interrupt_mask_page, 1);
		free_percpu(kgni_dev->percpu_errno);
		kfree(kgni_dev);
	}
	ghal_set_subsys_data(pdev, GHAL_SUBSYS_GNI, NULL);
}

/**
 * kgni_unprotected_ops_init - Initialize unprotected ops
 *
 * If we are running on a simulator, unprotected ops will be turned on
 * by default.  Otherwise, it will be turned off by default.
 **/
static void kgni_unprotected_ops_init(void)
{
	/* Check for simulator */
#if (defined(CONFIG_CRAY_SOFTSDV) || defined(CONFIG_CRAY_SOFTSDV_MODULE)) \
	&& defined(CONFIG_CRAY_SIMNOW)
	kgni_unprotected_ops = softsdv_boot || simnow_boot;
#elif defined(CONFIG_CRAY_SOFTSDV) || defined(CONFIG_CRAY_SOFTSDV_MODULE)
	kgni_unprotected_ops = softsdv_boot;
#elif defined(CONFIG_CRAY_SIMNOW)
	kgni_unprotected_ops = simnow_boot;
#else
	kgni_unprotected_ops = 0;
#endif
}


gni_return_t
gni_get_errno(gni_nic_handle_t nic_hndl, gni_errno_t *errno_ptr)
{
	kgni_device_t	*kdev = (kgni_device_t *)nic_hndl->kdev_hndl;
	gni_errno_t	*cpu_errno;
	gni_return_t	status = GNI_RC_INVALID_PARAM;

	if (!nic_hndl) {
		goto exit;
	}

	cpu_errno = per_cpu_ptr(kdev->percpu_errno, smp_processor_id());
	if (cpu_errno->valid) {
		/* clearing the valid flag if called with NULL errno_ptr is a feature */
		cpu_errno->valid = 0;

		if (errno_ptr) {
			*errno_ptr = *cpu_errno;
			status = GNI_RC_SUCCESS;
		}
	} else {
		status = GNI_RC_INVALID_STATE;
	}

 exit:
	return status;
}
EXPORT_SYMBOL(gni_get_errno);

/**
 * kgni_init - driver registration routine
 *
 * Returns 0 if registered successfully.
 **/
static int __init kgni_init(void)
{
	int err;

	GPRINTK_RL(0, KERN_INFO, "%s - %s version 0x%08x rev %d",
	           kgni_driver_string, __DATE__,
	           kgni_driver_version, kgni_driver_buildrev);
	GPRINTK_RL(0, KERN_INFO, "%s", kgni_driver_copyright);

	err = craytrace_create_buf("kgnidrv", 0, &kgni_craytrace_idx);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "Failed to create private craytrace buffer for kGNI (%d)", err);
		goto err_craytrace;
	}

	/* Register char driver */
	if (!kgni_major) {
		err = alloc_chrdev_region(&kgni_dev_id, 0, kgni_driver_numdev, kgni_driver_name);
		kgni_major = MAJOR(kgni_dev_id);
	} else {
		kgni_dev_id = MKDEV(kgni_major, 0);
		err = register_chrdev_region(kgni_dev_id, kgni_driver_numdev, kgni_driver_name);
	}
	if (err) {
		goto err_alloc_chrdev;
	}

	err = class_create_file(gemini_class, &class_attr_version);
	if (err) {
		GPRINTK_RL(0, KERN_ERR, "couldn't create version attribute");
		goto err_version_file;
	}

	/* Create lookaside cache for SM info */
	kgni_sm_mem_cache = kmem_cache_create("kgni_sm_info",
					      sizeof(kgni_sm_info_t), 0,
					      SLAB_HWCACHE_ALIGN,
					      NULL);
	if (kgni_sm_mem_cache == NULL) {
		err = -ENOMEM;
		goto err_kmem_cache;
	}
	/* Create lookaside cache pages used during mem. registration */
	kgni_mem_page_cache = kmem_cache_create("kgni_pages_info",
						KGNI_PAGEINFO_SIZE, 0,
						SLAB_HWCACHE_ALIGN,
						NULL);
	if (kgni_mem_page_cache == NULL) {
		err = -ENOMEM;
		goto err_page_cache;
	}

	/* Create lookaside cache pages used to queue overflow RDMA requests */
	kgni_rdma_qelm_cache = kmem_cache_create("kgni_rdma_qelement",
						sizeof(kgni_pkt_queue_t), 0,
						SLAB_HWCACHE_ALIGN,
						NULL);
	if (kgni_rdma_qelm_cache == NULL) {
		err = -ENOMEM;
		goto err_rdma_cache;
	}

	/* Create EEQ cache pages used to report errors to uGNI apps */
	kgni_eeq_cache = kmem_cache_create("kgni_error_eeqs",
					   sizeof(gni_eeq_t), 0,
					   SLAB_HWCACHE_ALIGN,
					   NULL);
	if(kgni_eeq_cache == NULL) {
		err = -ENOMEM;
		goto err_eeq_cache;
	}

	/* Create gni_umap_desc_t cache */
	kgni_umap_desc_cache = kmem_cache_create("kgni_umap_desc",
					   sizeof(gni_umap_desc_t), 0,
					   SLAB_HWCACHE_ALIGN,
					   NULL);
	if(kgni_umap_desc_cache == NULL) {
		err = -ENOMEM;
		goto err_umap_desc_cache;
	}

	/* Create workqueue for kgni_sm_task */
	kgni_sm_wq = create_workqueue("kgni_sm_wq");
	if (kgni_sm_wq == NULL) {
		err = -ENOMEM;
		goto err_kgni_sm_wq;
	}

	/* register with GHAL */
	ghal_add_subsystem(GHAL_SUBSYS_GNI, kgni_probe, kgni_remove);

	/* setup unprotected ops default value */
	kgni_unprotected_ops_init();

	/* Try and load exmem.ko symbols. There is no return value,
	 * as the kernel module is not always loaded. */
	kgni_exmem_init();

	return 0;
	err_kgni_sm_wq:
		kmem_cache_destroy(kgni_umap_desc_cache);
	err_umap_desc_cache:
		kmem_cache_destroy(kgni_eeq_cache);
	err_eeq_cache:
		kmem_cache_destroy(kgni_rdma_qelm_cache);
	err_rdma_cache:
		kmem_cache_destroy(kgni_mem_page_cache);
	err_page_cache:
		kmem_cache_destroy(kgni_sm_mem_cache);
	err_kmem_cache:
		class_remove_file(gemini_class, &class_attr_version);
	err_version_file:
		unregister_chrdev_region(kgni_dev_id, kgni_driver_numdev);
	err_alloc_chrdev:
		craytrace_destroy_buf(kgni_craytrace_idx);
	err_craytrace:
	return err;

}

/**
 * kgni_exit - driver cleanup routine
 *
 * Called before the driver is removed from the memory
 **/
static void __exit kgni_exit(void)
{
	kgni_exmem_fini();
	ghal_remove_subsystem(GHAL_SUBSYS_GNI);
	class_remove_file(gemini_class, &class_attr_version);
	unregister_chrdev_region(kgni_dev_id, kgni_driver_numdev);
	if (kgni_sm_wq) {
		destroy_workqueue(kgni_sm_wq);
	}
	if (kgni_umap_desc_cache) {
		kmem_cache_destroy(kgni_umap_desc_cache);
	}
	if (kgni_eeq_cache) {
		kmem_cache_destroy(kgni_eeq_cache);
	}
	if (kgni_rdma_qelm_cache) {
		kmem_cache_destroy(kgni_rdma_qelm_cache);
	}
	if (kgni_sm_mem_cache) {
		kmem_cache_destroy(kgni_sm_mem_cache);
	}
	if (kgni_mem_page_cache) {
		kmem_cache_destroy(kgni_mem_page_cache);
	}
	craytrace_destroy_buf(kgni_craytrace_idx);
}

module_init(kgni_init);
module_exit(kgni_exit);
