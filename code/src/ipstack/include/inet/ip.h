#ifndef _IP_H_
#define _IP_H_
#include <inet/inet.h>
#include <inet/dev.h>
/*
 * Option flags per-NetSocket. These are the same like SO_XXX.
 */
#define SOF_DEBUG     (u16)0x0001U    /* turn on debugging info recording */
#define SOF_ACCEPTCONN  (u16)0x0002U    /* NetSocket has had listen() */
#define SOF_REUSEADDR (u16)0x0004U    /* allow local address reuse */
#define SOF_KEEPALIVE (u16)0x0008U    /* keep connections alive */
#define SOF_DONTROUTE (u16)0x0010U    /* just use interface addresses */
#define SOF_BROADCAST (u16)0x0020U    /* permit sending of broadcast msgs */
#define SOF_USELOOPBACK (u16)0x0040U    /* bypass hardware when possible */
#define SOF_LINGER      (u16)0x0080U    /* linger on close if data present */
#define SOF_OOBINLINE (u16)0x0100U    /* leave received OOB data in line */
#define SOF_REUSEPORT (u16)0x0200U    /* allow local address & port reuse */

#define IP_RF 0x8000        /* reserved fragment flag */
#define IP_DF 0x4000        /* dont fragment flag */
#define IP_MF 0x2000        /* more fragments flag */
#define IP_OFFMASK 0x1fff   /* mask for fragmenting bits */


struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	u8	ihl:4,
		version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	u8	version:4,
  		ihl:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	u8	tos;
	u16	tot_len;
	u16	id;
	u16	frag_off;
	u8	ttl;
	u8	protocol;
	u16	check;
	u32	saddr;
	u32	daddr;
	/*The options start here. */
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif

;


#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP 17
#define IP_PROTO_UDPLITE 136
#define IP_PROTO_TCP 6

void ip_init(void);
struct route *ip_route(u32 dest, INET_DEV_T *);
err_t ip_input(struct sk_buff *skb, INET_DEV_T *in) ;
err_t ip_output(struct sk_buff *skb, u32 src, u32 dest,
				u8 ttl, u8 tos, u8 proto, struct route *rt);
void ip_build_header(struct iphdr *iph, u32 src, u32 dest,
				u8 ttl, u8 tos, u8 proto, u16 total_len);

#define IP_IN 0
#define IP_OUT 1
#define IP_DROP 1
#define IP_PASS 0
int ip_policy(struct sk_buff *skb, INET_DEV_T *out, int direct);


#endif/* _IP_H_ */
