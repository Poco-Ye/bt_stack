
#include <base.h>
#include <stdarg.h>
#include "posapi.h"
#include "..\puk\puk.h"
#include "..\download\localdownload.h"
#include "..\kb\kb.h"
#include "..\comm\comm.h"
#include "..\ModuleCheck\ModuleCheck.h"
#include "..\fat\fsapi.h"
#include "..\cfgmanage\cfgmanage.h"
#include "..\audio\audio.h"
#include "..\font\font.h"
#include "..\wifi\apis\wlanapp.h"


struct T_FUN_TABLE{
	uint ucFunIdx;//模块编号
	void * PtrShowStrCH;
	void * PtrShowStrEN;
	void (* RunCaseFun)();//执行函数
};

#define MENU_ALL            0XFFFFFFFF
#define MENU_SUB(type)      (1<<type)
#define MENU_MAX_ITEMS      32

#define ITEM_VALUE_MAX_LEN	128
typedef struct SYSCONFIG_ITEM_TAG
{
	char		*pItemName;			/* item name */
	void		(*funcPtr)(int value);	/* item register function */
}T_SYSCONFIG_ITEM;

extern void s_ModemConfig(int value);
extern void s_PrnSmallFontConfig(int value);
extern void s_IconConfig(int value);
extern void s_SetCLcdSleepMode(int value);
extern void s_BaseBtLinkEnable(int value);
/* init system configuration table */
static T_SYSCONFIG_ITEM sysconfigTab[] =
{
	/* Item name			module get/set func ptr*/
	{"KeyCombination", 		s_KeyCombConfig},
	{"EnterKeyCombination", s_EnterKeyCombConfig},
	{"ModemPowerDown",      s_ModemConfig},
	{"SMALLFONT",           s_PrnSmallFontConfig},
    {"IconCFG",             s_IconConfig},	
    {"CLcdSleepMode",       s_SetCLcdSleepMode},
	{"BaseBtLinkEnable",	s_BaseBtLinkEnable}
};

//--------------POS自测试
extern void SCR_PRINT(uchar x, uchar y, uchar mode, uchar *HanziMsg, uchar *EngMsg);
void RemoteDload(void);
extern uchar bIsChnFont;
int  s_open(char *filename, unsigned char mode, unsigned char *attr);

extern uchar GetUseAreaCode(void);
extern void ShowVer(void);
extern void AdjustLCDGray(void);
extern void SetSystemTime(void);
extern void ScannerReset(void);
extern void ScanUpdate(void);
extern void ScannerMatching(void);

extern void TSCalibration(void);
extern void mdm_adjust_tx_level(uchar en_manual_set,uchar level_value);

extern APPINFO MonitorInfo;

extern void McCheckICC(void);
extern void McCheckMag(void);
extern void McCheckRF(void);
extern void McCheckKB(void);
extern void McCheckLCD(void);
extern void McCheckModem(void);
extern void McCheckETH(void);
extern void McCheckWNET(void);

uchar WaitKeyBExit(int usTimeout,uchar ucDefaultKey)
{
	uchar ucGetKey;

	TimerSet(3, (int) (usTimeout * 10));
	kbflush();

	while(1)
	{
		if(kbhit() == 0x00)
		{
			ucGetKey = getkey();
			break;
		}
		else if(TimerCheck(3) == 0)// when the parameter one is zero, regard as the no timeout
		{
			ucGetKey = ucDefaultKey;
			break;
		}
	}
	
	return ucGetKey;
}


void SetLineRever(uchar ucLine,uchar ucMode)
{
	if (ucMode == CFONT)
	{
		ScrPrint(0,ucLine,REVER|ucMode,"                ");
	}
	else
	{
		ScrPrint(0,ucLine,REVER|ucMode,"                     ");
	}
}


enum {
	DO_NONE = 0,
	DO_LCD,
	DO_BUZZER,
	DO_ICC,
	DO_MAGIC,
	DO_RFID,
	DO_PRINTER,
	DO_MODEM,
	DO_WIFI,
	DO_LAN,
	DO_WNET,
	DO_UART,
};

//-----显示错误信息
const struct T_ERRINFO
{
	ushort usID;
	uchar * szErrStrEN;
	uchar * szErrStrCN;
}tErrInfo[] = {
	DO_PRINTER*256 + PRN_BUSY,"PRINTER BUSY.","打印机忙",
	DO_PRINTER*256 + PRN_PAPEROUT,"NO PAPER.","打印机缺纸",
	DO_PRINTER*256 + PRN_WRONG_PACKAGE,"FORMAT ERR.","打印数据包格式错",
	DO_PRINTER*256 + PRN_FAULT,"PRINTER FALUT.","打印机故障",
	DO_PRINTER*256 + PRN_TOOHEAT,"TOO HEAT.","打印机过热",
	DO_PRINTER*256 + PRN_UNFINISHED,"UNFINISHED.","打印未完成",
	DO_PRINTER*256 + PRN_NOFONTLIB,"NO FONT.","打印机未装字库",
	DO_PRINTER*256 + PRN_OUTOFMEMORY,"OVER MEMEORY.","数据包过长",
	0xffff,"UNKNOWN ERR.","未知错误信息",
};

void DispErrInfo(uchar ucMdIdx,uchar ucErrCode,uchar * ucMdStr)
{
	uchar szErrInfo[50],ucIdx;
	ScrCls();
	SetLineRever(0,CFONT);
	SCR_PRINT(0,0,0x41|0x80, "错误信息", "ERROR INFO");
	memset(szErrInfo,0,50);

	for (ucIdx = 0; ucIdx < sizeof(tErrInfo)/sizeof(struct T_ERRINFO);ucIdx++)
	{//最后一位的数据拷贝
		if (tErrInfo[ucIdx].usID == (ucMdIdx * 256 + ucErrCode) || (ucIdx + 1) == sizeof(tErrInfo)/sizeof(struct T_ERRINFO))
		{
			if (bIsChnFont)
			{
				strcpy(szErrInfo,tErrInfo[ucIdx].szErrStrCN);
			}
			else
			{
				strcpy(szErrInfo,tErrInfo[ucIdx].szErrStrEN);
			}
			break;
		}
	}
	
	ScrPrint(0,2,CFONT,"%s\n%s\n:%02x",ucMdStr,szErrInfo,ucErrCode);
	WaitKeyBExit(600,NOKEY);
}

#define DISPERRINFO(_A_,_B_) DispErrInfo(_A_,_B_,#_A_)

void ListFun(struct T_FUN_TABLE * tFunTab,uchar ucAllCnt,uchar ucPage,uchar ucItemOfPage)
{
	uchar ucBegin,ucDspCnt,ucShowCnt;

	ucBegin = ucPage * ucItemOfPage;
    if((ucBegin+ucItemOfPage)>=ucAllCnt)
    	ucShowCnt = ucAllCnt-ucPage*ucItemOfPage;
    else 
        ucShowCnt = ucItemOfPage;

	ScrClrLine(2,20);

	for (ucDspCnt = 0; ucDspCnt < ucShowCnt; ucDspCnt++)
	{
		if (bIsChnFont)
		{
			ScrPrint(0,(uchar)(ucDspCnt*2 + 2),CFONT,"%d-%s",ucDspCnt+1,tFunTab[ucBegin + ucDspCnt].PtrShowStrCH);
		}
		else
		{
			ScrPrint(0,(uchar)(ucDspCnt*2 + 2),CFONT,"%d-%s",ucDspCnt+1,tFunTab[ucBegin + ucDspCnt].PtrShowStrEN);
		}
	}
}

void RunFun(struct T_FUN_TABLE * tFunTab,uchar ucAllCnt,uchar ucPage,
    uchar ucItemOfPage,uchar ucSel)
{
	uchar ucOffset = ucPage * ucItemOfPage + ucSel;

	if(ucOffset >= ucAllCnt) return;
    if(ucSel >= ucItemOfPage) return;
        
	ScrSetIcon(ICON_UP, CLOSEICON);
	ScrSetIcon(ICON_DOWN, CLOSEICON);
	ScrCls();
	SetLineRever(0,CFONT);
	if (bIsChnFont)
	{
		ScrPrint(0,0,0xc1,"%s",tFunTab[ucOffset].PtrShowStrCH);
	}
	else
	{
		ScrPrint(0,0,0xc1,"%s",tFunTab[ucOffset].PtrShowStrEN);
	}
	tFunTab[ucOffset].RunCaseFun();
}

//-------other fun define above
int GetVenderName(uchar *vendername)
{
	strcpy(vendername,(s_CheckVanstoneOEM()?"Vanstone":"PAX"));
	return 0;
}

int GetDeviceInfo(unsigned char * GetInfoDataIn)
{
	unsigned char szSN[33], TermName[16];
	memset(szSN,0,sizeof(szSN));
	ReadSN(szSN);
	memset(TermName, 0x00, sizeof(TermName));
	get_term_name(TermName);
	return sprintf(GetInfoDataIn,"%s-%s",TermName,szSN);
}
//--------------------获取一、	CUP型号、主频、级别
int GetCPUInfo(uchar * GetInfoDataIn)
{
	uchar szSN[33], TermName[16];
	memset(szSN,0,sizeof(szSN));
	ReadSN(szSN);
	memset(TermName, 0x00, sizeof(TermName));
	get_term_name(TermName);

	return	sprintf(GetInfoDataIn,"SN=\x27%s;POS品牌=%s;型号=%s;CPU.型号=BCM5892;CPU.主频=266MHz;CPU.级别=ARM11 32Bit;",szSN,(s_CheckVanstoneOEM()?"Vanstone":"PAX"),TermName);//,21
}
//--------------------获取二、	存储芯片型号、大小、生产厂商
int GetMemoryInfo(uchar * GetInfoDataIn)
{
		return	sprintf(GetInfoDataIn,"存储器Count=2;存储器.存储芯片型号1=FLASH H9DA1GG51HAMBR;存储器.大小1=128MB;存储器.厂商1=HYNIX;\
	存储器.存储芯片型号2=DDR H9DA1GG51HAMBR;存储器.大小2=64MB;存储器.厂商2=HYNIX;");
}

//------------三、	磁条读写器型号
int GetMagReadInfo(uchar * GetInfoDataIn)
{
	return	sprintf(GetInfoDataIn,"磁条读写器型号=ISO7812标准1/2或2/3或1/2/3磁道，可双向刷卡;");//,26
}


//------------四、	打印机型号
int GetPrinterInfo(uchar * GetInfoDataIn)
{
	uchar buff[20];
	int ret;
	
	memset(buff, 0x00, sizeof(buff));
	ReadCfgInfo("PRINTER", buff);
	if (buff[0]) ret = sprintf(GetInfoDataIn,"打印机型号=%s热敏打印机;", buff);
	else ret = sprintf(GetInfoDataIn,"打印机型号=热敏打印机;");
	return	ret;//,17
}

