/******************************************************
 2005-01-26:
 增加PEDCLEARKEY接口，用于清除全部主密钥或全部工作密钥或全部DES密钥
 2008-11-07
 杨瑞彬
 添加对串口通道口的判断,当串口通道为2时,不允许操作串口
 2008-11-08
 修改Close函数调用,上层函数调用close时改为调用closefile
 添加BatteryCheck API接口
 ******************************************************/
#include "base.h"
#include "dll.h"
#include "posapi.h"
#include "..\wnet\wlapi.h"
#include "..\puk\puk.h"
#include "..\font\font.h"
#include "..\comm\comm.h"
#include "..\lcd\lcdapi.h"
#include "..\iccard\protocol\icc_apis.h"
#include "..\wifi\apis\wlanapp.h"

PARAM_STRUCT            *glParam;

extern volatile uchar   k_AutoPowerSaveMode;

extern ulong           k_CheckUpTimeMs;
//extern ulong		   k_ShowDevelopPos;

extern void MonitorSelfTest(void);
void CheckParamInValid(void * uiAddr,int iNullIsValid);

//#define DEBUG_DLL

void LoadDll(void)
{
	glParam = (PARAM_STRUCT *)DLL_PARAM_ADDR;
	glParam->Addr1 = (void *)0x28000000;
	glParam->Addr2 = (void *)0x28000000;
	glParam->u_str1 = (void *)0x28000000;
	glParam->u_str2 = (void *)0x28000000;
	glParam->u_str3 = (void *)0x28000000;
	glParam->u_str4 = (void *)0x28000000;
	glParam->up_short1 = (void *)0x28000000;
}

void SysFun(void)
{
	ST_FONT *SingleCodeFont;
	ST_FONT *MultiCodeFont;

    switch(glParam->FuncNo)
    {
    case DLL_BEEP:
        //BeepUser();
        Beep();
        break;
    case DLL_BEEF:
        Beepf(glParam->u_char1, glParam->u_short1);
        break;
    case DLL_SETTIME:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1 = SetTime(glParam->u_str1);
        break;
    case DLL_GETTIME:
		CheckParamInValid(glParam->u_str1, 0);
        GetTime(glParam->u_str1);
        break;
    case DLL_READSN:
		CheckParamInValid(glParam->u_str1, 0);
        ReadSN(glParam->u_str1);
        break;
	case DLL_READCSN:
	 	CheckParamInValid(glParam->u_str1, 0);
        glParam->int1 = ReadCSN(glParam->u_char1,glParam->u_str1);
        break;
    case DLL_READVERINFO:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1 = ReadVerInfo(glParam->u_str1);
        break;
    case DLL_TIMERSET:
        TimerSet(glParam->u_char1, glParam->u_short1);
        break;
    case DLL_EXREADSN:
		CheckParamInValid(glParam->u_str1, 0);
        EXReadSN(glParam->u_str1);
        break;
    case DLL_GETLASTERROR:
        glParam->int1 = GetLastError();
        break;
    case DLL_SETTIMEREVENT:
   //     glParam->int1 = SetTimerEvent(glParam->u_short1, (void (*)(void))glParam->Addr1);
        break;
    case DLL_KILLTIMEREVENT:
  //      KillTimerEvent(glParam->int1);
        break;
    case DLL_TIMERCHECK:
        glParam->u_short1 = TimerCheck(glParam->u_char1);
        break;
    case DLL_CHECKFIRST:
        glParam->u_char1 = CheckFirst();
		//--------------启动一个定时器事件
	//	ScrRestore(0);
	//	s_SetTimerEvent(3000,ShowDevlopPos);
	//	ScrRestore(1);
        break;
    case DLL_POWEROFF:
        PowerOff();
        break;
    case DLL_OFFBASE:
        break;
    case DLL_ONBASE:
        glParam->u_char1 = OnBase();
        break;
    case DLL_BATTERYCHECK:
        glParam->u_char1 = BatteryCheck();
        break;
    case DLL_AUTOSHUTDOWN:
        break;
    case DLL_AUTOSHUTDOWNSEC:
        break;
    case DLL_POWERSAVE:
        //glParam->u_int1 = PowerSave((uint)(glParam->int1), (uint)(glParam->int2));
        break;
    case DLL_AUTOPOWERSAVE:
        //AutoPowerSave(glParam->u_char1);
        break;
    case DLL_GETTERMINFO:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->int1 = GetTermInfo(glParam->u_str1);
        break;
    case DLL_GETTIMERCOUNT_P90:
    case DLL_GETTIMERCOUNT_P80:
        glParam->u_int1 = GetTimerCount();
        break;
    case DLL_GETVOLT:
        glParam->int1 = GetVolt();
        break;
    case DLL_FILE_TO_APP:
        //glParam->int1 = FileToApp(glParam->u_str1);
        break;
    case DLL_FILE_TO_PARAM:
        //glParam->int1 = FileToParam(glParam->u_str1, glParam->u_str2, glParam->int1);
        break;
	case DLL_READ_FONTLIB:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = ReadFontLib(glParam->int1,glParam->Addr1,glParam->int2);
		break;
//New API for Application
	case DLL_ENUM_FONT:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = EnumFont(glParam->Addr1, glParam->int1);
		break;  
	case DLL_SEL_SCR_FONT:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		glParam->int1 =  SelScrFont(glParam->Addr1, glParam->Addr2);
		break;
	case DLL_SEL_PRN_FONT:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		glParam->int1 = SelPrnFont(glParam->Addr1, glParam->Addr2);
		break;
	case DLL_REBOOT:
		Soft_Reset();
		break;
    case DLL_SYS_WAKEUP_CHANNEL:
        CheckParamInValid(glParam->u_str1, 0);
        glParam->int1 = SetWakeupChannel(glParam->u_int1,glParam->u_str1);
        break;
	case DLL_SYS_SLEEP:
		CheckParamInValid(glParam->u_str1, 1);
		glParam->int1 = SysSleep(glParam->u_str1);
		break;  
	case DLL_SYS_IDLE:
		SysIdle();
		break;
	case DLL_GET_EXTINFO:
		CheckParamInValid(glParam->u_str1, 0);
		glParam->int1 = GetTermInfoExt(glParam->u_str1, glParam->int1);
		break;
	case DLL_SYS_PCIGETRANDOM:
		CheckParamInValid(glParam->u_str1, 0);
		GetRandom(glParam->u_str1);
		break;
	case DLL_SYS_CONFIG:
		CheckParamInValid(glParam->u_str1, 0);
		glParam->int1=SysConfig(glParam->u_str1, glParam->int1);
		break;
	case DLL_GET_HWCFG:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 0);
		glParam->int1=GetHardwareConfig(glParam->u_str1,glParam->u_str2,glParam->int1);
		break;		

    case DLL_GETCURSCRFONT:
        CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		s_GetCurScrFont(glParam->Addr1, glParam->Addr2);
        break;

    case DLL_READ_TUSN:
    	CheckParamInValid(glParam->u_str1, 0);
    	glParam->int1 = ReadTUSN(glParam->u_str1,glParam->u_int1);
    	break;
    }
}


