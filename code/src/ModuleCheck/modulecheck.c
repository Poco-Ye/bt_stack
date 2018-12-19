//#include "..\bluetooth\wlt2564.h"
#include <posapi.h>
#include <base.h>
#include <stdarg.h>
#include "ModuleCheck.h"
#include "ModuleMenu.h"
#include "..\ipstack\include\netapi.h"
#include "..\ipstack\include\socket.h"
#include "..\comm\comm.h"
extern unsigned char bIsChnFont; 
extern void s_ScrFillRect(int left,int top, int right,int bottom);

typedef struct {
	//********UART
	uchar ucSendCommN;//发送
	uchar ucRecvCommN;//接收
	uchar szCommParamStr[20];//
	int  iExeUARTCnt;
	int  iExeUARTSucss;//执行成功的次数
	int  iUARTSendTimes;//一次连接发送的次数
	int  iUARTSucssTimes;//一次连接通讯成功的次数
	int  iUARTSendCnt;//发送的字节数
	//*******Modem
	COMM_PARA ModemParam;
	uchar szModemDialNum[50];
	uchar ucMode;
	int iExeModemCnt;
	int  iExeModemSucss;//执行成功的次数
	int  iModemSendTimes;//一次连接发送的次数
	int  iModemSucssTimes;//一次连接通讯成功的次数
	int  iModemSendCnt;//发送的字节数
	//*******TCPIP_LAN
	uchar ucDHCPFlag;
	uchar szLoclIP[20];
	uchar szHostIP[20];
	uchar szHostPort[10];
	uchar szMaskIP[20];
	uchar szGetWayIP[20];
	uchar szPinHostIP[20];
	uchar szDnsIP[20];
	int  iPingTimeout;//Ping 超时时间
	int  iPingSize;//包大小
	int	 iExeLANCnt;
	int  iLANSendTimes;//一次连接发送的次数
	int  iExeLANSucss;//执行成功的次数
	int  iLANSucssTimes;//一次连接通讯成功的次数
	int  iLANSendCnt;//发送的字节数
	//*******TCPIP_WIFI
	int iExeWIFICnt;
	int  iExeWIFISucss;//执行成功的次数
	int  iWIFISendTimes;//一次连接发送的次数
	int  iWIFISucssTimes;//一次连接通讯成功的次数
	int  iWIFISendCnt;//发送的字节数
	//*******WNET
	uchar szWNetDailNum[100];
	uchar szSIMPIN[8];
	uchar szUID[64];
	uchar szPWD[16];
	uchar szDespIP[20];
	uchar szDespPort[10];
	int iWNetTCPTime;//一次PPP连接TCPIP连接的次数
	int  iWNetSucssTimes;//一次连接通讯成功的次数
	int iWNetSendTimes;//一次连接发送的次数
	int iExeWNetCnt;//执行的次数
	int  iExeWNetSucss;//执行成功的次数
	int iWNetSendCnt;//发送的字节个数
}TCommParam;
TCommParam tCommParam;
void McCheckICC(void);
void McCheckMag(void);
void McCheckRF(void);
void McCheckKB(void);
void McCheckLCD(void);
void McCheckModem(void);
void McCheckETH(void);
void McCheckWNET(void);
uchar GetKeyWait(int Ms);
uchar McGetIpAddrStr(uchar * StrOut,uchar ucLen,uchar ucRow);
uchar McCheckIpStr(uchar *IpAddR, uchar ucLen);
void McLoadCommParam();
uchar McStoreCommParam();
void McRemoveDot(uchar * ptrDest,uchar * ptrSource);
void McAddDot(uchar * ptrDest,uchar * ptrSource);
uchar McDispTrack(uchar ucResult, uchar *szTrack1, uchar *szTrack2, uchar *szTrack3, Menu tMageMenu[]);
int McSetLanParam();
int McSetWNetParam();
int McSetModemParam();
int McSetCommParam(uchar ucType);
void WnetError(int iRet);
void ModemError(uchar ucRet);
void EthError(int iRet);
int ModemTest(void);
void AddToMenu(Menu *sModule, int id, int ucX, int ucY, unsigned int uiAttr, 
			   char  *acChnName, char  *acEngName, void *pFun)
{
	sModule->id = id;
	sModule->ucX = ucX;
	sModule->ucY = ucY;
	sModule->uiAttr = uiAttr;
	strcpy(sModule->acChnName, acChnName);
	strcpy(sModule->acEngName, acEngName);
	sModule->pFun = pFun;
}

