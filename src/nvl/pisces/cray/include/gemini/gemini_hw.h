/*
 *      The Gemini hardware definitions.
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

#ifndef _GEMINI_HW_H_
#define _GEMINI_HW_H_ 

#undef  PCI_VENDOR_ID_CRAY
#define PCI_VENDOR_ID_CRAY              0x17DB 
#define PCI_DEVICE_ID_CRAY_GEMINI_1     0x0201

#define GHAL_MAX_PCI_DEVS 1
 /* forwards compat */
#define GHAL_PCI_SYSTEM_DEVICE 0
#define GHAL_PCI_IOMMU_DEVICE GHAL_MAX_PCI_DEVS

/* alignment requirements for FMA and RDMA operations */
#define GHAL_LOCAL_ADDR_ALIGNMENT 3   /* 4 byte */
#define GHAL_ADDR_ALIGNMENT       3   /* 4 byte */
#define GHAL_AMO_ALIGNMENT        7   /* 8 byte */
#define GHAL_AAX_ALIGNMENT        15  /* 16 byte */

/* PE is 18 bits*/
#define GHAL_PE_MASK    0x3FFFF

/* Gemini Address Map */
#define GHAL_LOCAL_BLOCK_OFFSET         0x0000000000UL  /* see Local Block below */ 
#define GHAL_LOCAL_BLOCK_SIZE           0x6100000UL
#define GHAL_CQ_DESC_USER_OFFSET        0x0800000000UL  /* CQ descriptors alias */
#define GHAL_CQ_USER_OFF_BYID(id)       ((id) << 12)    /* CQ user space alias offset */
#define GHAL_CQ_DESC_USER_PITCH         GHAL_CQ_USER_OFF_BYID(1)
#define GHAL_FMA_DESC_USER_OFFSET       0x0800400000UL  /* FMA descriptors alias */
#define GHAL_FMA_DESC_OFFSET            0x0800480000UL  /* FMA descriptors */
#define GHAL_FMA_DESC_PRVMASK_OFFSET    0x80            /* FMA descriptors private mask */
#define GHAL_FMA_OFF_BYID(id)           ((id) << 12)
#define GHAL_FMA_DESC_PITCH             GHAL_FMA_OFF_BYID(1) 
#define GHAL_FMA_PUTWC_OS_OFFSET        0x0d00000000UL  /* OS PUT WC window */
#define GHAL_FMA_GET_OS_OFFSET          0x0e00000000UL  /* OS GET window */
#define GHAL_FMA_PUTUC_OS_OFFSET        0x0f00000000UL  /* OS PUT UC window */
#define GHAL_FMA_PUTWC_OFFSET           0x1000000000UL  /* PUT WC window */
#define GHAL_FMA_GET_OFFSET             0x2000000000UL  /* GET window */
#define GHAL_FMA_PUTUC_OFFSET           0x3000000000UL  /* PUT UC window */
#define GHAL_FMA_WINOFF_BYID(id)        ((uint64_t)(id) << 30)  /* 1GB window */

#define GHAL_VMDH_BASE_MASK             0xFFF
#define GHAL_VMDH_ENABLE_BIT            0x8000

/* Available HW Resources */
#define GHAL_CQ_MAX_NUMBER              1024            /* number of CQ descriptors */
#define GHAL_FMAOS_MAX_NUMER            4               /* number of FMA descriptors dedicated to OS */
#define GHAL_FMA_MAX_NUMBER             64              /* number of general FMA descriptors */
#define GHAL_FMA_TOTAL_NUMBER           (GHAL_FMAOS_MAX_NUMER + GHAL_FMA_MAX_NUMBER)
#define GHAL_BTE_TX_MAX_NUMBER          1023            /* number of BTE TX descriptors (last descriptor is reserved for HSS)*/
#define GHAL_BTE_RX_MAX_NUMBER          1024            /* number of BTE TX descriptors */
#define GHAL_BTE_CHNL_MAX_NUMBER        4               /* number of BTE channels */
#define GHAL_BTE_MAX_LENGTH             ((1UL << 32) - 1)
#define GHAL_MDD_MAX_NUMBER             (4*1024)        /* number of Mem.Domain descriptors */
#define GHAL_MRT_MAX_NUMBER             (16*1024)       /* number of MRT entries */
#define GHAL_NTT_MAX_NUMBER             (8*1024)        /* number of NTT entries */
#define GHAL_VMDH_MAX_NUMBER            (256)           /* number of vMDH entries */
#define GHAL_PTAG_MAX_NUMBER            (256)           /* number of PTAGs available (0 is invalid) */
#define GHAL_ORB_MAX_NUMBER             (1024)          /* number of ORB entries */
#define GHAL_SSID_MAX_NUMBER            (255)           /* number of SSID entries */
#define GHAL_VCE_MAX_NUMBER             (0)             /* number of VCE channels */

/* Local Block */
#define GHAL_HT_CONFIG_OFFSET           0x0000000
#define GHAL_NODE_SUBID_OFFSET          0x000002e
#define GHAL_IOAPIC_REGS_OFFSET         0x0000100
#define GHAL_IOAPIC_STATUS_OFFSET       (GHAL_IOAPIC_REGS_OFFSET + 0x18)
#define GHAL_HPET_CTRL_OFFSET           0x0010000
#define GHAL_HPET_FRQ_OFFSET            0x00120a0
#define GHAL_RTC_FRQ_OFFSET             0x00120b0
#define GHAL_DEADLOCK_FLWCTRL_OFFSET    0x0148
#define GHAL_DEADLOCK_RECOVERY_OFFSET   0x0150