void IcFun(void)
{
    uint event;
    switch(glParam->FuncNo)
    {
    case DLL_ICCINIT:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1 = IccInit(glParam->u_char1,glParam->u_str1);
        break;
    case DLL_ICCAUTORESP:
        IccAutoResp(glParam->u_char1, glParam->u_char2);
        break;
    case DLL_ICCCLOSE:
        IccClose(glParam->u_char1);
        break;
    case DLL_ICCISOCOMMAND:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
        glParam->u_char1 = IccIsoCommand(glParam->u_char1,(APDU_SEND *)glParam->Addr1,(APDU_RESP *)glParam->Addr2);
        break;
    case DLL_ICCDETECT:
        glParam->u_char1 = IccDetect(glParam->u_char1);
//        if(glParam->u_char1 && k_AutoPowerSaveMode)
//        {
//            event = PowerSave(EVENT_OVERTIME|EVENT_KEYPRESS|EVENT_MAGSWIPED|EVENT_ICCIN|EVENT_UARTRECV|EVENT_WNETRECV, 20);
//            //  if(event & EVENT_ICCIN)       glParam->u_char1 = 0;
//        }
        break;
    case DLL_MC_CLK_ENABLE:
        Mc_Clk_Enable(glParam->u_char1, glParam->u_char2);
        break;
    case DLL_MC_IO_DIR:
        Mc_Io_Dir(glParam->u_char1, glParam->u_char2);
        break;
    case DLL_MC_IO_READ:
        glParam->u_char1 = Mc_Io_Read(glParam->u_char1);
        break;
    case DLL_MC_IO_WRITE:
        Mc_Io_Write(glParam->u_char1, glParam->u_char2);
        break;
    case DLL_MC_VCC:
        Mc_Vcc(glParam->u_char1,glParam->u_char2);
        break;
    case DLL_MC_RESET:
        Mc_Reset(glParam->u_char1, glParam->u_char2);
        break;
    case DLL_MC_CLK:
        Mc_Clk(glParam->u_char1, glParam->u_char2);
        break;
    case DLL_MC_C4_WRITE:
        Mc_C4_Write(glParam->u_char1, glParam->u_char2);
        break;
    case DLL_MC_C8_WRITE:
        Mc_C8_Write(glParam->u_char1, glParam->u_char2);
        break;
    case DLL_MC_C4_READ:
        glParam->u_char1 = Mc_C4_Read(glParam->u_char1);
        break;
    case DLL_MC_C8_READ:
        glParam->u_char1 = Mc_C8_Read(glParam->u_char1);
        break;
    case DLL_READ_CARDSLOTINFO:
        glParam->u_char1 = Read_CardSlotInfo(glParam->u_char1);
        break;
    case DLL_WRITE_CARDSLOTINFO:
        Write_CardSlotInfo(glParam->u_char1, glParam->u_char2);
        break;
    }

}


void WNetFun(void)
{
//    switch(glParam->FuncNo)
//    {
//    case    DLL_WNETINIT        :
//        glParam->u_char1 = WNetInit();
//        break;
//    case    DLL_WNETCHECKSIGNAL :
//        glParam->u_char1 = WNetCheckSignal(glParam->u_str1);
//        break;
//    case    DLL_WNETCHECKSIM    :
//        glParam->u_char1 = WNetCheckSim();
//        break;
//    case    DLL_WNETSIMPIN      :
//        glParam->u_char1 = WNetSimPin(glParam->u_str1);
//        break;
//    case    DLL_WNETUIDPWD      :
//        glParam->u_char1 = WNetUidPwd(glParam->u_str1, glParam->u_str2);
//        break;
//    case    DLL_WNETDIAL        :
//        glParam->u_char1 = WNetDial(glParam->u_str1, glParam->u_str2, glParam->u_str3);
//        break;
//    case    DLL_WNETCHECK       :
//        glParam->u_char1 = WNetCheck();
//        break;
//    case    DLL_WNETCLOSE       :
//        glParam->u_char1 = WNetClose();
//        break;
//    case    DLL_WNETLINKCHECK   :
//        glParam->u_char1 = WNetLinkCheck();
//        break;
//    case    DLL_WNETTCPCONNECT  :
//        glParam->u_char1 = WNetTcpConnect(glParam->u_str1, glParam->u_str2);
//        break;
//    case    DLL_WNETTCPCLOSE    :
//        glParam->u_char1 = WNetTcpClose();
//        break;
//    case    DLL_WNETTCPCHECK    :
//        glParam->u_char1 = WNetTcpCheck();
//        break;
//    case    DLL_WNETTCPTXD      :
//        glParam->u_char1 = WNetTcpTxd(glParam->u_str1, glParam->u_short1);
//        break;
//    case    DLL_WNETTCPRXD      :
//        glParam->u_char1 = WNetTcpRxd(glParam->u_str1, glParam->up_short1, glParam->u_short1);
//        break;
//    case    DLL_WNETRESET       :
//        glParam->u_char1 = WNetReset();
//        break;
//    case    DLL_WNETSENDCMD     :
//        glParam->u_char1 = WNetSendCmd(glParam->u_str1, glParam->u_str2, glParam->u_short1, glParam->u_short2);
//        break;
//    case    DLL_WPHONECALL      :
//        glParam->u_char1 = WPhoneCall(glParam->u_str1);
//        break;
//    case    DLL_WPHONEHANGUP    :
//        glParam->u_char1 = WPhoneHangUp();
//        break;
//    case    DLL_WPHONESTATUS    :
//        glParam->u_char1 = WPhoneStatus();
//        break;
//    case    DLL_WPHONEANSWER    :
//        glParam->u_char1 = WPhoneAnswer();
//        break;
//    case    DLL_WPHONEMICGAIN   :
//        glParam->u_char1 = WPhoneMicGain(glParam->u_char1);
//        break;
//    case    DLL_WPHONESPKGAIN   :
//        glParam->u_char1 = WPhoneSpkGain(glParam->u_char1);
//        break;
//    case    DLL_WPHONESENDDTMF  :
//        glParam->u_char1 = WPhoneSendDTMF(glParam->u_char1, glParam->u_short1);
//        break;
//    }
}

volatile int giIsPiccDetected=0;

void PiccFun(void)
{
	switch(glParam->FuncNo)
	{
		case DLL_PICC_OPEN:
			glParam->u_char1=PiccOpen();
			break;
		case DLL_PICC_SETUP:
			CheckParamInValid(glParam->Addr1, 0);
			glParam->u_char1=PiccSetup(glParam->u_char1,glParam->Addr1);//直接返回错误
			break;
		case DLL_PICC_CLOSE:
			PiccClose();
			vled_close();
			giIsPiccDetected = 0;
			break;
		/* case DLL_PICC_INFO:
			CheckParamInValid(glParam->Addr1, 0);
			glParam->u_char1=PiccInfoSetup(glParam->u_char1,glParam->Addr1);
			break;*/
		case DLL_PICC_DETECT:
			CheckParamInValid(glParam->u_str1, 1);
			CheckParamInValid(glParam->u_str2, 1);
			CheckParamInValid(glParam->u_str3, 1);
			CheckParamInValid(glParam->u_str4, 1);
			glParam->u_char1=PiccDetect(glParam->u_char1,glParam->u_str1,glParam->u_str2,glParam->u_str3,glParam->u_str4);
			if(0==glParam->u_char1 || 3==glParam->u_char1) giIsPiccDetected = 1;
			else giIsPiccDetected = 0;
			break;
		case DLL_PICC_ISO_CMD:
			CheckParamInValid(glParam->Addr1, 0);
			CheckParamInValid(glParam->Addr2, 0);
			glParam->u_char1=PiccIsoCommand(glParam->u_char1,glParam->Addr1,glParam->Addr2);
			break;
		case DLL_PICC_REMOVE:
			glParam->u_char1=PiccRemove(glParam->u_char1,glParam->u_char2);
			break;
		case DLL_M1AUTHORITY:
			CheckParamInValid(glParam->u_str1, 0);
			CheckParamInValid(glParam->u_str2, 0);
			glParam->u_char1=M1Authority(glParam->u_char1,glParam->u_char2,glParam->u_str1,glParam->u_str2);
			break;
		case DLL_M1READBLOCK:
			CheckParamInValid(glParam->u_str1, 0);
			glParam->u_char1=M1ReadBlock(glParam->u_char1,glParam->u_str1);
			break;
		case DLL_M1WRITEBLOCK:
			CheckParamInValid(glParam->u_str1, 0);
			glParam->u_char1=M1WriteBlock(glParam->u_char1,glParam->u_str1);
			break;
		case DLL_M1OPERATE:
			CheckParamInValid(glParam->u_str1, 0);
			glParam->u_char1=M1Operate(glParam->u_char1,glParam->u_char2,glParam->u_str1,glParam->u_char3);
			break;
		case DLL_PICC_LED_CON:
			PiccLight(glParam->u_char1,glParam->u_char2);
			break;
		case DLL_PICCCMDEXCHANGE:
			CheckParamInValid(glParam->u_str1, 0);
			CheckParamInValid(glParam->u_str2, 0);
			CheckParamInValid(glParam->Addr1, 0);
			glParam->u_char1=PiccCmdExchange(glParam->u_int1, glParam->u_str1, (uint*)glParam->Addr1, glParam->u_str2);
			break;
		// liuxl 20091118
		case DLL_PICCINITFELICA:
			glParam->u_char1=PiccInitFelica(glParam->u_char1, glParam->u_char2);
			break;
		case DLL_PICCMANAGEREG:
			CheckParamInValid(glParam->u_str1, 0);
			glParam->u_char1=PiccManageReg(glParam->u_char1, glParam->u_char2, glParam->u_str1);
			break;
	}
}

