/* -*- c-basic-offset: 8; indent-tabs-mode: t -*- */
/**
 *	Device operation for GNI driver.
 *
 *	Copyright (C) 2007 Cray Inc. All Rights Reserved.
 *	Written by Igor Gorodetsky <igorodet@cray.com>
 *      Some code is borrowed from uGNI.
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
 **/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/mman.h>
#include <asm/page.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/job_acct.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "gni_priv.h"
#include "ghal.h"
#include "ghal_pub.h"
#include "kgni.h"
#include "gni_post_fma.h"

static int multi_bte = 1;
module_param(multi_bte, int, S_IRUGO | S_IWUSR);

static gni_cdm_handle_t	kgni_cdm_array[KGNI_MAX_CDM_NUMBER] = {NULL};
static DEFINE_SPINLOCK(kgni_cdm_lock);
static LIST_HEAD(kgni_sm_list);
static DEFINE_SPINLOCK(kgni_sm_lock);
static DECLARE_WORK(kgni_sm_work, kgni_sm_task);
static uint64_t	   	job_generation = 0;
static uint64_t	   	sm_generation = 1;

/* Internal kGNI interfaces */

int kgni_cdm_barr(gni_nic_handle_t nic_hndl, uint32_t size, uint32_t timeout)
{
	kgni_device_t		*kgni_dev = (kgni_device_t *)nic_hndl->kdev_hndl;
	unsigned long		flags;
	kgni_resource_info_t	*res;
	int			rc;
	kgni_barr_res_t		*barr;
	unsigned long 		timeout_jiffies = MAX_SCHEDULE_TIMEOUT;
	int			init_barr = 0;
	int			freed_barr = 0;

	if (size < 1) {
		return (-EINVAL);
	} else if (size == 1) {
		return 0;
	}

	write_lock_irqsave(&kgni_dev->rsrc_lock, flags);
	if (kgni_dev->resources[nic_hndl->ptag] == NULL) {
		write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
		GPRINTK_RL(1, KERN_ERR, "(entry) kgni_dev->resources[ptag] == NULL (ptag = %d)",
		           nic_hndl->ptag);
		GASSERT(0);
	}

	/* allocate the barrier */
	res = kgni_dev->resources[nic_hndl->ptag];
	if (res->barr == NULL) {

		write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
		barr = (kgni_barr_res_t *)kzalloc(sizeof(kgni_barr_res_t), GFP_KERNEL);
		if (barr == NULL) {
			GPRINTK_RL(1, KERN_INFO, "Failed to allocate barrier resources");
			return (-ENOMEM);
		}
		write_lock_irqsave(&kgni_dev->rsrc_lock, flags);

		if(res->barr == NULL) {
			sema_init(&barr->sem, 0);
			atomic_set(&barr->size, size);
			atomic_set(&barr->count, 1);
			res->barr = barr;
			init_barr = 1;
		} else {
			kfree(barr);
		}
	}

	if(!init_barr) {
		/* I joined after a barrier completed, before it was destroyed */
		if (atomic_read(&res->barr->count) == atomic_read(&res->barr->size)) {
			/* I can set up the barrier for me now, participants in
			 * the last barrier won't use them anymore; before I
			 * release the rsrc_lock, I'll inc. the ref_cnt so
			 * participants in the previous barrier wont destroy
			 * anything */
			atomic_set(&res->barr->size, size);
			atomic_set(&res->barr->count, 1);
			init_barr = 1;
		} else {
			/* return 'invalid' if an instance attempts to join with
			 * mismatching barrier size */
			if (atomic_read(&res->barr->size) != size) {
				write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
				GPRINTK_RL(1, KERN_INFO, "bad barrier size");
				return (-EINVAL);
			}

			/* join the barrier */
			atomic_inc(&res->barr->count);

			/* if I was the last to join, wake up the others and exit */
			if (atomic_read(&res->barr->count) == atomic_read(&res->barr->size)) {
				int i;
				for (i = 0; i < atomic_read(&res->barr->size) - 1; i++) {
					up(&res->barr->sem);
				}

				write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
				return 0;
			}
		}
	}

	++res->barr->ref_cnt;
	write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);

	if(init_barr) {
		GDEBUG(3, "inst %u initialized barrier: ptag: %u, count: %u, size: %u",
				nic_hndl->inst_id, nic_hndl->ptag,
				atomic_read(&res->barr->count),
				atomic_read(&res->barr->size));
	} else {
		GDEBUG(3, "inst %u joined barrier: ptag: %u, count: %u, size: %u",
				nic_hndl->inst_id, nic_hndl->ptag,
				atomic_read(&res->barr->count),
				atomic_read(&res->barr->size));
	}

	/* wait for the last instance to join */
	if (timeout != (uint32_t)(-1)) {
		timeout_jiffies = msecs_to_jiffies(timeout);
		rc = down_timeout(&res->barr->sem, timeout_jiffies);
	} else {
		rc = down_interruptible(&res->barr->sem);
	}

	/* rc is 0, ETIME, EINTR */
	if (rc) {
		rc = -EAGAIN;
	}

	/* last instance woken up destroys the barrier */
	write_lock_irqsave(&kgni_dev->rsrc_lock, flags);
	if (kgni_dev->resources[nic_hndl->ptag] == NULL) {
		write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
		GPRINTK_RL(1, KERN_ERR, "(exit) kgni_dev->resources[ptag] == NULL (ptag = %d)",
		           nic_hndl->ptag);
		GASSERT(0);
	}

	if (--res->barr->ref_cnt == 0) {
		kfree(res->barr);
		res->barr = NULL;
		freed_barr = 1;
	}
	write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);

	if(freed_barr) {
		GDEBUG(3, "inst %u freed barrier: ptag %u",
				nic_hndl->inst_id, nic_hndl->ptag);
	}

	return rc;
}

/**
 * kgni_page_uc - set page uncached flags
 **/
static pgprot_t kgni_page_uc(pgprot_t _prot)
{
    unsigned long prot = pgprot_val(_prot);
    /* !!! This could changed if MTRR is not configuring Write-Combining space and
    we use PAT to do that */
    prot |= _PAGE_PCD;

    return __pgprot(prot);
}

/**
 *  kgni_notify_user - notify user-space application of one or more datagrams
 *                     that are ready to be pulled out of the linked list of
 *                     datagrams by a call to GNI_EpPostDataTest or GNI_EpPostDataWait.
 *                     The notification allows for GNI_EpPostDataTest to avoid a system
 *                     call when there are no datagrams ready to be received.
 *
 *                     A datagram is ready to be received when it is in one of these states -
 *                     KGNI_SM_STATE_COMPLETED
 *                     KGNI_SM_STATE_TERMINATED
 *                     KGNI_SM_STATE_TIMEOUT
 *
 *                     Kernel space applications don't use this optimization. For
 *                     kernel applications, datagram_cnt_kern in the chnl_hndl struct is NULL.
 **/

static inline void kgni_notify_user(kgni_chnl_handle_t chnl_hndl, kgni_sm_info_t *sm_info)
{
        if (chnl_hndl->datagram_cnt_kern != NULL && 
                       !(sm_info->flags & KGNI_SM_FLAG_USR_NOTIFIED)) {
                atomic_inc(chnl_hndl->datagram_cnt_kern);
                sm_info->flags |= KGNI_SM_FLAG_USR_NOTIFIED;
//              GDEBUG(5,"incremented datagram_cnt_kern %p",chnl_hndl->datagram_cnt_kern);
        }
}

/**
 * kgni_sm_probe_state - probe a cdm for first datagram of the linked list of datagrams/sessions
 *                  in the KGNI_SM_STATE_COMPLETED,
 *                  KGNI_SM_STATE_TERMINATED, or KGNI_SM_STATE_TIMEOUT state.
 *                  A subsequent call to kgni_sm_get_state is required to retrieve
 *                  the application data.
 **/
int kgni_sm_probe_state(gni_nic_handle_t nic_hndl, gni_ep_postdata_test_args_t *ep_posttest_attr)
{
        kgni_sm_info_t          *sm_info;
        gni_cdm_handle_t        cdm_handle;
        struct list_head        *ptr, *next;
        int                     retval = -ESRCH;
        uint32_t                remote_pe;
        uint32_t                match_by_post_id = 0;

        /*
         * do a safety check again
         */
	if (nic_hndl->cdm_id == GNI_CDM_INVALID_ID) {
		GPRINTK_RL(1, KERN_INFO, "NIC is not attached");
		return (-EINVAL);
	}

        cdm_handle = kgni_cdm_array[nic_hndl->cdm_id];
        if (cdm_handle == NULL) {
                GPRINTK_RL(1, KERN_INFO, "CDM is not found");
                return (-EINVAL);
        }

        if (ep_posttest_attr->modes & GNI_EP_PDT_MODE_USE_POST_ID) {
                match_by_post_id = 1;
        }

        GDEBUG_DGRAM(5, NULL, NULL,
                     "Probing for session: local[pe=0x%x inst=%u] match_by_post_id = %d",
                     nic_hndl->nic_pe, nic_hndl->inst_id, match_by_post_id);

        spin_lock_bh(&kgni_sm_lock);
        list_for_each_safe(ptr, next, &cdm_handle->sessions) {
                sm_info = list_entry(ptr, kgni_sm_info_t, cdm_list);
                if (list_empty(&sm_info->smtask_list) &&
                    (sm_info->pkt.state == KGNI_SM_STATE_COMPLETED ||
                     sm_info->pkt.state == KGNI_SM_STATE_TERMINATED ||
                     sm_info->pkt.state == KGNI_SM_STATE_TIMEOUT)) {

                        if (!match_by_post_id) {
                                GDEBUG_DGRAM(5, sm_info, NULL,
                                             "Returning remote_pe %u and remote_id %u",
                                             sm_info->pkt.remote_id.pe,
                                             sm_info->pkt.remote_id.inst_id);
                                ep_posttest_attr->remote_id = sm_info->pkt.remote_id.inst_id;

                                /* see if we need to translate phys. PE to virtual PE */
                                if (nic_hndl->ntt_grp_size) {

					ep_posttest_attr->remote_pe = -1;
					remote_pe = kgni_ntt_physical_to_logical(nic_hndl, sm_info->pkt.remote_id.pe);
					if (remote_pe < 0) {
						GPRINTK_RL(1, KERN_INFO, "Failed to lookup PE 0x%x (ntt_base=%d ntt_size=%d)",
						           sm_info->pkt.remote_id.pe, nic_hndl->ntt_base, nic_hndl->ntt_grp_size);
						spin_unlock_bh(&kgni_sm_lock);
						return -EFAULT;
					}
					ep_posttest_attr->remote_pe = remote_pe;
					if (ep_posttest_attr->remote_pe == -1) {
						GPRINTK_RL(1, KERN_INFO, "Failed to find virtual PE for phys PE 0x%x",
						           sm_info->pkt.remote_id.pe);

						retval = -ENXIO;
						break;
					}

                                } else {
                                        /* ntt is not configured, return phys. PE address */
                                        ep_posttest_attr->remote_pe = sm_info->pkt.remote_id.pe;
                                }

                                retval = 0;
                                break;
                        } else if (/* match_by_post_id && */ sm_info->post_id != -1) {
                                ep_posttest_attr->post_id = sm_info->post_id;
                                GDEBUG(5,"returning post_id: 0x%llx", ep_posttest_attr->post_id);

                                retval = 0;
                                break;
                        }
                }
        }

        spin_unlock_bh(&kgni_sm_lock);

        return retval;
}

int kgni_umap_list_copy(gni_nic_handle_t nic_hndl, gni_list_t *dest_list, int *count)
{
	int umap_count = 0;
	gni_list_t *src_list = &nic_hndl->umap_list;
	gni_umap_desc_t *desc, *copied_desc;

	gni_list_iterate(desc, src_list, list) {
		copied_desc = kmem_cache_alloc(kgni_umap_desc_cache, GFP_KERNEL);
		if (copied_desc == NULL) {
			return -ENOMEM;
		}
		copied_desc->address = desc->address;
		copied_desc->size = desc->size;
		gni_list_add_tail(&copied_desc->list, dest_list);
		umap_count++;
	}
	*count = umap_count;
	return 0;
}

void kgni_unmap_area_locked(struct mm_struct    *mm,
		            gni_umap_desc_t     *umap_desc) 
{
	/* check mm_users here as process may have already exited */
	if (atomic_read(&mm->mm_users) > 0) {
		do_munmap(mm, (unsigned long)umap_desc->address, umap_desc->size);
	}
	GDEBUG(3, "freed mm %p umap %p, users %d",
	       mm, umap_desc->address, atomic_read(&mm->mm_users));
	gni_list_del(&umap_desc->list);
	kmem_cache_free(kgni_umap_desc_cache, umap_desc);
}

void kgni_unmap_area(struct mm_struct    *mm,
	             gni_umap_desc_t     *umap_desc) 
{
	down_write(&mm->mmap_sem);
	kgni_unmap_area_locked(mm, umap_desc);
	up_write(&mm->mmap_sem);
}

/**
 * kgni_umap_area - map address segment to user space
 **/
int kgni_umap_area(gni_nic_handle_t	nic_hndl,
		   uint64_t		address,
		   uint64_t		size,
		   int			wc_flag,
		   unsigned long	vm_flags,
		   gni_umap_desc_t	**out_umap_desc,
		   int			is_ram)
{
	struct vm_area_struct *vma;
	int 		err = 0;
	unsigned long	virt_window;
	pgprot_t	pg_prot_flags;
	unsigned long 	mmap_flags = (MAP_ANONYMOUS | MAP_PRIVATE);
	gni_umap_desc_t *umap_desc;

	/* !!! We do not use wc_flag right now as it gets Write-Combining
	 properties according to MTRR configuratin.
	When Linux gets PAT working, we will use it to configure WC according to wc_flag */
	down_write(&current->mm->mmap_sem);
	/*
	 * To avoid overcommit accounting, map the region PRIVATE READ-only.
	 * Later, we'll change the VMA to SHARED READ-WRITE.
	 * (See the table in Documentation/vm/overcommit-accounting.)
	 *
	 * Why do we want to avoid overcommit accounting?
	 * (1) FMA windows are backed by the Gemini device and not by
	 * physical pages so it doesn't make sense to charge them against the
	 * address space commit.
	 * (2) If FMA windows were to be charged against the commit, we'd
	 * quickly hit the limit (XT nodes have no swap).  The alternative,
	 * requiring sites to set the overcommit policy to "always overcommit",
	 * is undesirable -- see BUG 745609.
	 * (3) For DLA FIFO fill status, it's backed by kernel memory, and
	 * also doesn't need to get charged against user.  We leave the region
	 * read-only.
	 */
	virt_window= do_mmap_pgoff(NULL, 0, size, PROT_READ, mmap_flags, 0);
	if ( IS_ERR((void *) virt_window) ) {
		err = PTR_ERR((void *)virt_window);
		goto error_mmap_fma;
	}

	vma = find_vma(current->mm, virt_window);
	if ( !vma ||
	     (virt_window != vma->vm_start) ||
	     ((vma->vm_end - vma->vm_start) < size)
	   ) {
		if (vma) {
			GPRINTK_RL(1, KERN_ERR, "Failed to set VM_IO flag: vaddr=0x%lx vm_start=0x%lx vm_end=0x%lx size=%lld",
				    virt_window, vma->vm_start, vma->vm_end, size);
		} else {
			GPRINTK_RL(1, KERN_ERR, "Failed to set VM_IO flag: vaddr=0x%lx vma=NULL size=%lld",
				    virt_window, size);
		}
		err = -EINVAL;
		goto error_remap_fma;
	}
	/*
	 * Change the flags as described above in the discussion of overcommit
	 * accounting.  Note that this works with kernel 2.6.16, but it is
	 * not guaranteed to work with future versions of the kernel.
	 * Linux really ought to export a method to map a device region in
	 * one fell swoop, instead of this do_mmap_pgoff+remap_pfn_range
	 * paradigm.
	 */
	if (vm_flags & VM_WRITE) {
		vma->vm_flags |= VM_MAYSHARE|VM_SHARED|VM_WRITE;
		pg_prot_flags = PAGE_SHARED;
	} else {
		vma->vm_flags |= VM_MAYSHARE|VM_SHARED;
		pg_prot_flags = PAGE_READONLY;
	}
	if (!is_ram) {
		pg_prot_flags = kgni_page_uc(pg_prot_flags);
	}

	/* map virtual address into FMA IO area */
	err = remap_pfn_range(vma,
			      virt_window,
			      (address >> PAGE_SHIFT),
			      size,
			      pg_prot_flags);
	if (err) {
		goto error_remap_fma;
	}

	/* record mmap info in appropriate umap list */
	if (nic_hndl->mm == NULL) {
		err = -EEXIST;
		goto error_umap_alloc;
	}
	umap_desc = kmem_cache_alloc(kgni_umap_desc_cache, GFP_KERNEL);
	if (umap_desc == NULL) {
		err = -ENOMEM;
		goto error_umap_alloc;
	}
	umap_desc->address = (volatile void *)virt_window;
	umap_desc->size = size;

	if (current->mm == nic_hndl->mm) {
		gni_list_add_tail(&umap_desc->list, &nic_hndl->umap_list);
	} else {
		/* if we allow it, this supports child to create own CQ */  
		int found = 0;
		gni_child_desc_t *child_desc;

		gni_list_iterate(child_desc, &nic_hndl->child_list, list) {
			if (current->mm == child_desc->mm) {
				gni_list_add_tail(&umap_desc->list, &child_desc->umap_list);
				found = 1;
				break;
			}
		}
		if (found == 0) {
			err = -EEXIST;
			goto error_umap_add;
		}
	}

	up_write(&current->mm->mmap_sem);

	*out_umap_desc = umap_desc;

	return 0;

	error_umap_add:
	kmem_cache_free(kgni_umap_desc_cache, umap_desc);
	error_umap_alloc:
	error_remap_fma:
	do_munmap(current->mm, virt_window, size);
	error_mmap_fma:
	up_write(&current->mm->mmap_sem);
	return err;
}

/**
 *  kgni_sm_rx_set_delay - Compute the delay for resending a sm datagram to a target
 *                         if the RX ring at the target is empty.
 *
 **/
static void kgni_sm_rx_set_delay(kgni_sm_info_t *sm_info)
{
	unsigned long jiffy;
	uint32_t ms = 0;
	uint32_t tmp;

	if (sm_info->nrx_cnt > 2) {
		tmp = sm_info->nrx_cnt - 1;
		ms = tmp * tmp;
		ms = (ms > 1000) ? 1000 : ms;
		jiffy = msecs_to_jiffies(ms);
		sm_info->rx_delay = jiffies + jiffy;
	} else {
		sm_info->rx_delay = 0;
	}
}

/**
 *  kgni_sm_rx_delay - Check if the retransmit of the sm datagram
 *                     should be delayed.
 *
 *                     Returns 1 if the retransmit should be delayed, otherwise 0.
 **/
static int kgni_sm_rx_delay(kgni_sm_info_t *sm_info)
{
	int rc = 0;

	if (sm_info->rx_delay != 0) {
		rc = time_before(jiffies, sm_info->rx_delay);
	}
	return rc;
}

/**
 * kgni_sm_schedule_pending_work - run session management task
 **/
void kgni_sm_schedule_pending_work(void)
{
	if (!list_empty(&kgni_sm_list)) {
		/* schedule task */
		queue_work(kgni_sm_wq, &kgni_sm_work);
	}
}

/**
 * kgni_sm_task - session management task
 **/
void kgni_sm_task(struct work_struct *workq)
{
	kgni_chnl_handle_t	chnl_hndl;
	struct list_head	*ptr, *next;
	kgni_sm_info_t		*sm_info;
	uint16_t			sm_flags;
	uint16_t		sm_flrcnt;
	int			err = 0;
	int			delayed_retransmit = 0;
	int			i;
	unsigned long		start = jiffies;
	int			pkts_out;

	spin_lock_bh(&kgni_sm_lock);
	list_for_each_safe(ptr, next, &kgni_sm_list) {
		if (jiffies - start > 1) {
			err = 1;
			break;
		}

		sm_info = list_entry(ptr, kgni_sm_info_t, smtask_list);
		chnl_hndl = sm_info->nic_hndl->sm_chnl_hndl;
		pkts_out = atomic_read(&sm_info->pkts_out);
		if ((chnl_hndl != NULL) &&
		    (pkts_out == 0) &&
		    (sm_info->flags & (KGNI_SM_FLAG_STATE_CHANGED | KGNI_SM_FLAG_SEND_FAILED)) &&
		    (sm_info->pkt.state != KGNI_SM_STATE_TERMINATED)) {
			sm_flrcnt = sm_info->flrcnt;
			/* this entry has changed or failed to deliver */
			if (sm_info->flags & KGNI_SM_FLAG_SEND_FAILED) {
				if (sm_info->flrcnt > KGNI_SM_MAX_RETRANSMITS) {
					sm_info->pkt.state = KGNI_SM_STATE_TIMEOUT;
					list_del_init(&sm_info->smtask_list);
                                        kgni_notify_user(chnl_hndl,sm_info);
					wake_up_interruptible(&sm_info->nic_hndl->sm_wait_queue);
					GDEBUG_DGRAM(1, sm_info, NULL, "Session timed out");
					continue;
				}
				if (sm_info->snd_status != GNI_RC_ERROR_RESOURCE) {
					/* do not count errors due to shortage of RX descriptors */
					sm_info->flrcnt++;
				} else {
					/* up the no-rx descriptors at target count,
					 * set possible delay for retransmit */
					sm_info->nrx_cnt++;
					kgni_sm_rx_set_delay(sm_info);
				}
			}

			if (kgni_sm_rx_delay(sm_info)) {
				delayed_retransmit = 1;
				continue;
			}

			sm_info->pkt.seq_num = sm_info->snd_seq_num;
			/* get next available pkt_id */
			GDEBUG_DGRAM(3, sm_info, NULL, "Sending packet (pkt_id=%d)",
				     sm_info->nic_hndl->sm_pkt_id);

			sm_info->nic_hndl->sm_pkts[sm_info->nic_hndl->sm_pkt_id] = sm_info;
			sm_flags = sm_info->flags;
			sm_info->flags &= ~(KGNI_SM_FLAG_STATE_CHANGED | KGNI_SM_FLAG_SEND_FAILED);
			sm_info->snd_status = GNI_RC_NOT_DONE;
			atomic_inc(&sm_info->pkts_out);
			/* send it and clear flags if successful */
			err = kgni_chnl_send((kgni_chnl_handle_t)sm_info->nic_hndl->sm_chnl_hndl,
					   sm_info->pkt.remote_id.pe,
					   sm_info->pkt.remote_id.inst_id,
					   sm_info->nic_hndl->sm_pkt_id,
					   &sm_info->pkt, sizeof(kgni_sm_pkt_t));
			if (err) {
				GASSERT(err == (-EAGAIN));
				sm_info->nic_hndl->sm_pkts[sm_info->nic_hndl->sm_pkt_id] = NULL;
				sm_info->flags = sm_flags;
				sm_info->flrcnt = sm_flrcnt;
				atomic_dec(&sm_info->pkts_out);
				break;
			}

			for (i = sm_info->nic_hndl->sm_pkt_id + 1; i != sm_info->nic_hndl->sm_pkt_id; i++) {
				if (i > chnl_hndl->max_tx_pkts) i = 0;
				if (sm_info->nic_hndl->sm_pkts[i] == NULL) break;
			}
			/* we should always have space for the next packet */
			GASSERT(i != sm_info->nic_hndl->sm_pkt_id);
			sm_info->nic_hndl->sm_pkt_id = i;
		} else if (pkts_out == 0) {
			/* there are no outstanding transactions for this entry */
			if (sm_info->pkt.state == KGNI_SM_STATE_TERMINATE_INIT) {
				sm_info->pkt.state = KGNI_SM_STATE_TERMINATED;
			}
			list_del_init(&sm_info->smtask_list);
			if (chnl_hndl) {
				kgni_notify_user(chnl_hndl,sm_info);
			}
			wake_up_interruptible(&sm_info->nic_hndl->sm_wait_queue);
		}
	}
	spin_unlock_bh(&kgni_sm_lock);

	if (err || delayed_retransmit) {
		/* schedule task */
		queue_work(kgni_sm_wq, &kgni_sm_work);
	}

	if (work_pending(workq)) {
		schedule();
	}
}

