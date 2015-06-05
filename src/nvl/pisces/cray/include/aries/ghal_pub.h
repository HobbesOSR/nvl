/**
 *      The Generic Hardware Abstraction Layer not kernel specific interface
 *      This file should be included by user level application
 *      directly interfacing NIC
 *
 *      Copyright 2007 Cray Inc. All Rights Reserved.
 *      Written by Kyle Hubert <khubert@cray.com>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation; either version 2 of
 *      the License, or (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be
 *      useful, but WITHOUT ANY WARRANTY; without even the implied
 *      warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *      PURPOSE.  See the GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public
 *      License along with this program; if not, write to the Free
 *      Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *      Boston, MA 02110-1301 USA
 **/

#ifndef _GHAL_PUB_H_
#define _GHAL_PUB_H_

#include "aries_hw.h"
#include "ghal_err.h"

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

/* other strings */
#define GHAL_CLDEV_MDD_STR		"MDD"
#define GHAL_CLDEV_MRT_STR		NULL
#define GHAL_CLDEV_CQ_STR		"CQ"
#define GHAL_CLDEV_FMA_STR		"FMA"
#define GHAL_CLDEV_CE_STR		"CE"
#define GHAL_CLDEV_DLA_STR		"DLA"

/* GNI default sysfs class path */
#define GHAL_CLDEV_CLASS_PATH		SYS_CLASS_PATH_STR "/" GHAL_CLDEV_CLASS_STR

/* GNI device class directory format strings */
#define GHAL_CLDEV_KGNI_FMT		GHAL_CLDEV_KGNI_STR "%d"
#define GHAL_CLDEV_GHAL_FMT		GHAL_CLDEV_GHAL_STR "%d"
#define GHAL_CLDEV_KGNI_PATH_FMT	GHAL_CLDEV_CLASS_PATH "/" GHAL_CLDEV_KGNI_FMT
#define GHAL_CLDEV_GHAL_PATH_FMT	GHAL_CLDEV_CLASS_PATH "/" GHAL_CLDEV_GHAL_FMT

/* GNI device class files */
#define GHAL_CLDEV_JOBRES_FMT		GHAL_CLDEV_KGNI_PATH_FMT "/" GHAL_CLDEV_RES_FILE_STR
#define GHAL_CLDEV_DEVRES_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_RES_FILE_STR
#define GHAL_CLDEV_NICTYPE_PATH		GHAL_CLDEV_CLASS_PATH "/" GHAL_CLDEV_TYPE_FILE_STR
#define GHAL_CLDEV_VERS_PATH		GHAL_CLDEV_CLASS_PATH "/" GHAL_CLDEV_VERS_FILE_STR
#define GHAL_CLDEV_ADDR_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_ADDR_FILE_STR
#define GHAL_CLDEV_PARAM_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_PARAM_FILE_STR
#define GHAL_CLDEV_NTT_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_NTT_FILE_STR
#define GHAL_CLDEV_MRT_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_MRT_FILE_STR
#define GHAL_CLDEV_BI_FMT		GHAL_CLDEV_GHAL_PATH_FMT "/" GHAL_CLDEV_BI_FILE_STR

#ifdef __KERNEL__

extern uint64_t cgm_gart;

static inline void memcpy64_fromio(void *to, const volatile void __iomem *from, size_t bytes)
{
	while (bytes){
		*(uint64_t *) to = *(const volatile uint64_t *) from;
		to = (uint64_t *) to + 1;
		from = (const volatile uint64_t *) from + 1;
		bytes -= sizeof(uint64_t);
	}
}

static inline void memcpy64_toio(volatile void __iomem *to, const void *from, size_t bytes)
{
	while (bytes) {
		*(volatile uint64_t *) to = *(const uint64_t *) from;
		to = (volatile uint64_t *) to + 1;
		from = (const uint64_t *) from + 1;
		bytes -= sizeof(uint64_t);
	}
}

/* Write WrIdx index to RX channel descriptor */
#define GHAL_VC_WRITE_RX_WRINDEX(vc_desc, index) \
	do { \
	wmb(); \
	writeq(index, &(((volatile ghal_rxvc_desc_t *)vc_desc.rx_ctl_desc)->qwords[3]));\
	wmb(); \
	} while(0)

/* Write WrIdx index to TX channel descriptor */
#define GHAL_VC_WRITE_TX_WRINDEX(vc_desc, index) \
	do { \
	wmb(); \
	writeq(index, &(((volatile ghal_txvc_desc_t *)vc_desc.tx_ctl_desc)->qwords[3]));\
	wmb();\
	} while(0)

#define GHAL_RX_DESC(vc_desc, id)       ((volatile ghal_rx_desc_t __iomem *)(vc_desc.rx_desc + GHAL_BTE_RX_OFF_BYID(id)))
#define GHAL_TX_DESC(vc_desc, id)       ((volatile ghal_tx_desc_t __iomem *)(vc_desc.tx_desc + GHAL_BTE_TX_OFF_BYID(id)))
#define GHAL_RX_CPU_DESC(vc_desc, id)   ((ghal_cpurx_desc_t *)(vc_desc.cpu_rx_desc + GHAL_BTE_CPURX_OFF_BYID(id)))
#define GHAL_TX_CPU_DESC(vc_desc, id)   ((ghal_tx_desc_t *)(vc_desc.cpu_tx_desc + GHAL_BTE_CPUTX_OFF_BYID(id)))

/* BTE TX Descriptor */
/* Reset TX descriptor fields */
#define GHAL_TXD_INIT {{0}}
/* BTE Options */
#define GHAL_TXD_SET_BTE_OP(desc, value) desc.bte_op = value
#define GHAL_TXD_SET_CQ_GLOBAL(desc) desc.src_ssid_cq_en = 1
#define GHAL_TXD_SET_CQ_LOCAL(desc) desc.src_bte_cq_en = 1
#define GHAL_TXD_SET_NAT(desc) desc.nat_en = 1
#define GHAL_TXD_SET_NTT(desc) desc.ntt_en = 1
#define GHAL_TXD_SET_TX_CONCAT(desc) desc.tx_concatenate = 1
#define GHAL_TXD_SET_IMM(desc) desc.bte_immediate = 1
#define GHAL_TXD_SET_FENCE(desc) desc.bte_fence = 1
#define GHAL_TXD_SET_IRQ(desc) desc.irq_mode = 1
#define GHAL_TXD_SET_DELAYED_IRQ(desc) desc.delayed_irq_en = 1
#define GHAL_TXD_SET_COHERENT(desc) desc.rd_no_snoop = 0
#define GHAL_TXD_SET_PASSPW(desc) desc.rd_ro = 1
#define GHAL_TXD_SET_WC(desc, value) (desc).wc = (value);

/* BTE TX RC mode setters */
#define _GHAL_TXD_SET_RC(desc_cpy,mode)		(desc_cpy).rc = (mode)
#define GHAL_TXD_SET_RC_NMIN_HASH(desc_cpy)	_GHAL_TXD_SET_RC((desc_cpy),A_RC_NON_MIN_HASHED)
#define GHAL_TXD_SET_RC_MIN_HASH(desc_cpy)	_GHAL_TXD_SET_RC((desc_cpy),A_RC_MIN_HASHED)
#define GHAL_TXD_SET_RC_MNON_HASH(desc_cpy)	_GHAL_TXD_SET_RC((desc_cpy),A_RC_MIN_NON_HASHED)
#define GHAL_TXD_SET_RC_ADAPT(desc_cpy,mode)	_GHAL_TXD_SET_RC((desc_cpy),_GHAL_RC_ADAPT_MODE(mode))

