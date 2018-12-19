/*
 * Broadcom Dongle Host Driver (DHD), network interface. Port to the
 * OSL extension API.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: dhd_osl_ext.c,v 1.28.2.12 2011-01-13 23:55:14 Exp $
 */

/* ---- Include Files ---------------------------------------------------- */

#include "typedefs.h"
#include "osl_ext.h"
#include "dhd_dbg.h"
#include "bcmsdh_generic.h"
#include "bcmutils.h"
#include "dngl_stats.h"
#include "dhd.h"
#include "dhd_bus.h"
#include "dhd_proto.h"
#include "wl_drv.h"
#include "bcmendian.h"
#include "epivers.h"
#include <wlfc_proto.h>
#include <dhd_wlfc.h>
#include <wlu_cmd.h>
#include <wlm.h>
#include "generic_osl.h"
#include "irqs.h"
#include "platform.h"
#include "base.h"

/* Use polling */
uint dhd_poll;

/* Use interrupts */
uint dhd_intr;

/* Map between public WL driver handle and internal DHD info struct. */
#define WL_DRV_HDL_TO_DHD_INFO(hdl) ((struct dhd_info*) (hdl))
#define DHD_INFO_TO_WL_DRV_HDL(dhd) ((wl_drv_hdl) (dhd))

/* Maximum register netif callbacks */
#define MAX_NETIF_CALLBACK	3

/* Maximum register event callbacks */
#define MAX_EVENT_CALLBACK	3

/* Maximum register event packet callbacks */
#define MAX_EVENT_PACKET_CALLBACK	1

#define MAX_INTERFACES			3
#define MAX_IFNAME_SIZE			8

typedef int WLM_CHANNEL;

/* Interface control information */
typedef struct dhd_if {

	/* Back pointer to dhd_info. */
	struct dhd_info			*info;

	/* Inteface index assigned by dongle */
	int 				idx;

	/* Interface name. */
	char				name[MAX_IFNAME_SIZE];

	/* User registered callback for interfacing to network stack (e.g. TCP/IP stack). */
	wl_drv_netif_callbacks_t	netif_callbacks[MAX_NETIF_CALLBACK];

	/* User registered callback for posting events. */
	wl_drv_event_callback		event_callback[MAX_EVENT_CALLBACK];

	/* User registered callback for event packets. */
	wl_drv_event_packet_callback	event_packet_callback[MAX_EVENT_PACKET_CALLBACK];

	bool 				in_use;
} dhd_if_t;

/* Local private structure (extension of pub) */
typedef struct dhd_info {
	dhd_pub_t			pub;

	/* IOCTL response wait semaphore */
	osl_ext_sem_t			ioctl_resp_wait;

	/* Mutex for wl flow control */
	osl_ext_mutex_t			wlfc_mutex;

	/* Mutex for protocol critical sections. */
	osl_ext_mutex_t			proto_mutex;

	/* Semaphore used to signal DHD task to run. */
	osl_ext_sem_t			dhd_task_signal;
	
	/* Mutex for bus critical sections. */
	osl_ext_mutex_t			sd_mutex;

	/* Mutex for TX queue critical sections. */
	osl_ext_mutex_t			txq_mutex;

	/* Semaphore/flag used for ctrl event waits. */
	osl_ext_sem_t			ctrl_wait;
	bool					ctrl_waiting;
	bool					ctrl_notified;

	/* Flag used to signal DHD task to exit for driver de-initialization. */
	bool 				exit_dhd_task;

	/* Flag used to signal that DHD task has exited. */
	bool 				dhd_task_done;

	/* Flag used to signal watchdog task to exit for driver de-initialization. */
	bool 				exit_watchdog_task;

	/* Flag used to signal that watchdog task has exited. */
	bool 				watchdog_task_done;

	/* IOCTL response timeout */
	unsigned int			ioctl_timeout_msec;

	/* User registered callback for interfacing to file-system. */
	wl_drv_file_callbacks_t		file_callbacks;

	/* Pending 802.1X packets in TX queue. */
	volatile unsigned int 		pend_8021x_cnt;

	/* Interface control information */
	dhd_if_t 			iflist[MAX_INTERFACES];

	/* Link status */
	bool blLinked;

	/* SW AP mode */
	bool blSWAP;
} dhd_info_t;

typedef struct wl_connection_info
{
	uint8	Ssid_len;
	uint8	Ssid[32];
	int	 	Rssi;
	int		Phy_rate;
	int 	channel;
} wl_connection_info_t;

typedef int wlan_event_msg_t;
dhd_info_t *g_dhd_info = NULL;

enum {
	WLAN_DISCONNECTED = 0,
	WLAN_CONNECTED,
	WLAN_START_QUEUE,
	WLAN_STOP_QUEUE
};

