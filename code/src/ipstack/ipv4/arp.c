/*
090415 sunJH
arp_init增加了对arp_list的初始化
090514 sunJH
增加接口NetAddStaticArp提供静态arp功能
100925 sunJH
美国客户使用时发现了一个Bug:
具体请看arp_input函数内注释
*/
#include <inet/inet.h>
#include <inet/inet_type.h>
#include <inet/dev.h>
#include <inet/skbuff.h>
#include <inet/arp.h>
#include <inet/ethernet.h>
#include <inet/ip.h>
#include <inet/mem_pool.h>
#include <inet/inet_softirq.h>

#define MAX_ARP_ENTRY_COUNT (MAX_SOCK_COUNT+10)
struct net_stats  g_arp_stats={0,};
struct list_head arp_list={&arp_list, &arp_list};

static MEM_POOL arp_entry_pool;
static char arp_entry_static_buf[MEM_NODE_SIZE(sizeof(struct arpentry))*MAX_ARP_ENTRY_COUNT+4];

struct arp_wait_s
{
	u32 nexthop;
	struct sk_buff skb;
	u8  buf[1518];
	INET_DEV_T *dev;
	u16 proto;
};

static struct arp_wait_s arp_wait_pkt;
static struct inet_softirq_s arp_timer_softirq;

static void arp_wait_pkt_init(void)
{
	struct sk_buff *skb;
	memset(&arp_wait_pkt, 0, sizeof(arp_wait_pkt));
	skb = &arp_wait_pkt.skb;
	skb->head =	arp_wait_pkt.buf;
	skb->end = arp_wait_pkt.buf+sizeof(arp_wait_pkt.buf);
	skb->data = skb->tail = skb->head;
}

static struct arpentry *arp_create(void)
{
	return (struct arpentry *)mem_malloc(&arp_entry_pool);
}

static void arp_destroy(struct arpentry *entry)
{
	mem_free(&arp_entry_pool, entry);
}

static void arp_timer_fun(unsigned long softirq)
{
	struct arpentry *entry;
	struct list_head *p;
	
	list_for_each(p, &arp_list)
	{
		entry = list_entry(p, struct arpentry, list);
		if(entry->status==ARP_STATIC)
			continue;
		if(entry->timestamp<inet_jiffier)
		{
			switch(entry->status)
			{
			case ARP_TIMEOUT:
				if(entry->retry>0)
				{
					entry->retry--;
					entry->timestamp = inet_jiffier+1*1000;
				}else
				{
					entry->status = ARP_DEAD;
					entry->timestamp = inet_jiffier+5*1000;/*5 sec */
				}
				/* send arp request */
				arp_send(ARPOP_REQUEST, 
					entry->dev->mac, entry->dev->addr,
					bc_mac, entry->ip, entry->dev);
				break;
			case ARP_OK:
				entry->status = ARP_TIMEOUT;
				entry->timestamp = inet_jiffier+1*1000;
				entry->retry = 10;
				/* send arp request */
				arp_send(ARPOP_REQUEST, 
					entry->dev->mac, entry->dev->addr,
					bc_mac, entry->ip, entry->dev);
				IPSTACK_DEBUG(ARP_DEBUG, "arp request %s\n", inet_ntoa(htonl(entry->ip)));
				break;
			default:
				p = p->prev;
				list_del(&entry->list);
				arp_destroy(entry);
				break;
			}
		}
		
	}
}

int arp_init(void)
{
	INIT_LIST_HEAD(&arp_list);
	mem_pool_init(&arp_entry_pool, sizeof(struct arpentry),
		MAX_ARP_ENTRY_COUNT,
		arp_entry_static_buf,
		sizeof(arp_entry_static_buf),
		"arp_entry"
		);
	arp_timer_softirq.mask = INET_SOFTIRQ_TIMER;
	arp_timer_softirq.h = arp_timer_fun;
	inet_softirq_register(&arp_timer_softirq);
	return 0;
}

void arp_table_show(void)
{
	struct arpentry *entry;
	struct list_head *p;
	u32 addr;
	u8  *str;
	char *status;
	s80_printf("\n----------arp table----------------\n");
	s80_printf("Now TimeStamp=%d\n", inet_jiffier);
	list_for_each(p, &arp_list)
	{	
		entry = list_entry(p, struct arpentry, list);
		addr = htonl(entry->ip);
		str = (u8*)&addr;
		s80_printf("%d.%d.%d.%03d ",
				str[0], str[1], str[2], str[3]
				);
		str = entry->mac;
		s80_printf("%02x:%02x:%02x:%02x:%02x:%02x ",
			str[0], str[1], str[2],str[3],str[4],str[5]
		);
		switch(entry->status)
		{
		case ARP_STATIC:
			status = "STATIC ";
			break;
		case ARP_OK:
			status = "OK     ";
			break;
		case ARP_TIMEOUT:
			status = "TIMEOUT";
			break;
		case ARP_DEAD:
			status = "DEAD   ";
			break;
		default:
			break;
		}
		s80_printf("%s T=%d\n", status, entry->timestamp);
	}
	s80_printf("\n");
}

