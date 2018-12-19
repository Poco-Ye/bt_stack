#ifndef _Wl_FUN_
#define _Wl_FUN_

#ifndef uchar
#define uchar unsigned char
#endif
#ifndef uint
#define uint unsigned int
#endif
#ifndef ulong
#define ulong unsigned long
#endif
#ifndef ushort
#define ushort unsigned short
#endif


#ifndef	OK
	#define OK 0
#endif
#ifndef RET_OK 
	#define RET_OK 0
#endif

#ifndef	NET_ERR_TIMEOUT
	#define NET_ERR_TIMEOUT -13
#endif
#ifndef	NET_ERR_PPP
	#define NET_ERR_PPP -21
#endif
#ifndef	NET_ERR_CONN
	#define NET_ERR_CONN -6
#endif

#if 1
//模块口被占用
#define WL_RET_ERR_PORTINUSE 	-201
//模块没有应答
#define WL_RET_ERR_NORSP		-202
//模块应答错误
#define WL_RET_ERR_RSPERR		-203
//模块串口没有打开
#define WL_RET_ERR_PORTNOOPEN -204
//需要PIN码
#define WL_RET_ERR_NEEDPIN	-205
//需要PUK解PIN码
#define WL_RET_ERR_NEEDPUK	-206
//参数错误
#define WL_RET_ERR_PARAMER	-207
//密码错误
#define WL_RET_ERR_ERRPIN		-208
//没有插入SIM卡
#define	WL_RET_ERR_NOSIM      -209
//不支持的类型
#define WL_RET_ERR_NOTYPE		-210
//网络没有注册
#define WL_RET_ERR_NOREG		-211
//模块曾初始化
#define WL_RET_ERR_INIT_ONCE	-212
//没有连接
#define WL_RET_ERR_NOCONNECT  -213
//线路断开
//#define WL_RET_ERR_LINEOFF	-214
//没有socket可用
#define WL_RET_ERR_NOSOCKETUSE	-215
//超时
#define WL_RET_ERR_TIMEOUT		-216
//正在拨号中
#define WL_RET_ERR_CALLING 		-217
//重复的socket请求
#define WL_RET_ERR_REPEAT_SOCKET	-218
//socket 已经断开
#define WL_RET_ERR_SOCKET_DROP	-219
//socket 正在连接中
#define WL_RET_ERR_CONNECTING     -220
//socket 正在关闭
#define WL_RET_ERR_SOCKET_CLOSING	-221
//网络注册中
#define WL_RET_ERR_REGING			-222
//关闭串口出错
#define WL_RET_ERR_PORTCLOSE  	-223
//发送失败
#define WL_RET_ERR_SENDFAIL		-224
//错误的模块版本
#define WL_RET_ERR_MODEVER		-225
//拨号中
#define WL_RET_ERR_DIALING		-226
//挂机中
#define WL_RET_ERR_ONHOOK		-227
//发现模块复位
#define WL_RET_ERR_RST			-228

#define WL_RET_ERR_NOSIG         -229    /* Signal level is too  low */
//模块已下电	
#define WL_RET_ERR_POWEROFF      -230    /* Power Off */
//模块繁忙
#define WL_RET_ERR_BUSY			 -231
//打开物理串口错误
#define WL_RET_ERR_OPENPORT		-232			
// 模块没有初始化
#define WL_RET_ERR_NOINIT		-233

#define WL_RET_ERR_OTHER			-300
#endif
int		WlInit(const uchar *SimPin);
int		WlGetSignal(uchar * SingnalLevel);
int		WlPppLogin(const uchar * APNIn,const uchar * UidIn,const uchar * PwdIn,  long Auth, int TimeOut,int ALiveInterVal);
void	WlPppLogout (void);
int		WlPppCheck(void);
int		WlSendCmd(const uchar * ATstrIn, uchar *RspOut,ushort Rsplen, ushort TimeOut, ushort Mode);
void	WlSwitchPower( unsigned char Onoff );
int		WlOpenPort(void);
int		WlClosePort(void);
int WlSelBand(unsigned char mod);

//提供给monitor调用的

//获取模块版本
int WlGetModuleInfo(uchar *modinfo,uchar *imei);
/* 
tpmsg 返回模块类型信息 EXTB CDMA GPRS WCDM WIFI
modulemsg 从模块读到的信息
*/
void WlSetCtrl(unsigned char pin,unsigned char val);
int WlSelSim(uchar simnum);

void WlTcpRetryNum(int value);

void WlDetectTcpOpened(int value);


#define WLAN_PWR 		ptWlDrvConfig->pwr_port, ptWlDrvConfig->pwr_pin
#define WLAN_WAKE 		ptWlDrvConfig->wake_port, ptWlDrvConfig->wake_pin
#define WLAN_ONOFF 	ptWlDrvConfig->onoff_port, ptWlDrvConfig->onoff_pin
#define WLAN_DTR_CTS	ptWlDrvConfig->dtr_cts_port,ptWlDrvConfig->dtr_cts_pin
#define WLAN_DUAL_SIM	ptWlDrvConfig->dual_sim_port,ptWlDrvConfig->dual_sim_pin
#endif


