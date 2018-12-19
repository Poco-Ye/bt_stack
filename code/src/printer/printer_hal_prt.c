#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "base.h"
#include "posapi.h"
#include "printer.h"

static void s_MotorTimer(void);
static void s_HeatTimer(void);

typedef struct{
	int port;		//port
	int pin;		//pin number or mask
	int value;		//for control, enable value
}T_PinCtl;

typedef struct{
	T_PinCtl lat;
	T_PinCtl stb;
	T_PinCtl power;
	T_PinCtl motor;		//multiple pin, 

	void (*cal_block_time)(void);

	int mask;		//for control mask, see below mask value
	int tempForbiden;
	int tempStart;
	int maxVoltage;
}T_PrnDrvCfg;

//mask value
#define PRN_MASK_MOTOR_DIR_REVERT		1				//if set  the motor will counter rotation
#define PRN_MASK_REST_ADJUST			(1<<1)			//for PT486F
#define PRN_MASK_DOT_ADJUST				(1<<2)			//according the AUTO_ADJUST macro
#define PRN_MASK_BLACK_ADJUST			(1<<3)			//according the BLACK_ADJUST macro
#define PRN_MASK_TEMP_ADC_REVERT		(1<<4)			//for PT486F

#define PRN_MASK_HEAT_TWINCE			(1<<5)

T_PrnDrvCfg SxxxPrnCfg;
T_PrnDrvCfg S500PrnCfg;
T_PrnDrvCfg D210PrnCfg;


T_PrnDrvCfg *ptPrnDrvCfg = NULL;
#define enable_pin(a)	gpio_set_pin_val(a.port, a.pin, a.value)
#define disable_pin(a)	gpio_set_pin_val(a.port, a.pin, !(a.value))
#define set_pin_output(a)	gpio_set_pin_type(a.port, a.pin, GPIO_OUTPUT);

/*

1.硬件移植

*/
/* 
	硬件接线图
	PWR		-->GPE3

	MTA 	-->  GPE4
	MTNA	-->  GPE5
	MTB		-->	GPE6
	MTNB	-->  GPE7
	STB		-->	GPD1
	LAT		-->  GPE9
	PAPER	<--  PGB6
	
	MOSI3	--- 		GPC28	FUNC1
	SPICLK3 	---		GPC26	FUNC1
*/

//SPI数据锁存控制
#define CFG_PRN_LAT()		set_pin_output(ptPrnDrvCfg->lat)
#define ENABLE_PRN_LAT()	enable_pin(ptPrnDrvCfg->lat)
#define DISABLE_PRN_LAT()	disable_pin(ptPrnDrvCfg->lat)

//电源配置
#define CFG_PRN_PWR()		set_pin_output(ptPrnDrvCfg->power)
#define ENABLE_PRN_PWR()	enable_pin(ptPrnDrvCfg->power)
#define DISABLE_PRN_PWR()	disable_pin(ptPrnDrvCfg->power)


#define CFG_MOTOR()		gpio_set_mpin_type(ptPrnDrvCfg->motor.port, ptPrnDrvCfg->motor.pin, GPIO_OUTPUT)
				

//低功耗口状态配置
#define SET_MOTOR_IDLE()	gpio_set_mpin_val(ptPrnDrvCfg->motor.port, ptPrnDrvCfg->motor.pin,0);

//加热控制
#define CFG_PRN_STB()	set_pin_output(ptPrnDrvCfg->stb)
#define ENABLE_STB()    enable_pin(ptPrnDrvCfg->stb)
#define DISABLE_STB()   disable_pin(ptPrnDrvCfg->stb)

/*SPI HAL*/
#define CFG_PRN_SPI() \
		gpio_set_pin_type(GPIOC,28,GPIO_FUNC1); \
		gpio_set_pin_type(GPIOC,26,GPIO_FUNC1);	\
        spi_config(3,5000000,8,0)

#define SEND_AND_LATCH_DATA(buf,len) \
    DISABLE_PRN_LAT();  \
    spi_tx(3,buf,len,8); \
    while(spi_txcheck(3)); \
    ENABLE_PRN_LAT(); \
    DelayUs(1); \
    DISABLE_PRN_LAT()

//#define DEBUG_PRINTER	
#ifdef DEBUG_PRINTER
//#define DEBUG_MOTOR
static uint mtrecbuf[12000],mtreclen=0;
void print_motor_time_rec(void)
{
	int ii,jj, mm;
	int mask, offset;
	
	printk("\r\n motor rec %d",mtreclen);
	mask = 0x80;
	for(offset=0; offset<8; offset++)
	{
		printk("\r\n----- mask %02X --------\r\n%4d:	", mask, 0);
		for(ii=jj=mm=0;ii<mtreclen;ii++)
		{
			if((mtrecbuf[ii]&(mask<<24)&0xff000000 ))
			{
				printk("%5d ",(mtrecbuf[ii]&0xffffff));
				jj++;
				mm++;
				if(jj==9) 
				{
					printk("\r\n%4d:	", mm);
					jj=0;
				}
			}
		}
		printk("\r\n----- mask %02X<%d> --------\r\n", mask, mm);
		mask >>= 1;
	}

	memset(mtrecbuf, 0, sizeof(mtrecbuf));
	mtreclen = 0;
	
}
void add_motor_time_rec(uint rect)
{
	if(mtreclen<sizeof(mtrecbuf)/sizeof(uint))
	{
		mtrecbuf[mtreclen++]=rect;
	}
}
void init_motor_time_rec(void)
{
	memset(mtrecbuf, 0, sizeof(mtrecbuf));
	mtreclen = 0;
}

	
#define debug_time_get() GetUsClock()
	
static unsigned int s_GetTimeDuration(unsigned int last)
{
	unsigned int now;
	
	now = debug_time_get(); 
	if(now>=last)
	{
		return (now-last);
	}
	else//微秒计数溢出
	{
		return (0xffffffff-last + now);
	}
}


static unsigned int s_GetTimeDurationAndSet(unsigned int *p)
{
	unsigned int now;
	unsigned last = *p;
	
	now = debug_time_get(); 
	*p = now;
	if(now>=last)
	{
		return (now-last);
	}
	else//微秒计数溢出
	{
		return (0xffffffff-last + now);
	}
}

#else
#define print_motor_time_rec()
#define add_motor_time_rec(a) 
#define init_motor_time_rec() 
#define debug_time_get() 	0
#define s_GetTimeDuration(t) 0
#endif

/*Timer HAL*/
static	struct platform_timer *timer_motor,*timer_heat;
static  struct platform_timer_param  param_motor,param_heat;
	
static void s_MotorTimerInit(void)
{
	param_motor.handler=s_MotorTimer;
	param_motor.id		=TIMER_PRN_0;
	param_motor.timeout=10000;
	param_motor.mode =TIMER_ONCE;
	param_motor.interrupt_prior =INTC_PRIO_5;
}

static void s_MotorTimerStart(uint TimeUs)
{
	param_motor.timeout=TimeUs;
	timer_motor = platform_get_timer(&param_motor);
	platform_start_timer(timer_motor);
}

static void s_MotorTimerStop(void)
{
	platform_timer_IntClear(timer_motor);
}

static void s_HeatTimerInit(void)
{
	param_heat.handler=s_HeatTimer;
	param_heat.id		=TIMER_PRN_1;
	param_heat.timeout=10000;
	param_heat.mode =TIMER_ONCE;
	param_heat.interrupt_prior =INTC_PRIO_3;
}

static void s_HeatTimerStart(uint TimeUs)
{
	param_heat.timeout=TimeUs;
	timer_heat = platform_get_timer(&param_heat);
	platform_start_timer(timer_heat);
}