//------------	五、	IC芯片型号、生产厂商、级别
int GetICCReadInfo(uchar * GetInfoDataIn)
{
	return	sprintf(GetInfoDataIn,"IC芯片.型号=NCN8025;IC芯片.生产厂商=ON Semiconductor;IC芯片.级别=符合EMV4.2,ISO7816标准;");//,43
}

//------------	六、	密码键盘芯片及型号
int GetPEDChipInfo(uchar * GetInfoDataIn)
{
	return	sprintf(GetInfoDataIn,"密码键盘.型号=内置PED;密码键盘.芯片型号=BCM5892安全芯片;");//,30);
}

//------------	七、	Modem主芯片型号、通讯速率
int GetModemCInfo(uchar * GetInfoDataIn)
{
	uchar szModemType[16];
	memset(szModemType,0,16);
	s_ModemInfo(NULL,szModemType,NULL,NULL);
	return	sprintf(GetInfoDataIn,"Modem.主芯片型号=%s;Modem.通讯速率=同步1200/2400/9600 bps,异步最高56Kbps;",szModemType);//,37);
}

//------------	八、	软件版本号
int GetMonitorInfo(uchar * GetInfoDataIn)
{
	return	sprintf(GetInfoDataIn,"软件版本号=\x27%s;",MonitorInfo.AppVer);//,17);
}

//------------	九、	终端是否支持双倍长密钥算法，十、	终端软件是否被篡改，是否储存磁道及PIN信息
//十一、	终端支持的应用种类
int GetPOSSupportInfo(uchar * GetInfoDataIn)
{
	return	sprintf(GetInfoDataIn,"终端是否支持双倍长密钥=是;终端软件是否被篡改,是否存储磁道及PIN信息=否;");
//终端支持的应用种类.外卡收单=支持;终端支持的应用种类\
//.EMV应用=支持;终端支持的应用种类.PBOC借贷记应用=支持;终端支持的应用种类.小额支付应用=支持;终端支持的应用种类.磁条卡应用=支持;\
//终端支持的应用种类.其他行业卡应用=支持;");//,17);
}

int GetPOSAppInfo(uchar * GetInfoDataIn)
{
	uchar szAppName[24 * 48];
	int iAppCnt,iRetLen,iLen;
	APPINFO stAppInfo;
	memset(szAppName,0,24*32);
	iRetLen = sprintf(GetInfoDataIn,"%s=","终端所有的应用");
	for (iAppCnt = 0; iAppCnt < 24; iAppCnt++)
	{
		if(ReadAppInfo(iAppCnt,&stAppInfo) == 0)
		{
			iLen = sprintf(szAppName,"%s,",stAppInfo.AppName);
			memcpy(GetInfoDataIn + iRetLen,szAppName,iLen);
			iRetLen +=iLen;
		}
	}
	return iRetLen;
}

//发送终端信息到PC
void SendPOSInfo()
{
	uchar szSendBuf[8096];
	int iGetLen = 0;
	memset(szSendBuf,0,8096);
	
    ScrCls();
    SCR_PRINT(0,0,0x81,"   上送POS信息  ", "  SEND POS INFO ");
	
	if(PortOpen(COM1, "115200,8,n,1"))
	{
        ScrGotoxy(0, 5);
		SCR_PRINT(0,5,0x01,"    发送失败    ", "   SEND FAIL!   ");
		getkey();
	}

	iGetLen += GetCPUInfo(szSendBuf);
	iGetLen += GetMemoryInfo(szSendBuf + iGetLen);
	iGetLen += GetMagReadInfo(szSendBuf + iGetLen);
	iGetLen += GetPrinterInfo(szSendBuf + iGetLen);
	iGetLen += GetICCReadInfo(szSendBuf + iGetLen);
	iGetLen += GetPEDChipInfo(szSendBuf + iGetLen);
	iGetLen += GetModemCInfo(szSendBuf + iGetLen);
	iGetLen += GetMonitorInfo(szSendBuf + iGetLen);
	iGetLen += GetPOSSupportInfo(szSendBuf + iGetLen);
	iGetLen += GetPOSAppInfo(szSendBuf + iGetLen);
	
    ScrAttrSet(0);
	if(PortSends(COM1,szSendBuf,iGetLen))
	{
		SCR_PRINT(0,5,0x01,"    发送失败    ", "   SEND FAIL!   ");
	}
	else
	{
		SCR_PRINT(0,5,0x01,"    发送成功    ", "   SEND OK!   ");
	}
	getkey();
		//SendResp(GET_MODULE_INFO,iGetLen);
}

void PrnTermInfo(void)
{
	uchar ucRet,ucNandSpace;//,ucX,ucY
	uchar szSN[33],szExSN[33],szTUSN[64],szVer[9],DispMAIN[40],DispEXTB[30],DispMAGB[30],DispNAND[30],DispPORT[30],pedver[16];
	uchar szMacAddr[7],szModemVer1[50],szModemVer2[50],szICCVers[64],TermName[16],buff[32];
	FS_DISK_INFO disk_info;
	R_RSA_PUBLIC_KEY PubKey;
	uchar ip_ver[30];
	uchar WlnetInfo[128], aucImei[16];

	int iRet = 0;
	uchar TempBuff[40];
    tSignFileInfo siginfo;	
    char context[32];
	
	memset(aucImei,0x00,sizeof(aucImei));
    memset(pedver,0x00,sizeof(pedver));
    
	ScrCls();
	SCR_PRINT(0,0,0xc1,"打印终端信息","PRN POS INFO");
	
	if((ucRet = PrnInit()) != PRN_OK)
	{
		DISPERRINFO(DO_PRINTER,ucRet);
		return;
	}

	SCR_PRINT(0,4,0x40|0x01,"打印中...","Printing..."); 
	PrnFontSet(6,6);
	
	PrnStr("***********************\n");
	if (bIsChnFont) PrnStr("***** 终 端 信 息 *****\n");
	else PrnStr("**** TERMINAL INFO ****\n");
	PrnStr("***********************\n");

	memset(TermName, 0x00, sizeof(TermName));
	get_term_name(TermName);
	if (bIsChnFont) PrnStr("终端名称: %s\n",TermName);
	else PrnStr("NAME : %s\n",TermName);
	
	//-----version info--------
	//SN信息
	memset(szSN,0,sizeof(szSN));
	memset(szExSN,0,sizeof(szExSN));
	memset(szTUSN,0,sizeof(szTUSN));
	ReadSN(szSN);
	EXReadSN(szExSN);
	ReadTUSN(szTUSN,64); 
	if (szSN[0] == 0x00) PrnStr("SN   : NULL\n");
	else PrnStr("SN   : %s\n",szSN);
	if (szExSN[0] == 0x00) PrnStr("EXSN : NULL\n\n");
	else PrnStr("EXSN : %s\n\n",szExSN);

	if (szTUSN[0] == 0x00)
		PrnStr("TUSN : NULL\n\n");
	else
		PrnStr("TUSN : %s\n\n",szTUSN);
	//固件版本
	if (bIsChnFont) PrnStr("固件信息\n");
	else PrnStr("FIRMWARE INFO\n");

    PrnStr("BIOS   : %02d[C%C]\n",GetBiosVer(),(s_GetFwDebugStatus() == 1? 'D':'R'));
	PrnStr("MONITOR: %s[%c]\n", MonitorInfo.AppVer,(CheckIfDevelopPos() == 1? 'D':'R'));

    if(s_BaseSoLoader())
    {
        SO_INFO soInfo;
        memset(&soInfo,0,sizeof(soInfo));
        if(GetSoInfo("PAXBASE.SO",&soInfo)>0)
            PrnStr("BaseSo : %s\n",soInfo.head.version);
    }
    
    GetPciVer(pedver);
	PrnStr("PCI PED: %s\n",pedver);
	
	if (s_GetPuK(ID_UA_PUK,&PubKey,&siginfo) < 0)
	   PrnStr("UA_PUK : [N]\n");
	else
	   PrnStr("UA_PUK : [Y]\n");

	if (s_GetPuK(ID_US_PUK,&PubKey,&siginfo) < 0)
	   PrnStr("US_PUK : [N]\n\n");
	else
	{
	    PrnStr("US_PUK%d : [Y]\n", GetCurPukLevel());
        if(siginfo.ucHeader ==SIGNFORMAT1)
        {
            memset(buff, 0x00, sizeof(buff));
            if (s_CheckVanstoneOEM() && !strcmp(siginfo.owner, "PAX")) strcpy(buff, "Vanstone");
            else strcpy(buff, siginfo.owner);
                                                          
            PrnStr("Owner:%s\n", buff);
            PrnStr("DigestTime:%c%c/%c%c/20%c%c\n",
                    (siginfo.aucDigestTime[1]>>4) + 0x30,(siginfo.aucDigestTime[1]&0x0f) + 0x30,
                    (siginfo.aucDigestTime[2]>>4) + 0x30, (siginfo.aucDigestTime[2]&0x0f) + 0x30,
                    (siginfo.aucDigestTime[0]>>4) + 0x30, (siginfo.aucDigestTime[0]&0x0f) + 0x30);
        
        }
	    PrnStr("\n");
	}    
	
	if (bIsChnFont) PrnStr("硬件信息\n");
	else PrnStr("HARDWARE INFO\n");

	PrnStr("CPU  : ARM11 32BIT\n");
	PrnStr("DDR  : %dMB\n", CheckSDRAMSize());
	ReadVerInfo(szVer);
	//主版版本
	if(szVer[3] == 0xff) sprintf((uchar *)DispMAIN,"%s", "MAIN : **");
	else sprintf((uchar *)DispMAIN, "MAIN : %02x", szVer[3]);

	//接口板版本
	if(szVer[4] == 0xff) sprintf((uchar *)DispPORT,"%s", "PORT : **");
	else sprintf((uchar *)DispPORT, "PORT : %02x", szVer[4]);

	//磁头版本
	if(0==GetMsrType()) sprintf((uchar *)DispMAGB,"%s", "MAGB : **");
	else sprintf((uchar *)DispMAGB, "MAGB : %02x", szVer[6]);

	//扩展版信息
	if (szVer[5] == 0xff)		sprintf(DispEXTB,"%s","EXTB : **"); 	
	else if(is_gprs_module()) 	sprintf(DispEXTB, "GPRS : %02x",szVer[5]);
	else if(is_cdma_module()) 	sprintf(DispEXTB, "CDMA : %02x",szVer[5]);
	else if(is_wcdma_module()) 	sprintf(DispEXTB, "WCDMA : %02x",szVer[5]);

	PrnStr("%s\n%s\n%s\n%s\n", DispMAIN, DispPORT, DispMAGB, DispEXTB);

	//----WNet Version
	if(GetWlType())
	{
	    WlGetModuleInfo(WlnetInfo, aucImei);
		PrnStr("WNET : %s\n",WlnetInfo);		
		if(is_cdma_module()) {
			if (s_CdmaIsMeid()==1)
				PrnStr("MEID : %s\n",aucImei);
			else
				PrnStr("IMSI : %s\n",aucImei);
		}
	}
	
	memset(szMacAddr,0,7);
	if (GetLanInfo(NULL,szMacAddr) == 0xff)
	{
		PrnStr("LAN  : [N]\n");//0xff表示没有改模块
	}
	else
	{
		PrnStr("LAN  : [Y]\n");//0xff表示没有改模块
		PrnStr("MAC  : %02x%02x%02x%02x%02x%02x\n",szMacAddr[0],szMacAddr[1],szMacAddr[2],szMacAddr[3],szMacAddr[4],szMacAddr[5]);
	}	

	memset(ip_ver,0,30);
	NetGetVersion_std(ip_ver);

	PrnStr("IP   : %s\n",ip_ver);

	if (is_wifi_module()) PrnStr("WIFI : [Y]\n");
	else PrnStr("WIFI : [N]\n");
	if (is_bt_module()) PrnStr("BT   : [Y]\n");
	else PrnStr("BT   : [N]\n");
	if (get_rftype() == 0) PrnStr("RF   : [N]\n");
	else PrnStr("RF   : [Y]\n");

	//----Modem Version
	memset(szModemVer2, 0x00, sizeof(szModemVer2) );
	GetModemInfo(szModemVer1,szModemVer2);
	if (szModemVer2[0])PrnStr("MODEM: %s %s\n",szModemVer1,szModemVer2);
	else PrnStr("MODEM: [N]\n");
	
	//----WNet Version
	memset(szICCVers,0,30);
	SciGetVer(szICCVers);		
	PrnStr("ICC  : %s\n",szICCVers);
	//----PED Version
	{
		uchar szPEDVers[256];
		if (!PedGetLibVer(szPEDVers))
		{
			PrnStr("PED  : %s\n",szPEDVers);
		}
		else
		{
			PrnStr("PED  : N/A\n");
		}
	}

	PrnStr("FLASH: %ldMB\n",CheckFlashSize());//freesize();
	PrnStr("       %ldKB[FREE]\n",freesize()/1024);//freesize();
	
	//check font info
	if (bIsChnFont) PrnStr("字库信息\n");
	else    PrnStr("FONT LIB INFO\n");
	PrnFontInfo();

	 //座机相关信息
    if (is_hasbase())
    {
    	GetBaseVer();
        if (bIsChnFont) PrnStr("\n座机信息\n");
        else    PrnStr("\nBASE INFORMATION\n");
        if(base_info.ver_exboard!=0xff)
            PrnStr("B_HV   : %02d  EXTB : %02X\n",base_info.ver_hard,base_info.ver_exboard);
        else
            PrnStr("B_HV   : %02d  EXTB : **\n",base_info.ver_hard);
        if(base_info.ver_boot)   
            PrnStr("B_BV   : %02d  B_SV : %s\n",base_info.ver_boot,base_info.ver_soft);
        else    
            PrnStr("B_BV   : **  B_SV : %s\n",base_info.ver_soft);
        
        PrnStr("FLASH: %dMB",base_info.flash_size);
        PrnStr("\nSDRAM: %dMB\n",base_info.sdram_size);     
             
    }

	PrnStr("\n\n\n\n\n\n\n\n\n\n");

	if ((ucRet = PrnStart()) != PRN_OK)
	{
		DISPERRINFO(DO_PRINTER,ucRet);
		return;
	}	
}