void McCheckICC(void)
{
	int iSelectedItem;
	int iWidth,iHeight;
	int iMenuStatus[5] = {0,0,0,0,0},iMenuStatusBak[5];
	int iMenuChange = 0,iFirstCheck = 0;
	Menu tIccMenu[4];
	Menu tIccMenu1[7] = 
	{
		0,0,0,0x00c1,"ICC阅读器测试","ICC READER TEST",NULL,
		0,0,0,0x1000,"Card Test Processing","Card Test Processing",NULL,
		0,0,0,0x1300,"","",NULL,
		0,0,0,0x1300,"","",NULL,
		0,0,0,0x1300,"","",NULL,
		0,0,0,0x1300,"","",NULL,
		0,0,0,0x1300,"","",NULL
	};
	char Name[5][MENU_MAX_NAME_SIZE] = 
	{
		"1 - User Card:\x1c",
		"2 - SAM1 Slot:\x1c",
		"3 - SAM2 Slot:\x1c",
		"4 - SAM3 Slot:\x1c",
		"5 - SAM4 Slot:\x1c"
	};
	int iNum = 0, i = 0, j = 0;
	unsigned char ucSlot[5] = {0x00,0x82,0x83,0x84,0x85};
	unsigned char ucRet,szATR[32];
	memset(tIccMenu,0,sizeof(tIccMenu));
	ScrGetLcdSize(&iWidth,&iHeight);
	if(iHeight == 272)//MT30
	{
		AddToMenu(&tIccMenu[0],0,0,0,0x00c1,"ICC阅读器测试","ICC READER TEST",NULL);
		AddToMenu(&tIccMenu[1],0,0,0,0x1401,"","",NULL);
		AddToMenu(&tIccMenu[2],0,0,0,0x1401,"","",NULL);
		AddToMenu(&tIccMenu[3],0,0,0,0x1101,"请插卡!","PLS INSERT CARD!",NULL);
		iNum = 4;
	}
	else
	{
		AddToMenu(&tIccMenu[0],0,0,0,0x00c1,"ICC阅读器测试","ICC READER TEST",NULL);
		AddToMenu(&tIccMenu[1],0,0,0,0x1401,"","",NULL);
		AddToMenu(&tIccMenu[2],0,0,0,0x1101,"请插卡!","PLS INSERT CARD!",NULL);
		iNum = 3;
	}
	ScrCls();
	iShowMenu(tIccMenu, iNum, 0, NULL);
	if(iHeight == 272)//MT30
	{
		tIccMenu1[1].uiAttr = 0x1001;
		for(i = 2;i < sizeof(tIccMenu1) / sizeof(Menu); i++)
		{
			tIccMenu1[i].uiAttr = 0x1301;
		}
	}
	i = 0;
	TimerSet(1,600);
	while (IccDetect(ucSlot[i]) != 0)
	{
		if(TimerCheck(1) == 0)
		{
			return;
		}
		if (!kbhit() && getkey() == KEYCANCEL)
		{
			return;
		}
		i = (i+1) % 5;
	}
	
	i = 0;
	TimerSet(1,600);
	while (TimerCheck(1) != 0)
	{
		if (!kbhit() && getkey() == KEYCANCEL)
		{
			return;
		}
		for(j = 0;j < 5;j++)
		{
			iMenuStatusBak[j] = iMenuStatus[j];
		}
		memset(tIccMenu1[i+2].acChnName,0,MENU_MAX_NAME_SIZE);
		strcpy(tIccMenu1[i+2].acChnName,Name[i]);
		memset(tIccMenu1[i+2].acEngName,0,MENU_MAX_NAME_SIZE);
		strcpy(tIccMenu1[i+2].acEngName,Name[i]);
		if(IccDetect(ucSlot[i]) != 0)
		{
			strcat(tIccMenu1[i+2].acChnName,"N/A ");
			strcat(tIccMenu1[i+2].acEngName,"N/A ");
			iMenuStatus[i] = 1;
		}
		else
		{
			if((ucRet = IccInit(ucSlot[i],szATR)) == 0x00)
			{
				strcat(tIccMenu1[i+2].acChnName,"OK ");
				strcat(tIccMenu1[i+2].acEngName,"OK ");
				iMenuStatus[i] = 2;
			}
			else
			{
				strcat(tIccMenu1[i+2].acChnName,"ERR ");
				strcat(tIccMenu1[i+2].acEngName,"ERR ");
				iMenuStatus[i] = 3;
			}
			IccClose(ucSlot[i]);
		}
		if(i == 4 && iFirstCheck == 0)
		{
			iFirstCheck = 1;
		}
		i = (i+1) % 5;
		for(j = 0;j < 5;j++)
		{
			if(iMenuStatus[j] != iMenuStatusBak[j])
			{
				iMenuChange = 1;
				break;
			}
		}
		if(iMenuChange == 1 && iFirstCheck == 1)
		{
			iShowMenu(tIccMenu1, 7, 0, NULL);
			iMenuChange = 0;
		}
	}

}
void McCheckMag(void)
{
	Menu tMagMenu[4];
	uchar szTrack1[256],szTrack2[256],szTrack3[256];
	int iTimerCheck,iTimerBack=0;
	int iWidth,iHeight;
	unsigned char ucRet;
	ScrGetLcdSize(&iWidth,&iHeight);
	memset(szTrack1,0,sizeof(szTrack1));
	memset(szTrack2,0,sizeof(szTrack2));
	memset(szTrack3,0,sizeof(szTrack3));
WAIT_FOR_MAGSWIPE:
	memset(tMagMenu,0,sizeof(tMagMenu));
	AddToMenu(&tMagMenu[0],0,0,0,0x00c1,"磁卡阅读器测试","MAG READER TEST",NULL);
	AddToMenu(&tMagMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tMagMenu[2],0,0,0,0x1101,"请刷卡!","PLS SWIPE CARD!",NULL);
	AddToMenu(&tMagMenu[3],0,0,0,0x1101,"","",NULL);
	if(iHeight == 272)//MT30
	{
		tMagMenu[1].uiAttr = 0x1401;
	}
	MagOpen();
	ScrCls();
	iShowMenu(tMagMenu, 4, 0, NULL);
	TimerSet(1,600);
	MagReset();
	while(1)
	{
		if(!kbhit() && getkey() == KEYCANCEL)
		{
			return;
		}
		iTimerCheck = TimerCheck(1);
		if(iTimerCheck == 0)
		{
			return;
		}
		if(iHeight == 272) //MT30
		{
			if(iTimerBack != iTimerCheck/10)
			{
				ScrPrint(0,6,0x41,"%d",iTimerCheck/10);
			}
		}
		else
		{
			if(iTimerBack != iTimerCheck/10)
			{
				ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
			}
		}
		iTimerBack = iTimerCheck/10;
		
		DelayMs(200);
		if(!MagSwiped())
		{
			break;
		}

	}

	ucRet = McDispTrack(MagRead(szTrack1, szTrack2, szTrack3), szTrack1, szTrack2, szTrack3,tMagMenu);

	if(ucRet == 0x01)
	{
		goto WAIT_FOR_MAGSWIPE;//返回值表示跳至等待刷卡界面
	}
	MagClose();
}
void McCheckRF(void)
{
	Menu tRFMenu[4];
	uchar ucRet;
	uchar ucCardType,szSerialInfo[10],szCID[8],szOther[10];
	int iTimerCheck,iTimerBack=0;
	int iWidth,iHeight;
	uchar ucTermName[16];
	memset(ucTermName,0,sizeof(ucTermName));
	ScrGetLcdSize(&iWidth,&iHeight);
WAIT_FOR_RFSWIPE:
	memset(tRFMenu,0,sizeof(tRFMenu));
	AddToMenu(&tRFMenu[0],0,0,0,0x00c1,"RF阅读器测试","RF READER TEST",NULL);
	AddToMenu(&tRFMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tRFMenu[2],0,0,0,0x1101,"请挥卡!","PLS TAP CARD!",NULL);
	AddToMenu(&tRFMenu[3],0,0,0,0x1101,"","",NULL);
	if(iHeight == 272)//MT30
	{
		tRFMenu[1].uiAttr = 0x1401;
	}
	ScrCls();
	iShowMenu(tRFMenu, 4, 0, NULL);
	ucRet= PiccOpen();
	if(ucRet != 0x00)
	{
		if(iHeight == 272)//MT30
		{

			tRFMenu[2].uiAttr = 0x1401;
		}
		else
		{
			tRFMenu[2].uiAttr = 0x1400;
		}
		AddToMenu(&tRFMenu[3],0,0,0,0x1101,"没有RF模块","NO RF MODULE",NULL);
		ScrCls();
		iShowMenu(tRFMenu, 4, 0, NULL);
		PiccClose();
		GetKeyWait(5000);
		return;
	}
	PiccClose();
	TimerSet(1,600);
	PiccOpen();
	while(1)
	{
		if(!kbhit() && getkey() == KEYCANCEL)
		{
			PiccClose();
			return;
		}
		iTimerCheck = TimerCheck(1);
		if(iTimerCheck == 0)
		{
			if(iHeight == 272)//MT30
			{
			
				tRFMenu[2].uiAttr = 0x1401;
			}
			else
			{
				tRFMenu[2].uiAttr = 0x1400;
			}

			AddToMenu(&tRFMenu[3],0,0,0,0x1101,"读卡超时","TIME OUT",NULL);
			ScrCls();
			iShowMenu(tRFMenu, 4, 0, NULL);
			while(2)
			{
				PiccClose();
				ucRet = GetKeyWait(30000);
				if(ucRet == 0x00 || ucRet == KEYENTER)
				{
					goto WAIT_FOR_RFSWIPE;
				}
				else if(ucRet == KEYCANCEL)
				{
					return;
				}
			}
		}
		if(iHeight == 272) //MT30
		{
			if(iTimerBack != iTimerCheck/10)
			{
				ScrPrint(0,6,0x41,"%d",iTimerCheck/10);
			}
		}
		else
		{
			if(iTimerBack != iTimerCheck/10)
			{
				ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
			}
		}
		iTimerBack = iTimerCheck/10;
		
		DelayMs(200);
		ucRet = PiccDetect(0,&ucCardType,szSerialInfo,szCID,szOther);
		if(ucRet == 0x00)
		{
			PiccClose();
			break;
		}
		ucRet = PiccDetect('M',&ucCardType,szSerialInfo,szCID,szOther);
		if(ucRet == 0x00)
		{
			PiccClose();
			break;
		}
	}
	if(iHeight == 272)//MT30
	{
		tRFMenu[2].uiAttr = 0x1401;
	}
	else
	{
		tRFMenu[2].uiAttr = 0x1400;
	}

	if(ucCardType == 'M')
	{
		AddToMenu(&tRFMenu[3],0,0,0,0x1101,"M1卡测试完成!","M1 CARD TEST OK!",NULL);
	}
	else if(ucCardType == 'B')
	{
		AddToMenu(&tRFMenu[3],0,0,0,0x1101,"B卡测试完成!","B CARD TEST OK!",NULL);
	}
	else 
	{
		AddToMenu(&tRFMenu[3],0,0,0,0x1101,"A卡测试完成!","A CARD TEST OK!",NULL);
	}
	ScrCls();
	iShowMenu(tRFMenu, 4, 0, NULL);
	while(1)
	{
		PiccClose();
		ucRet = GetKeyWait(30000);
		if(ucRet == 0x00 || ucRet == KEYENTER)
		{
			goto WAIT_FOR_RFSWIPE;
		}
		else if(ucRet == KEYCANCEL)
		{
			return;
		}
	}
}
void McCheckKB(void)
{
	Menu tKBMenu[4];
	uchar ucRet;
	memset(tKBMenu,0,sizeof(tKBMenu));
	AddToMenu(&tKBMenu[0],0,0,0,0x00c1,"键盘测试","KEYBOARD TEST",NULL);
	AddToMenu(&tKBMenu[1],0,0,0,0x1101,"请按任意键:","PLS PRESS KEY:",NULL);
	AddToMenu(&tKBMenu[2],0,0,0,0x1401,"","",NULL);
	AddToMenu(&tKBMenu[3],0,0,0,0x1101,"连续按两次0退出","TWICE [0] EXIT",NULL);
	ScrCls();
	iShowMenu(tKBMenu, 4, 0, NULL);
	while(1)
	{
		ucRet = getkey();
		if(ucRet == KEY0)
		{
			if(bIsChnFont)
			{
				ScrPrint(0,4,0x41,"当前按键:%c",ucRet);
			}
			else
			{
				ScrPrint(0,4,0x41,"[%c] Pressed",ucRet);
			}
			ucRet = GetKeyWait(3000);
			if(ucRet == KEY0)
			{
				return;
			}
			if(ucRet == 0x00)
			{
				continue;
			}
		}
		if(bIsChnFont)
		{
			if(ucRet >= 0x30 && ucRet <= 0x39)
			{
				ScrPrint(0,4,0x41,"当前按键:%c",ucRet);
			}
			else
			{
				switch(ucRet)
				{
					case KEYCLEAR:
						ScrPrint(0,4,0x41,"当前按键:清除");
						break;
					case KEYENTER:
						ScrPrint(0,4,0x41,"当前按键:确认");
						break;
					case KEYCANCEL:
						ScrPrint(0,4,0x41,"当前按键:取消");
						break;
					case KEYMENU:
						ScrPrint(0,4,0x41,"当前按键:菜单");
						break;
					case KEYALPHA:
						ScrPrint(0,4,0x41,"当前按键:字母");
						break;
					case KEYUP:
						ScrPrint(0,4,0x41,"当前按键:上翻");
						break;
					case KEYDOWN:
						ScrPrint(0,4,0x41,"当前按键:下翻");
						break;
					case KEYFN:
						ScrPrint(0,4,0x41,"当前按键:功能");
						break;
					case KEYF1:
						ScrPrint(0,4,0x41,"当前按键:F1");
						break;
					case KEYF2:
						ScrPrint(0,4,0x41,"当前按键:F2");
						break;
					case KEYF3:
						ScrPrint(0,4,0x41,"当前按键:F3");
						break;
					case KEYF4:
						ScrPrint(0,4,0x41,"当前按键:F4");
						break;
					case KEYF5:
						ScrPrint(0,4,0x41,"当前按键:F5");
						break;
					case KEYF6:
						ScrPrint(0,4,0x41,"当前按键:F6");
						break;
					case KEYATM1:
						ScrPrint(0,4,0x41,"当前按键:ATM1");
						break;
					case KEYATM2:
						ScrPrint(0,4,0x41,"当前按键:ATM2");
						break;
					case KEYATM3:
						ScrPrint(0,4,0x41,"当前按键:ATM3");
						break;
					case KEYATM4:
						ScrPrint(0,4,0x41,"当前按键:ATM4");
						break;
					default:
						ScrPrint(0,4,0x41,"其它按键:0x%2X",ucRet);
						break;
				}
			}
		}
		else
		{
			if(ucRet >= 0x30 && ucRet <= 0x39)
			{
				ScrPrint(0,4,0x41,"[%c] Pressed",ucRet);
			}
			else
			{
				switch(ucRet)
				{
					case KEYCLEAR:
						ScrPrint(0,4,0x41,"[CLEAR] Pressed");
						break;
					case KEYENTER:
						ScrPrint(0,4,0x41,"[ENTER] Pressed");
						break;
					case KEYCANCEL:
						ScrPrint(0,4,0x41,"[CANCEL] Pressed");
						break;
					case KEYMENU:
						ScrPrint(0,4,0x41,"[MENU] Pressed");
						break;
					case KEYALPHA:
						ScrPrint(0,4,0x41,"[ALPHA] Pressed");
						break;
					case KEYUP:
						ScrPrint(0,4,0x41,"[UP] Pressed");
						break;
					case KEYDOWN:
						ScrPrint(0,4,0x41,"[DOWN] Pressed");
						break;
					case KEYFN:
						ScrPrint(0,4,0x41,"[FN] Pressed");
						break;
					case KEYF1:
						ScrPrint(0,4,0x41,"[F1] Pressed");
						break;
					case KEYF2:
						ScrPrint(0,4,0x41,"[F2] Pressed");
						break;
					case KEYF3:
						ScrPrint(0,4,0x41,"[F3] Pressed");
						break;
					case KEYF4:
						ScrPrint(0,4,0x41,"[F4] Pressed");
						break;
					case KEYF5:
						ScrPrint(0,4,0x41,"[F5] Pressed");
						break;
					case KEYF6:
						ScrPrint(0,4,0x41,"[F6] Pressed");
						break;
					case KEYATM1:
						ScrPrint(0,4,0x41,"[ATM1] Pressed");
						break;
					case KEYATM2:
						ScrPrint(0,4,0x41,"[ATM2] Pressed");
						break;
					case KEYATM3:
						ScrPrint(0,4,0x41,"[ATM3] Pressed");
						break;
					case KEYATM4:
						ScrPrint(0,4,0x41,"[ATM4] Pressed");
						break;
					default:
						ScrPrint(0,4,0x41,"[0x%2X] Pressed",ucRet);
						break;
				}
			}
		}
			
	}
}
void McCheckLCD(void)
{
	Menu tLCDMenu[4];
	int iWidth,iHeight,i,j;
	uchar ucRet;
	ScrGetLcdSize(&iWidth,&iHeight);
	while(1)
	{
		memset(tLCDMenu,0,sizeof(tLCDMenu));
		AddToMenu(&tLCDMenu[0],0,0,0,0x00c1,"屏幕测试","LCD TEST",NULL);
		AddToMenu(&tLCDMenu[1],0,0,0,0x1400,"","",NULL);
		AddToMenu(&tLCDMenu[2],0,0,0,0x1101,"按[确认]开始","[ENTER]   START",NULL);
		AddToMenu(&tLCDMenu[3],0,0,0,0x1101,"按[取消]返回","[CANCEL] RETURN",NULL);
		if(iHeight == 272)//MT30
		{
			tLCDMenu[1].uiAttr = 0x1401;
		}
		ScrCls();
		iShowMenu(tLCDMenu, 4, 0, NULL);
		while(2)
		{
			ucRet = getkey();
			if(ucRet == KEYCANCEL)
			{
				return;
			}
			else if(ucRet == KEYENTER)
			{
				break;
			}
			else 
			{
				continue;
			}
		}
		for(j = 0;j < iHeight;j++)
		{
			for(i=0;i<iWidth;i++)
			{
				ScrPlot(i,j,1);
			}
		}
		while(2)
		{
			ucRet = getkey();
			if(ucRet == KEYCANCEL)
			{
				return;
			}
			else if(ucRet == KEYENTER)
			{
				break;
			}
		}
		ScrCls();
		while(2)
		{
			ucRet = getkey();
			if(ucRet == KEYCANCEL)
			{
				return;
			}
			else if(ucRet == KEYENTER)
			{
				break;
			}
		}
		ScrBackLight(0);
		while(2)
		{
			ucRet = getkey();
			if(ucRet == KEYCANCEL)
			{
				return;
			}
			else if(ucRet == KEYENTER)
			{
				break;
			}
		}
		ScrBackLight(1);
		if(iHeight != 272)//非MT30
		{
			for(i = 1;i <= 8;i++)
			{
				ScrSetIcon(i,1);
			}
			while(2)
			{
				ucRet = getkey();
				if(ucRet == KEYCANCEL)
				{
					return;
				}
				else if(ucRet == KEYENTER)
				{
					break;
				}
			}
			for(i = 1;i <= 8;i++)
			{
				ScrSetIcon(i,0);
			}
			while(2)
			{
				ucRet = getkey();
				if(ucRet == KEYCANCEL)
				{
					return;
				}
				else if(ucRet == KEYENTER)
				{
					break;
				}
			}
		}
	}
}

