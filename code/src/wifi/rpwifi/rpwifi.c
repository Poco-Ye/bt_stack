#include <base.h>
#include "rpwifi.h"
#include "rpwlanapp.h"
#include "../../comm/comm.h"
#include "../apis/wlanapp.h"

#define WIFI_SOFT_IRQ		(IRQ_OTIMER9)


extern void s_RpSocketInit(void);
extern void redpinewifi_timer_handler(void);
extern void redpinewifi_gpio_handler(void);

ST_WIFI_STATUS gszWifiStatus={0,};
//当前连接的热点数据,该数据不会在WIFI关闭后清除，可保证快速连接
ST_WIFI_AP gszNowAp={0,};
ST_WIFI_PARAM gszNowConnectParam={0,};
int giNowApChannel=0;
int giWifiErrorCode = WIFI_RET_OK;
uint guiLock = 0;

static int giRecWrite = 0;
static int giRecRead = 0;
static int giWifiPowerState;
static int giDeepSleepFlag = 0;
static int giRpWifiStateBak;
static uchar gucRecFifo[1500];
static uchar gucWifiFwVer[6];

volatile int giWifiIntCount = 0;
volatile int giAsyMsgBufWrite=0;

static int giCtrlWrite = 0;
static int giCtrlRead = 0;
static uchar gucCtrlFifo[1500];

typedef struct {
	int intr_port;
	int intr_pin;
	int pwr_port;
	int pwr_pin;
	int pwr_on;
	int rst_port;
	int rst_pin;
	int rst_on;
	int baudrate;
	void (*PowerOn)(void);
	void (*PowerOff)(void);	
}T_WifiDrvConfig;

static T_WifiDrvConfig *ptWifiDrvCfg = NULL;
#define WIFI_INT_GPIO	ptWifiDrvCfg->intr_port, ptWifiDrvCfg->intr_pin
#define WIFI_PWR_GPIO	ptWifiDrvCfg->pwr_port, ptWifiDrvCfg->pwr_pin
#define WIFI_RST_GPIO	ptWifiDrvCfg->rst_port, ptWifiDrvCfg->rst_pin
#define WIFI_BAUDRATE  ptWifiDrvCfg->baudrate
#define WIFI_PWR_ON    (ptWifiDrvCfg->pwr_on)
#define WIFI_RST_ON    (ptWifiDrvCfg->rst_on)

void s_RpWifiPowerSwitch (int on)
{
	if(!on)
		ptWifiDrvCfg->PowerOff();
	else
		ptWifiDrvCfg->PowerOn();
		
}

int RedPineCheckModule(void)
{
	int iRet;
	uchar uRet;
	unsigned int flag;
	uchar ComParam[100];
	int i;
	uchar Resp[200];
	uchar string[100];
	uchar FwVersion[20];
	
	
	memset(ComParam, 0, sizeof(ComParam));
	sprintf(ComParam, "%d,8,n,2",WIFI_BAUDRATE);
	uRet = PortOpen(P_WIFI,ComParam);
	if (uRet != 0)
	{
		return WIFI_RET_ERROR_NODEV;
	}
	s_RpWifiPowerSwitch(1);
	s_RpAtCmdFailCntReset();
	
	iRet = s_RpWifiCheckBaudRate();
	WIFI_DEBUG_PRINT("[%s,%d]BaudCheck=%d\r\n",FILELINE,iRet);
	if (iRet != WIFI_RET_OK) 
	{
		s_RpWifiPowerSwitch(0);
		return WIFI_RET_ERROR_NODEV;
	}
	memset(Resp, 0, sizeof(Resp));
	//read the init info
	memset(string,0,sizeof(string));
	strcpy(string, " WELCOME TO REDPINE SIGNALS\r\n \r\n ");
	strcat(string, " Firmware upgrade (y/n) ");
	iRet = PortRecvs(P_WIFI, Resp,strlen(string), 100);
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
	PortSends(P_WIFI,"n\n",2);
	PortRecvs(P_WIFI, Resp, 2, 10);
	memset(Resp, 0, sizeof(Resp));
	memset(string,0,sizeof(string));
	strcpy(string, " Loading... \r\n");
	strcat(string, " Loading Done\r\n");
	iRet= PortRecvs(P_WIFI, Resp, strlen(string), 100);
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);

	memset(string, 0, sizeof(string));
	strcpy(string, "AT+RSI_FWVERSION?\r\n");
	memset(Resp, 0, sizeof(Resp));
	PortReset(P_WIFI);
	PortSends(P_WIFI,string, strlen(string));
	iRet = PortRecvs(P_WIFI, Resp, 9, 300);
	WIFI_DEBUG_PRINT("AT+RSI_FWVERSION?\r\n");
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
	if(iRet < 0) 
	{
		iRet = WIFI_RET_ERROR_NORESPONSE;
	} 
	else 
	{
		memset(FwVersion, 0, sizeof(FwVersion));
		RedPineGetPatchVersion(FwVersion, sizeof(FwVersion));
		if (FwVersion[0] == 0)
			iRet = WIFI_RET_OK;//no driver,do not update firmware
		else if (strstr(Resp,FwVersion) != NULL)
			iRet = WIFI_RET_OK;//firmware version is same, do not update firmware
		else
			iRet = WIFI_RET_ERROR_DRIVER;
	}
	s_RpWifiPowerSwitch(0);
	//3.关闭串口
	PortClose(P_WIFI);
	DelayMs(150);

	return iRet;
}

