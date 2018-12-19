/*******************************************************
* 文件名称：
*     main.c
* 功能描述：
*     Monitor主入口函数
*********************************************************/
//#define TMP_VER "T"
#define TMP_VER ""
#include <string.h>
#include "version.h"
#include "Posapi.h"
#include "base.h"
#include "../comm/comm.h"
#include "../download/localdownload.h"
#include "..\lcd\lcdapi.h"
#include "..\TouchScreen\tsc2046.h"
#include "..\font\font.h"
#include "../file/filedef.h"
//#include "../btstack/src/btstack_debug.h"
//#include "../btstack/test/bt_init.h"

// 定义Monitor第一次运行标志
volatile uchar k_MonitorStatus;

#define SEC_LEVEL 0x07  /*value range:0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f,0xff*/
#define DMU_LAST_RST_OPEN_WDT	(1<<11)
#define DMU_LAST_RST_SPL_WDT	(1<<8)

APPINFO MonitorInfo={
	"Sxx-MONITOR",
	"SecLevel",
	MONI_VERSION,//"1.13",
	"PAX",
	"Monitor",
	"",
	SEC_LEVEL,
	0x20000000,
	0xFF,
	""
};

static uchar      k_MajorVer, k_MinorVer;//, k_DebugVer;
static uchar      k_UseAreaCode = CHINA_VER;
static uchar      k_BiosVer = 0;
static uchar	  k_SafeLevel = 'D';
static uchar      k_TermName[16];
static uchar      k_FirstRunStatus = 0;
static POS_AUTH_INFO k_AuthInfo;
uchar       k_SdramSize = 64;//64M
uint       k_FlashSize = 128;//128M
uchar k_Charging = 0;

uchar               bIsChnFont    = 0;
volatile uchar		k_SafeConfig[8];
OS_TASK *g_taskpoweroff=NULL;

extern uchar bIsChnFont;

uchar AsciiToHex(uchar *ucBuffer);
void ShowVer();
extern unsigned char SystemInit(void);
extern void FormatData(void);
extern char net_version_get();
extern void PaxStart(void);
int main_code(void);
void Soft_Reset(void);
void bcm5892_watchdog_rst(void);

//---------获取终端硬件配置信息
void SendCommand(unsigned char *pszCommString)
{
	unsigned char ucRet;
	
	while( *pszCommString )
	{
		ucRet=PortSend(P_WIFI, *pszCommString++);
		if(ucRet!=0) return;
	}
}

uchar is_bt_module(void)
{
    char context[64];
	static int btType = -1;
	if(btType >= 0) return btType;
	
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("BLUE_TOOTH",context) < 0) btType = 0;
    else if(strstr(context,"01")) btType = 1;
    else btType = 0;

    return btType;	
}

uchar is_wifi_module()
{
    char context[64];
	static int wifiType = -1;
    if(wifiType >= 0) return wifiType;
	
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("WIFI_NET",context)<0) wifiType = 0;
	else if (strstr(context, "RS9110-N-11-24")) wifiType = 1;
	else if (strstr(context, "03")) wifiType = 3;
	else wifiType = 0;

    return wifiType;
}

uchar CheckICRVer()
{
	//0x01 IC芯片型号:NCN6001;生产厂商:ON Semiconductor;级别:符合EMV2000,ISO7816标准
	 return 0x01;       //  IC读卡器信息
}


int CheckCPUfrequency()
{
	return 266;       //  CPU频率 单位:MHz
}

uint CheckFlashSize()
{
	return k_FlashSize;       //  Flash大小 单位:Muchar
}

uchar  CheckSDRAMSize()
{
	return k_SdramSize;       //  SDRAM大小 单位:Muchar
}

uchar CheckNANDFSize()
{
	return 128;
}

uchar CheckSupportInfo()
{
	/*
	bit0:终端支持双倍长密钥算法
	bit1:软件未被篡改,不储存磁道及PIN信息
	bit2: 终端支持外卡收单
	bit3:终端支持EMV应用
	bit4:终端支持PBOC借贷记应用
	bit5:终端支持小额支付应用
	bit6:终端支持磁条卡应用
	bit7:其他行业卡应用
	*/
	return 0xff;      //终端支持能力
}
uchar is_eth_module()
{
    char context[64];
	static int ethType =-1;
    if(ethType >= 0) return ethType;
	
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("ETHERNET",context)<0) ethType = 0;
    else if(strstr(context, "BCM5892") || strstr(context, "ON-CHIP")) ethType = 1;
    else if((get_machine_type() == S60) && strstr(context, "DM9000")) ethType = 1;

    return ethType;
}
uchar is_modem_module()
{
	char context[64];
	static int is_modem = 0;
	static int get_flag = 0;
	
	if(get_flag) return is_modem;
	memset(context,0,sizeof(context));
	if(ReadCfgInfo("MODEM",context) > 0)
	{
		is_modem = 1;
	}
	get_flag = 1;
	return is_modem;	
}

/*-----------------------------------------------------------
 功能：获取LAN版本号以及mac地址
 返回：0xff-----没有modem模块
 	   其他-----LAN版本号首字节
 ------------------------------------------------------------*/
uchar GetLanInfo(uchar *lanVer,uchar *ethMac)
{
	char ret;

	// D210有座机，读取座机以太网信息
	if(is_hasbase() )		
	{			
		ret = base_info.ver_lan[0];	
		if(!ret) return 0xff;
		if(lanVer)
			memcpy(lanVer,base_info.ver_lan,30);
		if(ethMac)
			memcpy(ethMac,base_info.eth_mac,6);
		return ret;
	}	
	else if (!is_eth_module()) return 0xff;
	ret = net_version_get();
	if (ret < 0)return 0xff;
	ReadMacAddr(ethMac);
	return ret;
}

/*-----------------------------------------------------------
 功能：获取modem的驱动版本合名字
 返回：0xff-----没有modem模块
 	   其他-----modem名字首字节
 ------------------------------------------------------------*/
uchar GetModemInfo(uchar *drv_ver,uchar *mdm_name)
{
	char ret=0;	
	uchar mdmDev[30],mdmName[30];
	// D210有座机，读取座机MODEM信息
	if(is_hasbase())		
	{		
		ret = base_info.ver_mdm_hard[0];
		if(0 == ret) return 0xff;
		if(drv_ver)
			memcpy(drv_ver,base_info.ver_mdm_soft,4);
		if(mdm_name)
			memcpy(mdm_name,base_info.ver_mdm_hard,20);
		return ret;
	}
	else if(!is_modem_module()) return 0xff;
	memset(mdmDev,0,sizeof(mdmDev));
	memset(mdmName,0,sizeof(mdmName));
	s_ModemInfo_std(mdmDev,mdmName,NULL,NULL);
	if(drv_ver)
		strcpy(drv_ver,mdmDev);
	if(mdm_name)
		strcpy(mdm_name,mdmName);
	ret = mdmName[0];
	
	return ret;
}

int GetTsType(void)
{
    char context[64];
    static int Tstype =-1;

    if(Tstype >= 0) return Tstype;

    memset(context,0,sizeof(context));
    if(ReadCfgInfo("TOUCH_SCREEN",context) < 0) Tstype = 0 ;//无TS Module
    else if(strstr(context, "TM035KBH08")) Tstype = TS_TM035KBH08;//S900, S300
    else if(strstr(context, "RTP_001")) Tstype = TS_S98037A;//S500,S200,S90Q,S58Q
    else Tstype = 0;
    
    return Tstype;
}

uchar GetUseAreaCode(void)
{
	return k_UseAreaCode;
}
uchar GetBiosVer(void)
{
	return k_BiosVer;
}

/*
Vanstone OEM: C941, C942,  C943, C944, C945,   C946, C947
                      S200, S60-T, S300, T620, S60-S, S78,  S90
                      V10 ,  V50,    V37,   V75 ,  V50,    V60,  V70
*/

int s_CheckVanstoneOEM()
{
    uchar C_TermName[16];
    int ret;
    
	memset(C_TermName, 0x00, sizeof(C_TermName));
	ret = ReadCfgInfo("C_TERMINAL", C_TermName);
    if (ret < 0) return 0;
    if (!strcmp(C_TermName, "V10") || 
        !strcmp(C_TermName, "V50") ||
        !strcmp(C_TermName, "V37") ||
        !strcmp(C_TermName, "V75") ||
        !strcmp(C_TermName, "V60") ||
        !strcmp(C_TermName, "V70")) return 1;

    return 0;
}

