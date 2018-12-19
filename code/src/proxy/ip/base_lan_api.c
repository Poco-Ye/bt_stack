//S60 Handset TCP/IP API
//2012.12.18,First edition
//2013.02.23,Revised non-block process in NetConnect() and NetRecv()
#include "base_lan_api.h"
#define DNS_NAME_SIZE 255/* the max limit */

enum IP_FUNC{
	F_NET_CLOSE=0,F_NET_CONNECT,F_NET_SEND,F_NET_RECV,F_GET_EVENT=6,
	F_DHCP_START=10,F_DHCP_STOP,F_DHCP_CHECK,F_ETH_GET,F_ETH_SET,
	F_NET_SOCKET,F_NET_IOCTRL,F_SET_ROUTE,F_GET_ROUTE,F_GET_CFG,
	F_DNS_RESOLVE,F_GET_DNS_REPLY,F_NET_PING,F_GET_PING_REPLY,F_GET_MAC,F_SET_MAC,
	F_GET_VERSION,F_GET_VERSIONS,F_PPP_LOGIN,F_PPP_CHECK,F_PPP_LOGOUT,F_ADD_STATIC_ARP,
	F_SET_RATE_DUPLEX,F_GET_FIRST_ROUTE_MAC,F_NET_BIND,F_NET_LISTEN,F_NET_ACCEPT,F_DNS_RESOLVE_EXT,
	F_SET_DHCP_DNS,
};

#define TM_LAN 4
#define LINK_PAYLOAD 1024
#define LAN_IO_SIZE 4097
#define TO_REPLY 8000
#define SOCK_BLOCK   1
#define SOCK_NOBLOCK 2
//#define MAXDELAY 3000      /* maximum delay in retry loop */

static struct ETH_CONFIG
{
	u8   bitmap;
	u8   ip[20];
	u8   mask[20];
	u8   gateway[20];
	u8   dns[20];	
} __attribute__((__packed__)) eth_conf;

static struct LAN_STATE
{	
	int   dhcp_enable;
	char  route_config;
	int   default_route;
	//0 --- not created    	1 --- block socket 
	//2 --- NO block socket	<0 --- no socket because SYN 
	volatile int   socket_status[MAX_SOCK_COUNT];
	int socket_opened[MAX_SOCK_COUNT];
	int socket_conn[MAX_SOCK_COUNT];//>=0 Connect OK,<0 DisConnect
	int socket_timeout[MAX_SOCK_COUNT];
	int socket_type[MAX_SOCK_COUNT];
}lan_state;

u8 RspPool[LINK_PAYLOAD];
u16 RspLen;

void TcpipProcess(u8 func_no,u16 uiDataLen,u8 *pucData)
{
	memcpy(RspPool,pucData,uiDataLen);
	RspLen=uiDataLen;
}

static void reply_reset(void)
{
	RspLen=0;
}
static int reply_get(int *result,u16 *dlen,u8 *dout)
{
	int tmpd;
	
	if(!RspLen)return 0;
	if(RspLen<4)return NET_ERR_IRDA_BUF;
	*result=RspPool[0]+(RspPool[1]<<8)+(RspPool[2]<<16)+(RspPool[3]<<24);
	if(dlen)*dlen=RspLen-4;
	if(dout)memcpy(dout,RspPool+4,RspLen-4);
	return 1;
}

int DhcpStart_Proxy(void)
{
	u8 tmpc;
	int tmpd,result;
	uint t0;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_DHCP_START,0,0,NULL);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0) return tmpd;
		if(tmpd>0)	break;

		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{			
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	if(!result)lan_state.dhcp_enable = 1;
	if(result<0)
	{
		return result;
	}
	return result;
}

void DhcpStop_Proxy(void)
{
	u8 tmpc;
	int tmpd,result;
	uint t0;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_DHCP_STOP,0,0,NULL);
	if(tmpc)return;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return;
		}
	}//while(1)
	
	if(!result)lan_state.dhcp_enable = 0;
	return;
}

int DhcpCheck_Proxy(void)
{
	u8 tmpc;
	int tmpd,result;
	uint t0;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_DHCP_CHECK,0,0,NULL);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return NET_ERR_IRDA_OFFBASE;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	return result;
}

int EthGet_Proxy(char *host_ip, char *host_mask,
				 char *gw_ip,char *dns_ip,long *state)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	u16 dn;
	int tmpd,result;
	uint t0;
	typedef struct 
	{
		u8   ip[20];
		u8   mask[20];
		u8   gateway[20];
		u8   dns[20];	
		u32  state;
	}ETH_CFG;
	ETH_CFG *eth_cfg;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_ETH_GET,0,0,NULL);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,&dn,tmps);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	if(dn<sizeof(ETH_CFG))return NET_ERR_IRDA_BUF;
	eth_cfg=(ETH_CFG *)tmps;

	if(state)*state=eth_cfg->state;
	if(host_ip)strcpy(host_ip,eth_cfg->ip);
	if(host_mask)strcpy(host_mask,eth_cfg->mask);
	if(gw_ip)strcpy(gw_ip,eth_cfg->gateway);
	if(dns_ip)strcpy(dns_ip,eth_cfg->dns);

	return result;
}