int RedpineWifiOpen(void)
{
	int iRet;
	unsigned int flag;
	uchar uRet,ComParam[100];
    int retry = 2;
	
	if (gszWifiStatus.WState != WIFI_STATUS_CLOSE)
	{
		RedpineWifiClose();
	}
	memset(ComParam, 0, sizeof(ComParam));
	sprintf(ComParam, "%d,8,n,2",WIFI_BAUDRATE);
	uRet = PortOpen(P_WIFI,ComParam);
	if (uRet != 0)
	{
		return WIFI_RET_ERROR_NODEV;
	}

    s_RpWifiPowerSwitch(1);
    s_RpAtCmdFailCntReset();
	
	while(retry)
	{
        iRet = sRedpineInit();
        if (iRet != WIFI_RET_OK)
        {
			retry--;
            if(retry == 0)
            {
                s_RpWifiPowerSwitch(0);
                //3.关闭串口
                PortClose(P_WIFI);
                DelayMs(150);
                return WIFI_RET_ERROR_NODEV;
            }
			else
			{
				s_RpWifiPowerSwitch(1);
			}
        }
        else
        {
            break;
        }
    }

	memset(&gszWifiStatus, 0, sizeof(ST_WIFI_STATUS));
	memset(gucRecFifo, 0, sizeof(gucRecFifo));
	giRecWrite = 0;
	giRecRead = 0;
	PortReset(P_WIFI);
	s_RpSocketInit();
    giWifiErrorCode = WIFI_RET_ERROR_NOTCONN;
	gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
	s_SetSoftInterrupt(WIFI_SOFT_IRQ, redpinewifi_timer_handler);
    RpwifiSoftIntEnable(1);
	iRet = s_RpWifiSoftReset();
	if (iRet != 0)
	{	
		gszWifiStatus.WState = WIFI_STATUS_CLOSE;
        RpwifiSoftIntEnable(0);
		s_RpWifiPowerSwitch(0);
		//3.关闭串口
		PortClose(P_WIFI);
		DelayMs(150);
		return WIFI_RET_ERROR_NORESPONSE;
	}

	gpio_set_pin_type(WIFI_INT_GPIO,GPIO_INPUT);
	s_setShareIRQHandler(WIFI_INT_GPIO,INTTYPE_FALL,redpinewifi_gpio_handler);
	gpio_set_pin_interrupt(WIFI_INT_GPIO,1);
	//ScrSetIcon(0,1);
	return WIFI_RET_OK;
}
int RedpineWifiClose(void)
{
	unsigned int flag;
	if (gszWifiStatus.WState == WIFI_STATUS_CLOSE)
	{
		return WIFI_RET_OK;
	}
	
	gpio_set_pin_interrupt(WIFI_INT_GPIO, 0);
	
	//2.关闭连接
	RedpineWifiDisconnect();
    RpwifiSoftIntEnable(0);
	s_RpWifiPowerSwitch(0);
	//3.关闭串口
	DelayMs(150);
	PortClose(P_WIFI);
	//ScrSetIcon(0,0);
	gszWifiStatus.WState = WIFI_STATUS_CLOSE;
	return WIFI_RET_OK;
}
int RedpineWifiSendCmd(const uchar * ATstrIn,int ATstrLen,uchar *RspOut,ushort want_len,ushort TimeOut)
{
	uchar ucRet;
	int iRet;
	int len;
    T_SOFTTIMER wifi_timer;

	WIFI_DEBUG_PRINT("[%s,%d]guiLock=%d\r\n",FILELINE,guiLock);
	while(TryLock(&guiLock)==0);//被定时器刷新图标锁定时等待
	if (ATstrIn != NULL)
	{
		giRecRead = giRecWrite;//clean rx fifo
		if (ATstrLen == 0)
		{
			ATstrLen = strlen(ATstrIn);
		}
		WIFI_DEBUG_PRINT("[%s,%d]giWifiIntCount=%d\r\n",FILELINE,giWifiIntCount);
		while(giWifiIntCount);//等待异步事件处理结束后再发送
		CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,ATstrIn);
		ucRet = PortSends(P_WIFI,(uchar *)ATstrIn, ATstrLen);
		if (ucRet != 0)
		{
			UnLock(&guiLock);
			return WIFI_RET_ERROR_NOTOPEN;
		}
		while(PortTxPoolCheck(P_WIFI));
	}
	if (RspOut == NULL || want_len == 0)
	{
		UnLock(&guiLock);
		return WIFI_RET_OK;
	}
	CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
	s_TimerSetMs(&wifi_timer,TimeOut);
	len=0;
	while(1)
	{
		if (s_TimerCheckMs(&wifi_timer) == 0)
		{
			break;
		}
		iRet = s_RpWifiReadFromFifo(RspOut+len, want_len-len);
		if(iRet > 0)
		{
			len += iRet;
			if (len >= want_len) break;
		}
	}
	RspOut[len] = 0;
	CMD_DEBUG_PRINT("\r\n");
	CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,RspOut);
	UnLock(&guiLock);
	return WIFI_RET_OK;
}
int RedpineWifiScan(ST_WIFI_AP *outAps, unsigned int ApCount)
{
	uchar Resp[2048],Resp2[100];
	int wantlen,apcount,iRet,secmode,i = 0;
    T_SOFTTIMER wifi_timer;
    
	if (gszWifiStatus.WState == WIFI_STATUS_CLOSE)
	{
		return WIFI_RET_ERROR_NOTOPEN;
	}
	if (gszWifiStatus.WState != WIFI_STATUS_NOTCONN)
	{
		return WIFI_RET_ERROR_STATUSERROR;
	}
	if (ApCount == 0)
	{
		return WIFI_RET_ERROR_PARAMERROR;
	}
	if (outAps == NULL)
	{
		return WIFI_RET_ERROR_NULL;
	}
	//因为为了省电，pwmode已经被设置为2，当需要扫描和连接时需要设置为0
	memset(Resp2, 0, sizeof(Resp2));
	iRet = RedpineWifiSendCmd("AT+RSI_PWMODE=0\r\n",0, Resp2, 8, 50);
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp2,iRet);
	if (strstr(Resp2, "OK") == NULL)
	{
		s_RpSetErrorCode(Resp2[5]);
		s_RpAtCmdFail();
		if (s_RpGetAtCmdFailCnt() > 10)
		{
			s_RpWifiSoftReset();
		}
		return WIFI_RET_ERROR_NORESPONSE;
	}
	
	//one ap ssid=32bytes,secmode=1byte,rssi=1byte
	apcount = 0;
	wantlen = SSID_MAXLEN+2+strlen("OK");
	memset(Resp, 0, wantlen);
	RedpineWifiSendCmd("AT+RSI_SCAN=0\r\n",0, Resp, wantlen, 10000);
	if (strstr(Resp, "ERROR") != NULL) 
	{	
		apcount = 0;
		return apcount;
	}
	
	apcount++;
	s_TimerSetMs(&wifi_timer,2000);
	while(s_TimerCheckMs(&wifi_timer))
	{
		RedpineWifiSendCmd(NULL,0, Resp+apcount*(SSID_MAXLEN+2)+2, SSID_MAXLEN+2, 100);
		if (strstr(Resp+apcount*(SSID_MAXLEN+2)+2, "\r\n") !=NULL)
		{
			break;
		}
		apcount++;
	}
	if (apcount > MAX_AP_COUNT) apcount = MAX_AP_COUNT;
	WIFI_DEBUG_PRINT("[%s,%d]apcount=%d\r\n",FILELINE,apcount);
	if (apcount > ApCount)
	{
		apcount = ApCount;
	}
	for (i = 0;i < apcount;i++)
	{
		memcpy(outAps[i].Ssid, Resp+strlen("OK")+34*i,SSID_MAXLEN);
		secmode = Resp[strlen("OK")+34*i+SSID_MAXLEN];
		if (secmode == RP_WLAN_SEC_UNSEC)
		{
			outAps[i].SecMode = WLAN_SEC_UNSEC;
		}
		else if (secmode == RP_WLAN_SEC_WEP)
		{
			outAps[i].SecMode = WLAN_SEC_WEP;
		}
		else
		{
			outAps[i].SecMode = WLAN_SEC_WPAPSK_WPA2PSK;
		}
		outAps[i].Rssi= Resp[strlen("OK")+34*i+SSID_MAXLEN+1];
		outAps[i].Rssi = (0-outAps[i].Rssi);//rssi 是负值
	}
	memset(Resp2, 0, sizeof(Resp2));
	RedpineWifiSendCmd("AT+RSI_PWMODE=2\r\n",0, Resp2, 4, 50);
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp2,iRet);
	if (strstr(Resp2, "OK") == NULL)
	{
		s_RpSetErrorCode(Resp2[5]);
		s_RpAtCmdFail();
		return WIFI_RET_ERROR_NORESPONSE;
	}
	return apcount;
}
int RedpineWifiSendRecv(const uchar * ATstrIn,int ATstrLen,uchar *RspOut,ushort want_len,ushort TimeOut)
{
    const int INTERVAL_TIMEOUT = 100;
	uchar ucRet;
	int iRet;
	int len = 0;
    T_SOFTTIMER wifi_timer;

	WIFI_DEBUG_PRINT("[%s,%d]guiLock=%d\r\n",FILELINE,guiLock);
	while(TryLock(&guiLock)==0);//被定时器刷新图标锁定时等待
	if (ATstrIn != NULL)
	{
		giCtrlRead = giCtrlWrite;//clean rx fifo
		if (ATstrLen == 0) ATstrLen = strlen(ATstrIn);

		ucRet = PortSends(P_WIFI,(uchar *)ATstrIn, ATstrLen);
		if (ucRet != 0)
		{
			UnLock(&guiLock);
			return WIFI_RET_ERROR_NOTOPEN;
		}
		while(PortTxPoolCheck(P_WIFI));
	}
	if (RspOut == NULL || want_len == 0)
	{
		UnLock(&guiLock);
		return len;
	}

	s_TimerSetMs(&wifi_timer,TimeOut);
    len = 0;
    while(1)
    {
        if (s_TimerCheckMs(&wifi_timer) == 0) break;
        iRet = s_RpWifiReadFromFifo(RspOut+len, want_len-len);
        if (iRet  > 0)
        {
            len += iRet;
            break;
        }
    }
    if (len >= want_len) goto exit;

    s_TimerSetMs(&wifi_timer,INTERVAL_TIMEOUT);
	while(1)
	{
		if (s_TimerCheckMs(&wifi_timer) == 0) break;
		iRet = s_RpWifiReadFromFifo(RspOut+len, want_len-len);
		if(iRet > 0)
		{
			len += iRet;
			if (len >= want_len) break;
            s_TimerSetMs(&wifi_timer,INTERVAL_TIMEOUT);
		}
	}

exit:
	RspOut[len] = 0;
	UnLock(&guiLock);
	return len;
}

