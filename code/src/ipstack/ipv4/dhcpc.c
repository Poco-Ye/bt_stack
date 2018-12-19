/*
   DHCP Client
修改历史:
20080605:
1.inet_delay时间粒度应该采用INET_TIMER_SCALE,而不是1；从而可以适合sp30,
因为sp30的INET_TIMER_SCALE=10
2.dhcp offer的server id有时候会存放在options里，而
siaddr=0
20081020 sunJH
DhcpStart:启动后直接返回
20090311 sunJH
1. discover,request的transaction id保持一致
因此point客户使用的cisco router有防火墙功能,
对discover和request的id不一样，会拒绝(nak)
2. 每次都从discover开始
20090319 sunJH
Request增加Client ID Option
20090709 sunJH
问题描述如下:
DHCP failed when Fortigate 100 firewall acts as server 
问题分析:
根据RFC, client address必须要保持一致;
修改如下:
msg_out->ciaddr = 0;//htonl(client->offer_ip);
20090803 sunJH
租借时间单位应为秒而不是毫秒
因此dhcp_ack_process在保存lease_time要进行转换
20090928 sunJH
在dhcp_ack_process中，
解决dhcp 不能分配到IP时由于一些变量没有初始化从而导致
配置信息错误
20101111 sunJH
dhcp_input对报文的识别增加chaddr域判断
*/
//2016.9.23--Revised DHCP renew process
//           Revised secs field initialization in dhcp_build_request()
//           Changed DISCOVER and REQUEST option list to be same
//2016.12.13--Added OPTIONS_GET_LONGS macro to process multiple intergers

#include <inet/inet.h>
#include <inet/inet_type.h>
#include <inet/dev.h>
#include <inet/skbuff.h>
#include <inet/ip.h>
#include <inet/udp.h>
#include <inet/ethernet.h>
#include <inet/dhcp.h>
#include <inet/inet_softirq.h>

/** BootP options */
#define DHCP_OPTION_PAD 0
#define DHCP_OPTION_SUBNET_MASK 1 /* RFC 2132 3.3 */
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DNS_SERVER 6 
#define DHCP_OPTION_DNS        0xf
#define DHCP_OPTION_HOSTNAME 12
#define DHCP_OPTION_IP_TTL 23
#define DHCP_OPTION_MTU 26
#define DHCP_OPTION_BROADCAST 28
#define DHCP_OPTION_TCP_TTL 37
#define DHCP_OPTION_END 255

/** DHCP options */
#define DHCP_OPTION_REQUESTED_IP 50 /* RFC 2132 9.1, requested IP address */
#define DHCP_OPTION_LEASE_TIME 51 /* RFC 2132 9.2, time in seconds, in 4 bytes */
#define DHCP_OPTION_OVERLOAD 52 /* RFC2132 9.3, use file and/or sname field for options */

#define DHCP_OPTION_MESSAGE_TYPE 53 /* RFC 2132 9.6, important for DHCP */
#define DHCP_OPTION_MESSAGE_TYPE_LEN 1


#define DHCP_OPTION_SERVER_ID 54 /* RFC 2132 9.7, server IP address */
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 55 /* RFC 2132 9.8, requested option types */

#define DHCP_OPTION_MAX_MSG_SIZE 57 /* RFC 2132 9.10, message size accepted >= 576 */
#define DHCP_OPTION_MAX_MSG_SIZE_LEN 2

#define DHCP_OPTION_T1 58 /* T1 renewal time */
#define DHCP_OPTION_T2 59 /* T2 rebinding time */
#define DHCP_OPTION_VENDOR_CLASS_ID	60
#define DHCP_OPTION_CLIENT_ID 61
#define DHCP_OPTION_TFTP_SERVERNAME 66
#define DHCP_OPTION_BOOTFILE 67


#define DHCP_HTYPE_ETH 1

#define DHCP_HLEN_ETH 6


#define DHCP_CHADDR_LEN 16U
#define DHCP_SNAME_LEN 64U
#define DHCP_FILE_LEN 128U
#define DHCP_OPTIONS_LEN 68U

#define DHCP_DEBUG //s80_printf

