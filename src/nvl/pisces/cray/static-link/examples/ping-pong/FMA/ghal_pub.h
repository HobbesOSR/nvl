/**
 *      The Gemini Hardware Abstraction Layer not kernel specific interface
 *      This file should be included by user level application directly interfacing NIC 
 *
 *      Copyright (C) 2007 Cray Inc. All Rights Reserved.
 *      Written by Igor Gorodetsky <igorodet@cray.com>
 *
 *      This program is free software; you can redistribute it and/or modify it 
 *      under the terms of the GNU General Public License as published by the 
 *      Free Software Foundation; either version 2 of the License, 
 *      or (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful, 
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *      See the GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License 
 *      along with this program; if not, write to the Free Software Foundation, Inc., 
 *      51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 **/

#ifndef _GHAL_PUB_H_
#define _GHAL_PUB_H_

#include "gemini_hw.h"
#include "ghal_err.h"
#include "gni_fence.h"

#ifndef MIN
#define MIN(x, y)   (((x) < (y)) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y)   (((x) > (y)) ? (x) : (y))
#endif

/* sysfs defs */

/* filenames and paths */
#define SYS_CLASS_PATH_STR		"/sys/class"
#define GHAL_CLDEV_CLASS_STR		"gni"
#define GHAL_CLDEV_GEM_CLASS_STR	"gemini"
#define GHAL_CLDEV_KGNI_STR		"kgni"
#define GHAL_CLDEV_GHAL_STR		"ghal"
#define GHAL_CLDEV_RES_FILE_STR		"resources"
#define GHAL_CLDEV_TYPE_FILE_STR	"nic_type"
#define GHAL_CLDEV_VERS_FILE_STR	"version"
#define GHAL_CLDEV_ADDR_FILE_STR	"address"
#define GHAL_CLDEV_PARAM_FILE_STR	"parameters"
#define GHAL_CLDEV_NTT_FILE_STR		"ntt"
#define GHAL_CLDEV_MRT_FILE_STR		"mrt"
#define GHAL_CLDEV_BI_FILE_STR		"bi_config"
#define GHAL_CLDEV_USR_PART_STR		"user_partition"
#define GHAL_CLDEV_PARTITIONS_STR	"partitions"
#define GHAL_CLDEV_PLATFORM_ID_STR	"platform_id"
#define GHAL_CLDEV_FMA_DEF_WIN_FILE_STR	"fma_def_win_sz"
#define GHAL_CLDEV_FMA_SM_WIN_FILE_STR	"fma_sm_win_sz"
#define GHAL_CLDEV_AER_FILE_STR 	"aer"
#define GHAL_CLDEV_BTE_CHNL_FILE_STR	"bte_chnl_proc"
#define GHAL_CLDEV_CMDQ_HARD_FILE_STR	"cmdq_hard_failure"
#define GHAL_CLDEV_CMDQ_SOFT_FILE_STR	"cmdq_soft_failure"
#define GHAL_CLDEV_CONSTRAINTS_FILE_STR	"constraints"
#define GHAL_CLDEV_CQ_IRQS_FILE_STR 	"cq_irqs"
#define GHAL_CLDEV_CQ_LIST_FILE_STR	"cq_list"
#define GHAL_CLDEV_DEBUG_PTE_FILE_STR	"debug_pt_entries"
#define GHAL_CLDEV_DEV_FILE_STR 	"dev"
#define GHAL_CLDEV_DLA_CONF_FILE_STR	"dla_config"
#define GHAL_CLDEV_DLA_FILL_FILE_STR	"dla_fill"
#define GHAL_CLDEV_FIXUP_MASK_FILE_STR	"fixup_mask"
#define GHAL_CLDEV_FLOWCTRL_FILE_STR	"flowctrl"
#define GHAL_CLDEV_IOMMU_LOGR_FILE_STR	"iommu_logr"
#define GHAL_CLDEV_IOMMU_RSCR_FILE_STR	"iommu_resources"
#define GHAL_CLDEV_KERN_CQ_IRQS_FILE_STR "kern_cq_irqs"
#define GHAL_CLDEV_MDD_REG_FILE_STR	"mdd_registrations"
#define GHAL_CLDEV_NUMA_MAP_FILE_STR	"numa_map"
#define GHAL_CLDEV_PROC_ERRORS_FILE_STR	"process_errors"
#define GHAL_CLDEV_PTAG_RANGE_FILE_STR	"ptag_range"
#define GHAL_CLDEV_QUIESCE_FILE_STR	"quiesce"
#define GHAL_CLDEV_STATS_FILE_STR	"stats"
#define GHAL_CLDEV_SUBSYS_DBG_FILE_STR	"subsys_debug"
#define GHAL_CLDEV_UEVENT_FILE_STR	"uevent"
#define GHAL_CLDEV_UNPROT_OPS_FILE_STR	"unprotected_ops"
#define GHAL_CLDEV_USER_CQ_IRQS_FILE_STR "user_cq_irqs"

/* other strings */
#define GHAL_CLDEV_MDD_STR		"MDD"
#define GHAL_CLDEV_MRT_STR		"MRT"
#define GHAL_CLDEV_IOMMU_STR		"IOMMU"
#define GHAL_CLDEV_GART_STR		"GART"
#define GHAL_CLDEV_CQ_STR		"CQ"
#define GHAL_CLDEV_FMA_STR		"FMA"
#define GHAL_CLDEV_RDMA_STR		"RDMA"
#define GHAL_CLDEV_CE_STR		"CE"
#define GHAL_CLDEV_DLA_STR		"DLA"
#define GHAL_CLDEV_SFMA_STR		"SFMA"
#define GHAL_CLDEV_DIRECT_STR		"DIRECT"
#define GHAL_CLDEV_PCI_IOMMU_STR	"PCI-IOMMU"
#define GHAL_CLDEV_NONVMDH_STR		"non-VMDH"
#define GHAL_CLDEV_NA_STR		"N/A"
#define GHAL_CLDEV_BTE_STR		"BTE"
#define GHAL_CLDEV_NTT_STR		"NTT"
#define GHAL_CLDEV_VMDH_STR		"VMDH"
#define GHAL_CLDEV_TCR_STR		"TCR"
#define GHAL_CLDEV_DVA_STR		"DVA"