void WIFI_DEBUG_HEX_PRINT(const char *tip, unsigned char *data, int len)
{
    int i;

    WIFI_DEBUG_PRINT("%s", tip);   

    for (i = 0; i < len; i++)
        WIFI_DEBUG_PRINT("%02x ", *(data + i));

    WIFI_DEBUG_PRINT("\r\n");
    return ;
}

int RedpineWifiScanExt(ST_WIFI_AP_EX *pstWifiApsInfo, unsigned int maxApCnt)
{
	return WIFI_RET_ERROR_STATUSERROR;
}

void s_WifiAsciiToHex(uchar *In, uchar *Out, int InLen);
int RedpineWifiConnect(ST_WIFI_AP *Ap,ST_WIFI_PARAM *WifiParam)
{
	uchar ATstrIn[100];
	uchar Resp[200];
	int iRet;
	int SecMode = 0;
	uchar ucWepHex[40];
	if (gszWifiStatus.WState == WIFI_STATUS_CLOSE)
	{
		return WIFI_RET_ERROR_NOTOPEN;
	}
	if (gszWifiStatus.WState != WIFI_STATUS_NOTCONN)
	{
		return WIFI_RET_ERROR_STATUSERROR;
	}
	s_RpPortInit();
	s_RpNetCloseAllSocket();	
	memset(Resp, 0, sizeof(Resp));
	iRet = RedpineWifiSendCmd("AT+RSI_PWMODE=0\r\n",0, Resp, 8, 50);
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
	if (strstr(Resp, "OK") == NULL)
	{
		s_RpSetErrorCode(Resp[5]);
		s_RpAtCmdFail();
		if (s_RpGetAtCmdFailCnt() > 10)
		{
			s_RpWifiSoftReset();
		}
		return WIFI_RET_ERROR_NORESPONSE;
	}
	
	//加密方式的定义保持兼容
	if (Ap->SecMode == WLAN_SEC_UNSEC)
	{
		SecMode = RP_WLAN_SEC_UNSEC;
	}
	else if (Ap->SecMode == WLAN_SEC_WEP)
	{
		SecMode = RP_WLAN_SEC_WEP;
		if (WifiParam->Wep.KeyLen != KEY_WEP_LEN_64 
		 && WifiParam->Wep.KeyLen != KEY_WEP_LEN_128)
		{
			return WIFI_RET_ERROR_PARAMERROR;
		}
		if (WifiParam->Wep.Idx != 0)
		{
			return WIFI_RET_ERROR_PARAMERROR;
		}
	}
	else  if (Ap->SecMode == WLAN_SEC_WPAPSK_WPA2PSK || 
              Ap->SecMode == WLAN_SEC_WPA_PSK || 
              Ap->SecMode == WLAN_SEC_WPA2_PSK)
	{
		SecMode = RP_WLAN_SEC_WPAPSK_WPA2PSK;
	}
	else
	{
		return WIFI_RET_ERROR_PARAMERROR;
	}
	//set psk
	if (SecMode == RP_WLAN_SEC_WEP)
	{
		memset(Resp, 0, sizeof(Resp));
		RedpineWifiSendCmd("AT+RSI_AUTHMODE=0\r\n",0, Resp, 4, 50);
		WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			s_RpAtCmdFail();
			return WIFI_RET_ERROR_NORESPONSE;
		}
		memset(ATstrIn, 0, sizeof(ATstrIn));
		
		memset(ucWepHex, 0, sizeof(ucWepHex));
		s_WifiAsciiToHex(WifiParam->Wep.Key[WifiParam->Wep.Idx],ucWepHex, WifiParam->Wep.KeyLen);
		sprintf(ATstrIn, "AT+RSI_PSK=%s\r\n",ucWepHex);
		
		memset(Resp, 0, sizeof(Resp));
		RedpineWifiSendCmd(ATstrIn,0, Resp, 4, 50);
		WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			s_RpAtCmdFail();
			return WIFI_RET_ERROR_NORESPONSE;
		}
	}
	else if (SecMode == RP_WLAN_SEC_WPA_WPA2 || SecMode == RP_WLAN_SEC_WPAPSK_WPA2PSK)
	{
		if (strlen(WifiParam->Wpa) == 0)
		{
			return WIFI_RET_ERROR_PARAMERROR;
		}
		memset(ATstrIn, 0, sizeof(ATstrIn));
		sprintf(ATstrIn, "AT+RSI_PSK=%s\r\n",WifiParam->Wpa);
		memset(Resp, 0, sizeof(Resp));
		RedpineWifiSendCmd(ATstrIn,0, Resp, 4, 50);
		WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			s_RpAtCmdFail();
			return WIFI_RET_ERROR_NORESPONSE;
		}
	}
	
	if (strcmp(gszNowAp.Ssid, Ap->Ssid) != 0)
	{
		giNowApChannel = 0;//已连接过的热点不是此次连接的热点，因此无法使用已知信道快速连接
	}
	memcpy(&gszNowAp, Ap, sizeof(ST_WIFI_AP));
	memcpy(&gszNowConnectParam, WifiParam, sizeof(ST_WIFI_PARAM));
	gszWifiStatus.WState = WIFI_STATUS_SCAN;
	giWifiErrorCode = WIFI_RET_ERROR_NOTCONN;
	return WIFI_RET_OK;
	
}
int RedpineWifiDisconnect(void)
{
    uchar Resp[200];
    int iRet, flag;

	if (gszWifiStatus.WState == WIFI_STATUS_CLOSE)
	{
		return WIFI_RET_ERROR_NOTOPEN;
	}
    if (gszWifiStatus.WState == WIFI_STATUS_SLEEP)
        return WIFI_RET_ERROR_STATUSERROR;

	s_RpNetCloseAllSocket();
    if (gszWifiStatus.WState == WIFI_STATUS_NOTCONN)
        goto exit;

    irq_save(flag);
    while(1)
    {
        if (gszWifiStatus.WState == WIFI_STATUS_IPCONF ||
            gszWifiStatus.WState == WIFI_STATUS_SETDNS ||
            gszWifiStatus.WState == WIFI_STATUS_CONNECTED ||
            gszWifiStatus.WState == WIFI_STATUS_SETAUTHMODE ||
            gszWifiStatus.WState == WIFI_STATUS_NWPARAMS)
        {
            gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
            break;
        }

        if (gszWifiStatus.WState == WIFI_STATUS_NOTCONN ||
            gszWifiStatus.WState == WIFI_STATUS_CLOSE)
        {
            irq_restore(flag);
            goto exit;
        }

        irq_restore(flag);
        DelayMs(20);
        irq_save(flag);
    }
    irq_restore(flag);

    gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
    memset(Resp, 0, sizeof(Resp));
    iRet = RedpineWifiSendCmd("AT+RSI_PWMODE=0\r\n",0, Resp, 4, 50);
    WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
    if (strstr(Resp, "OK") == NULL)
    {
        s_RpSetErrorCode(Resp[5]);
        s_RpAtCmdFail();
        return WIFI_RET_ERROR_NORESPONSE;
    }
    memset(Resp, 0, sizeof(Resp));
    iRet = RedpineWifiSendCmd("AT+RSI_DISASSOC\r\n",0,Resp,4,1000);
    WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
    if (strstr(Resp, "OK") == NULL)
    {
        s_RpSetErrorCode(Resp[5]);
        s_RpAtCmdFail();
        return WIFI_RET_ERROR_NORESPONSE;
    }
    DelayMs(50);
    memset(Resp, 0, sizeof(Resp));
    iRet = RedpineWifiSendCmd("AT+RSI_PWMODE=2\r\n",0, Resp, 4, 50);
    WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
    if (strstr(Resp, "OK") == NULL)
    {
        s_RpSetErrorCode(Resp[5]);
        s_RpAtCmdFail();
        return WIFI_RET_ERROR_NORESPONSE;
    }
    memset(gszWifiStatus.HostAddr, 0, sizeof(gszWifiStatus.HostAddr));
    DelayMs(50);//wait powermode stable

exit:
	giWifiErrorCode = WIFI_RET_ERROR_NOTCONN;
	return WIFI_RET_OK;
}