struct dhcp_msg                  
{                                
	u8 op;                        /*!< \brief Packet opcode type: 1=request, 2=reply */    
	u8 htype;                     /*!< \brief Hardware address type: 1=Ethernet */         
	u8 hlen;                      /*!< \brief Hardware address length: 6 for Ethernet */   
	u8 hops;                      /*!< \brief Gateway hops */                              
	u32 xid;                      /*!< \brief Transaction ID */                            
	u16 secs;                     /*!< \brief Seconds since boot began */                  
	u16 flags;                    /*!< \brief RFC1532 broadcast, etc. */                   
	u32 ciaddr;                   /*!< \brief Client IP address */                         
	u32 yiaddr;                   /*!< \brief 'Your' IP address */                         
	u32 siaddr;                   /*!< \brief Server IP address */                         
	u32 giaddr;                   /*!< \brief Gateway IP address */                        
	u8 chaddr[DHCP_CHADDR_LEN];   /*!< \brief Client hardware address */                   
	u8 sname[DHCP_SNAME_LEN];     /*!< \brief Server host name */                          
	u8 file[DHCP_FILE_LEN];       /*!< \brief Boot file name */                            
	u32 cookie;                   /*!< \brief Cookie */                                    
	u8 options[DHCP_OPTIONS_LEN]; /*!< \brief Vendor-specific area */                      
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif
;
static u32 xid = 0xdeca0000;
unsigned char InfoData[50]={0,};
unsigned char VenderName[16] = "PAX";
static void dhcp_build_request(struct dhcp_msg *msg_out, struct dhcp_client_s *client)
{
	INET_DEV_T *dev = client->dev;
	msg_out->op = DHCP_DISCOVER;
	/* TODO: make link layer independent */
	msg_out->htype = DHCP_HTYPE_ETH;
	/* TODO: make link layer independent */
	msg_out->hlen = DHCP_HLEN_ETH;
	msg_out->hops = 0;
	msg_out->xid = htonl(xid);
	msg_out->secs = htons(4);//0;format revised on 2016.9.23
	msg_out->flags = htons(0x8000);/* broadcast */
	msg_out->ciaddr = 0;//htonl(client->offer_ip);
	msg_out->yiaddr = 0;
	msg_out->siaddr = 0;
	msg_out->giaddr = 0;
	INET_MEMCPY(msg_out->chaddr, dev->mac, DHCP_HLEN_ETH);
	msg_out->cookie = htonl(0x63825363UL);
}

static void dhcp_option(u8 *options, int *p_options_len, u8 option_type, u8 option_len)
{
	options[(*p_options_len)++] = option_type;
	options[(*p_options_len)++] = option_len;
}

static void dhcp_option_end(u8 *options, int *p_options_len)
{
	options[(*p_options_len)++] = 0xff;
}

/*
 * Concatenate a single byte to the outgoing DHCP message.
 *
 */
static void dhcp_option_byte(u8 *options, int *p_options_len, u8 value)
{
	options[(*p_options_len)++] = value;
}
static void dhcp_option_short(u8 *options, int *p_options_len, u16 value)
{
	options[(*p_options_len)++] = (value & 0xff00U) >> 8;
	options[(*p_options_len)++] =  value & 0x00ffU;
}
static void dhcp_option_long(u8 *options, int *p_options_len, u32 value)
{
	options[(*p_options_len)++] = (u8)((value & 0xff000000UL) >> 24);
	options[(*p_options_len)++] = (u8)((value & 0x00ff0000UL) >> 16);
	options[(*p_options_len)++] = (u8)((value & 0x0000ff00UL) >> 8);
	options[(*p_options_len)++] = (u8)((value & 0x000000ffUL));
}
static void dhcp_option_buf(u8 *options, int *p_options_len, u8 *buf, int len)
{
	for(;len>0;len--)
	{
		options[(*p_options_len)++] = *buf;
		buf++;
	}
}

int dhcp_discover(struct dhcp_client_s *client)
{
	INET_DEV_T *dev = client->dev;
	struct dhcp_msg *msg;
	struct udphdr *udph;
	int option_len, err;
	int size = dev->header_len+sizeof(struct iphdr)+
				sizeof(struct udphdr)+
				sizeof(struct dhcp_msg);
	struct sk_buff *skb = skb_new(size);

	DHCP_DEBUG("dhcp_discover...........\n");
	if(skb == NULL)
	{
		return NET_ERR_MEM;
	}
	skb_reserve(skb, size);
	msg = (struct dhcp_msg*)__skb_push(skb, sizeof(struct dhcp_msg));
	xid++;//保持discover,request的transaction ID一致
	dhcp_build_request(msg, client);
	client->id = msg->xid;
	GetDeviceInfo(InfoData);
	GetVenderName(VenderName);
	DHCP_DEBUG("host name: %s\n",InfoData);
	/*
	** 1.测试发现一些dhcp 需要client id
	** 2.option根据netgear的dhcp 
	**/
	option_len = 0;	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
	dhcp_option_byte(msg->options, &option_len, DHCP_DISCOVER);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
	dhcp_option_short(msg->options, &option_len,  590);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_HOSTNAME,strlen(InfoData));
	dhcp_option_buf(msg->options, &option_len,  InfoData, strlen(InfoData));
	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_VENDOR_CLASS_ID,strlen(VenderName));
	dhcp_option_buf(msg->options, &option_len,  VenderName, strlen(VenderName));
	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_PARAMETER_REQUEST_LIST, 5/*num options*/);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_SUBNET_MASK);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_ROUTER);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_BROADCAST);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_DNS_SERVER);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_DNS);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_CLIENT_ID, 7);
	dhcp_option_byte(msg->options, &option_len,  1);
	dhcp_option_buf(msg->options, &option_len,  dev->mac, 6);

	/** option END ***/
	dhcp_option_end(msg->options, &option_len);
	

	skb->h.raw = __skb_push(skb, sizeof(struct udphdr));
	udph = skb->h.uh;
	udph->source = htons(DHCPC_PORT);
	udph->dest = htons(DHCPS_PORT);
	udph->len = htons(sizeof(struct udphdr)+sizeof(struct dhcp_msg));
	udph->check = 0;
	udph->check = inet_chksum_pseudo(udph, skb->len, 0, 0xffffffff,
				IP_PROTO_UDP);

	skb->nh.raw = __skb_push(skb, sizeof(struct iphdr));
	ip_build_header(skb->nh.iph, 0, 0xffffffff, 64, 0, 
    			IP_PROTO_UDP, (u16)skb->len);
	err = dev->dev_ops->xmit(dev, skb, 0, ETH_P_IP);
	client->state = DHCP_DISCOVER;
	client->time_out = 5000;
	skb_free(skb);

	DHCP_DEBUG("dhcp_discover err=%d\n", err);
	return NET_OK;
}