uchar gucSSetup = 0x06;
void McCheckModem(void)
{
	int iRet = 0;
	int iExeModemCnt;

	// D210的机器可能需要根据对应的座机切换到蓝牙链路
	if(GetHWBranch() == D210HW_V2x )
	{
		if(GetBaseVer() != 0) return;
		if(GetLinkType() && base_info.bt_exist)			
		{
			// 创建蓝牙链路
			if(CreateHandsetBaseLink(0)) return;
		}
		else if((0 == GetLinkType() && base_info.bt_exist) || (1 == GetLinkType() && 0 == base_info.bt_exist))
		{
			return;
		}
	}
	while(1)
	{
		ScrCls();
		gucSSetup = 0x06;
		iRet = McSetCommParam(KEY2);
		if(iRet)
		{
			if(GetHWBranch() == D210HW_V2x )
			{
				RestoreHandsetBaseLink();
			}
			return;
		}
		iExeModemCnt = tCommParam.iExeModemCnt;
		while(iExeModemCnt > 0)
		{
			iRet = ModemTest();
			if(iRet)
			{
				if(GetHWBranch() == D210HW_V2x )
				{
					RestoreHandsetBaseLink();
				}
				return;
			}
			iExeModemCnt--;
		}
	}
}
int ModemTest(void)
{
	uchar ucRet;
	int iTimerCheck;
	uchar szSendBuf[1024 * 2],szRecvBuf[1024 * 2];
	ushort usDataCnt,usRecvCnt;
	uchar szDispBuf[50];
	int i = 0;
	Menu tModemMenu[4];
	int iSendTimes = tCommParam.iModemSendTimes;
	tCommParam.iModemSucssTimes = 0;
	memset(szDispBuf,0,sizeof(szDispBuf));
	memset(tModemMenu,0,sizeof(tModemMenu));
	AddToMenu(&tModemMenu[0],0,0,0,0x00c1,"MODEM测试","MODEM TEST",NULL);
	AddToMenu(&tModemMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tModemMenu[2],0,0,0,0x1101,"拨号中...","DIALING...",NULL);
	AddToMenu(&tModemMenu[3],0,0,0,0x1401,"","",NULL);
	
	usDataCnt = (tCommParam.iModemSendCnt > (1024 * 2))? (1024 * 2) :tCommParam.iModemSendCnt;
	
	if((ucRet = ModemReset()) != 0x00)
	{
		ModemError(ucRet);
		return 1;
	}
	ScrCls();
	iShowMenu(tModemMenu,4,0,NULL);
	TimerSet(1,600);
	iTimerCheck = TimerCheck(1);
	ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
	if((ucRet = ModemDial(&tCommParam.ModemParam,tCommParam.szModemDialNum,tCommParam.ucMode)) != 0x00)
	{
		OnHook();
		ModemError(ucRet);
		return 1;
	}
	while (ModemCheck() == 0x0a)//拨号中
	{
		if(!kbhit() && getkey()==KEYCANCEL)
		{
			OnHook();
			return 1;
		}
		iTimerCheck = TimerCheck(1);
		if(iTimerCheck == 0)
		{
			OnHook();
			return 1;
		}
		ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
	}

	if ((ucRet = ModemCheck()) != 0x00)
	{
		ModemError(ucRet);
		OnHook();
		return 1;
	}

	AddToMenu(&tModemMenu[2],0,0,0,0x1101,"拨号成功","DAIL OK",NULL);
	ScrCls();
	iShowMenu(tModemMenu,4,0,NULL);

	while (iSendTimes > 0)
	{
		memset(szSendBuf,0,1024 * 2);
		memset(szRecvBuf,0,1024 * 2);
		for(i = 0; i < usDataCnt;i++)
		{
			szSendBuf[i] = i % 256;
		}
		memcpy(szSendBuf,"\x60\x00\x00\x00\x00",5);

		AddToMenu(&tModemMenu[2],0,0,0,0x1101,"发送中...","SENDING...",NULL);
		ScrCls();
		iShowMenu(tModemMenu,4,0,NULL);
		
		if ((ucRet = ModemTxd(szSendBuf,usDataCnt)) != 0x00)
		{
			ModemError(ucRet);
			OnHook();
			return 1;
		}

		//一分钟超时

		AddToMenu(&tModemMenu[2],0,0,0,0x1101,"接收中...","RECVING...",NULL);
		ScrCls();
		iShowMenu(tModemMenu,4,0,NULL);

		TimerSet(0,600);
		while ((ucRet = ModemRxd(szRecvBuf,&usRecvCnt)) != 0x00)
		{
			if (!kbhit() && getkey() == KEYCANCEL)
			{
				OnHook();
				return 1;
			}
			else if (TimerCheck(0) == 0)
			{
				AddToMenu(&tModemMenu[2],0,0,0,0x1101,"接收超时","TIMEOUT",NULL);
				ScrCls();
				iShowMenu(tModemMenu,4,0,NULL);
				GetKeyWait(2000);
				OnHook();
				return 1;
			}
			else if(ucRet == 0x0c)
			{
				ScrPrint(0,5,0x41,"%d",TimerCheck(0)/10);
				continue;
			}
			else
			{
				ModemError(ucRet);
				OnHook();
				return 1;
			}
		}
		
		if (usRecvCnt != usDataCnt)
		{
			AddToMenu(&tModemMenu[2],0,0,0,0x1101,"接收长度错误","RECV LEN ERR",NULL);
			ScrCls();
			iShowMenu(tModemMenu,4,0,NULL);
			GetKeyWait(2000);
			OnHook();
			return 1;
		}

		if (memcmp(szRecvBuf,szSendBuf,usDataCnt) != 0)
		{
			AddToMenu(&tModemMenu[2],0,0,0,0x1101,"接收数据错误","RECV DATA ERR",NULL);
			ScrCls();
			iShowMenu(tModemMenu,4,0,NULL);
			GetKeyWait(2000);
			OnHook();
			return 1;
		}
		iSendTimes--;
		tCommParam.iModemSucssTimes++;
	}		

	if ((ucRet = OnHook()) != 0x00)
	{
		ModemError(ucRet);
		return 1;
	}
	
	DelayMs(2000);		
	AddToMenu(&tModemMenu[2],0,0,0,0x1101,"MODEM测试成功","MODEM TEST OK",NULL);
	ScrCls();
	iShowMenu(tModemMenu,4,0,NULL);
	GetKeyWait(2000);
	return 0;
}

