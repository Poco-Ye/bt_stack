#include <base.h>
#include "rpwlanapp.h"
#include "rpwifi.h"
#include "../../ipstack/include/netapi.h"
#include "../../comm/comm.h"
#include "../apis/wlanapp.h"
#include "../../dll/dll.h"

extern ST_WIFI_STATUS gszWifiStatus;
extern ST_WIFI_AP gszNowAp;
extern int giNowApChannel;
extern ST_WIFI_PARAM gszNowConnectParam;
extern int giWifiErrorCode;
extern volatile int giWifiIntCount;
extern volatile int giAsyMsgBufWrite;
extern uchar WifiSocketFifo[MAX_SOCKET*SOCKET_BUF_SIZE];
extern uint guiLock;

static int giNetBlockMode[MAX_SOCKET] = 
	{BLOCK,BLOCK,BLOCK,BLOCK,BLOCK,BLOCK,BLOCK,BLOCK};//Block Mode
static int giNetBlockTimeOut[MAX_SOCKET] = 
	{SOCKET_TIMEOUT,SOCKET_TIMEOUT,SOCKET_TIMEOUT,SOCKET_TIMEOUT,
	 SOCKET_TIMEOUT,SOCKET_TIMEOUT,SOCKET_TIMEOUT,SOCKET_TIMEOUT};//ms
static T_SOFTTIMER gszWifiAppTimer;
static ST_SOCKET_INFO  CommSocket[MAX_SOCKET];
static uchar *gucSocketFifo[MAX_SOCKET];
static int giSocketRead[MAX_SOCKET];
static int giSocketWrite[MAX_SOCKET];
static ushort gusUsedLocalPortList[3][MAX_SOCKET] = {0,};
static uchar gucErrorCode = 0;
static int giAsyMsgSize = 0;
static int giFindSocket=-1;
static int gucPortRxTempIndex=0;//gucPortRxTemp有效数据个数
static int guiIrqTimerStart = 0;//interrupt wait time
static uchar gucPortRxTemp[1024];
static uchar gucAsyMsgBuf[SOCKET_RECV_SIZE+100];
static volatile int rpwifi_softinterrupt_enable=0;
static uint guiAtFailCnt = 0;

static void s_RpIpToString(char *ip, char *ipstr);
static int s_RpStringToIp(char *ipstr, char *ip);
static int s_RpFindFreesocket(void);
int s_RpCheckSocket(void);
static int s_RpCheckSocketLeft(void);
static ushort s_RpGetNewPort(int type);
static int s_RpDeletePortInList(int type, ushort port);
static int s_RpPortAddInUsedList(int type, ushort port);
static int s_RpCheckPortIsUsed(int type, ushort port);
static void s_RpEscapeChar(uchar *Source, uchar *Result, int SourceLen, int *ResultLen);
static int s_RfBufOutfromArray(int socket, uchar *buf, int want_len);
static void s_RfBufInsertArray(int socket, uchar *buf, int len);
static void s_RpProcessFifo(void);
static void s_RpPortReinit(void);

void s_RpAtCmdFail(void);
uint s_RpGetAtCmdFailCnt(void);
void s_RpAtCmdFailCntReset(void);

void RpwifiSoftIntEnable(int enable)
{
	unsigned int flag;
	
	irq_save(flag);
	rpwifi_softinterrupt_enable = enable;
	irq_restore(flag);
}

int RpNetSocket(int domain, int type, int protocol)
{
	int socket;
	uchar Resp[200];
	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if (domain != NET_AF_INET)
	{
		return NET_ERR_VAL;
	}
	if (type != NET_SOCK_STREAM && type != NET_SOCK_DGRAM)
	{
		return NET_ERR_VAL;
	}
	if (protocol != 0)
	{
		return NET_ERR_VAL;
	}
	socket = s_RpFindFreesocket();
	if(socket == -1) 
	{
		return NET_ERR_MEM;
	}
	
	//when socket try to connect ,set pwmode to normal mode	
	RedpineWifiSendCmd("AT+RSI_PWMODE=0\r\n",0, Resp,4,50);
	
	CommSocket[socket].iUsed = 1;
	CommSocket[socket].type = type;
	CommSocket[socket].domaint= domain;
	CommSocket[socket].protocol = protocol;
	CommSocket[socket].localPort= 0;//no bind or connect
	CommSocket[socket].id= 0;//no listen or connect
	giSocketRead[socket] = 0;
	giSocketWrite[socket] = 0;
	giNetBlockMode[socket] = BLOCK;
	giNetBlockTimeOut[socket] = SOCKET_TIMEOUT;
	return socket;
}

int RpNetConnect(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
	uchar ATstrIn[100],Resp[200],ipstr[16];
	ushort port, localport;
	int iRet;

	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_VAL;
	}
	if (addrlen != sizeof(struct net_sockaddr))
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].type == NET_SOCK_DGRAM)//UDP
	{
		return NET_ERR_SYS;
	}
	if (CommSocket[socket].localPort != 0)//already connect
	{
		return NET_ERR_SYS;
	}
	
	memset(ATstrIn, 0, sizeof(ATstrIn));
	memset(Resp, 0, sizeof(Resp));
	memset(ipstr, 0, sizeof(ipstr));
	RpSockAddrGet(addr, ipstr, &port);
	localport = s_RpGetNewPort(PORT_TYPE_TCPCLIENT);
	sprintf(ATstrIn, "AT+RSI_TCP=%s,%d,%d\r\n",ipstr,port,localport);
	
	iRet = RedpineWifiSendCmd(ATstrIn,0, Resp,5,20000);
	if (strstr(Resp, "OK") == NULL)
	{
		s_RpSetErrorCode(Resp[5]);
		s_RpPortReinit();
		return NET_ERR_IF;
	}
	s_RpGetLocalIp(CommSocket[socket].localIp);
	CommSocket[socket].localPort = localport;
	CommSocket[socket].id = Resp[2];
	CommSocket[socket].role = SOCKET_ROLE_CLIENT;
	s_RpStringToIp(ipstr, CommSocket[socket].remoteIp);
	CommSocket[socket].remotePort = port;
	s_RpPortAddInUsedList(PORT_TYPE_TCPCLIENT,localport);
	
	return NET_OK;
}

int RpNetBind(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
    uchar ATstrIn[100],Resp[200],ipstr[16];
	ushort port;
	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (addr == NULL)
	{
		return NET_ERR_VAL;
	}
	if (addrlen != sizeof(struct net_sockaddr))
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].localPort != 0)//already bind
	{
		return NET_ERR_SYS;
	}
	memset(ipstr, 0, sizeof(ipstr));
	RpSockAddrGet(addr, ipstr, &port);
	s_RpStringToIp(ipstr, CommSocket[socket].localIp);
	
	if (CommSocket[socket].type == NET_SOCK_STREAM)
	{
		CommSocket[socket].role = SOCKET_ROLE_LISTEN;//server
		if(s_RpCheckPortIsUsed(PORT_TYPE_TCPSERVER, port)==1)
		{
			return NET_ERR_USE;
		}
		CommSocket[socket].localPort = port;
		s_RpPortAddInUsedList(PORT_TYPE_TCPSERVER,port);
	}
	else
	{
		if(s_RpCheckPortIsUsed(PORT_TYPE_UDP, port)==1)
		{
			return NET_ERR_USE;
		}
		CommSocket[socket].localPort = port;
		CommSocket[socket].role = SOCKET_ROLE_CLIENT;//server
		memset(ATstrIn, 0, sizeof(ATstrIn));
		sprintf(ATstrIn, "AT+RSI_LUDP=%d\r\n",CommSocket[socket].localPort);
		memset(Resp, 0, sizeof(Resp));
		RedpineWifiSendCmd(ATstrIn,0,Resp,5,100);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			return NET_ERR_IF;
		}
		CommSocket[socket].id = Resp[2];
		//when socket connected ,set pwmode to normal mode	
		RedpineWifiSendCmd("AT+RSI_PWMODE=0\r\n",0, Resp,4,50);
		s_RpPortAddInUsedList(PORT_TYPE_UDP,port);
	}
	
	return NET_OK;
}

int RpNetListen(int socket, int backlog)
{
	uchar ATstrIn[100],Resp[200];
	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].type == NET_SOCK_DGRAM)
	{
		return NET_ERR_SYS;
	}
	if (CommSocket[socket].role != SOCKET_ROLE_LISTEN)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].localPort == 0)//no bind
	{
		return NET_ERR_MEM;
	}
	if (CommSocket[socket].id != 0)//already listened
	{
		return NET_ERR_SYS;
	}
	if (backlog <= 0 || backlog > 5)
	{
		return NET_ERR_VAL;
	}
	memset(ATstrIn, 0, sizeof(ATstrIn));
	sprintf(ATstrIn, "AT+RSI_LTCP=%d\r\n",CommSocket[socket].localPort);
	memset(Resp, 0, sizeof(Resp));
	RedpineWifiSendCmd(ATstrIn,0,Resp,5,100);
	if (strstr(Resp, "OK") == NULL)
	{
		s_RpSetErrorCode(Resp[5]);
		return NET_ERR_IF;
	}
	CommSocket[socket].backlog = backlog;
	CommSocket[socket].id = Resp[2];
	memset(CommSocket[socket].remoteIp, 0, 4);
	CommSocket[socket].remotePort = 0;

	//when socket connected ,set pwmode to normal mode	
	RedpineWifiSendCmd("AT+RSI_PWMODE=0\r\n",0, Resp,4,50);
	return NET_OK;
}

int RpNetAccept(int socket, struct net_sockaddr *addr, socklen_t *addrlen)
{
	uchar ATstrIn[100],Resp[200],clientip[4],ipstr[20],buf[1024];
	ushort clientport;
	uint flag;
	int commsocket,len;
	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_CONN;
	}
	if (CommSocket[socket].role != SOCKET_ROLE_LISTEN)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].type == NET_SOCK_DGRAM)
	{
		return NET_ERR_SYS;
	}
	if (CommSocket[socket].localPort == 0)//no bind
	{
		return NET_ERR_MEM;
	}
	if (CommSocket[socket].id == 0)//no listened
	{
		return NET_ERR_SYS;
	}
	if (addr == NULL)
	{
		return NET_ERR_VAL;
	}
	if (addrlen == NULL)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].backlog == 0)
	{
		return NET_ERR_ABRT;
	}
	if (CommSocket[socket].id > 0)//listen socket is alived
	{
		memset(ATstrIn, 0, sizeof(ATstrIn));
		sprintf(ATstrIn, "AT+RSI_CTCP=%d\r\n",CommSocket[socket].id);

		if (giNetBlockTimeOut[socket] != 0)
		{
			s_TimerSetMs(&gszWifiAppTimer,giNetBlockTimeOut[socket]);
		}
		while(1)
		{
			if (giNetBlockTimeOut[socket] != 0)
			{
				if (s_TimerCheckMs(&gszWifiAppTimer) == 0)
				{
					return NET_ERR_TIMEOUT;
				}
			}
			memset(Resp, 0, sizeof(Resp));
			RedpineWifiSendCmd(ATstrIn,0, Resp, sizeof(Resp), 200);
			if (strstr(Resp, "OK") == NULL)
			{
				s_RpSetErrorCode(Resp[5]);
				return NET_ERR_IF;
			}
			if (Resp[2] == 0xff)
			{
				if (giNetBlockMode[socket] == NOBLOCK) return NET_ERR_AGAIN;
				continue;
			}
			else
			{
				WIFI_DEBUG_PRINT("[%s,%d]socket=%d,port=%d\r\n",FILELINE,Resp[2],Resp[7]+256*Resp[8]);
				WIFI_DEBUG_PRINT("[%s,%d]ip=%d.%d.%d.%d\r\n",FILELINE,Resp[3],Resp[4],Resp[5],Resp[6]);
				memcpy(clientip, Resp+3, 4);
				clientport = Resp[7]+256*Resp[8];
				break;
			}

		}
	}
	commsocket = RpNetSocket(NET_AF_INET, NET_SOCK_STREAM, 0);
	if (commsocket < 0)//创建通讯套接字失败，关闭连接
	{
		if (CommSocket[socket].id > 0)//listen socket is alived
		{
			memset(ATstrIn, 0, sizeof(ATstrIn));
			memset(Resp, 0, sizeof(Resp));
			sprintf(ATstrIn, "AT+RSI_CLS=%d\r", CommSocket[socket].id);
			RedpineWifiSendCmd(ATstrIn,0,Resp,4,20000);
			if (strstr(Resp, "OK") == NULL)
			{
				s_RpSetErrorCode(Resp[5]);
				return NET_ERR_IF;
			}
		}
		memset(ATstrIn, 0, sizeof(ATstrIn));
		memset(Resp, 0, sizeof(Resp));
		sprintf(ATstrIn, "AT+RSI_LTCP=%d\r\n",CommSocket[socket].localPort);
		RedpineWifiSendCmd(ATstrIn,0,Resp,5,100);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			return NET_ERR_IF;
		}
		CommSocket[socket].id = Resp[2];
		memset(CommSocket[socket].remoteIp, 0, 4);
		CommSocket[socket].remotePort = 0;
		return NET_ERR_BUF;
	}
	else
	{
		memcpy(CommSocket[commsocket].localIp, CommSocket[socket].localIp, 4);
		CommSocket[commsocket].localPort = CommSocket[socket].localPort;
		if (CommSocket[socket].id > 0)//listen socket is alived
		{
			memcpy(CommSocket[commsocket].remoteIp, clientip, 4);
			CommSocket[commsocket].remotePort = clientport;
		}
		else
		{
			memcpy(clientip, CommSocket[socket].remoteIp, sizeof(clientip));
			clientport = CommSocket[socket].remotePort;
			memcpy(CommSocket[commsocket].remoteIp, clientip, 4);
			CommSocket[commsocket].remotePort = clientport;
		}
		
		while(1)
		{
			/* becareful of socket data miss or corrupt,
			 * make sure no wifi interrupt pending and no socket transfer are processing 
			 */
			irq_save(flag);
            if(!giWifiIntCount && !giAsyMsgBufWrite)
            {                
                irq_restore(flag);
                break;			
            }    
			irq_restore(flag);
			DelayMs(5);
		}
        RpwifiSoftIntEnable(0);
		while(1)
		{	
			/* listen socket will reiceive data before accept it,
			 * copy data from listern socket to commsocket
			 */
			len = s_RfBufOutfromArray(socket, buf, sizeof(buf));
			if (len == 0) break;
			s_RfBufInsertArray(commsocket, buf, len);
		}
		/* socket change */
		if (CommSocket[socket].id > 0)//listen socket is alived
		{
			CommSocket[commsocket].id = CommSocket[socket].id;
		}
		else
		{
			CommSocket[commsocket].id = 0;
		}
		CommSocket[socket].id = 0;
        RpwifiSoftIntEnable(1);
		CommSocket[socket].backlog--;
		memset(ipstr, 0, sizeof(ipstr));
		s_RpIpToString(clientip, ipstr);
		RpSockAddrSet(addr, ipstr, clientport);
		*addrlen = sizeof(struct net_sockaddr);
		if (CommSocket[socket].backlog == 0)
		{
			return commsocket;
		}
		//原来的监听套接字变成了通讯套接字，需要重新建立一个监听套接字
		memset(ATstrIn, 0, sizeof(ATstrIn));
		memset(Resp, 0, sizeof(Resp));
		sprintf(ATstrIn, "AT+RSI_LTCP=%d\r\n",CommSocket[socket].localPort);
		RedpineWifiSendCmd(ATstrIn,0,Resp,5,100);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			memset(&CommSocket[socket], 0, sizeof(ST_SOCKET_INFO));
			return NET_ERR_BUF;
		}
		CommSocket[socket].id = Resp[2];
		memset(CommSocket[socket].remoteIp, 0, 4);
		CommSocket[socket].remotePort = 0;
	}
	
	return commsocket;
}