int EthSet_Proxy(char *host_ip, char *host_mask,
				 char *gw_ip,char *dns_ip)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	u16 dn;
	int tmpd,result;
	uint t0;

	if(host_ip==NULL && host_mask==NULL && gw_ip==NULL && dns_ip==NULL)
		return NET_ERR_VAL;
	
	if(host_ip && strlen(host_ip)>15)
		return NET_ERR_STR;
	if(host_mask && strlen(host_mask)>15)
		return NET_ERR_STR;
	if(gw_ip && strlen(gw_ip)>15)
		return NET_ERR_STR;
	if(dns_ip && strlen(dns_ip)>15)
		return NET_ERR_STR;
		
	if(lan_state.dhcp_enable)DhcpStop_Proxy();
	
	reply_reset();
	memset(tmps,0x00,sizeof(tmps));
	if(host_ip){tmps[0]|=0x01;strcpy(tmps+1,host_ip);}
	if(host_mask){tmps[0]|=0x02;strcpy(tmps+21,host_mask);}
	if(gw_ip){tmps[0]|=0x04;strcpy(tmps+41,gw_ip);}
	if(dns_ip){tmps[0]|=0x08;strcpy(tmps+61,dns_ip);}
	dn=81;

	tmpc=link_send(TM_LAN,F_ETH_SET,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	if(!result)memcpy(&eth_conf,tmps,sizeof(eth_conf));
	return result;
}

//store an integer in little endian format
void le_store(int d,u8 *s)
{
	s[0]=d&0xff;
	s[1]=(d>>8)&0xff;
	s[2]=(d>>16)&0xff;
	s[3]=(d>>24)&0xff;
}



//load an integer in little endian format
int le_load(u8 *s)
{
	int tmpd;
	
	tmpd=s[0]+(s[1]<<8)+(s[2]<<16)+(s[3]<<24);
	return tmpd;
}

int NetSocket_Proxy(int domain, int type, int protocol)
{
	u8 tmpc,tmps[100];
	u16 dn;
	int i,tmpd,result;
	uint t0;

	//--check if there is free socket
	for(i=0; i<MAX_SOCK_COUNT; i++)
		if(!lan_state.socket_opened[i])break;
	if(i>=MAX_SOCK_COUNT)return NET_ERR_MEM;

	le_store(domain,tmps);
	le_store(type,tmps+4);
	le_store(protocol,tmps+8);
	dn=12;
	reply_reset();
	tmpc=link_send(TM_LAN,F_NET_SOCKET,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	if(result<0)
	{
		return result;
	}
	if(result>=MAX_SOCK_COUNT)return NET_ERR_IRDA_BUF;

	i=result;
	lan_state.socket_opened[i]=1;
	lan_state.socket_status[i] = SOCK_BLOCK;
	lan_state.socket_timeout[i] = 20*1000;
	lan_state.socket_type[i] = type;
	if(type == NET_SOCK_STREAM)
	{
		lan_state.socket_conn[i] = -1;//Must Conn
	}else
	{
		lan_state.socket_conn[i] = 0;
	}
	return i;
}

int SockAddrSet_Proxy(struct net_sockaddr *addr, char *ip_str, unsigned short port)
{
	struct net_sockaddr_in *in = (struct net_sockaddr_in*)addr;
	u32 ip = 0;
	
	if(in == NULL)return NET_ERR_ARG;
	if(ip_str != NULL && strlen(ip_str)>MAX_IPV4_ADDR_STR_LEN)return NET_ERR_STR;
	
	memset(in, 0, sizeof(*in));
	in->sin_family=NET_AF_INET;
	in->sin_port=htons((unsigned short)port);
	if(ip_str)
	{
		ip = inet_addr(ip_str);
		if(ip==(u32)-1)return NET_ERR_VAL;
	}
	in->sin_addr.s_addr = ip;
	return 0;
}

int SockAddrGet_Proxy(struct net_sockaddr *addr,char *ip_str,unsigned short *port)
{
	struct net_sockaddr_in *in = (struct net_sockaddr_in*)addr;
	u32 ip;
	
	if(addr==NULL)return NET_ERR_ARG;
	ip = ntohl(in->sin_addr.s_addr);
	if(ip_str)
		sprintf(ip_str, "%d.%d.%d.%d",
			(ip>>24)&0xff,(ip>>16)&0xff,(ip>>8)&0xff,(ip)&0xff);
	if(port)*port = ntohs(in->sin_port);
	return 0;
}

int NetBind_Proxy(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	ushort dn;
	uint t0;

	if(socket < 0 || socket >= MAX_SOCK_COUNT || addr == NULL)
	{
		return NET_ERR_VAL;
	}
	if(lan_state.socket_type[socket] != NET_SOCK_STREAM)return NET_ERR_VAL;

	memset(tmps,0,sizeof(tmps));
	le_store(socket,tmps);
	dn = 4;
	memcpy(&tmps[4],(u8 *)addr,sizeof(struct net_sockaddr));
	dn += sizeof(struct net_sockaddr);
	reply_reset();
	tmpc=link_send(TM_LAN,F_NET_BIND,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	return result;
}

int NetListen_Proxy(int socket, int backlog)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	ushort dn;
	uint t0;

	if(socket < 0 || socket >= MAX_SOCK_COUNT || backlog <= 0 || backlog > 5)
	{
		return NET_ERR_VAL;
	}
	if(lan_state.socket_type[socket] != NET_SOCK_STREAM)return NET_ERR_VAL;

	memset(tmps,0,sizeof(tmps));
	le_store(socket,tmps);
	dn = 4;
	le_store(backlog,&tmps[4]);
	dn += 4;
	reply_reset();
	tmpc=link_send(TM_LAN,F_NET_LISTEN,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	return result;
}

int NetAccept_Proxy(int socket, struct net_sockaddr *addr, socklen_t *addrlen)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result,timeout;
	ushort dn;
	uint t0,t1;
	u32 ip;
	u16 port;
	struct net_sockaddr_in *in;
	
	if(socket < 0 || socket >= MAX_SOCK_COUNT || (!addr) || (!addrlen))
	{
		return NET_ERR_VAL;
	}
	if(lan_state.socket_type[socket] != NET_SOCK_STREAM)return NET_ERR_VAL;

	in = (struct net_sockaddr_in*)addr;
	
	timeout = lan_state.socket_timeout[socket];
	if(timeout)  
	{
		if(timeout<TO_REPLY)  
			timeout = TO_REPLY;
	}
	t1 = GetTimerCount();
	while(1)
	{
		
		memset(tmps,0,sizeof(tmps));
		le_store(socket,tmps);
		dn = 4;
		reply_reset();
		tmpc=link_send(TM_LAN,F_NET_ACCEPT,0,dn,tmps);
		if(tmpc)return NET_ERR_IRDA_SEND;
		
		t0=GetTimerCount();
		while(2)
		{
			tmpd=reply_get(&result,&dn,tmps);
			
			if(tmpd<0)return tmpd;
			if(tmpd>0)break;
			
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)
			{
				if(-10 == tmpd)
				{
					return NET_ERR_IRDA_OFFBASE;
				}
				else
				{
					return NET_ERR_IRDA_DISCONN;
				}
			}
			if(tmpd>0)t0=GetTimerCount();//reset timeout start
			
			if(GetTimerCount()-t0 > TO_REPLY)
			{
				return NET_ERR_IRDA_RECV;
			}
		}
		
		if(result >= 0) break;
		else if(result && NET_ERR_TIMEOUT != result)
		{
			return result;
		}

		if(GetTimerCount() - t1> timeout) return NET_ERR_TIMEOUT;
		
		DelayMs(10);
	}

	// 直接上报信息即可
	memcpy((unsigned char *)addr,tmps,sizeof(struct net_sockaddr));
//	memcpy((u8 *)&ip,tmps,4);
//	memcpy((u8 *)&port,&tmps[4],2);
//	in->sin_family = NET_AF_INET;
//	in->sin_port = htons((unsigned short)port);
//	in->sin_addr.s_addr = htonl(ip);
	*addrlen = sizeof(*in);
	lan_state.socket_opened[result]=1;
	lan_state.socket_status[result] = SOCK_BLOCK;
	lan_state.socket_timeout[result] = 20*1000;
	lan_state.socket_type[result] = NET_SOCK_STREAM;
	lan_state.socket_conn[result] = 0;
	return result;
}