static void s_HeatTimerStop(void)
{
	platform_timer_IntClear(timer_heat);
}

// 温度和ADC结构体
typedef struct
{
    short Temperature;
    short ADValue;
}ADConvT;

static const ADConvT     k_ADConvTListPRT48AF[121]={
{-20, 993},{-19, 991},{-18, 989},{-17, 987},{-16, 984},{-15, 982},{-14, 980},{-13, 977},{-12, 974},{-11, 972},
{-10, 969},{-9, 966},{-8, 962},{-7, 959},{-6, 956},{-5, 952},{-4, 948},{-3, 944},{-2, 940},{-1, 936},
{ 0, 932},{ 1, 927},{ 2, 923},{ 3, 918},{ 4, 913},{ 5, 907},{ 6, 902},{ 7, 897},{ 8, 891},{ 9, 885},
{10, 879},{11, 873},{12, 866},{13, 860},{14, 853},{15, 846},{16, 839},{17, 832},{18, 824},{19, 817},
{20, 809},{21, 801},{22, 793},{23, 785},{24, 776},{25, 768},{26, 759},{27, 751},{28, 742},{29, 733},
{30, 724},{31, 715},{32, 705},{33, 696},{34, 686},{35, 677},{36, 667},{37, 658},{38, 648},{39, 638},
{40, 629},{41, 619},{42, 609},{43, 599},{44, 589},{45, 579},{46, 570},{47, 560},{48, 550},{49, 540},
{50, 531},{51, 521},{52, 511},{53, 502},{54, 492},{55, 483},{56, 474},{57, 464},{58, 455},{59, 446},
{60, 437},{61, 428},{62, 419},{63, 411},{64, 402},{65, 394},{66, 386},{67, 377},{68, 369},{69, 361},
{70, 353},{71, 346},{72, 338},{73, 331},{74, 323},{75, 316},{76, 309},{77, 302},{78, 295},{79, 289},
{80, 282},{81, 276},{82, 269},{83, 263},{84, 257},{85, 251},{86, 246},{87, 240},{88, 234},{89, 229},
{90, 224},{91, 218},{92, 213},{93, 208},{94, 204},{95, 199},{96, 194},{97, 190},{98, 185},{99, 181},
{100, 177},
};

static const unsigned short  k_PreStepTimePRT48AF[] =
{
    5000, 4875, 3013, 2327, 1953, 1712, 1541, 1412,
	1310, 1227, 1158, 1099, 1048, 1004,  964,  929,
	898,  869,	843,  819,	798,  777,	758,  741,
	725,  709,	694
};

const ADConvT     k_ADConvTListPRT486F[61]=
{
{-20, 985},{-18, 980},{-16, 975},{-14, 970},{-12, 965},
{-10, 959},{-8, 952},{-6, 945},{-4, 937},{-2, 929},
{ 0, 921},{ 2, 912},{ 4, 902},{ 6, 892},{ 8, 881},
{10, 870},{12, 858},{14, 845},{16, 833},{18, 819},
{20, 805},{22, 791},{24, 776},{26, 760},{28, 745},
{30, 728},{32, 712},{34, 695},{36, 678},{38, 661},
{40, 644},{42, 627},{44, 609},{46, 592},{48, 574},
{50, 557},{52, 540},{54, 523},{56, 506},{58, 489},
{60, 473},{62, 457},{64, 441},{66, 425},{68, 410},
{70, 395},{72, 381},{74, 367},{76, 353},{78, 340},
{80, 327},{82, 314},{84, 302},{86, 291},{88, 279},
{90, 269},{92, 258},{94, 248},{96, 238},{98, 229},
{100, 220},
};

const unsigned short      k_FastPreStepTimePRT486F[] =
{	
  3029,3029,2527,2184,1934,1744,1594,	  
  1472,1371,1286,1213,1150,1094, 1045, 1001,	  
   976, 953, 931, 911, 893, 875, 859, 843,	  
   828, 814, 801, 789, 777, 766, 755, 744,	  
   734, 725, 716, 707, 699, 691
};


 static const ADConvT     k_ADConvTListPrtPT48d[121]={
{-20, 993},{-19, 991},{-18, 989},{-17, 987},{-16, 984},{-15, 982},{-14, 980},{-13, 977},{-12, 974},{-11, 972},
{-10, 969},{-9, 966},{-8, 962},{-7, 959},{-6, 956},{-5, 952},{-4, 948},{-3, 944},{-2, 940},{-1, 936},
{ 0, 932},{ 1, 927},{ 2, 923},{ 3, 918},{ 4, 913},{ 5, 907},{ 6, 902},{ 7, 897},{ 8, 891},{ 9, 885},
{10, 879},{11, 873},{12, 866},{13, 860},{14, 853},{15, 846},{16, 839},{17, 832},{18, 824},{19, 817},
{20, 809},{21, 801},{22, 793},{23, 785},{24, 776},{25, 768},{26, 759},{27, 751},{28, 742},{29, 733},
{30, 724},{31, 715},{32, 705},{33, 696},{34, 686},{35, 677},{36, 667},{37, 658},{38, 648},{39, 638},
{40, 629},{41, 619},{42, 609},{43, 599},{44, 589},{45, 579},{46, 570},{47, 560},{48, 550},{49, 540},
{50, 531},{51, 521},{52, 511},{53, 502},{54, 492},{55, 483},{56, 474},{57, 464},{58, 455},{59, 446},
{60, 437},{61, 428},{62, 419},{63, 411},{64, 402},{65, 394},{66, 386},{67, 377},{68, 369},{69, 361},
{70, 353},{71, 346},{72, 338},{73, 331},{74, 323},{75, 316},{76, 309},{77, 302},{78, 295},{79, 289},
{80, 282},{81, 276},{82, 269},{83, 263},{84, 257},{85, 251},{86, 246},{87, 240},{88, 234},{89, 229},
{90, 224},{91, 218},{92, 213},{93, 208},{94, 204},{95, 199},{96, 194},{97, 190},{98, 185},{99, 181},
{100, 177},
};
 
 static const unsigned short k_PreStepTimePrtPT48d[] = {
	 2890, 1786, 1381, 1157, 1014, 914, 838, 777, 
	 728, 687, 651, 621, 600, 572, 552, 533, 516, 
	 500
 };


/*2.打印机配置*/
// 允许步进细分
#define ENABLE_MOTOR_SUBDIVISION    

//为保证电机时序需要的调整,单位为us,这个需要精确调试
#define ADJ_MOTOR_PRE 18
#define ADJ_MOTOR_ST1 25
#define ADJ_MOTOR_ST2 25

//加热头中断耗时 它包括SPI通许耗时,这个时间建议测试出来
#define HEAD_INT_TIME 105

//缓加速模式
#define MODE_SLOW_START 1
//缓停止模式
#define MODE_SLOW_STOP	2

//预走纸步数
#define PRESTEP_NUM 	26

//允许连续走纸步数
#define MAX_FEEDPAPER_STEP     (MAX_DOT_LINE*2 + 1000)
	
static unsigned short HEAT_TIME=900;//加热基本时间
static ushort MAX_LINE_DOT;
static ushort BLACK_HEADER_DOT;//黑块每次最多加热点数
static ushort SEL_HEADER_DOT;//每次最多加热点数
static ushort TEMPERATURE_ALLOW;//允许启动打印的最高温度值

//打印中允许的最高温度值
#define TEMPERATURE_FORBID   85     

//最大加热点精确到位
//#define BIT_SEL
//打印过程中根据温度调整加热时间
#define TEMPERATURE_ADJ 