int RpNetSend(int socket, void *buf/* IN */, int size, int flags)
{
	uchar AtstrIn[SOCKET_SEND_SIZE+100],Resp[200],tmpbuf[SOCKET_SEND_SIZE];
	int bufsize,len,bufwrite;
	T_SOFTTIMER SocketSendTimer;

	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (buf == NULL)
	{
		return NET_ERR_VAL;
	}
	if (size <= 0 || size >= SOCKET_BUF_SIZE)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].role == 1)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].type == NET_SOCK_DGRAM)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].id == 0)
	{
		return NET_ERR_CLSD;
	}
	
	bufwrite = 0;
	s_TimerSetMs(&SocketSendTimer,3000);
	while(bufwrite < size)
	{
		s_RpEscapeChar((uchar *) (buf+bufwrite), tmpbuf, (size-bufwrite), &bufsize);
		memset(AtstrIn, 0, sizeof(AtstrIn));		
		sprintf(AtstrIn, "AT+RSI_SND=%d,%d,0,0,",CommSocket[socket].id,bufsize);
		len = strlen(AtstrIn);
		memcpy(AtstrIn+len, tmpbuf,bufsize);
		memcpy(AtstrIn+len+bufsize,"\r\n",2);
		len = len+bufsize+2;
		memset(Resp, 0, sizeof(Resp));
		
		RedpineWifiSendCmd(AtstrIn,len, Resp, 6, 10000);
		WIFI_DEBUG_PRINT("Resp=%s\r\n",Resp);
		if (strstr(Resp, "OK") != NULL)
		{
			bufwrite += (Resp[2] + 256*Resp[3]);
			s_TimerSetMs(&SocketSendTimer,3000);
		}
		else
		{
			if (Resp[5] != 0x8F)
			{
				s_RpSetErrorCode(Resp[5]);
				break;
			}
			else
			{
				//if the error=0x8F,try send again in 3 seconds
				if (!s_TimerCheckMs(&SocketSendTimer))
				{
					s_RpSetErrorCode(Resp[5]);
					break;
				}
			}
		}
	}
	if (!bufwrite)
	{
		return NET_ERR_CONN;
	}
	return bufwrite;
}
int RpNetSendto(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t addrlen)
{
	uchar AtstrIn[SOCKET_SEND_SIZE+100],Resp[200],ipstr[16],tmpbuf[SOCKET_SEND_SIZE];
	ushort port;
	int bufsize,len,bufwrite;
	struct net_sockaddr localaddr;
	T_SOFTTIMER SocketSendTimer;

	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (buf == NULL || addr == NULL)
	{
		return NET_ERR_VAL;
	}
	if (addrlen != sizeof(struct net_sockaddr))
	{
		return NET_ERR_VAL;
	}
	if (size <= 0 || size >= SOCKET_BUF_SIZE)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].role == 1)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].type == NET_SOCK_STREAM)
	{
		return NET_ERR_SYS;
	}
	
	memset(ipstr, 0, sizeof(ipstr));
	RpSockAddrGet(addr, ipstr, &port);
	if (CommSocket[socket].localPort == 0)//no bind
	{
		ushort localport;
		localport = s_RpGetNewPort(PORT_TYPE_UDP);
		RpSockAddrSet(&localaddr, "0.0.0.0", localport);
		RpNetBind(socket,&localaddr, sizeof(localaddr));
	}
	bufwrite = 0;
	s_TimerSetMs(&SocketSendTimer,3000);
	while(bufwrite < size)
	{
		s_RpEscapeChar((uchar *) (buf+bufwrite), tmpbuf, (size-bufwrite), &bufsize);
		WIFI_DEBUG_PRINT("[%s,%d]bufwrite=%d,bufsize=%d\r\n",FILELINE,bufwrite,bufsize);
		memset(AtstrIn, 0, sizeof(AtstrIn));
		memset(Resp, 0, sizeof(Resp));
		
		sprintf(AtstrIn, "AT+RSI_SND=%d,%d,%s,%d,",CommSocket[socket].id,bufsize,ipstr,port);
		len = strlen(AtstrIn);
		memcpy(AtstrIn+len, tmpbuf,bufsize);
		memcpy(AtstrIn+len+bufsize,"\r\n",2);
		len = len+bufsize+2;
		
		RedpineWifiSendCmd(AtstrIn,len, Resp, 6, 5000);
		WIFI_DEBUG_PRINT("[%s,%d]Resp=%s\r\n",FILELINE,Resp);
		if (strstr(Resp, "OK") != NULL)
		{
			bufwrite += (Resp[2] + 256*Resp[3]);
			s_TimerSetMs(&SocketSendTimer,3000);
		}
		else
		{
			if (Resp[5] != 0x8F)
			{
				s_RpSetErrorCode(Resp[5]);
				break;
			}
			else
			{
				//if the error=0x8F,try send again in 3 seconds
				if (!s_TimerCheckMs(&SocketSendTimer))
				{
					s_RpSetErrorCode(Resp[5]);
					break;
				}
			}
		}
	}
	s_RpStringToIp(ipstr, CommSocket[socket].remoteIp);
	CommSocket[socket].remotePort = port;
	if (!bufwrite)
	{
		return NET_ERR_CONN;
	}
	return bufwrite;
}

int RpNetRecv(int socket, void *buf/* OUT */, int size, int flags)
{
	volatile int iRet;
	int len=0,len_bak;
	static T_SOFTTIMER szWifiTimer;
	
	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (buf == NULL)
	{
		return NET_ERR_VAL;
	}
	if (size <= 0 || size >= SOCKET_BUF_SIZE)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].role == 1)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].type == NET_SOCK_DGRAM)
	{
		return NET_ERR_SYS;
	}
	if (CommSocket[socket].id == 0)
	{
		if (giSocketRead[socket] == giSocketWrite[socket])
		{
			return NET_ERR_CLSD;//the fifo is empty
		}
	}
	
	memset(buf, 0, size);
	if (giNetBlockMode[socket] == NOBLOCK)
	{
		iRet = s_RfBufOutfromArray(socket, buf, size);
		len = iRet;
	}
	else
	{
		if (giNetBlockTimeOut[socket] != 0)
		{
			s_TimerSetMs(&gszWifiAppTimer, giNetBlockTimeOut[socket]);
		}
		while(1)
		{
			if (giNetBlockTimeOut[socket] 
			    && !s_TimerCheckMs(&gszWifiAppTimer)) break;
			if (len == size) break;
			len_bak = len;
			s_TimerSetMs(&szWifiTimer, 200);
			while(2)
			{
				if (s_TimerCheckMs(&szWifiTimer)==0) break;
				iRet = s_RfBufOutfromArray(socket, buf+len, size-len);
				len += iRet;
                if (len == size) break;
			}
			if (len_bak == len && len != 0)//200ms间隔超时无数据，则退出
				break;
		}
		if (len == 0) len = NET_ERR_TIMEOUT;
	}
	return len;
}

