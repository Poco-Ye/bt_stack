#ifndef _ARP_H_
#define _ARP_H_
#include <inet/inet.h>
#include <inet/ethernet.h>
#include <inet/inet_list.h>
#include <inet/dev.h>
#include <inet/skbuff.h>

/* ARP protocol HARDWARE identifiers. */
#define ARPHRD_NETROM	0		/* from KA9Q: NET/ROM pseudo	*/
#define ARPHRD_ETHER 	1		/* Ethernet 10Mbps		*/
#define	ARPHRD_EETHER	2		/* Experimental Ethernet	*/
#define	ARPHRD_AX25	3		/* AX.25 Level 2		*/
#define	ARPHRD_PRONET	4		/* PROnet token ring		*/
#define	ARPHRD_CHAOS	5		/* Chaosnet			*/
#define	ARPHRD_IEEE802	6		/* IEEE 802.2 Ethernet/TR/TB	*/
#define	ARPHRD_ARCNET	7		/* ARCnet			*/
#define	ARPHRD_APPLETLK	8		/* APPLEtalk			*/
#define ARPHRD_DLCI	15		/* Frame Relay DLCI		*/
#define ARPHRD_ATM	19		/* ATM 				*/
#define ARPHRD_METRICOM	23		/* Metricom STRIP (new IANA id)	*/
#define	ARPHRD_IEEE1394	24		/* IEEE 1394 IPv4 - RFC 2734	*/
#define ARPHRD_EUI64	27		/* EUI-64                       */
#define ARPHRD_INFINIBAND 32		/* InfiniBand			*/

/* ARP protocol opcodes. */
#define	ARPOP_REQUEST	1		/* ARP request			*/
#define	ARPOP_REPLY	2		/* ARP reply			*/
#define	ARPOP_RREQUEST	3		/* RARP request			*/
#define	ARPOP_RREPLY	4		/* RARP reply			*/
#define	ARPOP_InREQUEST	8		/* InARP request		*/
#define	ARPOP_InREPLY	9		/* InARP reply			*/
#define	ARPOP_NAK	10		/* (ATM)ARP NAK			*/

/*
 *	This structure defines an ethernet arp header.
 */
struct arphdr
{
	u16	ar_hrd;		/* format of hardware address	*/
	u16	ar_pro;		/* format of protocol address	*/
	u8	ar_hln;		/* length of hardware address	*/
	u8	ar_pln;		/* length of protocol address	*/
	u16 ar_op;		/* ARP opcode (command)		*/

	 /*
	  *	 Ethernet looks like this : This bit is variable sized however...
	  */
	u8		smac[ETH_ALEN];	/* sender hardware address	*/
	u32		sip;		/* sender IP address		*/
	u8		dmac[ETH_ALEN];	/* target hardware address	*/
	u32		dip;		/* target IP address		*/
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif

;

/* ARP STATUS */
#define ARP_STATIC 0x1/* static arp table */
#define ARP_OK     0x2/* it is ok */
#define ARP_TIMEOUT 0x3/* it is timeout */
#define ARP_DEAD    0x4/* it is dead */

/* ARP TIMER */
#define MAX_ARP_TIME 120*1000/* 120 sec */

struct arpentry
{
	struct list_head list;
	u8  mac[ETH_ALEN];
	u32 ip;
	u8  status;
	INET_DEV_T *dev;
	u32  timestamp;/* in ms */
	int  retry;/* max=10 */
};

int arp_init(void);
struct arpentry * arp_lookup(INET_DEV_T *dev, u32 dip);
int arp_entry_add(u32 ip, u8 *mac, u8 flag);
int arp_wait(struct sk_buff *skb, u32 nexthop, u16 proto, INET_DEV_T *out);
err_t arp_input(struct sk_buff *skb, INET_DEV_T *dev);
err_t arp_send(u16 op, u8 *smac, u32 sip, u8 *dmac, u32 dip, INET_DEV_T *out);
void arp_table_show(void);
int arp_entry_delete(INET_DEV_T *dev);

#endif/* _ARP_H_ */