int NetSendto_Proxy(int socket, void *buf, int size, int flags, 
					struct net_sockaddr *addr, socklen_t addrlen)
{
	return NET_ERR_SYS;//not supported
}

int NetRecvfrom_Proxy(int socket, void *buf, int size, int flags,
					  struct net_sockaddr *addr, socklen_t *addrlen)
{
	return NET_ERR_SYS;//not supported
}

int NetConnect_Proxy(int socket,struct net_sockaddr *addr,socklen_t addrlen)
{
	u8 tmpc,event,tmps[100];
	u16 dn;
	int i,tmpd,result;
	uint t0,timeout;
	
	if(socket<0||socket>=MAX_SOCK_COUNT)return NET_ERR_VAL;
	if(!lan_state.socket_status[socket])return NET_ERR_VAL;
	if(lan_state.socket_status[socket]<0)return NET_ERR_IRDA_SYN;

	le_store(socket,tmps);
	memcpy(tmps+4,addr,sizeof(struct net_sockaddr));
	le_store(addrlen,tmps+4+sizeof(struct net_sockaddr));
	dn=8+sizeof(struct net_sockaddr);
	reply_reset();
	tmpc=link_send(TM_LAN,F_NET_CONNECT,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	if(result && result!=NET_ERR_TIMEOUT)
	{
		return result;
	}
	if(lan_state.socket_status[socket]==SOCK_NOBLOCK)return 0;	

	timeout=lan_state.socket_timeout[socket];
	t0=GetTimerCount();
	while(2)
	{
		//if(lan_state.socket_status[socket]<0)
		//	return NET_ERR_IRDA_SYN;
		tmpc=link_send(TM_LAN,F_GET_EVENT,socket,0,tmps);
		if(tmpc)return NET_ERR_IRDA_SEND;

		event=tmps[socket];
		//if(event<0)return event;

		if(event&(SOCK_EVENT_CONN|SOCK_EVENT_WRITE|SOCK_EVENT_READ))
			break;

		if(event&SOCK_EVENT_ERROR)
		{
			//tmpc=irda_link_send(TM_LAN,F_GET_EVENT,socket,0,tmps);
			tmpd=Netioctl_Proxy(socket,CMD_EVENT_GET,SOCK_EVENT_ERROR);
			return tmpd;
		}
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_RECV;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start

		if(!timeout)continue;
		if(GetTimerCount()-t0 > timeout)return NET_ERR_TIMEOUT;
	}//while(2)

	lan_state.socket_conn[socket]=0;
	return 0;
}

int NetSend_Proxy(int socket, void *buf, int size, int flags)
{
	u8 tmpc;
	int timeout,tmpd;
	uint t0;

	if(buf==NULL)return NET_ERR_VAL;
	if(socket<0||socket>=MAX_SOCK_COUNT)return NET_ERR_VAL;
	if(!lan_state.socket_status[socket])return NET_ERR_VAL;
	if(lan_state.socket_status[socket]<0)return NET_ERR_IRDA_SYN;
	if(lan_state.socket_conn[socket]<0)return NET_ERR_CONN;
	if(lan_state.socket_type[socket] != NET_SOCK_STREAM)return NET_ERR_VAL;
	if(size>=LAN_IO_SIZE)return NET_ERR_BUF;

	timeout = lan_state.socket_timeout[socket];
	if(timeout)  
	{
		if(timeout<TO_REPLY)  
			timeout = TO_REPLY;
	}
	t0=GetTimerCount();

	while(1)
	{	
		tmpc=link_send(TM_LAN,F_NET_SEND,socket,size,buf);		
		if(tmpc)
		{
			if(lan_state.socket_status[socket]==SOCK_NOBLOCK) 
			{				
				if(tmpc==1) return NET_ERR_BUF;//tx pool overflow			
				return NET_ERR_IRDA_SEND;				
			}
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)
			{				
				if(-10 == tmpd)
				{
					return NET_ERR_IRDA_OFFBASE;
				}
				else
				{
					return NET_ERR_IRDA_DISCONN;
				}
			}
			if(tmpd>0) t0=GetTimerCount();//reset timeout start			

			if(!timeout)continue;			

			if(GetTimerCount()-t0<=timeout)continue;

			return NET_ERR_TIMEOUT;//timeout
		}
		else  break;	
	}
	return size;
}