/**
 * kgni_sm_chnl_event - session management event handler
 **/
void kgni_sm_chnl_event(kgni_chnl_handle_t chnl_hndl, uint64_t usr_cookie, uint32_t pkt_id, gni_return_t status)
{
	kgni_sm_info_t		*sm_info;

	GASSERT(usr_cookie < KGNI_MAX_CDM_NUMBER);
	GASSERT(kgni_cdm_array[usr_cookie] != NULL);
	GASSERT(pkt_id <= KGNI_MAX_SM_PKTS);

	sm_info = chnl_hndl->nic_hndl->sm_pkts[pkt_id];
	if (sm_info) {
		chnl_hndl->nic_hndl->sm_pkts[pkt_id] = NULL;
		sm_info->snd_status = status;
		if (status != GNI_RC_SUCCESS) {
			sm_info->flags |= KGNI_SM_FLAG_SEND_FAILED;
                        /* we can't deliver TERMINATE_INIT, so we just terminate */
                        if (sm_info->pkt.state == KGNI_SM_STATE_TERMINATE_INIT) {
                                sm_info->pkt.state = KGNI_SM_STATE_TERMINATED;
                        }
		} else {
			/* increment seq.num only after we got successful delivery event */
			sm_info->snd_seq_num++;
		}
		/* make sure pkts_out is decremented after sm_info->flags is set for failed transactions, 
		otherwise sm_task can mistakenly remove this sm_info from its list */
		atomic_dec(&sm_info->pkts_out);
		GASSERT(atomic_read(&sm_info->pkts_out) >= 0);

		GDEBUG_DGRAM(5, sm_info, NULL, "Got event (pkt_id=%d)", pkt_id);

		/* schedule sm_task */
		queue_work(kgni_sm_wq, &kgni_sm_work);
	} else {
		GDEBUG_DGRAM(1, NULL, NULL,
			     "Got event, but SM does not exists: pkt_id = %d status=%d",
			     pkt_id, status);
	}

	return;
}

/**
 * kgni_sm_chnl_rcv - session management receive callback
 **/
int kgni_sm_chnl_rcv(kgni_chnl_handle_t chnl_hndl, uint64_t usr_cookie, uint8_t *pkt_ptr, uint32_t pkt_len)
{
	gni_cdm_handle_t 	cdm_handle;
	kgni_sm_pkt_t		*rcv_sm_pkt = (kgni_sm_pkt_t *)pkt_ptr;
	kgni_sm_info_t		*sm_info;
	struct list_head	*ptr;
	kgni_sm_info_t		*open_sm_info = NULL;
	int			schedule_flag = 0;
	int			matched = 0;

	GASSERT(usr_cookie < KGNI_MAX_CDM_NUMBER);
	GASSERT(kgni_cdm_array[usr_cookie] != NULL);
	GASSERT(rcv_sm_pkt != NULL);

	cdm_handle = kgni_cdm_array[usr_cookie];
	/* Verify if this is our packet */
	if (pkt_len != sizeof(kgni_sm_pkt_t) ||
	    rcv_sm_pkt->cookie != cdm_handle->cookie ||
	    rcv_sm_pkt->remote_id.inst_id != cdm_handle->inst_id) {

		GDEBUG_DGRAM(2, NULL, rcv_sm_pkt,
			     "Packet NOTMINE: pkt_ptr 0x%p pkt_len %d, ptag %d %d, pkt cookie %d, cdm cookie %d",
			     pkt_ptr, pkt_len, rcv_sm_pkt->ptag, cdm_handle->ptag,
			     rcv_sm_pkt->cookie, cdm_handle->cookie);
		return GNI_CHNL_RCV_NOTMINE;
	}

	GDEBUG_DGRAM(5, NULL, rcv_sm_pkt, "Packet received");

	spin_lock(&kgni_sm_lock);
	list_for_each(ptr, &cdm_handle->sessions) {
		sm_info = list_entry(ptr, kgni_sm_info_t, cdm_list);

		GDEBUG_DGRAM(5, sm_info, rcv_sm_pkt, "Looking at session");

		if (sm_info->pkt.remote_id.pe == rcv_sm_pkt->local_id.pe &&
		    sm_info->pkt.local_id.pe == rcv_sm_pkt->remote_id.pe &&
		    sm_info->pkt.remote_id.inst_id == rcv_sm_pkt->local_id.inst_id &&
		    sm_info->pkt.local_id.inst_id == rcv_sm_pkt->remote_id.inst_id) {
			/* we have found entry */
			GDEBUG_DGRAM(3, sm_info, rcv_sm_pkt, "Matching session found");

			/* Sanity check: we should never get packet with remote_gen = 0 
			and state = KGNI_SM_STATE_COMPLETED*/
			if (rcv_sm_pkt->remote_gen == 0 && rcv_sm_pkt->state == KGNI_SM_STATE_COMPLETED) {
				GDEBUG_DGRAM(2, sm_info, rcv_sm_pkt,
					     "Got packet with remote_gen = 0 && state = KGNI_SM_STATE_COMPLETED");
				spin_unlock(&kgni_sm_lock);
				kgni_chnl_pkt_free(chnl_hndl, pkt_ptr);
				return GNI_CHNL_RCV_HANDLED;
			}
			/* Check generation: remote generation in the packet can be either 0, 
			if peer send his packet before getting one from me, or match my
			local generation. */
			if (rcv_sm_pkt->remote_gen != 0 && rcv_sm_pkt->remote_gen != sm_info->pkt.local_gen) {
				/* got old packet from some previous session */
				GDEBUG_DGRAM(2, sm_info, rcv_sm_pkt, "Got packet from old generation");
				spin_unlock(&kgni_sm_lock);
				kgni_chnl_pkt_free(chnl_hndl, pkt_ptr);
				return GNI_CHNL_RCV_HANDLED;
			}
			if (rcv_sm_pkt->seq_num != (sm_info->rcvd_seq_num + 1)) {
				GDEBUG_DGRAM(2, sm_info, rcv_sm_pkt, "Bad sequence number");
			}

			/* taking care of TERMINATE_INIT as proc. is common for all states */
			if (rcv_sm_pkt->state == KGNI_SM_STATE_TERMINATE_INIT) {
				if (sm_info->pkt.state == KGNI_SM_STATE_REMINFO_READY) {
					/* REMINFO_READY sessions are transparent locally,
					   free the session immediately */
					GASSERT(list_empty(&sm_info->smtask_list));
					list_del(&sm_info->cdm_list);
					kmem_cache_free(kgni_sm_mem_cache, sm_info);
					sm_info = NULL;
				} else {
					sm_info->pkt.state = KGNI_SM_STATE_TERMINATED;
					sm_info->flags &= ~(KGNI_SM_FLAG_STATE_CHANGED | KGNI_SM_FLAG_SEND_FAILED);
					/* session in TERMINATED state would not re-transmit,
					   but need to wait for outstanding events */
					matched = 1;
				}
			} else {
				if (sm_info->pkt.state == KGNI_SM_STATE_LOCINFO_READY) {
					if (rcv_sm_pkt->state == KGNI_SM_STATE_LOCINFO_READY) {
						sm_info->rem_udata_size = rcv_sm_pkt->local_udata_size;
						memcpy(sm_info->rem_udata, rcv_sm_pkt->local_udata,
						       rcv_sm_pkt->local_udata_size);
						sm_info->pkt.state = KGNI_SM_STATE_COMPLETED;
						sm_info->flags |= KGNI_SM_FLAG_STATE_CHANGED;
						if (list_empty(&sm_info->smtask_list)) {
							list_add(&sm_info->smtask_list,&kgni_sm_list);
						}
						schedule_flag = 1;
						matched = 1;
					} else if (rcv_sm_pkt->state == KGNI_SM_STATE_COMPLETED) {
						sm_info->rem_udata_size = rcv_sm_pkt->local_udata_size;
						memcpy(sm_info->rem_udata, rcv_sm_pkt->local_udata,
						       rcv_sm_pkt->local_udata_size);
						sm_info->pkt.state = KGNI_SM_STATE_COMPLETED;
						matched = 1;
					} else {
						GDEBUG_DGRAM(2, sm_info, rcv_sm_pkt,
							     "In state %d got unexpected state %d",
							     sm_info->pkt.state, rcv_sm_pkt->state);
					}
				} else if (sm_info->pkt.state == KGNI_SM_STATE_COMPLETED) {
					/* can only get another COMPLETED here */
					if (rcv_sm_pkt->state == KGNI_SM_STATE_COMPLETED) {
						/* no event */
						GDEBUG_DGRAM(2, sm_info, rcv_sm_pkt,
							     "Got COMPLETED while in COMPLETED state");
					} else {
						GDEBUG_DGRAM(1, sm_info, rcv_sm_pkt,
							     "In state COMPLETED got unexpected state %d",
							     rcv_sm_pkt->state);
						continue;
					}
				} else {
					GDEBUG_DGRAM(1, sm_info, rcv_sm_pkt,
						     "In state %d got unexpected state %d",
						     sm_info->pkt.state, rcv_sm_pkt->state);
				}
			}

			if (matched) {
				/* we have a complete match, update the local sessions
				 * generation and sequence number */
				sm_info->rcvd_seq_num = rcv_sm_pkt->seq_num;
				sm_info->pkt.remote_gen = rcv_sm_pkt->local_gen;
			}

			if (schedule_flag) {
				queue_work(kgni_sm_wq, &kgni_sm_work);
			} else if (matched) {
				/* We signal matched when a datagram enters a final state.
				   Do not queue_work AND notify_user. */
				kgni_notify_user(chnl_hndl,sm_info);
				wake_up_interruptible(&chnl_hndl->nic_hndl->sm_wait_queue);
			}

			spin_unlock(&kgni_sm_lock);

			kgni_chnl_pkt_free(chnl_hndl, pkt_ptr);
			return GNI_CHNL_RCV_HANDLED;

		} else if (open_sm_info == NULL &&
				!GNI_EP_BOUND(sm_info->pkt.remote_id.pe, sm_info->pkt.remote_id.inst_id) &&
				sm_info->pkt.local_id.inst_id == rcv_sm_pkt->remote_id.inst_id &&
				sm_info->pkt.local_id.pe == rcv_sm_pkt->remote_id.pe) {
			open_sm_info = sm_info;
		}
	}

	/* Do not create a new session when we get pkt. in STATE_COMPLETED or
	 * remote_gen != 0.  This could be for some old session */
	if (rcv_sm_pkt->state == KGNI_SM_STATE_COMPLETED ||
			rcv_sm_pkt->state == KGNI_SM_STATE_TERMINATE_INIT ||
			rcv_sm_pkt->remote_gen != 0) {
		spin_unlock(&kgni_sm_lock);
		kgni_chnl_pkt_free(chnl_hndl, pkt_ptr);
		GDEBUG_DGRAM(1, NULL, rcv_sm_pkt,
			     "Got state=%d remote_gen=%llu and did not find a matching session",
			     rcv_sm_pkt->state, rcv_sm_pkt->remote_gen);
		return GNI_CHNL_RCV_HANDLED;
	}

	/* Matching entry was not found; did we find open-ended connection */
	if (open_sm_info != NULL) {
		open_sm_info->pkt.remote_id.pe = rcv_sm_pkt->local_id.pe;
		open_sm_info->pkt.remote_id.inst_id = rcv_sm_pkt->local_id.inst_id;
		open_sm_info->rem_udata_size = rcv_sm_pkt->local_udata_size;
		memcpy(open_sm_info->rem_udata, rcv_sm_pkt->local_udata,
		       rcv_sm_pkt->local_udata_size);
		open_sm_info->pkt.remote_gen = rcv_sm_pkt->local_gen;
		open_sm_info->rcvd_seq_num = rcv_sm_pkt->seq_num;
		open_sm_info->pkt.state = KGNI_SM_STATE_COMPLETED;
		open_sm_info->flags = KGNI_SM_FLAG_STATE_CHANGED;
		/* should not be in smtask_list */
		GASSERT(list_empty(&open_sm_info->smtask_list));
		list_add(&open_sm_info->smtask_list,&kgni_sm_list);
		spin_unlock(&kgni_sm_lock);

		GDEBUG_DGRAM(3, open_sm_info, rcv_sm_pkt, "Open-ended session matched");

		/* schedule sm_task */
		queue_work(kgni_sm_wq, &kgni_sm_work);

	} else {
		/*  create new entry in KGNI_SM_STATE_REMINFO_READY state */
		sm_info = kmem_cache_alloc(kgni_sm_mem_cache, GFP_ATOMIC);
		if (sm_info != NULL) {
			INIT_LIST_HEAD(&sm_info->smtask_list);
			INIT_LIST_HEAD(&sm_info->cdm_list);
			sm_info->pkt.ptag = cdm_handle->ptag;
			sm_info->pkt.cookie = cdm_handle->cookie;
			sm_info->pkt.state = KGNI_SM_STATE_REMINFO_READY;
                        sm_info->post_id = -1;
			sm_info->flags = 0;
			atomic_set(&sm_info->pkts_out, 0);
			sm_info->flrcnt = 0;
			sm_info->nrx_cnt = 0;
			sm_info->rx_delay = 0;
			sm_info->virt_remote_pe = -1;
			sm_info->snd_status = GNI_RC_NOT_DONE;
			sm_info->nic_hndl = chnl_hndl->nic_hndl;
			sm_info->pkt.local_id.inst_id = cdm_handle->inst_id;
			sm_info->pkt.local_id.pe = chnl_hndl->nic_hndl->nic_pe;
			sm_info->pkt.local_gen = sm_generation++;
			sm_info->pkt.remote_gen = rcv_sm_pkt->local_gen;
			sm_info->pkt.seq_num = 1;
			sm_info->snd_seq_num = 1;
			sm_info->rcvd_seq_num = rcv_sm_pkt->seq_num;
			sm_info->rem_udata_size = rcv_sm_pkt->local_udata_size;
			memcpy(sm_info->rem_udata, rcv_sm_pkt->local_udata,
			       rcv_sm_pkt->local_udata_size);
			sm_info->pkt.remote_id.inst_id = rcv_sm_pkt->local_id.inst_id;
			sm_info->pkt.remote_id.pe = rcv_sm_pkt->local_id.pe;
			list_add_tail(&sm_info->cdm_list, &cdm_handle->sessions);

			GDEBUG_DGRAM(3, sm_info, rcv_sm_pkt, "New session added");
		}

		spin_unlock(&kgni_sm_lock);
	}

	kgni_chnl_pkt_free(chnl_hndl, pkt_ptr);

	return GNI_CHNL_RCV_HANDLED;
}
/**
 * kgni_sm_terminate - triger the terminate
 **/
int kgni_sm_terminate(gni_nic_handle_t nic_hndl, gni_ep_postdata_term_args_t *ep_postterm_attr)
{
	kgni_device_t 		*kgni_dev = (kgni_device_t *) nic_hndl->kdev_hndl;
	kgni_sm_info_t		*sm_info;
	kgni_chnl_handle_t      chnl_hndl = (kgni_chnl_handle_t)nic_hndl->sm_chnl_hndl;
	gni_cdm_handle_t 	cdm_handle;
	struct list_head	*ptr, *next;
	uint32_t		virt_remote_pe = -1;
        uint32_t                match_by_post_id = 0;
        uint32_t                matched=0;
	unsigned char		bound_ep = GNI_EP_BOUND(ep_postterm_attr->remote_pe,
						ep_postterm_attr->remote_id);

	if (nic_hndl->cdm_id == GNI_CDM_INVALID_ID) {
		GPRINTK_RL(1, KERN_INFO, "NIC is not attached");
		return (-EINVAL);
	}

	cdm_handle = kgni_cdm_array[nic_hndl->cdm_id];
	if (cdm_handle == NULL) {
		GPRINTK_RL(1, KERN_INFO, "CDM is not found");
		return (-EINVAL);
	}

	/* Open ended sessions must use post_id */
	if (!bound_ep) {
		if (ep_postterm_attr->use_post_id) {
			match_by_post_id = 1;
		} else {
			GPRINTK_RL(1, KERN_INFO, "Open ended sessions must use post_id");
			return (-EINVAL);
		}
	}

	if ((nic_hndl->ntt_grp_size) && (ep_postterm_attr->remote_pe != -1)) {
		/* translate virtual PE to physical */
		if (ep_postterm_attr->remote_pe >= nic_hndl->ntt_grp_size) {
			GPRINTK_RL(1, KERN_INFO, "NTT translation failed: vitualPE(%d) >= NPES(%d)",
			           ep_postterm_attr->remote_pe, nic_hndl->ntt_grp_size);
			return (-ENXIO);
		}
		virt_remote_pe = ep_postterm_attr->remote_pe;
		ep_postterm_attr->remote_pe = ghal_ntt_to_pe(kgni_dev->pdev, nic_hndl->ntt_base + virt_remote_pe);
		if (ep_postterm_attr->remote_pe < 0) {
			GPRINTK_RL(1, KERN_INFO, "NTT translation failed: ghal_ntt_to_pe returned error %d",
			           ep_postterm_attr->remote_pe);
			ep_postterm_attr->remote_pe = virt_remote_pe;
			return ep_postterm_attr->remote_pe;
		}
	}

	spin_lock_bh(&kgni_sm_lock);
	list_for_each_safe(ptr, next, &cdm_handle->sessions) {
		sm_info = list_entry(ptr, kgni_sm_info_t, cdm_list);
                matched = 0;
		if (match_by_post_id) {
			if (sm_info->post_id != -1 &&
			    ep_postterm_attr->post_id == sm_info->post_id) {
				matched = 1;
			}
		} else {
			if (sm_info->pkt.remote_id.pe == ep_postterm_attr->remote_pe &&
			    sm_info->pkt.remote_id.inst_id == ep_postterm_attr->remote_id) {
				matched = 1;
			}
                }
                if (matched) {
		        /* open-ended session that hasn't been matched is treated special,
                            calling terminate on openended session will 
                            make driver release first one in the list */
        		if (!bound_ep &&
        		    sm_info->pkt.remote_id.pe == ep_postterm_attr->remote_pe) {
				list_del(&sm_info->cdm_list);
        			/* increment userspace mapped counter to make sure application
				   gets notified of the change when fast polling */
				if (chnl_hndl->datagram_cnt_kern != NULL) {
					atomic_inc(chnl_hndl->datagram_cnt_kern);
				}
				GDEBUG_DGRAM(3, sm_info, NULL, "Deleting open-ended session");
        			kmem_cache_free(kgni_sm_mem_cache, sm_info);
                                spin_unlock_bh(&kgni_sm_lock);
                                return 0;                                
        		} else {
        			sm_info->pkt.state = KGNI_SM_STATE_TERMINATE_INIT;
        			sm_info->flags = KGNI_SM_FLAG_STATE_CHANGED;
        			/* check if this session is in the task list already and add if not */
        			if (list_empty(&sm_info->smtask_list)) {
        				list_add(&sm_info->smtask_list,&kgni_sm_list);
        			}
        			spin_unlock_bh(&kgni_sm_lock);
        			/* schedule sm_task */
        			queue_work(kgni_sm_wq, &kgni_sm_work);
        			return 0;
		        }
	        }
        }
	spin_unlock_bh(&kgni_sm_lock);
	return (-ESRCH);

}

/**
 * kgni_sm_wait_condition - return TRUE/FALSE condition of the session
 **/
inline int kgni_sm_wait_condition(gni_nic_handle_t nic_hndl, gni_ep_postdata_test_args_t *ep_posttest_attr, int *err)
{
	/* check if it's ready; also it will check for errors */
	*err = kgni_sm_get_state(nic_hndl, ep_posttest_attr);
	if (*err ||
	    ep_posttest_attr->state == GNI_POST_COMPLETED ||
	    ep_posttest_attr->state == GNI_POST_TERMINATED ||
	    ep_posttest_attr->state == GNI_POST_TIMEOUT) {
		return 1;
	}
	return 0;
}

/**
 * kgni_sm_probe_wait_condition - return TRUE if kgni_sm_probe_state finds a completed session
 **/
inline int kgni_sm_probe_wait_condition(gni_nic_handle_t nic_hndl,
					gni_ep_postdata_test_args_t *ep_posttest_attr, int *err)
{
	/* check if it's ready; also it will check for errors */
	*err = kgni_sm_probe_state(nic_hndl, ep_posttest_attr);

	if (*err == (-ESRCH)) {
		/* no datagram matched, continue sleeping */
		return 0;
	}

	return 1;
}

/**
 * kgni_sm_wait_state - wait until the session's post is finished
 **/
