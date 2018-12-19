#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "base.h"
#include "printer_stylus.h"
#include "..\..\font\font.h"
#include "..\..\cfgmanage\cfgmanage.h"

//Printer CMD
#define PRN_END         0x00
#define PRN_LF          0x0a
#define PRN_FF          0x0c
#define PRN_STEP        0x19
#define PRN_SPACE_SET   0x20
#define PRN_FONT_SET    0x57
#define PRN_LEFT_INDENT 0x6c 
#define PRN_STR         0x43
#define PRN_LOGO        0x59
#define PRN_FONT_SEL 	0x55

#define PRN_OK          0x00
#define PRN_PRINTING    0x01
#define PRN_NOPAPER     0x02
#define PRN_DATAERROR   0x03
#define PRN_ERROR       0x04
#define PRN_NOFONTLIB   0xfc

#define MAX_LINES       500
static int RIGHT_ENCODER;
static int LEFT_ENCODER;
//定义默认每行点数
#define LINE_DOT_DEFAULT 8	
//从检查有纸到切纸位置
#define CNT_STEP_SLIP 154
//定义从切纸位置到打印位置需要倒退点数
#define CNT_STEP_PRNSTAR -240

#define PRNDATA         0    //前8针数据
#define PRNCTRL         1    //打印模块控制
volatile uchar  k_PortPrnData;
volatile uchar  k_PortPrnCtrl;
//     bit7   bit6    bit5   bit4   bit3    bit2   bit1   bit0
//       0     PIN    PWR   MOTOR   STEP    PHB    PHA   PRNPIN8
//走纸控制
#define enable_motor_paper()  k_PortPrnCtrl&=(~(1<<3));
#define disable_motor_paper() k_PortPrnCtrl|=(1<<3);
//打印头控制
#define enable_motor_header()  k_PortPrnCtrl&=(~(1<<4));
#define disable_motor_header() k_PortPrnCtrl|=(1<<4);

//电源控制
#define enable_prn_power()  k_PortPrnCtrl|=(1<<5);
#define disable_prn_power() k_PortPrnCtrl&=(~(1<<5));


#define disable_prn_all()	k_PortPrnData=0xff;k_PortPrnCtrl=0x5f;

static int LINE_WITH_DOT;

int k_PrnSmallFontFlag;   // 每行打印240个像素点

//For printer status
volatile uchar              k_PrnStatus;        // 打印状态
static unsigned short PAGE_DOTLINES;
//for step
volatile static uchar  *k_StepPHABptr,*k_StepPHABEnd;
volatile static ushort k_CurStepNum,k_CurStepCnt;
volatile static ushort k_PrnStepOK;

const  uchar  STEP_PHAB[8] = {0x51,0x55,0x57,0x53,0x41,0x45,0x47,0x43};
const  uchar  STEP_bPHAB[8]= {0x53,0x57,0x55,0x51,0x43,0x47,0x45,0x41};

volatile ushort k_PrinterBusy;
volatile int k_FeedKeyStatus;//0=release  1=slip fw  2=slip bw  3=auto slip
volatile static ushort k_Encoder;
static ushort k_FirstStep,k_NoPaperCnt;
static ushort k_PrintFlag,k_StepTimer,k_RightPrint;
static ushort k_SetupParam,k_Prn9Pin,k_LineWidth,k_PrintData;
static uchar  *k_LeftNextPtr,*k_RightNextPtr,*k_LeftNextPtr9,*k_RightNextPtr9;
static uchar  *k_CurDotPtr,*k_CurDotPtr9;
static uchar  k_ResultFlag,k_PrnResult;
static short          k_LineStep[MAX_LINES+10],k_DoubleFlag[MAX_LINES+10];
static short          *k_LineStepPtr,*k_LineStepEnd,*k_DoubleFlagPtr;
static short          k_RightStopT,k_LeftStopT;
extern uchar k_DotBuf[];
uchar  *k_LineDot = k_DotBuf; //不单独申请内存，与热敏打印机复用内存
//added by tanjue 记录是否换行 1表示遇到了\n，换行
int k_PrnLF;
//added by tanjue 记录是否换行 1表示遇到了\n，换行

//for generate dot
static ushort k_CurCol;
static uchar  *k_DotPtr;
static uint  k_AllLines;
static uchar  k_LeftIndent,k_CurLineHigh;
static uchar  k_AsciiMode,k_CfontMode;
static uchar  k_CharSpace,k_LineSpace;
volatile static ushort k_AutoStepCnt,k_StepCnt,k_LockPaperCnt,k_TimerStopCnt;
volatile static ushort k_PrnPinTime,k_PrnPinTimeCnt;
volatile static uint  k_PrnPinTimeTotal;
static ushort k_FeedAutoCnt,k_FeedBackCnt,k_FeedForCnt;
//extern volatile int k_Timer2User;
volatile int k_Timer2User; //和IC卡共用？？？
//0: no user   1: for cpu card 2: for step in printing  3: for paper feed
//extern volatile int k_Timer3User;
volatile int k_Timer3User;
//0: no user   1: for cpu card 2: for encode timing check  3: for lock paper

//check home 
#define MIN_ENCODER_CNT_VALID	5
#define MAX_ERR_LOST_CNT		1

volatile static uchar k_homeCount=0;
volatile static uchar k_homeFlag=0;
volatile static uchar k_homeValid=0;

char get_prn_hysteresis(void);
#define MAX_HYSTERESIS    6
static char hysteresis_err;

volatile uchar gucCountFlag = 0;

static unsigned char  *k_LeftNextPtrBak,*k_RightNextPtrBak,*k_LeftNextPtr9Bak,*k_RightNextPtr9Bak;
static uchar encode_print_cnt=0;
static uchar home_mis=0;
static short home_cnt[4],cnt_idx;
static ushort last_home_flag;
#define LockPaper_SEC(a) k_LockPaperCnt=a*100;k_LockPaperCnt=a*100;StartTimerEvent(PRINTER_HANDLER, 1);
#define StartCntEven() k_TimerStopCnt=20;k_TimerStopCnt=20; StartTimerEvent(PRINTER_HANDLER,1);


void ps_PrnInitStatus(void)
{
	k_PrnResult = PRN_OK;
}

void ps_PrnSmallFontInit(void)
{
    if(k_PrnSmallFontFlag)
    {
        LEFT_ENCODER = 813*2;
        RIGHT_ENCODER = 455*2;
        LINE_WITH_DOT = 240;
    }
    else
    {
        LEFT_ENCODER = 813;
        RIGHT_ENCODER = 455;          
        LINE_WITH_DOT = 180;    
    }
}