int NetRecv_Proxy(int socket, void *buf, int size, int flags)
{
	u8 tmpc;
	int timeout,i,tmpd;
	uint t0;

	if(buf==NULL)return NET_ERR_VAL;
	if(socket<0||socket>=MAX_SOCK_COUNT||size<0)return NET_ERR_VAL;
	if(lan_state.socket_status[socket]==0)return NET_ERR_VAL;
	if(lan_state.socket_status[socket]<0)return NET_ERR_IRDA_SYN;
	if(lan_state.socket_conn[socket]<0)return NET_ERR_CONN;
	if(lan_state.socket_type[socket] != NET_SOCK_STREAM)return NET_ERR_VAL;

	timeout = lan_state.socket_timeout[socket];
	i=0;
	t0=GetTimerCount();
	while(i<size)
	{
		tmpc=link_send(TM_LAN,F_NET_RECV,socket,1,buf+i);
		if(tmpc)
		{
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)
			{
				if(i)break;
				if(-10 == tmpd)
				{
					return NET_ERR_IRDA_SEND;
				}
				else
				{
					return NET_ERR_IRDA_DISCONN;
				}
			}
			if(tmpd>0)t0=GetTimerCount();//reset timeout start

			if(lan_state.socket_status[socket]==SOCK_NOBLOCK)
				break;

			if(!timeout)continue;	
			if(i)break;

			if(GetTimerCount()-t0<=timeout)continue;

			return NET_ERR_TIMEOUT;//timeout
		}
		
		i++;
	}//while(i)

	return i;
}

int NetCloseSocket_Proxy(int socket)
{
	u8 tmpc,tmps[100];
	int tmpd,result;
	ushort dn;
	uint t0;

	if(socket<0||socket>=MAX_SOCK_COUNT)
		return NET_ERR_VAL;

	if(lan_state.socket_status[socket]<=0)
	{
		lan_state.socket_status[socket] = 0;
		return NET_ERR_VAL;
	}

	le_store(socket,tmps);
	dn=4;
	reply_reset();
	tmpc=link_send(TM_LAN,F_NET_CLOSE,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	lan_state.socket_status[socket]=0;
	lan_state.socket_opened[socket]=0;
	
	return result;
}

int RouteSetDefault_Proxy(int ifIndex)
{
	u8 tmpc,tmps[100];
	int tmpd,result;
	ushort dn;
	uint t0;

	le_store(ifIndex,tmps);
	dn=4;
	reply_reset();
	tmpc=link_send(TM_LAN,F_SET_ROUTE,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	if(!result)lan_state.default_route = ifIndex;
	return result;
}

int RouteGetDefault_Proxy(void)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	ushort dn;
	uint t0;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_GET_ROUTE,0,0,NULL);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,&dn,tmps);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	if(result)return result;

	if(dn<4)return NET_ERR_BUF;
	tmpd=le_load(tmps);
	return tmpd;
}	