void* pkt_tonative(osl_t *osh, void *drv_pkt);
void * pkt_frmnative(osl_t *osh, void *native_pkt, int len);
extern void wifi_link_task(void);
static int dhd_open(dhd_info_t *dhd);
static int dhd_stop(dhd_info_t *dhd);
      void dhd_task(void);
static int dhd_wait_pend8021x(dhd_info_t *dhd);
extern int wl_set_scan(void *wl);
extern int wl_get_scan(void *wl, int opc, char *scan_buf, uint buf_len);
extern int wl_get_rssi(int *rssi);
extern int wl_get_channel(int *ch);
extern int wlu_get(void *wl, int cmd, void *cmdbuf, int len);
       int wl_drv_rx_pkt_if (void * pkt, unsigned int buffer_len);

wl_drv_hdl      g_wl_drv_hdlr;
volatile int link_can_stop = 0;

void ap6181_pwr(int state)
{
	if(get_machine_type()==S500)
	{
		gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);
		gpio_set_pin_val(GPIOB, 31, state);
	}
	else
	{
		gpio_set_pin_type(GPIOA, 3, GPIO_OUTPUT);
    	gpio_set_pin_val(GPIOA, 3, state);
	}
	return;
}
void wl_event_callback(wl_event_msg_t* event, void* event_data)
{
	
	switch(event->event_type)
	{
		case WLC_E_SET_SSID:
		case WLC_E_ROAM:
			if (event->status == WLC_E_STATUS_SUCCESS && !g_dhd_info->blSWAP)
            {
				g_dhd_info->blLinked = wifi_get_ap_secmode();
                WIFI_MANAGER_SET_RENEW_IP(g_dhd_info->blLinked);
            }
			break;

		case WLC_E_LINK: /* Link is up */
			if(event->flags)
				break;
			else if(event->flags == 0 && event->status == 0)
			{
				WIFI_MANAGER_SET_STATUS_CONNECTING();
				break;
			}
			break;
        case WLC_E_PSK_SUP:
            if(event->status == 6) 
            {
                g_dhd_info->blLinked = LINK_WPA_SUC;
                WIFI_MANAGER_SET_RENEW_IP(LINK_WPA_SUC);
            }
            break;

        case WLC_E_REASSOC:
            WIFI_MANAGER_SET_RENEW_IP(LINK_NONE);
            break;

		case WLC_E_DISASSOC_IND:
            WIFI_MANAGER_SET_STATUS_CONNECTING();
            break;

        case WLC_E_TXFAIL:
            if(event->status == 2 && event->flags == 24)
                WIFI_MANAGER_SET_STATUS_CONNECTING();
			break;

		case WLC_E_SCAN_COMPLETE:
			/* To send signal only if we received WLC_E_SCAN_COMPLETE event */
			break;
	}
}

int _wifi_tx(uchar *buffer, int len)
{
    dhd_info_t *dhd_info = g_dhd_info;

    if(dhd_info && (dhd_info->blLinked==LINK_WEP_SUC || dhd_info->blLinked==LINK_WPA_SUC))
    {
        wl_drv_tx_pkt_if(DHD_INFO_TO_WL_DRV_HDL(dhd_info), 0, (wl_drv_netif_pkt)buffer, len);
        return 0;
    }

    return -1;
}

int ap6181_init(void)
{	
	ap6181_pwr(1);
    DelayMs(15); /* for chip fully power up */

	return (g_wl_drv_hdlr = wl_drv_init()) ? 0 : -1;
}

void reset_connection_status(void)
{
	g_dhd_info->blLinked = LINK_NONE;
}

bool wlan_get_connection_status(void)
{
	return g_dhd_info->blLinked;
}

extern volatile int dhd_can_stop;
void ap6181_deinit(void)
{
	wl_drv_hdl hdl = g_wl_drv_hdlr;
    int flag;

    //irq_save_asm(&flag);
    while(!dhd_can_stop)
    {
        //irq_restore_asm(&flag);
        DelayMs(10);
        //irq_save_asm(&flag);
    }

    g_dhd_info = NULL;
    //irq_restore_asm(&flag);

	if(hdl) wl_drv_deinit(hdl);

	ap6181_pwr(0);
    DelayMs(10);
    return ;
}

int wlan_set_scan(void)
{
	if(wl_set_scan(NULL))
		return -1;
	
	return 0;
}

int wlan_get_scan(char* scan_buf, uint buf_size)
{
	memset(scan_buf, 0x00, buf_size);
	
	if(wl_get_scan(NULL, WLC_SCAN_RESULTS, scan_buf, buf_size))
		return -1;
	
	return 0;
}

