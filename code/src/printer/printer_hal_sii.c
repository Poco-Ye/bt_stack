#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "base.h"
#include "posapi.h"
#include "printer.h"

static void s_MotorTimer(void);
static void s_HeatTimer(void);


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
#define CFG_PRN_LAT()		gpio_set_pin_type(GPIOE,9,GPIO_OUTPUT)	
#define ENABLE_PRN_LAT()	gpio_set_pin_val(GPIOE,9,0)
#define DISABLE_PRN_LAT()	gpio_set_pin_val(GPIOE,9,1)

//电源配置
#define CFG_PRN_PWR()		gpio_set_pin_type(GPIOE,3,GPIO_OUTPUT)	
#define ENABLE_PRN_PWR()	gpio_set_pin_val(GPIOE,3,1)
#define DISABLE_PRN_PWR()	gpio_set_pin_val(GPIOE,3,0)

/*Motor HAL*/
#define CLR_MTA(v)	v&= ~1;
#define SET_MTA(v)	v|=  1;
#define CLR_MTNA(v)	v&= ~2;
#define SET_MTNA(v)	v|=  2;
#define CLR_MTB(v)	v&= ~4;
#define SET_MTB(v)	v|=  4;
#define CLR_MTNB(v)	v&= ~8;
#define SET_MTNB(v)	v|=  8;
#define DRV_MOTOR(v) gpio_set_mpin_val(GPIOE,(0x0f<<4),(v<<4))
#define CFG_MOTOR()	gpio_set_pin_type(GPIOE,4,GPIO_OUTPUT);	\
				gpio_set_pin_type(GPIOE,5,GPIO_OUTPUT);	\
				gpio_set_pin_type(GPIOE,6,GPIO_OUTPUT);	\
				gpio_set_pin_type(GPIOE,7,GPIO_OUTPUT)
//低功耗口状态配置
#define SET_MOTOR_IDLE()	gpio_set_mpin_val(GPIOE,0x0f0,0);

/*SPI HAL*/
#define CFG_PRN_SPI() \
		gpio_set_pin_type(GPIOC,28,GPIO_FUNC1); \
		gpio_set_pin_type(GPIOC,26,GPIO_FUNC1);	\
        spi_config(3,5000000,8,0)

#define SEND_DATA(buf,len)   spi_tx(3,buf,len,8)
#define LATCH_DATA() \
        ENABLE_PRN_LAT(); \
    	DelayUs(1);			/* >min (120ns) */ \
    	DISABLE_PRN_LAT()

#define SEND_AND_LATCH_DATA(buf,len) \
    DISABLE_PRN_LAT();  \
    spi_tx(3,buf,len,8); \
    while(spi_txcheck(3)); \
    ENABLE_PRN_LAT(); \
    DelayUs(1);//for (i = 0; i < 100; i++) ; \
    DISABLE_PRN_LAT()

/*HEAT HAL*/
static void s_PrnSTB(unsigned int pulseUs)
{
    if(pulseUs)
    {
        pwm_config(PWM1, 0xfffe, 0, 0);        
        if(pulseUs<3000)pwm_duty(PWM1, pulseUs);
        else pwm_duty(PWM1,3000);
        pwm_enable(PWM1, 1);
    }
    else
    {
        pwm_enable(PWM1, 0);
    	gpio_set_pin_type(GPIOD,1, GPIO_OUTPUT);
    	gpio_set_pin_val(GPIOD,1, 1);
    }
}

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

/* accelerometer for SII setp motor */
static unsigned short k_PreStepTimePRT_SII[] = { /* us */
	4654, 4654, 2876, 2221, 1865, 1635, 1471, 1348, 1251, 1171,
	1105, 1049, 1001,  958,  921,  887,  857,  830,  805,  782,
	 761,  742,  724,  707,  692,  677,  663,  650,  638,  627,
	 616,  605,  595,  586,  577,  568,  560,  552,  545,  537,
	 530,  524,  517,  511,  505,  499,  494,  488,  483,  478,
	 473,  468,  463,  459,  455,  450,  446,  442,  438,  434,
	 431,  427,  423,  420,  417
};