#define GHAL_NTT0_OFFSET                0x2100000
#define GHAL_NTT1_OFFSET                0x2110000
#define GHAL_NTT_OFF_BYID(id)           ((id) << 3)
#define GHAL_NTT_ADDR_MASK_WRBOTH       0x0008000
#define GHAL_NTT_PITCH                  8               /* size of the single entry */
#define GHAL_NODE_ID_OFFSET             0x2120000
#define GHAL_NTT_CONFIG                 0x2120008

#define GHAL_BTE_TX_OFFSET              0x4000000
#define GHAL_BTE_TX_OFF_BYID(id)        ((id) << 6)
#define GHAL_BTE_TX_PITCH               GHAL_BTE_TX_OFF_BYID(1)
#define GHAL_BTE_CPUTX_OFF_BYID(id)     ((id) << 6)
#define GHAL_BTE_CPUTX_PITCH            GHAL_BTE_CPUTX_OFF_BYID(1)
#define GHAL_BTE_TCHNL_OFFSET           0x4020000
#define GHAL_BTE_TCHNL_OFF_BYID(id)     ((id) << 6)   
#define GHAL_BTE_TCHNL_PITCH            GHAL_BTE_TCHNL_OFF_BYID(1)
/* Base descriptor index from which TX VC ring will begin */
#define GHAL_BTE_TCHNL_BASE_OFFSET      0x4040050           
/* Number of descriptors in the TX ring */
#define GHAL_BTE_TCHNL_COUNT_OFFSET     0x4040058           
/* BTE channel performance counters (one per channel) */
#define GHAL_BTE_TX_CNTRS_OFFSET        0x4020020
#define GHAL_BTE_TX_CNTRS_PITCH         64
#define GHAL_BTE_TCHNL_NPOPCTRL_OFFSET  0x40400E0
#define GHAL_BTE_TCHNL_NPOPCTRL_OFF_BYID(id) ((id) << 3)

#define GHAL_MRT_ENCODE(addr) (((addr >> 15) & ~0x3) | 0x1)
#define GHAL_MRT_DECODE(value) (((uint64_t)(value) & ~0x3LL) << 15)

#define GHAL_MRT_OFFSET                 0x4400000           
#define GHAL_MRT_OFF_BYID(id)           ((id) << 2)           
#define GHAL_MDD_OFFSET                 0x4410000
#define GHAL_MDD_OFF_BYID(id)           ((id) << 4)           
#define GHAL_VMDH_OFFSET                0x4420000   
#define GHAL_MRT_PAGESIZE_OFFSET        0x4420400
#define GHAL_MRT_PAGESIZE_MASK          0x7
#define GHAL_NAT_PARAM_OFFSET           0x4420408
#define GHAL_MRT_INDUCE_ERROR_OFFSET    0x4420410
#define GHAL_MDD_INDUCE_ERROR_OFFSET    0x4420418
#define GHAL_VMDH_INDUCE_ERROR_OFFSET   0x4420420
#define GHAL_CQ_OFFSET                  0x4800000           
#define GHAL_CQ_OFF_BYID(id)            ((id) << 5)

#define GHAL_BTE_RX_OFFSET              0x4c00000
#define GHAL_BTE_RX_OFF_BYID(id)        ((id) << 3)
#define GHAL_BTE_RX_PITCH               GHAL_BTE_RX_OFF_BYID(1)
#define GHAL_BTE_CPURX_OFF_BYID(id)     ((id) << 5)
#define GHAL_BTE_CPURX_PITCH            GHAL_BTE_CPURX_OFF_BYID(1)
#define GHAL_BTE_RCHNL_OFFSET           0x4c02000           
#define GHAL_BTE_RCHNL_OFF_BYID(id)     ((id) << 6)       
#define GHAL_BTE_RCHNL_PITCH            GHAL_BTE_RCHNL_OFF_BYID(1)
/* PUT Sequence Performance counters */
#define GHAL_BTE_RX_PUTCNTRS_OFFSET     0x4c02020           
/* SEND Sequence Performance counters */
#define GHAL_BTE_RX_SENDCNTRS_OFFSET    0x4c02060           
/* RMT and RX Error flags */
#define GHAL_BTE_RX_ERRFLAGS_OFFSET     0x4c04000    
/* RMT and RX Error Clear regsiter */
#define GHAL_BTE_RX_ERRCLEAR_OFFSET     0x4c04008    
/* Base descriptor index from which RX VC ring will begin */
#define GHAL_BTE_RCHNL_BASE_OFFSET      0x4c04060           
/* Number of descriptors in the RX ring */
#define GHAL_BTE_RCHNL_COUNT_OFFSET     0x4c04068           

#define GHAL_RAT_OFFSET                0x5980000
#define GHAL_RAT_LINKACTIVE_OFFSET     0x5980008
#define GHAL_RAT_LINKACTIVE_ENABLE     0x1
#define GHAL_RAT_LINKACTIVE_DISABLE    0

#define GHAL_SSID_OFFSET               0x5600000
#define GHAL_SSID_SCRUBCTRL_OFFSET     0x5600028
#define GHAL_SSID_SCRUBCTRL_EPOCH_DEF  0x13

/* If an SSID entry becomes stale, the minimum duration between when the SSID
 * entry enters the "waiting" state and is determined to be stale is 6 full
 * periods of the epoch counter tick rate.  The maximum duration is 8 full
 * periods.  Maximum duration timeout value is in milliseconds.  We now add 100
 * milliseconds padding. */
#define GHAL_SSID_SCRUB_TIMEOUT(epoch_period) \
                (100 + (8 * ((2 * (1 << epoch_period)) / 1000)))