/* Routing control macro for backward compat. */
#define GHAL_TXD_SET_OPT_DELIV(desc)		GHAL_TXD_SET_RC_ADAPT(desc,0)

#define GHAL_TXD_SET_VMDH(desc)
/* Set Destination PE value */
#define GHAL_TXD_SET_PE(desc, value) desc.dest_endpoint = value
/* Set Src. data length */
#define GHAL_TXD_SET_LENGTH(desc, value) desc.xfer_len = value
/* Set Src. Physical Address, when NAT is disabled */
#define GHAL_TXD_SET_LOC_PHADDR(desc, value) desc.loc_phys_addr = value
/* Set Src. Memory Offset, when NAT is enabled */
#define GHAL_TXD_SET_LOC_MEMOFF(desc, value) desc.loc_mem_offset = value
/* Set Src. Memory Domain Handle */
#define GHAL_TXD_SET_LOC_MDH(desc, value) desc.loc_mdh = value
/* Set Src. CQ handle */
#define GHAL_TXD_SET_SRC_CQ_BS(desc, cq_hndl) desc.src_cqh = cq_hndl->hw_cq_hndl
#define GHAL_TXD_SET_SRC_CQ(desc, hw_cq_hndl) desc.src_cqh = hw_cq_hndl
/* Set UserData */
#define GHAL_TXD_SET_USRDATA(desc, value) desc.dest_user_data = value
/* Set SrcCqData */
#define GHAL_TXD_SET_SRCDATA(desc, value) desc.src_cq_data = value
/* Set CQ events */
#define GHAL_TXD_SET_CQE_LOCAL(desc, value) desc.src_cq_data = value
#define GHAL_TXD_SET_CQE_GLOBAL(desc, value_ari, value_gem) desc.src_cq_data = value_ari
#define GHAL_TXD_SET_CQE_REMOTE(desc, value) desc.dest_user_data = value
/* Set Src. Protection Tag */
#define GHAL_TXD_SET_LOC_PTAG(desc, value) desc.loc_ptag = value
/* Set Dest. Memory Domain Handle */
#define GHAL_TXD_SET_REM_MDH(desc, value) desc.rem_mdh = value
/* Set Dest. Protection Tag */
#define GHAL_TXD_SET_REM_PTAG(desc, value) desc.rem_ptag = value
/* Set Dest. Memory Offset */
#define GHAL_TXD_SET_REM_MEMOFF(desc, value) desc.rem_mem_offset = value

/* Write TX descriptor to the device */
#define GHAL_VC_WRITE_TXD(vc_desc, index, desc) memcpy64_toio(GHAL_TX_DESC(vc_desc, index), &desc, sizeof(desc))
/* Check TX descriptor completion */
#define GHAL_VC_IS_TXD_COMPLETE(vc_desc, index) (GHAL_TX_CPU_DESC(vc_desc, index)->bte_complete)
/* Check TX descriptor timeout error */
#define GHAL_VC_IS_TXD_TIMEOUT(vc_desc, index) (GHAL_TX_CPU_DESC(vc_desc, index)->timeout_err)
/* Get TX error status */
#define GHAL_VC_GET_TXD_ERROR(vc_desc, index) (GHAL_TX_CPU_DESC(vc_desc, index)->tx_sts)

/* Clear TX descriptor completeion bit and timeout error flag*/
#define GHAL_VC_CLEAR_TXD_COMPLETE(vc_desc, index) \
		do {\
		ghal_tx_desc_t *tmp_txd = GHAL_TX_CPU_DESC(vc_desc, index);\
		tmp_txd->bte_complete = 0;\
		tmp_txd->timeout_err = 0;\
		} while(0)

/* Read SSID_IN_USE counter*/
#define GHAL_VC_TXC_READ_SSID_INUSE(vc_desc, ssid_in_use) \
		do {\
		ghal_sts_txvc_desc_t tmp_txvc;\
		tmp_txvc.qwords[0] = readq(&(((volatile ghal_sts_txvc_desc_t *)(vc_desc).sts_tx_ctl)->qwords[0]));\
		ssid_in_use = tmp_txvc.ssids_in_use;\
		} while (0)

/* Wait for SSID_IN_USE to be zero */
#define GHAL_WAIT_FOR_NO_SSIDSINUSE(vc_desc)			\
  do {								\
    uint64_t ssid_in_use;					\
    GHAL_VC_TXC_READ_SSID_INUSE(vc_desc,ssid_in_use);		\
    if(ssid_in_use) {						\
      struct timeval tv;					\
      nsec_t nsec;						\
      might_sleep();						\
      do_gettimeofday(&tv);					\
      nsec = timeval_to_ns(&tv);				\
      do {							\
	schedule();						\
	GHAL_VC_TXC_READ_SSID_INUSE(vc_desc,ssid_in_use);	\
	do_gettimeofday(&tv);					\
	if((timeval_to_ns(&tv) - nsec) > NSEC_PER_SEC) BUG();	\
      } while(ssid_in_use);					\
    }								\
  } while(0)

/* Get TX ring indices */
#define GHAL_VC_TXC_GET_RW_IDX(vc_desc, rd_idx, wr_idx) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		wmb();\
		tmp_txvc.qwords[2] = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qwords[2]));\
		tmp_txvc.qwords[3] = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qwords[3]));\
		rd_idx = tmp_txvc.tx_rd_idx; \
		wr_idx = tmp_txvc.tx_wr_idx; \
		} while (0)

/* Enable TX channel */
#define GHAL_VC_TXC_ENABLE(vc_desc, irq_en) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		wmb();\
		tmp_txvc.qwords[1] = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qwords[1]));\
		tmp_txvc.tx_en = 1;\
		tmp_txvc.tx_irq_en = (irq_en ? 1 : 0);\
		tmp_txvc.tx_rst = 0;\
		writeq(tmp_txvc.qwords[1], &(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qwords[1]));\
		wmb();\
		} while (0)
/* Disable TX channel */
#define GHAL_VC_TXC_DISABLE(vc_desc) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		wmb();\
		tmp_txvc.qwords[1] = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qwords[1]));\
		tmp_txvc.tx_en = 0;\
		tmp_txvc.tx_irq_en = 0;\
		tmp_txvc.tx_rst = 0;\
		writeq(tmp_txvc.qwords[1], &(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qwords[1]));\
		wmb();\
		} while (0)

/* Reset TX channel */
#define GHAL_VC_TXC_RESET(vc_desc) \
		do {\
		ghal_txvc_desc_t tmp_txvc;\
		wmb();\
		tmp_txvc.qwords[1] = readq(&(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qwords[1]));\
		tmp_txvc.tx_en = 0;\
		tmp_txvc.tx_irq_en = 0;\
		tmp_txvc.tx_rst = 1;\
		writeq(tmp_txvc.qwords[1], &(((volatile ghal_txvc_desc_t *)(vc_desc).tx_ctl_desc)->qwords[1]));\
		wmb();\
		} while (0)