#define GHAL_CLDEV_MDD_AVAILABLE	1
#define GHAL_CLDEV_MRT_AVAILABLE	1
#define GHAL_CLDEV_IOMMU_AVAILABLE	0
#define GHAL_CLDEV_GART_AVAILABLE	1
#define GHAL_CLDEV_CQ_AVAILABLE		1
#define GHAL_CLDEV_FMA_AVAILABLE	1
#define GHAL_CLDEV_RDMA_AVAILABLE	1
#define GHAL_CLDEV_CE_AVAILABLE		0
#define GHAL_CLDEV_DLA_AVAILABLE	0
#define GHAL_CLDEV_SFMA_AVAILABLE	1
#define GHAL_CLDEV_DIRECT_AVAILABLE	1
#define GHAL_CLDEV_PCI_IOMMU_AVAILABLE	0
#define GHAL_CLDEV_NONVMDH_AVAILABLE	0
#define GHAL_CLDEV_TCR_AVAILABLE	1
#define GHAL_CLDEV_DVA_AVAILABLE	1

/* GNI default sysfs class path */
#define GHAL_CLDEV_CLASS_PATH		SYS_CLASS_PATH_STR "/" GHAL_CLDEV_CLASS_STR

/* GNI device class directory format strings */
#define GHAL_CLDEV_KGNI_FMT		GHAL_CLDEV_KGNI_STR "%d"
#define GHAL_CLDEV_GHAL_FMT		GHAL_CLDEV_GHAL_STR "%d"
#define GHAL_CLDEV_KGNI_PATH_FMT	GHAL_CLDEV_CLASS_PATH "/" GHAL_CLDEV_KGNI_FMT
#define GHAL_CLDEV_GHAL_PATH_FMT	GHAL_CLDEV_CLASS_PATH "/" GHAL_CLDEV_GHAL_FMT

/* GNI device class files */
#define GHAL_CLDEV_JOBRES_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_RES_FILE_STR
#define GHAL_CLDEV_FMA_DEF_WIN_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_FMA_DEF_WIN_FILE_STR
#define GHAL_CLDEV_FMA_SM_WIN_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_FMA_SM_WIN_FILE_STR
#define GHAL_CLDEV_DEVRES_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_RES_FILE_STR
#define GHAL_CLDEV_NICTYPE_PATH		GHAL_CLDEV_CLASS_PATH "/" GHAL_CLDEV_TYPE_FILE_STR
#define GHAL_CLDEV_VERS_PATH		GHAL_CLDEV_CLASS_PATH "/" GHAL_CLDEV_VERS_FILE_STR
#define GHAL_CLDEV_ADDR_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_ADDR_FILE_STR
#define GHAL_CLDEV_PARAM_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_PARAM_FILE_STR
#define GHAL_CLDEV_NTT_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_NTT_FILE_STR
#define GHAL_CLDEV_MRT_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_MRT_FILE_STR
#define GHAL_CLDEV_BI_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_BI_FILE_STR
#define GHAL_CLDEV_USR_PART_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_USR_PART_STR
#define GHAL_CLDEV_PARTITIONS_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_PARTITIONS_STR
#define GHAL_CLDEV_PLATFORM_ID_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_PLATFORM_ID_STR
#define GHAL_CLDEV_AER_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_AER_FILE_STR
#define GHAL_CLDEV_BTE_CHNL_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_BTE_CHNL_FILE_STR
#define GHAL_CLDEV_CMDQ_HARD_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_CMDQ_HARD_FILE_STR
#define GHAL_CLDEV_CMDQ_SOFT_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_CMDQ_SOFT_FILE_STR
#define GHAL_CLDEV_CONSTRAINTS_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_CONSTRAINTS_FILE_STR
#define GHAL_CLDEV_CQLIST_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_CQ_LIST_FILE_STR
#define GHAL_CLDEV_CQ_IRQS_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_CQ_IRQS_FILE_STR
#define GHAL_CLDEV_DEBUG_PTE_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_DEBUG_PTE_FILE_STR
#define GHAL_CLDEV_DLA_FILL_FMT 	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_DLA_FILL_FILE_STR
#define GHAL_CLDEV_FIXUP_MASK_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_FIXUP_MASK_FILE_STR
#define GHAL_CLDEV_FLOWCTRL_FMT 	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_FLOWCTRL_FILE_STR
#define GHAL_CLDEV_GHAL_DEV_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_DEV_FILE_STR
#define GHAL_CLDEV_GHAL_DLA_CONF_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_DLA_CONF_FILE_STR
#define GHAL_CLDEV_GHAL_SUBS_DBG_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_SUBSYS_DBG_FILE_STR
#define GHAL_CLDEV_GHAL_UEVENT_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_UEVENT_FILE_STR
#define GHAL_CLDEV_GHAL_VERS_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_VERS_FILE_STR
#define GHAL_CLDEV_IOMMU_LOGR_FMT 	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_IOMMU_LOGR_FILE_STR
#define GHAL_CLDEV_IOMMU_RSCR_FMT 	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_IOMMU_RSCR_FILE_STR
#define GHAL_CLDEV_KGNI_DEV_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_DEV_FILE_STR
#define GHAL_CLDEV_KGNI_DLA_CONF_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_DLA_CONF_FILE_STR
#define GHAL_CLDEV_KGNI_NTT_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_NTT_FILE_STR
#define GHAL_CLDEV_KGNI_SUBS_DBG_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_SUBSYS_DBG_FILE_STR
#define GHAL_CLDEV_KGNI_UEVENT_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_UEVENT_FILE_STR
#define GHAL_CLDEV_KGNI_VERS_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_VERS_FILE_STR
#define GHAL_CLDEV_KERN_CQ_IRQS_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_KERN_CQ_IRQS_FILE_STR
#define GHAL_CLDEV_MDDREG_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_MDD_REG_FILE_STR
#define GHAL_CLDEV_NUMAMAP_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_NUMA_MAP_FILE_STR
#define GHAL_CLDEV_PROC_ERRORS_FMT	GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_PROC_ERRORS_FILE_STR
#define GHAL_CLDEV_PTAG_RANGE_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_PTAG_RANGE_FILE_STR
#define GHAL_CLDEV_QUIESCE_FMT 		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_QUIESCE_FILE_STR
#define GHAL_CLDEV_STATS_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_STATS_FILE_STR
#define GHAL_CLDEV_UNPROT_OPS_FMT 	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_UNPROT_OPS_FILE_STR
#define GHAL_CLDEV_USER_CQ_IRQS_FMT	GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_USER_CQ_IRQS_FILE_STR

#ifdef __KERNEL__

#define GHAL_PCI_IOMMU_HPAGE            0

extern uint64_t cgm_gart;

static inline void
memcpy64_fromio (void *to, const volatile void __iomem * from, size_t bytes)
{
  while (bytes)
    {
      *(uint64_t *) to = *(const volatile uint64_t *) from;
      to = (uint64_t *) to + 1;
      from = (const volatile uint64_t *) from + 1;
      bytes -= sizeof (uint64_t);
    }
}