int wlan_check_existence(char *ssid)
{
    wl_scan_results_t *list = NULL;
    wl_bss_info_t *bi = NULL;
    char buffer[500];
    int ret = 0;

    if(ssid == NULL) return 0;

    ret = wlan_scan_ap(ssid, buffer, sizeof(buffer));
    if(ret == -3) /* force stop */
    {
        return -3;
    }
    else if(ret < 0) 
    {
        return 0;
    }

    return 0;
}

int16 check_security(wl_bss_info_t *bi)
{
	vndr_ie_t *ie;
	int parsed_len=0;
	int16 security = 0;
	
	ie = (vndr_ie_t*)((int8*)bi+bi->ie_offset);

	/* check whether privacy bit is set */
	if(bi->capability & DOT11_CAP_PRIVACY) {
		bool found_wpa = FALSE, found_rsn = FALSE;
			
		while(bi->ie_length > parsed_len) {
			uint8 *data_ie = NULL;
			int i = 0;
			uint16 ucst_cipher_suite_count = 0, auth_key_mgmt_suite_count = 0;
			uint32 ucst_cipher_suite = 0, auth_key_mgmt_suite = 0;
					
			if(ie->id == DOT11_MNG_WPA_ID) {
				uint32 OUI=0;

				memcpy(&OUI, (uint8*)ie+2, 4);
				
				if(OUI == WPA_OUI_TYPE_WPA) {
					/* WPA IE found */
					found_wpa = TRUE;
				
					/* Skip ID (1-byte), length  (1-byte), OUI  (3-byte), type (1-byte) and WPA version (2-byte) */
					data_ie = (uint8*)ie + 8;

					/* Skip multicast cipher suite */
					data_ie += 4;

					/* Get unicast cipher suite count */
					memcpy(&ucst_cipher_suite_count, data_ie, 2);
					
					/* Skip unicast cipher suite count */
					data_ie += 2;

					for(i=0; i<ucst_cipher_suite_count; i++) {
						memcpy(&ucst_cipher_suite, data_ie, 4);
						
						if(ucst_cipher_suite == WPA_CIPHER_SUITE_TKIP) {
							security |= WLM_ENCRYPT_TKIP;
						}
						else if(ucst_cipher_suite == WPA_CIPHER_SUITE_AES) {
							security |= WLM_ENCRYPT_AES;
						}

						/* Get next unicast cipher suite */
						data_ie += 4;
					}

					/* Get auth key management suite count */
					memcpy(&auth_key_mgmt_suite_count, data_ie, 2);					
					
					/* Skip auth key management suite count */
					data_ie += 2;

					memcpy(&auth_key_mgmt_suite, data_ie, 4);					
					if(auth_key_mgmt_suite == WPA_KEY_MGMT_SUITE_PSK) {
						security |= WLM_WPA_AUTH_PSK<<8;
						//osl_ext_printf("%s: auth key management suite = 0x%x\n", __FUNCTION__, security);
					}
				}
			}
			else if(ie->id == DOT11_MNG_RSN_ID) {
				/* RSN IE found */
				found_rsn = TRUE;
				
				/* Skip ID (1-byte), length  (1-byte) and RSN version (2-byte) */
				data_ie = (uint8*)ie + 4;

				/* Skip group cipher suite */
				data_ie += 4;

				/* Get pairwise cipher suite count */
				memcpy(&ucst_cipher_suite_count, data_ie, 2);

				/* Skip pairwise cipher suite count */
				data_ie += 2;
				
				for(i=0; i<ucst_cipher_suite_count; i++) {
					memcpy(&ucst_cipher_suite, data_ie, 4);

					if(ucst_cipher_suite == WPA2_CIPHER_SUITE_TKIP) {
						security |= WLM_ENCRYPT_TKIP;
					}		
					else if(ucst_cipher_suite == WPA2_CIPHER_SUITE_AES) {
						security |= WLM_ENCRYPT_AES;					
					}

					/* Get next unicast cipher suite */
					data_ie += 4;
				}

				/* Get auth key management suite count */
				memcpy(&auth_key_mgmt_suite_count, data_ie, 2);
				
				/* Skip auth key management suite count */
				data_ie += 2;

				memcpy(&auth_key_mgmt_suite, data_ie, 4);
				if(auth_key_mgmt_suite == WPA2_KEY_MGMT_SUITE_PSK) {
					security |= WLM_WPA2_AUTH_PSK<<8;
				}
			}

			/* Jump to next IE */
			ie = (vndr_ie_t*)((int8*)ie + (ie->len+2));
			parsed_len += (ie->len+2);
		}

		/* It is WEP if neither WPA nor RSN IE was found */
		if(!found_wpa && !found_rsn)
			security = WLM_ENCRYPT_WEP;
	}
	
	return security;	
}