/* BTE RX Descriptor */
/* Reset TX descriptor fields */
#define GHAL_RXD_INIT {{0}}
/* BTE Operations */
/* Set RX descriptor modes */
#define GHAL_RXD_SET_PASSPW(desc)
#define GHAL_RXD_SET_COHERENT(desc)
#define GHAL_RXD_SET_IRQ_DELAY(desc) desc.delayed_irq_en = 1
#define GHAL_RXD_SET_IRQ(desc) desc.irq_en = 1
/* Set Buffer Physical Address */
#define GHAL_RXD_SET_BUFF_ADDR(desc, value) desc.base_addr = ((value) >> 6)
/* Write RX descriptor to the device */
#define GHAL_VC_WRITE_RXD(vc_desc, index, desc)   memcpy64_toio(GHAL_RX_DESC(vc_desc, index), &desc, sizeof(desc))
/* Clear RX descriptor status flags */
#define GHAL_VC_CLEAR_RXD_STATUS(vc_desc, index) (GHAL_RX_CPU_DESC(vc_desc, index)->flags = 0)
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
		wmb();\
		tmp_rxvc.qwords[0] = readq(&(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qwords[0]));\
		tmp_rxvc.rx_rst = 0;\
		tmp_rxvc.rx_en = 1;\
		tmp_rxvc.rx_intr_en = (irq_en ? 1 : 0);\
		writeq(tmp_rxvc.qwords[0], &(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qwords[0]));\
		wmb();\
		} while (0)

/* Disable RX channel */
#define GHAL_VC_RXC_DISABLE(vc_desc) \
		do {\
		ghal_rxvc_desc_t tmp_rxvc;\
		wmb();\
		tmp_rxvc.qwords[0] = readq(&(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qwords[0]));\
		tmp_rxvc.rx_rst = 0;\
		tmp_rxvc.rx_en = 0;\
		tmp_rxvc.rx_intr_en = 0;\
		writeq(tmp_rxvc.qwords[0], &(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qwords[0]));\
		wmb();\
		} while (0)

/* Set RX buffer size */
#define GHAL_VC_WR_RXC_BUFFSIZE(vc_desc, value) \
		do {\
		ghal_rxvc_desc_t tmp_rxvc;\
		tmp_rxvc.qwords[0] = readq(&(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qwords[0]));\
		tmp_rxvc.rx_limit = value;\
		writeq(tmp_rxvc.qwords[0], &(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qwords[0]));\
		wmb();\
		} while (0)

/* Reset RX channel */
#define GHAL_VC_RXC_RESET(vc_desc) \
		do {\
		ghal_rxvc_desc_t tmp_rxvc;\
		wmb();\
		tmp_rxvc.qwords[0] = readq(&(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qwords[0]));\
		tmp_rxvc.rx_rst = 1;\
		tmp_rxvc.rx_en = 0;\
		tmp_rxvc.rx_intr_en = 0;\
		writeq(tmp_rxvc.qwords[0], &(((volatile ghal_rxvc_desc_t *)(vc_desc).rx_ctl_desc)->qwords[0]));\
		wmb();\
		} while (0)

/* irq index -> IRQ */
int ghal_irq_index_to_irq(struct pci_dev *pdev, int idx);
/* IRQ -> irq index */
int ghal_irq_to_irq_index(struct pci_dev *pdev, int irq);

#define GHAL_IRQ_TO_IRQ_INDEX(pdev, irq) ghal_irq_to_irq_index(pdev, irq)

/* Interrupt vectors */
#define GHAL_IRQ_GNI_RX(pdev, vcid) ghal_irq_index_to_irq(pdev, GHAL_IRQ_INDEX(vcid, GHAL_IRQ_RX))
#define GHAL_IRQ_GNI_TX(pdev, vcid) ghal_irq_index_to_irq(pdev, GHAL_IRQ_INDEX(vcid, GHAL_IRQ_TX))
#define GHAL_IRQ_IPOGIF_RX(pdev) ghal_irq_index_to_irq(pdev, GHAL_IRQ_INDEX(GHAL_VC_IPOGIF, GHAL_IRQ_RX))
#define GHAL_IRQ_IPOGIF_TX(pdev) ghal_irq_index_to_irq(pdev, GHAL_IRQ_INDEX(GHAL_VC_IPOGIF, GHAL_IRQ_TX))

/* Reset the corresponding bit of IRQ status register */
#define GHAL_IRQ_RESET_GNI_RX(vcid, status_reg) //(*status_reg = (1 << GHAL_IRQ_INDEX(vcid, GHAL_IRQ_RX)))
#define GHAL_IRQ_RESET_GNI_TX(vcid, status_reg) //(*status_reg = (1 << GHAL_IRQ_INDEX(vcid, GHAL_IRQ_TX)))
#define GHAL_IRQ_RESET_IPOGIF_RX(status_reg) //(*status_reg = (1 << GHAL_IRQ_INDEX(GHAL_VC_IPOGIF, GHAL_IRQ_RX)))
#define GHAL_IRQ_RESET_IPOGIF_TX(status_reg) //(*status_reg = (1 << GHAL_IRQ_INDEX(GHAL_VC_IPOGIF, GHAL_IRQ_TX)))

/* CQ interrupt vectors */
#define GHAL_IRQ_GNI_CQ(pdev, index) ghal_irq_index_to_irq(pdev, index)
#define GHAL_IRQ_RESET_GNI_CQ(status_reg, index) //(*status_reg = (1 << (index)))
#define GHAL_DEF_KERN_CQ_IRQS	3
#define GHAL_DEF_USER_CQ_IRQS	(GHAL_IRQ_CQ_MAX_INDEX - GHAL_DEF_KERN_CQ_IRQS)

#endif /* __KERNEL__ */

/* FMA Descriptor */
#define GHAL_FMA_INIT {{{{0}}}}
/* Set PEBase (part of BaseOffset on Aries)
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
#define GHAL_FMA_SET_REM_MDH(desc, value) desc.rem_mdh = value
/* Set Remote Memory Domain Handle */
#define GHAL_FMA_SET_LOC_MDH(desc, value) desc.loc_mdh = value
/* Set Dest. & Src. PTags */
#define GHAL_FMA_SET_PTAGS(desc, value) { desc.loc_ptag = value; desc.rem_ptag = value; }
/* Set FMA Command */
#define GHAL_FMA_SET_CMD(desc, value) desc.fma_op = ((value) & 0x7f)
#define GHAL_FMA_SET_FETCH(desc) desc.get_launch = 1
/* Set FMA GET Write Combining Enable Field */
#define GHAL_FMA_SET_WC(desc, value) (desc).wc = (value)

/* Set/Clear Adapt and Hash bits, these are all NOPs for Aries */
#define GHAL_FMA_SET_ADP_BIT(desc)
#define GHAL_FMA_CLEAR_ADP_BIT(desc)
#define GHAL_FMA_SET_HASH_BIT(desc)
#define GHAL_FMA_CLEAR_HASH_BIT(desc)
#define GHAL_FMA_SET_RADP_BIT(desc)
#define GHAL_FMA_CLEAR_RADP_BIT(desc)

/* Set/Clear Aries RC modes */
#define _GHAL_FMA_SET_RC(desc_cpy,mode)		(desc_cpy).rc = (mode)
#define GHAL_FMA_ROUTE_NMIN_HASH(desc_cpy)	_GHAL_FMA_SET_RC((desc_cpy),A_RC_NON_MIN_HASHED)
#define GHAL_FMA_ROUTE_MIN_HASH(desc_cpy)	_GHAL_FMA_SET_RC((desc_cpy),A_RC_MIN_HASHED)
#define GHAL_FMA_ROUTE_MNON_HASH(desc_cpy)	_GHAL_FMA_SET_RC((desc_cpy),A_RC_MIN_NON_HASHED)
#define GHAL_FMA_ROUTE_ADAPT(desc_cpy,mode)	_GHAL_FMA_SET_RC((desc_cpy),_GHAL_RC_ADAPT_MODE(mode))