static inline void
memcpy64_toio (volatile void __iomem * to, const void *from, size_t bytes)
{
  while (bytes)
    {
      *(volatile uint64_t *) to = *(const uint64_t *) from;
      to = (volatile uint64_t *) to + 1;
      from = (const uint64_t *) from + 1;
      bytes -= sizeof (uint64_t);
    }
}

/* Write WrIdx index to RX channel descriptor */
#define GHAL_VC_WRITE_RX_WRINDEX(vc_desc, index) \
	do { \
	sfence(); \
	writeq(index, &(((volatile ghal_rxvc_desc_t *)vc_desc.rx_ctl_desc)->write_index));\
	} while(0)

/* Write WrIdx index to TX channel descriptor */
#define GHAL_VC_WRITE_TX_WRINDEX(vc_desc, index) \
	do { \
	sfence(); \
	writeq(index, &(((volatile ghal_txvc_desc_t *)vc_desc.tx_ctl_desc)->write_index));\
	} while(0)

#define GHAL_RX_DESC(vc_desc, id)       ((volatile ghal_rx_desc_t __iomem *)(vc_desc.rx_desc + GHAL_BTE_RX_OFF_BYID(id)))
#define GHAL_TX_DESC(vc_desc, id)       ((volatile ghal_tx_desc_t __iomem *)(vc_desc.tx_desc + GHAL_BTE_TX_OFF_BYID(id)))
#define GHAL_RX_CPU_DESC(vc_desc, id)   ((ghal_cpurx_desc_t *)(vc_desc.cpu_rx_desc + GHAL_BTE_CPURX_OFF_BYID(id)))
#define GHAL_TX_CPU_DESC(vc_desc, id)   ((ghal_tx_desc_t *)(vc_desc.cpu_tx_desc + GHAL_BTE_CPUTX_OFF_BYID(id)))

/* BTE TX Descriptor */
/* Reset TX descriptor fields */
#define GHAL_TXD_INIT {0}
/* Set modes in 1st QWORD */
#define GHAL_TXD_SET_MODE1(desc, value) (desc).mode1 |= value
/* Set modes in 2nd QWORD */
#define GHAL_TXD_SET_MODE2(desc, value) (desc).mode2 |= value
/* Set modes in 4th QWORD */
#define GHAL_TXD_SET_MODE4(desc, value) (desc).nat_enb.mode4 |= value
/* BTE Options */
#define GHAL_TXD_SET_BTE_OP(desc, value) GHAL_TXD_SET_MODE1(desc, value)
#define GHAL_TXD_SET_CQ_GLOBAL(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_CQ_GLOBAL)
#define GHAL_TXD_SET_CQ_LOCAL(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_CQ_LOCAL)
#define GHAL_TXD_SET_NAT(desc) GHAL_TXD_SET_MODE4(desc, GHAL_TXD_MODE_NAT_ENABLE)
#define GHAL_TXD_SET_NTT(desc)
#define GHAL_TXD_SET_TX_CONCAT(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_BTE_CONCATENATE)
#define GHAL_TXD_SET_IMM(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_BTE_IMMED)
#define GHAL_TXD_SET_FENCE(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_BTE_FENCE)
#define GHAL_TXD_SET_IRQ(desc) GHAL_TXD_SET_MODE2(desc, GHAL_TXD_MODE_IRQ_ENABLE)
#define GHAL_TXD_SET_DELAYED_IRQ(desc) GHAL_TXD_SET_MODE2(desc, GHAL_TXD_MODE_IRQ_DELAY_ENABLE)
#define GHAL_TXD_SET_COHERENT(desc) GHAL_TXD_SET_MODE4(desc, GHAL_TXD_MODE_COHERENT)
#define GHAL_TXD_SET_PASSPW(desc) GHAL_TXD_SET_MODE4(desc, GHAL_TXD_MODE_NP_PASSPW)
#define GHAL_TXD_SET_OPT_DELIV(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_OPT_DELIVERY)
#define GHAL_TXD_SET_ADP(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_ADP_ENABLE)
#define GHAL_TXD_SET_RADP(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_RADP_ENABLE)
#define GHAL_TXD_SET_HASH(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_HASH_ENABLE)
#define GHAL_TXD_SET_VMDH(desc) GHAL_TXD_SET_MODE1(desc, GHAL_TXD_MODE_VMDH_ENABLE)
/* Set Destination PE value */
#define GHAL_TXD_SET_PE(desc, value) desc.dest_pe = value
/* Set Src. data length */
#define GHAL_TXD_SET_LENGTH(desc, value) desc.data_len = value
/* Set Src. Physical Address, when NAT is disabled */
#define GHAL_TXD_SET_LOC_PHADDR(desc, value) desc.nat_dsb.src_ph_addr = value
/* Set Src. Memory Offset, when NAT is enabled */
#define GHAL_TXD_SET_LOC_MEMOFF(desc, value) desc.nat_enb.src_mem_off = value
/* Set Src. Memory Domain Handle */
#define GHAL_TXD_SET_LOC_MDH(desc, value) desc.nat_enb.src_mem_dh = value
/* Set Src. CQ handle */
#define GHAL_TXD_SET_SRC_CQ_BS(desc, cq_hndl) desc.src_cq_hndl = cq_hndl->hw_cq_hndl
#define GHAL_TXD_SET_SRC_CQ(desc, hw_cq_hndl) desc.src_cq_hndl = hw_cq_hndl
/* Set UserData */
#define GHAL_TXD_SET_USRDATA(desc, value) desc.user_data = value
/* Set SrcCqData */
#define GHAL_TXD_SET_SRCDATA(desc, value) desc.src_cq_data = value
/* Set CQ events */
#define GHAL_TXD_SET_CQE_LOCAL(desc, value) desc.src_cq_data = value
#define GHAL_TXD_SET_CQE_GLOBAL(desc, value_ari, value_gem) desc.user_data = value_gem
#define GHAL_TXD_SET_CQE_REMOTE(desc, value) desc.user_data = value
/* Set Src. Protection Tag */
#define GHAL_TXD_SET_LOC_PTAG(desc, value) desc.src_ptag = value
/* Set Dest. Memory Domain Handle */
#define GHAL_TXD_SET_REM_MDH(desc, value) desc.dest_mem_dh = value
/* Set Dest. Protection Tag */
#define GHAL_TXD_SET_REM_PTAG(desc, value) desc.dest_ptag = value
/* Set Dest. Memory Offset */
#define GHAL_TXD_SET_REM_MEMOFF(desc, value) desc.dest_mem_off = value
#define GHAL_TXD_SET_WC(desc, value)

