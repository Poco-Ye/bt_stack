#include <stdarg.h>
#include "base.h"
#include "dll.h"
#include "posapi.h"
#include "sys_combin_api.h"
#include "..\comm\comm.h"
#include "..\wifi\apis\wlanapp.h"
#include "..\fat\fsapi.h"

static struct MODEM_FUNCTION    s_modem_func;
static struct COMM_FUNCTION     s_comm_func;
static struct PRINTER_FUNCTION  s_print_func;
static struct NET_FUNCTION s_net_func;

static struct NET_FUNCTION *pt_socket = &s_net_func;

void static nop_function_api(){} /*空函数不做任何处理*/
struct NET_FUNCTION s_wifi_net_func = {
    .eth_mac_set = (void *)nop_function_api,
    .eth_mac_get = (void *)nop_function_api,
    .EthSet = (void *)nop_function_api,
    .EthGet = (void *)nop_function_api,
    .DhcpStart = (void *)nop_function_api,
    .DhcpStop = (void *)nop_function_api,
    .DhcpCheck = (void *)nop_function_api,
    .DhcpSetDns = (void *)nop_function_api,
    .RouteSetDefault = WifiRouteSetDefault,
    .RouteGetDefault = WifiRouteGetDefault,
    .DnsResolve = WifiDnsResolve,
    .DnsResolveExt = WifiDnsResolveExt,
    .NetPing = WifiNetPing,
    .NetDevGet = WifiNetDevGet,
    .NetGetVersion = (void *)nop_function_api,
    .net_version_get = (void *)nop_function_api,
    .NetSocket = WifiNetSocket,
    .NetCloseSocket = WifiNetCloseSocket,
    .Netioctl = WifiNetioctl,
    .NetConnect = WifiNetConnect,
    .NetBind = WifiNetBind,
    .NetListen = WifiNetListen,
    .NetAccept = WifiNetAccept,
    .NetSend = WifiNetSend,
    .NetSendto = WifiNetSendto,
    .NetRecv = WifiNetRecv,
    .NetRecvfrom = WifiNetRecvfrom,
    .SockAddrSet = WifiSockAddrSet,
    .SockAddrGet = WifiSockAddrGet,
    .s_initNet = (void *)nop_function_api,
    .ip_forcesyn= (void *)nop_function_api,
    .EthSetRateDuplexMode = (void *)nop_function_api,
    .EthGetFirstRouteMac = (void *)nop_function_api,
    .NetAddStaticArp = (void *)nop_function_api,
      
};