void get_term_name(unsigned char *name_out)
{
    uchar C_TermName[16];
    int ret;
    
	memset(C_TermName, 0x00, sizeof(C_TermName));
	ret = ReadCfgInfo("C_TERMINAL", C_TermName);
    if (ret < 0) memcpy(name_out, k_TermName, sizeof(k_TermName));
    else memcpy(name_out, C_TermName, sizeof(C_TermName));
}
uchar IsMonFirstRunning(void)
{
	return k_FirstRunStatus;
}
int get_machine_type()
{
    static int machine_type,get_flag=0;
    if(get_flag) return machine_type;
    machine_type =ERR_MACHINE;
    if (!strcmp(k_TermName, "S300")) machine_type=S300;
    else if (!strcmp(k_TermName, "S800")) machine_type=S800;
    else if (!strcmp(k_TermName, "S900")) machine_type=S900;
	else if (!strcmp(k_TermName, "S80")) machine_type=S80;
	else if (!strcmp(k_TermName, "S90")) machine_type=S90;
	else if (!strcmp(k_TermName, "S60")) machine_type=S60;
	else if (!strcmp(k_TermName, "S58")) machine_type=S58;
	else if (!strcmp(k_TermName, "S78")) machine_type=S78;
	else if (!strcmp(k_TermName, "SP30")) machine_type=SP30;
	else if (!strcmp(k_TermName, "S500")) machine_type=S500;
	else if (!strcmp(k_TermName, "D200")) machine_type=D200;
	else if (!strcmp(k_TermName, "D210")) machine_type=D210;
	else return 0;
    get_flag=1;
    return machine_type;
}

void SCR_PRINT(uchar x, uchar y, uchar mode, uchar *HanziMsg, uchar *EngMsg)
{
    if(bIsChnFont)
    {
        ScrPrint(x, y, mode, "%s", HanziMsg);
    }
    else
    {
        ScrPrint(x, y, mode, "%s", EngMsg);
    }
}

void SCR_PRINT_EXT(uchar x, uchar y, uchar mode, uchar *HanziMsg, uchar *EngMsg, int iPara)
{
    if (bIsChnFont)
    {
        ScrPrint(x, y, mode, HanziMsg, iPara);
    }
    else
    {
        ScrPrint(x, y, mode, EngMsg, iPara);
    }
}

int UpperCase(char *sourceStr, char *destStr, int MaxLen)
{
    int     i;

    for(i=0; i<MaxLen; i++)
    {
        if((sourceStr[i] >= 'a') && (sourceStr[i] <= 'z'))
        {
            destStr[i] = sourceStr[i] - 32;
        }
        else
        {
            destStr[i] = sourceStr[i];
            if(destStr[i] == 0x00)
            {
				break;
			}
        }
    }
    return(i);
}

//切换主应用功能目前需要v11至v19或v60(不含V60)以上的Boot支持。
int IsSupportSwitchApp(void)
{
	if(((k_BiosVer > 10) && (k_BiosVer < 20)) || (k_BiosVer > 60))
	{
		return 1;
	}
	return 0;
}

//用于关机界面提示。
//英文界面下不支持该功能
void PowerOffHandler(void)
{
    const uchar	WINDOW_RATIO = 8;

	static int left, top, winWidth, winHeight; 
	int tmpX, tmpY, j, i;
	int singleWidth, multiWidth;
	uchar uaShowWord[30];
	static int isWindowShow=0;
	int color = 0xffff00;
	uchar r, g, b;
	ST_FONT bkSingleFont, bkMultiFont;
	ST_LCD_INFO  tLcdInfo;
	const int ALPHA=235;
	uchar key;

	if(!isWindowShow)
	{
        s_StopBeep();
        
		CLcdGetInfo(&tLcdInfo);
		winWidth = tLcdInfo.width - (2 * tLcdInfo.width / WINDOW_RATIO);
		winHeight = tLcdInfo.height - (2 * tLcdInfo.height / WINDOW_RATIO);
		left = tLcdInfo.width / WINDOW_RATIO;
		top = tLcdInfo.height / WINDOW_RATIO;

		s_GetCurScrFont(&bkSingleFont, &bkMultiFont);
		ScrFontSet(1);		
		s_GetLcdFontCharAttr(&singleWidth, NULL, &multiWidth, NULL);
		FrameBufferFillRect(left, top, winWidth, winHeight, 0x3079EA, ALPHA);

		memset(uaShowWord, 0, sizeof(uaShowWord));
		if(bIsChnFont)
		{
			strcpy(uaShowWord,  "[确认] - 关机 ");
			uaShowWord[strlen(uaShowWord)] = '\0';
			tmpX = (winWidth + 2*left - multiWidth * strlen(uaShowWord) / 2) / 2;
			tmpY = top + 1*(winHeight/5);
			FrameBufferTextOut(tmpX, tmpY, 1, color, uaShowWord);

			if(get_machine_type() == D200)
			{
				strcpy(uaShowWord,  "[F1] - 重启 ");
			}
			else
			{
				strcpy(uaShowWord,  "[功能] - 重启 ");
			}
			uaShowWord[strlen(uaShowWord)] = '\0';
			tmpX = (winWidth + 2*left - multiWidth * strlen(uaShowWord) / 2) / 2;
			tmpY = top + 2*(winHeight/5);
			FrameBufferTextOut(tmpX, tmpY, 1, color, uaShowWord);

			if(IsSupportSwitchApp())
			{
				strcpy(uaShowWord,  "[0] - 切换主应用 ");
				uaShowWord[strlen(uaShowWord)] = '\0';
				tmpX = (winWidth + 2*left - multiWidth * strlen(uaShowWord) / 2) / 2;
				tmpY = top + 3*(winHeight/5);
				FrameBufferTextOut(tmpX, tmpY, 1, color, uaShowWord);
			}

			strcpy(uaShowWord,  "其它键 - 返回 ");
			uaShowWord[strlen(uaShowWord)] = '\0';
			tmpX = (winWidth + 2*left - multiWidth * strlen(uaShowWord) / 2) / 2;
			if(IsSupportSwitchApp())
			{
				tmpY = top + 4*(winHeight/5);
			}
			else
			{
				tmpY = top + 3*(winHeight/5);
			}
			FrameBufferTextOut(tmpX, tmpY, 1, color, uaShowWord);
		}
		else
		{
			strcpy(uaShowWord,  "[ENTER] - POWER OFF");
			uaShowWord[strlen(uaShowWord)] = '\0';
			tmpX = (winWidth + 2*left - singleWidth * strlen(uaShowWord)) / 2;
			tmpY = top + 1*(winHeight/5);
			FrameBufferTextOut(tmpX, tmpY, 1, color, uaShowWord);

			if(get_machine_type() == D200)
			{
				strcpy(uaShowWord,  "[F1] - REBOOT");
			}
			else
			{
				strcpy(uaShowWord,  "[FUNC] - REBOOT");
			}
			uaShowWord[strlen(uaShowWord)] = '\0';
			tmpX = (winWidth + 2*left - singleWidth * strlen(uaShowWord)) / 2;
			tmpY = top + 2*(winHeight/5);
			FrameBufferTextOut(tmpX, tmpY, 1, color, uaShowWord);

			strcpy(uaShowWord,  "Other Key - RETURN");
			uaShowWord[strlen(uaShowWord)] = '\0';
			tmpX = (winWidth + 2*left - singleWidth * strlen(uaShowWord)) / 2;
			tmpY = top + 3*(winHeight/5);
			FrameBufferTextOut(tmpX, tmpY, 1, color, uaShowWord);
		}
		KbLock(2);
		isWindowShow = 1;
		kbflush();
		SelScrFont(&bkSingleFont, &bkMultiFont);
	}

	if(!kbhit())
	{
		key = getkey();
		if(key == KEYENTER) s_PowerOff();
		else if((get_machine_type() != D200 && key == KEYFN) || (get_machine_type() == D200 && key == KEYF1)) 
		{
			ScrCls();
			Soft_Reset();
		}
		else if((key == KEY0) && (((k_BiosVer>10)&&(k_BiosVer<20))||k_BiosVer>60) && bIsChnFont) 
		{
			ScrCls();
			bcm5892_watchdog_rst();
			while(1);
		}
		else
		{
			CLcdFresh(left, top, winWidth, winHeight);
			isWindowShow = 0;
			KbLock(1);
			OsSuspend(g_taskpoweroff);
		}
	}
}
void PowerOffTask(void)
{
    while(g_taskpoweroff==NULL) OsSleep(2);
	
    OsSuspend(g_taskpoweroff);
    while(1) {
        PowerOffHandler();
    }
}

void Lcd_Powerdown_Prepare(void)
{
    /*预防重启后彩屏有残影*/
    s_ScrLightCtrl(2);
	if(get_machine_type() == D200)
	{
		s_KbLock(0);
		s_TouchKeyStop();
	}
	else
	{
		s_KBlightCtrl(0);
	}
    s_LcdClear();


	/*复位LCD，预防出现黑条，目前只针对S800*/
	s_LcdReset();
}

