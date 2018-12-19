#ifndef __AP6181_WIFI_H__
#define __AP6181_WIFI_H__
#include <base.h>
#include <socket.h>
#include "../apis/wlanapp.h"

#define     MAX_AP_CACHE_COUNT  40
#define     MAX_INVISIBLE_AP_CACHE_COUNT    10
#define     MAC_ADDR_LENGTH     6
#define 	BSSID_LENGTH		6


typedef enum {
    WIFI_ENC_NONE = 0,
    WIFI_ENC_TKIP,
    WIFI_ENC_CCMP, 
    WIFI_ENC_WEP,
} WIFI_ENCRYPT_MODE;


typedef enum 
{
	WIFI_STATUS_CLOSE=0,
	WIFI_STATUS_NOTCONN,
	WIFI_STATUS_RESET,
	WIFI_STATUS_SLEEP,
	WIFI_STATUS_SCAN,
	WIFI_STATUS_CONNECTING,
	WIFI_STATUS_SETDNS,
	WIFI_STATUS_IPCONF,
	WIFI_STATUS_NWPARAMS,
	WIFI_STATUS_PWMODE,
	WIFI_STATUS_DISCON,
	WIFI_STATUS_CONNECTED,
} WIFI_STATUS;

typedef struct 
{
    char essid[32];
    int essid_len;
    int enc_mode;
    int sec_mode;
} wifi_cache_ap_t;

enum SCAN_ASYN_EVENT {
    SCAN_ASYN_I =0,
    SCAN_ASYN_Q,
    SCAN_ASYN_A,
    SCAN_ASYN_N,
};

#define WIFI_DHCP_SET_DNS_FLG_EN	(0x01)	/*dhcp 使能， 设置DNS 地址标志*/
#define WIFI_DHCP_SET_DNS_FLG_DIS	(0x00)	/*dhcp 使能， 不设置DNS 地址标志 */

#endif /* __AP6181_WIFI_H__ */
