#include "wlm.h"
#include "wlioctl.h"
#include "wl_drv.h"
#include "inet\dev.h"
#include "inet\ethernet.h"
#include "inet\arp.h"
#include "inet\inet_softirq.h"
#include <base.h>
#include "../../dll/dll.h"
#include "../../file/filedef.h"
#include "../apis/wlanapp.h"
#include "ap6181.h"
#include <inet/udp.h>
#include <inet/dhcp.h>


/*sec_mode */
const char wifi_link_sec_mode[2] = {LINK_WEP_SUC,LINK_WPA_SUC}; 
/*link ap  timeout */
const char link_ap_timeout[8]={0x0,0x01,0x02,0x03,0x04,0x04,0x4,0x4};
/*dhcp 变量*/
static struct dhcp_client_s dhcp_client_wifi={0,};
unsigned char wifi_bssid_bak[BSSID_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define MAX_SCANEXT_COUNT	110
typedef struct wifi_scan_cache
{
	int SecMode;	//安全模式
	int Rssi;	//搜索到的AP的信号强度	
	wl_bss_info_t *bi;//保存有效ap数据在缓存中的地址
}WL_SCAN_CACHE;
WL_SCAN_CACHE wl_scan_cache[MAX_SCANEXT_COUNT] = {0};

typedef struct 
{
    char            Ssid[SSID_MAXLEN + 4];
    int             sec_mode;
    int             auth_mode;
    int             encrypt_mode;
    char            key[64];
    int             keyid;
    ST_WEP_KEY      wep;
    int             reconn;
    WIFI_STATUS     current_w_status; 
    unsigned char   mac[6];
	unsigned char   bssid[BSSID_LENGTH];
    int             errno;
    int             dhcp_enable; 
    unsigned char   Ip[4];
    unsigned char   Mask[4];
    unsigned char   Gate[4];
    unsigned char   Dns[4];
    int             rssi;
    int             renew;
    int             icon_fresh_counter;
    wifi_cache_ap_t ap_cache_list[MAX_AP_CACHE_COUNT];
    wifi_cache_ap_t invisible_ap_cache_list[MAX_INVISIBLE_AP_CACHE_COUNT];
    int             ap_cnt_in_cache;
    int             invisible_ap_cnt_in_cache;
    char            scan_buf[20 * 1024]; /* for saving stack space for task */
    volatile int    scan_asyn;
	char			set_dns_flg;	/*DHCP Settings under static DNS flage*/
} wifi_manager_t;

INET_DEV_T      bcm_wifi_dev;
static wifi_manager_t g_wifi_manager;

void dhd_task(void);

static void dhcp_timer_wifi(unsigned long flag)
{
    INET_DEV_T *dev = dev_lookup(WIFI_IFINDEX_BASE);
    dhcp_timer(flag,dev);
}
static int wifi_dhcp_start(INET_DEV_T *dev)
{
	int err;

	if(!dev) return NET_ERR_IF;
	
    dev->private_dhcp = &dhcp_client_wifi;
    dev->dhcp_timer_softirq.h = &dhcp_timer_wifi;
    
	err = dhcpc_start(dev, 0);
	if(err != NET_ERR_AGAIN &&
		err != NET_OK)
		return err;
	return NET_OK;
}
wifi_manager_t * get_global_wifi_manager()
{
    return &g_wifi_manager;
}

static void s_BrcmNetCloseAllSocket(void)
{
	int i;

	for (i = 0; i < MAX_SOCKET; i++)
	{
        WifiNetCloseSocket(i);
	}

    return ;
}

static void ntostr(int num, uchar *arr)
{
    arr[3] = (num & 0xff);
    arr[2] = ((num >> 8) & 0xff);
    arr[1] = ((num >> 16) & 0xff);
    arr[0] = ((num >> 24) & 0xff);
    return ;
}

static void strton(uchar *arr, unsigned long *num)
{
    *num = 0;
    *num |= arr[0]; *num = *num << 8;
    *num |= arr[1]; *num = *num << 8;
    *num |= arr[2]; *num = *num << 8;
    *num |= arr[3];
    return ;
}

static int wifi_arp_entry_delete(INET_DEV_T *dev, wifi_manager_t *p_wifi_manager)
{
	if(!dev || !p_wifi_manager)
		return WIFI_RET_ERROR_PARAMERROR;
	
	wlan_get_bssid(p_wifi_manager->bssid, BSSID_LENGTH);
	if(memcmp(wifi_bssid_bak, p_wifi_manager->bssid, BSSID_LENGTH) == 0)
		return WIFI_RET_OK;
	
	arp_entry_delete(dev);
	memcpy(wifi_bssid_bak, p_wifi_manager->bssid, BSSID_LENGTH);
	return WIFI_RET_OK;
}
static int s_GetConnParam(void)
{
#define DHCP_TIMEOUT    2000 /* unit 10 ms, 20 sec */
    wifi_manager_t * p_wifi_manager = get_global_wifi_manager();
	INET_DEV_T *dev;
	int err, now, done;

	if(!(dev = dev_lookup(WIFI_IFINDEX_BASE))) 
        return WIFI_RET_ERROR_DRIVER;

    if(p_wifi_manager->dhcp_enable)
    {
        if(RouteGetDefault_std() != WIFI_IFINDEX_BASE)
            return WIFI_RET_ERROR_ROUTE;

        dhcpc_stop(dev);

        err = wifi_dhcp_start(dev);
        if((err != NET_ERR_AGAIN) && (err != NET_OK))
        {
            dhcpc_stop(dev);
            return WIFI_RET_ERROR_NOTCONN;
        }

        now = s_Get10MsTimerCount();
        while(dhcpc_check(dev) != NET_OK)
        {
            link_can_stop_enable();
            OsSleep(5); /* warning: this value is relative to DELAY time after "wifi_link_can_stop" 
                       in WifiDisConnect, and must be less than DELAY time. Be careful to modify this, 
                       (10 miliseconds)
                       */
            link_can_stop_disable();
			/*  //dhcp 后 需更新p_wifi_manager参数与bssid 2017-3-6
            if(!WIFI_MANAGER_STATUS_IS_CONNECTING())
            {
            	CMD_DEBUG_PRINT("[%s-%d]state\r\n",FILELINE);
                return WIFI_RET_ERROR_FORCE_STOP;
            }
            */
            if (p_wifi_manager->renew == 2)
            {
                return WIFI_RET_ERROR_FORCE_STOP;
            }

            if(s_Get10MsTimerCount() - now > DHCP_TIMEOUT)
                return WIFI_RET_ERROR_NOTCONN;
        }

        ntostr(dev->addr, p_wifi_manager->Ip);
        ntostr(dev->mask, p_wifi_manager->Mask);
        ntostr(dev->next_hop, p_wifi_manager->Gate);
		if(p_wifi_manager->set_dns_flg == WIFI_DHCP_SET_DNS_FLG_EN)
		{
			dev->dhcp_set_dns_enable = WIFI_DHCP_SET_DNS_FLG_EN;
			strton(p_wifi_manager->Dns, &dev->dns);
		}
		else
		{
        	ntostr(dev->dns, p_wifi_manager->Dns);
		}
    }
    else
    {
        strton(p_wifi_manager->Ip, &dev->addr);
        strton(p_wifi_manager->Mask, &dev->mask);
        strton(p_wifi_manager->Gate, &dev->next_hop);
        strton(p_wifi_manager->Dns, &dev->dns);
    }
	wifi_arp_entry_delete(dev, p_wifi_manager);
    return WIFI_RET_OK;
}

static int bcm_wifi_open(INET_DEV_T *dev, unsigned int flags)
{
	return 0;
}

static int bcm_wifi_close(INET_DEV_T *dev)
{
	return 0;
}

static int bcm_wifi_xmit(INET_DEV_T *dev, struct sk_buff *skb, unsigned long nexthop, unsigned short proto)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
	struct arpentry *entry; 
	int i, err=0;
	
    if(RouteGetDefault_std() != WIFI_IFINDEX_BASE) return NET_OK;
	if(skb == NULL) return NET_OK;

    /* for avoiding confliction */
    if (p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTED &&
        p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTING) 
        return NET_OK;

	if(proto == ETH_P_IP)
    {
        do {
            if(nexthop == 0 || nexthop == 0xffffffff)
            {
                eth_build_header(skb, bc_mac, dev->mac, proto);
                break;
            }

            entry = arp_lookup(dev, nexthop);
            if(!entry)
                return arp_wait(skb, nexthop, proto, dev);
		
	      
            eth_build_header(skb, entry->mac, dev->mac, proto);
        } while(0);
    }
		
    err = _wifi_tx(skb->data, skb->len);
		
	if(proto == ETH_P_IP)
	{
		__skb_pull(skb, sizeof(struct ethhdr));
	}

    return err;
}