static void ps_PrnControl(unsigned char part,unsigned char value)
{
	unsigned short i,j;
	unsigned short ctch;

	switch(part)
	{
	case PRNDATA:

        s_SetPrnHeader1_8(value);

        k_PortPrnData = value;
		break;

	case PRNCTRL:

        s_SetPrnControl(value);
		
		k_PortPrnCtrl = value;
		break;
	}
}

volatile uchar GuardTimerIntFlag=0;

void s_GuardTimer_Int(void)
{
	CLEAR_GUARD_TIMER_INT();
    GuardTimerIntFlag = 1;
}

void s_SetGuardTimer(unsigned short wtus)
{
    GuardTimerIntFlag = 0;
    s_GuardTimerStart(wtus);
}

#define s_IsGuardTimerOver() (GuardTimerIntFlag)

static void StopLockEven()
{
	
	k_LockPaperCnt=0;
	k_LockPaperCnt=0;
	if((k_TimerStopCnt == 0)&&(k_LockPaperCnt == 0))
	{
		StopTimerEvent(PRINTER_HANDLER);
	}
}

static void StopCntEven()
{
	k_TimerStopCnt=0;
	k_TimerStopCnt=0;
	if((k_TimerStopCnt == 0)&&(k_LockPaperCnt == 0))
	{
		StopTimerEvent(PRINTER_HANDLER);
	}
}
void ps_PrintOver(void)
{
	DISABLE_ENCODE_INT();	//disenable eint0 interrupt (enoder int)
    StopCntEven();
    disable_prn_power();
	k_PortPrnCtrl |= 0x51;
	k_PortPrnCtrl |= (1<<3);    //  yaocy 20061207
	ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
	k_PortPrnData=0xff;
	ps_PrnControl(PRNDATA, k_PortPrnData);
	DISABLE_PRNOUT_PWM();
	k_ResultFlag=1;
	k_PrinterBusy=0;
}

static void ps_PrinterEven()
{
	if(k_TimerStopCnt)
	{
		if(--k_TimerStopCnt == 0)
		{
			k_PrnResult = PRN_ERROR;
			ps_PrintOver();
		}
	}
	if(k_LockPaperCnt)
	{
		if(--k_LockPaperCnt == 0) 
		{
			disable_prn_power();  //power off
			k_PortPrnCtrl |= 0x08;
			ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
			StopLockEven();
			return;
		}
	}
}

void ps_PrnInit(void)
{
	k_PrinterBusy   = 0;
	k_ResultFlag    = 0;
	k_PrnResult     = PRN_OK;
	k_PrnStepOK     = 1;
	PAGE_DOTLINES   = 396;
	k_FeedKeyStatus = 0;//0=release  1=slip fw  2=slip bw  3=auto slip
    k_PrnSmallFontFlag = 0;
    k_LineDot = k_DotBuf;
    s_PrinterHardwareInit();
    
	k_PortPrnCtrl = 0x5f;
	k_PortPrnData = 0xff;
	ps_PrnControl(PRNDATA, k_PortPrnData);
	ps_PrnControl(PRNCTRL, k_PortPrnCtrl);

	memset(k_LineStep,0x00,sizeof(k_LineStep));
	k_CurLineHigh=LINE_DOT_DEFAULT;

	s_SetTimerEvent(PRINTER_HANDLER, ps_PrinterEven);    
}

static void ps_PrnIntStep(void)
{
	unsigned char ch;

	k_CurStepCnt++;
	if(k_CurStepCnt > k_CurStepNum)
	{
		k_PortPrnCtrl = ((k_PortPrnCtrl & 0xf9) | (*k_StepPHABptr & 0x06)); //disable step
		k_PortPrnCtrl |= (1<<3);        //  yaocy 20061207
		ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
		s_PaperMStepTimerStop();
		k_PrnStepOK = 1;
		return;
	}
	if(++k_StepPHABptr == k_StepPHABEnd) k_StepPHABptr -= 4;                //drive step

	k_PortPrnCtrl = ((k_PortPrnCtrl & 0xf9) | (*k_StepPHABptr & 0x06));     //  yaocy 20061206
	k_PortPrnCtrl &= ~(1<<3);       //  yaocy 20061207
	ps_PrnControl(PRNCTRL, k_PortPrnCtrl);

	if(have_paper()) k_NoPaperCnt=0;
	else	k_NoPaperCnt++;			

	s_PaperMStepTimerStart(2860); //2.86ms
}

//adjust drive step
static void ps_AdjustStep(void)
{
	k_StepPHABEnd=k_StepPHABptr+4;
	while(1)
    {
		if((*k_StepPHABptr & 0x06) == (k_PortPrnCtrl & 0x06)) break;
		k_StepPHABptr++;
		if(k_StepPHABptr==k_StepPHABEnd) k_StepPHABptr-=4;
	}
}
//feed paper while printing
static void ps_PrnStep(short StepNum)
{
	uchar ch;

	//printk("\n\rStepNum:%d\n\r",StepNum);
	k_PrnStepOK = 0;
	if(!StepNum)
	{
		k_PrnStepOK = 1;
		return;
	}
	//printk("\n\rk_PortPrnCtrl:%x\n\r",k_PortPrnCtrl);
	if(StepNum > 0)
	{
		if(k_PortPrnCtrl & 0x10) k_StepPHABptr = (uchar *)STEP_PHAB;
		else k_StepPHABptr = (uchar *)STEP_PHAB + 4;
	}
	else
	{
		if(k_PortPrnCtrl & 0x10) k_StepPHABptr = (uchar *)STEP_bPHAB;
		else k_StepPHABptr = (uchar *)STEP_bPHAB + 4;
		StepNum = -StepNum;
	}
	ps_AdjustStep();

	if(k_StepPHABptr == k_StepPHABEnd) k_StepPHABptr -= 4;

	k_CurStepNum = StepNum;
	k_CurStepCnt = 0;

	k_PortPrnCtrl  = ((k_PortPrnCtrl & 0xf9) | (*k_StepPHABptr & 0x06));     //  yaocy 20061206
	k_PortPrnCtrl &= ~(1<<3);      //  yaocy 20061207
	ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
	s_PaperMStepTimerStart(2860);// //2.86MS
}