int kgni_sm_wait_state(gni_nic_handle_t nic_hndl, gni_ep_postdata_test_args_t *ep_posttest_attr)
{
	int 		err;
	unsigned long 	timeout = MAX_SCHEDULE_TIMEOUT;
	long 		ret;

	if (ep_posttest_attr->timeout != (uint32_t)(-1)) {
		timeout = msecs_to_jiffies(ep_posttest_attr->timeout);
	}

	if (ep_posttest_attr->modes & GNI_EP_PDT_MODE_PROBE) {
		ret = wait_event_interruptible_timeout(nic_hndl->sm_wait_queue,
						 kgni_sm_probe_wait_condition(nic_hndl,
								ep_posttest_attr, &err),
						 timeout);
	} else {
		ret = wait_event_interruptible_timeout(nic_hndl->sm_wait_queue,
						 kgni_sm_wait_condition(nic_hndl,
						 		ep_posttest_attr, &err),
						 timeout);
	}

	if(ret == 0) {
		ret = -EAGAIN;
	} else if (ret == -ERESTARTSYS) {
		ret = -EINTR;
	} else {
		ret = err;
	}

	return ret;
}

/**
 * kgni_sm_get_state - get state of the session
 **/
int kgni_sm_get_state(gni_nic_handle_t nic_hndl, gni_ep_postdata_test_args_t *ep_posttest_attr)
{
	kgni_device_t 		*kgni_dev = (kgni_device_t *) nic_hndl->kdev_hndl;
	kgni_sm_info_t		*sm_info;
        kgni_chnl_handle_t      chnl_hndl=(kgni_chnl_handle_t)nic_hndl->sm_chnl_hndl;
	gni_cdm_handle_t 	cdm_handle;
	struct list_head	*ptr, *next;
	int 			retval = -ESRCH;
	uint32_t		remote_pe;
	uint32_t		virt_remote_pe = -1;
        uint32_t                match_by_post_id = 0;
        uint32_t                matched=0;
	char 			max_datagram_buffer[GNI_DATAGRAM_MAXSIZE];
	size_t			out_data_buffer_size = 0;
	void 			*out_data_buffer = NULL;
	unsigned char		bound_ep = GNI_EP_BOUND(ep_posttest_attr->remote_pe,
						ep_posttest_attr->remote_id);
	int			free_sm = 0;

	if (nic_hndl->cdm_id == GNI_CDM_INVALID_ID) {
		GPRINTK_RL(1, KERN_INFO, "NIC is not attached");
		return (-EINVAL);
	}

	cdm_handle = kgni_cdm_array[nic_hndl->cdm_id];
	if (cdm_handle == NULL) {
		GPRINTK_RL(1, KERN_INFO, "CDM is not found");
		return (-EINVAL);
	}

        /*
         * short circuit to probe code if not removing a session
         */
        if (ep_posttest_attr->modes & GNI_EP_PDT_MODE_PROBE) {
                retval = kgni_sm_probe_state(nic_hndl,ep_posttest_attr);
                return retval;
        }

	if ((nic_hndl->ntt_grp_size) && bound_ep) {
		/* translate virtual PE to physical */
		if (ep_posttest_attr->remote_pe >= nic_hndl->ntt_grp_size) {
			GPRINTK_RL(1, KERN_INFO, "NTT translation failed: vitualPE(%d) >= NPES(%d)",
			           ep_posttest_attr->remote_pe, nic_hndl->ntt_grp_size);
			return (-ENXIO);
		}
		virt_remote_pe = ep_posttest_attr->remote_pe;
		ep_posttest_attr->remote_pe = ghal_ntt_to_pe(kgni_dev->pdev, nic_hndl->ntt_base + virt_remote_pe);
		if (ep_posttest_attr->remote_pe < 0) {
			GPRINTK_RL(1, KERN_INFO, "NTT translation failed: ghal_ntt_to_pe returned error %d",
			           ep_posttest_attr->remote_pe);
			ep_posttest_attr->remote_pe = virt_remote_pe;
			return ep_posttest_attr->remote_pe;
		}
	}

	/* Open ended sessions must use post_id */
	if(ep_posttest_attr->modes & GNI_EP_PDT_MODE_USE_POST_ID) {
		match_by_post_id = 1;
		GDEBUG_DGRAM(5, NULL, NULL, "Looking for session: local[pe=0x%x inst=%d] post_id: 0x%llx",
			     nic_hndl->nic_pe, nic_hndl->inst_id, ep_posttest_attr->post_id);
	} else {
		GDEBUG_DGRAM(5, NULL, NULL, "Looking for session: local[pe=0x%x inst=%d] remote[pe=0x%x inst=%d]",
			     nic_hndl->nic_pe, nic_hndl->inst_id, ep_posttest_attr->remote_pe, ep_posttest_attr->remote_id);
	}

	if (!bound_ep && !match_by_post_id) {
		GPRINTK_RL(0, KERN_INFO, "Open ended sessions must use post_id");
		return (-EINVAL);
	}

	if (ep_posttest_attr->out_buff) {
		/* 
		 * Set pointer used for later memcpy.
		 * To avoid doing a double copy for kernel callers, we just do
		 * the memcpy once (directly into ep_posttest_attr->out_buff).
		 */
		if (nic_hndl->cdm_type != GNI_CDM_TYPE_KERNEL) {
			out_data_buffer = max_datagram_buffer;
		} else {
			out_data_buffer = ep_posttest_attr->out_buff;
		}
	}

	spin_lock_bh(&kgni_sm_lock);
	list_for_each_safe(ptr, next, &cdm_handle->sessions) {
		sm_info = list_entry(ptr, kgni_sm_info_t, cdm_list);
                matched = 0;
		if (match_by_post_id) {
			if (sm_info->post_id != -1 &&
			    ep_posttest_attr->post_id == sm_info->post_id) {
				matched = 1;
			}
		} else {
			if (sm_info->pkt.remote_id.pe == ep_posttest_attr->remote_pe &&
			    sm_info->pkt.remote_id.inst_id == ep_posttest_attr->remote_id) {
				matched = 1;
			}
		}
		if (matched) {
			switch (sm_info->pkt.state) {
			case KGNI_SM_STATE_COMPLETED:
				ep_posttest_attr->state = GNI_POST_COMPLETED;
				break;
			case KGNI_SM_STATE_TERMINATED:
				ep_posttest_attr->state = GNI_POST_TERMINATED;
				break;
			case KGNI_SM_STATE_TIMEOUT:
				ep_posttest_attr->state = GNI_POST_TIMEOUT;
				break;
			case KGNI_SM_STATE_REMINFO_READY:
				ep_posttest_attr->state = GNI_POST_REMOTE_DATA;
				break;
			default:
				ep_posttest_attr->state = GNI_POST_PENDING;
			}
			retval = 0;

			GDEBUG_DGRAM(5, sm_info, NULL, "Session found, match_by_postid: %d", match_by_post_id);

			if (sm_info->pkt.state == KGNI_SM_STATE_COMPLETED ||
			    sm_info->pkt.state == KGNI_SM_STATE_TERMINATED ||
			    sm_info->pkt.state == KGNI_SM_STATE_TIMEOUT ||
			    sm_info->pkt.state == KGNI_SM_STATE_REMINFO_READY) {
				/* Remove and free the entry */
				/* Copy remote user data */
				ep_posttest_attr->remote_pe = sm_info->pkt.remote_id.pe;
				ep_posttest_attr->remote_id = sm_info->pkt.remote_id.inst_id;
				ep_posttest_attr->post_id = sm_info->post_id;

				if (out_data_buffer) {
					if (unlikely(ep_posttest_attr->buff_size < sm_info->rem_udata_size)) {
						GDEBUG_DGRAM(1, sm_info, NULL, "Buffer is too small for remote data, buff_size: %u",
							     ep_posttest_attr->buff_size);
						retval = -E2BIG;
					} else {
						memcpy(out_data_buffer, sm_info->rem_udata,
						       sm_info->rem_udata_size);
						out_data_buffer_size = sm_info->rem_udata_size;
					}
				}

				if (sm_info->pkt.state != KGNI_SM_STATE_REMINFO_READY) {
					if (list_empty(&sm_info->smtask_list)) {
						if (chnl_hndl->datagram_cnt_kern != NULL) {
							GDEBUG_DGRAM(5, NULL, NULL, "Decrementing kern_counter %p",
								     chnl_hndl->datagram_cnt_kern);
							atomic_dec(chnl_hndl->datagram_cnt_kern);
						}
						free_sm = 1;
					} else {
						GDEBUG_DGRAM(3, sm_info, NULL, "session still on smtask_list");
						/* session is still in smtask_list; user should wait for re-transmission to succeed */
						ep_posttest_attr->state = GNI_POST_PENDING;
					}
				}

				/* see if we need to translate phys. PE to virtual PE */
				if (nic_hndl->ntt_grp_size) {
					if (sm_info->virt_remote_pe == -1) {
						remote_pe = kgni_ntt_physical_to_logical(nic_hndl,sm_info->pkt.remote_id.pe);
						if (remote_pe < 0) {
							retval = remote_pe;
							GPRINTK_RL(1, KERN_INFO, "Failed to lookup PE 0x%x (ntt_base=%d ntt_size=%d)",
							           sm_info->pkt.remote_id.pe, nic_hndl->ntt_base, nic_hndl->ntt_grp_size);
							spin_unlock_bh(&kgni_sm_lock);
							return retval;
						}
						else {
							sm_info->virt_remote_pe = remote_pe;
						}
					}
					/*
					 * if virt_remote_pe is still -1 we didn't find it in the ntt,
					 * something is wrong.
					 */
					if (sm_info->virt_remote_pe == -1) {
						GPRINTK_RL(1, KERN_INFO, "Failed to find virtual pe for phys pe 0x%x",
						           sm_info->pkt.remote_id.pe);
						spin_unlock_bh(&kgni_sm_lock);
						return (-ENXIO);
					}
					ep_posttest_attr->remote_pe = sm_info->virt_remote_pe;
				}
			}

			if (ep_posttest_attr->remote_pe != -1) {
				/* we found a match, break and return */
				if (free_sm) {
					/* our match was in a completed state, free it */
					GDEBUG_DGRAM(3, sm_info, NULL, "Deleting session");
					list_del(&sm_info->cdm_list);
					kmem_cache_free(kgni_sm_mem_cache, sm_info);
				}
				break;
			}
		}
	}
	if (virt_remote_pe != -1) {
		ep_posttest_attr->remote_pe = virt_remote_pe;
	}
	spin_unlock_bh(&kgni_sm_lock);

	if (out_data_buffer_size &&
	    (nic_hndl->cdm_type != GNI_CDM_TYPE_KERNEL)) {
		/* 
		 * here we return ENOMEM because we already insured
		 * the buffer sizes were correct, and the memory was
		 * accessible. The only reason for failure is not being
		 * able to swap in a page. NB: copy_to_user returns bytes
		 * not copied, so we switch this to -ENOMEM.
		 * Or this could be a bogus user pointer, in which case
		 * maybe we should change to return -EFAULT.
		 */
		retval = copy_to_user(ep_posttest_attr->out_buff, out_data_buffer,
				      out_data_buffer_size) ? -ENOMEM : 0;
	}

	if (retval == (-ESRCH)) {
		GDEBUG_DGRAM(1, NULL, NULL,
			     "Failed to find session: local[pe=0x%x inst=%d] remote[pe=0x%x inst=%d] "
			     "post_id: 0x%llx, match_by_post_id: %d",
			     nic_hndl->nic_pe, nic_hndl->inst_id,
			     ep_posttest_attr->remote_pe, ep_posttest_attr->remote_id,
			     ep_posttest_attr->post_id, match_by_post_id);
	}

	return retval;
}

/**
 * kgni_sm_add_session - add a new session into CDM list
 **/
int kgni_sm_add_session(gni_nic_handle_t nic_hndl, gni_ep_postdata_args_t *ep_postdata_attr)
{
	kgni_device_t 		*kgni_dev = (kgni_device_t *) nic_hndl->kdev_hndl;
	kgni_sm_info_t		*sm_info;
	gni_cdm_handle_t 	cdm_handle;
	struct list_head	*ptr;
	uint32_t		virt_remote_pe = -1;
	char 			max_datagram_buffer[GNI_DATAGRAM_MAXSIZE];
	void 			*in_data_buffer = NULL;	
	char			bound_ep = GNI_EP_BOUND(ep_postdata_attr->remote_pe,
						ep_postdata_attr->remote_id);

	if (nic_hndl->cdm_id == GNI_CDM_INVALID_ID) {
		GPRINTK_RL(1, KERN_INFO, "NIC is not attached");
		return (-EINVAL);
	}

	cdm_handle = kgni_cdm_array[nic_hndl->cdm_id];
	if (cdm_handle == NULL) {
		GPRINTK_RL(1, KERN_INFO, "CDM is not found");
		return (-EINVAL);
	}

	if (ep_postdata_attr->in_data_len > GNI_DATAGRAM_MAXSIZE) {
		GPRINTK_RL(1, KERN_INFO, "User data is too big");
		return (-E2BIG);
	}

	if (!bound_ep && !(ep_postdata_attr->use_post_id)) {
		GPRINTK_RL(1, KERN_INFO, "Open ended sessions must use post_id");
		return (-EINVAL);
	}

	if ((nic_hndl->ntt_grp_size) && (ep_postdata_attr->remote_pe != -1)) {
		if (ep_postdata_attr->remote_pe >= nic_hndl->ntt_grp_size) {
			GPRINTK_RL(1, KERN_INFO, "NTT translation failed: vitualPE(%d) >= NPES(%d)",
			           ep_postdata_attr->remote_pe, nic_hndl->ntt_grp_size);
			return (-ENXIO);
		}
		virt_remote_pe = ep_postdata_attr->remote_pe;
		ep_postdata_attr->remote_pe = ghal_ntt_to_pe(kgni_dev->pdev, nic_hndl->ntt_base + virt_remote_pe);
		if (ep_postdata_attr->remote_pe < 0) {
			GPRINTK_RL(1, KERN_INFO, "NTT translation failed: ghal_ntt_to_pe returned error %d",
			           ep_postdata_attr->remote_pe);
			ep_postdata_attr->remote_pe = virt_remote_pe;
			return ep_postdata_attr->remote_pe;
		}
	}

	GDEBUG_DGRAM(5, NULL, NULL, "Looking for an existing session: local[pe=0x%x inst=%d] remote[pe=0x%x inst=%d]",
		     nic_hndl->nic_pe, nic_hndl->inst_id, ep_postdata_attr->remote_pe, ep_postdata_attr->remote_id);

	if (ep_postdata_attr->in_data) {
		/* 
		 * To avoid doing a double copy for kernel callers, we will just
		 * do the memcpy once directly from ep_postdata_attr->in_data.
		 */
		if (nic_hndl->cdm_type != GNI_CDM_TYPE_KERNEL) {
			/* 
			 * here we return ENOMEM because we already insured
			 * the buffer sizes were correct, and the memory was
			 * accessible. The only reason for failure is not being
			 * able to swap in a page. NB: copy_from_user returns
			 * bytes not copied, so we switch this to -ENOMEM.
			 * Or this could be a bogus user pointer, in which case
			 * maybe we should change to return -EFAULT.
			 */
			in_data_buffer = max_datagram_buffer;
			if (copy_from_user(in_data_buffer, ep_postdata_attr->in_data,
					   ep_postdata_attr->in_data_len)) {
				return (-ENOMEM);
			}
		} else {
			in_data_buffer = ep_postdata_attr->in_data;
		}
	}

	/* look for existing entry in KGNI_SM_STATE_REMINFO_READY state */
	spin_lock_bh(&kgni_sm_lock);
	list_for_each(ptr, &cdm_handle->sessions) {
		sm_info = list_entry(ptr, kgni_sm_info_t, cdm_list);
		if (sm_info->pkt.state == KGNI_SM_STATE_REMINFO_READY &&
		    (!bound_ep ||
		     (ep_postdata_attr->remote_pe == sm_info->pkt.remote_id.pe &&
		      ep_postdata_attr->remote_id == sm_info->pkt.remote_id.inst_id))) {
			if (in_data_buffer) {
				sm_info->pkt.local_udata_size = ep_postdata_attr->in_data_len;
				memcpy(sm_info->pkt.local_udata, in_data_buffer,
				       ep_postdata_attr->in_data_len);
			} else {
				sm_info->pkt.local_udata_size = 0;
			}
			sm_info->pkt.state = KGNI_SM_STATE_COMPLETED;
			sm_info->flags = KGNI_SM_FLAG_STATE_CHANGED;
			sm_info->virt_remote_pe = virt_remote_pe;
                        if (ep_postdata_attr->use_post_id) {
                                sm_info->post_id = ep_postdata_attr->post_id;
                        }
			/* should not be in smtask_list */
			GASSERT(list_empty(&sm_info->smtask_list));
			list_add(&sm_info->smtask_list,&kgni_sm_list);
			spin_unlock_bh(&kgni_sm_lock);
			/* schedule sm_task */
			queue_work(kgni_sm_wq, &kgni_sm_work);

			GDEBUG_DGRAM(3, sm_info, NULL, "Session matched");

			return 0;
		} else if (bound_ep &&
				ep_postdata_attr->remote_pe == sm_info->pkt.remote_id.pe &&
				ep_postdata_attr->remote_id == sm_info->pkt.remote_id.inst_id) {
			GDEBUG_DGRAM(1, sm_info, NULL, "Duplicate entry found in state = %d",
				     sm_info->pkt.state);
			spin_unlock_bh(&kgni_sm_lock);
			return (-EEXIST);
		}
	}

	/* No existing entry, create a new one */

	sm_info = kmem_cache_alloc(kgni_sm_mem_cache, GFP_ATOMIC);
	if (sm_info == NULL) {
		spin_unlock_bh(&kgni_sm_lock);
		return (-ENOMEM);
	}
	INIT_LIST_HEAD(&sm_info->smtask_list);
	INIT_LIST_HEAD(&sm_info->cdm_list);
	sm_info->pkt.ptag = cdm_handle->ptag;
	sm_info->pkt.cookie = cdm_handle->cookie;
	sm_info->pkt.state = KGNI_SM_STATE_LOCINFO_READY;
	sm_info->post_id = -1;
	sm_info->flags = 0;
	sm_info->virt_remote_pe = virt_remote_pe;
	atomic_set(&sm_info->pkts_out, 0);
	sm_info->flrcnt = 0;
	sm_info->nrx_cnt = 0;
	sm_info->rx_delay = 0;
	sm_info->snd_status = GNI_RC_NOT_DONE;
	sm_info->nic_hndl = nic_hndl;
	sm_info->rem_udata_size = 0;
	sm_info->pkt.local_id.inst_id = nic_hndl->inst_id;
	sm_info->pkt.local_id.pe = nic_hndl->nic_pe;
	sm_info->pkt.local_gen = sm_generation++;
	sm_info->pkt.remote_gen = 0;
	sm_info->pkt.seq_num = 1;
	sm_info->snd_seq_num = 1;
	sm_info->rcvd_seq_num = 0;
	if (in_data_buffer) {
		sm_info->pkt.local_udata_size = ep_postdata_attr->in_data_len;
		memcpy(sm_info->pkt.local_udata, in_data_buffer, ep_postdata_attr->in_data_len);
	} else {
		sm_info->pkt.local_udata_size = 0;
	}

        if (ep_postdata_attr->use_post_id) {
                sm_info->post_id = ep_postdata_attr->post_id;
        }
	sm_info->pkt.remote_id.inst_id = ep_postdata_attr->remote_id;
	sm_info->pkt.remote_id.pe = ep_postdata_attr->remote_pe;
	list_add_tail(&sm_info->cdm_list, &cdm_handle->sessions);
	if (sm_info->pkt.remote_id.pe != -1) {
		/* if not open-ended session */
		sm_info->flags = KGNI_SM_FLAG_STATE_CHANGED;
		list_add(&sm_info->smtask_list,&kgni_sm_list);
	}
	spin_unlock_bh(&kgni_sm_lock);

	/* schedule sm_task */
	queue_work(kgni_sm_wq, &kgni_sm_work);

	GDEBUG_DGRAM(3, sm_info, NULL, "New session added");

	return 0;
}

/**
 * kgni_sm_cleanup - Free sm resources allocated for NIC instance
 * Caller must hold kgni_sm_lock
 **/
static void kgni_sm_cleanup(gni_nic_handle_t nic_hndl, gni_cdm_handle_t cdm_handle)
{
	kgni_sm_info_t		*sm_info;
	struct list_head	*ptr, *next;

	if (nic_hndl->cdm_id == GNI_CDM_INVALID_ID) {
		GPRINTK_RL(0, KERN_INFO, "NIC is not attached");
		return;
	}

	if (nic_hndl->cdm_id >= KGNI_MAX_CDM_NUMBER) {
		GPRINTK_RL(0, KERN_INFO, "Invalid CDM ID %d", nic_hndl->cdm_id);
		return;
	}

	if (cdm_handle == NULL) {
		GPRINTK_RL(0, KERN_INFO, "CDM is invalid");
		return;
	}

	list_for_each_safe(ptr, next, &cdm_handle->sessions) {
		sm_info = list_entry(ptr, kgni_sm_info_t, cdm_list);
		if (sm_info->nic_hndl == nic_hndl) {
			if (!list_empty(&sm_info->smtask_list)) {
				list_del_init(&sm_info->smtask_list);
				wake_up_interruptible(&sm_info->nic_hndl->sm_wait_queue);
			}
			list_del_init(&sm_info->cdm_list);
			GDEBUG(3,"Session (rem_pe=0x%x rem_inst=%d state=%d) is freed",
			       sm_info->pkt.remote_id.pe, sm_info->pkt.remote_id.inst_id,
			       sm_info->pkt.state);
			kmem_cache_free(kgni_sm_mem_cache, sm_info);
		}
	}

	if (nic_hndl->sm_pkts) {
		kfree(nic_hndl->sm_pkts);
		nic_hndl->sm_pkts = NULL;
	}

	return;
}

/**
 * kgni_rdma_post - post RDMA transaction
 *
 * Note: rdma_mode = GNI_RDMAMODE_PHYS_ADDR is not currently implemented
 * If needed coincider implementing it as a separate function, keeping more
 * common case simpler.
 **/