//自动调节最多加热点
//#define AUTO_ADJ_DOT 
//使用自动调节加热点时候,当上一行点超过这个数值时候,需要降低加热点
//#define MAX_LINE_DOT (384-192)
//打印黑块时自动调整
//#define BLACK_ADJ
//连续4行黑块
#define MAX_BLACK_LINE 4

/*

3.控制变量

*/
static unsigned char  k_AdjTime,k_stepmode;
//步进加速指针
static ushort *k_PreStepTime;
//最大步进表元素的个数
static ushort k_maxstep;

static ADConvT *k_ADConvTList;
static ushort k_ADConvNum;
static ushort k_StepTime;         // 定义最小走纸标准时间

//For motor control
static volatile int k_FeedCounter;
static volatile int k_MotorStep,k_StepIntCnt;

//For printer status
static volatile uchar k_Heating;          // 正在加热状态
static volatile uchar k_PaperLack;        // 缺纸标志

// 一行最多分次加热块数
#define MAX_HEAT_BLOCK      (384/16)   

/************   打印缓冲结构             **************************/
typedef struct
{
    short	DataBlock;                      //  表示多少个块等待打印 0=no
    short	Temperature;                    //  表示采样温度
	
	uint    StepTime;                       //  表示该缓冲打印时步进的时间
    uchar   SendDot[MAX_HEAT_BLOCK][48];    //  存储即将送入的点
	ushort  DotHeatTime[MAX_HEAT_BLOCK];    //  存储加热时间,供加热时递减
	uchar 	NxtLiFstDot[48];
}HeatUnit;

static HeatUnit                k_HeatUnit[2];
//一个是加热指针，一个是运算指针
static HeatUnit *uhptr,*chptr;
static char k_StepGo;
static unsigned short k_BlockTime;		//定义单个块加热标准时间
static unsigned short k_HeatBlock;		//定义当前的加热块
static uint k_LastStepTime;     // 上一步步进时间

static int k_NextHalfDot = 0;

//用以与打印机缓冲与加热单元之间过渡
static  uchar   k_next_SendDot[MAX_HEAT_BLOCK][48],k_next_block; //下一行的点

static unsigned short k_LastDotNum;
static void make_dot_tbl(void);

static uchar ADC_TEMP_CHANNEL;//温度控制ADC通道	
static uchar ADC_PAPER_CHANNEL;	//缺纸通道
static uchar ADC_POWER_VOL_CHANNEL;//电压检测通道
static int VAL_CHECK_PAPER;//有纸检测阀值

static void Thermal_TypeSel(uchar tp)
{
	ADC_TEMP_CHANNEL =	1;
	ADC_PAPER_CHANNEL = 0;
	ADC_POWER_VOL_CHANNEL = 2;
	if(tp==TYPE_PRN_PRT48F)
	{
    	VAL_CHECK_PAPER = 100;
		//配置温度表
		k_ADConvTList=(ADConvT *)k_ADConvTListPRT48AF;
		k_ADConvNum=sizeof(k_ADConvTListPRT48AF)/sizeof(k_ADConvTListPRT48AF[0]);
		//配置加速表
		k_PreStepTime=(ushort*)k_PreStepTimePRT48AF;
		k_maxstep=sizeof(k_PreStepTimePRT48AF)/sizeof(k_PreStepTimePRT48AF[0]);
        if(get_machine_type()==S900)
        {
            k_StepTime=1227;
            HEAT_TIME=800;
        }
        else
        {
    		k_StepTime=1004;
    		HEAT_TIME=700;
        }
        k_AdjTime=72;
		k_stepmode=MODE_SLOW_START; //使用缓加速
		MAX_LINE_DOT=192;
		BLACK_HEADER_DOT=48;
		SEL_HEADER_DOT=48;
		TEMPERATURE_ALLOW=65; //过热保护65度
	}
	else if(tp == TYPE_PRN_PRT486F)
	{ 
		//配置温度表
		k_ADConvTList=(ADConvT *)k_ADConvTListPRT486F;
		k_ADConvNum=sizeof(k_ADConvTListPRT486F)/sizeof(k_ADConvTListPRT486F[0]);
		HEAT_TIME=700;
		k_AdjTime=62;	
		MAX_LINE_DOT=192;
		BLACK_HEADER_DOT=48;
		SEL_HEADER_DOT=48;
		TEMPERATURE_ALLOW=65;
		VAL_CHECK_PAPER = 800;//PT486F高电平有纸		

        //配置加速表
        k_PreStepTime=(ushort*)k_FastPreStepTimePRT486F;
        k_maxstep=sizeof(k_FastPreStepTimePRT486F)/sizeof(k_FastPreStepTimePRT486F[0]);
        k_StepTime=691;
        k_stepmode=MODE_SLOW_START; //使用缓加速
	}
	else if (tp == TYPE_PRN_PT48D)	//D210使用
	{
		k_ADConvTList=(ADConvT *)k_ADConvTListPrtPT48d;
		k_ADConvNum=sizeof(k_ADConvTListPrtPT48d)/sizeof(k_ADConvTListPrtPT48d[0]);
		//配置加速表
		k_PreStepTime=(ushort*)k_PreStepTimePrtPT48d;
		k_maxstep=sizeof(k_PreStepTimePrtPT48d)/sizeof(k_PreStepTimePrtPT48d[0]);

		k_StepTime=1000;
		HEAT_TIME=1000;

		k_AdjTime=66;
		k_stepmode=MODE_SLOW_START; //使用缓加速
		MAX_LINE_DOT=192;
		BLACK_HEADER_DOT=48;
		SEL_HEADER_DOT=48;
		TEMPERATURE_ALLOW=65; 		//过热保护65度
		VAL_CHECK_PAPER = 512;//PT486F低电平有纸		
	}
	//调整一下最大步进
	for(;k_maxstep>1;k_maxstep--)
	{
		if(k_PreStepTime[k_maxstep-1]>=k_StepTime) break;
	}
}

// check the paper ,return PRN_PAPEROUT = no paper, PRN_OK = ok
static int s_CheckPaper(void)
{
	int nValue;

	nValue = s_ReadAdc(ADC_PAPER_CHANNEL);
	if (ptPrnDrvCfg->mask & PRN_MASK_TEMP_ADC_REVERT)
	{
		if(nValue>VAL_CHECK_PAPER) return PRN_OK;
	}
	else
	{
		if(nValue<VAL_CHECK_PAPER) return PRN_OK;
	}
	return PRN_PAPEROUT;
}

int CheckPaper(void)
{
	int ret;
	
	ENABLE_PRN_PWR();
	DelayMs(50);
	ret = s_CheckPaper();
	DelayMs(50);
	DISABLE_PRN_PWR();
	return ret;
}

//返回毫伏值
static int s_ReadPrnVol(void)
{
	int rsl;
/*	
	rsl=s_ReadAdc(ADC_POWER_VOL_CHANNEL);
	rsl=(rsl*3*25*11)>>6;
*/	
	rsl = GetVolt();
	return rsl;
}

// 计算温度并返回温度值
int s_GetPrnTemp_PRT(void)
{
    int i,nValue,nPreValue,diff;
	
    // 读取温度采样AD转换值
    nPreValue = 0x8000;
    for (i=0; i<3; i++)
    {
        nValue = s_ReadAdc(ADC_TEMP_CHANNEL); 
        diff   = (nValue > nPreValue) ? (nValue - nPreValue) : (nPreValue - nValue);
        if(diff < 3)    break;
        nPreValue = nValue;
    }

    // 换算成温度
    for (i=0; i<k_ADConvNum-1; i++)
    {
        if (nValue > k_ADConvTList[i].ADValue)
        {
	        break;
	    }
    }
	diff=k_ADConvTList[i].Temperature;
	//add_motor_time_rec(diff|(1<<30));
    return(diff);
}


