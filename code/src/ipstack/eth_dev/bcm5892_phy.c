/**
 ** @ file: bcm5892_eth.c
 ** @ author: NO.773 @ pax.com 
 ** @ brief:  bcm5892 ethernet driver
 ** @ history:
 ** -20120618-: initial
 ** 
 **/

//#define DEBUG
 
#include <string.h>
#include <inet/inet.h>
#include <inet/inet_irq.h>
#include <inet/inet_type.h>
#include <inet/dev.h>
#include <inet/ethernet.h>
#include <inet/ip.h>
#include <inet/arp.h>
#include <inet/inet_softirq.h>

extern int set_mac(u8 *mac_addr);
extern u32 eth_rx(u32 (*f_rx)(u8*, u32));
extern int eth_tx(u8 *data, int len);
extern int is_linkup();
extern int eth_init();
extern void geth_halt(void);

#ifdef DEBUG
	#define __dump printk
#else
	#define __dump
#endif


//static unsigned char sMacAddr[6]={0x00, 0x11, 0x22,0x33, 0x44, 0x55};
static unsigned char sMacAddr[6]={0x00, 0x01, 0x02,0x03, 0x04, 0x05};

static INET_DEV_T bcm_eth_dev;
struct inet_softirq_s  bcm_timer;


static void hex_dump(const u8 *buf, u32 size)
{
	u32 i;
	__dump("-----------------------------------\n");
	for(i=0; i<size; i++)
		__dump("%02x,%s", buf[i], (i+1)%16?"":"\n");
	__dump("\n-----------------------------------\n");
}


static int bcm_set_mac(INET_DEV_T *dev, u8 *mac)
{
	int ret;
	ret = set_mac(mac);
	INET_MEMCPY(dev->mac, mac, 6);
	return ret;
}

static u32 eth_RxPacket(u8 *data, u32 len)
{
	struct sk_buff *skb;
	INET_DEV_T *dev=&bcm_eth_dev;
	
	skb = skb_new(len);
	if(skb == NULL) return;
    memcpy(skb->data,data,len);
	__skb_put(skb, len);
	dev->stats.recv_pkt++;
	eth_input(skb, dev);

	return 0;
}

static int bcm_open(INET_DEV_T *dev, unsigned int flags)
{
	return 0;
}

static int bcm_close(INET_DEV_T *dev)
{
	return 0;
}

static int _bcm_xmit(INET_DEV_T *dev, struct sk_buff *skb, 
					 unsigned long	 nexthop, unsigned short proto)
{
	struct arpentry *entry; 
	int i, err=0;
	
	if(skb == NULL) return NET_OK;
	
	if(proto == ETH_P_IP)
	do{
		if(nexthop == 0 || nexthop == 0xffffffff)
		{
			eth_build_header(skb, bc_mac, dev->mac, proto);
			break;
		}
		entry = arp_lookup(dev, nexthop);
		if(!entry)
		{
			return arp_wait(skb, nexthop, proto, dev);
		}
		eth_build_header(skb, entry->mac, dev->mac, proto);
	}while(0);
		
	err = eth_tx(skb->data, skb->len);
		
	if(proto == ETH_P_IP)
	{
		__skb_pull(skb, sizeof(struct ethhdr));
	}
	return err;
}

static int bcm_xmit(INET_DEV_T *dev, struct sk_buff *skb, 
					unsigned long nexthop, unsigned short proto)
{
	return _bcm_xmit(dev, skb, nexthop, proto);
}

static int bcm_poll(INET_DEV_T *dev, int count)
{
	return 0;
}

static INET_DEV_OPS bcm_eth_ops=
{
	bcm_open,
	bcm_close,
	bcm_xmit,
	bcm_poll,
	NULL,//dmfe_show,
	bcm_set_mac,
};

static void bcm_timer_fun(u32 data)//10ms
{
	static u32 i = 0;
	if (i >= 50){
		i = 0;
		if(is_linkup()){
			bcm_eth_dev.flags |= FLAGS_UP;
			__dump("--%s-- ether linkup!\r\n", __func__);
		} else {
			bcm_eth_dev.flags &= ~FLAGS_UP;
			__dump("--%s-- ether linkdown!\r\n", __func__);
		}
	}
	i++;
	eth_rx(eth_RxPacket);
}

extern int ReadCfgInfo(char * keyword,char * context);

int mac_get(unsigned char mac[6]);

void s_initEth(void)
{
	u32 mii_sr;
	if(!is_eth_module()) return;
	
	eth_init();
	memset(&bcm_eth_dev, 0, sizeof(INET_DEV_T));
	strcpy(bcm_eth_dev.name, "bcm5892");
	bcm_eth_dev.magic_id = DEV_MAGIC_ID;
	bcm_eth_dev.flags |= FLAGS_TX;
	bcm_eth_dev.ifIndex = ETH_IFINDEX_BASE;
	bcm_eth_dev.header_len = sizeof(struct ethhdr);
	INET_MEMCPY(bcm_eth_dev.mac, sMacAddr, 6);
	bcm_eth_dev.mtu = 1518;
	bcm_eth_dev.dev_ops = &bcm_eth_ops;
#if DEBUG
	bcm_set_mac(&bcm_eth_dev, sMacAddr);
	bcm_eth_dev.addr = ntohl(inet_addr(ETH_TEST_IP));
	bcm_eth_dev.mask = ntohl(inet_addr("255.255.254.0"));
	bcm_eth_dev.next_hop = ntohl(inet_addr("172.16.112.1"));
	bcm_eth_dev.dns = ntohl(inet_addr("172.16.112.1"));
#endif
	dev_register(&bcm_eth_dev, 1/* default */);
	bcm_timer.h = bcm_timer_fun;
	bcm_timer.mask = INET_SOFTIRQ_TIMER;
	inet_softirq_register(&bcm_timer);
	__dump("s_initEth ok \r\n");	

	SetMacAddr();
}