/* Write TX descriptor to the Gemini */
#define GHAL_VC_WRITE_TXD(vc_desc, index, desc) memcpy64_toio(GHAL_TX_DESC(vc_desc, index), &desc, sizeof(desc))
/* Check TX descriptor completion */
#define GHAL_VC_IS_TXD_COMPLETE(vc_desc, index) (GHAL_TX_CPU_DESC(vc_desc, index)->flags & GHAL_TXD_FLAG_COMPLETE)
/* Check TX descriptor timeout error */
#define GHAL_VC_IS_TXD_TIMEOUT(vc_desc, index) (GHAL_TX_CPU_DESC(vc_desc, index)->flags & GHAL_TXD_FLAG_TIMEOUT_ERR)
/* Get TX error status */
#define GHAL_VC_GET_TXD_ERROR(vc_desc, index) GHAL_TX_CPU_DESC(vc_desc, index)->bte_tx_status

/* Clear TX descriptor completeion bit and timeout error flag*/
#define GHAL_VC_CLEAR_TXD_COMPLETE(vc_desc, index) GHAL_TX_CPU_DESC(vc_desc, index)->flags = 0

/* Read SSID_IN_USE counter*/
#define GHAL_VC_TXC_READ_SSID_INUSE(vc_desc, ssid_in_use) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		tmp_txvc.qword3 = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qword3));\
		ssid_in_use = tmp_txvc.ssid_in_use;\
		} while (0)

/* Get TX ring indices */
#define GHAL_VC_TXC_GET_RW_IDX(vc_desc, rd_idx, wr_idx) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		sfence();\
		tmp_txvc.qword3 = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qword3));\
		tmp_txvc.write_index = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->write_index));\
		rd_idx = tmp_txvc.read_index; \
		wr_idx = tmp_txvc.write_index; \
		} while (0)

/* Enable TX channel */
#define GHAL_VC_TXC_ENABLE(vc_desc, irq_en) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		sfence();\
		tmp_txvc.qword1 = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qword1));\
		tmp_txvc.flags = GHAL_TXVC_FLAG_ENABLE | (irq_en ? GHAL_TXVC_FLAG_IRQ_ENABLE:0);\
		writeq(tmp_txvc.qword1, &(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qword1));\
		} while (0)
/* Disable TX channel */
#define GHAL_VC_TXC_DISABLE(vc_desc) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		sfence();\
		tmp_txvc.qword1 = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qword1));\
		tmp_txvc.flags = GHAL_TXVC_FLAG_DISABLE;\
		writeq(tmp_txvc.qword1, &(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qword1));\
		} while (0)

/* Reset TX channel */
#define GHAL_VC_TXC_RESET(vc_desc) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		sfence();\
		tmp_txvc.qword1 = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qword1));\
		tmp_txvc.flags = GHAL_TXVC_FLAG_RESET;\
		writeq(tmp_txvc.qword1, &(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qword1));\
		} while (0)

/* BTE RX Descriptor */
/* Reset TX descriptor fields */
#define GHAL_RXD_INIT {0}
/* Set RX descriptor modes */
#define GHAL_RXD_SET_MODE(desc, value) desc.mode1 |= value
#define GHAL_RXD_SET_PASSPW(desc) GHAL_RXD_SET_MODE(desc,GHAL_RXD_MODE_PASSPW)
#define GHAL_RXD_SET_COHERENT(desc) GHAL_RXD_SET_MODE(desc,GHAL_RXD_MODE_COHERENT)
#define GHAL_RXD_SET_IRQ_DELAY(desc) GHAL_RXD_SET_MODE(desc,GHAL_RXD_MODE_IRQ_DELAY_ENABLE)
#define GHAL_RXD_SET_IRQ(desc) GHAL_RXD_SET_MODE(desc,GHAL_RXD_MODE_IRQ_ENABLE)
/* Set Buffer Physical Address */
#define GHAL_RXD_SET_BUFF_ADDR(desc, value) desc.base_addr = value
/* Write RX descriptor to the Gemini */
#define GHAL_VC_WRITE_RXD(vc_desc, index, desc)   memcpy64_toio(GHAL_RX_DESC(vc_desc, index), &desc, sizeof(desc))
/* Clear RX descriptor status flags */
#define GHAL_VC_CLEAR_RXD_STATUS(vc_desc, index) GHAL_RX_CPU_DESC(vc_desc, index)->flags = 0
/* Check RX descriptor completion */
#define GHAL_VC_IS_RXD_COMPLETE(vc_desc, index) (GHAL_RX_CPU_DESC(vc_desc, index)->flags & GHAL_RXD_FLAG_BTE_COMPLETE)
#define GHAL_VC_IS_RXD_COMPLETE_SET(status) (status & GHAL_RXD_FLAG_BTE_COMPLETE)
/* Get RX descriptor status flags */
#define GHAL_VC_GET_RXD_STATUS(vc_desc, index)  (GHAL_RX_CPU_DESC(vc_desc, index)->flags)
/* Get RX descriptor received length */
#define GHAL_VC_GET_RXD_LENGTH(vc_desc, index)  (GHAL_RX_CPU_DESC(vc_desc, index)->data_len)
/* Check for errors */
#define GHAL_VC_IS_RXD_ERROR(vc_desc, index)  (GHAL_RX_CPU_DESC(vc_desc, index)->flags & GHAL_RXD_FLAG_ERR_STATUS_MASK)
#define GHAL_VC_IS_RXD_ERROR_SET(status)  ((status) & GHAL_RXD_FLAG_ERR_STATUS_MASK)
#define GHAL_VC_RX_ERR_STATUS(status)     (((status) & GHAL_RXD_FLAG_ERR_STATUS_MASK) >> GHAL_RXD_FLAG_ERR_STATUS_SHIFT)
#define GHAL_VC_RX_ERR_LOCATION(status)   (((status) & GHAL_RXD_FLAG_ERR_LOC_MASK) >> GHAL_RXD_FLAG_ERR_LOC_SHIFT)
/* Check for Immediate flag*/
#define GHAL_VC_IS_RXD_IMMED(vc_desc, index)  (GHAL_RX_CPU_DESC(vc_desc, index)->flags & GHAL_RXD_FLAG_IMM_AVAILABLE)
#define GHAL_VC_IS_RXD_IMMED_SET(status)  ((status) & GHAL_RXD_FLAG_IMM_AVAILABLE)
/* Get RX descriptor Immediate data */
#define GHAL_VC_GET_RXD_IMMED(vc_desc, index)  GHAL_RX_CPU_DESC(vc_desc, index)->imm_data
#define GHAL_VC_CLEAR_RXD_IMMED(vc_desc, index) (GHAL_RX_CPU_DESC(vc_desc, index)->imm_data = 0)