// 电源开关控制
static void s_PrnPower(uchar status)
{
    volatile uint i = 0;
    
    if(status == ON)
    {
		DISABLE_PRN_LAT();
        ENABLE_PRN_PWR();
		for (i = 0; i < 0x2000;i++);
    }
    else
    {
        DISABLE_PRN_PWR();
        for (i = 0; i < 0x2000;i++);
        ENABLE_PRN_LAT();
    }
}

static void s_MotorStep(uchar status)
{
	int phase_tab[8] = {0x05, 0x04, 0x06, 0x02, 0x0a, 0x08, 0x09, 0x01};
	int val = 0;

#ifdef DEBUG_MOTOR
	static unsigned ln=0;
	val = s_GetTimeDurationAndSet(&ln);
	add_motor_time_rec(0x10000000|val);
#endif
	
	if (status)
	{
		k_MotorStep += k_StepGo;
		k_MotorStep &= 7;

		if (ptPrnDrvCfg->mask & PRN_MASK_MOTOR_DIR_REVERT)
			val = phase_tab[7-k_MotorStep];
		else
			val = phase_tab[k_MotorStep];
	}
	else
	{
		val = phase_tab[0];
	}
	
	gpio_set_mpin_val(ptPrnDrvCfg->motor.port, ptPrnDrvCfg->motor.pin, val<<4);
}

static void s_ClearLatchData(void)
{
	uchar EmptyBuf[48];
    memset(EmptyBuf, 0, sizeof(EmptyBuf));
    SEND_AND_LATCH_DATA(EmptyBuf,48);
}

static void s_StopPrn(uchar status)
{
    int     i,t;
    //Stop timer
    s_HeatTimerStop();
    s_MotorTimerStop();
    DISABLE_PRN_LAT();
    DISABLE_STB();
	s_ClearLatchData();
    s_PrnPower(OFF);
	SET_MOTOR_IDLE();
    
	k_Heating   = 0;
    k_PrnStatus = status;
    ScrSetIcon(ICON_PRINTER,0);
}

//==============================================================================
//系统的打印机初始化
void s_PrnHalInit_PRT(uchar prnType)
{
	if (prnType == TYPE_PRN_PRT486F)
	{
		ptPrnDrvCfg = &S500PrnCfg;
	}
	else if (prnType == TYPE_PRN_PT48D)
	{
		ptPrnDrvCfg = &D210PrnCfg;
	}
	else
	{
		ptPrnDrvCfg = &SxxxPrnCfg;
	}
	
	//关打印机电源
	CFG_PRN_PWR();
	s_PrnPower(OFF);

    Thermal_TypeSel(prnType);

    // 配置Strobe信号
    CFG_PRN_STB();
    DISABLE_STB();
	
	//数据送出关闭
	CFG_PRN_LAT();
    DISABLE_PRN_LAT();

	//打印机相位配置
	CFG_MOTOR();
	s_MotorStep(0);

	//SPI配置
    CFG_PRN_SPI();

	//清除打印头数据,开机时其打印数据是随机数
	s_ClearLatchData();

	//低功耗口配置
	SET_MOTOR_IDLE();
	
    s_MotorTimerInit();
    s_HeatTimerInit();

    //Init var
    k_MotorStep    = 0;
    k_Heating      = 0;
    k_PaperLack    = 0;
	
	k_LastDotNum=0;
	
		
	make_dot_tbl();
	k_StepGo=1;	
}

#ifdef TABLE_ADJ
/*电压调整表，参考值为9.1V 调整系数要除以1024 */
const unsigned short cuVolAdj[][2]=
{
5000,9525,5100,8640,5200,7872,5300,7202,5400,6615,5500,6096,5600,5636,5700,5226,
5800,4860,5900,4530,6000,4233,6100,3964,6200,3721,6300,3498,6400,3296,6500,3110,
6600,2940,6700,2783,6800,2638,6900,2505,7000,2381,7100,2266,7200,2160,7300,2060,
7400,1968,7500,1881,7600,1800,7700,1724,7800,1653,7900,1586,8000,1524,8100,1464,
8200,1409,8300,1356,8400,1306,8500,1259,8600,1215,8700,1172,8800,1132,8900,1094,
9000,1058,9100,1024,9200, 991,9300, 960,9400, 930,9500, 901,9600, 874,9700, 848,
9800, 824,9900, 800,10000, 777,10100, 755,10200, 735,10300, 715,10400, 695,10500, 677,
10600, 659,10700, 642,10800, 626,10900, 610,11000, 595,11100, 580,11200, 566,11300, 553,
11400, 540,11500, 527,11600, 515,11700, 503,11800, 492,11900, 481,12000, 470,12100, 460,
12200, 450,12300, 440,12400, 431,12500, 422,12600, 413,12700, 404,12800, 396,12900, 388,
13000, 381,
};
#endif

//获取温度及电压并计算一个块加热时间 在第一个步进时候获取
static void get_block_time_common(void)
{
	int temp_in,temp,vol;
	int rsl;
#ifdef TABLE_ADJ
	int ii;
#endif
    uint heatTime;

	temp_in=s_GetPrnTemp_PRT();
	if(chptr)chptr->Temperature=temp_in;
    heatTime = HEAT_TIME*k_GrayLevel/100;
	if(temp_in > 25)
	{
		temp = temp_in - 25;
		rsl=heatTime-heatTime*temp/k_AdjTime;
	}
	else
	{
		temp = 25 - temp_in;
		rsl=heatTime+heatTime*temp/k_AdjTime;
	}
	k_BlockTime=rsl;	
	vol=s_ReadPrnVol();
#ifdef TABLE_ADJ
	for(ii=0;ii<sizeof(cuVolAdj)/sizeof(cuVolAdj[0]);ii++)
	{
		if(vol < cuVolAdj[ii][0])
		{
			break;
		}
	}
	if(ii >= sizeof(cuVolAdj)/sizeof(cuVolAdj[0])) ii=sizeof(cuVolAdj)/sizeof(cuVolAdj[0])-1;
	//调整系数要除以1024
	rsl=(k_BlockTime*cuVolAdj[ii][1])>>10;
	k_BlockTime=rsl;
#else
	vol=vol-3000;
	k_BlockTime=k_BlockTime*6100/vol;
	k_BlockTime=k_BlockTime*6100/vol;
#endif
	//add_motor_time_rec(k_BlockTime|(1<<29));
}

int const Heat_Temperater_Ratio[]=
{/*
	//25 - 30
	75, 75, 75, 75, 75,
	//31 - 35
	67, 67, 67, 65, 65, //
	//36 - 40
	63, 63, 61, 61, 61, 
	//41 - 45
	60, 60, 58, 58, 58,
	//46 - 50
	55, 55, 55, 55, 55,
	//51 - 55
	53, 53, 53, 53, 53,	//	
	//56 - 60
	55, 55, 55, 55, 55,
	//61 - 65
	53, 53, 53, 53, 53,	//
	
	//66 - 70
	57, 57, 57, 57, 57,
	//71 - 75
	40, 40, 40, 40, 40, 
	//76 - 80
	40, 40, 40, 40, 40, 
	//81 - 85
	40, 40, 40, 40, 40, 
	//86 - 90
	40, 40, 40, 40, 40, 
	//91 - 95
	40, 40, 40, 40, 40, 
	//96 - 100
	40, 40, 40, 40, 40, 
	*/
	
	//25 - 30
	100, 100, 100, 100, 100,
	//31 - 35
	 89,  89,  89,	87,  87,
	//36 - 40
	 84,  84,  81,	81,  81,
	//41 - 45
	 80,  80,  77,	77,  77,
	//46 - 50
	 73,  73,  73,	73,  73,
	//51 - 55
	 71,  71,  71,	71,  71,	
	//56 - 60
	 73,  73,  73,	73,  73,
	//61 - 65
	 71,  71,  71,	71,  71,
	 
	//66 - 70
	 76,  76,  76,	76,  76,
	//71 - 75
	 53,  53,  53,	53,  53,
	//76 - 80
	 53,  53,  53,	53,  53,
	//81 - 85
	 53,  53,  53,	53,  53,
	//86 - 90
	 53,  53,  53,	53,  53,
	//91 - 95
	 53,  53,  53,	53,  53,
	//96 - 100
	 53,  53,  53,	53,  53,
};