int PrnUdiskDLInfo(uchar *context, int len)/*打印U盘下载日志*/
{
    uchar ucRet, tmpbuf[2048];
    int i, n;

    if (len<=0) return 0;
	if((ucRet = PrnInit()) != PRN_OK)
	{
		DISPERRINFO(DO_PRINTER,ucRet);
		return ucRet;
	}	
	PrnFontSet(6,6);
	PrnStr("**** Udisk Load Log ****\n");
    i = 0;
	while (1)
	{
	    if (len<=0) break;
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        if (len > (sizeof(tmpbuf)-1)) n = sizeof(tmpbuf)-1;
        else n = len;
        memcpy(tmpbuf, context+i, n);
        PrnStr("%s", tmpbuf);
        len -= n;
        i += n;
	}
	PrnStr("\n\n\n\n\n");
	if ((ucRet = PrnStart()) != PRN_OK)
	{
		DISPERRINFO(DO_PRINTER,ucRet);
		return ucRet;
	}

	return 0;
}

void LocalDload(void)
{
	iUartDownloadMain(1);
	if(0 == FsUdiskIn())
	{
		UDiscDload();
		PortClose(P_USB_HOST);
	}
}

void VoiceAdjust(void)
{
	char volume;
	uchar key;

	volume = SysGetAudioVolume();
	if(volume < 0) volume = DEFAULT_AUDIO_VOLUME;

	kbflush();
	while(1)
	{
		ScrClrLine(2,7);
        if(bIsChnFont)
        {
            ScrPrint(0,3,1,"语音播放[0-200%%]");
            ScrPrint(0,6,0x41,"音量 = %d%%",volume*200/MAX_AUDIO_VOLUME);
        }
        else
        {
            ScrPrint(0,3,1,"VOICE   [0-200%%]");
            ScrPrint(0,6,0x41,"VOLUME = %d%%",volume*200/MAX_AUDIO_VOLUME);
        }
        
		key = getkey();
		if(key == KEYUP) 
		{
			if(volume < MAX_AUDIO_VOLUME) volume++;
		}
		else if(key == KEYDOWN)
		{
			if(volume > MIN_AUDIO_VOLUME) volume--;
		}
        else if(key == KEYCANCEL)break;
		else if(key==KEYENTER)
		{
            if(SysGetAudioVolume() == volume) break;
            ScrClrLine(3, 4);
            if(SysSetAudioVolume(volume))
            {
                if(bIsChnFont) ScrPrint(0,3,0x41,"保存失败!");
                else ScrPrint(0,3,0x41,"Save Fail!");
            }
            else
            {
                if(bIsChnFont) ScrPrint(0,3,0x41,"保存成功");
                else ScrPrint(0,3,0x41,"Save Success");
            }
            WaitKeyBExit(3,0);
			break;
		}
	}
}

void CopySW(void)
{   
    PortClose(P_USB_DEV);
	iDupDownloadMain(1);
	PortClose(P_USB_HOST);
	PortClose(P_BASE_HOST);
	PortOpen(P_USB_DEV, NULL);
}

void PEDManage()
{
	vPedDownloadMenu();
}

uchar ShowCfgInfo(int type)
{
	char keyword[33],context[33],buff[33];
	int i,j,total_num,str_len;
	int page;
	int i_per_page[20];
	uchar key;
	total_num =CfgTotalNum();
#define CFG_START_LINE 2
	
	
	i=0;
	page=0;
	i_per_page[0]=0;
	key = KEYDOWN;

RE_DISP_LAST_PAGE:
	j=CFG_START_LINE;
	for(i=i_per_page[page];i<total_num;i++)
	{
		if(CFG_START_LINE==j) i_per_page[page]=i;
		
		if(CfgGet(i,type,keyword,context)<0) continue;
        if(!strcmp(keyword,"SEN_PARA"))continue;
        if(!strcmp(keyword,"TERMINAL_NAME") && (ReadCfgInfo("C_TERMINAL",buff)>0))continue;
        if(!strcmp(keyword,"PN") && (ReadCfgInfo("C_PN",buff)>0))continue;

		str_len=strlen(keyword)+strlen(context);
		if(str_len<=20)
		{
			ScrPrint(0,j,0,"%s:%s",keyword,context);
			j++;
		}
		else
		{
			ScrPrint(0,j,0,"%s:",keyword);
			ScrPrint(0,j+1,0,"%s",context);  //内容行从后往前显示
			j+=2;
		}
		
		if(j>6)
		{
			key=getkey();
			ScrClrLine(2,7);
			j=CFG_START_LINE;        //满页后，从起始行开始显示
			if(KEYUP ==key || KEYF1==key)
			{
				page--;
				if(page<0)return key;
				goto RE_DISP_LAST_PAGE;
			}
			else if(KEYMENU ==key ||KEYCLEAR ==key || KEYCANCEL == key)	return key;
			else		++page;
		}

	}
	if(CFG_START_LINE==j)
	{	
		if(i_per_page[0]==total_num-1)
			return getkey();

		return key;
		//return KEYDOWN; //最后一页已经等待了按键时，不再等待按键	

	}	
	key=getkey();
	if(KEYUP ==key || KEYF1==key)
	{
		page--;
		if(page<0) return key;
		ScrClrLine(2,7);
		goto RE_DISP_LAST_PAGE;
	}
	return key;
}

void ShowHardWareInfo()
{
	int block_no = 0;
	uchar key;
	uchar sel;
UP_MENU:
	block_no = 0;
	sel = KEY1;
	while(is_hasbase())
	{	
		ScrCls();
		SCR_PRINT(0, 0, 0xC1, "硬件配置信息", "HW CFG INFO");		
		SCR_PRINT(0, 3, 0x41, "1-手机配置信息", "1-Hand CFG INFO");
		SCR_PRINT(0, 5, 0x41, "2-座机配置信息", "2-Base CFG INFO");
		key=getkey();
		if (KEYMENU == key || KEYCANCEL == key) return;
		if(key == KEY1 || key == KEY2 )
		{
			sel = key;	
			break;
		}			
	}
	while(1)
	{
		ScrCls();
		switch(block_no)
		{
			case 0:
				SCR_PRINT(0, 0, 0xc1, "产品信息", "PRODUCT INFO");
				if(sel==KEY1)
					key = ShowCfgInfo(OTHER_TYPE);
				else
					key = ShowbaseCfgInfo(OTHER_TYPE);
				break;
			
			case 1:
				SCR_PRINT(0, 0, 0xc1, "配置信息", "CFG INFO");
				if(sel==KEY1)
					key = ShowCfgInfo(CFG_TYPE);
				else 
					key = ShowbaseCfgInfo(CFG_TYPE);
				break;
			case 2:
				SCR_PRINT(0, 0, 0xc1, "PCB版本", "PCB VER INFO ");
				if(sel==KEY1)
					key = ShowCfgInfo(BOARD_TYPE);
				else
					key = ShowbaseCfgInfo(BOARD_TYPE);
				break;
			case 3:
				SCR_PRINT(0, 0, 0xc1, "参数信息", "PARA INFO");
				if(sel==KEY1)
					key = ShowCfgInfo(PARA_TYPE);
				else
					key = ShowbaseCfgInfo(PARA_TYPE);
				break;
		}
		if(is_hasbase() && KEYCANCEL == key)  goto UP_MENU;
		if (KEYMENU == key || KEYCANCEL == key) return;
		if(key==KEYUP || key==KEYF1) block_no = (block_no == 0? 3 : (block_no-1));
		else if(key==KEYDOWN || key==KEYENTER || key==KEYF2) block_no = (block_no == 3? 0 : (block_no+1));
	}
}