uchar ucDHCPFlag = 0;
void McCheckETH(void)
{
	int iSelectItem;
	int iTimerCheck;
	int iTimerBack = 0;
	uchar szDispBuf[50];
	uchar ucRet;
	int iRet = 0;
	struct net_sockaddr serve_addr;
	int iSocket,i;
	Menu tETHMenu[4];
	char txBuf[9000];
	char rxBuf[9000];
	int txLen = 0,rxLen = 0;
	int iWidth,iHeight;
	int route_bak;

	// D210的机器可能需要根据对应的座机切换到蓝牙链路
	if(GetHWBranch() == D210HW_V2x )
	{
		if(GetBaseVer() != 0) return;
		if(1 == GetLinkType() && base_info.bt_exist)			
		{
			// 创建蓝牙链路
			if(CreateHandsetBaseLink(1)) return;
		}
		else if((0 == GetLinkType() && base_info.bt_exist) || (1 == GetLinkType() && 0 == base_info.bt_exist))
		{
			return;
		}
	}
	ScrGetLcdSize(&iWidth,&iHeight);
	memset(txBuf,0,sizeof(txBuf));
	memset(rxBuf,0,sizeof(rxBuf));
	memset(tETHMenu,0,sizeof(tETHMenu));
	memset(szDispBuf,0,sizeof(szDispBuf));
	route_bak = RouteGetDefault();
	RouteSetDefault(0);
	while(1)
	{
		AddToMenu(&tETHMenu[0],0,0,0,0x00c1,"以太网测试","ETHERNET TEST",NULL);
		AddToMenu(&tETHMenu[1],0,0,0,0x1001,"1-Static","1-Static",NULL);
		AddToMenu(&tETHMenu[2],0,0,0,0x1001,"2-DHCP","2-DHCP",NULL);
		AddToMenu(&tETHMenu[3],0,0,0,0x1401,"","",NULL);
		ScrCls();
		iShowMenu(tETHMenu,4,0,NULL);
		ucRet = getkey();
		
		if(ucRet == KEY1)
		{
			ucDHCPFlag = 0x00;
		}
		else if(ucRet == KEY2)
		{
			ucDHCPFlag = 0x01;
		}
		else if(ucRet == KEYCANCEL)
		{
			RouteSetDefault(route_bak);
			if(GetHWBranch() == D210HW_V2x )
			{
				RestoreHandsetBaseLink();
			}
			return;
		}
		else
		{
			continue;
		}
		iRet = 0;
		iRet = McSetCommParam(KEY3);
		if(iRet)
		{
			RouteSetDefault(route_bak);
			if(GetHWBranch() == D210HW_V2x )
			{
				RestoreHandsetBaseLink();
			}
			return;
		}
		AddToMenu(&tETHMenu[1],0,0,0,0x1400,"","",NULL);
		AddToMenu(&tETHMenu[2],0,0,0,0x1101,"设置网络参数...","SET NETPARA...",NULL);
		AddToMenu(&tETHMenu[3],0,0,0,0x1401,"","",NULL);
		if(iHeight == 272) //MT30
		{
			tETHMenu[1].uiAttr = 0x1401;
		}
		ScrCls();
		iShowMenu(tETHMenu,4,0,NULL);
		TimerSet(1,600);
		iTimerCheck = TimerCheck(1);
		if(iHeight == 272) //MT30
		{
			ScrPrint(0,6,0x41,"%d",iTimerCheck/10);
		}
		else
		{
			ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
		}
		
		if(tCommParam.ucDHCPFlag == 0x00)
		{
			if((iRet = EthSet(tCommParam.szLoclIP, tCommParam.szMaskIP, tCommParam.szGetWayIP, NULL))<0)
			{
				RouteSetDefault(route_bak);
				return EthError(iRet);
			}
			iTimerCheck = TimerCheck(1);
			if(iHeight == 272) //MT30
			{
				ScrPrint(0,6,0x41,"%d",iTimerCheck/10);
			}
			else
			{
				ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
			}

		}
		else if(tCommParam.ucDHCPFlag == 0x01)
		{
			DhcpStop();
			if((iRet = DhcpStart())<0)
			{
				RouteSetDefault(route_bak);
				return EthError(iRet);
			}
			TimerSet(2,800);
			while((iRet = DhcpCheck()) != 0)
			{
				iTimerCheck = TimerCheck(1);
				if(iHeight == 272) //MT30
				{
					if(iTimerBack != iTimerCheck/10)
					{
						ScrPrint(0,6,0x41,"%d",iTimerCheck/10);
					}
				}
				else
				{
					if(iTimerBack != iTimerCheck/10)
					{
						ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
					}
				}
				iTimerBack = iTimerCheck/10;

				if(TimerCheck(2) == 0)
				{
					RouteSetDefault(route_bak);
					return EthError(iRet);
				}
				if(!kbhit() && getkey() == KEYCANCEL)
				{
					RouteSetDefault(route_bak);
					if(GetHWBranch() == D210HW_V2x )
					{
						RestoreHandsetBaseLink();
					}
					return;
				}
			}
			
		}
		
		if((iRet = SockAddrSet(&serve_addr,(char *)tCommParam.szHostIP,(short)atoi(tCommParam.szHostPort) ))<0)
		{
			RouteSetDefault(route_bak);
			return EthError(iRet);
		}
		iTimerCheck = TimerCheck(1);
		if(iHeight == 272) //MT30
		{
			ScrPrint(0,6,0x41,"%d",iTimerCheck/10);
		}
		else
		{
			ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
		}

		if((iSocket = NetSocket(NET_AF_INET,NET_SOCK_STREAM,0)) < 0)
		{
			NetCloseSocket(iSocket);
			RouteSetDefault(route_bak);
			return EthError(iRet);
		}
		iTimerCheck = TimerCheck(1);
		if(iHeight == 272) //MT30
		{
			ScrPrint(0,6,0x41,"%d",iTimerCheck/10);
		}
		else
		{
			ScrPrint(0,5,0x41,"%d",iTimerCheck/10);
		}


		if(iRet = NetPing(tCommParam.szHostIP,tCommParam.iPingTimeout,tCommParam.iPingSize) >= 0)
		{	
			//设置套接字属性
			Netioctl(iSocket, CMD_TO_SET, 60*1000);
			//TCP连接
			iRet=NetConnect(iSocket, &serve_addr, sizeof(serve_addr));
			if(iRet || (!kbhit() && getkey()==KEYCANCEL))
			{
				NetCloseSocket(iSocket);
				RouteSetDefault(route_bak);
				return EthError(iRet);
			}
			//发送数据
			txLen = (tCommParam.iPingSize > 8192) ? 8192 : tCommParam.iPingSize;
			for (i=0;i<txLen;i++)
			{
				txBuf[i] = i % 256;
			}
			iRet = NetSend(iSocket,txBuf,txLen,0);
			if(iRet < txLen || (!kbhit() && getkey()==KEYCANCEL))
			{
				NetCloseSocket(iSocket);
				RouteSetDefault(route_bak);
				return EthError(iRet);
			}
			//接收数据
			TimerSet(2,30);
			rxLen = 0;
			while(rxLen < txLen && TimerCheck(2))
			{
			    iRet = NetRecv(iSocket, rxBuf+rxLen, 9000, 0);
			    if (iRet < 0) break;
			    rxLen += iRet;
			}
			if(rxLen < txLen || (!kbhit() && getkey()==KEYCANCEL))
			{
				NetCloseSocket(iSocket);
				RouteSetDefault(route_bak);
				return EthError(iRet);
			}
			if(memcmp(txBuf,rxBuf,rxLen))
			{
				NetCloseSocket(iSocket);
				RouteSetDefault(route_bak);
				return EthError(iRet);
			}
			NetCloseSocket(iSocket);
			AddToMenu(&tETHMenu[2],0,0,0,0x1400,"","",NULL);
			AddToMenu(&tETHMenu[3],0,0,0,0x1101,"以太网测试成功!","ETHERNET OK!",NULL);
			if(iHeight == 272) //MT30
			{
				tETHMenu[2].uiAttr = 0x1401;
			}
			ScrCls();
			iShowMenu(tETHMenu,4,0,NULL);
			ucRet = getkey();
			if(ucRet == KEYCANCEL)
			{
				RouteSetDefault(route_bak);
				if(GetHWBranch() == D210HW_V2x )
				{
					RestoreHandsetBaseLink();
				}
				return;
			}
			else
			{
				continue;
			}
		}
		NetCloseSocket(iSocket);
		AddToMenu(&tETHMenu[2],0,0,0,0x1400,"","",NULL);
		AddToMenu(&tETHMenu[3],0,0,0,0x1101,"以太网测试失败!","ETHERNET FAIL!",NULL);
		if(iHeight == 272) //MT30
		{
			tETHMenu[2].uiAttr = 0x1401;
		}
		ScrCls();
		iShowMenu(tETHMenu,4,0,NULL);
		while(2)
		{
			ucRet = getkey();
			if(ucRet == KEYENTER)
			{
				RouteSetDefault(route_bak);
				if(GetHWBranch() == D210HW_V2x )
				{
					RestoreHandsetBaseLink();
				}
				return;
			}
		}
	}

}
void McCheckWNET(void)
{
	Menu tWNETMenu[4];
	uchar ucRet,pSignalLevel = 0,sim_no=0;
	int iRet = 0,iRet1 = 0,i;
	struct net_sockaddr serve_addr;
	int iSocket;
	int iTimerCheck;
	uchar szDispBuf[50];
	char txBuf[9000];
	char rxBuf[9000];
	int txLen = 0,rxLen = 0;
	uchar SignalLevelOut = 0,SignalLevelBak = -1,SigMaxLevel;
	uchar ucTermName[16];
	uchar out_buf[30];
	int route_bak;
	iRet = McSetCommParam(KEY5);
	if(iRet)
	{
		return;
	}
	memset(ucTermName,0,sizeof(ucTermName));
	memset(txBuf,0,sizeof(txBuf));
	memset(rxBuf,0,sizeof(rxBuf));
	memset(szDispBuf,0,sizeof(szDispBuf));
	memset(tWNETMenu,0,sizeof(tWNETMenu));
	//第一步,开电源
	WlSwitchPower(1);

	sim_no = 0;//单sim卡编号为0
	if(is_double_sim())
	{
    	AddToMenu(&tWNETMenu[0],0,0,0,0x00c1,"WNET测试","WNET TEST",NULL);
    	AddToMenu(&tWNETMenu[1],0,0,0,0x1001,"请选择SIM卡","PLS SELECT SIM",NULL);
    	AddToMenu(&tWNETMenu[2],0,0,0,0x1001,"1-SIM0","1-SIM0",NULL);
    	AddToMenu(&tWNETMenu[3],0,0,0,0x1001,"2-SIM1","2-SIM1",NULL);
    	ScrCls();
    	iShowMenu(tWNETMenu,4,0,NULL);
        ucRet = GetKeyWait(3000);
        if(ucRet==KEY1) sim_no=0;
        else if(ucRet==KEY2) sim_no=1;
        else sim_no = 0;

        AddToMenu(&tWNETMenu[0],0,0,0,0x00c1,"WNET测试","WNET TEST",NULL);
        AddToMenu(&tWNETMenu[1],0,0,0,0x1400,"","",NULL);
        AddToMenu(&tWNETMenu[2],0,0,0,0x1400,"","",NULL);
        AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"切换SIM中...","PLS WAIT...",NULL);
        ScrCls();
        iShowMenu(tWNETMenu,4,0,NULL);
    }
	//第二步,选卡
	memset(out_buf, 0, sizeof(out_buf));
	GetTermInfo(out_buf);
	if (out_buf[0] != 11)//S78机型没有WlSelSim函数
	{
		iRet = WlSelSim(sim_no);
		if(iRet)
		{
			WlClosePort();
			WlSwitchPower(0);
			return WnetError(iRet);
		}
	}

	AddToMenu(&tWNETMenu[0],0,0,0,0x00c1,"无线测试","WNET TEST",NULL);
	AddToMenu(&tWNETMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tWNETMenu[2],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"无线拨号中...","WNET DIALING...",NULL);
	ScrCls();
	iShowMenu(tWNETMenu,4,0,NULL);
	//第三步,初始化
	iRet = WlInit(NULL);
	if(iRet != 0 && iRet != -212)
	{
		WlSwitchPower(0);
		return WnetError(iRet);
	}
	
	
	if(!kbhit() && getkey()==KEYCANCEL)
	{
		WlClosePort();
		WlSwitchPower(0);
		return;
	}
	//第四步,拨号
	WlPppLogin(tCommParam.szWNetDailNum,tCommParam.szUID,tCommParam.szPWD,0xff,0,60);
	TimerSet(1,600);
	while(1)
	{
		if(TimerCheck(1) == 0)
		{
			WlPppLogout();
			WlClosePort();
			WlSwitchPower(0);
			ScrSetIcon(ICON_SIGNAL,0);
			return WnetError(iRet);
		}
		if(!kbhit() && getkey()==KEYCANCEL)
		{
			WlPppLogout();
			WlClosePort();
			WlSwitchPower(0);
			ScrSetIcon(ICON_SIGNAL,0);
			return;
		}
		//设置图标信号值
		WlGetSignal(&pSignalLevel);
		ScrSetIcon(ICON_SIGNAL,6 - pSignalLevel);
        SigMaxLevel = 6;
        SignalLevelOut = 6 - pSignalLevel;
		sprintf(tWNETMenu[2].acChnName,"信号:%02d/%02d",SignalLevelOut,SigMaxLevel);
		sprintf(tWNETMenu[2].acEngName,"SIG:%02d/%02d",SignalLevelOut,SigMaxLevel);
		tWNETMenu[2].uiAttr = 0x1101;
		if(SignalLevelOut != SignalLevelBak)
		{
			iShowMenu(tWNETMenu,4,0,NULL);
			SignalLevelBak = SignalLevelOut;
		}
		//第五步,检测连接状态
		iRet=WlPppCheck();
		if(iRet==1) 
		{
			continue;
		}
		else
		{
			break;
		}
	}
	if(iRet)
	{
		WlPppLogout();
		WlClosePort();
		WlSwitchPower(0);
		ScrSetIcon(ICON_SIGNAL,0);
		return WnetError(iRet);
	}
	AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"按确认键继续","PRS ENTER",NULL);
	WlGetSignal(&pSignalLevel);
	ScrSetIcon(ICON_SIGNAL,6 - pSignalLevel);
    SigMaxLevel = 6;
    SignalLevelOut = 6 - pSignalLevel;
	sprintf(tWNETMenu[2].acChnName,"信号:%02d/%02d",SignalLevelOut,SigMaxLevel);
	sprintf(tWNETMenu[2].acEngName,"SIG:%02d/%02d",SignalLevelOut,SigMaxLevel);
	tWNETMenu[2].uiAttr = 0x1101;
	iShowMenu(tWNETMenu,4,0,NULL);
	SignalLevelBak = SignalLevelOut;
	while(1)
	{
		if(!kbhit())
		{
			ucRet = getkey();
			if(ucRet == KEYCANCEL)
			{
				WlPppLogout();
				WlClosePort();
				WlSwitchPower(0);
				ScrSetIcon(ICON_SIGNAL,0);
				return;
			}
			else if(ucRet == KEYENTER)
			{
				break;
			}
		}
		WlGetSignal(&pSignalLevel);
		ScrSetIcon(ICON_SIGNAL,6 - pSignalLevel);
        SigMaxLevel = 6;
        SignalLevelOut = 6 - pSignalLevel;
		sprintf(tWNETMenu[2].acChnName,"信号:%02d/%02d",SignalLevelOut,SigMaxLevel);
		sprintf(tWNETMenu[2].acEngName,"SIG:%02d/%02d",SignalLevelOut,SigMaxLevel);
		tWNETMenu[2].uiAttr = 0x1101;
		if(SignalLevelOut != SignalLevelBak)
		{
			iShowMenu(tWNETMenu,4,0,NULL);
			SignalLevelBak = SignalLevelOut;
		}
	}
	AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"无线测试中...","WNET TESTING...",NULL);
	WlGetSignal(&pSignalLevel);
	ScrSetIcon(ICON_SIGNAL,6 - pSignalLevel);
    SigMaxLevel = 6;
    SignalLevelOut = 6 - pSignalLevel;
	sprintf(tWNETMenu[2].acChnName,"信号:%02d/%02d",SignalLevelOut,SigMaxLevel);
	sprintf(tWNETMenu[2].acEngName,"SIG:%02d/%02d",SignalLevelOut,SigMaxLevel);
	tWNETMenu[2].uiAttr = 0x1101;
	iShowMenu(tWNETMenu,4,0,NULL);

	route_bak = RouteGetDefault();
	RouteSetDefault(11);
	//第六步，建立连接套接字
	SockAddrSet(&serve_addr, tCommParam.szDespIP, atoi(tCommParam.szDespPort));
	iSocket = NetSocket(NET_AF_INET,NET_SOCK_STREAM,0);
	//第七步，设置套接字属性
	Netioctl(iSocket, CMD_TO_SET, 60*1000);
	//第八步，TCP连接
	iRet=NetConnect(iSocket, &serve_addr, sizeof(serve_addr));
	if(iRet)
	{
		NetCloseSocket(iSocket);
		WlPppLogout();
		WlClosePort();
		WlSwitchPower(0);
		ScrSetIcon(ICON_SIGNAL,0);
		RouteSetDefault(route_bak);
		return WnetError(iRet);
	}
	if(!kbhit() && getkey()==KEYCANCEL)
	{
		NetCloseSocket(iSocket);
		WlPppLogout();
		WlClosePort();
		WlSwitchPower(0);
		ScrSetIcon(ICON_SIGNAL,0);
		RouteSetDefault(route_bak);
		return;
	}
	WlGetSignal(&pSignalLevel);
	ScrSetIcon(ICON_SIGNAL,6 - pSignalLevel);
    SigMaxLevel = 6;
    SignalLevelOut = 6 - pSignalLevel;
	sprintf(tWNETMenu[2].acChnName,"信号:%02d/%02d",SignalLevelOut,SigMaxLevel);
	sprintf(tWNETMenu[2].acEngName,"SIG:%02d/%02d",SignalLevelOut,SigMaxLevel);
	tWNETMenu[2].uiAttr = 0x1101;
	iShowMenu(tWNETMenu,4,0,NULL);
	//第九步，发送数据
	txLen = (tCommParam.iWNetSendCnt > 8192) ? 8192 : tCommParam.iWNetSendCnt;
	for (i=0;i<txLen;i++)
	{
		txBuf[i] = i % 256;
	}
	iRet = NetSend(iSocket,txBuf,txLen,0);
	if(iRet < txLen)
	{
		NetCloseSocket(iSocket);
		WlPppLogout();
		WlClosePort();
		WlSwitchPower(0);
		ScrSetIcon(ICON_SIGNAL,0);
		RouteSetDefault(route_bak);
		return WnetError(iRet);
	}
	if(!kbhit() && getkey()==KEYCANCEL)
	{
		NetCloseSocket(iSocket);
		WlPppLogout();
		WlClosePort();
		WlSwitchPower(0);
		ScrSetIcon(ICON_SIGNAL,0);
		RouteSetDefault(route_bak);
		return;
	}
	WlGetSignal(&pSignalLevel);
	ScrSetIcon(ICON_SIGNAL,6 - pSignalLevel);
    SigMaxLevel = 6;
    SignalLevelOut = 6 - pSignalLevel;
	if(SignalLevelOut != SignalLevelBak)
	{
		sprintf(tWNETMenu[2].acChnName,"信号:%02d/%02d",SignalLevelOut,SigMaxLevel);
		sprintf(tWNETMenu[2].acEngName,"SIG:%02d/%02d",SignalLevelOut,SigMaxLevel);
		tWNETMenu[2].uiAttr = 0x1101;
		iShowMenu(tWNETMenu,4,0,NULL);
	}
	SignalLevelBak = SignalLevelOut;
	//第十步，接收数据
	TimerSet(1,600);
	rxLen = 0;
	while(1)
	{
		if(!kbhit() && getkey()==KEYCANCEL)
		{
			NetCloseSocket(iSocket);
			WlPppLogout();
			WlClosePort();
			WlSwitchPower(0);
			ScrSetIcon(ICON_SIGNAL,0);
			RouteSetDefault(route_bak);
			return;
		}
		if(TimerCheck(1) == 0)
		{
			if(rxLen < txLen)
			{
				NetCloseSocket(iSocket);
				WlPppLogout();
				WlClosePort();
				WlSwitchPower(0);
				ScrSetIcon(ICON_SIGNAL,0);
				RouteSetDefault(route_bak);
				return WnetError(-13);//返回超时
			}
			break;
		}
		if(rxLen >= txLen)
		{
			break;
		}
		iRet = NetRecv(iSocket, rxBuf+rxLen, 9000, 0);
		rxLen += iRet;
	}
	if(memcmp(txBuf,rxBuf,iRet))
	{
		NetCloseSocket(iSocket);
		WlPppLogout();
		WlClosePort();
		WlSwitchPower(0);
		ScrSetIcon(ICON_SIGNAL,0);
		RouteSetDefault(route_bak);
		return WnetError(iRet);
	}
	WlGetSignal(&pSignalLevel);
	ScrSetIcon(ICON_SIGNAL,6 - pSignalLevel);
    SigMaxLevel = 6;
    SignalLevelOut = 6 - pSignalLevel;
	sprintf(tWNETMenu[2].acChnName,"信号:%02d/%02d",SignalLevelOut,SigMaxLevel);
	sprintf(tWNETMenu[2].acEngName,"SIG:%02d/%02d",SignalLevelOut,SigMaxLevel);
	tWNETMenu[2].uiAttr = 0x1101;
	iShowMenu(tWNETMenu,4,0,NULL);
	//第十一步，关闭套接字
	NetCloseSocket(iSocket);
	//第十二步，端口PPP连接
	WlPppLogout();
	TimerSet(1,100);
	while(1)
	{
		//第十三步,等待断开成功
		if(TimerCheck(1) == 0)
		{
			WlClosePort();
			WlSwitchPower(0);
			ScrSetIcon(ICON_SIGNAL,0);
			RouteSetDefault(route_bak);
			return;
		}
		if(!kbhit() && getkey()==KEYCANCEL)
		{
			WlClosePort();
			WlSwitchPower(0);
			ScrSetIcon(ICON_SIGNAL,0);
			RouteSetDefault(route_bak);
			return;
		}
		iRet=WlPppCheck();
		if(iRet==1) 
		{
			continue;
		}
		else
		{
			break;
		}
		
	}
	//第十四步,关闭电源
	WlSwitchPower(0);
	//测试成功
	AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"无线测试完成!","WNET TEST OK!",NULL);
	tWNETMenu[2].uiAttr = 0x1400;
	ScrCls();
	iShowMenu(tWNETMenu,4,0,NULL);
	while(1)
	{
		ucRet  = getkey();
		if(ucRet == KEYENTER)
		{
			ScrSetIcon(ICON_SIGNAL,0);
			RouteSetDefault(route_bak);
			return;
		}
	}

}