int RedpineWifiCheck(ST_WIFI_AP *Ap)
{
	if (gszWifiStatus.WState == WIFI_STATUS_CLOSE)
	{
		return WIFI_RET_ERROR_NOTOPEN;
	}
	else if (gszWifiStatus.WState == WIFI_STATUS_CONNECTED)
	{
		int Signal;
		Signal = WifiRssiToSignal(gszNowAp.Rssi);
		if (Ap != NULL)
		{
			memcpy(Ap, &gszNowAp, sizeof(ST_WIFI_AP));
		}
		return Signal;
	}
	else if (gszWifiStatus.WState == WIFI_STATUS_NOTCONN)
	{
		return giWifiErrorCode;
	}
	else if (gszWifiStatus.WState == WIFI_STATUS_SLEEP)
	{
		return WIFI_RET_ERROR_STATUSERROR;
	}
	else
	{
		return 0;//connecting
	}
}
int RedpineWifiCtrl(uint iCmd, void *pArgIn, uint iSizeIn,void *pArgOut, uint iSizeOut)
{
	uchar Resp[20];
	T_SOFTTIMER tm;
    static uchar gucRpMac[6]={0,};

	if (gszWifiStatus.WState == WIFI_STATUS_CLOSE)
	{
		return WIFI_RET_ERROR_NOTOPEN;
	}
	if (iCmd > (WIFI_CTRL_CMD_MAX_INDEX - 1)) 
		return WIFI_RET_ERROR_PARAMERROR;
	if (iCmd == WIFI_CTRL_ENTER_SLEEP)
	{
		if (gszWifiStatus.WState != WIFI_STATUS_NOTCONN)
		{
			return WIFI_RET_ERROR_STATUSERROR;
		}
		memset(Resp, 0, sizeof(Resp));
		RedpineWifiSendCmd("AT+RSI_SLEEPTIMER=1000\r\n", 0, Resp, 4, 50);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			RedpineWifiSendCmd("AT+RSI_PWMODE=2\r\n", 0, Resp, 4, 50);
			return WIFI_RET_ERROR_NORESPONSE;
		}
		memset(Resp, 0, sizeof(Resp));
		RedpineWifiSendCmd("AT+RSI_PWMODE=1\r\n", 0, Resp, 11, 50);
		if (strstr(Resp, "OK") == NULL)
		{
			s_RpSetErrorCode(Resp[5]);
			RedpineWifiSendCmd("AT+RSI_PWMODE=2\r\n", 0, Resp, 4, 50);
			return WIFI_RET_ERROR_NORESPONSE;
		}
		//OK\r\nSLEEP\r\n
		gszWifiStatus.WState = WIFI_STATUS_SLEEP;
		return WIFI_RET_OK;
	}
	else if (iCmd == WIFI_CTRL_OUT_SLEEP)
	{
		if (gszWifiStatus.WState != WIFI_STATUS_SLEEP)
		{
			return WIFI_RET_ERROR_STATUSERROR;
		}
		gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
		s_TimerSetMs(&tm,2000); //auto wakeup at 1 second,  set 2 seconds
		while(s_TimerCheckMs(&tm))
		{
			//SLEEP\r\nOK\r\n
			memset(Resp, 0, sizeof(Resp));
			RedpineWifiSendCmd("AT+RSI_PWMODE=2\r\n", 0, Resp, 11, 100);
			if (strstr(Resp, "OK"))
			{
				WIFI_DEBUG_PRINT("wakeup, got:%s\r\n", Resp);
				return WIFI_RET_OK;
			}	
		}
		return WIFI_RET_ERROR_NORESPONSE;
	}
	else if (iCmd == WIFI_CTRL_GET_PARAMETER)
	{
		if (pArgOut == NULL) return WIFI_RET_ERROR_NULL;
		if (iSizeOut != sizeof(ST_WIFI_PARAM)) return WIFI_RET_ERROR_PARAMERROR;
		if (gszWifiStatus.WState != WIFI_STATUS_CONNECTED) return WIFI_RET_ERROR_STATUSERROR;
		memcpy((ST_WIFI_PARAM *)pArgOut, &gszNowConnectParam, sizeof(ST_WIFI_PARAM));
		return WIFI_RET_OK;
	}
	else if (iCmd == WIFI_CTRL_GET_MAC)
	{
		if (pArgOut == NULL) return WIFI_RET_ERROR_NULL;
		if (iSizeOut != 6) return WIFI_RET_ERROR_PARAMERROR;
		if (memcmp(gucRpMac, "\x00\x00\x00\x00\x00\x00", 6) != 0)
		{
			memcpy((uchar *)pArgOut, gucRpMac, 6);
		}
		else
		{
			if ((gszWifiStatus.WState != WIFI_STATUS_NOTCONN) && (gszWifiStatus.WState != WIFI_STATUS_CONNECTED))
				return WIFI_RET_ERROR_STATUSERROR;
			if (s_rpwifi_mac_get(gucRpMac) != 0) 
				return WIFI_RET_ERROR_NORESPONSE;
			memcpy((uchar *)pArgOut, gucRpMac, 6);
		}
		return WIFI_RET_OK;
	}
	else if ((iCmd == WIFI_CTRL_GET_BSSID) || (iCmd == WIFI_CTRL_SET_DNS_ADDR))
	{		
		return WIFI_RET_ERROR_NOTSUPPORT;
	}
}