int wlan_scan_network(char* scan_buf, uint buf_size)
{
    int ret, t0, i = 0;

    if (wlan_set_scan())
        return -1;

    /* Wait for scan complete */
    t0 = s_Get10MsTimerCount();
    while (s_Get10MsTimerCount() - t0 <= 500)
    {
        ret = wl_check_scan_status();
        if (!ret) break;

        if(!IsMainCodeTask())
        {
            link_can_stop = 1;
            OsSleep(5); /* warning: this value is relative to DELAY time after "wifi_link_can_stop" 
                           in WifiDisConnect, and must be less than DELAY time. Be careful to modify this, 
                           (10 miliseconds)
                           */
            link_can_stop = 0;

            if(!WIFI_MANAGER_STATUS_IS_CONNECTING())
            {
                return -3; /* force exit */
            }
        }
        else
        {
            DelayMs(1000);
        }
    }

    ret = wlan_get_scan(scan_buf, buf_size);
    if (ret)
    {
        return ret;
    }

    wl_scan_results_t *list = (wl_scan_results_t *) scan_buf;
    wl_bss_info_t *bi = NULL;
    bi = list->bss_info;

    for (i = 0; i < list->count; i++, bi = (wl_bss_info_t *) ((int8 *) bi + bi->length))
    {
        int16 sec = check_security(bi);
        bi->reserved[0] = (uint8) sec;
        bi->reserved[1] = (uint8) (sec >> 8);
    }

    return 0;
}

int wlan_scan_network2(char* scan_buf, uint buf_size)
{
    int ret, t0, i = 0;

    if (wlan_set_scan())
        return -1;

    /* Wait for scan complete */
    t0 = s_Get10MsTimerCount();
    while (s_Get10MsTimerCount() - t0 <= 500)
    {
        ret = wl_check_scan_status();
        if (!ret) break;

        if(!IsMainCodeTask())
        {
            link_can_stop = 1;
            OsSleep(5); /* warning: this value is relative to DELAY time after "wifi_link_can_stop" 
                           in WifiDisConnect, and must be less than DELAY time. Be careful to modify this, 
                           (10 miliseconds)
                           */
            link_can_stop = 0;
        }
    }

    ret = wlan_get_scan(scan_buf, buf_size);
    if (ret)
    {
        return ret;
    }

    wl_scan_results_t *list = (wl_scan_results_t *) scan_buf;
    wl_bss_info_t *bi = NULL;
    bi = list->bss_info;

    for (i = 0; i < list->count; i++, bi = (wl_bss_info_t *) ((int8 *) bi + bi->length))
    {
        int16 sec = check_security(bi);
        bi->reserved[0] = (uint8) sec;
        bi->reserved[1] = (uint8) (sec >> 8);
    }

    return 0;
}

inline void link_can_stop_enable(void)
{
    link_can_stop = 1;
}

inline void link_can_stop_disable(void)
{
    link_can_stop = 0;
}

int wlan_scan_ap(char *ssid, char* scan_buf, uint buf_size) /* use in task */
{
    int ret, t0, i = 0;

    if(wl_set_ap_scan(ssid))
        return -1;

    t0 = s_Get10MsTimerCount();
    while (s_Get10MsTimerCount() - t0 <= 500)
    {
        ret = wl_check_scan_status();
        if (!ret) break;

        link_can_stop = 1;
        OsSleep(5); /* warning: this value is relative to DELAY time after "wifi_link_can_stop" 
                       in WifiDisConnect, and must be less than DELAY time. Be careful to modify this, 
                       (10 miliseconds)
                       */
        link_can_stop = 0;

        if(!WIFI_MANAGER_STATUS_IS_CONNECTING())
        {
            return -3; /* force exit */
        }
    }

    ret = wlan_get_scan(scan_buf, buf_size);
    if (ret)
    {
        DHD_ERROR(("%s: wlan_get_scan fail\n", __FUNCTION__));
        return ret;
    }

    wl_scan_results_t *list = (wl_scan_results_t *) scan_buf;
    wl_bss_info_t *bi = NULL;
    bi = list->bss_info;

    for (i = 0; i < list->count; i++, bi = (wl_bss_info_t *) ((int8 *) bi + bi->length))
    {
        int16 sec = check_security(bi);
        bi->reserved[0] = (uint8) sec;
        bi->reserved[1] = (uint8) (sec >> 8);
    }

    return 0;
}
int wlan_join_network(char *ssid, WLM_AUTH_TYPE authType, WLM_AUTH_MODE authMode, WLM_ENCRYPTION encryption, const char *key, int keyid)
{
	struct ether_addr ea;
	int ret = 0;

	/* We're not in SW AP mode now */
	g_dhd_info->blSWAP = FALSE;

	memset((void*)&ea, 0x00, sizeof(struct ether_addr));
    wifi_get_module_mac(&ea);

	/* wl down */
	if(!wlmEnableAdapterUp(FALSE))
		return -1;

	/* enter STA mode, wl ap 0 */
	if(!wlmSTASet())
		return -1;

	if(!wlmCountryCodeSet(WLM_COUNTRY_ALL))
		return -1;

	/* wl down */
	if(!wlmEnableAdapterUp(TRUE))
		return -1;

	/* make sure not currently associated */
	if(!wlmDisassociateNetwork())
		ret = -1;

	if(!wlmSecuritySet(authType, authMode, encryption, key, keyid))
		ret = -1;

	if(!wlmBssidSet((char*)&ea))
		ret = -1;

	if(!wlmJoinNetwork(ssid, WLM_MODE_BSS))
		ret = -1;

	return ret;
}