// 0: std api 1: proxy
void SysRegNetApi(int mode)//1      
{
	if(mode==0)
	{
		/*net part*/
		s_net_func.eth_mac_set = eth_mac_set_std;
		s_net_func.eth_mac_get = eth_mac_get_std;
		s_net_func.EthSet = EthSet_std;
		s_net_func.EthGet = EthGet_std;
		s_net_func.DhcpStart = DhcpStart_std;
		s_net_func.DhcpStop = DhcpStop_std;
		s_net_func.DhcpCheck = DhcpCheck_std;
		s_net_func.RouteSetDefault = RouteSetDefault_std;
		s_net_func.RouteGetDefault = RouteGetDefault_std;
		s_net_func.DnsResolve = DnsResolve_std;
        s_net_func.DnsResolveExt = DnsResolveExt_std;
        s_net_func.DhcpSetDns = DhcpSetDns_std;
		s_net_func.NetPing = NetPing_std;
		s_net_func.NetDevGet = NetDevGet_std;
		s_net_func.NetGetVersion = NetGetVersion_std;
		s_net_func.net_version_get = net_version_get_std;
		s_net_func.NetSocket = NetSocket_std;
		s_net_func.NetCloseSocket = NetCloseSocket_std;
		s_net_func.Netioctl = Netioctl_std;
		s_net_func.NetConnect = NetConnect_std;
		s_net_func.NetBind = NetBind_std;
		s_net_func.NetListen = NetListen_std;
		s_net_func.NetAccept = NetAccept_std;
		s_net_func.NetSend = NetSend_std;
		s_net_func.NetSendto = NetSendto_std;
		s_net_func.NetRecv = NetRecv_std;
		s_net_func.NetRecvfrom = NetRecvfrom_std;
		s_net_func.SockAddrSet = SockAddrSet_std;
		s_net_func.SockAddrGet = SockAddrGet_std;
		s_net_func.s_initNet = s_initNet_std;
		s_net_func.ip_forcesyn =  (void *)nop_function_api;	
		s_net_func.EthSetRateDuplexMode = EthSetRateDuplexMode_std;
		s_net_func.EthGetFirstRouteMac = EthGetFirstRouteMac_std;
		s_net_func.NetAddStaticArp = NetAddStaticArp_std;		
	}
	else
	{
		/*net part*/	
		s_net_func.eth_mac_set = eth_mac_set_Proxy;
		s_net_func.eth_mac_get = eth_mac_get_Proxy;
		s_net_func.EthSet = EthSet_Proxy;
		s_net_func.EthGet = EthGet_Proxy;
		s_net_func.DhcpStart = DhcpStart_Proxy;
		s_net_func.DhcpStop = DhcpStop_Proxy;
		s_net_func.DhcpCheck = DhcpCheck_Proxy;
		s_net_func.RouteSetDefault = RouteSetDefault_Proxy;
		s_net_func.RouteGetDefault = RouteGetDefault_Proxy;
		s_net_func.DnsResolve = DnsResolve_Proxy;
        s_net_func.DnsResolveExt = DnsResolveExt_Proxy;        
        s_net_func.DhcpSetDns = DhcpSetDns_Proxy;
		s_net_func.NetPing = NetPing_Proxy;
		s_net_func.NetDevGet = NetDevGet_Proxy;
		s_net_func.NetGetVersion = NetGetVersion_Proxy;
		s_net_func.net_version_get = net_version_get_Proxy;
		s_net_func.NetSocket = NetSocket_Proxy;
		s_net_func.NetCloseSocket = NetCloseSocket_Proxy;
		s_net_func.Netioctl = Netioctl_Proxy;
		s_net_func.NetConnect = NetConnect_Proxy;
		s_net_func.NetBind = NetBind_Proxy;
		s_net_func.NetListen = NetListen_Proxy;
		s_net_func.NetAccept = NetAccept_Proxy;
		s_net_func.NetSend = NetSend_Proxy;
		s_net_func.NetSendto = NetSendto_Proxy;
		s_net_func.NetRecv = NetRecv_Proxy;
		s_net_func.NetRecvfrom = NetRecvfrom_Proxy;
		s_net_func.SockAddrSet = SockAddrSet_Proxy;
		s_net_func.SockAddrGet = SockAddrGet_Proxy;
		s_net_func.s_initNet = s_initNet_Proxy;
		s_net_func.ip_forcesyn = (void *)nop_function_api; //ip_forcesyn_Proxy;
		s_net_func.EthSetRateDuplexMode = EthSetRateDuplexMode_Proxy;
		s_net_func.EthGetFirstRouteMac = EthGetFirstRouteMac_Proxy;		
		s_net_func.NetAddStaticArp = NetAddStaticArp_Proxy;
	}
}

void SysUseProxy(void)
{	
	if(GetHWBranch() != D210HW_V2x) return;
	/*modem part*/
	s_modem_func.s_ModemInfo = s_ModemInfo_Proxy;
	s_modem_func.s_ModemInit= s_ModemInit_Proxy;
	s_modem_func.ModemReset= ModemReset_Proxy;
	s_modem_func.ModemDial= ModemDial_Proxy;
	s_modem_func.ModemCheck=ModemCheck_Proxy;
	s_modem_func.ModemTxd= ModemTxd_Proxy;
	s_modem_func.ModemRxd= ModemRxd_Proxy;
	s_modem_func.OnHook= OnHook_Proxy;
	s_modem_func.ModemAsyncGet=ModemAsyncGet_Proxy;
	s_modem_func.ModemExCommand=ModemExCommand_Proxy;
	s_modem_func.PPPLogin = PPPLogin_Proxy;
    s_modem_func.PPPCheck = PPPCheck_Proxy;
    s_modem_func.PPPLogout = PPPLogout_Proxy;	
			
	/*comm part*/
	s_comm_func.PortOpen = PortOpen_Proxy;
	s_comm_func.PortClose =PortClose_Proxy;
	s_comm_func.PortSend  =PortSend_Proxy;
	s_comm_func.PortRecv  =PortRecv_Proxy;
	s_comm_func.PortReset =PortReset_Proxy;
	s_comm_func.PortSends =PortSends_Proxy;
	s_comm_func.PortTxPoolCheck =PortTxPoolCheck_Proxy;
	s_comm_func.PortRecvs =PortRecvs_Proxy;
	s_comm_func.PortPeep  =PortPeep_Proxy;	

	#if 0				// 避免单手持机在使用网络协议栈的过程中放上座机会自动切换到代理方式
	/*net part*/
	s_net_func.eth_mac_set = eth_mac_set_Proxy;
	s_net_func.eth_mac_get = eth_mac_get_Proxy;
	s_net_func.EthSet = EthSet_Proxy;
	s_net_func.EthGet = EthGet_Proxy;
	s_net_func.DhcpStart = DhcpStart_Proxy;
	s_net_func.DhcpStop = DhcpStop_Proxy;
	s_net_func.DhcpCheck = DhcpCheck_Proxy;
	s_net_func.RouteSetDefault = RouteSetDefault_Proxy;
	s_net_func.RouteGetDefault = RouteGetDefault_Proxy;
	s_net_func.DnsResolve = DnsResolve_Proxy;
	s_net_func.NetPing = NetPing_Proxy;
	s_net_func.NetDevGet = NetDevGet_Proxy;
	s_net_func.NetGetVersion = NetGetVersion_Proxy;
	s_net_func.net_version_get = net_version_get_Proxy;
	s_net_func.NetSocket = NetSocket_Proxy;
	s_net_func.NetCloseSocket = NetCloseSocket_Proxy;
	s_net_func.Netioctl = Netioctl_Proxy;
	s_net_func.NetConnect = NetConnect_Proxy;
	s_net_func.NetBind = NetBind_Proxy;
	s_net_func.NetListen = NetListen_Proxy;
	s_net_func.NetAccept = NetAccept_Proxy;
	s_net_func.NetSend = NetSend_Proxy;
	s_net_func.NetSendto = NetSendto_Proxy;
	s_net_func.NetRecv = NetRecv_Proxy;
	s_net_func.NetRecvfrom = NetRecvfrom_Proxy;
	s_net_func.SockAddrSet = SockAddrSet_Proxy;
	s_net_func.SockAddrGet = SockAddrGet_Proxy;
	s_net_func.s_initNet = s_initNet_Proxy;
	s_net_func.ip_forcesyn = (void *)nop_function_api; //ip_forcesyn_Proxy;
	s_net_func.EthSetRateDuplexMode = EthSetRateDuplexMode_Proxy;
	s_net_func.EthGetFirstRouteMac = EthGetFirstRouteMac_Proxy;		
	s_net_func.NetAddStaticArp = NetAddStaticArp_Proxy;
	#endif
}

