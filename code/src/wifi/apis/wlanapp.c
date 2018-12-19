#include <base.h>
#include "wlanapp.h"
#include "../../ipstack/include/netapi.h"
#include "../../comm/comm.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

extern struct ST_WIFIOPS gszRpWifiOps;
extern struct ST_WIFIOPS gszBrcmWifiOps;
struct ST_WIFIOPS *gptWifiOps=NULL;
uchar WifiSocketFifo[MAX_SOCKET*SOCKET_BUF_SIZE];//for all wifi modules
static uchar gucWifiMacGetFlag = 0;
static uchar gucWifiMac[6];
static uchar gucWifiHVer[32] = {0,};
static int gIsWifiOpened = 0;

int s_wifi_mac_get(unsigned char mac[6]);
int WifiGetHVer(uchar * WifiFwVer);
int WifiOpen(void);
int WifiClose(void);


void s_WifiGetInfo(void)
{
	int iRet;
	WifiOpen();
	iRet = s_wifi_mac_get(gucWifiMac);
	if (iRet == 0) gucWifiMacGetFlag = 1;
	WifiGetHVer(gucWifiHVer);
	WifiClose();
}
void s_WifiGetMac(uchar Mac[6])
{
	if (gucWifiMacGetFlag != 0)
	{
		memcpy(Mac, gucWifiMac, sizeof(gucWifiMac));
	}
	else
	{
		s_WifiGetInfo();
		memcpy(Mac, gucWifiMac, sizeof(gucWifiMac));
	}
}

void s_ClsWifiVer(void)
{
	memset(gucWifiHVer, 0, sizeof(gucWifiHVer));
}
void s_WifiGetHVer(uchar *WifiHVer)
{
	if (gucWifiHVer[0] != 0)
	{
		memset(WifiHVer, 0, sizeof(gucWifiHVer));
		memcpy(WifiHVer, gucWifiHVer, sizeof(gucWifiHVer));
	}
	else
	{
		s_WifiGetInfo();
		memset(WifiHVer, 0, sizeof(gucWifiHVer));
		memcpy(WifiHVer, gucWifiHVer, sizeof(gucWifiHVer));
	}
}

int get_wifi_type(void)
{
	char context[64];
	static int wifiType =-1;
    if(wifiType >= 0) return wifiType;
	
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("WIFI_NET",context)<0) wifiType = WIFI_DEV_NONE;
	else if (strstr(context, "RS9110-N-11-24")) wifiType = WIFI_DEV_RS9110;
    else if (strstr(context, "03")) wifiType = WIFI_DEV_AP6181;

    return wifiType;
}


void s_WifiInit(void)
{
	int type;

	type = get_wifi_type();
	switch(type)
	{
	case WIFI_DEV_RS9110:	
		gptWifiOps = &gszRpWifiOps;
		break;

    case WIFI_DEV_AP6181:
        gptWifiOps = &gszBrcmWifiOps;
        break;

	default:
		gptWifiOps = NULL;
		break;
	}

	if (gptWifiOps)
	{
		gptWifiOps->WifiDrvInit();	
	}
}

//ip stack
int WifiNetSocket(int domain, int type, int protocol)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetSocket(domain, type, protocol);
}
int WifiNetConnect(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetConnect(socket, addr, addrlen);
}
int WifiNetBind(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetBind(socket, addr, addrlen);
}
int WifiNetListen(int socket, int backlog)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetListen(socket, backlog);
}
int WifiNetAccept(int socket, struct net_sockaddr *addr, socklen_t *addrlen)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetAccept(socket, addr, addrlen);
}
int WifiNetSend(int socket, void *buf/* IN */, int size, int flags)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetSend(socket, buf, size, flags);
}
int WifiNetSendto(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t addrlen)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetSendto(socket, buf, size, flags, addr, addrlen);
}
int WifiNetRecv(int socket, void *buf/* OUT */, int size, int flags)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetRecv(socket, buf, size, flags);
}
int WifiNetRecvfrom(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t *addrlen)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetRecvfrom(socket, buf, size, flags, addr, addrlen);
}
int WifiNetCloseSocket(int socket)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetCloseSocket(socket);
}
int WifiNetioctl(int socket, int cmd, int arg)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->Netioctl(socket, cmd, arg);
}
int WifiSockAddrSet(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->SockAddrSet(addr, ip_str, port);
}
int WifiSockAddrGet(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */,unsigned short *port/* OUT */)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->SockAddrGet(addr, ip_str, port);
}
int WifiDnsResolve(char *name, char *result, int len)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->DnsResolve(name, result, len);
}