int kgni_rdma_post(gni_nic_handle_t 		nic_hndl,
		   uint32_t 			remote_pe,
		   gni_post_descriptor_t 	*post_desc,
		   uint64_t 			hw_cq_hndl,
		   uint64_t			src_cq_data,
		   uint64_t			usr_event_data)
{
	kgni_device_t 		*kgni_dev = (kgni_device_t *) nic_hndl->kdev_hndl;
	ghal_tx_desc_t		tx_desc = GHAL_TXD_INIT;
	kgni_bte_ctrl_t		*cur_bte_ctrl, *tx_bte_ctrl;
	ghal_vc_desc_t 		*vc_desc;
	kgni_ring_desc_t	*tx_ring;
	kgni_bte_queue_t	*bte_txq;
	kgni_pkt_queue_t	*queue_elm = NULL;
	unsigned long		tx_flags = 0, pkt_flags = 0;
	uint32_t		vcid;
	int			entry;
	int			trans_pe;
	int			single_bte;

#ifdef GNI_DEBUG
	if (nic_hndl->type_id != GNI_TYPE_NIC_HANDLE) {
		GPRINTK_RL(1, KERN_INFO, "Invalid NIC handle");
		return (-EINVAL);
	}

	if (!GNI_MEMHNDL_VERIFY(post_desc->local_mem_hndl)) {
		GPRINTK_RL(1, KERN_INFO, "Invalid Local Mem.Handle");
		return (-EINVAL);
	}

	if (!GNI_MEMHNDL_VERIFY(post_desc->remote_mem_hndl)) {
		GPRINTK_RL(1, KERN_INFO, "Invalid Remote Mem.Handle");
		return (-EINVAL);
	}

	if (post_desc->type != GNI_POST_RDMA_GET &&
	    post_desc->type != GNI_POST_RDMA_PUT) {
		GPRINTK_RL(1, KERN_INFO, "Invalid type of transaction (%d)",
		           post_desc->type);
		return (-EINVAL);
	}

	if (post_desc->length == 0) {
		GPRINTK_RL(1, KERN_INFO, "Zero transaction length is not allowed");
		return (-EINVAL);
	}

	if (post_desc->local_addr < GNI_MEMHNDL_GET_VA(post_desc->local_mem_hndl) ||
	    (post_desc->local_addr + post_desc->length) >
	     (GNI_MEMHNDL_GET_VA(post_desc->local_mem_hndl) +
	     (GNI_MEMHNDL_GET_PAGESIZE(post_desc->local_mem_hndl)* GNI_MEMHNDL_GET_NPAGES(post_desc->local_mem_hndl)))) {
		GPRINTK_RL(1, KERN_INFO, "Local address is out of range: addr=0x%llx length=%lld "
		           "reg_start=0x%llx page_sz=%ld page_num=%lld",
		           post_desc->local_addr, post_desc->length,
		           GNI_MEMHNDL_GET_VA(post_desc->local_mem_hndl), (long)GNI_MEMHNDL_GET_PAGESIZE(post_desc->local_mem_hndl),
		           GNI_MEMHNDL_GET_NPAGES(post_desc->local_mem_hndl));
		return (-EINVAL);
	}
	if (post_desc->remote_addr < GNI_MEMHNDL_GET_VA(post_desc->remote_mem_hndl) ||
	    (post_desc->remote_addr + post_desc->length) >
	     (GNI_MEMHNDL_GET_VA(post_desc->remote_mem_hndl) +
	     (GNI_MEMHNDL_GET_PAGESIZE(post_desc->remote_mem_hndl)* GNI_MEMHNDL_GET_NPAGES(post_desc->remote_mem_hndl)))) {
		GPRINTK_RL(1, KERN_INFO, "Remote address is out of range: addr=0x%llx length=%lld "
		           "reg_start=0x%llx page_sz=%ld page_num=%lld",
		           post_desc->remote_addr, post_desc->length,
		           GNI_MEMHNDL_GET_VA(post_desc->remote_mem_hndl), (long)GNI_MEMHNDL_GET_PAGESIZE(post_desc->remote_mem_hndl),
		           GNI_MEMHNDL_GET_NPAGES(post_desc->remote_mem_hndl));
		return (-EINVAL);
	}
	if ((post_desc->cq_mode & (GNI_CQMODE_GLOBAL_EVENT |GNI_CQMODE_LOCAL_EVENT)) &&
	    hw_cq_hndl >= GHAL_CQ_MAX_NUMBER) {
		GPRINTK_RL(1, KERN_INFO, "Event requested, but CQ is invalid");
		return (-EINVAL);
	}
	if (post_desc->length > GHAL_BTE_MAX_LENGTH) {
		return (-EINVAL);
	}

#endif /* GNI_DEBUG */

	single_bte = (nic_hndl->modes & GNI_CDM_MODE_BTE_SINGLE_CHANNEL) || (multi_bte == 0);
	cur_bte_ctrl = kgni_dev->domain_bte_ctrl[nic_hndl->cdm_type];
	bte_txq = &kgni_dev->domain_bte_txq[nic_hndl->cdm_type];
	if (unlikely(single_bte)) {
		tx_bte_ctrl = cur_bte_ctrl;
		vcid = cur_bte_ctrl->vcid;
	} else {
		unsigned chnl;
		chnl = ((uint32_t)atomic_add_return(1, &kgni_dev->cur_bte_chnl)) % kgni_dev->num_bte_chnls;
		tx_bte_ctrl = kgni_bte_next_chnl(kgni_dev, GHAL_VC_MODE_DIRECT_BTE, chnl);
		vcid = KGNI_BTE_VCID_ANY;
	}
	tx_ring = &tx_bte_ctrl->tx_ring;
	vc_desc = &tx_bte_ctrl->bte_vc_desc;

	if (nic_hndl->ntt_grp_size) {
		if (remote_pe >= nic_hndl->ntt_grp_size) {
			GPRINTK_RL(1, KERN_INFO, "NTT translation failed: vitualPE(%d) >= NPES(%d)",
			           remote_pe, nic_hndl->ntt_grp_size);
			return (-ENXIO);
		}
		trans_pe = ghal_ntt_to_pe(kgni_dev->pdev, nic_hndl->ntt_base + remote_pe);
		if (trans_pe < 0) {
			GPRINTK_RL(1, KERN_INFO, "NTT translation failed: ghal_ntt_to_pe returned error %d",
			           trans_pe);
			return trans_pe;
		}
		remote_pe = trans_pe;
	}

	if ((post_desc->type == GNI_POST_RDMA_PUT && (GNI_MEMHNDL_GET_FLAGS(post_desc->remote_mem_hndl) & GNI_MEMHNDL_FLAG_READONLY)) ||
	    (post_desc->type == GNI_POST_RDMA_GET && (GNI_MEMHNDL_GET_FLAGS(post_desc->local_mem_hndl) & GNI_MEMHNDL_FLAG_READONLY))) {
		GPRINTK_RL(1, KERN_DEBUG, "Trying to use read only mem.handle as destination");
		return (-EACCES);
	}

	if (post_desc->type == GNI_POST_RDMA_GET &&
	    (post_desc->local_addr & GNI_LOCAL_ADDR_ALIGNMENT ||
	     post_desc->remote_addr & GNI_ADDR_ALIGNMENT ||
	     post_desc->length 	 & GNI_ADDR_ALIGNMENT	)) {
		GPRINTK_RL(1, KERN_INFO, "Data is not properly aligned");
		return (-EFAULT);
	}

	if (unlikely(atomic_read(&kgni_dev->resources[nic_hndl->ptag]->rdma.count) >= (uint64_t)kgni_dev->resources[nic_hndl->ptag]->rdma.limit)) {
		GPRINTK_RL(1, KERN_INFO, "RDMA limit reached (ptag = %d, count = %d, limit = %d)",
		           nic_hndl->ptag, atomic_read(&kgni_dev->resources[nic_hndl->ptag]->rdma.count),
		           kgni_dev->resources[nic_hndl->ptag]->rdma.limit);
		return (-ENOSPC);
	}

	if (post_desc->type == GNI_POST_RDMA_GET) {
		GHAL_TXD_SET_BTE_OP(tx_desc, GHAL_TXD_MODE_BTE_GET);
	} else {
		GHAL_TXD_SET_BTE_OP(tx_desc, GHAL_TXD_MODE_BTE_PUT);
	}

	if (hw_cq_hndl < GHAL_CQ_MAX_NUMBER) {
		if (unlikely(kgni_dev->cq_hdnls[hw_cq_hndl] == NULL ||
		    kgni_dev->cq_hdnls[hw_cq_hndl]->nic_hndl != nic_hndl)) {
			GPRINTK_RL(1, KERN_INFO, "Invalid CQ handle");
			return (-EINVAL);
		}
		GHAL_TXD_SET_SRC_CQ(tx_desc, hw_cq_hndl);
	}
	if (post_desc->cq_mode & GNI_CQMODE_LOCAL_EVENT) {
		GHAL_TXD_SET_CQ_LOCAL(tx_desc);
		GHAL_TXD_SET_CQE_LOCAL(tx_desc, src_cq_data);
	}

	if (post_desc->cq_mode & GNI_CQMODE_GLOBAL_EVENT) {
		GHAL_TXD_SET_CQ_GLOBAL(tx_desc);
		GHAL_TXD_SET_CQE_GLOBAL(tx_desc, src_cq_data, usr_event_data);
	}

	if (post_desc->cq_mode & GNI_CQMODE_REMOTE_EVENT) {
		GHAL_TXD_SET_IMM(tx_desc);
		GHAL_TXD_SET_CQE_REMOTE(tx_desc, usr_event_data);
	}

	if (kgni_bte_tx_set_rc(post_desc->dlvr_mode, &tx_desc)) {
		return (-EINVAL);
	}

	if (unlikely(post_desc->rdma_mode & GNI_RDMAMODE_FENCE)) {
		if (!single_bte) {
			GPRINTK_RL(1, KERN_INFO, "Use GNI_RDMAMODE_FENCE with GNI_CDM_MODE_BTE_SINGLE_CHANNEL");
			return (-EINVAL);
		}
		GHAL_TXD_SET_FENCE(tx_desc);
	}

	if (GNI_MEMHNDL_GET_FLAGS(post_desc->remote_mem_hndl) & GNI_MEMHNDL_FLAG_VMDH) {
		GHAL_TXD_SET_VMDH(tx_desc);
	}

	if (GNI_MEMHNDL_GET_FLAGS(post_desc->local_mem_hndl) & GNI_MEMHNDL_FLAG_VMDH) {
		/* Translate local memory vMDH to real MDH */
		if (unlikely(kgni_dev->vmdh_table[nic_hndl->vmdh_entry_id].descr.blk_size == 0)) {
			GPRINTK_RL(1, KERN_INFO, "vMDH translation required, but not configured");
			return (-EINVAL);
		}
		GHAL_TXD_SET_LOC_MDH(tx_desc,
                                     kgni_translate_mdh(GNI_MEMHNDL_GET_MDH(post_desc->local_mem_hndl),
                                                        kgni_dev->vmdh_table[nic_hndl->vmdh_entry_id].descr.blk_base));
	} else {
		GHAL_TXD_SET_LOC_MDH(tx_desc, GNI_MEMHNDL_GET_MDH(post_desc->local_mem_hndl));
	}

	GHAL_TXD_SET_IRQ(tx_desc);
	GHAL_TXD_SET_DELAYED_IRQ(tx_desc);
	GHAL_TXD_SET_COHERENT(tx_desc);
	GHAL_TXD_SET_PASSPW(tx_desc);
	GHAL_TXD_SET_NAT(tx_desc);
	GHAL_TXD_SET_PE(tx_desc, remote_pe);
	GHAL_TXD_SET_LENGTH(tx_desc, post_desc->length);
	GHAL_TXD_SET_REM_MDH(tx_desc, GNI_MEMHNDL_GET_MDH(post_desc->remote_mem_hndl));
	GHAL_TXD_SET_LOC_PTAG(tx_desc, nic_hndl->ptag);
	GHAL_TXD_SET_REM_PTAG(tx_desc, nic_hndl->ptag);
	GHAL_TXD_SET_LOC_MEMOFF(tx_desc,
				(post_desc->local_addr -
				 GNI_MEMHNDL_GET_VA(post_desc->local_mem_hndl)));
	GHAL_TXD_SET_REM_MEMOFF(tx_desc,
				(post_desc->remote_addr -
				 GNI_MEMHNDL_GET_VA(post_desc->remote_mem_hndl)));

	if (!(post_desc->rdma_mode & GNI_RDMAMODE_GETWC_DIS)) {
		GHAL_TXD_SET_WC(tx_desc, 1);
	}

	atomic_inc(&kgni_dev->resources[nic_hndl->ptag]->rdma.count);
	/* Lock and see if the backlog queue is non-empty */
	if (!list_empty(&bte_txq->pkt_queue)) {
		spin_lock_irqsave(&bte_txq->pkt_lock, pkt_flags);
		if (!list_empty(&bte_txq->pkt_queue)) {
			/* we can not send; queue it */
			queue_elm = kmem_cache_alloc(kgni_rdma_qelm_cache, GFP_ATOMIC);
			if (queue_elm == NULL) {
				spin_unlock_irqrestore(&bte_txq->pkt_lock, pkt_flags);
				atomic_dec(&kgni_dev->resources[nic_hndl->ptag]->rdma.count);
				GPRINTK_RL(1, KERN_DEBUG, "Failed to queue RDMA transaction.");
				return (-ENOMEM);
			}

			queue_elm->tx_desc = tx_desc;
			queue_elm->buf_info.type = KGNI_RDMA;
			queue_elm->buf_info.ptag = nic_hndl->ptag;
			queue_elm->buf_info.vcid = vcid;
			queue_elm->buf_info.rsrc_gen = kgni_dev->resources[nic_hndl->ptag]->generation;
			list_add_tail(&queue_elm->list, &bte_txq->pkt_queue);
			spin_unlock_irqrestore(&bte_txq->pkt_lock, pkt_flags);
			return 0;
		}
		spin_unlock_irqrestore(&bte_txq->pkt_lock, pkt_flags);
	}

	/* Lock and try to grab a ring entry */
	spin_lock_irqsave(&tx_bte_ctrl->tx_lock, tx_flags);
	if (tx_ring->status & KGNI_RING_FULL) {
		spin_unlock_irqrestore(&tx_bte_ctrl->tx_lock, tx_flags);

		tx_bte_ctrl = cur_bte_ctrl;
		tx_ring = &tx_bte_ctrl->tx_ring;
		vc_desc = &tx_bte_ctrl->bte_vc_desc;

		/* Test again, in case our tx_bte_ctrl pointer has
		 * changed. If it was the same as cur_bte_ctrl, it
		 * doesn't hurt since we still would have had an extra
		 * conditional anyways. */
		spin_lock_irqsave(&tx_bte_ctrl->tx_lock, tx_flags);
		if (tx_ring->status & KGNI_RING_FULL) {
			/* we can not send; queue it */
			/* cannot unlock tx_lock yet, chnl could become empty and enqueued packet stranded */
			queue_elm = kmem_cache_alloc(kgni_rdma_qelm_cache, GFP_ATOMIC);
			if (queue_elm == NULL) {
				spin_unlock_irqrestore(&tx_bte_ctrl->tx_lock, tx_flags);
				atomic_dec(&kgni_dev->resources[nic_hndl->ptag]->rdma.count);
				GPRINTK_RL(1, KERN_DEBUG, "Failed to queue RDMA transaction.");
				return (-ENOMEM);
			}

			spin_lock_irqsave(&bte_txq->pkt_lock, pkt_flags);
			queue_elm->tx_desc = tx_desc;
			queue_elm->buf_info.type = KGNI_RDMA;
			queue_elm->buf_info.ptag = nic_hndl->ptag;
			queue_elm->buf_info.vcid = vcid;
			queue_elm->buf_info.rsrc_gen = kgni_dev->resources[nic_hndl->ptag]->generation;
			list_add_tail(&queue_elm->list, &bte_txq->pkt_queue);
			spin_unlock_irqrestore(&bte_txq->pkt_lock, pkt_flags);
			spin_unlock_irqrestore(&tx_bte_ctrl->tx_lock, tx_flags);
			return 0;
		}
	}

	KGNI_NEXT_RING_ENTRY(tx_ring, entry);
	tx_ring->buffer_info[entry].type = KGNI_RDMA;
	tx_ring->buffer_info[entry].ptag = nic_hndl->ptag;
	tx_ring->buffer_info[entry].rsrc_gen = kgni_dev->resources[nic_hndl->ptag]->generation;
	GHAL_VC_WRITE_TXD((*vc_desc), entry, tx_desc);
	GHAL_VC_WRITE_TX_WRINDEX((*vc_desc), tx_ring->next_to_use);
	spin_unlock_irqrestore(&tx_bte_ctrl->tx_lock, tx_flags);
	return 0;
}

/**
 * kgni_ntt_decref - decrement refcount and cleanup NTT
 **/
static void kgni_ntt_decref(kgni_device_t *kgni_dev, kgni_ntt_block_t *ntt_block)
{
	int retval;

	spin_lock(&kgni_dev->ntt_list_lock);
	if (ntt_block) {
		ntt_block->ref_cnt--;
		GASSERT(ntt_block->ref_cnt >= 0);
		if (ntt_block->ref_cnt == 0 && ntt_block->selfdestruct) {
			GDEBUG(1,"Clearing NTT region: base = %d size %d",
			       ntt_block->base, ntt_block->size);

			retval = ghal_ntt_free(kgni_dev->pdev, ntt_block->base, ntt_block->size);
			if (retval){
				GPRINTK_RL(0, KERN_INFO, "ghal_ntt_free() failed with error %d",
				           retval);
			}

			list_del(&ntt_block->dev_list);
			kfree(ntt_block);
		}
	}
	spin_unlock(&kgni_dev->ntt_list_lock);
}

/**
 * kgni_ntt_find_incref - return NTT block entry and increment refcount
 **/
static int kgni_ntt_find_incref(kgni_device_t *kgni_dev, int ntt_base, int ntt_size,
				kgni_ntt_block_t **ntt_block, uint8_t selfdestruct)
{
	struct list_head	*ptr;
	kgni_ntt_block_t	*ntt_ptr;
	int			retval = (-ENOENT);

	spin_lock(&kgni_dev->ntt_list_lock);
	list_for_each(ptr, &kgni_dev->active_ntts) {
		ntt_ptr = list_entry(ptr, kgni_ntt_block_t, dev_list);
		if (ntt_ptr->base == ntt_base && ntt_ptr->size == ntt_size) {
			ntt_ptr->ref_cnt++;
			ntt_ptr->selfdestruct = selfdestruct;
			*ntt_block = ntt_ptr;
			retval = 0;
			/* make sure we do not have more references than the max number of resources */
			GASSERT(ntt_ptr->ref_cnt <= GHAL_PTAG_MAX_NUMBER);

			break;
		}
	}
	spin_unlock(&kgni_dev->ntt_list_lock);

	return retval;
}

/**
 * kgni_ntt_cleanup - cleanup NTT regions
 **/
int kgni_ntt_cleanup(kgni_device_t *kgni_dev, uint32_t base, uint32_t size)
{
	struct list_head	*ptr, *next;
	kgni_ntt_block_t	*ntt_ptr;
	int retval = -EINVAL;

	spin_lock(&kgni_dev->ntt_list_lock);
        if ((base == -1) && list_empty(&kgni_dev->active_ntts)) {   /* for base == -1 wildcard, need to treat
                                                                       case of no NTT entries as okay*/
	        spin_unlock(&kgni_dev->ntt_list_lock);
                return 0;
        }
	list_for_each_safe(ptr, next, &kgni_dev->active_ntts) {
		ntt_ptr = list_entry(ptr, kgni_ntt_block_t, dev_list);
		if ((base != -1) &&
		    (ntt_ptr->base != base ||
		     ntt_ptr->size != size)) {
			continue;
		}
		if (ntt_ptr->ref_cnt != 0) {
			GPRINTK_RL(0, KERN_INFO, "The following NTT block is in use and cannot be freed: ref_cnt = %d base = %d size = %d",
			           ntt_ptr->ref_cnt, ntt_ptr->base, ntt_ptr->size);
			continue;
		}
		retval = 0;
		GDEBUG(1,"Free NTT region: base = %d size = %d",
		       ntt_ptr->base, ntt_ptr->size);
		retval = ghal_ntt_free(kgni_dev->pdev, ntt_ptr->base, ntt_ptr->size);
		if (retval){
			GPRINTK_RL(0, KERN_INFO, "ghal_ntt_free() failed with error %d",
			           retval);
		}

		list_del(&ntt_ptr->dev_list);
		kfree(ntt_ptr);
	}
	spin_unlock(&kgni_dev->ntt_list_lock);

	return retval;
}

/* kgni_ntt_config - configure NTT table */
int kgni_ntt_config(gni_nic_handle_t nic_hndl, gni_nic_nttconfig_args_t *nic_ntt_attr)
{
	kgni_device_t *kgni_dev = (kgni_device_t *) nic_hndl->kdev_hndl;
	kgni_ntt_block_t *ntt_block;
	int	retval = 0;

	if (nic_ntt_attr->flags & GNI_NTT_FLAG_SET_GRANULARITY) {
		GDEBUG(1,"Setting NTT granularity to %d", nic_ntt_attr->granularity);

		retval = ghal_ntt_set_granularity(kgni_dev->pdev, nic_ntt_attr->granularity);
		if (retval) {
			GPRINTK_RL(1, KERN_INFO, "ghal_ntt_set_granularity returned error %d",
			           retval);
			return retval;
		}
	}

	if (nic_ntt_attr->num_entries) {
		if (nic_ntt_attr->flags & GNI_NTT_FLAG_CLEAR) {
			/* clearing existing entries */
			GDEBUG(1,"Clearing NTT region: base = %d size %d",
			       nic_ntt_attr->base, nic_ntt_attr->num_entries);
			retval = kgni_ntt_cleanup(kgni_dev, nic_ntt_attr->base, nic_ntt_attr->num_entries);
			if (retval){
				GPRINTK_RL(1, KERN_INFO, "kgni_ntt_cleanup() failed with error %d",
				           retval);
			}

		} else {
			/* allocating and setting new entries */

			retval = ghal_ntt_alloc(kgni_dev->pdev, nic_ntt_attr->num_entries, &nic_ntt_attr->base);
			if (retval) {
				GPRINTK_RL(1, KERN_INFO, "ghal_ntt_alloc() failed with error %d",
				           retval);
				return retval;
			}
			/* entries_v2 for aries */
			retval = kgni_ntt_set_tbl(kgni_dev->pdev, nic_ntt_attr->base, nic_ntt_attr->num_entries, nic_ntt_attr->entries);
			if (retval) {
				GPRINTK_RL(1, KERN_INFO, "ghal_ntt_set_tbl() failed with error %d",
				           retval);
				ghal_ntt_free(kgni_dev->pdev, nic_ntt_attr->base, nic_ntt_attr->num_entries);
				return retval;
			}
			ntt_block = kzalloc(sizeof(kgni_ntt_block_t), GFP_KERNEL);
			if (ntt_block == NULL) {
				ghal_ntt_free(kgni_dev->pdev, nic_ntt_attr->base, nic_ntt_attr->num_entries);
				return (-ENOMEM);
			}
			ntt_block->base = nic_ntt_attr->base;
			ntt_block->size = nic_ntt_attr->num_entries;
			spin_lock(&kgni_dev->ntt_list_lock);
			list_add(&ntt_block->dev_list,&kgni_dev->active_ntts);
			spin_unlock(&kgni_dev->ntt_list_lock);

			GDEBUG(1,"Allocated and configured NTT region: base = %d size %d",
			       nic_ntt_attr->base, nic_ntt_attr->num_entries);
		}
	}

	return retval;
}