void MagFun(void)
{
    switch(glParam->FuncNo)
    {
    case DLL_MAGOPEN:
		MagOpen();
        break;
    case DLL_MAGCLOSE:
		MagClose();
        break;
    case DLL_MAGRESET:
		MagReset();
        break;
    case DLL_MAGSWIPED:
		glParam->u_char1=MagSwiped();
        break;
    case DLL_MAGREAD:
		CheckParamInValid(glParam->u_str1, 1);
		CheckParamInValid(glParam->u_str2, 1);
		CheckParamInValid(glParam->u_str3, 1);
		glParam->u_char1=MagRead(glParam->u_str1, glParam->u_str2, glParam->u_str3);
        break;
    }
}

void LcdFun(void)
{
    switch(glParam->FuncNo)
    {
    case DLL_SCRCLS:
        ScrCls();
        break;
    case DLL_SCRCLRLINE:
        ScrClrLine(glParam->u_char1,glParam->u_char2);
        break;
    case DLL_SCRGRAY:
        ScrGray(glParam->u_char1);
        break;
    case DLL_SCRBACKLIGHT:
        ScrBackLight(glParam->u_char1);
        break;
    case DLL_SCRGOTOXY:
        ScrGotoxy(glParam->u_char1,glParam->u_char2);
        break;
    case DLL_FPUTC:
        break;
    case DLL_SCRPLOT:
        ScrPlot(glParam->u_char1,glParam->u_char2,glParam->u_char3);
        break;
    case DLL_SCRDRLOGO:
		CheckParamInValid(glParam->u_str1, 0);
        ScrDrLogo(glParam->u_str1);
        break;
    case DLL_SCRFONTSET:
        glParam->u_char1=ScrFontSet(glParam->u_char1);
        break;
    case DLL_SCRATTRSET:
        glParam->u_char1=ScrAttrSet(glParam->u_char1);
        break;
    case DLL_SCRSETICON:
        ScrSetIcon(glParam->u_char1,glParam->u_char2);
        break;
    case DLL_SCRPRINT:
		CheckParamInValid(glParam->u_str1, 0);
        s_ScrPrint(glParam->u_char1,glParam->u_char2,glParam->u_char3,(char *)glParam->u_str1);
        break;
    case DLL_LCDPRINTF:
		CheckParamInValid(glParam->u_str1, 0);
        s_Lcdprintf((char *)glParam->u_str1);
        break;
    case DLL_SCRDRLOGOXY:
		CheckParamInValid(glParam->u_str1, 0);
        ScrDrLogoxy(glParam->int1,glParam->int2,glParam->u_str1);
        break;
    case DLL_SCRRESTORE:
        glParam->u_char1=ScrRestore(glParam->u_char1);
        break;
    case DLL_SCRDRAWBOX:
        ScrDrawBox(glParam->u_char1,glParam->u_char2,glParam->u_char3,glParam->u_char4);
        break;
    case DLL_SETLIGHTTIME:
        SetLightTime(glParam->u_short1);
        break;
	case DLL_SCRGOTOXY_EX:	
		ScrGotoxyEx(glParam->int1, glParam->int2);
		break;
	case DLL_SCRGETXY_EX:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		ScrGetxyEx(glParam->Addr1, glParam->Addr2);  
		break;
	case DLL_SCRDRAWRECT:
		ScrDrawRect(glParam->int1, glParam->int2,glParam->long1,glParam->short1);
		break;
	case DLL_SCRCLRRECT:
		ScrClrRect(glParam->int1, glParam->int2,glParam->long1,glParam->short1);
		break;
	case DLL_SCRSPACESET:
		ScrSpaceSet(glParam->int1, glParam->int2);
		break;
	case DLL_SCRGETLCDSIZE:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		ScrGetLcdSize(glParam->Addr1, glParam->Addr2);
		break; 
	case DLL_SCRTEXTOUT:
		CheckParamInValid(glParam->Addr1, 0);
		ScrTextOut(glParam->int1, glParam->int2, glParam->Addr1);
		break;
	case DLL_SCRSETOUTPUT:
		ScrSetOutput(glParam->int1);
		break;
	case DLL_SCRSETECHO:
		ScrSetEcho(glParam->int1);
		break;
	/* color screen */
	case DLL_CLCDSETFGCOLOR:
		glParam->int1 = CLcdSetFgColor(glParam->u_int1);
		break;
	case DLL_CLCDBGDRAWBOX:
		glParam->int1 = CLcdBgDrawBox(glParam->int1, glParam->int2,glParam->long1,glParam->u_long1,glParam->u_int1);
		break;
	case DLL_CLCDSETBGCOLOR:
		glParam->int1 = CLcdSetBgColor(glParam->u_int1);
		break;
	case DLL_CLCDDRAWPIXEL:
		glParam->int1 = CLcdDrawPixel(glParam->int1,glParam->int2,glParam->u_int1);
		break;
	case DLL_CLCDGETPIXEL:
		glParam->int1 = CLcdGetPixel(glParam->int1, glParam->int2, glParam->Addr1);
		break;
	case DLL_CLCDCLRLINE:
		glParam->int1 = CLcdClrLine(glParam->int1, glParam->int2);
		break;
	case DLL_CLCDTEXTOUT:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->int1 = s_CLcdTextOut(glParam->int1, glParam->int2, (char *)glParam->u_str1);
		break;
	case DLL_CLCDPRINT:
		CheckParamInValid(glParam->u_str1, 0);
		glParam->int1 =	s_CLcdPrint(glParam->int1, glParam->int2,  glParam->u_int1, glParam->u_str1);
		break;
	case DLL_CLCDDRAWRECT:
		glParam->int1 = CLcdDrawRect(glParam->int1, glParam->int2,glParam->long1,glParam->u_long1,glParam->u_int1);
		break;
	case DLL_CLCDCLRRECT:
		glParam->int1 = CLcdClrRect(glParam->int1, glParam->int2,glParam->long1,glParam->u_long1, glParam->u_int1);
		break;
	case DLL_CLCDGETINFO:
		glParam->int1 = CLcdGetInfo((ST_LCD_INFO *)glParam->Addr1);
		break;
	}
}

void TouchScreenFun(void)
{
	switch(glParam->FuncNo)
	{
		case DLL_TOUCHSCREEN_OPEN:
			glParam->int1 = TouchScreenOpen(glParam->u_int1);
			break;
			
		case DLL_TOUCHSCREEN_READ:
			glParam->int1 = TouchScreenRead(glParam->Addr1, glParam->u_int1);
			break;

		case DLL_TOUCHSCREEN_CLOSE:
			TouchScreenClose();
			break;

		case DLL_TOUCHSCREEN_FLUSH:
			TouchScreenFlush();
			break;

		case DLL_TOUCHSCREEN_ATTRSET:
			TouchScreenAttrSet(glParam->Addr1);
			break;
			
		case DLL_TOUCHSCREEN_CALIBRATION:
			glParam->int1 = TouchScreenCalibration();
			break;
			
		default:
			break;
	}
}