typedef struct ghal_ssid_scrubber_ctl_status_desc {
        uint64_t scrubberdisable:1;     /* When set, the SSID Scrubber will not detect or de-allocate stale SSID entries */
        uint64_t epochperiod:5; /* This field controls the rate at which the SSID Scrubber Epoch counter increments */
        uint64_t undefine_7_6:2;        /* undefined */
        uint64_t currentepoch:3;        /* The current value of the SSID Scrubber Epoch counter */
        uint64_t undefine_15_11:5;      /* undefined */
        uint64_t nextssid:8;    /* The ID of the next SSID entry that will be checked to see if it is stale */
        uint64_t undefine_63_24:40;     /* undefined */
} ghal_ssid_scrubber_ctl_status_desc_t;

#define GHAL_ORB_OFFSET                0x5000000
#define GHAL_ORB_SCRUBCTRL_OFFSET      0x5008010
/* minimum time before an entry will be considered stale is 1 sec.  (Max is 2 sec.)*/
#define GHAL_ORB_SCRUBCTRL_RATE        0x9af8e 
typedef struct ghal_orb_scrub_ctrl_desc {
        uint64_t ord_scrub_disable:1;   /* When set the ORD Scrubber is disabled */
        uint64_t ord_scrub_rate:23;     /* Sets the maximum value to which the counter in the ORD (Outstanding Request Data */
        uint64_t ord_next_scrub_addr:10;        /* The value of the last address checked to see if a scrub is needed */
        uint64_t ord_current_epoch:2;   /* The current value of the ORD Scrubber Epoch */
        uint64_t ord_stale_ord_cnt_ovf:1;       /* Indicates ord_stale_ord_cnt has overflowed at least once */
        uint64_t ord_stale_ord_cnt:16;  /* Counts the number of stale entries that have been scrubbed */
        uint64_t undefine_63_53:11;     /* undefined */
} ghal_orb_scrub_ctrl_desc_t;

#define GHAL_RMT_OFFSET                0x4c02100
#define GHAL_RMT_CAM_PARAM0_OFFSET     0x4c04048
#define GHAL_RMT_CAM_PARAM1_OFFSET     0x4c04050
typedef struct ghal_rmt_cfg_cam_params0_desc {
        uint64_t amo_credits:8; /* AMO credits */
        uint64_t cq_credits:8;  /* The MAX number of outstanding CQEs from the RMT */
        uint64_t undefine_59_16:44;     /* undefined */
        uint64_t scrubbing_bubbles:1;   /* Inject IDLE flits into RMT pipeline instead of actively searching for flits long */
        uint64_t memory_power_save:1;   /* Enables use of Memory Enable (ME) pins on Virage memories */
        uint64_t stall_on_msgcmp:1;     /* Stall until MsgComplete and byte PUT */
        uint64_t rmt_enable:1;  /* RMT Enable */
} ghal_rmt_cfg_cam_params0_desc_t;

/* an entry is scrubbed from the RMT if 500 milliseconds elapses without another packet for that same entry arriving */
#define GHAL_RMT_TIMEOUT               500 
typedef struct ghal_rmt_cfg_cam_params1_desc {
        uint64_t evict_threshold:11;    /* Send Eviction Threshold */
        uint64_t undefine_15_11:5;      /* undefined */
        uint64_t max_cam_entries:11;    /* Maximum number of active CAM entries RMT may have at any given time */
        uint64_t undefine_31_27:5;      /* undefined */
        uint64_t timeout_value:19;      /* Timeout value, in SCRUB_INTERVAL cycle counts, of a Sequence Table entry based o */
        uint64_t undefine_62_51:12;     /* undefined */
        uint64_t timeout_mode:1;        /* Sequence Table entry timeout mode */
} ghal_rmt_cfg_cam_params1_desc_t;


#define GHAL_NL_OFFSET                 0x2120000
#define GHAL_NL_PARAMS_OFFSET          0x2120010
typedef struct ghal_nl_params_desc {
        uint64_t vc0_credits:8; /* Nettile VC0 Buffer Size */
        uint64_t vc1_credits:8; /* Nettile VC1 Buffer Size */
        uint64_t core_req_credits:8;    /* NIC Request Buffer Size Min = 0x6, Max = 0x20 */
        uint64_t core_rsp_credits:12;   /* NIC Response Buffer Size 8 h0 = no backpressure, If non-zero, Min = 0x6, Max = 0 */
        uint64_t phit_req_credits:8;    /* Request arbitration to Phit Buffer Credits (Internal) */
        uint64_t phit_rsp_credits:8;    /* Response arbitration to Phit Buffer Credits (Internal) */
        uint64_t chk_nodeid_req:1;      /* Check Node ID, if mismatch, squash request packet */
        uint64_t chk_nodeid_rsp:1;      /* Check Node ID, if mismatch, squash response packet */
        uint64_t dis_ecc:1;     /* Disable NTT error detection and correction */
        uint64_t dis_ntt_kill:1;        /* Disable squashing packets if there is a Multi-bit error in the NTT */
        uint64_t dis_cmpr:1;    /* Disable compression */
        uint64_t hash_on_coreid:1;      /* Enables hashing on the two-bit Destination ID and two-bit Source ID */
        uint64_t lcb2nif_resync:1;      /* Add one more clock to the LCB2NIF resynchronization logic */
        uint64_t nif2lcb_resync:1;      /* Add one more clock to the NIF2LCB resynchronization logic */
        uint64_t disable_power_saving_ntt:1;    /* Disable Power Saving mode in NTT */
        uint64_t disable_power_saving_lcb2nif:1;        /* Disable Power Saving mode in the LCB2NIF queue s */
        uint64_t disable_power_saving_nif2lcb:1;        /* Disable Power Saving mode in the NIF2LCB queue s */
        uint64_t power_down_perf_mon:1; /* If is set, the performance counters are shut off and powered down */
} ghal_nl_params_desc_t;