int SystemApiInit()
{
    int type;
    char context[32];
    type= get_machine_type();
    switch(type)
    {
        case S60:			
        	break;
        case SP30:
            /*modem part*/
            s_modem_func.s_ModemInfo = (void *)nop_function_api;
            s_modem_func.s_ModemInit= (void *)nop_function_api;
            s_modem_func.ModemReset= (void *)nop_function_api;
            s_modem_func.ModemDial= (void *)nop_function_api;
            s_modem_func.ModemCheck=(void *)nop_function_api;
            s_modem_func.ModemTxd= (void *)nop_function_api;
            s_modem_func.ModemRxd= (void *)nop_function_api;
            s_modem_func.OnHook= (void *)nop_function_api;
            s_modem_func.ModemAsyncGet=(void *)nop_function_api;
            s_modem_func.ModemExCommand=(void *)nop_function_api;
            /*comm part*/
            s_comm_func.PortOpen = PortOpen_std;
            s_comm_func.PortClose =PortClose_std;
            s_comm_func.PortSend  =PortSend_std;
            s_comm_func.PortRecv  =PortRecv_std;
            s_comm_func.PortReset =PortReset_std;
            s_comm_func.PortSends =PortSends_std;
            s_comm_func.PortTxPoolCheck =PortTxPoolCheck_std;
            s_comm_func.PortRecvs =PortRecvs_std;
            s_comm_func.PortPeep  =PortPeep_std;
            /*printer part*/
            s_print_func.s_PrnInit      = (void *)nop_function_api;
            s_print_func.PrnInit        = (void *)nop_function_api;
            s_print_func.PrnFontSet     = (void *)nop_function_api;
            s_print_func.PrnSpaceSet    = (void *)nop_function_api;
            s_print_func.PrnLeftIndent  = (void *)nop_function_api;
            s_print_func.PrnStep        = (void *)nop_function_api;
            s_print_func.s_PrnStr       = (void *)nop_function_api;
            s_print_func.PrnLogo        = (void *)nop_function_api;
            s_print_func.PrnStart       = (void *)nop_function_api;
            s_print_func.s_PrnStop      = (void *)nop_function_api;
            s_print_func.PrnStatus      = (void *)nop_function_api;
            s_print_func.PrnGetDotLine  = (void *)nop_function_api;
            s_print_func.PrnSetGray     = (void *)nop_function_api;
            s_print_func.PrnGetFontDot  = (void *)nop_function_api;
            s_print_func.PrnTemperature = (void *)nop_function_api;
            s_print_func.PrnDoubleWidth = (void *)nop_function_api;
            s_print_func.PrnDoubleHeight= (void *)nop_function_api;
            s_print_func.PrnAttrSet     = (void *)nop_function_api;
            /*net part*/
            s_net_func.eth_mac_set = (void *)nop_function_api;
            s_net_func.eth_mac_get = (void *)nop_function_api;
            s_net_func.EthSet = (void *)nop_function_api;
            s_net_func.EthGet = (void *)nop_function_api;
            s_net_func.DhcpStart = (void *)nop_function_api;
            s_net_func.DhcpStop = (void *)nop_function_api;
            s_net_func.DhcpCheck = (void *)nop_function_api;
            s_net_func.DhcpSetDns = (void *)nop_function_api;
            s_net_func.RouteSetDefault = (void *)nop_function_api;
            s_net_func.RouteGetDefault = (void *)nop_function_api;
            s_net_func.DnsResolve = (void *)nop_function_api;
            s_net_func.DnsResolveExt = (void *)nop_function_api;
            s_net_func.NetPing = (void *)nop_function_api;
            s_net_func.NetDevGet = (void *)nop_function_api;
            s_net_func.NetGetVersion = (void *)nop_function_api;
            s_net_func.net_version_get = (void *)nop_function_api;
            s_net_func.NetSocket = (void *)nop_function_api;
            s_net_func.NetCloseSocket = (void *)nop_function_api;
            s_net_func.Netioctl = (void *)nop_function_api;
            s_net_func.NetConnect = (void *)nop_function_api;
            s_net_func.NetBind = (void *)nop_function_api;
            s_net_func.NetListen = (void *)nop_function_api;
            s_net_func.NetAccept = (void *)nop_function_api;
            s_net_func.NetSend = (void *)nop_function_api;
            s_net_func.NetSendto = (void *)nop_function_api;
            s_net_func.NetRecv = (void *)nop_function_api;
            s_net_func.NetRecvfrom = (void *)nop_function_api;
            s_net_func.SockAddrSet = (void *)nop_function_api;
            s_net_func.SockAddrGet = (void *)nop_function_api;
            s_net_func.s_initNet = (void *)nop_function_api;
            s_net_func.ip_forcesyn =  (void *)nop_function_api;    
        break;    
        default:
            /*modem part*/
            s_modem_func.s_ModemInfo = s_ModemInfo_std;
            s_modem_func.s_ModemInit= s_ModemInit_std;
            s_modem_func.ModemReset= ModemReset_std;
            s_modem_func.ModemDial= ModemDial_std;
            s_modem_func.ModemCheck=ModemCheck_std;
            s_modem_func.ModemTxd= ModemTxd_std;
            s_modem_func.ModemRxd= ModemRxd_std;
            s_modem_func.OnHook= OnHook_std;
            s_modem_func.ModemAsyncGet=ModemAsyncGet_std;
            s_modem_func.ModemExCommand=ModemExCommand_std;
			s_modem_func.PPPLogin = PPPLogin_std;
            s_modem_func.PPPCheck = PPPCheck_std;
            s_modem_func.PPPLogout = PPPLogout_std;
            /*comm part*/
            s_comm_func.PortOpen = PortOpen_std;
            s_comm_func.PortClose =PortClose_std;
            s_comm_func.PortSend  =PortSend_std;
            s_comm_func.PortRecv  =PortRecv_std;
            s_comm_func.PortReset =PortReset_std;
            s_comm_func.PortSends =PortSends_std;
            s_comm_func.PortTxPoolCheck =PortTxPoolCheck_std;
            s_comm_func.PortRecvs =PortRecvs_std;
            s_comm_func.PortPeep  =PortPeep_std;

            /*printer part*/
            s_print_func.s_PrnInit      = s_PrnInit_thermal;
            s_print_func.PrnInit        = PrnInit_thermal;
            s_print_func.PrnFontSet     = PrnFontSet_thermal;
            s_print_func.PrnSpaceSet    = PrnSpaceSet_thermal;
            s_print_func.PrnLeftIndent  = PrnLeftIndent_thermal;
            s_print_func.PrnStep        = PrnStep_thermal;
            s_print_func.s_PrnStr       = s_PrnStr_thermal;
            s_print_func.PrnLogo        = PrnLogo_thermal;
            s_print_func.PrnStart       = PrnStart_thermal;
            s_print_func.s_PrnStop      = s_PrnStop_thermal;
            s_print_func.PrnStatus      = PrnStatus_thermal;
            s_print_func.PrnGetDotLine  = PrnGetDotLine_thermal;
            s_print_func.PrnSetGray     = PrnSetGray_thermal;
            s_print_func.PrnGetFontDot  = PrnGetFontDot;
            s_print_func.PrnTemperature = PrnTemperature_thermal;
            s_print_func.PrnDoubleWidth = PrnDoubleWidth_thermal;
            s_print_func.PrnDoubleHeight= PrnDoubleHeight_thermal;
            s_print_func.PrnAttrSet     = PrnAttrSet_thermal;

    		/*net part*/
    		s_net_func.eth_mac_set = eth_mac_set_std;
    		s_net_func.eth_mac_get = eth_mac_get_std;
    		s_net_func.EthSet = EthSet_std;
    		s_net_func.EthGet = EthGet_std;
    		s_net_func.DhcpStart = DhcpStart_std;
    		s_net_func.DhcpStop = DhcpStop_std;
    		s_net_func.DhcpCheck = DhcpCheck_std;
            s_net_func.DhcpSetDns = DhcpSetDns_std;
    		s_net_func.RouteSetDefault = RouteSetDefault_std;
    		s_net_func.RouteGetDefault = RouteGetDefault_std;
    		s_net_func.DnsResolve = DnsResolve_std;
            s_net_func.DnsResolveExt = DnsResolveExt_std;
    		s_net_func.NetPing = NetPing_std;
    		s_net_func.NetDevGet = NetDevGet_std;
    		s_net_func.NetGetVersion = NetGetVersion_std;
    		s_net_func.net_version_get = net_version_get_std;
    		s_net_func.NetSocket = NetSocket_std;
    		s_net_func.NetCloseSocket = NetCloseSocket_std;
    		s_net_func.Netioctl = Netioctl_std;
    		s_net_func.NetConnect = NetConnect_std;
    		s_net_func.NetBind = NetBind_std;
    		s_net_func.NetListen = NetListen_std;
    		s_net_func.NetAccept = NetAccept_std;
    		s_net_func.NetSend = NetSend_std;
    		s_net_func.NetSendto = NetSendto_std;
    		s_net_func.NetRecv = NetRecv_std;
    		s_net_func.NetRecvfrom = NetRecvfrom_std;
    		s_net_func.SockAddrSet = SockAddrSet_std;
    		s_net_func.SockAddrGet = SockAddrGet_std;
    		s_net_func.s_initNet = s_initNet_std;
    		s_net_func.ip_forcesyn =  (void *)nop_function_api;			
			s_net_func.EthSetRateDuplexMode = EthSetRateDuplexMode_std;
			s_net_func.EthGetFirstRouteMac = EthGetFirstRouteMac_std;	
			s_net_func.NetAddStaticArp = NetAddStaticArp_std;				
         break;
    }
    /*printer part*/
    if(type == S78)
    {
        s_print_func.s_PrnInit      = s_PrnInit_stylus;
        s_print_func.PrnInit        = PrnInit_stylus;
        s_print_func.PrnFontSet     = PrnFontSet_stylus;
        s_print_func.PrnSpaceSet    = PrnSpaceSet_stylus;
        s_print_func.PrnLeftIndent  = PrnLeftIndent_stylus;
        s_print_func.PrnStep        = PrnStep_stylus;
        s_print_func.s_PrnStr       = s_PrnStr_stylus;
        s_print_func.PrnLogo        = PrnLogo_stylus;
        s_print_func.PrnStart       = PrnStart_stylus;
        s_print_func.s_PrnStop      = ps_PrnStop_stylus;
        s_print_func.PrnStatus      = PrnStatus_stylus;
        s_print_func.PrnGetDotLine  = (void *)nop_function_api;
        s_print_func.PrnSetGray     = (void *)nop_function_api;
        s_print_func.PrnGetFontDot  = PrnGetFontDot;
        s_print_func.PrnTemperature = (void *)nop_function_api;
        s_print_func.PrnDoubleWidth = (void *)nop_function_api;
        s_print_func.PrnDoubleHeight= (void *)nop_function_api;
        s_print_func.PrnAttrSet     = (void *)nop_function_api;
    }
	/*modem part*/
	if (ReadCfgInfo("MODEM", context) < 0)
	{
		s_modem_func.s_ModemInfo = (void *)nop_function_api;
		s_modem_func.s_ModemInit= (void *)nop_function_api;
		s_modem_func.ModemReset= (void *)nop_function_api;
		s_modem_func.ModemDial= (void *)nop_function_api;
		s_modem_func.ModemCheck=(void *)nop_function_api;
		s_modem_func.ModemTxd= (void *)nop_function_api;
		s_modem_func.ModemRxd= (void *)nop_function_api;
		s_modem_func.OnHook= (void *)nop_function_api;
		s_modem_func.ModemAsyncGet=(void *)nop_function_api;
		s_modem_func.ModemExCommand=(void *)nop_function_api;
		s_modem_func.PPPLogin = (void *)nop_function_api;
        s_modem_func.PPPCheck = (void *)nop_function_api;
        s_modem_func.PPPLogout = (void *)nop_function_api;
	}		
	/*printer part*/
	if (ReadCfgInfo("PRINTER", context) < 0)
	{
        s_print_func.s_PrnInit      = (void *)nop_function_api;
        s_print_func.PrnInit        = (void *)nop_function_api;
        s_print_func.PrnFontSet     = (void *)nop_function_api;
        s_print_func.PrnSpaceSet    = (void *)nop_function_api;
        s_print_func.PrnLeftIndent  = (void *)nop_function_api;
        s_print_func.PrnStep        = (void *)nop_function_api;
        s_print_func.s_PrnStr       = (void *)nop_function_api;
        s_print_func.PrnLogo        = (void *)nop_function_api;
        s_print_func.PrnStart       = (void *)nop_function_api;
        s_print_func.s_PrnStop      = (void *)nop_function_api;
        s_print_func.PrnStatus      = (void *)nop_function_api;
        s_print_func.PrnGetDotLine  = (void *)nop_function_api;
        s_print_func.PrnSetGray     = (void *)nop_function_api;
        s_print_func.PrnGetFontDot  = (void *)nop_function_api;
        s_print_func.PrnTemperature = (void *)nop_function_api;
        s_print_func.PrnDoubleWidth = (void *)nop_function_api;
        s_print_func.PrnDoubleHeight= (void *)nop_function_api;
        s_print_func.PrnAttrSet     = (void *)nop_function_api;
	}
   
}