int dhcp_request(struct dhcp_client_s *client)
{
	INET_DEV_T *dev = client->dev;
	struct dhcp_msg *msg;
	struct udphdr *udph;
	int option_len;
	int size = dev->header_len+sizeof(struct iphdr)+
				sizeof(struct udphdr)+
				sizeof(struct dhcp_msg);
	struct sk_buff *skb;
	int err;

	DHCP_DEBUG("dhcp_request..........\n");
	skb = skb_new(size);
	if(skb == NULL)
	{
		return NET_ERR_MEM;
	}
	skb_reserve(skb, size);
	msg = (struct dhcp_msg*)__skb_push(skb, sizeof(struct dhcp_msg));
	dhcp_build_request(msg, client);
	client->id = msg->xid;

	option_len = 0;	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
	dhcp_option_byte(msg->options, &option_len, DHCP_REQUEST);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
	dhcp_option_short(msg->options, &option_len,  576);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_REQUESTED_IP, 4);
	dhcp_option_long(msg->options, &option_len, client->offer_ip);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_SERVER_ID, 4);
	dhcp_option_long(msg->options, &option_len, client->server_ip);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_HOSTNAME,strlen(InfoData));
	dhcp_option_buf(msg->options, &option_len,  InfoData, strlen(InfoData));
	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_VENDOR_CLASS_ID,strlen(VenderName));
	dhcp_option_buf(msg->options, &option_len,  VenderName, strlen(VenderName));
	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_PARAMETER_REQUEST_LIST, 5/*num options*/);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_SUBNET_MASK);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_ROUTER);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_BROADCAST);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_DNS_SERVER);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_DNS);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_CLIENT_ID, 7);
	dhcp_option_byte(msg->options, &option_len,  1);
	dhcp_option_buf(msg->options, &option_len,  dev->mac, 6);

	/** option END ***/
	dhcp_option_end(msg->options, &option_len);

	skb->h.raw = __skb_push(skb, sizeof(struct udphdr));
	udph = skb->h.uh;
	udph->source = htons(DHCPC_PORT);
	udph->dest = htons(DHCPS_PORT);
	udph->len = htons(sizeof(struct udphdr)+sizeof(struct dhcp_msg));

	skb->nh.raw = __skb_push(skb, sizeof(struct iphdr));
	ip_build_header(skb->nh.iph, 0, 0xffffffff, 64, 0, 
    			IP_PROTO_UDP, (u16)skb->len);
	err = dev->dev_ops->xmit(dev, skb, 0, ETH_P_IP);
	client->state = DHCP_REQUEST;
	client->time_out = 5000;
	skb_free(skb);

	DHCP_DEBUG("dhcp_request err=%d\n", err);
	return NET_OK;
}