void KeyboardFun(void)
{
    switch(glParam->FuncNo)
    {
    case DLL_KBHIT:
        glParam->u_char1 = kbhit();
        break;
    case DLL__GETKEY:
    case DLL_GETKEY:
        glParam->u_char1 = UserGetKey(1);        //  getkey();
        break;
    case DLL_KBBEEF:
     //   kbbeef(glParam->u_char1,glParam->u_short1);
        break;
    case DLL_KBSOUND:
   //     kbsound(glParam->u_char1,glParam->u_short1);
        break;
    case DLL_KBFLUSH:
        kbflush();
        break;
    case DLL_GETSTRING:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1 = GetString(glParam->u_str1,glParam->u_char1,glParam->u_char2,glParam->u_char3);
        break;
    case DLL_KBMUTE:
        kbmute(glParam->u_char1);
        break;
    case DLL_GETSTRINGFORMAT:
        //glParam->u_char1=GetStringFormat(glParam->u_char1,glParam->u_char2,glParam->u_char3,glParam->u_long1);
        break;
    case DLL_GETHZSTRING:
        break;
    case DLL_GETSTRINGRESULT:
        //glParam->u_char1=GetStringResult(glParam->u_str1);
        break;
    case DLL_KBLIGHT:
        kblight(glParam->u_char1);
        break;
	case DLL_KBLOCK:
		KbLock(glParam->u_char1);
		break;
	case DLL_KBCHECK:
		glParam->int1 = KbCheck(glParam->int1);
		break;
//    case DLL_SETSLIPFW:
//        SetSlipFW(glParam->u_char1);
//        break;
    default:
        break;
    }
}


void CommFun(void)
{
//	ScrPrint(2,2,0,"%02x mon dll = %d   ",glParam->FuncNo,glParam->u_int1);
//	ScrPrint(7,7,0,"%d mon dll = %d   ",glParam->FuncNo,glParam->u_int1);

    switch(glParam->FuncNo)
    {
  case DLL_MODEMDIAL:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1=ModemDial((COMM_PARA *)glParam->Addr1,glParam->u_str1,glParam->u_char1);
        break;
    case DLL_MODEMCHECK:
        glParam->u_char1=ModemCheck();
        break;
    case DLL_MODEMTXD:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1=ModemTxd(glParam->u_str1,glParam->u_short1);
        break;
//	case DLL_ISAINTHERE:
//		glParam->u_char1 = iSaintHere(glParam->u_str1,glParam->u_char1);
// 		break;
    case DLL_MODEMRXD:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1=ModemRxd(glParam->u_str1,&glParam->u_short1);
		break;
    case DLL_MODEMRESET:
        glParam->u_char1=ModemReset();
        break;
    case DLL_ASMODEMRXD:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1=ModemAsyncGet(glParam->u_str1);
        break;
    case DLL_ONHOOK:
        glParam->u_char1=OnHook();
        break;
    case DLL_SMODEMINFO:
		CheckParamInValid(glParam->u_str1, 1);
		CheckParamInValid(glParam->u_str2, 1);
		CheckParamInValid(glParam->u_str3, 1);
		CheckParamInValid(glParam->u_str4, 1);
        s_ModemInfo(glParam->u_str1,glParam->u_str2,glParam->u_str3,glParam->u_str4);
        break;
	case DLL_MODEMEXCMD:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 1);
		glParam->u_short1 = ModemExCommand(glParam->u_str1,glParam->u_str2,&glParam->u_short2,glParam->u_short1);
        break;
    case DLL_PORTOPEN:
		if(glParam->u_char1>=P_USB || glParam->u_char1 == P_BT)
		{
			CheckParamInValid(glParam->u_str1, 1);
		}
		else
		{
			CheckParamInValid(glParam->u_str1, 0);
		}
//		glParam->u_char1=PortOpen(glParam->u_char1,(uchar *)glParam->u_str1);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->u_char1 = BtPortOpen();  //deleted by shics
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->u_char1 = 2;//应用访问WIFI串口无效
		}
		else
		{
			 glParam->u_char1=PortOpen(glParam->u_char1,(unsigned char *)glParam->u_str1);
		}
        break;
    case DLL_RS485ATTRSET:
        //glParam->u_char1=RS485AttrSet(glParam->u_char1);
        break;
    case DLL_PORTCLOSE:
//		glParam->u_char1=PortClose(glParam->u_char1);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->u_char1 = BtPortClose(); //deleted by shics
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->u_char1 = 2;//应用访问WIFI串口无效
		}
		else
		{
    	    glParam->u_char1=PortClose(glParam->u_char1);
		}
        break;
    case DLL_PORTSEND:
//		glParam->u_char1=PortSend(glParam->u_char1,glParam->u_char2);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->u_char1 = BtPortSend(glParam->u_char2); ////deleted by shics
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->u_char1 = 2;//应用访问WIFI串口无效
		}
		else
		{
			glParam->u_char1=PortSend(glParam->u_char1,glParam->u_char2);
		}
        break;
    case DLL_PORTRECV:
		CheckParamInValid(glParam->u_str1, 0);
//		glParam->u_char1=PortRecv(glParam->u_char1,glParam->u_str1,glParam->u_int1);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->u_char1 = BtPortRecv(glParam->u_str1,glParam->u_int1);//
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->u_char1 = 2;//应用访问WIFI串口无效
		}
		else
		{
			glParam->u_char1=PortRecv(glParam->u_char1,glParam->u_str1,glParam->u_int1);
		}
		break;
	case DLL_PORTRECVS:
		CheckParamInValid(glParam->u_str1, 0);
//		glParam->int1=PortRecvs(glParam->u_char1,glParam->u_str1,glParam->u_short1, glParam->u_short2);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->int1 = BtPortRecvs(glParam->u_str1,glParam->u_short1, glParam->u_short2);////deleted by shics
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->int1 = -2;//应用访问WIFI串口无效
		}
		else
		{
			glParam->int1=PortRecvs(glParam->u_char1,glParam->u_str1,glParam->u_short1, glParam->u_short2);
		}
		break;
    case DLL_PORTRESET:
//        glParam->u_char1=PortReset(glParam->u_char1);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->u_char1 = BtPortReset();//deleted by shics
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->u_char1 = 2;//应用访问WIFI串口无效
		}
		else
		{
        	glParam->u_char1=PortReset(glParam->u_char1);
		}
        break;
    case DLL_SPORTOPEN:
        //glParam->u_char1=s_PortOpen(glParam->u_char1,glParam->u_long1,glParam->u_char2,glParam->u_char3,glParam->u_char4,glParam->int1);
        break;
    case DLL_SPORTSEND:
        //glParam->u_char1=s_PortSend(glParam->u_char1,glParam->u_char2);
        break;
    case DLL_SPORTRECV:
    //  glParam->u_char1=s_PortRecv(glParam->u_char1,glParam->u_str1,glParam->u_int1);
        break;
    case DLL_SPORTCLOSE:
    //  glParam->u_char1=s_PortClose(glParam->u_char1);
        break;
    case DLL_ENABLESWITCH:
        //glParam->u_char1=EnableSwitch();
        break;
    case DLL_SETOFFBASEPROC:
        //SetOffBaseProc((uchar (*)(void))glParam->Addr1);
        break;

    case DLL_PORTTXPOOLCHECK:
//		glParam->u_char1=PortTxPoolCheck(glParam->u_char1);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->u_char1 = BtPortTxPoolCheck();//deleted by shics
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->u_char1 = 2;//应用访问WIFI串口无效
		}
		else
		{
			glParam->u_char1=PortTxPoolCheck(glParam->u_char1);
		}
        break;
    case DLL_PORTSENDS:
		CheckParamInValid(glParam->u_str1, 0);
//		glParam->u_char1=PortSends(glParam->u_char1,glParam->u_str1,glParam->u_short1);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->u_char1 = BtPortSends(glParam->u_str1,glParam->u_short1);//deleted by shics
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->u_char1 = 2;//应用访问WIFI串口无效
		}
		else
		{
			glParam->u_char1=PortSends(glParam->u_char1,glParam->u_str1,glParam->u_short1);
		}
        break;
	case DLL_PORTPEEP:
		CheckParamInValid(glParam->u_str1, 0);
//		glParam->int1 = PortPeep(glParam->u_char1,glParam->u_str1,glParam->u_short1);
		if (glParam->u_char1 == P_BT)
		{
			//glParam->int1 = BtPortPeep(glParam->u_str1,glParam->u_short1);//deleted by shics
		}
		else if (glParam->u_char1 == P_WIFI)
		{
			glParam->int1 = 2;//应用访问WIFI串口无效
		}
		else
		{
			glParam->int1 = PortPeep(glParam->u_char1,glParam->u_str1,glParam->u_short1);
		}
		break;
	case DLL_HEARTBEAT:
		glParam->int1=SetHeartBeat(glParam->u_char1,glParam->int1,glParam->u_str1,glParam->u_short1,glParam->int2);
        break;
    }
}

