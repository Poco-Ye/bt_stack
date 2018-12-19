/*
 * Debug/trace/assert driver definitions for Dongle Host Driver.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: dhd_dbg.h,v 1.9.6.1 2010-12-22 23:47:24 Exp $
 */

#ifndef _dhd_dbg_
#define _dhd_dbg_

#if defined(OEM_ANDROID) || defined(OEM_CHROMIUMOS)
#define DHD_ERROR(args)    	do {if (net_ratelimit()) osl_ext_printf args;} while (0)
#else
#define DHD_ERROR(args)
#endif /* OEM_ANDROID || OEM_CHROMIUMOS */
#define DHD_TRACE(args)
#define DHD_INFO(args)
#define DHD_DATA(args)
#define DHD_CTL(args)
#define DHD_TIMER(args)
#define DHD_HDRS(args)
#define DHD_BYTES(args)
#define DHD_INTR(args)
#define DHD_GLOM(args)
#define DHD_EVENT(args)
#define DHD_BTA(args)
#define DHD_ISCAN(args)
#define DHD_WPS(args)
#define DHD_DHKEY(args)

#define DHD_ERROR_ON()		0
#define DHD_TRACE_ON()		0
#define DHD_INFO_ON()		0
#define DHD_DATA_ON()		0
#define DHD_CTL_ON()		0
#define DHD_TIMER_ON()		0
#define DHD_HDRS_ON()		0
#define DHD_BYTES_ON()		0
#define DHD_INTR_ON()		0
#define DHD_GLOM_ON()		0
#define DHD_EVENT_ON()		0
#define DHD_BTA_ON()		0
#define DHD_ISCAN_ON()		0
#define DHD_WPS_ON()		0
#define DHD_DHKEY_ON()		0

#define DHD_LOG(args)

#define DHD_NONE(args)

/* Defines msg bits */
#include <dhdioctl.h>

#endif /* _dhd_dbg_ */