int _wifi_rx(uchar *data, int len)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    INET_DEV_T *dev = &bcm_wifi_dev;
    struct sk_buff *skb;
    int flag;

    if (RouteGetDefault_std() != WIFI_IFINDEX_BASE) 
        return NET_OK;

    /* for avoiding confliction */
    if (p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTED &&
        p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTING)
        return NET_OK;

    while(1)
    {
        irq_save(flag);
        if(inet_usable() == 0)
        {
            irq_restore(flag);
            OsSleep(1);
        }
        else
        {
            irq_restore(flag);
            break;
        }
    }
    
    skb = skb_new(len); /* discard data! */
    if (skb == NULL) 
    {
        irq_save(flag);
        inet_release();
        irq_restore(flag);
        return 0;
    }

    memcpy(skb->data, data, len);
    __skb_put(skb, len);
    dev->stats.recv_pkt++;
    eth_input(skb, dev);

    irq_save(flag);
    inet_release();
    irq_restore(flag);
    return 0;
}

static int bcm_wifi_poll(INET_DEV_T *dev, int count)
{
    return 0;
}

static int bcm_wifi_set_mac(INET_DEV_T *dev, uchar *mac)
{
	return 0;
}

static INET_DEV_OPS bcm_wifi_ops=
{
	bcm_wifi_open,
	bcm_wifi_close,
	bcm_wifi_xmit,
	bcm_wifi_poll,
	NULL,
	bcm_wifi_set_mac,
};
int wifi_get_ap_secmode(void)
{
	wifi_manager_t * p_wifi_manager=get_global_wifi_manager();

	return wifi_link_sec_mode[(p_wifi_manager->sec_mode>>1) ? 0x01 : 0x0];

}
void WIFI_MANAGER_SET_STATUS_CONNECTING(void)
{
    wifi_manager_t * p_wifi_manager=get_global_wifi_manager();

    if (p_wifi_manager->current_w_status == WIFI_STATUS_CONNECTED)
    {
        p_wifi_manager->reconn = 1;
        p_wifi_manager->current_w_status = WIFI_STATUS_CONNECTING;
    }
    return ;
}

void WIFI_MANAGER_SET_RENEW_IP(int status)
{
    wifi_manager_t * p_wifi_manager=get_global_wifi_manager();

    if (p_wifi_manager->current_w_status == WIFI_STATUS_CONNECTED)
    {
        if (status == LINK_NONE)
        {
            p_wifi_manager->renew = 1;
			return;
        }
		if (p_wifi_manager->renew == 1)
		{
	       	if(status == LINK_WEP_SUC)
			{
	            if(p_wifi_manager->auth_mode == WLM_WPA_AUTH_NONE || p_wifi_manager->auth_mode == WLM_WPA_AUTH_DISABLED)
	                    p_wifi_manager->renew = 2;
	        }
			else if (status == LINK_WPA_SUC)
	        {
			    if(p_wifi_manager->auth_mode == WLM_WPA_AUTH_PSK || p_wifi_manager->auth_mode == WLM_WPA2_AUTH_PSK)
	                    p_wifi_manager->renew = 2;
			 }
		}
    }
}

int WIFI_MANAGER_STATUS_IS_CONNECTING(void)
{
    wifi_manager_t * p_wifi_manager=get_global_wifi_manager();
    return (p_wifi_manager->current_w_status == WIFI_STATUS_CONNECTING);
}
int wlan_get_join_status(int auth_mode, const int cnt)
{
    const int CONNECT_TIMEOUT=500; /* unit: 10ms */
    int ret, status = 0, sec_mode = LINK_NONE;
	int t0 = s_Get10MsTimerCount();
    reset_connection_status();
	if(auth_mode == WLM_WPA_AUTH_PSK || auth_mode == WLM_WPA2_AUTH_PSK)
	{
		sec_mode = LINK_WPA_SUC;
	}
	else if(auth_mode == WLM_WPA_AUTH_NONE || auth_mode == WLM_WPA_AUTH_DISABLED)
	{
		sec_mode = LINK_WEP_SUC;
	}
	else
	{
		return -2;/*????*/
	}
	if(cnt >= 5)
		return -3;/*????*/
	while (s_Get10MsTimerCount() - t0 < (CONNECT_TIMEOUT+link_ap_timeout[cnt]*100)) 
    {
        ret = wlan_get_connection_status();
        if(ret == sec_mode)
        {
			status = 1;
			break;
		}
        link_can_stop_enable();
        OsSleep(5); /* warning: this value is relative to DELAY time after "wifi_link_can_stop" 
                       in WifiDisConnect, and must be less than DELAY time. Be careful to modify this, 
                       (10 miliseconds)
                       */
        link_can_stop_disable();

        if(!WIFI_MANAGER_STATUS_IS_CONNECTING())
        {
            return -1;
        }
	}
    return !status;
}

void wifi_set_module_mac(uchar *mac)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    memcpy(p_wifi_manager->mac, mac, MAC_ADDR_LENGTH);
}

void wifi_get_module_mac(uchar *mac)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    memcpy(mac, p_wifi_manager->mac, MAC_ADDR_LENGTH);
}

static void wifi_netif_preinit(void)
{
	static int init_flag = 0;
	
	if(!init_flag)
	{
		memset(&bcm_wifi_dev, 0, sizeof(INET_DEV_T));
		bcm_wifi_dev.magic_id = DEV_MAGIC_ID;
		bcm_wifi_dev.flags = FLAGS_TX | FLAGS_UP;
		bcm_wifi_dev.ifIndex = WIFI_IFINDEX_BASE;
		bcm_wifi_dev.header_len = sizeof(struct ethhdr);
		bcm_wifi_dev.mtu = 1518;
		bcm_wifi_dev.dev_ops = &bcm_wifi_ops;

		dev_register(&bcm_wifi_dev, 1);
		init_flag = 1;
	}
}

static void wifi_netif_init(void)
{
    wifi_get_module_mac(bcm_wifi_dev.mac);
    return ;
}

static void wifi_netif_deinit(void)
{
    /* data can be sent out event the ipservice is running */
	BrcmWifiDisconnect();
    return ;
}

static void wifi_ap_info_rearrange(WL_SCAN_CACHE *aps, int count)
{
    WL_SCAN_CACHE tmpAps;
    int i, j;

    for (i = count - 1; i > 0; i--)
    {
        for (j = 0; j < i; j++)
        {
            if(aps[j].Rssi < aps[j+1].Rssi)
            {
                tmpAps = aps[j];
                aps[j] = aps[j+1];
                aps[j+1] = tmpAps;
            }
        }
    }
}