void PrintFun(void)
{
    switch(glParam->FuncNo)
    {
    case DLL_PRNINIT:
        glParam->u_char1=PrnInit();
        break;
    case DLL_PRNFONTSET:
        PrnFontSet(glParam->u_char1,glParam->u_char2);
        break;
    case DLL_PRNSPACESET:
        PrnSpaceSet(glParam->u_char1,glParam->u_char2);
        break;
    case DLL_PRNLEFTINDENT:
        PrnLeftIndent((int)(glParam->u_short1));
        break;
    case DLL_PRNSTEP:
        //PrnStep(glParam->u_char1);
        PrnStep(glParam->short1);
        break;
    case DLL_PRNLOGO:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1=PrnLogo(glParam->u_str1);
        break;
    case DLL_PRNSTR:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->u_char1=s_PrnStr((char *)glParam->u_str1);
        break;
    case DLL_PRNSTART:
        glParam->u_char1=PrnStart();
        break;
    case DLL_PRNSTATUS:
        glParam->u_char1=PrnStatus();
        break;
    case DLL_PRNGETDOTLINE:
        glParam->int1=PrnGetDotLine();
        break;
    case DLL_PRNSETGRAY:
        PrnSetGray(glParam->int1);
        break;
    case DLL_PRNGETFONTDOT:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 0);
        glParam->int1=PrnGetFontDot(glParam->int1,glParam->u_str1,glParam->u_str2);
        break;
    case DLL_PRNTEMPERATURE:
        glParam->int1 = PrnTemperature();
        break;
	case DLL_PRNDOUBLEWIDTH:	
		PrnDoubleWidth(glParam->int1,glParam->int2);
		break;
	case DLL_PRNDOUBLEHEIGHT:	
		PrnDoubleHeight(glParam->int1,glParam->int2);
		break;
	case DLL_PRNATTRSET:	
		PrnAttrSet(glParam->int1);
		break;
    case DLL_PRNPREFEEDSET:
        glParam->int1=PrnPreFeedSet(glParam->u_int1);
		break;
    }
}

void FileFun(void)
{
    switch(glParam->FuncNo)
    {
    case DLL_OPEN:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->int1=open((char *)glParam->u_str1,glParam->u_char1);
        break;

    case DLL_READ:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->int1=read(glParam->int1,glParam->u_str1,glParam->int2);
        break;

    case DLL_WRITE:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->int1=write(glParam->int1,glParam->u_str1,glParam->int2);
        break;

    case DLL_CLOSE:
        glParam->int1=closefile(glParam->int1);
        break;

    case DLL_SEEK:
        glParam->int1=seek(glParam->int1,glParam->long1,glParam->u_char1);
        break;

    case DLL_REMOVE:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->int1=remove((char *)glParam->u_str1);
        break;

    case DLL_FILESIZE:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->long1=filesize((char *)glParam->u_str1);
        break;

    case DLL_FREESIZE:
        glParam->long1=freesize();
        break;

    case DLL_TRUNCATE:
        glParam->int1=truncate(glParam->int1,glParam->long1);
        break;

    case DLL_FEXIST:
		CheckParamInValid(glParam->u_str1, 0);
        glParam->int1=fexist((char *)glParam->u_str1);
        break;

    case DLL_GETFILEINFO:
		CheckParamInValid(glParam->Addr1, 0);
        glParam->int1=GetFileInfo((FILE_INFO *)glParam->Addr1);
        break;

    case DLL_EX_OPEN:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 0);
        glParam->int1=ex_open((char *)glParam->u_str1,glParam->u_char1,glParam->u_str2);
        break;

    case DLL_FILETOAPP:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1=FileToApp(glParam->Addr1);
		break;

    case DLL_FILETOPARAM:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		glParam->int1=FileToParam(glParam->Addr1,glParam->Addr2,glParam->int1);
		break;

    case DLL_FILETOFONT:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1=FileToFont(glParam->Addr1);
		break;

    case DLL_FILETOMONITOR:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1=FileToMonitor(glParam->Addr1);
		break;

    case DLL_DELAPP:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1=DelAppFile(glParam->Addr1);
		break;			

    case DLL_DELFONT:
		glParam->int1=DelFontFile();
		break;		
#if 0
	case DLL_SSLSAVECERPRIVKEY:
		glParam->int1=SslSaveCerPrivkey((unsigned char *)glParam->up_short1,glParam->u_short1,glParam->Addr1,glParam->int1,glParam->u_str1,glParam->Addr2,glParam->int2,glParam->u_char1,glParam->u_str2,glParam->u_char2);
		break;		
	case DLL_SSLDELCERPRIVKEY:
		glParam->int1=DelCerPrivkey(glParam->Addr1,glParam->u_short1);
		break;	
	case DLL_SSLDELCERPRIVKEY + 1:
		glParam->int1=SslGetCertPrivkey(glParam->u_int1,glParam->u_short1,glParam->Addr1,glParam->int1,glParam->u_str1,glParam->Addr2,glParam->int2,glParam->u_str2);
		break;	
#endif
	case DLL_FILETOPUK:	
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1=FileToPuk(glParam->int1, glParam->int2, glParam->Addr1);
		break;		

    case DLL_FSRECYCLE:
		glParam->int1=FsRecycle(glParam->int1);
        break;

    case DLL_TELL:
		glParam->long1 = tell(glParam->int1);
		break;
    case DLL_FILETOSO:
        glParam->int1=MpConvertToSo(glParam->u_str1,glParam->u_str2);
        break;
    case DLL_FILETODRIVER:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1=FileToDriver(glParam->Addr1,(unsigned int)(glParam->int1));
		break;
	}
}

void OtherFun(void)
{
    switch(glParam->FuncNo)
    {
    case DLL_GETENV:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 0);
        glParam->u_char1=GetEnv((char *)glParam->u_str1,glParam->u_str2);
        break;
    case DLL_PUTENV:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 0);
        glParam->u_char1=PutEnv((char *)glParam->u_str1,glParam->u_str2);
        break;
    case DLL_DES:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 0);
		CheckParamInValid(glParam->u_str3, 0);
        des(glParam->u_str1,glParam->u_str2,glParam->u_str3,glParam->int1);
        break;
    case DLL_RSARECOVER:
       // RSARecover(glParam->u_str1,glParam->int1,glParam->u_str2,glParam->int2,glParam->u_str3,glParam->u_str4);
        break;
    case DLL_HASH:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 0);
        Hash(glParam->u_str1, glParam->u_int1, glParam->u_str2);
        break;
    case DLL_READAPPINFO:
		CheckParamInValid(glParam->Addr1, 0);
        glParam->int1=ReadAppInfo(glParam->u_char1, (APPINFO *)glParam->Addr1);
        break;
    case DLL_READAPPSTATE:
        glParam->u_char1=ReadAppState(glParam->u_char1);
        break;
    case DLL_SETAPPACTIVE:
     //   AppInfo.AppNum=glParam->AppNum;
    //    glParam->int1=SetAppActive(glParam->u_char1,glParam->u_char2);
        break;
    case DLL_RUNAPP:
        glParam->int1=RunApp(glParam->u_char1);
        if(glParam->int1==0)
		glParam->u_int1=app_even_start();
        break;
    case DLL_DOEVENT:
		CheckParamInValid(glParam->Addr1, 1);
		glParam->int1=DoEvent(glParam->u_char1,glParam->Addr1);
		if(glParam->int1==0) glParam->u_int1=app_even_start();
	//	ScrPrint(0,0,0,"%08x,%08x",glParam->u_int1=k_RunAppStartAddr,glParam->u_int1);getkey();
		break;
	case DLL_CHECKMANAGE:
		glParam->u_char1 = CheckIfManageExist();
		break;
    case DLL_GETMATRIXDOT:
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->u_str2, 0);
        s_GetMatrixDot(glParam->u_str1,glParam->u_str2, &glParam->u_short1, glParam->u_char1);
     //   glParam->u_short1=glParam->u_char2;
        //uchar s_GetMatrixDot(uchar *str,uchar *MatrixDot,uchar *len, uchar font);
        break;