extern void SetHardWareInfo();
void HardWareInfo()
{
	uchar key;
	while(1)
	{
		ScrClrLine(2,7);
		SCR_PRINT(0, 2, 0x01, "1-查看信息", "1-Display INFO");		
		if(s_GetBootSecLevel()!=0)SCR_PRINT(0, 4, 0x01, "2-修改信息", "2-Modify INFO");
        key = getkey();
        switch(key)
        {
            case KEY1:
                ShowHardWareInfo();
            return;
            case KEY2:
                if(s_GetBootSecLevel()==0) break;
                SetHardWareInfo();
            return;
            case KEYCANCEL:
            case KEYENTER:
                return;
            break;
            default:
            break;
        }
	}
}

const struct T_FUN_TABLE tFunModuleCheckMenu[] =
{
	MENU_ALL,"ICC阅读器",   "ICC READER",   McCheckICC,
	MENU_ALL,"磁卡阅读器",  "MAG READER",   McCheckMag,
	MENU_ALL,"RF阅读器",    "RF READER",    McCheckRF,
	MENU_ALL,"键盘",        "KEYBOARD",     McCheckKB,
	MENU_ALL,"屏幕",        "LCD",          McCheckLCD,	
	MENU_ALL,"MODEM",       "MODEM",        McCheckModem,
	MENU_ALL,"以太网",      "ETHERNET",     McCheckETH,
	MENU_ALL,"无线",        "WNET",         McCheckWNET
};


void ModuleCheck()
{
	int type,nums,i,tmpd,w,h;
	uchar ucPage = 0,lastPage=0xff,ucMaxMenu,ucMaxPage,ucItemOfPage,ucGetKey;//选择第几页的第几个
    struct T_FUN_TABLE tMenu[MENU_MAX_ITEMS];

    nums = sizeof(tFunModuleCheckMenu)/sizeof(tFunModuleCheckMenu[0]);
    if(nums>MENU_MAX_ITEMS)nums = MENU_MAX_ITEMS;
    
    type = get_machine_type();
    
    for(i=0,ucMaxMenu=0;i<nums;i++)
    {
        if(tFunModuleCheckMenu[i].ucFunIdx&MENU_SUB(type))
            tMenu[ucMaxMenu++]=tFunModuleCheckMenu[i];
    }
    ScrGetLcdSize(&w, &h);
    ucItemOfPage = h/16-1;//减去标题行
    if(ucItemOfPage>9)ucItemOfPage=9;
    
	ucMaxPage = (ucMaxMenu+ucItemOfPage-1)/ucItemOfPage;
    if(ucMaxPage)ucMaxPage-=1;
    
MAIN_MENU:
	ScrSetIcon(ICON_UP, OPENICON);
	ScrSetIcon(ICON_DOWN, OPENICON);
	ScrCls();
	SetLineRever(0,CFONT);		
	SCR_PRINT(0, 0, 0xC1, "模块诊断", "MODULE CHECK");
    lastPage=0xff;
	while (1)
	{
		kbflush();

        if(lastPage!=ucPage)
        {
    		ListFun((struct T_FUN_TABLE *)tMenu,ucMaxMenu,ucPage,ucItemOfPage);//显示第几页，第几个被选中
            lastPage = ucPage;
        }
        
    	ucGetKey = getkey();

		switch(ucGetKey)
		{
		case KEYF1:
		case KEYUP:
            
			if (ucPage)ucPage--;
			else ucPage = ucMaxPage; 
            
			break;
		case KEYF2:
		case KEYDOWN:
            
			if (ucPage >= ucMaxPage) ucPage = 0;
			else ucPage++;
            
			break;
            
		case KEYCANCEL:

            ScrSetIcon(ICON_UP, CLOSEICON);
            ScrSetIcon(ICON_DOWN, CLOSEICON);
            ScrCls();
			return;
            
		default:

            if(ucGetKey>KEY0 && ucGetKey<=KEY9)
            {
    			RunFun((struct T_FUN_TABLE *)tMenu,ucMaxMenu,ucPage,ucItemOfPage,ucGetKey - KEY1);
        		goto MAIN_MENU;
            }

			break;
		}
	}
}

/*针式打印机右边界调整*/
void PrnAdjPrnRightT(void)
{
	unsigned char key = 0;
	char buf[150] = {0};
	char hysteresis_error = 0;
    char max_hysteresis_error;

    max_hysteresis_error = get_prn_max_hysteresis();
    hysteresis_error = get_prn_hysteresis();

	ScrSetIcon(ICON_UP,   CLOSEICON);
	ScrSetIcon(ICON_DOWN, CLOSEICON); 
	memset(buf, 0x00, sizeof(buf));
	memset(buf, 'H', 30*4);    
	while(1)
	{
		if(hysteresis_error > 0)
			ScrPrint(0, 2, 0x41, "MOV RIGHT %d DOT", hysteresis_error);
		else if(hysteresis_error == 0)
			ScrPrint(0, 2, 0x41, "MIDDLE %d", hysteresis_error);
		else
			ScrPrint(0, 2, 0x41, "MOV LEFT %d DOT",0 - hysteresis_error);

		ScrFontSet(1);
		ScrClrLine(4, 7);
		ScrGotoxy(24, 4);              
		Lcdprintf("HHHHHHHHHH");
		ScrGotoxy(24+hysteresis_error, 6);
		Lcdprintf("HHHHHHHHHH");
		key = getkey();
		switch(key)
		{
			case KEYUP:
				if(hysteresis_error < max_hysteresis_error)
					hysteresis_error++;
				break;
                
			case KEYDOWN:
				if(hysteresis_error > (-max_hysteresis_error))
					hysteresis_error--;
				break;
                
			case KEYENTER:
				set_prn_hysteresis(hysteresis_error);
				PrnInit();
				PrnStr("%s\n\n\n\n", buf);
				PrnStart();
				break;

			case KEYCANCEL:
				return;
			default:
				break;
		}
	}
}

extern void WlModelComUpdate(void);
extern void ScanUpdate(void);
struct T_FUN_TABLE tFunMdUpdateMenu[] =
{
	MENU_ALL,"无线模块","Wireless",WlModelComUpdate,
	MENU_ALL,"WIFI模块","Wifi", WifiUpdate,
	MENU_ALL,"扫描头模块", "Scan", ScanUpdate,
};

void ModuleUpdate(void)
{    	
	ScrSetIcon(ICON_UP, OPENICON);
	ScrSetIcon(ICON_DOWN, OPENICON);
	while (1)
	{
		ScrCls();
		kbflush();
		ListFun((struct T_FUN_TABLE *)tFunMdUpdateMenu,3,0,3);//显示第几页，第几个被选中
		SetLineRever(0,CFONT);		
        SCR_PRINT(0,0,0xC1,"    模块升级 ","  MODULE UPDATE ");		

		switch(getkey())
		{
		case KEY1:
			RunFun((struct T_FUN_TABLE *)tFunMdUpdateMenu,3,0,3,0);
			break;
		case KEY2:
			RunFun((struct T_FUN_TABLE *)tFunMdUpdateMenu,3,0,3,1);
			break;
		case KEY3:
			RunFun((struct T_FUN_TABLE *)tFunMdUpdateMenu,3,0,3,2);
			break;
		case KEYCANCEL:
			return;
		default:
			break;
		}
	}
}

void AdjustModemLevel(void)
{
    uchar   key;
    uchar   logo[256], Dispbuf_EN[128], Dispbuf_CH[128], value[3] = {0};
    int ret, level;
    
	ret = SysParaRead(MODEM_LEVEL_INFO, value);
	if (ret<0 || !value[0]) level = 10;
	else level = value[1]-'0';

    kbflush();
	while(1)
    {        
        ScrClrLine(2,7);
        sprintf(Dispbuf_CH, "      手动      ");
        sprintf(Dispbuf_EN, "    Customize   ");
        SCR_PRINT(0, 2, 0x01, Dispbuf_CH, Dispbuf_EN);
        
        sprintf(Dispbuf_CH, "弱 --(%02d)--> 强 ", level);
        sprintf(Dispbuf_EN, " Weak(%02d)Strong ", level);
        SCR_PRINT(0, 6, 0x01,Dispbuf_CH, Dispbuf_EN);
        memset(logo, 0, sizeof(logo));
        logo[0] = 1;
        logo[1] = 0;
        logo[2] = 128;
        if((level >= 1) && (level <= 15))
        {
             memset(logo+3, 0xff, (15-level)*8);
        }
        ScrGotoxy(8, 4);
        ScrDrLogo(logo);
        ScrGotoxy(8, 5);
        ScrDrLogo(logo);    
        key = getkey();
        if(key == KEYCANCEL) return;
        if(key == KEYENTER)
        {
            ret = SysParaRead(MODEM_LEVEL_INFO, value);
			if((value[1]-'0') != level)
			{
				value[0] = 1;
				value[1] = level+'0';
                ret = SysParaWrite(MODEM_LEVEL_INFO, value);
                
				ScrClrLine(2, 7);
	            if(ret < 0)
	            {
                    if(bIsChnFont)
                        ScrPrint(0,3,0x41,"保存 %2d 失败!", level);
                    else
                        ScrPrint(0,3,0x41,"Save %2d Fail!", level);
	            }
				else 
				{
                    if(bIsChnFont)
                        ScrPrint(0,3,0x41,"保存 %2d 成功", level);
                    else
                        ScrPrint(0,3,0x41,"Save %2d Success", level);
					if(is_modem_module())
						mdm_adjust_tx_level(value[0], value[1]-'0');					
				}
				WaitKeyBExit(3,0);
			}
			return;
        }
		if((key != KEYUP) && (key != KEYDOWN)) continue;

        if(key == KEYDOWN)
        {
			if (level >= 15) level = 15;
			else level++;
        }
        else if(key == KEYUP)
        {
			if (level <= 1) level = 1;
            else level--;
        }
    }
}