typedef struct{
	int volt;
	int rate;
}T_VoltRateAdjust;

T_VoltRateAdjust gVoltAdjustTab[] = {
	{10000, 60},
	{9600,  65},
	{9400,  65},
	{9200,  65},
	{9000,  70},
	{8800,  70},
	{8600,  75},
	{8400,  75},
	{8300,  80},
	{8200,  90},
	{8100,  95},
	{8000, 100},
	{7900, 110},
	{7800, 115},
	{7700, 120},
	{7600, 125},
	{7500, 130},
	{7400, 135},
	{7300, 140},
	{7200, 145},
	{7100, 150},
	{7000, 160},
	{6900, 165},
	{6800, 175},
	{6700, 185},
	{6600, 195},
	{6500, 200},
	{6400, 210},
	{6300, 215},
	{6200, 225},
	{6100, 230},
	{6000, 235},
	{5900, 240},
};

static void get_block_time_D210(void)
{
	int temp,temp_in,volt;
	static int last_temp=0, last_volt=0;
	int rsl, i, cnt;
    uint heatTime;

	volt = s_ReadPrnVol();
	temp_in=s_GetPrnTemp_PRT();
	if (last_volt == 0)
		last_volt = volt;
	else
	{
		last_volt = (last_volt + volt)/2;
		volt = last_volt;
	}
/*
	if (last_temp == 0)
		last_temp = temp_in;
	else
	{
		last_temp = (last_temp + temp_in)/2;
		temp_in = last_temp;
	}
*/	
	if(chptr)
		chptr->Temperature=temp_in;
    heatTime = HEAT_TIME*k_GrayLevel/100;
	
	if(temp_in > 25)
	{
		temp = temp_in - 25;
		rsl=heatTime-heatTime*temp/k_AdjTime;
		rsl = rsl*Heat_Temperater_Ratio[temp_in-25]/100;
	}
	else
	{
		temp = 25 - temp_in;
		rsl=heatTime+heatTime*temp/k_AdjTime;
	}

	k_BlockTime=rsl;
	k_BlockTime /= 2;
	
	cnt = sizeof(gVoltAdjustTab)/sizeof(gVoltAdjustTab[0]);
	for(i=cnt; i>1; i--)
	{
		if (volt < gVoltAdjustTab[i-1].volt)
			break;
	}
	k_BlockTime = k_BlockTime * gVoltAdjustTab[i-1].rate / 100;
/*	
	add_motor_time_rec(0x80000000|k_BlockTime);
	add_motor_time_rec(0x08000000|volt);
	add_motor_time_rec(0x04000000|rate);
	add_motor_time_rec(0x02000000|temp_in);
*/	
}

static void get_block_time(void)
{
	ptPrnDrvCfg->cal_block_time();
}

//0--256对应有1个数的表
static unsigned char s_kdotnum[256];
static void make_dot_tbl(void)
{
	int ii,jj,nn;
	uchar tch;
	for(ii=0;ii<256;ii++)
	{
		tch=ii;
		for(jj=nn=0;(jj<8)&&(tch);jj++)
		{
			if(tch&1) nn++;
			tch>>=1;
		}
		s_kdotnum[ii]=nn;
	}
}
static uint get_dot(unsigned char *dot,int len)
{
	uint rsl;
	rsl=0;
	while(len--)
	{
		rsl=rsl+s_kdotnum[*dot++];
	}
	return rsl;
}
static unsigned short dot_time(uint dotnum)
{
	unsigned int rsl;
	rsl=dotnum/SEL_HEADER_DOT;
	if(dotnum%SEL_HEADER_DOT) 
	{
		rsl++;
	}
	rsl=rsl*(k_BlockTime+HEAD_INT_TIME);
	return rsl;
}

static unsigned short next_step_tbl[100],p_last_step,p_stop_step,p_last_time;
static void create_next_step_tbl(void)
{
	int ii,jj;
	p_last_step=0;
	get_block_time();
	p_stop_step=k_maxstep;
	for(ii=0;ii<k_maxstep;ii++)
	{
		if( ii	< k_CurDotLine)	
		{
			uint dnum;
			dnum=get_dot(k_DotBuf[ii],48);
			next_step_tbl[ii]=dot_time(dnum);	
		}
		else
		{
			next_step_tbl[ii]=k_PreStepTime[0];	
		}
		for(jj=k_maxstep;jj;jj--)
		{
			if(k_PreStepTime[jj-1]>next_step_tbl[ii]) break;
		}
		jj+=ii;
		if((jj)<p_stop_step) p_stop_step=(jj);
	}
	p_last_time=k_PreStepTime[p_stop_step];
	
}

//计算当前该走的时间
static unsigned short get_now_time(unsigned short mt)
{
	int ii,jj,kk,mm;
	uint dnum;
	unsigned short nextlasttime,mintime;

	//查找多少步以后减速
	ii=p_last_step;
	next_step_tbl[ii]=mt;
	mm=k_maxstep;
	for(jj=0;jj<k_maxstep;jj++)
	{
		if(next_step_tbl[ii] >= k_PreStepTime[jj])
		{
			kk=jj;
			while(kk)
			{
				if(next_step_tbl[ii]<=k_PreStepTime[kk]) break;
				kk--;
			}
			if(kk)
			{
				if(mm>(kk+jj)) mm=kk+jj;
			}
			else
			{
				//查到的是停止步数
				mm=jj;
				break;
			}
		}
		ii++;
		ii%=k_maxstep;
	}
	if(mm<k_maxstep)  mintime=	k_PreStepTime[mm];
	else				   mintime=	k_PreStepTime[k_maxstep-1];
	if(mt<mintime) mt=mintime;
	ii=k_CurPrnLine+k_maxstep-1;
	if(ii<k_CurDotLine)
	{
		dnum=get_dot(k_DotBuf[ii],48);
		nextlasttime=dot_time(dnum);	
	}
	else
	{
		nextlasttime=k_PreStepTime[0];
	}
	next_step_tbl[p_last_step]=nextlasttime;
	p_last_step++;
	p_last_step%=k_maxstep;
	
	if(mt<p_last_time)
	{
		for(ii=0;ii<k_maxstep;ii++)
		{
			if(p_last_time>k_PreStepTime[ii])
			{
				break;
			}
		}
		if(ii>=k_maxstep) ii--;
		if(mt< k_PreStepTime[ii]) mt=k_PreStepTime[ii];
	}
	p_last_time=mt;
	return mt;
}