static void ps_FeedDrvInt(void)
{
	unsigned char ch;

	while(1)
	{
		k_StepTimer++;
		if(k_StepTimer < 180)
		{
			if(k_FeedKeyStatus == 3) k_StepTimer = 180;//auto slip
			else if(!k_FeedKeyStatus) break;
            else if(k_StepTimer > 4)
			{
				s_PaperMStepTimerStart(2220);// //2.22MS
				return;
			}
		}

		if(k_StepTimer == 2000) break;

		if(k_PrinterBusy == 1)
		{
			if(k_FeedKeyStatus == 3) k_PrinterBusy = 2;
			else if(!k_FeedKeyStatus) break;
		}

		if(k_PrinterBusy == 3)  //Auto  b feed
		{
			if(--k_AutoStepCnt == 0) break;
			if (!have_paper())     //no paper
			{
				k_StepPHABptr = (unsigned char *)STEP_PHAB;
				k_PrinterBusy = 4;
				ps_AdjustStep();
				k_AutoStepCnt = 1200;
				k_StepCnt     = 0;
			}
		}
		else if(k_PrinterBusy == 4)     //Auto  f feed
		{
			if(--k_AutoStepCnt == 0) break;
			if(have_paper()) 
			{
				if(++k_StepCnt == CNT_STEP_SLIP) break;  //  146   //  152 yaocy20061025;  //Head len ???
			}
		}
		else if(k_PrinterBusy == 2)      //Auto feed
		{
			if(have_paper())     //detect paper
			{
				k_StepPHABptr = (unsigned char *)STEP_bPHAB;
				k_PrinterBusy = 3;
			}
			else
			{
				k_StepPHABptr = (unsigned char *)STEP_PHAB;
				k_PrinterBusy = 4;
				k_StepCnt     = 0;
				//enable_motor_header();
			}
			ps_AdjustStep();
			k_AutoStepCnt = 1200;                  //AutoSteps ???
		}

		if(++k_StepPHABptr == k_StepPHABEnd) k_StepPHABptr -= 4;
		k_PortPrnCtrl  = ((k_PortPrnCtrl & 0xf9) | (*k_StepPHABptr & 0x06));     //  yaocy 20061206
		enable_prn_power();
		enable_motor_paper();
		ps_PrnControl(PRNCTRL,k_PortPrnCtrl);
		///////////
		//STEP_450PPS
		s_PaperMStepTimerStart(2220);
		return;
	}

	k_PortPrnCtrl = ((k_PortPrnCtrl & 0xf9) | (*k_StepPHABptr & 0x06));
	//disable step
	disable_motor_paper();
	disable_prn_power();
	ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
	s_PaperMStepTimerStop();
	k_Timer2User = 2;  //0: no user   1: for cpu card 2: for step in printing  3: for paper feed
	k_PrinterBusy   = 0;
	k_FeedKeyStatus = 0;//finished auto feed,clear feed key status.
}

void s_ManualFeedPaper_stylus(char FeedDir, char StepOnOff)
{
	//printk("\n\rFeedDir:%d StepOnOff:%d\n\r",FeedDir,StepOnOff);
	if(k_Timer2User == 1)  return;  //icc busy
	if(k_Timer3User == 1)  return;  //icc busy
	if(k_PrinterBusy == 5) return;  //printing

	k_FeedAutoCnt = 0;
	k_FeedBackCnt = 0;
	k_FeedForCnt  = 0;

	if(StepOnOff == 0)
	{
		k_PortPrnCtrl = ((k_PortPrnCtrl & 0xf9) | (*k_StepPHABptr & 0x06));     //disable step
		k_PortPrnCtrl |= (1<<3);       
		k_PortPrnCtrl |= (1<<5);        
		ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
		s_PaperMStepTimerStop();
		k_Timer2User  = 2;
		k_PrinterBusy = 0;
		return;
	}

	s_PaperMStepTimerStop();
	StopLockEven();

	k_PortPrnCtrl &= 0xdf;    //power on
	k_PortPrnCtrl &= ~(1<<3);  //step on   
	ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
	k_Timer2User = 3;
	if(FeedDir == 1) 
	{
		k_StepPHABptr = (unsigned char *)STEP_PHAB;
		k_FeedKeyStatus = 1;//slip fw 
	}
	else 
	{
		k_StepPHABptr = (unsigned char *)STEP_bPHAB;
		k_FeedKeyStatus = 2;//slip bw
	}
	ps_AdjustStep();
	k_PrinterBusy = 1;
	k_StepTimer   = 0;

	s_PaperMStepTimerStart(1000); //1ms
}

void s_AutoFeedPaper(void)
{
	if(k_Timer2User == 1)   return;
	if(k_Timer3User == 1)   return;
	if(k_PrinterBusy == 5)  return;
	k_FeedAutoCnt = 0;
	k_FeedBackCnt = 0;
	k_FeedForCnt  = 0;

	if(k_PrinterBusy > 1) return;
	s_PaperMStepTimerStop();
	StopLockEven();

	k_PortPrnCtrl &= 0xdf;    //power on
	k_PortPrnCtrl &= ~(1<<3);       //  yaocy 20061207
	ps_PrnControl(PRNCTRL, k_PortPrnCtrl);

	k_Timer2User = 3;

	// added by yangrb 2008-01-31 添加k_FeedKeyStatus设置
	k_FeedKeyStatus = 3;
	k_PrinterBusy = 2;
	k_StepTimer   = 0;

	s_PaperMStepTimerStart(10000); //10ms
}

char get_prn_hysteresis(void)
{
	int ret;
	char hysteresis[6];

    ret = SysParaRead(SET_PRN_HYSTERE_PARA, hysteresis);
	if(ret < 0) return 0;
	if(abs(hysteresis[0]) > MAX_HYSTERESIS) return 0;

    return hysteresis[0];
}

unsigned char set_prn_hysteresis(char hysteresis)
{
    char buff[2];
    if(abs(hysteresis)>MAX_HYSTERESIS)return 0;
    buff[0] = hysteresis;
    buff[1] = 0x00;
    if(SysParaWrite(SET_PRN_HYSTERE_PARA, buff) == 0)
	{        
        hysteresis_err = hysteresis;
		return 1;
	}
	return 0;
}

char get_prn_max_hysteresis(void)
{
    return MAX_HYSTERESIS;
}

void static RePrintLine(void)
{
	if(k_SetupParam)
	{
		k_LeftNextPtr=k_LeftNextPtrBak;
		k_LeftNextPtr9=k_LeftNextPtr9Bak;
		k_RightNextPtr=k_RightNextPtrBak;
		k_RightNextPtr9=k_RightNextPtr9Bak;
	}
	k_PrintData=0;
	k_SetupParam=0;
}