/* Enable RX channel */
#define GHAL_VC_RXC_ENABLE(vc_desc, irq_en) \
		do {\
		ghal_rxvc_desc_t tmp_rxvc;\
		sfence();\
		tmp_rxvc.qword1 = readq(&(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qword1));\
		tmp_rxvc.flags = GHAL_RXVC_FLAG_ENABLE | GHAL_RXVC_FLAG_OVRPROTECT | \
				 (irq_en ? GHAL_RXVC_FLAG_IRQ_ENABLE:0);\
		writeq(tmp_rxvc.qword1, &(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qword1));\
		} while (0)

/* Disable RX channel */
#define GHAL_VC_RXC_DISABLE(vc_desc) \
		do {\
		ghal_rxvc_desc_t tmp_rxvc;\
		sfence();\
		tmp_rxvc.qword1 = readq(&(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qword1));\
		tmp_rxvc.flags = GHAL_RXVC_FLAG_DISABLE;\
		writeq(tmp_rxvc.qword1, &(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qword1));\
		} while (0)

/* Set RX buffer size */
#define GHAL_VC_WR_RXC_BUFFSIZE(vc_desc, value) \
		do {\
		ghal_rxvc_desc_t tmp_rxvc;\
		tmp_rxvc.qword1 = readq(&(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qword1));\
		tmp_rxvc.buf_size = value;\
		writeq(tmp_rxvc.qword1, &(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qword1));\
		} while (0)

/* Reset RX channel */
#define GHAL_VC_RXC_RESET(vc_desc) \
		do {\
		ghal_rxvc_desc_t tmp_rxvc;\
		sfence();\
		tmp_rxvc.qword1 = readq(&(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qword1));\
		tmp_rxvc.flags = GHAL_RXVC_FLAG_RESET;\
		writeq(tmp_rxvc.qword1, &(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qword1));\
		} while (0)

#define GHAL_IRQ_TO_IRQ_INDEX(_pdev, _irq) ((_irq) - (_pdev)->irq)

/* Interrupt vectors */
#define GHAL_IRQ_GNI_RX(pdev, vcid) (pdev->irq + GHAL_IRQ_INDEX(vcid, GHAL_IRQ_RX))
#define GHAL_IRQ_GNI_TX(pdev, vcid) (pdev->irq + GHAL_IRQ_INDEX(vcid, GHAL_IRQ_TX))
#define GHAL_IRQ_IPOGIF_RX(pdev) (pdev->irq + GHAL_IRQ_INDEX(GHAL_VC_IPOGIF, GHAL_IRQ_RX))
#define GHAL_IRQ_IPOGIF_TX(pdev) (pdev->irq + GHAL_IRQ_INDEX(GHAL_VC_IPOGIF, GHAL_IRQ_TX))
#define GHAL_IRQ_ERROR(pdev,block) (pdev->irq + block)
#define GHAL_IRQ_FLWCTRL(pdev) (pdev->irq + GHAL_IRQ_FLWCTRL_INDEX)

/* Reset the corresponding bit of IRQ status register */
#define GHAL_IRQ_RESET_GNI_RX(vcid, status_reg) (*status_reg = (1 << GHAL_IRQ_INDEX(vcid, GHAL_IRQ_RX)))
#define GHAL_IRQ_RESET_GNI_TX(vcid, status_reg) (*status_reg = (1 << GHAL_IRQ_INDEX(vcid, GHAL_IRQ_TX)))
#define GHAL_IRQ_RESET_IPOGIF_RX(status_reg) (*status_reg = (1 << GHAL_IRQ_INDEX(GHAL_VC_IPOGIF, GHAL_IRQ_RX)))
#define GHAL_IRQ_RESET_IPOGIF_TX(status_reg) (*status_reg = (1 << GHAL_IRQ_INDEX(GHAL_VC_IPOGIF, GHAL_IRQ_TX)))

/* CQ interrupt vectors */
#define GHAL_IRQ_GNI_CQ(pdev, index) ((pdev)->irq + (index))
#define GHAL_IRQ_RESET_GNI_CQ(status_reg, index) (*status_reg = (1 << (index)))
#define GHAL_DEF_KERN_CQ_IRQS	1
#define GHAL_DEF_USER_CQ_IRQS	(GHAL_IRQ_CQ_MAX_INDEX - GHAL_DEF_KERN_CQ_IRQS)

/* Get error sub-block from interrupt vector */
#define GHAL_IRQ_ERROR_INDEX(pdev, irq) GHAL_IRQ_TO_IRQ_INDEX((pdev), (irq))

#endif /* __KERNEL__ */

/* FMA Descriptor */
#define GHAL_FMA_INIT {{{0}}}
/* Set PEBase (part of BaseOffset on Gemini)
  This macro is dedicated for PEMaskMode 000b. 
  We may add generic macro later which will take PeMaskMode as a parameter
  and shift PE value correspondingly or add a separate macro for each mode */
#define GHAL_FMA_SET_PE(desc, value) ((desc.base_offset) = \
((desc.base_offset) & ((1UL << 40) - 1)) | ((uint64_t) (value) << 40))
/* Set Base Offset (same as above for PEMaskMode 000b)*/
#define GHAL_FMA_SET_RMDH_OFFSET(desc, value) ((desc.base_offset) = \
((desc.base_offset) & ~((1UL << 40) - 1)) | ((uint64_t) (value) & ((1UL << 40) - 1)))
/* Get the base offset memory address, should be different if PEMaskMode != 000b */
#define GHAL_FMA_GET_RMDH_OFFSET_ADDR(desc) (desc.base_offset & ((1UL << 40) - 1))
/* Set Remote Memory Domain Handle */
#define GHAL_FMA_SET_REM_MDH(desc, value) desc.rem_mem_dh = value
/* Set Remote Memory Domain Handle */
#define GHAL_FMA_SET_LOC_MDH(desc, value) desc.src_mem_dh = value
/* Set Dest. & Src. PTags */
#define GHAL_FMA_SET_PTAGS(desc, value) desc.src_ptag = value; desc.dest_ptag = value
/* Set FMA Command */
#define GHAL_FMA_SET_CMD(desc, value) desc.command = ((value) & 0x1f)
#define GHAL_FMA_SET_FETCH(desc)

/* Set/Clear Adapt and Hash bits */
#define GHAL_FMA_SET_ADP_BIT(desc) (desc).mode2 |= GHAL_FMA_MODE_ADP_ENABLE
#define GHAL_FMA_CLEAR_ADP_BIT(desc) (desc).mode2 &= ~GHAL_FMA_MODE_ADP_ENABLE
#define GHAL_FMA_SET_HASH_BIT(desc) (desc).mode2 |= GHAL_FMA_MODE_HASH_ENABLE
#define GHAL_FMA_CLEAR_HASH_BIT(desc) (desc).mode2 &= ~GHAL_FMA_MODE_HASH_ENABLE
#define GHAL_FMA_SET_RADP_BIT(desc) (desc).mode2 |= GHAL_FMA_MODE_RADP_ENABLE
#define GHAL_FMA_CLEAR_RADP_BIT(desc) (desc).mode2 &= ~GHAL_FMA_MODE_RADP_ENABLE

