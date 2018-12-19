/*
修改历史
080722 sunJH
ip_input增加判断:
源地址为网络口的IP Address则认为回显
*/
#include <inet/inet.h>
#include <inet/inet_list.h>
#include <inet/skbuff.h>
#include <inet/dev.h>
#include <inet/ip.h>
#include <inet/tcp.h>
#include <inet/icmp.h>
#include <inet/udp.h>
#include <inet/ethernet.h>

struct net_stats g_ip_stats={0,};

/*
 * Initializes the IP layer.
 */
void ip_init(void)
{
#if IP_FRAG
	ip_frag_init();
#endif
}


/*
 * This function is called by the network interface device driver when
 * an IP packet is received. The function does the basic checks of the
 * IP header such as packet size being at least larger than the header
 * size etc. If the packet was not destined for us, the packet is
 * forwarded (using ip_forward). The IP checksum is always checked.
 *
 * Finally, the packet is sent to the upper layer protocol input function.
 * 
 */

err_t ip_input(struct sk_buff *skb, INET_DEV_T *in) 
{
	struct iphdr *iph;
	int iphdrlen;
	u16 tot_len;

	skb->dev = in;
	g_ip_stats.recv_pkt++;
	iph = skb->nh.iph;

	if(skb->len<20||(skb->h.raw=__skb_pull(skb, iph->ihl*4))==NULL)
	{
		//bad packet;
		skb_free(skb);
		g_ip_stats.recv_err_pkt++;
		return NET_ERR_BUF;
	}
	if(iph->version != 4)
	{
		skb_free(skb);
		g_ip_stats.recv_err_pkt++;
		return NET_ERR_BUF;
	}
	iphdrlen = iph->ihl*4;
	tot_len = ntohs(iph->tot_len);
	if(skb->len < (tot_len-iphdrlen))
	{
		skb_free(skb);
		g_ip_stats.recv_err_pkt++;
		return NET_ERR_BUF;
	}
	__skb_trim(skb, tot_len-iphdrlen);

	#if IPSTACK_IN_CHECK==1
	if (inet_chksum(iph, iphdrlen) != 0) 
	{
		skb_free(skb);
		g_ip_stats.recv_err_check++;
		return NET_ERR_BUF;
	}
	#endif

	iph->tot_len = tot_len;
	iph->saddr = ntohl(iph->saddr);
	iph->daddr = ntohl(iph->daddr);

	if(iph->saddr == in->addr)
	{
		in->echo_count++;
	}

	/*
	** 对于dhcp(udp=67), dhcp_offer dhcp_ack的dest不
	** 一定是广播地址,有可能是unicast addr,
	** 但是dhcp这时候还未完成(ip addr=0);
	** 新的做法 :    对于udp packet不检查dest ip ;
	**/
	if(iph->protocol!=IP_PROTO_UDP&&
		ip_local(iph->daddr)==0)
	{
		//forward????
		skb_free(skb);
		g_ip_stats.drop++;
		return NET_OK;
	}

	if(ip_policy(skb, in, IP_IN)==IP_DROP)
	{
		IPSTACK_DEBUG(IP_DEBUG, "IP DROP IN PACKET\n");
		skb_free(skb);
		g_ip_stats.drop++;
		return NET_OK;
	}

	if((iph->frag_off&htons(IP_OFFMASK | IP_MF))!=0)
	{
		//iph->frag_off = ntohs(iph->frag_off);
		skb_free(skb);
		g_ip_stats.drop++;
		return NET_OK;
	}

	switch(iph->protocol)
	{
	case IP_PROTO_TCP:
		#if 0
		s80_printf("----------------- TCP input-----------\n");
		mem_debug_show();
		tcp_debug_print_pcbs();
		#endif
		tcp_input(skb);
		#if 0
		mem_debug_show();
		tcp_debug_print_pcbs();
		#endif
		break;
	case IP_PROTO_ICMP:
		icmp_input(skb);
		break;
	case IP_PROTO_UDP:
		udp_input(skb);
		break;
	default:
		skb_free(skb);
		g_ip_stats.drop++;
		break;
	}
	return NET_OK;
}

int ip_policy(struct sk_buff *skb, INET_DEV_T *out, int direct)
{
	static int in_count = 0, out_count = 0;
	static int ip_policy_out_tb[]=
	{
		IP_DROP,
		IP_DROP,
		IP_PASS,
		IP_PASS,
	};
	static int ip_policy_in_tb[]=
	{
		IP_PASS,
	};
	int ip_policy_in_tb_size = sizeof(ip_policy_in_tb)/4;
	int ip_policy_out_tb_size = sizeof(ip_policy_out_tb)/4;
	return IP_PASS;
	switch(direct)
	{
	case IP_IN:
		if(in_count>=ip_policy_in_tb_size)
			return IP_PASS;
		return ip_policy_in_tb[in_count++];
		break;
	case IP_OUT:
		if(out_count>=ip_policy_out_tb_size)
			return IP_PASS;		
		return ip_policy_out_tb[out_count++];
		break;
	default:
		assert(0);
	}
	return IP_PASS;
}