int arp_wait(struct sk_buff *skb, u32 nexthop, u16 proto, INET_DEV_T *out)
{
	static int test =0;
	struct sk_buff *t_skb = &arp_wait_pkt.skb;
	char *data;
	arp_wait_pkt_init();
	skb_reserve(t_skb, out->header_len);
	data = (void*)__skb_put(t_skb, skb->len);
	INET_MEMCPY(data, skb->data, skb->len);
	arp_wait_pkt.dev = out;
	arp_wait_pkt.nexthop = nexthop;
	arp_wait_pkt.proto = proto;
	arp_send(ARPOP_REQUEST, out->mac, out->addr,
				bc_mac, nexthop, out);
	return 0;
}


struct arpentry * arp_lookup(INET_DEV_T *dev, u32 dip)
{
	struct list_head *p;
	struct arpentry *entry;

	list_for_each(p, &arp_list)
	{
		entry = list_entry(p, struct arpentry, list);
		if(entry->dev == dev && entry->ip == dip)
			return entry;
	}
	return NULL;
}

int arp_time(void)
{
	return inet_jiffier;
}

void arp_fresh_nocreate(INET_DEV_T *dev, u32 dip, u8 *dmac)
{
	struct arpentry *entry = arp_lookup(dev, dip);

	if(!entry)
	{
		return;
	}
	if(entry->status==ARP_STATIC)
		return;	
	INET_MEMCPY(entry->mac, dmac, ETH_ALEN);
	entry->status = ARP_OK;
	entry->timestamp = arp_time()+MAX_ARP_TIME;
}

void arp_fresh(INET_DEV_T *dev, u32 dip, u8 *dmac)
{
	struct arpentry *entry = arp_lookup(dev, dip);

	if(!entry)
	{
		entry = arp_create();
		if(!entry)
			return;
		memset(entry, 0, sizeof(struct arpentry));
		list_add(&entry->list, &arp_list);
		entry->status = ARP_OK;
		entry->ip = dip;
		entry->dev = dev;
	}
	if(entry->status==ARP_STATIC)
		return;	
	INET_MEMCPY(entry->mac, dmac, ETH_ALEN);
	entry->status = ARP_OK;
	entry->timestamp = arp_time()+MAX_ARP_TIME;
}

err_t arp_input(struct sk_buff *skb, INET_DEV_T *dev)
{
	struct arphdr *arph = skb->nh.arph;
	u16 op;
	g_arp_stats.recv_pkt++;
	if(__skb_pull(skb, sizeof(struct arphdr))==NULL)
	{
		skb_free(skb);
		g_arp_stats.recv_err_pkt++;
		return NET_ERR_BUF;
	}
	if(arph->ar_hrd != htons(ARPHRD_ETHER)||
		arph->ar_pro != htons(ETH_P_IP)||
		arph->ar_hln != ETH_ALEN||
		arph->ar_pln != 4)
	{
		skb_free(skb);
		g_arp_stats.recv_err_pkt++;
		return NET_ERR_BUF;
	}
	op = ntohs(arph->ar_op);
	arph->dip = ntohl(arph->dip);
	arph->sip = ntohl(arph->sip);
	switch(op)
	{
	case ARPOP_REQUEST:
		if(!ip_addr_cmp(arph->dip, dev->addr))
			goto drop;
		arp_send(ARPOP_REPLY, dev->mac, dev->addr, 
			arph->smac, arph->sip, dev);
		break;
	case ARPOP_REPLY:
		if(arph->sip == dev->addr)
		{
			//ip address collision
			break;
		}
		IPSTACK_DEBUG(ARP_DEBUG, "wait pkt %08x, arp ip %08x\n",
				arp_wait_pkt.nexthop, arph->sip);
		/*
		在美国一个客户使用中的网络环境会
		发出ARP异常报文:
		该报文为ARP应答, SIP=0;
		从而导致上次cache的报文重传(dst mac=全ff,广播地址);
		另外,因为增加arp entry 会导致arp会定时请求(ip=0的mac地址);
		解决办法:增加arp_wait_pkt.nexthop !=0 的判断;		
		*/
		if(arp_wait_pkt.nexthop!=0 && arp_wait_pkt.nexthop == arph->sip)
		{
			INET_DEV_T *out = arp_wait_pkt.dev;
			s80_printf("ARP Add %08x, %02x:%02x:%02x:%02x:%02x:%02x\n",
				arph->sip, arph->smac[0], arph->smac[1], arph->smac[2],
				arph->smac[3], arph->smac[4], arph->smac[5]
				);
			arp_fresh(dev, arph->sip, arph->smac);
			out->dev_ops->xmit(out, &arp_wait_pkt.skb,
					arp_wait_pkt.nexthop,
					arp_wait_pkt.proto
					);	
			arp_wait_pkt.nexthop = 0;
		}else
		{
			arp_fresh_nocreate(dev, arph->sip, arph->smac);
		}
		break;
	}
	skb_free(skb);
	return NET_OK;
drop:
	skb_free(skb);
	g_arp_stats.drop++;
	return NET_OK;
}