int Netioctl_Proxy(int socket, int s_cmd, int arg)
{
	u8 tmpc,tmps[100];
	int tmpd,result;
	ushort dn;
	uint t0;

	if(s_cmd == CMD_IO_SET || s_cmd == CMD_IO_GET||	s_cmd==CMD_TO_SET  || 
	   s_cmd == CMD_TO_GET || s_cmd==CMD_EVENT_GET)
	{
		if(socket<0||socket>=MAX_SOCK_COUNT)return NET_ERR_VAL;

		if(s_cmd==CMD_EVENT_GET)
		switch(arg)
		{
		case 0:
			//--get rx_has count of the current socket
			tmpc=link_send(TM_LAN,F_GET_EVENT,socket,0,tmps);
			if(tmpc)return NET_ERR_IRDA_SEND;

			result=tmps[socket];
			tmpd=le_load(tmps+14);//rx_has
			if(tmpd>0)result|=SOCK_EVENT_READ;
				
			tmpd=le_load(tmps+10);//tx_free
			if(tmpd<=0)result&=~SOCK_EVENT_WRITE;
			
			if(result&SOCK_EVENT_WRITE)
				lan_state.socket_conn[socket]=0;
			return result;
		case SOCK_EVENT_READ:
			//--get rx_has count of the current socket
			tmpc=link_send(TM_LAN,F_GET_EVENT,socket,0,tmps);
			if(tmpc)return NET_ERR_IRDA_SEND;
				
			result=le_load(tmps+14);//rx_has
			return result;
		case SOCK_EVENT_WRITE:
			//--get tx_free count of the current socket
			tmpc=link_send(TM_LAN,F_GET_EVENT,socket,0,tmps);
			if(tmpc)return NET_ERR_IRDA_SEND;
			
			result=le_load(tmps+10);//tx_free
			return result;
 		case SOCK_EVENT_ACCEPT:
			if(IsSupportBaseStateExt())
 			{
	 			tmpc=link_send(TM_LAN,F_GET_EVENT,socket,0,tmps);
				if(tmpc)return NET_ERR_IRDA_SEND;

				result=le_load(tmps+18);
				return result;
			}
			else
			{
				return NET_ERR_ARG;
			}
 		case SOCK_EVENT_ERROR:
 			if(IsSupportBaseStateExt())
 			{
				tmpc=link_send(TM_LAN,F_GET_EVENT,socket,0,tmps);
				if(tmpc)return NET_ERR_IRDA_SEND;

				result=le_load(tmps+22);
				return result;
			}
			else
			{
				return NET_ERR_ARG;
			}
 		default:
			return NET_ERR_ARG;
		}//switch(arg)
		
		if(lan_state.socket_status[socket]<0)return NET_ERR_IRDA_SYN;
		if(lan_state.socket_status[socket]==0)return NET_ERR_VAL;

		switch(s_cmd)
		{
		case CMD_IO_SET:
			if(arg&NOBLOCK)lan_state.socket_status[socket]=SOCK_NOBLOCK;
			else lan_state.socket_status[socket]=SOCK_BLOCK;
			return 0;
		case CMD_IO_GET:
			return (lan_state.socket_status[socket]==SOCK_BLOCK)?0:NOBLOCK;
		case CMD_TO_SET:
			if(arg<=0)arg = 0;
			lan_state.socket_timeout[socket] = arg;
			break;
		case CMD_TO_GET:
			return lan_state.socket_timeout[socket];
		}//switch
	}//if()

	le_store(socket,tmps);
	le_store(s_cmd,tmps+4);
	le_store(arg,tmps+8);
	dn=12;
	reply_reset();
	tmpc=link_send(TM_LAN,F_NET_IOCTRL,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	return result;
}

int NetDevGet_Proxy(int Dev,char *HostIp,char *Mask,char *GateWay,char *Dns)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	ushort dn;
	uint t0;
	
	le_store(Dev,tmps);
	dn=4;
	reply_reset();
	tmpc=link_send(TM_LAN,F_GET_CFG,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,&dn,tmps);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	if(result)return result;
	if(dn<80)return NET_ERR_BUF;

	tmps[80]=0;//trim the string to avoid overflow
	if(HostIp)strcpy(HostIp,tmps);
	if(Mask)strcpy(Mask,tmps+20);
	if(GateWay)strcpy(GateWay,tmps+40);
	if(Dns)strcpy(Dns,tmps+60);

	return 0;
}

