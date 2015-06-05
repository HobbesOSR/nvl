/**
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

#ifndef _GHAL_ERR_H_
#define _GHAL_ERR_H_

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aries_hw.h"

/*
 * Return true if the CQE source and status fields indicate that the
 * transaction failed due to BTE RXD/CAM miss.
 */
inline static int ghal_cq_bte_send_miss(uint32_t src, uint32_t code)
{
	return ((code == GHAL_CQE_STATUS_RMT_DESC_UNAVAILABLE ||
		 code == GHAL_CQE_STATUS_RMT_SEQTBL_UNAVAILABLE) &&
		(src == GHAL_CQE_SOURCE_SSID)) ? 1 : 0;
}

/*
 * Return true if the RXD source and status fields indicate that the
 * transaction failed due to BTE RXD/CAM miss.
 */
inline static int ghal_rmt_rx_cam_miss(uint32_t src, uint32_t code)
{
	return ((code == GHAL_CQE_STATUS_RMT_DESC_UNAVAILABLE ||
		 code == GHAL_CQE_STATUS_RMT_SEQTBL_UNAVAILABLE) &&
		(src == GHAL_RXD_FLAG_ERR_LOC_DREQ)) ? 1 : 0;
}

/*
 * The three functions in this file assist in decoding the error
 * status in a CQ entry.  The string positions within the string arrays,
 * and the names of the errors and sources are highly aries instantiation
 * specific.  Consult the aries spec for further details.
 */

inline static char *ghal_cq_source_str(uint32_t src)
{
        char *s=NULL;
        const char *cq_source_str[] = {
                "SOURCE_NOT_DONE",
                "SOURCE_BTE",
		"SOURCE_SSID",
		"SOURCE_RMT",
		"SOURCE_DLA",
        };

        /* check input */
        if (src > GHAL_CQE_SOURCE_MASK) return s;
        if (src == 0) return s;

        s = (char *)cq_source_str[src];
        return s;
}

inline static char *ghal_cq_status_str(uint32_t src,uint32_t code)
{
        char *s=NULL;
        const char *cq_err_str[] = {
		"OKAY",
		"REQ_DISABLED",
		"REQ_ILLEGAL_CMD",
		"REQ_ILLEGAL_ADDR",
		"REQ_ILLEGAL_SIZE",
		"REQ_INSUFFICIENT_PRIV",
		"PKT_MALFORMED_CMD",
		"PKT_MALFORMED_ADDR",
		"PKT_MALFORMED_SIZE",
		"PKT_MALFORMED_RESERVED",
		"PKT_MISROUTED",
		"DATA_ERR",
		"DATA_POISON",
		"AT_VMDHCAM_MALFORMED",
		"AT_MDD_UNCORRECTABLE",
		"AT_MDD_INV",
		"AT_MDD_MALFORMED",
		"AT_MDD_WR_PERMISSION_ERR",
		"AT_BOUNDS_ERR",
		"AT_PTT_UNCORRECTABLE",
		"AT_PROTECTION_ERR",
		"AT_TCE_UNCORRECTABLE",
		"AT_TCE_INV",
		"AT_PF_UNCORRECTABLE",
		"AT_PF_TIMEOUT",
		"AT_PF_INV",
		"AT_PF_MALFORMED",
		"AT_PF_RD_PERMISSION_ERR",
		"AT_PF_WR_PERMISSION_ERR",
		"AT_UNCORRECTABLE",
		"PE_BOUNDS_ERR",
		"FMA_UNCORRECTABLE",
		"DLA_OVERFLOW",
		"BTE_UNCORRECTABLE",
		"BTE_SEQUENCE_TERMINATE",
		"BTE_TIMEOUT",
		"MSG_PROTOCOL_ERR",
		"SSID_UNCORRECTABLE",
		"SSID_PKT_MALFORMED_SIZE",
		"SSID_TIMEOUT",
		"RMT_DESC_UNCORRECTABLE",
		"RMT_DESC_UNAVAILABLE",
		"RMT_SEQTBL_UNCORRECTABLE",
		"RMT_CAM_UNCORRECTABLE",
		"RMT_SEQTBL_UNAVAILABLE",
		"RMT_BUF_OVERRUN",
		"RMT_TIMEOUT",
		"CQ_OVERRUN",
		"CQ_ERR",
		"CE_UNCORRECTABLE",
		"CE_OPERATION_UNEXPECTED",
		"CE_JOIN_DUPLICATE",
		"CE_JOIN_CHILD_INV",
		"CE_JOIN_INCONSISTENT",
		"CE_PROTECTION_ERR",
		"CE_REDUCTION_ID_MISMATCH",
		"AMO_CACHE_ERR",
		"ORB_TIMEOUT",
		"PI_ERR",
		"PI_TIMEOUT",
		"WC_TAG_UNCORRECTABLE",
		"ENDPOINT_UNREACHABLE",
		"RESERVED_3E",
		"RESERVED_3F",
        };

        /* sanity check of inputs,
           return NULL if bogus input values */
        if (code > GHAL_CQE_STATUS_MASK) return s;

        s = (char *)cq_err_str[code];
	return s;
}