static void ps_PrnEncoder(void)
{
	short  offset;
	uchar   ch;
	uint i = 0;

	k_TimerStopCnt = 20;

	DISABLE_PRNOUT_PWM();

	if(s_IsGuardTimerOver())
	{
		home_mis=1;
		RePrintLine();
	}
	s_SetGuardTimer(3000);
	if(s_CheckHome() == last_home_flag)
	{
		if(last_home_flag)	
			home_cnt[cnt_idx]++;
		else	
			home_cnt[cnt_idx]--;
	}	
	else 
	{
		last_home_flag = s_CheckHome();
		if(k_PrnSmallFontFlag)
		{
			if((last_home_flag)&&(home_cnt[cnt_idx] < -60*2))
			{
				offset=(900*2-k_Encoder)/2;
				if(offset < 0) offset =-offset;
				//Print("\r\nhomemis=%d %d",home_mis,offset);
				if((offset > MAX_ERR_LOST_CNT)||(home_mis)) k_homeValid=1;
			}
		}
		else if((last_home_flag)&&(home_cnt[cnt_idx] < -60))
		{
			offset=900-k_Encoder;
			if(offset < 0) offset =-offset;
			//Print("\r\nhomemis=%d %d",home_mis,offset);
			if((offset > MAX_ERR_LOST_CNT)||(home_mis)) k_homeValid=1;
		}
		//Print(" %d-%d ",home_cnt[cnt_idx],k_Encoder);
		cnt_idx=(cnt_idx+1)&3;
		home_cnt[cnt_idx]=0;
	}
    
	if(home_mis)
	{
		if(!k_homeValid) 
		{
			return ;
		}
		home_mis=0;
	}

    if(k_PrnSmallFontFlag)
    {
    	if(k_Encoder == 1800 || k_homeValid==1 )
    	{
    		k_Encoder   = 0;
    		k_PrintFlag = 0;
			k_homeValid = 0;
			k_homeFlag = 0;
			k_homeCount = 0;
            encode_print_cnt=0;
            RePrintLine();
    	}
        if(encode_print_cnt--==0)
        {
        	k_PrintFlag = 1;
            encode_print_cnt=2;
        }
        else
            k_PrintFlag = 0;
	}
	else
	{
        if(k_Encoder == 900 || k_homeValid==1 )
        {
            k_Encoder   = 0;
            k_PrintFlag = 0;
			k_homeValid = 0;
			k_homeFlag = 0;
			k_homeCount = 0;
            RePrintLine();
        }
        k_PrintFlag ^= 1;
	}
    
	if(k_PrinterBusy != 5) return;
	k_Encoder++;
	//printk("\n\rk_Encoder:%d\n\r",k_Encoder);

    if(!k_PrnStepOK) 
	{
		if ( k_NoPaperCnt > 5 )
		{
			//added by tanjue 20110704解决打印机步进缺纸继续走的BUG
			k_PrnResult = PRN_NOPAPER;
			ps_PrintOver();
			s_PaperMStepTimerStop();
			StopLockEven();
			//added by tanjue 20110704解决打印机步进缺纸继续走的BUG end
		}
		return;
	}                         //not step over

    if(!k_PrintData && !k_SetupParam)                //not print
	{
		if(k_LineStepPtr == k_LineStepEnd)           //print complete
		{
			k_PrnResult = PRN_OK;
			ps_PrintOver();
			if(k_NoPaperCnt < 1)
			{
				s_PaperMStepTimerStop();
				StopLockEven();
				if(have_paper())
				{
					enable_prn_power();
					ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
					LockPaper_SEC(20);
				}
			}
			return;
		}

		if(k_NoPaperCnt > 26)
		{
			k_PrnResult = PRN_NOPAPER;
			ps_PrintOver();
			StopLockEven();//added by tanjue 20110628解决打印机连续打印缺纸字车不会停的BUG
			return;
		}

		if ((k_Encoder> (unsigned short)((signed short)RIGHT_ENCODER
            - hysteresis_err)) || (k_Encoder==1))
		{
			if(k_RightPrint==2) return;
            
			k_LeftNextPtrBak=k_LeftNextPtr;
			k_LeftNextPtr9Bak=k_LeftNextPtr9;
			k_RightNextPtrBak=k_RightNextPtr;
			k_RightNextPtr9Bak=k_RightNextPtr9;

			if(*k_DoubleFlagPtr) k_RightPrint = 1;
			else k_RightPrint=0;

            k_CurDotPtr       = k_LeftNextPtr;
			k_CurDotPtr9      = k_LeftNextPtr9;

			k_LeftNextPtr9  = k_CurDotPtr9+k_LineWidth;
			k_RightNextPtr9 = k_CurDotPtr9+k_LineWidth+LINE_WITH_DOT-1;

			k_LeftNextPtr  = k_CurDotPtr+ k_LineWidth;
			k_RightNextPtr = k_CurDotPtr+ k_LineWidth+LINE_WITH_DOT - 1;
		}
		else if(*k_DoubleFlagPtr) return;
        else
		{
            k_RightPrint=0;

			k_LeftNextPtrBak=k_LeftNextPtr;
			k_LeftNextPtr9Bak=k_LeftNextPtr9;
			k_RightNextPtrBak=k_RightNextPtr;
			k_RightNextPtr9Bak=k_RightNextPtr9;

			k_CurDotPtr  = k_RightNextPtr;
			k_CurDotPtr9 = k_RightNextPtr9;

			k_LeftNextPtr9  = k_CurDotPtr9+LINE_WITH_DOT+1;
			k_RightNextPtr9 = k_CurDotPtr9+k_LineWidth;
			k_LeftNextPtr  = k_CurDotPtr+LINE_WITH_DOT+1;
			k_RightNextPtr = k_CurDotPtr+k_LineWidth;
		}
		k_SetupParam = 1;
	}
    
	//printk("\n\rk_RightStopT:%d\n\r",k_RightStopT);
	if(k_Encoder <= k_RightStopT)
	{
		if(!k_Prn9Pin && !k_PrintFlag) return;
		if(k_Encoder == 1) k_PrintData = 1;
		if(!k_PrintData) return;

		ch = *k_CurDotPtr++;
		//printk("\n\rch++:%x\n\r",ch);
		ps_PrnControl(PRNDATA, ch);

		if(*k_CurDotPtr9++ == 0xff) k_PortPrnCtrl |= 0x01;
		else						k_PortPrnCtrl &= 0xfe;

		ENABLE_PRNOUT_PWM();

		if(k_Encoder >= k_RightStopT)
		{
			ps_PrnStep(*k_LineStepPtr++);
			k_SetupParam = 0;
			k_PrintData  = 0;
			k_DoubleFlagPtr++;
		}
		return;
	}
/*
	if(hysteresis_err & 1)
	{
		if((!k_Prn9Pin) && k_PrintFlag &&!k_PrnSmallFontFlag) 
            return;
	}
	*/
	if(k_Encoder == (unsigned short)(RIGHT_ENCODER - hysteresis_err) )
        k_PrintData = 1;    
	else if(!k_Prn9Pin && (!k_PrintFlag)) return;

	if(!k_PrintData) return;

	ch = *k_CurDotPtr--;
	//printk("\n\rch--:%x\n\r",ch);
	ps_PrnControl(PRNDATA, ch);

	if(*k_CurDotPtr9-- == 0xff) k_PortPrnCtrl |= 0x01;
	else						k_PortPrnCtrl &= 0xfe;
	//printk("\n\rk_PortPrnCtrl:%d\n\r",k_PortPrnCtrl);

	ENABLE_PRNOUT_PWM();

	if(k_Encoder >= (ushort)(k_LeftStopT - hysteresis_err))
	{
		ps_PrnStep(*k_LineStepPtr++);
		k_SetupParam = 0;
		k_PrintData  = 0;
		k_DoubleFlagPtr++;
	}
}