/* IRQ assignment */
#define GHAL_FIRST_INBAND_IRQ           21
#define GHAL_NUM_INBAND_IRQS            32
#define GHAL_IRQ_RX                     0
#define GHAL_IRQ_TX                     1
#define GHAL_IRQ_INDEX(vc_id, rx_tx)    (GHAL_FIRST_INBAND_IRQ + ((vc_id) << 1) + (rx_tx))
#define GHAL_FIRST_CQ_IRQ               (GHAL_FIRST_INBAND_IRQ + (GHAL_BTE_CHNL_MAX_NUMBER << 1))
#define GHAL_IRQ_CQ_INDEX(idx)          (GHAL_FIRST_CQ_IRQ + (idx))
#define GHAL_IRQ_CQ_MAX_INDEX           (GHAL_NUM_INBAND_IRQS - GHAL_FIRST_CQ_IRQ)

#define GHAL_IRQ_FLWCTRL_INDEX          32

/* CQ Event format */
#define GHAL_CQE_SOURCE_SHIFT           56
#define GHAL_CQE_SOURCE_MASK            0x7
#define GHAL_CQE_STATUS_SHIFT           59
#define GHAL_CQE_STATUS_MASK            0xF
#define GHAL_CQE_OVERRUN_SHIFT          63
#define GHAL_CQE_SOURCE_SSID_SREQ       0x1
#define GHAL_CQE_SOURCE_SSID_DREQ       0x2
#define GHAL_CQE_SOURCE_SSID_SRSP       0x3
#define GHAL_CQE_SOURCE_SSID_MASK       0x3
#define GHAL_CQE_SOURCE_BTE             0x4
#define GHAL_CQE_SOURCE_RMT_SREQ        0x5
#define GHAL_CQE_SOURCE_RMT_DREQ        0x6
#define GHAL_CQE_SOURCE_RMT_SRSP        0x7

/* BTE TX Descriptor */
#define GHAL_BTE_TX_TIMEOUT_DISABLE     0
/* Modes from the 1st QWORD */
#define GHAL_TXD_MODE_BTE_SEND          0x0000  /* BTE Mode: BTE_SEND */
#define GHAL_TXD_MODE_BTE_CONCATENATE   0x0001  /* Concat. descr. with next */
#define GHAL_TXD_MODE_BTE_PUT           0x0002  /* BTE Mode: BTE_PUT */
#define GHAL_TXD_MODE_BTE_GET           0x0004  /* BTE Mode: BTE_GET */
#define GHAL_TXD_MODE_BTE_IMMED         0x0008  /* BTE Immediate data */
#define GHAL_TXD_MODE_BTE_FENCE         0x0010  /* BTE Fence */
#define GHAL_TXD_MODE_CQ_NONE           0x0000  /* No CQ Notification */
#define GHAL_TXD_MODE_CQ_LOCAL          0x0020  /* Gen. CQ when TXD was processed */
#define GHAL_TXD_MODE_CQ_GLOBAL         0x0040  /* Gen. CQ when data reached gl. ordered point */
#define GHAL_TXD_MODE_DT_ENABLE         0x0080  /* Dest. Type enable bit (0 for Baker)*/
#define GHAL_TXD_MODE_RADP_ENABLE       0x0100  /* En. adaptive routing for responce */
#define GHAL_TXD_MODE_ADP_ENABLE        0x0200  /* En. adaptive routing for data */
#define GHAL_TXD_MODE_HASH_ENABLE       0x0400  /* En. hash on addr. for data */
#define GHAL_TXD_MODE_OPT_DELIVERY      (GHAL_TXD_MODE_RADP_ENABLE | \
                                         GHAL_TXD_MODE_ADP_ENABLE)
#define GHAL_TXD_MODE_VMDH_ENABLE       0x0800  /* En. vMDH translation */
/* Flags from the 2nd QWORD */
#define GHAL_TXD_FLAG_COMPLETE          0x0001  /* Transmit transfer complete */
#define GHAL_TXD_FLAG_TIMEOUT_ERR       0x0002  /* Timeout Error */
/* Modes from the 2nd QWORD */
#define GHAL_TXD_MODE_IRQ_ENABLE        0x0001  /* Gen. IRQ on TX complete */
#define GHAL_TXD_MODE_IRQ_DELAY_ENABLE  0x0002  /* Delayed IRQ enabled */
/* Modes from the 4nd QWORD */
#define GHAL_TXD_MODE_COHERENT          0x0001  /* En. coherency bit twrds cpu */
#define GHAL_TXD_MODE_NP_PASSPW         0x0002  /* Use relaxed HT ordering */
#define GHAL_TXD_MODE_NAT_ENABLE        0x0004  /* Use NAT at source */
/* Status from the 5th QWORD */
#define GHAL_TXD_STATUS_OK                            0x0
#define GHAL_TXD_STATUS_UNCORRECTABLE_ERR             0x2
#define GHAL_TXD_STATUS_INV_CMD                       0x3
#define GHAL_TXD_STATUS_UNCORRECTABLE_TRANSLATION_ERR 0x5
#define GHAL_TXD_STATUS_VMDH_INV                      0x6
#define GHAL_TXD_STATUS_MDD_INV                       0x7
#define GHAL_TXD_STATUS_PROTECTION_VIOLATION          0x8
#define GHAL_TXD_STATUS_MEM_BOUNDS_ERR                0x9
#define GHAL_TXD_STATUS_SEQUENCE_TERMINATE            0xA
#define GHAL_TXD_STATUS_REQUEST_TIMEOUT               0xB
#define GHAL_TXD_STATUS_HT_DATA_ERROR                 0xD

