/*
 * Header file describing the internal (inter-module) DHD interfaces.
 *
 * Provides type definitions and function prototypes used to link the
 * DHD OS, bus, and protocol modules.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: dhd_bus.h,v 1.14.28.1 2010-12-23 01:13:17 Exp $
 */

#ifndef _dhd_bus_h_
#define _dhd_bus_h_

/*
 * Exported from dhd bus module (dhd_usb, dhd_sdio)
 */

/* Download firmware image and nvram image */
extern bool dhd_bus_download_firmware(struct dhd_bus *bus, osl_t *osh, firmware_info_t *fw_info);

/* Stop bus module: clear pending frames, disable data flow */
extern void dhd_bus_stop(struct dhd_bus *bus, bool enforce_mutex);

/* Initialize bus module: prepare for communication w/dongle */
extern int dhd_bus_init(dhd_pub_t *dhdp, bool enforce_mutex);

/* Send a data frame to the dongle.  Callee disposes of txp. */
extern int dhd_bus_txdata(struct dhd_bus *bus, void *txp);

/* Send/receive a control message to/from the dongle.
 * Expects caller to enforce a single outstanding transaction.
 */
extern int dhd_bus_txctl(struct dhd_bus *bus, uchar *msg, uint msglen);
extern int dhd_bus_rxctl(struct dhd_bus *bus, uchar *msg, uint msglen);

/* Deferred processing for the bus, return TRUE requests reschedule */
extern bool dhd_bus_dpc(struct dhd_bus *bus);
extern void dhd_bus_isr(bool * InterruptRecognized, bool * QueueMiniportHandleInterrupt, void *arg);

/* return the dongle chipid */
extern uint dhd_bus_chip(struct dhd_bus *bus);

extern void *dhd_bus_pub(struct dhd_bus *bus);
extern uint dhd_bus_hdrlen(struct dhd_bus *bus);

#endif /* _dhd_bus_h_ */