/*Modem Part*/
void s_ModemInfo(uchar *drv_ver,uchar *mdm_name,uchar *last_make_date,uchar *others)
{
    s_modem_func.s_ModemInfo(drv_ver,mdm_name,last_make_date,others);
}
uchar s_ModemInit(uchar mode)//0--first step,1--last step,2--both steps
{
    return s_modem_func.s_ModemInit(mode);
}
uchar ModemReset(void)
{
    return s_modem_func.ModemReset();
}
uchar ModemDial(COMM_PARA *MPara, uchar *TelNo, uchar mode)
{
    return s_modem_func.ModemDial(MPara,TelNo,mode);
}
uchar ModemCheck(void)
{
    return s_modem_func.ModemCheck();
}
uchar ModemRxd(uchar *RecvData, ushort *len)
{
    return s_modem_func.ModemRxd(RecvData,len);
}
uchar ModemTxd(uchar *SendData, ushort len)
{
    return s_modem_func.ModemTxd(SendData,len);
}

uchar OnHook(void)
{
    return s_modem_func.OnHook();
}

uchar ModemAsyncGet(uchar *ch)
{
    return s_modem_func.ModemAsyncGet(ch);
}
ushort ModemExCommand(uchar *CmdStr,uchar *RespData,ushort *Dlen,ushort Timeout10ms)
{
    return s_modem_func.ModemExCommand(CmdStr,RespData,Dlen,Timeout10ms);
}

