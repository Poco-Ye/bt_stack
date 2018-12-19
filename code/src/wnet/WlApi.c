#include <string.h>
#include <ctype.h>
#include "base.h"
#include "stdarg.h"
#include "WlApi.h"
#include "../ipstack/include/socket.h"
#include "../comm/comm.h"

//#define Wl_DEBUG
#ifdef Wl_DEBUG
#define see_debug printk
#else
#define see_debug
#endif
/* Wirless Type */
#define WTYPE_GPRS 1
#define WTYPE_CDMA 2
#define WTYPE_TDSCDMA 3
#define WTYPE_WCDMA	4

#define WL_POWERON_TO 15  //15 Second
#define WL_SENDCMD_TO  4000  //4000ms
#define WL_RECVBYTE_TO 100   //100ms
#define WNET_UNKNOWN      		  	0x00
#define WNET_CDMA_MC323				0x0c
#define WNET_GPRS_MG323				0x0d
#define WNET_WCDMA_MU509			0x0e
#define WNET_CDMA_MC509				0x0f
#define WNET_GPRS_G620              0x10
#define WNET_GPRS_G610              0x11 
#define WNET_WCDMA_H330             0x12

#define WNET_GPRS_G510				0x13
#define WNET_GPRS_G535				0x14
#define WNET_WCDMA_H350             0x15
#define WNET_WCDMA_MU709            0x16

#define WL_PIN_DTR 0
#define WL_TCP_CONN_DEFAULT 2
#define RESET_CDMA_TO    (85*1000) //85 Seconds
#define RESET_GPRS_TO    (85*1000) //85 Seconds
#define CELL_WCDMA_TO    (30*1000) //85 Seconds
#define CDMA_SYS_VALID      2
#define PIN_MAXLEN          99

typedef struct{
	char DialNum[128];
	char Apn[128];
	char Uid[128];
	char Pwd[128];
	long Auth;
	int AliveInterval;
} WL_PARAM_STRUCT;

//Joshua _a for compatible
typedef struct{
	int onoff_port;
	int onoff_pin;
	int wake_port;
	int wake_pin;
	int pwr_port;
	int pwr_pin;
	int dtr_cts_port;
	int dtr_cts_pin;
	int dual_sim_port;
	int dual_sim_pin;
}T_WlDrvConfig;

static T_WlDrvConfig *ptWlDrvConfig = NULL;
/*********************************************
	 Global Variables Definition
*********************************************/

int mc509_flag=0;//MC509: 0->IMSI, 1->MEID

static uchar WlType = WNET_UNKNOWN; 
static int WlPortOpened=0; 
static uchar WlPowerFlag=0;

static int sim_ready=0;

static WL_PARAM_STRUCT s_WlParam;
static uchar apn_buff_ex[128];
static uchar dialnum_buff_ex[128];
static uchar mc509_rsp[256]={0,};
static int connect_num = WL_TCP_CONN_DEFAULT;
static int detect_tcp_peer_opened = 0;
static volatile uint PowerStep=0x00;
extern uchar cell_cmd[32];
extern uchar cell_rsp[32];
static volatile int sWlPowering;  /*After initial,the background task continue to power on*/
static T_SOFTTIMER tm_wl_pwr;
static T_SOFTTIMER tm_wl_pwr_waitfor_at;
static uchar sim_num=0;
static uchar WlEnableInit=0;/*Promit module re-init*/

volatile int task_lbs_flag=0;

extern uchar wnet_cell_buf[2048];
extern uchar wnet_ncell_buf[2048];
extern uchar wnet_mode_buf[2048];

int is_gprs_module();
int is_cdma_module();
int is_wcdma_module();
int TaskRelinkMgr();

void LBSTaskSet(int flag);
int LBSTaskGet(void);


uchar GetWlType()
{
    char context[64];
    static uchar wltype =0;

    if(wltype) return wltype;

    memset(context,0,sizeof(context));
    if(ReadCfgInfo("GPRS",context)>0){
        if(strstr(context, "MG323-B")) wltype=WNET_GPRS_MG323;
        else if(strstr(context, "G620-A50")) wltype=WNET_GPRS_G620;
        else if(strstr(context, "G610") || strstr(context, "01")) wltype=WNET_GPRS_G610;
        else if(strstr(context, "02")) wltype=WNET_GPRS_G510;
        else if(strstr(context, "03")) wltype=WNET_GPRS_G535;
        return wltype;
    }
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("CDMA",context)>0){
        if(strstr(context, "MC323")) wltype=WNET_CDMA_MC323;
        if(strstr(context, "MC509")) wltype=WNET_CDMA_MC509;
        return wltype;
    }
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("WCDMA",context)>0){
        if(strstr(context, "MU509")) wltype=WNET_WCDMA_MU509;
		if(strstr(context, "01")||strstr(context, "05")) wltype=WNET_WCDMA_H330;
        if(strstr(context, "10")) wltype=WNET_WCDMA_H350;
		if(strstr(context, "06")||strstr(context, "07")) wltype=WNET_WCDMA_MU709;
        return wltype;
    }

    return WNET_UNKNOWN;
}
uchar is_double_sim()
{
    char context[64];
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("DUAL_SIM",context)<0) return 0;
    if(strstr(context, "TRUE")) return 1;    
    return 0;
}

void WlComExchange(void)
{
	uchar ch;
	PortOpen(0,"115200,8,n,1");
	ScrPrint(0,7,0," MODEL Contact COM 0 ");
	while(1)
	{
        	if(PortRx(0,&ch)==0)PortTx(P_WNET,ch);
        	if(PortRx(P_WNET,&ch)==0)PortTx(0,ch);
        	if (!kbhit() && (getkey()==KEYCANCEL))break;
	}
	PortClose(0);
}
/*****************************
mode : 0 wait for a valid packet
	    >0 time out for byte 
*****************************/
int  WlSendCmd(const uchar * ATstr, uchar *rsp,ushort rsplen, ushort wtms, ushort mode)
{
    ushort rlen;
    int ret,i;
    char AcT;
    T_SOFTTIMER tm;	
    uchar rch;
    char *ptr, *ptr1;

    if(rsp) memset(rsp,0,rsplen);

	/* LBS start: "AT+LBSSTART\r", LBS stop: "AT+LBSSTOP\r" */
	if(strcmp(ATstr, "AT+LBSSTART\r") == 0) {
		LBSTaskSet(1);
		if (rsp&&rsplen>=7) strcpy(rsp, "\r\nLBS START OK\r\n");
		return 0;
	}
	else if(strcmp(ATstr, "AT+LBSSTOP\r") == 0) {
		LBSTaskSet(0);
		if (rsp&&rsplen>=7) strcpy(rsp, "\r\nLBS STOP OK\r\n");
		return 0;
	}
	
    if(wnet_ppp_used()){
		if(rsplen > 2048)
				rsplen = 2048;
		if(strcmp(ATstr,"AT+GETSCI\r") == 0){	
			wnet_get_scellinfo(rsp, rsplen);
			return 0;
		}else if(strcmp(ATstr,"AT+GETNETMODE\r") == 0){
			wnet_get_mode(rsp,rsplen);
			return 0;
		}else if(strcmp(ATstr,"AT+GETNCI\r") == 0){
			wnet_get_ncellinfo(rsp, rsplen);
			return 0;
		}else if(strcmp(ATstr,"AT+GETCIMI\r") == 0){
			memcpy(rsp,mc509_rsp,sizeof(mc509_rsp));
			return 0;
		}else
			return WL_RET_ERR_PORTINUSE;
    } 
    if(!WlPortOpened) return WL_RET_ERR_PORTNOOPEN;
    
    PortReset(P_WNET);

    if((ATstr==NULL)&&(rsp==NULL))
    {
    	if((rsplen|wtms|mode)==0)
    	{
    		WlComExchange();
    		return RET_OK;
    	}
    	return WL_RET_ERR_PARAMER;
    }
    if(ATstr!=NULL)
    {
    	/*send AT command*/
    	if(strlen(ATstr) > 1024) return WL_RET_ERR_PARAMER;
    	PortSends(P_WNET, (uchar *)ATstr,strlen(ATstr));	
    	see_debug("%s\r\n",ATstr);
    }
    if((rsp==NULL)||(rsplen ==0)) return RET_OK;

    /*waiting for recv*/
    s_TimerSetMs(&tm,wtms);
    rlen=0;
    while(rlen != rsplen)
    {
    	ret=PortRecv(P_WNET,&rch,(ushort)wtms);
    	if(RET_OK==ret)
    	{
    		rsp[rlen++]=rch;
    		if(mode) wtms=mode>WL_RECVBYTE_TO?mode:WL_RECVBYTE_TO;
    		else
    		{
    			wtms=s_TimerCheckMs(&tm);
    			if((rch!='\n')) continue;
                        for(i=0;i<rlen;i++) { /*check the response pool has valid data*/
                            if(('\r'!=rsp[i])&&('\n'!=rsp[i])) return RET_OK;
                        }
    		}
    	}
    	else 
    	{
    		if(rlen&&mode) break;
    		return WL_RET_ERR_TIMEOUT;
    	}
    }
    if(rlen<rsplen) rsp[rlen]=0;
    see_debug(".%s\r\n",rsp);

    /*patch for re-connect*/
    if (strncmp(ATstr, "ATD",3) ==0) //record the dialing num
    {
    	strncpy(dialnum_buff_ex, ATstr,sizeof(dialnum_buff_ex)-1);
    }
    if (strncmp(ATstr, "AT+CGDCONT=",11) ==0) //record the APN
    {
        //AT+CGDCONT=1,"IP","cmnet";
        ptr = strstr(ATstr, "IP");
        if(ptr != NULL) ptr = strstr(ptr, ",");
        if(ptr != NULL) ptr = strstr(ptr, "\"");
        if(ptr != NULL) 
    	 {
            ptr++;
            ptr1 = strstr(ptr, "\"");
            if(ptr1 != NULL)
            {
                if(ptr1- ptr < sizeof(apn_buff_ex))  strncpy(apn_buff_ex,ptr,ptr1- ptr);				
            }	
        }	
    }
    return RET_OK;
}