static int wifi_ap_check_existence(char *ssid, int ssid_len, int *sec_mode, int *encrypt_mode)
{
    wifi_manager_t    * p_wifi_manager = get_global_wifi_manager();
    uchar             * scan_buf_ptr   = p_wifi_manager->scan_buf;
    wifi_cache_ap_t   * ap_ptr;
    wl_scan_results_t * list;
    wl_bss_info_t     * bi;
    int i, ret, temp, retry = 0;

    while(1)
    {
        ap_ptr = p_wifi_manager->ap_cache_list;

        if(p_wifi_manager->ap_cnt_in_cache <= 0)
        {
            ret = wlan_scan_network(scan_buf_ptr, sizeof(p_wifi_manager->scan_buf)); /* scan all */
            if(ret == -3) 
            {
                return -3; /* wifi disconnect */
            }
            else if(ret) 
            {
                return -2;
            }

            list = (wl_scan_results_t *)scan_buf_ptr;
            bi = (wl_bss_info_t *)list->bss_info;

            p_wifi_manager->ap_cnt_in_cache = 0;
            list->count = (list->count > MAX_AP_CACHE_COUNT) ? MAX_AP_CACHE_COUNT : list->count;

            for (i = 0; i < list->count; i++)
            {
                memcpy((ap_ptr + i)->essid, bi->SSID, bi->SSID_len);
                (ap_ptr + i)->essid_len = bi->SSID_len;
                (ap_ptr + i)->essid[(ap_ptr + i)->essid_len] = '\0';

                temp = bi->reserved[0];
                if(temp == (WLM_ENCRYPT_TKIP | WLM_ENCRYPT_AES))
                    (ap_ptr + i)->enc_mode = WLM_ENCRYPT_AES;
                else
                    (ap_ptr + i)->enc_mode = temp;

                temp = bi->reserved[1];
                if(temp & WLM_WPA2_AUTH_PSK)
                    (ap_ptr + i)->sec_mode = WLAN_SEC_WPA2_PSK;
                else if(temp & WLM_WPA_AUTH_PSK)
                    (ap_ptr + i)->sec_mode = WLAN_SEC_WPA_PSK;
                else if(temp & WLM_WPA_AUTH_NONE)
                    (ap_ptr + i)->sec_mode = WLAN_SEC_UNSEC;
                else if(temp == WLM_WPA_AUTH_DISABLED)
                    (ap_ptr + i)->sec_mode = WLAN_SEC_UNSEC;

                p_wifi_manager->ap_cnt_in_cache++;
                bi = (wl_bss_info_t *) ((unsigned char *)bi + bi->length);
            }

            retry = 1;
        }

        for (i = 0; i < p_wifi_manager->ap_cnt_in_cache; i++, ap_ptr++)
        {
            if((ap_ptr->essid_len == ssid_len) && !memcmp(ap_ptr->essid, ssid, ssid_len))
            {
                *encrypt_mode = ap_ptr->enc_mode;
                if(*sec_mode == WLAN_SEC_WPAPSK_WPA2PSK) 
                    *sec_mode = ap_ptr->sec_mode;

                return 1;
            }
        }

        if(retry == 1) break;
        p_wifi_manager->ap_cnt_in_cache = 0;
    }

    ap_ptr = p_wifi_manager->invisible_ap_cache_list;

    for (i = 0; i < p_wifi_manager->invisible_ap_cnt_in_cache; i++, ap_ptr++)
    {
        if((ap_ptr->essid_len == ssid_len) && !memcmp(ap_ptr->essid, ssid, ssid_len))
        {
            *encrypt_mode = ap_ptr->enc_mode;
            if(*sec_mode == WLAN_SEC_WPAPSK_WPA2PSK) 
                *sec_mode = ap_ptr->sec_mode;
            return 1;
        }
    }

    return 0;
}

static void wifi_add_invisible_ap_in_cache(char *ssid, int ssid_len, int encrypt_mode, int sec_mode)
{
    wifi_manager_t    * p_wifi_manager = get_global_wifi_manager();
    wifi_cache_ap_t   * ap_ptr         = p_wifi_manager->invisible_ap_cache_list;
    int               i;

    for (i = 0; i <  p_wifi_manager->invisible_ap_cnt_in_cache; i++)
    {
        if(((ap_ptr + i)->essid_len == ssid_len) && !memcmp((ap_ptr + i)->essid, ssid, ssid_len))
        {
            ap_ptr->enc_mode = encrypt_mode;
            ap_ptr->sec_mode = sec_mode;
            return ;
        }
    }

    for (i = p_wifi_manager->invisible_ap_cnt_in_cache - 1; i >= 0; i--)
    {
        *(ap_ptr  + i + 1) = *(ap_ptr + i);
    }

    memcpy(ap_ptr->essid, ssid, ssid_len);
    ap_ptr->essid_len = ssid_len;
    ap_ptr->enc_mode = encrypt_mode; 

    if(sec_mode == WLM_WPA2_AUTH_PSK)
        ap_ptr->sec_mode = WLAN_SEC_WPA2_PSK;
    else if(sec_mode == WLM_WPA_AUTH_PSK)
        ap_ptr->sec_mode = WLAN_SEC_WPA_PSK;
    else if(sec_mode == WLM_WPA_AUTH_NONE)
        ap_ptr->sec_mode = WLAN_SEC_UNSEC;
    else if(sec_mode == WLM_WPA_AUTH_DISABLED)
        ap_ptr->sec_mode = WLAN_SEC_UNSEC;

    if(p_wifi_manager->invisible_ap_cnt_in_cache < MAX_AP_CACHE_COUNT - 1)
        p_wifi_manager->invisible_ap_cnt_in_cache++;

    return ;    
}

static void wifi_del_invisible_ap_in_cache(char *ssid, int ssid_len)
{
    wifi_manager_t    * p_wifi_manager = get_global_wifi_manager();
    wifi_cache_ap_t   * ap_ptr         = p_wifi_manager->invisible_ap_cache_list;
    int               i, j;

    if (p_wifi_manager->invisible_ap_cnt_in_cache <= 0)
    {
        p_wifi_manager->invisible_ap_cnt_in_cache = 0;
        return ;
    }

    for (i = 0;  i < p_wifi_manager->invisible_ap_cnt_in_cache; i++)
    {
        if(((ap_ptr + i)->essid_len == ssid_len) && !memcmp((ap_ptr + i)->essid, ssid, ssid_len))
            break;
    }

    if (i == p_wifi_manager->invisible_ap_cnt_in_cache)
        return ;

    for (j = i; j < p_wifi_manager->invisible_ap_cnt_in_cache - 1; i++)
    {
        *(ap_ptr + i) = *(ap_ptr + i + 1);
    }

    p_wifi_manager->invisible_ap_cnt_in_cache--;
    return ;
}