int dhcp_renew(struct dhcp_client_s *client,int cur_step)
{
	INET_DEV_T *dev = client->dev;
	struct dhcp_msg *msg;
	struct udphdr *udph;
	int option_len;
	int size = dev->header_len+sizeof(struct iphdr)+
				sizeof(struct udphdr)+
				sizeof(struct dhcp_msg);
	struct sk_buff *skb;
	int err;

	skb = skb_new(size);
	if(skb == NULL)
	{
		return NET_ERR_MEM;
	}

	if(cur_step>=3)
		return NET_ERR_BUF;

	skb_reserve(skb, size);
	msg = (struct dhcp_msg*)__skb_push(skb, sizeof(struct dhcp_msg));
	dhcp_build_request(msg, client);
	client->id = msg->xid;
	
	if(!cur_step)msg->flags = 0;//unicast
	msg->secs = 0;
	msg->ciaddr=htonl(client->offer_ip);

	option_len = 0;	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
	dhcp_option_byte(msg->options, &option_len, DHCP_REQUEST);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
	dhcp_option_short(msg->options, &option_len,  576);


	dhcp_option(msg->options, &option_len, DHCP_OPTION_HOSTNAME,strlen(InfoData));
	dhcp_option_buf(msg->options, &option_len,  InfoData, strlen(InfoData));
	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_VENDOR_CLASS_ID,strlen(VenderName));
	dhcp_option_buf(msg->options, &option_len,  VenderName, strlen(VenderName));
	
	dhcp_option(msg->options, &option_len, DHCP_OPTION_PARAMETER_REQUEST_LIST, 5/*num options*/);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_SUBNET_MASK);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_ROUTER);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_BROADCAST);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_DNS_SERVER);
	dhcp_option_byte(msg->options, &option_len, DHCP_OPTION_DNS);

	dhcp_option(msg->options, &option_len, DHCP_OPTION_CLIENT_ID, 7);
	dhcp_option_byte(msg->options, &option_len,  1);
	dhcp_option_buf(msg->options, &option_len,  dev->mac, 6);

	/** option END ***/
	dhcp_option_end(msg->options, &option_len);

	skb->h.raw = __skb_push(skb, sizeof(struct udphdr));
	udph = skb->h.uh;
	udph->source = htons(DHCPC_PORT);
	udph->dest = htons(DHCPS_PORT);
	udph->len = htons(sizeof(struct udphdr)+sizeof(struct dhcp_msg));

	skb->nh.raw = __skb_push(skb, sizeof(struct iphdr));

	//--form a unicast frame or a broadcast frame
	if(!cur_step)
	{
		ip_build_header(skb->nh.iph, client->offer_ip, client->server_ip, 64, 0, IP_PROTO_UDP, (u16)skb->len);
		err = dev->dev_ops->xmit(dev, skb, dev->next_hop, ETH_P_IP);
	}
	else
	{
		ip_build_header(skb->nh.iph, 0, 0xffffffff, 64, 0,IP_PROTO_UDP, (u16)skb->len);
		err = dev->dev_ops->xmit(dev, skb, 0, ETH_P_IP);
	}
	client->state = DHCP_ACK;
	
	client->time_out = client->lease_time_switch[cur_step];//next state switch time if server keeps absent
	skb_free(skb);


	return NET_OK;
}

#define OPTIONS_MOVE(opt, step) \
do{ \
	if((opt)+(step)>=(end)) \
		goto err; \
	opt += step; \
}while(0)