/*thermel resistor and temperature convert table */
static const int TP2RES_SII[] = {
	316971, 298074, 280440, 263974, 248592, 234215, 220771, 208194, 196421, 185397,
	175068, 165386, 156307, 147789, 139794, 132286, 125233, 118604, 112371, 106508,
	100990,  95796,  90903,  86293,  81948,  77851,  73985,  70337,  66893,  63640,
	 60567,  57662,  54916,  52318,  49860,  47533,  45330,  43243,  41266,  39391,

	 37614,  35928,  34329,  32810,  31369,  29999,  28699,  27462,  26287,  25169,
	 24106,  23094,  22131,  21213,  20340,  19508,  18715,  17959,  17238,  16550,
	 15894,  15268,  14670,  14099,  13554,  13033,  12536,  12060,  11605,  11170,
	 10753,  10355,   9974,   9609,   9259,   8924,   8604,   8296,   8002,   7719,

	  7448,   7188,   6939,   6700,   6470,   6249,   6038,   5834,   5639,   5451,
	  5270,   5097,   4930,   4770,   4615,   4467,   4324,   4186,   4054,   3926,
	  3803,   3685,   3571,   3461,   3355,   3253,   3154,   3059,   2968,   2879,
	  2794,   2712,   2632,   2556,   2482,   2410,   2341,   2274,   2210,   2147,
};

/*2.打印机配置*/
				
//adc配置
#define ADC_PAPER_CHANNEL       0//缺纸通道
#define ADC_TEMP_CHANNEL        1//温度控制ADC通道	
#define ADC_POWER_VOL_CHANNEL   2//电压检测通道
#define VAL_CHECK_PAPER        90//检测缺纸阈值


/* over heat protection level1 */
#define TEMPERATURE_LEVEL1	65

/* over heat protection level2 */
#define TEMPERATURE_LEVEL2	85

//加热头中断耗时 它包括SPI通许耗时,这个时间建议测试出来
#define HEAD_INT_TIME 80

// 一行最多分次加热块数
#define MAX_HEAT_BLOCK      (384/48)   

#define BLACK_HEADER_DOT    48//调整后打印黑块时最大加热点
#define SEL_HEADER_DOT      48// 每次最多加热点数
#define HEAT_TIME           450//单位微秒
#define HEAT_TIME_MAX       1000//单次加热最大时间，单位微秒

//打印黑块时自动调整
#define BLACK_ADJ
//连续4行黑块
#define MAX_BLACK_LINE 4

/*

3.控制变量

*/
static int k_PreStepNum;//预走纸 step数目
static int k_StepTimeTblSize;//motor step 时间表数目
static unsigned short * k_StepTimeTbl;//motor step时间表

#define MAX_FEEDPAPER_STEP     (MAX_DOT_LINE*2 + 1000)

static void make_dot_tbl(void);
static volatile uchar              k_Heating;          // 正在加热状态
static volatile uchar              k_PaperLack;        // 缺纸标志
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

static HeatUnit k_HeatUnit[2];
//一个是加热指针，一个是运算指针
static HeatUnit *uhptr,*chptr;
static unsigned short k_HeatBlock;		//定义当前的加热块

//用以与打印机缓冲与加热单元之间过渡
static  uchar k_next_SendDot[MAX_HEAT_BLOCK][48];
static uchar k_next_block; //下一行的点
//For motor control
static volatile int k_FeedCounter;
static volatile int k_StepIntCnt;

static unsigned int k_pulse_width;//点行间加热间隔，单位微秒
static unsigned int k_block_time,k_step_time;
static unsigned int k_step_num,k_prestep_num;
static int k_upper_halfline,k_under_halfline;//上半个点行,下半个点行
static int k_print_flag,k_heatpause_flag;//打印机标志，分割加热标志
#define K_PAUSE_TIME 800 //分割加热时的暂停时间，单位微秒
static int k_last_block_dotnum, k_max_step;