#define BAUDCHECK_REQ 0x1C
#define BAUDCHECK_ACK 0x55
int s_RpWifiCheckBaudRate(void)
{
	uchar ucRet,ch;
    T_SOFTTIMER timer1,timer2;	
    
	DelayMs(100);
	s_TimerSetMs(&timer1,1000);
	while(1)
	{
		PortSend(P_WIFI,BAUDCHECK_REQ);
		//if the 0x55 not response,send 0x1c 200ms later again
		s_TimerSetMs(&timer2,200);
		while(2)
		{
			ucRet = PortRecv(P_WIFI, &ch, 0);
			if (ucRet == 0)
			{
				if (ch == BAUDCHECK_ACK)
				{
					PortSend(P_WIFI,BAUDCHECK_ACK);
					return WIFI_RET_OK;
				}
			}
			if (s_TimerCheckMs(&timer2)==0) break;
		}
		if (s_TimerCheckMs(&timer1)==0) break;
	}
	return WIFI_RET_ERROR_NODEV;
	
}
int sRedpineInit(void)
{
	int iRet,i,buflen,count;
	uchar Resp[200], string[100], buffer[60*1024];
	const uchar *buf;

	iRet = s_RpWifiCheckBaudRate();
	if (iRet != WIFI_RET_OK) return WIFI_RET_ERROR_NODEV;
	memset(Resp, 0, sizeof(Resp));
	//read the init info
	memset(string,0,sizeof(string));
	strcpy(string, " WELCOME TO REDPINE SIGNALS\r\n \r\n ");
	strcat(string, " Firmware upgrade (y/n) ");
	iRet = PortRecvs(P_WIFI, Resp,strlen(string), 100);
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);
	PortSends(P_WIFI,"n\n",2);
	PortRecvs(P_WIFI, Resp, 2, 10);
	memset(Resp, 0, sizeof(Resp));
	memset(string,0,sizeof(string));
	strcpy(string, " Loading... \r\n");
	strcat(string, " Loading Done\r\n");
	iRet= PortRecvs(P_WIFI, Resp, strlen(string), 100);
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);

	//upgrade the fw
	memset(Resp, 0, sizeof(Resp));
	PortReset(P_WIFI);
	PortSends(P_WIFI,"AT+RSI_FWVERSION?\r\n", strlen("AT+RSI_FWVERSION?\r\n"));
	iRet = PortRecvs(P_WIFI, Resp, 9, 300);
	if (iRet<0) return WIFI_RET_ERROR_NORESPONSE;

	iRet = s_RpWifiGetAndCheckDriver(buffer, sizeof(buffer), Resp);
	if (iRet < 0) return WIFI_RET_ERROR_DRIVER;
    buf=buffer; 
	buflen = iRet;

	i = 0;
	count = buflen / 1024;
	while(i < count)
	{
		PortSends(P_WIFI, (uchar *)buf+i*1024, 1024);
		while(PortTxPoolCheck(P_WIFI));
		i++;
	}
	PortSends(P_WIFI, (uchar *)buf+i*1024, buflen-1024*count);
	while(PortTxPoolCheck(P_WIFI));
	
	memset(Resp, 0, sizeof(Resp));
	iRet = PortRecvs(P_WIFI, Resp, strlen("OK\r\n"),10);
	WIFI_DEBUG_PRINT("[%s,%d]Resp=%s,iRet=%d\r\n",FILELINE,Resp,iRet);

	return WIFI_RET_OK;
}
int s_RpWifiSoftReset(void)
{
	uchar Resp[200];
	int iRet;
	memset(Resp,0,sizeof(Resp));
	iRet = RedpineWifiSendCmd("AT+RSI_RESET\r\n",0, Resp,strlen("OK\r\n"), 50);
	if (strstr(Resp, "OK") == NULL) return WIFI_RET_ERROR_NORESPONSE;
	memset(Resp,0,sizeof(Resp));
	iRet = RedpineWifiSendCmd("AT+RSI_BAND=0\r\n",0, Resp,strlen("OK\r\n"), 50);
	if (strstr(Resp, "OK") == NULL) return WIFI_RET_ERROR_NORESPONSE;
	memset(Resp,0,sizeof(Resp));
	iRet = RedpineWifiSendCmd("AT+RSI_INIT\r\n",0, Resp,strlen("OK\r\n"), 50);
	if (strstr(Resp, "OK") == NULL) return WIFI_RET_ERROR_NORESPONSE;
	memset(Resp,0,sizeof(Resp));
	iRet = RedpineWifiSendCmd("AT+RSI_PWMODE=2\r\n",0, Resp,strlen("OK\r\n"), 50);
	if (strstr(Resp, "OK") == NULL) return WIFI_RET_ERROR_NORESPONSE;
	memset(Resp,0,sizeof(Resp));
	//wpa2-psk len is less then 63bytes,Dnsserver add to nwparams
	iRet = RedpineWifiSendCmd("AT+RSI_FEAT_SEL=9\r\n",0, Resp,strlen("OK\r\n"), 50);
	if (strstr(Resp, "OK") == NULL) return WIFI_RET_ERROR_NORESPONSE;
	memset(Resp,0,sizeof(Resp));
	iRet = RedpineWifiSendCmd("AT+RSI_FWVERSION?\r\n",0, Resp,9, 50);
	if (strstr(Resp, "OK") == NULL) return WIFI_RET_ERROR_NORESPONSE;
	//Resp=OKx.x.x\r\n
    memset(gucWifiFwVer, 0x00, sizeof(gucWifiFwVer));
    memcpy(gucWifiFwVer, Resp+2, 5);
	return WIFI_RET_OK;
}
int s_RpWifiInitForBack(void)
{
	uchar AtstrIn[30];
	int iRet;
	static int timeout_cnt = 0;
	static uchar Resp[10];
	static int reset_step = 0;
	static int resp_flag = 0;
	static int resp_index = 0;

	if (0 == resp_flag)
	{
		memset(AtstrIn,0,sizeof(AtstrIn));
		memset(Resp, 0, sizeof(Resp));
		resp_index = 0;
		resp_flag = 1;
		timeout_cnt = 0;
		switch(reset_step)
		{
			case 0:
				strcpy(AtstrIn, "AT+RSI_RESET\r\n");
				break;
			case 1:
				strcpy(AtstrIn, "AT+RSI_BAND=0\r\n");
				break;
			case 2:
				strcpy(AtstrIn, "AT+RSI_INIT\r\n");
				break;
			case 3:
				strcpy(AtstrIn, "AT+RSI_FEAT_SEL=9\r\n");
				break;
			default:
				break;
		}
		CMD_DEBUG_PRINT("[%s,%d]ATCMD:%s\r\n",FILELINE,AtstrIn);
		PortSends(P_WIFI, AtstrIn, strlen(AtstrIn));
		CMD_DEBUG_PRINT("[%s,%d]ATRESP:\r\n",FILELINE);
	}
	else
	{
		timeout_cnt++;
		iRet = PortRecvs(P_WIFI, Resp+resp_index, sizeof(Resp), 0);
		if (iRet > 0)
		{
			resp_index += iRet;
		}
		if (timeout_cnt >= 100)//1second
		{
			reset_step = 0;
			timeout_cnt = 0;
			CMD_DEBUG_PRINT("\r\n");
			CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,Resp);
			return -1;
		}
		if (paxstrnstr(Resp, "\r\n", resp_index, 2) != NULL)
		{
			resp_flag = 0;
			reset_step++;
			if (4 == reset_step)
			{
				reset_step = 0;
				CMD_DEBUG_PRINT("\r\n");
				CMD_DEBUG_PRINT("[%s,%d]ATRESP:%s\r\n",FILELINE,Resp);
				return 1;
			}
		}
	}
	return 0;
}