// 软件复位操作
void Soft_Reset(void)
{
    Lcd_Powerdown_Prepare();
	
	DelayMs(100);
	
	writel(1, DMU_R_dmu_sw_rst_MEMADDR);
	while(1);
}

void bcm5892_watchdog_rst(void)
{
	uint reg, val;
	static unsigned int wdt_count = 0;
	unsigned int us_wdog_time = 100;

	Lcd_Powerdown_Prepare();

	reg = DMU_R_dmu_pwd_blk2_MEMADDR;
	val = readl(reg);
	val &= ~(1<<25);
	writel(val, reg);

	//enable L2 Open Watchdog reset timer
	reg = (DMU_R_dmu_rst_enable_MEMADDR);
	val = readl(reg);
	val |= (1<<11);
	writel(val, reg);        

	//disable open watchdog irq.not service the irq , service the restet
	disable_irq(IRQ_OWDOG_TIMOUT);

	//WDOG_LOAD:write watch dog load register
	wdt_count = (us_wdog_time * BCM5892_WDOG_CLK / 1000000) - 1;
	writel(wdt_count, (WDT_REG_BASE_ADDR + WDOG_LOAD));

	//WDOG_CTL:Enable  watch dog timer         
	writel ((1 << 0) | (1 << 1), (WDT_REG_BASE_ADDR + WDOG_CTL));         

	while(1);
}

void MenuOperate()
{
    uchar   ucRet=0, fn_key, menu_key, PreAction = 1;
    uint    page = 0;
	int iDownloadResult;
	int machine = get_machine_type();
	
	if (D200 == machine)	fn_key = KEYF1;
	else fn_key = KEYFN;
	if (D200 == machine)	menu_key = KEYENTER;
	else menu_key = KEYMENU;
	if(0 != iUartDownloadMain(0))
    {
		if(0 != FsUdiskIn())//没有U盘插入
		{		
QueryKey:            
			while(!kbhit())
			{
				ucRet = getkey();
				if (ucRet == fn_key)
				{
					ShowVer();
					break;
				}
				if(ucRet==menu_key)
				{
					break;
				}
			}
			
			if((ucRet!=fn_key) && (ucRet!=menu_key))
			{
                PortClose(P_USB_HOST);
				return;
			}
		}
		else if(UDiscDload()==0)
		{
	        goto QueryKey;
		}
    }

	PortClose(P_USB_HOST);
	MenuSelect();
}

/*
***PN定义***
*POS:产品名称 - 通讯配置 - 功能和性能 - 外观和其他配置
产品名称 - 有线|无线|IP - 非接|存储|安全 - X|X|包材|电源线
*POS NAME-xxx-xxx-xxxx
*/
int GetPciVer(uchar * VerInfoOut)
{
    uchar *pt,*str,tmpbuf[64];
    int i;

    memset(tmpbuf, 0x00, sizeof(tmpbuf));
    ReadCfgInfo("PN",tmpbuf);

    i=0;
    str = tmpbuf;
    while(i<2)
    {
        pt = strstr(str, "-");
        if(pt==NULL) break;
        str = pt+1;
        i++;
    }
    if(i!=2) return -1;

    VerInfoOut[0]=str[2];
    VerInfoOut[1]=0x00;

    return 0;
}

int s_GetBootInfo(void)
{
	uchar   buf[16];
	uint    i, begin;
	uchar   *pT;
	pT=(uchar*)BOOTVER_STORE_ADDR; 
	if (strcmp(pT+0x100, "VALID_BOOT_PARAM")) return -1;
	memset((void*)&k_AuthInfo,0x00,sizeof(k_AuthInfo));
    if (!strcmp(pT+0x120,"VALID_AUTH_PARAM"))
    {     
        memcpy((void*)&k_AuthInfo,pT+0x140,sizeof(k_AuthInfo));        
    }
    else
    {
        k_AuthInfo.SecMode = POS_SEC_L1;        
        k_AuthInfo.security_level = 0;     
        k_AuthInfo.SnDownLoadSum = 0;        
        k_AuthInfo.LastBblStatus = 0;
        k_AuthInfo.AppDebugStatus = 0;       
        k_AuthInfo.FirmDebugStatus = 0;      
        k_AuthInfo.UsPukLevel = 1;           
        k_AuthInfo.AuthDownSn = 0;           
        k_AuthInfo.TamperClear = 0;
        k_AuthInfo.MachineType= 0;
        strcpy(MonitorInfo.AID,"12345");/*for compatibility :old boot + new monitor*/       
        memset(k_AuthInfo.AesKey, 0x00, sizeof(k_AuthInfo.AesKey)); 
    }	
	k_BiosVer     = pT[0];
	k_UseAreaCode = pT[1];
	k_SafeLevel   = pT[2];
	memset((uchar *)k_SafeConfig,0,8);
	memcpy((uchar *)k_SafeConfig, pT+3, 8);

    k_SdramSize = pT[12];
	k_FirstRunStatus = pT[13];
    k_Charging = pT[14];

    k_MajorVer = 0;
    k_MinorVer = 0;

    memset(buf, 0x00, sizeof(buf));

    for(i=0; i<16; i++)
    {
        if((MonitorInfo.AppVer[i]>='0') && (MonitorInfo.AppVer[i]<='9'))
            buf[i] = MonitorInfo.AppVer[i];
        else
        {
            buf[i] = 0;
            break;
        }
    }
	
    k_MajorVer = atoi(buf);

    begin = i + 1;
    memset(buf, 0x00, sizeof(buf));

    for(i=0; i<16; i++)
    {
        if((MonitorInfo.AppVer[i+begin]>='0') && (MonitorInfo.AppVer[i+begin]<='9'))
            buf[i] = MonitorInfo.AppVer[i+begin];
        else
        {
            buf[i] = 0;
            break;
        }
    }
    k_MinorVer = atoi(buf);

    begin += (i + 1);
    strstr(MonitorInfo.AppVer + begin, "T");

	return 0;
}
POS_AUTH_INFO *GetAuthParam(void)
{
    return &k_AuthInfo;
}
/*
2015/10/09
0:旧有安全方案
非0:授权机制安全方案版本
*/
ushort s_GetBootSecLevel()
{
    POS_AUTH_INFO *authinfo;

    authinfo=GetAuthParam();
    return authinfo->security_level;
}
uint s_GetUsPukLevel()
{
    POS_AUTH_INFO *authinfo;

    authinfo=GetAuthParam();
    return authinfo->UsPukLevel;
}

uchar s_GetFwDebugStatus(void)
{
    POS_AUTH_INFO *authinfo;

    authinfo=GetAuthParam();
    if(0==authinfo->security_level) return ((k_SafeLevel=='D') ? 1:0);

    return authinfo->FirmDebugStatus;    
}

int s_GetBootAesKey(uchar *key)
{
    POS_AUTH_INFO *authinfo;

    authinfo=GetAuthParam();
	memcpy(key, authinfo->AesKey, sizeof(authinfo->AesKey));
	memcpy(key+8, key, 8);

	return 0;
}

int CheckIfDevelopPos(void)
{
    POS_AUTH_INFO *authinfo;

    authinfo=GetAuthParam();
    if(authinfo->security_level) return authinfo->AppDebugStatus;

    return s_CheckIfDevelopPos();
}

int s_GetSecMode(void)
{
    POS_AUTH_INFO *authinfo;

    authinfo=GetAuthParam();
    return authinfo->SecMode; 
}
int s_GetTamperStatus(void)/*1-tamperd,0-no tamper or unlocked*/
{
    POS_AUTH_INFO *authinfo;
    int status = 1;
    authinfo=GetAuthParam();
    switch(authinfo->SecMode) 
    {
        case POS_SEC_L0:
        case POS_SEC_L1:
            if(!s_GetBBLStatus()) status = 0;
            else status = 1;
        break;
        case POS_SEC_L2:
            if(!s_GetBBLStatus() && authinfo->TamperClear>0) status = 0;
            else status = 1;    
        break;
    }
    return status;
}

unsigned char ReadBoardVer(char *keyword)
{
	int iRet = 0;
	uchar TempBuff[40];
	memset(TempBuff, 0, sizeof(TempBuff));
	iRet = ReadCfgInfo(keyword,TempBuff);
	if (iRet >= 0) 
	{
		 if (TempBuff[0] == 'V')
		 {
		 	iRet = (TempBuff[1]-'0')*0x10 + (TempBuff[2]-'0');
		 	return iRet;
		 }
	}
	return 0xff;
}