static kgni_resource_info_t *kgni_alloc_rsrc_info(uint8_t ptag) {
	kgni_resource_info_t *rsrcs;
	int retval;

	rsrcs = kzalloc(sizeof(kgni_resource_info_t), GFP_KERNEL);
	if (rsrcs == NULL) {
		return NULL;
	}

	retval = kgni_fma_pool_init(rsrcs, ptag);
	if(retval) {
		kfree(rsrcs);
		return NULL;
	}

	GTRACE(1, "alocrsrc", ((uint64_t)rsrcs), 1);
	return rsrcs;
}

static void kgni_job_resources_init(kgni_resource_info_t *rsrcs)
{
	rsrcs->mdd.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->mrt.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->gart.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->iommu.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->pci_iommu.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->cq.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->fma.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->rdma.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->ce.limit = GHAL_RSRC_NO_LIMIT;
	rsrcs->dla.limit = GHAL_RSRC_NO_LIMIT;
}

static void kgni_free_rsrc_info(kgni_device_t *kdev, kgni_resource_info_t *rsrcs,
				uint8_t ptag) {
	GASSERT(rsrcs);

	kgni_fma_pool_fini(kdev, rsrcs, ptag);

	GTRACE(1, "freersrc", ((uint64_t)rsrcs), 0);
	GKFREE(rsrcs);
}

/**
 * kgni_resource_incref - create resource structure or increment ref. counter
 **/
int kgni_resource_incref(gni_nic_handle_t nic_hndl, uint8_t ptag)
{
	kgni_device_t 		*kgni_dev = (kgni_device_t *) nic_hndl->kdev_hndl;
	unsigned long           flags;
	kgni_resource_info_t	*rsrc_info;
	int			retval = 0;
	uint64_t		job_id = job_getjid(current);

	/* fast path */
	read_lock_irqsave(&kgni_dev->rsrc_lock, flags);
	if (kgni_dev->resources[ptag]) {
		if (nic_hndl->cdm_type != GNI_CDM_TYPE_KERNEL &&
 		    kgni_dev->resources[ptag]->job_id != 0  &&
		    job_id != kgni_dev->resources[ptag]->job_id) {
			GPRINTK_RL(1, KERN_INFO, "job_id 0x%llx configured for ptag %d does not match job_id 0x%llx of this process",
			           kgni_dev->resources[ptag]->job_id, ptag, job_id);
			retval = (-EPERM);
		} else {
			nic_hndl->ptag = ptag;
			nic_hndl->fma_res_id = atomic_inc_return(&kgni_dev->resources[ptag]->ref_cnt);
		}
		if (kgni_dev->resources[ptag]->ntt_block) {
			/* take ntt_base and ntt_size from job resource structure */
			nic_hndl->ntt_base = kgni_dev->resources[ptag]->ntt_block->base;
			nic_hndl->ntt_grp_size = kgni_dev->resources[ptag]->ntt_block->size * ghal_ntt_get_granularity(kgni_dev->pdev);
		}

		GDEBUG(1,"[1]Job %lld ptag %d cookie 0x%x ref.count %d",
		       kgni_dev->resources[ptag]->job_id, ptag, nic_hndl->cookie,
		       atomic_read(&kgni_dev->resources[ptag]->ref_cnt));

		read_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);

		return retval;
	}
	read_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);

#ifndef GNI_UNPROTECTED_OPS
	/* For user level apps. job needs to be configured first */
	if (!kgni_unprotected_ops && nic_hndl->cdm_type != GNI_CDM_TYPE_KERNEL) {
		retval = (-EPERM);
		return retval;
	}
#endif

	rsrc_info = kgni_alloc_rsrc_info(ptag);
	if (rsrc_info == NULL) {
		return (-ENOMEM);
	}

	write_lock_irqsave(&kgni_dev->rsrc_lock, flags);
	if (kgni_dev->resources[ptag]) {
		if (nic_hndl->cdm_type != GNI_CDM_TYPE_KERNEL &&
		   kgni_dev->resources[ptag]->job_id != 0 &&
		   job_id != kgni_dev->resources[ptag]->job_id) {
			GPRINTK_RL(1, KERN_INFO, "job_id 0x%llx configured for ptag %d does not match job_id 0x%llx of this process",
			           kgni_dev->resources[ptag]->job_id, ptag, job_id);
			retval = (-EPERM);
		} else {
			nic_hndl->ptag = ptag;
			nic_hndl->fma_res_id = atomic_inc_return(&kgni_dev->resources[ptag]->ref_cnt);
		}

		kgni_free_rsrc_info(kgni_dev, rsrc_info, ptag);
	} else {
		ghal_ptt_set_entry(kgni_dev->pdev, ptag, GNI_COOKIE_PKEY(nic_hndl->cookie));
		nic_hndl->ptag = ptag;
		kgni_dev->resources[ptag] = rsrc_info;
		atomic_set(&kgni_dev->resources[ptag]->ref_cnt, 1);
		nic_hndl->fma_res_id = 1;
		kgni_dev->resources[ptag]->cookie = nic_hndl->cookie;
		kgni_dev->resources[ptag]->generation = job_generation++;
		kgni_dev->resources[ptag]->vmdh_entry_id = GNI_VMDH_INVALID_ID;

		kgni_job_resources_init(kgni_dev->resources[ptag]);
	}

	if(!retval) {
		GDEBUG(1,"[2]Job %lld ptag %d cookie 0x%x ref.count %d",
		       kgni_dev->resources[ptag]->job_id, ptag, nic_hndl->cookie,
		       atomic_read(&kgni_dev->resources[ptag]->ref_cnt));
	}

	write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);

	return retval;
}

/**
 * kgni_resource_decref - decrement ref. counter and destroy resource structure
 **/
void kgni_resource_decref(gni_nic_handle_t nic_hndl)
{
	kgni_device_t 	*kgni_dev = (kgni_device_t *) nic_hndl->kdev_hndl;
	unsigned long	flags;
	int		mdd_count, mrt_count, gart_count;
	kgni_resource_info_t *rsrcs = NULL;

	if (nic_hndl->ptag == 0) {
		return;
	}

	write_lock_irqsave(&kgni_dev->rsrc_lock, flags);
	if (kgni_dev->resources[nic_hndl->ptag] == NULL) {
		write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
		GPRINTK_RL(0, KERN_ERR, "kgni_dev->resources[ptag] == NULL (ptag = %d)", nic_hndl->ptag);
		GASSERT(0);
	}

	GDEBUG(1,"Job %lld ptag %d cookie 0x%x ref.count %d",
	       kgni_dev->resources[nic_hndl->ptag]->job_id, nic_hndl->ptag, nic_hndl->cookie,
	       atomic_read(&kgni_dev->resources[nic_hndl->ptag]->ref_cnt));

	if(atomic_dec_and_test(&kgni_dev->resources[nic_hndl->ptag]->ref_cnt)) {
		kgni_ntt_decref(kgni_dev, kgni_dev->resources[nic_hndl->ptag]->ntt_block);

		mrt_count = atomic_read(&kgni_dev->resources[nic_hndl->ptag]->mrt.count);
	       	gart_count = atomic_read(&kgni_dev->resources[nic_hndl->ptag]->gart.count);
	       	mdd_count = atomic_read(&kgni_dev->resources[nic_hndl->ptag]->mdd.count);
	        GDEBUG(3,"ptag %d, mrt_count %d, gart_count %d, mdd_count %d", nic_hndl->ptag, mrt_count, gart_count, mdd_count);
		/* resource_decref: verify MDDs, MRTs and GARTs were freed */
		GTRACE(1, "gnirdec1", mrt_count, gart_count);
		GASSERT(mdd_count == 0);
		GASSERT(mrt_count == 0);
		GASSERT(gart_count == 0);
		// TODO: atomic_read, GASSERT(iommu_count == 0);

		ghal_ptt_clear_entry(kgni_dev->pdev, nic_hndl->ptag);

		rsrcs = kgni_dev->resources[nic_hndl->ptag];
		kgni_dev->resources[nic_hndl->ptag] = NULL;

		/* FLBTE counter reset */
		kgni_flbte_tx_reset(kgni_dev, nic_hndl);
	}
	write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);

	if (rsrcs) {
		kgni_free_rsrc_info(kgni_dev, rsrcs, nic_hndl->ptag);
	}
}

/**
 * kgni_pdev_init - Set a pdev in the nic_handle for mem registration
 **/
int kgni_pdev_init(gni_nic_handle_t nic_handle)
{
	kgni_device_t *kgni_dev = (kgni_device_t *)nic_handle->kdev_hndl;
	int err = 0;

	if (nic_handle->modes & GNI_CDM_MODE_USE_PCI_IOMMU) {

		err = ghal_pdev_from_pci_idx(kgni_dev->pdev, GHAL_PCI_IOMMU_DEVICE, &nic_handle->pdev);
		if (err) {
                        GPRINTK_RL(1, KERN_INFO, "PCI IOMMU device is not available.");
			return err;
		}

		if (!ghal_pci_iommu_enabled(nic_handle->pdev)) {
			err = (-ENXIO);
			GPRINTK_RL(1, KERN_INFO, "PCI IOMMU is not enabled.");
			return err;
		}
	} else {
		nic_handle->pdev = kgni_dev->pdev;
	}
	return err;
}

/**
 * kgni_nic_attach - attach NIC instance to CDM
 **/
int kgni_nic_attach(gni_nic_handle_t nic_handle, gni_nic_setattr_args_t	*nic_attr)
{
	kgni_device_t    *kgni_dev = (kgni_device_t *) nic_handle->kdev_hndl;
	uint8_t          mask = GNI_ERRMASK_CRITICAL | GNI_ERRMASK_TRANSACTION |
				GNI_ERRMASK_UNKNOWN_TRANSACTION;
	gni_cdm_handle_t cdm_handle;
	gni_return_t	rc;
	int		i;
	int		nic_cnt = 0;
	int		retval = 0;
	int		retry_lookup = 0;

        if (nic_handle->cdm_id != GNI_CDM_INVALID_ID) {
		GDEBUG(1,"NIC is already attached[pe=0x%x]", nic_handle->nic_pe);
		return (-EEXIST);
	}

	GPRINTK_RL(1, KERN_INFO, "kgni_nic_attach: ptag %lu  cookie %lu", (unsigned long)nic_attr->ptag, (unsigned long)nic_attr->cookie);
	retval = kgni_user_ptag_pkey_check(nic_attr->ptag,
					   GNI_COOKIE_PKEY(nic_attr->cookie));
	if (retval) {
		return retval;
	}

RETRY_LOOKUP:
	cdm_handle = kgni_cdm_lookup(nic_attr->ptag, nic_attr->cookie, nic_attr->rank, NULL);
	if (cdm_handle == NULL) {
		/* Do not have CDM. Create one.*/
		rc = gni_cdm_create(nic_attr->rank, nic_attr->ptag, nic_attr->cookie, nic_attr->modes, &cdm_handle);
		if (rc != GNI_RC_SUCCESS) {
			/* rc is GNI_RC_INVALID_PARAM or GNI_RC_ERROR_NOMEM */
			retval = KGNI_ERROR_FROM_GNI_RETURN_CODE(rc);
			/* retval is -EINVAL, -EEXIST, -ENOMEM, or -EBUSY */
			return retval;
		}
	}

	spin_lock(&cdm_handle->lock);

	/*
	 * Is this CDM still valid?
	 * Prevent a race condition with kgni_nic_detach().
	 */
	if (cdm_handle->type_id != GNI_TYPE_CDM_HANDLE) {
		spin_unlock(&cdm_handle->lock);

		/* Retry CDM lookup/create once more, otherwise return error. */
		if (retry_lookup == 0) {
			retry_lookup++;
			goto RETRY_LOOKUP;
		}

		GDEBUG(1,"CDM type_id is invalid.");
		return (-EINVAL);
	}

	/* Find an empty NIC entry in the CDM. */
	for (i = 0; i < GNI_MAX_NICS_ATTACHED; i++) {
		if (cdm_handle->nic_hndls[i] == NULL) {
			if (nic_handle->cdm_id == GNI_CDM_INVALID_ID) {
				cdm_handle->nic_hndls[i] = nic_handle;
				nic_handle->cdm_id = cdm_handle->my_id;
				nic_handle->my_id = i;
			}
		} else {
			/* Check the CDM for this same NIC. */
			if (cdm_handle->nic_hndls[i]->nic_pe == nic_handle->nic_pe) {
				/*
				 * If a duplicate nic_handle is found, this means that we did
				 * not create the CDM.
				 */

				if (nic_handle->cdm_id != GNI_CDM_INVALID_ID) {
					/* If the nic handle's cdm_id has been set,
					 * then remove the NIC handle from the CDM.
					 */
					cdm_handle->nic_hndls[nic_handle->my_id] = NULL;
					nic_handle->cdm_id = GNI_CDM_INVALID_ID;
				}

				/*
				 * We must not remove the CDM from the cdm_array.
				 * We can just return.
				 */
				GDEBUG(1, "kgni_nic_attach: CDM already has this NIC attached");
				spin_unlock(&cdm_handle->lock);
				return -EEXIST;
			}
		}
	}
	spin_unlock(&cdm_handle->lock);

	if (nic_handle->cdm_id == GNI_CDM_INVALID_ID) {
		GPRINTK_RL(1, KERN_ERR, "kgni_nic_attach: Too many NICs attached");
		retval = (-EMFILE);
		goto resr_error;
	}

	/* to avoid race condition with resource ref. count,
	ptag should be assigned by kgni_resource_incref() */
	nic_handle->inst_id = nic_attr->rank;
	nic_handle->cookie = nic_attr->cookie;
	nic_handle->pkey = GNI_COOKIE_PKEY(nic_attr->cookie);
	nic_handle->modes = nic_attr->modes;

	retval = kgni_resource_incref(nic_handle, nic_attr->ptag);
	if (retval) {
		/* retval is -ENOMEM */
		GPRINTK_RL(1, KERN_ERR, "kgni_resource_incref() failed (%d)", retval);
		goto incref_error;
	}

	/* this must come before any kgni_umap_area calls */
	retval = kgni_mm_init(nic_handle);
	if (retval) {
		if (retval != -ERESTARTSYS) {
			GPRINTK_RL(1, KERN_ERR, "kgni_mm_init() failed (%d)", retval);
		}
		goto mm_error;
	}

	if (nic_handle->cdm_type != GNI_CDM_TYPE_KERNEL) {
		/* setup shared page mapping into user space */
		retval = kgni_umap_area(nic_handle, (uint64_t)virt_to_phys((void *)kgni_dev->rw_page),
					PAGE_SIZE, 0, VM_WRITE, &nic_handle->rw_page_desc, 1);
		if (retval) {
			GPRINTK_RL(1, KERN_ERR, "kgni_umap_area() failed (%d)", retval);
			goto rw_page_error;
		}

		/* This request must be done before any cq creates. */
		retval = kgni_umap_area(nic_handle, (uint64_t)virt_to_phys(kgni_dev->cq_interrupt_mask_page),
					KGNI_CQ_INTERRUPT_MASK_SIZE, 0 , VM_WRITE,
					&nic_handle->cq_interrupt_mask_desc, 1);
		if (retval) {
			GPRINTK_RL(1, KERN_ERR, "kgni_umap_area() cq_interrupt_mask failed (%d)", retval);
			goto cq_interrupt_mask_page_error;
		}
	}

	retval = kgni_pdev_init(nic_handle);
	if (retval) {
		GDEBUG(1,"Failed to setup PCI device in nic_handle");
		goto pdev_init_error;
	}

	/* Allocated outstanding packet array for SM */
	nic_handle->sm_pkts = kzalloc((KGNI_MAX_SM_PKTS+1)*sizeof(void *), GFP_KERNEL);
	if (nic_handle->sm_pkts == NULL) {
		GDEBUG(1,"Failed to allocate buffer for SM packets.");
		retval = -ENOMEM;
		goto smpkt_alloc_error;
	}
	/* Create control channel for SM */
	retval = kgni_chnl_create(nic_handle, cdm_handle->my_id, KGNI_MAX_SM_PKTS, kgni_sm_chnl_rcv,
			     kgni_sm_chnl_event, (kgni_chnl_handle_t *)&nic_handle->sm_chnl_hndl);
	if (retval) {
		/* retval is -EINVAL, -ENOMEM, or -EBUSY */
		GDEBUG(1,"gni_chnl_create failed (%d)",(int)retval);
		goto chnl_error;
	}

	retval = kgni_fma_init(nic_handle);
	if (retval) {
		if (retval != -ENOSPC) {
			GPRINTK_RL(1, KERN_ERR, "kgni_fma_init() failed (%d)", retval);
		}
		goto fma_init_err;
	}

	rc = gni_subscribe_errors(nic_handle,mask,0,NULL,NULL,&nic_handle->err_handle);
	if(rc != GNI_RC_SUCCESS) {
		/* rc is GNI_RC_INVALID_PARAM or GNI_RC_ERROR_NOMEM */
		retval = KGNI_ERROR_FROM_GNI_RETURN_CODE(rc);
		/* retval is -EINVAL or -ENOMEM */
		goto subscribe_error;
	}

	GDEBUG(1,"NIC is attached to CDM[pe=0x%x ptag=%d inst=%d]",
	       nic_handle->nic_pe, nic_handle->ptag, nic_handle->inst_id);

	return retval;

        subscribe_error:
		kgni_fma_fini(nic_handle);
	fma_init_err:
		kgni_chnl_destroy(nic_handle->sm_chnl_hndl);
		nic_handle->sm_chnl_hndl = NULL;
	chnl_error:
		kfree(nic_handle->sm_pkts);
		nic_handle->sm_pkts = NULL;
	smpkt_alloc_error:
	pdev_init_error:
		if (nic_handle->cdm_type != GNI_CDM_TYPE_KERNEL) {
			kgni_unmap_area(current->mm, nic_handle->cq_interrupt_mask_desc);
			nic_handle->cq_interrupt_mask_desc = NULL;
		}
	cq_interrupt_mask_page_error:
		if (nic_handle->cdm_type != GNI_CDM_TYPE_KERNEL) {
			kgni_unmap_area(current->mm, nic_handle->rw_page_desc);
			nic_handle->rw_page_desc = NULL;
		}
	rw_page_error:
		kgni_mm_fini(nic_handle);
	mm_error:
		kgni_resource_decref(nic_handle);
	incref_error:
		spin_lock(&cdm_handle->lock);

		/* disassociate NIC and CDM from each other */
		cdm_handle->nic_hndls[nic_handle->my_id] = NULL;
		nic_handle->cdm_id = GNI_CDM_INVALID_ID;

		/* determine number of NICs still on this CDM */
		for (i = 0; i < GNI_MAX_NICS_ATTACHED; i++) {
			if (cdm_handle->nic_hndls[i] != NULL) {
				nic_cnt++;
			}
		}

		/* destroy CDM if no more NICs */
		if (nic_cnt == 0) {
			cdm_handle->type_id = GNI_TYPE_INVALID;
			spin_unlock(&cdm_handle->lock);
			spin_lock(&kgni_cdm_lock);
			kgni_cdm_array[cdm_handle->my_id] = NULL;
			spin_unlock(&kgni_cdm_lock);
			kfree(cdm_handle);
		} else {
			spin_unlock(&cdm_handle->lock);
		}

	resr_error:
		/* in case user tries to call other commands despite failure */
		nic_handle->ptag = 0;
	return retval;

}


/**
 * kgni_nic_detach - detach NIC instance from CDM
 **/
int kgni_nic_detach(gni_nic_handle_t nic_hndl)
{
	gni_cdm_handle_t 	cdm_handle;
	kgni_chnl_handle_t	chnl_handle;
	int i, nic_cnt = 0;
	int retval = 0;

	if (nic_hndl->cdm_id == GNI_CDM_INVALID_ID) {
		GDEBUG(1,"NIC was not attached.");
		return (-ENXIO);
	}
	GASSERT(nic_hndl->my_id != GNI_NIC_INVALID_ID);

	gni_release_errors(nic_hndl->err_handle);

	cdm_handle = kgni_cdm_array[nic_hndl->cdm_id];
	GASSERT(cdm_handle != NULL);

	spin_lock_bh(&kgni_sm_lock);

	chnl_handle = (kgni_chnl_handle_t)nic_hndl->sm_chnl_hndl;
	if (chnl_handle != NULL) {
		nic_hndl->sm_chnl_hndl = NULL;
		spin_unlock_bh(&kgni_sm_lock);
		kgni_chnl_destroy(chnl_handle);
		spin_lock_bh(&kgni_sm_lock);
	}

	kgni_sm_cleanup(nic_hndl, cdm_handle);

	spin_unlock_bh(&kgni_sm_lock);

	spin_lock(&cdm_handle->lock);

	/* Is this CDM still valid? */
	if (cdm_handle->type_id != GNI_TYPE_CDM_HANDLE) {
		GDEBUG(1,"CDM type_id is invalid.");
		spin_unlock(&cdm_handle->lock);
		return retval;
	}

	/* Break the link between the CDM and the NIC. */
	cdm_handle->nic_hndls[nic_hndl->my_id] = NULL;

	/* Break the link between the NIC and the CDM. */
	nic_hndl->cdm_id = GNI_CDM_INVALID_ID;

	if (nic_hndl->cdm_type != GNI_CDM_TYPE_KERNEL &&
	    nic_hndl->rw_page_desc != NULL) {
		kgni_unmap_area(nic_hndl->mm, nic_hndl->rw_page_desc);
		nic_hndl->rw_page_desc = NULL;
	}

	/* Count the number of NICs attached to the CDM. */
	for (i = 0; i < GNI_MAX_NICS_ATTACHED; i++) {
		if (cdm_handle->nic_hndls[i] != NULL) {
			nic_cnt++;
		}
	}

	/* If no NICs are attached to CDM, then destroy the CDM. */
	if (nic_cnt == 0) {
		/*
		 * Set CDM type_id to invalid to prevent
		 * a race with kgni_nic_attach().
		 */
		cdm_handle->type_id = GNI_TYPE_INVALID;
		spin_unlock(&cdm_handle->lock);

		spin_lock(&kgni_cdm_lock);
		kgni_cdm_array[cdm_handle->my_id] = NULL;
		spin_unlock(&kgni_cdm_lock);

		kfree(cdm_handle);
	} else {
		spin_unlock(&cdm_handle->lock);
	}

	return retval;
}