#define GHAL_FMA_RC_MODES (GHAL_FMA_MODE_ADP_ENABLE | GHAL_FMA_MODE_HASH_ENABLE | GHAL_FMA_MODE_RADP_ENABLE)
#define GHAL_FMA_SET_RC(desc, mode) (desc).mode2 |= (mode & GHAL_FMA_RC_MODES);
#define GHAL_FMA_GET_RC(desc) ((desc).mode2 & GHAL_FMA_RC_MODES);

/* Set/Clear Aries RC modes, these are all NOPs for Gemini */
#define GHAL_FMA_ROUTE_NMIN_HASH(desc_cpy)
#define GHAL_FMA_ROUTE_MIN_HASH(desc_cpy)
#define GHAL_FMA_ROUTE_MNON_HASH(desc_cpy)
#define GHAL_FMA_ROUTE_ADAPT(desc_cpy,mode)

/* Set default SMSG routing mode, Adapt bit only on Gemini */
#define GHAL_FMA_ROUTE_SMSG_DEF(desc) GHAL_FMA_SET_ADP_BIT(desc)

/* Set FMA Enable */
#define GHAL_FMA_SET_FMA_ENABLE(desc) desc.mode2 |= GHAL_FMA_MODE_FMA_ENABLE

/* Aries compatibility and to keep compiler warnings away */
#define GHAL_FMA_COPY_QW1(to, from)   (to).qword1 = (from).qword1
#define GHAL_FLBTE_SET_PRIV(desc)
#define GHAL_FLBTE_SET_BTE_CHAN(desc,value)
#define GHAL_FLBTE_SET_ENQ_STATUS_CQ(desc, cq_hndl)
#define GHAL_FLBTE_CLR_ENQ_STATUS_CQ(desc)

#define GHAL_FMA_PEMASKMODE_0	0x0
#define GHAL_FMA_PEMASKMODE_1	0x1
#define GHAL_FMA_PEMASKMODE_2	0x2
#define GHAL_FMA_PEMASKMODE_3	0x3
#define GHAL_FMA_PEMASKMODE_4	0x4
#define GHAL_FMA_PEMASKMODE_5	0x5
#define GHAL_FMA_PEMASKMODE_6	0x6
#define GHAL_FMA_PEMASKMODE_7	0x7

/* Evaluates to the bit shift to the PE field of the FMAD base_offset given a
 * specific PE mask mode. Modes 0 and 7 are most interesting. */
#define _GHAL_FMA_PEMASKMODE_PESHIFT(_pe_mm)				\
	(((_pe_mm) == GHAL_FMA_PEMASKMODE_0) ? 40 :			\
	 ((_pe_mm) == GHAL_FMA_PEMASKMODE_7) ?  6 :			\
	 ((_pe_mm) == GHAL_FMA_PEMASKMODE_1) ? 35 :			\
	 ((_pe_mm) == GHAL_FMA_PEMASKMODE_2) ? 21 :			\
	 ((_pe_mm) == GHAL_FMA_PEMASKMODE_3) ? 12 : 			\
	 ((_pe_mm) == GHAL_FMA_PEMASKMODE_4) ? 10 :			\
	 ((_pe_mm) == GHAL_FMA_PEMASKMODE_5) ?  9 :			\
	 /* _pe_mm == GHAL_FMA_PEMASKMODE_6 */  8)

/* Set PEBase using the provided PE mask mode */
#define GHAL_FMA_SET_PE_W_MM(desc, pe_mm, value)			\
do {									\
	int pe_width = 18;						\
	uint64_t pe_mask = ((1ULL << pe_width) - 1);			\
	int pe_shift = _GHAL_FMA_PEMASKMODE_PESHIFT(pe_mm & 0x7);	\
	(desc).base_offset =						\
		((desc.base_offset) & ~(pe_mask << pe_shift)) |		\
		(((value) & pe_mask) << pe_shift);			\
} while(0)

/* Set Base Offset using the provided PE mask mode */
#define GHAL_FMA_SET_RMDH_OFFSET_W_MM(desc, pe_mm, value)		\
do {									\
	int pe_width = 18, off_width = 40, off_shift;			\
	uint64_t pe_mask = ((1ULL << pe_width) - 1);			\
	uint64_t off_lmask, off_umask;					\
	int pe_shift = _GHAL_FMA_PEMASKMODE_PESHIFT(pe_mm & 0x7);	\
	off_lmask = ((1ULL << pe_shift) - 1);				\
	off_umask = ((1ULL << (off_width - pe_shift)) - 1);		\
	off_shift = pe_shift + pe_width;				\
	(desc).base_offset =						\
		(((desc).base_offset) & (pe_mask << pe_shift)) |	\
		((value) & off_lmask) |					\
		((((value) >> pe_shift) & off_umask) << off_shift);	\
} while(0)

/* Set PE mask */
#define GHAL_FMA_SET_PEMASK(desc, value) desc.pe_mask_mode = value
/* Set CQ Handle */
#define GHAL_FMA_SET_CQ(desc, value) desc.cq_hndl = value
#define GHAL_FMA_CLEAR_CQ(desc) desc.cq_hndl = 0
/* Set/Clear VMDH enable */
#define GHAL_FMA_SET_VMDH_ENABLE(desc) desc.mode2 |= GHAL_FMA_MODE_VMDH_ENABLE
#define GHAL_FMA_CLEAR_VMDH_ENABLE(desc) desc.mode2 &= ~GHAL_FMA_MODE_VMDH_ENABLE
/* Set/Clear NTT enable */
#define GHAL_FMA_SET_NTT_ENABLE(desc) desc.mode3 |= GHAL_FMA_MODE_NTT_ENABLE
#define GHAL_FMA_CLEAR_NTT_ENABLE(desc) desc.mode3 &= ~GHAL_FMA_MODE_NTT_ENABLE
/* Set PE base */
#define GHAL_FMA_SET_PE_BASE(desc, value) desc.pe_base = value
/* Set NTT group size */
#define GHAL_FMA_SET_NPES(desc, value) desc.num_pes = value
/* DLA items (for compatibility with Aries) */
#define GHAL_FMA_SET_DLA_CQ(desc, value)
#define GHAL_FMA_SET_DLA_CD_LOW_MODE(desc)
#define GHAL_FMA_SET_DLA_CD_HIGH_MODE(desc)
#define GHAL_FMA_SET_DLA_PR_MODE(desc)
#define GHAL_FMA_WR_DLA_MODE(fma_ctrl, desc)
/* Write BaseOffset to the NIC */
#define GHAL_FMA_WR_BASE_OFFSET(fma_ctrl, desc) ((volatile ghal_fma_desc_t *) fma_ctrl)->qword1 = desc.qword1
/* Write Command, Src. MDH, Remote MDH, Hash/Adapt and vMDH modes to the NIC.
   It's all in the second QWORD on Gemini */