void s_RpGetLocalIp(uchar *ip)
{
	memcpy(ip, gszWifiStatus.HostAddr,4);
}

void s_RpWifiClearFifo(void)
{
	giRecRead = giRecWrite;
}
void s_RpWifiWriteToFifo(uchar *buf, int len)
{
	int size;
	if (len <= 0)return;
	size = sizeof(gucRecFifo);
	if (giRecWrite + len >= size)
	{
		int len1, len2;
		len1 = size - giRecWrite;
		len2 = len - len1;
		memcpy(&gucRecFifo[giRecWrite], buf, len1);
		giRecWrite = 0;
		memcpy(&gucRecFifo[giRecWrite], buf+len1, len2);
		giRecWrite += len2;
	}
	else
	{
		memcpy(&gucRecFifo[giRecWrite], buf, len);
		giRecWrite += len;
	}
}
int s_RpWifiReadFromFifo(uchar *buf, int want_len)
{
	int size;
	int canread_len;
	size = sizeof(gucRecFifo);
	canread_len = (giRecWrite-giRecRead+size) % size;
	if (canread_len < want_len)
	{
		want_len = canread_len;
	}
	if (giRecRead + want_len >= size)
	{
		int len1,len2;
		len1 = size - giRecRead;
		len2 = want_len - len1;
		memcpy(buf, &gucRecFifo[giRecRead], len1);
		giRecRead = 0;
		memcpy(buf+len1, &gucRecFifo[giRecRead], len2);
		giRecRead += len2;
	}
	else
	{
		memcpy(buf, &gucRecFifo[giRecRead], want_len);
		giRecRead += want_len;
	}
	return want_len;
}
int RpWifiUpdateOpen(void)
{
	int iRet;
	uchar Resp[200];
	uchar string[100];
	
	if (gszWifiStatus.WState != WIFI_STATUS_CLOSE)
	{
		RedpineWifiClose();
	}
	PortOpen(P_WIFI,"115200,8,n,2");
	s_RpWifiPowerSwitch(1);
	iRet = s_RpWifiCheckBaudRate();
	if (iRet != WIFI_RET_OK) return iRet;
	gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
	//read the init info
	memset(string,0,sizeof(string));
	strcpy(string, " WELCOME TO REDPINE SIGNALS\r\n \r\n ");
	strcat(string, " Firmware upgrade (y/n) ");
	PortRecvs(P_WIFI, Resp,strlen(string), 100);
	PortSends(P_WIFI,"y\n",2);
	PortRecvs(P_WIFI, Resp, 2, 10);
	return WIFI_RET_OK;
}