static void Thermal_TypeSel(uchar tp)
{
	k_StepTimeTbl = k_PreStepTimePRT_SII;
	k_StepTimeTblSize=sizeof(k_PreStepTimePRT_SII) / sizeof(k_PreStepTimePRT_SII[0]);	

	k_PreStepNum = 20;//预走纸step数目
}

// check the paper ,return PRN_PAPEROUT = no paper, PRN_OK = ok
static int s_CheckPaper(void)
{
	int nValue;

	nValue = s_ReadAdc(ADC_PAPER_CHANNEL);
	if(nValue<VAL_CHECK_PAPER) return PRN_OK;

	return PRN_PAPEROUT;
}

//返回毫伏值
static int s_ReadPrnVol(void)
{
    int vol_value;

	vol_value = s_ReadAdcLatest(ADC_POWER_VOL_CHANNEL);
	return ((vol_value*1200*11)/1024);			/*Vp ,voltage,mv*/	
}

// 计算温度并返回温度值
int s_GetPrnTemp_SII(void)
{
	int tmp_value, tmp_power, thermistor, count, tmp_real, i;
    
	tmp_value = s_ReadAdcLatest(ADC_TEMP_CHANNEL);
	tmp_power = tmp_value *1200/1024;				/*Temp, voltage,mv*/	
	thermistor = (tmp_power *10000)/(1200 - tmp_power);	/*get the thermistor*/	

    count = sizeof(TP2RES_SII)/sizeof(TP2RES_SII[0]) - 1;
	for (i = 0; i < count; i++) 
		if ( thermistor >= TP2RES_SII[i])
            break;

	if (i&& (thermistor - TP2RES_SII[i])>(TP2RES_SII[i-1] - thermistor))
		i--;

	return (i - 20);
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

/*Motor HAL API*/
static char		k_StepGo;
static volatile int  k_MotorStep;
static unsigned char b_outio=0;

static void s_MotorSetStep(char step)
{
    k_StepGo=step;
}

static void s_MotorStep(void)
{
	k_MotorStep += k_StepGo;
	k_MotorStep &= 7;
	switch (k_MotorStep) 
    {
	case 0://start step
		SET_MTA(b_outio);
		SET_MTB(b_outio);
		break;
	case 1:
		CLR_MTA(b_outio);
		SET_MTB(b_outio);
		break;
	case 2:
		SET_MTNA(b_outio);
		SET_MTB(b_outio);
		break;
	case 3:
		SET_MTNA(b_outio);
		CLR_MTB(b_outio);
		break;
	case 4:
		SET_MTNA(b_outio);
		SET_MTNB(b_outio);
		break;
	case 5:
		CLR_MTNA(b_outio);
		SET_MTNB(b_outio);
		break;
	case 6:
		SET_MTA(b_outio);
		SET_MTNB(b_outio);
		break;
	case 7:
		SET_MTA(b_outio);
		CLR_MTNB(b_outio);
		break;
	}
    DRV_MOTOR(b_outio);
}

static void s_ClearLatchData(void)
{
	uchar EmptyBuf[48];
    memset(EmptyBuf, 0, sizeof(EmptyBuf));
    SEND_AND_LATCH_DATA(EmptyBuf,48);
}

static void s_StopPrn(uchar status)
{
    int     i;
    //Stop timer
    s_HeatTimerStop();
    s_MotorTimerStop();
    DISABLE_PRN_LAT();
    s_PrnSTB(0);
    s_PrnPower(0);
	s_ClearLatchData();
	SET_MOTOR_IDLE();
    
	k_Heating   = 0;
    k_PrnStatus = status;
    ScrSetIcon(ICON_PRINTER,0);
}

void s_PrnHalInit_SII(uchar prnType)
{
	//关打印机电源
	CFG_PRN_PWR();
	DISABLE_PRN_PWR();

    Thermal_TypeSel(prnType);

    // 配置Strobe信号
    s_PrnSTB(0);

	//数据送出关闭
	CFG_PRN_LAT();
    DISABLE_PRN_LAT();

	//打印机相位配置
	CFG_MOTOR();

	//SPI配置
    CFG_PRN_SPI();

	//清除打印头数据,开机时其打印数据是随机数
	s_ClearLatchData();

	//低功耗口配置
	SET_MOTOR_IDLE();


    s_MotorTimerInit();
    s_HeatTimerInit();

    k_MotorStep    = 0;
    k_Heating      = 0;
    k_PaperLack    = 0;
	make_dot_tbl();
	s_MotorSetStep(1);	

}


static volatile unsigned int Point_T0=0,Point_T1=0;
//设置时间起始点
static unsigned int s_SetTimeOriginPoint(void)
{
    Point_T0 = GetUsClock();
}
//获取两点时间长并重新设置时间点
static unsigned int s_GetTimeDistAndSetPoint(void)
{
    unsigned int time;
    
    Point_T1 = GetUsClock();
    if(Point_T1>=Point_T0)
    {
        time = Point_T1-Point_T0;
    }
    else//微秒计数溢出
    {
        time = 0xffffffff-Point_T0 + Point_T1;
    }
    Point_T0 = Point_T1;

    return time;
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

/* get the moto step number */
static int s_GetStepNum(int usec)
{
	int step_num; 
    
	if (usec <k_StepTimeTbl[k_StepTimeTblSize-1])
		usec =k_StepTimeTbl[k_StepTimeTblSize-1];
	for (step_num = 1; step_num <k_StepTimeTblSize; step_num++) 
		if (usec >= k_StepTimeTbl[step_num])
			break;

	if (usec >k_StepTimeTbl[step_num])
		return (step_num-1);
	else 
		return step_num;
}

#ifdef BLACK_ADJ
static short k_blkln=0;
#endif 
static unsigned short split_block(unsigned char blk[MAX_HEAT_BLOCK][48],unsigned char *tPtr)
{
	unsigned short jj,dn,bn,bt;
	unsigned char tch,nch,tb;
	unsigned short max_dot;
	unsigned short lndot;

	max_dot=SEL_HEADER_DOT;
#ifdef BLACK_ADJ
	if(k_blkln>=MAX_BLACK_LINE)
	{
		k_blkln=MAX_BLACK_LINE;
		max_dot=BLACK_HEADER_DOT;
	}
	lndot=0;
#endif
	memset(blk,0,48*MAX_HEAT_BLOCK);
	for(jj=bn=dn=0;jj<48;jj++)
	{
		tch=*tPtr++;
		if(tch==0) continue;

#ifdef BLACK_ADJ
    	lndot+=s_kdotnum[tch];
#endif

    	blk[bn][jj]=tch;
    	dn+=s_kdotnum[tch];
    	if(dn>=max_dot)//数得最大加热点数则换另一加热块
    	{
    		bn++;				
    		dn=0;
    	}
    }
	if(dn) 
	{
        bn++;//用以最后一次的数据不够 最大点数
        k_last_block_dotnum=dn;
	}
    else 
        k_last_block_dotnum=SEL_HEADER_DOT;
#ifdef BLACK_ADJ
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
#endif
	return bn;
}

/* get the moto step number 电压现在走步*/
static int s_CalcMaxStep(int volt)
{
/*
Pm = Vp*266.7 + 133.3(pps)
Pm:Maximum drive pulse rate at Vp(pps)
Vp:Motor driver voltage(V)
*/
    
	int pps,step_time;
    
	pps = (volt*2667+1333000) / 10000;
	step_time = 1000000/pps;
	if(step_time <k_StepTimeTbl[k_StepTimeTblSize-1])
		step_time = k_StepTimeTbl[k_StepTimeTblSize-1];

	return s_GetStepNum(step_time);
}

static unsigned int s_CalcTime(int dotnum,int width)
{
/*
    T(ms) = E * R * C / (V * V)
	t = 1000*1000*T;
	t(ns) = (100000E * 10000R * 1000C)/(1000V * 1000V)

	t:Thermal head activation pulse width(ms)
	E:Printing energy(mj)
	R:Adjusted resistance(OM)
	C:Thermal head activation pluse cycle coefficient
 */

	unsigned long e, r, c, v;
	int volt,temp;

    DelayUs(10);//电压稳定时间
	temp = s_GetPrnTemp_SII();/*get the convert real temperature*/
	volt = s_ReadPrnVol();/*Vp ,voltage,mv*/	
    
    if(chptr)chptr->Temperature = temp;
	if(get_machine_type()==S900)
		k_max_step = s_CalcMaxStep(volt);

    if(k_GrayLevel<100)
    {
        if(uhptr->DataBlock<5)
        {
            e = (23040) - 293 * ( temp*8/10 - 25);
        }
        else
        {
            e = (23040*80/100) - 293 * ( temp*8/10 - 25);
        }
    }
    else if(k_GrayLevel<200)
    {
		if(uhptr->DataBlock<5)
			e = (23040*(k_GrayLevel+k_GrayLevel/2)/100) - 293 * ( temp - 25);		/* 150% energy */
		else 
		{
			e = (23040) - 293 * ( temp*8/10 - 25);		/* 100% energy */
		}
    }
    else if(k_GrayLevel<300)
        e = (23040*2) - 293 * ( temp - 25);
    else if(k_GrayLevel<400)
        e = (23040*250/100) - 293 * ( temp - 25);
    else 
        e = (23040*300/100) - 293 * ( temp - 25);
	
	r = 180000 + 9000 + (256 + 0) * dotnum;
	r /= 10;
	r = (r * r) / 180;							/* 220000 */
	v = 1118 * volt / 1000 - 1231;		/* 8272 */
	c = 1818 * volt - 5261 * 1000;
	c /= (volt - 3780) * (width + 1917)/1000 + 2016;
	c = 1000 - c;

	return ((r/v*e/v*c)/1000);					/*return us*/
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
	return hdptr;
}

//Timer for heating
static void s_HeatTimer(void)
{
	s_HeatTimerStop();
	s_PrnSTB(0);
    if(!k_heatpause_flag)
    {   
	    k_HeatBlock++;
	    if (k_HeatBlock < uhptr->DataBlock) 
        {
            LATCH_DATA();
    		if(k_HeatBlock == uhptr->DataBlock-1)	/* this fun consume 21us */
    		{
    			k_block_time = s_CalcTime(k_last_block_dotnum, k_pulse_width);	
    		}
    		else
    		{
    			k_block_time = s_CalcTime(SEL_HEADER_DOT, k_pulse_width);
    		}
    		if(k_GrayLevel >=300)
            {
    			s_PrnSTB(k_block_time/2);
    			k_heatpause_flag = 1;
    			s_HeatTimerStart(K_PAUSE_TIME+k_block_time/2); 
    		}
    		else
            {
    			s_PrnSTB(k_block_time);
    			s_HeatTimerStart(k_block_time);		
    		}	

    	    if((k_HeatBlock+1)<uhptr->DataBlock)				 /* this fun consume 80us */
    		    SEND_DATA(uhptr->SendDot[k_HeatBlock+1],48);	 /* send next block data while heat last block */
	    }
	    else 
        {
    		k_HeatBlock = 0;
    		if(k_upper_halfline)
    			SEND_DATA(uhptr->SendDot[0],48);
    		if(k_under_halfline)
    			SEND_DATA(uhptr->NxtLiFstDot,48);
    		k_Heating = 0;
    	}
    }
    else	
    {
    	s_PrnSTB(k_block_time/2);
    	s_HeatTimerStart(k_block_time/2);
    	k_heatpause_flag = 0;
    }	
}

//马达驱动的中断服务程序
static void s_MotorTimer(void)
{
	static unsigned char icon_on;
	static T_SOFTTIMER icon_tm;

    s_MotorTimerStop();

	if(s_TimerCheckMs(&icon_tm) == 0)
	{
		if(k_FeedCounter < 3) icon_on=1;
		ScrSetIcon(ICON_PRINTER,icon_on++&1);
		s_TimerSetMs(&icon_tm,500);
	}
    
    k_FeedCounter++;

    if((k_FeedCounter >= 2*MAX_FEEDPAPER_STEP))
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

    if (k_FeedCounter < k_PreStepNum)
    {
		s_MotorStep();
        k_step_time = k_StepTimeTbl[k_FeedCounter];
		s_MotorTimerStart(k_step_time);
		if(k_FeedCounter == k_PreStepNum-1)
        {
			chptr=s_LoadData(chptr);				/*get the first dot line data*/
			SEND_DATA(chptr->SendDot[0],48);		/* send first dot line ,first block data */
			k_prestep_num = s_GetStepNum(k_step_time);
		}
		return;
	}
    
    switch (k_StepIntCnt)
    {
    HeatUnit *tp;
    case 0:
        if(k_Heating==1)
        {
            s_MotorTimerStart(50);
            break;
        }
        s_PrnSTB(0);     // stop heating            
        if(chptr==NULL)
        {
            s_StopPrn(PRN_OK);
            return;
        }
        //  判断当前缓冲采样的温度是否过温,如果过温则退出
        if(chptr->Temperature >= TEMPERATURE_LEVEL2)  // 超过允许打印上限
        {
            s_StopPrn(PRN_TOOHEAT);
            return;
        }

        s_MotorStep();
		k_StepIntCnt++;		
		tp = chptr;
		chptr = uhptr;
		uhptr = tp;
		k_HeatBlock = 0;
		k_Heating = 0;
		k_upper_halfline = 1;
		k_under_halfline = 0;		
        if (uhptr->DataBlock)
        {
			k_Heating = 1;
			k_print_flag =1;
            LATCH_DATA();
			/* thermal head activation pulse width */
			k_pulse_width = s_GetTimeDistAndSetPoint();
            
			if(uhptr->DataBlock==1)	
				k_block_time = s_CalcTime(k_last_block_dotnum, 
				                    k_pulse_width);
			else 
            {
				if((get_machine_type()==S900)&&(uhptr->DataBlock>4))
				{
                    if(is_ExPower())k_block_time = s_CalcTime(SEL_HEADER_DOT*3/2, k_pulse_width);
                    else k_block_time = s_CalcTime(SEL_HEADER_DOT*2, k_pulse_width);
				}
				else 
				{
					k_block_time = s_CalcTime(SEL_HEADER_DOT, k_pulse_width);
				}
			}
            
			if(k_GrayLevel >=300)
            {               
				s_PrnSTB(k_block_time/2);		/* open heating */
				k_heatpause_flag = 1;
				s_HeatTimerStart(K_PAUSE_TIME+k_block_time/2);   /* set heat timer */	
				k_step_time = ((k_block_time+HEAD_INT_TIME+K_PAUSE_TIME)*uhptr->DataBlock+100)/2;
			}
		    else
            {
    			s_PrnSTB(k_block_time);						/* open heating */
	    		s_HeatTimerStart(k_block_time); /* set heat timer */	
		    	k_step_time = ((k_block_time+HEAD_INT_TIME)*uhptr->DataBlock+100)/2;
		    }
            
		    k_step_num = s_GetStepNum(k_step_time);		/* get the step num */
		    if(k_step_num >k_max_step)					/*limit max pps for low volt*/	
			    k_step_num = k_max_step;
		    if(k_step_num>k_prestep_num)/* compare step_num with prestep_num */
            {               
			    k_step_num = k_prestep_num+1;
			    k_prestep_num =k_step_num;
		    }
		    else k_prestep_num = k_step_num ;		
		    if(k_step_time>k_StepTimeTbl[0])s_MotorTimerStart(k_step_time);
		    else s_MotorTimerStart(k_StepTimeTbl[k_step_num]);			
		    if(uhptr->DataBlock>1)		/* spi send next block data while heating first block*/
			    SEND_DATA(uhptr->SendDot[1],48);	/* this fun consume 80us */
	    }
        else
        {/* accelerate in blank dot line 0 step */
    		if(k_print_flag)
            {
    			if(k_prestep_num<64)k_prestep_num++;
    			if(get_machine_type()==S900)
    				k_max_step = s_CalcMaxStep(s_ReadPrnVol());	
    			if(k_prestep_num >k_max_step)	/*limit max pps for low volt*/
    				k_prestep_num = k_max_step;
    		}
    		s_MotorTimerStart(k_StepTimeTbl[k_prestep_num]);
    		SEND_DATA(uhptr->NxtLiFstDot,48);	/* send next dot line data */
    	}
        break;
		
	case 1:
        s_MotorStep();
	    k_StepIntCnt++;
		if (uhptr->DataBlock) 
        {
			if(k_step_time>k_StepTimeTbl[0])
				s_MotorTimerStart(k_step_time);
			else
				s_MotorTimerStart(k_StepTimeTbl[k_step_num]);
		}		
		else/* accelerate in blank dot line 1 step */
        {
			if(k_print_flag)
            {
				if(k_prestep_num<64)k_prestep_num++;
				if(k_prestep_num >k_max_step)	/*limit max pps for low volt*/
					k_prestep_num = k_max_step;
			}
			s_MotorTimerStart(k_StepTimeTbl[k_prestep_num]);
		}
	    break;
	case 2:
        if (k_Heating == 1) 
        {
			s_MotorTimerStart(50);
			return;			
		}
		s_PrnSTB(0);				/* stop heating */
		s_MotorStep();						/* motot step */
		k_StepIntCnt++;		
		k_HeatBlock = 0;
		k_Heating = 0;
		k_upper_halfline = 0;
		k_under_halfline = 1;
		if (uhptr->DataBlock) 
        {
			k_Heating = 1;
            LATCH_DATA();
			k_pulse_width = s_GetTimeDistAndSetPoint();
			if(uhptr->DataBlock==1)	
				k_block_time = s_CalcTime(k_last_block_dotnum, k_pulse_width);
			else
            {
				if((get_machine_type()==S900)&&(uhptr->DataBlock>4))
				{
                    if(is_ExPower())k_block_time = s_CalcTime(SEL_HEADER_DOT*3/2, k_pulse_width);
                    else k_block_time = s_CalcTime(SEL_HEADER_DOT*2, k_pulse_width);
				}
				else 
				{
					k_block_time = s_CalcTime(SEL_HEADER_DOT, k_pulse_width);
				}
			}
            
			if(k_GrayLevel >=300)
            {
                s_PrnSTB(k_block_time/2);					/* open heating */
				k_heatpause_flag = 1;
				s_HeatTimerStart(K_PAUSE_TIME+k_block_time/2);   /* set heat timer */
				k_step_time = ((k_block_time+HEAD_INT_TIME+K_PAUSE_TIME)*uhptr->DataBlock+100)/2;
			}
			else
            {
				s_PrnSTB(k_block_time);						/* open heating */
				s_HeatTimerStart(k_block_time);	/* set heat timer */	
				k_step_time = ((k_block_time+HEAD_INT_TIME)*uhptr->DataBlock+100)/2;
			}

            k_step_num = s_GetStepNum(k_step_time);		/* get the step num */
			if(k_step_num >k_max_step)					/*limit max pps for low volt*/
				k_step_num = k_max_step;
			if(k_step_num>k_prestep_num)				/* now step is faster than pre step */
            {
				k_step_num = k_prestep_num+1;
				k_prestep_num =k_step_num;
			}
			else k_prestep_num = k_step_num ;	
			if(k_step_time>k_StepTimeTbl[0])
				s_MotorTimerStart(k_step_time);
			else
				s_MotorTimerStart(k_StepTimeTbl[k_step_num]);			
			if(uhptr->DataBlock>1)				/* spi send next block data while heating first block*/	
				SEND_DATA(uhptr->SendDot[1],48);	/* this fun consume 80us */
		}
	    else
        {   /* accelerate in blank dot line 2 step */
		    if(k_print_flag)
            {
    			if(k_prestep_num<64)k_prestep_num++;
    			if(k_prestep_num >k_max_step)	/*limit max pps for low volt*/
    				k_prestep_num = k_max_step;
		    }
		    s_MotorTimerStart(k_StepTimeTbl[k_prestep_num]);
	    }
	    break;					

    default:
        s_MotorStep();
	    k_StepIntCnt = 0;
		if (uhptr->DataBlock) 
        {
			if(k_step_time>k_StepTimeTbl[0])
				s_MotorTimerStart(k_step_time);
			else			
				s_MotorTimerStart(k_StepTimeTbl[k_step_num]);
		}		
		else/* accelerate in blank dot line 3 step */
        {
			if(k_print_flag)
            {
				if(k_prestep_num<64)
					k_prestep_num++;
				if(k_prestep_num >k_max_step)	/*limit max pps for low volt*/
					k_prestep_num = k_max_step;
			}
			s_MotorTimerStart(k_StepTimeTbl[k_prestep_num]);
		}
		chptr=s_LoadData(chptr);				/*get next dot line data */	
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
	
	s_PrnSTB(0);
	DISABLE_PRN_LAT();
    DelayMs(2);
    // 初始化
    memset(k_HeatUnit,0x00,sizeof(k_HeatUnit));
	chptr=&k_HeatUnit[0];
	uhptr=&k_HeatUnit[1];

    
	//把一行数据分好加热块
	if(k_CurDotLine) k_next_block=split_block(k_next_SendDot,k_DotBuf[0]);
#ifdef BLACK_ADJ 
	k_blkln=0;// 刚刚启动不会打黑块，还有预走纸
#endif

    // 送空数据启动Latch
    s_ClearLatchData();
    
    // 步进控制
	k_upper_halfline = 0;
	k_under_halfline = 0;
	k_pulse_width = 1<<20;
	k_print_flag = 0;
	k_heatpause_flag = 0;
	k_max_step = 64;

    s_MotorTimerStart(k_StepTimeTbl[0]);         
    
    ScrSetIcon(ICON_PRINTER,1);
}


uchar s_PrnStart_SII(void)
{
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

	if(s_ReadPrnVol() >= 10600)
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

    s_PrnSTB(0);
	ENABLE_PRN_PWR();
	DelayMs(10);
//先检测是否过热再检测缺纸
	if(s_GetPrnTemp_SII() >= TEMPERATURE_LEVEL1)      
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

    k_PrnStatus = PRN_BUSY;
    s_SetTimeOriginPoint();
    // 启动打印
    s_StartMotor();
    while (k_PrnStatus == PRN_BUSY);
    return k_PrnStatus;
}

uchar s_PrnStatus_SII(void)
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
			s_PrnSTB(0);
			DelayMs(1);
			ENABLE_PRN_PWR();
			DelayMs(2);
            PrnTemp = s_GetPrnTemp_SII();

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
            if(PrnTemp >= TEMPERATURE_LEVEL1)
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

void s_PrnStop_SII(void)
{
    s_HeatTimerStop();
    s_MotorTimerStop();
    DISABLE_PRN_LAT();
    s_PrnSTB(0);
//    s_PrnSPI(0);
    DISABLE_PRN_PWR();
}