/* Set default SMSG routing mode, ADAPTIVE_0 on Aries */
#define GHAL_FMA_ROUTE_SMSG_DEF(desc) GHAL_FMA_ROUTE_ADAPT(desc, 0)

/* Set FMA Enable */
#define GHAL_FMA_SET_FMA_ENABLE(desc) desc.fma_en = 0

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
({									\
	unsigned char mm = ((_pe_mm) & GHAL_FMA_PEMASKMODE_7);		\
	(((mm) == GHAL_FMA_PEMASKMODE_0) ? 40 :				\
	 ((mm) == GHAL_FMA_PEMASKMODE_7) ?  6 :				\
	 ((mm) == GHAL_FMA_PEMASKMODE_1) ? 35 :				\
	 ((mm) == GHAL_FMA_PEMASKMODE_2) ? 21 :				\
	 ((mm) == GHAL_FMA_PEMASKMODE_3) ? 12 : 			\
	 ((mm) == GHAL_FMA_PEMASKMODE_4) ? 10 :				\
	 ((mm) == GHAL_FMA_PEMASKMODE_5) ?  9 :				\
	 /* mm == GHAL_FMA_PEMASKMODE_6 */  8);				\
})

#define _GHAL_FMA_GET_BASE_OFFSET_PE(pe_shift, pe)			\
({									\
	(((pe) & GHAL_PE_MASK) << pe_shift);				\
})

#define _GHAL_FMA_GET_BASE_OFFSET_RMDH_OFFSET(pe_shift, offset)		\
({									\
	uint64_t off_lmask, off_umask;					\
	int off_shift = pe_shift + GHAL_PE_WIDTH;			\
	off_lmask = ((1ULL << pe_shift) - 1);				\
	off_umask = ((1ULL << (GHAL_RDMA_OFF_WIDTH - pe_shift)) - 1);	\
	(((offset) & off_lmask) |					\
	 ((((offset) >> pe_shift) & off_umask) << off_shift));		\
})

#define _GHAL_FMA_GET_BASE_OFFSET(pe_shift, off, pe)			\
({									\
	(_GHAL_FMA_GET_BASE_OFFSET_RMDH_OFFSET((pe_shift), (off)) |	\
	 _GHAL_FMA_GET_BASE_OFFSET_PE((pe_shift), (pe)));		\
})

/* Set PE within the base offset using the provided PE mask mode */
#define GHAL_FMA_SET_PE_W_MM(desc, pe_mm, value)			\
do {									\
	int pe_shift = _GHAL_FMA_PEMASKMODE_PESHIFT((pe_mm));		\
	(desc).base_offset =						\
		(((desc).base_offset) & ~(GHAL_PE_MASK << pe_shift)) |	\
		(_GHAL_FMA_GET_BASE_OFFSET_PE(pe_shift, (value)));	\
} while(0)

/* Set RDMA offset within the base offset using the provided PE mask mode */
#define GHAL_FMA_SET_RMDH_OFFSET_W_MM(desc, pe_mm, value)			\
do {										\
	int pe_shift = _GHAL_FMA_PEMASKMODE_PESHIFT((pe_mm));			\
	(desc).base_offset =							\
		(((desc).base_offset) & (GHAL_PE_MASK << pe_shift)) |		\
		(_GHAL_FMA_GET_BASE_OFFSET_RMDH_OFFSET(pe_shift, (value)));	\
} while(0)

/* Set Base Offset (logical OR of PE and RDMA offset) using the provided PE
 * mask mode */
#define GHAL_FMA_SET_BASE_OFFSET_W_MM(desc, pe_mm, off, pe)		\
do {									\
	int pe_shift = _GHAL_FMA_PEMASKMODE_PESHIFT((pe_mm));		\
	(desc).base_offset =						\
		_GHAL_FMA_GET_BASE_OFFSET(pe_shift, (off), (pe));	\
} while(0)

#define _GHAL_FMA_GET_PUT_GLOBAL_OFFSET(pe, pe_mm, base, win_off)	\
({									\
	int pe_shift = _GHAL_FMA_PEMASKMODE_PESHIFT((pe_mm));		\
	(_GHAL_FMA_GET_BASE_OFFSET(pe_shift, base, pe) + (win_off));	\
})

#define _GHAL_FMA_GET_GET_GLOBAL_OFFSET(pe, pe_mm, base, win_off)	\
({									\
	int pe_shift = _GHAL_FMA_PEMASKMODE_PESHIFT((pe_mm));		\
	(_GHAL_FMA_GET_BASE_OFFSET(pe_shift, base, pe) +		\
		((win_off) >> 1));					\
})

/* Perform FMA Launch using the provided fetching flag, global offset and
 * operand */
#define GHAL_FMA_LAUNCH_GLOBAL_OFFSET(fma_ctrl, get, goff, op)					\
do {												\
	uint64_t dwords[2];									\
	uint64_t *fmal_ptr = &(((ar_nic_fma_cfg_desc_sfma_desc_t *)(fma_ctrl))->qwords[24]);	\
	WF_AR_NIC_FMA_CFG_DESC_SFMA_FMA_LAUNCH_GET(dwords[0], (get));				\
	WF_AR_NIC_FMA_CFG_DESC_SFMA_FMA_LAUNCH_GLOBAL_OFFSET(dwords[0], (goff));		\
	WF_AR_NIC_FMA_CFG_DESC_SFMA_FMA_LAUNCH_OPERAND(dwords[1], (op));			\
	/* FMA Launch doorbell can be used with 2 8-byte writes or 1 16 byte write */		\
	/*fmal_ptr[0] = dwords[0];*/								\
	/*fmal_ptr[1] = dwords[1];*/								\
	asm volatile ("movdqa %1, %%xmm0;"							\
		      "movdqa %%xmm0, %0;"							\
		      :"=m" (*(unsigned char *)fmal_ptr)					\
		      : "m" (*(unsigned char *)dwords)						\
		      :"%xmm0");								\
} while(0)

/* FMA Launch
 *
 * ASDG rev. 2.2 S 1.3.4.11.1:
 *
 * The FMA_LAUNCH doorbell is an alternate mechanism for generating 8-byte or
 * 4-byte Put requests, Get requests, and single operand non-fetching AMO
 * requests.
 *
 * This macro writes to an FMA descriptor's FMA LAUNCH doorbell.
 *
 * Parameters:
 * fma_ctrl - pointer to the FMA descriptor to use
 * get - boolean, is this a fetching transaction?
 * mm - the PE mask mode used for address translation
 * pe - the target PE address
 * boff - the base offset to the target memory
 * woff - the window offset to the target memory
 * op - the transaction opererand (data for a PUT, GCW for a get)
 */
#define GHAL_FMA_LAUNCH(fma_ctrl, get, mm, pe, boff, woff, op)					\
do {												\
	uint64_t goff = ((get) ? _GHAL_FMA_GET_GET_GLOBAL_OFFSET((pe), (mm), (boff), (woff)) :	\
				 _GHAL_FMA_GET_PUT_GLOBAL_OFFSET((pe), (mm), (boff), (woff)));	\
	GHAL_FMA_LAUNCH_GLOBAL_OFFSET((fma_ctrl), (get), goff, (op));				\
} while(0)