int wlan_get_bssid(char *bssid, int length)
{
	if(wlmBssidGet(bssid,length) == TRUE)
		return 0;
	return -1;
}

int wlan_disassociate_network(void)
{
    if(!wlmDisassociateNetwork()) return -1;
    return 0;
}

int wifi_link_can_stop(void)
{
    return link_can_stop;
}


wl_drv_hdl wl_drv_init(void)
{
	int		    ret = -1;
	struct dhd_bus	*bus;
	osl_t 		    *osh;
	dhd_info_t 	    *dhd;

	dhd_poll = TRUE, dhd_intr = FALSE;

	osh = os_resource_init(); /* malloc for tx & rx & cfg */
	if (osh == NULL) return NULL;

	bus = bcmsdh_probe(osh);  /* attach hc and target device */
	if (bus == NULL) return NULL;

	dhd = *((dhd_info_t **)bus);
    ret = dhd_open(dhd);
	if (ret) return  NULL;

	return (DHD_INFO_TO_WL_DRV_HDL(dhd));
}

void wl_drv_deinit(wl_drv_hdl hdl)
{
	osl_t *osh;
	struct dhd_info *dhd = WL_DRV_HDL_TO_DHD_INFO(hdl);

	if (dhd == NULL)
		dhd = g_dhd_info;

	dhd_stop(dhd);

	/* save osh as dhd will be free'ed */
	osh = dhd->pub.osh;

	bcmsdh_remove(osh, dhd->pub.bus);
	os_resource_free(osh);
}

#define ROAM_THRESHOLD      -65
static int dhd_open(dhd_info_t *dhd)
{
    firmware_info_t fw_info;
	int ret = -1, i, val;

    ret = BrcmWifiUpdateFw();
    if(ret)
    {
        ret = BrcmGetFwInfo(&fw_info, sizeof(firmware_info_t));
        if(ret) 
        {
            return -1;
        }

        if (!(dhd_bus_download_firmware(dhd->pub.bus, dhd->pub.osh, &fw_info))) 
        {
            ASSERT(0);
            return -2;
        }
    }

	if ((ret = dhd_bus_init(&dhd->pub, TRUE)) != 0) { /* clock on */
        ASSERT(0);
        return -3;
    }

	if (dhd->pub.busstate != DHD_BUS_DATA)
        return -4;
	
	g_dhd_info = dhd;

	dhd_prot_init(&dhd->pub); /* get mac & set event */

	/* Allow transmit calls */
	dhd->pub.up = 1;
    wifi_set_module_mac(dhd->pub.mac.octet);

    ret = wlmRoamingOn();
    if (ret != TRUE) return -5;

    ret = wlmRoamTriggerLevelSet(ROAM_THRESHOLD, WLM_BAND_2G);
    if (ret != TRUE) return -6;

    ret = wlmRoamTriggerLevelGet(&val, WLM_BAND_2G);
    if (ret != TRUE || val != ROAM_THRESHOLD) 
        return -7;

	return 0;
}

static int dhd_stop(dhd_info_t *dhd)
{
	dhd->pub.up = 0;
	dhd_bus_stop(dhd->pub.bus, TRUE);
	return 0;
}

void dhd_timeout_start(dhd_timeout_t *tmo, uint usec)
{
	tmo->limit = usec;
	tmo->increment = 0;
	tmo->elapsed = 0;
	tmo->tick = 10000;
}

int wifi_sleep(int on)
{
    return dhd_set_suspend(on, &g_dhd_info->pub) ? -1 : 0;
}

int dhd_timeout_expired(dhd_timeout_t *tmo)
{
	/* Does nothing the first call */
	if (tmo->increment == 0) {
		tmo->increment = 1;
		return 0;
	}

	if (tmo->elapsed >= tmo->limit)
		return 1;

	/* Add the delay that's about to take place */
	tmo->elapsed += tmo->increment;

	OSL_DELAY(tmo->increment);
	tmo->increment *= 2;
	if (tmo->increment > tmo->tick)
		tmo->increment = tmo->tick;

	return 0;
}