int RpNetRecvfrom(int socket, void *buf, int size, int flags, 
		struct net_sockaddr *addr, socklen_t *addrlen)
{
	volatile int iRet;
	static T_SOFTTIMER szWifiTimer;
	int len_bak,len=0;
	uchar remoteip[20];
	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (buf == NULL)
	{
		return NET_ERR_VAL;
	}
	if (size <= 0 || size >= SOCKET_BUF_SIZE)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_CLSD;
	}
	if (CommSocket[socket].role == 1)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].type == NET_SOCK_STREAM)
	{
		return NET_ERR_SYS;
	}
	if (addr == NULL)
	{
		return NET_ERR_VAL;
	}
	if (addrlen == NULL)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].localPort == 0)//no bind
	{
		return NET_ERR_MEM;
	}
	
	memset(buf, 0, size);
	if (giNetBlockMode[socket] == NOBLOCK)
	{
		iRet = s_RfBufOutfromArray(socket, buf, size);
		len = iRet;
	}
	else
	{
		if (giNetBlockTimeOut[socket] != 0)
		{
			s_TimerSetMs(&gszWifiAppTimer, giNetBlockTimeOut[socket]);
		}
		while(1)
		{
			if (giNetBlockTimeOut[socket] 
			    && !s_TimerCheckMs(&gszWifiAppTimer)) break;
			if (len == size) break;
			len_bak = len;
			s_TimerSetMs(&szWifiTimer, 100);
			while(2)
			{
				if (s_TimerCheckMs(&szWifiTimer) == 0) break;
				iRet = s_RfBufOutfromArray(socket, buf+len, size-len);
				len += iRet;
			}
			if (len_bak == len && len != 0)//100ms间隔超时无数据，则退出
				break;
		}
		if (len == 0) len = NET_ERR_TIMEOUT;
	}
	memset(remoteip, 0, sizeof(remoteip));
	s_RpIpToString(CommSocket[socket].remoteIp, remoteip);
	RpSockAddrSet(addr, remoteip, CommSocket[socket].remotePort);
	return len;
}
int s_RpNetCloseAllSocket(void)
{
	uchar ATstrIn[100],Resp[200];
	int i;

	memset(ATstrIn, 0, sizeof(ATstrIn));
	memset(Resp, 0, sizeof(Resp));
	for (i = 0;i < MAX_SOCKET;i++)
	{
		if (CommSocket[i].iUsed == 1)
		{
			if(CommSocket[i].localPort != 0)
			{
				if (CommSocket[i].type == NET_SOCK_STREAM)
				{
					if (CommSocket[i].role == SOCKET_ROLE_LISTEN)
					{
						s_RpDeletePortInList(PORT_TYPE_TCPSERVER, CommSocket[i].localPort);
					}
					else
					{
						s_RpDeletePortInList(PORT_TYPE_TCPCLIENT, CommSocket[i].localPort);
					}
				}
				else if (CommSocket[i].type == NET_SOCK_DGRAM)
				{
					s_RpDeletePortInList(PORT_TYPE_UDP, CommSocket[i].localPort);
				}
			}

			if(CommSocket[i].id > 0)
			{
				sprintf(ATstrIn, "AT+RSI_CLS=%d\r\n", CommSocket[i].id);
				RedpineWifiSendCmd(ATstrIn,0,Resp,4,20000);
			}
		}
		
		memset(&CommSocket[i], 0, sizeof(CommSocket[i]));
		giNetBlockMode[i] = BLOCK;
		giNetBlockTimeOut[i] = SOCKET_TIMEOUT;
	}
}
int RpNetCloseSocket(int socket)
{
	uchar ATstrIn[100],Resp[200];
	int i,listensocket;
	int ret_flg = 0;
	
	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	memset(ATstrIn, 0, sizeof(ATstrIn));
	memset(Resp, 0, sizeof(Resp));
	if (CommSocket[socket].iUsed == 1)
	{
		if(CommSocket[socket].localPort != 0)
		{
			if (CommSocket[socket].type == NET_SOCK_STREAM)
			{
				if (CommSocket[socket].role == SOCKET_ROLE_LISTEN)
				{
					s_RpDeletePortInList(PORT_TYPE_TCPSERVER, CommSocket[socket].localPort);
				}
				else
				{
					s_RpDeletePortInList(PORT_TYPE_TCPCLIENT, CommSocket[socket].localPort);
				}
			}
			else if (CommSocket[socket].type == NET_SOCK_DGRAM)
			{
				s_RpDeletePortInList(PORT_TYPE_UDP, CommSocket[socket].localPort);
			}
		}

		if(CommSocket[socket].id > 0)
		{
			sprintf(ATstrIn, "AT+RSI_CLS=%d\r\n", CommSocket[socket].id);
			RedpineWifiSendCmd(ATstrIn,0,Resp,4,10000);
			if (strstr(Resp, "OK") == NULL)
			{
				s_RpSetErrorCode(Resp[5]);
				WIFI_DEBUG_PRINT("[%s-%d] ERROR = %d\r\n",__FUNCTION__, __LINE__,Resp[5]);
				ret_flg = 1;
			}
		}
	}
	i = MAX_SOCKET;
	if (CommSocket[socket].role == SOCKET_ROLE_CLIENT)
	{
		for (i=0;i<MAX_SOCKET;i++)
		{
            if (CommSocket[i].role != SOCKET_ROLE_LISTEN) continue;
            if (CommSocket[i].localPort == CommSocket[socket].localPort)
            {
                listensocket = i;//find the listen socket
                break;
            }
        }
    }

	memset(&CommSocket[socket], 0, sizeof(CommSocket[socket]));
	giNetBlockMode[socket] = BLOCK;
	giNetBlockTimeOut[socket] = SOCKET_TIMEOUT;
	if (i < MAX_SOCKET)
	{
		if (CommSocket[listensocket].backlog == 0)
		{
			//create new listener
			memset(ATstrIn, 0, sizeof(ATstrIn));
			memset(Resp, 0, sizeof(Resp));
			sprintf(ATstrIn, "AT+RSI_LTCP=%d\r\n",CommSocket[listensocket].localPort);
			RedpineWifiSendCmd(ATstrIn,0,Resp,5,100);
			if (strstr(Resp, "OK") == NULL)
			{
				memset(&CommSocket[listensocket], 0, sizeof(ST_SOCKET_INFO));
				return NET_OK;
			}
			CommSocket[listensocket].id = Resp[2];
			memset(CommSocket[socket].remoteIp, 0, 4);
			CommSocket[socket].remotePort = 0;
			CommSocket[listensocket].backlog++;//add backlog
		}
		else
		{
			CommSocket[listensocket].backlog++;//add backlog
		}
		
	}
	
	if (s_RpCheckSocket())
	{
		//when socket all closed ,set pwmode to power save mode	
		RedpineWifiSendCmd("AT+RSI_PWMODE=2\r\n",0, Resp,4,50);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			ret_flg = 1;
		}
	}
	return (ret_flg ? NET_ERR_IF : NET_OK);
}
int RpNetioctl(int socket, int cmd, int arg)
{
	int ret,len;
	uchar ATstrIn[100],Resp[200];

	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED)
	{
		return NET_ERR_IF;
	}
	if(socket < 0 || socket >= MAX_SOCKET)
	{
		return NET_ERR_VAL;
	}
	if (CommSocket[socket].iUsed == 0)
	{
		return NET_ERR_CLSD;
	}
	switch(cmd)
	{
		case CMD_IO_SET:
			if (arg != NOBLOCK && arg != BLOCK) return NET_ERR_ARG;
			giNetBlockMode[socket] = arg;
			return NET_OK;
		case CMD_IO_GET:
			return giNetBlockMode[socket];
		case CMD_TO_SET:
			if (giNetBlockMode[socket] != BLOCK) return NET_ERR_VAL;
            if (arg < 0) arg = 0;
            else if (arg > 120000) arg = 120000;//max timeout 2mins                
            giNetBlockTimeOut[socket] = arg;
			return NET_OK;
		case CMD_TO_GET:
			if (giNetBlockMode[socket] != BLOCK) return 0;
			return giNetBlockTimeOut[socket];
		case CMD_IF_SET:
			return NET_ERR_SYS;
		case CMD_IF_GET:
			return NET_ERR_SYS;
		case CMD_EVENT_GET:
			if (CommSocket[socket].type == NET_SOCK_DGRAM)
			{
				return NET_ERR_VAL;
			}
			if (arg == 0)
			{
				ret = 0;
				if (CommSocket[socket].role == SOCKET_ROLE_LISTEN)//Listen
				{
					if (CommSocket[socket].id > 0)
					{
						memset(ATstrIn, 0, sizeof(ATstrIn));
						memset(Resp, 0, sizeof(Resp));
						sprintf(ATstrIn, "AT+RSI_CTCP=%d\r\n",CommSocket[socket].id);
						RedpineWifiSendCmd(ATstrIn, 0, Resp, sizeof(Resp), 100);
						if (strstr(Resp, "OK") == NULL)
						{
							s_RpSetErrorCode(Resp[5]);
							return NET_ERR_IF;
						}
						if (Resp[2] != 0xff)
						{
							WIFI_DEBUG_PRINT("[%s,%d]socket=%d,port=%d\r\n",FILELINE,Resp[2],Resp[7]+256*Resp[8]);
							WIFI_DEBUG_PRINT("[%s,%d]ip=%d.%d.%d.%d\r\n",FILELINE,Resp[3],Resp[4],Resp[5],Resp[6]);
							ret |= SOCK_EVENT_ACCEPT;
							memcpy(CommSocket[socket].remoteIp, Resp+3, 4);
							CommSocket[socket].remotePort = Resp[7]+256*Resp[8];
						}
					}
					else if (CommSocket[socket].id == -1)
					{
						ret |= SOCK_EVENT_ACCEPT;
					}
					else
					{
						ret |= SOCK_EVENT_ERROR;
					}
				}
				else
				{
					if (CommSocket[socket].id > 0)
					{
						ret |= SOCK_EVENT_CONN;
					}
					else
					{
						ret |= SOCK_EVENT_ERROR;
					}
					if (giSocketRead[socket] != giSocketWrite[socket])
					{
						ret |= SOCK_EVENT_READ;
					}
					if (CommSocket[socket].id > 0 && CommSocket[socket].role == SOCKET_ROLE_CLIENT)
					{
						ret |= SOCK_EVENT_WRITE;
					}
				}
				return ret;
			}
			else if (arg == SOCK_EVENT_ACCEPT)
			{
				if (CommSocket[socket].role != SOCKET_ROLE_LISTEN) return 0;//not Listen

				if (CommSocket[socket].id > 0)
				{
					memset(ATstrIn, 0, sizeof(ATstrIn));
					memset(Resp, 0, sizeof(Resp));
					sprintf(ATstrIn, "AT+RSI_CTCP=%d\r\n",CommSocket[socket].id);
					RedpineWifiSendCmd(ATstrIn,0, Resp, sizeof(Resp), 100);
					if (strstr(Resp, "OK") == NULL)
					{
						s_RpSetErrorCode(Resp[5]);
						return NET_ERR_IF;
					}
					if (Resp[2] != 0xff)
					{
						WIFI_DEBUG_PRINT("[%s,%d]socket=%d,port=%d\r\n",FILELINE,Resp[2],Resp[7]+256*Resp[8]);
						WIFI_DEBUG_PRINT("[%s,%d]ip=%d.%d.%d.%d\r\n",FILELINE,Resp[3],Resp[4],Resp[5],Resp[6]);
						memcpy(CommSocket[socket].remoteIp, Resp+3, 4);
						CommSocket[socket].remotePort = Resp[7]+256*Resp[8];
						return 1;
					}
				}
				else if (CommSocket[socket].id == -1)
				{
					return 1;
				}
                return 0;
			}
			else if (arg == SOCK_EVENT_ERROR)
			{
				if (CommSocket[socket].role == SOCKET_ROLE_LISTEN)//Listen
				{
					if (CommSocket[socket].id > 0)
					{
						return 0;
					}
					else if (CommSocket[socket].id == -1)
					{
						return 0;
					}
					return 1;
				}
				else
				{
					if (CommSocket[socket].id > 0)
					{
						return 0;
					}
					else
					{
						return 1;
					}
				}
			}
			else if (arg == SOCK_EVENT_READ)
			{
				if (CommSocket[socket].role == SOCKET_ROLE_LISTEN)//Listen
				{
					return 0;
				}
				else
				{
					len = (giSocketWrite[socket]-giSocketRead[socket]+SOCKET_BUF_SIZE)%SOCKET_BUF_SIZE;
					return len;
				}
				
			}
			else if (arg == SOCK_EVENT_WRITE)
			{
				if (CommSocket[socket].role == SOCKET_ROLE_LISTEN)//Listen
				{
					return 0;
				}
				else
				{
					return SOCKET_BUF_SIZE-1;
				}
			}
			else
			{
				return NET_ERR_VAL;
			}
		case CMD_BUF_GET:
			if (CommSocket[socket].type == NET_SOCK_DGRAM)
			{
				return NET_ERR_VAL;
			}
			if (arg == TCP_SND_BUF_MAX)
			{
				return SOCKET_BUF_SIZE-1;
			}
			else if (arg == TCP_RCV_BUF_MAX)
			{
				return SOCKET_BUF_SIZE-1;
			}
			else if (arg == TCP_SND_BUF_AVAIL)
			{
				return SOCKET_BUF_SIZE-1;
			}
			else
			{
				return NET_ERR_VAL;
			}
		case CMD_FD_GET:
			return s_RpCheckSocketLeft();
		case CMD_KEEPALIVE_SET:
			return NET_ERR_SYS;
		case CMD_KEEPALIVE_GET:
			return NET_ERR_SYS;
		default:
			return NET_ERR_VAL;
	}
}

int s_RpStringToIp(char *ipstr, char *ip);
int RpSockAddrSet(struct net_sockaddr *addr/* OUT */, char *ip_str/* IN */, unsigned short port/* IN */)
{
	char ip[4];
	if (addr == NULL)
	{
		return NET_ERR_ARG;
	}
	memset(addr->sa_data, 0, sizeof(addr->sa_data));
	addr->sa_len = sizeof(struct net_sockaddr);
	addr->sa_family = NET_AF_INET;
	addr->sa_data[0] = (port) / 256;
	addr->sa_data[1] = (port) % 256;
	if (ip_str != NULL)
	{
		if (strcmp(ip_str, "255.255.255.255") == 0)
		{
			return NET_ERR_VAL;
		}
		if(s_RpStringToIp(ip_str, ip))
		{
			return NET_ERR_VAL;
		}
	}
	else
	{
		if(s_RpStringToIp("0.0.0.0", ip))
		{
			return NET_ERR_VAL;
		}
	}
	
	memcpy(addr->sa_data+2, ip, sizeof(ip));
	return NET_OK;
}

int RpSockAddrGet(struct net_sockaddr *addr/* IN */, char *ip_str/* OUT */,unsigned short *port/* OUT */)
{
	char ip[4];
	char ipstr[16];
	if (addr == NULL || ip_str == NULL || port == NULL)
	{
		return NET_ERR_VAL;
	}
	*port = (uchar)addr->sa_data[0]*256+(uchar)addr->sa_data[1];
	ip[0] = addr->sa_data[2];
	ip[1] = addr->sa_data[3];
	ip[2] = addr->sa_data[4];
	ip[3] = addr->sa_data[5];
	memset(ipstr, 0, sizeof(ipstr));
	s_RpIpToString(ip, ipstr);
	strcpy(ip_str, ipstr);
	return NET_OK;
}