void SetModemLevel(void)
{
    uchar key = NOKEY, value[3]={0}, flag = 0;
    int ret = 0;

    ScrCls();  
    SetLineRever(0,CFONT);  
    SCR_PRINT(0, 0, 0x81, "  设置MODEM电平 ", "   Modem Level  ");
    while(1)
    {
        flag = 1;
        ret = SysParaRead(MODEM_LEVEL_INFO, value);
        if ((ret == -1) || !value[0])
        {
            flag = 0;
            value[1] = 10+'0'; 
        }    

        ScrClrLine(2, 7);
        SCR_PRINT(0, 3, flag ? 0x01:0x81, "1.默认          ", "1.Default       ");
        if(bIsChnFont) ScrPrint(0, 6, flag ? 0x81:0x01, "2.手动 -%ddBm   ", value[1]-'0');
        else           ScrPrint(0, 6, flag ? 0x81:0x01, "2.Customize -%d ", value[1]-'0');

        while(2)
        {
            kbflush();
            key = getkey();
            if (key==KEYCANCEL) return;
            if ((key!=KEY1) && (key!=KEY2) && (key!=KEYENTER)) 
                continue;
            if (key == KEY2 || (key==KEYENTER && flag)) 
            {
                AdjustModemLevel();/*手动设置*/
            }
            else
            {
                value[0] = 0;/*使用APP的电平设置*/
                SysParaWrite(MODEM_LEVEL_INFO, value);
                if(is_modem_module())
                	mdm_adjust_tx_level(value[0], value[1]-'0');               
            }
			if(is_hasbase())
			{
				//--refresh BASESET's tx_level
	         	s_ModemInit_Proxy(0);//Added on 2015.12.4
			}
            break;
        }
    }
}

void SetMdmAnswerTone(void)
{
    uchar key = NOKEY, value[20], flag = 0, buf[20]={0}, ucret;
    int ret = 0;    

    ScrCls();  
    SetLineRever(0,CFONT);  
    SCR_PRINT(0, 0, 0x81, "  应答音门限值 ", "   Answer Tone  ");

    while(1)
    {
        flag = 1;
        memset(value, 0x00, sizeof(value));
        ret = SysParaRead(MODEM_ANSWER_TONE_INFO, value);
        if ((ret == -1) || !value[0])
        {
            flag = 0;
            value[1] = '2';
            value[2] = '0';
            value[3] = 0;
        }    
        kbflush();
        ScrClrLine(2, 7);
        SCR_PRINT(0, 3, flag ? 0x01:0x81, "1.默认          ", "1.Default       ");
        if(bIsChnFont) ScrPrint(0, 6, flag ? 0x81:0x01, "2.手动 %s      ", value+1);
        else           ScrPrint(0, 6, flag ? 0x81:0x01, "2.Customize %s ", value+1);

        while(2)
        {
            key = getkey();
            if (key==KEYCANCEL) return;
            if ((key!=KEY1) && (key!=KEY2) && (key!=KEYENTER)) continue;
            if (key == KEY2 || (key==KEYENTER && flag))/*手动设置*/
            {
                while(3)
                {
                    ScrClrLine(2, 7);
                    SCR_PRINT(0, 2, 0x01, " 手动(单位:10ms)", " Redefine(10ms) ");
                    SCR_PRINT(0, 4, 0x01, "输入(1~255)", "Input(1~255)");
                    ScrGotoxy(0,6);
                    strcpy(buf, value+1);
                    ucret = GetString(buf,0xe5,1,3);
                    if (ucret == 0xff) break;/*cancle*/
                    if (ucret) continue;
                    buf[buf[0]+1]=0;
                    if (atol(buf+1) < 1 || atol(buf+1) > 0xff) continue;                
                    strcpy(value+1, buf+1);
                    
    				value[0] = 1;
                    ret = SysParaWrite(MODEM_ANSWER_TONE_INFO, value);
                    ScrClrLine(2, 7);
                    if(ret < 0)
    	            {
                        if(bIsChnFont)
                            ScrPrint(0,3,0x41,"保存 %s 失败!", value+1);
                        else
                            ScrPrint(0,3,0x41,"Save %s Fail!", value+1);
    	            }
    				else 
    				{
                        if(bIsChnFont)
                            ScrPrint(0,3,0x41,"保存 %s 成功", value+1);
                        else
            				ScrPrint(0,3,0x41,"Save %s Success", value+1);
						if(is_modem_module())
        					mdm_adjust_answer_tone(value[0], atol(value+1));                       
        				WaitKeyBExit(3,0);
        				break;
        			}	
                }
            }    
            else
            {
                value[0] = 0;/*default*/
                SysParaWrite(MODEM_ANSWER_TONE_INFO, value);
				if(is_modem_module())
                	mdm_adjust_answer_tone(value[0], atol(value+1));                
            }
			if(is_hasbase())
			{
				//--refresh BASESET's answer_tone
				s_ModemInit_Proxy(0);//Added on 2015.12.4
			}
            break;
        }
    }
}
void SetNetworkMode(void)
{
	uchar key = NOKEY, value[3]={0}, flag = 0;
	int ret = 0;

    ret = SysParaRead(SET_NETWORK_MODE, value); 
    if ((ret == -1) || !value[0]) strcpy(value,"8");

	while(1)
	{		
		flag = value[0] -'0';
		kbflush();
        ScrCls(); 
		SCR_PRINT(0, 0, 0x81, "    设置网络    ", "   Set Net Mode ");
		SCR_PRINT(0, 3, (flag==8)? 0x81:0x01, "1.混合模式      ", "1.HYBRID mode   ");
		SCR_PRINT(0, 6, (flag==2)? 0x81:0x01, "2.CDMA 模式     ", "2.CDMA mode     ");

		key = getkey();
		switch(key)
		{
			case KEYCANCEL:
				return;
            case KEYENTER:
                SysParaWrite(SET_NETWORK_MODE, value);
                return;
			case KEY1:
				strcpy(value,"8");
				break;
			case KEY2:
				strcpy(value,"2");
				break;
		}
    }
}
extern int WlGprsChapPara(int *val, int op);
void SetGPRSCHAP()
{
	uchar key = NOKEY;
	int ret = 0;
	int val = 0;
	ret = WlGprsChapPara(&val, 0);
   	if (ret!=0)  
	{		
		kbflush();
		ScrCls();
		SCR_PRINT(0, 3, 0x01, "    不支持      ", "   Not Support  ");
		key=getkey();
		return;
   	}
	while(1)
	{
		kbflush();
        ScrCls(); 
		SCR_PRINT(0, 0, 0x81, "  设置GPRS-CHAP ", " Set GPRS-CHAP  ");
		SCR_PRINT(0, 3, (val== 8)? 0x81:0x01, "1.CHAP-8        ", "1.CHAP-8        ");
		SCR_PRINT(0, 6, (val==16)? 0x81:0x01, "2.CHAP-16       ", "2.CHAP-16       ");
		key = getkey();
		switch(key)
		{
			case KEYCANCEL:
				return;
            case KEYENTER:
                ret=WlGprsChapPara(&val, 1);
                return;
			case KEY1:
				val=8;
				break;
			case KEY2:
				val=16;
				break;				
		}
    }
}

void SetRelayMode(void)
{
	uchar key = NOKEY, value[3]={0}, flag = 0;
	int ret = 0;

	ret = SysParaRead(SET_RELAY_MODE, value); 
	if ((ret == -1) || !value[0]) strcpy(value,"1");

	while(1)
	{		
		flag = value[0] -'0';
		kbflush();
        ScrCls(); 
		SCR_PRINT(0, 0, 0x81, "    设置模式    ", "   Set RelayMode");
		SCR_PRINT(0, 3, (flag==1)? 0x81:0x01, "1.中继模式      ", "1.Repeat mode   ");
		SCR_PRINT(0, 6, (flag==2)? 0x81:0x01, "2.网络模式      ", "2.Network mode  ");

		key = getkey();
		switch(key)
		{
			case KEYCANCEL:
				return;
            case KEYENTER:
                SysParaWrite(SET_RELAY_MODE, value);
                return;
			case KEY1:
				strcpy(value,"1");
				break;
			case KEY2:
				strcpy(value,"2");
				break;
		}
    }
}

struct T_FUN_TABLE tFunMdSettingMenu[] =
{
	MENU_ALL,"无线网络模式","Network Mode",SetNetworkMode,
	MENU_ALL,"无线中继模式","Relay Mode",SetRelayMode,
	MENU_ALL,"GPRS-CHAP设置","Set GPRS-CHAP",SetGPRSCHAP,
	MENU_SUB(S800)|MENU_SUB(S500)|MENU_SUB(D210),"设置MODEM电平","Modem Level",SetModemLevel,
	MENU_SUB(S800)|MENU_SUB(S500)|MENU_SUB(D210),"应答音门限值","Answer Tone",SetMdmAnswerTone,    
};

void CommSetting()
{
	int type,nums,i,w,h;
	uchar ucPage = 0,ucMaxMenu,ucMaxPage,ucGetKey,ucItemOfPage;//选择第几页的第几个
    struct T_FUN_TABLE tMenu[MENU_MAX_ITEMS];

    nums = sizeof(tFunMdSettingMenu)/sizeof(tFunMdSettingMenu[0]);
    if(nums>MENU_MAX_ITEMS) nums = MENU_MAX_ITEMS;
    
    type = get_machine_type();    
    for(i=0,ucMaxMenu=0;i<nums;i++)
    {
    	if(SetGPRSCHAP==tFunMdSettingMenu[i].RunCaseFun) //Set GPRS-CHAP
    	{
    		if(GetWlType()!=0x11 && GetWlType()!=0x13) continue; //WNET G610,G510
    	}
        if(tFunMdSettingMenu[i].ucFunIdx&MENU_SUB(type))
            tMenu[ucMaxMenu++]=tFunMdSettingMenu[i];
    }
    ScrGetLcdSize(&w, &h);
    ucItemOfPage = h/16 -1;//减去标题行
    if(ucItemOfPage>9)ucItemOfPage=9;
    
	ucMaxPage = (ucMaxMenu+ucItemOfPage-1)/ucItemOfPage;

    if(ucMaxPage)ucMaxPage-=1;    
//	ucMaxPage = (ucMaxMenu+2)/3;
    	
	ScrSetIcon(ICON_UP, OPENICON);
	ScrSetIcon(ICON_DOWN, OPENICON);

	while (1)
	{
		ScrCls();
		kbflush();
		ListFun((struct T_FUN_TABLE *)tMenu,ucMaxMenu,ucPage,ucItemOfPage);//显示第几页，第几个被选中
		SetLineRever(0,CFONT);		
        SCR_PRINT(0,0,0xC1, "   通讯设置    ","  COMM Setting ");

		switch(ucGetKey = getkey())
		{
		case KEYUP:
		case KEYF1:
			if (ucPage)ucPage--;
	        else ucPage = ucMaxPage;
			break;
		case KEYDOWN:
		case KEYF2:
			if (ucPage >= ucMaxPage) ucPage = 0;
			else ucPage++;
			break;
		case KEYCANCEL:
			return;
		default:
			if(ucGetKey>KEY0 && ucGetKey<=KEY9)
            {
    			RunFun((struct T_FUN_TABLE *)tMenu,ucMaxMenu,ucPage,ucItemOfPage,ucGetKey - KEY1);
            }
			break;
		}
	}
}

