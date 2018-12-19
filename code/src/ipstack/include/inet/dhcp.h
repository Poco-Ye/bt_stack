/*
 *  DHCP
 *  
 */
#ifndef _DHCP_H_
#define _DHCP_H_
#include <inet/inet_type.h>
#include <inet/dev.h>

/* msg type */
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_ACK 5
#define DHCP_NAK 6


struct dhcp_client_s
{
	volatile int state;/* discover, offer, request and ack */
	int time_out;
	int retry;
	u32 id;/* trascation id */
	u32 lease_time;
	u32 offer_ip;
	u32 offer_mask;
	u32 offer_router;
	u32 dns_ip;
	u32 server_ip;
	struct udp_filter_s udp_filter;
	//INET_DEV_T *dev;
	struct inet_dev_s *dev;

	u32 lease_time_switch[3];//added for lease extending
};


/* dhcp client start
 *  dev    ethernet dev
 *  timeout   <0 wait forever,
 *                =0 NOT wait,
 *                >0 the time for waiting
 *  
 */
int dhcpc_start(INET_DEV_T *dev, int timeout);

int dhcpc_stop(INET_DEV_T *dev);

struct dhcp_client_s* dhcpc_get(INET_DEV_T *dev);

int dhcpc_check(INET_DEV_T *dev);
int dhcp_set_dns(INET_DEV_T *dev,const char *addr);

#endif/* _DHCP_H_ */