int  PPPLogin(char *name, char *passwd, long auth, int timeout)
{
    return s_modem_func.PPPLogin(name, passwd, auth, timeout);
}
int PPPCheck(void)
{
    return s_modem_func.PPPCheck();
}
void PPPLogout(void)
{
    return s_modem_func.PPPLogout();
}

uchar PortOpen(uchar channel, uchar * Attr)
{	
    if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B)))
		return s_comm_func.PortOpen(channel,Attr); 
    else 
		return PortOpen_std(channel,Attr); 
}
uchar PortClose(uchar channel)
{
    if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B))) 
		return s_comm_func.PortClose(channel);		
    else 
		return PortClose_std(channel); 
}
uchar PortSend(uchar channel, uchar ch)
{
    if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B)))
		return s_comm_func.PortSend(channel, ch);	
    else 
		return PortSend_std(channel, ch); 
}
uchar PortRecv(uchar channel, uchar * ch, uint ms)
{
    if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B))) 
		return s_comm_func.PortRecv(channel,ch,ms);	
    else 
		return PortRecv_std(channel,ch,ms); 
}
uchar PortReset(uchar channel)
{
    if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B)))  
		return s_comm_func.PortReset(channel);	
    else 
		return PortReset_std(channel); 
}
uchar PortSends(uchar channel, uchar * str, ushort str_len)
{
   if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B))) 
   		return s_comm_func.PortSends(channel,str,str_len);  
   else 
   		return PortSends_std(channel,str,str_len);
}
uchar PortTxPoolCheck(uchar channel)
{
    if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B)))
		return s_comm_func.PortTxPoolCheck(channel);	
    else 
		return PortTxPoolCheck_std(channel);
}
int PortRecvs(uchar channel, uchar * pszBuf, ushort usRcvLen, ushort usTimeOut)
{
    if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B)))
		return s_comm_func.PortRecvs(channel,pszBuf,usRcvLen,usTimeOut);	
    else 
		return PortRecvs_std(channel,pszBuf,usRcvLen,usTimeOut); 
}
int PortPeep(uchar channel, uchar * buf, ushort want_len)
{
    if( (is_hasbase() && channel==P_MODEM) || ((channel>=P_BASE_DEV) && (channel<=P_BASE_B)))
		return s_comm_func.PortPeep(channel,buf,want_len);	
    else 
		return PortPeep_std(channel,buf,want_len);
}