static unsigned short get_pretime(unsigned short sn)
{
	
	unsigned short rsl,nn;
	if(k_stepmode==MODE_SLOW_STOP)
	{
		p_stop_step=k_maxstep;
	}
	else
	{
	    p_stop_step=k_maxstep;
	}

	nn=sn;
	if((PRESTEP_NUM-sn+p_stop_step)<nn)
	{
		nn=(PRESTEP_NUM-sn+p_stop_step);
	}
	p_last_time=k_PreStepTime[nn];
	if(p_last_time<k_StepTime) p_last_time=k_StepTime;
	k_LastStepTime=p_last_time;
	return p_last_time;
}

static uint get_now_steptime(uint nowtm,uint lasttm)
{
	int ii,tt;
	uint mindt;
	//速度比之前慢则直接返回
	if(nowtm>=lasttm) return nowtm;
	//速度比之前快则加以控制
	tt=k_maxstep;
	for(ii=0;ii<tt;ii++)
	{
		if(k_PreStepTime[ii]<lasttm)//找到合适的速度
		{
			break;
		}
	}
	if(ii<(tt-1))
	{
		mindt=k_PreStepTime[ii];
	}
	else
	{
		mindt=k_PreStepTime[tt-1];
	}

	if(nowtm<lasttm)
	{
		//防加速过快
		if(nowtm<mindt) nowtm=mindt;
	}
	if(nowtm<k_StepTime) nowtm=k_StepTime;
	return nowtm;
}

static void s_SetPrnTime(HeatUnit *hdptr)
{
	int ii,jj,allt;
	allt=0;
	//设置各个块加热时间
	for(ii=0;ii<hdptr->DataBlock;ii++)
	{
	     hdptr->DotHeatTime[ii]=k_BlockTime;
		 allt=allt+hdptr->DotHeatTime[ii];
	}
	
	if(allt)
	{
	    if (ptPrnDrvCfg->mask & PRN_MASK_REST_ADJUST)
	    {
			//休息时间根据加热块的不同而不同，加热块数多的，休息时间少，反之增加，最多为5*HEAD_INT_TIME
			//当每块为48个点时，共有8块为基准，此时，若加热8块，则不需要多余的休息时间，其余分块情况，根据块数调整。
			if (ii<3) ii=3;
			if (ii>8) ii=8;
			allt+=(8-ii)*HEAD_INT_TIME;	    
	    }
        else  allt+=ii*HEAD_INT_TIME;
	}
	
	if (ptPrnDrvCfg->mask & PRN_MASK_HEAT_TWINCE)
	{
		hdptr->StepTime=(allt);
	}
	else
	{
		hdptr->StepTime=(allt)/2;
	}
	if(k_stepmode==MODE_SLOW_STOP)
	{
		//以缓减速缓加速方式走
		hdptr->StepTime=get_now_time(hdptr->StepTime);
	}
	else if(k_stepmode==MODE_SLOW_START)
	{
		//以缓加速方式走
		hdptr->StepTime=get_now_steptime(hdptr->StepTime,k_LastStepTime);
	}
	else
	{
		//既不进行加速也不进行减速方式走
	    ii =PRESTEP_NUM+ k_CurPrnLine * 2;
		if(ii < k_maxstep)
		{
			if(hdptr->StepTime<k_PreStepTime[ii])
				hdptr->StepTime=k_PreStepTime[ii];
		}
		if(hdptr->StepTime<k_StepTime)hdptr->StepTime=k_StepTime;

	}
    k_LastStepTime = hdptr->StepTime;
}


static short k_blkln=0;

const unsigned char cbtn[8]={0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff};
static unsigned short split_block(unsigned char blk[MAX_HEAT_BLOCK][48],unsigned char *tPtr)
{
		unsigned short jj,dn,bn,bt;
		unsigned char tch,nch,tb;
		unsigned short max_dot;
		unsigned short lndot;

	if(ptPrnDrvCfg->mask & PRN_MASK_DOT_ADJUST)
	{
		if(k_LastDotNum>MAX_LINE_DOT)
		{
			max_dot=SEL_HEADER_DOT;
		}
		else
		{
			max_dot=2*SEL_HEADER_DOT;
		}
		k_LastDotNum=0;
	}
	else
	{
		max_dot=SEL_HEADER_DOT;
	}

	if (ptPrnDrvCfg->mask & PRN_MASK_BLACK_ADJUST)
	{
		if(k_blkln>=MAX_BLACK_LINE)
		{
			k_blkln=MAX_BLACK_LINE;
			max_dot=BLACK_HEADER_DOT;
		}
		lndot=0;
	}
		memset(blk,0,48*MAX_HEAT_BLOCK);
		for(jj=bn=dn=0;jj<48;jj++)
		{
			tch=*tPtr++;
			if(tch==0) 
				continue;
			if(ptPrnDrvCfg->mask & PRN_MASK_DOT_ADJUST)
				k_LastDotNum+=s_kdotnum[tch];
					
			if (ptPrnDrvCfg->mask & PRN_MASK_BLACK_ADJUST)
				lndot+=s_kdotnum[tch];
	

	blk[bn][jj]=tch;
#ifdef BIT_SEL
			bt=jj<<3;
			for(;tch!=0;)
			{
				if(tch&0x80) 
				{
					if(++dn==max_dot)
					{
						//需要精确的位控制
						tb=bt&7;
						if(tb!=7)
						{
							nch=blk[bn][jj];
							blk[bn][jj] =nch&cbtn[tb];
							blk[bn+1][jj] =nch&(~cbtn[tb]);
						}
						bn++;				
						dn=0;
					}
				}
				bt++;
				tch=(tch<<1)&0xff;
			}
#else
	dn+=s_kdotnum[tch];
	if(dn>=max_dot)//数得最大加热点数则换另一加热块
	{
		bn++;				
		dn=0;
	}
#endif
	}
	if(dn) bn++;//用以最后一次的数据不够 最大点数

	if (ptPrnDrvCfg->mask & PRN_MASK_BLACK_ADJUST)
	{
		if(lndot>=192)
		{
			k_blkln++;
		}
		else if(lndot>=128)
		{

		}
		else 
		{
			if(k_blkln) k_blkln--;
		}
	}
	return bn;
}

// 预备下一个点行的打印数据到对应的缓冲, hdptr表示结构指针
static HeatUnit *s_LoadData(HeatUnit *hdptr)
{
    if(hdptr==NULL)return NULL;
    //  判断打印是否已经完成
    if(k_CurPrnLine >= k_CurDotLine)
    {
    	memset(hdptr->SendDot[0],0,sizeof(hdptr->SendDot));
    	memset(hdptr->NxtLiFstDot,0,sizeof(hdptr->NxtLiFstDot));
		hdptr->DataBlock=0;
        return NULL;
    }
	//把缓冲中的数据，和信息加载到加热单元
	memcpy(hdptr->SendDot[0],k_next_SendDot,sizeof(hdptr->SendDot));
	hdptr->DataBlock=k_next_block;//此块信息之前已经算好
	k_CurPrnLine++;
	if(k_CurPrnLine >= k_CurDotLine)
	{
		memset(k_next_SendDot,0,sizeof(k_next_SendDot));
		k_next_block=0;
	}	
	else
	{
		k_next_block=split_block(k_next_SendDot,k_DotBuf[k_CurPrnLine]);
	}
	memcpy(hdptr->NxtLiFstDot,k_next_SendDot[0],sizeof(hdptr->NxtLiFstDot));
	s_SetPrnTime(hdptr);
	return hdptr;
}

