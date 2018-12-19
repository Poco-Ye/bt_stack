/*
**
修改历史
20080605
1.增加了header file : inet_softirq.h
20080624
EthSet增加字符串长度检查
080710 sunJH
force_ipstack_print中增加无线信息的显示
080917 sunJH
增加接口NetDevGet获取各种网络设备的IP配置信息
081110 sunJH
s_initNet增加了手机的s_NetAppProxyInit和
座机的s_NetBaseProxyInit调用
081217 sunJH
增加Softirq初始化代码，开关为SOFTIRQ_ENABLE
090423 sunJH
改变系统初始化判断方式
090824 sunJH
增加pppoe初始化入口
090928 sunJH
eth_mac_set不允许设置全0xff和全0的地址
**/
#include <inet/inet.h>
#include <inet/dev.h>
#include <inet/tcp.h>
#include <inet/udp.h>
#include <inet/dhcp.h>
#include <inet/arp.h>
#include <inet/inet_timer.h>
#include <inet/inet_softirq.h>
#include <inet/mem_pool.h>
#ifdef SOFTIRQ_ENABLE
#include "softirq.h"
#endif

static char net_ver=0x15;
static struct dhcp_client_s dhcp_client_eth={0,};

int eth_mac_set_std(unsigned char mac[6])
{
	INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
	if(mac == NULL)
		return NET_ERR_VAL;
	if(memcmp(mac, bc_mac, 6)==0||memcmp(mac, zero_mac, 6)==0)
	{
		return NET_ERR_VAL;
	}
	if(!dev||!dev->dev_ops->set_mac)
		return NET_ERR_IF;
	return dev->dev_ops->set_mac(dev, (u8*)mac);
}

int eth_mac_get_std(unsigned char mac[6])
{
	INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
	if(!dev)
		return NET_ERR_IF;
	INET_MEMCPY(mac, dev->mac, 6);
	return NET_OK;
}

extern INET_DEV_T *g_default_dev;
int WlSetDns(char *dns_ip)
{
	u32 dns;
	INET_DEV_T *dev = dev_lookup(WNET_IFINDEX);

	if (dns_ip==NULL) return NET_ERR_ARG;
	dns = ntohl(inet_addr(dns_ip));
	if(dns == 0 || dns==(u32)-1)
		return NET_ERR_VAL;

	if (dev==NULL) return NET_ERR_IF;
	dev->dns = dns;
	g_default_dev = dev;
	return NET_OK;
}


int EthSet_std(
		char *host_ip, char *host_mask,
		char *gw_ip,
		char *dns_ip
		)
{
	u32 addr, mask, nexthop, dns;
	int ip_max_len = MAX_IPV4_ADDR_STR_LEN;
	INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
	
	if(str_check_max(host_ip, ip_max_len)!=NET_OK||
		str_check_max(host_mask, ip_max_len)!=NET_OK||
		str_check_max(gw_ip, ip_max_len)!=NET_OK||
		str_check_max(dns_ip, ip_max_len)!=NET_OK)
		return NET_ERR_STR;		
	if(!dev)
		return NET_ERR_IF;
	if(host_ip)
	{
		addr = ntohl(inet_addr(host_ip));
		if(addr == 0 || addr==(u32)-1)
			return NET_ERR_VAL;
	}
	else
		addr = dev->addr;
	if(host_mask)
	{
		mask = ntohl(inet_addr(host_mask));
		if(mask == 0 || mask==(u32)-1)
			return NET_ERR_VAL;
	}
	else
		mask = dev->mask;
	if(gw_ip)
	{
		nexthop = ntohl(inet_addr(gw_ip));
		if(nexthop == 0 || nexthop==(u32)-1)
			return NET_ERR_VAL;
	}
	else
		nexthop = dev->next_hop;
	if(dns_ip)
	{
		dns = ntohl(inet_addr(dns_ip));
		if(dns == 0 || dns==(u32)-1)
			return NET_ERR_VAL;
	}
	else
		dns = dev->dns;
	
	dev->addr = addr;
	dev->mask = mask;
	dev->next_hop = nexthop;
	dev->dns = dns;
	if(dev->addr&&dev->dhcp_enable)
	{
		DhcpStop();/* force to stop dhcp */
	}
    dev->dhcp_set_dns_enable = 0;
	return NET_OK;
}

int EthGet_std(
		char *host_ip, char *host_mask,
		char *gw_ip,
		char *dns_ip,
		long *state
		)
{
	INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
	if(!dev)
		return NET_ERR_IF;
	if(host_ip)
		inet_ntoa2(htonl(dev->addr), host_ip);
	if(host_mask)
		inet_ntoa2(htonl(dev->mask), host_mask);
	if(gw_ip)
		inet_ntoa2(htonl(dev->next_hop), gw_ip);
	if(dns_ip)
		inet_ntoa2(htonl(dev->dns), dns_ip);
	if(state)
		*state = dev->flags;
	return NET_OK;
}