void wifi_link_task(void)
{
#define CHECK_EXIST_RETRY_CNT   2
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
	int connect_cnt = 0;
	const int MAX_RECONNECT_CNT = 4;
    int ret, retry, auth_type = -1, auth_mode = -1, encrypt_mode = -1, tmp;

    while(1)
    {
        OsSleep(5); /* 10 miliseconds */

        if (p_wifi_manager->scan_asyn == SCAN_ASYN_Q)
        {
            ret = wlan_scan_network2(p_wifi_manager->scan_buf, sizeof(p_wifi_manager->scan_buf)); /* scan all */
            p_wifi_manager->scan_asyn = ret ? SCAN_ASYN_N : SCAN_ASYN_A;
            continue;
        }

        p_wifi_manager->icon_fresh_counter++;
        if(p_wifi_manager->icon_fresh_counter > 100) /* 1 sec */
        {
            p_wifi_manager->icon_fresh_counter = 0;
            if (p_wifi_manager->current_w_status == WIFI_STATUS_CONNECTED) /* update icon */
            {
                if(!wl_get_rssi(&tmp))
                    p_wifi_manager->rssi = tmp;

                if (p_wifi_manager->renew == 2)
                {
                    p_wifi_manager->renew = 0;

                    ret = s_GetConnParam();
                    if(ret == WIFI_RET_ERROR_FORCE_STOP)
                    {
                    }
                    else if(ret) 
                    {
                        wlan_disassociate_network();
                        p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
                        p_wifi_manager->errno = WIFI_RET_ERROR_IPCONF;
                    }
	
                }
            }
        }
        
        if (p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTING)
            continue;

        
        if(encrypt_mode == -1)
        {
            for (retry = 0; retry < CHECK_EXIST_RETRY_CNT; retry++)
            {
                ret = wifi_ap_check_existence(p_wifi_manager->Ssid, strlen(p_wifi_manager->Ssid), &p_wifi_manager->sec_mode,  &encrypt_mode);
                if(ret) break;
            }

            if(ret < 0)
            {
                auth_type    =  -1; 
                auth_mode    =  -1; 
                encrypt_mode =  -1;
                connect_cnt  =   0;
                p_wifi_manager->reconn = 0;

                if(ret == -3)
                    p_wifi_manager->errno = WIFI_RET_ERROR_NOTCONN;
                else 
                    p_wifi_manager->errno = WIFI_RET_ERROR_NORESPONSE;
                p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
                continue;
            }

            if (p_wifi_manager->sec_mode == WLAN_SEC_WPAPSK_WPA2PSK ||
                p_wifi_manager->sec_mode == WLAN_SEC_WPA2_PSK ||
                p_wifi_manager->sec_mode == WLAN_SEC_WPA_PSK)
            {
                p_wifi_manager->encrypt_mode = encrypt_mode;
            }
        }

        if(p_wifi_manager->current_w_status == WIFI_STATUS_CONNECTING)
        {
            if(p_wifi_manager->sec_mode == WLAN_SEC_WEP)
            {
                if(auth_type == WLM_TYPE_SHARED)
                    auth_type =  WLM_TYPE_OPEN;
                else
                    auth_type = WLM_TYPE_SHARED;

                auth_mode = p_wifi_manager->auth_mode;
                encrypt_mode = p_wifi_manager->encrypt_mode;
            }
            else if(p_wifi_manager->sec_mode == WLAN_SEC_WPAPSK_WPA2PSK)
            {
                auth_type = WLM_TYPE_OPEN;

                if(p_wifi_manager->encrypt_mode == -1)
                {
                    if(encrypt_mode == WLM_ENCRYPT_TKIP && auth_mode == WLM_WPA2_AUTH_PSK)
                    {
                        encrypt_mode = WLM_ENCRYPT_TKIP;
                        auth_mode = WLM_WPA_AUTH_PSK;
                    }
                    else if(encrypt_mode == WLM_ENCRYPT_AES && auth_mode == WLM_WPA_AUTH_PSK)
                    {
                        encrypt_mode = WLM_ENCRYPT_TKIP;
                        auth_mode = WLM_WPA2_AUTH_PSK;
                    }
                    else if(encrypt_mode == WLM_ENCRYPT_AES && auth_mode == WLM_WPA2_AUTH_PSK)
                    {
                        encrypt_mode = WLM_ENCRYPT_AES;
                        auth_mode = WLM_WPA_AUTH_PSK;
                    }
                    else
                    {
                        encrypt_mode = WLM_ENCRYPT_AES;
                        auth_mode = WLM_WPA2_AUTH_PSK;
                    }
                }
                else
                {
                    encrypt_mode = p_wifi_manager->encrypt_mode;

                    if(auth_mode == WLM_WPA2_AUTH_PSK)
                        auth_mode = WLM_WPA_AUTH_PSK;
                    else
                        auth_mode = WLM_WPA2_AUTH_PSK;
                }
            }
            else if(p_wifi_manager->sec_mode == WLAN_SEC_WPA_PSK ||
                    p_wifi_manager->sec_mode == WLAN_SEC_WPA2_PSK)
            {
                auth_type = WLM_TYPE_OPEN;

                if(p_wifi_manager->sec_mode == WLAN_SEC_WPA_PSK)
                    auth_mode = WLM_WPA_AUTH_PSK;
                else 
                    auth_mode = WLM_WPA2_AUTH_PSK;

                if(p_wifi_manager->encrypt_mode == -1)
                {
                    if(encrypt_mode == WLM_ENCRYPT_AES)
                        encrypt_mode = WLM_ENCRYPT_TKIP;
                    else
                        encrypt_mode = WLM_ENCRYPT_AES;
                }
                else
                {
                    encrypt_mode = p_wifi_manager->encrypt_mode;
                }
            }
			else if(p_wifi_manager->sec_mode == WLAN_SEC_UNSEC)
			{
				auth_type = WLM_TYPE_OPEN;
				auth_mode = p_wifi_manager->auth_mode;
				encrypt_mode = p_wifi_manager->encrypt_mode;
			}
            else
            {
                auth_type = -1; 
                auth_mode = -1; 
                encrypt_mode = -1;
                connect_cnt = 0;
                p_wifi_manager->reconn = 0;
                p_wifi_manager->ap_cnt_in_cache = 0;

                p_wifi_manager->errno = WIFI_RET_ERROR_DRIVER;
                p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
                continue;
            }

            ret = wlan_join_network(p_wifi_manager->Ssid, auth_type, auth_mode, encrypt_mode, 
                                    p_wifi_manager->key, p_wifi_manager->keyid);
            if (ret) 
            {
                auth_type = -1; 
                auth_mode = -1; 
                encrypt_mode = -1;
                connect_cnt = 0;
                p_wifi_manager->reconn = 0;

                p_wifi_manager->errno = WIFI_RET_ERROR_PARAMERROR;
                p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
                continue;
            }

            ret = wlan_get_join_status(auth_mode, connect_cnt);
            if(!ret)
            {
                
                ret = s_GetConnParam();
                if(ret == WIFI_RET_ERROR_FORCE_STOP)
                {
                    connect_cnt = 0;
                    p_wifi_manager->reconn = 0;
                }
                else if(ret) 
                {
                    wlan_disassociate_network();
                    p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
                    p_wifi_manager->errno = WIFI_RET_ERROR_IPCONF;
                }
                else
                {
                    if(p_wifi_manager->encrypt_mode == -1)
                    {
                        wifi_add_invisible_ap_in_cache(p_wifi_manager->Ssid, 
                                                       strlen(p_wifi_manager->Ssid), 
                                                       encrypt_mode, auth_mode);
                    }

                    p_wifi_manager->encrypt_mode = encrypt_mode;
                    wl_get_rssi(&p_wifi_manager->rssi);
                    p_wifi_manager->current_w_status = WIFI_STATUS_CONNECTED;
                    p_wifi_manager->errno = WIFI_RET_OK;
                }

                auth_type = -1; 
                auth_mode = -1; 
                encrypt_mode = -1; /* position here for save 'encrypt_mode' */
            }
            else if(ret == -1) /* force stop */
            {
                auth_type = -1; 
                auth_mode = -1; 
                encrypt_mode = -1;
            }
            else
            {
                if(p_wifi_manager->reconn)
                {
                    if(p_wifi_manager->sec_mode == WLAN_SEC_WEP)
                    {
                        if(auth_type == WLM_TYPE_SHARED)
                            continue;
                    }
                    else if(p_wifi_manager->sec_mode == WLAN_SEC_WPAPSK_WPA2PSK)
                    {
                        if(p_wifi_manager->encrypt_mode == -1)
                        {
                            if(auth_mode != WLM_WPA_AUTH_PSK || encrypt_mode != WLM_ENCRYPT_TKIP)
                                continue;
                        }
                        else
                        {
                            if(auth_mode != WLM_WPA_AUTH_PSK)
                                continue;
                        }
                    }
                    else if(p_wifi_manager->sec_mode == WLAN_SEC_WPA2_PSK ||
                            p_wifi_manager->sec_mode == WLAN_SEC_WPA_PSK)
                    {
                        if(p_wifi_manager->encrypt_mode == -1)
                        {
                            if(encrypt_mode == WLM_ENCRYPT_AES)
                                continue;
                        }
                    }
                    
                    connect_cnt++;
                    if(connect_cnt <= MAX_RECONNECT_CNT) continue; /* try again */
                    p_wifi_manager->errno = WIFI_RET_ERROR_NOTCONN;
                }
                else
                {
                    p_wifi_manager->errno = WIFI_RET_ERROR_AUTHERROR;
                }

                auth_type = -1; 
                auth_mode = -1; 
                encrypt_mode = -1;

                p_wifi_manager->ap_cnt_in_cache = 0;
                wifi_del_invisible_ap_in_cache(p_wifi_manager->Ssid, strlen(p_wifi_manager->Ssid));
                wlan_disassociate_network();
                p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
            }

            connect_cnt = 0;
            p_wifi_manager->reconn = 0;
        } /* end of WIFI_STATUS_CONNECTING */
    }
}

