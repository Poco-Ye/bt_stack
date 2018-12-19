/*
 * This defines the API between the Wireless LAN driver and the host application.
 * It may be used independent of the WLAN mode (NIC mode, dongle mode), and also
 * independent of the WLAN split-model (LMAC, BMAC, full-dongle etc). It is
 * intended to be used on platforms where the interface between the application
 * and driver is not defined or coupled with the operating system. This interface
 * provides APIs to:
 *   - Init/de-init the WLAN driver.
 *   - Send IOCTLs to the driver.
 *   - Register application callbacks for driver events.
 *   - Send and receive packets to the network stack.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: wl_drv.h,v 1.7 2009-12-10 21:48:57 Exp $
 */


#ifndef wl_drv_h
#define wl_drv_h

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Include Files ---------------------------------------------------- */

#include "dhdioctl.h"
#include "wlioctl.h"
#include "proto/bcmevent.h"


/* ---- Constants and Types ---------------------------------------------- */

/* Wireless driver handle. */
typedef void* wl_drv_hdl;

/* IOCTL declaration. */
typedef struct
{
	dhd_ioctl_t		d;
	wl_ioctl_t		w;
	unsigned int		ifidx;
} wl_drv_ioctl_t;

enum LINK_STATUS
{
    LINK_NONE = 0,
    LINK_WEP_SUC,
    LINK_WPA_SUC,
    LINK_FAILED,
};


/****************************************************************************
* Function:   wl_drv_event_callback
*
* Purpose:    User registered callback to receive events.
*
* Parameters: event      (in) Driver event.
*             event_data (in) Optional event data.
*
* Returns:    Nothing.
*****************************************************************************
*/
typedef void (*wl_drv_event_callback)(wl_event_msg_t* event, void* event_data);


/****************************************************************************
* Function:   wl_drv_event_packet_callback
*
* Purpose:    User registered callback to receive event packet.
*
* Parameters: event      (in) Driver event packet.
*
* Returns:    Nothing.
*****************************************************************************
*/
typedef void (*wl_drv_event_packet_callback)(bcm_event_t* event);


/* ----------------------------------------------------------------------- */
/* Network interface packet. */
typedef void* wl_drv_netif_pkt;

/****************************************************************************
* Function:   wl_drv_netif_start_queue
*
* Purpose:    Callback invoked by driver to start tx of packets from network
*             interface to driver.
*
* Parameters: None.
*
* Returns:    0 on success, else -1.
*****************************************************************************
*/
typedef int (*wl_drv_netif_start_queue)(void);

/****************************************************************************
* Function:   wl_drv_netif_stop_queue
*
* Purpose:    Callback invoked by driver to stop tx of packets from network
*             interface to driver.
*
* Parameters: None.
*
* Returns:    0 on success, else -1.
*****************************************************************************
*/
typedef int (*wl_drv_netif_stop_queue)(void);

/****************************************************************************
* Function:   wl_drv_netif_rx_pkt
*
* Purpose:    Callback invoked by driver to send packet from driver to network
*             interface.
*
* Parameters: pkt (in) Received network packet. The packet format is network
*                      interface stack-specific.
*             len (in) Length of received packet.
*
* Returns:    0 on success, else -1.
*****************************************************************************
*/
typedef int (*wl_drv_netif_rx_pkt)(wl_drv_netif_pkt pkt, unsigned int len);


/* Network interface callback functions registered with driver. */
typedef struct
{
	wl_drv_netif_start_queue	start_queue;
	wl_drv_netif_stop_queue		stop_queue;
	wl_drv_netif_rx_pkt		rx_pkt;

} wl_drv_netif_callbacks_t;


/* ----------------------------------------------------------------------- */

/****************************************************************************
* Function:   wl_drv_btamp_rx_pkt
*
* Purpose:    Callback invoked by driver to send BTAMP packet from driver to
*             network interface.
*
* Parameters: pkt (in) Received BTAMP packet.
*
* Returns:    0 on success, else -1.
*****************************************************************************
*/
typedef int (*wl_drv_btamp_rx_pkt_callback)(wl_drv_netif_pkt pkt, unsigned int len);

/****************************************************************************
* Function:   wl_drv_btamp_rx_pkt
*
* Purpose:    Callback invoked by driver to send BTAMP packet from driver to
*             network interface.
*
* Parameters: pkt (in) Received BTAMP packet.
*
* Returns:    0 on success, else -1.
*****************************************************************************
*/
typedef void* (*wl_drv_btamp_pkt_alloc_callback)(unsigned int len);


/* BTAMP callback functions registered with driver. */
typedef struct wl_drv_btamp_callbacks_t
{
	wl_drv_btamp_rx_pkt_callback		rx_pkt;
	wl_drv_btamp_pkt_alloc_callback		pkt_alloc;

} wl_drv_btamp_callbacks_t;


/* ----------------------------------------------------------------------- */
/* File handle. */
typedef void* wl_drv_file_hdl;

/****************************************************************************
* Function:   wl_drv_file_open
*
* Purpose:    Callback invoked by driver to open a file. This may be used to
*             open a nvram text file or dongle binary image stored on the
*             file-system.
*
* Parameters: filename (in) Full path of file to open.
*
* Returns:    File handle.
*****************************************************************************
*/
typedef wl_drv_file_hdl(*wl_drv_file_open)(const char *filename);

/****************************************************************************
* Function:   wl_drv_file_read
*
* Purpose:    Callback invoked by driver to read contents of a file.
*
* Parameters: buf      (mod) Destination buffer to read file contents.
*             len      (in)  Maximum number of bytes to read.
*             file_hdl (mod) File handle returned by wl_drv_file_open().
*
* Returns:    Number of bytes actually read.
*****************************************************************************
*/
typedef int (*wl_drv_file_read)(char *buf, int len, wl_drv_file_hdl file_hdl);

/****************************************************************************
* Function:   wl_drv_file_close
*
* Purpose:    Callback invoked by driver to close a file.
*
* Parameters: file_hdl (mod) File handle returned by wl_drv_file_open().
*
* Returns:    Nothing.
*****************************************************************************
*/
typedef void (*wl_drv_file_close)(wl_drv_file_hdl file_hdl);


/* File-system access functions registered with driver. */
typedef struct wl_drv_file_callbacks_t
{
	wl_drv_file_open	open;
	wl_drv_file_read	read;
	wl_drv_file_close	close;

} wl_drv_file_callbacks_t;

wl_drv_hdl wl_drv_init(void);

/****************************************************************************
* Function:   wl_drv_deinit
*
* Purpose:    De-initialize driver.
*
* Parameters: hdl (mod) Pointer to driver context (returned by wl_drv_init()).
*
* Returns:    Nothing.
*****************************************************************************
*/
void wl_drv_deinit(wl_drv_hdl hdl);

/****************************************************************************
* Function:   wl_drv_ioctl
*
* Purpose:    Issue an I/O control command to the driver.
*
* Parameters: hdl (mod) Pointer to driver context (returned by wl_drv_init()).
*             ioc (in)  I/O control command.
*
* Returns:    0 on success, else -1.
*****************************************************************************
*/
int wl_drv_ioctl(wl_drv_hdl hdl, wl_drv_ioctl_t *ioc);

#ifdef __cplusplus
	}
#endif

#endif  /* wl_drv_h  */