/*Printer part*/

void s_PrnInit(void)
{
    s_print_func.s_PrnInit();
}

uchar PrnInit(void)
{
    return s_print_func.PrnInit();
}

void PrnFontSet(uchar Ascii, uchar CFont)
{
    s_print_func.PrnFontSet(Ascii,CFont); 
}
void PrnSpaceSet(uchar x, uchar y)
{
    s_print_func.PrnSpaceSet(x, y); 
}
void PrnLeftIndent(ushort Indent)
{
    s_print_func.PrnLeftIndent(Indent);
}
void PrnStep(short pixel)
{
    s_print_func.PrnStep(pixel); 
}
uchar s_PrnStr(char *str)
{
    return(s_print_func.s_PrnStr(str));
}
uchar PrnStr(char *str,...)
{
    va_list marker;
    char glBuff[2048];
    ushort len;

    glBuff[0] = 0;
    va_start(marker, str);
    vsprintf(glBuff,str,marker);
    va_end( marker );
    len = strlen(glBuff);
    if(!len)        return 0;
    if(len>2048)    return 0xfe;    
    return(s_PrnStr(glBuff));
}

uchar PrnLogo(uchar *logo)
{
    return s_print_func.PrnLogo(logo);

}
uchar PrnStart(void)
{
    return s_print_func.PrnStart(); 
}
void s_PrnStop(void)
{
    s_print_func.s_PrnStop(); 
}