/* Set PE mask */
#define GHAL_FMA_SET_PEMASK(desc, value) desc.pe_mask_mode = value
/* Set CQ Handle */
#define GHAL_FMA_SET_CQ(desc, value) desc.src_cqh = value
#define GHAL_FMA_CLEAR_CQ(desc) desc.src_cqh = 0
/* Set/Clear VMDH enable */
#define GHAL_FMA_SET_VMDH_ENABLE(desc)
#define GHAL_FMA_CLEAR_VMDH_ENABLE(desc)
/* Set/Clear NTT enable */
#define GHAL_FMA_SET_NTT_ENABLE(desc) desc.ntt_en = 1
#define GHAL_FMA_CLEAR_NTT_ENABLE(desc) desc.ntt_en = 0
/* Set PE base */
#define GHAL_FMA_SET_PE_BASE(desc, value) desc.pe_base = value
/* Set NTT group size */
#define GHAL_FMA_SET_NPES(desc, value) desc.npes = value
/* DLA items */
#define GHAL_FMA_SET_DLA_CQ(desc, value)    desc.dla_cqh = value
#define GHAL_FMA_SET_DLA_MODE(desc, cd, cdh, pr)	\
  do {							\
    desc.dla_alloc_cd = (cd); desc.dla_high_priority = (cdh); desc.dla_alloc_pr = (pr);	\
  } while(0)
#define GHAL_FMA_CLEAR_DLA_MODE(desc)       GHAL_FMA_SET_DLA_MODE(desc, 0, 0, 0)
#define GHAL_FMA_SET_DLA_CD_LOW_MODE(desc)  GHAL_FMA_SET_DLA_MODE(desc, 1, 0, 0)
#define GHAL_FMA_SET_DLA_CD_HIGH_MODE(desc) GHAL_FMA_SET_DLA_MODE(desc, 1, 1, 0)
#define GHAL_FMA_SET_DLA_PR_MODE(desc)      GHAL_FMA_SET_DLA_MODE(desc, 0, 0, 1)
/* Write DLA MODE to the NIC (this needs to be followed by sfence before doorbell write) */
#define GHAL_FMA_WR_DLA_MODE(fma_ctrl, desc)    ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[17] = desc.qwords[17]
/* Write BaseOffset to the NIC */
#define GHAL_FMA_WR_BASE_OFFSET(fma_ctrl, desc) ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[0] = desc.qwords[0]
/* Write Command, Src. MDH, Remote MDH, Hash/Adapt and vMDH modes to the NIC.
   It's all in the second QWORD on Aries */
#define GHAL_FMA_WR_SRMDH_MODES(fma_ctrl, desc) ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[1] = desc.qwords[1]
/* Set Remote Event */
#define GHAL_FMA_WR_DEST_CQDATA(fma_ctrl, val)  ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[5] = (val)
/* Allocate a Source Sync ID */
#define GHAL_FMA_WR_ALLOC_SEQID(fma_ctrl, val)	((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[8] = (val)
#define GHAL_FMA_ALLOC_SYNCID(fma_ctrl)		GHAL_FMA_WR_ALLOC_SEQID(fma_ctrl, GHAL_FMA_ALLOC_SEQID_TRANS_START)
#define GHAL_FMA_PR_CREATE(fma_ctrl, pr_id, pr_credits)      \
  do {                                                       \
    ghal_fma_alloc_seqid_t alloc_seqid;                      \
    ghal_fma_desc_t desc = GHAL_FMA_INIT;                    \
    alloc_seqid.qword = GHAL_FMA_ALLOC_SEQID_PR_START;       \
    alloc_seqid.block_id = (pr_id);                          \
    alloc_seqid.credits_required = (pr_credits);             \
    GHAL_FMA_SET_DLA_PR_MODE(desc);                          \
    GHAL_FMA_WR_DLA_MODE(fma_ctrl, desc);                    \
    sfence();                                                \
    GHAL_FMA_WR_ALLOC_SEQID(fma_ctrl, alloc_seqid.qword);    \
    sfence();                                                \
    GHAL_FMA_CLEAR_DLA_MODE(desc);                           \
    GHAL_FMA_WR_DLA_MODE(fma_ctrl, desc);                    \
  } while (0)
/* Write Sync Complete to the NIC */
#define GHAL_FMA_SYNC_COMPLETE(fma_ctrl, sync_mode, sync_data)     \
  do {                                                             \
    uint64_t tmp = (((uint64_t)(sync_mode) << GHAL_FMA_SYNC_TYPE_OFFSET) | (sync_data) | GHAL_FMA_SYNC_TRANS_END | GHAL_FMA_SYNC_DLA_DEALLOC); \
    ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[13] = tmp;     \
  } while (0)
#define GHAL_FMA_PR_SYNC_COMPLETE(fma_ctrl, sync_mode, sync_data)  \
  do {                                                             \
    uint64_t tmp = (((uint64_t)(sync_mode) << GHAL_FMA_SYNC_TYPE_OFFSET) | (sync_data) | GHAL_FMA_SYNC_TRANS_END); \
    ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[13] = tmp;     \
  } while (0)
/* DLA Marker Doorbell */
#define GHAL_FMA_DLA_MARKER(fma_ctrl, id)     \
  do {                                        \
    ghal_fma_dla_marker_t marker;             \
    marker.qword = 0;                         \
    marker.marker_id = (id);                  \
    ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[26] = marker.qword; \
  } while (0)
/* DLA Marker Doorbell - DEALLOC reservation */
#define GHAL_FMA_DLA_MARKER_DEALLOC(fma_ctrl) \
  do {                                        \
    ghal_fma_dla_marker_t marker;             \
    marker.qword = 0;                         \
    marker.marker_dealloc = 1;                \
    ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[26] = marker.qword; \
  } while (0)
/* Write remote flag value to the offset in the remote MDD */
#define GHAL_FMA_WR_REMOTE_FLAG(fma_ctrl, offset, value)           \
  do {                                                             \
    ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[3] = value;    \
    ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[4] = offset;   \
  } while(0)
/* Set AMO operand to register 0 */
#define GHAL_FMA_WR_SCRATCH0(fma_ctrl, operand) ((volatile ghal_fma_desc_t *) fma_ctrl)->amo_oprnd1 = operand
/* Set AMO operand to register 1 */
#define GHAL_FMA_WR_SCRATCH1(fma_ctrl, operand) ((volatile ghal_fma_desc_t *) fma_ctrl)->amo_oprnd2 = operand
/* Write event for the remote CQ */
#define GHAL_FMA_WR_REMOTE_CQ_EVENT(fma_ctrl, value) ((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[9] = value

/* Dealing with error flags and counters: read fma_status using GHAL_FMA_RD_STATUS, extract information
 * using GHAL_FMA_IS_ERROR, GHAL_FMA_GET_xxx_COUNTER and then clear with GHAL_FMA_CLEAR_STATUS */
