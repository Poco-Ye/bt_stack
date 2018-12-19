/*
修改历史：
080624 sunJH
修改路由选择规则：Eth(UP)>Modem(UP)>CDMA(UP)>GPRS(UP)
080625 sunJH
fix the bug: ip_route中
当以太网没有配置时,addr,nexthop,mask都为0,
这时候会导致查找的route都为以太网,
但实际上这时候以太网是不可用的;
080710 sunJH
ip_route: 当dev为PPP链路时,不检查nexthop,
因为nexthop这时候没有任何作用;
090415 sunJH
增加了s_DevInit接口
090824 sunJH
对于PPPoE的处理原则为：优先使用PPPoE而不是Ethernet
*/
#include <inet/inet.h>
#include <inet/dev.h>
#include <inet/udp.h>
#include <inet/dhcp.h>

struct list_head g_dev_list={&g_dev_list, &g_dev_list};
INET_DEV_T *g_default_dev = NULL;

void s_DevInit(void)
{
	INIT_LIST_HEAD(&g_dev_list);
}

void dev_register(INET_DEV_T *dev, int bDefault)
{
	list_add(&dev->list, &g_dev_list);
	/*
	** 取消下面的代码,
	** 改用由ip_route查找可用的dev
	**/
	#if 0
	if(bDefault)
	{
		if(g_default_dev==NULL)
		{
			g_default_dev = dev;
			return;
		}
		if(g_default_dev->ifIndex<PPP_IFINDEX_BASE&&
			dev->ifIndex>=PPP_IFINDEX_BASE)
		{
			//eth dev priority higher than ppp dev
			return;
		}
		g_default_dev = dev;
	}
	#endif
}

INET_DEV_T *dev_lookup(int ifIndex)
{
	struct list_head *p;
	INET_DEV_T *dev;
	list_for_each(p, &g_dev_list)
	{
		dev = list_entry(p, INET_DEV_T, list);
		if(dev->ifIndex == ifIndex)
			return dev;
	}
	return NULL;
}

void dev_show(void)
{
	struct list_head *p;
	INET_DEV_T *dev;
	list_for_each(p, &g_dev_list)
	{
		dev = list_entry(p, INET_DEV_T, list);
		s80_printf("\n%s:\n", dev->name);
		s80_printf("\tStatus        : %s%s%s\n", 
			DEV_FLAGS(dev, FLAGS_UP)?"UP ":"DOWN ",
				DEV_FLAGS(dev, FLAGS_TX)?"TX ":"",
				DEV_FLAGS(dev, FLAGS_RX)?"RX ":""
				);
		s80_printf("\tStats         : %d Snd, %d Rcv, %d Drop, %d CheckErr, %d RQlen, %d SQlen, %d Rst_Tx, %d Rst_Rx, %d RecvNULL\n",
					dev->stats.snd_pkt,
					dev->stats.recv_pkt,
					dev->stats.drop,
					dev->stats.recv_err_check,
					dev->rcv_qlen,
					dev->snd_qlen,
					dev->reset_tx_timeout,
					dev->reset_rx_timeout,
					dev->stats.recv_null
					);
		s80_printf("\tDHCP Enable   : %s\n",
				dev->dhcp_enable?"TRUE":"FALSE"
				);
		s80_printf("\tMAC Addr      : %02X:%02X:%02X:%02X:%02X:%02X\n",
					dev->mac[0],
					dev->mac[1],
					dev->mac[2],
					dev->mac[3],
					dev->mac[4],
					dev->mac[5]
					);
		s80_printf("\tIP Addr       : %d.%d.%d.%d\n",
					((dev->addr)>>24)&0xff,
					((dev->addr)>>16)&0xff,
					((dev->addr)>>8)&0xff,
					((dev->addr)&0xff)&0xff
					);
		s80_printf("\tMask          : %d.%d.%d.%d\n",
					((dev->mask)>>24)&0xff,
					((dev->mask)>>16)&0xff,
					((dev->mask)>>8)&0xff,
					((dev->mask)&0xff)&0xff
					);	
		s80_printf("\tGateWay       : %d.%d.%d.%d\n",
					((dev->next_hop)>>24)&0xff,
					((dev->next_hop)>>16)&0xff,
					((dev->next_hop)>>8)&0xff,
					((dev->next_hop)&0xff)&0xff
					);
		s80_printf("\tDNS Server    : %d.%d.%d.%d\n",
					((dev->dns)>>24)&0xff,
					((dev->dns)>>16)&0xff,
					((dev->dns)>>8)&0xff,
					((dev->dns)&0xff)&0xff
					);
		if(dev->dhcp_enable)
		{
			struct dhcp_client_s *client = dhcpc_get(dev);
			u32 dhcps_ip = 0, leas_time = 0;
			if(client->state == DHCP_ACK)
			{
				dhcps_ip = client->server_ip;
				leas_time = client->lease_time;
			}
			s80_printf("\tDHCP Server   : %d.%d.%d.%d\n",
					((dhcps_ip)>>24)&0xff,
					((dhcps_ip)>>16)&0xff,
					((dhcps_ip)>>8)&0xff,
					((dhcps_ip)&0xff)&0xff
					);
			s80_printf("\tLease Time    : %d\n",
					leas_time
					);
		}
		if(dev->dev_ops->show)
		{
			dev->dev_ops->show(dev);
		}
				
	}
	return ;
}

