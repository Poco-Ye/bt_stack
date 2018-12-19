#ifndef _WLANAPP_H
#define _WLANAPP_H
#include <socket.h>

#define WIFI_DEV_NONE		0
#define WIFI_DEV_RS9110		1
#define WIFI_DEV_CC3100		2
#define WIFI_DEV_AP6181     3

#define FILELINE	__FILE__, __LINE__
//#define DEBUG_WIFI
//#define DEBUG_CMD
#ifdef DEBUG_CMD
#define CMD_DEBUG_PRINT		
#else
#define CMD_DEBUG_PRINT     
#endif
#ifdef DEBUG_WIFI
#define WIFI_DEBUG_PRINT 	
#else
#define WIFI_DEBUG_PRINT          
#endif

#define BSS_TYPE_INFRA  1
#define BSS_TYPE_IBSS   2

//wifi
#define WLAN_SEC_UNSEC          (0)
#define WLAN_SEC_WEP	        (1)
#define WLAN_SEC_WPA_WPA2	    (2)
#define WLAN_SEC_WPAPSK_WPA2PSK	(3)
#define WLAN_SEC_WPA_PSK        (4) 
#define WLAN_SEC_WPA2_PSK       (5)

#define SOCKET_SEND_SIZE 1024
#define SOCKET_RECV_SIZE 1460

#define SOCKET_BUF_SIZE 10241
#define MAX_SOCKET 8
#define SOCKET_ROLE_LISTEN 1 
#define SOCKET_ROLE_CLIENT 0

#define MAX_UDP_SEND_SIZE (1024) //see API sheet


#define IPLEN 4
#define KEY_WEP_LEN_MAX 16
#define KEY_WEP_LEN_64 5
#define KEY_WEP_LEN_128 13
#define KEY_WEP_LEN_152 16

#define KEY_WPA_MAXLEN 63
#define SSID_MAXLEN 32
#define SCAN_AP_MAX 15
#define BSSID_LEN   6


typedef struct 
{ 
	uchar Key[4][KEY_WEP_LEN_MAX]; /* WEP 密码数据 */ 
	int KeyLen; /* WEP 密码长度 5 or 13 or 16*/ 
	int Idx; /* WEP密钥索引[0..3] */ 
} ST_WEP_KEY;

typedef struct
{
	int DhcpEnable;	//DHCP使能，0-关闭，1-开启
	uchar Ip[IPLEN];	//设置静态IP
	uchar Mask[IPLEN];	//掩码
	uchar Gate[IPLEN];	//网关
	uchar Dns[IPLEN];	//DNS
	ST_WEP_KEY Wep;	//wep密码
	uchar Wpa[KEY_WPA_MAXLEN];	//WPA连接密码
	uchar reserved[256];//预留
}ST_WIFI_PARAM;

typedef struct
{
	uchar Ssid[SSID_MAXLEN];		//搜索到的AP的SSID
	int SecMode;	//安全模式
	int Rssi;	//搜索到的AP的信号强度
}ST_WIFI_AP;

typedef struct 
{
	ST_WIFI_AP  stBasicInfo;
	unsigned char Bssid[6]; 
	unsigned int bssType;
    unsigned int channel; 
    unsigned char reserved[124]; //保留使用，注意在驱动中清零
} ST_WIFI_AP_EX;

struct ST_WIFIOPS
{
	//ip stack
	int (* NetSocket)(int,int,int);

	int (* NetConnect)(int,struct net_sockaddr *,socklen_t);

	int (* NetBind)(int,struct net_sockaddr *,socklen_t);

	int (* NetListen)(int,int);

	int (* NetAccept)(int,struct net_sockaddr *, socklen_t *);

	int (* NetSend)(int,void *,int,int);
	int (* NetSendto)(int,void *,int,int, 
		struct net_sockaddr *, socklen_t);

	int (* NetRecv)(int,void *,int,int);
	int (* NetRecvfrom)(int, void *,int, int, 
		struct net_sockaddr *, socklen_t *);
	int (* NetCloseSocket)(int);
	int (* Netioctl)(int,int,int);

	int (* SockAddrSet)(struct net_sockaddr *,char *,unsigned short);
	int (* SockAddrGet)(struct net_sockaddr *,char *,unsigned short *);

	int (* DnsResolve)(char *,char *,int);
    int (* DnsResolveExt)(char *,char *,int,int);
    
	int (* NetPing)(char *,long ,int);
	int (* RouteSetDefault)(int);
	int (* RouteGetDefault)(void);
	int (* NetDevGet)(int,char *,char *,char *,char *);