void s_RpWifiInSleep(void)
{
	unsigned int flag;
	
	WIFI_DEBUG_PRINT("[%s,%d]state=%d,SleepFlag=%d\r\n",FILELINE,gszWifiStatus.WState,giDeepSleepFlag);
	if (gszWifiStatus.WState == WIFI_STATUS_CLOSE)
	{
		return;
	}
	if (giDeepSleepFlag == 1)
	{
		return;
	}
	giRpWifiStateBak = gszWifiStatus.WState;

	gpio_set_pin_interrupt(WIFI_INT_GPIO,0);
	
	//2.关闭连接
	RedpineWifiDisconnect();

    RpwifiSoftIntEnable(0);
	s_RpWifiPowerSwitch(0);
	//3.关闭串口
	PortClose(P_WIFI);
	//ScrSetIcon(0,0);
	giDeepSleepFlag = 1;
}
void s_RpWifiOutSleep(void)
{
	int iRet;
	unsigned int flag;
	uchar ComParam[100];

	
	WIFI_DEBUG_PRINT("[%s,%d]state=%d,SleepFlag=%d\r\n",FILELINE,gszWifiStatus.WState,giDeepSleepFlag);
	if (gszWifiStatus.WState == WIFI_STATUS_CLOSE)
	{
		return;
	}
	if (giDeepSleepFlag == 0)
	{
		return;
	}
	giDeepSleepFlag = 0;
	memset(ComParam, 0, sizeof(ComParam));
	sprintf(ComParam, "%d,8,n,2",WIFI_BAUDRATE);
	PortOpen(P_WIFI,ComParam);
	s_RpWifiPowerSwitch(1);
	s_RpAtCmdFailCntReset();
	iRet = sRedpineInit();
	if (iRet != WIFI_RET_OK)
	{
		s_RpWifiPowerSwitch(0);
		PortClose(P_WIFI);
		return;
	}
	memset(&gszWifiStatus, 0, sizeof(ST_WIFI_STATUS));
	memset(gucRecFifo, 0, sizeof(gucRecFifo));
	giRecWrite = 0;
	giRecRead = 0;
	PortReset(P_WIFI);
	s_RpSocketInit();
	gszWifiStatus.WState = WIFI_STATUS_NOTCONN;
    RpwifiSoftIntEnable(1);
	iRet = s_RpWifiSoftReset();
	if (iRet != 0)
	{	
		gszWifiStatus.WState = WIFI_STATUS_CLOSE;
        RpwifiSoftIntEnable(0);
		s_RpWifiPowerSwitch(0);
		PortClose(P_WIFI);
		return;
	}
	
	gpio_set_pin_type(WIFI_INT_GPIO,GPIO_INPUT);
	s_setShareIRQHandler(WIFI_INT_GPIO,INTTYPE_FALL,redpinewifi_gpio_handler);
	gpio_set_pin_interrupt(WIFI_INT_GPIO,1);

	if (giRpWifiStateBak == WIFI_STATUS_CONNECTED)
	{
		RedpineWifiConnect(&gszNowAp,&gszNowConnectParam);
	}
}
//用于在带0x00的数组中查找固定数组
char * paxstrnstr(char *str1, char *str2, int str1len,int str2len)
{
	if(*str2)
	{
		int m,n;
		m = 0;
		while(m < str1len)
		{
			if (m+str2len > str1len)break;
			for(n=0; *(str1 + n) == *(str2 + n); n++)
			{
				if(n == str2len-1)
					return (char *)str1;
			}
			str1++;
			m++;
		}
		return NULL;
	}
	else
		return (char *)str1; 
}
unsigned int TryLock(unsigned int *lock)
{
	int getlock = 0;
	unsigned int flag;
	irq_save(flag);
	if(*lock == 0) {
		*lock = 1;
		getlock = 1;
		}
	irq_restore(flag);
	return getlock;
}
void UnLock(unsigned int *lock)
{
	*lock = 0;
}
void s_WifiAsciiToHex(uchar *In, uchar *Out, int InLen)
{
	int i;
	for (i = 0;i < InLen;i++)
	{
		sprintf(Out+2*i, "%02x",In[i]);
	}
	Out[2*i]= 0;
}

int s_RpWifiGetAndCheckDriver(char *driver_buf, int size, char *verRes)
{
	int iRet;
	int fd;
	int offset;
	int driver_size;

	memset(driver_buf, 0, size);
	offset = sprintf(driver_buf, "%s", "at+rsi_load=");
	iRet = RedPineGetTadm2(driver_buf+offset, size - offset);
	if (iRet < 0)
	{
		WIFI_DEBUG_PRINT("Get tadm2 failed<%d>\n", iRet);
		return WIFI_RET_ERROR_DRIVER;
	}
	driver_size = iRet;
	offset += driver_size-10;
	if (strstr(verRes, driver_buf+offset))
	{
		WIFI_DEBUG_PRINT("Find firmware, version is %s\n", driver_buf+offset);
		driver_buf[driver_size + 12] = '\r';
		driver_buf[driver_size+13] = '\n';
		return driver_size+14;
	}
	else
	{
		WIFI_DEBUG_PRINT("Driver not same<%s> != <%s>\n", verRes, driver_buf+driver_size+2);
		return WIFI_RET_ERROR_DRIVER;
	}
}

int RedPineWifiGetHVer(uchar * WifiFwVer)
{
	if(NULL==WifiFwVer) return WIFI_RET_ERROR_NULL;
    strcpy(WifiFwVer, gucWifiFwVer);
	return WIFI_RET_OK;
}

void SxxxWifiPowerOn(void)
{
	gpio_set_pin_val(WIFI_RST_GPIO, !WIFI_RST_ON);
	DelayMs(50);	
	gpio_set_pin_val(WIFI_PWR_GPIO, WIFI_PWR_ON);
	DelayMs(50);//等待电源稳定
	DelayMs(500);
	gpio_set_pin_val(WIFI_RST_GPIO, WIFI_RST_ON);
	DelayMs(50);	
	gpio_set_pin_val(WIFI_RST_GPIO, !WIFI_RST_ON);
	giWifiPowerState = 1;
}

int s_IsWifiPowerOn(void)
{
	return (giWifiPowerState == 1);
}

void SxxxWifiPowerOff(void)
{
	gpio_set_pin_val(WIFI_PWR_GPIO, !WIFI_PWR_ON);
	DelayMs(50);
	gpio_set_pin_val(WIFI_RST_GPIO, WIFI_RST_ON);
	DelayMs(50);	
	giWifiPowerState = 0;

}