inline static char *ghal_cq_info_str(uint32_t src, uint32_t info)
{
        char *s=NULL;
	const char *cq_info_str[GHAL_CQE_SOURCE_MASK+1][GHAL_CQE_INFO_MASK+1] = {
	    {
		NULL,
		NULL,
		NULL,
		NULL,
	    },
	    {
		"TX_DONE",
		"TX_DONE_NO_SSID",
		"ENQ_STATUS",
		NULL,
	    },
	    {
		NULL,
		"CPLTN_SREQ",
		"CPLTN_DREQ",
		"CPLTN_SRSP",
	    },
	    {
		"CQWRITE",
		"CPLTN_SREQ",
		"CPLTN_DREQ",
		"CPLTN_SRSP",
	    },
	    {
		"ALLOC_STATUS",
		"DLA_MARKER",
		NULL,
		NULL,
	    },
	};

        /* check input */
        if (src > GHAL_CQE_SOURCE_MASK) return s;
        if (src == 0) return s;
	if (info > GHAL_CQE_INFO_MASK) return s;

        s = (char *)cq_info_str[src][info];
        return s;
}

#define UNDEF -1
#define UNRECOV 0
#define RECOV 1
inline static int ghal_cq_recoverable(uint32_t src, uint32_t code)
{
        /* See Aries Error Handling section of ASDG for further details */
	/* numbers in comments reflect constant values in Table 59 of ASDG */
        const uint8_t recoverable_cq_error[GHAL_CQE_STATUS_MASK+1] = {
	    RECOV,	//0x0
	    UNRECOV,	//0x01
	    UNRECOV,	//0x02
	    UNRECOV,	//0x03
	    UNRECOV,	//0x04
	    UNRECOV,	//0x05
	    RECOV,	//0x06
	    RECOV,	//0x07
	    RECOV,	//0x08
	    RECOV,	//0x09
	    UNRECOV,	//0x0A
	    RECOV,	//0x0B
	    UNRECOV,	//0x0C
	    UNRECOV,	//0x0D
	    UNRECOV,	//0x0E
	    UNRECOV,	//0x0F
	    UNRECOV,	//0x10
	    UNRECOV,	//0x11
	    UNRECOV,	//0x12
	    UNRECOV,	//0x13
	    UNRECOV,	//0x14
	    UNRECOV,	//0x15
	    UNRECOV,	//0x16
	    UNRECOV,	//0x17
	    RECOV,	//0x18
	    UNRECOV,	//0x19
	    UNRECOV,	//0x1A
	    UNRECOV,	//0x1B
	    UNRECOV,	//0x1C
	    RECOV,	//0x1D
	    UNRECOV,	//0x1E
	    UNRECOV,	//0x1F
	    RECOV,	//0x20
	    RECOV,	//0x21
	    RECOV,	//0x22
	    RECOV,	//0x23
	    UNRECOV,	//0x24
	    UNRECOV,	//0x25
	    RECOV,	//0x26
	    RECOV,	//0x27
	    RECOV,	//0x28
	    RECOV,	//0x29
	    RECOV,	//0x2A
	    RECOV,	//0x2B
	    RECOV,	//0x2C
	    UNRECOV,	//0x2D
	    RECOV,	//0x2E
	    RECOV,	//0x2F
	    UNRECOV,	//0x30
	    UNRECOV,	//0x31
	    UNRECOV,	//0x32
	    UNRECOV,	//0x33
	    UNRECOV,	//0x34
	    UNRECOV,	//0x35
	    UNRECOV,	//0x36
	    UNRECOV,	//0x37
	    UNRECOV,	//0x38
	    RECOV,	//0x39
	    UNRECOV,	//0x3A
	    RECOV,	//0x3B
	    RECOV,	//0x3C
	    RECOV,	//0x3D
	    UNRECOV,	//0x3E
	    UNRECOV,	//0x3F
        };

        if ((!code) || (code > GHAL_CQE_STATUS_MASK)) {
                return UNDEF;
        }

        return recoverable_cq_error[code];
}

#endif /*_GHAL_ERR_H_*/