/* Read Error flags and counters.*/
#define GHAL_FMA_RD_STATUS(fma_ctrl) readq(&((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[16])
/* Check for Errors (for err_mask use: GHAL_FMA_FLAG_SNCID_ALLOC_ERROR,... eg. )*/
#define GHAL_FMA_IS_ERROR(fma_status, err_mask) (((fma_status) >> GHAL_FMA_FLAG_SHIFT) & (err_mask))
/* Get WC, RC and SSID_IN_USE counters */
#define GHAL_FMA_GET_WC_COUNTER(fma_status, err_mask) ((fma_status) & GHAL_FMA_COUNTER_MASK)
#define GHAL_FMA_GET_RC_COUNTER(fma_status, err_mask) (((fma_status) >> GHAL_FMA_RC_SHIFT) & GHAL_FMA_COUNTER_MASK)
#define GHAL_FMA_GET_SSID_COUNTER(fma_status, err_mask) (((fma_status) >> GHAL_FMA_SSID_SHIFT) & GHAL_FMA_SSID_MASK)
/* Clear Error flags and counters */
#define GHAL_FMA_CLEAR_STATUS(fma_ctrl) writeq(0, &((volatile ghal_fma_desc_t *) fma_ctrl)->qwords[16])
#define GHAL_FMA_COPY_QW1(to, from)                 (to).qwords[1] = (from).qwords[1]
/* FMA Operations */
#define GHAL_FMA_GET            0x000
#define GHAL_FMA_PUT            0x000
#define GHAL_FMA_PUT_MSG        0x001
#define GHAL_FMA_PUT_S          0x002

/* FMA's FLBTE descriptor */
#define GHAL_FLBTE_INIT                             {{0}}
/* QW0 (0x00) */
#define GHAL_FLBTE_SET_DEST(desc, dest)             (desc).logical_dest = (dest)
#define GHAL_FLBTE_SET_REM_MEMOFF(desc, value)      (desc).rem_mem_offset = (value)
/* QW1 (0x08) */
#define GHAL_FLBTE_SET_BTE_FENCE(desc)              (desc).bte_fence = 1
#define GHAL_FLBTE_SET_PRIV(desc)                   (desc).privileged = 1
#define GHAL_FLBTE_SET_BTE_CHAN(desc,value)         (desc).bte_chan = value
#define GHAL_FLBTE_SET_ENQ_STATUS_CQ(desc, cq_hndl) (desc).enq_status_cqh = (cq_hndl)
#define GHAL_FLBTE_CLR_ENQ_STATUS_CQ(desc)          (desc).enq_status_cqh = 0
#define GHAL_FLBTE_SET_COHERENT(desc)               (desc).rd_no_snoop = 0
#define GHAL_FLBTE_SET_PASSPW(desc)                 (desc).rd_ro = 1
#define GHAL_FLBTE_SET_NAT(desc)                    (desc).nat_en = 1
#define GHAL_FLBTE_CLEAR_NAT(desc)                  (desc).nat_en = 0
#define GHAL_FLBTE_SET_BTE_OP(desc, value)          (desc).bte_op = (value)
/* QW3 (0x18) */
#define GHAL_FLBTE_SET_LOC_PHADDR(desc, value)      (desc).loc_phys_addr = (value)
#define GHAL_FLBTE_SET_LOC_MEMOFF(desc, value)      (desc).loc_mem_offset = (value)
/* QW4 (0x20) */
#define GHAL_FLBTE_SET_LENGTH(desc, value)          (desc).xfer_len = (value)
/* QW10 (0x50) - FLBTE doorbell */
#define GHAL_FLBTE_SET_RPT_ENQ_STATUS(desc)         (desc).flbte_rpt_enq_status = 1
#define GHAL_FLBTE_SET_BTE_IMM(desc)                (desc).flbte_bte_immediate = 1
#define GHAL_FLBTE_SET_CQ_GLOBAL(desc)              (desc).flbte_src_ssid_cq_en = 1
#define GHAL_FLBTE_SET_CQ_LOCAL(desc)               (desc).flbte_src_bte_cq_en = 1
#define GHAL_FLBTE_SET_SRCDATA(desc, value)         (desc).flbte_src_cq_data = (value)

/* Write FLBTE fields to FMA control */
#define GHAL_FMA_WR_FLBTE(fma_ctrl, desc)                                     \
do {                                                                          \
  ((volatile ghal_flbte_desc_t *)(fma_ctrl))->qwords[0] = (desc).qwords[0];   \
  ((volatile ghal_flbte_desc_t *)(fma_ctrl))->qwords[1] = (desc).qwords[1];   \
  ((volatile ghal_flbte_desc_t *)(fma_ctrl))->qwords[3] = (desc).qwords[3];   \
  ((volatile ghal_flbte_desc_t *)(fma_ctrl))->qwords[4] = (desc).qwords[4];   \
} while(0)
/* Write FLBTE doorbell */
#define GHAL_FMA_LAUNCH_BTE(desc_ptr,desc_cpy)    ((volatile ghal_flbte_desc_t *)(desc_ptr))->qwords[10] = (desc_cpy).qwords[10]

/* GCW (GET CONTROL WORD)  */
#define GHAL_GCW_INIT {0}
/* Write cmd */
#define GHAL_GCW_SET_CMD(gcw, command) ((volatile ghal_gcw_t *)&gcw)->cmd = (command & 0x7f)
/* Write count */
#define GHAL_GCW_SET_COUNT(gcw, cnt) ((volatile ghal_gcw_t *)&gcw)->count = cnt
/* Write local offset */
#define GHAL_GCW_SET_LOCAL_OFFSET(gcw, loc_off) ((volatile ghal_gcw_t *)&gcw)->local_offset = (uint64_t)(loc_off)
/* Set GB bit offset */
#define GHAL_GCW_SET_GB(gcw) ((volatile ghal_gcw_t *)&gcw)->gb = 1
/* Set FMA GCW Flagged Response bit */
#define GHAL_GCW_SET_FR(gcw) ((volatile ghal_gcw_t *)&gcw)->fr = 1

/* CQ Destriptor */
#define GHAL_CQ_INIT {{0}}
/* Set CQEnable flag */
#define GHAL_CQ_SET_ENABLE(desc) desc.en = 1
/* Set IRQReqIndex */
#define GHAL_CQ_SET_IRQ_IDX(desc, value) desc.irq_idx = value
/* Set IRQThreshIndex */
#define GHAL_CQ_SET_DELAY_CNT(desc, value) desc.irq_thresh_idx = value
/* Set CQMaxIndex */
#define GHAL_CQ_SET_MAX_CNT(desc, value) desc.max_idx = value
/* Set IRQReq */
#define GHAL_CQ_SET_IRQ_REQ(desc) desc.irq_req = 1
/* Set CQAddr */
#define GHAL_CQ_SET_ADDR(desc, value) desc.mdd0_base_addr_47_12 = ((value) >> 12)
/* Set IOMMU enable */
#define GHAL_CQ_SET_IOMMU(desc) desc.mdd0_mmu_en = 1
/* Set TCR */
#define GHAL_CQ_SET_TCR(desc, tcr) desc.mdd0_mmu_tc = tcr
/* Write CQ ReadIndex */
#define	GHAL_CQ_WRITE_RDINDEX(rd_index_ptr, read_index)	*rd_index_ptr = read_index
/* Check if CQ event has flag READY set */
#define GHAL_CQ_IS_EVENT_READY(event) (((event) >> GHAL_CQE_SOURCE_SHIFT) & GHAL_CQE_SOURCE_MASK)
/* Set ready bit in CQ event - only needed for xd1 emulation */
#define GHAL_CQ_SET_EVENT_READY(event)
/* Check if CQ source is BTE */
#define GHAL_CQ_SOURCE_BTE(event) (GHAL_CQ_IS_EVENT_READY(event) == GHAL_CQE_SOURCE_BTE)
/* Check if CQ source is SSID on a local side */
#define GHAL_CQ_SOURCE_SSID(event) (GHAL_CQ_IS_EVENT_READY(event) & GHAL_CQE_SOURCE_SSID_MASK)
/* Check if this is a remote event */
#define GHAL_CQ_SOURCE_RMT(event) (GHAL_CQ_IS_EVENT_READY(event) >= GHAL_CQE_SOURCE_RMT_SREQ)