int WlCheckSim()
{
    uchar rsp[512];
    int cnt,ret;

    for(cnt=0;cnt<30;cnt++)
    {
        ret=WlSendCmd("AT+CPIN?\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
        if(WL_RET_ERR_TIMEOUT==ret) continue;
        if(RET_OK!=ret) return ret;

        if(strstr(rsp, "+CME ERROR:") && strstr(rsp, "10")) /*"+CME ERROR: 10"or "+CME ERROR:10"*/
        {
            if(cnt<20) continue;
            sim_ready=WL_RET_ERR_NOSIM;
            return WL_RET_ERR_NOSIM;
        }
        else if(strstr(rsp, "+CPIN:"))
        {
            if(strstr(rsp, "READY"))
            {
                sim_ready=RET_OK;
                return RET_OK;
            }
            else if(strstr(rsp, "SIM PIN")) return WL_RET_ERR_NEEDPIN;
            else if(strstr(rsp, "SIM PUK")) return WL_RET_ERR_NEEDPUK;
            else if(strstr(rsp, "RUIM PIN")) return WL_RET_ERR_NEEDPIN;
            else if(strstr(rsp, "RUIM PUK")) return WL_RET_ERR_NEEDPUK;
            else if(strstr(rsp, "UIM PIN")) return WL_RET_ERR_NEEDPIN;
            else if(strstr(rsp, "UIM PUK")) return WL_RET_ERR_NEEDPUK;
        }
        else if(strstr(rsp,"+CME ERROR:R-UIM not inserted"))
        {
            sim_ready=WL_RET_ERR_NOSIM;
            return WL_RET_ERR_NOSIM;
        }
		else if(strstr(rsp, "+CME ERROR:") && strstr(rsp, "14"))
		{
			DelayMs(2500);
		}
        else if(strstr(rsp, "+CME ERROR:") && strstr(rsp, "13"))
        {
            if(WlType == WNET_WCDMA_MU509||WlType == WNET_WCDMA_MU709)
            {
                sim_ready=WL_RET_ERR_NOSIM;
                return WL_RET_ERR_NOSIM;
            }
            DelayMs(1000);
        }
		else if(strstr(rsp, "+CME ERROR:") && strstr(rsp, "3"))//operation not allowed
        {
            if(WlType != WNET_WCDMA_H330 && WlType != WNET_WCDMA_H350) return WL_RET_ERR_RSPERR;
			DelayMs(2000);
        }
        else if(strstr(rsp, "ERROR"))
        {
            return WL_RET_ERR_RSPERR;
        }
    }
    return WL_RET_ERR_RSPERR;
}
static int WlSimPin(const uchar *pin)
{
    uchar   rsp[1024];
    uchar   cmd[128];
    if(strlen((char *)pin)>PIN_MAXLEN) return WL_RET_ERR_PARAMER;
    sprintf(cmd,"AT+CPIN=\"%s\"\r",pin);
    WlSendCmd(cmd,rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
    if(strstr(rsp,"OK")) return RET_OK;
    if(strstr(rsp,"ERROR"))
    {
        if(strstr(rsp, "+CME ERROR:") && strstr(rsp, "12")) return WL_RET_ERR_NEEDPUK;
        return WL_RET_ERR_ERRPIN;
    }
    return WL_RET_ERR_NORSP;
}

static void sel_sim(uchar simnum)
{
	if(simnum == 0) gpio_set_pin_val(WLAN_DUAL_SIM,0);
	else gpio_set_pin_val(WLAN_DUAL_SIM,1);
}

static int s_PowerOn() 
{
    static T_SOFTTIMER tm_step_pwr;
    
    switch(PowerStep)
    {
        case 0x00: goto STEP00;
        case 0x11: goto STEP11;
        case 0x12: goto STEP12;
        case 0x21: goto STEP21;
        case 0x31: goto STEP31;
        case 0x32: goto STEP32;
        case 0x41: goto STEP41;
        case 0x42: goto STEP42;  
		case 0x51: goto STEP51;
        case 0x52: goto STEP52;  
		case 0x61: goto STEP61; 
		case 0x71: goto STEP71;
        case 0x72: goto STEP72;
        default: return 1;
    }
STEP00:    
    s_TimerSetMs(&tm_wl_pwr,0);
    s_TimerSetMs(&tm_wl_pwr_waitfor_at,0);
    /* 
    	GPE0	----> POWER
    	EINT4	<---- CTS
    	GPE2	----> ONOFF
    	GPE1	----> RTS
    */
		
    if(WlType ==WNET_GPRS_MG323)
    {
        gpio_set_pin_val(WLAN_ONOFF, 0); //ON/OFF high
        s_TimerSetMs(&tm_step_pwr,10);
        PowerStep = 0x11;
STEP11:
        if(s_TimerCheckMs(&tm_step_pwr))return 0;
        gpio_set_pin_val(WLAN_PWR, 1); //PWR ON
        s_TimerSetMs(&tm_step_pwr,300);
        PowerStep = 0x12;
STEP12:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;
        gpio_set_pin_val(WLAN_ONOFF, 1); //ON/OFF LOW
        s_TimerSetMs(&tm_wl_pwr_waitfor_at,3000);    
    }
    else if(WlType ==WNET_GPRS_G620 || WlType ==WNET_GPRS_G610 || WlType == WNET_GPRS_G510)
    {
    	gpio_set_pin_val(WLAN_ONOFF, 1); //ON/OFF low
        s_TimerSetMs(&tm_step_pwr,500);
        PowerStep = 0x21;
STEP21:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;

        gpio_set_pin_val(WLAN_PWR, 1); //power high
        s_TimerSetMs(&tm_wl_pwr_waitfor_at,4000);    	
    }
    else if(WlType == WNET_WCDMA_MU509)
    {
        gpio_set_pin_val(WLAN_ONOFF, 0); 
        DelayMs(1);
        gpio_set_pin_val(WLAN_PWR, 1);	//Power On
        s_TimerSetMs(&tm_step_pwr,3500);
        PowerStep = 0x31;    
STEP31:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;
        gpio_set_pin_val(WLAN_ONOFF, 1); 
        s_TimerSetMs(&tm_step_pwr,950);
        PowerStep = 0x32;
STEP32:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;

        gpio_set_pin_val(WLAN_ONOFF, 0);
        gpio_disable_pull(WLAN_WAKE);
        gpio_set_pin_type(WLAN_WAKE, GPIO_INPUT); //wake up
        gpio_set_pin_val(WLAN_DTR_CTS,0); //dtr on
    }	
    else if (WlType == WNET_CDMA_MC509)
    {
        gpio_set_pin_val(WLAN_ONOFF, 0); 
        DelayMs(1);    
        
        /*PWR ON*/
        gpio_set_pin_val(WLAN_PWR, 1); 
        s_TimerSetMs(&tm_step_pwr,4000);
        PowerStep = 0x41;
STEP41:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;

        gpio_set_pin_val(WLAN_ONOFF, 1); 
        s_TimerSetMs(&tm_step_pwr,900);
        PowerStep = 0x42;
STEP42:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;

        gpio_set_pin_val(WLAN_ONOFF, 0); 
        //gpio_disable_pull(WLAN_WAKE);
        //gpio_set_pin_type(WLAN_WAKE, GPIO_INPUT); //wake up
        gpio_set_pin_val(WLAN_WAKE,1);
	}
	else if (WlType == WNET_WCDMA_H330)
	{
		see_debug("h330 power on1\r\n");
		//if(s_TimerCheckMs(&tm_step_pwr))return 0;
		gpio_set_pin_val(WLAN_WAKE,0); //ON high
		s_TimerSetMs(&tm_step_pwr,500);
		PowerStep = 0x51;
STEP51: 
		if(s_TimerCheckMs(&tm_step_pwr))return 0;
		gpio_set_pin_val(WLAN_PWR,1); //power high
        s_TimerSetMs(&tm_step_pwr,500);
        PowerStep = 0x52;
STEP52:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;
        gpio_set_pin_val(WLAN_WAKE,1); //ON low
        s_TimerSetMs(&tm_wl_pwr_waitfor_at,3000);
	}
     else if(WlType == WNET_WCDMA_H350)
    {
              
	    gpio_set_pin_val(WLAN_ONOFF, 0); // HIGH 
		s_TimerSetMs(&tm_step_pwr, 500);
		PowerStep = 0x61;	 
STEP61: 	   
		if(s_TimerCheckMs(&tm_step_pwr))return 0;
		gpio_set_pin_val(WLAN_PWR, 1); 
		gpio_set_pin_val(WLAN_WAKE,0);  //wake 
    }
	 else if(WlType == WNET_WCDMA_MU709)
	{
		gpio_set_pin_val(WLAN_ONOFF, 0); 
        DelayMs(1);
        gpio_set_pin_val(WLAN_PWR, 1);	//Power On
		s_TimerSetMs(&tm_step_pwr,3500);
        PowerStep = 0x71;    
STEP71:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;
        gpio_set_pin_val(WLAN_ONOFF, 1); 
		s_TimerSetMs(&tm_step_pwr,2000);
        PowerStep = 0x72;
STEP72:        
        if(s_TimerCheckMs(&tm_step_pwr))return 0;

        gpio_set_pin_val(WLAN_ONOFF, 0);
        gpio_disable_pull(WLAN_WAKE);
        gpio_set_pin_type(WLAN_WAKE, GPIO_INPUT); //wake up
        gpio_set_pin_val(WLAN_DTR_CTS,0); //dtr on
	}
    return 1;
}

static void WlPowerOn(void)
{
	while(s_TimerCheckMs(&tm_wl_pwr));
	s_TimerSetMs(&tm_wl_pwr,0);
	/************************ 
		GPE0	----> POWER
		EINT4	<---- CTS
		GPE2	----> ONOFF
		GPE1	----> RTS
	*************************/
	PowerStep =0;
	while(s_PowerOn() == 0);
	PortReset(P_WNET);
}

int WlSleep(int onoff, char *msg)
{
	int ret=0;
	if (WlType != WNET_CDMA_MC509) return -1;
	if (onoff==1){//sleep
		if(wnet_ppp_used()) return 1;
		//see_debug("WlSleep Sleep!\r\n");
		gpio_set_pin_val(WLAN_WAKE,0);
	}
	if (onoff==2){//wakeup
		//see_debug("WlSleep Wake Up!\r\n");
		gpio_set_pin_val(WLAN_WAKE,1);	
	}
	return ret;
}


void WlPowerOff(void)
{
    T_SOFTTIMER tm;
    uint Timer_start;
    Timer_start = GetTimerCount();
    while(sWlPowering){  /*wait for first poweron */
    	if(GetTimerCount() - Timer_start > 20*1000) break; /*avoid program abortion,20 Second exit*/
    }

    if(wnet_ppp_used())
    {
        WlPppLogout();
        s_TimerSetMs(&tm, 5*1000);
        while(1== WlPppCheck())
        {
            if (!s_TimerCheckMs(&tm)) break;
        }       
    }

    switch(WlType)
    {   
    	case WNET_WCDMA_MU509:
		case WNET_WCDMA_MU709:
    		see_debug("mu709/mu509 power off\r\n");
    		gpio_set_pin_val(WLAN_ONOFF, 1); 
    		DelayMs(3000);
    		gpio_set_pin_val(WLAN_ONOFF, 0);
    	break;
    	case WNET_CDMA_MC509:
    		see_debug("mc509 power off\r\n");
    		gpio_set_pin_val(WLAN_ONOFF, 1); /*ON/OFF line*/
    		DelayMs(3500);
    		gpio_set_pin_val(WLAN_ONOFF, 0);
    	break;
        case WNET_GPRS_G620:
        case WNET_GPRS_G610:			
		case WNET_GPRS_G510:
    		see_debug("g620 power off\r\n");	

    		gpio_set_pin_val(WLAN_ONOFF, 0); //ON/OFF line high
    		DelayMs(1200);
    		gpio_set_pin_val(WLAN_ONOFF, 1); //ON/OFF  line low
    		DelayMs(3500);
    		gpio_set_pin_val(WLAN_PWR, 0); //power line low
    	break;
    	case  WNET_GPRS_MG323:
            see_debug("MG323 power off\r\n");
            gpio_set_pin_val(WLAN_ONOFF, 0);//ON/OFF line high
            DelayMs(50);
            gpio_set_pin_val(WLAN_ONOFF, 1); 
            DelayMs(2000);        
    	break;
		case WNET_WCDMA_H330:
			see_debug("h330 power off\r\n");	

			gpio_set_pin_val(WLAN_WAKE,0);//ON line high
    		gpio_set_pin_val(WLAN_ONOFF,1); //OFF line low
    		DelayMs(300);
    		gpio_set_pin_val(WLAN_ONOFF,0); //OFF  line hight
    		DelayMs(3000);
    		gpio_set_pin_val(WLAN_PWR,0); //power line low
    	break;
		case WNET_WCDMA_H350:
			see_debug("h350 power off\r\n");
			gpio_set_pin_val(WLAN_ONOFF, 1);  // LOW
    	    DelayMs(500);
    		gpio_set_pin_val(WLAN_PWR, 0);
	     break;
    }
    see_debug("power off\r\n");
    gpio_set_pin_val(WLAN_PWR, 0); //PWR OFF
    s_TimerSetMs(&tm_wl_pwr,8*1000); /*re-power must be after 8s*/
    WlEnableInit=1;
}

char *stristr(const char *String, const char *Pattern)
{
      char *pptr, *sptr, *start;
      uint  slen, plen;
      start = (char *)String;
      pptr  = (char *)Pattern;
      slen  = strlen(String);
      plen  = strlen(Pattern);
      for (;slen >= plen;start++, slen--)
      {
            /* find start of pattern in string */
            while (toupper(*start) != toupper(*Pattern))
            {
                  start++;
                  slen--;
                  /* if pattern longer than string */
                  if (slen < plen)
                        return(NULL);
            }
            sptr = start;
            pptr = (char *)Pattern;
            while (toupper(*sptr) == toupper(*pptr))
            {
                  sptr++;
                  pptr++;
                  /* if end of pattern then pattern was found */
                  if (*pptr=='\0')
                        return (start);
            }
      }
      return(NULL);
}

void GetWlFirmwareVer(uchar *s,int len)
{
    int i,j,flag;
    uchar buf[2048+4];
    memset(buf, 0, sizeof(buf));
    switch(WlType)
    {
    	case WNET_WCDMA_H330:	
        case WNET_WCDMA_H350:
        case WNET_GPRS_G610:
        case WNET_GPRS_G620:     
		case WNET_GPRS_G510:
            flag = 0;
            for(i=0,j=0;i<len;i++)
            {
                if(s[i] == '"'){
                    flag++;
                    continue;
                }    
                if(flag == 1)  buf[j++] = s[i];
                if(flag==2 ) break;
            }
            if(j> 63) j = 63;
            memcpy(s,buf,j);
            s[j] = 0;
        break; 
        case WNET_CDMA_MC323:
        case WNET_GPRS_MG323:
            for(i=0,j=0;i<len;i++){
                if(s[i]=='+' || s[i] == ' ') continue;
                if(s[i]=='\r' || s[i] == '\n') continue;
                if(s[i]=='O' && s[i+1]=='K') {
                    i++;
                    continue;
                }
                buf[j++] = s[i];
            }
            if(j> 63) j = 63;          
            memcpy(s,buf,j);
            s[j] = 0;
        break;
        case WNET_WCDMA_MU509:
        case WNET_CDMA_MC509:        
            for(i=0,j=0;i<len;i++){
                if(s[i]=='+' || s[i] == ' ') continue;
                if(s[i]=='\r' || s[i] == '\n') continue;
                if(s[i]=='O' && s[i+1]=='K') {
                    i++;
                    continue;
                }
                buf[j++] = s[i];
            }
            if(j> 63) j = 63;
            
            if(!stristr(buf,"demo")){
                for(i=0;i<j;i++)
                {
                    if( (buf[i]>='0' && buf[i]<='9') || (buf[i]=='.'))
                        continue;
                    j = i;
                    break;
                }
            }            
            memcpy(s,buf,j);
            s[j] = 0;
        break;
		case WNET_WCDMA_MU709:
			for(i=0,j=0;i<len;i++){
				if(s[i] == 'A' && s[i+1] == 'T') {
					i++;
					continue;
				}
				if(s[i]=='C' && s[i+1] == 'G' && s[i+2] == 'M' && s[i+3] == 'R'){ 
					i+=3;
					continue;
				}
                if(s[i]=='+' || s[i] == ' ') continue;
                if(s[i]=='\r' || s[i] == '\n') continue;
                if(s[i]=='O' && s[i+1] == 'K') {
                    i++;
                    continue;
                }
                buf[j++] = s[i];
            }
            if(j> 63) j = 63;
   
            if(!stristr(buf,"demo")){
                for(i=0;i<j;i++)
                {
                    if( (buf[i]>='0' && buf[i]<='9') || (buf[i]=='.'))
                        continue;
                    j = i;
                    break;
                }
            }            

            memcpy(s,buf,j);
            s[j] = 0;
		break;
        default:
        break;
    }
}

static void s_WlSetPPPType()
{	
	switch(WlType)
	{
		case WNET_GPRS_G620:
		case WNET_GPRS_G610:
		case WNET_GPRS_G510:
                        see_debug("set type:G6x0\n");
			wnet_type_set(WTYPE_GPRS,0);
		break;
		case WNET_WCDMA_H330:
		case WNET_WCDMA_H350:
			see_debug("set type:H33 H350\n");
			wnet_type_set(WTYPE_WCDMA,4);
		break;
		case WNET_WCDMA_MU509:
                      see_debug("set type:MU509/MU709\n");
			wnet_type_set(WTYPE_WCDMA,2);
		break;
		case WNET_CDMA_MC323:
		case WNET_CDMA_MC509:
                        see_debug("set type:MC509/MC323\n");
			wnet_type_set(WTYPE_CDMA,3);
		break;
		case WNET_GPRS_MG323:
                        see_debug("set type:MG323\n");
			wnet_type_set(WTYPE_GPRS,1);
		break;
		case WNET_WCDMA_MU709:
			see_debug("set type:MU709\n");
			wnet_type_set(WTYPE_WCDMA,5);
			break;
		default:
                        see_debug("set type:NONE\n");
			wnet_type_set(WTYPE_GPRS,-1);
		break;
 
	}

}

int WlGetInitStatus()
{
	return WlEnableInit;
}

static int is_mu709_up1165268()
{
    uchar rsp[2048+4];
    uchar buf[65];
    int i,j,ret;
    unsigned long tmp;
    
    for(i=0;i<3;i++){
        ret=WlSendCmd("AT+CGMR\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
        if(ret==RET_OK){
            if(strstr((char *)rsp, "OK")) 
                break;
        }
    }
    if(i>=3)
        return -1;
    
    GetWlFirmwareVer(rsp,sizeof(rsp));
    memset(buf,0,sizeof(buf));
    for(i=0,j=0;i<strlen(rsp);i++){
        if(rsp[i]!='.')buf[j++]=rsp[i];     
    }
    buf[7]=0;
    
    tmp = atol(buf);  
    if(tmp < 1165268)
        return 1;
    return 0;//>=11.652.68
    
}

int   WlInit(const  uchar *Pin)
{
    int ret, ii, status, ckcnt;
    int reset_cnt=0;
    static int flag = 0;
    char *fnd;    
    uchar rsp[2048+4],value[3]={0},buf[32] = {0};
    uint Timer_start, Timer_end;

	if(WlType == WNET_UNKNOWN) return WL_RET_ERR_NOTYPE;
    Timer_start = GetTimerCount();
    while(sWlPowering){
    	if(GetTimerCount() - Timer_start > 20*1000) break; /*avoid program abortion,20 Second exit*/
    }
    while(s_TimerCheckMs(&tm_wl_pwr_waitfor_at)); //wait for AT after power on.
    memset(apn_buff_ex, 0, sizeof(apn_buff_ex));
    memset(dialnum_buff_ex, 0, sizeof(dialnum_buff_ex));
    if(!WlEnableInit)	return WL_RET_ERR_INIT_ONCE;

    ret=WlOpenPort();
    if(ret) return WL_RET_ERR_OPENPORT;

    s_WlSetPPPType();
    reset_cnt=0;
RESET_MODULE:
    if (reset_cnt++ >1) goto ERROR_EXIT;//reset 2 times
    ckcnt=WL_POWERON_TO*2;
    //Check to make sure the module is ready for "AT" commands, and disable character echoes
    for(ii=0; ii<ckcnt; ii++)
    {
        if(WlSendCmd("ATE0\r",rsp,sizeof(rsp),500,WL_RECVBYTE_TO)==RET_OK)
        {
        	if(strstr((char *)rsp, "OK")) break;
    	}
    }
    if(ii == ckcnt) /*No Response*/
    {
    	ret=WL_RET_ERR_NOTYPE;
    	see_debug("wlinit no response\r\n");
    	goto ERROR_EXIT;
    }	

	if (WlType == WNET_GPRS_G620 || WlType == WNET_GPRS_G610 || WlType == WNET_GPRS_G510)
	{
		WlSendCmd("AT&D1;&w\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		if(!strstr(rsp,"OK")){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}
		WlSendCmd("ATV1\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		if(!strstr(rsp,"OK")){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}
	}
	
	if(WlType == WNET_WCDMA_MU709)
	{
        //"ATE0" must be resent.
        WlSendCmd("ATE0\r",rsp,sizeof(rsp),500,WL_RECVBYTE_TO);
	}
	
	memset(rsp,0,sizeof(rsp));	
	WlSendCmd("AT+CGMR?\r",rsp, sizeof(rsp), 5000, 200);
	see_debug("rsp,%s\n",rsp);
	if(strstr(rsp,"G610_V0D")){
		flag = 1;
	}
	if(WlType == WNET_GPRS_G510 || flag == 1)
		memcpy(cell_cmd,"AT+MCELL=0,25\r",strlen("AT+MCELL=0,25\r"));
	else if(WlType == WNET_GPRS_G620 || WlType == WNET_GPRS_G610)
		memcpy(cell_cmd,"AT+MCELL=0,21\r",strlen("AT+MCELL=0,21\r"));

CHK_SIM:
    ret=WlSendCmd("AT+CMEE=1\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
	
    ret=WlCheckSim();
    if(ret!=RET_OK)
    {
    	if(ret==WL_RET_ERR_NEEDPIN)
    	{
    		if(Pin==NULL)
    		{
    			WlEnableInit=1;
    			goto ERROR_EXIT;				
    		}
    		else
    		{
    			if(strlen(Pin) > PIN_MAXLEN) 
    			{
    			    WlEnableInit=1;
    			    return WL_RET_ERR_PARAMER;
    			}
    		}
    		/*Verify pin*/
    		ret=WlSimPin(Pin);
    		if(ret!=RET_OK)
    		{
                if (ret == WL_RET_ERR_ERRPIN) WlEnableInit=1;
    			goto ERROR_EXIT;
    		}
    	}
    	else 
    	{
    		WlEnableInit=1;
    		goto ERROR_EXIT;
	}
	}
	if(WlType == WNET_WCDMA_MU709)
	{
        ret = is_mu709_up1165268();
        if(ret == 0 || ret == -1){
            //To improve the response speed for 11.652.68.00.00,Other versions return an error.
            WlSendCmd("AT^custfeature=6,1\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
            if((ret==0)&&(!strstr(rsp,"OK"))){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}

            //firmware ver>=11.652.68.00.00,valid after reset,Other versions return "error".
            //=0,Closing speech function
            WlSendCmd("at^callsrv=0\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
            if((ret==0)&&(!strstr(rsp,"OK"))){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}
        }
		//The presentation of the unsolicited indications in Table 10-1(AT Command Interface Specification) is disabled	
		WlSendCmd("AT^CURC=0\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		if(!strstr(rsp,"OK")){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}
        //Data service prefer
		WlSendCmd("AT^DVCFG=1\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		if(!strstr(rsp,"OK")){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}
        //Configure the Mode of STK,0:Disable STK
		WlSendCmd("AT^STSF=0\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		if(!strstr(rsp,"OK")){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}
        //Automatic,Automatic,ANY,CAN ROAMING,PS_ONLY
		WlSendCmd("AT^SYSCFG=2,0,3FFFFFFF,1,2\r",rsp,sizeof(rsp),10000,100);
		
	}
	if(WlType == WNET_WCDMA_H350 || WlType == WNET_WCDMA_H330)
	{
		memcpy(cell_cmd,"AT+MUCELL=0,21\r",strlen("AT+MUCELL=0,21\r"));
		memcpy(cell_rsp,"+MUCELL",strlen("+MUCELL"));
		WlSendCmd("AT+GTSET=\"LCPECHO\",3,6\r\n",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
    	if(!strstr(rsp,"OK")){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}
		WlSendCmd("AT&D1;&w\r\n",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
    	if(!strstr(rsp,"OK")){ret=WL_RET_ERR_RSPERR; goto ERROR_EXIT;}
		if(LBSTaskGet() == 1){
			WlSendCmd("AT+GTNWSCAN=2,3,3\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		}
	}
	if(WlType==WNET_CDMA_MC509)
	{
		ret = SysParaRead(SET_NETWORK_MODE, value); 
		if(ret == 0)
		{
		    sprintf(buf,"AT^PREFMODE=%c\r",value[0]);
		    ret=WlSendCmd(buf,rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);		
		}
		else
		{
			sprintf(buf,"AT^PREFMODE=2\r");
		    ret=WlSendCmd(buf,rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		}
		ret = SysParaRead(SET_RELAY_MODE, value); 
		if(ret == 0)
		{
			memset(buf,0,sizeof(buf));
			sprintf(buf,"AT+CRM=%c\r",value[0]);
			ret=WlSendCmd(buf,rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		}
		else
		{
			ret=WlSendCmd("AT+CRM=1\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
		}
		ret=WlSendCmd("AT^CURC=0\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO); //disable auto report from MTD

		Timer_start = GetTimerCount();
		Timer_end   = Timer_start;
		while(Timer_end - Timer_start < RESET_CDMA_TO)
		{
			WlSendCmd("AT^SYSINFO\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);	
			fnd = strstr(rsp,"^SYSINFO:");
			if(fnd)
			{
				fnd += 9;         //strlen of  "^SYSINFO:"
				see_debug("fnd = %s\n",fnd);
				while(*fnd == ' ')
					fnd++;
				status = atoi(fnd);
				see_debug("\r\n^SYSINFO status:%d\r\n",status);
				if(status == CDMA_SYS_VALID)
				{
					see_debug("info:Check System Service OK!!\r\n");
					DelayMs(20);                
					PortReset(P_WNET);
					break;
				}
			}
			DelayMs(2000);                //if <2000,the success rate becomes low
		}
		Timer_end = GetTimerCount();
    	if (Timer_end -Timer_start>=RESET_CDMA_TO) {
    		DelayMs(30);
    		PortReset(P_WNET);
    		WlSendCmd("AT^RESET=0\r", 0, 0, 0, 0);
    		DelayMs(6000);	
    		ret = WL_RET_ERR_NOREG;
    		goto RESET_MODULE;
    	}
        /*Data service prefer*/
        WlSendCmd("AT^DVCFG=1\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);	
		if(LBSTaskGet() == 1){
	 		WlSendCmd("AT+CIMI\r",mc509_rsp, sizeof(mc509_rsp), WL_SENDCMD_TO, WL_RECVBYTE_TO);
			WlSendCmd("AT^BSINFO\r",wnet_cell_buf, sizeof(wnet_cell_buf), WL_SENDCMD_TO, WL_RECVBYTE_TO);
			WlSendCmd("AT^SIQ\r",wnet_ncell_buf, sizeof(wnet_ncell_buf), WL_SENDCMD_TO, WL_RECVBYTE_TO);
		}
    }
    
    if (is_gprs_module() || is_wcdma_module()) {/* check SIM registe status */
    	Timer_start = GetTimerCount();
    	Timer_end=Timer_start;
    	while (Timer_end -Timer_start <RESET_GPRS_TO) {
    		WlSendCmd("AT+CGREG?\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
    		fnd = strstr(rsp, "+CGREG:");
    		if (fnd) fnd = strstr(fnd, ","); 
    		if (fnd) {
    			fnd++;
    			if (*fnd == ' ') fnd++;
    			status = atoi(fnd);
    			see_debug("\r\n+CGREG status:%d\r\n", status);	
    			if (status==1||status==5) //native or roaming
    			{
    				see_debug("info:Check SIM CGREG OK!!!\r\n");
    				DelayMs(30);
    				PortReset(P_WNET);
    				break;
    			}
    			if (status==3) {
    				see_debug("err:Check SIM CGREG Failed!!!\r\n");
    				ret = WL_RET_ERR_NOREG;
    				WlSendCmd("at+cfun=1,1\r", 0, 0, 0, 0);
    				DelayMs(6000);	
    				goto RESET_MODULE;
    			}
    		}
    		DelayMs(2000);
    		Timer_end = GetTimerCount();
    	}
    	if (Timer_end -Timer_start>=RESET_GPRS_TO) {
    		DelayMs(30);
    		PortReset(P_WNET);
    		WlSendCmd("at+cfun=1,1\r", 0, 0, 0, 0);
    		DelayMs(6000);	
    		ret = WL_RET_ERR_NOREG;
    		goto RESET_MODULE;
    	}
		if(LBSTaskGet() == 1){
			if(is_wcdma_module() && WlType != WNET_WCDMA_MU709){
				//wait for scanning cell info.
				Timer_start = GetTimerCount();
    			Timer_end=Timer_start;
    			while (Timer_end - Timer_start < CELL_WCDMA_TO) {
					WlSendCmd(cell_cmd,wnet_cell_buf, sizeof(wnet_cell_buf), WL_SENDCMD_TO, WL_RECVBYTE_TO);
					if(strstr(wnet_cell_buf,"ERROR") != NULL) {
						see_debug("Scanning Cell Info!\n");
						DelayMs(500);
					}else if(strstr(wnet_cell_buf,"OK") != NULL) break;
						Timer_end = GetTimerCount();
				}
			}else
				WlSendCmd(cell_cmd,wnet_cell_buf, sizeof(wnet_cell_buf), WL_SENDCMD_TO, WL_RECVBYTE_TO);
		}

    }
	
    WlClosePort();    
    WlEnableInit=0;
    return RET_OK;
ERROR_EXIT:
    WlClosePort();   
    return ret;
}

static uchar s_WlSignalLevel(uchar val)
{
	if((val>=32)||(val==0))		  return 5;
	else if(val<3) 				  return 4;
	else if(val<8) 				  return 3;
	else if(val<13)				  return 2;
	else if(val<19)				  return 1;
	else if(val<32)				  return 0;
	return 0; 
}

int WlGetSignal(uchar * pSignalLevel)
{
    uchar rsp[512],*ptr;
    int temp, opened,ret,i;

    if(pSignalLevel == NULL) return WL_RET_ERR_PARAMER; 
    *pSignalLevel=5;
    if(wnet_ppp_used())
    {
    	ret=wnet_get_signal(rsp);
    	if(ret==RET_OK) *pSignalLevel=s_WlSignalLevel(rsp[0]);
    	return ret;
    }

    opened=0;
    if(!WlPortOpened)
    {
    	ret=WlOpenPort();
    	opened=1;
    	if(ret!=RET_OK) return ret;
    }

    for(i=0;i<5;i++)
    {
        if((WlType == WNET_CDMA_MC323)||(WlType == WNET_CDMA_MC509))
            WlSendCmd("AT+CSQ?\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
        else
            WlSendCmd("AT+CSQ\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
        
        ret=WL_RET_ERR_RSPERR;
        ptr=strstr(rsp,"+CSQ:");
        if(ptr) 
        {
            ret=RET_OK;
            temp = atoi(ptr+5);
            if((temp==0)||(temp==99)) continue;
            *pSignalLevel=s_WlSignalLevel((uchar)temp);
            break;
        }
    }
    if(opened) WlClosePort();
    return ret;
}

int WlOpenPort(void)
{
    if(wnet_ppp_used()) return WL_RET_ERR_PORTINUSE;
    if(PortOpen(P_WNET,"115200,8,n,1")) return WL_RET_ERR_OPENPORT;
    WlPortOpened =1;
    return RET_OK;
}

int WlClosePort(void)
{
    if(!WlPortOpened) return WL_RET_ERR_PORTNOOPEN;
    if(wnet_ppp_used()) return WL_RET_ERR_PORTINUSE;
    if(PortClose(P_WNET)) return WL_RET_ERR_PORTCLOSE;
    WlPortOpened =0;
    return RET_OK;
}
//return: 1->succ, 0->fail
int s_checkimei(uchar *szbuf, uchar *imei_out, int flag)
{
	int i=0,j=0;
	char *p1;

	//^DSN:a00000591d648a,80c85d49
	if (flag==1 && WlType==WNET_CDMA_MC509) {
		p1=strstr((char*)szbuf, "^DSN:");
		if (p1==NULL) {imei_out[0]=0; return 0;}
		p1=p1+5;
		j=0;
		while(*p1!='\0') {
			if(*p1 == ',' || j >= 15) { imei_out[j]=0; break; }
			imei_out[j++]=*p1++; 
		}		
		if (j==14) return 1;
		imei_out[0]=0;
		return 0;
	}
	
	while(szbuf[i]) {
       	if(isdigit(szbuf[i])) imei_out[j++]=szbuf[i];
		if(j==15) break; /*IMEI is 15 Bytes*/
		i++;
	}
	imei_out[j]=0;
	return ((j==15)?1:0); //succ->return 1; fail->return 0;
}

static void s_WlGetInfo(uchar *softver,uchar *imei)
{
    int ret,i,ckcnt;
    uchar rsp[2048+4];
    static int GetFlag =0;
    uint Timer_start;

    if(GetFlag) return ;

    ret=WlOpenPort();
    if(ret) return ;
    Timer_start = GetTimerCount();
    while(sWlPowering){
    	if(GetTimerCount() - Timer_start > 20*1000) break; /*avoid program abortion,20 Second exit*/
    }

    while(s_TimerCheckMs(&tm_wl_pwr_waitfor_at)); //wait for AT after power on.
    ckcnt=WL_POWERON_TO*2;
    //Check to make sure the module is ready for "AT" commands, and disable character echoes
    for(i=0; i<ckcnt; i++)
    {
        if((ret= WlSendCmd("ATE0\r",rsp,sizeof(rsp),500,WL_RECVBYTE_TO))==RET_OK)
        {
        	if(strstr((char *)rsp, "OK")) break;
    	}
    }

    if(i == ckcnt) /*no answer*/
    {
    	ret=WL_RET_ERR_NOTYPE;
    	see_debug("wlinit no response\r\n");
    	goto ERROR_EXIT;
    }	
    
    if(WlType == WNET_WCDMA_MU709)
    {   
        WlSendCmd("ATE0\r",rsp,sizeof(rsp),500,WL_RECVBYTE_TO);
        ret = is_mu709_up1165268();
        if(ret == 0 || ret == -1){
            //To improve the response speed for 11.652.68.00.00,Other versions return an error.
            WlSendCmd("AT^custfeature=6,1\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
            if(!strstr(rsp,"OK")){
                 WlSendCmd("AT^custfeature=6,1\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);   
            }
        }
    }

	for(i=0; i<4; i++) //check imei
	{
		memset(rsp, 0, sizeof(rsp));
	    switch(WlType)
	    {
	        case WNET_CDMA_MC323:
				WlSendCmd("AT+CIMI\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
				break;
	        case WNET_CDMA_MC509:
				if (i<2){
					WlSendCmd("AT^DSN\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
					mc509_flag=1;
				}
				else{
					WlSendCmd("AT+CIMI\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
					mc509_flag=0;
				}
				break;
	        default :
	            WlSendCmd("AT+CGSN\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
	       		break;
	    }        
		if (s_checkimei(rsp, imei, mc509_flag)) {
			break; /*IMEI is 15 Bytes*/
		}
		DelayMs(2000);
	}
    /*get  the software version*/
    WlSendCmd("AT+CGMR\r",rsp,sizeof(rsp),WL_SENDCMD_TO,WL_RECVBYTE_TO);
    GetWlFirmwareVer(rsp,sizeof(rsp));
    strcpy(softver, rsp);
    GetFlag = 1;
ERROR_EXIT:
    WlClosePort();   
}

int s_CdmaIsMeid()
{
	if(WlType==WNET_CDMA_MC509)
		return mc509_flag;
	else
		return 0;
}

int WlGetModuleInfo(uchar *modinfo,uchar *imei)
{
    int len;
    uchar context[64],softver[64];
    static uchar sWlImei[16],sWlModInfo[128],flag=0;
    if(flag){
        strcpy(modinfo,sWlModInfo);
        strcpy(imei,sWlImei);
        flag = 1;
        return 0;
    }
    memset(sWlModInfo,0,sizeof(sWlModInfo));
    memset(sWlImei,0,sizeof(sWlImei));

    len = ReadCfgInfo("GPRS",context);
    if(len <0)len = ReadCfgInfo("CDMA",context);
    if(len <0)len = ReadCfgInfo("WCDMA",context);
    if(len>0) memcpy(sWlModInfo,context,len);
    softver[0] = 0;
    s_WlGetInfo(softver,sWlImei);
    if(softver[0]) {
    	strcat(sWlModInfo, " ");
    	strcat(sWlModInfo, softver);

        strcpy(modinfo,sWlModInfo);
        strcpy(imei,sWlImei);       
    	flag = 1;
    }
    return 0;
}

static void s_WlPowerOnHandler(void)
{
    static T_SOFTTIMER tm_step_pwr;
    static uint timer10ms=0;
    if(timer10ms ==s_Get10MsTimerCount()) return;
    timer10ms = s_Get10MsTimerCount();
    if(s_PowerOn()==0) return;
    sWlPowering = 0;
    s_SetSoftInterrupt(IRQ_OTIMER3,NULL);	
}

T_WlDrvConfig SxxxWl_Ver = {
	.onoff_port = GPIOE,
	.onoff_pin = 2,
	.wake_port = GPIOE,
	.wake_pin = 1,
	.pwr_port = GPIOE,
	.pwr_pin = 0,
	.dtr_cts_port = GPIOB,
	.dtr_cts_pin = 4,
	.dual_sim_port = GPIOD,
	.dual_sim_pin = 12,
};

T_WlDrvConfig S900Wl_Ver2 = {
	.onoff_port = GPIOE,
	.onoff_pin = 2,
	.wake_port = GPIOD,
	.wake_pin = 13,
	.pwr_port = GPIOC,
	.pwr_pin = 8,
	.dtr_cts_port = GPIOB,
	.dtr_cts_pin = 4,
	.dual_sim_port = GPIOD,
	.dual_sim_pin = 12,
};

T_WlDrvConfig S800Wl_Ver2 = {
	.onoff_port = GPIOE,
	.onoff_pin = 2,
	.wake_port = GPIOD,
	.wake_pin = 13,
	.pwr_port = GPIOC,
	.pwr_pin = 8,
	.dtr_cts_port = GPIOB,
	.dtr_cts_pin = 4,
	.dual_sim_port = GPIOD,
	.dual_sim_pin = 12,
};

T_WlDrvConfig D200Wl_Ver = {
	.onoff_port = GPIOE,
	.onoff_pin = 1,
	.wake_port = GPIOB,
	.wake_pin = 10,
	.pwr_port = GPIOE,
	.pwr_pin = 0,
	.dtr_cts_port = GPIOB,
	.dtr_cts_pin = 10,
	.dual_sim_port = GPIOD,
	.dual_sim_pin = 12,
};

void s_WlInit(void)
{
    uchar buff[100];
    WlType=GetWlType();
    if(WlType == WNET_UNKNOWN) return;
    WlPortOpened=0;
    WlEnableInit=1;
    sWlPowering = 1;

	if (get_machine_type() == S900 && (GetHWBranch()!=S900HW_Vxx))
	{
		ptWlDrvConfig = &S900Wl_Ver2; 
	}
	else if (get_machine_type() == S800 && ((GetHWBranch()==S800HW_V2x)||(GetHWBranch()==S800HW_V4x)))//S800 developement hardware
	{
		ptWlDrvConfig = &S800Wl_Ver2; 
	}
	else if (get_machine_type() == D210)	//D210's hw same with S900HW_V2x
	{
		ptWlDrvConfig = &S900Wl_Ver2; 
	}
	else if (get_machine_type() == D200)
	{
		ptWlDrvConfig = &D200Wl_Ver;
	}
	else
	{
		ptWlDrvConfig = &SxxxWl_Ver; //old hardware
	}
	
    gpio_set_pin_type(WLAN_PWR, GPIO_OUTPUT);	//PWR	
    gpio_set_pin_type(WLAN_WAKE, GPIO_OUTPUT);	//Wake	
    gpio_set_pin_type(WLAN_ONOFF, GPIO_OUTPUT);	//ON_OFF	
    gpio_disable_pull(WLAN_DTR_CTS);
    switch(WlType)
    {
    	case WNET_GPRS_MG323:
    		gpio_set_pin_type(WLAN_DTR_CTS,GPIO_INPUT); //CTS	
    	break;
    	case  WNET_WCDMA_MU509: 
		case  WNET_WCDMA_MU709:
    	case  WNET_CDMA_MC509:
    	case  WNET_GPRS_G610: 
		case  WNET_WCDMA_H330:
		case  WNET_WCDMA_H350:
		case  WNET_GPRS_G510:	//Joshua _a
    		gpio_set_pin_type(WLAN_DTR_CTS,GPIO_OUTPUT);	//DTR
    		gpio_set_pin_val(WLAN_DTR_CTS,0);	//DTR
    	break;
    }
    if(is_double_sim()) 
    {
    	sim_num = 0;
    	gpio_set_pin_type(WLAN_DUAL_SIM,GPIO_OUTPUT); /*config dual sim IO port to output*/	
    	if(SysParaRead(SET_SIM_CHAN_PARA,buff)==0) 
    		sim_num = buff[0] - 0x30;
    	sel_sim(sim_num);
    }
    PowerStep = 0;
    s_SetSoftInterrupt(IRQ_OTIMER3,s_WlPowerOnHandler);
	#if 0 //deleted by shics, save memory
	if (WlType == WNET_CDMA_MC509)		
			OsCreate((OS_TASK_ENTRY)TaskRelinkMgr,TASK_PRIOR_MC509_LINK,0x3000);
		#endif
}

void WlSetCtrl(uchar pin, uchar val)
{
    if(WlType == WNET_UNKNOWN) return;
    switch(pin)
    {
    	case WL_PIN_DTR: 
			if (WlType == WNET_CDMA_MC323 || WlType == WNET_CDMA_MC509) 
			{ //DTR ctrl just for CDMA module
				if(val)
				{
					gpio_set_pin_type(WLAN_DTR_CTS,GPIO_OUTPUT); 
					gpio_set_pin_val(WLAN_DTR_CTS,0);  //dtr on
				}
				else 
				{
					gpio_set_pin_type(WLAN_DTR_CTS,GPIO_INPUT);  //dtr off, when set to high level, must set the port to input, not set to high directly on SXXX .
				}
			}
    	break;				
    }
}

int WlSelSim(uchar simnum)
{
	uchar buff[2];
	int ret,opened;
	
	if(simnum > 1) return WL_RET_ERR_PARAMER;
	if(sim_num==simnum) return sim_ready;
	if (is_double_sim() ==0 ) return WL_RET_ERR_NOTYPE;

	if(wnet_ppp_used()) return WL_RET_ERR_BUSY;
	WlPowerOff();
	buff[0] = simnum+0x30;
	buff[1] = 0x00;
	SysParaWrite(SET_SIM_CHAN_PARA, buff);
	sel_sim(simnum);

	sim_num=simnum;
	opened=WlPortOpened;
	WlPowerOn();
	ret=WlInit(NULL);
	if(opened) WlOpenPort();
	return ret;
}
//op: 1-write; 0-read;
int WlGprsChapPara(int *val, int op)
{
	char rsp[2048], buf[100];
	int rval=0, ret=0;
	uint Timer_start;
	
	if (op!=0 && op!=1) return -1;

    Timer_start = GetTimerCount();
    while(sWlPowering){
    	if(GetTimerCount()-Timer_start>5*1000) return -4;
    }
	
    WlOpenPort();
	
	//read
	memset(rsp, 0, sizeof(rsp));
	WlSendCmd("AT+GTSET?\r",rsp,sizeof(rsp), 3000, 1000);
	if(strstr(rsp, "CHALLENGE:")==NULL) { ret=-2; goto END;} //gprs not support AT command

	if(strstr(rsp, "CHALLENGE: 8") || strstr(rsp, "CHALLENGE:8")) rval=8;
	if(strstr(rsp, "CHALLENGE: 16") || strstr(rsp, "CHALLENGE:16")) rval=16;

	if (op==0) { *val=rval; ret=0; goto END; }

	if (*val!=8 && *val!=16) { ret=-1; goto END;}
	if (rval==*val) {ret=0; goto END; }//the same value.	
	
	//write
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "AT+GTSET=\"challenge\",%d\r", *val); 
	WlSendCmd(buf,rsp,sizeof(rsp), 3000, 1000);

	//read
	memset(rsp, 0, sizeof(rsp));
	WlSendCmd("AT+GTSET?\r",rsp,sizeof(rsp), 3000, 1000);
	if(strstr(rsp, "CHALLENGE:")==NULL)  { ret=-2; goto END;} //gprs not support AT command

	if(strstr(rsp, "CHALLENGE: 8") || strstr(rsp, "CHALLENGE:8")) rval=8;
	if(strstr(rsp, "CHALLENGE: 16") || strstr(rsp, "CHALLENGE:16")) rval=16;

	if (rval!=*val)  {ret=-3; goto END;}
	
END:
	WlClosePort();
	return ret;
}


void WlModelComUpdate(void)
{
    uchar ch;
    switch(WlType)
    {
    	case WNET_GPRS_MG323:
    	case WNET_GPRS_G620:
    	case WNET_GPRS_G610:
    	case WNET_GPRS_G510:
		case WNET_WCDMA_H330:
		case WNET_WCDMA_H350:
        case WNET_CDMA_MC509:
        case WNET_WCDMA_MU509:
        case WNET_WCDMA_MU709:
    	break;
    	default:
    	ScrPrint(0,7,0,"Not Support Update");
    	getkey();
    	return;
    }	
    ScrPrint(0,7,0,"Waiting for update...");

    WlPowerOff();

    s_TimerSetMs(&tm_wl_pwr,2*1000); /*modify the interval of poweroff to poweron*/
    WlPowerOn();
    WlOpenPort();
    PortOpen(0, "115200,8,n,1");
    while(1)
    {			
    	if (PortRx(0, &ch)==0) PortTx(P_WNET, ch);
    	if (PortRx(P_WNET, &ch)==0) PortTx(0, ch);
    	if (!kbhit() && (getkey()==KEYCANCEL))break;
    }
    PortClose(0);
}

int WlAutoStart(uchar *pin, uchar *APNIn, uchar *UidIn, 
         uchar *PwdIn, long Auth, int TimeOut, int AliveInterval)
{
    return 0;
}

int WlAutoCheck(void) 
{
    return 0;
}

void WlSwitchPower(uchar sw) /*API Function*/
{/*
	if (sw)
		WlPowerOn();
	else
		WlPowerOff();
		*/
}

void WlTcpRetryNum(int value)
{
	if(value >= 0 && value <10)	connect_num = value;
}
void WlSetTcpDetect(int value)
{
	if(value ==0  || value ==1)	detect_tcp_peer_opened = value;
}

void wl_ppp_login_ex_param(const char *DialNum, const char *apn,
			char *Uid, char *Pwd,  long Auth, int timeout,
			int  AliveInterval)
{
    memset(&s_WlParam, 0, sizeof(s_WlParam));
    				
    if(apn == NULL) //APN is NULL
    {
    	strncpy(s_WlParam.DialNum,dialnum_buff_ex, sizeof(s_WlParam.DialNum)-1);
    	strncpy(s_WlParam.Apn, apn_buff_ex, sizeof(s_WlParam.Apn)-1);	
    }
    else
    {
    	if(DialNum == NULL)
    	{
    		strncpy(s_WlParam.DialNum,"ATD*99***1#",sizeof(s_WlParam.DialNum)-1);
    	}
    	else
    	{
    		strncpy(s_WlParam.DialNum, DialNum,sizeof(s_WlParam.DialNum)-1);
    	}
    	
    	strncpy(s_WlParam.Apn, apn,sizeof(s_WlParam.Apn)-1);		
    }

    memset(dialnum_buff_ex, 0,sizeof(dialnum_buff_ex));
    memset(apn_buff_ex, 0,sizeof(apn_buff_ex));
    if(Uid != NULL)
    {
    	strncpy(s_WlParam.Uid, Uid, sizeof(s_WlParam.Uid) -1);			
    }
    if(NULL != Pwd)
    {
    	strncpy(s_WlParam.Pwd, Pwd, sizeof(s_WlParam.Pwd) -1);
    }
    s_WlParam.Auth = Auth;
    s_WlParam.AliveInterval = AliveInterval;		
}

/*
function: check the current route, if it is ppp, return 1, else return 0.
param in: socket,socket index
		addr: peer socket addr
		addrlen:peer socket addr length
param out: no
*/
int is_wl_route(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
	if (11== router_type(socket, addr, addrlen))// wireless
		return 1;
	return 0;
}

static int wl_logout()
{
    uint timer_start;
    WlPppLogout();
    timer_start = GetTimerCount();
    while( 1==WlPppCheck())
    {
    	if(GetTimerCount() - timer_start > 180*1000)
    	{ 
    		return NET_ERR_PPP;//ppp logout error
    	}
        DelayMs(200);
    }
    return RET_OK;
}

/*
function: for wireless tcp connect, if failed, retry connect_num times.
param in: socket,socket index
		addr: peer socket addr
		addrlen:peer socket addr length
param out: no
*/
int wl_net_connect(int socket, struct net_sockaddr *addr, socklen_t addrlen)
{
    int i, ret, ret1,socktmp;	

    for(i=0; i<2; i++)
    {	    
        ret = NetConnect(socket, addr, addrlen);
        if( 0==ret)  break;

        if(socket != 0  || SockUsed() > 1 || !is_tcp_socket(socket)) goto NORMAL_OUT;
        NetCloseSocket(socket);
        socket = NetSocket(1, 1, 0);//create TCP type socket

        if(socket != 0)
        {
            NetCloseSocket(socket); 
            goto ERR_OUT;
        }   
        Netioctl(socket, 3, 8000); //set connet timeout to 8s
    }

    if((ret != NET_ERR_TIMEOUT) && (ret != NET_ERR_PPP)) 
    {			
        Netioctl(socket, 3, 55000); //set send and receive timeout to 55s
        goto NORMAL_OUT;
    }


    NetCloseSocket(socket);
    ret1 = wl_logout();
    if( RET_OK != ret1)
    {
        connect_num= WL_TCP_CONN_DEFAULT;
        return NET_ERR_PPP;
    }

    for(i=0;i<connect_num;i++)
    {
        ret = WlPppLoginEx(s_WlParam.DialNum, s_WlParam.Apn, s_WlParam.Uid, s_WlParam.Pwd, s_WlParam.Auth, 20*1000, s_WlParam.AliveInterval);								
        if(ret < 0)	goto ERR_OUT;	

        socket = NetSocket(1, 1, 0);//create TCP type socket

        if(socket != 0)
        { 
            NetCloseSocket(socket); 
            goto ERR_OUT;
        }


        Netioctl(socket, 3, 8000); //set connet timeout to 8s
        tcp_port_reinit();
        ret = NetConnect(socket, addr, addrlen);
        if((ret != NET_ERR_TIMEOUT) && (ret != NET_ERR_PPP))
        {			
            Netioctl(socket, 3, 55000); //set send and receive timeout to 55s
            goto NORMAL_OUT;
        }
        else //re-connect
        {
            NetCloseSocket(socket);
            ret1 = wl_logout();
            if( RET_OK != ret1)
            {
                connect_num= WL_TCP_CONN_DEFAULT;                                 
                return NET_ERR_PPP;
            }
        }
    }

    if(connect_num == i) //reach the max retry,return;
    {
        connect_num = WL_TCP_CONN_DEFAULT;
        return NET_ERR_PPP;
    }

ERR_OUT:	
    connect_num = WL_TCP_CONN_DEFAULT;	
    wl_logout();	
    return NET_ERR_PPP;

NORMAL_OUT:
    connect_num = WL_TCP_CONN_DEFAULT;
    if(0 == ret)//connect successfully, and then send keepalive to detect the connection
    {    
        if(!is_tcp_socket(socket)) return ret;
        if(detect_tcp_peer_opened && 0== tcp_peer_opened(socket))
        { 
            NetCloseSocket(socket);
            socktmp = NetSocket(1, 1, 0);//create TCP type socket
            if(socket != socktmp) {
                NetCloseSocket(socktmp);
                return NET_ERR_CONN;
            }
            Netioctl(socket, 3, 8000); //set connet timeout to 8s
            ret = NetConnect(socket, addr, addrlen);
            Netioctl(socket, 3, 55000); //set send and receive timeout to 55s
            return ret;
        }
    }
    return ret;
}		

int is_gprs_module()
{
    if (WlType==WNET_GPRS_MG323 || WlType==WNET_GPRS_G620 ||
		WlType==WNET_GPRS_G610 || WlType==WNET_GPRS_G510) return 1;
    return 0;
}
int is_cdma_module()
{
    if (WlType==WNET_CDMA_MC509 || WlType==WNET_CDMA_MC323) return 1;
    return 0;
}
int is_wcdma_module()
{
    if (WlType==WNET_WCDMA_MU509 || WlType == WNET_WCDMA_H330 || WlType == WNET_WCDMA_H350||WlType==WNET_WCDMA_MU709 ) return 1;
    return 0;
}



int GetLbsFlag()
{
	return task_lbs_flag;
}

int TaskRelinkMgr()
{
	OsSleep(500*60*2);//2 min
	while (1) {
		OsSleep(500*60*5);//5 min
		if (LBSTaskGet() == 1) PppRetryLinking();
	}
	return 0;
}

/* flag: 0(start), 1(stop) */
void LBSTaskSet(int flag)
{
	task_lbs_flag = flag;		
}

int LBSTaskGet(void)
{
	return task_lbs_flag;
}

int Mc509ReLoginEx()
{
	int ret=0;
	int err=0;
	int dail_cnt=0;
	int time_cnt=0;
	int login_cnt=0;
	int logout_cnt=0;
	
	while (1) {	
		ret = WlPppLoginEx("ATD#777", s_WlParam.Apn, s_WlParam.Uid, s_WlParam.Pwd, s_WlParam.Auth, 0, s_WlParam.AliveInterval);
		while((ret=WlPppCheck())==1){ 
			OsSleep(500*1); 
			time_cnt++;
			if (time_cnt>=20){//20 sec timeout!
				time_cnt=0;
				err=-1;
				/* do logout, for next login */
				WlPppLogout();
				while(WlPppCheck()==1) {
					logout_cnt++;
					OsSleep(500*1);
					if (logout_cnt>10) { //10 sec for logout
						logout_cnt=0; 
						break;
					}
				}
				/* logout ok, so break */
				break;
			}
		}
		/* step one: do timeout event! */
		if (err==-1) {//timeout
			err=0;
			login_cnt++;
			if (login_cnt>=2) break;//retry twice, error return.
		}
		/* step two: success return! */
		if (ret==0) break;//login OK!
		/* step three: ERROR return! (except -214 dial error) */
		if (ret<0 && ret!=-214) break;
		/* step four: do Dial "ATD#777" failed error! */
		if (ret==-214) {//ATD#777 reutrn NO CARRIER failed. (return WL_RET_ERR_LINEOFF)			
			dail_cnt++; 
			OsSleep(dail_cnt*dail_cnt*500*1);
			if (dail_cnt>=3) break;//retry dial triple failed, so break;
		}
	}
	
	return ret;
}