err_t arp_send(u16 op, u8 *smac, u32 sip, u8 *dmac, u32 dip, INET_DEV_T *out)
{
	int size = sizeof(struct arphdr)+out->header_len;
	struct sk_buff *skb = skb_new(size);
	struct arphdr *arph;
	err_t err;
	if(skb==NULL)
	{
		return NET_ERR_MEM;
	}
	g_arp_stats.snd_pkt++;
	skb_reserve(skb, size);
	skb->nh.raw = __skb_push(skb, sizeof(struct arphdr));
	arph = skb->nh.arph;
	arph->ar_hrd = htons(ARPHRD_ETHER);
	arph->ar_pro = htons(ETH_P_IP);
	arph->ar_hln = ETH_ALEN;
	arph->ar_pln = 4;
	arph->ar_op = htons(op);
	INET_MEMCPY(arph->smac, smac, ETH_ALEN);
	arph->sip = htonl(sip);
	INET_MEMCPY(arph->dmac, dmac, ETH_ALEN);
	arph->dip = htonl(dip);
	eth_build_header(skb, dmac, smac, ETH_P_ARP);
	err = out->dev_ops->xmit(out, skb, dip, ETH_P_ARP);
	skb_free(skb);
	return err;
}

int arp_entry_add(u32 ip, u8 *mac, u8 flag)
{
	struct arpentry *entry;
	INET_DEV_T   *dev;
	struct route *rt = ip_route(ip, NULL);
	if(!rt)
		return NET_ERR_RTE;
	dev = rt->out;
	arp_fresh(dev, ip, mac);
	entry = arp_lookup(dev, ip);
	if(!entry)
	{
		return NET_ERR_MEM;		
	}
	if(flag&ARP_STATIC)
		entry->status = ARP_STATIC;
	return 0;
}
int arp_entry_delete(INET_DEV_T *dev)
{
	struct list_head *p;
	struct arpentry *entry;
	if(!dev)
		return NET_ERR_ARG;
	inet_softirq_disable();
	list_for_each(p, &arp_list)
	{
			entry = list_entry(p, struct arpentry, list);
			if(entry->dev == dev)
			{
				p = p->prev;
				list_del(&entry->list);
				arp_destroy(entry);
			}				
	}
	inet_softirq_enable();
	return NET_OK;
}

int NetAddStaticArp_std(char *ip_str, unsigned char mac[6])
{
	u32 ip;
	int ret,cnt, found;
	struct arpentry *entry;
	struct list_head *p;
	
	if(ip_str==NULL)
		return NET_ERR_VAL;
	ip = ntohl(inet_addr(ip_str));
	if(ip==0||ip==(u32)-1)
		return NET_ERR_VAL;
	cnt=0;
	found=0;
	inet_softirq_disable();
	/*
	lookup arp table
	*/
	list_for_each(p, &arp_list)
	{
		entry = list_entry(p, struct arpentry, list);
		if(entry->ip==ip)
		{
			found=1;//found it!!!!!
			break;
		}
		if(entry->status==ARP_STATIC)
		{
			cnt++;
		}
	}
	if(found)
	{
		INET_MEMCPY(entry->mac, mac, ETH_ALEN);
		entry->status = ARP_STATIC;
		ret = NET_OK;
	}else if(cnt<4)
	{
		/* 静态ARP表不能超过4个*/
		ret = arp_entry_add(ip, mac, ARP_STATIC);
	}else
	{
		ret = NET_ERR_MEM;
	}
	inet_softirq_enable();
	return ret;
}

int EthGetFirstRouteMac_std(const char *dest_ip, unsigned char *mac, int len)
{
     int ret=0;
     struct arpentry *entry;
     INET_DEV_T   *dev;
     struct route *rt;
     u32 addr;
     unsigned char mac_tmp[ETH_ALEN];

     if (!dest_ip) return NET_ERR_STR;
     if (len <= 0) return NET_ERR_VAL;     
     if (!mac) mac = mac_tmp;
     
     if(str_check_max(dest_ip, MAX_IPV4_ADDR_STR_LEN)!=NET_OK) return NET_ERR_STR;
     addr = ntohl(inet_addr(dest_ip));
     if(addr == 0 || addr==(u32)-1) return NET_ERR_VAL;
     inet_softirq_disable();
     rt = ip_route(addr, NULL);
     if(!rt) {
        ret = NET_ERR_RTE;
        goto __END;
     }
     dev = rt->out;
     entry = arp_lookup(dev, addr);
     if (dev->next_hop!=0 && dev->next_hop!=(u32)-1)
        entry = arp_lookup(dev, dev->next_hop);
     if (!entry) {
        NetPing(dest_ip, 5000, 0);
        entry = arp_lookup(dev, addr);
        if (!entry) {
            if (dev->next_hop!=0 && dev->next_hop!=(u32)-1)
                     entry = arp_lookup(dev, dev->next_hop);
        }
     }
     if (!entry) {
        ret = NET_ERR_RTE;
        goto __END;
     }
     INET_MEMCPY(mac, entry->mac, ((len<ETH_ALEN)?len:ETH_ALEN));
     inet_softirq_enable();
     return 0;
__END:    
     inet_softirq_enable();
     return ret;
}

