
#ifndef _SYS_COMBIN_API_H_
#define _SYS_COMBIN_API_H_

#include "../ipstack/include/socket.h"

/*modem part*/
extern void s_ModemInfo_Proxy(uchar * drv_ver, uchar * mdm_name, uchar * last_make_date, uchar * others);
extern uchar s_ModemInit_Proxy(uchar mode);
extern uchar ModemReset_Proxy(void);
extern uchar ModemDial_Proxy(COMM_PARA * Para, uchar * TelNo, uchar mode);
extern uchar ModemCheck_Proxy(void);
extern uchar ModemTxd_Proxy(uchar * SendData, ushort len);
extern uchar ModemRxd_Proxy(uchar * RecvData, ushort * len);
extern uchar OnHook_Proxy(void);
extern uchar ModemAsyncGet_Proxy(uchar * ch);
extern ushort ModemExCommand_Proxy(uchar * CmdStr, uchar * RespData, ushort * Dlen, ushort Timeout10ms);
extern int PPPLogin_Proxy(char *name, char *passwd, long auth, int timeout);
extern int PPPCheck_Proxy(void);
extern void PPPLogout_Proxy(void);

/*comm part*/
extern uchar PortOpen_Proxy(uchar channel, uchar * Attr);
extern uchar PortClose_Proxy(uchar channel);
extern uchar PortSend_Proxy(uchar channel, uchar ch);
extern uchar PortRecv_Proxy(uchar channel, uchar * ch, uint ms);
extern uchar PortReset_Proxy(uchar channel);
extern uchar PortSends_Proxy(uchar channel, uchar * str, ushort str_len);
extern uchar PortTxPoolCheck_Proxy(uchar channel);
extern int PortRecvs_Proxy(uchar channel, uchar * pszBuf, ushort usRcvLen, ushort usTimeOut);
extern int PortPeep_Proxy(uchar channel, uchar * buf, ushort want_len);
/*printer part*/
extern void  s_PrnInit_Proxy(void);
extern uchar PrnInit_Proxy(void);
extern void  PrnFontSet_Proxy(uchar Ascii, uchar Locale);
extern void  PrnSpaceSet_Proxy(uchar x, uchar y);
extern void  PrnLeftIndent_Proxy(ushort x);
extern void  PrnStep_Proxy(short pixel);
extern uchar s_PrnStr_Proxy(uchar * str);
extern uchar PrnLogo_Proxy(uchar * logo);
extern uchar PrnStart_Proxy(void);
extern uchar PrnStatus_Proxy(void);
extern int   PrnGetDotLine_Proxy(void);
extern void  PrnSetGray_Proxy(int Level);
extern int   PrnTemperature_Proxy(void);
extern void  PrnDoubleWidth_Proxy(int AscDoubleWidth, int LocalDoubleWidth);
extern void  PrnDoubleHeight_Proxy(int AscDoubleHeight, int LocalDoubleHeight);
extern void  PrnAttrSet_Proxy(int Reverse);

/*modem part*/
extern void s_ModemInfo_std(uchar * drv_ver, uchar * mdm_name, uchar * last_make_date, uchar * others);
extern uchar s_ModemInit_std(uchar mode);
extern uchar ModemReset_std(void);
extern uchar ModemDial_std(COMM_PARA * MPara, uchar * TelNo, uchar mode);
extern uchar ModemCheck_std(void);
extern uchar ModemTxd_std(uchar * SendData, ushort len);
extern uchar ModemRxd_std(uchar * RecvData, ushort * len);
extern uchar OnHook_std(void);
extern uchar ModemAsyncGet_std(uchar * ch);
extern ushort ModemExCommand_std(uchar * CmdStr, uchar * RespData, ushort * Dlen, ushort Timeout10ms);
extern int PPPLogin_std(char *name, char *passwd, long auth, int timeout);
extern int PPPCheck_std(void);
extern void PPPLogout_std(void);


/*comm part*/
extern uchar PortOpen_std(uchar channel, uchar * Attr);
extern uchar PortClose_std(uchar channel);
extern uchar PortSend_std(uchar channel, uchar ch);
extern uchar PortRecv_std(uchar channel, uchar * ch, uint ms);
extern uchar PortReset_std(uchar channel);
extern uchar PortSends_std(uchar channel, uchar * str, ushort str_len);
extern uchar PortTxPoolCheck_std(uchar channel);
extern int PortRecvs_std(uchar channel, uchar * pszBuf, ushort usRcvLen, ushort usTimeOut);
extern int PortPeep_std(uchar channel, uchar * buf, ushort want_len);