#define OPTIONS_BYTE_CHECK(options, value) \
do{ \
	OPTIONS_MOVE(options, 1); \
	if(*options != 1) \
		goto err; \
	OPTIONS_MOVE(options, 1); \
	if(*options != value) \
		goto err; \
	OPTIONS_MOVE(options, 1); \
}while(0)

#define OPTIONS_GET_LONG(options, value) \
do{ \
	OPTIONS_MOVE(options, 1); \
	if(*options != 4) \
		goto err; \
	OPTIONS_MOVE(options, 1); \
	INET_MEMCPY(&(value), options, 4); \
	value = ntohl(value); \
	OPTIONS_MOVE(options, 4); \
}while(0)

//Added on 2016.12.13 to process multiple intergers
#define OPTIONS_GET_LONGS(options, value) \ 
do{ \
	unsigned char dn;\
	OPTIONS_MOVE(options, 1); \
	dn=*options;\
	if(!dn || dn%4) \
		goto err; \
	OPTIONS_MOVE(options, 1); \
	INET_MEMCPY(&(value), options, 4); \
	value = ntohl(value); \
	OPTIONS_MOVE(options, dn); \
}while(0)

int dhcp_offer_process(struct dhcp_client_s *client, struct dhcp_msg *msg, int len)
{
	int boffer = 0;
	u32 renew_time = 0;
	u32 rebind_time = 0;
	u32 lease_time = 0;
	u32 offer_ip = 0;
	u32 server_ip = 0;
	u8 *options = msg->options;
	u8 *end = options+len;

	DHCP_DEBUG("dhcp_offer_process...........\n");
	while(options < end)
	{
		if(*options == 0xff)//end
		{
			break;
		}
		if(*options == DHCP_OPTION_MESSAGE_TYPE)
		{
			OPTIONS_BYTE_CHECK(options, DHCP_OFFER);
			boffer = 1;
		}else if(*options == DHCP_OPTION_SERVER_ID)
		{
			INET_MEMCPY(&msg->siaddr, options+2, 4);
			options += 6;
		}else
		{
			options++;
			options += options[0] + 1;//len
		}
	}
	if(!boffer)
		goto err;
	//the packet is ok
	client->offer_ip = ntohl(msg->yiaddr);
	client->server_ip = ntohl(msg->siaddr);
	DHCP_DEBUG("dhcp_offer_process packet is OK!\n");
	dhcp_request(client);
err:
	return 0;
}

int dhcp_ack_process(struct dhcp_client_s *client, struct dhcp_msg *msg, int len)
{
	int back = 0,i;
	INET_DEV_T *dev = client->dev;
	u32 mask, lease_time, router, dns;
	u8 *options = msg->options;
	u8 *end = options+len;

	/* 下面这些变量要初始化 090928,
	   当dhcp server不能分配IP地址时(地址池已使用完成)
	   会响应ACK,但是IP是错误的,且没有mask,gateway;
	*/
	mask = lease_time = router = dns = 0;

	DHCP_DEBUG("dhcp_ack_process..............\n");
	
	while(options < end)
	{
		if(*options == 0xff)//end
		{
			break;
		}
		switch(*options)
		{
		case DHCP_OPTION_MESSAGE_TYPE:
			OPTIONS_BYTE_CHECK(options, DHCP_ACK);
			back = 1;
			break;
		case DHCP_OPTION_SUBNET_MASK:
			OPTIONS_GET_LONG(options, mask);
			break;
		case DHCP_OPTION_LEASE_TIME:
			OPTIONS_GET_LONG(options, lease_time);
			break;
		case DHCP_OPTION_ROUTER:
			OPTIONS_GET_LONGS(options, router);//revised on 2016.12.13
			break;
		case DHCP_OPTION_DNS_SERVER:
		{
			u8 len;
			OPTIONS_MOVE(options, 1); 
			len = *options;
			if((len&0x3)!=0)/* len must be multiple of 4 */
				goto err; 
			OPTIONS_MOVE(options, 1); 
			INET_MEMCPY(&(dns), options, 4); 
			dns = ntohl(dns); 
			OPTIONS_MOVE(options, len); 
		}
			break;
		default:
			options++;
			options += *options+1;
		}
	}
	if(back==0||mask==0||lease_time==0)
	{
		goto err;
	}
	//packet is ok
	DHCP_DEBUG("dhcp_ack_process packet OK!\n");
	if(lease_time > 24*3600)
		lease_time = 24*3600;
	lease_time *= 1000;//转换成毫秒
	client->lease_time = lease_time;
	client->offer_ip = ntohl(msg->yiaddr);
	client->offer_mask = mask;
	client->offer_router = router;
	client->dns_ip = dns;
	client->state = DHCP_ACK;
	client->time_out = lease_time/2;//T1 of the lease time

	client->retry=0;//for the following lease extending step
	client->lease_time_switch[0]=(lease_time-client->time_out)/2;//unicast retry time of DHCP_REQUEST
	client->lease_time_switch[1]=lease_time/8;//first broadcast time T2 of DHCP_REQUEST
	client->lease_time_switch[2]=lease_time/16;//first broadcast time T2 of DHCP_REQUEST
	//--ensure each time is no less than 60 seconds
	for(i=0;i<3;i++)
		if(client->lease_time_switch[i]<60000)client->lease_time_switch[i]=60000;
	//bind it
	dev->addr = client->offer_ip;
	dev->mask = client->offer_mask;
	dev->next_hop = client->offer_router;
	if(dev->dhcp_set_dns_enable == 0x01)
		dev->dns = dev->dhcp_set_dns_value;
    else
        dev->dns = client->dns_ip;
	
err:
	return 0;
}