#define IOCTL_RESP_TIMEOUT  20000 /* In milli second */

dhd_info_t g_dhd;

dhd_pub_t * dhd_attach(osl_t *osh, struct dhd_bus *bus, uint bus_hdrlen)
{
	dhd_info_t *dhd=&g_dhd;
	osl_ext_status_t status;

	memset(dhd, 0, sizeof(dhd_info_t));
	dhd->pub.osh = osh;
	dhd->pub.info = dhd; /* Link to info module */
	dhd->pub.bus = bus;  /* Link to bus module */
	dhd->pub.hdrlen = bus_hdrlen;
	dhd->ioctl_timeout_msec = IOCTL_RESP_TIMEOUT;

	status = osl_ext_mutex_create("dhdprot", &dhd->proto_mutex);
	ASSERT(status == OSL_EXT_SUCCESS);

	status = osl_ext_mutex_create("dhdsd", &dhd->sd_mutex);
	ASSERT(status == OSL_EXT_SUCCESS);

	status = osl_ext_mutex_create("dhdtxq", &dhd->txq_mutex);
	ASSERT(status == OSL_EXT_SUCCESS);

	status = osl_ext_sem_create("dhdioc", 0, &dhd->ioctl_resp_wait);
	ASSERT(status == OSL_EXT_SUCCESS);

	status = osl_ext_sem_create("dhdctrl", 0, &dhd->ctrl_wait);
	ASSERT(status == OSL_EXT_SUCCESS);

    /* Attach and link in the protocol */
	if (dhd_prot_attach(&dhd->pub) != 0) {
        ASSERT(0);
		goto fail;
	}

	pkt_set_headroom(TRUE, bus_hdrlen);
	return (&dhd->pub);

fail:
	if (dhd)
		dhd_detach(&(dhd->pub));

	return NULL;
}

void dhd_detach(dhd_pub_t *dhdp)
{
	dhd_info_t *dhd = (dhd_info_t *)dhdp->info;

	osl_ext_sem_delete(&dhd->ioctl_resp_wait);
	osl_ext_sem_delete(&dhd->ctrl_wait);
	osl_ext_mutex_delete(&dhd->txq_mutex);
	osl_ext_mutex_delete(&dhd->sd_mutex);
	osl_ext_mutex_delete(&dhd->proto_mutex);

	if (dhdp->prot)
		dhd_prot_detach(&dhd->pub);
}

int wl_drv_ioctl(wl_drv_hdl hdl, wl_drv_ioctl_t *ioc)
{
	int 		bcmerror = BCME_ERROR;
	int 		len = 0;
	void 		*buf = NULL;
	dhd_ioctl_t	*dhd_ioc;
	wl_ioctl_t 	*wl_ioc;
	struct dhd_info *dhd = WL_DRV_HDL_TO_DHD_INFO(hdl);
	bool		is_set_key_cmd;

	if (dhd == NULL)
		dhd = g_dhd_info;

	if ((dhd == NULL) || (ioc == NULL)) 
    {
		bcmerror = BCME_BADARG;
		goto done;
	}

	if (!dhd->pub.up || (dhd->pub.busstate != DHD_BUS_DATA)) 
    {
		bcmerror = BCME_DONGLE_DOWN;
		goto done;
	}

    wl_ioc = &ioc->w;
    if (wl_ioc->buf) 
    {
        len = MIN(wl_ioc->len, WLC_IOCTL_MAXLEN);
        buf = wl_ioc->buf;
    }

	/* Intercept WLC_SET_KEY IOCTL - serialize M4 send and set key IOCTL to
	 * prevent M4 encryption.
	 */
	is_set_key_cmd = ((ioc->w.cmd == WLC_SET_KEY) ||
	                 ((ioc->w.cmd == WLC_SET_VAR) && !(strncmp("wsec_key", ioc->w.buf, 9))) ||
	                 ((ioc->w.cmd == WLC_SET_VAR) && !(strncmp("bsscfg:wsec_key", ioc->w.buf, 15))));
	if (is_set_key_cmd)
		dhd_wait_pend8021x(dhd);

	bcmerror = dhd_wl_ioctl(&dhd->pub, ioc->ifidx, wl_ioc, buf, len); /* Send to dongle... */

done:
	if ((bcmerror != 0) && (dhd != NULL))
		dhd->pub.bcmerror = bcmerror;

	return OSL_ERROR(bcmerror);
}

volatile int dhd_can_stop = 1;