void NewLine(unsigned short PrintFlag)
{
	unsigned short lines;
    k_PrnLF = 1;
	if(k_CurCol != k_LeftIndent)
	{
		if(k_CurLineHigh >= 9)
		{
			if (k_DotPtr + k_LineWidth * 2 < k_LineDot + 150000)
				k_DotPtr += k_LineWidth*2;
			else return;

			if(k_CurLineHigh >= 9)
			{
				*k_LineStepEnd++   = 8;
				k_AllLines        += 8;
				*k_LineStepEnd     = 0;
				lines              = k_LineSpace+k_CurLineHigh-8;
				*k_DoubleFlagPtr++ = 1;
				*k_DoubleFlagPtr++ = 1;
			}
			else
			{
				lines = k_LineSpace+k_CurLineHigh-8;    //for 德丰
				*k_DoubleFlagPtr++ = 0;
			}
		}
		else if (k_DotPtr + k_LineWidth >= k_LineDot + 150000) return;
        else
		{
			k_DotPtr          += k_LineWidth;
			lines              = k_LineSpace+k_CurLineHigh;
			*k_DoubleFlagPtr++ = 0;
		}

		if (k_DotPtr + k_LineWidth * 2 >= k_LineDot + 150000) return;
		memset(k_DotPtr, 0xff, k_LineWidth*2);
		k_CurCol = k_LeftIndent;
	}
	else if(k_AsciiMode)lines = k_LineSpace+16;		
	else lines = k_LineSpace+8;

	*k_LineStepEnd += lines;
	k_AllLines     += lines;
	//addbywxk 2008.10.29 每次换行换页后,打印行为默认点数8个
	k_CurLineHigh=LINE_DOT_DEFAULT;
	if(PrintFlag)
	{
		k_LineStepEnd++;
		*k_LineStepEnd = 0;
	}
}

static void ps_ReverseData(unsigned char *dec,unsigned char *src,unsigned short len)
{
	int i;
	for(i=0; i<len; i++) dec[i] = ~src[i];
}

unsigned char ps_GetStatus(void)
{
	int             i,PowerOn;
	unsigned char   TmpStatus;

	if(k_PrinterBusy == 5) return PRN_PRINTING;

	if (!(k_PortPrnCtrl & 0x20))
	{
		PowerOn = 0;
		enable_prn_power();
		ps_PrnControl(PRNCTRL, k_PortPrnCtrl);
	}
	else PowerOn = 1;

	for(i=0; i<3; i++)
	{
		DelayMs(50);
		if(have_paper()) break; //检测有纸
	}

	if(i >= 3) TmpStatus = PRN_NOPAPER;
	//revised by tanjue 2011-7-6 无纸时调用PrnStart,然后装纸，再调用PrnStatus()，返回0xf0； 修改和热敏机器一致，返回0x00
	else if ( k_PrnResult && (k_PrnResult != PRN_NOPAPER) )
	{
		if (k_PrnResult == 0x04)
			TmpStatus=0xf0;//prnstatus=print is not finised.
		else TmpStatus=k_PrnResult;
	}
	else TmpStatus = PRN_OK;

	if(!PowerOn)
	{
		disable_prn_power();    //k_PortPrnCtrl |= 0x20;  //power off
		ps_PrnControl(PRNCTRL,k_PortPrnCtrl);
	}
	return TmpStatus;
}

unsigned char ps_GetPrnResult(unsigned char *PrnResult)
{
	if(k_ResultFlag)
	{
		*PrnResult   = k_PrnResult;
		k_ResultFlag = 0;
		return 1;
	}
	return 0;
}

//点阵往上移保证对齐
static void ps_adjdotup(uchar *ssup,int len,uchar btno)
{
	unsigned short fgrd;
	int ii;
	unsigned long fgor;
	uchar *sslw;

	fgor=0xffff0000>>btno;
	sslw=ssup+k_LineWidth;

	for(ii=0;ii<len;ii++)
	{
		//读入
		fgrd=((*sslw)<<8)|(*ssup);
		fgrd=(fgrd>>btno);
		fgrd=fgrd|fgor;
		*sslw++=((uchar)(fgrd>>8));
		*ssup++=(uchar)fgrd;
	}
}

//点阵往下移保证对齐
static void ps_adjdotdown(uchar *ssdown,int len,uchar topno)
{
	unsigned short fgrd;
	int ii;
	unsigned long fgor;
	uchar *sslw;

	fgor=~(0xffffffff<<topno);
	sslw=ssdown+k_LineWidth;

	for(ii=0;ii<len;ii++)
	{
		//读入
		fgrd=((*sslw)<<8)|(*ssdown);
		fgrd = (fgrd<<topno);
		fgrd = fgrd | fgor;
		*sslw++=((uchar)(fgrd>>8));
		*ssdown++=(uchar)fgrd;
	}
}

