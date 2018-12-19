#ifndef _APP_API_H_
#define _APP_API_H_
#include "inet/inet.h"
#include "netapi.h"
#include "socket.h"

int eth_mac_set_Proxy (unsigned char mac[6]);
int eth_mac_get_Proxy (unsigned char mac[6]);
int EthSet_Proxy(char *host_ip, char *host_mask,char *gw_ip,char *dns_ip);
int EthGet_Proxy(char *host_ip, char *host_mask,char *gw_ip,	char *dns_ip,long *state);
int DhcpStart_Proxy(void);
void DhcpStop_Proxy(void);
int DhcpCheck_Proxy(void);
int RouteSetDefault_Proxy(int ifIndex);
int RouteGetDefault_Proxy(void);
int DnsResolve_Proxy(char *name/*IN*/, char *result/*OUT*/, int len);
int DnsResolveExt_Proxy(char *name/*IN*/, char *result/*OUT*/, int len,int timeout);
int DhcpSetDns_Proxy(const char *addr);
int NetPing_Proxy(char* dst_ip_str, long timeout, int size);
int NetDevGet_Proxy(int Dev,char *HostIp,char *Mask,char *GateWay,char *Dns);/* Dev Interface Index */ 
void NetGetVersion_Proxy(char ip_ver[30]);
char net_version_get_Proxy(void);
int NetSocket_Proxy(int domain, int type, int protocol);
int Netioctl_Proxy(int socket, int s_cmd, int arg);
int NetConnect_Proxy(int socket, struct net_sockaddr *addr, socklen_t addrlen);
int NetBind_Proxy(int socket, struct net_sockaddr *addr, socklen_t addrlen);
int NetListen_Proxy(int socket, int backlog);
int NetAccept_Proxy(int socket, struct net_sockaddr *addr, socklen_t *addrlen);
int NetSend_Proxy(int socket, void *buf/* IN */, int size, int flags);
int NetSendto_Proxy(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t addrlen);
int NetRecv_Proxy(int socket, void *buf/* OUT */, int size, int flags);
int NetRecvfrom_Proxy(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t *addrlen);
int NetCloseSocket_Proxy(int socket);
int SockAddrSet_Proxy(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */);
int SockAddrGet_Proxy(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */, unsigned short *port/* OUT */);

#endif/* _APP_API_H_ */