/**
 * kgni_job_config - configure job parameters
 **/
int kgni_job_config(gni_nic_handle_t nic_hndl, gni_nic_jobconfig_args_t *nic_jobcfg_attr)
{
	kgni_device_t *kgni_dev = (kgni_device_t *) nic_hndl->kdev_hndl;
	unsigned long flags;
	kgni_resource_info_t *rsrc_info;
	int retval = 0;

	retval = kgni_user_ptag_pkey_check(nic_jobcfg_attr->ptag,
					   GNI_COOKIE_PKEY(nic_jobcfg_attr->cookie));
	if (retval) {
		return retval;
	}

	rsrc_info = kgni_alloc_rsrc_info(nic_jobcfg_attr->ptag);
	if (rsrc_info == NULL) {
		return (-ENOMEM);
	}

	write_lock_irqsave(&kgni_dev->rsrc_lock, flags);
	if (kgni_dev->resources[nic_jobcfg_attr->ptag]) {
		kgni_free_rsrc_info(kgni_dev, rsrc_info, nic_jobcfg_attr->ptag);
		if (kgni_dev->resources[nic_jobcfg_attr->ptag]->cookie != nic_jobcfg_attr->cookie) {
			GPRINTK_RL(1, KERN_INFO, "cookie 0x%x configured for ptag %d does not match given cookie 0x%x",
			           kgni_dev->resources[nic_jobcfg_attr->ptag]->cookie, nic_jobcfg_attr->ptag,
			           nic_jobcfg_attr->cookie);
			write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
			return (-EPERM);
		}
		if (kgni_dev->resources[nic_jobcfg_attr->ptag]->job_id != nic_jobcfg_attr->job_id) {
			GPRINTK_RL(1, KERN_INFO, "job_id 0x%llx configured for ptag %d does not match given job_id 0x%llx",
			           kgni_dev->resources[nic_jobcfg_attr->ptag]->job_id, nic_jobcfg_attr->ptag,
			           nic_jobcfg_attr->job_id);
			write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);
			return (-EPERM);
		}
		atomic_inc(&kgni_dev->resources[nic_jobcfg_attr->ptag]->ref_cnt);
	} else {
		ghal_ptt_set_entry(kgni_dev->pdev, nic_jobcfg_attr->ptag, GNI_COOKIE_PKEY(nic_jobcfg_attr->cookie));
		kgni_dev->resources[nic_jobcfg_attr->ptag] = rsrc_info;
		atomic_set(&kgni_dev->resources[nic_jobcfg_attr->ptag]->ref_cnt, 1);
		kgni_dev->resources[nic_jobcfg_attr->ptag]->cookie = nic_jobcfg_attr->cookie;
		kgni_dev->resources[nic_jobcfg_attr->ptag]->job_id  = nic_jobcfg_attr->job_id;
		kgni_dev->resources[nic_jobcfg_attr->ptag]->generation = job_generation++;
		kgni_dev->resources[nic_jobcfg_attr->ptag]->vmdh_entry_id = GNI_VMDH_INVALID_ID;

		kgni_job_resources_init(kgni_dev->resources[nic_jobcfg_attr->ptag]);
	}

	nic_hndl->ptag = nic_jobcfg_attr->ptag;
	nic_hndl->cookie = nic_jobcfg_attr->cookie;

	GDEBUG(1,"Job %lld configured: ptag %d cookie 0x%x ref.count %d",
	       kgni_dev->resources[nic_jobcfg_attr->ptag]->job_id, nic_hndl->ptag, nic_hndl->cookie,
	       atomic_read(&kgni_dev->resources[nic_jobcfg_attr->ptag]->ref_cnt));

	if (nic_jobcfg_attr->limits) {
		kgni_set_limits_locked(nic_hndl, kgni_dev->resources[nic_jobcfg_attr->ptag], nic_jobcfg_attr->limits);
		if (nic_jobcfg_attr->limits->ntt_base != GNI_JOB_INVALID_LIMIT && nic_jobcfg_attr->limits->ntt_size > 0) {
			/* verify an existing NTT configuration */
			if (kgni_dev->resources[nic_jobcfg_attr->ptag]->ntt_block != NULL) {
				if (kgni_dev->resources[nic_jobcfg_attr->ptag]->ntt_block->base != nic_jobcfg_attr->limits->ntt_base ||
				    kgni_dev->resources[nic_jobcfg_attr->ptag]->ntt_block->size != nic_jobcfg_attr->limits->ntt_size) {
					GPRINTK_RL(1, KERN_INFO, "Invalid attempt to re-configure NTT settings (job_id 0x%llx ptag %d)!!!",
					           kgni_dev->resources[nic_jobcfg_attr->ptag]->job_id, nic_jobcfg_attr->ptag );
					retval = (-EINVAL);
				}
			} else {
				/* find configured NTT block and increment refcount  */
				retval = kgni_ntt_find_incref(kgni_dev, nic_jobcfg_attr->limits->ntt_base,
                                                              nic_jobcfg_attr->limits->ntt_size,
							      &kgni_dev->resources[nic_jobcfg_attr->ptag]->ntt_block,
							      (nic_jobcfg_attr->limits->ntt_ctrl & GNI_JOB_CTRL_NTT_CLEANUP));
				if (retval) {
					GPRINTK_RL(1, KERN_INFO, "Invalid NTT parameters (job_id 0x%llx ptag %d): ntt_base = %d ntt_size = %d!!!",
					           kgni_dev->resources[nic_jobcfg_attr->ptag]->job_id, nic_jobcfg_attr->ptag,
					            nic_jobcfg_attr->limits->ntt_base, nic_jobcfg_attr->limits->ntt_size);
				}
			}
		}

	}

	write_unlock_irqrestore(&kgni_dev->rsrc_lock, flags);

	return retval;
}

/**
 * kgni_cdm_lookup - Find CDM descriptor
 **/
gni_cdm_handle_t kgni_cdm_lookup(uint8_t 	ptag,
				 uint32_t 	cookie,
				 uint32_t	inst_id,
				 uint32_t	*index_ptr)
{
	gni_cdm_handle_t cdm_tmp = NULL;
	int		 i;

	spin_lock(&kgni_cdm_lock);
	for (i = 0; i < KGNI_MAX_CDM_NUMBER; i++) {
		if (kgni_cdm_array[i] != NULL &&
		    kgni_cdm_array[i]->type_id == GNI_TYPE_CDM_HANDLE &&
		    kgni_cdm_array[i]->ptag == ptag &&
		    kgni_cdm_array[i]->cookie == cookie && kgni_cdm_array[i]->inst_id == inst_id) {
			cdm_tmp = kgni_cdm_array[i];
			break;
		}
	}
	spin_unlock(&kgni_cdm_lock);

	if (cdm_tmp && index_ptr) {
		*index_ptr = i;
	}
	return cdm_tmp;
}

/**
 * kgni_cdm_lookup_next - Find CDM descriptor based on ptag at given index pointer
 **/
gni_cdm_handle_t kgni_cdm_lookup_next(uint8_t  ptag,
				      uint32_t *index)
{
	gni_cdm_handle_t cdm_tmp = NULL;

	if(!index) return NULL;

	spin_lock(&kgni_cdm_lock);
	while(*index < KGNI_MAX_CDM_NUMBER) {
		if(kgni_cdm_array[*index] && (kgni_cdm_array[*index]->ptag == ptag)) {
			cdm_tmp = kgni_cdm_array[(*index)++];
			break;
		}
		(*index)++;
	}
	spin_unlock(&kgni_cdm_lock);

	return cdm_tmp;
}

/**
 * kgni_cdm_kill_apps - Kill apps associated with this CDM.
 **/
int kgni_cdm_kill_apps(gni_cdm_handle_t cdm_handle, const char *message)
{
	gni_nic_handle_t nic_handle;
	uint32_t nic_index = 0;
	int      flag = 0;

	for(nic_index = 0; nic_index < GNI_MAX_NICS_ATTACHED; nic_index++) {
		if((nic_handle = cdm_handle->nic_hndls[nic_index])) {
			if(nic_handle->cdm_type == GNI_CDM_TYPE_KERNEL) {
				kgni_bte_notify_kern(nic_handle);
			} else {
				if(nic_handle->modes & GNI_CDM_MODE_ERR_NO_KILL)
					continue;
				kgni_kill_app(nic_handle, message, 0, 0);
			}
			flag = 1;
		}
	}

	return flag;
}

/**
 * kgni_cdm_kill_all - Kill apps associated with all active CDMs.
 **/
void kgni_cdm_kill_all(const char *message)
{
	gni_cdm_handle_t cdm_handle;
	int i;

	spin_lock(&kgni_cdm_lock);
	for (i = 0; i < KGNI_MAX_CDM_NUMBER; i++) {
		cdm_handle = kgni_cdm_array[i];
		if (cdm_handle != NULL) {
			spin_unlock(&kgni_cdm_lock);
			kgni_cdm_kill_apps(cdm_handle, message);
			spin_lock(&kgni_cdm_lock);
		}
	}
	spin_unlock(&kgni_cdm_lock);
	return;
}

/**
 * kgni_reqlist_alloc - Initialize a request list for a given cq handle
 **/
int kgni_reqlist_alloc(gni_cq_handle_t cq_hndl)
{
	int i;
	uint32_t nreqs_max;
	int error = 0;
	gnii_req_t **freelist;
	gnii_req_t *base;
	gnii_req_list_t *req_list;
        gni_cdm_handle_t cdm_hndl;

	/* validate the NIC handle */
	if (!cq_hndl || cq_hndl->type_id != GNI_TYPE_CQ_HANDLE) {
		return (-EINVAL);
	}

	/*
	 * req list only attached to cq's used for local(src) cqs, not
	 * for receiving remote CQEs
	 */
	if (cq_hndl->mdh_attached != 0) {
		return (-EINVAL);
	}

	/* allocate the req_list structure */

	req_list = (gnii_req_list_t *) kmalloc(sizeof (gnii_req_list_t), GFP_KERNEL);
	if (req_list == NULL) {
		return (-ENOMEM);
	}

	cdm_hndl = cq_hndl->nic_hndl->cdm_hndl; 

	/* By default, allocate our transfer request list equal in size to the
	 * number of entries in the CQ.  If DUAL_EVENTS is set, allocate 1
	 * transfer request for every 2 CQ event slots. */

	/* The entry count was incremented during CQ creation to fit error events */
	nreqs_max = cq_hndl->entry_count - 2;

	if (cdm_hndl->modes & GNI_CDM_MODE_DUAL_EVENTS) {
		nreqs_max /= 2;
	} 

	/* allocate the pointer array */
	freelist = (gnii_req_t **) vmalloc(nreqs_max * sizeof (gnii_req_t *));
	if (freelist == NULL) {
		error = -ENOMEM;
		goto err_flist_alloc;
	}

	/* allocate space for the requests */
	base = (gnii_req_t *) vmalloc(nreqs_max * sizeof (gnii_req_t));
	if (base == NULL) {
		error = -ENOMEM;
		goto err_base_alloc;
	}

	/* set up the free list */
	for (i = 0; i < nreqs_max; i++) {
		freelist[i] = &base[i];
		base[i].id = i + GNII_REQ_ID_OFFSET;
		base[i].type = GNII_REQ_TYPE_INACTIVE;
		GNII_REQ_CLEAR_DLA_STATE(&base[i]);
	}

	/* initialize req list structures in the nic handle */

	req_list->nreqs_max = nreqs_max;
	req_list->freehead = 0;
	req_list->base = base;
	req_list->freelist = freelist;
	atomic_set(&req_list->ref_cnt, 0);
	cq_hndl->req_list = (void *) req_list;

	return error;

	err_base_alloc:
		vfree(freelist);
	err_flist_alloc:
		kfree(req_list);

	return error;

}

/**
 * kgni_reqlist_free - free request list
 **/
int kgni_reqlist_free(gni_cq_handle_t cq_hndl)
{
	gnii_req_list_t *req_list;

        if (!cq_hndl || cq_hndl->req_list == NULL) {
                return 0;
        }

	/* validate the CQ handle */
	if (cq_hndl->type_id != GNI_TYPE_CQ_HANDLE) {
		return (-EINVAL);
	}

	req_list = (gnii_req_list_t *) cq_hndl->req_list;
	vfree(req_list->freelist);
	vfree(req_list->base);
	kfree(cq_hndl->req_list);

	cq_hndl->req_list = NULL;

	return 0;

}

int kgni_ep_req_cancel(gni_ep_handle_t ep_hndl)
{
	int			i;
	gnii_req_t		*req = NULL;
	gnii_req_list_t		*req_list;
	gni_cq_handle_t		cq_hndl;
	int			reqs = 0;

	gni_list_iterate(cq_hndl, &ep_hndl->nic_hndl->cq_list, nic_list) {
		req_list = (gnii_req_list_t *)cq_hndl->req_list;

		if ((req_list == NULL) || (req_list->freehead == 0))
			continue;

		for (i = 0; i < req_list->nreqs_max; i++) {
			req = &req_list->base[i];
			if (req->type != GNII_REQ_TYPE_INACTIVE && req->ep_hndl == ep_hndl) {
				if (req->cqes_pending & GNI_CQMODE_LOCAL_EVENT) {
					ep_hndl->outstanding_tx_reqs--;
					reqs++;
				}
				if (req->cqes_pending & GNI_CQMODE_GLOBAL_EVENT) {
					ep_hndl->outstanding_tx_reqs--;
					reqs++;
				}

				req->type = GNII_REQ_TYPE_CANCEL;
				req->ep_hndl = NULL;
			}
		}
	}

	if (ep_hndl->outstanding_tx_reqs) {
		GPRINTK_RL(0, KERN_WARNING, "outstanding_tx_reqs out of sync: %d (cancelled %d)",
			      ep_hndl->outstanding_tx_reqs, reqs);
		GASSERT(0);
	}

	return reqs;
}

/**
 * kgni_post_fma_retransmit - Called upon DLA Discard to Re-Post FMA transaction
 **/
gni_return_t
kgni_post_fma_retransmit (
		gni_ep_handle_t              ep_hndl,
		gni_post_descriptor_t        *post_descr,
		gnii_req_t                   *req,
		uint32_t                     block_id,
		uint32_t                     num_dla_blocks)
{
	gni_return_t    status = GNI_RC_SUCCESS;

	GASSERT(GNII_FmaGet(ep_hndl->nic_hndl, ep_hndl->src_cq_hndl, 1, -1) == GNI_RC_SUCCESS);

	/* Update fields of the user's post_descr to reflect correct state. */
	post_descr->next_descr = NULL;
	post_descr->status = GNI_RC_NOT_DONE;

	/* Retransmit FMA operation depending on its type. */
	switch (post_descr->type) {
	case GNI_POST_FMA_PUT:
	case GNI_POST_FMA_PUT_W_SYNCFLAG:
		status = GNII_POST_FMA_PUT(post_descr, ep_hndl, req, block_id, num_dla_blocks);
		break;
	case GNI_POST_FMA_GET:
#if defined CRAY_CONFIG_GHAL_ARIES
        case GNI_POST_FMA_GET_W_FLAG:
#endif
		status = GNII_POST_FMA_GET(post_descr, ep_hndl, req, block_id, num_dla_blocks);
		break;
	case GNI_POST_AMO:
#if defined CRAY_CONFIG_GHAL_ARIES
        case GNI_POST_AMO_W_FLAG:
#endif
		status = GNII_POST_AMO(post_descr, ep_hndl, req, block_id, num_dla_blocks);
		break;
	default:
		break;
	}

	GASSERT(GNII_FmaPut(ep_hndl->nic_hndl, 1) == GNI_RC_SUCCESS);

	return status;
}

static inline void
kgni_rdma_cq_entries(uint64_t *src_cq_data, uint64_t *usr_event_data, uint32_t local, uint32_t remote, uint32_t req_id)
{
	/*
	 * set the src_cq_data field to report local events.
	 * src_cq_data is used to signal local completion event when
	 * BTE finishes initiation of put/get request. On Aries, this
	 * is also the data used to signal global completion. On
	 * Gemini, the usr_event_data is used for global completion.
	 */
	*src_cq_data = kgni_gen_cq_entry(local, req_id);

#if defined CRAY_CONFIG_GHAL_GEMINI
	/*
	 * set the usr_event_data field to report remote_event
	 * value.  usr_event_data is used to signal global
	 * and remote completion of a BTE request on Gemini.
	 */
	*usr_event_data = kgni_gen_cq_entry(remote, req_id);
#else
	/*
	 * set the usr_event_data field to report remote_event value.
	 * usr_event_data is used to signal remote completion of a BTE
	 * request on Aries.
	 */
	*usr_event_data = kgni_gen_rem_cq_entry(remote);
#endif
}

static inline void
kgni_qsce_destroy(gni_nic_handle_t nic_hndl)
{
	kgni_device_t		*kgni_dev = (kgni_device_t *)nic_hndl->kdev_hndl;

	if (!nic_hndl->qsce_func) {
		return;
	}

	spin_lock(&kgni_dev->qsce_lock);
	list_del(&nic_hndl->qsce_list);
	nic_hndl->qsce_func = NULL;
	spin_unlock(&kgni_dev->qsce_lock);
}


/* Kernel Level GNI API */

/**
 * gni_cdm_create - Create Communication Domain
 **/
gni_return_t
	gni_cdm_create(
		IN uint32_t		inst_id,
		IN uint8_t 		ptag,
		IN uint32_t 		cookie,
		IN uint32_t 		modes,
		OUT gni_cdm_handle_t	*cdm_hndl
		)
{
	gni_cdm_handle_t cdm_handle;
	gni_return_t	rc = GNI_RC_SUCCESS;
	int		i;

	if (cdm_hndl == NULL) {
		return GNI_RC_INVALID_PARAM;
	}

	cdm_handle = kzalloc(sizeof(gni_cdm_t), GFP_KERNEL);
	if (cdm_handle == NULL) {
		return GNI_RC_ERROR_NOMEM;
	}

	cdm_handle->type_id = GNI_TYPE_CDM_HANDLE;
	cdm_handle->inst_id = inst_id;
	cdm_handle->ptag = ptag;
	cdm_handle->cookie = cookie;
	cdm_handle->modes = modes;
	cdm_handle->my_id = GNI_CDM_INVALID_ID;
	spin_lock_init(&cdm_handle->lock);
	INIT_LIST_HEAD(&cdm_handle->sessions);

	/* Add to the list */
	spin_lock(&kgni_cdm_lock);
	for (i = 0; i < KGNI_MAX_CDM_NUMBER; i++) {
		if (kgni_cdm_array[i] == NULL && cdm_handle->my_id == GNI_CDM_INVALID_ID) {
			kgni_cdm_array[i] = cdm_handle;
			cdm_handle->my_id = i;
		} else {
			if (kgni_cdm_array[i] != NULL &&
			    kgni_cdm_array[i]->ptag == ptag &&
			    kgni_cdm_array[i]->cookie == cookie &&
			    kgni_cdm_array[i]->inst_id == inst_id) {
				GPRINTK_RL(1, KERN_ERR, "ptag(%d) & cookie(%d) & inst_id(%d) are not unique",
				           ptag, cookie, inst_id);
				rc = GNI_RC_INVALID_STATE;
				break;
		       }
		}
	}

	if (rc == GNI_RC_INVALID_PARAM) {
		if (cdm_handle->my_id != GNI_CDM_INVALID_ID) {
			kgni_cdm_array[cdm_handle->my_id] = NULL;
		}
		kfree(cdm_handle);
		cdm_handle = NULL;
	}

	spin_unlock(&kgni_cdm_lock);
	*cdm_hndl = cdm_handle;
	return rc;
}
EXPORT_SYMBOL(gni_cdm_create);

/**
 * gni_cdm_destroy - Destroy Communication Domain
 **/