#define GHAL_CQ_GET_INFO(event) (((event) >> GHAL_CQE_INFO_SHIFT) & GHAL_CQE_INFO_MASK)
/* Check if this is a FLBTE ENQ_STATUS INFO */
#define GHAL_CQ_INFO_ENQ_STATUS(event) (GHAL_CQ_GET_INFO(event) == GHAL_CQE_INFO_ENQ_STATUS)

/* CQE macros for new values that don't exist on other architectures */
#define GHAL_CQE_STATUS_IS_DLA_OVERFLOW(status)	(status == GHAL_CQE_STATUS_DLA_OVERFLOW)

/* Memory Domain Destriptor */
#define GHAL_MDD_INIT {{0}}
/* Memory Base address */
#define GHAL_MDD_SET_MEMBASE(desc, value) desc.mem_base = ((value) >> 12)
/* Protection Tag */
#define GHAL_MDD_SET_PTAG(desc, value) desc.ptag = value
/* CQ handle */
#define GHAL_MDD_SET_CQHNDL(desc, value) desc.cqh = value
/* Max Base address */
#define GHAL_MDD_SET_MAX_MBASE(desc, value) desc.mem_max = ((value) >> 12)
#define GHAL_MDD_SET_COHERENT(desc) desc.no_snoop = 0
#define GHAL_MDD_SET_RO_ALLOW(desc) (desc).use_rc = (1)
#define GHAL_MDD_SET_RO_ENABLE(desc) (desc).wr_ro = (1); (desc).rd_ro = (1); (desc).resp_ro = (1)
#define GHAL_MDD_SET_FLUSH(desc) desc.flush_complete = 1
#define GHAL_MDD_SET_VALID(desc) desc.valid = 1
#define GHAL_MDD_SET_WRITABLE(desc) desc.writable = 1

#define GHAL_MDD_SET_IOMMU_ENABLE(desc, tcr) do { desc.mmu_en = 1; desc.mmu_tc = tcr; } while(0)
#define GHAL_MDD_SET_PCI_IOMMU_ENABLE(desc, tcr) do { desc.mmu_en = 0; desc.mmu_tc = tcr; } while(0)

#define GHAL_MDD_SET_MRT_ENABLE(desc)
#define GHAL_MDD_SET_MRT_DISABLE(desc)
#define GHAL_GART_GET_ADDR(entry) 0
#define GHAL_MRT_GET_ADDR(entry, shift) 0
#define GHAL_IOMMU_GET_ADDR(entry, shift) ((entry) << (shift))
#define GHAL_PCI_IOMMU_GET_ADDR(entry) ((entry) << PAGE_SHIFT)

#define GHAL_MRT_SIZE_TO_CODE(size) 0
#define GHAL_MRT_CODE_TO_SIZE(code) 0
#define GHAL_MRT_CODE_TO_SHIFT(code) 0
#define GHAL_MRT_CODE_TO_MASK(code) 0

#define GHAL_PE_ADDR_TO_DST(pe_addr)            ((pe_addr) >> 2)
#define GHAL_PE_ADDR_TO_DSTID(pe_addr)          ((pe_addr) & 0x3)

/* VCE Descriptor */
#define GHAL_VCE_INIT                           {{{0}}}
#define GHAL_VCE_SET_PKEY(vce,_pkey)            (vce).ibase.pkey = (_pkey)
#define _GHAL_VCE_SET_ROUND_MODE(vce,mode)      (vce).ibase.fp_rounding_mode = (mode)
#define GHAL_VCE_SET_ROUND_MODE_NEAR(vce)       _GHAL_VCE_SET_ROUND_MODE((vce),GHAL_VCE_ROUND_MODE_NEAR)
#define GHAL_VCE_SET_ROUND_MODE_DOWN(vce)       _GHAL_VCE_SET_ROUND_MODE((vce),GHAL_VCE_ROUND_MODE_DOWN)
#define GHAL_VCE_SET_ROUND_MODE_UP(vce)         _GHAL_VCE_SET_ROUND_MODE((vce),GHAL_VCE_ROUND_MODE_UP)
#define GHAL_VCE_SET_ROUND_MODE_ZERO(vce)       _GHAL_VCE_SET_ROUND_MODE((vce),GHAL_VCE_ROUND_MODE_ZERO)
#define GHAL_VCE_SET_CHILD_CNT(vce,cnt)         (vce).ibase.number_of_children = (cnt)
#define GHAL_VCE_SET_ROOT(vce,root)             (vce).obase.is_root = (root)
#define GHAL_VCE_SET_PTAG(vce,_ptag)            (vce).obase.ptag = (_ptag)
#define GHAL_VCE_SET_DST(vce,_dst)              (vce).obase.dst = (_dst)
#define GHAL_VCE_SET_DSTID(vce,_dstid)          (vce).obase.dstid = (_dstid)
#define GHAL_VCE_SET_NTT(vce)                   (vce).obase.ntt = 1
#define GHAL_VCE_CLR_NTT(vce)                   (vce).obase.ntt = 0
#define GHAL_VCE_SET_CQ(vce,cq)                 (vce).obase.ssid_lcqh = (cq)
#define GHAL_VCE_CLR_CQ(vce)                    (vce).obase.ssid_lcqh = 0
#define _GHAL_VCE_SET_NSRC(vce,nsrc)            (vce).obase.ssid_nsrc = (nsrc)
#define GHAL_VCE_SET_NSRC_COMP(vce)             _GHAL_VCE_SET_NSRC((vce),GHAL_VCE_CQE_MODE_COMP)
#define GHAL_VCE_SET_NSRC_ERR(vce)              _GHAL_VCE_SET_NSRC((vce),GHAL_VCE_CQE_MODE_ERR)
#define _GHAL_VCE_SET_RC(vce,mode)              (vce).obase.rc_2_0 = (mode)
#define GHAL_VCE_SET_RC_NMIN_HASH(vce)          _GHAL_VCE_SET_RC((vce),A_RC_NON_MIN_HASHED)
#define GHAL_VCE_SET_RC_MIN_HASH(vce)           _GHAL_VCE_SET_RC((vce),A_RC_MIN_HASHED)
#define GHAL_VCE_SET_RC_MNON_HASH(vce)          _GHAL_VCE_SET_RC((vce),A_RC_MIN_NON_HASHED)
#define GHAL_VCE_SET_RC_ADAPT(vce,mode)         _GHAL_VCE_SET_RC((vce),_GHAL_RC_ADAPT_MODE(mode))
#define GHAL_VCE_SET_CEID(vce,ceid)             (vce).obase.ce_id = (ceid)
#define GHAL_VCE_SET_CHID(vce,chid)             (vce).obase.child_id = (chid)
#define _GHAL_VCE_SET_CHILD_TYPE(vce,child,type)                        \
  do {                                                                  \
    if ((child) % 2) (vce).chnl[(child) / 2].child1_type = (type);      \
    else             (vce).chnl[(child) / 2].child0_type = (type);      \
  } while (0)