typedef struct ghal_tx_desc {
        /* 1st QWORD */
        uint64_t        dest_pe:18;     /* Destination PE */
        uint64_t        unused1:34;
        uint64_t        mode1:12;       /* Modes (see definitions above) */
        /* 2nd QWORD */
        uint64_t        dest_mem_off:40;/* Destination memory offset */
        uint64_t        unused2:8;      
        uint64_t        dest_mem_dh:12; /* Dest. memory domain handle */
        uint64_t        flags:2;        /* Complete & Error flags */
        uint64_t        mode2:2;        /* Modes (see definitions above) */
        /* 3rd QWORD */
        uint64_t        src_cq_hndl:10; /* Src. completion queue handle */
        uint64_t        unused3:6;
        uint64_t        dest_ptag:8;    /* Dest. PTAG */
        uint64_t        src_ptag:8;     /* Src. PTAG */
        uint64_t        data_len:32;    /* Data length */
        /* 4th QWORD */
        union {
                struct {
                        uint64_t        src_mem_off:40; /* Src. mem. offset */
                        uint64_t        src_mem_dh:12;  /* Src. domain handle */
                        uint64_t        unused41:9;
                        uint64_t        mode4:3;        /* Modes (see definitions above) */     
                } nat_enb;
                struct {
                        uint64_t        src_ph_addr:48; /* Src. physical addr. */
                        uint64_t        unused42:13;
                        uint64_t        mode4:3;        /* Modes (see definitions above) */
                } nat_dsb;
        };
        /* 5th QWORD */
        uint64_t        src_cq_data:56; /* Src. CQ data */
        uint64_t        unused51:3;
        uint64_t        bte_tx_status:4;/* Filled by BTE upon completion */
        uint64_t        unused52:1;
        /* 6th QWORD */
        uint64_t        user_data;      /* Data to be deliv. to rem. CQ or Immediate data */

} ghal_tx_desc_t;

/* BTE RX Descriptor */
/* Modes from the 1st QWORD */
#define GHAL_RXD_MODE_PASSPW            0x0001  /* Relaxed HT ordering */
#define GHAL_RXD_MODE_COHERENT          0x0002  /* En. coherency bit twrds cpu */
#define GHAL_RXD_MODE_IRQ_DELAY_ENABLE  0x4000  /* Delayed IRQ enabled */
#define GHAL_RXD_MODE_IRQ_ENABLE        0x8000  /* Gen. IRQ on RX complete */
/* Flags from the 2nd QWORD */
#define GHAL_RXD_FLAG_BTE_COMPLETE      0x0001  /* Receive complete */
#define GHAL_RXD_FLAG_IMM_AVAILABLE     0x0002  /* Immediate data available */
#define GHAL_RXD_FLAG_ERR_LOC_MASK      0x001C  /* RxErrLocation mask */
#define GHAL_RXD_FLAG_ERR_LOC_SHIFT     2
#define GHAL_RXD_FLAG_ERR_STATUS_MASK   0x01E0  /* RxStatus mask */
#define GHAL_RXD_FLAG_ERR_STATUS_SHIFT  5

typedef struct ghal_rx_desc {
        /* 1st QWORD */
        uint64_t        base_addr:48;   /* Buffer address */
        uint64_t        mode1:16;       /* Modes (see definitions above) */
} ghal_rx_desc_t;

typedef struct ghal_cpurx_desc {
        /* 1st QWORD */
        uint64_t        buff_addr:48;   /* Buffer address */
        uint64_t        mode1:16;       /* Modes (see definitions above) */
        /* 2nd QWORD */
        uint64_t        flags:9;       /* Status flags (see definitions above) */
        uint64_t        unused2:23;
        uint64_t        data_len:32;    /* Received data size */
        /* 3rd QWORD */
        uint64_t        imm_data;       /* Immediate data received */
        /* 4th QWORD */
        uint64_t        unused4;
} ghal_cpurx_desc_t;

/* TX Channel flags*/
#define GHAL_TXVC_FLAG_DISABLE          0
#define GHAL_TXVC_FLAG_RESET            0x1
#define GHAL_TXVC_FLAG_ENABLE           0x2
#define GHAL_TXVC_FLAG_IRQ_ENABLE       0x4

/* BTE TX Channel descriptor */
typedef struct ghal_txvc_desc {
        /* 1st QWORD */
        union {
                struct {
                        uint64_t        idv:8;          /* interrupt delay value */
                        uint64_t        irq_req_index:5;/* interrupt request index*/
                        uint64_t        unused11:19;
                        uint64_t        timeout:19;     /* TX timeout value */
                        uint64_t        unused12:10;
                        uint64_t        flags:3;        /* see definitions above */
                };
                uint64_t        qword1;
        };

        /* 2nd QWORD */
        uint64_t        cpu_ring_addr;  /* address of CPU copy of TX descr. 
                                           ring (48bits), Bits[5:0] must be zero */
        /* 3rd QWORD */
        union {
                struct {
                        uint64_t        read_index:16;  /* TX queue read index */
                        uint64_t        unused31:16;
                        uint64_t        ssid_in_use:9;  /* number of SSIDs associated with channel */
                        uint64_t        unused32:7;
                        uint64_t        ssid_max_num:10;/* Max number of SSIDs channel can use */
                        uint64_t        unsuded33:6;
                };
                uint64_t        qword3;
        };

        /* 4th QWORD */
        uint64_t        write_index;    /* TX queue write index (16bits)*/

} ghal_txvc_desc_t;

