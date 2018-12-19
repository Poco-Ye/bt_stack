/*
修改历史:
080624 sunJH
增加了MODEM_IFINDEX、CDMA_IFINDEX、GPRS_IFINDEX等宏定义
080722 sunJH
INET_DEV_T增加域echo_count作为
源地址为本机的IP报文个数统计
080729 sunJH
把CDMA_IFINDEX、GPRS_IFINDEX统一为WNET_IFINDEX
090824 sunJH
增加PPPOE_IFINDEX编号
*/
#ifndef _DEV_H_
#define _DEV_H_
#include <inet/inet.h>
#include <inet/inet_list.h>
#include <inet/if.h>
#include <inet/skbuff.h>
#include <inet/inet_softirq.h>
/*
 * INET DEV Flags
 */
#define FLAGS_UP 0x1 /* link state OK */
#define FLAGS_TX 0x2 /* can tx */
#define FLAGS_RX 0x4 /* can rx */
#define FLAGS_NOARP 0x8 /* NO ARP */
#define FLAGS_NODHCP 0x10 /* NO DHCP */

#define DEV_FLAGS(dev, flag) (((dev)->flags&(flag))==(flag))
struct inet_dev_ops;
struct net_stats;
struct sk_buff;

#define DEV_MAGIC_ID 0x11223344

typedef struct inet_dev_s
{
	struct list_head list;
	u32  magic_id;
	char name[IFNAMSIZ];
	u32  addr;
	u32  mask;
	u32  next_hop;
	u32  dns;
	u32  broadcast_addr;
	int  ifIndex;
	int  header_len;
	u8   mac[6];
	u8   dhcp_enable;	
    u8   dhcp_set_dns_enable;	/*使能静态DNS server设置*/
    u32  dhcp_set_dns_value;	/*静态DNS server值*/
    void    *private_dhcp;
    struct inet_softirq_s dhcp_timer_softirq;
    
	u32  rcv_qlen;
	u32  snd_qlen;
	volatile unsigned int		flags;	/* interface flags (a la BSD)	*/
	int                 mtu;

	/*
	 *	I/O specific fields
	 *	FIXME: Merge these and struct ifmap into one
	 */
	unsigned long		mem_end;	/* shared mem end	*/
	unsigned long		mem_start;	/* shared mem start */
	unsigned long		base_addr;	/* device I/O address	*/
	unsigned int		irq;		/* device IRQ number	*/

	struct inet_dev_ops *dev_ops;
	
	struct net_stats   stats;
	int                reset_tx_timeout;
	int                reset_rx_timeout;

	void               *private_data;

	int                 echo_count;/* 源地址为本机的IP报文个数统计*/

}INET_DEV_T;

typedef struct inet_dev_ops
{
	int (*open)(INET_DEV_T *dev, unsigned int flags);
	int (*close)(INET_DEV_T *dev);
	int (*xmit)(INET_DEV_T *dev, struct sk_buff *skb, u32 nexthop, u16 proto);
	int (*poll)(INET_DEV_T *dev, int count);
	void (*show)(INET_DEV_T *dev);
	int (*set_mac)(INET_DEV_T *dev, u8 *mac);
}INET_DEV_OPS;

struct route
{
	u32 next_hop;
	INET_DEV_T *out;
};

void dev_register(INET_DEV_T *dev, int bDefault);
void dev_show(void);
INET_DEV_T *dev_lookup(int ifIndex);
struct route *ip_route(u32 dest, INET_DEV_T *bind);
int ip_local(u32 dest);
#define ETH_IFINDEX_BASE 0
#define PPPOE_IFINDEX    1
#define PPP_IFINDEX_BASE 10
#define WIFI_IFINDEX_BASE 12
enum
{
	MODEM_IFINDEX = PPP_IFINDEX_BASE,
	WNET_IFINDEX = PPP_IFINDEX_BASE+1,
	MAX_PPP_IFINDEX = 20,
};
#endif/* _DEV_H_ */