void GetHardWareVer(void)
{
	memset(k_TermName, 0x00, sizeof(k_TermName));
	ReadCfgInfo("TERMINAL_NAME", k_TermName);

}
//  打印机类型: 'S'－针式打印机, 'T'－热敏打印机,0-无打印机
uchar get_prn_type(void)
{
	int ret;
	uchar context[20];
	
	ret = ReadCfgInfo("PRINTER",context);
	if (ret <= 0) return 0;
    if(!strcmp(context,"MP130")) return 'S';
    else return 'T';
}

int IsSupportUsbDevice(void)
{
	return 1;
}
//	tilt sensor，1-支持，0-不支持
int IsSupportTiltSensor(void)
{
	uchar chkTiltSensor = 0, context[30];

    if(ReadCfgInfo("G_SENSOR",context) < 0) return 0;
	Tiltsensor_Get_ID(&chkTiltSensor);

	if(chkTiltSensor == 0xe5) return 1;	//has tilt sensor
	else return 0;						//no tilt sensor

}

static uchar GetMainBoardVer(void)
{
	//uchar hver;
	static uchar ret = 0xff;

	if (ret != 0xff)
		return ret;
	
	ret = ReadBoardVer("MAIN_BOARD");
	return ret;
}

/*
获取硬件资源分配差异化分支
S300:
发展版:MB>=0X20,S300HW_V2x
高级版:MB=V08, S300HW_Vxx(Prolin OS)

S500: 无分支

S800:
发展版:MB>=0X20,S800HW_V2x
高级版:MB<0x20, V07、V08, S800HW_Vxx

S900:
统一版:MB>=0X30,S900HW_V3x
发展版:MB>=0X20,S900HW_V2x
高级版:MB<0x20, V06、V08, S900HW_Vxx

D210:
国密版:MB>=0X15, D210HW_V2x
Going版:MB=V04 ,D210HW_Vxx
*/
int GetHWBranch()
{
	uchar hver;
    int type;
    
	hver = GetMainBoardVer();
	type = get_machine_type();
	switch(type)
	{
	    case S900:
            if (hver >= 0x30) hver = S900HW_V3x;
            else if (hver >= 0x20) hver = S900HW_V2x;
            else hver = S900HW_Vxx; 
	    break;
	    case S800:
            if (hver >= 0x40) hver = S800HW_V4x;
            else if (hver >= 0x20)hver = S800HW_V2x;
            else hver = S800HW_Vxx;  	    
	    break;
	    case D210:
	        if (hver>=0x15) hver = D210HW_V2x;
	        else hver = D210HW_Vxx;
	    break;
	    default:
	        hver = 0;
	    break;	    
	}
	return hver;
}

//获取终端非接外观
//01:表示S500-R
//02:表示S900-R
//默认情况下返回0，表示旧外观
int GetTerminalRfStyle(void)
{
	static int RfStyle = -1;
	char context[64];

	if(RfStyle >= 0) return RfStyle;
	memset(context,0,sizeof(context));
	if(ReadCfgInfo("TERMINAL_RF_STYLE",context) > 0)
	{
		if(!memcmp(context,"01",2))
		{
			RfStyle = 1;
		}
		else if(!memcmp(context,"02",2))
		{
			RfStyle = 2;
		}
		else
		{
			RfStyle = 0;
		}
	}
	else
	{
		RfStyle = 0;
	}
	
	return RfStyle;
}

int GetTermInfo(unsigned char *out_info)
{
	uchar temp,context[20];
    if(out_info == NULL) return 0;
    memset(out_info, 0x00, 20);
    
    out_info[0] = get_machine_type();
    out_info[1] = get_prn_type();   
    temp = GetModemInfo(NULL,NULL);
	if (temp != 0xff)
	{
		out_info[2] = 0x40; 	   //  modem模块配置: 0-不支持
		out_info[3] = 3;		//	modem最高同步速率信息，0-不支持
		out_info[4] = 15;		 //  modem最高异步速率信息，0-不支持
	}

	out_info[5] = 1; //  1-支持PCI，0-不支持
	out_info[6] = fs_get_version();
	out_info[7] = IsSupportUsbDevice();
	temp = GetLanInfo(NULL,NULL);
	if (temp==0xff) 
	{
		out_info[8] = 0;
	}	
	else out_info[8] = temp;
	
    if (is_gprs_module()) out_info[9] = GetWlType();    //  GPRS
    if (is_cdma_module()) out_info[10] = GetWlType();   //  cdma模块配置信息
    out_info[11]= is_wifi_module();   //  WIFI模块配置信息
    out_info[12] = get_rftype();            //  非接触式读卡模块配置信息,RC663

    out_info[13] = bIsChnFont;      //  显示字库版本信息
    out_info[14] = s_GetFontVer();  //  打印字库版本信息
    out_info[15] = CheckICRVer();   //  IC读卡器信息
    out_info[16] = GetMsrType();   //  磁头信息
    out_info[17] = IsSupportTiltSensor(); //  是否有Tilt Sensor 
    if (is_wcdma_module()) out_info[18] = GetWlType(); //  WCDMA 模块配置信息,0－无WCDMA模块;其它－WCDMA模块版本编码 
	if (GetTsType()>0)out_info[19] |= 0x01;//bit0:0－无touchscreen,1－有touchscreen.
	out_info[19] |= 0x02;//bit1:0－无彩屏,1－有彩屏.
	out_info[19] |= 0x04;//bit2:0－monitor,1－monitor_plus,支持动态库.
	if (is_bt_module())//bit3:0-无蓝牙模块,1-有蓝牙模块
	{
		out_info[19] |= 0x08;
	}
	
	memset(context, 0x00, sizeof(context));
	GetBasesoInfo(context);
	if(0x08&context[0]) out_info[19] |= 0x10;//bit4:0-不支持国密算法，1-支持国密算法
    return 20;
}


/****************************************************************************
  函数名     :
  描述       :
  输入参数   :
  输出参数   :
  返回值     :
  修改历史   :
      修改人     修改时间    修改版本号   修改原因
  1、
****************************************************************************/
uchar ReadVerInfo(uchar *VerInfo)
{
    /*主要内容有：BIOS，MONITOR，射频卡，扩展模块版本信息*/
    /*
       [0]=BIOS 版本号
       [1]=监控主版本号：
       [2]=监控次版本号：
       [3]=主板硬件版本号：00-31  (%d)
       [4]=保留;                            //  接口板配置信息：00－31 (%d)
       [5]=扩展功能板配置信息:
                 0x00-0x07:  LAN+VER (%x)(adc:0-7)
                 0x10-0x17:  GPRS+VER(%x)(adc:8-15)
                 0x20-0x27:  CDMA+VER(%x)(adc:16-23)
                 0x30-0x37:  保留           (adc:24-31)
       [6]=磁头配置信息:
                 0x00-0x07:  1/2+VER   (%x)(adc:0-7)
                 0x10-0x17:  2/3+VER   (%x)(adc:8-15)
                 0x20-0x27:  1/2/3+VER (%x)(adc:16-23)
                 0x30-0x37:  保留           (adc:24-31)
       [7]=保留;
//       [7]=射频卡配置信息:
//                 0x00-0x07:  射频卡+VER (%x)(adc:0-7)
//                 0x10-0x17:  保留       (%x)(adc:8-15)
//                 0x20-0x27:  保留       (%x)(adc:16-23)
//                 0x30-0x37:  保留       (%x)(adc:24-31)
    */

    VerInfo[0] = k_BiosVer;
    VerInfo[1] = k_MajorVer;
    VerInfo[2] = k_MinorVer;
    VerInfo[3] = ReadBoardVer("MAIN_BOARD");
    VerInfo[4] = ReadBoardVer("PORT_BOARD");
	VerInfo[5] = 0xff;
    if (is_gprs_module())VerInfo[5] = ReadBoardVer("GPRS_BOARD");
    else if (is_cdma_module())VerInfo[5] = ReadBoardVer("CDMA_BOARD");
    else if (is_wcdma_module())VerInfo[5] = ReadBoardVer("WCDMA_BOARD");
	
    VerInfo[6] = 0x23;
    VerInfo[7] = get_rftype();//  非接触式读卡模块配置信息,RC663
    return 0;
}