//Timer for heating
static void s_HeatTimer(void)
{		
    s_HeatTimerStop();
	k_HeatBlock++;
	if(k_HeatBlock<uhptr->DataBlock)
	{
		SEND_AND_LATCH_DATA(uhptr->SendDot[k_HeatBlock],48);
        s_HeatTimerStart(uhptr->DotHeatTime[k_HeatBlock]);
	}
	else
	{
		k_HeatBlock=0;
		
		if (k_NextHalfDot == 0 && (ptPrnDrvCfg->mask & PRN_MASK_HEAT_TWINCE))
		{
			SEND_AND_LATCH_DATA(uhptr->SendDot[k_HeatBlock],48);
		}
		else
		{
			//已经加热完毕送入下一个加热数据
			SEND_AND_LATCH_DATA(uhptr->NxtLiFstDot,48);
		}
        DISABLE_STB();
        k_Heating = 0;
	}
}

//获取下一步步进时间
static int  get_step_time(int start_step,int stop_step)
{
	int rsl;
	if(start_step>k_maxstep) start_step=k_maxstep;
	if(stop_step>k_maxstep) stop_step=k_maxstep;
	if(start_step>stop_step) stop_step=stop_step;
	rsl=k_PreStepTime[stop_step];
	if(rsl<k_StepTime) rsl=k_StepTime;
	return rsl;
	
}

//马达驱动的中断服务程序
static void s_MotorTimer(void)
{
	static unsigned char icon_on;
	static T_SOFTTIMER icon_tm;
    HeatUnit *tp;

    s_MotorTimerStop();
	

	if(s_TimerCheckMs(&icon_tm) == 0)
	{
		if(k_FeedCounter < 3) icon_on=1;
		ScrSetIcon(ICON_PRINTER,icon_on++&1);
		s_TimerSetMs(&icon_tm,500);
	}
    k_FeedCounter++;


#ifdef ENABLE_MOTOR_SUBDIVISION    
    if((k_FeedCounter >= 2*MAX_FEEDPAPER_STEP))
#else
    if((k_FeedCounter >= MAX_FEEDPAPER_STEP))
#endif
    {
        s_StopPrn(PRN_OK);
        return;
    }
    
	// 判断到缺纸
	if(s_CheckPaper()==0)
	{
		k_PaperLack = 0;
	}
	else
	{
		// 连续40次判断到缺纸,则认为是缺纸
		k_PaperLack++;
		if(k_PaperLack > 40)
		{
			s_StopPrn(PRN_PAPEROUT);
			return;
		}
	}

    if (0 != k_FeedStat)
    {
        //判断是否需要预走纸
    #ifdef ENABLE_MOTOR_SUBDIVISION    
        if((k_FeedCounter < (2*PRESTEP_NUM)))
    #else
        if((k_FeedCounter < (PRESTEP_NUM)))
    #endif
        {
            int gt;
            // 进行预走纸步骤
            s_MotorStep(1);

    #ifdef ENABLE_MOTOR_SUBDIVISION    
            gt=get_pretime(k_FeedCounter/2)/2;
            if(k_FeedCounter==2*PRESTEP_NUM-1)
    #else
            gt=get_pretime(k_FeedCounter);
            if(k_FeedCounter==PRESTEP_NUM-1)//最后一步预走纸则开始装数
    #endif
            {
                s_MotorTimerStart(gt-ADJ_MOTOR_PRE);

                s_LoadData(chptr);
                SEND_AND_LATCH_DATA(chptr->SendDot[0],48);
                k_HeatBlock=0;
            }
            else
            {
                s_MotorTimerStart(gt-ADJ_MOTOR_PRE);
            }
            return;
        }
    }

	if((k_Heating)&&(k_StepIntCnt==0 || (k_StepIntCnt==2&&(ptPrnDrvCfg->mask & PRN_MASK_HEAT_TWINCE))))
	{
		//如果加热为完成但步进已完则插入等待,这段代码是不会进入的
		//如果进入请查找原因步进时间是否不对
	//add_motor_time_rec(0x40000000|k_StepIntCnt);
		s_MotorTimerStart(50);
		k_FeedCounter--;
		return;
	}
    
    switch (k_StepIntCnt)
    {
        case 0:
            DISABLE_STB();// stop heating
			if(chptr==NULL)
            {
                s_StopPrn(PRN_OK);
                return;
            }

            //  判断当前缓冲采样的温度是否过温,如果过温则退出
            //if(chptr->Temperature >= TEMPERATURE_FORBID)  // 超过允许打印上限
            if(chptr->Temperature >= ptPrnDrvCfg->tempForbiden)
            {
                s_StopPrn(PRN_TOOHEAT);
                return;
            }
            
			tp=chptr;
			chptr=uhptr;
			uhptr=tp;

		   
		    //  进行第一步走纸
			s_MotorStep(1);
	        k_StepIntCnt++;   
			//因为在上次加热完最后一块之后就已经把下一行的数据先装到打印机
#ifdef ENABLE_MOTOR_SUBDIVISION           
			s_MotorTimerStart((uhptr->StepTime/2)-ADJ_MOTOR_ST1);
#else
            s_MotorTimerStart(uhptr->StepTime-ADJ_MOTOR_ST1);
#endif
           	k_HeatBlock=0;
			k_Heating=0;
			k_NextHalfDot = 0;
            if(uhptr->DataBlock)
            {
				k_Heating=1;
				ENABLE_STB();
				s_HeatTimerStart(uhptr->DotHeatTime[0]);
            }
			else
			{
				//直接送入下一行的点
				SEND_AND_LATCH_DATA(uhptr->NxtLiFstDot,48);
			}
#ifdef TEMPERATURE_ADJ
			get_block_time();
#else
			chptr->Temperature=s_GetPrnTemp_PRT();
#endif
            break;
#ifdef ENABLE_MOTOR_SUBDIVISION           
		case 1:
			s_MotorStep(1);
			s_MotorTimerStart((uhptr->StepTime/2)-ADJ_MOTOR_ST2);
			k_StepIntCnt++;
			break;
		case 2:
			s_MotorStep(1);
			s_MotorTimerStart((uhptr->StepTime/2)-ADJ_MOTOR_ST2);
			k_StepIntCnt++;
			
			if (ptPrnDrvCfg->mask & PRN_MASK_HEAT_TWINCE)
			{
				k_NextHalfDot = 1;
	           	k_HeatBlock=0;
				k_Heating=0;
	            if(uhptr->DataBlock)
	            {
					k_Heating=1;
					ENABLE_STB();
					s_HeatTimerStart(uhptr->DotHeatTime[0]);
	            }
				else
				{
					//直接送入下一行的点
					SEND_AND_LATCH_DATA(uhptr->NxtLiFstDot,48);
				}
			}
			break;
#endif
        default:
			//DISABLE_STB();
            s_MotorStep(1);
#ifdef ENABLE_MOTOR_SUBDIVISION           
			s_MotorTimerStart((uhptr->StepTime/2)-ADJ_MOTOR_ST2);
#else
            s_MotorTimerStart(uhptr->StepTime-ADJ_MOTOR_ST2);
#endif
			chptr=s_LoadData(chptr);
            k_StepIntCnt = 0;
            break;
    }
}