void ps_ProcStr(unsigned char *PrnData,unsigned char len)
{
	unsigned short i,bCodeNum;
	unsigned char bFont,bDotLen,sDotsBuf[2048];
	int iWidth = 0;
	int iHeight = 0;
	int lsthigh;
    static int PrnDataWidth[41] = {0};  //此数组用来存放每个打印字符的宽度 数组大小为 （180/6 = ）30
		                                //一行最多可打30个字符，数组0号单元用来存放本行字符个数
	static int PrnDataHeight[41] = {0}; //此数组用来存放每个打印字符的高度 数组大小与宽度数组一致
	                                    //数组0号元素用来存放本行打印字符的最大高度
	static int PrnDataCharSpace[41] = {0};   //数组用来记录字间距，0号单元不用,目的是为了防止一行未打完改变字间距时又再次填点阵信息
	static int PrnDataAdjustFlag = 0;//用来记录点阵是否需要移动
	static int PrnDataCount = 0; //用来记录当前正在处理第几个字符  
	unsigned short PrnOldCurCol = 0;       //用来记录换行之前的列的位置
	unsigned char *PrnOldDOtPrt = 0;
	int k;   //for loop count
	uint j = 0;

	if (1 == k_PrnLF)
	{
		PrnDataCount = 0;
		memset(PrnDataWidth,0,sizeof(PrnDataWidth));
		memset(PrnDataHeight,0,sizeof(PrnDataHeight));
		PrnDataAdjustFlag = 1;
		k_PrnLF = 0;
	}	

	for(i=0; i<len; )
	{
		//bCodeNum = 1;
		if(*PrnData >= 0x80)
		{
			//if(k_tFontLib.bExtendedCode == DOUBLE_BYTES) bCodeNum = 2;
			bFont = k_CfontMode;
		}
		else bFont = k_AsciiMode;
        
		bCodeNum = s_GetSPrnFontDot(0,PrnData,sDotsBuf,&iWidth,&iHeight);
		
		if ( (k_CurCol + iWidth) > LINE_WITH_DOT ) 
		{
			PrnOldCurCol = k_CurCol;
			PrnOldDOtPrt = k_DotPtr - PrnDataCharSpace[PrnDataWidth[0]];
            
			for (k = PrnDataWidth[0]; k > 0; k--)
			{
		    	if  (PrnDataHeight[k] < PrnDataHeight[0]) 
				{
					ps_adjdotdown(PrnOldDOtPrt + PrnOldCurCol - PrnDataWidth[k],
                        PrnDataWidth[k],PrnDataHeight[0] - PrnDataHeight[k]);
				}
				PrnOldCurCol -= (PrnDataWidth[k] + PrnDataCharSpace[k-1]);
			}

            NewLine(1);
			PrnDataCount = 0;
			memset(PrnDataWidth,0,sizeof(PrnDataWidth));
			memset(PrnDataHeight,0,sizeof(PrnDataHeight));
			memset(PrnDataCharSpace,0,sizeof(PrnDataCharSpace));
			PrnDataAdjustFlag = 1;
			k_PrnLF = 0;
		}  
		    
		//打印字符个数加1
		PrnDataWidth[0]++;
	    //PrnDataHeight[0]++;
		PrnDataCount++;
		PrnDataCharSpace[PrnDataCount] = k_CharSpace;
	
		//addbywxk 2008.11.04
		lsthigh=k_CurLineHigh;
		if(iHeight>k_CurLineHigh) k_CurLineHigh=iHeight;

		if(iHeight>16)
		{
			//大于16的点阵不处理
			if ((k_DotPtr + k_CurCol + iWidth >= k_LineDot + 150000) ||
				(k_DotPtr + LINE_WITH_DOT + k_CurCol + iWidth >= k_LineDot + 150000))
				return;

			memset(k_DotPtr+k_CurCol,0xff,iWidth);
			memset(k_DotPtr+k_LineWidth+k_CurCol,0xff,iWidth);

			PrnDataWidth[PrnDataCount] = iWidth;
			if (iHeight > PrnDataHeight[0]) PrnDataHeight[0] = 16;
			PrnDataHeight[PrnDataCount] = 16;
		}
		else if (iHeight > 9)
		{
			if ((k_DotPtr + k_CurCol + iWidth >= k_LineDot + 150000) ||
				(k_DotPtr + LINE_WITH_DOT+ k_CurCol + iWidth >= k_LineDot + 150000))
				return;
			
			ps_ReverseData(k_DotPtr+k_CurCol, sDotsBuf, iWidth);
			ps_ReverseData(k_DotPtr+k_LineWidth+k_CurCol, sDotsBuf+iWidth, iWidth);
			
			PrnDataWidth[PrnDataCount] = iWidth;
			if (iHeight > PrnDataHeight[0]) PrnDataHeight[0] = iHeight;
			PrnDataHeight[PrnDataCount] = iHeight;
		}
		else if(iHeight==9)
		{
            if ((k_DotPtr + k_CurCol + iWidth >= k_LineDot + 150000) ||
				(k_DotPtr + LINE_WITH_DOT + k_CurCol + iWidth >= k_LineDot + 150000))
				return;

			ps_ReverseData(k_DotPtr+k_CurCol, sDotsBuf, iWidth);
			ps_ReverseData(k_DotPtr+k_LineWidth+k_CurCol, sDotsBuf+iWidth, iWidth);

			PrnDataWidth[PrnDataCount] = iWidth;
			if (iHeight > PrnDataHeight[0]) PrnDataHeight[0] = iHeight;
			PrnDataHeight[PrnDataCount] = iHeight;
		}
        else if ((k_DotPtr + k_CurCol + iWidth >= k_LineDot + 150000) ||
				(k_DotPtr + k_LineWidth + k_CurCol + iWidth >= k_LineDot + 150000))
		{
			return;
		}
		else
		{
            ps_ReverseData(k_DotPtr+k_CurCol, sDotsBuf, iWidth);

			PrnDataWidth[PrnDataCount] = iWidth;
			if (iHeight > PrnDataHeight[0]) PrnDataHeight[0] = iHeight;
			PrnDataHeight[PrnDataCount] = iHeight;
		}

		k_CurCol += (k_CharSpace+iWidth);
		PrnData  += bCodeNum;
		i        += bCodeNum;
		if (i >= len) PrnDataAdjustFlag = 1;
	}//end for len
	//之前全部不调整点阵，待点阵全部填写完成后再调整
	if (1 == PrnDataAdjustFlag)
	{
		PrnOldDOtPrt = k_DotPtr;
		PrnOldCurCol = k_CurCol - PrnDataCharSpace[PrnDataWidth[0]];
        for (i = PrnDataWidth[0]; i > 0; i--)
		{
		    if ( (PrnDataHeight[i] < PrnDataHeight[0]) )
			{
				ps_adjdotdown(PrnOldDOtPrt + PrnOldCurCol - PrnDataWidth[i],
                    PrnDataWidth[i], PrnDataHeight[0] - PrnDataHeight[i]);
			    PrnDataHeight[i] = PrnDataHeight[0];
			}
			PrnOldCurCol -= (PrnDataWidth[i] + PrnDataCharSpace[i-1]);	
		}
		PrnDataAdjustFlag = 0;
	}		
}