/*printer part*/
extern void  s_PrnInit_thermal(void);
extern uchar PrnInit_thermal(void);
extern void  PrnFontSet_thermal(uchar Ascii, uchar Locale);
extern void  PrnSpaceSet_thermal(uchar x, uchar y);
extern void  PrnLeftIndent_thermal(ushort x);
extern void  PrnStep_thermal(short pixel);
extern int   PrnPreFeedSet_thermal(unsigned int cmd);
extern uchar s_PrnStr_thermal(uchar * str);
extern uchar PrnLogo_thermal(uchar * logo);
extern uchar PrnStart_thermal(void);
extern void s_PrnStop_thermal(void);
extern uchar PrnStatus_thermal(void);
extern int   PrnGetDotLine_thermal(void);
extern void  PrnSetGray_thermal(int Level);
extern int   PrnTemperature_thermal(void);
extern void  PrnDoubleWidth_thermal(int AscDoubleWidth, int LocalDoubleWidth);
extern void  PrnDoubleHeight_thermal(int AscDoubleHeight, int LocalDoubleHeight);
extern void  PrnAttrSet_thermal(int Reverse);

/*printer part*/
extern void  s_PrnInit_stylus(void);
extern uchar PrnInit_stylus(void);
extern void  PrnFontSet_stylus(unsigned char Ascii, unsigned char Cfont);
extern void  PrnSpaceSet_stylus(unsigned char x, unsigned char y);
extern void  PrnLeftIndent_stylus(ushort num);
extern void  PrnStep_stylus(short pixel);
extern uchar s_PrnStr_stylus(uchar * s);
extern uchar PrnLogo_stylus(uchar * logo);
extern uchar PrnStart_stylus(void);
extern void ps_PrnStop_stylus(void);
extern uchar PrnStatus_stylus(void);

/*net part*/
extern int eth_mac_set_Proxy (unsigned char mac[6]);
extern int eth_mac_get_Proxy (unsigned char mac[6]);
extern int EthSet_Proxy(char *host_ip, char *host_mask,char *gw_ip,char *dns_ip);
extern int EthGet_Proxy(char *host_ip, char *host_mask,char *gw_ip,	char *dns_ip,long *state);
extern int DhcpStart_Proxy(void);
extern void DhcpStop_Proxy(void);
extern int DhcpCheck_Proxy(void);
extern int RouteSetDefault_Proxy(int ifIndex);
extern int RouteGetDefault_Proxy(void);
extern int DnsResolve_Proxy(char *name/*IN*/, char *result/*OUT*/, int len);
extern int DnsResolveExt_Proxy(char *name/*IN*/, char *result/*OUT*/, int len, int timeout);
extern int DhcpSetDns_Proxy(const char *addr);
extern int NetPing_Proxy(char* dst_ip_str, long timeout, int size);
extern int NetDevGet_Proxy(int Dev,char *HostIp,char *Mask,char *GateWay,char *Dns);/* Dev Interface Index */ 
extern void NetGetVersion_Proxy(char ip_ver[30]);
extern char net_version_get_Proxy(void);
extern int NetSocket_Proxy(int domain, int type, int protocol);
extern int Netioctl_Proxy(int socket, int s_cmd, int arg);
extern int NetConnect_Proxy(int socket, struct net_sockaddr *addr, socklen_t addrlen);
extern int NetBind_Proxy(int socket, struct net_sockaddr *addr, socklen_t addrlen);
extern int NetListen_Proxy(int socket, int backlog);
extern int NetAccept_Proxy(int socket, struct net_sockaddr *addr, socklen_t *addrlen);
extern int NetSend_Proxy(int socket, void *buf/* IN */, int size, int flags);
extern int NetSendto_Proxy(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t addrlen);
extern int NetRecv_Proxy(int socket, void *buf/* OUT */, int size, int flags);
extern int NetRecvfrom_Proxy(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t *addrlen);
extern int NetCloseSocket_Proxy(int socket);
extern int SockAddrSet_Proxy(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */);
extern int SockAddrGet_Proxy(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */, unsigned short *port/* OUT */);
extern void s_initNet_Proxy(void);
extern int ip_forcesyn_Proxy (void);
extern void EthSetRateDuplexMode_Proxy(int mode);
extern int EthGetFirstRouteMac_Proxy(const char *dest_ip, unsigned char *mac, int len);
extern int  NetAddStaticArp_Proxy(char *ip_str, unsigned char mac[6]);