//    case DLL_SSRAMTEST:
//        glParam->u_char1=s_SramTest();
//        break;
//    case DLL_ERASESN:
//         glParam->u_char1=EraseSN(glParam->u_char1);
//         break;
    case DLL_UARTECHO:
        //uart_echo((char *)glParam->u_str1);
        break;
    case DLL_UARTECHOIN:
        //uart_echoin((char *)glParam->u_str1);
        break;
    case DLL_UARTPRINTF:
        //s_uartprintf((char *)glParam->u_str1);
        break;
    case DLL_UARTECHOCH:
        //glParam->int1=uart_echoch(glParam->int1,(char *)glParam->u_str1);
        break;

    }
}


void LineNetFun(void)
{
	switch(glParam->FuncNo) {
	case DLL_NET_SOCKET:
		glParam->int1 = NetSocket(glParam->int1,glParam->int2,glParam->long1);
		break;
	case DLL_NET_BIND:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = NetBind(glParam->int1, glParam->Addr1,glParam->int2);
		break;
	case DLL_NET_CONNECT:
		CheckParamInValid(glParam->Addr1, 0);
		if( is_wl_route(glParam->int1, glParam->Addr1,glParam->int2))//wireless
		{
			glParam->int1 = wl_net_connect(glParam->int1, glParam->Addr1,glParam->int2);
		}
		else
		{
			glParam->int1 = NetConnect(glParam->int1, glParam->Addr1,glParam->int2);
		}			
		break;
	case DLL_NET_LISTEN:
		glParam->int1 = NetListen(glParam->int1,glParam->int2);		
		break;
	case DLL_NET_ACCEPT:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		glParam->int1 = NetAccept(glParam->int1, glParam->Addr1,glParam->Addr2);		
		break;
	case DLL_NET_SEND:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = NetSend(glParam->int1,glParam->Addr1,glParam->int2,glParam->long1);
		break;
	case DLL_NET_SENDTO:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		glParam->int1 = NetSendto(glParam->int1,glParam->Addr1,glParam->int2,glParam->u_int1,glParam->Addr2,glParam->long1);
		break;
	case DLL_NET_RECV:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = NetRecv(glParam->int1,glParam->Addr1,glParam->int2,glParam->long1);
		break;
	case DLL_NET_RECVFROM:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		CheckParamInValid(glParam->u_str1, 0);
		glParam->int1 = NetRecvfrom(glParam->int1,glParam->Addr1,glParam->int2,glParam->long1, glParam->Addr2,glParam->u_str1);
		break;
	case DLL_NET_CLOSESOCKET:
		glParam->int1 = NetCloseSocket(glParam->int1);
		break;
	case DLL_NET_IOCTL:
		glParam->int1 = Netioctl(glParam->int1,glParam->int2,glParam->long1);		
		break;
	case DLL_SOCKADDR_SET:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->u_str1, 1);
		glParam->int1 = SockAddrSet(glParam->Addr1,glParam->u_str1,glParam->u_short1);				
		break;
	case DLL_SOCKADDR_GET:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->u_str1, 0);
		CheckParamInValid(glParam->up_short1, 0);
		glParam->int1 = SockAddrGet(glParam->Addr1,glParam->u_str1,glParam->up_short1);				
		break;
	case DLL_DHCP_START:
		glParam->int1 = DhcpStart();
		break;
	case DLL_DHCP_STOP:
		glParam->int1 = DhcpStop();
		break;
	case DLL_DHCP_CHECK:
		glParam->int1 = DhcpCheck();
		break;	
	case DLL_PPP_LOGIN:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		glParam->int1 = PPPLogin(glParam->Addr1,glParam->Addr2,glParam->long1,glParam->int1);		
		break;
	case DLL_PPP_LOGOUT:
		glParam->int1 = PPPLogout();
		break;
	case DLL_PPP_CHECK:
		glParam->int1 = PPPCheck();
		break;
/*	case DLL_ETH_MAC_SET:
		glParam->int1 = eth_mac_set(glParam->u_str1);		
		break;*/
	case DLL_ETH_MAC_GET:
		CheckParamInValid(glParam->u_str1, 0);
		glParam->int1 = eth_mac_get(glParam->u_str1);
//		showmac(6);
		break;
	case DLL_ETH_SET:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		CheckParamInValid(glParam->u_str1, 1);
		CheckParamInValid(glParam->u_str2, 1);
		glParam->int1 = EthSet(glParam->Addr1,glParam->Addr2,glParam->u_str1,glParam->u_str2);
		break;
	case DLL_ETH_GET:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		CheckParamInValid(glParam->u_str1, 1);
		CheckParamInValid(glParam->u_str2, 1);
		CheckParamInValid(glParam->up_short1, 1);
		glParam->int1 = EthGet(glParam->Addr1,glParam->Addr2,glParam->u_str1,glParam->u_str2,glParam->up_short1);
		break;
	case DLL_ETH_SET_RATE_DUPLEX_MODE:
		EthSetRateDuplexMode(glParam->int1);
		break;        
	case DLL_DNS_RESOLVE:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		glParam->int1 = DnsResolve(glParam->Addr1,glParam->Addr2,glParam->int1);
		break;
    case DLL_DNS_RESOLVE_EXT:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		glParam->int1 = DnsResolveExt(glParam->Addr1,glParam->Addr2,glParam->int1,glParam->int2);
        break;        
	/*case DLL_DNS_RESOLVE_IP:
		glParam->int1 = DnsResolveIp(glParam->Addr1,glParam->up_short1);
		break;*/
	case DLL_NET_SET_DHCP_DNS:
        CheckParamInValid(glParam->Addr1, 0);
        glParam->int1 = DhcpSetDns(glParam->Addr1);
        break;
	case DLL_NET_PING:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = NetPing(glParam->Addr1,glParam->long1,glParam->int1);
		break;
	case DLL_ROUTE_SET_DEFAULT:
		glParam->int1 = RouteSetDefault(glParam->int1);
		break;
	case DLL_ROUTE_GET_DEFAULT:
		glParam->int1 = RouteGetDefault();		
		break;
	case DLL_NET_DEV_GET:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		CheckParamInValid(glParam->u_str1, 1);
		CheckParamInValid(glParam->u_str2, 1);
		glParam->int1  = NetDevGet(glParam->int1,/* Dev Interface Index */ 
                   glParam->Addr1,
                   glParam->Addr2,
                   glParam->u_str1,
                   glParam->u_str2
                   );
		break;
	case DLL_NET_ADD_STATICARP:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		glParam->int1  = NetAddStaticArp(
                   glParam->Addr1,
                   glParam->Addr2
                   );		
		break;
	case DLL_NET_SETICMP:
		NetSetIcmp(glParam->u_long1 );		
		break;

	case DLL_PPPOE_LOGIN:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		glParam->int1 = PppoeLogin(glParam->Addr1,glParam->Addr2,glParam->int1);		
		break;
	case DLL_PPPOE_LOGOUT:
		glParam->int1 = PppoeLogout();
		break;
	case DLL_PPPOE_CHECK:
		glParam->int1 = PppoeCheck();
		break;
	case  DLL_ETH_GET_FIRST_ROUTE_MAC:
		glParam->int1 = EthGetFirstRouteMac(glParam->u_str1, glParam->u_str2,glParam->int1);
		break;

		
	case DLL_WIFI_OPEN:
		glParam->int1 = WifiOpen();
		break;
	case DLL_WIFI_CLOSE:
		glParam->int1 = WifiClose();
		break;
	case DLL_WIFI_SCAN:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = WifiScan((ST_WIFI_AP *)(glParam->Addr1), (unsigned int)glParam->int1);
		break;
    case DLL_WIFI_SCAN_EXT:
        CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = WifiScanEx((ST_WIFI_AP_EX *)(glParam->Addr1), (unsigned int)glParam->int1);
        break;
	case DLL_WIFI_CONNECT:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		glParam->int1 = WifiConnect((ST_WIFI_AP *)(glParam->Addr1),(ST_WIFI_PARAM *)(glParam->Addr2));
		break;
	case DLL_WIFI_DISCON:
		glParam->int1 = WifiDisconnect();
		break;
	case DLL_WIFI_CHECK:
		glParam->int1 = WifiCheck((ST_WIFI_AP *)(glParam->Addr1));
		break;
	case DLL_WIFI_IOCTRL:
		glParam->int1 = WifiCtrl((unsigned int)glParam->long1, glParam->Addr1, (unsigned int)glParam->int1, glParam->Addr2, (unsigned int)glParam->int2);
		break;
	default:
		break;
	}
}