void dhcp_input(u32 arg, struct sk_buff *skb)
{
	struct dhcp_msg *msg;
	struct dhcp_client_s *client = (struct dhcp_client_s *)arg;

	msg = (struct dhcp_msg*)skb->data;
	if(msg->xid != client->id)
	{
		DHCP_DEBUG("dhcp_input: id %08x is bad,[%08x]\n",
			msg->xid, client->id);
		goto drop;
	}
	if(msg->op != 2/* reply */)
	{
		DHCP_DEBUG("dhcp_input: op %d is not REPLY!\n",msg->op);
		goto drop;
	}
	/*
	增加检查chaddr,
	避免XID一样而导致报文识别错误
	*/
	if(client->dev==NULL||INET_MEMCMP(msg->chaddr, client->dev->mac, ETH_ALEN)!=0)
	{
		DHCP_DEBUG("dhcp_input: chaddr is not REPLY!\n");
		goto drop;
	}
	switch(client->state)
	{
	case DHCP_DISCOVER:
		//xid++;不改变transaction id
		/* Cisco Server can only accept transaction ID which is  similar to Discover */
		dhcp_offer_process(client, msg, skb->len);
		break;
	case DHCP_REQUEST:
	case DHCP_ACK:
		//xid++;不改变transaction id
		dhcp_ack_process(client, msg, skb->len);
		break;
	default:
		DHCP_DEBUG("dhcp_input: unknow state=%d\n", client->state);
	}

drop:
	//skb_free(skb);
	return;
}

void dhcp_timer(unsigned long flag, INET_DEV_T *dev)
{
	struct dhcp_client_s *client = (struct dhcp_client_s*)dev->private_dhcp;
	assert(flag&INET_SOFTIRQ_TIMER);
	if(client->time_out < 0)
		return;
	client->time_out -= INET_TIMER_SCALE;
	if(client->time_out>0)return;

	switch(client->state)
	{
		case DHCP_DISCOVER:
			if(client->retry > 0)
			{
				//client->retry--;
				dhcp_discover(client);
			}
			break;
		case DHCP_REQUEST:
			if(client->retry > 0)
			{
				client->retry--;
				dhcp_request(client);
			}else
			{
				client->retry = 10;
				dhcp_discover(client);
			}
			break;
		case DHCP_ACK:
			//dhcp_request(client);
			dhcp_renew(client,client->retry);
			if(client->retry<2)client->retry++;
			break;
	}//switch
}



/* dhcp client start
 *  dev    ethernet dev
 *  timeout   <0 wait forever,
 *                =0 NOT wait,
 *                >0 the time for waiting
 *  
 */