typedef struct ghal_txvc_cntrs {
} ghal_txvc_cntrs_t;

/* TX Descriptor VC Base Index */
typedef struct ghal_txvc_base {
        uint16_t        vc[4];
} ghal_txvc_base_t;

/* TX Descriptor VC Count */
typedef struct ghal_txvc_count {
        uint16_t        vc[4];
} ghal_txvc_count_t;

#define GHAL_BTE_VC_MAX_GET_REQUEST     768
#define GHAL_BTE_VC_GET_QUIESCE_COUNT   512
#define GHAL_BTE_VC_GET_DELAY           5
#define GHAL_BTE_VC_MAX_HT_NP_REQUEST   32
#define GHAL_BTE_VC_HT_NP_QUIESCE_COUNT 24
#define GHAL_BTE_VC_HT_NP_DELAY         5
/* Virtual channel Non-posted operational Control */
typedef struct ghal_bte_vc_np_op_ctrl_desc {
        uint64_t max_get_request:11;    /* Maximum number of GET request this channel may have outstanding t */
        uint64_t get_quiesce_count:11;  /* Once this channel has this many GET requests in process, wait GET */
        uint64_t undefine_23_22:2;      /* undefined */
        uint64_t get_delay:8;   /* Once GET quiescent count is reached, wait this many BTE core cloc */
        uint64_t max_ht_np_request:8;   /* Maximum number of HT NP requests this channel may have to the HT  */
        uint64_t ht_np_quiesce_count:8; /* Once this channel has this many HT read requests in process, wait */
        uint64_t ht_np_delay:8; /* Once quiescent count is reached, wait this many BTE core clock cy */
        uint64_t undefine_62_56:7;      /* undefined */
        uint64_t read_rsp_passpw:1;     /* For HT read requests, this bit directly reflects the RespPassPW f */
} ghal_bte_vc_np_op_ctrl_desc_t;

/* RX Channel flags */
#define GHAL_RXVC_FLAG_DISABLE          0
#define GHAL_RXVC_FLAG_OVRPROTECT       0x1
#define GHAL_RXVC_FLAG_RESET            0x2
#define GHAL_RXVC_FLAG_ENABLE           0x4
#define GHAL_RXVC_FLAG_IRQ_ENABLE       0x8

/* BTE RX Channel descriptor */
typedef struct ghal_rxvc_desc {
        /* 1st QWORD */
        union {
                struct {
                        uint64_t        buf_size:32;    /* receive buffer size */
                        uint64_t        idv:8;          /* interrupt delay value */
                        uint64_t        unused1:15;
                        uint64_t        irq_req_index:5;/* interrupt request index*/
                        uint64_t        flags:4;        /* see definitions above */
                };
                uint64_t        qword1;
        };

        /* 2nd QWORD */
        uint64_t        cpu_ring_addr;  /* address of CPU copy of RX descr. 
                                           ring (48bits), Bits[5:0] must be zero */
        /* 3rd QWORD */
        union {
                struct {
                        uint64_t        read_index:16;  /* Rx queue read index */
                        uint64_t        unused31:16;
                        uint64_t        rmt_enties:10;   /* number of active RMT entries */
                        uint64_t        unused32:22;
                };
                uint64_t        qword3;
        };

        /* 4th QWORD */
        uint64_t        write_index;    /* RX queue write index (16bits)*/

} ghal_rxvc_desc_t;

/* PUT Sequence Performance counters */
typedef struct ghal_rxvc_putcnts {
} ghal_rxvc_putcnts;

/* SEND Sequence Performance counters */
typedef struct ghal_rxvc_sendcnts {
} ghal_rxvc_sendcnts;

/* RX Descriptor VC Base Index */
typedef struct ghal_rxvc_base {
        uint16_t        vc[4];
} ghal_rxvc_base_t;

/* RX Descriptor VC Count */
typedef struct ghal_rxvc_count {
        uint16_t        vc[4];
} ghal_rxvc_count_t;

/* MDD descriptor */
/* MDD modes */
#define GHAL_MDD_MODE_COHERENT  0x0001  /* Enables coherency bit twrds cpu */
#define GHAL_MDD_MODE_P_PASPW   0x0002  /* Use relaxed ord. rules for posted wr.*/
#define GHAL_MDD_MODE_NP_PASSPW 0x0004  /* Use relaxed ord. rules for non-posted req.*/
#define GHAL_MDD_MODE_FLUSH     0x0008  /* Send HT FLUSH cmnd prior to netw. resp.*/
#define GHAL_MDD_MODE_VALID     0x0010  /* Enable this entry */
#define GHAL_MDD_MODE_WRITABLE  0x0020  /* Enable writes using this entry */
typedef struct ghal_mdd_desc {
        /* 1st QWORD */
        uint64_t        ptag:8;         /* protection tag */
        uint64_t        unused1:4;
        uint64_t        mem_base:36;    /* Bits[47:12] of mem.base addr */
        uint64_t        unused12:8;
        uint64_t        mrt_enable:1;   /* Use MRT for transalations */
        uint64_t        unused13:7;
        /* 2nd QWORD */
        uint64_t        modes:12;       /* Modes (see definitions above) */ 
        uint64_t        max_base:36;    /* Bits[47:12] of maximum valid addr */
        uint64_t        unused2:6;
        uint64_t        cqh:10;         /* CQ handle */
} ghal_mdd_desc_t;