void ShowVer()
{
	int page = 0, maxpage=12, subpage, maxsubpage,lastpage, tusn_len = 0, line=0;
	uchar sn[33], esn[40],tusn[64], szModemVerH[30],szModemVerS[30],szRemoteVer[30],module_check_ver[30];
	uchar basesn[33],baseexsn[40];
	uchar  DispMAIN[40], DispEXTB[30], DispMAGB[30], WnetModInfo[128], WnetImei[20];
	uchar  DispPORT[30], ip_ver[30], icc_ver[30], szFSKVerH[30], TermName[16];
	uchar ucPEDVerInfo[20],temp[40],pedver[16];
	uchar ethMac[6],lanVer[30];
	uchar Area, Type, ucNandSpace,ucGetKey;
	ulong ulFsFreeSize =0;	//文件系统剩余大小(单位:KB)		
	long lFontSize =0;	//字库大小
    SO_INFO soInfo;
	FILE_INFO astFileInfo[MAX_FILES];
    int fNums,i,fd,tmpd;
    uchar attr[2];
	POS_AUTH_INFO *authinfo;
	uchar csn[129];
	int len=0;
	uchar subpageindex[MAX_FILES];
	uchar uckeyup = KEYUP ,uckeydown = KEYDOWN;
	int SecLevelDisplay,index;
	uchar PN[64];
	
	memset(sn, 0, sizeof(sn));
	memset(esn, 0, sizeof(esn));
	memset(tusn, 0, sizeof(tusn));
	memset(ip_ver,0x00,sizeof(ip_ver));
	memset(icc_ver,0x00,sizeof(icc_ver));
    memset(pedver,0x00,sizeof(pedver));

    if(get_machine_type() == D200)
    {
    	uckeyup = KEYF1;
    	uckeydown = KEYF2;
    }

	ScrFontSet(0);
	ScrCls();
	GetPciVer(pedver);
	memset(TermName, 0x00, sizeof(TermName));
	get_term_name(TermName);
	ScrPrint(0, 0, 0xc0, "%s PCI PED %s", TermName, pedver);

	memset(PN,0,sizeof(PN));
	memset(sn, 0, sizeof(sn));
	memset(esn, 0, sizeof(esn));
	memset(csn, 0, sizeof(csn));
	ReadCfgInfo("PN",PN);
	ReadSN(sn);
	EXReadSN(esn);
	ReadCSN(128,csn);
	tusn_len=ReadTUSN(tusn, sizeof(tusn));
	
	Area = ' ';
	memset(DispEXTB, 0, sizeof(DispEXTB));
	memset(DispPORT, 0, sizeof(DispPORT));
	memset(DispMAIN, 0, sizeof(DispMAIN));
	memset(DispMAGB, 0, sizeof(DispMAGB));

	if(k_UseAreaCode == CHINA_VER) 	       Area = 'C';
	if(k_UseAreaCode == HONGKONG_VER)	Area = 'E';

	while (1)
	{
START:
		ScrClrLine(1,7);

		switch(page) 
		{
		case 0:
			ScrCls();//
			ScrPrint(0, 0, 0xc0, "%s PCI PED %s", TermName, pedver);
			if(sn[0])  ScrPrint(0,1,0,"SN  : %s", sn);
			else    	ScrPrint(0,1,0,"SN  : ");

			if(esn[0])
			{
				ScrPrint(0,2,0,"EXSN: %s",esn);
				if(strlen(esn)>15) 
				{
					ScrPrint(0,3,0,"      %s",(uchar*)&esn[15]);
				}
			}
			else         ScrPrint(0,2,0,"EXSN: ");
			
			// csn	
			line=0;
			if (csn[0])
			{
				len = strlen(csn);
				if (len > 15)
				{
					memcpy(temp,csn,15);
					temp[16] = '\0';
					ScrPrint(0,4,0,"CSN : %s", temp);
					memset(temp,0,sizeof(temp));
					if(len>27)
					{
						memcpy(temp,csn+15,12);
						strcpy(temp+12,"***");
					}
					else
					{
						strcpy(temp,csn+15);
					}
					ScrPrint(0,5,0,"      %s", temp);
					line=2;
				}
				else
				{
					ScrPrint(0,4,0,"CSN : %s", csn);
					line=1;
				}
			}
			if(tusn_len>0)
			{
				ScrPrint(0,4+line,0,"TUSN: %s",tusn);
				if(strlen(tusn)>15) ScrPrint(0,5+line,0,"      %s",(uchar*)&tusn[15]);	
				line += 2;
			}		

			ScrPrint(0, 4+line, 0x00, "BIOS: %02d(%C%C)",k_BiosVer,Area,(s_GetFwDebugStatus() == 1? 'D':'R'));
			ScrPrint(0, 5+line, 0x00, "MON : %s(%c)", MonitorInfo.AppVer,(CheckIfDevelopPos() == 1? 'D':'R'));
			break;
		case 1:
			if(is_hasbase())
			{	
				GetBaseVer();
				ScrCls();//
				ScrPrint(0, 0, 0xc0, "BASE INFO");
				memset(basesn, 0, sizeof(basesn));
				memset(baseexsn, 0, sizeof(baseexsn));
				memcpy(basesn, base_info.BaseSN, 8);
				if(strlen(base_info.BaseSNLeft))
				{
					memcpy(&basesn[8],base_info.BaseSNLeft,strlen(base_info.BaseSNLeft));
				}
				memcpy(baseexsn, base_info.BaseEXSN, 22);				
				if(basesn[0]) 			ScrPrint(0,1,0,"SN  : %s", basesn);
				else    				ScrPrint(0,1,0,"SN  : ");

				if(baseexsn[0])
				{
					ScrPrint(0,2,0,"EXSN: %s",baseexsn);
					if(strlen(baseexsn)>15) 
					{
						ScrPrint(0,3,0,"      %s",(uchar*)&baseexsn[15]);
					}
				}
				else    ScrPrint(0,2,0,"EXSN: ");			
				
				ScrPrint(0,4, 0x00, "B_BIOS  : %02d ",base_info.ver_boot);
				ScrPrint(0,5, 0x00, "B_MON   : %s ", base_info.ver_soft);
				ScrPrint(0,6, 0X00, "B_HardV : %02d ",base_info.ver_hard); 
				ScrPrint(0,7, 0x00, "B_Type  : %d ",base_info.bt_exist);
			}
			else 
			{
				if(lastpage==0)  page =2;
				if(lastpage==2)  page =0; 
				continue;
			}	
			break;
        
        case 2:
			ScrCls();//
			ScrPrint(0, 0, 0xc0, "%s PCI PED %s", TermName, pedver);
        	fNums=GetFileInfo(astFileInfo);
			memset(subpageindex,0,sizeof(subpageindex));
			for(i=0,maxsubpage=0;i<fNums;i++)
			{
				if(astFileInfo[i].attr==FILE_ATTR_PUBSO&&
					astFileInfo[i].type==FILE_TYPE_SYSTEMSO&&
					strstr(astFileInfo[i].name, MPATCH_EXTNAME))
				{
					memset(&soInfo,0,sizeof(soInfo));
					if(GetSoInfo(astFileInfo[i].name, &soInfo)<0)continue;
					if(soInfo.head.lib_type!=LIB_TYPE_SYSTEM)continue;
					maxsubpage++;
					subpageindex[maxsubpage] = i;
				}
			}

        	if(lastpage == 3) subpage = maxsubpage;
        	else subpage = 0;

        	do
        	{
        		switch (subpage)
        		{
        		case 0:
        			ScrClrLine(1,7);
					if(s_BaseSoLoader()==NULL)
						ScrPrint(0,1,0,"No BaseSo");
					else
					{
						memset(&soInfo,0,sizeof(soInfo));
						if(GetSoInfo("PAXBASE.SO",&soInfo)<=0)break;
						ScrPrint(0,1,0,"Base: %s",soInfo.head.version);
						ScrPrint(0,2,0,"  %s",soInfo.head.date);

						memset(&soInfo,0,sizeof(soInfo));
						if(GetSoInfo("SXXAPI01",&soInfo)<=0)break;
						ScrPrint(0,4,0,"API : %s",soInfo.head.version);
						ScrPrint(0,5,0,"  %s",soInfo.head.date);
					}
					break;
        		default:
					memset(&soInfo,0,sizeof(soInfo));
					GetSoInfo(astFileInfo[subpageindex[subpage]].name, &soInfo);
					ScrClrLine(1,7);
					ScrPrint(0,2,0,"%s:",astFileInfo[subpageindex[subpage]].name);
					ScrPrint(0,4,0,"  V%s",soInfo.head.version);
					ScrPrint(0,6,0,"  %s",soInfo.head.date);
					break;
        		}
				lastpage = page;
        		ucGetKey = getkey();
        		if (ucGetKey == KEYCANCEL || ucGetKey == KEYMENU)	return;
        		if(ucGetKey==uckeyup)
        		{
        			if(subpage == 0)
        			{
        				page = (page == 0? maxpage : (page-1));
        				goto START;
        			}
        			else subpage--;
        		}
        		else if(ucGetKey==uckeydown || ucGetKey==KEYENTER)
        		{
        			if(subpage == maxsubpage)
        			{
        				page = (page == maxpage? 0 : (page+1));
        				goto START;
        			}
        			else
        				subpage++;
        		}
        		else if(ucGetKey>=KEY0 && ucGetKey<=KEY9) WakeUpFtestApp(ucGetKey);
        		
        		ScrPrint(0, 0, 0xc0, "%s PCI PED %s", TermName, pedver);
        	}
        	while(1);
            break;
            
		case 3:
			ScrCls();
			ScrPrint(0, 0, 0xc0, "CONFIGURATION ");
			ScrPrint(0,1,0,"CPU  : ARM11 32BIT\n");
			ScrPrint(0,2,0,"FLASH: %ldMB\n",CheckFlashSize());
			ScrPrint(0,3,0,"DDR  : %dMB", CheckSDRAMSize());
			ulFsFreeSize=freesize()/1024; //unit:KB
			if(ulFsFreeSize/1000) ScrPrint(0,4,0,"FS FREE: %d,%03dKB\n",ulFsFreeSize/1000,ulFsFreeSize%1000);
			else ScrPrint(0,4,0,"FS FREE: %dKB\n",ulFsFreeSize%1000);
			memset(szModemVerH, 0x00, sizeof(szModemVerH) );
			GetModemInfo(szModemVerS,szModemVerH);
			ScrPrint(0,5,0,"MSR  :[%c]   MODEM:[%c]", (GetMsrType()?'Y':'N'), (szModemVerH[0]?'Y':'N'));
			ScrPrint(0,6,0,"PRN  :[%c]   ICC  :[Y]", (0 == get_prn_type()) ? 'N':'Y');
			ScrPrint(0,7,0,"RF   :[%c]   LAN  :[%c]", (get_rftype()?'Y':'N'), (GetLanInfo(NULL,NULL) != 0xff)?'Y':'N');		
			break;
		case 4:	
			ScrCls();
			ScrPrint(0, 0, 0xc0, "CONFIGURATION ");
			ScrPrint(0,1,0,"WIFI :[%c]   BT   :[%c]", (is_wifi_module() ? 'Y':'N'), (is_bt_module() ? 'Y':'N'));			
			ScrPrint(0,2,0,"USBH :[%c]   USBD :[%c]", (0x01 == IsSupportUsbDevice()) ? 'Y':'N',(0x01 == IsSupportUsbDevice()) ? 'Y':'N');	
            ScrPrint(0,3,0,"TILT :[%c]   GPRS :[%c]", (0x01 == IsSupportTiltSensor()) ? 'Y':'N',(is_gprs_module() ? 'Y':'N'));
            ScrPrint(0,4,0,"CDMA :[%c]   WCDMA:[%c]", (is_cdma_module() ? 'Y':'N'),(is_wcdma_module() ? 'Y':'N'));
			break;
		case 5:
			if(is_hasbase())
			{
				GetBaseVer();
			}
			ScrCls();
			ScrPrint(0, 0, 0xc0, "MODULE INFO ");
			if(GetLanInfo(lanVer,ethMac) == 0xff) ScrPrint(0, 1, 0x00, "MAC  : N/A");	
			else
			{
				ScrPrint(0, 1, 0x00, "MAC  : %02X%02X%02X%02X%02X%02X",
							ethMac[0],ethMac[1],ethMac[2],ethMac[3],ethMac[4],ethMac[5]);				
			}
			if (GetWlType())
			{
				memset(WnetModInfo, 0, sizeof(WnetModInfo));
				memset(WnetImei, 0, sizeof(WnetImei));
				memset(temp, 0, sizeof(temp));
				WlGetModuleInfo(WnetModInfo, WnetImei);
				sprintf(temp, "WNET : %s", WnetModInfo);
				ScrPrint(0,3,0,"       %s", &temp[21]);//屏幕一行最多可显示21个ASCII字符
				temp[21]=0x00;
				ScrPrint(0,2,0, "%s", temp);				

				ScrPrint(0, 5, 0x00, "       %s", &WnetImei[14]);
				WnetImei[14] = 0x00;
				if (is_cdma_module()) {
					if (s_CdmaIsMeid()==1)
						ScrPrint(0, 4, 0x00, "MEID : %s", WnetImei);
					else
						ScrPrint(0, 4, 0x00, "IMSI : %s", WnetImei);
				}
				else ScrPrint(0, 4, 0x00, "IMEI : %s", WnetImei); 
			}
			else
			{
				ScrPrint(0, 2, 0x00, "WNET : N/A");	
				ScrPrint(0, 4, 0x00, "IMEI : N/A", temp);
			}	
			memset(szModemVerH, 0x00, sizeof(szModemVerH) );
			GetModemInfo(szModemVerS,szModemVerH);
			if(szModemVerH[0])
			{
				memset(temp, 0x00, sizeof(temp));
				sprintf(temp,  "MODEM: %s,%s", szModemVerS,szModemVerH);
				ScrPrint(0,7,0,"       %s", &temp[21]);//屏幕一行最多可显示21个ASCII字符
				temp[21]=0x00;
				ScrPrint(0,6,0, "%s", temp);
			}	
			else
				ScrPrint(0,6,0, "MODEM: N/A");//modem板不存在
				
			break;			
		case 6:
			ScrCls();
			ScrPrint(0, 0, 0xc0, "MODULE INFO ");
			memset(temp, 0x00, sizeof(temp));
			GetUsbHostVersion(temp);
			ScrPrint(0, 1, 0x00, "USBH : %s", temp);	

			memset(temp, 0x00, sizeof(temp));
			GetUsbDevVersion(temp);
			ScrPrint(0, 2, 0x00, "USBD : %s", temp);	

			memset(temp, 0x00, sizeof(temp));
			if(ScanGetVersion(temp, sizeof(temp)) == 0)
			{
				ScrPrint(0, 3, 0x00, "SCAN : %s", temp);	
			}
			else
			{
				ScrPrint(0, 3, 0x00, "SCAN : N/A");
			}

			memset(temp, 0x00, sizeof(temp));
            SMGetModuleInfo(temp);
			ScrPrint(0, 4, 0x00, "CIPHER: %s", temp);
			break;
		case 7:
			ScrCls();
			ScrPrint(0, 0, 0xc0, "WIFI INFO ");

			if (is_wifi_module())
			{
				memset(temp, 0, sizeof(temp));
				ReadCfgInfo("WIFI_NET",temp);
				ScrPrint(0, 1, 0x00, "NAME : %s", temp);
				memset(temp, 0, sizeof(temp));
				s_WifiGetMac(temp);
				ScrPrint(0, 2, 0x00, "MAC  : %02X%02X%02X%02X%02X%02X",
							temp[0],temp[1],temp[2],temp[3],temp[4],temp[5]);
				
				s_WifiGetHVer(temp);
				ScrPrint(0, 3, 0x00, "FWVER: %s",temp);
				
			}
			else
			{
				ScrPrint(0, 1, 0x00, "NAME : N/A");
				ScrPrint(0, 2, 0x00, "FWVER: N/A");
				ScrPrint(0, 3, 0x00, "MAC  : N/A");
			}
			break;	
		case 8:
			ScrCls();
			ScrPrint(0, 0, 0xc0, "BT INFO ");
			if (is_bt_module())					// 有蓝牙模块
			{
				memset(temp,0,sizeof(temp));
				
				//s_BtGetName(temp); //deleted by shics must add again
			
				ScrPrint(0, 1, 0x00, "NAME : %s", temp);
				memset(temp,0,sizeof(temp));
				
				//s_BtGetMac(temp); //deleted by shics must add again
				
				ScrPrint(0, 2, 0x00, "MAC  : %02X%02X%02X%02X%02X%02X",
					temp[0],temp[1],temp[2],temp[3],temp[4],temp[5]);
				memset(temp,0,sizeof(temp));
				
				//s_BtGetVer(temp); //deleted by shics must add again
				
				ScrPrint(0, 3, 0x00, "FWVER: %s",temp);
			}
			else
			{
				ScrPrint(0, 1, 0x00, "NAME   : N/A");
				ScrPrint(0, 2, 0x00, "MAC    : N/A");
				ScrPrint(0, 3, 0x00, "FWVER  : N/A");
			}
			break;		
		case 9:
			ScrCls();
			ScrPrint(0, 0, 0xc0, "SOFTWARE INFO");
			//PED Lib
			memset(ucPEDVerInfo,0X00, sizeof(ucPEDVerInfo));
			if (!PedGetLibVer(ucPEDVerInfo))
			{
				ScrPrint(0, 1, 0x00, "PED : %s",ucPEDVerInfo);
			}
			else
			{
				ScrPrint(0, 1, 0x00, "PED : N/A");
			}			

			memset(ip_ver,0,30);
			NetGetVersion_std(ip_ver);
			ScrPrint(0,2,0x00, "IP  : %s",ip_ver);
			SciGetVer(icc_ver);
			ScrPrint(0,3,0x00, "ICC : %s",icc_ver);
			memset(temp,0,sizeof(temp));
			PcdGetVer(temp);
			if (temp[0])
				ScrPrint(0,4,0x00, "RF  : %s",temp);
			else
				ScrPrint(0,4,0x00, "RF  : N/A");
			memset(szRemoteVer,0,sizeof(szRemoteVer));
            if (!RemoteLoadVer(szRemoteVer))
				ScrPrint(0,5,0x00, "TMS : %s", szRemoteVer);
			else
				ScrPrint(0,5,0x00, "TMS : N/A");
			break;
		case 10:
			ScrCls();
			ScrPrint(0, 0, 0xc0, "FONT LIB INFO");
			ScrPrint(0, 1, 0x00, "VER  : %d",s_GetFontVer());
			 //size
			lFontSize = filesize(FONT_FILE_NAME);
			if (lFontSize < 0) ScrPrint(0,2,0,"NO FONT FILE");
			else ScrPrint(0,2,0,"SIZE : %dBytes",lFontSize);
			break;
		case 11:
			ScrCls();
			ScrPrint(0, 0, 0xc0, "SECURITY VERSION");

			if(pedver[0] == '4')			
			{
				ScrPrint(0, 1, 0x00, "PCI version: 4.x");
				ScrPrint(0, 2, 0x00, "SecurityVer: 4.00.00");
				ScrPrint(0, 3, 0x00, "Firmware # : 4.00.00");
			}
			else
			{
				ScrPrint(0, 1, 0x00, "PCI version: 3.x");
				if(get_machine_type() == D200)
				{
					ScrPrint(0, 2, 0x00, "SecurityVer: 1.6");
					ScrPrint(0, 3, 0x00, "Firmware # : 1.6");
				}
				else
				{
					ScrPrint(0, 2, 0x00, "SecurityVer: 3.02.00");
					ScrPrint(0, 3, 0x00, "Firmware # : 3.02.00");
				}
			}
			ScrPrint(0, 4, 0x00, "Hardware # :\n%s",PN); 
			break;
		case 12:
			if(0!=s_GetBootSecLevel())
			{
				ScrCls();
				ScrPrint(0, 0, 0xc0, "ADDITIONAL INFO");
    			authinfo=GetAuthParam();
    			SecLevelDisplay = 0;
    			for(index=0;index<16;index++)
    			{
    				if(!((authinfo->security_level) & (1<<index)))
    				{
    					break;
    				}
    				else
    				{
    					SecLevelDisplay++;
    				}
    			}
    			ScrPrint(0, 1, 0x00, "SecLevel   : S%.2d", SecLevelDisplay);
    			ScrPrint(0, 2, 0x00, "SecMode    : L%d", (authinfo->SecMode)&0x01 + ((authinfo->SecMode&0x02)>>1));
    			ScrPrint(0, 3, 0x00, "SnLoadSum  : %d", authinfo->SnDownLoadSum);
    			ScrPrint(0, 4, 0x00, "AppDebug   : %d", CheckIfDevelopPos());
    			ScrPrint(0, 5, 0x00, "FirmDebug  : %d", s_GetFwDebugStatus());
				ScrPrint(0, 6, 0x00, "SnkUpload  : %s",(GetIDKeyState()==1)?"[Y]":"[N]");
            }
            else 
			{
				if(lastpage==11)  page =0;
				if(lastpage==0)  page =11; 
				continue;
			}	
			break;
		default:
			break;
		}
		lastpage = page;
		ucGetKey = getkey();
		if (ucGetKey == KEYCANCEL || ucGetKey == KEYMENU)	return;
		if(ucGetKey==uckeyup) page = (page == 0? maxpage : (page-1));
		else if(ucGetKey==uckeydown) page = (page == maxpage? 0 : (page+1));
		else if(ucGetKey==KEYENTER) page = (page == maxpage? 0 : (page+1));
		else if(ucGetKey>=KEY0 && ucGetKey<=KEY9) WakeUpFtestApp(ucGetKey);
	}
}

