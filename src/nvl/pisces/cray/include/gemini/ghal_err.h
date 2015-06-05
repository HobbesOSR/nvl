/**
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

#ifndef _GHAL_ERR_H_
#define _GHAL_ERR_H_ 

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "gemini_hw.h"

inline static int ghal_cq_bte_send_miss(uint32_t src, uint32_t code)
{
	/*
	 * This matches BTE_SEND_MISS error:
	 *    src == SOURCE_SSID_DREQ  (GeminiSpec 3.6, table 24)
	 *    code == BTE_SEND_MIS_ERR  (GeminiSpec 6.7, table 31)
	 */
	return ((code == 0xe) && (src == 0x2)) ? 1 : 0;
}

inline static int ghal_rmt_rx_cam_miss(uint32_t src, uint32_t code)
{
	/*
	 * This matches RX_SEND_CAM_MISS error:
	 *    src == SOURCE_RMT_DREQ  (GeminiSpec 3.6, table 24)
	 *    code == RX_SEND_CAM_MISS  (GeminiSpec 6.7, table 31)
	 */
	return ((code == 0xe) && (src == 0x6)) ? 1 : 0;
}

/*
 * The two functions in this file assist in decoding the error
 * status in a CQ entry.  The string positions within the string arrays,
 * and the names of the errors and sources are highly gemini instantiation
 * specific.  Consult the gemini spec and related ErrorHandling.doc document
 * for further details.
 */

inline static char *ghal_cq_source_str(uint32_t src)
{
        char *s=NULL;
        const char *cq_source_str[] = {
                "SOURCE_NOT_DONE",
                "SOURCE_SSID_SREQ",
                "SOURCE_SSID_DREQ",
                "SOURCE_SSID_SRSP",
                "SOURCE_BTE",
                "SOURCE_RMT_SREQ",
                "SOURCE_RMT_DREQ",
                "SOURCE_RMT_SRSP",
        };

        /* check input */
        if (src > GHAL_CQE_SOURCE_MASK) return s;
        if (src == 0) return s;

        s = (char *)cq_source_str[src];
        return s;
}