void D210WifiPowerOn(void)
{
	gpio_set_pin_type(WIFI_RST_GPIO,GPIO_OUTPUT);
	gpio_set_pin_val(WIFI_PWR_GPIO, WIFI_PWR_ON);
	DelayMs(100);//等待电源稳定
	gpio_set_pin_val(WIFI_RST_GPIO, !WIFI_RST_ON);
	DelayMs(500);
	gpio_set_pin_val(WIFI_RST_GPIO, WIFI_RST_ON);
	DelayMs(50);	
	gpio_set_pin_val(WIFI_RST_GPIO, !WIFI_RST_ON);
	giWifiPowerState = 1;
}

void D210WifiPowerOff(void)
{
	gpio_set_pin_val(WIFI_PWR_GPIO, !WIFI_PWR_ON);
	DelayMs(50);
	gpio_set_pin_val(WIFI_RST_GPIO, WIFI_RST_ON);
	DelayMs(50);

	gpio_set_pin_type(WIFI_RST_GPIO,GPIO_INPUT);
	gpio_set_pull(WIFI_RST_GPIO, 0);
	gpio_enable_pull(WIFI_RST_GPIO);
	giWifiPowerState = 0;

}

T_WifiDrvConfig S900_cfg = {
	.intr_port = GPIOB,
	.intr_pin = 25,
	.pwr_port = GPIOB,
	.pwr_pin = 7,
	.pwr_on = 0,
	.rst_port = GPIOA,
	.rst_pin = 3,
	.rst_on = 0,
	.baudrate = 921600,
	.PowerOn = SxxxWifiPowerOn,
	.PowerOff = SxxxWifiPowerOff,
};
T_WifiDrvConfig D210_cfg = {
	.intr_port = GPIOB,
	.intr_pin = 25,
	.pwr_port = GPIOB,
	.pwr_pin = 7,
	.pwr_on = 0,
	.rst_port = GPIOA,
	.rst_pin = 3,
	.rst_on = 0,
	.baudrate = 921600,
	.PowerOn = D210WifiPowerOn,
	.PowerOff = D210WifiPowerOff,
};
T_WifiDrvConfig S500_cfg = {
	.intr_port = GPIOB,
	.intr_pin = 4,
	.pwr_port = GPIOE,
	.pwr_pin = 0,
	.pwr_on = 1,
	.rst_port = GPIOE,
	.rst_pin = 2,
	.rst_on = 0,
	.baudrate = 115200,
	.PowerOn = SxxxWifiPowerOn,
	.PowerOff = SxxxWifiPowerOff,
};
T_WifiDrvConfig S800_cfg = {
	.intr_port = GPIOB,
	.intr_pin = 4,
	.pwr_port = GPIOC,
	.pwr_pin = 8,
	.pwr_on = 1,
	.rst_port = GPIOE,
	.rst_pin = 2,
	.rst_on = 1,
	.baudrate = 115200,
	.PowerOn = SxxxWifiPowerOn,
	.PowerOff = SxxxWifiPowerOff,
};
void D200WifiPowerOn(void)
{
	gpio_set_pin_type(WIFI_RST_GPIO,GPIO_OUTPUT);
	
	gpio_set_pin_val(WIFI_RST_GPIO, !WIFI_RST_ON);
	DelayMs(50);	
	s_PmuWifiPowerSwitch(1);
	DelayMs(50);//等待电源稳定
	DelayMs(500);
	gpio_set_pin_val(WIFI_RST_GPIO, WIFI_RST_ON);
	DelayMs(50);	
	gpio_set_pin_val(WIFI_RST_GPIO, !WIFI_RST_ON);	
	giWifiPowerState = 1;
}

void D200WifiPowerOff(void)
{
	s_PmuWifiPowerSwitch(0);
	DelayMs(50);
	gpio_set_pin_val(WIFI_RST_GPIO, WIFI_RST_ON);
	DelayMs(50);		
	giWifiPowerState = 0;
}

T_WifiDrvConfig D200_cfg = {
	.intr_port = GPIOB,
	.intr_pin = 10,
	.pwr_port = GPIOE,
	.pwr_pin = 0,
	.pwr_on = 0,
	.rst_port = GPIOB,
	.rst_pin = 9,
	.rst_on = 0,
	.baudrate = 921600,
	.PowerOn = D200WifiPowerOn,
	.PowerOff = D200WifiPowerOff,
};
void s_RedPineWifiDriverInit(void)
{
	int type, iRet;
	uchar FwVersion[20];

	type = get_machine_type();
	switch(type)
	{
	case S900:
		ptWifiDrvCfg = &S900_cfg;
		break;
	case D210:
		ptWifiDrvCfg = &D210_cfg;
		//add for power save
		writel_and(~0x0C000,GIO3_R_GRPF3_AUX_SEL_MEMADDR);
		gpio_set_pin_type(GPIOD, 14, GPIO_INPUT);
		gpio_set_pin_type(GPIOD, 15, GPIO_INPUT);
		gpio_set_pull(GPIOD, 14, 0);
		gpio_set_pull(GPIOD, 15, 0);
		gpio_enable_pull(GPIOD, 14);
		gpio_enable_pull(GPIOD, 15);
		break;
	case D200:
		ptWifiDrvCfg = &D200_cfg;
		break;
	case S500:
		ptWifiDrvCfg = &S500_cfg;
		break;
	case S800:
		ptWifiDrvCfg = &S800_cfg;
		break;
	default:
		ptWifiDrvCfg = NULL;
		return;
	}

    memset(FwVersion, 0, sizeof(FwVersion));
    iRet = RedPineGetPatchVersion(FwVersion, sizeof(FwVersion));
    WIFI_DEBUG_PRINT("[%s,%d]FwVersion=%s\r\n",FILELINE, FwVersion);
    if (iRet < 0)
    {
    	ScrPrint(5, 7, 0x00, "Wifi patch error<%d>",iRet);
    	DelayMs(1000);
    }
		
	gpio_set_pin_type(WIFI_PWR_GPIO,GPIO_OUTPUT);
	gpio_set_pin_type(WIFI_RST_GPIO,GPIO_OUTPUT);
	s_RpWifiPowerSwitch(0);

	//update firmware
	iRet = RedPineCheckModule();
	if (iRet != WIFI_RET_ERROR_DRIVER)
		return;
	ScrRestore(0);
	ScrCls();
	ScrPrint(0, 0, 0xE0, "Redpine WIFI Update");
	ScrPrint(0, 3, 0x60, "Update to %s", FwVersion);
	ScrPrint(0, 4, 0x60, "Please wait...");
	iRet = RpWifiUpdateFw();
	WIFI_DEBUG_PRINT("[%s,%d]WifiUpdateFw=%d\r\n",FILELINE, iRet);
	ScrRestore(1);
}