int WifiDnsResolveExt(char *name, char *result, int len,int timeout)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->DnsResolveExt(name, result, len, timeout);
}

int WifiNetPing(char *dst_ip_str, long timeout, int size)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetPing(dst_ip_str, timeout, size);
}
int WifiRouteSetDefault(int ifIndex)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->RouteSetDefault(ifIndex);
}
int WifiRouteGetDefault(void)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->RouteGetDefault();
}
int WifiNetDevGet(int Dev,char *HostIp,char *Mask, char *GateWay,char *Dns)
{
	if (!gptWifiOps)		
		return NET_ERR_IF;
	
	return gptWifiOps->NetDevGet(Dev, HostIp, Mask, GateWay, Dns);
}

int s_IsWifiOpened(void)
{
	return gIsWifiOpened ? 1 : 0;
}


//wifi
int WifiOpen(void)
{
	int ret;
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	ret = gptWifiOps->WifiOpen();
	if(ret == WIFI_RET_OK) gIsWifiOpened = 1;
	return ret;
}
int WifiClose(void)
{
	int ret;
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;

	ret = gptWifiOps->WifiClose();
	if(ret == WIFI_RET_OK)gIsWifiOpened = 0;
	return ret;
}
int WifiScan (ST_WIFI_AP *outAps, unsigned int ApCount)
{
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->WifiScan(outAps, ApCount);
}
int WifiScanEx(ST_WIFI_AP_EX *outAps, unsigned int apCount)
{
    if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->WifiScanEx(outAps, apCount);
}
int WifiConnect(ST_WIFI_AP *Ap,ST_WIFI_PARAM *WifiParam)
{
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->WifiConnect(Ap, WifiParam);
}
int WifiDisconnect(void)
{
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->WifiDisconnect();
}
int WifiCheck(ST_WIFI_AP *Ap)
{
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->WifiCheck(Ap);
}
int WifiCtrl(unsigned int iCmd, void *pArgIn, unsigned int iSizeIn ,void* pArgOut, unsigned int iSizeOut)
{
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->WifiCtrl(iCmd, pArgIn, iSizeIn,pArgOut,iSizeOut);
}
//内部使用
int s_wifi_mac_get(unsigned char mac[6])
{
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->s_wifi_mac_get(mac);
}
void s_WifiPowerSwitch(int OnOff)
{
	if (!gptWifiOps)		
		return;
	
	gptWifiOps->s_WifiPowerSwitch(OnOff);
}

int WifiGetHVer(uchar * WifiFwVer)
{
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->WifiGetHVer(WifiFwVer);
}

int WifiUpdateOpen(void)
{
	if (!gptWifiOps)		
		return WIFI_RET_ERROR_NODEV;
	
	return gptWifiOps->WifiUpdateOpen();
}
void s_WifiInSleep(void)
{
	if (!gptWifiOps)		
		return;
	
	gptWifiOps->s_WifiInSleep();
}
void s_WifiOutSleep(void)
{
	if (!gptWifiOps)		
		return;
	
	gptWifiOps->s_WifiOutSleep();
}


void WifiUpdate(void)
{
	if (!gptWifiOps)		
		return;
	
	gptWifiOps->WifiUpdate();
}

int WifiRssiToSignal(int rssi)
{
	if (rssi < -75)
	{
		return 1;
	}
	else if (rssi >= -65)
	{
		return 3;
	}
	else
	{
		return 2;
	}
}

void NoWifiFun(void)
{
}
int NoDebugFun(char * fmt,...)
{
	
}