uchar McDispTrack(uchar ucResult, uchar *szTrack1, uchar *szTrack2, uchar *szTrack3, Menu tMageMenu[])
{
	uchar ucKey;
	uchar buf[7],szDispBuf[50];
	int i = 0, iTrackLen = 0;
	int iWidth,iHeight;
	memset(buf, 0, sizeof(buf));
	memset(szDispBuf,0,sizeof(szDispBuf));
	ScrGetLcdSize(&iWidth,&iHeight);
	switch(ucResult)
	{
		case 0x01:
			AddToMenu(&tMageMenu[2],0,0,0,0x1101,"1磁正确:","READ TRACK1 OK:",NULL);
			AddToMenu(&tMageMenu[3],0,0,0,0x1101,"MagRead()=0x01","MagRead()=0x01",NULL);
		break;
		case 0x02:
			AddToMenu(&tMageMenu[2],0,0,0,0x1101,"2磁正确:","READ TRACK2 OK:",NULL);
			AddToMenu(&tMageMenu[3],0,0,0,0x1101,"MagRead()=0x02","MagRead()=0x02",NULL);
		break;
		case 0x03:
			AddToMenu(&tMageMenu[2],0,0,0,0x1101,"1、2磁正确:","READ TRACK1/2 OK:",NULL);
			AddToMenu(&tMageMenu[3],0,0,0,0x1101,"MagRead()=0x03","MagRead()=0x03",NULL);
		break;
		case 0x04:
			AddToMenu(&tMageMenu[2],0,0,0,0x1101,"3磁正确:","READ TRACK3 OK:",NULL);
			AddToMenu(&tMageMenu[3],0,0,0,0x1101,"MagRead()=0x04","MagRead()=0x04",NULL);
		break;
		case 0x05:
			AddToMenu(&tMageMenu[2],0,0,0,0x1101,"1、3磁正确:","READ TRACK1/3 OK:",NULL);
			AddToMenu(&tMageMenu[3],0,0,0,0x1101,"MagRead()=0x05","MagRead()=0x05",NULL);
		break;
		case 0x06:
			AddToMenu(&tMageMenu[2],0,0,0,0x1101,"2、3磁正确:","READ TRACK2/3 OK:",NULL);
			AddToMenu(&tMageMenu[3],0,0,0,0x1101,"MagRead()=0x06","MagRead()=0x06",NULL);
		break;
		case 0x07:
			AddToMenu(&tMageMenu[2],0,0,0,0x1101,"1、2、3磁正确:","READ TRACK1/2/3 OK:",NULL);
			AddToMenu(&tMageMenu[3],0,0,0,0x1101,"MagRead()=0x07","MagRead()=0x07",NULL);
		break;		
		default:
			AddToMenu(&tMageMenu[2],0,0,0,0x1101,"读卡错误!","MagRead Error!",NULL);
			AddToMenu(&tMageMenu[3],0,0,0,0x1101,"","",NULL);
			sprintf((char *)szDispBuf,"MagRead()=0x%2X",ucResult);
			strcpy(tMageMenu[3].acChnName,szDispBuf);
			strcpy(tMageMenu[3].acEngName,szDispBuf);
		break;
	}
	ScrCls();
	iShowMenu(tMageMenu,4,0,NULL);
	if(ucResult == 0x00 || ucResult > 0x07)
	{
		while(1)
		{
			ucKey = getkey();
			if(ucKey == KEYENTER)
			{
				return 0x01;//返回待刷卡界面
			}
		}
	}
	else
	{
		while(1)
		{
			ucKey = getkey();
			if(ucKey == KEYENTER)
			{
				break;
			}
			else if(ucKey == KEYCANCEL)
			{
				return 0x01;//返回待刷卡界面
			}
		}
		if(iHeight == 272)
		{
			ScrClrLine(2,16);
			iTrackLen = strlen(szTrack1) - 1;
			ScrPrint(0,2,0x01,"Track1: len=[%d]",iTrackLen);
			if(iTrackLen > 6)
			{
				memcpy(buf, szTrack1, 6);
				buf[6] = 0x00;
				ScrPrint(0,4,0x01,"%s******",buf);
			}
			else
			{
				strcpy(buf, szTrack1);
				ScrPrint(0,4,0x01,"%s",buf);
			}
			iTrackLen = strlen(szTrack2) - 1;
			ScrPrint(0,6,0x01,"Track2: len=[%d]",iTrackLen);
			if(iTrackLen > 6)
			{
				memcpy(buf, szTrack2, 6);
				buf[6] = 0x00;
				ScrPrint(0,8,0x01,"%s******",buf);
			}
			else
			{
				strcpy(buf, szTrack2);
				ScrPrint(0,8,0x01,"%s",buf);
			}
			iTrackLen = strlen(szTrack3) - 1;
			ScrPrint(0,10,0x01,"Track3: len=[%d]",iTrackLen);
			if(iTrackLen > 6)
			{
				memcpy(buf, szTrack3, 6);
				buf[6] = 0x00;
				ScrPrint(0,12,0x01,"%s******",buf);
			}
			else
			{
				strcpy(buf, szTrack3);
				ScrPrint(0,12,0x01,"%s",buf);
			}
		}
		else
		{
			ScrClrLine(2,7);
			iTrackLen = strlen(szTrack1) - 1;
			ScrPrint(0,2,0x00,"Track1: len=[%d]",iTrackLen);
			if(iTrackLen > 6)
			{
				memcpy(buf, szTrack1, 6);
				buf[6] = 0x00;
				ScrPrint(0,3,0x00,"%s******",buf);
			}
			else
			{
				strcpy(buf, szTrack1);
				ScrPrint(0,3,0x00,"%s",buf);
			}
			iTrackLen = strlen(szTrack2) - 1;
			ScrPrint(0,4,0x00,"Track2: len=[%d]",iTrackLen);
			if(iTrackLen > 6)
			{
				memcpy(buf, szTrack2, 6);
				buf[6] = 0x00;
				ScrPrint(0,5,0x00,"%s******",buf);
			}
			else
			{
				strcpy(buf, szTrack2);
				ScrPrint(0,5,0x00,"%s",buf);
			}
			iTrackLen = strlen(szTrack3) - 1;
			ScrPrint(0,6,0x00,"Track3: len=[%d]",iTrackLen);
			if(iTrackLen > 6)
			{
				memcpy(buf, szTrack3, 6);
				buf[6] = 0x00;
				ScrPrint(0,7,0x00,"%s******",buf);
			}
			else
			{
				strcpy(buf, szTrack3);
				ScrPrint(0,7,0x00,"%s",buf);
			}
		}
		while(1)
		{
			ucKey = getkey();
			if(ucKey == KEYENTER)
			{
				return 0x01;//返回待刷卡界面
			}
			else if(ucKey == KEYCANCEL)
			{
				return 0x02;//返回模块检测界面
			}
		}
	}
	
}
uchar GetKeyWait(int Ms)
{
	unsigned char ucRet;

	TimerSet(1,Ms/100);
	while(1)
	{
		if(!kbhit())
		{
			ucRet = getkey();
			return ucRet;
		}
		if(TimerCheck(1) == 0)
		{
			return 0;
		}
	}
}
//-------保存通讯参数
uchar McStoreCommParam()
{
	int iFid,len;

	s_remove("COMMPARAM","\xff\x05");

    iFid = s_open("COMMPARAM",O_CREATE|O_RDWR,"\xff\x05");
	if (iFid< 0)return 1;

	seek(iFid,0,SEEK_SET);

    len = write(iFid,(uchar *)&tCommParam,sizeof(TCommParam));
	close(iFid);
	if( len != sizeof(TCommParam))return 1;

	return 0;
}