int GetPUKSig(uchar KeyID, uchar *Sig, int *SigLen);
void ShowPukInfo()
{
    tSignFileInfo siginfo;
    int len=0;
    uchar key, buff[17];

    memset(&siginfo, 0x00, sizeof(tSignFileInfo));
    memset(buff, 0x00, sizeof(buff));
    while (1)
    {
        ScrCls();  
        SetLineRever(0,CFONT);  
        SCR_PRINT(0, 0, 0x81, "  用户公钥信息 ", "  User PUK Info ");
        ScrPrint(0,2,0,"US_PUK%d : [Y]", GetCurPukLevel()); 

        if (0==GetPUKSig(ID_US_PUK, (uchar*)&siginfo, &len) 
            && (siginfo.ucHeader==SIGNFORMAT1))
        {
            memset(buff, 0x00, sizeof(buff));
            if (s_CheckVanstoneOEM() && !strcmp(siginfo.owner, "PAX")) strcpy(buff, "Vanstone");
            else strcpy(buff, siginfo.owner);

            ScrPrint(0,3,0,"Owner:%s", buff);        

            ScrPrint(0,4,0,"DigestTime:%c%c/%c%c/20%c%c",
                   (siginfo.aucDigestTime[1]>>4) + 0x30,(siginfo.aucDigestTime[1]&0x0f) + 0x30,
                   (siginfo.aucDigestTime[2]>>4) + 0x30, (siginfo.aucDigestTime[2]&0x0f) + 0x30,
                   (siginfo.aucDigestTime[0]>>4) + 0x30, (siginfo.aucDigestTime[0]&0x0f) + 0x30);
        }   

        key = getkey();
        if(key==KEYENTER || key==KEYCANCEL) break;        
    }
}

const struct T_FUN_TABLE tFunMainMenu[] =
{
	MENU_ALL,"本地下载","Local Download",LocalDload,
	MENU_ALL,"对拷","Duplicate",CopySW,
	MENU_SUB(S300)|MENU_SUB(S900)|MENU_SUB(S500)|MENU_SUB(D210),"触屏校准","TS Calibrate",TSCalibration,
	MENU_ALL,"远程下载","RemoteDownload",RemoteDload,
	MENU_ALL,"密钥下载","Key Download",PEDManage,
    MENU_ALL,"用户公钥信息","User PUK Info",ShowPukInfo,
	MENU_ALL,"显示版本","Show Version",ShowVer,	
	MENU_ALL&(~MENU_SUB(S300)),"打印终端信息","Prn POS Info",PrnTermInfo,
	MENU_ALL,"系统时间设置","Set Time",SetSystemTime,
	MENU_SUB(S300)|MENU_SUB(S800)|MENU_SUB(S900),"语音音量调节","Voice Adjust", VoiceAdjust,
	MENU_ALL,"屏幕亮度调节","LCD Adjust",AdjustLCDGray,
	MENU_ALL,"模块诊断","Module Check",ModuleCheck,
	MENU_ALL,"硬件配置信息","HW CFG INFO",HardWareInfo,
	MENU_SUB(S78),"右边界调整","Set R-Margin", PrnAdjPrnRightT,
	MENU_ALL,"模块升级","Module Update",ModuleUpdate,
	MENU_ALL,"通讯设置","COMM Setting", CommSetting,
	MENU_SUB(S900)|MENU_SUB(S300),"扫描头复位", "Scanner Reset",ScannerReset,
	MENU_SUB(S300),"扫描头匹配", "ScannerMatching",ScannerMatching,
	
};

const struct T_FUN_TABLE tFunMainMenu_VS_OEM[] =
{
	MENU_ALL,"本地下载","Local Download",LocalDload,
	//MENU_ALL,"对拷","Duplicate",CopySW,
	MENU_SUB(S300)|MENU_SUB(S900)|MENU_SUB(S500)|MENU_SUB(D210),"触屏校准","TS Calibrate",TSCalibration,
	//MENU_ALL,"远程下载","RemoteDownload",RemoteDload,
	//MENU_ALL,"密钥下载","Key Download",PEDManage,
	MENU_ALL,"用户公钥信息","User PUK Info",ShowPukInfo,
	MENU_ALL,"显示版本","Show Version",ShowVer,
	MENU_ALL,"打印终端信息","Prn POS Info",PrnTermInfo,
	MENU_ALL,"系统时间设置","Set Time",SetSystemTime,
	MENU_SUB(S300)|MENU_SUB(S800)|MENU_SUB(S900),"语音音量调节","Voice Adjust", VoiceAdjust,
	MENU_ALL,"屏幕亮度调节","LCD Adjust",AdjustLCDGray,
	MENU_ALL,"模块诊断","Module Check",ModuleCheck,
	MENU_ALL,"硬件配置信息","HW CFG INFO",HardWareInfo,
	MENU_SUB(S78),"右边界调整","Set R-Margin", PrnAdjPrnRightT,
	MENU_ALL,"模块升级","Module Update",ModuleUpdate,
	MENU_ALL,"通讯设置","COMM Setting", CommSetting,
	MENU_SUB(S900)|MENU_SUB(S300),"扫描头复位", "Scanner Reset",ScannerReset,
	MENU_SUB(S300),"扫描头匹配", "ScannerMatching",ScannerMatching,
};

void MenuSelect(void)
{
	int iDownloadResult,type,nums,i,tmpd,w,h;
	uchar ucPage = 0,lastPage=0xff,ucMaxMenu,ucMaxPage,ucItemOfPage,ucGetKey;//选择第几页的第几个
    struct T_FUN_TABLE tMenu[MENU_MAX_ITEMS],*pt;

    if (s_CheckVanstoneOEM())
    {
        pt=(struct T_FUN_TABLE *)tFunMainMenu_VS_OEM;
        nums = sizeof(tFunMainMenu_VS_OEM)/sizeof(tFunMainMenu_VS_OEM[0]);
    }
    else
    {
        pt=(struct T_FUN_TABLE *)tFunMainMenu;
        nums = sizeof(tFunMainMenu)/sizeof(tFunMainMenu[0]);
    }

    if(nums>MENU_MAX_ITEMS)nums = MENU_MAX_ITEMS;
    
    type = get_machine_type();
    
    for(i=0,ucMaxMenu=0;i<nums;i++)
    {
        if(pt[i].ucFunIdx&MENU_SUB(type))
            tMenu[ucMaxMenu++]=pt[i];
    }

    ScrGetLcdSize(&w, &h);
    ucItemOfPage = h/16 -1;//减去标题行
    if(ucItemOfPage>9)ucItemOfPage=9;
    
	ucMaxPage = (ucMaxMenu+ucItemOfPage-1)/ucItemOfPage;
    if(ucMaxPage)ucMaxPage-=1;
    
	iPedInit();

MAIN_MENU:
	ScrSetIcon(ICON_UP, OPENICON);
	ScrSetIcon(ICON_DOWN, OPENICON);
	ScrCls();
	SetLineRever(0,CFONT);		
	SCR_PRINT(0, 0, 0x81, "      菜单      ", "      MENU      ");
    lastPage=0xff;
	while (1)
	{
		kbflush();

        if(lastPage!=ucPage)
        {
    		ListFun((struct T_FUN_TABLE *)tMenu,ucMaxMenu,ucPage,ucItemOfPage);//显示第几页，第几个被选中
            lastPage = ucPage;
        }
        
		iUartDownloadInit(&iDownloadResult);

		while(1)
        {
        	ucGetKey = 0xff;
            if(!kbhit())
            {
                ucGetKey = GetKeyMs(6000 * 3);///3分钟内没响应就直接退到应用调用的状态
                break;
            }
            else
            {
			    if(iUartDownloadShake(&iDownloadResult)==2)
				{
					ScrSetIcon(ICON_UP, CLOSEICON);
					ScrSetIcon(ICON_DOWN, CLOSEICON);
					while(iUartDownloadReq(&iDownloadResult)==2);
					goto MAIN_MENU;
                }
            }
        }
        iUartDownloadDone(&iDownloadResult);

		if (ucGetKey == NOKEY)
		{
            ScrSetIcon(ICON_UP, CLOSEICON);
            ScrSetIcon(ICON_DOWN, CLOSEICON);
            ScrCls();
            return;
		}

		switch(ucGetKey)
		{
		case KEYF1:
			if (D200 != type)
			{
				ShowVer();
				goto MAIN_MENU;
			}
			else
			{
				if (ucPage)ucPage--;
	            else ucPage = ucMaxPage;
				break;
			}
		case KEYUP:
            
			if (ucPage)ucPage--;
            else ucPage = ucMaxPage;
            
			break;
		case KEYF2:
		case KEYDOWN:
            
			if (ucPage >= ucMaxPage) ucPage = 0;
			else ucPage++;
            
			break;
            
            
		case KEYCANCEL:

            ScrSetIcon(ICON_UP, CLOSEICON);
            ScrSetIcon(ICON_DOWN, CLOSEICON);
            ScrCls();
			return;
            
		case KEYFN:
		//case KEYF1:
            
			ShowVer();
			goto MAIN_MENU;
            
		case KEY0:
            
			SendPOSInfo();
			goto MAIN_MENU;
            
		default:
            if(ucGetKey>KEY0 && ucGetKey<=KEY9)
            {
    			RunFun((struct T_FUN_TABLE *)tMenu,ucMaxMenu,ucPage,ucItemOfPage,ucGetKey - KEY1);
    			goto MAIN_MENU;
            }

			break;
		}
	}
}

#define XML_DOC_MAX    8192  
#define ELE_NAME_MAX   100   