static struct route local_route;
static struct route default_route;
/*
 * Finds the appropriate network interface for a given IP address. It
 * searches the list of network interfaces linearly. A match is found
 * if the masked IP address of the network interface equals the masked
 * IP address given to the function.
 */
int Mc509MainTaskWaitPppRelink(int time);

struct route *ip_route(u32 dest, INET_DEV_T *bind)
{
	INET_DEV_T *dev=bind;
	struct list_head *p;
	INET_DEV_T *first_up_dev = NULL;/* 优先级最高的可用dev*/

	if (-1==Mc509MainTaskWaitPppRelink(30))
		return NULL;
	
    if (RouteGetDefault_std() == WIFI_IFINDEX_BASE)
    {
        if(ip_addr_netcmp(dest, g_default_dev->addr, g_default_dev->mask))
        {
            local_route.next_hop = dest;
            local_route.out = g_default_dev;
            return &local_route;
        }

        default_route.next_hop = g_default_dev->next_hop;
        default_route.out = g_default_dev;
        return &default_route;
    }

	if(bind)
	{
		if(ip_addr_netcmp(dest, dev->addr, dev->mask))
		{
			local_route.next_hop = dest;
			local_route.out = dev;
			return &local_route;
		}
		default_route.next_hop = dev->next_hop;
		default_route.out = dev;
		return &default_route;
	}

	//s80_printf("ip_route %08x\n", dest);
	list_for_each(p, &g_dev_list)
	{
		dev = list_entry(p, INET_DEV_T, list);
        if(dev && dev->ifIndex == WIFI_IFINDEX_BASE)
            continue;
		if(dev->addr==0)
			continue;
		if(DEV_FLAGS(dev, FLAGS_UP))
		{	/*优先使用PPPoE而不是Ethernet*/		
			switch(dev->ifIndex)
			{
			case ETH_IFINDEX_BASE:
				/*
				当PPPoE可用时则直接使用PPPoE
				*/
				if(first_up_dev==NULL||first_up_dev->ifIndex != PPPOE_IFINDEX)
					first_up_dev = dev;
				break;
			case PPPOE_IFINDEX:
				first_up_dev = dev;
				break;
			default:
				if((first_up_dev&&first_up_dev->ifIndex>dev->ifIndex)||
					first_up_dev==NULL)
				{
					/*
					当dev为PPP链路时,不检查nexthop
					*/
					if((dev->ifIndex>=PPP_IFINDEX_BASE&&
						dev->ifIndex<=MAX_PPP_IFINDEX)||
						dev->next_hop!=0)
						first_up_dev = dev;
				}
			}
		}
		//s80_printf("dev addr %08x mask %08x\n", dev->addr, dev->mask);
		if(ip_addr_netcmp(dest, dev->addr, dev->mask))
		{
			local_route.next_hop = dest;
			local_route.out = dev;
			//s80_printf("Ok\n");
			return &local_route;
		}
	}
	if(!g_default_dev)
	{
		/* 没有设置缺省路由,
		** 采用优先级最高的可用dev
		** Ethernet<---WLAN<---Modem PPP<---CDMA PPP<---GPRS PPP
		**/
		if(first_up_dev)
		{
			default_route.next_hop = first_up_dev->next_hop;
			default_route.out = first_up_dev;
			return &default_route;
		}
		return NULL;
	}
	//s80_printf("Use Default next_hop %08x\n", g_default_dev->next_hop);
	default_route.next_hop = g_default_dev->next_hop;
	default_route.out = g_default_dev;
	return &default_route;
}

int ip_local(u32 dest)
{
	INET_DEV_T *dev;
	struct list_head *p;
	if(dest == 0xffffffff ||
		dest == 0)
	{
		return 1;
	}
	list_for_each(p, &g_dev_list)
	{
		dev = list_entry(p, INET_DEV_T, list);
		if(dest == dev->addr)
			return 1;
	}
	return 0;
}