void ShowDevelopInfo()
{
	uchar ucFlag = 0;

	if(CheckIfDevelopPos())
	{
		ScrRestore(0);
		ScrCls();
		
		if(k_UseAreaCode == CHINA_VER && bIsChnFont == CHINA_VER)
		{
			ScrPrint(0,0,0x81,"      警告      ");
			ScrPrint(0,3,0x01,"本机仅用于调试");
			ScrPrint(0,6,0x01,"请勿用于商业用途");
			kbflush();

			while (kbhit())
			{
				ScrPrint(0,0,(ucFlag == 0 ? 0x01:0x81),"      警告      ");	
				ucFlag = (ucFlag == 0 ?1:0);
				DelayMs(500);
			}
		}
		else
		{
			ScrPrint(0,0,0x81,"    WARNING     ");
            ScrPrint(0,2,0x01,"This PED only");
            ScrPrint(0,4,0x01,"for DEBUG, Not");
            ScrPrint(0,6,0x01,"for COMMERCIAL!");
			kbflush();
			
			while (kbhit())
			{
				ScrPrint(0,0,(ucFlag == 0 ? 0x01:0x81),"    WARNING     ");	
				ucFlag = (ucFlag == 0 ?1:0);
				DelayMs(500);
			}
		//	kbflush();
		}
		ScrRestore(1);
	}
}