void BrcmWifiDrvInit(void)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    unsigned char file_name[30], attr[2];

    memset(p_wifi_manager, 0, sizeof(wifi_manager_t));
    p_wifi_manager->current_w_status = WIFI_STATUS_CLOSE;

    memset(file_name, 0, sizeof(file_name));
    strcpy(file_name, MPATCH_NAME_AP6181);
    strcat(file_name, MPATCH_EXTNAME);
    attr[0]=FILE_ATTR_PUBSO;
    attr[1]=FILE_TYPE_SYSTEMSO;
    if(s_SearchFile(file_name, attr) < 0)
    {
        ScrPrint(0, 2, 0, "Wifi Init Failed!");
        ScrPrint(0, 4, 0, "MPATCH NOT FOUND!");
        DelayMs(1000);
    }
	
    wifi_netif_preinit();
	
    OsCreate((OS_TASK_ENTRY)dhd_task, TASK_PRIOR_WIFI_DRV, 0x3000);
    OsCreate((OS_TASK_ENTRY)wifi_link_task, TASK_PRIOR_WIFI_LINK, 0x2000);
    return ;
}

int BrcmWifiOpen(void)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    char buff[128];
    int ret;

    ret = BrcmGetPatchVersion(buff, sizeof(buff));
    if(ret < 0) return WIFI_RET_ERROR_STATUSERROR;

    if (p_wifi_manager->current_w_status != WIFI_STATUS_CLOSE)
        BrcmWifiClose();

    ret = sdio_init();
    if(ret) return WIFI_RET_ERROR_NODEV;

    ret = ap6181_init();
    if(ret) return WIFI_RET_ERROR_NODEV;

    wifi_netif_init();

    p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
    p_wifi_manager->errno = WIFI_RET_ERROR_NOTCONN;
	p_wifi_manager->ap_cnt_in_cache = 0;
	p_wifi_manager->invisible_ap_cnt_in_cache = 0;
    p_wifi_manager->scan_asyn = SCAN_ASYN_I;
	p_wifi_manager->set_dns_flg = WIFI_DHCP_SET_DNS_FLG_DIS;
    return WIFI_RET_OK;
}

int BrcmWifiScan(ST_WIFI_AP *outAps, unsigned int ApCount)
{
	wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
	wifi_cache_ap_t *cache_ap_ptr = p_wifi_manager->ap_cache_list;
	wl_scan_results_t *list;
    wl_bss_info_t *bi;
	
	char scan_buf[20 * 1024];
	int ret, i, temp, ApToal = 0;
	
    if (p_wifi_manager->current_w_status == WIFI_STATUS_CLOSE)
		return WIFI_RET_ERROR_NOTOPEN;

    if (p_wifi_manager->current_w_status != WIFI_STATUS_NOTCONN)
		return WIFI_RET_ERROR_STATUSERROR;

	if (ApCount == 0)
		return WIFI_RET_ERROR_PARAMERROR;

	if (outAps == NULL)
		return WIFI_RET_ERROR_NULL;
	
	memset(outAps, 0x00, ApCount * sizeof(ST_WIFI_AP));
	memset(wl_scan_cache,0x00, (MAX_AP_CACHE_COUNT * sizeof(WL_SCAN_CACHE)));
	
	ret = wlan_scan_network(scan_buf, sizeof(scan_buf)); /* scan all */
    if(ret) return WIFI_RET_ERROR_NODEV;

	list = (wl_scan_results_t *)scan_buf;
    bi = (wl_bss_info_t *)list->bss_info;
    p_wifi_manager->ap_cnt_in_cache = 0;
	
	for (i = 0; i < list->count; i++,bi = (wl_bss_info_t *) ((unsigned char *)bi + bi->length))
	{
		/*去掉无效的ap*/
		if((bi->SSID_len == 0) || (strlen(bi->SSID)==0)) continue;
		if(ApToal >= MAX_AP_CACHE_COUNT) break;
			
		memcpy(cache_ap_ptr->essid, bi->SSID, bi->SSID_len);
	    cache_ap_ptr->essid_len = bi->SSID_len;
	    cache_ap_ptr->essid[cache_ap_ptr->essid_len] = '\0';

	    temp = bi->reserved[1];

	    if(temp & WLM_WPA2_AUTH_PSK)
	    {
	        if(temp & WLM_WPA_AUTH_PSK)
	            wl_scan_cache[ApToal].SecMode = WLAN_SEC_WPAPSK_WPA2PSK;
	        else
	            wl_scan_cache[ApToal].SecMode = WLAN_SEC_WPA2_PSK;
	    }
	    else if(temp & WLM_WPA_AUTH_PSK)
	    {
	        wl_scan_cache[ApToal].SecMode = WLAN_SEC_WPA_PSK;
	    }
	    else if(temp & WLM_WPA_AUTH_NONE)
	    {
	        wl_scan_cache[ApToal].SecMode = WLAN_SEC_UNSEC;
	    }
	    else if(temp == WLM_WPA_AUTH_DISABLED)
	    {
	        wl_scan_cache[ApToal].SecMode = WLAN_SEC_UNSEC;
	    }
	    else 
	    {
	        wl_scan_cache[ApToal].SecMode = temp;
	        return WIFI_RET_ERROR_PARAMERROR;
	    }

	    cache_ap_ptr->sec_mode = wl_scan_cache[ApToal].SecMode;

	    temp = bi->reserved[0];

	    if(temp == WLM_ENCRYPT_WEP)
	    {
	        wl_scan_cache[ApToal].SecMode = WLAN_SEC_WEP;
	    }
    
	    if(temp == (WLM_ENCRYPT_TKIP | WLM_ENCRYPT_AES))
	        cache_ap_ptr->enc_mode = WLM_ENCRYPT_AES;
	    else
	        cache_ap_ptr->enc_mode = temp;
		
		p_wifi_manager->ap_cnt_in_cache++;
		cache_ap_ptr++;

	    wl_scan_cache[ApToal].Rssi = bi->RSSI;
		wl_scan_cache[ApToal].bi = bi;
		++ApToal;
	}

	/*根据 RSSi 排序*/
	wifi_ap_info_rearrange(wl_scan_cache, ApToal);
	/*拷贝数*/
	ApCount = (ApCount >= ApToal) ? ApToal : ApCount;
	for(i = 0; i < ApCount; i++)
	{
		if(wl_scan_cache[i].bi->SSID_len >= sizeof(outAps->Ssid))
	    {
	        memcpy(outAps->Ssid, wl_scan_cache[i].bi->SSID, sizeof(outAps->Ssid));
	    }
	    else
	    {
	        memcpy(outAps->Ssid, wl_scan_cache[i].bi->SSID, wl_scan_cache[i].bi->SSID_len);
	        outAps->Ssid[wl_scan_cache[i].bi->SSID_len] = '\0';
	    }
		outAps->SecMode = wl_scan_cache[i].SecMode;
		outAps->Rssi = wl_scan_cache[i].Rssi;
		outAps++;
	}
	return ApCount;
}

