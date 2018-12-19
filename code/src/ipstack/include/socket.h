/*
 *  BSD Compatibility Socket API
修改历史
080624 sunJH:
SockAddrSet和SockAddrGet中第三个参数由short变为unsigned short
 */
#ifndef _SOCKET_H_
#define _SOCKET_H_

#ifdef  __cplusplus
extern "C" {
#endif

#define NET_AF_INET 1
enum
{
	NET_SOCK_STREAM=1,/* TCP */
	NET_SOCK_DGRAM,/* UDP */
};

struct net_in_addr 
{
	unsigned long s_addr;
};

struct net_sockaddr_in 
{
         char  sin_len;
         char  sin_family;
         short sin_port;
         struct net_in_addr sin_addr;
         char sin_zero[8];
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif
;

struct net_sockaddr
{
         char sa_len;
         char sa_family;
         char sa_data[14];
}
#ifdef WIN32
#pragma warning(disable:4103)
#pragma pack(1)
#elif !defined(CC_NO_PACKED)
__attribute__((__packed__))
#endif
;

#ifndef socklen_t
#define socklen_t int
#endif

/*
 * Create a NetSocket handle
 * Param:
 *   domain    : always NET_AF_INET
 *   type        : NET_SOCK_STREAM or NET_SOCK_DGRAM
 *   protocol   : always 0
 * succeed, return >=0;
 * fail, return <0
 */
int NetSocket_std(int domain, int type, int protocol);

int NetConnect_std(int socket, struct net_sockaddr *addr, socklen_t addrlen);

int NetBind_std(int socket, struct net_sockaddr *addr, socklen_t addrlen);

int NetListen_std(int socket, int backlog);

int NetAccept_std(int socket, struct net_sockaddr *addr, socklen_t *addrlen);

int NetSend_std(int socket, void *buf/* IN */, int size, int flags);
int NetSendto_std(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t addrlen);

int NetRecv_std(int socket, void *buf/* OUT */, int size, int flags);
int NetRecvfrom_std(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t *addrlen);

int NetCloseSocket_std(int socket);

/*********************************************************
 *     从下面开始为私有接口，与bsd不兼容   *
 *********************************************************/
int NetCloseSocket(int socket);

#define NOBLOCK 1
#define SOCK_EVENT_READ    (1<<0)
#define SOCK_EVENT_WRITE   (1<<1)
#define SOCK_EVENT_CONN    (1<<2)
#define SOCK_EVENT_ACCEPT  (1<<3)
#define SOCK_EVENT_ERROR   (1<<4)
#define SOCK_EVENT_MASK    (0xff)
enum
{
	CMD_IO_SET=1,
	CMD_IO_GET,
	CMD_TO_SET,/* timeout set */
	CMD_TO_GET,/* timeout get */
	CMD_IF_SET,/* NetSocket bind dev IF (interface)*/
	CMD_IF_GET,/* get the dev if NetSocket bind */
	CMD_EVENT_GET,/* get event , such as SOCK_EVENT_READ,SOCK_EVENT_WRITE, etc*/
	CMD_BUF_GET,/* get send or recv buffer , only for NET_SOCK_STREAM */
	CMD_FD_GET,/* get free NetSocket fd num */
	CMD_KEEPALIVE_SET,/* KeepAlive Set */
	CMD_KEEPALIVE_GET,/* KeepAlive Get */
	CMD_SEND_FIN,/* 判断发送数据是否已经完成,为P60增加的*/
	CMD_GET_PCB,/* 获取Socket对应的TCP PCB, 为P60增加的*/
};

/* arg for CMD_BUF_GET */
#define TCP_SND_BUF_MAX 1
#define TCP_RCV_BUF_MAX 2
#define TCP_SND_BUF_AVAIL 3

/*
 *  SOCKET ioctl
 *
 *      cmd                          arg   
 *   -----------------------------------------------------
 *   CMD_IO_SET               NOBLOCK  or 0
 *   CMD_IO_GET                 0
 *   CMD_TO_SET                time(in milliseconds, <=0 wait forever )
 *   CMD_TO_GET                0
 *
 *  Return Value:
 *   when cmd is CMD_IO_SET or CMD_TO_SET, succeed return 0, fail return <0;
 *   when cmd is CMD_IO_GET 
 *          return value >0, means IO NO BLOCK;
 *          return value=0, means IO BLOCK;
 *   when cmd is CMD_TO_GET
 *          return value <=0, means wait forever
 *          return value > 0, means waiting time when NetSocket is BLOCK;
 */
int Netioctl_std(int socket, int cmd, int arg);

int SockAddrSet_std(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */);
int SockAddrGet_std(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */, unsigned short *port/* OUT */);
void EthSetRateDuplexMode_std(int mode);

int router_type(int s, struct net_sockaddr *addr, socklen_t addrlen);
int tcp_peer_opened(int s);

#ifdef  __cplusplus
}
#endif

#endif/* _SOCKET_H_ */