static void s_StartMotor(void)
{
    k_StepIntCnt    = 0;
    k_Heating       = 0;

    k_FeedCounter   = 0;
    k_CurPrnLine    = 0;
    k_PaperLack     = 0;
    k_LastStepTime  = k_StepTime;
	
    
	DISABLE_PRN_LAT();
	DISABLE_STB();
    s_PrnPower(ON);
	CFG_PRN_SPI();
    DelayMs(2);
    // 初始化
    memset(k_HeatUnit,0x00,sizeof(k_HeatUnit));
	chptr=&k_HeatUnit[0];
	uhptr=&k_HeatUnit[1];
	if(k_stepmode==MODE_SLOW_STOP)
	{
		create_next_step_tbl();
	}
	else
	{
		get_block_time();
	}
	//把一行数据分好加热块
	if(!k_FeedStat)				// 没有预走纸
	{
		k_next_block=split_block(k_next_SendDot,k_DotBuf[0]);
		s_LoadData(chptr);
		SEND_AND_LATCH_DATA(chptr->SendDot[0],48);
		k_HeatBlock=0;

	    // 步进控制
	    s_MotorStep(0);
	    s_MotorTimerStart(10); 
    }
    else						// 有预走纸
    {
    	if(k_CurDotLine) k_next_block=split_block(k_next_SendDot,k_DotBuf[0]);
	
		k_blkln=0;// 刚刚启动不会打黑块，还有预走纸
	    
	    // 送空数据启动Latch
	    s_ClearLatchData();

	    // 步进控制
	    s_MotorStep(0);
	    s_MotorTimerStart(k_PreStepTime[0]); 
    }

    ScrSetIcon(ICON_PRINTER,1);    
}

//获取打印机加热头和切纸刀之间增加的距离，将距离转换为像素点
//默认距离为0
//S500-R距离为1.5mm
//S900-R距离为6mm
int s_GetPrinterKnifePix(void)
{
	char context[64];
	static int PrinterKnifeSpace,RfStyle;
	static char InitFlag = 0;
	double space;

	if(InitFlag) return PrinterKnifeSpace;

	if(get_machine_type() == S800)
	{
		PrinterKnifeSpace = 24;				// S800为固定的24
	}
	else
	{
		RfStyle = GetTerminalRfStyle();
		space = 0;
		if(RfStyle)
		{
			if(RfStyle == 1)			// S500-R 距离为1.5mm
			{
				space = 1.5;
			}
			else if(RfStyle == 2)		// S900-R 距离为6mm
			{
				space = 6;
			}
			else
			{
				space = 0;
			}
		}
		if(space)
		{
			PrinterKnifeSpace = space * 8;	//每个像素点为0.125mm
		}
	}
	
	InitFlag = 1;
    return PrinterKnifeSpace;	
}

uchar s_PrnStart_PRT(void)
{
	int val;
	
	if(k_PrnStatus==PRN_OUTOFMEMORY)
	{
		return k_PrnStatus;
	}
    if(k_CurDotCol != k_LeftIndent)
    {
        if(s_NewLine())
            return(k_PrnStatus);
    }
    if(k_CurDotLine == 0)
    {
        return PRN_OK;
    }
    
	CompensatePapar(s_GetPrinterKnifePix());

	val = s_ReadPrnVol();
	if(val >= ptPrnDrvCfg->maxVoltage)
	{
	    ScrCls();
	    ScrPrint(0,1,0," Voltage Too High...");
		ScrPrint(0,3,0," Check the Adapter!!!");
		kbflush();
		getkey();
		return PRN_TOOHEAT;
	}
    // 等待打印机空闲
    while(k_PrnStatus == PRN_BUSY);

	DISABLE_STB();

/* 加入检测是否有纸 */
	ENABLE_PRN_PWR();
	DelayMs(10);
//先检测是否过热再检测缺纸
	val = s_GetPrnTemp_PRT();
	//if(val >= TEMPERATURE_ALLOW)      
	if(val >= ptPrnDrvCfg->tempStart)
    {
		DISABLE_PRN_PWR();
		return PRN_TOOHEAT;
    }
		
/* 加入检测是否有纸 */
	if(s_CheckPaper()!=0)
	{
		int ii;
		ii=0;
		while(1)
		{
			DelayMs(2);
			if(s_CheckPaper()==0)
			{
				break;
			}
			if(ii++>10)
			{
				DISABLE_PRN_PWR();
				return PRN_PAPEROUT;
			}
		}
	}

	DisableCharge();	//only for D210
	
    k_PrnStatus = PRN_BUSY;
	k_StepGo=1;
	k_NextHalfDot = 0;
    // 启动打印
    s_StartMotor();
    while (k_PrnStatus == PRN_BUSY);
	print_motor_time_rec();

	EnableCharge();
    return k_PrnStatus;
}

uchar s_PrnStatus_PRT(void)
{
    volatile int PrnTemp,paperstatus0,paperstatus1,i;

    switch(k_PrnStatus)
    {
        case    PRN_BUSY:           break;
        case    PRN_WRONG_PACKAGE:  break;
        case    PRN_NOFONTLIB:      break;
        case    PRN_OUTOFMEMORY:    break;
        default:
            k_PrnStatus = PRN_BUSY;
			DISABLE_STB();
			DelayMs(1);
			ENABLE_PRN_PWR();
			DelayMs(2);
            PrnTemp = s_GetPrnTemp_PRT();

            paperstatus0 = s_CheckPaper();
            DelayMs(3);
            for(i=0;i<10;i++)
            {
                paperstatus1 = s_CheckPaper();
                if(paperstatus1==paperstatus0)break;
                paperstatus0 = paperstatus1;
                DelayMs(3);
            }

            DISABLE_PRN_PWR();
			DelayMs(1);
            SET_MOTOR_IDLE();
           // if(PrnTemp >= TEMPERATURE_ALLOW)
            if(PrnTemp >= ptPrnDrvCfg->tempStart)
            {
                k_PrnStatus = PRN_TOOHEAT;
            }
            else
            {
                k_PrnStatus = paperstatus0;
            }
            break;
    }

	if (k_PrnStatus == PRN_OK)//检测字库文件是否存在
	{
		if(s_SearchFile(FONT_NAME_S80, (uchar *)"\xff\x02") < 0)
		{
			return PRN_NOFONTLIB;
		}
	}

    return k_PrnStatus;
}

void s_PrnStop_PRT(void)
{
    s_HeatTimerStop();
    s_MotorTimerStop();
    DISABLE_PRN_LAT();
    DISABLE_STB();
    DISABLE_PRN_PWR();
}

T_PrnDrvCfg SxxxPrnCfg = {
	.lat = {GPIOE, 9, 0},
	.stb = {GPIOD, 1, 0},
	.power = {GPIOE, 3, 1},
	.motor = {GPIOE, 0xf0, 0},
	.mask = PRN_MASK_DOT_ADJUST|PRN_MASK_BLACK_ADJUST,
	.cal_block_time = get_block_time_common,
	.tempForbiden = TEMPERATURE_FORBID,
	.tempStart = 65,
	.maxVoltage = 10600,
};

T_PrnDrvCfg S500PrnCfg = {
	.lat = {GPIOE, 9, 0},
	.stb = {GPIOE, 8, 1},
	.power = {GPIOE, 3, 0},
	.motor = {GPIOE, 0xf0, 0},
	.mask = PRN_MASK_DOT_ADJUST|PRN_MASK_BLACK_ADJUST|PRN_MASK_MOTOR_DIR_REVERT|PRN_MASK_REST_ADJUST|PRN_MASK_TEMP_ADC_REVERT,
	.cal_block_time = get_block_time_common,
	.tempForbiden = TEMPERATURE_FORBID,
	.tempStart = 65,
	.maxVoltage = 10600,
};

T_PrnDrvCfg D210PrnCfg = {
	.lat = {GPIOE, 9, 0},
	.stb = {GPIOD, 1, 1},
	.power = {GPIOE, 3, 1},
	.motor = {GPIOE, 0xf0, 0},
	.mask = PRN_MASK_DOT_ADJUST|PRN_MASK_BLACK_ADJUST|PRN_MASK_MOTOR_DIR_REVERT|PRN_MASK_HEAT_TWINCE,
	.cal_block_time = get_block_time_D210,
	.tempForbiden = 75,
	.tempStart = 65,
	.maxVoltage = 10300,
	//.tempStart = 75,
};