int BrcmWifiScanExt(ST_WIFI_AP_EX *pstWifiApsInfo, unsigned int apCnt)
{
	#define SCAN_TIMEOUT 700 /* unit 10ms, 5 sec */
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    unsigned char *scan_buf_ptr = p_wifi_manager->scan_buf;
    wl_scan_results_t *list;
    wl_bss_info_t *bi;
    int ret, i, j, temp, ApToal = 0;

    if (p_wifi_manager->current_w_status == WIFI_STATUS_CLOSE)
		return WIFI_RET_ERROR_NOTOPEN;

    if (apCnt <= 0)
        return WIFI_RET_ERROR_PARAMERROR;

	if (pstWifiApsInfo == NULL)
		return WIFI_RET_ERROR_NULL;

    p_wifi_manager->scan_asyn = SCAN_ASYN_Q; 
    temp = s_Get10MsTimerCount();

    while(p_wifi_manager->scan_asyn != SCAN_ASYN_A)
    {
        if (s_Get10MsTimerCount() - temp > SCAN_TIMEOUT)
        {
            p_wifi_manager->scan_asyn = SCAN_ASYN_I;
            return WIFI_RET_ERROR_NORESPONSE;
        }

        if (p_wifi_manager->scan_asyn == SCAN_ASYN_N)
        {
            p_wifi_manager->scan_asyn = SCAN_ASYN_I;
            return WIFI_RET_ERROR_DRIVER;
        }
    }

	memset(pstWifiApsInfo, 0, apCnt * sizeof(ST_WIFI_AP_EX));
	memset(wl_scan_cache, 0, MAX_SCANEXT_COUNT * sizeof(WL_SCAN_CACHE));

	list = (wl_scan_results_t *)scan_buf_ptr;
	bi = (wl_bss_info_t *)list->bss_info;

    for (i = 0; i < list->count; i++, bi = (wl_bss_info_t *) ((unsigned char *)bi + bi->length))
    {
    	if((bi->SSID_len == 0) || (strlen(bi->SSID) == 0)) continue;
		if(ApToal >= MAX_SCANEXT_COUNT) break;
			
        temp = bi->reserved[1];

        if(temp & WLM_WPA2_AUTH_PSK)
        {
            if(temp & WLM_WPA_AUTH_PSK)
                wl_scan_cache[ApToal].SecMode = WLAN_SEC_WPAPSK_WPA2PSK;
            else
                wl_scan_cache[ApToal].SecMode = WLAN_SEC_WPA2_PSK;
        }
        else if(temp & WLM_WPA_AUTH_PSK)
        {
            wl_scan_cache[ApToal].SecMode = WLAN_SEC_WPA_PSK;
        }
        else if(temp & WLM_WPA_AUTH_NONE)
        {
            wl_scan_cache[ApToal].SecMode = WLAN_SEC_UNSEC;
        }
        else if(temp == WLM_WPA_AUTH_DISABLED)
        {
            wl_scan_cache[ApToal].SecMode = WLAN_SEC_UNSEC;
        }
        else 
        {
            wl_scan_cache[ApToal].SecMode = temp;
            return WIFI_RET_ERROR_PARAMERROR;
        }
        temp = bi->reserved[0];

        if(temp == WLM_ENCRYPT_WEP)
            wl_scan_cache[ApToal].SecMode = WLAN_SEC_WEP;
        
        wl_scan_cache[ApToal].Rssi = bi->RSSI;
		wl_scan_cache[ApToal].bi = bi;
		ApToal++;
    }
	/*根据 RSSi 排序*/
	wifi_ap_info_rearrange(wl_scan_cache, ApToal);
	/*拷贝数*/
	apCnt = (apCnt >= ApToal) ? ApToal : apCnt;
	for(i = 0; i < apCnt; i++)
	{

        if(wl_scan_cache[i].bi->SSID_len >= sizeof(pstWifiApsInfo->stBasicInfo.Ssid))
        {
            memcpy(pstWifiApsInfo->stBasicInfo.Ssid, wl_scan_cache[i].bi->SSID, sizeof(pstWifiApsInfo->stBasicInfo.Ssid));
        }
        else
        {
            memcpy(pstWifiApsInfo->stBasicInfo.Ssid, wl_scan_cache[i].bi->SSID, wl_scan_cache[i].bi->SSID_len);
            pstWifiApsInfo->stBasicInfo.Ssid[wl_scan_cache[i].bi->SSID_len] = '\0';
        }
		pstWifiApsInfo->stBasicInfo.Rssi = wl_scan_cache[i].Rssi;
		pstWifiApsInfo->stBasicInfo.SecMode = wl_scan_cache[i].SecMode;
		pstWifiApsInfo->bssType = (wl_scan_cache[i].bi->capability & 0x02) ? BSS_TYPE_IBSS : BSS_TYPE_INFRA;
        pstWifiApsInfo->channel = wl_scan_cache[i].bi->chanspec & 0x0F;
        
        memcpy(pstWifiApsInfo->Bssid, wl_scan_cache[i].bi->BSSID.octet, BSSID_LEN);
		pstWifiApsInfo++;
	}
    return apCnt;
}



int BrcmWifiConnect(ST_WIFI_AP *Ap, ST_WIFI_PARAM *WifiParam)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    int ret, auth_mode, encrypt_mode, i;
    char key[256] = {0};

    if (WifiParam->DhcpEnable != 0 && WifiParam->DhcpEnable != 1)
        return WIFI_RET_ERROR_PARAMERROR;

    if (p_wifi_manager->current_w_status == WIFI_STATUS_CLOSE)
		return WIFI_RET_ERROR_NOTOPEN;

	if (p_wifi_manager->current_w_status != WIFI_STATUS_NOTCONN)
		return WIFI_RET_ERROR_STATUSERROR;

    if(Ap == NULL) return WIFI_RET_ERROR_PARAMERROR;

    if(Ap->SecMode == WLAN_SEC_WPA2_PSK || Ap->SecMode == WLAN_SEC_WPAPSK_WPA2PSK || Ap->SecMode == WLAN_SEC_WPA_PSK)
    {
        if(!strlen(WifiParam->Wpa)) 
            return WIFI_RET_ERROR_PARAMERROR;

        encrypt_mode = -1;
        if(Ap->SecMode == WLAN_SEC_WPA2_PSK || Ap->SecMode == WLAN_SEC_WPAPSK_WPA2PSK)
            auth_mode = WLM_WPA2_AUTH_PSK;
        else if(Ap->SecMode == WLAN_SEC_WPA_PSK)
            auth_mode = WLM_WPA_AUTH_PSK;

        strcpy(key, WifiParam->Wpa);
    }