#define GHAL_FMA_WR_SRMDH_MODES(fma_ctrl, desc) ((volatile ghal_fma_desc_t *) fma_ctrl)->qword2 = desc.qword2
/* Write QWs to make FMAD stores contiguous (not desired on Gemini) */
#define GHAL_FMA_WR_QW2(fma_ctrl, val)
#define GHAL_FMA_WR_QW3(fma_ctrl, val)
#define GHAL_FMA_WR_QW4(fma_ctrl, val)
/* Set Remote Event, Dummy for forwards compat with Aries */
#define GHAL_FMA_SET_DEST_CQDATA(desc, val)
#define GHAL_FMA_WR_DEST_CQDATA(fma_ctrl, val)
/* Allocate a Source Sync ID */
#define GHAL_FMA_ALLOC_SYNCID(fma_ctrl) ((volatile ghal_fma_desc_t *) fma_ctrl)->alloc_syncid = GHAL_FMA_ALLOC_SEQID_DOORBELL
#define GHAL_FMA_PR_CREATE(fma_ctrl, pr_id, pr_credits)
/* Write Sync Complete to the NIC */
#define GHAL_FMA_SYNC_COMPLETE(fma_ctrl, sync_mode, sync_data)  \
  do {                                                          \
    uint64_t tmp = (((uint64_t)(sync_mode) << GHAL_FMA_SYNC_TYPE_OFFSET) | (sync_data));   \
    ((volatile ghal_fma_desc_t *) fma_ctrl)->qword8 = tmp;      \
  } while (0);
#define GHAL_FMA_PR_SYNC_COMPLETE(fma_ctrl, mode, data)    GHAL_FMA_SYNC_COMPLETE(fma_ctrl, mode, data)
/* Write remote flag value to the offset in the remote MDD */
#define GHAL_FMA_SET_REMOTE_FLAG(desc, offset, value)           \
{desc.scratch0 = value;                                         \
 desc.scratch1 = offset;                                        \
}
#define GHAL_FMA_WR_REMOTE_FLAG(fma_ctrl, offset, value)        \
{((volatile ghal_fma_desc_t *) fma_ctrl)->scratch0 = value;     \
 ((volatile ghal_fma_desc_t *) fma_ctrl)->scratch1 = offset;    \
}
/* Set AMO operand to register 0 */
#define GHAL_FMA_WR_SCRATCH0(fma_ctrl, operand) ((volatile ghal_fma_desc_t *) fma_ctrl)->scratch0 = operand
/* Set AMO operand to register 1 */
#define GHAL_FMA_WR_SCRATCH1(fma_ctrl, operand) ((volatile ghal_fma_desc_t *) fma_ctrl)->scratch1 = operand
/* Write event for the remote CQ */
#define GHAL_FMA_WR_REMOTE_CQ_EVENT(fma_ctrl, value) ((volatile ghal_fma_desc_t *) fma_ctrl)->cq_write = value

/* Dealing with error flags and counters: read fma_status using GHAL_FMA_RD_STATUS, extract information
 * using GHAL_FMA_IS_ERROR, GHAL_FMA_GET_xxx_COUNTER and then clear with GHAL_FMA_CLEAR_STATUS */
/* Read Error flags and counters.*/
#define GHAL_FMA_RD_STATUS(fma_ctrl) readq(&((volatile ghal_fma_desc_t *) fma_ctrl)->qword4)
/* Check for Errors (for err_mask use: GHAL_FMA_FLAG_SNCID_ALLOC_ERROR,... eg. )*/
#define GHAL_FMA_IS_ERROR(fma_status, err_mask) (((fma_status) >> GHAL_FMA_FLAG_SHIFT) & (err_mask))
/* Get WC, RC and SSID_IN_USE counters */
#define GHAL_FMA_GET_WC_COUNTER(fma_status, err_mask) ((fma_status) & GHAL_FMA_COUNTER_MASK)
#define GHAL_FMA_GET_RC_COUNTER(fma_status, err_mask) (((fma_status) >> GHAL_FMA_RC_SHIFT) & GHAL_FMA_COUNTER_MASK)
#define GHAL_FMA_GET_SSID_COUNTER(fma_status, err_mask) (((fma_status) >> GHAL_FMA_SSID_SHIFT) & GHAL_FMA_SSID_MASK)
/* Clear Error flags and counters */
#define GHAL_FMA_CLEAR_STATUS(fma_ctrl) writeq(0, &((volatile ghal_fma_desc_t *) fma_ctrl)->qword4)
/* FMA Operations */
#define GHAL_FMA_GET            0x000
#define GHAL_FMA_PUT            0x100
#define GHAL_FMA_PUT_MSG        0x110
#define GHAL_FMA_PUT_S          0x100	/* PUT_S is not supported on Gemini */

/* GCW (GET CONTROL WORD)  */
#define GHAL_GCW_INIT {0}
/* Write cmd */
#define GHAL_GCW_SET_CMD(gcw, command) ((volatile ghal_gcw_t *)&gcw)->cmd = command
/* Write count */
#define GHAL_GCW_SET_COUNT(gcw, cnt) ((volatile ghal_gcw_t *)&gcw)->count = cnt
/* Write local offset */
#define GHAL_GCW_SET_LOCAL_OFFSET(gcw, loc_off) ((volatile ghal_gcw_t *)&gcw)->local_offset = (uint64_t)(loc_off)
/* Set GB bit offset */
#define GHAL_GCW_SET_GB(gcw) ((volatile ghal_gcw_t *)&gcw)->gb = 1