gni_return_t
	gni_cdm_destroy(
		IN gni_cdm_handle_t	cdm_hndl
		)
{
	int i;
	kgni_chnl_handle_t	chnl_handle;
	gni_nic_handle_t	tmp_nic_handle;

	if ((cdm_hndl == NULL) || (cdm_hndl->type_id != GNI_TYPE_CDM_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	/*
	 * Remove the CDM handle from the cdm_handle array.
	 * Do this first to prevent some other process from
	 * getting this CDM handle and potentially using a
	 * destroyed, a partially destroyed or freed CDM handle.
	 */
	spin_lock(&kgni_cdm_lock);
	kgni_cdm_array[cdm_hndl->my_id] = NULL;
	spin_unlock(&kgni_cdm_lock);

	spin_lock_bh(&kgni_sm_lock);

	/* Clean up the NIC's channel handles. */
	for (i = 0; i < GNI_MAX_NICS_ATTACHED; i++) {
		if (cdm_hndl->nic_hndls[i] != NULL) {
			if (cdm_hndl->nic_hndls[i]->sm_chnl_hndl) {
				chnl_handle = (kgni_chnl_handle_t)cdm_hndl->nic_hndls[i]->sm_chnl_hndl;
				cdm_hndl->nic_hndls[i]->sm_chnl_hndl = NULL;
				spin_unlock_bh(&kgni_sm_lock);
				kgni_chnl_destroy(chnl_handle);
				spin_lock_bh(&kgni_sm_lock);
			}
			kgni_sm_cleanup(cdm_hndl->nic_hndls[i], cdm_hndl);
		}
	}

	spin_unlock_bh(&kgni_sm_lock);

	/*
	 * The locking of the CDM handle is to prevent the NIC
	 * from being cleared out after we check to see if it
	 * is NULL and before we use it.
	 */
	spin_lock(&cdm_hndl->lock);

	/*
	 * Clean up the CDM's NIC handles.
	 *
	 * The loop above and the following loop are separated
	 * to eliminate the holding of multiple locks in the
	 * same loop.
	 */
	for (i = 0; i < GNI_MAX_NICS_ATTACHED; i++) {
		if (cdm_hndl->nic_hndls[i] != NULL) {
			/* Save a copy of the NIC handle to use in the cleanup functions. */
			tmp_nic_handle = cdm_hndl->nic_hndls[i];

			/* Break the link between the NIC and the CDM. */
			tmp_nic_handle->cdm_id = GNI_CDM_INVALID_ID;

			/*
			 * Break the link between the CDM and the NIC, this
			 * prevents another process from using this NIC handle.
			 */
			cdm_hndl->nic_hndls[i] = NULL;
			spin_unlock(&cdm_hndl->lock);

			kgni_qsce_destroy(tmp_nic_handle);
			kgni_dla_free(tmp_nic_handle);
			kgni_nic_destroy(tmp_nic_handle);

			spin_lock(&cdm_hndl->lock);
		}
	}

	/*
	 * Make the CDM handle type invalid to help prevent the
	 * usage of a stale CDM handle and then destroy the CDM.
	 */
	cdm_hndl->type_id = GNI_TYPE_INVALID;
	spin_unlock(&cdm_hndl->lock);
	kfree(cdm_hndl);

	return GNI_RC_SUCCESS;
}
EXPORT_SYMBOL(gni_cdm_destroy);

/**
 * gni_cdm_attach - Attach CDM to NIC device
 **/
gni_return_t
	gni_cdm_attach(
		IN gni_cdm_handle_t	cdm_hndl,
		IN uint32_t 		device_id,
		OUT uint32_t 		*local_addr,
		OUT gni_nic_handle_t 	*nic_hndl
		)
{
	kgni_device_t 		*kgni_dev;
	gni_nic_handle_t	nic_handle;
	gni_return_t		rc = GNI_RC_ERROR_RESOURCE, retval;
	int 			i, err;

	if (!cdm_hndl || cdm_hndl->type_id != GNI_TYPE_CDM_HANDLE ||
	    !local_addr || !nic_hndl) {
		return GNI_RC_INVALID_PARAM;
	}

	retval = kgni_kern_ptag_pkey_check(cdm_hndl->ptag,
					   GNI_COOKIE_PKEY(cdm_hndl->cookie));
	if (retval != GNI_RC_SUCCESS) {
		return retval;
	}

	/* !!! Does not support device_id = -1 yet.
	 Low priority, as XT6 is going to have only one NIC per SMP */

	kgni_dev = kgni_get_device_by_num(device_id);
	if (kgni_dev == NULL) {
		return GNI_RC_NO_MATCH;
	}

	err = kgni_nic_create(kgni_dev, &nic_handle);
	if (err) {
		/* err is -EINVAL or -ENOMEM */
		GDEBUG(1,"kgni_nic_create() failed (%d)",err);
		return GNI_RETURN_CODE_FROM_KGNI_ERROR(err, "nic_create");
	}

	/* to avoid race condition with resource ref. count,
	ptag should be assigned by kgni_resource_incref() */
	nic_handle->inst_id = cdm_hndl->inst_id;
	nic_handle->cookie = cdm_hndl->cookie;
	nic_handle->pkey = GNI_COOKIE_PKEY(cdm_hndl->cookie);
	nic_handle->modes = cdm_hndl->modes;
	nic_handle->gart_domain_hndl = ghal_get_gart_user_domain(0);
	nic_handle->gart_domain_hndl_kern = ghal_get_gart_kern_domain();
	nic_handle->cdm_hndl = cdm_hndl;
	nic_handle->smsg_dlvr_mode = GNI_DLVMODE_PERFORMANCE;

	err = kgni_resource_incref(nic_handle, cdm_hndl->ptag);
	if (err) {
		/* err is -ENOMEM */
		GDEBUG(1,"kgni_resource_incref() failed (%d)",err);
		return GNI_RETURN_CODE_FROM_KGNI_ERROR(err, "resource_incref");
	}

	spin_lock(&cdm_hndl->lock);
	for (i = 0; i < GNI_MAX_NICS_ATTACHED; i++) {
		if (cdm_hndl->nic_hndls[i] == NULL) {
			cdm_hndl->nic_hndls[i] = nic_handle;
			nic_handle->cdm_id = cdm_hndl->my_id;
			nic_handle->my_id = i;
			break;
		}
	}
	spin_unlock(&cdm_hndl->lock);
	if (i == GNI_MAX_NICS_ATTACHED) {
		goto err_no_nic;
	}

	err = kgni_pdev_init(nic_handle);
	if (err) {
		GDEBUG(1,"Failed to setup PCI device in nic_handle");
		goto err_pdev_init;
	}

	/* Allocated outstanding packet array for SM */
	nic_handle->sm_pkts = kzalloc((KGNI_MAX_SM_PKTS+1)*sizeof(void *), GFP_KERNEL);
	if ( nic_handle->sm_pkts == NULL) {
		rc = GNI_RC_ERROR_NOMEM;
		goto err_no_pkt_array;
	}

	/* Create control channel */
	err = kgni_chnl_create(nic_handle, cdm_hndl->my_id, KGNI_MAX_SM_PKTS, kgni_sm_chnl_rcv,
			     kgni_sm_chnl_event, (kgni_chnl_handle_t *)&nic_handle->sm_chnl_hndl);
	if (err) {
		/* err is -EINVAL, -ENOMEM, or -EBUSY */
		GDEBUG(1,"kgni_chnl_create failed (%d)",(int)err);
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(err, "chnl_create");
		goto err_chnl_create;
	}

	rc = kgni_fma_init(nic_handle);
	if (rc) {
		GPRINTK_RL(1, KERN_ERR, "kgni_fma_init() failed (%d)", rc);
		goto err_fma_init;
	}

	rc = kgni_dla_init(nic_handle);
	if (rc != GNI_RC_SUCCESS) {
		goto err_dla_init;
	}

	*nic_hndl = nic_handle;
	*local_addr = nic_handle->nic_pe;

	return GNI_RC_SUCCESS;

	err_dla_init:
		kgni_fma_fini(nic_handle);
	err_fma_init:
		kgni_chnl_destroy(nic_handle->sm_chnl_hndl);
		nic_handle->sm_chnl_hndl = NULL;
	err_chnl_create:
		kfree(nic_handle->sm_pkts);
	err_no_pkt_array:
	err_pdev_init:
		spin_lock(&cdm_hndl->lock);
		cdm_hndl->nic_hndls[i] = NULL;
		spin_unlock(&cdm_hndl->lock);
	err_no_nic:
		nic_handle->cdm_id = GNI_CDM_INVALID_ID;
		kgni_nic_destroy(nic_handle);

	return rc;
}
EXPORT_SYMBOL(gni_cdm_attach);

/**
 * gni_ep_create - Create logical Endpoint
 **/
gni_return_t
        gni_ep_create(
                IN gni_nic_handle_t     nic_hndl,
                IN gni_cq_handle_t      src_cq_hndl,
                OUT gni_ep_handle_t     *ep_hndl
                )
{
	gni_ep_t *ep;

        if (!nic_hndl || nic_hndl->type_id != GNI_TYPE_NIC_HANDLE) {
                return GNI_RC_INVALID_PARAM;
        }

	if (src_cq_hndl && src_cq_hndl->type_id != GNI_TYPE_CQ_HANDLE) {
		return GNI_RC_INVALID_PARAM;
	}

	ep = (gni_ep_t *) kzalloc(sizeof (gni_ep_t), GFP_KERNEL);
	if (ep == NULL) {
		return GNI_RC_ERROR_NOMEM;
	}

	ep->type_id = GNI_TYPE_EP_HANDLE;
	ep->nic_hndl = nic_hndl;
	ep->remote_pe = GNII_REMOTE_PE_ANY;
	ep->remote_id = GNII_REMOTE_ID_ANY;
	ep->remote_event = ep->remote_id;
	ep->src_cq_hndl = src_cq_hndl;
	if (ep->src_cq_hndl != NULL)
		++ep->src_cq_hndl->ep_ref_count;
	ep->smsg_type = GNI_SMSG_TYPE_INVALID;

	*ep_hndl = ep;

	return GNI_RC_SUCCESS;
}
EXPORT_SYMBOL(gni_ep_create);

/**
 * gni_ep_set_eventdata - Set event data  for local and remote events
 **/
gni_return_t
        gni_ep_set_eventdata(
                IN gni_ep_handle_t      ep_hndl,
                IN uint32_t             local_event,
                IN uint32_t             remote_event
                )
{
	gni_return_t status;

	/*
	 * validate the EP handle
	 */
	if (!ep_hndl || ep_hndl->type_id != GNI_TYPE_EP_HANDLE) {
		status = GNI_RC_INVALID_PARAM;
		goto fn_exit;
	}

	ep_hndl->local_event = local_event;
	ep_hndl->remote_event = remote_event;

	status = GNI_RC_SUCCESS;
      fn_exit:
	return status;
}
EXPORT_SYMBOL(gni_ep_set_eventdata);

/**
 * gni_ep_bind - Bind logical Endpoint to a peer
 **/
gni_return_t
        gni_ep_bind(
                IN gni_ep_handle_t      ep_hndl,
                IN uint32_t             remote_addr,
                IN uint32_t             remote_id
                )
{
	int err;

	if (!ep_hndl || ep_hndl->type_id != GNI_TYPE_EP_HANDLE) {
		return GNI_RC_INVALID_PARAM;
	}

	if (GNI_EP_BOUND(ep_hndl->remote_pe, ep_hndl->remote_id)) {
		GPRINTK_RL(1, KERN_INFO, "EP already bound (remote_pe: %u, remote_id: %u)",
		           ep_hndl->remote_pe, ep_hndl->remote_id);
		return GNI_RC_INVALID_PARAM;
	}

	ep_hndl->remote_pe = remote_addr;
	ep_hndl->remote_id = remote_id;

	/* default settings, may get re-assigned by gni_ep_set_eventdata(ep_hndl) */
	ep_hndl->local_event  = remote_id;
	ep_hndl->remote_event = ep_hndl->nic_hndl->inst_id;

	/*
	 * If this is the first endpoint attached to this nic handle, then
	 * allocate a request cache for the cq hndl
	 */
	if ((ep_hndl->src_cq_hndl != NULL) &&
	    (ep_hndl->src_cq_hndl->req_list == NULL)) {
		err =  kgni_reqlist_alloc(ep_hndl->src_cq_hndl);
		if (err) {
			/* err is -EINVAL or -ENOMEM */
			GPRINTK_RL(0, KERN_INFO, "kgni_reqlist_alloc failed (%d)", err);
			return GNI_RETURN_CODE_FROM_KGNI_ERROR(err, "reqlist_alloc");
		}
	}

	GDEBUG(1,"local:addr=0x%x inst=%d remote:addr=0x%x inst=%d",
	       ep_hndl->nic_hndl->nic_pe, ep_hndl->nic_hndl->inst_id,
	       ep_hndl->remote_pe, ep_hndl->remote_id);

	return GNI_RC_SUCCESS;
}
EXPORT_SYMBOL(gni_ep_bind);

/**
 * gni_ep_unbind - Unbind logical Endpoint
 **/
gni_return_t
        gni_ep_unbind(
                IN gni_ep_handle_t      ep_hndl
                )
{
	if (!ep_hndl || ep_hndl->type_id != GNI_TYPE_EP_HANDLE) {
		return GNI_RC_INVALID_PARAM;
	}

	if ((ep_hndl->outstanding_tx_reqs > 0) || (ep_hndl->pending_datagram)) {
		return GNI_RC_NOT_DONE;
	}

	GDEBUG(1,"local:addr=0x%x inst=%d remote:addr=0x%x inst=%d",
	       ep_hndl->nic_hndl->nic_pe, ep_hndl->nic_hndl->inst_id,
	       ep_hndl->remote_pe, ep_hndl->remote_id);

	ep_hndl->remote_pe = GNII_REMOTE_PE_ANY;
	ep_hndl->remote_id = GNII_REMOTE_ID_ANY;

        /*
         * release short messaging resources
         * user is responsible for unfreeing registered mailbox
         * memory and freeing
         */
        if (ep_hndl->smsg != NULL) {
                kfree(ep_hndl->smsg);
                ep_hndl->smsg = NULL;
        }

	/* remove the ep from the source cqs blocked list */
	GNII_SMSG_REMOVE_BLOCKED_EP(ep_hndl);

	return GNI_RC_SUCCESS;
}
EXPORT_SYMBOL(gni_ep_unbind);

/**
 * gni_ep_destroy - Destroy logical Endpoint
 **/
gni_return_t
        gni_ep_destroy(
                IN gni_ep_handle_t ep_hndl
                )
{
	gni_return_t status = GNI_RC_SUCCESS;

	if (!ep_hndl || ep_hndl->type_id != GNI_TYPE_EP_HANDLE) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Cancel any outstanding requests */
	if (ep_hndl->outstanding_tx_reqs > 0) {
		kgni_ep_req_cancel(ep_hndl);
	}

	ep_hndl->type_id = GNI_TYPE_INVALID;
	if (ep_hndl->smsg != NULL) {
		 kfree(ep_hndl->smsg);
	}

	if (ep_hndl->src_cq_hndl != NULL){
		/* remove the ep from the source cqs blocked list if there */
		GNII_SMSG_REMOVE_BLOCKED_EP(ep_hndl);
		--ep_hndl->src_cq_hndl->ep_ref_count;
	}

	kfree(ep_hndl);

	return status;
}
EXPORT_SYMBOL(gni_ep_destroy);

/**
 * gni_ep_postdata - Exchange datagram with a remote Endpoint
 **/
gni_return_t
        gni_ep_postdata(
                IN gni_ep_handle_t ep_hndl,
                IN void            *in_data,
                IN uint16_t        data_len,
                IN void            *out_buf,
                IN uint16_t        buf_size
                )
{
	int retval;
	gni_ep_postdata_args_t postdata_args;
	gni_return_t rc = GNI_RC_SUCCESS;

	if ((ep_hndl == NULL) || (ep_hndl->type_id != GNI_TYPE_EP_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Open ended sessions must use gni_ep_postdata_w_id */
	if (!GNI_EP_BOUND(ep_hndl->remote_pe, ep_hndl->remote_id)) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Check if there is already a pending datagram for this ep. */
	if (ep_hndl->pending_datagram) {
		return GNI_RC_INVALID_PARAM;
	}
	postdata_args.remote_pe = ep_hndl->remote_pe;
	postdata_args.remote_id = ep_hndl->remote_id;
	postdata_args.in_data = in_data;
	postdata_args.in_data_len = data_len;
	ep_hndl->datagram_out_buf = out_buf;
	ep_hndl->datagram_buf_size = buf_size;
	retval = kgni_sm_add_session(ep_hndl->nic_hndl, &postdata_args);
	if (retval) {
		/* err is -EINVAL, -ENXIO, -E2BIG, -EEXIST, or -ENOMEM */
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_add_session");
	} else {
		ep_hndl->pending_datagram = 1;
	}

	return rc;
}
EXPORT_SYMBOL(gni_ep_postdata);

/**
 * gni_ep_postdata_w_id - Exchange a datagram with an assigned Id with a remote Endpoint
 **/
gni_return_t
        gni_ep_postdata_w_id(
                IN gni_ep_handle_t ep_hndl,
                IN void            *in_data,
                IN uint16_t        data_len,
                IN void            *out_buf,
                IN uint16_t        buf_size,
                IN uint64_t        datagram_id
                )
{
	int retval;
	gni_ep_postdata_args_t postdata_args;
	gni_return_t rc = GNI_RC_SUCCESS;

	if ((ep_hndl == NULL) || (ep_hndl->type_id != GNI_TYPE_EP_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Check if there is already a pending datagram for this ep. */
	if (ep_hndl->pending_datagram) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Check for invalid datagram_id */
	if (datagram_id == -1UL) {
		return GNI_RC_INVALID_PARAM;
	}

	postdata_args.remote_pe = ep_hndl->remote_pe;
	postdata_args.remote_id = ep_hndl->remote_id;
	postdata_args.in_data = in_data;
	postdata_args.in_data_len = data_len;
	postdata_args.use_post_id = 1;
	postdata_args.post_id = datagram_id;
	ep_hndl->datagram_out_buf = out_buf;
	ep_hndl->datagram_buf_size = buf_size;
	retval = kgni_sm_add_session(ep_hndl->nic_hndl, &postdata_args);
	if (retval) {
		/* err is -EINVAL, -ENXIO, -E2BIG, -EEXIST, or -ENOMEM */
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_add_session");
	} else {
		ep_hndl->pending_datagram = 1;
	}

	return rc;
}
EXPORT_SYMBOL(gni_ep_postdata_w_id);

/**
 * gni_ep_postdata_test - Tests for completion of GNI_EpPostData operation
 **/
gni_return_t
        gni_ep_postdata_test(
                IN gni_ep_handle_t      ep_hndl,
                OUT gni_post_state_t    *post_state,
                OUT uint32_t            *remote_addr,
                OUT uint32_t            *remote_id
                )
{
	int retval;
	gni_return_t rc = GNI_RC_SUCCESS;
	gni_ep_postdata_test_args_t postdata_test_args;

	if ((ep_hndl == NULL) || (ep_hndl->type_id != GNI_TYPE_EP_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	if ((ep_hndl->datagram_out_buf == NULL)
	    && (ep_hndl->datagram_buf_size > 0)) {
		return GNI_RC_INVALID_PARAM;
	}

	if ((post_state == NULL) || (remote_addr == NULL)
	    || (remote_id == NULL)) {
                return GNI_RC_INVALID_PARAM;
	}

	/* Open ended sessions must use gni_ep_postdata_test_by_id */
	if (!GNI_EP_BOUND(ep_hndl->remote_pe, ep_hndl->remote_id)) {
		return GNI_RC_INVALID_PARAM;
	}

	postdata_test_args.timeout = 0;
	postdata_test_args.remote_pe = ep_hndl->remote_pe;
	postdata_test_args.remote_id = ep_hndl->remote_id;
	postdata_test_args.out_buff = ep_hndl->datagram_out_buf;
	postdata_test_args.buff_size = ep_hndl->datagram_buf_size;

	retval = kgni_sm_get_state(ep_hndl->nic_hndl, &postdata_test_args);
	if (retval) {
		/* err is -EINVAL, -ENXIO, -E2BIG, -ESRCH, or -ENOMEM */
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_get_state");
		if (rc == GNI_RC_NO_MATCH) {
			ep_hndl->pending_datagram = 0;
		}
	} else {
		*post_state = postdata_test_args.state;
		*remote_addr = postdata_test_args.remote_pe;
		*remote_id = postdata_test_args.remote_id;

		if (postdata_test_args.state == GNI_POST_COMPLETED ||
		    postdata_test_args.state == GNI_POST_TERMINATED ||
		    postdata_test_args.state == GNI_POST_TIMEOUT) {
			ep_hndl->pending_datagram = 0;
		}
	}

	return rc;
}
EXPORT_SYMBOL(gni_ep_postdata_test);

/**
 * gni_ep_postdata_test_by_id - Tests for completion of GNI_EpPostData operation
 *                              with a specified post id.
 **/
gni_return_t
        gni_ep_postdata_test_by_id(
                IN gni_ep_handle_t      ep_hndl,
                IN uint64_t             datagram_id,
                OUT gni_post_state_t    *post_state,
                OUT uint32_t            *remote_addr,
                OUT uint32_t            *remote_id
                )
{
	int retval;
	gni_return_t rc = GNI_RC_SUCCESS;
	gni_ep_postdata_test_args_t postdata_test_args;

	if ((ep_hndl == NULL) || (ep_hndl->type_id != GNI_TYPE_EP_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	if ((ep_hndl->datagram_out_buf == NULL)
	    && (ep_hndl->datagram_buf_size > 0)) {
		return GNI_RC_INVALID_PARAM;
	}

	if ((post_state == NULL) || (remote_addr == NULL)
	    || (remote_id == NULL)) {
                return GNI_RC_INVALID_PARAM;
	}

	postdata_test_args.timeout = 0;
	postdata_test_args.remote_pe = ep_hndl->remote_pe;
	postdata_test_args.remote_id = ep_hndl->remote_id;
	postdata_test_args.out_buff = ep_hndl->datagram_out_buf;
	postdata_test_args.buff_size = ep_hndl->datagram_buf_size;
	postdata_test_args.post_id = datagram_id;
	postdata_test_args.modes = GNI_EP_PDT_MODE_USE_POST_ID;

	retval = kgni_sm_get_state(ep_hndl->nic_hndl, &postdata_test_args);
	if (retval) {
		/* err is -EINVAL, -ENXIO, -E2BIG, -ESRCH, or -ENOMEM */
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_get_state");
		if (rc == GNI_RC_NO_MATCH) {
			ep_hndl->pending_datagram = 0;
		}
	} else {
		*post_state = postdata_test_args.state;
		*remote_addr = postdata_test_args.remote_pe;
		*remote_id = postdata_test_args.remote_id;

		if (postdata_test_args.state == GNI_POST_COMPLETED ||
		    postdata_test_args.state == GNI_POST_TERMINATED ||
		    postdata_test_args.state == GNI_POST_TIMEOUT) {
			ep_hndl->pending_datagram = 0;
		}
	}

	return rc;
}
EXPORT_SYMBOL(gni_ep_postdata_test_by_id);

/**
 * gni_ep_postdata_wait - Wait for the Endpoint to connect
 **/
gni_return_t
        gni_ep_postdata_wait(
                IN gni_ep_handle_t      ep_hndl,
                IN uint32_t             timeout,
                OUT gni_post_state_t    *post_state,
                OUT uint32_t            *remote_addr,
                OUT uint32_t            *remote_id
                )
{
	int retval;
	gni_return_t rc = GNI_RC_SUCCESS;
	gni_ep_postdata_test_args_t postdata_test_args;

	if ((ep_hndl == NULL) || (ep_hndl->type_id != GNI_TYPE_EP_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	if ((ep_hndl->datagram_out_buf == NULL)
	    && (ep_hndl->datagram_buf_size > 0)) {
		return GNI_RC_INVALID_PARAM;
	}

	if ((post_state == NULL) || (remote_addr == NULL)
	    || (remote_id == NULL)) {
                return GNI_RC_INVALID_PARAM;
	}

	/* Open ended sessions must use post id */
	if (!GNI_EP_BOUND(ep_hndl->remote_pe, ep_hndl->remote_id)) {
		return GNI_RC_INVALID_PARAM;
	}

	postdata_test_args.timeout = timeout;
	postdata_test_args.remote_pe = ep_hndl->remote_pe;
	postdata_test_args.remote_id = ep_hndl->remote_id;
	postdata_test_args.out_buff = ep_hndl->datagram_out_buf;
	postdata_test_args.buff_size = ep_hndl->datagram_buf_size;

	retval = kgni_sm_wait_state(ep_hndl->nic_hndl, &postdata_test_args);

	if (likely(retval == 0)) {
		*post_state = postdata_test_args.state;
		*remote_addr = postdata_test_args.remote_pe;
		*remote_id = postdata_test_args.remote_id;
		ep_hndl->pending_datagram = 0;
	} else if (retval == -EAGAIN || retval == -EINTR) {
		rc = GNI_RC_TIMEOUT;
	} else {
		/* err is -EINVAL, -ENXIO, -E2BIG, -ESRCH, or -ENOMEM */
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_wait_state");
		if (rc == GNI_RC_NO_MATCH) {
			ep_hndl->pending_datagram = 0;
		}
	} 

	return rc;
}
EXPORT_SYMBOL(gni_ep_postdata_wait);

/**
 * gni_postdata_probe
 **/
gni_return_t
	gni_postdata_probe(
		IN gni_nic_handle_t nic_hndl,
		OUT uint32_t        *remote_addr,
		OUT uint32_t        *remote_id
		)
{
	int retval;
	gni_ep_postdata_test_args_t postdata_test_args;
	gni_return_t status = GNI_RC_NO_MATCH;

	if ((nic_hndl == NULL) || (nic_hndl->type_id != GNI_TYPE_NIC_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	if ((remote_id == NULL) || (remote_addr == NULL)) {
		return GNI_RC_INVALID_PARAM;
	}

	postdata_test_args.modes = GNI_EP_PDT_MODE_PROBE;

	retval = kgni_sm_get_state(nic_hndl, &postdata_test_args);
	if(retval) {
		/* err is -EINVAL, -ESRCH, -EFAULT */
		GDEBUG(5,"kgni_sm_get_state() failed (%d)", retval);
		status = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_get_state");
	} else {
		*remote_addr = postdata_test_args.remote_pe;
		*remote_id = postdata_test_args.remote_id;
	}

	return status;
}
EXPORT_SYMBOL(gni_postdata_probe);

/**
 * gni_postdata_probe_by_id
 **/
gni_return_t
	gni_postdata_probe_by_id(
		IN gni_nic_handle_t nic_hndl,
		OUT uint64_t        *datagram_id
		)
{
	int retval;
	gni_ep_postdata_test_args_t postdata_test_args;
	gni_return_t status = GNI_RC_SUCCESS;

	if ((nic_hndl == NULL) || (nic_hndl->type_id != GNI_TYPE_NIC_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	if (datagram_id == NULL) {
		return GNI_RC_INVALID_PARAM;
	}

	postdata_test_args.modes = GNI_EP_PDT_MODE_USE_POST_ID | GNI_EP_PDT_MODE_PROBE;

	retval = kgni_sm_get_state(nic_hndl, &postdata_test_args);
	if(retval) {
		/* err is -EINVAL, -ESRCH */
		GDEBUG(5,"kgni_sm_get_state() failed (%d)", retval);
		status = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_get_state");
	} else {
		*datagram_id = postdata_test_args.post_id;
	}

	return status;
}
EXPORT_SYMBOL(gni_postdata_probe_by_id);

/**
 * gni_postdata_probe_wait_by_id
 **/
gni_return_t
	gni_postdata_probe_wait_by_id(
		IN gni_nic_handle_t nic_hndl,
		IN uint32_t         timeout,
		OUT uint64_t        *datagram_id
		)
{
	int retval;
	gni_ep_postdata_test_args_t postdata_test_args;
	gni_return_t status = GNI_RC_SUCCESS;

	if ((nic_hndl == NULL) || (nic_hndl->type_id != GNI_TYPE_NIC_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	if (datagram_id == NULL) {
		return GNI_RC_INVALID_PARAM;
	}

	postdata_test_args.modes = GNI_EP_PDT_MODE_USE_POST_ID | GNI_EP_PDT_MODE_PROBE;
	postdata_test_args.timeout = timeout;

	retval = kgni_sm_wait_state(nic_hndl, &postdata_test_args);

	if (retval != 0) {
		GDEBUG(5,"kgni_sm_wait_state() failed (%d)", retval);
	}

	if (likely(retval == 0)) {
		*datagram_id = postdata_test_args.post_id;
	} else if (retval == -EAGAIN || retval == -EINTR) {
		status = GNI_RC_TIMEOUT;
	} else {
		/* err is -EINVAL, -EFAULT */
		status = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "kgni_sm_wait_state");
	}

	return status;
}
EXPORT_SYMBOL(gni_postdata_probe_wait_by_id);

/**
 * gni_ep_postdata_cancel - Cancels postdata transaction
 **/
gni_return_t
        gni_ep_postdata_cancel (
                IN gni_ep_handle_t ep_hndl
                )
{
	int retval;
	gni_return_t rc = GNI_RC_SUCCESS;
	gni_ep_postdata_term_args_t postdata_term_args;

	if ((ep_hndl == NULL) || (ep_hndl->type_id != GNI_TYPE_EP_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Open ended sessions must use gni_ep_postdata_cancel_by_id */
	if (!GNI_EP_BOUND(ep_hndl->remote_pe, ep_hndl->remote_id)) {
		return GNI_RC_INVALID_PARAM;
	}

	postdata_term_args.remote_pe = ep_hndl->remote_pe;
	postdata_term_args.remote_id = ep_hndl->remote_id;
	postdata_term_args.use_post_id = 0;

	retval = kgni_sm_terminate(ep_hndl->nic_hndl, &postdata_term_args);
	if (retval) {
		/* err is -EINVAL, ENXIO, or -ESRCH */
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_terminate");
	}

	return rc;
}
EXPORT_SYMBOL(gni_ep_postdata_cancel);

/**
 * gni_ep_postdata_cancel_by_id - Cancels postdata transaction with post id
 **/
gni_return_t
        gni_ep_postdata_cancel_by_id (
                IN gni_ep_handle_t ep_hndl,
		IN uint64_t        datagram_id
                )
{
	int retval;
	gni_return_t rc = GNI_RC_SUCCESS;
	gni_ep_postdata_term_args_t postdata_term_args;

	if ((ep_hndl == NULL) || (ep_hndl->type_id != GNI_TYPE_EP_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	postdata_term_args.remote_pe = ep_hndl->remote_pe;
	postdata_term_args.remote_id = ep_hndl->remote_id;
	postdata_term_args.post_id = datagram_id;
	postdata_term_args.use_post_id = 1;

	retval = kgni_sm_terminate(ep_hndl->nic_hndl, &postdata_term_args);
	if (retval) {
		/* err is -EINVAL, ENXIO, or -ESRCH */
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "sm_terminate");
	}

	return rc;
}
EXPORT_SYMBOL(gni_ep_postdata_cancel_by_id);

/**
 * gni_post_rdma - Post RDMA transaction
 **/
gni_return_t
        gni_post_rdma (
                IN gni_ep_handle_t              ep_hndl,
                IN gni_post_descriptor_t        *post_descr
                )
{
	gni_return_t 	rc = GNI_RC_SUCCESS;
	uint64_t	kern_cq_descr;
	uint64_t        src_cq_data = 0;
	uint64_t        usr_event_data = 0;
	gnii_req_t 	*gnii_req = NULL;
	int		retval;
        gni_cdm_handle_t cdm_hndl;

	/* Verify that the endpoint handle is valid. */
	if (unlikely((ep_hndl == NULL) ||
		     (ep_hndl->type_id != GNI_TYPE_EP_HANDLE))) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Verify that this is indeed an RDMA operation. */
	if ((post_descr == NULL) ||
	    ((post_descr->type != GNI_POST_RDMA_PUT) &&
	     (post_descr->type != GNI_POST_RDMA_GET))) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Do not allow the use of both source side event types without 
	 * GNI_CDM_MODE_DUAL_EVENTS set */

	cdm_hndl = ep_hndl->nic_hndl->cdm_hndl; 
	if (unlikely(
	    ((post_descr->cq_mode & GNI_CQMODE_DUAL_EVENTS) ==
	     GNI_CQMODE_DUAL_EVENTS) &&
	    !(cdm_hndl->modes & GNI_CDM_MODE_DUAL_EVENTS))) {
		return GNI_RC_INVALID_PARAM;
	}

	if (likely(post_descr->cq_mode != GNI_CQMODE_SILENT)) {
		/* If cq_mode is unequal 0, a src_cq_handle can have be supplied.
		   If no supplied, use src_cq_hndl on input ep_hndl.  If this
		   is NULL, return GNI_RC_INVALID_PARAM.  */

		if (post_descr->src_cq_hndl == NULL) {
			post_descr->src_cq_hndl = ep_hndl->src_cq_hndl;
		}

		if (post_descr->src_cq_hndl == NULL) {
			return GNI_RC_INVALID_PARAM;
		}

		/* Check if the src_cq_hndl has a request cache, if not allocate one.
		   We need to do this here because it is possible that the src_cq_hndl
		   supplied is not the one associated with the endpoint. */

		if (unlikely(post_descr->src_cq_hndl->req_list == NULL)) {
			if (post_descr->src_cq_hndl->mdh_attached) {
				return GNI_RC_INVALID_PARAM;
			}
			retval = kgni_reqlist_alloc(post_descr->src_cq_hndl);
			if (retval) {
				/* err is -EINVAL or -ENOMEM */
				GPRINTK_RL(1, KERN_INFO, "kgni_reqlist_alloc failed (%d)", retval);
				return GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "reqlist_alloc");
			}
		}

		/* Allocate a GNI internal transfer request in order to keep track
		   of the user's transfer request, or if fail, return with
		   GNI_RC_ERROR_RESOURCE. */
		gnii_req = kgni_req_alloc(post_descr->src_cq_hndl);
		if (gnii_req == NULL) {
			return GNI_RC_ERROR_RESOURCE;
		}
		gnii_req->type = GNII_REQ_TYPE_POST;
		gnii_req->u.post.desc = post_descr;
		gnii_req->u.post.local_event = ep_hndl->local_event;
		gnii_req->ep_hndl = ep_hndl;
		gnii_req->u.post.user_post_id = post_descr->post_id;
		gnii_req->cqes_pending = post_descr->cq_mode &
					 (GNI_CQMODE_LOCAL_EVENT | GNI_CQMODE_GLOBAL_EVENT);

		/* Post id is returned to the user for their own record keeping. */
		post_descr->post_id = gnii_req->id;

		kern_cq_descr = post_descr->src_cq_hndl->hw_cq_hndl;

		kgni_rdma_cq_entries(&src_cq_data, &usr_event_data,
				     ep_hndl->local_event, ep_hndl->remote_event, gnii_req->id);
	} else {
		/* Cq_mode is zero, i.e. no src_cq_handle was supplied and no
		   events will be delivered. */
		kern_cq_descr = GNI_INVALID_CQ_DESCR;
	}

	/* Update fields of the user's post_descr to reflect correct state. */

	post_descr->next_descr = NULL;
	post_descr->status = GNI_RC_NOT_DONE;

	/* Post rdma transfer request to the Gemini Network Service Layer. */

	retval = kgni_rdma_post(ep_hndl->nic_hndl, ep_hndl->remote_pe, post_descr,
				kern_cq_descr, src_cq_data, usr_event_data);
	if (likely(retval == 0)) {
		/* Increment counter for each CQE we expect to receive back. */
		if (post_descr->cq_mode & GNI_CQMODE_LOCAL_EVENT) {
			++ep_hndl->outstanding_tx_reqs;
		}
		if (post_descr->cq_mode & GNI_CQMODE_GLOBAL_EVENT) {
			++ep_hndl->outstanding_tx_reqs;
		}
		return GNI_RC_SUCCESS;
	} else if (retval == -EFAULT) {
		rc = GNI_RC_ALIGNMENT_ERROR;
	} else {
		/* err is -EINVAL, -ENXIO, -EACCES, -ENOSPC, or -ENOMEM */
		rc = GNI_RETURN_CODE_FROM_KGNI_ERROR(retval, "rdma_post");
	}

	kgni_req_free(post_descr->src_cq_hndl, gnii_req);
	return rc;
}
EXPORT_SYMBOL(gni_post_rdma);

/**
 * gni_post_fma - Post FMA transaction
 **/
gni_return_t
        gni_post_fma (
                IN gni_ep_handle_t              ep_hndl,
                IN gni_post_descriptor_t        *post_descr
                )
{
	gni_return_t    status = GNI_RC_SUCCESS;
	gnii_req_t      *req = NULL;
	int             num_dla_blocks = 0;

	/* Verify that the endpoint handle is valid. */
	if (unlikely((ep_hndl == NULL) ||
		     (ep_hndl->type_id != GNI_TYPE_EP_HANDLE) ||
		     (post_descr == NULL))) {
		return GNI_RC_INVALID_PARAM;
	}

	/* GNI_CQMODE_LOCAL_EVENT is invalid for FMA operations */
	if (unlikely(post_descr->cq_mode & GNI_CQMODE_LOCAL_EVENT)) {
		/* User error. */
		GPRINTK_RL(1, KERN_INFO, "User Error: Invalid post_descr cq_mode 0x%x", post_descr->cq_mode);
		return GNI_RC_INVALID_PARAM;
	}

	/* Verify that this is indeed an FMA operation and that
	   data pointers and data length are properly aligned. */

	switch (post_descr->type) {
	case GNI_POST_FMA_PUT:
	case GNI_POST_FMA_PUT_W_SYNCFLAG:
		/* There are no alignment restrictions for FMA Puts. */
		break;
	case GNI_POST_FMA_GET:
#if defined CRAY_CONFIG_GHAL_ARIES
        case GNI_POST_FMA_GET_W_FLAG:
#endif
		if (post_descr->length == 0) {
			return GNI_RC_INVALID_PARAM;
		}
		/* The local and remote addr and length need to be
		   Dword (4 byte) aligned for FMA GETs. */
		if (unlikely((post_descr->local_addr & GNI_LOCAL_ADDR_ALIGNMENT) ||
		    (post_descr->remote_addr & GNI_ADDR_ALIGNMENT) ||
		    (post_descr->length & GNI_ADDR_ALIGNMENT))) {
			return GNI_RC_ALIGNMENT_ERROR;
		}
#if defined CRAY_CONFIG_GHAL_ARIES
                if (post_descr->type == GNI_POST_FMA_GET_W_FLAG) {
                        /* The length of the data cannot be greater than the size of a cacheline
                           minus the size of the flagged response. */
                        if (unlikely(post_descr->length > GNI_FMA_FR_XFER_MAX_SIZE)) {
                                return GNI_RC_INVALID_PARAM;
                        }

                        /* The local address must be 32 bit aligned. */
	                if (unlikely(post_descr->local_addr & GNI_ADDR_ALIGNMENT)) {
		                return GNI_RC_ALIGNMENT_ERROR;
                        }

                        /* The data and the flagged response can not cross a cacheline boundry. */
	                if (unlikely(((post_descr->local_addr & (GNI_CACHELINE_SIZE - 1)) +
                                      post_descr->length + GNI_FMA_FLAGGED_RESPONSE_SIZE) > GNI_CACHELINE_SIZE)) {
		                return GNI_RC_ALIGNMENT_ERROR;
                        }
	        } 
#endif
		break;
	case GNI_POST_AMO:
#if defined CRAY_CONFIG_GHAL_ARIES
        case GNI_POST_AMO_W_FLAG:
#endif
		if (post_descr->length == 0) {
			return GNI_RC_INVALID_PARAM;
		}
		/* AMOs are restricted to 8-byte aligned Qword elements.
		   Alignment of remote memory can be checked here.
		   Alignment of local memory will be checked selectively
		   later since it only applies to GET-type AMOs. */
		if (post_descr->amo_cmd & GNI_FMA_ATOMIC_32_MASK) { 
			/* This is a 32 bit AMO request */
			if (unlikely((post_descr->remote_addr & GNI_AMO_32_ALIGNMENT) ||
			    (post_descr->length & GNI_AMO_32_ALIGNMENT))) {
				return GNI_RC_ALIGNMENT_ERROR;
			}
		} else {
			if (unlikely((post_descr->remote_addr & GNI_AMO_ALIGNMENT) ||
			    (post_descr->length & GNI_AMO_ALIGNMENT))) {
				return GNI_RC_ALIGNMENT_ERROR;
			}
		}
		break;
	default:
		return GNI_RC_INVALID_PARAM;
		break;
	}

	/* Update fields of the user's post_descr to reflect correct state. */
	post_descr->next_descr = NULL;
	post_descr->status = GNI_RC_NOT_DONE;

	/* Keep track of this request within uGNI. */

	if (likely(post_descr->cq_mode != GNI_CQMODE_SILENT)) {
		req = kgni_req_alloc(ep_hndl->src_cq_hndl);
		if (unlikely(req == NULL)) {
			return GNI_RC_ERROR_RESOURCE;
		}
		req->type = GNII_REQ_TYPE_POST;
		req->ep_hndl = ep_hndl;
		req->u.post.desc = post_descr;
		req->u.post.user_post_id = post_descr->post_id;
		req->u.post.local_event = ep_hndl->local_event;
		req->cqes_pending = post_descr->cq_mode & GNI_CQMODE_GLOBAL_EVENT;

		/* allocate dla blocks and credits for AllocSeqid doorbell */
		status = kgni_req_set_dla_credits(req);
		if (unlikely(status != GNI_RC_SUCCESS)) {
			kgni_req_free(ep_hndl->src_cq_hndl, req);
			return status;
		}
		num_dla_blocks = req->num_dla_blocks;

		++ep_hndl->outstanding_tx_reqs;
		/* Post id is returned to the user for their own record keeping. */
		post_descr->post_id = req->id;
	}

	status = GNII_FmaGet(ep_hndl->nic_hndl, ep_hndl->src_cq_hndl, 1, -1);
	if (unlikely(status != GNI_RC_SUCCESS)) {
		if (status == GNI_RC_ERROR_RESOURCE) {
			/* Edge case: no shared FMADs available */
			goto exit;
		} else if (status != GNI_RC_SUCCESS) {
			GNI_ERROR(GNI_ERR_FATAL, "%s: GNII_FmaGet failed with (%d)",
				  __FUNCTION__, status);
		}
	}

	/* Finish operation depending on its type. */

	switch (post_descr->type) {
	case GNI_POST_FMA_PUT:
	case GNI_POST_FMA_PUT_W_SYNCFLAG:
		status = GNII_POST_FMA_PUT(post_descr, ep_hndl, req, 0, num_dla_blocks);
		break;
	case GNI_POST_FMA_GET:
#if defined CRAY_CONFIG_GHAL_ARIES
        case GNI_POST_FMA_GET_W_FLAG:
#endif
		status = GNII_POST_FMA_GET(post_descr, ep_hndl, req, 0, num_dla_blocks);
		break;
	case GNI_POST_AMO:
#if defined CRAY_CONFIG_GHAL_ARIES
        case GNI_POST_AMO_W_FLAG:
#endif
		status = GNII_POST_AMO(post_descr, ep_hndl, req, 0, num_dla_blocks);
		break;
	default:
		break;
	}

	GASSERT(GNII_FmaPut(ep_hndl->nic_hndl, 1) == GNI_RC_SUCCESS);

exit:
	if (unlikely(status != GNI_RC_SUCCESS && req != NULL)) {
		--ep_hndl->outstanding_tx_reqs;
		kgni_req_unset_dla_credits(req);
		kgni_req_free(ep_hndl->src_cq_hndl, req);
	}

	return status;
}
EXPORT_SYMBOL(gni_post_fma);

/**
 * gni_get_completed - Get next completed descriptor
 **/
gni_return_t
        gni_get_completed (
                IN gni_cq_handle_t              cq_hndl,
                IN gni_cq_entry_t               event_data,
                OUT gni_post_descriptor_t       **post_descr
                )
{
	gni_post_descriptor_t *desc;
	int req_id;
	gnii_req_t *gnii_req;
	int err;
	gni_return_t status;

	if (unlikely((cq_hndl == NULL) || (cq_hndl->type_id != GNI_TYPE_CQ_HANDLE))) {
		return GNI_RC_INVALID_PARAM;
	}

	/* Pull the request id out of the event data. */
	req_id = kgni_cqentry_reqid(event_data);

	/* Find the request specified by req_id in the vector of requests kept
	   internally for bookkeeping. If no valid request is found, return
	   an error. */
	err = kgni_req_find(cq_hndl, req_id, &gnii_req);
	if (unlikely(err)) {
		return GNI_RC_INVALID_PARAM;
	}

	if (unlikely(gnii_req->type != GNII_REQ_TYPE_POST)) {
		*post_descr = NULL;
		return GNI_RC_DESCRIPTOR_ERROR;
	}

	/* Retrieve the original post descriptor and check its error status. */
	desc = gnii_req->u.post.desc;
	status = desc->status;
	if (status != GNI_RC_SUCCESS && status != GNI_RC_TRANSACTION_ERROR) {
		status = GNI_RC_DESCRIPTOR_ERROR;
	}

	*post_descr = desc;

	/* If we have gotten all cqes requested for
	   this descriptor, remove the descriptor from
	   the linked list and release the associated
	   request. */
	if (!gnii_req->cqes_pending) {
		kgni_req_free(cq_hndl, gnii_req);	/* free the request */
	}

	return status;
}
EXPORT_SYMBOL(gni_get_completed);

/**
 * gni_get_quiesce_status - Get quiesce status
 */
uint32_t
	gni_get_quiesce_status(
		IN gni_nic_handle_t nic_hndl
		)
{
	kgni_device_t		*kgni_dev;

	if ((nic_hndl == NULL) || (nic_hndl->type_id != GNI_TYPE_NIC_HANDLE)) {
		return 0;
	}

	kgni_dev = (kgni_device_t *)nic_hndl->kdev_hndl;

	return ghal_quiesce_status(kgni_dev->pdev);
}
EXPORT_SYMBOL(gni_get_quiesce_status);

gni_return_t
	gni_set_quiesce_callback(
		IN gni_nic_handle_t nic_hndl,
		IN void (*qsce_func)(gni_nic_handle_t, uint64_t)
		)
{
	struct list_head	*ptr;
	kgni_device_t		*kgni_dev;
	gni_nic_handle_t	nic_handle;

	if ((nic_hndl == NULL) || (nic_hndl->type_id != GNI_TYPE_NIC_HANDLE)) {
		return GNI_RC_INVALID_PARAM;
	}

	kgni_dev = (kgni_device_t *)nic_hndl->kdev_hndl;

	if (!qsce_func) {
		return GNI_RC_INVALID_PARAM;
	}

	if (nic_hndl->qsce_func) {
		return GNI_RC_INVALID_STATE;
	}

	spin_lock(&kgni_dev->qsce_lock);
	list_for_each(ptr, &kgni_dev->qsce_list) {
		nic_handle = list_entry(ptr, struct gni_nic, qsce_list);
		if (nic_handle == nic_hndl) {
			spin_unlock(&kgni_dev->qsce_lock);
			return GNI_RC_INVALID_STATE;
		}
	}

	nic_hndl->qsce_func = qsce_func;
	list_add(&nic_hndl->qsce_list, &kgni_dev->qsce_list);
	spin_unlock(&kgni_dev->qsce_lock);
	return GNI_RC_SUCCESS;
}
EXPORT_SYMBOL(gni_set_quiesce_callback);
