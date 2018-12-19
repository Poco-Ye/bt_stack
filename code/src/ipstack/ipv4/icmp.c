/*
修改历史
080624 sunJH
NetPing增加字符串长度检查
081104 sunJH
NetPing修改如下
1. 增加对timeout和size参数合法性判断
2. 对timeout减要求判断是否足够
081106 sunJH
IcmpReply提供判断是否有icmp回复
090514 sunJH
增加ping echo reply开关功能(icmp_flag),
接口为NetSetIcmp
*/
#include <inet/inet.h>
#include <inet/inet_softirq.h>
#include <inet/ip.h>
#include <inet/icmp.h>

static u16 ping_id = 1;
static u16 ping_seq = 1;
static volatile int ping_reply = 0;
static u32 icmp_flag=
#ifdef PTS
0x0;
#else
0x1;
#endif

struct net_stats g_icmp_stats={0,};
#define PING_DEBUG //s80_printf
#define PING_ECHO_ENABLE() (icmp_flag&0x1)
int NetPing_std(char* dst_ip_str, long timeout, int size)
{
	int pkt_size;
	int err;
	struct sk_buff *skb;
	struct route *rt; 
	struct icmphdr *icmph;
	u32 total = timeout;
	u32  dst_ip;
	
	if(timeout<0||size<0)
		return NET_ERR_VAL;

	if(str_check_max(dst_ip_str, MAX_IPV4_ADDR_STR_LEN)!=NET_OK)
		return NET_ERR_STR;
	dst_ip = ntohl(inet_addr(dst_ip_str));

	if(dst_ip == (u32)-1)
	{
		return NET_ERR_ARG;
	}
	rt=ip_route(dst_ip, NULL);

	if(!rt)
	{
		return NET_ERR_RTE;
	}
	pkt_size = rt->out->header_len+sizeof(struct iphdr)+
			sizeof(struct icmphdr)+size;
	if(pkt_size > rt->out->mtu)
	{
		return NET_ERR_VAL;
	}
	PING_DEBUG("NetPing skb_new\n");
	skb = skb_new(pkt_size);
	if(!skb)
	{
		return NET_ERR_MEM;
	}
	PING_DEBUG("NetPing skb_reserv\n");
	skb_reserve(skb, pkt_size);
	skb->h.raw = __skb_push(skb, sizeof(struct icmphdr)+size);
	PING_DEBUG("NetPing icmph\n");
	icmph = skb->h.icmph;
	icmph->type = ICMP_ECHO;
	icmph->code = 0;
	icmph->un.echo.id = ping_id;
	icmph->un.echo.sequence = ++ping_seq;
	icmph->checksum = 0;
	icmph->checksum = inet_chksum(icmph, sizeof(struct icmphdr)+size);

	ping_reply = 0;

	PING_DEBUG("NetPing inet_softirq_disable\n");
	inet_softirq_disable();/* enter half-bottom */
	while(timeout)
	{
		if(!DEV_FLAGS(rt->out, FLAGS_TX))
		{
			inet_softirq_enable();
			if(timeout > INET_TIMER_SCALE)
				timeout -= INET_TIMER_SCALE;
			else if(timeout>0)
				timeout = 0;
			inet_delay(INET_TIMER_SCALE);
			inet_softirq_disable();			
		}else
		{
			break;
		}
	}
	PING_DEBUG("timeout ms %d\n", timeout);
	err = NET_OK;
	//if(timeout)
	{
		g_icmp_stats.snd_pkt++;
		err = ip_output(skb, rt->out->addr, dst_ip,
			64, 0, IP_PROTO_ICMP, rt);
		skb_free(skb);
	}
	inet_softirq_enable();/* leave half-bottom */
	
	if(err != NET_OK)
	{
		PING_DEBUG("Error %d\n", err);
		return err;
	}
	if(timeout<=0)
	{
		return NET_ERR_TIMEOUT;
	}
	PING_DEBUG("icmp wait\n");
	while(!ping_reply&&timeout)
	{
		if(timeout>100)
			timeout-=100;
		else
			timeout = 0;
		inet_delay(100);
	}
	PING_DEBUG("icmp tm %d\n", timeout);
	if(timeout <= 0)
		return NET_ERR_TIMEOUT;
	return total-timeout;
}

void icmp_input(struct sk_buff *skb)
{
	struct icmphdr *icmph = skb->h.icmph;
	struct iphdr *iph = skb->nh.iph;

	g_icmp_stats.recv_pkt++;

	if(icmph->type == ICMP_ECHOREPLY&&icmph->code == 0)
	{
		if(icmph->un.echo.id == ping_id &&
			icmph->un.echo.sequence == ping_seq)
		{
			ping_reply = 1;
		}
	}
	else if(PING_ECHO_ENABLE()&&
		icmph->type == ICMP_ECHO&&icmph->code == 0)
	{
		//icmp reply, reused the packet NetRecv
		icmph->type = ICMP_ECHOREPLY;
		icmph->checksum = 0;
		icmph->checksum = inet_chksum(icmph, skb->len);
		g_icmp_stats.snd_pkt++;
		ip_output(skb, iph->daddr, iph->saddr, 
				64, 0, IP_PROTO_ICMP, NULL);
	}
	skb_free(skb);
}

int IcmpReply(void)
{
	return ping_reply;
}

void NetSetIcmp(unsigned long flag)
{
	flag &= 0x1;
	icmp_flag = flag;
}