/* FMA descriptor */
/* FMA size on Gemini is configurable, for now set it to Hobbit FMA size */
#define GHAL_FMA_WINDOW_SIZE    (1024 * 1024 * 1024L)
#define GHAL_FMA_CTRL_SIZE      0x1000UL
/* FMA Privilege Mask */
#define GHAL_FMA_MASK_CQWRITE           0x00000001      /* CQ Write doorbell */
#define GHAL_FMA_MASK_SYNCCMP           0x00000002      /* SyncCmp doorbell */
#define GHAL_FMA_MASK_ALLOCSYNCID       0x00000004      /* Alloc SyncId doorbell */
#define GHAL_FMA_MASK_SCRATCHREG1       0x00000008      /* Scratch register #1 */
#define GHAL_FMA_MASK_SCRATCHREG0       0x00000010      /* Scratch register #0 */
#define GHAL_FMA_MASK_NPES              0x00000020      /* Number of PEs in NTT group */
#define GHAL_FMA_MASK_PEBASE            0x00000040      /* PE Base in NTT group */      
#define GHAL_FMA_MASK_CQHANDLE          0x00000080      /* CQ Handle */ 
#define GHAL_FMA_MASK_PTAG              0x00000100      /* Protection tag */
#define GHAL_FMA_MASK_NTTENABLE         0x00000200      /* NTT Enable */
#define GHAL_FMA_MASK_VMDHENABLE        0x00000400      /* VMDH Enable */
#define GHAL_FMA_MASK_CMD               0x00000800      /* Command */
#define GHAL_FMA_MASK_REMOTEMDH         0x00001000      /* Remote MDH */
#define GHAL_FMA_MASK_SOURCEMDH         0x00002000      /* Source MDH */
#define GHAL_FMA_MASK_PEMASKMODE        0x00004000      /* PE Mask Mode */
#define GHAL_FMA_MASK_FMAENABLE         0x00008000      /* FMA Enable */
#define GHAL_FMA_MASK_HARA              0x00010000      /* Hash, Request Adapt, Response Adapt*/
#define GHAL_FMA_MASK_BASEOFFSET        0x00020000      /* Base Offset */
#define GHAL_FMA_MASK_DEFAULT           (GHAL_FMA_MASK_CQWRITE | GHAL_FMA_MASK_SYNCCMP | GHAL_FMA_MASK_ALLOCSYNCID | \
                                        GHAL_FMA_MASK_SCRATCHREG1 | GHAL_FMA_MASK_SCRATCHREG0 | GHAL_FMA_MASK_CMD |  \
                                        GHAL_FMA_MASK_REMOTEMDH | GHAL_FMA_MASK_SOURCEMDH | GHAL_FMA_MASK_BASEOFFSET | \
                                        GHAL_FMA_MASK_VMDHENABLE | GHAL_FMA_MASK_HARA | GHAL_FMA_MASK_PEMASKMODE)
/* Modes from the 2nd QWORD */
#define GHAL_FMA_MODE_VMDH_ENABLE       0x01            /* vMDH Enable */
#define GHAL_FMA_MODE_DT_ENABLE         0x02            /* DT Enable */
#define GHAL_FMA_MODE_FMA_ENABLE        0x04            /* FMA Enable */
#define GHAL_FMA_MODE_RADP_ENABLE       0x08            /* Response Adapt Enable */
#define GHAL_FMA_MODE_ADP_ENABLE        0x10            /* Request Adapt Enable */
#define GHAL_FMA_MODE_HASH_ENABLE       0x20            /* Hash Enable */
/* Modes from the 3rd QWORD */
#define GHAL_FMA_MODE_NTT_ENABLE        0x01            /* NTT Enable */
/* Flags from the 4th QWORD */
#define GHAL_FMA_FLAG_SNCID_ALLOC_ERROR 0x01            /* SyncID allocation error */
#define GHAL_FMA_FLAG_NPES_ERROR        0x02            /* NPES violation (valid when NTT enabled) */
#define GHAL_FMA_FLAG_HT_REQ_ERROR      0x04            /* Bad HT request */
#define GHAL_FMA_FLAG_SHIFT             48
#define GHAL_FMA_FLAG_ERROR_MASK        (GHAL_FMA_FLAG_SNCID_ALLOC_ERROR | GHAL_FMA_FLAG_NPES_ERROR | GHAL_FMA_FLAG_HT_REQ_ERROR)
#define GHAL_FMA_ALLOC_SEQID_DOORBELL   0xff
/* Sync. Completion Types */
#define GHAL_FMA_SYNCTYPE_NO_EVENT      0x00            /* No Local or Remote CQE */
#define GHAL_FMA_SYNCTYPE_LOCAL_EVENT   0x01            /* Local CQE */
#define GHAL_FMA_SYNCTYPE_REMOTE_EVENT  0x02            /* Remote CQE */
#define GHAL_FMA_SYNCTYPE_WITH_FLAG     0x04            /* Deliver flag at target PE */

#define GHAL_FMA_SYNC_TYPE_OFFSET       56              /* offset of the SYNC TYPE in the QWORD */

#define GHAL_FMA_COUNTER_MASK           0xffff
#define GHAL_FMA_RC_SHIFT               16
#define GHAL_FMA_SSID_MASK              0xff
#define GHAL_FMA_SSID_SHIFT             32