static void s_RpIpToString(char *ip, char *ipstr)
{
	sprintf(ipstr,"%d.%d.%d.%d",(uchar)ip[0],(uchar)ip[1],(uchar)ip[2],(uchar)ip[3]);
}

static void s_RpIpToStringExt(char *ip, char *ipstr)
{
   
   memset(ipstr,0,16);
    sprintf(ipstr,"%d.%d.%d.%d%d.%d.%d.%d",(uchar)ip[0],(uchar)ip[1],(uchar)ip[2],(uchar)ip[3],(uchar)ip[4],(uchar)ip[5],(uchar)ip[6],(uchar)ip[7]);
   
}

int s_RpCheckStrValid(char *str)
{
	while((*str)!=0)
	{
		if ((*str) < '0' || (*str) > '9')
		{
			return -1;
		}
		str++;
	}
	return 0;
}
static int s_RpStringToIp(char *ipstr, char *ip)
{
	char temp[4];
	char *pStr1, *pStr2;
	int i = 0;
	pStr1 = ipstr;
	for (i = 0;i < 3;i++)
	{
		memset(temp, 0, sizeof(temp));
		pStr2 = strstr(pStr1, ".");
		if (pStr2 == NULL) return -1;
		memcpy(temp, pStr1, pStr2 - pStr1);
		if (s_RpCheckStrValid(temp)) return -1;
		ip[i] = atoi(temp);
		pStr1 = pStr2 + strlen(".");
	}
	memset(temp, 0, sizeof(temp));
	strcpy(temp, pStr1);
	if (s_RpCheckStrValid(temp)) return -1;
	ip[3] = atoi(temp);
	return 0;
}

int RpDnsResolveGet(char *name/*IN*/, char *result/*OUT*/, int len, int min_len, int timeout)
{

#define MIN_DNS_TIME    1500
#define MAX_DNS_TIME    7000

	uchar ATstrIn[400],Resp[200];
	uchar temp[21],name_max[256];
	int iRet,name_len;
	int tmpTime = 3000;
	int Ip_Count;
    uchar *ptr_resp;
    uint t0,t1,tout,i,tmp;
	uchar ip_maxnum = 0;
	 
	
	name_len = strlen(name);
	if (name_len > 42) return NET_ERR_STR;
	
    if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED) return NET_ERR_IF;
	if (name == NULL || result == NULL || len < min_len) return NET_ERR_VAL;
    if(timeout < MIN_DNS_TIME) timeout = MIN_DNS_TIME;
    if(timeout > 3600000) timeout = 3600000;
	if(min_len == 16)
		len = 16;

	if(len > 20)
	{
		ip_maxnum = (((len/20) >= 10) ? 10 : (len/20));
	}else{
		ip_maxnum = 1;
	}

	memset(ATstrIn, 0, sizeof(ATstrIn));
	memset(Resp, 0, sizeof(Resp));
	memset(name_max, 0, sizeof(name_max));
	memcpy(name_max, name, name_len);
	sprintf(ATstrIn, "AT+RSI_DNSGET=%s\r\n",name_max);
    
	t0 = GetTimerCount();
	tout = (timeout + 9)/10*10;//to be multiples of 10ms for compatibility
    while(1)
	{
        t1 = GetTimerCount();
        memset(Resp,0,sizeof(Resp));    
		iRet = RedpineWifiSendCmd(ATstrIn, 0, Resp, (ip_maxnum*4 + 5), tmpTime);
		
        if(Resp[0]=='O' && Resp[1]=='K')break;
       
        if(GetTimerCount()-t1 < 500)
        {
            if(GetTimerCount()-t0+1000 > tout)
                return NET_ERR_TIMEOUT;
            DelayMs(1000);
        }
               
        tmpTime += MIN_DNS_TIME;       
        if(tmpTime > MAX_DNS_TIME)
            tmpTime = MAX_DNS_TIME;        
            
        tmp = GetTimerCount()-t0;
        if( tmp < tout)
        {
            i = tout-tmp;
            if(tmpTime > i)
                tmpTime = i;      
            continue;
        }
        return NET_ERR_TIMEOUT;
	}
	
	memset(result, 0, len);
    Ip_Count = Resp[2];
    if(Ip_Count ==0)
       return NET_ERR_DNS;
	
	if(Ip_Count >= ip_maxnum)
		Ip_Count = ip_maxnum;
    ptr_resp = Resp+3;
    
    for(i=0;i<Ip_Count;i++)
    {
        s_RpIpToString(ptr_resp, temp);
        ptr_resp += 4;
        strcpy(result + i*20, temp);    
    }
	return NET_OK;
}


int RpDnsResolve(char *name, char *result, int len)
{
    return RpDnsResolveGet(name,result,len,16,30*1000);
}

int RpDnsResolveExt(char *name/*IN*/, char *result/*OUT*/, int len, int timeout)
{
    return RpDnsResolveGet(name,result,len,20,timeout);
}

int RpNetPing(char *dst_ip_str, long timeout, int size)
{
	//redpine 不支持ping功能
	return NET_ERR_IF;
}
int RpRouteSetDefault(int ifIndex)
{
	return NET_OK;
}
int RpRouteGetDefault(void)
{
	return WIFI_ROUTE_NUM;
}
int RpNetDevGet(int Dev,char *HostIp,char *Mask, char *GateWay,char *Dns)
{
	int No_Open_Socket;
	uchar Ssid[32],Resp[1024];

	if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED) return NET_ERR_IF;
	if (Dev != WIFI_ROUTE_NUM) return NET_ERR_IF;
	
	memset(Resp, 0, sizeof(Resp));
	RedpineWifiSendCmd("AT+RSI_NWPARAMS?\r\n",0, Resp, sizeof(Resp), 100);
	if (strstr(Resp, "OK") == NULL)
	{
		s_RpSetErrorCode(Resp[5]);
		return NET_ERR_IF;
	}
	memcpy(Ssid, Resp+2, sizeof(Ssid));
	if (strlen(Ssid) == 0)
	{
		RedpineWifiSendCmd("AT+RSI_PWMODE=2\r\n",0, Resp, 4, 50);
		memset(gszWifiStatus.HostAddr, 0, sizeof(gszWifiStatus.HostAddr));
		gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
		return NET_ERR_IF;
	}
	No_Open_Socket = Resp[118];
	giNowApChannel = Resp[98];
	if (HostIp) s_RpIpToString(Resp+106, HostIp);
	if (Mask) s_RpIpToString(Resp+110, Mask);
	if (GateWay) s_RpIpToString(Resp+114, GateWay);
	if (Dns) s_RpIpToString(Resp+119+No_Open_Socket*10, Dns);
	return NET_OK;
}

int s_rpwifi_mac_get(unsigned char mac[6])
{
	uchar ATstrIn[20],Resp[20];
	if ((gszWifiStatus.WState != WIFI_STATUS_NOTCONN) 
	    && (gszWifiStatus.WState != WIFI_STATUS_CONNECTED))
	{
		return NET_ERR_IF;
	}
	memset(ATstrIn, 0, sizeof(ATstrIn));
	memset(Resp, 0, sizeof(Resp));
	sprintf(ATstrIn, "AT+RSI_MAC?\r\n");
	RedpineWifiSendCmd(ATstrIn,0,Resp,10,50);
	if (strstr(Resp, "OK") == NULL)
	{
		s_RpSetErrorCode(Resp[5]);
		return NET_ERR_IF;
	}
	memcpy(mac, Resp+2, 6);
	return NET_OK;
}

void s_RpSocketInit(void)
{
	int i;
	memset(CommSocket, 0, sizeof(CommSocket));
	for (i = 0;i < MAX_SOCKET;i++)
	{
		gucSocketFifo[i] = WifiSocketFifo+i*SOCKET_BUF_SIZE;
		memset(gucSocketFifo[i], 0, SOCKET_BUF_SIZE);
	}
	memset(giSocketRead, 0, sizeof(giSocketRead));
	memset(giSocketWrite, 0, sizeof(giSocketWrite));
	for (i = 0;i < MAX_SOCKET;i++)
	{
		giNetBlockMode[i] = BLOCK;
		giNetBlockTimeOut[i] = SOCKET_TIMEOUT;
	}
	s_RpPortInit();
}
int s_RpFindSocket(int id)
{
	int i = 0;
	for (i = 0;i < MAX_SOCKET;i++)
	{
		if (CommSocket[i].id == id)
		{
			return i;
		}
	}
	return -1;
}
static int s_RpFindFreesocket(void)
{
	int i = 0;
	for (i = 0;i < MAX_SOCKET;i++)
	{
		if (CommSocket[i].iUsed == 0)
		{
			return i;
		}
	}
	return -1;
}
static void s_RfBufInsertArray(int socket, uchar *buf, int len)
{
	if (giSocketWrite[socket] + len >= SOCKET_BUF_SIZE)
	{
		int len1, len2;
		len1 = SOCKET_BUF_SIZE - giSocketWrite[socket];
		len2 = len - len1;
		memcpy(&gucSocketFifo[socket][giSocketWrite[socket]], buf, len1);
		giSocketWrite[socket] = 0;
		memcpy(&gucSocketFifo[socket][giSocketWrite[socket]], buf+len1, len2);
		giSocketWrite[socket] += len2;
	}
	else
	{
		memcpy(&gucSocketFifo[socket][giSocketWrite[socket]], buf, len);
		giSocketWrite[socket] += len;
	}
}
static int s_RfBufOutfromArray(int socket, uchar *buf, int want_len)
{
	int canread_len;
	canread_len = (giSocketWrite[socket]-giSocketRead[socket]+SOCKET_BUF_SIZE) % SOCKET_BUF_SIZE;
	
	if (canread_len < want_len)
	{
		want_len = canread_len;
	}
	if (giSocketRead[socket] + want_len >= SOCKET_BUF_SIZE)
	{
		int len1, len2;
		len1 = SOCKET_BUF_SIZE - giSocketRead[socket];
		len2 = want_len - len1;
		memcpy(buf, &gucSocketFifo[socket][giSocketRead[socket]], len1);
		giSocketRead[socket] = 0;
		memcpy(buf+len1, &gucSocketFifo[socket][giSocketRead[socket]], len2);
		giSocketRead[socket] += len2;
	}
	else
	{
		memcpy(buf, &gucSocketFifo[socket][giSocketRead[socket]], want_len);
		giSocketRead[socket] += want_len;
	}
	return want_len;
}

void s_WifiIconReversal(void)
{
	static int flag = 0;
	if (flag == 0)
	{
//		ScrSetIcon(ICON_WIFI,0);
		flag = 1;
	}
	else
	{
//		ScrSetIcon(ICON_WIFI,3);
		flag = 0;
	}
}

void s_RpUpdateIcon(void)
{
    static ushort gusTimerCount=0, TryCount=0;
    uchar Resp[6];
    int signal_level,iRet;

	if (gszWifiStatus.WState == WIFI_STATUS_CONNECTED)
	{
		gusTimerCount++;
		if (gusTimerCount == 999)
		{
			//TryLock用于与SendCmd进行互斥，保证at指令响应不受异步事件干扰
			if (TryLock(&guiLock) == 0)
			{
				//正在执行SendCmd函数,10ms后继续检测
				gusTimerCount--;
				return;
			}
			//获取锁成功，SendCmd将会进入等待状态
			if(giWifiIntCount)
			{
				//有at+rsi_read或at+rsi_close事件，优先处理事件
				gusTimerCount--;
				UnLock(&guiLock);
				return;
			}
			s_RpWifiClearFifo();//clean rx fifo
			PortSends(P_WIFI, "AT+RSI_RSSI?\r\n",14);
			CMD_DEBUG_PRINT("[%s,%d]ATCMD:AT+RSI_RSSI?\r\n",FILELINE);
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
			TryCount = 0;
		}
		else if (gusTimerCount == 1000)// 10seconds get the signal
		{
			memset(Resp, 0, sizeof(Resp));
			iRet = s_RpWifiReadFromFifo(Resp, 5);
			if (strstr(Resp, "OK") == NULL)
			{
				//未等待at指令响应，10ms后继续获取
				gusTimerCount--;
				TryCount++;
				if (TryCount >= 100)//响应丢失
				{
					gusTimerCount = 0;
					UnLock(&guiLock);
				}
				return;
			}
			//获取成功，重置时间，刷新图标，解锁
			gusTimerCount = 0;			
			gszNowAp.Rssi = -Resp[2];/*Resp=OK<Rssi1byte>\r\n*/
			signal_level = WifiRssiToSignal(gszNowAp.Rssi);
//			ScrSetIcon(ICON_WIFI,signal_level);
			UnLock(&guiLock);
		}
	}
	else
	{
		gusTimerCount++;
		if (gusTimerCount >= 100)
		{
			//未连接热点时反转图标
			gusTimerCount = 0;
			if (guiLock)
			{
				UnLock(&guiLock);//防止查询信号未完成时已断开连接,则无法获取到锁
			}
			s_WifiIconReversal();
		}
	}
}