int dhcpc_start(INET_DEV_T *dev, int timeout)
{
	UDP_FILTER_T *filter; 
    struct dhcp_client_s *p_dhcp_client = (struct dhcp_client_s *)dev->private_dhcp;
	DHCP_DEBUG("dchp client start! %p\r\n",p_dhcp_client);
	if(DEV_FLAGS(dev, FLAGS_NODHCP))
	{
		DHCP_DEBUG("dev %s no dhcp!\n", dev->name);
		return NET_ERR_IF;
	}
	if(dev->dhcp_enable)
	{
		//DHCP_DEBUG("dhcp client stop firstly!\n");
		//dhcpc_stop(dev);
		return NET_OK;
	}
	inet_softirq_disable();
	filter = &p_dhcp_client->udp_filter;
	filter->source = DHCPS_PORT;
	filter->dest = DHCPC_PORT;
	filter->arg = (u32)p_dhcp_client;
	filter->fun = dhcp_input;
	udp_filter_register(filter);
	dev->dhcp_enable = 1;
	p_dhcp_client->offer_ip = dev->addr;
	p_dhcp_client->server_ip = 0;
	dev->addr = dev->mask = dev->next_hop = dev->dns = 0;
	#if 0//从discover阶段开始
	if(dhcp_client.dev == dev &&
		dhcp_client.state == DHCP_ACK)
	{
		dhcp_request(&dhcp_client);
	}else
	#endif
	{
		p_dhcp_client->state = 0;
		p_dhcp_client->dev = dev;
		dhcp_discover(p_dhcp_client);
	}
	p_dhcp_client->retry = 10;
	dev->dhcp_timer_softirq.mask = INET_SOFTIRQ_TIMER;
	inet_softirq_enable();
	
	inet_softirq_register(&dev->dhcp_timer_softirq);

	if(timeout == 0)
	{
		return NET_ERR_AGAIN;
	}
	if(timeout > 0)
	{
		do{
			if(p_dhcp_client->state == DHCP_ACK)
				break;
			inet_delay(INET_TIMER_SCALE);
			timeout-=INET_TIMER_SCALE;
		}while(timeout>0);
		if(p_dhcp_client->state == DHCP_ACK)
			return NET_OK;
		return NET_ERR_TIMEOUT;
	}else
	{
		while(p_dhcp_client->state != DHCP_ACK)
		{
			inet_delay(INET_TIMER_SCALE);
			if(p_dhcp_client->retry<=0)
				return NET_ERR_TIMEOUT;
		}
	}	
	return NET_OK;
}

int dhcpc_check(INET_DEV_T *dev)
{   
    struct dhcp_client_s *p_dhcp_client = (struct dhcp_client_s *)dev->private_dhcp;
	if(p_dhcp_client->state == DHCP_ACK)
		return NET_OK;
	return NET_ERR_AGAIN;
}

int dhcpc_stop(INET_DEV_T *dev)
{
	struct dhcp_client_s *p_dhcp_client = (struct dhcp_client_s *)dev->private_dhcp;
    dev->dhcp_set_dns_enable = 0;
    
	if(!dev->dhcp_enable)
		return NET_OK;
	/* do in half bottom */
	inet_softirq_disable();
	udp_filter_unregister(&p_dhcp_client->udp_filter);
	inet_softirq_enable();
	
	inet_softirq_unregister(&dev->dhcp_timer_softirq);
	
	dev->dhcp_enable = 0;
	p_dhcp_client->state = 0;
	return NET_OK;
}

int dhcp_set_dns(INET_DEV_T *dev,const char *addr)
{
    u32 dns;
    char *ptr;
    char buf[16];
    u32 tmp_len = 0,len;
   
	len = strlen(addr);
	
    ptr = strchr(addr,';');
    if(ptr)
        tmp_len = ptr - addr;
    else
        tmp_len = len;

    len = (tmp_len>len)?len:tmp_len;
    
    if( len<7 || len>15)//0.0.0.0~ 255.255.255.255
            return NET_ERR_VAL;

    memcpy(buf,addr,len);
    buf[len] = 0;
    
    dns = ntohl(inet_addr(buf));
	if(dns == 0 || dns==(u32)-1)
		return NET_ERR_VAL; 
    
    if(dev->dhcp_enable == 0)
        return NET_ERR_NEED_START_DHCP;

    inet_softirq_disable();
    dev->dhcp_set_dns_value = dns;//当dhcp获取到或dhcpstart后地址时设置值正式生效。    
    dev->dhcp_set_dns_enable = 1;
    dev->dns = dev->dhcp_set_dns_value;
	inet_softirq_enable();    
    return NET_OK;

}
struct dhcp_client_s* dhcpc_get(INET_DEV_T *dev)
{
    return (struct dhcp_client_s *)dev->private_dhcp;
}