#define GHAL_VCE_SET_CHILD_TYPE_UN(vce,child)   _GHAL_VCE_SET_CHILD_TYPE(vce,child,GHAL_VCE_CHILD_UN)
#define GHAL_VCE_SET_CHILD_TYPE_PE(vce,child)   _GHAL_VCE_SET_CHILD_TYPE(vce,child,GHAL_VCE_CHILD_PE)
#define GHAL_VCE_SET_CHILD_TYPE_VCE(vce,child)  _GHAL_VCE_SET_CHILD_TYPE(vce,child,GHAL_VCE_CHILD_VCE)
#define GHAL_VCE_SET_CHILD_CEID(vce,child,ce_id)                        \
  do {                                                                  \
    if ((child) % 2) (vce).chnl[(child) / 2].ce_id_1 = (ce_id);         \
    else             (vce).chnl[(child) / 2].ce_id_0 = (ce_id);         \
  } while (0)
#define GHAL_VCE_SET_CHILD_NTT(vce,child,ntt)                           \
  do {                                                                  \
    if ((child) % 2) (vce).chnl[(child) / 2].ntt_1 = (ntt);             \
    else             (vce).chnl[(child) / 2].ntt_0 = (ntt);             \
  } while (0)
#define GHAL_VCE_SET_CHILD_DST(vce,child,dst)                           \
  do {                                                                  \
    if ((child) % 2) (vce).chnl[(child) / 2].dst_1 = (dst);             \
    else             (vce).chnl[(child) / 2].dst_0 = (dst);             \
  } while (0)
#define GHAL_VCE_SET_CHILD_DSTID(vce,child,dstid)                       \
  do {                                                                  \
    if ((child) % 2) (vce).chnl[(child) / 2].dstid_1 = (dstid);         \
    else             (vce).chnl[(child) / 2].dstid_0 = (dstid);         \
  } while (0)

/* CE floating point exception definitions */
#define GNI_FMA_CE_FPE_OP_INVAL          0x1
#define GNI_FMA_CE_FPE_OFLOW             0x2
#define GNI_FMA_CE_FPE_UFLOW             0x4
#define GNI_FMA_CE_FPE_PRECISION         0x8

/* FMA CE Descriptor */
#define GHAL_CEFMA_INIT                                 {{0}}
#define GHAL_CEFMA_SET_MDH(desc_cpy,mdh)                (desc_cpy).rem_mdh = (mdh)
#define GHAL_CEFMA_SET_SCATTER_OFF(desc_cpy,off)        (desc_cpy).scatter_offset = (off)
#define _GHAL_CEFMA_SET_RC(desc_cpy,mode)               (desc_cpy).rc = (mode)
#define GHAL_CEFMA_ROUTE_NMIN_HASH(desc_cpy)            _GHAL_CEFMA_SET_RC((desc_cpy),A_RC_NON_MIN_HASHED)
#define GHAL_CEFMA_ROUTE_MIN_HASH(desc_cpy)             _GHAL_CEFMA_SET_RC((desc_cpy),A_RC_MIN_HASHED)
#define GHAL_CEFMA_ROUTE_MNON_HASH(desc_cpy)            _GHAL_CEFMA_SET_RC((desc_cpy),A_RC_MIN_NON_HASHED)
#define GHAL_CEFMA_ROUTE_ADAPT(desc_cpy,mode)           _GHAL_CEFMA_SET_RC((desc_cpy),_GHAL_RC_ADAPT_MODE(mode))
#define GHAL_CEFMA_SET_DEST(desc_cpy,dest)              (desc_cpy).logical_dest = (dest)
#define GHAL_CEFMA_SET_CE_ID(desc_cpy,id)               (desc_cpy).ce_id = (id)
#define GHAL_CEFMA_SET_CHILD_ID(desc_cpy,id)            (desc_cpy).child_id = (id)
#define GHAL_CEFMA_SET_CMD(desc_cpy,cmd)                (desc_cpy).ce_op = (cmd)
#define GHAL_CEFMA_SET_OP1(desc_cpy,op1)                (desc_cpy).ce_oprnd1 = (op1)
#define GHAL_CEFMA_SET_OP2(desc_cpy,op2)                (desc_cpy).ce_oprnd2 = (op2)
#define GHAL_CEFMA_SET_OP2_EN(desc_cpy)                 (desc_cpy).cedb_use_oprnd2 = 1
#define GHAL_CEFMA_SET_RED_ID(desc_cpy,id)              (desc_cpy).cedb_reductn_id = (id)
#define _GHAL_CEFMA_SET_FPE(desc_cpy,fpe)               (desc_cpy).fp_exception = (fpe)
#define GHAL_CEFMA_SET_FPE_OP_INVAL(desc_cpy)           _GHAL_CEFMA_SET_FPE((desc_cpy),GNI_FMA_CE_FPE_OP_INVAL)
#define GHAL_CEFMA_SET_FPE_OFLOW(desc_cpy)              _GHAL_CEFMA_SET_FPE((desc_cpy),GNI_FMA_CE_FPE_OFLOW)
#define GHAL_CEFMA_SET_FPE_UFLOW(desc_cpy)              _GHAL_CEFMA_SET_FPE((desc_cpy),GNI_FMA_CE_FPE_UFLOW)
#define GHAL_CEFMA_SET_FPE_PRECISION(desc_cpy)          _GHAL_CEFMA_SET_FPE((desc_cpy),GNI_FMA_CE_FPE_PRECISION)
#define GHAL_CEFMA_WRITE_FMAD(desc_ptr,desc_cpy)                                        \
do {                                                                                    \
  ((volatile ghal_ce_desc_t *)(desc_ptr))->qwords[0] = (desc_cpy).qwords[0];            \
  ((volatile ghal_ce_desc_t *)(desc_ptr))->qwords[1] = (desc_cpy).qwords[1];            \
  ((volatile ghal_ce_desc_t *)(desc_ptr))->qwords[3] = (desc_cpy).qwords[3];            \
  ((volatile ghal_ce_desc_t *)(desc_ptr))->qwords[4] = (desc_cpy).qwords[4];            \
  ((volatile ghal_ce_desc_t *)(desc_ptr))->qwords[5] = (desc_cpy).qwords[5];            \
} while(0)
#define GHAL_CEFMA_SEQ_BEGIN(desc_ptr)                  GHAL_FMA_ALLOC_SYNCID((desc_ptr))
#define GHAL_CEFMA_SEQ_COMPLETE(desc_ptr,mode,data)     GHAL_FMA_SYNC_COMPLETE((desc_ptr),(mode),(data))
#define GHAL_CEFMA_SEQ_COMPLETE_PR(desc_ptr,mode,data)  GHAL_FMA_PR_SYNC_COMPLETE((desc_ptr),(mode),(data))
#define GHAL_CEFMA_CE_LAUNCH(desc_ptr,desc_cpy)         ((volatile ghal_ce_desc_t *)(desc_ptr))->qwords[11] = (desc_cpy).qwords[11]

/* translate an index [0-3] to an adaptive routing mode value */
#define _GHAL_RC_ADAPT_MODE(mode)		\
	(((mode) == 0) ? A_RC_ADAPTIVE_0 :	\
	 ((mode) == 1) ? A_RC_ADAPTIVE_1 :	\
	 ((mode) == 2) ? A_RC_ADAPTIVE_2 :	\
	 ((mode) == 3) ? A_RC_ADAPTIVE_3 :	\
	 A_RC_ADAPTIVE_3)


#endif /*_GHAL_PUB_H_*/