int DnsResolveExt_Proxy(char *name,char *ip_str,int len,int timeout)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result,namelen,org_len;
	ushort dn,i,delay_ms,dn_tx;
	uint t0,t1; 
	if(name == NULL || len<20 || name[0] == 0 || ip_str == NULL)
		return NET_ERR_VAL;
	if(str_check_max(name, DNS_NAME_SIZE)!=NET_OK)
		return NET_ERR_STR;

	if(timeout<1500)
		timeout = 1500;
	if(timeout>3600000)
		timeout = 3600000;
	ip_str[0]=0;

	memset(tmps,0,sizeof(tmps));
	namelen = strlen(name) + 1;
	memcpy(tmps,(unsigned char *)&namelen,4);
	dn_tx = 4;
	strcpy(&tmps[dn_tx],name);
	dn_tx += namelen;//including tail 0x00
	memcpy(&tmps[dn_tx],(unsigned char *)&len,4);
	dn_tx += 4;
	memcpy(&tmps[dn_tx],(unsigned char *)&timeout,4);
	dn_tx += 4;

	t1 = GetTimerCount();
	i = 0;
	delay_ms = 500;
	while(1)
	{
		if(!i) tmps[dn_tx+1] = 1;
		else tmps[dn_tx+1] = 2;
		dn_tx += 2;
		
		reply_reset();
		tmpc=link_send(TM_LAN,F_DNS_RESOLVE_EXT,0,dn_tx,tmps);
		if(tmpc)return NET_ERR_IRDA_SEND;
	
		t0=GetTimerCount();
		while(2)
		{
			tmpd=reply_get(&result,&dn,tmps);
			if(tmpd<0)return tmpd;
			if(tmpd>0)break;
		
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)
			{
				if(-10 == tmpd)
				{
					return NET_ERR_IRDA_OFFBASE;
				}
				else
				{
					return NET_ERR_IRDA_DISCONN;
				}
			}
			if(tmpd>0)
			{
				t0=GetTimerCount();//reset timeout start
				t1=t0;
			}
		
			if(GetTimerCount()-t0 > TO_REPLY)
			{
				result=NET_ERR_TIMEOUT;
				break;//return NET_ERR_TIMEOUT;
			}
		}

		if(!result)break;
		if(result && result != NET_ERR_IRDA_HANDLING && result != NET_ERR_TIMEOUT)return result;

		if(GetTimerCount() - t1 >= timeout)
		{
			return NET_ERR_TIMEOUT;
		}

		DelayMs(delay_ms);
		delay_ms <<= 1;
		if(delay_ms > 3000) delay_ms = 3000;

	}//while(1)
	org_len = len;
	if(len <= 16) len = 16;
	else
	{
		len = len/20 * 20;
		if(len > 200) len = 200;
	}
	if(dn < len) len = dn;

	memcpy(ip_str,tmps,len);
	if(org_len > 20 && org_len > len) ip_str[len] = 0;
	return 0;
}


int DnsResolve_Proxy(char *name,char *ip_str,int len)
{
#define DNS_NAME_SIZE 255/* the max limit */

	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	ushort dn,i,delay_ms;
	uint t0,t1;	
	if(name == NULL || len<16 || name[0] == 0)
		return NET_ERR_VAL;
	if(ip_str == NULL )
		return NET_ERR_VAL;
	if(str_check_max(name, DNS_NAME_SIZE)!=NET_OK)
		return NET_ERR_STR;
	ip_str[0]=0;
	i = 0;
	t1 = GetTimerCount();
	delay_ms = 500;
	while(1)
	{
		strcpy(tmps,name);
		
		dn=strlen(name);//including tail 0x00
		if(!i) tmps[dn+1] = 1;
		else tmps[dn+1] = 2;
		dn += 2; 
		
		reply_reset();
		tmpc=link_send(TM_LAN,F_DNS_RESOLVE,0,dn,tmps);
		if(tmpc)return NET_ERR_IRDA_SEND;
	
		t0=GetTimerCount();
		while(11)
		{
			tmpd=reply_get(&result,&dn,tmps);
			if(tmpd<0)return tmpd;
			if(tmpd>0)break;
		
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)
			{
				if(-10 == tmpd)
				{
					return NET_ERR_IRDA_OFFBASE;
				}
				else
				{
					return NET_ERR_IRDA_DISCONN;
				}
			}
			if(tmpd>0)
			{
				t0=GetTimerCount();//reset timeout start
				t1=t0;
			}
		
			if(GetTimerCount()-t0 > TO_REPLY)
			{
				result=NET_ERR_TIMEOUT;
				break;//return NET_ERR_TIMEOUT;
			}
		}//while(11)

		if(!result)break;
		if(result && result != NET_ERR_TIMEOUT)return result;

		if(GetTimerCount() - t1 > TO_REPLY)
		{
			return NET_ERR_TIMEOUT;
		}

		i++;

		DelayMs(delay_ms);
		delay_ms <<= 1;
		if(delay_ms > 3000) delay_ms = 3000;
	}//while(1)
	tmps[15] = 0;
	strcpy(ip_str,tmps);
	return 0;
}