//--------读取通讯参数
void McLoadCommParam()
{
	int iFid;

	memset((uchar *)&tCommParam,0,sizeof(TCommParam));
	if (s_SearchFile("COMMPARAM","\xff\x05") < 0)
	{
		tCommParam.ModemParam.AsMode = 0;
		tCommParam.ModemParam.CHDT = 0;
		tCommParam.ModemParam.DP = 5;
		tCommParam.ModemParam.DT1 = 5;
		tCommParam.ModemParam.DT2 = 5;
		tCommParam.ModemParam.DTIMES = 1;
		tCommParam.ModemParam.HT = 50;
		tCommParam.ModemParam.SSETUP = 6;
		tCommParam.ModemParam.TimeOut = 2;
		tCommParam.ModemParam.WT = 5;
		tCommParam.ucMode = 1;//测试不作预拨号
		tCommParam.iUARTSendCnt = 10;
		tCommParam.iUARTSendTimes = 1;
		strcpy(tCommParam.szModemDialNum,"9,86169709");
		tCommParam.iExeModemCnt = 1;
		tCommParam.ucRecvCommN = COM1;
		tCommParam.ucSendCommN = COM1;
		tCommParam.iExeUARTCnt = 1;
		strcpy(tCommParam.szCommParamStr,"9600,8,n,1");
		tCommParam.iModemSendCnt = 10;
		tCommParam.iModemSendTimes = 1;
		strcpy(tCommParam.szPinHostIP,"192.168.000.001");
		strcpy(tCommParam.szLoclIP,"192.168.000.002");
		strcpy(tCommParam.szGetWayIP,"192.168.000.254");
		strcpy(tCommParam.szMaskIP,"255.255.254.000");
		strcpy(tCommParam.szDnsIP,"192.168.000.111");
		strcpy(tCommParam.szHostIP,"192.168.000.001");
		strcpy(tCommParam.szHostPort,"60180");
		tCommParam.iPingSize = 1024;
		tCommParam.iPingTimeout = 10000;
		//WNET
		strcpy(tCommParam.szWNetDailNum,"CMNET");
		strcpy(tCommParam.szUID,"card");
		strcpy(tCommParam.szPWD,"card");
		strcpy(tCommParam.szDespIP,"219.134.185.069");
		strcpy(tCommParam.szDespPort,"60180");
		tCommParam.iWNetSendCnt = 512;
	//	ScrPrint(0,4,CFONT,"%s",tCommParam.szCommParamStr);getkey();
	//	McStoreCommParam();
	}
	else
	{
        iFid = s_open("COMMPARAM",O_CREATE|O_RDWR,"\xff\x05");
		if (iFid< 0)return;
		
		seek(iFid,0,SEEK_SET);
		read(iFid,(uchar *)&tCommParam,sizeof(TCommParam));
		close(iFid);
	}
}
uchar McCheckIpStr(uchar *IpAddR, uchar ucLen)
{
	uchar ucPlace = 0;
	int iDuValue;
	uchar ucCnt;
	uchar szDuStr[4];

	memset(szDuStr,0,4);

	if (ucLen > 16)
	{
		return 1;
	}

	for (ucCnt = 0; ucCnt < ucLen ; ucCnt++)
	{
		if (IpAddR[ucCnt] == '.')
		{
			memcpy(szDuStr,IpAddR + ucPlace,ucCnt - ucPlace);
			ucPlace = ucCnt + 1;
			iDuValue = atoi(szDuStr);
			if (iDuValue > 255)
			{
				return 1;
			}
			memset(szDuStr,0,4);
		}		
	}

	if (IpAddR[ucLen] != '.')
	{
		memcpy(szDuStr,IpAddR + ucPlace,ucCnt - ucPlace);
		iDuValue = atoi(szDuStr);
		if (iDuValue > 255)
		{
			return 1;
		}		
	}
	return 0;
}
void McRemoveDot(uchar * ptrDest,uchar * ptrSource)
{
	memset(ptrDest,0,12);
	memcpy(ptrDest,ptrSource,3);
	memcpy(ptrDest + 3,ptrSource + 4,3);
	memcpy(ptrDest + 6,ptrSource + 8,3);
	memcpy(ptrDest + 9,ptrSource + 12,3);
}

void McAddDot(uchar * ptrDest,uchar * ptrSource)
{
	int i = 0;
	for(i = 0;i < 16;i++)
	{	
		if(ptrSource[i] == 0x20)
		{
			break;
		}
	}
	memset(ptrDest,0,16);
	memcpy(ptrDest,ptrSource,3);
	if(i < 4) return;
	ptrDest[3] = 0x2E;
	memcpy(ptrDest + 4,ptrSource + 3,3);
	if(i < 7) return;
	ptrDest[7] = 0x2E;	
	memcpy(ptrDest + 8,ptrSource + 6,3);
	if(i < 10) return;
	ptrDest[11] = 0x2E;	
	memcpy(ptrDest + 12,ptrSource + 9,3);
}
uchar McGetIpAddrStr(uchar * StrOut,uchar ucLen,uchar ucRow)
{
	uchar ucKey;
	uchar ucLenth;
	uchar StrConv[30];
	uchar StrFull[30];
	memset(StrConv,0,30);
	memset(StrFull,0,30);
	
	sprintf((char *)StrFull,"%s",StrOut);
//	memset(StrOut,0,12);
//	ScrClrLine(ucRow,ucRow + 1);
//	McAddDot(StrFull,StrConv);

	ScrPrint(0,ucRow,CFONT|0x40,StrFull);
	McRemoveDot(StrConv,StrFull);
	ucLenth = strlen(StrConv);

	if (StrConv[0] == 0x20)
	{
		ucLenth = 0;
	}
	
	while (1) 
	{
		ucKey = GetKeyWait(60 * 1000);
		if(ucKey == 0) 
		{
			return 1;//等待按键超时
		}
		if ((ucKey == KEYENTER)&&(ucLenth == ucLen))
		{
			sprintf(StrOut,"%s",StrFull);
			return 0;
		}
		else if (ucKey == KEYCANCEL)
		{
			return 1;
		}
		else if ((ucKey == KEYF5 || ucKey == KEYMENU)&&(ucLenth > 0))
		{
			if (ucLenth > 0)
			{
				ucLenth--;
				StrConv[ucLenth]=0x20;
			}
		}
		else if (ucKey == KEYCLEAR)
		{
		//	ucLenth = 0;
		//	memset(StrConv,0x20,ucLen);
			if (ucLenth > 0)
			{
				ucLenth--;
				StrConv[ucLenth]=0x20;
			}
		}
		else
		{
			switch(ucKey)
			{
			case KEY0:
			case KEY1:
			case KEY2:
			case KEY3:
			case KEY4:
			case KEY5:
			case KEY6:
			case KEY7:
			case KEY8:
			case KEY9:
				if (ucLenth >= ucLen)
				{
					Beep();
					break;					
				}
				StrConv[ucLenth] = ucKey;
				ucLenth++;
				break;
			default:
				Beep();
				break;
			}
		}
		ScrClrLine(ucRow,ucRow + 1);
		memset(StrFull,0x20,20);
		McAddDot(StrFull,StrConv);	
		ScrPrint(0,ucRow,CFONT|0x40,StrFull);
	}
}
int McSetLanParam()
{
	uchar szDispBuf[50],szGetBuf[50];
	Menu tSetLanMenu[4];
	int iSelectedItem;
	uchar ucRet;
	int iWidth, iHeight;
	ScrGetLcdSize(&iWidth, &iHeight);
	memset(tSetLanMenu,0,sizeof(tSetLanMenu));
	AddToMenu(&tSetLanMenu[0],0,0,0,0x00c1,"以太网测试","ETHERNET TEST",NULL);
	AddToMenu(&tSetLanMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"服务器IP:","SERVICE IP:",NULL);
	AddToMenu(&tSetLanMenu[3],0,0,0,0x1401,"","",NULL);
	if(iHeight == 272) //MT30
	{
		tSetLanMenu[1].uiAttr = 0x1401;
	}
	do{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"服务器IP:","SERVICE IP:",NULL);
		AddToMenu(&tSetLanMenu[3],0,0,0,0x1401,"","",NULL);
		ScrCls();
		iShowMenu(tSetLanMenu,4,0,NULL);
		sprintf((char *)szDispBuf,"%s",tCommParam.szHostIP);
		if(iHeight == 272) //MT30
		{
			if(McGetIpAddrStr(szDispBuf,12,6) != 0x00)
			{
				return 1;
			}
		}
		else
		{
			if(McGetIpAddrStr(szDispBuf,12,5) != 0x00)
			{
				return 1;
			}
		}
		if (McCheckIpStr(szDispBuf,(uchar)strlen(szDispBuf)) != 0x00)
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"IP地址非法","INVALID IP ADDRS",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;			
		}
		break;
	}while(1);
	sprintf((char *)tCommParam.szHostIP,"%s",szDispBuf);	