static int ip_id = 1;

void ip_build_header(struct iphdr *iph, u32 src, u32 dest,
				u8 ttl, u8 tos, u8 proto, u16 total_len)
{
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = tos;
	iph->tot_len = htons(total_len);
	iph->id = ip_id++;
	iph->frag_off = htons(0x4000);//dont frag
	iph->ttl = ttl;
	iph->protocol = proto;
	iph->check = 0;
	iph->saddr = htonl(src);
	iph->daddr = htonl(dest);

	iph->check = inet_chksum(iph, 20);
}
err_t ip_output(struct sk_buff *skb, u32 src, u32 dest,
				u8 ttl, u8 tos, u8 proto, struct route *rt)

{
	int ret;
	struct iphdr *iph;
	INET_DEV_T *out;

	IPSTACK_DEBUG(IP_DEBUG, "ip_output ...........\n");
	IPSTACK_DEBUG(IP_DEBUG, "src=%08x, dest=%08x\n", src, dest);

	if(rt)
	{
		if(!DEV_FLAGS(rt->out,FLAGS_UP))
			return NET_ERR_IF;
		
	}else
	{
		rt = ip_route(dest, NULL);
		if(!rt)
			return NET_ERR_RTE;
	}
	out = rt->out;
	
	skb->nh.raw = __skb_push(skb, sizeof(struct iphdr));
	iph = skb->nh.iph;
	ip_build_header(iph, src, dest, ttl, tos, proto, (u16)skb->len);
	assert(iph);

	if(ip_policy(skb, out,IP_OUT)==IP_DROP)
	{
		IPSTACK_DEBUG(IP_DEBUG, "IP DROP OUT PACKET\n");
		g_ip_stats.drop++;
		ret = 0;
	}else
	{
		IPSTACK_DEBUG(IP_DEBUG, "dev xmit\n");
		g_ip_stats.snd_pkt++;
		ret = out->dev_ops->xmit(out, skb, rt->next_hop, ETH_P_IP);
	}
	__skb_pull(skb, sizeof(struct iphdr));

	IPSTACK_DEBUG(IP_DEBUG, "ip_output end\n");
	return ret;
}


#if IP_DEBUG
void
ip_debug_print(struct sk_buff *p)
{
  struct iphdr *iph = p->nh.iph;
  u8 *payload;

  payload = (u8 *)iph + 20;

  IPSTACK_DEBUG(IP_DEBUG, "IP header:\n");
  IPSTACK_DEBUG(IP_DEBUG, "+-------------------------------+\n");
  IPSTACK_DEBUG(IP_DEBUG, "|%2"S16_F" |%2"S16_F" |  0x%02X |     %5"U16_F"     | (v, hl, tos, len)\n",
                    iph->version,
                    iph->ihl,
                    iph->tos,
                    ntohs(iph->tot_len));
  IPSTACK_DEBUG(IP_DEBUG, "+-------------------------------+\n");
  IPSTACK_DEBUG(IP_DEBUG, "|    %5"U16_F"      |%"U16_F"%"U16_F"%"U16_F"|    %4"U16_F"   | (id, flags, offset)\n",
                    ntohs(iph->id),
                    ntohs(iph->frag_off) >> 15 & 1,
                    ntohs(iph->frag_off) >> 14 & 1,
                    ntohs(iph->frag_off) >> 13 & 1,
                    ntohs(iph->frag_off) & IP_OFFMASK);
  IPSTACK_DEBUG(IP_DEBUG, "+-------------------------------+\n");
  IPSTACK_DEBUG(IP_DEBUG, "|  %3"U16_F"  |  %3"U16_F"  |    0x%04X     | (ttl, proto, chksum)\n",
                    iph->ttl,
                    iph->protocol,
                    ntohs(iph->check));
  IPSTACK_DEBUG(IP_DEBUG, "+-------------------------------+\n");
  IPSTACK_DEBUG(IP_DEBUG, "|  %3"U16_F"  |  %3"U16_F"  |  %3"U16_F"  |  %3"U16_F"  | (src)\n",
                    ip4_addr1(iph->saddr),
                    ip4_addr2(iph->saddr),
                    ip4_addr3(iph->saddr),
                    ip4_addr4(iph->saddr));
  IPSTACK_DEBUG(IP_DEBUG, "+-------------------------------+\n");
  IPSTACK_DEBUG(IP_DEBUG, "|  %3"U16_F"  |  %3"U16_F"  |  %3"U16_F"  |  %3"U16_F"  | (dest)\n",
                    ip4_addr1(iph->daddr),
                    ip4_addr2(iph->daddr),
                    ip4_addr3(iph->daddr),
                    ip4_addr4(iph->daddr));
  IPSTACK_DEBUG(IP_DEBUG, "+-------------------------------+\n");
}
#endif /* IP_DEBUG */



