#ifndef _ICMP_H_
#define _ICMP_H_
#include <inet/inet_type.h>
#include <inet/skbuff.h>

#define ICMP_ECHOREPLY		0	/* Echo Reply			*/
#define ICMP_DEST_UNREACH	3	/* Destination Unreachable	*/
#define ICMP_SOURCE_QUENCH	4	/* Source Quench		*/
#define ICMP_REDIRECT		5	/* Redirect (change route)	*/
#define ICMP_ECHO		8	/* Echo Request			*/
#define ICMP_TIME_EXCEEDED	11	/* Time Exceeded		*/
#define ICMP_PARAMETERPROB	12	/* Parameter Problem		*/
#define ICMP_TIMESTAMP		13	/* Timestamp Request		*/
#define ICMP_TIMESTAMPREPLY	14	/* Timestamp Reply		*/
#define ICMP_INFO_REQUEST	15	/* Information Request		*/
#define ICMP_INFO_REPLY		16	/* Information Reply		*/
#define ICMP_ADDRESS		17	/* Address Mask Request		*/
#define ICMP_ADDRESSREPLY	18	/* Address Mask Reply		*/
#define NR_ICMP_TYPES		18


struct icmphdr {
  u8		type;
  u8		code;
  u16		checksum;
  union {
	struct {
		u16	id;
		u16	sequence;
	} echo;
	u32	gateway;
	struct {
		u16	__unused;
		u16	mtu;
	} frag;
  } un;
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif

;

/*
 *  return value:
 *     <0     Error code
 *     >=0   耗费的时间  
 */
int NetPing_std(char* dst_ip_str, long timeout_ms, int size);

void icmp_input(struct sk_buff *skb);

#endif/* _ICMP_H_ */