static void ps_AddStepPtr(void)
{
	k_LineStepEnd++;
	*k_LineStepEnd = 0;
}

void ps_ProcData(unsigned char *PrnData, int DataLen, int WaitFlag)
{
	int             tmp,flag;
	unsigned char   cmd, *EndDataPtr, *DotEndPtr,*TmpPtr;
	unsigned short  i,j,k/*,step*/;
	short step;
	unsigned long   a,b,TimeOutCnt;
	int revli; //退走的步数
    static int hysteresis_flag = 1; //读取hysteresis 标志
    
	while(k_PrinterBusy == 5);

	k_Prn9Pin   = 0;
	k_LineWidth = 360;
	k_LeftStopT = LEFT_ENCODER;

    if(k_PrnSmallFontFlag)
    {
        k_LineWidth = 480;
        k_LeftStopT = LEFT_ENCODER;         
    }

	k_AsciiMode  = 0;
	k_CfontMode  = 0;
	k_CharSpace  = 0;
	k_LineSpace  = 2;
	k_LeftIndent = 0;

	k_CurCol        = 0;
	k_AllLines      = 0;
	revli=140- 67-CNT_STEP_SLIP+8;

	k_LineStepEnd   = k_LineStep;
	k_DoubleFlagPtr = k_DoubleFlag;
	k_DotPtr        = k_LineDot;
	DotEndPtr       = k_DotPtr+150000;  //254 LINE(6*8 ASCII PER LINE)
	EndDataPtr      = PrnData+DataLen;
	memset(k_DotPtr, 0xff, 720);
	*k_LineStepEnd = 0;

	while(1)
	{
		if(PrnData>=EndDataPtr || k_DotPtr>DotEndPtr)
		{
			k_ResultFlag = 1;
			k_PrnResult  = PRN_DATAERROR;
			return;
		}

		cmd = *PrnData++;
		if(cmd == PRN_END)
		{
			if(k_CurCol != k_LeftIndent) NewLine(0);
			break;
		}
        
		switch(cmd)
		{
		case PRN_STR:
			i = *PrnData;
			if(i==5 || i==6)
			{
    			if(!memcmp(PrnData+1,"\x01\x01\x01\x01",4))
    			{
        			PAGE_DOTLINES = PrnData[5];
        			if(i == 6)
        			{
            			PAGE_DOTLINES <<= 8;
            			PAGE_DOTLINES  += PrnData[6];
    			    }
        			PrnData += (i+1);
        			continue;
    			}
			}
			if(k_LineStepEnd == k_LineStep) ps_AddStepPtr();
			if(*k_LineStepEnd) ps_AddStepPtr();
			i = *PrnData++;
			ps_ProcStr(PrnData, i);
			PrnData += i;
			break;
        
		case PRN_LF:
			NewLine(0);
			break;
        
		case PRN_SPACE_SET:
			k_CharSpace = *PrnData++;
			k_LineSpace = *PrnData++;
			if(k_CharSpace > 60) k_CharSpace = 60;
			if(k_LineSpace < 2) k_LineSpace  = 2;
			break;

        case PRN_FONT_SET:
			if(*PrnData < 4) k_AsciiMode = *PrnData++;
			else *PrnData++;
			if(*PrnData < 4) k_CfontMode = *PrnData++;
			else *PrnData++;
			//选择字库
			s_SetSPrnFont(k_AsciiMode,k_CfontMode);
			//printk("\r\n FontSet %d %d ",k_AsciiMode,k_CfontMode);
			break;
        
		case PRN_STEP:
			step   = *PrnData++;
			step <<= 8;
			step  += *PrnData++;
			if(k_CurCol != k_LeftIndent) NewLine(0);
			k_AllLines     += step;
			*k_LineStepEnd += step;
			if(*k_LineStepEnd==0) *++k_LineStepEnd=0;
			break;
        
		case PRN_FF:
			if(k_CurCol != k_LeftIndent) NewLine(0);
			k_AllLines     %= PAGE_DOTLINES;
			k_AllLines      = (PAGE_DOTLINES - k_AllLines);
			*k_LineStepEnd += k_AllLines;
			k_AllLines  =0;
			//addbywxk 2008.10.29 每次换行换页后,打印行为默认点数8个
			k_CurLineHigh=LINE_DOT_DEFAULT;
			break;
        
		case PRN_LEFT_INDENT:
			if(k_CurCol == k_LeftIndent) flag = 1;
			else flag = 0;
			k_LeftIndent = *PrnData++;
			if(k_LeftIndent > 240) k_LeftIndent = 240;
			if(k_LeftIndent % 2) k_LeftIndent = k_LeftIndent/2 + 1;
			else k_LeftIndent=k_LeftIndent/2;
			if(flag) k_CurCol = k_LeftIndent;
			break;
        
		case PRN_LOGO:
			if(k_LineStepEnd == k_LineStep) ps_AddStepPtr();
			if(k_CurCol != k_LeftIndent) NewLine(1);
			if(*k_LineStepEnd) ps_AddStepPtr();
			j        = *PrnData;
			//printk("\n\rLogoj:%d\n\r",j); 
			//printk("\n\rk_LineWidth:%d",k_LineWidth);
			PrnData += 2;
			for(i=0; i<j; i++)
			{
				k = (*(PrnData - 1) << 8) + (*PrnData);
			    PrnData++;
				//printk("\n\rk:%d\n\r",k);
				TmpPtr   = PrnData;
				PrnData += (k+1);
				if (k_DotPtr + k >= DotEndPtr) break;
				if (k_DotPtr + k_LineWidth >= DotEndPtr) break;
				memset(k_DotPtr, 0xff, k_LineWidth);
				//revised by tanjue 2011-6-28 解决不能打印宽度大于240的图像问题，超过180被截断 k_LineWidth -> k_LineWidth/2
				for(; k_CurCol < (k_LineWidth/2) && k>0; k_CurCol++,k--)
				{
					k_DotPtr[k_CurCol] = ~(*TmpPtr);
					TmpPtr++;
				}
				*k_LineStepEnd++   = 8;
				*k_DoubleFlagPtr++ = 2;
				k_AllLines        += 8;
				k_DotPtr += k_LineWidth;
				k_CurCol       = k_LeftIndent;
			}
            
			if(j)
			{
				k_LineStepEnd--;
				PrnData--;
			}
            
			if (k_DotPtr + k_LineWidth * 2 < DotEndPtr)
			{
				memset(k_DotPtr, 0xff, k_LineWidth*2);
			}
			break;
        
		case PRN_FONT_SEL:
			{
				ST_FONT sp,mp;
				memcpy(&sp,PrnData,sizeof(ST_FONT));
				memcpy(&mp,PrnData+sizeof(ST_FONT),sizeof(ST_FONT));
				SelSPrnFont(&sp,&mp);
				PrnData+=2*(sizeof(ST_FONT));
			}
			break;

		default:

			k_ResultFlag = 1;
			//printk("\r\n%d YJUTG",cmd);
			k_PrnResult  = PRN_DATAERROR;

			return;
		}
	}

	//printk("\n\rk_CurCol:%d\n\r",k_CurCol);
	//printk("\n\rk_LeftIndent:%d\n\r",k_LeftIndent);
	if(k_CurCol != k_LeftIndent) NewLine(1);
	*k_LineStepEnd += -revli;
	if(*k_LineStepEnd != 0) k_LineStepEnd++;

	//printk("\n\rk_PrinterBusy:%d\n\r",k_PrinterBusy);
	while(k_PrinterBusy);
	DISABLE_ENCODE_INT();
	enable_prn_power();
	ps_PrnControl(PRNCTRL,k_PortPrnCtrl);
	k_PrinterBusy = 5;
	k_SetupParam  = 0;
	k_PrintData   = 0;

    if(k_PrnSmallFontFlag)
    {
        k_RightStopT = 718;//240*3-2
    }
	else k_RightStopT = 359;

	k_DoubleFlagPtr = k_DoubleFlag;
	k_LineStepPtr   = k_LineStep+1;

	k_LeftNextPtr   = k_LineDot;
	k_RightNextPtr  = k_LineDot+LINE_WITH_DOT-1;
	k_LeftNextPtr9  = k_LineDot+LINE_WITH_DOT;
	k_RightNextPtr9 = k_LineDot+k_LineWidth-1;
    if (hysteresis_flag)
    {
        hysteresis_err = get_prn_hysteresis();
        hysteresis_flag = 0;
    }
	// 检测是否缺纸
	for(i=0; i<3; i++)
	{
		// high paper on
		DelayMs(20);
		if(have_paper()) break;	//have paper
		DelayMs(30);
	}

	// 连续三次判断到缺纸,则认为缺纸
	if(i >= 3) 
	{       
		//printk("NO paper\r\n");
		k_PrnResult=PRN_NOPAPER;
		ps_PrintOver();
		StopLockEven();//added by tanjue 20110628解决打印机连续打印缺纸字车不会停的BUG
		return;
	}

	k_NoPaperCnt = 0;
	k_Encoder    = 0;
	k_homeValid = 0;
	k_homeFlag = 0;
	k_homeCount = 0;
	k_PrintFlag  = 0;
	k_PrintFlag  = 0;
	k_RightPrint = 0;
	StopLockEven();
	StopCntEven();
	
	enable_prn_power();
	k_PortPrnCtrl &= 0xef;  //MOTOR = on
	ps_PrnControl(PRNCTRL, k_PortPrnCtrl);

	//printk("\n\rMotor On\n\r");
	k_Timer2User = 2;

	//printk("\n\rPrnStep=%d\n\r", k_LineStep[0]-67+140-CNT_STEP_SLIP);
	ps_PrnStep(k_LineStep[0]-67+140-CNT_STEP_SLIP);	

	while(!k_PrnStepOK)
	{
		if (!have_paper())
		{
			k_PrnResult = PRN_NOPAPER;
			ps_PrintOver();
			StopLockEven();
			s_PaperMStepTimerStop();
			return;
		}
	}
	//printk("\n\rPrnStep8\n\r");
	ps_PrnStep(8);	

	while(!k_PrnStepOK)
	{
		if (!have_paper())
		{
			k_PrnResult = PRN_NOPAPER;
			ps_PrintOver();
			StopLockEven();
			s_PaperMStepTimerStop();
			return;
		}
	}

	TimeOutCnt = 0;
    StopLockEven();

    //开启中断
	//printk("\n\rTimer Interrupt On\n\r");
	s_SetGuardTimer(1000);
    encode_print_cnt = 0;
    home_mis = 1;
    last_home_flag = s_CheckHome();
    cnt_idx = 0;
    home_cnt[cnt_idx]=0;
    
	ENABLE_ENCODE_INT();
	StartCntEven();
    StopLockEven();

    k_Timer3User = 2;
	
	while(WaitFlag && k_PrinterBusy);
	//ScrSetIcon(ICON_PRINTER, CLOSEICON);
}