void dhd_task(void)
{
    dhd_info_t *dhd;
    
    while(1)
    {
        dhd_can_stop = 1;
        OsSleep(2);
        dhd_can_stop = 0;

        if ((dhd = g_dhd_info) == NULL) continue;
        if (!read_int_state(dhd->pub.bus)) continue;

        if (dhd->pub.busstate != DHD_BUS_DOWN) 
        { 
            while(dhd_bus_dpc(dhd->pub.bus));
        }
        else 
        {
            ASSERT(0);
            dhd_bus_stop(dhd->pub.bus, TRUE);
        }
    }

    return ;
}

void dhd_rx_frame(dhd_pub_t *dhdp, int ifidx, void *pktbuf, int numpkt, uint8 chan)
{
	uchar 				*ptr;
	uint16 				type;
	wl_event_msg_t			event;
	void 				*data;
	void				*pnext;
	uint 				len;
	int 				i;
	void 				*netif_pkt;
	osl_t				*osh;
	dhd_info_t			*dhd;
	int				bcmerror;
	int				j;
	wl_drv_netif_rx_pkt		rx_pkt_cb;
	wl_drv_event_callback		evt_cb;
	wl_drv_event_packet_callback	evt_pkt_cb;

	osh = dhdp->osh;
	dhd = dhdp->info;

	for (i = 0; pktbuf && i < numpkt; i++, pktbuf = pnext) 
    {
		pnext = PKTNEXT(osh, pktbuf);
		PKTSETNEXT(osh, pktbuf, NULL);

		ptr = PKTDATA(osh, pktbuf);
		type  = *(uint16 *)(ptr + ETHER_TYPE_OFFSET);
		len = PKTLEN(osh, pktbuf);

		if (ntoh16(type) == ETHER_TYPE_BRCM) /* Process special event packets and then discard them */
        {
			ifidx = 0;		/* single interface for now */
			bcmerror = wl_host_event(dhdp, &ifidx, ptr, &event, &data);
			if (bcmerror == BCME_OK) /* Event packet callback */
            {
				wl_event_to_host_order(&event); /* Post event to application. */
                wl_event_callback(&event, data);
			}
			pktfree(osh, pktbuf);
			continue;
		}

        netif_pkt = pkt_tonative(osh, pktbuf);

        /* Regular network packet. */
        wl_drv_rx_pkt_if(netif_pkt, len);

		/* Free the driver packet. */
		pktfree(osh, pktbuf);
	}
}

void * get_dhd_bus(void)
{
    struct dhd_info *dhd = g_dhd_info;
    return ((dhd_pub_t *)&dhd->pub)->bus;
}

void * get_dhd_info(void)
{
    return g_dhd_info;
}

int wl_drv_tx_pkt_if(wl_drv_hdl hdl, unsigned int ifidx, wl_drv_netif_pkt netif_pkt, unsigned int len)
{
	struct dhd_info *dhd = WL_DRV_HDL_TO_DHD_INFO(hdl);
	void	*pktbuf;

	if (dhd == NULL)
		dhd = g_dhd_info;

	if (!(pktbuf = pkt_frmnative(dhd->pub.osh, netif_pkt, len))) 
    {
        ASSERT(0);
		return -1;
	}

	return (dhd_sendpkt(&dhd->pub, ifidx, pktbuf));
}

int dhd_sendpkt(dhd_pub_t *dhdp, int ifidx, void *pktbuf)
{
	int ret;
	dhd_info_t *dhd = (dhd_info_t *)(dhdp->info);

	if (!dhdp->up || (dhdp->busstate == DHD_BUS_DOWN)) 
    {
		ret = -BCME_NOTUP;
		goto done;
	}

	/* Look into the packet and update the packet priority */
	pktsetprio(pktbuf, FALSE);
    dhd_prot_hdrpush(dhdp, ifidx, pktbuf);
    ret = dhd_bus_txdata(dhdp->bus, pktbuf);

done:
	if (ret) pktfree(dhd->pub.osh, pktbuf);

	return ret;
}

void dhd_txcomplete(dhd_pub_t *dhdp, void *txp)
{
	int ifidx;
	dhd_info_t *dhd = (dhd_info_t *)(dhdp->info);

	dhd_prot_hdrpull(dhdp, &ifidx, txp);
    return ;
}

int dhd_os_sdlock(dhd_pub_t * pub)
{
	dhd_info_t *dhd = pub->info;
	return osl_ext_mutex_acquire(&dhd->sd_mutex, OSL_EXT_TIME_FOREVER);
}

int dhd_os_check_sdlock(void)
{
	dhd_info_t *dhd = g_dhd_info;
    if(dhd == NULL) return 1;

	return osl_ext_mutex_check(&dhd->sd_mutex);
}

void dhd_os_sdunlock(dhd_pub_t * pub)
{
	dhd_info_t *dhd = pub->info;
	osl_ext_mutex_release(&dhd->sd_mutex);
}

