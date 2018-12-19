/*
增加PPPoE处理入口
090928 sunJH
增加zero_mac定义
*/
#include <inet/inet.h>
#include <inet/dev.h>
#include <inet/skbuff.h>
#include <inet/ethernet.h>
#include <inet/ip.h>
#include <inet/arp.h>
u8 bc_mac[6]={0xff,0xff,0xff,0xff,0xff,0xff}; 
u8 zero_mac[6]={0x00,0x00,0x00,0x00,0x00,0x00};

#define ETH_DEBUG //s80_printf

err_t eth_build_header(struct sk_buff *skb, u8 *h_dest, u8 *h_src, u16 proto)
{
	struct ethhdr *eth;

	skb->mac.raw = __skb_push(skb, sizeof(struct ethhdr));
	if(skb->mac.raw == NULL)
		return NET_ERR_BUF;
	eth = (struct ethhdr *)skb->mac.raw;
	INET_MEMCPY(eth->h_dest, h_dest, ETH_ALEN);
	INET_MEMCPY(eth->h_source, h_src, ETH_ALEN);
	eth->h_proto = htons(proto);
	return NET_OK;
}
err_t eth_input(struct sk_buff *skb, INET_DEV_T *dev)
{
	struct ethhdr *eth;
	u16 proto;

	ETH_DEBUG("eth_input \n");

	skb->mac.raw = skb->data;
	skb->nh.raw = __skb_pull(skb, sizeof(struct ethhdr));
	if(skb->nh.raw == NULL)
	{
		skb_free(skb);
		ETH_DEBUG("bad ethhdr\n");
		return NET_ERR_BUF;
	}

	eth = (struct ethhdr*)skb->mac.raw;
	ETH_DEBUG("dmac %02x:%02x:%02x:%02x:%02x:%02x\n",
		eth->h_dest[0],
		eth->h_dest[1],
		eth->h_dest[2],
		eth->h_dest[3],
		eth->h_dest[4],
		eth->h_dest[5]);
	ETH_DEBUG("dev %02x:%02x:%02x:%02x:%02x:%02x\n",
		dev->mac[0],
		dev->mac[1],
		dev->mac[2],
		dev->mac[3],
		dev->mac[4],
		dev->mac[5]);
		
	if(memcmp(eth->h_dest, dev->mac, ETH_ALEN)==0||
		memcmp(eth->h_dest, bc_mac, ETH_ALEN)==0)
	{
		proto = ntohs(eth->h_proto);
		ETH_DEBUG("protocol %04x\n", proto);
		switch(proto)
		{
		case ETH_P_IP:
			return ip_input(skb, dev);
		case ETH_P_ARP:
			return arp_input(skb, dev);
		case ETH_P_PPP_DISC:
		case ETH_P_PPP_SES:
			return pppoe_input(skb, dev, proto);
		default:
			break;
		}
	}
	skb_free(skb);
	return 0;
}