int DhcpSetDns_Proxy(const char *addr)
{
    u32 dns;
    char *ptr;
    char buf[16];
    u32 len,tmp_len;
	u8 tmpc;
	int tmpd,result;
	uint t0;    
	u8 tmps[LINK_PAYLOAD];
	ushort dn_tx;    

    if(addr == NULL)
        return NET_ERR_VAL; 

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
    
	memset(tmps,0,sizeof(tmps));
	memcpy(tmps,(unsigned char *)&len,4);
	dn_tx = 4;
	memcpy(&tmps[dn_tx],buf,len);
	dn_tx += len;

	reply_reset();
    tmpc=link_send(TM_LAN,F_SET_DHCP_DNS,0,dn_tx,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return NET_ERR_IRDA_OFFBASE;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	return result;    
}

int NetPing_Proxy(char* dst_ip_str,long timeout,int size)
{
	u8 tmpc,tmps[100];
	int tmpd,result;
	ushort dn;
	uint t0,t1,t2,tt;
	u32  dst_ip;
	
	if(timeout<0||size<0)
		return NET_ERR_VAL;

	if(str_check_max(dst_ip_str, MAX_IPV4_ADDR_STR_LEN)!=NET_OK)
		return NET_ERR_STR;
	dst_ip = ntohl(inet_addr(dst_ip_str));

	if(dst_ip == (u32)-1)
	{
		return NET_ERR_ARG;
	}

	le_store(size,tmps);
	strcpy(tmps+4,dst_ip_str);
	dn=4+strlen(dst_ip_str)+1;
	reply_reset();
	tmpc=link_send(TM_LAN,F_NET_PING,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	t2=t0;
	while(1)
	{
		tmpd=reply_get(&result,&dn,tmps);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)
		{
			t0=GetTimerCount();//reset timeout start
			t2=t0;
		}
		
		if(GetTimerCount()-t0 > TO_REPLY)return NET_ERR_IRDA_RECV;
	}//while(1)
	
	if(!result)return 0;
	if(result<0 && result!=NET_ERR_TIMEOUT ) return result;

	//--get ICMP reply if not received above
	t1=GetTimerCount();
	if(timeout<=TO_REPLY)tt=TO_REPLY;
	else tt=timeout;
	while(2)
	{
		reply_reset();
		tmpc=link_send(TM_LAN,F_GET_PING_REPLY,0,0,NULL);
		if(tmpc)return NET_ERR_IRDA_SEND;
	
		t0=GetTimerCount();
		while(21)
		{
			tmpd=reply_get(&result,NULL,NULL);
			if(tmpd<0)return tmpd;
			if(tmpd>0)break;
		
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)
			{
				if(-10 == tmpd)
				{
					return NET_ERR_IRDA_OFFBASE;
				}
				else
				{
					return NET_ERR_IRDA_DISCONN;
				}
			}
			if(tmpd>0)
			{
				t0=GetTimerCount();//reset timeout start
				t1=t0;
				t2=t0;
			}
			
			if(GetTimerCount()-t0 > TO_REPLY)return NET_ERR_IRDA_RECV;
		}//while(21)

		if(result)break;

		if(GetTimerCount()-t1 > tt)return NET_ERR_TIMEOUT;
	}//while(2)

	return GetTimerCount()-t2;
}

int eth_mac_get_Proxy(unsigned char mac[6])
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	ushort dn;
	uint t0;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_GET_MAC,0,0,NULL);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,&dn,tmps);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	if(dn<6)return NET_ERR_BUF;

	memcpy(mac,tmps,6);
	return result;
}

int eth_mac_set_Proxy(unsigned char mac[6])
{
	u8 tmpc,tmps[100];
	int tmpd,result;
	ushort dn;
	uint t0;

	reply_reset();
	tmpc=link_send(TM_LAN,F_SET_MAC,0,6,mac);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	return result;
}

char net_version_get_Proxy(void)
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	ushort dn;
	uint t0;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_GET_VERSION,0,0,NULL);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,&dn,tmps);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	if(result)return result;
	if(dn<1)return NET_ERR_BUF;

	return tmps[0];
}

void NetGetVersion_Proxy(char ip_ver[30])
{
	u8 tmpc,tmps[LINK_PAYLOAD];
	int tmpd,result;
	ushort dn;
	uint t0;
	
	ip_ver[0]=0;

	reply_reset();
	tmpc=link_send(TM_LAN,F_GET_VERSIONS,0,0,NULL);
	if(tmpc)return;//NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,&dn,tmps);
		if(tmpd<0)return;//tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return;// NET_ERR_IRDA_OFFBASE;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return;// NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	if(result)return;
	
	strcpy(ip_ver,tmps);
	return;
}

void PPPLogout_Proxy(void)
{
	u8 tmpc;
	int tmpd,result;
	uint t0;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_PPP_LOGOUT,0,0,NULL);
	if(tmpc)return;//NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return;//tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return;//NET_ERR_IRDA_OFFBASE;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return;//NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	return;
}