void FATFun()
{
	switch(glParam->FuncNo) {
	case DLL_FAT_OPEN:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsOpen(glParam->Addr1,glParam->int1);
		break;
	case DLL_FAT_CLOSE:
		glParam->int1 = FsClose(glParam->int1);
		break;
	case DLL_FAT_DELETE:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsDelete(glParam->Addr1);
		break;		
	case DLL_FAT_GETINFO:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsGetInfo(glParam->int1,glParam->Addr1);
		break;	
	case DLL_FAT_RENAME:
		CheckParamInValid(glParam->Addr1, 0);
		CheckParamInValid(glParam->Addr2, 0);
		glParam->int1 = FsRename(glParam->Addr1,glParam->Addr2);
		break;	
	case DLL_FAT_DIRSEEK:
		glParam->int1 = FsDirSeek(glParam->int1,glParam->int2,glParam->long1);
		break;
	case DLL_FAT_DIRREAD:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsDirRead(glParam->int1,glParam->Addr1,glParam->int2);
		break;		
	case DLL_FAT_FILEREAD:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsFileRead(glParam->int1,glParam->Addr1,glParam->int2);
		break;	
	case DLL_FAT_FILEWRITE:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsFileWrite(glParam->int1,glParam->Addr1,glParam->int2);
		break;	
	case DLL_FAT_FILESEEK:
		glParam->int1 = FsFileSeek(glParam->int1,glParam->int2,glParam->long1);
		break;
	case DLL_FAT_TELL:
		glParam->int1 = FsFileTell(glParam->int1);
		break;
	case DLL_FAT_TRUNCATE:
		glParam->int1 = FsFileTruncate(glParam->int1,glParam->int2,glParam->long1);
		break;
	case DLL_FAT_SET_DIR:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsSetWorkDir(glParam->Addr1);
		break;
	case DLL_FAT_GET_DIR:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsGetWorkDir(glParam->Addr1);
		break;			
	case DLL_FAT_UDISKIN:
		glParam->int1 = FsUdiskIn();
		break;
	case DLL_FAT_GETDISKINFO:
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = FsGetDiskInfo(glParam->int1,glParam->Addr1);
		break;
	}

}

void RemoteFun()
{
	switch(glParam->FuncNo)
	{
    	case DLL_REMOTELOADAPP :
    		CheckParamInValid(glParam->Addr1, 0);
    		glParam->int1 = RemoteLoadApp(glParam->Addr1);
    		break;
        case DLL_PROTIMSENTRY:
       		CheckParamInValid(glParam->Addr1, 0);
            ProTimsEntry(glParam->Addr1);
            break;
    	default:
    		break;
	}
}
void ScanFun(void)
{
    switch(glParam->FuncNo)
    {
        case DLL_SCAN_OPEN:
            glParam->int1 = ScanOpen();
            break;
        case DLL_SCAN_CLOSE:
            ScanClose();
            break;
        case DLL_SCAN_READ:
            glParam->int1 = ScanRead(glParam->u_str1, glParam->u_int1);
            break;
    }
}

void SSLFun()
{
#if 0
		switch(glParam->FuncNo)
		{
		case DLL_SSL_CERTREAD :
			CheckParamInValid(glParam->Addr1, 0);
			CheckParamInValid(glParam->Addr2, 0);
			glParam->int1 = ssl_certRead(glParam->Addr1,glParam->Addr2,glParam->short1);
			break;
		case DLL_SSL_VERIFYCERT :
			CheckParamInValid(glParam->Addr1, 0);
			CheckParamInValid(glParam->Addr2, 0);
			glParam->int1 = ssl_verifyCert(glParam->Addr1,glParam->Addr2);
			break;
		case DLL_SSL_COPYCERT :
			CheckParamInValid(glParam->Addr1, 0);
			CheckParamInValid(glParam->Addr2, 0);
			glParam->int1 = ssl_copyCert(glParam->Addr1,glParam->Addr2,glParam->u_int1);
			break;
		case DLL_SSL_SIGN:
			CheckParamInValid(glParam->Addr1, 0);
			CheckParamInValid(glParam->Addr2, 0);
			glParam->int1 = ssl_sign(glParam->Addr1,glParam->Addr2,glParam->int1,glParam->u_str1,glParam->int2);
			break;
		case DLL_SSL_VER:
			glParam->int1 =ssl_ver();
			break;
		case DLL_SSL_PKEYENCRYPT:
			CheckParamInValid(glParam->Addr1, 0);
			CheckParamInValid(glParam->Addr2, 0);
			glParam->int1 =ssl_pkeyEncrypt(glParam->Addr1,glParam->Addr2,glParam->int1);
			break;
		default:
			break;
		}		
#endif

}

void ExWNetFun()
{
	switch(glParam->FuncNo)
	{
	case DLL_WXNETINIT :
		CheckParamInValid(glParam->Addr1, 1);
		glParam->int1 = WlInit(glParam->Addr1);
		break;
	case DLL_WXNETGETSIGNAL :
		CheckParamInValid(glParam->Addr1, 0);
		glParam->int1 = WlGetSignal(glParam->Addr1);
		break;
	case DLL_WXNETPPPLOGIN :
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		CheckParamInValid(glParam->u_str1, 1);
		
		wl_ppp_login_ex_param(NULL, glParam->Addr1,glParam->Addr2,glParam->u_str1,glParam->long1,glParam->int1,glParam->int2);
		
		glParam->int1 = WlPppLogin(glParam->Addr1,glParam->Addr2,glParam->u_str1,glParam->long1,glParam->int1,glParam->int2);
		break;
	case DLL_WXNETPPPLOGOUT:
		WlPppLogout();
		break;
	case DLL_WXNETPPPCHECK:
		glParam->int1 =WlPppCheck();
		break;
	case DLL_WXPORTOPEN:
		glParam->int1 =WlOpenPort();
		break;
	case DLL_WXPORTCLOSE:
		glParam->int1 =WlClosePort();
		break;
	case DLL_WXNETSENDCMD:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		glParam->int1 =WlSendCmd(glParam->Addr1,glParam->Addr2,glParam->u_short1,glParam->long1,glParam->u_short2);
		break;
	case DLL_WXNETPOWERSWITCH:
		WlSwitchPower(glParam->u_char1);
		break;
    case DLL_WXNETSELSIM:
        glParam->int1 = WlSelSim(glParam->u_char1);
        break;
	case DLL_WXNETAUTOSTART:
		CheckParamInValid(glParam->u_str1, 1);
		CheckParamInValid(glParam->u_str2, 1);
		CheckParamInValid(glParam->u_str3, 1);
		CheckParamInValid(glParam->u_str4, 1);
        glParam->int1 = WlAutoStart(glParam->u_str1,glParam->u_str2,glParam->u_str3,glParam->u_str4,glParam->u_long1,glParam->int1,glParam->int2);
        break;
    case DLL_WXNETAUTOCHECK:
        glParam->int1 = WlAutoCheck();
        break;		
	case DLL_WXNETPPPLOGIN_EX:
		CheckParamInValid(glParam->Addr1, 1);
		CheckParamInValid(glParam->Addr2, 1);
		CheckParamInValid(glParam->u_str1, 1);
		wl_ppp_login_ex_param(glParam->u_str2, glParam->Addr1,glParam->Addr2,glParam->u_str1,glParam->long1,glParam->int1,glParam->int2);	
		glParam->int1 = WlPppLoginEx(glParam->u_str2, glParam->Addr1,glParam->Addr2,glParam->u_str1,glParam->long1,glParam->int1,glParam->int2);
		break;		  
    case DLL_WXTCPRETRYNUM:
        WlTcpRetryNum(glParam->int1);
        break;        
    case DLL_WXSETTCPDETECT:
        WlSetTcpDetect(glParam->int1);
		break;		  
	case DLL_WXSETDNS:
		glParam->int1 = WlSetDns(glParam->Addr1);
		break;
	default:
		break;
	}

}

