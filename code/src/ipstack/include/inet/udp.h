#ifndef _UDP_H_
#define _UDP_H_
#include <inet/inet_type.h>

struct udphdr {
    u16 source;   /*!< \brief Source port */
    u16 dest;   /*!< \brief Destination port */
    u16 len;    /*!< \brief UDP length */
    u16 check;     /*!< \brief UDP checksum */
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif
;

struct udp_pcb
{
	/* Common members of all PCB types */
	u32 local_ip; 
	u32 remote_ip; 
	/* Socket options */  
	u16 so_options;
	/* Type Of Service */ 
	u8 tos;
	/* Time To Live */	
	u8 ttl;
  	struct udp_pcb *next; /* for the linked list */
	u8 prio;
	void *callback_arg;

	u8 flags;
	u16 local_port, remote_port;
	
	u16 chksum_len;

	struct sk_buff_head  incoming;
	
	void (* recv)(void *arg, struct udp_pcb *pcb, struct sk_buff *p,
	  u32 remote_ip, u16 remote_port);
	void *recv_arg;  

	INET_DEV_T *out_dev;/*bind interface */
};

typedef struct udp_filter_s
{
	struct list_head list;
	u16 source;
	u16 dest;
	u32 arg;
	void (*fun)(u32 arg, struct sk_buff *skb);
	int in_used;/* used by udp internal, init 0 */
}UDP_FILTER_T;

void udp_init(void);
struct udp_pcb *udp_new(void);
void udp_input(struct sk_buff *skb);
void udp_free(struct udp_pcb *pcb);
int udp_recv_data(struct udp_pcb *pcb, u8 *buf, int size, u32 *p_remote_ip, u16 *p_remote_port);
err_t udp_output(struct udp_pcb *pcb, void *data, int size, u32 dst, u16 dport);
int udp_bind(struct udp_pcb *pcb, u32 addr, u16 port);
int udp_filter_register(UDP_FILTER_T *filter);
void udp_filter_unregister(UDP_FILTER_T *filter);

#endif/* _UDP_H_ */
