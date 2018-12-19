#include <inet/inet.h>
#include <inet/inet_list.h>
#include <inet/skbuff.h>
#include <inet/dev.h>
#include <inet/ip.h>
#include <inet/udp.h>
#include <inet/mem_pool.h>

static MEM_POOL udp_pcb_pool;
static char udp_pcb_static_buf[MEM_NODE_SIZE(sizeof(struct udp_pcb))*MAX_UDP_COUNT+4];
struct udp_pcb *udp_active_list=NULL;
static struct list_head udp_filter_list;
struct net_stats g_udp_stats={0,};

static int udp_filter(struct sk_buff *skb, struct udphdr *udph)
{
	struct list_head *p;
	UDP_FILTER_T *filter;
	int flag = 0;

	list_for_each(p, &udp_filter_list)
	{
		filter = list_entry(p, UDP_FILTER_T, list);
		if((filter->source==0||filter->source==udph->source)&&
			(filter->dest==0||filter->dest==udph->dest))
		{
			filter->fun(filter->arg, skb);
			flag = 1;
		}
	}
    if(flag){
        skb_free(skb);
        return 1;
    }
	return 0;
}

static int udp_new_port(struct udp_pcb *new_pcb)
{
	static u16 port_base=1024;
	struct udp_pcb *pcb;
	u16 port;

	port = port_base;
again:
	pcb=udp_active_list;
	while(pcb)
	{
		if(pcb->local_port == port)
			break;
		pcb = pcb->next;
	}
	if(pcb)
	{
		port++;
		goto again;
	}
	port_base = port;
	new_pcb->local_port = port;
	return NET_OK;
}

void udp_init(void)
{
	INIT_LIST_HEAD(&udp_filter_list);
	mem_pool_init(&udp_pcb_pool, sizeof(struct udp_pcb),
		MAX_UDP_COUNT, 
		udp_pcb_static_buf,
		sizeof(udp_pcb_static_buf),
		"udp_pcb"
		);
}

int udp_recv_data(struct udp_pcb *pcb, u8 *buf, int size, u32 *p_remote_ip, u16 *p_remote_port)
{
	int true_size, total_size;
	struct sk_buff *skb;
	struct iphdr *iph;
	struct udphdr *uh;
	total_size = 0;
	if((skb=skb_peek(&pcb->incoming)))
	{
		iph = skb->nh.iph;
		uh = skb->h.uh;
		true_size = (skb->len>size)?size:skb->len;
		if(true_size>0)
			INET_MEMCPY(buf, skb->data, true_size);
		total_size += true_size;
		if(p_remote_ip)
			*p_remote_ip = iph->saddr;
		if(p_remote_port)
			*p_remote_port = uh->source;
		if(true_size == skb->len)
		{
			__skb_unlink(skb, &pcb->incoming);
			skb_free(skb);
		}else
		{
			__skb_pull(skb, true_size);
		}
	}
	return total_size;
}

void udp_input(struct sk_buff *skb)
{
	struct iphdr *iph=skb->nh.iph;
	struct udphdr *udph=skb->h.uh;
	struct udp_pcb *pcb = udp_active_list;

	NET_STATS_RCV(&g_udp_stats);

	if(skb->len != ntohs(udph->len))
	{
		NET_STATS_DROP(&g_udp_stats);
		goto drop;
	}
	#if IPSTACK_IN_CHECK==1
	if(udph->check != 0x0000){
		if (inet_chksum_pseudo(udph, skb->len,
				iph->saddr,
				iph->daddr,
				iph->protocol) != 0) 
		{
			skb_free(skb);
			NET_STATS_DROP(&g_udp_stats);
			return;
		}
	}
	#endif
	__skb_pull(skb, sizeof(struct udphdr));
	udph->dest = ntohs(udph->dest);
	udph->source = ntohs(udph->source);
	udph->len = ntohs(udph->len);

	if(udp_filter(skb, udph)==1)
	{
		return;
	}

	while(pcb)
	{
		if((pcb->local_port==udph->dest)&&
			(pcb->local_ip==0||pcb->local_ip==iph->daddr))
		{
			break;
		}
		pcb = pcb->next;
	}
	if(!pcb)
	{
		NET_STATS_DROP(&g_udp_stats);
		goto drop;
	}
	if(pcb->recv)
	{
		pcb->recv(pcb->recv_arg, pcb, skb, iph->saddr, udph->source);
	}else if(pcb->incoming.qlen < 2)
	{
		__skb_queue_tail(&pcb->incoming, skb);
		return ;
	}

drop:
	skb_free(skb);
}