#if 1 
void BtFun(void)
{
	switch(glParam->FuncNo)
	{
		case DLL_BTOPEN:
			//glParam->int1 = BtOpen();
			break;
		case DLL_BTCLOSE:
			//glParam->int1 = BtClose();
			break;
		case DLL_BTGETCONFIG:
			CheckParamInValid(glParam->Addr1, 0);
			//glParam->int1 = BtGetConfig(glParam->Addr1);
			break;
		case DLL_BTSETCONFIG:
			CheckParamInValid(glParam->Addr1, 0);
			//glParam->int1 = BtSetConfig(glParam->Addr1);
			break;
		case DLL_BTSCAN:
			CheckParamInValid(glParam->Addr1, 0);
			//glParam->int1 = BtScan(glParam->Addr1, glParam->u_int1, (unsigned int)glParam->u_long1);
			break;
		case DLL_BTCONNECT:
			CheckParamInValid(glParam->Addr1, 0);
			//glParam->int1 = BtConnect(glParam->Addr1);
			break;
		case DLL_BTDISCONNECT:
			//glParam->int1 = BtDisconnect();
			break;
		case DLL_BTGETSTATUS:
			CheckParamInValid(glParam->Addr1, 0);
			//glParam->int1 = BtGetStatus(glParam->Addr1);
			break;
		case DLL_BTIOCTRL: 				
			//glParam->int2 = BtCtrl((int)glParam->int1, glParam->Addr1,glParam->u_int1,\
			//	glParam->Addr2, glParam->u_long1);
			break;
		case DLL_MODULEUPDATE:
			//ModuleUpdate();
			break;
		default:
			break;
	}
}
#else //deleted by shics 
void BtFun(void)
{
	switch(glParam->FuncNo)
	{
		case DLL_BTOPEN:
			glParam->int1 = BtOpen();
			break;
		case DLL_BTCLOSE:
			glParam->int1 = BtClose();
			break;
		case DLL_BTGETCONFIG:
			CheckParamInValid(glParam->Addr1, 0);
			glParam->int1 = BtGetConfig(glParam->Addr1);
			break;
		case DLL_BTSETCONFIG:
			CheckParamInValid(glParam->Addr1, 0);
			glParam->int1 = BtSetConfig(glParam->Addr1);
			break;
		case DLL_BTSCAN:
			CheckParamInValid(glParam->Addr1, 0);
			glParam->int1 = BtScan(glParam->Addr1, glParam->u_int1, (unsigned int)glParam->u_long1);
			break;
		case DLL_BTCONNECT:
			CheckParamInValid(glParam->Addr1, 0);
			glParam->int1 = BtConnect(glParam->Addr1);
			break;
		case DLL_BTDISCONNECT:
			glParam->int1 = BtDisconnect();
			break;
		case DLL_BTGETSTATUS:
			CheckParamInValid(glParam->Addr1, 0);
			glParam->int1 = BtGetStatus(glParam->Addr1);
			break;
		case DLL_BTIOCTRL: 				
			glParam->int2 = BtCtrl((int)glParam->int1, glParam->Addr1,glParam->u_int1,\
				glParam->Addr2, glParam->u_long1);
			break;
		case DLL_MODULEUPDATE:
			ModuleUpdate();
			break;
		default:
			break;
	}
}
#endif

void MonitorHalt(void)
{
    ScrPrint(0,2,0,"DATA ACCESS EXCEPTION!\n");
	Beep();
	while(1);
}

/* 
	检查参数合法性
	参数:
		iNullIsValid: 0-Null addr is invalid; 1-Null addr is valid
*/
void CheckParamInValid(void * uiAddr,int iNullIsValid)
{
	if(uiAddr!=NULL || iNullIsValid) return;
	ScrCls();
    CLcdSetFgColor(COLOR_BLACK);
	CLcdSetBgColor(COLOR_WHITE);

	SCR_PRINT(0,0,0x01,"     警 告      ", "    WARNING!\n");
	SCR_PRINT(0,3,0x01,"程序出错了","ACCESS EXCEPTION\n");//居中显示
	ScrPrint(0,6,0,"0x%08X",uiAddr);
	ScrPrint(0,7,0," %d:%d ",glParam->FuncType,glParam->FuncNo);
	while(1);  
}

void  TiltSensorFun(void)
{
	switch(glParam->FuncNo)
	{
		case DLL_TILTSENSOR_GETLEANANGLE:
			CheckParamInValid(glParam->u_str1, 0);
			CheckParamInValid(glParam->u_str2, 0);
			CheckParamInValid(glParam->u_str3, 0);
			GetLeanAngle(glParam->u_str1,glParam->u_str2,glParam->u_str3);
			break;  
	}   
}

void WaveFun(void)
{
    switch(glParam->FuncNo)
    {
        case DLL_SOUNDPLAY:
            CheckParamInValid(glParam->u_str1, 0);
            glParam->int1 = SoundPlay(glParam->u_str1, glParam->char1);
            break;

		default:
			break;
    }
}

void GpsFun(void)
{
	switch(glParam->FuncNo)
	{
		case DLL_GPSOPEN:
			glParam->int1 = GpsOpen();
			break;
		case DLL_GPSREAD:
			CheckParamInValid(glParam->Addr1, 0);
			glParam->int1 = GpsRead(glParam->Addr1);
			break;
		case DLL_GPSCLOSE:
			glParam->int1 = GpsClose();
			break;
		default:
			break;
	}
}

void ProcFunc(void)
{
    switch(glParam->FuncType)
    {
        case DLL_SYS_FUN:
			SysFun();
            break;
        case DLL_IC_FUN:
            IcFun();
            break;
        case DLL_MAG_FUN:
            MagFun();
            break;
        case DLL_PICC_FUN:
            PiccFun();
            break;
        case DLL_LCD_FUN:
            LcdFun();
            break;
		case DLL_TOUCHSCREEN_FUN:
			TouchScreenFun();
			break;
        case DLL_KEYBOARD_FUN:
            KeyboardFun();
            break;
        case DLL_COMM_FUN:
            CommFun();
            break;
        case DLL_PRINT_FUN:
            PrintFun();
            break;
        case DLL_FILE_FUN:
            FileFun();
//          glParam->int2=errno;
            break;
        case DLL_PED_FUN:
            PedFun(glParam);
            break;
        case DLL_IRDA_FUN:
             break;
        case DLL_OTHER_FUN:
             OtherFun();
             break;
        case DLL_WNET_FUN:
             WNetFun();
             break;
        case DLL_REMOTE_FUN:
             RemoteFun();
              break;
        case DLL_PCI_FUN:
        	 break;
		case DLL_LNET_FUN:
			 LineNetFun();
			 break;
		case DLL_FAT_FUN:
			 FATFun();
			 break;
		case DLL_EXWNET_FUN:
			ExWNetFun();
			break;
		case DLL_SSL_FUN:
			SSLFun();
			break;
        case DLL_TILTSENSOR_FUN:
            TiltSensorFun();
            break;
		case DLL_SCAN_FUN:
            ScanFun();
			break;
        case DLL_WAVE_FUN:
            WaveFun();
            break;
		case DLL_BT_FUN:
			BtFun();
			break;
		case DLL_GPS_FUN:
			GpsFun();
			break;
            
    }
}

int DispCustomLogo(void)
{
	PARAM_STRUCT param;
	int iRet;
	
	param.FuncType = MPATCH_STARTLOGO_FUNC;
	param.FuncNo = MPATCH_UMS_STARTLOGO;
	iRet = s_MpatchEntry(MPATCH_NAME_STARTLOGO, &param);
	if (iRet < 0) return iRet;
	return 0;
}