extern void LoadFontLib(void);
void idletask()
{
    while(1);
}
//通过看门狗状态检测是否为切换主应用
void Check_QuickReboot(void)
{
	uint laststatus;
	
	laststatus = (*(volatile unsigned int *)0x01024084);
	if((laststatus == DMU_LAST_RST_OPEN_WDT) || (laststatus == DMU_LAST_RST_SPL_WDT))
	{
		SystemInit_quick(); 
		LoadFontLib();//读取字库   
		if(GetHWBranch() == D210HW_V2x) s_SetImplicitEcho();
		ShowDevelopInfo();
		OsCreate((OS_TASK_ENTRY)idletask,TASK_PRIOR_IDLE,0x100);
		g_taskpoweroff = OsCreate((OS_TASK_ENTRY)PowerOffTask,TASK_PRIOR_Firmware9,0x1000);
		s_BaseSoLoader();
		s_SelfVerifyApp(); 	/*self verify app and so*/
		ScrCls();
		while(2)
		{
			//OsSleep(5); 
			MonitorLoadApp();
			//如果没有应用，则运行进入菜单项
			MenuOperate();
			
		}
	}

}
typedef struct {
	uint8_t name[64];	
	unsigned char mac[6];	
	unsigned char RFU[10];
}ST_BT_SLAVE_T;

typedef void (*recv_callback)(void *data, size_t size);

typedef struct {
	uint8_t name[64];
	unsigned char pin[16];
	unsigned char mac[6];
	unsigned char RFU[10];
} ST_BT_CONFIG_T;


extern int BTOpen(void);
extern int BTClose(void);
extern int BtGetConfig(ST_BT_CONFIG_T *pCfg);
extern int BtSetConfig(ST_BT_CONFIG_T *pCfg);

extern int BtScanCommon(ST_BT_SLAVE_T *pSlave, unsigned int Cnt, unsigned int Timeout);
extern int BtConnectCommon(ST_BT_SLAVE_T *Slave);
extern int BtSendCommon(void* data, size_t size);

extern int BtRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms);

extern int BTScan(ST_BT_SLAVE_T *pSlave, unsigned int Cnt, unsigned int Timeout);
extern int BTStopScan(void);
extern int BTConnect(ST_BT_SLAVE_T *Slave);
extern int BTDisconnect(void);
extern int BTSend(void* data, size_t size);
extern void BTSetRecvCallback(recv_callback callback);
extern void BleServerSend(void* data, size_t size);
extern unsigned char BtPortSends(unsigned char *szData, unsigned short iLen);

extern void BTRunOnce(void);


extern void WltBtTest(void);

#if 0
void recv_handler(void *data, size_t size) {
	size_t i = 0;

	for (i = 0; i < size; i++) {
		printk("%02x ", ((char*)data)[i]);
	}

	printk("\n");
}
#endif


#define BLE_ENABLE 0
#define CLASSIC_ENABLE 1

