#ifndef _RPWLANAPP_H
#define _RPWLANAPP_H
#include <socket.h>


#define BLOCK 0
#define SOCKET_TIMEOUT 20000


#define PORT_TYPE_TCPCLIENT 0
#define PORT_TYPE_TCPSERVER 1
#define PORT_TYPE_UDP		  2
#define CMDFAIL_MAX_CNT 10//AT指令最大失败次数

int RpNetSocket(int domain, int type, int protocol);

int RpNetConnect(int socket, struct net_sockaddr *addr, socklen_t addrlen);

int RpNetBind(int socket, struct net_sockaddr *addr, socklen_t addrlen);

int RpNetListen(int socket, int backlog);

int RpNetAccept(int socket, struct net_sockaddr *addr, socklen_t *addrlen);

int RpNetSend(int socket, void *buf/* IN */, int size, int flags);
int RpNetSendto(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t addrlen);

int RpNetRecv(int socket, void *buf/* OUT */, int size, int flags);
int RpNetRecvfrom(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t *addrlen);


int RpNetCloseSocket(int socket);

int RpNetioctl(int socket, int cmd, int arg);

int RpSockAddrSet(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */);
int RpSockAddrGet(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */,unsigned short *port/* OUT */);

int RpDnsResolve(char *name, char *result, int len);
int RpNetPing(char *dst_ip_str, long timeout, int size);
int RpRouteSetDefault(int ifIndex);
int RpRouteGetDefault(void);
int RpNetDevGet(int Dev,char *HostIp,char *Mask, char *GateWay,char *Dns);


int s_rpwifi_mac_get(unsigned char mac[6]);
int s_RpNetCloseAllSocket(void);
void s_RpSetErrorCode(uchar error);
uchar s_RpGetErrorCode(void);
void s_RpPortInit(void);
#endif
