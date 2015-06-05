/**
 *      The Gemini Hardware Abstraction Layer not kernel specific interface
 *      This file should be included by user level application directly interfacing NIC 
 *
 *      Copyright 2007 Cray Inc. All Rights Reserved.
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

#ifndef _GLOBAL_GHAL_PUB_H_
#define _GLOBAL_GHAL_PUB_H_

#if defined CRAY_CONFIG_GHAL_GEMINI && defined CRAY_CONFIG_GHAL_ARIES
# error "Please specify only one target, CRAY_CONFIG_GHAL_GEMINI or CRAY_CONFIG_GHAL_ARIES"
#endif

#if defined CRAY_CONFIG_GHAL_GEMINI
# include "gemini/ghal_pub.h"
#else
# if defined CRAY_CONFIG_GHAL_ARIES
#  include "aries/ghal_pub.h"
# else
#  error "Please specify a target, CRAY_CONFIG_GHAL_GEMINI or CRAY_CONFIG_GHAL_ARIES"
# endif
#endif

#endif /*_GLOBAL_GHAL_PUB_H_*/