int NetDevGet_std(int Dev,/* Dev Interface Index */ 
		char *HostIp,
		char *Mask,
		char *GateWay,
		char *Dns
		)
{
	INET_DEV_T *dev = dev_lookup(Dev);
	if(dev == NULL || !DEV_FLAGS(dev, FLAGS_UP))
		return NET_ERR_IF;
	if(HostIp)
		inet_ntoa2(htonl(dev->addr), HostIp);
	if(Mask)
		inet_ntoa2(htonl(dev->mask), Mask);
	if(GateWay)
		inet_ntoa2(htonl(dev->next_hop), GateWay);
	if(Dns)
		inet_ntoa2(htonl(dev->dns), Dns);
	return NET_OK;
}

static void dhcp_timer_eth(unsigned long flag)
{
    INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
    dhcp_timer(flag,dev);
}

int DhcpStart_std(void)
{
    INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
	int err;

    if(RouteGetDefault_std() == WIFI_IFINDEX_BASE)
        return NET_ERR_RTE;

	if(!dev) return NET_ERR_IF;

    dev->private_dhcp = &dhcp_client_eth;
    dev->dhcp_timer_softirq.h = &dhcp_timer_eth;
    
	err = dhcpc_start(dev, 0);
	if(err != NET_ERR_AGAIN &&
		err != NET_OK)
		return err;
	return NET_OK;
}

void DhcpStop_std(void)
{
    INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
	int err;

     if(RouteGetDefault_std() == WIFI_IFINDEX_BASE)
        return ;

	if(!dev)
		return ;
	dhcpc_stop(dev);
}

int DhcpCheck_std(void)
{
	INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
	if(!dev)
		return NET_ERR_IF;
	return dhcpc_check(dev);
}

/*
    DHCP状态下指定设置DNS server IP,可在DhcpStart()前/后设置，但只有在
    DHCP获取到IP地址时才生效。
*/
int DhcpSetDns_std(const char *addr)
{
    INET_DEV_T *dev = dev_lookup(ETH_IFINDEX_BASE);
//    if(RouteGetDefault_std() != ETH_IFINDEX_BASE)
//        return NET_ERR_RTE;
    if(!dev) return NET_ERR_IF;    
    if(!addr) return NET_ERR_VAL;     
    return dhcp_set_dns(dev,addr);
} 

extern INET_DEV_T *g_default_dev;
int RouteSetDefault_std(int ifIndex)
{
	INET_DEV_T *dev = dev_lookup(ifIndex);
	if(!dev)
		return NET_ERR_IF;
	g_default_dev = dev;
	return 0;
}

int RouteGetDefault_std(void)
{
	if(g_default_dev)
		return g_default_dev->ifIndex;
	return NET_ERR_IF;
}

//debug
extern int is_ipstack_print;
extern struct net_stats g_ip_stats;
extern struct net_stats g_icmp_stats;
extern struct net_stats g_arp_stats;
extern struct net_stats g_udp_stats;

void inet_stats_print(char *name, struct net_stats *stats)
{
	s80_printf("%s:\n", name);
	s80_printf("\trcv %d, snd %d, drop %d\n"
			,
			stats->recv_pkt,
			stats->snd_pkt,
			stats->drop
			);
			
}
void inet_stats_show(void)
{
	inet_stats_print("IP", &g_ip_stats);
	inet_stats_print("ARP", &g_arp_stats);
	inet_stats_print("ICMP", &g_icmp_stats);
	inet_stats_print("TCP", &g_tcp_stats);
	inet_stats_print("UDP", &g_udp_stats);
}
void force_ipstack_print(void)
{
#ifdef NET_DEBUG
	int old;
	inet_softirq_disable();

	mem_debug_show();
	tcp_debug_print_pcbs();
	dev_show();
	inet_stats_show();
	arp_table_show();
	s80_printf("Free Socket Slot: %d\n",
		Netioctl(0, CMD_FD_GET, 0));
	inet_softirq_enable();
#endif	
}

char net_version_get_std(void)
{
	/*
	** 没有以太网,
	** 返回版本号0xFF
	**/
	if(dev_lookup(ETH_IFINDEX_BASE)==NULL)
	{
		return 0xFF;
	}
	return net_ver;
}
int  g_net_init = 0; 
int net_has_init(void)
{
	return g_net_init;
}
/*
** 系统模块初始化
**
**/
void s_initNet_std(void)
{
	g_net_init = 0;
	ipstack_init();
	inet_timer_init();
	s_initEth();
	s_initPPP();
	s_InitWxNetPPP();
	s_initPPPoe();
	g_net_init = 1;
}