//----------------
	while(1)
	{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"服务器端口:","SERVICE PORT:",NULL);	
		sprintf(szDispBuf,"%s",tCommParam.szHostPort);	
		AddToMenu(&tSetLanMenu[3],0,0,0,0x2101,szDispBuf,szDispBuf,NULL);
		ScrCls();
		ucRet = (uchar)iShowMenu(tSetLanMenu,4,0,&iSelectedItem);
		if(ucRet == KEYCANCEL)
		{
			return 1;
		}
		if(iSelectedItem != -1 && iSelectedItem <= 65535)
		{
			break;
		}
		else
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"参数非法","INVALID PARAM",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;	
		}
	}
	sprintf((uchar *)tCommParam.szHostPort,"%d",iSelectedItem);


tCommParam.ucDHCPFlag = ucDHCPFlag;
if(tCommParam.ucDHCPFlag == 0x00)//Static模式才需要设置本地IP、子网掩码、网关和DNS
{
//----------------	
	do{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"本地IP:","LOCAL IP:",NULL);
		AddToMenu(&tSetLanMenu[3],0,0,0,0x1401,"","",NULL);
		ScrCls();
		iShowMenu(tSetLanMenu,4,0,NULL);
		sprintf((uchar *)szDispBuf,"%s",tCommParam.szLoclIP);	
		if(iHeight == 272) //MT30
		{
			if(McGetIpAddrStr(szDispBuf,12,6) != 0x00)
			{
				return 1;
			}
		}
		else
		{
			if(McGetIpAddrStr(szDispBuf,12,5) != 0x00)
			{
				return 1;
			}
		}
		if (McCheckIpStr(szDispBuf,(uchar)strlen(szDispBuf)) != 0x00)
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"IP地址非法","INVALID IP ADDRS",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;			
		}
		break;
	}while(1);
	sprintf((uchar *)tCommParam.szLoclIP,"%s",szDispBuf);

//----------------
	do{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"子网掩码:","MASK IP:",NULL);
		AddToMenu(&tSetLanMenu[3],0,0,0,0x1401,"","",NULL);
		ScrCls();
		iShowMenu(tSetLanMenu,4,0,NULL);
		sprintf((uchar *)szDispBuf,"%s",tCommParam.szMaskIP);
		if(iHeight == 272) //MT30
		{
			if(McGetIpAddrStr(szDispBuf,12,6) != 0x00)
			{
				return 1;
			}
		}
		else
		{
			if(McGetIpAddrStr(szDispBuf,12,5) != 0x00)
			{
				return 1;
			}
		}
		if (McCheckIpStr(szDispBuf,(uchar)strlen(szDispBuf)) != 0x00)
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"IP地址非法","INVALID IP ADDRS",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;			
		}
		break;
	}while(1);
	sprintf((uchar *)tCommParam.szMaskIP,"%s",szDispBuf);
//----------------
	do{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"网关IP:","GATEWAY IP:",NULL);
		AddToMenu(&tSetLanMenu[3],0,0,0,0x1401,"","",NULL);
		ScrCls();
		iShowMenu(tSetLanMenu,4,0,NULL);
		sprintf((uchar *)szDispBuf,"%s",tCommParam.szGetWayIP);
		if(iHeight == 272) //MT30
		{
			if(McGetIpAddrStr(szDispBuf,12,6) != 0x00)
			{
				return 1;
			}
		}
		else
		{
			if(McGetIpAddrStr(szDispBuf,12,5) != 0x00)
			{
				return 1;
			}
		}
		if (McCheckIpStr(szDispBuf,(uchar)strlen(szDispBuf)) != 0x00)
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"IP地址非法","INVALID IP ADDRS",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;			
		}
		break;
	}while(1);
	sprintf((uchar *)tCommParam.szGetWayIP,"%s",szDispBuf);
//----------------	
	do{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"DNS服务器:","DNS IP:",NULL);
		AddToMenu(&tSetLanMenu[3],0,0,0,0x1401,"","",NULL);
		ScrCls();
		iShowMenu(tSetLanMenu,4,0,NULL);
		sprintf((uchar *)szDispBuf,"%s",tCommParam.szDnsIP);	
		if(iHeight == 272) //MT30
		{
			if(McGetIpAddrStr(szDispBuf,12,6) != 0x00)
			{
				return 1;
			}
		}
		else
		{
			if(McGetIpAddrStr(szDispBuf,12,5) != 0x00)
			{
				return 1;
			}
		}
		if (McCheckIpStr(szDispBuf,(uchar)strlen(szDispBuf)) != 0x00)
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"IP地址非法","INVALID IP ADDRS",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;			
		}
		break;
	}while(1);
	sprintf((uchar *)tCommParam.szDnsIP,"%s",szDispBuf);
}
//----------------
/*	do{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"PING IP:","PING IP:",NULL);
		AddToMenu(&tSetLanMenu[3],0,0,0,0x1401,"","",NULL);
		ScrCls();
		iShowMenu(tSetLanMenu,4,0,NULL);
		sprintf((uchar *)szDispBuf,"%s",tCommParam.szPinHostIP);
#ifdef MT30
		if(McGetIpAddrStr(szDispBuf,12,6) != 0x00)
		{
			return 1;
		}
#else
		if(McGetIpAddrStr(szDispBuf,12,5) != 0x00)
		{
			return 1;
		}
#endif
		if (McCheckIpStr(szDispBuf,(uchar)strlen(szDispBuf)) != 0x00)
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"IP地址非法","INVALID IP ADDRS",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;			
		}
		break;
	}while(1);
	sprintf((uchar *)tCommParam.szPinHostIP,"%s",szDispBuf);*/
//----------------
	while(1)
	{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"PING超时时间:","PING TIME:",NULL);	
		sprintf(szDispBuf,"%d",tCommParam.iPingTimeout);	
		AddToMenu(&tSetLanMenu[3],0,0,0,0x2101,szDispBuf,szDispBuf,NULL);
		ScrCls();
		ucRet = (uchar)iShowMenu(tSetLanMenu,4,0,&iSelectedItem);
		if(ucRet == KEYCANCEL)
		{
			return 1;
		}
		if(iSelectedItem != -1 && iSelectedItem <= 60000)
		{
			break;
		}
		else
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"参数非法","INVALID PARAM",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;	
		}
	}
	tCommParam.iPingTimeout = iSelectedItem;
//----------------
	while(1)
	{
		AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"PING包大小:","PING SIZE:",NULL);	
		sprintf(szDispBuf,"%d",tCommParam.iPingSize);	
		AddToMenu(&tSetLanMenu[3],0,0,0,0x2101,szDispBuf,szDispBuf,NULL);
		ScrCls();
		ucRet = (uchar)iShowMenu(tSetLanMenu,4,0,&iSelectedItem);
		if(ucRet == KEYCANCEL)
		{
			return 1;
		}
		if(iSelectedItem != -1 && iSelectedItem <= 1024)
		{
			break;
		}
		else
		{
			AddToMenu(&tSetLanMenu[2],0,0,0,0x1101,"参数非法","INVALID PARAM",NULL);
			AddToMenu(&tSetLanMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tSetLanMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;	
		}
	}
 	tCommParam.iPingSize = iSelectedItem;	
	return 0;
}
int McSetWNetParam()
{
	int iSelectItem;
	uchar ucRet = 0;
	uchar szDispBuf[100],szGetBuf[50];
	Menu tWNETMenu[4];
	memset(tWNETMenu,0,sizeof(tWNETMenu));
	memset(szDispBuf,0,sizeof(szDispBuf));
	AddToMenu(&tWNETMenu[0],0,0,0,0x00c1,"无线测试","WNET TEST",NULL);
	AddToMenu(&tWNETMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"接入点名称(APN):","APN:",NULL);
	AddToMenu(&tWNETMenu[3],0,0,0,0x1401,"","",NULL);
	ScrCls();
	iShowMenu(tWNETMenu,4,0,NULL);
//------------
	sprintf(szDispBuf,"%s",tCommParam.szWNetDailNum);
	ScrAttrSet(0);
	ScrFontSet(CFONT);
	ScrGotoxy(44,5);
	if(GetString(szDispBuf,0xf5,0,99) != 0x00)
	{
		return 1;
	}	
	memset(szGetBuf,0,50);
	memcpy(szGetBuf,szDispBuf + 1,szDispBuf[0]);
	sprintf((char *)tCommParam.szWNetDailNum,"%s",szGetBuf);
//------------
	AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"请输入用户名:","USER NAME:",NULL);
	ScrCls();
	iShowMenu(tWNETMenu,4,0,NULL);
	sprintf(szDispBuf,"%s",tCommParam.szUID);
	ScrAttrSet(0);
	ScrFontSet(CFONT);
	ScrGotoxy(44,5);
	if(GetString(szDispBuf,0xf5,0,8) != 0x00)
	{
		return 1;
	}	
	memset(szGetBuf,0,50);
	memcpy(szGetBuf,szDispBuf + 1,szDispBuf[0]);
	sprintf((char *)tCommParam.szUID,"%s",szGetBuf);
//------------
	AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"请输入密码:","PWD:",NULL);
	ScrCls();
	iShowMenu(tWNETMenu,4,0,NULL);
	sprintf(szDispBuf,"%s",tCommParam.szPWD);
	ScrAttrSet(0);
	ScrFontSet(CFONT);
	ScrGotoxy(44,5);
	if(GetString(szDispBuf,0xf5,0,8) != 0x00)
	{
		return 1;
	}	
	memset(szGetBuf,0,50);
	memcpy(szGetBuf,szDispBuf + 1,szDispBuf[0]);
	sprintf((char *)tCommParam.szPWD,"%s",szGetBuf);
//------------
	do{
		AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"请输入IP地址:","IP ADDR:",NULL);
		AddToMenu(&tWNETMenu[3],0,0,0,0x1401,"","",NULL);
		ScrCls();
		iShowMenu(tWNETMenu,4,0,NULL);
		sprintf((uchar *)szDispBuf,"%s",tCommParam.szDespIP);
		if(McGetIpAddrStr(szDispBuf,12,5) != 0x00)
		{
			return 1;
		}
		if (McCheckIpStr(szDispBuf,(uchar)strlen(szDispBuf)) != 0x00)
		{
			AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"IP地址非法","INVALID IP ADDRS",NULL);
			AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tWNETMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;			
		}
		break;
	}while(1);
	sprintf((char *)tCommParam.szDespIP,"%s",szDispBuf);
//----------------
	while(1)
	{
		AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"请输入端口号:","SERVICE PORT:",NULL);	
		sprintf(szDispBuf,"%s",tCommParam.szDespPort);	
		AddToMenu(&tWNETMenu[3],0,0,0,0x2101,szDispBuf,szDispBuf,NULL);
		ScrCls();
		ucRet = (uchar)iShowMenu(tWNETMenu,4,0,&iSelectItem);
		if(ucRet == KEYCANCEL)
		{
			return 1;
		}
		if(iSelectItem != -1 && iSelectItem <= 65535)
		{
			break;
		}
		else
		{
			AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"参数非法","INVALID PARAM",NULL);
			AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tWNETMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;	
		}
	}
	sprintf((uchar *)tCommParam.szDespPort,"%d",iSelectItem);