void xml_strlwr(char * turned)
{
	int i;
	i=0;
	while (turned[i]!=0)
	{
	  	if((turned[i]>='A')&&(turned[i]<='Z'))
	  	      turned[i]+=32;
		i+=1;
	}
}
/**************************************************************************
  int xml_find(char * doc,char *find_flag,int find_flag_len,int *find_location)
doc		:	被查找的字符串
doc_len		:	被查找的字符串的长度
find_flag	:	要找的字符串（find_flag中可以有任何的字符，包括'\0'
find_flag_len	:	find_flag的长度
find_location 	:	find_flag的第一个字符 在doc中的位置
**************************************************************************/
int xml_find(uchar *doc, int doc_len, char *find_flag, int find_flag_len, int *find_location)
{
	int flag,i,j;
	char temp[200],p[XML_DOC_MAX+1];

	flag=0;

	for(i=0;(i<doc_len-find_flag_len+1)&&(flag==0);i++)
	{
		memcpy(temp, doc+i, find_flag_len);
		temp[find_flag_len] = 0;
		xml_strlwr(temp);     
		if (memcmp(temp,find_flag,find_flag_len)==0)
	          	flag=1;
	}

	if (flag)
	{
		*find_location=i-1;
		#ifdef  TESTFIND
			printf("location: ");
			printf("%d\n",*find_location);
	
			printf("find succ");
			printf("%s\n",find_flag);
		#endif
		return(0);
	}
	else
	{
		#ifdef  TESTFIND
			printf("find fail  :"); printf("%s\n",find_flag);
		
		#endif
		return(1);
	}
}

/***************************************************************************
  int XmlGetElement(char * xml_doc,int xmldoc_len,char *ele_name,char * ele_value,int  va_max_len , int *value_real_len)

  参数说明
 Ruturn  0  XmlGetElement is called successful;
 Return  n  XmlGetElement is called failed ,n is the error code    ;
 xml_doc       : char *      . point to the xml document that will  been parsed;
                               要解析的xml document                                
 xmldoc_len    : int         . the xml_document's(xml_doc) length
 			       要解析的xml document的长度,该长度如果大等于报文的实际长度
 ele_name      : char *      . point to the xml element'name that you want to get it's name;
                               要解析的标签名,(不包括'<'和'>')         
 ele_value     : char *      . point to the buffer of you the xml element's value
                               存放解析结果的buffer
 value_real_len: int *       . point to the buffer of the xml element_value's real length;
                               解析后，buffer的实际长度
 va_max_len    :             . the max element_value's length
                               buffer的最大长度
*****************************************************************************/

int XmlGetElement(uchar *xml_doc, int xmldoc_len, char *ele_name,uchar *ele_value, int va_max_len, int *value_real_len)
{
	char start[ELE_NAME_MAX+4],end[ELE_NAME_MAX+5],turnbuff[ELE_NAME_MAX+4];
	char *value_start,*value_end,*startfindget,*endfindget;
	int start_len,end_len,i,j,find_location[2];
	int start_loca,end_loca,xml_doc_len;
	char test;
	
	xml_doc_len=xmldoc_len;
	if (xml_doc[0]==0) return(201);

	xml_strlwr(ele_name);
	if  (strcmp(ele_name,"root")==0)return(202);

	/*判断标签名是否为ROOT，是则返回错误码2*/
	/*if ele_name equal "ROOT"，return 2*/

	if (strlen(ele_name)>ELE_NAME_MAX) return(203);

	/*判断标签名长度是否超限 是则返回错误码3*/
	/*judge the length of element'name is longger then the max or not*/

	if (xml_find(xml_doc,xml_doc_len,"</root>",7,find_location))
		return(204);


	/*定位xml_document的结束位置*/

	xml_doc_len=find_location[0]+7;
	
	if (xml_doc_len>XML_DOC_MAX-1) return(205);
	/*判断xml document 的长度是否超限，是则返回错误码 5*/
	/****judge the length of xml document is too longger or not */


	strcpy(start,"<");
	strcat(start,ele_name);
	strcat(start,">");
	start_len=strlen(start);

	if (xml_find(xml_doc,xml_doc_len,start,start_len,find_location))return(208);
	else if (find_location[0]>xml_doc_len)return(209);

	start_loca=find_location[0];
	/*定位<标签名> 如果没有找到,返回错误吗9*/
	/*find the "<"+*ele_name+">"  . if not find return 1*/

	strcpy(end,"</");
	strcat(end,ele_name);
	strcat(end,">");
	start_len=strlen(start);


	if (xml_find(xml_doc,xml_doc_len,end,strlen(end),find_location))return(210);
	else if (find_location[0]>xml_doc_len)return(211);

	end_loca=find_location[0];
	/* 定位</标签名>,如果没有找到,返回错误码11*/
	/*find the "</"+*ele_name+">"  . if not find return error code 11*/

	*value_real_len=end_loca-start_loca-strlen(start);
	if (*value_real_len<0 ) return(212);
	if (*value_real_len>va_max_len)return(213);
	memcpy(ele_value,xml_doc+start_loca+start_len,*value_real_len);
	ele_value[*value_real_len] = 0;
	return(0);
	/*拷贝结果并返回0*/
	/* copy the ELEMENT  VALUE to ele_value*/
}

/*******************************************************************************************
  int XmlAddElement(char * xml_doc,char *ele_name,char *ele_value,int value_len,int *xml_real_len)

  参数说明
  xml_doc         : xml document 名
  xml_doc_max_len : xml_doc buffer最大长度
  ele_name        : 如果是root则新建一个xml document，xml_doc 指向该xml document。value_len,ele_value任意，
                    ×xml_real_len ：为新建的xml document 的实际长度。
                    如果不是root则在xml_doc最后（</root>前)加入一个标签和标签值为ele_value 的前value_len个字符。
  ele_value       : 要新建的标签值的指针。
  value_len       : 新建标签值的总长度，程序拷贝ele_value中的value_len到xml_doc中
  xml_real_len    : xml_doc 的总长度 调用时使用是xml_doc的长度，调用成功后返回的是xml_doc的新长度。

*******************************************************************************************/
int XmlAddElement(uchar *xml_doc, int xml_doc_max_len, char *ele_name, uchar *ele_value, int value_len, int *xml_real_len)
{
	char *insert_location;
	int name_len,xml_doc_len,i,find_location[1];
	char tmp[ELE_NAME_MAX+1];

	
	if (ele_name[0]==0) return (101);

	name_len=strlen(ele_name);
	xml_strlwr(ele_name);
	if(name_len>ELE_NAME_MAX) return(102);
	if  (value_len>XML_DOC_MAX) return(103);
	if ((strcmp(ele_name,"root")==0)||(strcmp(ele_name,"ROOT")==0))
	{
	   
	    if(xml_doc_max_len < strlen("<?xml version=\"1.0\"?><root></root>"))
	    return (108);
		
		strcpy(xml_doc,"<?xml version=\"1.0\"?><root></root>");
		*xml_real_len=strlen("<?xml version=\"1.0\"?><root></root>");
		return(0);
	}
	else
	{
		xml_doc_len=*xml_real_len;/*add by prs 20040116 */
		if ((xml_doc_len>XML_DOC_MAX)||(xml_doc_len<0))return(100);
		if (xml_doc[0]==0) return(104);
		if (xml_find(xml_doc,xml_doc_len,"</root>",7,find_location)) return(105);

		xml_doc_len=find_location[0]+7;
		if (name_len+name_len+5+value_len+xml_doc_len>XML_DOC_MAX) return(106);
		memcpy(tmp,"<",1);
		memcpy(tmp+1,ele_name,name_len);
		memcpy(tmp+1+name_len,">",1);
		tmp[name_len+2]=0;
		if (!xml_find(xml_doc,xml_doc_len,tmp,2+name_len,find_location)) return(107);
		/* 判断是否标签名是否已经存在 */
	
		if (name_len+name_len+5+value_len+xml_doc_len>xml_doc_max_len) return(108);

		*xml_real_len=name_len+name_len+5+value_len+xml_doc_len;
		
		insert_location=xml_doc+find_location[0];

		/*定位指针到xml文档的最后*/

		memcpy(insert_location,"<",1);
		insert_location+=1;
		memcpy(insert_location,ele_name,name_len);
		insert_location+=name_len;
		memcpy(insert_location,">",1);
		insert_location+=1;
		/*添加新的标签名 add new element name start*/

		memcpy(insert_location,ele_value,value_len);
		insert_location+=value_len;
		/*添加新的值,add new element value*/
		memcpy(insert_location,"</",2);
		insert_location+=2;
		memcpy(insert_location,ele_name,name_len);
		insert_location+=name_len;
		memcpy(insert_location,">",1);
		insert_location+=1;
		/*添加新标签名的结束符 add new element name end*/
		memcpy(insert_location,"</root>",7);
		insert_location+=7;
		insert_location[0]=0;

		return(0);
	}
}


/**************************************************************************
  int XmlDelElement(char * xml_doc,char * ele_name, int * xml_len)
      input:    xml_doc        :    输入的xml报文;
                ele_name       :    要删除的标签名，（不包括<>);
                xml_len        :    调用前xml_doc长度，返回后，删除标签ele_name后xml_doc 的长度
      out  :    删除成功返回0，否则返回对应的错误码，可加入DEBUG_DEL 查看错误信息
           
**************************************************************************/
int XmlDelElement(uchar *xml_doc, char *ele_name, int *xml_len)
{
	char ele_start[ELE_NAME_MAX+5],ele_end[ELE_NAME_MAX+5];
	int  ele_name_len,name_start,name_end,root_end,xml_doc_len;
	xml_doc_len=*xml_len;
	if ((xml_doc_len>XML_DOC_MAX)||(xml_doc_len<0))return(300);
	if (xml_find(xml_doc,xml_doc_len,"</root>",7,&root_end)) return(304); 
	xml_doc_len=root_end+7;
        
	ele_name_len=strlen(ele_name);
	if  (ele_name_len>ELE_NAME_MAX)return (301);
	strcpy(ele_start,"<");
	strcat(ele_start,ele_name);
	strcat(ele_start,">");
	ele_start[2+ele_name_len]=0;
	
	memcpy(ele_end,"</",2);
	memcpy(ele_end+2,ele_name,ele_name_len);
	memcpy(ele_end+2+ele_name_len,">",1);
	ele_end[3+ele_name_len]=0;

	if (xml_find(xml_doc,xml_doc_len,ele_start,2+ele_name_len,&name_start)) return(302);
	if (xml_find(xml_doc,xml_doc_len,ele_end,3+ele_name_len,&name_end))return(303);

	memcpy(xml_doc+name_start,xml_doc+name_end+3+ele_name_len,root_end+7-(name_start+3+ele_name_len));
	*xml_len=root_end+7-(name_end+3+ele_name_len-name_start);
	xml_doc[*xml_len]=0;
	return(0);
}
// 取公钥签名, 基于 s_CopyUserPuk()
// Sig: 不少于 580 空间
// -1: 无签名标记
int GetPUKSig(uchar KeyID, uchar *Sig, int *SigLen)
{
    int ret;
    R_RSA_PUBLIC_KEY PubKey;
    tSignFileInfo siginfo;
    
    ret = s_GetPuK(KeyID, &PubKey, &siginfo);
    if(ret) return -1;

    *SigLen = sizeof(tSignFileInfo);
    memcpy(Sig, &siginfo, *SigLen);

    return 0;
}