void s_PaperMStepTimer_Int(void)
{
	//int k_Timer2User;
	//0: no user   1: for cpu card 2: for step in printing  3: for paper feed
	CLEAR_MSTEP_TIMER_INT();
	if(k_Timer2User==1)       /*s_IccTimer2Int()*/;
	else if(k_Timer2User==2)  //step in printing
		ps_PrnIntStep();
	else if(k_Timer2User==3)  //paper feed
		ps_FeedDrvInt();
}

// added by yangrb 2008-01-28
void s_PrnEncoderInt(void)
{
    static int fcking = 0;
	//printk("Eint0\n\r");

    CLEAR_ENCODE_INT();
    
    if(k_PrnSmallFontFlag == 1)
    {
    	DISABLE_ENCODE_INT();
    	ps_PrnEncoder();
    	s_CfgPrnEncode(fcking);
    	fcking ^= 1;
    	//DelayUs(600);
    	ENABLE_ENCODE_INT();
	}
	else 
	{
	    ps_PrnEncoder();
	}
}

void s_PrnSmallFontConfig(int value)
{
    if(value != 0 && value != 1) return ;
    k_PrnSmallFontFlag = value;
}

void ps_PrnStop_stylus(void)
{
    ps_PrintOver();
	s_PaperMStepTimerStop();
	s_GuardTimerStop();
}