//----------------
	while(1)
	{
		AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"请输入数据长度:","DATA LENGTH:",NULL);	
		sprintf(szDispBuf,"%d",tCommParam.iWNetSendCnt);	
		AddToMenu(&tWNETMenu[3],0,0,0,0x2101,szDispBuf,szDispBuf,NULL);
		ScrCls();
		ucRet = (uchar)iShowMenu(tWNETMenu,4,0,&iSelectItem);
		if(ucRet == KEYCANCEL)
		{
			return 1;
		}
		if(iSelectItem != -1 && iSelectItem <= 8192 && iSelectItem > 0)
		{
			break;
		}
		else
		{
			AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"参数非法","INVALID PARAM",NULL);
			AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"请重新输入","PLS INPUT AGAIN!",NULL);
			ScrCls();
			iShowMenu(tWNETMenu,4,0,NULL);
			Beep();
			GetKeyWait(2000);
			continue;	
		}
	}
	tCommParam.iWNetSendCnt = iSelectItem;
	return 0;

}
int McSetModemParam()
{
	uchar szDispBuf[50],szGetBuf[50];
	Menu tModemMenu[6];
	int iSelectedItem;
	uchar ucRet;
	tCommParam.ModemParam.SSETUP = gucSSetup;
	memset(tModemMenu,0,sizeof(tModemMenu));
	AddToMenu(&tModemMenu[0],0,0,0,0x00c1,"MODEM测试","MODEM TEST",NULL);
	AddToMenu(&tModemMenu[1],0,0,0,0x1001,"通讯速率设置:","COMM BAUD RATE:",NULL);
	AddToMenu(&tModemMenu[2],0,0,0,0x1000,"1 - 1200","1 - 1200",NULL);
	AddToMenu(&tModemMenu[3],0,0,0,0x1000,"2 - 2400","2 - 2400",NULL);
	AddToMenu(&tModemMenu[4],0,0,0,0x1000,"3 - 9600","3 - 9600",NULL);
	AddToMenu(&tModemMenu[5],0,0,0,0x1000,"","",NULL);
	ScrCls();
	iShowMenu(tModemMenu,6,0,NULL);
	while(1)
	{
		ucRet = getkey();
		if(ucRet == KEY1)
		{
			break;
		}
		else if(ucRet == KEY2)
		{
			gucSSetup |= 0x20;
			break;
		}
		else if(ucRet == KEY3)
		{
			gucSSetup |= 0x40;
			break;
		}
		else if(ucRet == KEYCANCEL)
		{
			return 1;
		}
	}
	tCommParam.ModemParam.SSETUP = gucSSetup;
//---------
	if (gucSSetup & 0x80)//异步
	{
		AddToMenu(&tModemMenu[0],0,0,0,0x00c1,"MODEM测试","MODEM TEST",NULL);
		AddToMenu(&tModemMenu[1],0,0,0,0x1400,"","",NULL);
		AddToMenu(&tModemMenu[2],0,0,0,0x1101,"异步通讯率:","ASMODE PARAM:",NULL);
		AddToMenu(&tModemMenu[3],0,0,0,0x1401,szDispBuf,szDispBuf,NULL);
		tModemMenu[4].uiAttr = 0x1400;
		tModemMenu[5].uiAttr = 0x1400;
		ScrCls();
		iShowMenu(tModemMenu,4,0,NULL);
		
		sprintf(szDispBuf,"%d",tCommParam.ModemParam.AsMode & 0x0f);
		ScrAttrSet(0);
		ScrFontSet(CFONT);
		ScrGotoxy(60,5);
		if(GetString(szDispBuf,0xf5,1,1) != 0x00)
		{
			return 1;
		}	
		memset(szGetBuf,0,50);
		memcpy(szGetBuf,szDispBuf + 1,szDispBuf[0]);
		tCommParam.ModemParam.AsMode = (uchar)atoi(szGetBuf);
		
		ucRet = gucSSetup & 0x60;
		if(ucRet == 0x00)
		{
			tCommParam.ModemParam.AsMode |= 0x10;
		}
		else if(ucRet == 0x20)
		{
			tCommParam.ModemParam.AsMode |= 0x20;
		}
		else if(ucRet == 0x40)
		{
			tCommParam.ModemParam.AsMode |= 0x50;
		}
		else if(ucRet == 0x60)
		{
			tCommParam.ModemParam.AsMode |= 0x70;
		}
	}
//---------
	AddToMenu(&tModemMenu[0],0,0,0,0x00c1,"MODEM测试","MODEM TEST",NULL);
	AddToMenu(&tModemMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tModemMenu[2],0,0,0,0x1101,"设置超时时间:","SET TIMEOUT:",NULL);
	sprintf((char *)szDispBuf,"%d",tCommParam.ModemParam.TimeOut);
	AddToMenu(&tModemMenu[3],0,0,0,0x2101,szDispBuf,szDispBuf,NULL);
	tModemMenu[4].uiAttr = 0x1400;
	tModemMenu[5].uiAttr = 0x1400;
	ScrCls();
	ucRet = (uchar)iShowMenu(tModemMenu,4,0,&iSelectedItem);
	if(ucRet == KEYCANCEL)
	{
		return 1;
	}
	tCommParam.ModemParam.TimeOut = (uchar)iSelectedItem;
//---------
	AddToMenu(&tModemMenu[2],0,0,0,0x1101,"电话号码:","DIAL NUM:",NULL);
	tModemMenu[3].uiAttr = 0x1401;
	ScrCls();
	iShowMenu(tModemMenu,4,0,NULL);
	sprintf(szDispBuf,"%s",tCommParam.szModemDialNum);

	ScrAttrSet(0);
	ScrFontSet(CFONT);
	ScrGotoxy(10,5);
	if(GetString(szDispBuf,0xf3,0,16) != 0x00)
	{
		return 1;
	}	
	memset(szGetBuf,0,50);
	memcpy(szGetBuf,szDispBuf + 1,szDispBuf[0]);
	sprintf(tCommParam.szModemDialNum,"%s",szGetBuf);
//---------
	AddToMenu(&tModemMenu[2],0,0,0,0x1101,"连接次数:","CONN COUNT:",NULL);
	sprintf((char *)szDispBuf,"%d",tCommParam.iExeModemCnt);
	AddToMenu(&tModemMenu[3],0,0,0,0x2101,szDispBuf,szDispBuf,NULL);
	ScrCls();
	ucRet = (uchar)iShowMenu(tModemMenu,4,0,&iSelectedItem);
	if(ucRet == KEYCANCEL)
	{
		return 1;
	}
	tCommParam.iExeModemCnt = (uchar)iSelectedItem;	
//---------
	AddToMenu(&tModemMenu[2],0,0,0,0x1101,"发送次数:","SEND COUNT:",NULL);
	sprintf((char *)szDispBuf,"%d",tCommParam.iModemSendTimes);
	AddToMenu(&tModemMenu[3],0,0,0,0x2101,szDispBuf,szDispBuf,NULL);
	ScrCls();
	ucRet = (uchar)iShowMenu(tModemMenu,4,0,&iSelectedItem);
	if(ucRet == KEYCANCEL)
	{
		return 1;
	}
	tCommParam.iModemSendTimes = (uchar)iSelectedItem;	
	return 0;
}

int McSetCommParam(uchar ucType)
{
	int iRet = 0;
	McLoadCommParam();
	switch(ucType) 
	{
	case KEY2:
		iRet = McSetModemParam();
		if(!iRet)
		{
			McStoreCommParam();
		}
		return iRet;
		return;
	case KEY3:
		iRet = McSetLanParam();
		if(!iRet)
		{
			McStoreCommParam();
		}
		return iRet;
//	case KEY4:
//		SetWifiParam();
//		break;
	case KEY5:
		iRet = McSetWNetParam();
		if(!iRet)
		{
			McStoreCommParam();
		}
		return iRet;
	default:
		return;
	}
}
void WnetError(int iRet)
{
	uchar ucRet;
	Menu tWNETMenu[4];
	uchar szDispBuf[50];
	memset(szDispBuf,0,sizeof(szDispBuf));
	memset(tWNETMenu,0,sizeof(tWNETMenu));
	AddToMenu(&tWNETMenu[0],0,0,0,0x00c1,"无线测试","WNET TEST",NULL);
	AddToMenu(&tWNETMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tWNETMenu[2],0,0,0,0x1101,"无线测试异常!","WNET TEST ERROR!",NULL);
	AddToMenu(&tWNETMenu[3],0,0,0,0x1101,"返回值:","RETURN:",NULL);
	sprintf((char *)szDispBuf,"%d",iRet);
	strcat(tWNETMenu[3].acChnName,szDispBuf);
	strcat(tWNETMenu[3].acEngName,szDispBuf);
	ScrCls();
	iShowMenu(tWNETMenu,4,0,NULL);
	while(1)
	{
		ucRet = GetKeyWait(6000);
		if(ucRet == KEYENTER || ucRet == 0)
		{
			break;
		}
	}
	return;
}
void EthError(int iRet)
{
	Menu tETHMenu[4];
	uchar ucRet;
	uchar szDispBuf[50];
	int iWidth, iHeight;
	ScrGetLcdSize(&iWidth, &iHeight);
	memset(tETHMenu,0,sizeof(tETHMenu));
	memset(szDispBuf,0,sizeof(szDispBuf));
	AddToMenu(&tETHMenu[0],0,0,0,0x00c1,"以太网测试","ETHERNET TEST",NULL);
	AddToMenu(&tETHMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tETHMenu[2],0,0,0,0x1101,"设置网络参数失败","SET NETPARA ERR",NULL);
	AddToMenu(&tETHMenu[3],0,0,0,0x1101,"返回值:","RETURN:",NULL);
	if(iHeight == 272) //MT30
	{
		tETHMenu[1].uiAttr = 0x1401;
	}
	sprintf((char *)szDispBuf,"%d",iRet);
	strcat(tETHMenu[3].acChnName,szDispBuf);
	strcat(tETHMenu[3].acEngName,szDispBuf);
	ScrCls();
	iShowMenu(tETHMenu,4,0,NULL);
	while(1)
	{
		ucRet = GetKeyWait(6000);
		if(ucRet == KEYENTER|| ucRet == 0)
		{
			break;
		}
	}
	if(GetHWBranch() == D210HW_V2x )
	{
		RestoreHandsetBaseLink();
	}
	return;
}
void ModemError(uchar ucRet)
{
	uchar ucRet1;
	Menu tModemMenu[4];
	uchar szDispBuf[50];
	memset(szDispBuf,0,sizeof(szDispBuf));
	memset(tModemMenu,0,sizeof(tModemMenu));
	AddToMenu(&tModemMenu[0],0,0,0,0x00c1,"MODEM测试","MODEM TEST",NULL);
	AddToMenu(&tModemMenu[1],0,0,0,0x1400,"","",NULL);
	AddToMenu(&tModemMenu[2],0,0,0,0x1101,"","",NULL);
	AddToMenu(&tModemMenu[3],0,0,0,0x1101,"返回码: ","RETURN: ",NULL);
	if(ucRet < 0x10)
	{
		sprintf((char *)szDispBuf,"0%x",ucRet);
	}
	else
	{
		sprintf((char *)szDispBuf,"%2x",ucRet);
	}
	strcat(tModemMenu[3].acChnName,szDispBuf);
	strcat(tModemMenu[3].acEngName,szDispBuf);
	switch(ucRet)
	{
		case 0x33:
			strcpy(tModemMenu[2].acChnName,"电话线未接!");
			strcpy(tModemMenu[2].acEngName,"MISS TEL LINE!");
			break;
		case 0x05:
			strcpy(tModemMenu[2].acChnName,"拨号无应答!");
			strcpy(tModemMenu[2].acEngName,"DIAL NO ANSWER!");
			break;
		case 0xfd:
			strcpy(tModemMenu[2].acChnName,"取消拨号!");
			strcpy(tModemMenu[2].acEngName,"CANCEL DIALUP!");
			break;
		case 0x0d:
			strcpy(tModemMenu[2].acChnName,"被叫线路忙!");
			strcpy(tModemMenu[2].acEngName,"LINE BUSY!");
			break;
		default:
			strcpy(tModemMenu[2].acChnName,"其它错误!");
			strcpy(tModemMenu[2].acEngName,"OTHER ERROR!");
			break;
	}
	ScrCls();
	iShowMenu(tModemMenu,4,0,NULL);
	while(1)
	{
		ucRet1 = GetKeyWait(6000);
		if(ucRet1 == KEYENTER|| ucRet1 == 0)
		{
			break;
		}
	}
	return;
}