/* FMA Descriptor */
typedef struct ghal_fma_desc {
        /* 1st QWORD */
        union {
                struct {
                        uint64_t        base_offset:58; /* Base Offset (bit 5:0 is not used )*/
                        uint64_t        unused12:6;
                };
                uint64_t        qword1;
        };
        /* 2nd QWORD */
        union {
                struct {
                        uint64_t        command:5;      /* Put window command */
                        uint64_t        unused21:3;
                        uint64_t        rem_mem_dh:12;  /* Remote Memory Domain Handle */
                        uint64_t        src_mem_dh:12;  /* Source Memory Domain Handle */
                        uint64_t        pe_mask_mode:3; /* PE Mask Mode */
                        uint64_t        unused22:23;
                        uint64_t        mode2:6;        /* Modes (see definitions above) */
                };
                uint64_t        qword2;
        };
        /* 3rd QWORD */
        union {
                struct {
                        uint64_t        num_pes:18;     /* Number of PEs in NTT group */
                        uint64_t        pe_base:18;     /* Base PE in NTT group */
                        uint64_t        cq_hndl:10;     /* Completion Queue Handle */
                        uint64_t        src_ptag:8;     /* Source Protection Tag */
                        uint64_t        dest_ptag:8;    /* Destination Protection Tag */
                        uint64_t        mode3:2;        /* Modes (see definitions above) */
                };
                uint64_t        qword3;
        };
        /* 4th QWORD */
        union {
                struct {
                        uint64_t        wc:16;          /* Outstanding PUT Count (writes only clear) */
                        uint64_t        rc:16;          /* Outstanding GET Count (writes only clear) */
                        uint64_t        ssid_inuse:8;   /* SSID in use (writes only clear) */
                        uint64_t        unused41:8;
                        uint64_t        flags:3;        /* Error flags (see above); writes clear */
                        uint64_t        unused42:13;
                };
                uint64_t        qword4;
        };
        /* 5th QWORD */
        uint64_t        scratch0;       /* Scratch Register 0 */
        /* 6th QWORD */
        uint64_t        scratch1;       /* Scratch Register 1 */
        /* 7th QWORD */
        uint64_t        alloc_syncid;   /* Alloc SyncID Doorbell */
        /* 8th QWORD */
        union {
                struct {
                        uint64_t        synccmp_data:56;/* Data to be used for CQE */
                        uint64_t        synccmp_type:8; /* Sync.completion type (see above)*/
                };
                uint64_t        qword8;
        };
        /* 9th QWORD */
        uint64_t        cq_write;    /* Completion data for remote CQ */
} ghal_fma_desc_t;

#define ghal_flbte_desc_t  ghal_fma_desc_t  /* Aries compatibility */

typedef uint64_t ghal_fma_prvmask_t;

typedef struct ghal_fma_alloc_seqid {
	uint64_t qword;
} ghal_fma_alloc_seqid_t;

#define GHAL_CQ_READ_INDEX_OFFSET       0
#define GHAL_CQ_REVISIT_RATE_BASE_SHIFT 0
#define GHAL_CQ_REVISIT_RATE_BLOCK_SIZE 0

typedef struct ghal_cq_desc {
        /* 1st QWORD */
        uint64_t        rd_index:29;    /* CQ Read Index */
        uint64_t        unused11:3;
        uint64_t        wr_index:29;    /* CQ Write Index */
        uint64_t        unused12:3;
        /* 2nd QWORD */
        uint64_t        max_index:29;/* CQ Maximum Index */
        uint64_t        unused21:3;
        uint64_t        irq_thresh_index:13;/* Thresh.Index for interrupt signals*/
        uint64_t        unused22:3;
        uint64_t        irq_req_index:5;/* IRQ request index */
        uint64_t        enable:1;       /* CQ Enable */
        uint64_t        unused23:10;
        /* 3rd QWORD */
        uint64_t        base_addr:48;   /* CQ base address */
        uint64_t        irq_req:1;      /* IRQ request */
        uint64_t        unused31:15;
} ghal_cq_desc_t;

typedef struct ghal_gcw {
        uint64_t        local_offset:40;
        uint64_t        unused1:8;
        uint64_t        count:4;
        uint64_t        unused2:6;
        uint64_t        gb:1;
        uint64_t        cmd:5;
} ghal_gcw_t;

typedef struct ghal_ntt_desc {
        uint64_t        nic_pe0:18;     /* Physical NIC n */
        uint64_t        nic_pe1:18;     /* Physical NIC n+1 */
        uint64_t        check_bits:8;
        uint64_t        syndrome:8;
        uint64_t        unused11:11;
        uint64_t        wr_checkbits:1;
} ghal_ntt_desc_t;

typedef struct ghal_ntt_config {
        uint64_t        gran_ntt0:4;            /* Granularity NTT0 */
        uint64_t        write_grn_ntt0:1;       /* Write Gran. NTT0 */
        uint64_t        unused11:3;     
        uint64_t        gran_ntt1:4;            /* Granularity NTT1 */
        uint64_t        write_grn_ntt1:1;       /* Write Gran. NTT1 */
        uint64_t        unused12:51;
} ghal_ntt_config_t;

/* Gemini writes a 32bit value to the given flow control address, as follows:
 * 0x5555_5555 = XOFF
 * 0xAAAA_AAAA = XON
 */
#define GHAL_FLOWCTRL_XOFF      0x55555555
#define GHAL_FLOWCTRL_XON       0xAAAAAAAA

typedef struct ghal_deadlock_flowctrl {
        uint64_t        flow_ctrl_enable:1;
        uint64_t        unused:1;
        uint64_t        flow_ctrl_add:62;
} ghal_deadlock_flowctrl_t;

typedef struct ghal_deadlock_recovery {
        uint32_t        deadlock_det_timer:4; /* 2^(10+value)*100ns */
        uint32_t        xon_time:4;           /* 2^(10+value)*100ns */          
        uint32_t        deadlock_det_enable_pc:1; /* enable for posted channel*/
        uint32_t        deadlock_det_enable_nc:1; /* enable for non-posted channel*/
        uint32_t        unused1:2;
        uint32_t        deadlock_rec_state:3;     /* recovery state */
        uint32_t        unused2:17;
} ghal_deadlock_recovery_t;


#endif /*_GEMINI_HW_H_*/