extern int eth_mac_set_std (unsigned char mac[6]);
extern int eth_mac_get_std (unsigned char mac[6]);
extern int EthSet_std(char *host_ip, char *host_mask,char *gw_ip,char *dns_ip);
extern int EthGet_std(char *host_ip, char *host_mask,char *gw_ip,	char *dns_ip,long *state);
extern int DhcpStart_std(void);
extern void DhcpStop_std(void);
extern int DhcpCheck_std(void);
extern int RouteSetDefault_std(int ifIndex);
extern int RouteGetDefault_std(void);
extern int DnsResolve_std(char *name/*IN*/, char *result/*OUT*/, int len);
extern int DnsResolveExt_std(char *name/*IN*/, char *result/*OUT*/, int len, int timeout);
extern int DhcpSetDns_std(const char *addr);
extern int NetPing_std(char* dst_ip_str, long timeout, int size);
extern int NetDevGet_std(int Dev,char *HostIp,char *Mask,char *GateWay,char *Dns);/* Dev Interface Index */ 
extern void NetGetVersion_std(char ip_ver[30]);
extern char net_version_get_std(void);
extern int NetSocket_std(int domain, int type, int protocol);
extern int Netioctl_std(int socket, int s_cmd, int arg);
extern int NetConnect_std(int socket, struct net_sockaddr *addr, socklen_t addrlen);
extern int NetBind_std(int socket, struct net_sockaddr *addr, socklen_t addrlen);
extern int NetListen_std(int socket, int backlog);
extern int NetAccept_std(int socket, struct net_sockaddr *addr, socklen_t *addrlen);
extern int NetSend_std(int socket, void *buf/* IN */, int size, int flags);
extern int NetSendto_std(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t addrlen);
extern int NetRecv_std(int socket, void *buf/* OUT */, int size, int flags);
extern int NetRecvfrom_std(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t *addrlen);
extern int NetCloseSocket_std(int socket);
extern int SockAddrSet_std(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */);
extern int SockAddrGet_std(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */, unsigned short *port/* OUT */);
extern void s_initNet_std(void);
//extern int ip_forcesyn_std (void);
extern int  EthGetFirstRouteMac_std(const char *dest_ip, unsigned char *mac, int len); 
extern int  NetAddStaticArp_std(char *ip_str, unsigned char mac[6]);


struct MODEM_FUNCTION{
    void    (*s_ModemInfo)(uchar *drv_ver,uchar *mdm_name,uchar *last_make_date,uchar *others);
    uchar   (*s_ModemInit)(uchar mode);
    uchar   (*ModemReset)(void);
    uchar   (*ModemDial)(COMM_PARA *MPara, uchar *TelNo, uchar mode);
    uchar   (*ModemCheck)(void);
    uchar   (*ModemTxd)(uchar *SendData, ushort len);
    uchar   (*ModemRxd)(uchar *RecvData, ushort *len);
    uchar   (*OnHook)(void);
    uchar   (*ModemAsyncGet)(uchar *ch);
    ushort  (*ModemExCommand)(uchar *CmdStr,uchar *RespData,ushort *Dlen,ushort Timeout10ms);
	int     (*PPPLogin)(char *name, char *passwd, long auth, int timeout);
    int     (*PPPCheck)(void);
    void    (*PPPLogout)(void);
};
struct COMM_FUNCTION{    
    uchar   (*PortOpen)(uchar channel, uchar *Attr);
    uchar   (*PortClose)(uchar channel);
    uchar   (*PortSend)(uchar channel, uchar ch);
    uchar   (*PortRecv)(uchar channel, uchar *ch, uint ms);
    uchar   (*PortReset)(uchar channel);
    uchar   (*PortSends)(uchar channel,uchar *str,ushort str_len);
    uchar   (*PortTxPoolCheck)(uchar channel);
    int (*PortRecvs)(uchar channel,uchar *pszBuf,ushort usRcvLen,ushort usTimeOut);
    int (*PortPeep)(uchar channel,uchar *buf,ushort want_len);
};

