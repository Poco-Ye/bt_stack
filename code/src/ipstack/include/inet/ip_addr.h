#ifndef _IP_ADDR_H_
#define _IP_ADDR_H_

#include <inet/inet_type.h>
#include <inet/inet.h>

struct ip_addr 
{
	u32 addr;
};

extern const struct ip_addr ip_addr_any;
extern const struct ip_addr ip_addr_broadcast;

/** IP_ADDR_ can be used as a fixed IP address
 *  for the wildcard and the broadcast address
 */
#define IP_ADDR_ANY         ((struct ip_addr *)&ip_addr_any)
#define IP_ADDR_BROADCAST   ((struct ip_addr *)&ip_addr_broadcast)

#define INADDR_NONE         ((u32)0xffffffff)  /* 255.255.255.255 */
#define INADDR_LOOPBACK     ((u32)0x7f000001)  /* 127.0.0.1 */

/* Definitions of the bits in an Internet address integer.

   On subnets, host and network parts are found according to
   the subnet mask, not these masks.  */

#define IN_CLASSA(a)        ((((u32)(a)) & 0x80000000) == 0)
#define IN_CLASSA_NET       0xff000000
#define IN_CLASSA_NSHIFT    24
#define IN_CLASSA_HOST      (0xffffffff & ~IN_CLASSA_NET)
#define IN_CLASSA_MAX       128

#define IN_CLASSB(a)        ((((u32)(a)) & 0xc0000000) == 0x80000000)
#define IN_CLASSB_NET       0xffff0000
#define IN_CLASSB_NSHIFT    16
#define IN_CLASSB_HOST      (0xffffffff & ~IN_CLASSB_NET)
#define IN_CLASSB_MAX       65536

#define IN_CLASSC(a)        ((((u32)(a)) & 0xe0000000) == 0xc0000000)
#define IN_CLASSC_NET       0xffffff00
#define IN_CLASSC_NSHIFT    8
#define IN_CLASSC_HOST      (0xffffffff & ~IN_CLASSC_NET)

#define IN_CLASSD(a)        (((u32)(a) & 0xf0000000) == 0xe0000000)
#define IN_CLASSD_NET       0xf0000000          /* These ones aren't really */
#define IN_CLASSD_NSHIFT    28                  /*   net and host fields, but */
#define IN_CLASSD_HOST      0x0fffffff          /*   routing needn't know. */
#define IN_MULTICAST(a)     IN_CLASSD(a)

#define IN_EXPERIMENTAL(a)  (((u32)(a) & 0xf0000000) == 0xf0000000)
#define IN_BADCLASS(a)      (((u32)(a) & 0xf0000000) == 0xf0000000)

#define IN_LOOPBACKNET      127                 /* official! */

#define IP4_ADDR(ipaddr, a,b,c,d) \
        (ipaddr)->addr = htonl(((u32)((a) & 0xff) << 24) | \
                               ((u32)((b) & 0xff) << 16) | \
                               ((u32)((c) & 0xff) << 8) | \
                                (u32)((d) & 0xff))

#define ip_addr_set(dest, src) (dest)->addr = \
                               ((src) == NULL? 0:\
                               ((struct ip_addr*)src)->addr)
/**
 * Determine if two address are on the same network.
 *
 * @arg addr1 IP address 1
 * @arg addr2 IP address 2
 * @arg mask network identifier mask
 * @return !0 if the network identifiers of both address match
 */
#define ip_addr_netcmp(addr1, addr2, mask) (((addr1) & \
                                              (mask)) == \
                                             ((addr2) & \
                                              (mask)))
#define ip_addr_cmp(addr1, addr2) ((addr1) == (addr2))

#define ip_addr_isany(addr) (addr == 0)

#define ip_addr_isbroadcast(addr, dev)(((addr)&(dev)->mask)==(dev)->mask)

#define ip_addr_ismulticast(addr) (((addr)& 0xf0000000) == 0xe0000000)

#define ip_addr_debug_print(debug, ipaddr) \
        IPSTACK_DEBUG(debug, ("%"U16_F".%"U16_F".%"U16_F".%"U16_F, \
                ipaddr ? (u16)(ntohl((ipaddr)->addr) >> 24) & 0xff : 0, \
                ipaddr ? (u16)(ntohl((ipaddr)->addr) >> 16) & 0xff : 0, \
                ipaddr ? (u16)(ntohl((ipaddr)->addr) >> 8) & 0xff : 0, \
                ipaddr ? (u16)ntohl((ipaddr)->addr) & 0xff : 0))

/* These are cast to u16, with the intent that they are often arguments
 * to printf using the U16_F format from cc.h. */
#define ip4_addr1(ipaddr) ((u16)(ntohl(ipaddr) >> 24) & 0xff)
#define ip4_addr2(ipaddr) ((u16)(ntohl(ipaddr) >> 16) & 0xff)
#define ip4_addr3(ipaddr) ((u16)(ntohl(ipaddr) >> 8) & 0xff)
#define ip4_addr4(ipaddr) ((u16)(ntohl(ipaddr)) & 0xff)

u16 inet_chksum_pseudo(void *payload, int len,
			u32 src, u32 dest,
			u8 proto);
unsigned long inet_addr(const char *cp);

/* addr is net order */
char *inet_ntoa2(long addr, char *buf/*out*/);
/* addr is net order */
char *inet_ntoa(long addr);

#endif/* _IP_ADDR_H_ */