uchar PrnStatus(void)
{
    return s_print_func.PrnStatus();  
}

int PrnGetDotLine(void)
{
    return s_print_func.PrnGetDotLine();  
}
void PrnSetGray(int Level)
{
    s_print_func.PrnSetGray(Level);      
}

int PrnTemperature(void)
{
    return s_print_func.PrnTemperature();  
}
void PrnDoubleWidth(int AscDoubleWidth, int LocalDoubleWidth)
{
    s_print_func.PrnDoubleWidth(AscDoubleWidth, LocalDoubleWidth); 
}
void PrnDoubleHeight(int AscDoubleHeight, int LocalDoubleHeight)
{
    s_print_func.PrnDoubleHeight(AscDoubleHeight, LocalDoubleHeight); 
}

int PrnPreFeedSet(unsigned int cmd)
{
    int type = get_machine_type();
    if (D210 != type)
    {
		return -2; 
    }
    if (1 != cmd && 0 != cmd)
    {
		return -3;
    }

	return PrnPreFeedSet_thermal(cmd);
}


void PrnAttrSet(int Reverse)
{
    s_print_func.PrnAttrSet(Reverse);  
}

/*net part*/
int eth_mac_set(unsigned char mac[6])
{
    return pt_socket->eth_mac_set(mac);
}

int eth_mac_get(unsigned char mac[6])
{
    return pt_socket->eth_mac_get(mac);
}

int  EthSet(char *host_ip, char *host_mask,char *gw_ip,char *dns_ip)
{
    return pt_socket->EthSet(host_ip, host_mask,gw_ip,dns_ip);
}

int  EthGet(char *host_ip, char *host_mask,char *gw_ip,char *dns_ip,long *state)
{
    return pt_socket->EthGet(host_ip, host_mask,gw_ip,dns_ip,state);
}

int  DhcpStart(void)
{
    return pt_socket->DhcpStart();
}

void  DhcpStop(void)
{
    return pt_socket->DhcpStop();
}

int  DhcpCheck(void)
{
    return pt_socket->DhcpCheck();
}