int GetBootSig(uchar *Sig, int *SigLen)
{
	*SigLen = 0;
    return 0;
}

//0--未上传,1--已上传,2--系统不支持 SN KEY(ID KEY)
int GetIDKeyState()
{
    uchar state = 0x00;
    int ret = -1;
    
    ret = s_PedGetIdKeyState(1, &state);
    if (0 == ret) ret = state; //0--未上传,1--已上传。
    else if (-1 == ret) ret = 2;//系统不支持 SN KEY
    else ret = 0; 

    return ret;
}

/*
// 以 XML 格式返回终端信息
>=0 InfoOut 中实际信息长度
-1 输出缓冲区（InfoOut）长度不够
-2 输出缓冲区（InfoOut）指针为空
-3 获取座机字库时出错
-4 其它错误*/
int GetTermInfoExt(uchar *InfoOut, int MaxBufLen)
{
#define MAX_FONT 255
    int r,len,baseinfval=0;
    int iLoop,PUKIdx,SigLen;
	uchar ele_name[ELE_NAME_MAX],ele_doc[XML_DOC_MAX],Area;
    uchar pszTmp[128];
	ST_FONT astFontTable[MAX_FONT];
    APPINFO AppInfo;
	int tt;
	uchar VerInfo[8], WnetInfo[128], aucImei[16], rfParamVer[8], tempBuf[32];
	uchar basesn[33],baseexsn[40],basetype[2];

	if(InfoOut==NULL) return -2;

	memset(ele_name, 0x00, ELE_NAME_MAX);
	memset(ele_doc,  0x00, XML_DOC_MAX);
	memset(VerInfo, 0, sizeof(VerInfo));
	memset(aucImei, 0, sizeof(aucImei));
	memset(rfParamVer, 0, sizeof(rfParamVer));

	ReadVerInfo(VerInfo);

    strcpy(ele_name, "root"); 
    r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
    if (r != 0){
       if (r==108) return -1;   
	   else        return -4;
	}
    
	tt=CheckFlashSize()*(1024);

    // FLASH 大小
    strcpy(ele_name, "FlashSize"); 
	sprintf(ele_doc, "%dKB",tt);
    r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
    if (r != 0){
       if (r==108) return -1;   
	   else        return -4;
	}

    // SDRAM 大小
    strcpy(ele_name, "DDRSize"); 
	tt = CheckSDRAMSize()*1024;
	sprintf(ele_doc, "%dKB",tt);
    r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
    if (r != 0){
       if (r==108) return -1;   
	   else        return -4;
	}
   
	if (GetWlType())	
	{	
        // 无线模块版本信息
        memset(WnetInfo, 0x00, sizeof(WnetInfo));
        memset(aucImei, 0x00, sizeof(aucImei));
        WlGetModuleInfo(WnetInfo, aucImei);
    	strcpy(ele_name, "WNETVer"); 
    	sprintf(ele_doc, "%s", WnetInfo);

    	r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
    	if (r != 0)
    	{
    		if (r==108) 
    			return -1;   
    		else        
    			return -4;
    	}
		// 无线模块 IMEI 号
		if(is_cdma_module()){
			if (s_CdmaIsMeid()==1)
				strcpy(ele_name, "MEID");
			else
				strcpy(ele_name, "IMSI");
		}
		else strcpy(ele_name, "IMEI");
		
		sprintf(ele_doc, "%s", aucImei);
		
		r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
		if (r != 0)
		{
			if (r==108)
			{
				return -1;
			}
			else
			{
				return -4;
			}
		}
    }

    if(get_rftype()!= 0) 
    {
        strcpy(ele_name, "RFChip");

        sprintf(ele_doc, "%02x", get_rftype());

        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0)
        {
            if (r==108)
                return -1;
            else
                return -2;
        }
    }

    /*printer type*/
	memset(pszTmp, 0x00, sizeof(pszTmp));
	tt = ReadCfgInfo("PRINTER", pszTmp);
    if(tt > 0) 
    {
        strcpy(ele_name, "PRINTER");
		memcpy(ele_doc, pszTmp, tt+1);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0)
        {
            if (r==108)
                return -1;
            else
                return -2;
        }
    }

	// 座机信息
    if (is_hasbase())// && (GetBaseVer()==0))
    {
    	GetBaseVer();
    	// 座机机型
		strcpy(ele_name, "BaseMachineType");
		switch(base_info.MachineType)
		{
			case B210:
				strcpy(ele_doc, "B210");
				break;
			default:
				break;
		}
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0)
        {
            if (r==108) return -1;   
            else        return -4;
        }
    	
        strcpy(ele_name, "BaseMAIN");
        sprintf(ele_doc, "%02d", base_info.ver_hard);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0)
        {
            if (r==108) return -1;   
            else        return -4;
        }
        // 座机 Monitor 版本
        strcpy(ele_name, "BaseDRIVER");
        sprintf(ele_doc, "%s", base_info.ver_soft);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0){
            if (r==108) return -1;   
            else        return -4;
        }

        // 座机 FLASH 大小
        strcpy(ele_name, "BaseFlashSize");
        sprintf(ele_doc, "%ldKB",base_info.flash_size*1024);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0){
            if (r==108) return -1;   
            else        return -4;
        }

        // 座机 SDRAM 大小
        strcpy(ele_name, "BaseSdramSize");
        sprintf(ele_doc, "%ldKB",base_info.sdram_size*1024);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0){
        if (r==108) return -1;   
        else        return -4;
        }
        //座机 BOOT 版本
        strcpy(ele_name, "BaseBIOS");
        if (base_info.ver_boot)
        {		  
            sprintf(ele_doc, "%02d", base_info.ver_boot);
            r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
            if (r != 0){
                if (r==108) return -1;   
                else        return -4;
            }
        }

        // 座机字库总大小
        strcpy(ele_name, "BASEFONTSIZE");
        sprintf(ele_doc, "%ldB", base_info.font_size);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0){
            if (r==108) return -1;   
            else        return -4;
        }
		
		// 座机SN EXSN
		memset(basesn, 0, sizeof(basesn));
		memset(baseexsn, 0, sizeof(baseexsn));
		memcpy(basesn, base_info.BaseSN, 8);
		if(strlen(base_info.BaseSNLeft))
		{
			memcpy(&basesn[8],base_info.BaseSNLeft,strlen(base_info.BaseSNLeft));
		}
		memcpy(baseexsn, base_info.BaseEXSN, 22);
		
        strcpy(ele_name, "BaseSN");		
        sprintf(ele_doc, "%s", basesn);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0){
            if (r==108) return -1;   
            else        return -4;
        }

		strcpy(ele_name, "BaseEXSN");
        sprintf(ele_doc, "%s", baseexsn);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0){
            if (r==108) return -1;   
            else        return -4;
        }

        // 座机类型
        memset(basetype,0,sizeof(basetype));
        strcpy(ele_name, "BaseType");
        if(base_info.bt_exist)			//座机蓝牙存在
        {
        	basetype[0] = '1';			// 蓝牙座机
        }
        else 
        {
        	basetype[0] = '0';			// 串口座机
        }
        sprintf(ele_doc, "%s" ,basetype);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0){
            if (r==108) return -1;   
            else        return -4;
        }
    }
    
    for (iLoop=0;iLoop<MAX_APP_NUM;iLoop++){
        // 应用名称
        if (ReadAppInfo(iLoop, &AppInfo) != 0) continue;
        memset(pszTmp, 0x00, sizeof(pszTmp));
        memcpy(pszTmp, AppInfo.AppName, sizeof(AppInfo.AppName)); 
        sprintf(ele_name, "APP%dName", iLoop);
        sprintf(ele_doc,  "%s",        pszTmp);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
        if (r != 0){
           if (r==108) return -1;   
           else        return -4;
	    }

        // 应用签名
        sprintf(ele_name, "APP%dSig", iLoop);
		sprintf(pszTmp,   "%s%d", APP_FILE_NAME_PREFIX, iLoop);
        if (GetAppSigInfo(AppInfo.AppNum, ele_doc) != 0) continue;
        SigLen = sizeof(tSignFileInfo);
        r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, SigLen, &len); 
        if (r != 0){
           if (r==108) return -1;   
           else        return -4;
	    }
    }
    // 用户公钥签名
    strcpy(ele_name, "UsPukSig");
    if (GetPUKSig(ID_US_PUK, ele_doc, &SigLen) == 0)
	{
	    r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, SigLen, &len); 
		if (r != 0){
		if (r==108) return -1;   
		else        return -4;
		}
	}
	// Monitor 签名
	memset(ele_name, 0, sizeof(ele_name));
    strcpy(ele_name, "MonSig");
    if (GetMonSig(ele_doc, &SigLen) == 0) 
	{
		r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, SigLen, &len); 
		if (r != 0){
		if (r==108) return -1;   
		else        return -4;
		}
	}

	 // SN KEY (ID KEY) STATE
    strcpy(ele_name, "TIDKeyState"); 
	sprintf(ele_doc, "%d", GetIDKeyState());
    r = XmlAddElement(InfoOut, MaxBufLen, ele_name, ele_doc, strlen(ele_doc), &len); 
    if (r != 0){
       if (r==108) return -1;   
	   else        return -4;
	}

    return len;
}

/*
	***App interface!***
	Apps set the system configuration info
	Param:
		ConfigInfoIn:XML format info buffer.
		InfoInLen: XML info buffer total length.
	Return value:
		0:	Success
		-1: ConfigInfoIn is NULL
*/
int SysConfig(uchar *ConfigInfoIn, int InfoInLen)
{
	int 		ret, itemIdx;
	int 		valueRealLen;
	uchar	ele_value[ITEM_VALUE_MAX_LEN];
	char 	name[ELE_NAME_MAX];


	memset(ele_value, 0, sizeof(ele_value));

	for(itemIdx = 0; itemIdx < (sizeof(sysconfigTab)/sizeof(T_SYSCONFIG_ITEM)); itemIdx++)
	{/* match item one by one */
		memset(name, 0, sizeof(name));
		sprintf(name, "%s", sysconfigTab[itemIdx].pItemName);
		ret = XmlGetElement(ConfigInfoIn, InfoInLen, name, ele_value, ITEM_VALUE_MAX_LEN, &valueRealLen);
		DelayMs(100);
		if(ret != 0)
		{/* error param Or doesn't have this item */
			continue;
		}
		else
		{/* match the item */
			/* process the item value */
			if(strcmp(ele_value, "0") != 0 && strcmp(ele_value, "1") != 0)
			{/* error value */
				continue;
			}
			else
			{
				sysconfigTab[itemIdx].funcPtr(atoi(ele_value));
			}
		}
	}

	return 0;
}