/* CQ Destriptor */
#define GHAL_CQ_INIT {0}
/* Set CQEnable flag */
#define GHAL_CQ_SET_ENABLE(desc) desc.enable = 1
/* Set IRQReqIndex */
#define GHAL_CQ_SET_IRQ_IDX(desc, value) desc.irq_req_index = value
/* Set IRQThreshIndex */
#define GHAL_CQ_SET_DELAY_CNT(desc, value) desc.irq_thresh_index = value
/* Set CQMaxIndex */
#define GHAL_CQ_SET_MAX_CNT(desc, value) desc.max_index = value
/* Set IRQReq */
#define GHAL_CQ_SET_IRQ_REQ(desc) desc.irq_req = 1
/* Set CQAddr */
#define GHAL_CQ_SET_ADDR(desc, value) desc.base_addr = value
/* Set IOMMU enable */
#define GHAL_CQ_SET_IOMMU(desc)
/* Set TCR */
#define GHAL_CQ_SET_TCR(desc, tcr)
/* Write CQ ReadIndex */
#define	GHAL_CQ_WRITE_RDINDEX(rd_index_ptr, read_index)	*rd_index_ptr = read_index
/* Check if CQ event has flag READY set */
#define GHAL_CQ_IS_EVENT_READY(event) (((event) >> GHAL_CQE_SOURCE_SHIFT) & GHAL_CQE_SOURCE_MASK)
#define GHAL_CQE_SOURCE_CLR(event) (event) &= ~(GHAL_CQE_SOURCE_MASK << GHAL_CQE_SOURCE_SHIFT)
/* Set ready bit in CQ event - only needed for xd1 emulation */
#define GHAL_CQ_SET_EVENT_READY(event)
/* Check if CQ source is BTE */
#define GHAL_CQ_SOURCE_BTE(event) (GHAL_CQ_IS_EVENT_READY(event) == GHAL_CQE_SOURCE_BTE)
/* Check if CQ source is SSID on a local side */
#define GHAL_CQ_SOURCE_SSID(event) (GHAL_CQ_IS_EVENT_READY(event) & GHAL_CQE_SOURCE_SSID_MASK)
/* Check if this is a remote event */
#define GHAL_CQ_SOURCE_RMT(event) (GHAL_CQ_IS_EVENT_READY(event) >= GHAL_CQE_SOURCE_RMT_SREQ)

/* CQE macros for Aries compatibility */
#define GHAL_CQ_INFO_ENQ_STATUS(event)          0
#define GHAL_CQE_STATUS_IS_DLA_OVERFLOW(status) 0

/* Memory Domain Destriptor */
#define GHAL_MDD_INIT {0}
/* MRT Enable */
#define GHAL_MDD_SET_MRT_ENABLE(desc) desc.mrt_enable = 1
/* MRT Disable */
#define GHAL_MDD_SET_MRT_DISABLE(desc) desc.mrt_enable = 0
/* Memory Base address */
#define GHAL_MDD_SET_MEMBASE(desc, value) desc.mem_base = ((value) >> 12)
/* Protection Tag */
#define GHAL_MDD_SET_PTAG(desc, value) desc.ptag = value
/* CQ handle */
#define GHAL_MDD_SET_CQHNDL(desc, value) desc.cqh = value
/* Max Base address */
#define GHAL_MDD_SET_MAX_MBASE(desc, value) desc.max_base = ((value) >> 12)
/* Set modes (see GHAL_MDD_MODE_xxxx in gemini_hw.h) */
#define GHAL_MDD_SET_MODES(desc, value) desc.modes |= value
#define GHAL_MDD_CLEAR_MODES(desc, value) desc.modes = (desc.modes & (~value))
#define GHAL_MDD_SET_COHERENT(desc) GHAL_MDD_SET_MODES(desc, GHAL_MDD_MODE_COHERENT)
#define GHAL_MDD_SET_P_PASSPW(desc) GHAL_MDD_SET_MODES(desc, GHAL_MDD_MODE_P_PASPW)
#define GHAL_MDD_SET_NP_PASSPW(desc) GHAL_MDD_SET_MODES(desc, GHAL_MDD_MODE_NP_PASSPW)
#define GHAL_MDD_SET_RO_ALLOW(desc) GHAL_MDD_SET_NP_PASSPW(desc)
#define GHAL_MDD_SET_RO_ENABLE(desc) GHAL_MDD_SET_P_PASSPW(desc)
#define GHAL_MDD_SET_FLUSH(desc) GHAL_MDD_SET_MODES(desc, GHAL_MDD_MODE_FLUSH)
#define GHAL_MDD_SET_VALID(desc) GHAL_MDD_SET_MODES(desc, GHAL_MDD_MODE_VALID)
#define GHAL_MDD_SET_INVALID(desc) GHAL_MDD_CLEAR_MODES(desc, GHAL_MDD_MODE_VALID)
#define GHAL_MDD_SET_WRITABLE(desc) GHAL_MDD_SET_MODES(desc, GHAL_MDD_MODE_WRITABLE)
#define GHAL_MDD_SET_IOMMU_ENABLE(desc, tcr)
#define GHAL_MDD_SET_PCI_IOMMU_ENABLE(desc, tcr)

#define GHAL_GART_GET_ADDR(entry)  (cgm_gart + ((entry) << PAGE_SHIFT))
#define GHAL_MRT_GET_ADDR(entry, shift)  ((entry) << shift)
#define GHAL_IOMMU_GET_ADDR(entry, shift) 0
#define GHAL_PCI_IOMMU_GET_ADDR(entry, shift) 0

/* Memory Relocation Table */
inline static int
ghal_mrt_code_to_size (int code)
{
  int memhndl_pagesize[] = {
    128 * 1024,
    512 * 1024,
    2 * 1024 * 1024,
    8 * 1024 * 1024,
    16 * 1024 * 1024,
    64 * 1024 * 1024,
    256 * 1024 * 1024,
    1024 * 1024 * 1024
  };

  return memhndl_pagesize[code & GHAL_MRT_PAGESIZE_MASK];
}

inline static int
ghal_mrt_code_to_shift (int code)
{
  unsigned short memhndl_shift[] = {
    17,
    19,
    21,
    23,
    24,
    26,
    28,
    30
  };

  return memhndl_shift[code & GHAL_MRT_PAGESIZE_MASK];
}

inline static int
ghal_mrt_code_to_mask (int code)
{
  int memhndl_mask[] = {
    ~(128 * 1024 - 1),
    ~(512 * 1024 - 1),
    ~(2 * 1024 * 1024 - 1),
    ~(8 * 1024 * 1024 - 1),
    ~(16 * 1024 * 1024 - 1),
    ~(64 * 1024 * 1024 - 1),
    ~(256 * 1024 * 1024 - 1),
    ~(1024 * 1024 * 1024 - 1)
  };

  return memhndl_mask[code & GHAL_MRT_PAGESIZE_MASK];
}

inline static int
ghal_mrt_size_to_code (unsigned long size)
{
  int code;

  for (code = 0; code <= GHAL_MRT_PAGESIZE_MASK; code++)
    {
      if (size == ghal_mrt_code_to_size (code))
	{
	  return code;
	}
    }
  return -1;
}

#define GHAL_MRT_SIZE_TO_CODE(size) ghal_mrt_size_to_code(size)
#define GHAL_MRT_CODE_TO_SIZE(code) ghal_mrt_code_to_size(code)
#define GHAL_MRT_CODE_TO_SHIFT(code) ghal_mrt_code_to_shift(code)
#define GHAL_MRT_CODE_TO_MASK(code) ghal_mrt_code_to_mask(code)

#endif /*_GHAL_PUB_H_*/