struct PRINTER_FUNCTION{
    void    (*s_PrnInit)(void);
    uchar   (*PrnInit)(void);
    void    (*PrnFontSet)(uchar Ascii, uchar CFont);
    void    (*PrnSpaceSet)(uchar x, uchar y);
    void    (*PrnLeftIndent)(ushort Indent);
    void    (*PrnStep)(short pixel);
    uchar   (*s_PrnStr)(uchar *str);
    uchar   (*PrnLogo)(uchar *logo);
    uchar   (*PrnStart)(void);
    void    (*s_PrnStop)(void);
    uchar   (*PrnStatus)(void);
    int     (*PrnGetDotLine)(void);
    void    (*PrnSetGray)(int Level);
    int     (*PrnGetFontDot)(int FontSize, uchar *str, uchar *OutDot);
    int     (*PrnTemperature)(void);
    void    (*PrnDoubleWidth)(int AscDoubleWidth, int LocalDoubleWidth);
    void    (*PrnDoubleHeight)(int AscDoubleHeight, int LocalDoubleHeight);
    void    (*PrnAttrSet)(int Reverse);
};

struct NET_FUNCTION{
    int (*eth_mac_set) (unsigned char mac[6]);
    int (*eth_mac_get) (unsigned char mac[6]);
    int  (*EthSet)(char *host_ip, char *host_mask,char *gw_ip,char *dns_ip);
    int  (*EthGet)(char *host_ip, char *host_mask,char *gw_ip,	char *dns_ip,long *state);		
    int  (*DhcpStart)(void);
    void (* DhcpStop)(void);
    int  (*DhcpCheck)(void);		
    int  (*RouteSetDefault)(int ifIndex);
    int  (*RouteGetDefault)(void);
    int  (*DnsResolve)(char *name/*IN*/, char *result/*OUT*/, int len);
    int  (*DnsResolveExt)(char *name/*IN*/, char *result/*OUT*/, int len,int timeout);
    int  (*DhcpSetDns)(const char *addr);
    int  (*NetPing)(char* dst_ip_str, long timeout, int size);
    int  (*NetDevGet)(int Dev,char *HostIp,char *Mask,char *GateWay,char *Dns);/* Dev Interface Index */ 
    void  (*NetGetVersion)(char ip_ver[30]);
    char  (*net_version_get)(void);
    int  (*NetSocket)(int domain, int type, int protocol);
    int  (*NetCloseSocket)(int socket);
    int  (*Netioctl)(int socket, int s_cmd, int arg);
    int  (*NetConnect)(int socket, struct net_sockaddr *addr, socklen_t addrlen);
    int  (*NetBind)(int socket, struct net_sockaddr *addr, socklen_t addrlen);
    int  (*NetListen)(int socket, int backlog);
    int  (*NetAccept)(int socket, struct net_sockaddr *addr, socklen_t *addrlen);
    int  (*NetSend)(int socket, void *buf/* IN */, int size, int flags);
    int  (*NetSendto)(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t addrlen);
    int  (*NetRecv)(int socket, void *buf/* OUT */, int size, int flags);
    int  (*NetRecvfrom)(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t *addrlen);
    int  (*SockAddrSet)(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */);
    int  (*SockAddrGet)(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */, unsigned short *port/* OUT */);
    void (*s_initNet)(void);
    int (*ip_forcesyn) (void);
	void (*EthSetRateDuplexMode)(int mode);	
	int  (*EthGetFirstRouteMac)(const char *dest_ip, unsigned char *mac, int len); 
	int  (*NetAddStaticArp)(char *ip_str, unsigned char mac[6]);
} ;

//Joshua _a for socket
struct ST_SOCKET_FUNC
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

    int  (*DhcpSetDns)(const char *);
	int (* NetPing)(char *,long ,int);
	int (* RouteSetDefault)(int);
	int (* RouteGetDefault)(void);
	int (* NetDevGet)(int,char *,char *,char *,char *);
};

#endif

