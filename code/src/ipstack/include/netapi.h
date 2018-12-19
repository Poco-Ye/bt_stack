/*
修改历史:
080624 sunJH:
1.增加错误码NET_ERR_STR表示字符串太长，非法
2.增加错误码NET_ERR_DNS表示DNS解析失败
080717 sunJH
1.增加了一些NET错误
2.增加了无线错误（用于无线模块）
080722 sunJH
增加了WL_RET_ERR_POWEROFF表示未上电错误
080729 sunJH
增加网络出口编号
080917 sunJH
增加NetDevGet接口定义
081106 sunJH
增加S60 专用的错误码50~70
090514 sunJH
增加NetAddStaticArp、NetSetIcmp
090824 sunJH
增加了pppoe api和相关错误码
*/
#ifndef _NET_API_H_
#define _NET_API_H_

#ifdef  __cplusplus
extern "C" {
#endif

/**************Error Code ***********************/
#define NET_OK    0      /* No error, everything OK. */
#define NET_ERR_MEM  -1      /* Out of memory error.     */
#define NET_ERR_BUF  -2      /* Buffer error.            */

#define NET_ERR_ABRT -3      /* Connection aborted.      */
#define NET_ERR_RST  -4      /* Connection reset.        */
#define NET_ERR_CLSD -5      /* Connection closed.       */
#define NET_ERR_CONN -6      /* Not connected.           */

#define NET_ERR_VAL  -7      /* Illegal value.           */

#define NET_ERR_ARG  -8      /* Illegal argument.        */

#define NET_ERR_RTE  -9      /* Routing problem.         */

#define NET_ERR_USE  -10     /* Address in use.          */

#define NET_ERR_IF   -11     /* Low-level netif error    */
#define NET_ERR_ISCONN -12   /* Already connected.       */

#define NET_ERR_TIMEOUT -13  /* time out */

#define NET_ERR_AGAIN  -14   /*no block, try again */
#define NET_ERR_EXIST  -15   /* exist already */
#define NET_ERR_SYS    -16   /* sys don not support */

/* PPP ERROR code */
#define NET_ERR_PASSWD -17 /* error password */
#define NET_ERR_MODEM  -18 /* modem dial is fail */
#define NET_ERR_LINKDOWN -19/* DataLink Down */
#define NET_ERR_LOGOUT   -20/* User Logout */
#define NET_ERR_PPP      -21/* PPP Link Down*/

#define NET_ERR_STR     -22 /* String Too Long, Illeage */
#define NET_ERR_DNS     -23 /* DNS Failure: No such Name */
#define NET_ERR_INIT    -24 /* No Init */
#define NET_ERR_NEED_START_DHCP    -25 /*未启动DHCP*/
/* PPPoE ERROR code */
#define NET_ERR_SERV    -30/* Can not find PPPoE Service */

/* B210 专用的错误码50~70 */
#define NET_ERR_IRDA_SYN   -51 /* 座机重启或放置在不同的座机上*/
#define NET_ERR_IRDA_INDEX -52/*Mismatched function_no of baseset reply packet*/
#define NET_ERR_IRDA_BUF -53/* */
#define NET_ERR_IRDA_SEND -54/*fail to send request to baseset*/
#define NET_ERR_IRDA_RECV -55/*fail to receive reply from baseset*/
#define NET_ERR_IRDA_OFFBASE -56/* off base while receiving*/
#define NET_ERR_IRDA_BUSY	-57			// 协议栈忙
#define NET_ERR_IRDA_DISCONN -58		// 蓝牙未连接
#define NET_ERR_IRDA_HANDLING -59		// 协议栈正在处理中


/* Wireless Net Error Code */
#define WL_RET_ERR_PORTINUSE	-201	/*  Port Using       */
#define WL_RET_ERR_NORSP	    -202	/*  No Response       */
#define WL_RET_ERR_RSPERR	    -203  	/*  Error Response    */
#define WL_RET_ERR_PORTNOOPEN   -204	/*  No Open   */
#define WL_RET_ERR_NEEDPIN	    -205	/*  Need Pin         */
#define WL_RET_ERR_NEEDPUK	    -206	/*  Need PUK     */
#define WL_RET_ERR_PARAMER	    -207	/*  Param Error          */
#define WL_RET_ERR_ERRPIN	    -208	/*  Password Error       */
#define WL_RET_ERR_NOSIM	    -209	/*  SIM must be inserted */
#define WL_RET_ERR_NOTYPE	    -210	
#define WL_RET_ERR_NOREG	    -211	
#define WL_RET_ERR_LINEOFF	    -214	
#define WL_RET_ERR_TIMEOUT	    -216	
#define WL_RET_ERR_REGING	    -222	
#define WL_RET_ERR_PORTCLOSE	-223	
#define WL_RET_ERR_MODEVER	    -225	
#define WL_RET_ERR_DIALING       -226    /*  Dialing     */
#define WL_RET_ERR_ONHOOK        -227    /*  On Hook   */
#define WL_RET_ERR_RST           -228    /*  Module Reset   */
#define WL_RET_ERR_NOSIG         -229    /* Signal level is too  low */
#define WL_RET_ERR_POWEROFF      -230    /* Power Off */
#define WL_RET_ERR_OTHER	    -300	
#define WL_RET_ERR_SOFT_RESET	-231


#include "socket.h"

/**************************************
 *                   DHCP API         
 ***************************************/
int DhcpStart_std(void);
void DhcpStop_std(void);
int DhcpCheck_std(void);

/*************************************
 *                      PPP API
 *************************************/
/* 认证算法定义宏*/
#define PPP_ALG_PAP       0x1 /* PAP */
#define PPP_ALG_CHAP      0x2 /* CHAP */
#define PPP_ALG_MSCHAPV1  0x4 /* MS-CHAPV1 */
#define PPP_ALG_MSCHAPV2  0x8 /* MS-CHAPV2 */
int PPPLogin_std(char *name, char *passwd, long auth, int timeout);
void PPPLogout_std(void);
int PPPCheck_std(void);

/**************************************
 *                    Ethernet Dev Config API
 **************************************/
int eth_mac_set_std(unsigned char mac[6]);
int eth_mac_get_std(unsigned char mac[6]);
int EthSet(
		char *host_ip, char *host_mask,
		char *gw_ip,
		char *dns_ip
		);
int EthGet_std(
		char *host_ip, char *host_mask,
		char *gw_ip,
		char *dns_ip,
		long *state
		);

/**************************************
 *                    DNS API
 **************************************/
int DnsResolve_std(char *name/*IN*/, char *result/*OUT*/, int len);
int DnsResolveExt_std(char *name/*IN*/, char *result/*OUT*/, int len, int timeout);
/**************************************
 *                    ICMP PING API
 **************************************/
int NetPing_std(char* dst_ip_str, long timeout, int size);

/**************************************
 *                    Route API
 **************************************/
int RouteSetDefault_std(int ifIndex);
int RouteGetDefault_std(void);

int NetDevGet_std(int Dev,/* Dev Interface Index */ 
		char *HostIp,
		char *Mask,
		char *GateWay,
		char *Dns
		);
/*
** NetAddStaticArp 增加静态ARP表项
** ip   ipaddress
** mac  the host's mac address
** if success, return 0; otherwise, return < 0;
**/
int NetAddStaticArp_std(char *ip_str, unsigned char mac[6]);

/*
**  NetSetIcmp 设置ICMP开关
**  flag   bit0为1时  能够响应其他机器的ping请求
**         bit0为0时  关闭响应其他机器的ping请求
**/
void NetSetIcmp_std(unsigned long flag);

/*
** 获取网络模块版本号
**/
char net_version_get_std(void);

/* 网络出口编号*/
#define NET_IF_ETH      0 /* Ethernet Interface */
#define NET_IF_MODEM    10/* Modem PPP Interface */
#define NET_IF_WL       11/* Wireless PPP Interface*/

int PppoeLogin_std(char *name, char *passwd, int timeout);
int PppoeCheck_std(void);
void PppoeLogout_std(void);
void EthSetRateDuplexMode_std(int mode);

#ifdef  __cplusplus
}
#endif

#endif/* _NET_API_H_ */