#ifndef PED_PCI
    else if(Ap->SecMode == WLAN_SEC_WEP)
    {
        if (WifiParam->Wep.KeyLen != KEY_WEP_LEN_64 &&
            WifiParam->Wep.KeyLen != KEY_WEP_LEN_128)
            return WIFI_RET_ERROR_PARAMERROR;

        auth_mode = WLM_WPA_AUTH_NONE;
        encrypt_mode = WLM_ENCRYPT_WEP;
        p_wifi_manager->keyid = WifiParam->Wep.Idx;
        s_WifiAsciiToHex(WifiParam->Wep.Key[WifiParam->Wep.Idx], key, WifiParam->Wep.KeyLen);
    }
    else if(Ap->SecMode == WLAN_SEC_UNSEC)
    {
        auth_mode = WLM_WPA_AUTH_DISABLED;
        encrypt_mode = WLM_ENCRYPT_NONE;
    }
#endif
    else
    {
        return WIFI_RET_ERROR_PARAMERROR;
    }

    p_wifi_manager->dhcp_enable = WifiParam->DhcpEnable ? 1 : 0;
    if(!p_wifi_manager->dhcp_enable)
    {
        memcpy(p_wifi_manager->Ip, WifiParam->Ip, IPLEN);
        memcpy(p_wifi_manager->Mask, WifiParam->Mask, IPLEN);
        memcpy(p_wifi_manager->Gate, WifiParam->Gate, IPLEN);
        memcpy(p_wifi_manager->Dns, WifiParam->Dns, IPLEN);
    } 
    else
    {
    	 if(RouteSetDefault(WIFI_IFINDEX_BASE))
            return WIFI_RET_ERROR_ROUTE;
    }

    s_BrcmNetCloseAllSocket(); /* close all sockets absolutely */

    memcpy(&p_wifi_manager->wep, &WifiParam->Wep, sizeof(ST_WEP_KEY));
    strcpy(p_wifi_manager->key, key);
    memcpy(p_wifi_manager->Ssid, Ap->Ssid, sizeof(Ap->Ssid));
    p_wifi_manager->Ssid[sizeof(Ap->Ssid)] = '\0';
    p_wifi_manager->sec_mode = Ap->SecMode;
    p_wifi_manager->auth_mode = auth_mode;
    p_wifi_manager->encrypt_mode = encrypt_mode;
    p_wifi_manager->current_w_status = WIFI_STATUS_CONNECTING;
    p_wifi_manager->reconn = 1;
    p_wifi_manager->renew = 0;
	p_wifi_manager->set_dns_flg = WIFI_DHCP_SET_DNS_FLG_DIS;
    return WIFI_RET_OK;	
}

int BrcmWifiDisconnect(void)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
	int t0 = s_Get10MsTimerCount();

    if (p_wifi_manager->current_w_status == WIFI_STATUS_CLOSE)
		return WIFI_RET_ERROR_NOTOPEN;

    if (p_wifi_manager->current_w_status == WIFI_STATUS_SLEEP)
        return WIFI_RET_ERROR_STATUSERROR;

    s_BrcmNetCloseAllSocket(); /* close all sockets absolutely */
    
    /* warning: disconnect forbidded when dhcp works, may wait 20 second at most */
    while (p_wifi_manager->current_w_status == WIFI_STATUS_CONNECTING) 
    {
        int flag;
        irq_save_asm(&flag);
        if(wifi_link_can_stop() || (s_Get10MsTimerCount() - t0) >= 100)
        {
            p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
            irq_restore_asm(&flag);
            break;
        }
        irq_restore_asm(&flag);
        DelayMs(10);
    }

    DelayMs(20); /* warning: this value is relative to OsSleep time after "link_can_stop" 
                       in "wlan_get_join_status", and must be more than OsSleep time. 
                       <Be careful to modify this>
                  */

	if (p_wifi_manager->current_w_status == WIFI_STATUS_CONNECTED ||
        p_wifi_manager->current_w_status == WIFI_STATUS_NOTCONN)
	{
		p_wifi_manager->reconn = 0;
		p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
        wlan_disassociate_network();
        
	}

    if (p_wifi_manager->dhcp_enable)
    {
        INET_DEV_T *dev=dev_lookup(WIFI_IFINDEX_BASE);
        if(!dev) return WIFI_RET_ERROR_DRIVER;

        dhcpc_stop(dev);
    }
	p_wifi_manager->set_dns_flg = WIFI_DHCP_SET_DNS_FLG_DIS;
    p_wifi_manager->errno = WIFI_RET_ERROR_NOTCONN;
	return WIFI_RET_OK;
}

int BrcmWifiCheck(ST_WIFI_AP *Ap)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();

    if (p_wifi_manager->current_w_status == WIFI_STATUS_CLOSE)
		return WIFI_RET_ERROR_NOTOPEN;

    if (p_wifi_manager->current_w_status == WIFI_STATUS_SLEEP)
		return WIFI_RET_ERROR_STATUSERROR;

	if (p_wifi_manager->current_w_status == WIFI_STATUS_NOTCONN)
		return p_wifi_manager->errno;
	
	if (p_wifi_manager->current_w_status == WIFI_STATUS_CONNECTED)
    {
        int signal;

        signal = WifiRssiToSignal(p_wifi_manager->rssi);
    	if (Ap != NULL) 
        {
            if(strlen(p_wifi_manager->Ssid) >= sizeof(Ap->Ssid))
                memcpy(Ap->Ssid, p_wifi_manager->Ssid, sizeof(Ap->Ssid));
            else
                strcpy(Ap->Ssid, p_wifi_manager->Ssid);

            Ap->Rssi = p_wifi_manager->rssi;
            Ap->SecMode = p_wifi_manager->sec_mode;
        }
		return signal;
    }

    return WIFI_RET_OK; /* connecting */
}

int WifiIsBusy(void)
{
    return wifi_check_busy();
}

int BrcmWifiCtrl(uint iCmd, void *pArgIn, uint iSizeIn,void *pArgOut, uint iSizeOut)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    ST_WIFI_PARAM *p_wifi_param = pArgOut;
	INET_DEV_T *dev;
	u32 set_dns = 0;

	if (p_wifi_manager->current_w_status == WIFI_STATUS_CLOSE)
		return WIFI_RET_ERROR_NOTOPEN;
	if(iCmd > (WIFI_CTRL_CMD_MAX_INDEX - 1))
		return WIFI_RET_ERROR_PARAMERROR;
	
	switch(iCmd){
	case WIFI_CTRL_ENTER_SLEEP:/*进入休眠*/
		if (p_wifi_manager->current_w_status != WIFI_STATUS_NOTCONN)
			return WIFI_RET_ERROR_STATUSERROR;
		
        wifi_sleep(1);
		p_wifi_manager->current_w_status = WIFI_STATUS_SLEEP;
		break;
	case WIFI_CTRL_OUT_SLEEP:/*退出休眠*/
		if (p_wifi_manager->current_w_status != WIFI_STATUS_SLEEP)
			return WIFI_RET_ERROR_STATUSERROR;

        wifi_sleep(0);
		p_wifi_manager->current_w_status = WIFI_STATUS_NOTCONN;
		break;
	case WIFI_CTRL_GET_PARAMETER:/*获取连接参数*/
		if (pArgOut == NULL) return WIFI_RET_ERROR_NULL;
		if (iSizeOut != sizeof(ST_WIFI_PARAM)) return WIFI_RET_ERROR_PARAMERROR;
		if (p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTED) return WIFI_RET_ERROR_STATUSERROR;

        p_wifi_param->DhcpEnable = p_wifi_manager->dhcp_enable;
        memcpy(p_wifi_param->Ip, p_wifi_manager->Ip, IPLEN);
        memcpy(p_wifi_param->Mask, p_wifi_manager->Mask, IPLEN);
        memcpy(p_wifi_param->Gate, p_wifi_manager->Gate, IPLEN);
        memcpy(p_wifi_param->Dns, p_wifi_manager->Dns, IPLEN);
        strcpy(p_wifi_param->Wpa, p_wifi_manager->key);
        memcpy(&p_wifi_param->Wep, &p_wifi_manager->wep, sizeof(ST_WEP_KEY));
		break;
	case WIFI_CTRL_GET_MAC:/*获取设备MAC地址*/
		if (pArgOut == NULL) return WIFI_RET_ERROR_NULL;
		if (iSizeOut != MAC_ADDR_LENGTH) return WIFI_RET_ERROR_PARAMERROR;

        memcpy((uchar *)pArgOut, p_wifi_manager->mac, MAC_ADDR_LENGTH);
		break;
	case WIFI_CTRL_GET_BSSID:/*获取连接ap的bssid*/
		if (pArgOut == NULL) return WIFI_RET_ERROR_NULL;
		if (iSizeOut != BSSID_LENGTH) return WIFI_RET_ERROR_PARAMERROR;
		if (p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTED)
			return WIFI_RET_ERROR_STATUSERROR;
		
		memcpy((uchar *)pArgOut, p_wifi_manager->bssid, BSSID_LEN);
		break;
	case WIFI_CTRL_SET_DNS_ADDR:/*dhcp设置静态DNS地址*/
		if (pArgIn == NULL) return WIFI_RET_ERROR_NULL; 
		if (iSizeIn!=IPLEN) return WIFI_RET_ERROR_PARAMERROR;
		
		if(!(dev = dev_lookup(WIFI_IFINDEX_BASE))) return WIFI_RET_ERROR_DRIVER;
		if (p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTED) return WIFI_RET_ERROR_STATUSERROR;
		
		memcpy(p_wifi_manager->Dns,(uchar *)pArgIn,IPLEN);
		strton(p_wifi_manager->Dns, &set_dns);
		if(set_dns == 0 || set_dns == (u32)-1) return NET_ERR_VAL;
		
		if(p_wifi_manager->dhcp_enable == 1)
		{
			inet_softirq_disable();
			dev->dns = dev->dhcp_set_dns_value = set_dns;
			dev->dhcp_set_dns_enable = 0x01;
			p_wifi_manager->set_dns_flg = WIFI_DHCP_SET_DNS_FLG_EN;
			inet_softirq_enable();
		}
		else
		{
			inet_softirq_disable();
			dev->dns = set_dns;
			inet_softirq_enable();
		}
		break;
	}

    return WIFI_RET_OK;
}