void s_RouteSetDefault(int ifIndex)
{
	if (ifIndex == WIFI_ROUTE_NUM)	//it's wifi
	{
		pt_socket = &s_wifi_net_func;
		pt_socket->RouteSetDefault(ifIndex);
	}
	else
	{
		if(is_hasbase() && ( ifIndex==10 || ifIndex==0))
		{
			SysRegNetApi(1);// proxy api
		}
		else
			SysRegNetApi(0);// std api
		pt_socket = &s_net_func;			// 设置好对应的api接口，并没有真正设置路由
	}
}
int  RouteSetDefault(int ifIndex)
{
	if (ifIndex == WIFI_ROUTE_NUM)	//it's wifi
	{
		pt_socket = &s_wifi_net_func;
	}
	else
	{
		if(is_hasbase() && (ifIndex==10 || ifIndex==0))
		{
			SysRegNetApi(1);// proxy api
		}
		else
			SysRegNetApi(0);// std api
		pt_socket = &s_net_func;
	}
    return pt_socket->RouteSetDefault(ifIndex);
}

int  RouteGetDefault(void)
{
    return pt_socket->RouteGetDefault();
}

int  DnsResolve(char *name/*IN*/, char *result/*OUT*/, int len)
{
    return pt_socket->DnsResolve(name, result, len);
}

int  DnsResolveExt(char *name/*IN*/, char *result/*OUT*/, int len,int timeout)
{
    return pt_socket->DnsResolveExt(name, result, len, timeout);
}

int DhcpSetDns(const char *addr)
{
    return pt_socket->DhcpSetDns(addr);
}

int  NetPing(char* dst_ip_str, long timeout, int size)
{
    return pt_socket->NetPing(dst_ip_str, timeout,size);
}

int  NetDevGet(int Dev,char *HostIp,char *Mask,char *GateWay,char *Dns)/* Dev Interface Index */ 
{
	if (Dev == WIFI_ROUTE_NUM)	//it's wifi
	{
		return s_wifi_net_func.NetDevGet(Dev,HostIp,Mask,GateWay,Dns);
	}
	else
	{
		return s_net_func.NetDevGet(Dev,HostIp,Mask,GateWay,Dns);
	}
}

int  NetSocket(int domain, int type, int protocol)
{
    return pt_socket->NetSocket(domain,type, protocol);
}

int  NetCloseSocket(int socket)
{
    return pt_socket->NetCloseSocket(socket);
}

int  Netioctl(int socket, int s_cmd, int arg)
{
    return pt_socket->Netioctl(socket, s_cmd, arg);
}

int  NetConnect(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
    return pt_socket->NetConnect( socket, addr, addrlen);
}

int  NetBind(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
    return pt_socket->NetBind(socket,  addr,  addrlen);
}

int  NetListen(int socket, int backlog)
{
    return pt_socket->NetListen( socket,  backlog);
}

int  NetAccept(int socket, struct net_sockaddr *addr, socklen_t *addrlen)
{
    return pt_socket->NetAccept( socket,  addr, addrlen);
}

int  NetSend(int socket, void *buf/* IN */, int size, int flags)
{
    return pt_socket->NetSend( socket, buf,  size,  flags);
}

int  NetSendto(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t addrlen)
{
    return pt_socket->NetSendto( socket, buf,  size,  flags, addr,  addrlen);
}

int  NetRecv(int socket, void *buf/* OUT */, int size, int flags)
{
    return pt_socket->NetRecv( socket, buf,  size,  flags);
}

int  NetRecvfrom(int socket, void *buf, int size, int flags,struct net_sockaddr *addr, socklen_t *addrlen)
{
    return pt_socket->NetRecvfrom( socket, buf,  size,  flags, addr, addrlen);
}

int  SockAddrSet(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */)
{
    return pt_socket->SockAddrSet(addr, ip_str,   port);
}

int  SockAddrGet(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */, unsigned short *port/* OUT */)
{
    return pt_socket->SockAddrGet(addr, ip_str, port);
}


void  NetGetVersion(char ip_ver[30])
{
    return pt_socket->NetGetVersion(ip_ver);
}

char  net_version_get(void)
{
    return pt_socket->net_version_get();
}

void s_initNet(void)
{
    return pt_socket->s_initNet();
}

int ip_forcesyn(void)
{
    return pt_socket->ip_forcesyn();
}

void EthSetRateDuplexMode(int mode)
{
    return pt_socket->EthSetRateDuplexMode(mode);
}

int  EthGetFirstRouteMac(const char *dest_ip, unsigned char *mac, int len)
{
    return pt_socket->EthGetFirstRouteMac(dest_ip, mac, len);
}

int  NetAddStaticArp(char *ip_str, unsigned char mac[6])
{
    return pt_socket->NetAddStaticArp(ip_str, mac);
}

/////////////////////////////////////////////////////////

int FsGetDiskInfo(int disk_num, FS_DISK_INFO *disk_info)
{
	if(disk_num == 3 && is_hasbase())
	{	
		return FsGetDiskInfo_proxy(disk_num,disk_info);
	}
	else
	{
		return FsGetDiskInfo_std(disk_num, disk_info);
	}
}