void dhd_os_sdlock_txq(dhd_pub_t * pub)
{
	dhd_info_t *dhd = pub->info;
	osl_ext_mutex_acquire(&dhd->txq_mutex, OSL_EXT_TIME_FOREVER);
}

void dhd_os_sdunlock_txq(dhd_pub_t * pub)
{
	dhd_info_t *dhd = pub->info;

	osl_ext_mutex_release(&dhd->txq_mutex);
}

int dhd_os_proto_block(dhd_pub_t * pub)
{
	dhd_info_t *dhd = pub->info;
	if (osl_ext_mutex_acquire(&dhd->proto_mutex, OSL_EXT_TIME_FOREVER) == OSL_EXT_SUCCESS)
		return (1);
	else
		return (0);
}

int dhd_os_proto_unblock(dhd_pub_t * pub)
{
	dhd_info_t *dhd = pub->info;
	if (osl_ext_mutex_release(&dhd->proto_mutex) == OSL_EXT_SUCCESS)
		return (1);
	else
		return (0);
}

unsigned int dhd_os_get_ioctl_resp_timeout(void)
{
	return (g_dhd_info->ioctl_timeout_msec);
}

void dhd_os_set_ioctl_resp_timeout(unsigned int timeout_msec)
{
	g_dhd_info->ioctl_timeout_msec = timeout_msec;
}

int dhd_os_ioctl_resp_wait(dhd_pub_t *pub)
{
	dhd_info_t *dhd = pub->info;

	if (osl_ext_sem_take(&dhd->ioctl_resp_wait, g_dhd_info->ioctl_timeout_msec) == OSL_EXT_SUCCESS)
		return (1);
	else
		return (0);
}

int dhd_os_ioctl_resp_wake(dhd_pub_t *pub)
{
	dhd_info_t *dhd = pub->info;

	if (osl_ext_sem_give(&dhd->ioctl_resp_wait) == OSL_EXT_SUCCESS)
		return (1);
	else
		return (0);
}

void dhd_wait_for_event(dhd_pub_t *dhd)
{
	struct dhd_info *dhdinfo = dhd->info;
	osl_ext_status_t status;

	dhdinfo->ctrl_notified = FALSE;
	dhdinfo->ctrl_waiting = TRUE;

	dhd_os_sdunlock(dhd); /* attention here */
	status = osl_ext_sem_take(&dhdinfo->ctrl_wait, 2000);
	dhd_os_sdlock(dhd);

	dhdinfo->ctrl_waiting = FALSE;
	if (status != OSL_EXT_SUCCESS && dhdinfo->ctrl_notified == TRUE) {
		status = osl_ext_sem_take(&dhdinfo->ctrl_wait, 0);
		ASSERT(status == OSL_EXT_SUCCESS);
	}
}

void dhd_wait_event_wakeup(dhd_pub_t*dhd)
{
	struct dhd_info *dhdinfo = dhd->info;

	if (dhdinfo->ctrl_waiting == TRUE) {
		dhdinfo->ctrl_notified = TRUE;
		osl_ext_sem_give(&dhdinfo->ctrl_wait);
	}
}

#define MAX_WAIT_FOR_8021X_TX	10

static int dhd_wait_pend8021x(dhd_info_t *dhd)
{
	unsigned int timeout_msec = 10;
	unsigned int ntimes = MAX_WAIT_FOR_8021X_TX;

	while ((ntimes > 0)) 
    {
		OSL_DELAY(timeout_msec * 1000);
		ntimes--;
	}

	return 0;
}

#include "bcmdefs.h"
#include "osl.h"
#include "pkt_lbuf.h"

#define OS_HANDLE_MAGIC		0x1234abcd	/* Magic # to recognise osh */

/* Operating system state. */
struct osl_info {
	osl_pubinfo_t	pub; /* Publically accessible state variables. */
	uint		    magic; /* Used for debug/validate purposes. */
	pkt_handle_t 	*pkt_info; /* Network interface packet buffer handle. */
	uint 		    malloced; /* Number of bytes allocated. */
	uint 		    failed; /* Number of failed dynamic memory allocations. */
};

static osl_t g_osh;

osl_t * os_resource_init(void)
{
	osl_t *osh = &g_osh;

	bzero(osh, sizeof(osl_t));
	osh->magic = OS_HANDLE_MAGIC;
	osh->pkt_info = pkt_init(osh);
	return (osh);
}

void os_resource_free(osl_t *osh)
{
	if (osh == NULL)
		return;

	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	pkt_deinit(osh, osh->pkt_info);
}

void osl_assert(char *exp, char *file, int line)
{
	int *null = NULL;

	osl_ext_printf("\n\nASSERT \"%s\" failed: file \"%s\", line %d\n\n", exp, file, line);
	/* Force a crash. */
	*null = 0;
}