#define MAX_SCAN_RETRY_CNT          3

void s_RpWifiBackgroud(void)
{
	int iRet;
	static int auth_error_cnt = 0;
    static int scan_error_cnt = 0;
	static int timercount = 0;
	static int BackgroudConnectFail = 0;//if connect fail, then set pwmode=2
    static int giBackgroudSendFlag = 0;
    static int giBgRespIndex=0;    
    static uchar gucBgATstrIn[200], gucBgResp[200];

	if (s_RpGetAtCmdFailCnt() >= CMDFAIL_MAX_CNT)
	{
		gszWifiStatus.WState = WIFI_STATUS_RESET;//AT指令失败次数过多，做软件复位
	}
	if (gszWifiStatus.WState == WIFI_STATUS_SCAN)
	{
		//quickly scan
		if (giBackgroudSendFlag == 0)
		{
			memset(gucBgATstrIn, 0, sizeof(gucBgATstrIn));
			memset(gucBgResp, 0, sizeof(gucBgResp));
			giBgRespIndex = 0;
			sprintf(gucBgATstrIn, "AT+RSI_SCAN=%d,%s\r\n",giNowApChannel,gszNowAp.Ssid);
			PortReset(P_WIFI);
			PortSends(P_WIFI, gucBgATstrIn, strlen(gucBgATstrIn));
			giBackgroudSendFlag = 1;
			timercount = 0;
		}
		else
		{
			timercount++;
			//ERROR<1B>\r\n
			//OK<ssid 32B><Sec 1B><Rssi 1B>\r\n
			iRet = PortRecvs(P_WIFI, gucBgResp+giBgRespIndex, sizeof(gucBgResp), 0);
			if (iRet > 0)
			{
				giBgRespIndex += iRet;
			}
			if (timercount >= 1000)/*>=10s,SCAN FAIL*/
			{
				s_RpAtCmdFail();
				giBackgroudSendFlag = 0;
				if(gszNowAp.SecMode == WLAN_SEC_WEP)
                    gszWifiStatus.WState = WIFI_STATUS_SETAUTHMODE;
                else
					gszWifiStatus.WState = WIFI_STATUS_CONNECTING;
				giNowApChannel = 0; //scan error ,maybe the Channel is changed,then use the default
                giWifiErrorCode = WIFI_RET_ERROR_NORESPONSE;
				return;
			}
			if (paxstrnstr(gucBgResp, "\r\n", giBgRespIndex, 2) != NULL)
			{
				//OK or ERROR
				WIFI_DEBUG_PRINT("[%s,%d]gucBgResp=%s\r\n",FILELINE,gucBgResp);
				if (paxstrnstr(gucBgResp, "OK",giBgRespIndex, 2) == NULL)
				{
					s_RpSetErrorCode(gucBgResp[5]);
					giBackgroudSendFlag = 0;
					//scan error ,maybe the Channel is changed,then use the default
					//and try to scan again
					if (giNowApChannel != 0 || scan_error_cnt < MAX_SCAN_RETRY_CNT)
					{
                        scan_error_cnt++;
						giNowApChannel = 0;
						gszWifiStatus.WState = WIFI_STATUS_SCAN;
					}
					else
					{
						//the channel is the 0,but cann't find the AP
						gszWifiStatus.WState = WIFI_STATUS_DISCON;
						giWifiErrorCode = WIFI_RET_ERROR_NOAP;
                        scan_error_cnt = 0;
					}
				}
				else
				{
					if (giBgRespIndex < SSID_MAXLEN+6)
					{
						//try to scan again
						giBackgroudSendFlag = 0;
						giNowApChannel = 0;
						gszWifiStatus.WState = WIFI_STATUS_SCAN;
					}
					else
					{
						//one or more SSID
                        scan_error_cnt = 0;
						giBackgroudSendFlag = 0;
                        if(gszNowAp.SecMode == WLAN_SEC_WEP)
                            gszWifiStatus.WState = WIFI_STATUS_SETAUTHMODE;
                        else
                            gszWifiStatus.WState = WIFI_STATUS_CONNECTING;
					}
				}
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
			
		}
	}
	if (gszWifiStatus.WState == WIFI_STATUS_SETAUTHMODE)
	{
        if (gszNowAp.SecMode != WLAN_SEC_WEP)
        {
            gszWifiStatus.WState = WIFI_STATUS_CONNECTING;
            return ;
        }

		if (giBackgroudSendFlag == 0)
		{
			memset(gucBgATstrIn, 0, sizeof(gucBgATstrIn));
			memset(gucBgResp, 0, sizeof(gucBgResp));
			giBgRespIndex = 0;
        
            if (auth_error_cnt % 2)
                sprintf(gucBgATstrIn, "AT+RSI_AUTHMODE=1\r\n");
            else
                sprintf(gucBgATstrIn, "AT+RSI_AUTHMODE=0\r\n");

			WIFI_DEBUG_PRINT("[%s,%d]gucBgATstrIn=%s\r\n",FILELINE,gucBgATstrIn);
			PortReset(P_WIFI);
			CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,gucBgATstrIn);
			PortSends(P_WIFI, gucBgATstrIn, strlen(gucBgATstrIn));
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
			giBackgroudSendFlag = 1;
			timercount = 0;
		}
		else
		{
			timercount++;
			iRet = PortRecvs(P_WIFI, gucBgResp+giBgRespIndex, sizeof(gucBgResp), 0);
			if (iRet > 0)
			{
				giBgRespIndex += iRet;
			}
			if (timercount >= 1500)//15s
			{
				s_RpAtCmdFail();
				WIFI_DEBUG_PRINT("[%s,%d]timeout\r\n",FILELINE);

                auth_error_cnt = 0;
				giBackgroudSendFlag = 0;
                gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
				giWifiErrorCode = WIFI_RET_ERROR_NORESPONSE;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
			if (paxstrnstr(gucBgResp, "\r\n", giBgRespIndex, 2) != NULL)
			{
				WIFI_DEBUG_PRINT("[%s,%d]gucBgResp=%s\r\n",FILELINE,gucBgResp);
				if (paxstrnstr(gucBgResp, "OK",giBgRespIndex, 2) == NULL)
				{
					auth_error_cnt = 0;
					s_RpSetErrorCode(gucBgResp[5]);
					giBackgroudSendFlag = 0;
                    gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
				}
				else
				{
					giBackgroudSendFlag = 0;
                    gszWifiStatus.WState = WIFI_STATUS_CONNECTING;
				}
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
		}
	}
	if (gszWifiStatus.WState == WIFI_STATUS_CONNECTING)
	{
		//connect ap
		if (giBackgroudSendFlag == 0)
		{
			memset(gucBgATstrIn, 0, sizeof(gucBgATstrIn));
			memset(gucBgResp, 0, sizeof(gucBgResp));
			giBgRespIndex = 0;
			sprintf(gucBgATstrIn, "AT+RSI_JOIN=%s,0,2\r\n",gszNowAp.Ssid);
			WIFI_DEBUG_PRINT("[%s,%d]gucBgATstrIn=%s\r\n",FILELINE,gucBgATstrIn);
			PortReset(P_WIFI);
			CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,gucBgATstrIn);
			PortSends(P_WIFI, gucBgATstrIn, strlen(gucBgATstrIn));
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
			giBackgroudSendFlag = 1;
			timercount = 0;
		}
		else
		{
			timercount++;
			iRet = PortRecvs(P_WIFI, gucBgResp+giBgRespIndex, sizeof(gucBgResp), 0);
			if (iRet > 0)
			{
				giBgRespIndex += iRet;
			}
			if (timercount >= 1500)//15s
			{
				s_RpAtCmdFail();
				WIFI_DEBUG_PRINT("[%s,%d]timeout\r\n",FILELINE);
				giBackgroudSendFlag = 0;
				gszWifiStatus.WState = WIFI_STATUS_DISCON;
				giWifiErrorCode = WIFI_RET_ERROR_NORESPONSE;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
			if (paxstrnstr(gucBgResp, "\r\n", giBgRespIndex, 2) != NULL)
			{
				WIFI_DEBUG_PRINT("[%s,%d]gucBgResp=%s\r\n",FILELINE,gucBgResp);
				if (paxstrnstr(gucBgResp, "OK",giBgRespIndex, 2) == NULL)
				{
					s_RpSetErrorCode(gucBgResp[5]);
					giBackgroudSendFlag = 0;
					
					if (auth_error_cnt < 6)
					{
						//connect fail
						gszWifiStatus.WState = WIFI_STATUS_SCAN;
                        if (gszNowAp.SecMode == WLAN_SEC_WEP)
                            auth_error_cnt ++;
                        else
                            auth_error_cnt += 2;
					}
					else
					{
						//connect fail
						gszWifiStatus.WState = WIFI_STATUS_DISCON;
						auth_error_cnt = 0;
						if (gucBgResp[5]== 0xFD || gucBgResp[5] == 0xEB)
						{
							giWifiErrorCode = WIFI_RET_ERROR_AUTHERROR;
						}
						else
						{
							giWifiErrorCode = WIFI_RET_ERROR_NOAP;
						}
					}
					giNowApChannel = 0;
				}
				else
				{
					auth_error_cnt = 0;
					giBackgroudSendFlag = 0;
					if (gszNowConnectParam.DhcpEnable)
					{
						gszWifiStatus.WState = WIFI_STATUS_IPCONF;
					}
					else
					{
						gszWifiStatus.WState = WIFI_STATUS_SETDNS;
					}
				}
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
		}
	}
	if (gszWifiStatus.WState == WIFI_STATUS_SETDNS)
	{
		if (giBackgroudSendFlag == 0)
		{
			uchar Dns[16];
			memset(Dns, 0, sizeof(Dns));
			s_RpIpToString(gszNowConnectParam.Dns, Dns);
			memset(gucBgATstrIn, 0, sizeof(gucBgATstrIn));
			memset(gucBgResp, 0, sizeof(gucBgResp));
			giBgRespIndex = 0;
			sprintf(gucBgATstrIn, "AT+RSI_DNSSERVER=%s\r\n",Dns);
			PortReset(P_WIFI);
			CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,gucBgATstrIn);
			PortSends(P_WIFI, gucBgATstrIn, strlen(gucBgATstrIn));
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
			giBackgroudSendFlag = 1;
			timercount = 0;
		}
		else
		{
			timercount++;
			iRet = PortRecvs(P_WIFI, gucBgResp+giBgRespIndex, sizeof(gucBgResp), 0);
			if (iRet > 0)
			{
				giBgRespIndex += iRet;
			}
			if (timercount >= 500)//5s
			{
				WIFI_DEBUG_PRINT("[%s,%d]timeout\r\n",FILELINE);
				giBackgroudSendFlag = 0;
				gszWifiStatus.WState = WIFI_STATUS_DISCON;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
			if (paxstrnstr(gucBgResp, "\r\n", giBgRespIndex, 2) != NULL)
			{
				WIFI_DEBUG_PRINT("[%s,%d]gucBgResp=%s\r\n",FILELINE,gucBgResp);
				if (paxstrnstr(gucBgResp, "OK",giBgRespIndex, 2) == NULL)
				{
					s_RpSetErrorCode(gucBgResp[5]);
					giBackgroudSendFlag = 0;
					gszWifiStatus.WState = WIFI_STATUS_DISCON;
				}
				else
				{
					giBackgroudSendFlag = 0;
					gszWifiStatus.WState = WIFI_STATUS_IPCONF;
				}
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
		}
	}
	if (gszWifiStatus.WState == WIFI_STATUS_IPCONF)
	{
		//set ip or dhcp
		if (giBackgroudSendFlag == 0)
		{
			memset(gucBgATstrIn, 0, sizeof(gucBgATstrIn));
			memset(gucBgResp, 0, sizeof(gucBgResp));
			giBgRespIndex = 0;
				
			if (gszNowConnectParam.DhcpEnable)
			{
				sprintf(gucBgATstrIn, "AT+RSI_IPCONF=1,0,0,0\r\n");
			}
			else
			{
				uchar staticip[16],Subnet[16],Gateway[16];
				memset(staticip, 0, sizeof(staticip));
				memset(Subnet, 0, sizeof(Subnet));
				memset(Gateway, 0, sizeof(Gateway));
				s_RpIpToString(gszNowConnectParam.Ip, staticip);
				s_RpIpToString(gszNowConnectParam.Mask, Subnet);
				s_RpIpToString(gszNowConnectParam.Gate, Gateway);
				sprintf(gucBgATstrIn, "AT+RSI_IPCONF=0,%s,%s,%s\r\n",staticip,Subnet,Gateway);
			}
			PortReset(P_WIFI);
			CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,gucBgATstrIn);
			PortSends(P_WIFI, gucBgATstrIn, strlen(gucBgATstrIn));
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
			giBackgroudSendFlag = 1;
			timercount = 0;
		}
		else
		{
			timercount++;
			iRet = PortRecvs(P_WIFI, gucBgResp+giBgRespIndex, sizeof(gucBgResp), 0);
			if (iRet > 0)
			{
				giBgRespIndex += iRet;
			}
			if (timercount >= 3500)//35s
			{
				s_RpAtCmdFail();
				WIFI_DEBUG_PRINT("[%s,%d]timeout\r\n",FILELINE);
				giBackgroudSendFlag = 0;
				gszWifiStatus.WState = WIFI_STATUS_DISCON;
				giWifiErrorCode = WIFI_RET_ERROR_NORESPONSE;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
			if (paxstrnstr(gucBgResp, "\r\n", giBgRespIndex, 2) != NULL)
			{
				WIFI_DEBUG_PRINT("[%s,%d]gucBgResp=%s\r\n",FILELINE,gucBgResp);
				if (paxstrnstr(gucBgResp, "OK",giBgRespIndex, 2) == NULL)
				{
					s_RpAtCmdFail();
					s_RpSetErrorCode(gucBgResp[5]);
					giBackgroudSendFlag = 0;
					gszWifiStatus.WState = WIFI_STATUS_DISCON;
					giWifiErrorCode = WIFI_RET_ERROR_IPCONF;
				}
				else
				{
					giBackgroudSendFlag = 0;
					gszWifiStatus.WState = WIFI_STATUS_NWPARAMS;
					//Resp=OK<MAC6bytes><IP4bytes><Subnet4bytes><Gateway4bytes>\r\n
					memcpy(gszWifiStatus.HostAddr, gucBgResp+8,4);
				}
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
		}
	}
	if (gszWifiStatus.WState == WIFI_STATUS_NWPARAMS)
	{
		//get nwparams
		if (giBackgroudSendFlag == 0)
		{
			memset(gucBgATstrIn, 0, sizeof(gucBgATstrIn));
			memset(gucBgResp, 0, sizeof(gucBgResp));
			giBgRespIndex = 0;
			sprintf(gucBgATstrIn, "AT+RSI_NWPARAMS?\r\n");
			PortReset(P_WIFI);
			CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,gucBgATstrIn);
			PortSends(P_WIFI, gucBgATstrIn, strlen(gucBgATstrIn));
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
			giBackgroudSendFlag = 1;
			timercount = 0;
		}
		else
		{
			timercount++;
			//Resp=OK<32B SSID><1B SECTYPE><63B PSK><1B ch_no><6B mac><1B DHCP mode><4B IP>...
			iRet = PortRecvs(P_WIFI, gucBgResp+giBgRespIndex, sizeof(gucBgResp), 0);
			if (iRet > 0)
			{
				giBgRespIndex += iRet;
			}
			if (timercount >= 500)//5s
			{
				WIFI_DEBUG_PRINT("[%s,%d]timeout\r\n",FILELINE);
				giBackgroudSendFlag = 0;
				gszWifiStatus.WState = WIFI_STATUS_DISCON;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
			if (paxstrnstr(gucBgResp, "\r\n", giBgRespIndex, 2) != NULL)
			{
				WIFI_DEBUG_PRINT("[%s,%d]gucBgResp=%s\r\n",FILELINE,gucBgResp);
				if (paxstrnstr(gucBgResp, "OK",giBgRespIndex, 2) == NULL)
				{
					s_RpSetErrorCode(gucBgResp[5]);
					giBackgroudSendFlag = 0;
					gszWifiStatus.WState = WIFI_STATUS_DISCON;
				}
				else
				{
					if (memcmp(gucBgResp+106, "\x00\x00\x00\x00", 4) == 0)
					{
						//the ip is zero
						giBackgroudSendFlag = 0;
						gszWifiStatus.WState = WIFI_STATUS_DISCON;
					}
					else
					{
						giNowApChannel = gucBgResp[98];
						giBackgroudSendFlag = 0;
						gszWifiStatus.WState = WIFI_STATUS_PWMODE;
						//connect success,set pwmode=2,and set status=connected
						BackgroudConnectFail = 0;
                        memcpy(gszNowConnectParam.Ip, gucBgResp+106, IPLEN);
                        memcpy(gszNowConnectParam.Mask, gucBgResp+110, IPLEN);
                        memcpy(gszNowConnectParam.Gate, gucBgResp+114, IPLEN);
                        memcpy(gszNowConnectParam.Dns, gucBgResp+119+gucBgResp[118]*10, IPLEN);
					}
				}
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
		}
	}
	if (gszWifiStatus.WState == WIFI_STATUS_DISCON)
	{
		//Dis connect
		if (giBackgroudSendFlag == 0)
		{
			memset(gucBgATstrIn, 0, sizeof(gucBgATstrIn));
			memset(gucBgResp, 0, sizeof(gucBgResp));
			giBgRespIndex = 0;
			sprintf(gucBgATstrIn, "AT+RSI_DISASSOC\r\n");
			PortReset(P_WIFI);
			CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,gucBgATstrIn);
			PortSends(P_WIFI, gucBgATstrIn, strlen(gucBgATstrIn));
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
			giBackgroudSendFlag = 1;
			timercount = 0;
		}
		else
		{
			timercount++;
			//Resp=OK\r\n
			iRet = PortRecvs(P_WIFI, gucBgResp+giBgRespIndex, sizeof(gucBgResp), 0);
			if (iRet > 0)
			{
				giBgRespIndex += iRet;
			}
			if (timercount >= 1000)//10s
			{
				WIFI_DEBUG_PRINT("[%s,%d]timeout\r\n",FILELINE);
				giBackgroudSendFlag = 0;
				gszWifiStatus.WState = WIFI_STATUS_PWMODE;
				BackgroudConnectFail = 1;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
			if (paxstrnstr(gucBgResp, "\r\n", giBgRespIndex, 2) != NULL)
			{
				WIFI_DEBUG_PRINT("[%s,%d]gucBgResp=%s\r\n",FILELINE,gucBgResp);
				giBackgroudSendFlag = 0;
				gszWifiStatus.WState = WIFI_STATUS_PWMODE;
				BackgroudConnectFail = 1;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
		}
	}
	if (gszWifiStatus.WState == WIFI_STATUS_PWMODE)
	{
		//set pwmode=2
		if (giBackgroudSendFlag == 0)
		{
			memset(gucBgATstrIn, 0, sizeof(gucBgATstrIn));
			memset(gucBgResp, 0, sizeof(gucBgResp));
			giBgRespIndex = 0;
			sprintf(gucBgATstrIn, "AT+RSI_PWMODE=2\r\n");
			PortReset(P_WIFI);
			CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,gucBgATstrIn);
			PortSends(P_WIFI, gucBgATstrIn, strlen(gucBgATstrIn));
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
			giBackgroudSendFlag = 1;
			timercount = 0;
		}
		else
		{
			timercount++;
			iRet = PortRecvs(P_WIFI, gucBgResp+giBgRespIndex, sizeof(gucBgResp), 0);
			if (iRet > 0)
			{
				giBgRespIndex += iRet;
			}
			if (timercount >= 500)//5s
			{
				WIFI_DEBUG_PRINT("[%s,%d]timeout\r\n",FILELINE);
				giBackgroudSendFlag = 0;
				gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
			if (paxstrnstr(gucBgResp, "\r\n", giBgRespIndex, 2) != NULL)
			{
				WIFI_DEBUG_PRINT("[%s,%d]gucBgResp=%s\r\n",FILELINE,gucBgResp);
				if (paxstrnstr(gucBgResp, "OK",giBgRespIndex, 2) == NULL)
				{
					s_RpSetErrorCode(gucBgResp[5]);
					giBackgroudSendFlag = 0;
					gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
				}
				else
				{
					giBackgroudSendFlag = 0;
					if (BackgroudConnectFail == 1)
					{
						gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
					}
					else
					{
						gszWifiStatus.WState = WIFI_STATUS_CONNECTED;
					}
				}
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,gucBgResp);
				return;
			}
		}
	}

	if (gszWifiStatus.WState == WIFI_STATUS_SLEEP)
	{
		static int count = 0;
		if (count == 0)
		{
			PortReset(P_WIFI);
			PortSends(P_WIFI, "ACK\r\n",5);
		}
		count++;
		if (count == 100) count = 0;
	}
	if (gszWifiStatus.WState == WIFI_STATUS_RESET)
	{
		iRet = s_RpWifiInitForBack();
		if (1 == iRet)
		{
			s_RpAtCmdFailCntReset();
			gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
		}
		else if (-1 == iRet)
		{
			s_RpAtCmdFailCntReset();
			gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
		}
	}
	
}


//tcp:at+rsi_read<hn 1B><size 2B><stream MAX1400B>\r\n
//udp:at+rsi_read<hn 1B><size 2B><ip 4B><port 2B><stream MAX1400B>\r\n
//at+rsi_close<hn 1B>\r\n
//处理串口数据流，将串口数据流分成三部分处理:
//1- 无异步事件，数据流写入at指令响应数据fifo
//2- 刚发生异步事件，处理异步事件头，异步事件包括tcp读数据、udp读数据、客户端断开事件
//3- 已经发生tcp读数据或udp读数据事件，数据未读完，继续读取数据并写入socket缓冲区
void redpinewifi_timer_handler(void)
{
	int flag;
	static int data_read_timer_start = 0;
    static uint timer10ms=0;

    if(rpwifi_softinterrupt_enable==0) return;	
    if(timer10ms==s_Get10MsTimerCount()) return;
    timer10ms = s_Get10MsTimerCount();
	
	//刷新图标
	s_RpUpdateIcon();
	//定时器后台处理扫描连接和IPCONF
	if ((gszWifiStatus.WState != WIFI_STATUS_CONNECTED) && (gszWifiStatus.WState != WIFI_STATUS_NOTCONN))
	{
		s_RpWifiBackgroud();
	}
	else
	{
		//check gpio int
		if ((giWifiIntCount > 0) && (giAsyMsgBufWrite==0))
		{
			if (s_RpProcessHead() == 1)
			{
				//Find the socket, set timer
				data_read_timer_start = s_Get10MsTimerCount();
			}
		}
		else if ((giWifiIntCount == 0) && (giAsyMsgBufWrite == 0))
		{
			//无异步事件中断发生，且异步事件已经处理结束，将串口数据写入命令响应fifo
			s_RpProcessFifo();
		}
		else if (giAsyMsgBufWrite > 0)
		{
			//已经处理的异步事件头，但异步事件数据还未接收完成，在此一直读取剩余数据
			//直到读取到全部异步事件数据
			if (s_RpProcessSocket() < 0)
			{
				if ((s_Get10MsTimerCount() - data_read_timer_start) >= 200)//2s
				{
					//异步事件数据包处理异常，丢失包数据，置位相关变量
					WIFI_DEBUG_PRINT("[%s,%d]Socket Data lost\r\n",FILELINE);
					giFindSocket = -1;
					giAsyMsgBufWrite = 0;
					giAsyMsgSize = 0;
					irq_save(flag);
					giWifiIntCount--;
					irq_restore(flag);
				}
			}
		}
	}
}
int s_RpProcessHead(void)
{
	uint flag;
	int iRet,read_len=0,AsyHeaderLoc = 0,TempIndex1=0;
	int iHeadIndex = 0,timerstart = 0,timerend = 0;
    uchar Header[50],tmpbuf[1024];
	uchar *pStr1 = NULL;

	//产生了异步中断，并且还没开始取异步事件数据
	memset(tmpbuf, 0, sizeof(tmpbuf));
	//将未处理完的数据写入gucReadTemp继续处理
	memcpy(tmpbuf, gucPortRxTemp, gucPortRxTempIndex);
	iHeadIndex = gucPortRxTempIndex;
	timerstart = GetUsClock();
	while(1)
	{
		DelayUs(10);
		timerend = GetUsClock();
		if ((timerend - timerstart) >= 2000)//2ms
		{
			WIFI_DEBUG_PRINT("[%s,%d]softirq timeout,giWifiIntCount=%d\r\n",FILELINE,giWifiIntCount);
			return -1;
		}
		if ((s_Get10MsTimerCount() - guiIrqTimerStart) > 50)//500ms
		{
			//throw the false interrupt
			irq_save(flag);
			giWifiIntCount--;
			irq_restore(flag);
			WIFI_DEBUG_PRINT("[%s,%d]Lost Int,giWifiIntCount=%d\r\n",FILELINE,giWifiIntCount);
			return -1;
		}
		iRet = PortRecvs(P_WIFI, tmpbuf+iHeadIndex,sizeof(tmpbuf)-iHeadIndex, 0);
		if (iRet > 0)
		{
			iHeadIndex += iRet;
		}
		if (iHeadIndex >= 8)
		{
			memcpy(Header, tmpbuf, sizeof(Header));
			//ERROR<0xEA>\r\n,a ap lost event
			pStr1 = paxstrnstr(Header, "ERROR\xEA", sizeof(Header), 6);
			if (pStr1 != NULL) break;
			//AT+RSI_CLOSE=id\r\n, socket close event
			//AT+RSI_READ,socket read event
			pStr1 = paxstrnstr(Header, "AT+RSI", sizeof(Header), 5);
			if (pStr1 != NULL) break;

            pStr1 = paxstrnstr(Header, "OK", sizeof(Header), 2);
			if (pStr1 != NULL)
            {
                unsigned char *ps, *pe;
                ps = tmpbuf + (pStr1 - Header);
                if (ps)
                {
                    while(1)
                    {
                        if (pe = paxstrnstr(ps + 2, "\x0D\x0A", iHeadIndex - 2, 2))
                        {
                            s_RpWifiWriteToFifo(ps, pe + 2 - ps);
                            gucPortRxTempIndex = iHeadIndex - (pe + 2 - ps);
                            memcpy(gucPortRxTemp, pe + 2, gucPortRxTempIndex);
                            return 0;
                        }

                        iRet = PortRecvs(P_WIFI, tmpbuf+iHeadIndex,sizeof(tmpbuf)-iHeadIndex, 0);
                        if (iRet > 0) iHeadIndex += iRet;
                    }
                }
            }
		}
	}
	
	read_len = iHeadIndex;
	memset(gucPortRxTemp, 0, sizeof(gucPortRxTemp));
	gucPortRxTempIndex = 0;//gucPortRxTemp已经写入处理buf，重新复位
		
	memcpy(Header, tmpbuf, sizeof(Header));
	WIFI_DEBUG_PRINT("[%s,%d]Header=%s,read_len=%d\r\n",FILELINE,Header,read_len);
	pStr1 = paxstrnstr(Header, "ERROR\xEA", sizeof(Header), 6);
	if ( pStr1 != NULL)
	{
		AsyHeaderLoc = pStr1-Header;
		s_RpWifiWriteToFifo(tmpbuf, AsyHeaderLoc);
				
		//ERROR<0xEA>\r\n,a ap lost event
		gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
		PortSends(P_WIFI, "AT+RSI_PWMODE=2\r\n", 17);
		irq_save(flag);
		giWifiIntCount--;
		irq_restore(flag);
		WIFI_DEBUG_PRINT("[%s,%d]Lost Ap\r\n",FILELINE);
		//ScrSetIcon(0,1);
		return 0;
	}
				
	pStr1 = paxstrnstr(Header, "AT+RSI", sizeof(Header), 5);
	if (pStr1 != NULL)
	{
		WIFI_DEBUG_PRINT("[%s,%d]pStr1=%s\r\n",FILELINE,pStr1);
		AsyHeaderLoc = pStr1-Header;
	}
			
	//找到异步事件头，头之前的数据写入命令响应缓冲区
	s_RpWifiWriteToFifo(tmpbuf, AsyHeaderLoc);
	if ((read_len - AsyHeaderLoc) < 15)
	{
		//读到的数据小于能处理的异步事件头
		memcpy(gucAsyMsgBuf, tmpbuf+AsyHeaderLoc, read_len - AsyHeaderLoc);
		giAsyMsgBufWrite = read_len - AsyHeaderLoc;
		timerstart = GetUsClock();
		while(giAsyMsgBufWrite < 15)
		{
			iRet = PortRecv(P_WIFI,gucAsyMsgBuf+giAsyMsgBufWrite,0);
			if (iRet == 0)
			{
				giAsyMsgBufWrite++;
				read_len++;
			}
			timerend = GetUsClock();
			if ((timerend - timerstart) >= 2000)//2ms
			{
				//throw the false interrupt
				irq_save(flag);
				giWifiIntCount--;
				irq_restore(flag);
				WIFI_DEBUG_PRINT("[%s,%d]Lost Int,giWifiIntCount=%d\r\n",FILELINE,giWifiIntCount);
				return -1;
			}
		}
		TempIndex1 = read_len;//此次处理的异步事件包头结束位置
	}
	else
	{
		//异步事件数据写入gucAsyMsgBuf进行处理，异步事件数据最大为1482字节(udp最大数据包)
		memcpy(gucAsyMsgBuf, tmpbuf+AsyHeaderLoc, 15);
		//giAsyMsgBufWrite表示已经写入gucAsyMsgBuf的异步事件数据长度
		giAsyMsgBufWrite = 15;
		TempIndex1 = AsyHeaderLoc + 15;//此次处理的异步事件包头结束位置
	}
	uchar temp[20];
	memset(temp, 0, sizeof(temp));
	memcpy(temp, gucAsyMsgBuf, 15);
	WIFI_DEBUG_PRINT("[%s,%d]Header=%s\r\n",FILELINE,temp);
	//解析异步事件包头，分清是何种异步事件，并确定异步事件数据的具体长度
	if (strstr(temp, "AT+RSI_CLOSE") != NULL)
	{
		//找到需要处理的套接字
		giFindSocket = s_RpFindSocket(temp[12]);
		if (giFindSocket >= 0)
		{
			if (CommSocket[giFindSocket].role == SOCKET_ROLE_LISTEN)
			{
				CommSocket[giFindSocket].id = -1;//just set id=-1, the listen socket is closed before accept it as a commsocket
			}
			else
			{
				CommSocket[giFindSocket].id = 0;//just set id=0, user need to use NetCloseSocket to close it
			}
			
		}
		giAsyMsgBufWrite = 0;
		irq_save(flag);
		giWifiIntCount--;
		irq_restore(flag);
		if (TempIndex1 >= 15)//处理关闭套接字事件结束，多余数据保存,退出此次处理
		{
			memcpy(gucPortRxTemp, tmpbuf+TempIndex1,read_len-TempIndex1);
			gucPortRxTempIndex = read_len-TempIndex1;
		}
		if (s_RpCheckSocket())
		{
			//when socket all closed ,set pwmode to power save mode	
			PortSends(P_WIFI, "AT+RSI_PWMODE=2\r\n",17);
		}
		return 0;
	}
	else if (strstr(temp, "AT+RSI_READ") != NULL)
	{
		//找到需要处理的套接字
		giFindSocket = s_RpFindSocket(temp[11]);
		if (giFindSocket == -1)
		{
			WIFI_DEBUG_PRINT("[%s,%d]temp=%s",FILELINE,temp);
			giFindSocket = -1;
			giAsyMsgBufWrite = 0;
			giAsyMsgSize = 0;
			irq_save(flag);
			giWifiIntCount--;
			irq_restore(flag);
			return -1;
		}
		//解析是tcp读还是udp读,并计算异步事件包的大小
		if (CommSocket[giFindSocket].type == NET_SOCK_STREAM)//tcp
		{
			giAsyMsgSize = temp[12] + 256*temp[13] + 16;//all the msg size
			if (giAsyMsgSize > SOCKET_RECV_SIZE+16)
			{
				WIFI_DEBUG_PRINT("[%s,%d]giAsyMsgSize=%d",FILELINE,giAsyMsgSize);
				giFindSocket = -1;
				giAsyMsgBufWrite = 0;
				giAsyMsgSize = 0;
				irq_save(flag);
				giWifiIntCount--;	
				irq_restore(flag);
				return -1;
			}
		}
		else if (CommSocket[giFindSocket].type == NET_SOCK_DGRAM)//udp
		{
			giAsyMsgSize = temp[12] + 256*temp[13] + 22;//all the msg size
			if (giAsyMsgSize > SOCKET_RECV_SIZE+22)
			{
				giFindSocket = -1;
				giAsyMsgBufWrite = 0;
				giAsyMsgSize = 0;
				irq_save(flag);
				giWifiIntCount--;
				irq_restore(flag);
				return -1;
			}
		}
		else
		{
			WIFI_DEBUG_PRINT("[%s,%d]temp=%s",FILELINE,temp);
			giFindSocket = -1;
			giAsyMsgBufWrite = 0;
			giAsyMsgSize = 0;
			irq_save(flag);
			giWifiIntCount--;
			irq_restore(flag);
			return -1;
		}
		//此次timer_handler读到的数据除了异步事件头外，还有多余的异步事件数据，继续处理
		if ((read_len-TempIndex1) > 0)
		{
			//如果本次读到的数据包含了整个异步事件数据，则在此次处理完
			if ((giAsyMsgSize-giAsyMsgBufWrite)<=(read_len-TempIndex1))
			{
				//将异步事件数据剩余数据写入gucAsyMsgBuf
				memcpy(gucAsyMsgBuf+giAsyMsgBufWrite, tmpbuf+TempIndex1, giAsyMsgSize-giAsyMsgBufWrite);
				TempIndex1 += giAsyMsgSize-giAsyMsgBufWrite;

				//将数据写入socket fifo
				if (CommSocket[giFindSocket].type == NET_SOCK_DGRAM)//udp
				{
					memcpy(CommSocket[giFindSocket].remoteIp, gucAsyMsgBuf+14, 4);
					CommSocket[giFindSocket].remotePort = gucAsyMsgBuf[18] + 256*gucAsyMsgBuf[19];
					s_RfBufInsertArray(giFindSocket, gucAsyMsgBuf+20, giAsyMsgSize-22);
				}
				else
				{
					s_RfBufInsertArray(giFindSocket, gucAsyMsgBuf+14, giAsyMsgSize-16);
				}
				//异步事件处理结束，置位相关变量
				giFindSocket = -1;
				giAsyMsgBufWrite = 0;
				giAsyMsgSize = 0;
				irq_save(flag);
				giWifiIntCount--;
				irq_restore(flag);
						
				//异步读数据结束，将剩余数据写入PortRxTemp等待下次处理
				memcpy(gucPortRxTemp, tmpbuf+TempIndex1,read_len-TempIndex1);
				gucPortRxTempIndex = read_len-TempIndex1;
			}
			else
			{
				//本次读到的数据包不包含整个异步事件包数据，则将此次读到的数据写入gucAsyMsgBuf
				//此时异步事件处理的相关变量未置位，等待下次timer_handler事件继续处理
				//gucAsyMsgBuf需要读到所有的异步事件数据后才会将数据写入socket fifo
				memcpy(gucAsyMsgBuf+giAsyMsgBufWrite, tmpbuf+TempIndex1, read_len-TempIndex1);
				giAsyMsgBufWrite += read_len-TempIndex1;
			}
		}	
		return 1;
	}
	else
	{
		WIFI_DEBUG_PRINT("[%s,%d]Header error=%s\r\n",FILELINE,temp);
		giFindSocket = -1;
		giAsyMsgBufWrite = 0;
		giAsyMsgSize = 0;
		irq_save(flag);
		giWifiIntCount--;
		irq_restore(flag);
		return -1;
	}
}
void s_RpProcessFifo(void)
{
	int iRet,read_len=0;
	uchar tmpbuf[1024], *ptr;
	
	memset(tmpbuf, 0, sizeof(tmpbuf));
	memcpy(tmpbuf, gucPortRxTemp, gucPortRxTempIndex);

	read_len += gucPortRxTempIndex;
	memset(gucPortRxTemp, 0, sizeof(gucPortRxTemp));
	gucPortRxTempIndex = 0;

    while (1)
    {
        if (ptr = paxstrnstr(tmpbuf, "\x0D\x0A", read_len, 2))
            break;
             
        iRet = PortRecvs(P_WIFI, tmpbuf+read_len, sizeof(tmpbuf)-read_len, 0);
        if (iRet < 0) continue;
        if (iRet <= 0 && read_len <= 0) return ;
        read_len += iRet;
    }

    //如果在进入该处理流程后，突然产生了异步事件中断，并且被读到该处理流程
    //读完之后判断一下是否有中断，如果有，将此次数据保存，等待中断处理
    if (giWifiIntCount > 0)
    {
        memcpy(gucPortRxTemp, tmpbuf,read_len);
        gucPortRxTempIndex = read_len;
        WIFI_DEBUG_PRINT("[%s,%d]A Int in Normal=%d\r\n",FILELINE,giWifiIntCount);
        WIFI_DEBUG_PRINT("[%s,%d]gucPortRxTempIndex=%s,%d\r\n",FILELINE,gucPortRxTemp,gucPortRxTempIndex);
    }
    else
    {
        gucPortRxTempIndex = read_len - (ptr + 2 - tmpbuf);
        memcpy(gucPortRxTemp, ptr + 2, gucPortRxTempIndex);

        read_len = ptr + 2 - tmpbuf;
        s_RpWifiWriteToFifo(tmpbuf, read_len);
    }

    return ;
}
int s_RpProcessSocket(void)
{
	uint flag;
	int iRet,read_len=0;
	uchar tmpbuf[1024];
	iRet = PortRecvs(P_WIFI,tmpbuf,sizeof(tmpbuf),0);
	if (iRet <= 0)
	{
		//本次未读到异步事件数据，返回
		return -1;
	}
	
	WIFI_DEBUG_PRINT("[%s,%d]iRet=%d,needdata=%d\r\n",FILELINE,iRet,giAsyMsgSize-giAsyMsgBufWrite);
	read_len = iRet;
	if ((giAsyMsgSize-giAsyMsgBufWrite)<=read_len)
	{
		//本次读到的数据已经达到或超过异步事件所需的所有数据，
		//剩余数据可能是正常命令响应，因此保存剩余数据等待下次处理
		memcpy(gucAsyMsgBuf+giAsyMsgBufWrite, tmpbuf,giAsyMsgSize-giAsyMsgBufWrite);
		if (gucPortRxTempIndex > 0)
		{
			WIFI_DEBUG_PRINT("[%s,%d]gucPortRxTempIndex%d\r\n",FILELINE,gucPortRxTempIndex);
		}
		memcpy(gucPortRxTemp+gucPortRxTempIndex, tmpbuf+(giAsyMsgSize-giAsyMsgBufWrite),read_len-(giAsyMsgSize-giAsyMsgBufWrite));
		gucPortRxTempIndex += read_len-(giAsyMsgSize-giAsyMsgBufWrite);
		giAsyMsgBufWrite = giAsyMsgSize;
	}
	else
	{
		//将读到的数据写入gucAsyMsgBuf
		memcpy(gucAsyMsgBuf+giAsyMsgBufWrite, tmpbuf,read_len);
		giAsyMsgBufWrite += read_len;
	}
			
	if (giAsyMsgSize == giAsyMsgBufWrite)
	{
		//异步事件数据已经读取完成，写入socket fifo
		if (CommSocket[giFindSocket].type == NET_SOCK_DGRAM)//udp
		{
			memcpy(CommSocket[giFindSocket].remoteIp, gucAsyMsgBuf+14, 4);
			CommSocket[giFindSocket].remotePort = gucAsyMsgBuf[18] + 256*gucAsyMsgBuf[19];
			s_RfBufInsertArray(giFindSocket, gucAsyMsgBuf+20, giAsyMsgSize-22);
		}
		else
		{
			s_RfBufInsertArray(giFindSocket, gucAsyMsgBuf+14, giAsyMsgSize-16);
		}
		WIFI_DEBUG_PRINT("[%s,%d]Read Data Ok\r\n",FILELINE);
		//异步事件数据包处理完成，置位相关变量
		giFindSocket = -1;
		giAsyMsgBufWrite = 0;
		giAsyMsgSize = 0;
		irq_save(flag);
		giWifiIntCount--;
		irq_restore(flag);
		return 0;
	}
	return -1;
}

void redpinewifi_gpio_handler(void)
{
	//有异步事件发生
	giWifiIntCount++;
	guiIrqTimerStart = s_Get10MsTimerCount();
	//WIFI_DEBUG_PRINT("gpioisr\r\n");
}

static void s_RpEscapeChar(uchar *Source, uchar *Result, int SourceLen, int *ResultLen)
{
	int i,j;
	//对需要写入Redpine模块的套接字数据流需要进行转义处理
	//处理规则:连续的0x0D 0x0A替换成0xDB 0xDC
	//单个的0xDB替换成0xDB 0xDD
	//同时要保证转义后的数据包长度不超过1400字节
	//(可能出现最大数据包携带的真实数据小于1400的情况)
	//ResultLen返回转义后的数据长度
	for (i = 0,j = 0;(i < SourceLen)&&(j < SOCKET_SEND_SIZE);i++,j++)
	{
		if (Source[i] == 0x0D)
		{
			if (Source[i+1] == 0x0A)
			{
				Result[j] = 0xDB;
				Result[j+1] = 0xDC;
				i++;
				j++;
			}
			else
			{
				Result[j] = Source[i];
			}
		}
		else if (Source[i] == 0xDB)
		{
			Result[j] = 0xDB;
			Result[j+1] = 0xDD;
			j++;
		}
		else
		{
			Result[j] = Source[i];
		}
	}
	*ResultLen = j;
}
//检测是否所有套接字都关闭了
int s_RpCheckSocket(void)
{
	int i;
	for(i = 0;i < MAX_SOCKET;i++)
	{
		if (CommSocket[i].iUsed == 1)
		{
			return 0;
		}
	}
	return 1;
}
//检测剩余可用套接字句柄数
static int s_RpCheckSocketLeft(void)
{
	int i;
	int count=0;
	for(i = 0;i < MAX_SOCKET;i++)
	{
		if (CommSocket[i].iUsed == 0)
		{
			count++;
		}
	}
	return count;
}
#ifndef TCP_LOCAL_PORT_RANGE_START
#define TCP_LOCAL_PORT_RANGE_START 4096
#define TCP_LOCAL_PORT_RANGE_END   (61000)//0x7fff
#endif
void s_RpPortInit(void)
{
	int i = 0;
	for (i = 0;i < MAX_SOCKET;i++)
	{
		gusUsedLocalPortList[0][i] = 0;
		gusUsedLocalPortList[1][i] = 0;
		gusUsedLocalPortList[2][i] = 0;
	}
}
static int s_RpCheckPortIsUsed(int type, ushort port)
{
	int i;
	for (i = 0;i < MAX_SOCKET;i++)
	{
		if ((gusUsedLocalPortList[type][i] != 0)&&(gusUsedLocalPortList[type][i] == port))
		{
			return 1;
		}
	}
	return 0;
}
static int s_RpPortAddInUsedList(int type, ushort port)
{
	int i;
	for (i = 0;i < MAX_SOCKET;i++)
	{
		if (gusUsedLocalPortList[type][i] == 0)
		{
			gusUsedLocalPortList[type][i] = port;
			return 1;
		}
	}
	return 0;
}
static int s_RpDeletePortInList(int type, ushort port)
{
	int i;
	for (i = 0;i < MAX_SOCKET;i++)
	{
		if ((gusUsedLocalPortList[type][i] != 0)&&(gusUsedLocalPortList[type][i] == port))
		{
			gusUsedLocalPortList[type][i] = 0;
			return 1;
		}
	}
	return 0;
}

static volatile int s_PortInit = 0; 
static void s_RpPortReinit(void)
{
	s_PortInit = 0;
}

static ushort s_RpGetNewPort(int type)
{
	static ushort port = TCP_LOCAL_PORT_RANGE_START;
	ushort value[4];
	int i;
	uint num;
	if (!s_PortInit)
	{
		s_PortInit = 1;
		while(1)
		{
			GetRandom((uchar*)value);
			num = value[0];
			num = num%(TCP_LOCAL_PORT_RANGE_END-TCP_LOCAL_PORT_RANGE_START);
			port = TCP_LOCAL_PORT_RANGE_START+(ushort)num;
			if (s_RpCheckPortIsUsed(type,port) == 0)
			{
				break;
			}
		}
	}
	else
	{
		while(1)
		{
			port++;
			if (port > TCP_LOCAL_PORT_RANGE_END)
			{
				port = TCP_LOCAL_PORT_RANGE_START;
			}
			if (s_RpCheckPortIsUsed(type,port) == 0)
			{
				break;
			}
		}
	}
	return port;
	
}

void s_RpAtCmdFail(void)
{
	guiAtFailCnt++;
}
uint s_RpGetAtCmdFailCnt(void)
{
	return guiAtFailCnt;
}
void s_RpAtCmdFailCntReset(void)
{
	guiAtFailCnt = 0;
}

void s_RpSetErrorCode(uchar error)
{
	gucErrorCode = error;
	WIFI_DEBUG_PRINT("[%s,%d]gucErrorCode=0x%02x\r\n",FILELINE,gucErrorCode);
}
uchar s_RpGetErrorCode(void)
{
	return gucErrorCode;
}

int RpWifiUpdateFw(void)
{
	PARAM_STRUCT param;
	int iRet;
	if (is_wifi_module()!=1) return -1;
	
	param.FuncType = MPATCH_WIFI_FUNC;
	param.FuncNo = MPATCH_WIFI_UPDATE_FW;
	iRet = s_MpatchEntry(MPATCH_NAME_rs9100, &param);
	if (iRet < 0)
		return iRet;
	return param.int1;
}

int RedPineGetTadm2(uchar *buf, int size)
{
	PARAM_STRUCT param;
	int iRet;
	if (is_wifi_module()!=1) return -1;
	
	param.FuncType = MPATCH_WIFI_FUNC;
	param.FuncNo = MPATCH_WIFI_GET_TADM2;
	param.u_str1 = buf;
	param.int2 = size;
	iRet = s_MpatchEntry(MPATCH_NAME_rs9100, &param);
	if (iRet < 0)
		return iRet;
	return param.int1;
}

int RedPineGetPatchVersion(uchar *buf, int size)
{
	PARAM_STRUCT param;
	int iRet;
	if (is_wifi_module()!=1) return -1;
	
	memset(buf, 0, size);
	param.FuncType = MPATCH_WIFI_FUNC;
	param.FuncNo = MPATCH_WIFI_GET_PATCH_VER;
	param.u_str1 = buf;
	param.int1 = size;
	iRet = s_MpatchEntry(MPATCH_NAME_rs9100, &param);
	if (iRet < 0)
		return iRet;
	else
		return 0;
}

void RpWifiUpdate(void)
{
	int iRet;
	uchar ucRet;
	uchar version[20];


	ScrCls();
	if (!is_wifi_module())
	{
		ScrPrint(0, 0, 0xE0, "Wifi Update");
		ScrPrint(0, 3, 0x60, "No Wifi!");
		getkey();
		return;
	}
	ScrPrint(0, 0, 0xE0, "Redpine WIFI Update");
	ScrPrint(0, 3, 0x60, "Open WIFI...");
	iRet = WifiOpen();
	if ((WifiGetHVer(version) == 0) && (iRet == 0))
	{
		//if can't open wifi, then must update ,so needn't to display this
		ScrPrint(0, 3, 0x60, "Version is same:%s",version);
		ScrPrint(0, 4, 0x60, "Need Update?");
		ucRet = getkey();
		if (ucRet != KEYENTER)
		{
			WifiClose();
			return;
		}
	}
	WifiClose();
	memset(version, 0, sizeof(version));
	RedPineGetPatchVersion(version, sizeof(version));
	ScrClrLine(3, 4);
	ScrPrint(0, 3, 0x60, "Update to %s", version);
	ScrPrint(0, 4, 0x60, "Please wait...");
	
	iRet = RpWifiUpdateFw();
	Beep();
	if (iRet < 0)
	{
		ScrClrLine(4, 4);
		ScrPrint(0, 4, 0x60, "Update failed!");
	}
	else
	{
		ScrClrLine(4, 4);
		ScrPrint(0, 4, 0x60, "Update OK!");
		s_ClsWifiVer();
	}
	getkey();
	return;
	
}

struct ST_WIFIOPS gszRpWifiOps = 
{
	//ip stack
	.NetSocket = RpNetSocket,
	.NetConnect =RpNetConnect,
	.NetBind = RpNetBind,
	.NetListen = RpNetListen,
	.NetAccept = RpNetAccept,
	.NetSend = RpNetSend,
	.NetSendto = RpNetSendto,
	.NetRecv = RpNetRecv,
	.NetRecvfrom = RpNetRecvfrom,
	.NetCloseSocket = RpNetCloseSocket,
	.Netioctl = RpNetioctl,
	.SockAddrSet = RpSockAddrSet,
	.SockAddrGet = RpSockAddrGet,
	.DnsResolve = RpDnsResolve,
	.DnsResolveExt = RpDnsResolveExt,
	.NetPing = RpNetPing,
	.RouteSetDefault = RpRouteSetDefault,
	.RouteGetDefault = RpRouteGetDefault,
	.NetDevGet = RpNetDevGet,
	//wifi
	.WifiOpen = RedpineWifiOpen,
	.WifiClose = RedpineWifiClose,
	.WifiScan = RedpineWifiScan,
    .WifiScanEx = RedpineWifiScanExt,
	.WifiConnect = RedpineWifiConnect,
	.WifiDisconnect = RedpineWifiDisconnect,
	.WifiCheck = RedpineWifiCheck,
	.WifiCtrl = RedpineWifiCtrl,
	//内部使用
	.s_wifi_mac_get = s_rpwifi_mac_get,
	.s_WifiPowerSwitch = s_RpWifiPowerSwitch,
	.WifiUpdateOpen = RpWifiUpdateOpen,
	.s_WifiInSleep = s_RpWifiInSleep,
	.s_WifiOutSleep = s_RpWifiOutSleep,
	.WifiDrvInit = s_RedPineWifiDriverInit,
	.WifiUpdate = RpWifiUpdate,
	.WifiGetHVer = RedPineWifiGetHVer,
};