inline static char *ghal_cq_status_str(uint32_t src,uint32_t code)
{
        char *s=NULL,*offset;
        const char *cq_err_str[] = {
                "OKAY",
                "MISROUTED_PKT",
                "OVERLOAD:0010",
                "INV_CMD",
                "MALFORMED_PKT",
                "UNCORRECTABLE_TRANSLATION_ERR",
                "VMDH_INV",
                "MDD_INV",
                "PROTECTION_VIOLATION",
                "MEM_BOUNDS_ERR",
                "WRITE_PERMISSION_ERR",
                "REQUEST_TIMEOUT",
                "OVERLOAD:1100",
                "OVERLOAD:1101",
                "OVERLOAD:1110",
                "OVERLOAD:1111",
        };
        const char *cq_err_str_overload_0010[] = {
                "UNDEFINED_ERR",
                "UNCORRECTABLE_ERR",
                "UNCORRECTABLE_RMT_ERR",
                "UNDEFINED_ERR",
                "UNCORRECTABLE_ERR",
                "UNCORRECTABLE_ERR",
                "UNCORRECTABLE_RMT_ERR",
                "UNDEFINED_ERR",
        };
        const char *cq_err_str_overload_1100[] = {
                "UNDEFINED_ERR",
                "UNCORRECTABLE_SSIDSTATE_ERR",
                "RX_DESC_VC_INV",
                "UNCORRECTABLE_SSIDSTATE_ERR",
                "UNDEFINED_ERR",
                "UNCORRECTABLE_SSIDSTATE_ERR",
                "RX_DESC_VC_INV",
                "UNCORRECTABLE_SSIDSTATE_ERR",
        };
        const char *cq_err_str_overload_1101[] = {
                "UNDEFINED_ERR",
                "HT_DATA_ERR",
                "HT_DATA_ERR",
                "STALE_SSID",
                "HT_DATA_ERR",
                "HT_DATA_ERR",
                "HT_DATA_ERR",
                "STALE_SSID",
        };
        const char *cq_err_str_overload_1110[] = {
                "UNDEFINED_ERR",
                "NPES_VIOLATION",
                "BTE_SEND_MISS_ERR",
                "PROTOCOL_ERR",
                "NPES_VIOLATION",
                "NPES_VIOLATION",
                "BTE_SEND_MISS_ERR",
                "PROTOCOL_ERR",
        };
        const char *cq_err_str_overload_1111[] = {
                "UNDEFINED_ERR",
                "UNDEFINED_ERR",
                "RX_BUF_OVERRUN or CQ_ERR",
                "UNDEFINED_ERR",
                "UNDEFINED_ERR",
                "UNDEFINED_ERR",
                "RX_BUF_OVERRUN or CQ_ERR",
                "UNDEFINED_ERR",
        };

        /* sanity check of inputs, 
           return NULL if bogus input values */
        if (code > GHAL_CQE_STATUS_MASK) return s;
        if (src > GHAL_CQE_SOURCE_MASK) return s;
        if (src == 0) return s;

        s = (char *)cq_err_str[code];
        if (!strstr(s,"OVERLOAD")) {
                return s;
        }       


        offset = strstr(s,":");
        if (!offset) return NULL;
        ++offset;

        if (!strcmp(offset,"0010")) {
                s = (char *)cq_err_str_overload_0010[src];
                return s;
        }

        if (!strcmp(offset,"1100")) {
                s = (char *)cq_err_str_overload_1100[src];
                return s;
        }

        if (!strcmp(offset,"1101")) {
                s = (char *)cq_err_str_overload_1101[src];
                return s;
        }
        
        if (!strcmp(offset,"1110")) {
                s = (char *)cq_err_str_overload_1110[src];
                return s;
        }
        if (!strcmp(offset,"1111")) {
                s = (char *)cq_err_str_overload_1111[src];
                return s;
        }

        return s;
}

inline static char *ghal_cq_info_str(uint32_t src, uint32_t info)
{
        return NULL;
}

#define UNDEF -1
#define UNRECOV 0
#define RECOV 1
inline static int ghal_cq_recoverable(uint32_t src, uint32_t code)
{
        /* See Gemini Error Classification document for further details */
        const uint8_t recoverable_cq_error[GHAL_CQE_SOURCE_MASK+1][GHAL_CQE_STATUS_MASK+1] = {
                {UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF,UNDEF},                       /* undefined */
                {UNDEF,UNDEF,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,RECOV,RECOV,UNRECOV,RECOV,UNRECOV,UNDEF},   /* SSID_SREQ */
                {UNDEF,UNRECOV,RECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,RECOV,UNRECOV,RECOV,RECOV,UNRECOV}, /* SSID_DREQ */
                {UNDEF,UNDEF,UNDEF,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,RECOV,UNRECOV,RECOV,UNRECOV,UNDEF},   /* SSID_SRSP */
                {UNDEF,UNDEF,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,RECOV,RECOV,UNDEF,RECOV,UNRECOV,UNDEF},     /* BTE       */
                {UNDEF,UNDEF,RECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,RECOV,RECOV,UNRECOV,RECOV,UNRECOV,UNDEF},     /* RMT_SREQ  */
                {UNDEF,UNRECOV,RECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,RECOV,UNRECOV,RECOV,RECOV,UNRECOV}, /* RMT_DREQ  */
                {UNDEF,UNDEF,UNDEF,UNDEF,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,UNRECOV,RECOV,UNRECOV,UNDEF,RECOV,UNDEF},       /* RMT_SRSP  */
        };

        if ((!src) || (src > GHAL_CQE_SOURCE_MASK)) {
                return UNDEF;
        }

        if ((!code) || (code > GHAL_CQE_STATUS_MASK)) {
                return UNDEF;
        }

        return recoverable_cq_error[src][code];
}

#endif /*_GHAL_ERR_H_*/