struct udp_pcb *udp_new(void)
{
	struct udp_pcb *pcb = (struct udp_pcb*)mem_malloc(&udp_pcb_pool);
	if(!pcb)
		return NULL;
	skb_queue_head_init(&pcb->incoming);
	/* insert active list */	
	if(udp_active_list)
	{
		struct udp_pcb *prev = udp_active_list;
		while(prev->next)
		{
			prev = prev->next;
		}
		prev->next = pcb;
		pcb->next = NULL;
	}else
	{
		udp_active_list = pcb;
	}
	return pcb;
}

void udp_free(struct udp_pcb *pcb)
{
	struct udp_pcb *prev = udp_active_list;

	/* remove from active list */
	if(udp_active_list == pcb)
	{
		udp_active_list = pcb->next;
	}else if(prev)
	{
		while(prev->next)
		{
			if(prev->next == pcb)
				break;
			prev = prev->next;
		}
		if(prev->next == pcb)
		{
			prev->next = pcb->next;
		}
	}else
	{
		assert(0);
	}
	__skb_queue_purge(&pcb->incoming);
	mem_free(&udp_pcb_pool, pcb);
}

int udp_bind(struct udp_pcb *pcb, u32 addr, u16 port)
{
	struct udp_pcb *p;
	
	p = udp_active_list;
	while(p)
	{
		if(p->local_port == port)
			return NET_ERR_USE;
		p = p->next;
	}

	if(pcb->local_port != 0)
		return NET_ERR_VAL;
	
	if(addr)
		pcb->local_ip = addr;
	if(port)
		pcb->local_port = port;
	return NET_OK;
}

int udp_output(struct udp_pcb *pcb, void *data, int size, u32 dst, u16 dport)
{
	struct sk_buff *skb;
	struct route *rt = ip_route(dst, pcb->out_dev);
	INET_DEV_T *dev;
	char *buf;
	struct udphdr *uh;
	int len, total;
	
	if(!rt)
	{
		return NET_ERR_RTE;
	}
	dev = rt->out;
	if(!DEV_FLAGS(dev, FLAGS_UP))
		return NET_ERR_IF;
	if(ip_addr_isany(pcb->local_ip))
	{
		pcb->local_ip = dev->addr;
	}
	if(pcb->local_port == 0)
	{
		udp_new_port(pcb);
	}
	total = 0;
	buf = (char*)data;
	while(size>0)
	{
		len=(size>1024)?1024:size;
		if(!DEV_FLAGS(dev, FLAGS_TX|FLAGS_UP))
			break;
		skb = skb_new(dev->header_len+
					sizeof(struct iphdr)+
					sizeof(struct udphdr)+
					len
					);
		if(!skb)
		{
			break;
		}
		skb_reserve(skb,
				dev->header_len+
				sizeof(struct iphdr)+
				sizeof(struct udphdr)+
				len
				);
		__skb_push(skb, len);
		INET_MEMCPY(skb->data, buf, len);
		skb->h.raw = __skb_push(skb, sizeof(struct udphdr));
		uh = skb->h.uh;
		uh->source = htons(pcb->local_port);
		uh->dest = htons(dport);
		uh->len = (u16)skb->len;
		uh->len = htons(uh->len);
		uh->check = 0;
		#if IPSTACK_IN_CHECK==1
		uh->check = inet_chksum_pseudo(uh, skb->len,
				pcb->local_ip,
				dst,
				IP_PROTO_UDP);
		#endif
		NET_STATS_SND(&g_udp_stats);
		ip_output(skb, pcb->local_ip, dst, 64, 0, IP_PROTO_UDP, rt);
		skb_free(skb);
		size -= len;
		buf += len;
		total += len;
	}
	return total;
}

int udp_filter_register(UDP_FILTER_T *new_filter)
{
	struct list_head *p;
	UDP_FILTER_T *filter;

	if(new_filter->fun == NULL)
		return NET_ERR_VAL;

	if(new_filter->in_used==1)
		return NET_OK;

	list_for_each(p, &udp_filter_list)
	{
		filter = list_entry(p, UDP_FILTER_T, list);
	}
	list_add_tail(&new_filter->list, &udp_filter_list);
	new_filter->in_used = 1;
	return NET_OK;
}

void udp_filter_unregister(UDP_FILTER_T *rm_filter)
{
	struct list_head *p;
	UDP_FILTER_T *filter;

	if(rm_filter->in_used == 0)
		return ;

	list_for_each(p, &udp_filter_list)
	{
		filter = list_entry(p, UDP_FILTER_T, list);
		if(filter == rm_filter)
		{
			list_del(&filter->list);
			rm_filter->in_used = 0;
			return ;
		}
	}
	rm_filter->in_used = 0;
}