int PPPLogin_Proxy(char *name,char *passwd,long auth,int timeout)
{
	u8 tmpc,tmps[300];
	int tmpd,result;
	ushort dn;
	uint t0,t1;

	if(strlen(name)>99 || strlen(passwd)>99)return NET_ERR_STR;	
	auth &= 0xff;

	le_store(auth,tmps);
	strcpy(tmps+4,name);
	strcpy(tmps+4+strlen(name)+1,passwd);
	dn=4+strlen(name)+strlen(passwd)+2;

	reply_reset();
	tmpc=link_send(TM_LAN,F_PPP_LOGIN,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)

	if(result || !timeout)return result;

	t0=GetTimerCount();
	while(2)
	{
    	reply_reset();
    	tmpc=link_send(TM_LAN,F_PPP_CHECK,0,0,NULL);
    	if(tmpc)return NET_ERR_IRDA_SEND;
    	
    	t1=GetTimerCount();
    	while(21)
    	{
    		tmpd=reply_get(&result,NULL,NULL);
    		if(tmpd<0)return tmpd;
    		if(tmpd>0)break;
    		
    		//--if off base,waiting or aborted according to ScrSetEcho()
    		tmpd=OnbaseCheck();
    		if(tmpd<0)
			{
				if(-10 == tmpd)
				{
					return NET_ERR_IRDA_OFFBASE;
				}
				else
				{
					return NET_ERR_IRDA_DISCONN;
				}
			}
    		if(tmpd>0)t1=GetTimerCount();//reset timeout start
    		
    		if(GetTimerCount()-t1 > TO_REPLY)
    		{
    			return NET_ERR_IRDA_RECV;
    		}
    	}//while(21)

    	if(result<=0)break;

    	if(GetTimerCount()-t0 > timeout)
    	{
    		PPPLogout_Proxy();
    		return NET_ERR_TIMEOUT;
    	}
	}//while(2)

	return result;
}

int PPPCheck_Proxy(void)
{
	u8 tmpc;
	int tmpd,result;
	uint t0;

	reply_reset();
	tmpc=link_send(TM_LAN,F_PPP_CHECK,0,0,NULL);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	return result;
}

void EthSetRateDuplexMode_Proxy(int mode)
{
	u8 tmpc,tmps[100];
	int tmpd,result;
	ushort dn;
	uint t0;

	le_store(mode,tmps);
	dn=4;
	reply_reset();
	tmpc=link_send(TM_LAN,F_SET_RATE_DUPLEX,0,dn,tmps);
	if(tmpc)return;//NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return;//tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return;//NET_ERR_IRDA_OFFBASE;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return;//NET_ERR_IRDA_RECV;
		}
	}//while(1)
	return;//result;
}

int NetAddStaticArp_Proxy(char *ip_str, unsigned char mac[6])
{
	u8 tmpc,tmps[100];
	int tmpd,result;
	ushort dn;
	uint t0;

	if(ip_str == NULL)
	{
		return NET_ERR_VAL;
	}
	dn = 0;
	memset(tmps,0,sizeof(tmps));
	memcpy(tmps,ip_str,20);
	dn = 20;
	memcpy(&tmps[dn],mac,6);
	dn += 6;
	
	reply_reset();
	tmpc=link_send(TM_LAN,F_ADD_STATIC_ARP,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,NULL,NULL);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return NET_ERR_IRDA_OFFBASE;
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	return result;
}

void NetSetIcmp_Proxy(unsigned long flag)
{
	return;
}

int PppoeLogin_Proxy(char *name, char *passwd, int timeout)
{
	return NET_ERR_SYS;
}

int PppoeCheck_Proxy(void)
{
	return NET_ERR_SYS;
}

void PppoeLogout_Proxy(void)
{
	return;
}

int EthGetFirstRouteMac_Proxy(const char *dest_ip, unsigned char *mac, int len)
{
	u8 tmpc,tmps[100];
	int tmpd,result;
	ushort dn;
	uint t0;
	u32 addr;
	
	if (!dest_ip) return NET_ERR_STR;
	if (len <= 0) return NET_ERR_VAL;	
	
	if(str_check_max(dest_ip, MAX_IPV4_ADDR_STR_LEN)!=NET_OK) return NET_ERR_STR;
	addr = ntohl(inet_addr(dest_ip));
	if(addr == 0 || addr==(u32)-1) return NET_ERR_VAL;	

	strcpy(tmps,dest_ip);
	le_store(len,tmps+16);
	dn=20;
	reply_reset();
	tmpc=link_send(TM_LAN,F_GET_FIRST_ROUTE_MAC,0,dn,tmps);
	if(tmpc)return NET_ERR_IRDA_SEND;
	
	t0=GetTimerCount();
	while(1)
	{
		tmpd=reply_get(&result,&dn,tmps);
		if(tmpd<0)return tmpd;
		if(tmpd>0)break;
		
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)
		{
			if(-10 == tmpd)
			{
				return NET_ERR_IRDA_OFFBASE;
			}
			else
			{
				return NET_ERR_IRDA_DISCONN;
			}
		}
		if(tmpd>0)t0=GetTimerCount();//reset timeout start
		
		if(GetTimerCount()-t0 > TO_REPLY)
		{
			return NET_ERR_IRDA_RECV;
		}
	}//while(1)
	
	if(!result)
	{
		if(len>6)dn=6;
		else dn=len;
		memcpy(mac,tmps,dn);
	}
	return result;
}

void s_initNet_Proxy(void)
{
	memset(&lan_state,0x00,sizeof(lan_state));
	memset(&eth_conf,0x00,sizeof(eth_conf));
}

ushort LanGetConfig(u8 *out_cfg)
{
	ushort i;

	i=0;

	memcpy(out_cfg,&eth_conf,sizeof(eth_conf));
	i+=sizeof(eth_conf);

	out_cfg[i]=lan_state.dhcp_enable;
	i++;

	out_cfg[i]=lan_state.route_config;
	i++;

	le_store(lan_state.default_route,out_cfg+i);
	i+=4;

	return i;
}