	//wifi
	int (* WifiOpen)(void);
	int (* WifiClose)(void);
	int (* WifiScan)(ST_WIFI_AP *, unsigned int);
    int (* WifiScanEx)(ST_WIFI_AP_EX *, unsigned int);
	int (* WifiConnect)(ST_WIFI_AP *,ST_WIFI_PARAM *);
	int (* WifiDisconnect)(void);
	int (* WifiCheck)(ST_WIFI_AP *);
	int (* WifiCtrl)(unsigned int,void *,unsigned int,void *,unsigned int);
	
	//内部使用
	int (* s_wifi_mac_get)(unsigned char *);
	void (* s_WifiPowerSwitch) (int);
	int (* WifiUpdateOpen)(void);
	void (* s_WifiInSleep)(void);
	void (* s_WifiOutSleep)(void);
	void (*WifiDrvInit)(void);
	void (*WifiUpdate)(void);
	int (*WifiGetHVer)(unsigned char *);
};

//ip stack
int WifiNetSocket(int domain, int type, int protocol);
int WifiNetConnect(int socket, struct net_sockaddr *addr, socklen_t addrlen);
int WifiNetBind(int socket, struct net_sockaddr *addr, socklen_t addrlen);
int WifiNetListen(int socket, int backlog);
int WifiNetAccept(int socket, struct net_sockaddr *addr, socklen_t *addrlen);
int WifiNetSend(int socket, void *buf/* IN */, int size, int flags);
int WifiNetSendto(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t addrlen);
int WifiNetRecv(int socket, void *buf/* OUT */, int size, int flags);
int WifiNetRecvfrom(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t *addrlen);
int WifiNetCloseSocket(int socket);
int WifiNetioctl(int socket, int cmd, int arg);
int WifiSockAddrSet(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */);
int WifiSockAddrGet(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */,unsigned short *port/* OUT */);
int WifiDnsResolve(char *name, char *result, int len);
int WifiDnsResolveExt(char *name, char *result, int len,int timeout);
int WifiNetPing(char *dst_ip_str, long timeout, int size);
int WifiRouteSetDefault(int ifIndex);
int WifiRouteGetDefault(void);
int WifiNetDevGet(int Dev,char *HostIp,char *Mask, char *GateWay,char *Dns);

//wifi;
int WifiOpen(void);
int WifiClose(void);
int WifiScanAps(ST_WIFI_AP *outAps, int ApCount);
int WifiConnectAp(ST_WIFI_AP *Ap,ST_WIFI_PARAM *WifiParam);
int WifiDisconAp(void);
int WifiCheck(ST_WIFI_AP *Ap);
int WifiIoCtrl(int iCmd, void *pArgIn, int iSizeIn,void *pArgOut,int iSizeOut);


void s_WifiInit(void);

//内部使用
int s_wifi_mac_get(unsigned char mac[6]);
void s_WifiPowerSwitch(int OnOff);
void TestWifi(void);
int WifiUpdateOpen(void);
void s_WifiInSleep(void);
void s_WifiOutSleep(void);
void WifiUpdate(void);

/***********WifiCtrl CMD *******************/
typedef enum{
	WIFI_CTRL_ENTER_SLEEP = 0,
	WIFI_CTRL_OUT_SLEEP,
	WIFI_CTRL_GET_PARAMETER,
	WIFI_CTRL_GET_MAC,
	WIFI_CTRL_GET_BSSID,
	WIFI_CTRL_SET_DNS_ADDR,
	/*此处添加CMD*/
	WIFI_CTRL_CMD_MAX_INDEX
}WIFI_CTRL_CMD;

/********ERROR CODE********/
#define WIFI_RET_OK 0
#define WIFI_RET_ERROR_NODEV -1
#define WIFI_RET_ERROR_NORESPONSE -2
#define WIFI_RET_ERROR_NOTOPEN -3
#define WIFI_RET_ERROR_NOTCONN -4
#define WIFI_RET_ERROR_NULL -5
#define WIFI_RET_ERROR_PARAMERROR -6
#define WIFI_RET_ERROR_STATUSERROR -7
#define WIFI_RET_ERROR_DRIVER	-8
/*密码认证错误*/
#define WIFI_RET_ERROR_AUTHERROR -9 
/*扫描AP异常*/
#define WIFI_RET_ERROR_NOAP -10
/*设置或获取IP失败*/
#define WIFI_RET_ERROR_IPCONF -11
#define WIFI_RET_ERROR_ROUTE    -12
#define WIFI_RET_ERROR_NOTSUPPORT	-13
#define WIFI_RET_ERROR_FORCE_STOP -15
#define WIFI_RET_ERROR_RESPOND	WIFI_RET_ERROR_DRIVER
#define WIFI_ROUTE_NUM	12
#endif