int BrcmWifiClose(void)
{
    wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
    int ret;

	if (p_wifi_manager->current_w_status == WIFI_STATUS_CLOSE) 
        return WIFI_RET_OK;

    if (p_wifi_manager->current_w_status == WIFI_STATUS_SLEEP) 
        BrcmWifiCtrl(1, NULL, 0, NULL, 0);

    wifi_netif_deinit();
    ap6181_deinit();
    sdio_deinit();

	p_wifi_manager->current_w_status = WIFI_STATUS_CLOSE;
	return WIFI_RET_OK;
}

int BrcmWifiUpdateFw(void)
{
	PARAM_STRUCT param;
	int iRet;
	if (is_wifi_module()!=WIFI_DEV_AP6181) return -1;
	
	param.FuncType = MPATCH_WIFI_FUNC;
	param.FuncNo = MPATCH_WIFI_UPDATE_FW;
	iRet = s_MpatchEntry(MPATCH_NAME_AP6181, &param);
	if (iRet < 0)
		return iRet;
	return param.int1;
}

int BrcmGetPatchVersion(uchar *buf, int size)
{
	PARAM_STRUCT param;
	int iRet;
	if (is_wifi_module()!=WIFI_DEV_AP6181) return -1;
	
	memset(buf, 0, size);
	param.FuncType = MPATCH_WIFI_FUNC;
	param.FuncNo = MPATCH_WIFI_GET_PATCH_VER;
	param.u_str1 = buf;
	param.int1 = size;
	iRet = s_MpatchEntry(MPATCH_NAME_AP6181, &param);
	if (iRet < 0)
		return iRet;
	else
		return 0;
}

int BrcmGetFwInfo(firmware_info_t *firm_info, int size)
{
	PARAM_STRUCT param;
	int iRet;
	if (is_wifi_module()!=WIFI_DEV_AP6181) return -1;
	
	param.FuncType = MPATCH_WIFI_FUNC;
	param.FuncNo = MPATCH_WIFI_GET_FIRM_INFO;
	param.Addr1 = firm_info;
	param.u_int1 = size;
	iRet = s_MpatchEntry(MPATCH_NAME_AP6181, &param);
	if (iRet < 0)
		return iRet;
	else
		return 0;
}

int BrcmWifiGetVer(uchar * wifiFwVer)
{
    firmware_info_t fw_info;
    int ret;

    memset(wifiFwVer, 0, 6);
    ret = BrcmGetFwInfo(&fw_info, sizeof(firmware_info_t));
    if(ret) 
    {
        return -1;
    }

    strcpy(wifiFwVer, fw_info.block_info[0]->version);
    return 0;
}

void BrcmNone(void) 
{
    return ;
}

int BrcmWifiGetMac(unsigned char *mac)
{
    wifi_get_module_mac(mac);
    return 0;
}
int DnsResolveExt_ap6181_std(char *name/*IN*/, char *result/*OUT*/, int len, int timeout)
{
	if(strlen(name) > 42)
		return NET_ERR_STR;
	return DnsResolveExt_std(name, result, len, timeout);
}
int WifiNetDevGet_std(int Dev, char *HostIp,char *Mask,char *GateWay,char *Dns)
{
	int err = 0;
	wifi_manager_t *p_wifi_manager = get_global_wifi_manager();
	if (p_wifi_manager->current_w_status != WIFI_STATUS_CONNECTED) return WIFI_RET_ERROR_STATUSERROR;

	err = NetDevGet_std(Dev,HostIp, Mask, GateWay, Dns);
	return err;
}

void BrcmWifiPowerSwitch(int onoff)
{
	ap6181_pwr(onoff);
}

struct ST_WIFIOPS gszBrcmWifiOps = 
{
	//ip stack
	.NetSocket = NetSocket_std,
	.NetConnect = NetConnect_std,
	.NetBind = NetBind_std,
	.NetListen = NetListen_std,
	.NetAccept = NetAccept_std,
	.NetSend = NetSend_std,
	.NetSendto = NetSendto_std,
	.NetRecv = NetRecv_std,
	.NetRecvfrom = NetRecvfrom_std,
	.NetCloseSocket = NetCloseSocket_std,
	.Netioctl = Netioctl_std,
	.SockAddrSet = SockAddrSet_std,
	.SockAddrGet = SockAddrGet_std,
	.DnsResolve = DnsResolve_std,
	.DnsResolveExt = DnsResolveExt_ap6181_std,
	.NetPing = NetPing_std,
	.RouteSetDefault = RouteSetDefault_std,
	.RouteGetDefault = RouteGetDefault_std,
	.NetDevGet = WifiNetDevGet_std,
	//wifi
	.WifiOpen = BrcmWifiOpen,
	.WifiClose = BrcmWifiClose,
	.WifiScan = BrcmWifiScan,
    .WifiScanEx = BrcmWifiScanExt,
	.WifiConnect = BrcmWifiConnect,
	.WifiDisconnect = BrcmWifiDisconnect,
	.WifiCheck = BrcmWifiCheck,
	.WifiCtrl = BrcmWifiCtrl,
	//内部使用
	.s_wifi_mac_get = BrcmWifiGetMac,
	.s_WifiInSleep = BrcmNone,
	.s_WifiOutSleep = BrcmNone,
	.WifiDrvInit = BrcmWifiDrvInit,
	.WifiUpdate = BrcmNone,
	.WifiGetHVer = BrcmWifiGetVer,
	.s_WifiPowerSwitch = BrcmWifiPowerSwitch,
};