//主函数
int main_code(void)
{
	unsigned char szData[1024];
	unsigned short usLen;
	unsigned short ms;
	int retRecv;


	//检测是否为切换主应用
	Check_QuickReboot();
	SystemInit();//各个模块初始化
	LoadFontLib();//读取字库   

	// 将回显放在字库加载之后实现，确保能根据字库类型确定是显示中文还是英文
	if(GetHWBranch() == D210HW_V2x) s_SetImplicitEcho();
	g_taskpoweroff = OsCreate((OS_TASK_ENTRY)PowerOffTask,TASK_PRIOR_Firmware9,0x1000);
	ShowDevelopInfo();
    OsCreate((OS_TASK_ENTRY)idletask,TASK_PRIOR_IDLE,0x100);
	//本地下载通讯检测，如果有按键，进入菜单项
	while(1)
	{
        if(s_BaseSoLoader()) break;
    	MenuOperate();
        ScrCls();
        ScrPrint(0, 3, 0x41, "No BaseSo");
	}
	DispCustomLogo();
	s_SelfVerify();     /*self verify app and so*/
	ScrCls();//s_SelfVerify函数有时运行时间长，所以清屏放在s_SelfVerify后面
    
    //检测远程下载和本地下载
	//是否上次有没有下载完成的远程下载任务
	RunUpdataRemoteDownload();
    MenuOperate();//本地下载通讯检测，如果有按键，进入菜单项


	WltBtTest();

#if 0
	BTOpen();

	
	ScrCls();
    ScrPrint(0, 3, 0x41, "start test. input key");
	ST_BT_SLAVE_T remote_devices[5];
	uchar ret;
	int size =0;
	int n = 0, i = 0;
	int bl_size;
	unsigned int t0,t1,t2;

	
	kbflush();

	
	for (;;) {
		if(kbhit()==0) {
			ret = getkey();

			if(ret== KEY1) {

				ScrCls();
				ScrPrint(0, 0, 0, "Inquiry ...");

				if(BLE_ENABLE) {
					 bl_size = BleScan(remote_devices, 5, 10);
					 printk("ble: scan size:%d \r\n", bl_size);
					 for (i = 0; i < bl_size; i++) {
					 	printk("ble:name: %s, mac: %s \r\n", remote_devices[i].name, bd_addr_to_str(remote_devices[i].mac));
				 	}
				} else if(CLASSIC_ENABLE) {
					size = BTScan(remote_devices, 5, 10);
					printk("classic: scan size:%d \r\n", size);
					for (n = 0; n < size; n++) {
						printk("classic:name: %s, mac: %s \r\n", remote_devices[n].name, bd_addr_to_str(remote_devices[n].mac));
					}
				} else {
					size = BtScanCommon(remote_devices, 5, 20);
					printk("common: scan size:%d \r\n", size);
					for (n = 0; n < size; n++) {
					printk("common:n:%d name: %s, mac: %s \r\n", n, remote_devices[n].name, bd_addr_to_str(remote_devices[n].mac));
					}
				}	
			} else if (ret == KEY2) {
				ST_BT_SLAVE_T remote;
				ScrCls();
				ScrPrint(0, 0, 0, "Connect");
				//sangsung
				sscanf_bd_addr("A0:CB:FD:C5:0B:6F", remote.mac);
				//iphone 6s
				//sscanf_bd_addr("54:7A:99:56:E5:13", remote.mac);
				//D210
				//sscanf_bd_addr("00:E0:12:34:56:99", remote.mac);
				
				if(BLE_ENABLE) {
					BleConnect(&remote);
					//BleSetRecvCallback(recv_handler);
				} else if(CLASSIC_ENABLE){
					BTConnect(&remote);

					//BTSetRecvCallback(recv_handler);
				} else {
					BtConnectCommon(&remote);
				}

			} else if (ret == KEY3) {
				ST_BT_SLAVE_T remote;
				int iSentBytes;
				iSentBytes = 1024;
				char szSend[10000];

				for(i=0; i<iSentBytes; i++) {
					szSend[i] =  '0';
				}

				
				ScrCls();
				ScrPrint(0, 0, 0, "Send data");
				sscanf_bd_addr("A0:CB:FD:C5:0B:6F", remote.mac);
				if(BLE_ENABLE) {
					BleSend("Hello", sizeof("Hello"));
				} else if (CLASSIC_ENABLE){
					//BTSend("0123456", strlen("0123456"));
					//BTSend(szSend, iSentBytes);
					BtPortSends("0123456", 7);
				} else {
					BtSendCommon("Hello", sizeof("Hello"));
				}
				
			} else if (ret == KEY4) {
				ST_BT_SLAVE_T remote;
				ScrCls();
				ScrPrint(0, 0, 0, "Disconnect");
				sscanf_bd_addr("A0:CB:FD:C5:0B:6F", remote.mac);
				if(BLE_ENABLE) {
					 BleDisconnect();
					 BTClose();
				} else {
					BTDisconnect();
					BTClose();
				}

			} else if (ret == KEY5) {
				ScrCls();
				ScrPrint(0, 0, 0, "le server send ");
				BleServerSend("hello", sizeof("hello"));
			} 
#if 0			
			else if (ret == KEY6) {
				ST_BT_CONFIG_T devInfo[2];
				ScrCls();
				ScrPrint(0, 0, 0, "get config info ");
				BtGetConfig(devInfo);
				printk("devInfo name :%s mac:%s \r\n", devInfo[0].name ,bd_addr_to_str(devInfo[0].mac));
			}else if(ret == KEY7) {
				ST_BT_CONFIG_T *devInfoName;
				devInfoName = malloc(sizeof(*devInfoName));
				memcpy(devInfoName->name, "test", 5);
				ScrCls();
				ScrPrint(0, 0, 0, "set config info ");
				BtSetConfig(devInfoName);
			}else if(ret == KEY8) {
				retRecv = BtRecvs(szData, usLen, ms);
				printk("rtRecv:%d \r\n", retRecv);
				if(0 < retRecv) {
					printk("rtRecv > 0:%d \r\n", retRecv);
					for (i = 0; i < retRecv; i++) {
						printk("i:%d, usLen:%d \r\n", i, retRecv);
						printk("data:%02x \r\n", ((char*)szData)[i]);
					}
				
				}

			}
#endif			
			
		}

		t0 = GetTimerCount();
		BTRunOnce();
		t1 = GetTimerCount();
		t2 = t1 - t0;
		printk("t0:%d t1:%d t2:%d\r\n", t0,t1,t2);
	}

#endif	

	


	#if 0// shics test
	extern int bt_test_main();
	ScrCls();
    ScrPrint(0, 3, 0x41, "test_main");
	printk("test_main\r\n");
	bt_test_main();
	ScrCls();
    ScrPrint(0, 3, 0x41, "exit test_main");
	#endif	
	while(2)
	{
		MonitorLoadApp();
		//如果没有应用，则运行进入菜单项
		MenuOperate();
	}

}

uchar AsciiToHex(uchar *ucBuffer)
{
     uchar Temp[2];
	 uchar ucTemp = 0x00;

	 if((ucBuffer[0] >= '0') && (ucBuffer[0] <= '9'))
	 {
           Temp[0] = ucBuffer[0] - 0x30;
	 }
     else if((ucBuffer[0] >= 'a') && (ucBuffer[0] <= 'z'))
	 {
		   Temp[0] = ucBuffer[0] - 0x61 + 0x0a;
	 }
	 else if((ucBuffer[0] >= 'A') && (ucBuffer[0] <= 'Z'))
	 {
           Temp[0] = ucBuffer[0] - 0x41 + 0x0a;
	 }

     if((ucBuffer[1] >= '0') && (ucBuffer[1] <= '9'))
	 {
           Temp[1] = ucBuffer[1] - 0x30;
	 }
     else if((ucBuffer[1] >= 'a') && (ucBuffer[1] <= 'z'))
	 {
		   Temp[1] = ucBuffer[1] - 0x61 + 0x0a;
	 }
	 else if((ucBuffer[1] >= 'A') && (ucBuffer[1] <= 'Z'))
	 {
           Temp[1] = ucBuffer[1] - 0x41 + 0x0a;
	 }

     ucTemp = (Temp[0] << 4) + Temp[1];
	 return (ucTemp);
}

void HexToAscii(uchar ucData, uchar *pucDataOut)
{
     uchar Temp[3];

	 Temp[0] = (ucData & 0xF0) >> 4;
	 Temp[1] = ucData & 0x0F;

	 if(Temp[0] <= 0x09)
	 {
	      Temp[0] = Temp[0] + 0x30;
	 }
     else if((Temp[0] >= 0x0A)  && (Temp[0] <= 0x0F))
     {
          Temp[0] = (Temp[0] - 0x0A) + 'A';
     }

	 if(Temp[1] <= 0x09)
	 {
	      Temp[1] = Temp[1] + 0x30;
	 }
     else if((Temp[1] >= 0x0A)  && (Temp[1] <= 0x0F))
     {
          Temp[1] = (Temp[1] - 0x0A) + 'A';
     }

	 memcpy(pucDataOut, Temp, 2);
}

