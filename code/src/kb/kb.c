#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "Base.h"
#include "Kb.h"
#include "kb_touch.h"
#include "posapi.h"
#include "..\lcd\lcdapi.h"


//for Sxxx
//非组合键的键码表
static  uchar Key_Tab_Sxxx[]={
	KEY1,			KEY4,		KEY7,		KEYFN,      KEYCANCEL,
    KEY2,           KEY5,       KEY8,       KEY0,	    KEYCLEAR,
    KEY3,		    KEY6,       KEY9,		KEYALPHA,	0,
    KEYATM1,	    KEYUP, 		KEYDOWN,	KEYMENU,	KEYENTER,
    NOKEY,	        NOKEY, 		NOKEY,	    NOKEY,	    NOKEY,
};
//组合按键的键码表
static  uchar FKey_Tab_Sxxx[]={ 
	FNKEY1,			FNKEY4,		FNKEY7,		0,	        FNKEYCANCEL,
    FNKEY2,         FNKEY5,     FNKEY8,     FNKEY0,	    FNKEYCLEAR,
    FNKEY3,		    FNKEY6,	    FNKEY9,	    FNKEYALPHA,	0,
    FNKEYATM1,      FNKEYUP,	FNKEYDOWN,	FNKEYMENU,	FNKEYENTER,
    NOKEY,	        NOKEY, 		NOKEY,	    NOKEY,	    NOKEY,
};
//组合按键的键码表
static  uchar EKey_Tab_Sxxx[]={ 
    //ENTER
    ENKEY1,			ENKEY4,		ENKEY7,		FNKEYENTER,	ENKEYCANCEL,
    ENKEY2,         ENKEY5,     ENKEY8,     ENKEY0,	    ENKEYCLEAR,
    ENKEY3,		    ENKEY6,	    ENKEY9,	    ENKEYALPHA,	0,
    ENKEYATM1,      ENKEYUP,	ENKEYDOWN,	ENKEYMENU,	0,//KEYENTER
    NOKEY,	        NOKEY, 		NOKEY,	    NOKEY,	    NOKEY,
};

//for Sxx
//非组合键的键码表
static  uchar Key_Tab_Sxx[]={
	KEY1,			KEY4,		KEY7,		KEY0, 		
	KEY2,			KEY5,		KEY8,		0, 		 
	KEY3,			KEY6,		KEY9,		0,
	KEYCANCEL,		KEYENTER,	KEYCLEAR, 	0,
	KEYATM1, 		KEYATM3,	KEYFN,    		
	KEYATM2,		KEYATM4,	KEYMENU,		
	KEYUP,			KEYDOWN,	KEYALPHA 					
};
//组合按键的键码表
static  uchar FKey_Tab_Sxx[]={ 
	FNKEY1,			FNKEY4,		FNKEY7,		FNKEY0,
	FNKEY2,			FNKEY5,		FNKEY8,		0,      	
	FNKEY3,			FNKEY6,		FNKEY9,		0,
	FNKEYCANCEL,	FNKEYENTER,	FNKEYCLEAR,	0,
	FNKEYATM1,		FNKEYATM3,	0,		
	FNKEYATM2,		FNKEYATM4, 	FNKEYMENU,		
	FNKEYUP,		FNKEYDOWN,	FNKEYALPHA							
};

//组合按键的键码表
static  uchar EKey_Tab_Sxx[]={ 
	ENKEY1,			ENKEY4,		ENKEY7,		ENKEY0,
	ENKEY2,			ENKEY5,		ENKEY8,		0,      	
	ENKEY3,			ENKEY6,		ENKEY9,		0,
	ENKEYCANCEL,	0,	        ENKEYCLEAR,	0,
	ENKEYATM1,		ENKEYATM3,	0,		
	ENKEYATM2,		ENKEYATM4, 	ENKEYMENU,		
	ENKEYUP,		ENKEYDOWN,	ENKEYALPHA							
};

static void s_PowerKey_Int(void);
static void s_PowerKeyInit(void);

// 增加的组合键
static unsigned int KEYFN_SCAN_BITMAP;		//FN 键对应的按键位图中的位置 在s_Kb_Normal_Init()中初始化	   
//普通按键部分

#define KEYFN_PRESS (1U <<31)
#define KEYENTER_PRESS  (1U <<30)
#define KEY_INVALID_BITMAP        0xFF   //无效键对应的位图

#define WAITFOR_PRESS     0     // 等待按键事件
#define WAITFOR_RELEASE   1     // 等待按键释放事件

#define  KB_BUF_MAX          (32)
volatile uchar    kb_buf[KB_BUF_MAX]; /* 读取键值缓冲区 */
volatile uchar    kb_in_ptr;          /* 读入地址指针   */
volatile static uchar    kb_out_ptr;         /* 读出地址指针   */
volatile static uchar    kb_buf_over;        /* 键值缓冲溢出错误 */
volatile static int   kb_Buffer_Busy;

volatile static int   kb_TimerStep;	   //记录当前扫描定时器运行的阶段	
volatile static int   kb_OffTimerCount;   //记录扫描到nokey的次数
volatile static int   kb_OnTimerCount;//记录扫描到key的次数

static volatile int   kb_keyCombFlag;		/* key combination on/off flag. 0-Off; 1-On. */
static volatile int   kb_EnterkeyCombFlag;		/* key combination on/off flag. 0-Off; 1-On. */
static volatile int   kb_save_keyCombFlag;		/*按键按下到放开期间组合键的配置标志*/
static volatile int   kb_save_enterkeyCombFlag;		/*按键按下到放开期间组合键的配置标志*/

extern const uint BeepFreqTab[8];
	
volatile static int   kb_PressStatus;  //提供给系统休眠时使用

static volatile uchar gucPowerKeyDownFlag = 0;
static volatile uint guiPowerKeyCount = 0;
 volatile unsigned int    k_ScrBackLightTime;
 volatile unsigned int    k_LightTime;
 volatile unsigned int    k_KBLightTime;
 volatile unsigned char   k_KBLightMode;
static uchar kblightctrl = 0; // For D200 touch key backlight control
static uchar kblightstat = 0;

extern  unsigned char    k_ScrBackLightMode;
extern OS_TASK *g_taskpoweroff;


typedef struct {
	int fun_bitmap_num;
	uint key_gpioin_mask;
	uint key_gpioout_mask;
	int key_in_start;
	int key_in_end;
	int key_out_start;
	int key_out_end;
	int atm_in_start;
	int atm_in_end;
	int atm_out_start;
	int atm_out_end;
	uchar *Key_Tab;
	uchar *FKey_Tab;
	uchar *EKey_Tab;
}T_KbDrvConfig;
//for SXXX
T_KbDrvConfig Sxxx_KbDrvConfig = 
{
	.fun_bitmap_num = 3,
	.key_gpioin_mask = (0x00F00000),//bit20~23
	.key_gpioout_mask = (0x000F8000),//bit15~19
	.key_in_start = 20,
	.key_in_end = 23,
	.key_out_start = 15,
	.key_out_end = 19,
	.atm_in_start = 255,
	.atm_in_end = 0,
	.atm_out_start = 0,
	.atm_out_end = 0,
	.Key_Tab = Key_Tab_Sxxx,
	.FKey_Tab = FKey_Tab_Sxxx,
	.EKey_Tab = EKey_Tab_Sxxx,
};
//for SXX
T_KbDrvConfig Sxx_KbDrvConfig = 
{
	.fun_bitmap_num = 18,
	.key_gpioin_mask = (0x07F00000),//bit20~23
	.key_gpioout_mask = (0x380F0000),//bit15~19
	.key_in_start = 20,
	.key_in_end = 23,
	.key_out_start = 16,
	.key_out_end = 19,
	.atm_in_start = 24,
	.atm_in_end = 26,
	.atm_out_start = 27,
	.atm_out_end = 29,
	.Key_Tab = Key_Tab_Sxx,
	.FKey_Tab = FKey_Tab_Sxx,
	.EKey_Tab = EKey_Tab_Sxx,
};
T_KbDrvConfig *KbDrvConfig = NULL;
static  uchar Key_Tab[25];
static uchar FKey_Tab[25];
static uchar EKey_Tab[25];
static void s_Kb_Normal_Init(void);
static void s_KbInit_touchkey(void);
void s_KbInit(void)
{
	int mach = get_machine_type();
	//s_BeepInit();
	if(get_machine_type() == D200)
	{
		kb_i2c_config();
		if (s_GetBatteryEntryFlag() == 0)
		{
			s_PmuInit();
			return;
		}
		s_PowerInit();
		s_KbInit_touchkey();
		return ;
	}

	if (mach == S300 || mach == S800 || mach == S900 || mach == D210)
	{
		KbDrvConfig = &Sxxx_KbDrvConfig;
	}
	else if (mach == S500)
	{
		KbDrvConfig = &Sxx_KbDrvConfig;
	}
	if (KbDrvConfig != NULL)
	{
		memset(Key_Tab , 0, sizeof(Key_Tab));
		memcpy(Key_Tab, KbDrvConfig->Key_Tab, sizeof(Key_Tab));
		memset(FKey_Tab , 0, sizeof(FKey_Tab));
		memcpy(FKey_Tab, KbDrvConfig->FKey_Tab, sizeof(FKey_Tab));
        memset(EKey_Tab , 0, sizeof(EKey_Tab));
        memcpy(EKey_Tab, KbDrvConfig->EKey_Tab, sizeof(EKey_Tab));
    }
	
	/*使能电源*/
	gpio_set_pin_val(POWER_HOLD, 1);
	DelayUs(1);
	gpio_set_pin_type(POWER_HOLD, GPIO_OUTPUT);
	s_Kb_Normal_Init();
	s_StopBeep();
        
	k_LightTime         = 6000;//亮1分钟，自动熄灭
	k_ScrBackLightTime  = k_LightTime;//
	k_KBLightTime       = k_LightTime;//S80没有按键背光
	k_KBLightMode       = 1;
	
	if (mach == S500)
	{
		s_PowerKeyInit();
	}
}


#define FUN_BITMAP_NUM (KbDrvConfig->fun_bitmap_num)//FN key BITMAP所在位置,从电路
									//图第一个键开始数，起始为1
#define KEY_GPIOIN_MASK     (KbDrvConfig->key_gpioin_mask)    //输入口线对应的GPIO	 BITs 
#define KEY_GPIOOUT_MASK    (KbDrvConfig->key_gpioout_mask)    //输出口线对应的GPIO	 BITs 

#define KEY_IN_START  		(KbDrvConfig->key_in_start)       //普通按键，输入口线对应的GPIO的起始编号
#define KEY_IN_END      	(KbDrvConfig->key_in_end)       //普通按键，输入口线对应的GPIO的结束编号
#define KEY_OUT_START 		(KbDrvConfig->key_out_start)       //普通按键，输出口线对应的GPIO的起始编号
#define KEY_OUT_END 		(KbDrvConfig->key_out_end)       //普通按键，输出口线对应的GPIO的结束编号

#define ATM_IN_START  		(KbDrvConfig->atm_in_start)       //ATM键，输入口线对应的GPIO的起始编号
#define ATM_IN_END      	(KbDrvConfig->atm_in_end)       //ATM键，输入口线对应的GPIO的结束编号
#define ATM_OUT_START 		(KbDrvConfig->atm_out_start)       //ATM键，输出口线对应的GPIO的起始编号
#define ATM_OUT_END 		(KbDrvConfig->atm_out_end)       //ATM键，输出口线对应的GPIO的结束编号


static void s_TimerScanKey(void);
static uint ScanKey(int mode);
static void s_Key_Int(void);
extern uchar k_Charging;
//for Sxxx
//用于普通按键的初始化
static void s_Kb_Normal_Init(void)
{
	int i;
	uint temp;
	
    if (k_Charging > 0)		//Joshua _a  don't scan the key while in charge mode
    {
		gpio_set_mpin_type(GPIOB,KEY_GPIOIN_MASK|KEY_GPIOOUT_MASK,GPIO_INPUT);
		for(i=0;i<32;i++) /*config internal pull-up for input ports */
		{
			if((KEY_GPIOIN_MASK|KEY_GPIOOUT_MASK) & (1<<i))
			{
				gpio_set_pull(GPIOB,i, 1);
				gpio_enable_pull(GPIOB,i);
			}
		}
		gpio_set_pin_type(GPIOB, KEY_OUT_END, GPIO_OUTPUT);
		gpio_set_pin_val(GPIOB, KEY_OUT_END, 0);
		gpio_disable_pull(GPIOB, KEY_OUT_END);
    }
	else
	{
		gpio_set_mpin_type(GPIOB,KEY_GPIOIN_MASK,GPIO_INPUT);
		gpio_set_mpin_type(GPIOB,KEY_GPIOOUT_MASK,GPIO_OUTPUT);
		 // Configure the GPIOs as Output Pin, and OutPut 0
		gpio_set_mpin_val(GPIOB,KEY_GPIOOUT_MASK,0);
		for(i=0;i<32;i++) /*config internal pull-up for input ports */
		{
			if((KEY_GPIOIN_MASK & (1<<i)))
			{
				gpio_set_pull(GPIOB,i, 1);
				gpio_enable_pull(GPIOB,i);
			}
	        
			if((KEY_GPIOOUT_MASK & (1<<i)))
			{
				gpio_disable_pull(GPIOB,i);
			}
		}	
	}
	for(i = 0; i < KB_BUF_MAX; i++)
	{
		kb_buf[i] = NOKEY;
	}
    
    KEYFN_SCAN_BITMAP=(1<<(FUN_BITMAP_NUM));
    
	kb_in_ptr = 0;
	kb_out_ptr = 0;
	kb_buf_over = 0;

	kb_PressStatus = WAITFOR_PRESS;
	kb_Buffer_Busy = 0;

	kb_TimerStep = 0;
	kb_OffTimerCount = 0;
	
	kb_keyCombFlag = 0;			//默认不启用组合键
    kb_EnterkeyCombFlag = 0;         //默认不启用组合键

	if (k_Charging > 0)		//Joshua _a  don't scan the key while in charge mode
		return;

	 // Configure the GPIOs as Interrupt Pin
	 for(i=0;i<32;i++)
	 {
	 	if(KEY_GPIOIN_MASK & (1<<i))
	 	{
	 		gpio_set_pin_interrupt(GPIOB,i,0); /*disable sub port interrupt*/
			writel((1<<i),GIO1_R_GRPF1_INT_CLR_MEMADDR);	// Clear Interrupt Pending
			s_setShareIRQHandler(GPIOB,i,INTTYPE_LOWLEVEL,s_Key_Int);
	 	}
	}
   	 
	 // 触发定时器事件，20ms后开始扫描按键
	s_SetTimerEvent(KEY_HANDLER,s_TimerScanKey); //4

    for(i=0;i<32;i++)
	{
	 	if(KEY_GPIOIN_MASK & (1<<i))
	 	{
	 		gpio_set_pin_interrupt(GPIOB,i,1); /*enable sub port interrupt*/
	 	}
	}
}

// 按键中断服务程序
static void s_Key_Int(void)
{
	uint temp,flag;
    
	irq_save(flag);
	temp =readl(GIO1_R_GRPF1_INT_MSK_MEMADDR);
	temp &=	~KEY_GPIOIN_MASK;
	writel(temp,GIO1_R_GRPF1_INT_MSK_MEMADDR);
	irq_restore(flag);

    
	kb_TimerStep = 0;
	kb_OffTimerCount = 0;
    kb_OnTimerCount = 0;
	kb_PressStatus = WAITFOR_RELEASE;
	
	/*
	在此次按键扫描过程中，
	不再接受组合键状态的改变
	*/
	kb_save_keyCombFlag = kb_keyCombFlag;
	kb_save_enterkeyCombFlag = kb_EnterkeyCombFlag;
	// 触发定时器事件，20ms后开始扫描按键
	StartTimerEvent(KEY_HANDLER,2);
}

//键盘初始化时调用该函数
static void s_PowerKeyInit(void)
{
	gucPowerKeyDownFlag = 0;
	guiPowerKeyCount = 0;
	gpio_set_pin_type(POWER_KEY, GPIO_INPUT);
	
	writel((1<<15),GIO1_R_GRPF1_INT_CLR_MEMADDR);	// Clear Interrupt Pending
	s_setShareIRQHandler(POWER_KEY,INTTYPE_FALL,s_PowerKey_Int);
}

void s_EnterKeyCombConfig(int value)
{
    if(value != 0 && value != 1)
        return;

    kb_EnterkeyCombFlag = value;//键盘配置
}

void s_KeyCombConfig(int value)
{
    if(value != 0 && value != 1)
        return;

    kb_keyCombFlag = value;//键盘配置
}

/********************************************************************************
 *函数原型:unsigned int ScanKey(int mode)
 *输入参数:
 *  mode:0表示只是查看是否有键按下，非0则详细扫描按键
 *
 *返回:
 *  mode为0，返回0表示无按键按下，非0表示有按键按下。
 *  mode为1:
 *      1.kb_save_keyCombFlag为0时，不扫描组合键，返回值为按键值，若存在多个按键则返回KEY_INVALID_BITMAP.
 *      2.kb_save_keyCombFlag为1时，扫描组合键状态：
 *          如果除了FN键外多个键按下，则返回KEY_INVALID_BITMAP.
 *          KEY_FN_PRESS位(31位)置位表示FN按键有按下,低8位为其他按键的值
 *********************************************************************************/
static unsigned int ScanKey(int mode)
{
	uint temp1 = 0,temp2=0;
	uint ucRet = 0;
	volatile uint i = 0,j;
	uint invalid_flag=0,combin_flag = 0;
	int keynum=0;
	uint reg;
	
	gpio_set_mpin_val(GPIOB,KEY_GPIOOUT_MASK,0);
	temp1 = gpio_get_mpin_val(GPIOB,KEY_GPIOIN_MASK);
	temp2 = (temp1==KEY_GPIOIN_MASK);

	if(temp2)	return 0;  //no key
	if(!mode)   //仅用于分析是否有键按下
		return temp1;	//表示尚有键未释放		

	//对普通按键扫描
	for(i=KEY_IN_START;i<=KEY_IN_END;i++)  //按行扫描
	{
		gpio_set_mpin_type(GPIOB,KEY_GPIOOUT_MASK,GPIO_INPUT);

		for(j=KEY_OUT_START;j<=KEY_OUT_END;j++)  //在某行按列扫描
		{
			if(j!=KEY_OUT_START) 
			{
				gpio_set_pin_type(GPIOB,j-1,GPIO_INPUT);
			}
			gpio_set_pin_type(GPIOB,j,GPIO_OUTPUT);
			gpio_set_pin_val(GPIOB,j, 0);

			DelayUs(1);
			temp1 = gpio_get_mpin_val(GPIOB,KEY_GPIOIN_MASK);
			if(temp1 & (1<<i))  continue ;
			//扫描到按键，将键码位图存放下来
			ucRet|= 1<<((i-KEY_IN_START)*(KEY_OUT_END-KEY_OUT_START+1)
			            +j-KEY_OUT_START); 
		}		
	}
	//对ATM键扫描
	for(i=ATM_IN_START;i<=ATM_IN_END;i++)  //按行扫描
	{
		gpio_set_mpin_type(GPIOB,KEY_GPIOOUT_MASK,GPIO_INPUT);

		for(j=ATM_OUT_START;j<=ATM_OUT_END;j++)  //在某行按列扫描
		{
			if(j!=ATM_OUT_START) 
			{
				gpio_set_pin_type(GPIOB,j-1,GPIO_INPUT);
			}
			gpio_set_pin_type(GPIOB,j,GPIO_OUTPUT);
			gpio_set_pin_val(GPIOB,j, 0);

			DelayUs(1);
			temp1 = gpio_get_mpin_val(GPIOB,KEY_GPIOIN_MASK);
//printk("sscc temp1:%08x,i:%d,j:%d\r\n",temp1,i,j);
			if(temp1 & (1<<i))  continue ;
//printk("temp1:%08x,i:%d,j:%d\r\n",temp1,i,j);
			 //扫描到按键，将键码位图存放下来
			 temp2 = 1<<16;
			ucRet|= temp2<<((i-ATM_IN_START)*3+j-ATM_OUT_START); 
		}		
	}

	//将输出口线恢复到输出状态，并将所有输出口线电平拉低
	gpio_set_mpin_type(GPIOB,KEY_GPIOOUT_MASK,GPIO_OUTPUT);
	gpio_set_mpin_val(GPIOB,KEY_GPIOOUT_MASK,0);

	//对key bitmap 进行分析并给出键码

    /*只有FN键按下*/
    if((ucRet == KEYFN_SCAN_BITMAP) && (1 == kb_save_keyCombFlag))
        return KEYFN_PRESS;
    else if((ucRet == 0x80000) && (1 == kb_save_enterkeyCombFlag))//enter press
        return KEYENTER_PRESS;
	            
    if((ucRet & KEYFN_SCAN_BITMAP) && (1 == kb_save_keyCombFlag)) 
    {
        combin_flag = 2;
        ucRet &= ~(KEYFN_SCAN_BITMAP);
    }
    else if ((ucRet & 0x80000) && (1 == kb_save_enterkeyCombFlag))//enter press
    {
        combin_flag = 1;
        ucRet &= ~(0x80000);
    }

	for(i=0,j=0;i<32;i++)
	{
		if(ucRet&(1<<i)) 
		{
			j++; //记录按键个数
			keynum=i+1;    //将按键位图中的位置变成编号
		}
		if(j>1) //有两个非FN存在判断为非法按键
		{
			invalid_flag=1;
			break;
		}
	}
//printk("scan ret:%08X,Invaild:%d,combin_flag:%d!\r\n",ucRet,invalid_flag, combin_flag);

	if(invalid_flag)  //有无效的按键组合按下
		ucRet =KEY_INVALID_BITMAP;
	else			   //此次按键有效，记录下按键的编号
		ucRet =(combin_flag<<30) + keynum;  //最高位存放组合键标志，后面8位存放按键编号
	return ucRet;
}

static void s_TimerScanKey(void)
{
    static unsigned int kbCode0, kbCode1,kbCode2;  /*分别记录阶段0-2，过程中的扫描结果	*/
    static int fn_press = 0;
    static int enter_press = 0;
    static int exist_combine_key = 0;
    static int kb_invalid = 0;
    unsigned char uRet;
    uint reg;

    kb_OnTimerCount++;

    switch (kb_TimerStep)
    {
    case 0:  /*初步分析按键*/
        kbCode0 = ScanKey(1);
        if (kbCode0 == 0)
            kb_OffTimerCount++;
        else if (KEY_INVALID_BITMAP == kbCode0)
        {
            kb_OffTimerCount++;
            kb_invalid = 1;
        }
        else    /*扫描到有效按键*/
        {
            /*组合键状态有除fn之外的按键按下，或非组合键状态有键按下，
             *进入case 1确认按键
             */
            if (kbCode0 & ~(KEYFN_PRESS | KEYENTER_PRESS))
            {
                kb_OffTimerCount = 0;
                kb_TimerStep = 1;
            }
            else
            {
                if (kbCode0 & KEYENTER_PRESS)
                {
                    enter_press = 1;
                }
                else /*只有FN按下，继续等待组合键另一个键按下后再进入case 1确认按键*/
                {
                    fn_press = 1;
                }
                //按下Fn键开背光
                if (k_ScrBackLightMode < 2)
                    ScrBackLight(1);
                if (k_KBLightMode < 2)
                    kblight(1);
            }
        }

        if (kb_OffTimerCount > 2)
        {
            if (fn_press | enter_press)
            {
                /*之前FN键有按下，并且没有其他有效组合按键和
                *没有同时按下多个按键产生无效键码，
                *将FN键值放入键盘缓冲区
                */
                if (!exist_combine_key && !kb_invalid)
                {
                    if ((kb_buf_over == 0) && (kb_Buffer_Busy == 0))
                    {
                        //写入FN键到缓冲区
                        if (enter_press != 0)
                            kb_buf[kb_in_ptr] = 0x14; //
                        else
                            kb_buf[kb_in_ptr] = FUN_BITMAP_NUM + 1;

                        kb_in_ptr = (kb_in_ptr + 1) % KB_BUF_MAX;

                        if (kb_in_ptr == kb_out_ptr)
                            kb_buf_over = 1;
                    }

                    if (k_ScrBackLightMode < 2)
                        ScrBackLight(1);
                    if (k_KBLightMode < 2)
                        kblight(1);
                    s_TimerKeySound(0);
                }
                fn_press = 0;
                enter_press = 0;
                kb_TimerStep = 21;
            }
            else
                kb_TimerStep = 22;
            kb_OffTimerCount = 0;
        }
        break;

        case 1:      
        /*该阶段对有效键进行确认扫描，并获取按键值*/
        kbCode1 = ScanKey(1);
        /*如果没有键值或者扫描到两次不一致时,退出扫描*/
        if (!kbCode1 || (kbCode1 != kbCode0))
        {
            /*与case 0扫描结果不一致，置位kb_invalid*/
            kb_invalid = 1;
            if (fn_press | enter_press)
                kb_TimerStep = 21; /*FN键按下，进入等待组合键释放阶段*/
            else
                kb_TimerStep = 22; /*只有普通按键按下，进入等待释放按键阶段*/
            break;
        }

        if ((kb_buf_over == 0) && (kb_Buffer_Busy == 0))
        {
            /*存放按键到缓冲区中*/
            /*如果是组合键，其简码在普通键上增加FNKEY0*/
            if (kbCode1 & (1 << 31))
                kb_buf[kb_in_ptr] = (kbCode1 & (~(1 << 31))) + FNKEY0;
            else if (kbCode1 & (1 << 30))
            {
                kb_buf[kb_in_ptr] = (kbCode1 & (~(1 << 30))) + ENKEY0;
            }
            else
                kb_buf[kb_in_ptr] = kbCode1;

            kb_in_ptr = (kb_in_ptr + 1) % KB_BUF_MAX;

            if (kb_in_ptr == kb_out_ptr)
                kb_buf_over = 1;
        }

            s_TimerKeySound(0);
            if(k_ScrBackLightMode < 2)  ScrBackLight(1);
            if(k_KBLightMode < 2)	       kblight(1);

        if (kbCode1 & (1 << 31))
        {
            exist_combine_key = 1;
            kb_TimerStep = 21; /*FN键按下，进入等待组合键释放阶段*/
        }
        else if (kbCode1 & (1 << 30))
        {
            exist_combine_key = 1;
            kb_TimerStep = 21; /*FN键按下，进入等待组合键释放阶段*/
        }
        else
            kb_TimerStep = 22; /*只有普通按键按下，进入等待释放按键阶段*/
        break;

        /* 该阶段用于等待组合键释放的扫描*/
    case 21:
        uRet = s_TimerKeySound(1);
        kbCode2 = ScanKey(1);
        /*其他键已经释放，但FN键还被按下，回至case 0继续扫描按键*/
        if (kbCode2 == KEYFN_PRESS && !uRet)
        {
            kb_TimerStep = 0;
        }
        else if (kbCode2 == KEYENTER_PRESS && !uRet)
        {
            kb_TimerStep = 0;
        }
        /*所有键均已释放，则结束按键扫描*/
        else if (!kbCode2 && ++kb_OffTimerCount >= 2 && !uRet)
            goto quit_scan;
        break;

            /*该阶段等待普通按键释放*/
        case 22:
            uRet = s_TimerKeySound(1);
            kbCode2 = ScanKey(0);
            if((kb_OnTimerCount==100)&&((get_machine_type()==S800)||(get_machine_type()==S900)||(get_machine_type()==D210)))
            {
                if((kbCode0==5)&&(kbCode1==5)&&(ScanKey(1)==5)&&g_taskpoweroff)
                    OsResume(g_taskpoweroff);
            }
            
            if(!kbCode2 && ++kb_OffTimerCount >= 2 && !uRet)
                goto quit_scan;		
            break;

        default:
            break;
    }

    return ;

    /*结束此次按键扫描，开中断并打开定时器*/
quit_scan:
    s_TimerKeySound(2);
    StopTimerEvent(KEY_HANDLER);
    kb_PressStatus = WAITFOR_PRESS; 
    exist_combine_key = 0;
    kb_invalid = 0;
    
    /* 允许按键中断*/
	reg =readl(GIO1_R_GRPF1_INT_MSK_MEMADDR);
	reg |= KEY_GPIOIN_MASK;
	writel(reg,GIO1_R_GRPF1_INT_MSK_MEMADDR);
}

/*--------------------------------------------------
 功能：检测键值缓冲区中是否有未读取的键值
 返回值： 0XFF------无键值
                        0X00------有键值
 ---------------------------------------------------*/
unsigned char kbhit(void)
{
	if(kb_buf[kb_out_ptr] != NOKEY)
		return 0x00;

	return NOKEY;
}


/*--------------------------------------------
  功能：清键值缓冲区，并清按键板中的缓冲区
 ---------------------------------------------*/
void kbflush(void)
{
     	uchar i;
	/* 须注意中断引起指针乱 */
	kb_Buffer_Busy = 1;

	for(i = 0; i < KB_BUF_MAX; i++)
	{
	    kb_buf[i] = NOKEY;
	}

	kb_in_ptr = 0;
	kb_out_ptr = 0;
	kb_buf_over = 0;
	
	kb_Buffer_Busy = 0;
}


/*-----------------------------------------------------------
 功能：从键盘缓冲区中读取一键值，无键时等待，不显示到屏幕上
 返回：返回取得的键值代码
 ------------------------------------------------------------*/
unsigned char getkey(void)
{
	uchar temp,kbcode;

	while((kb_in_ptr == kb_out_ptr)&&(kb_buf_over == 0));
	kb_buf_over = 0;
	temp = kb_buf[kb_out_ptr]-1;
	if(temp<FNKEY0)
	{
		kbcode=Key_Tab[temp];
	}
	else if(temp<ENKEY0)
	{
		kbcode=FKey_Tab[temp-FNKEY0];
	}
    else
    {
		kbcode=EKey_Tab[temp-ENKEY0];
    }
	kb_buf[kb_out_ptr]=NOKEY;
	kb_out_ptr=(kb_out_ptr+1)%KB_BUF_MAX;
	return kbcode;
}

/*-----------------------------------------------------------
 功能：从键盘缓冲区中读取一键值,不清楚已读取的键值
 返回：返回取得的键值代码
 ------------------------------------------------------------*/
unsigned char kbcheck(void)
{
	uchar temp,kbcode;

	while((kb_in_ptr == kb_out_ptr)&&(kb_buf_over == 0));
	kb_buf_over = 0;
	temp = kb_buf[kb_out_ptr]-1;
	if(temp<FNKEY0)
	{
		kbcode=Key_Tab[temp];
	}
    else if (temp<ENKEY0)
    {
		kbcode=FKey_Tab[temp-FNKEY0];
	}
    else
	{
		kbcode=EKey_Tab[temp-ENKEY0];
	}
	return kbcode;
}
/*向按键缓冲区中填键值,返回0表示成功，其它表示错误*/
int putkey(uchar key)
{
    int ret=0;
    uint x,i;

    if(!key)return 1;
    
    for(i=0; i<sizeof(Key_Tab); i++) //搜索按键值的索引号
	{
		if(Key_Tab[i] == key) break;
	}
	if(i == sizeof(Key_Tab)) return 2;

    irq_save(x);
    if((kb_buf_over == 0)&&(kb_Buffer_Busy == 0))
    {
        /*存放按键到缓冲区中*/
        /*如果是组合键，其简码在普通键上增加FNKEY0*/
		kb_buf[kb_in_ptr] = i+1;
		kb_in_ptr=(kb_in_ptr+1)%KB_BUF_MAX;
		
		if(kb_in_ptr == kb_out_ptr)    
			kb_buf_over = 1;
    }
    else ret = 3;

    irq_restore(x);
    return ret;
}

/***************************************
*  UserGetKey()
*  GetKey For Application hierarchy call, with the display message register 
check.
***************************************/
uchar   UserGetKey(uchar NeedReg)
{
	return getkey();
}

uchar   PciGetKey(ucNeedDMRCheck)
{
    return(UserGetKey(ucNeedDMRCheck));
}

//  在Ms毫秒内等待按键，若超时则返回0. 0<= Ms <= 24*60*60*1000
//  Ms=0xFFFFFFFF表示不起用超时功能，此函数等效于getkey
//  Ms=0表示立即检测并返回

uchar GetKeyMs(uint Ms)
{
    volatile uint    Begin, Cur;

    if(Ms > 24*60*60*1000)
        Ms = 24*60*60*1000;

    Begin = GetTimerCount();

    if(Ms != 0xFFFFFFFF)
    {
        while(kbhit())
        {
            Cur = GetTimerCount();
            if((Cur-Begin) >= Ms)
                return(0);
        }
    }
    return(getkey());
}

int KbReleased(void)
{
	if(kb_PressStatus == WAITFOR_PRESS)return 1;
	return 0;
}
void s_KBlightCtrl(unsigned char OnOff)
{
	static int first_flag=0;
	if(get_machine_type() == D200)
	{
		kblightctrl = OnOff;
		return ;
	}
	if(!first_flag)
	{
		gpio_set_pin_type(GPIOD,0, GPIO_OUTPUT);
		first_flag=1;
	}
	if(!OnOff) gpio_set_pin_val(GPIOD,0, 1);
	else	gpio_set_pin_val(GPIOD,0, 0);
	kblightstat = OnOff;
}

unsigned char get_scrbacklightmode(void)
{
	return k_ScrBackLightMode;
}
unsigned char get_kblightmode(void)
{
	return k_KBLightMode;
}
static void s_PowerKey_Int(void)
{
    uint uiTemp = 0;
    
    uiTemp = gpio_get_pin_val(POWER_KEY) ;
    // 关机键被按下
    if (uiTemp) return;
    // 设置关机键被触发标志
    gucPowerKeyDownFlag = 1;
    // 重新计时,按键被按下两秒中才能关机
    guiPowerKeyCount = 0;
}


// 按键关机函数,该函数在通用定时器中被调用
void s_PowerOff(void)
{
	uint i;

    if(get_machine_type()==S300)return;

	// 清图标
	for(i=0; i<ICON_NUMBER; i++)
	{
		if ((i+1) == ICON_BATTERY)
    	{
    		s_ScrSetIcon(ICON_BATTERY, 6);
			continue;
    	}
		ScrSetIcon(i+1, 0); 
	}   
    //停止打印机
    s_PrnStop();
    //关闭wifi
//    if(is_wifi_module())WifiClose();
	//复位wifi模块  在任务里面关闭wifi模块会有问题，改为直接下电，但是rs9110直接在任务里面直接下电也会有问题
	if(is_wifi_module() == 3) s_WifiPowerSwitch(0);
    
    if(GetWlType() != 0)
    {
    	CLcdSetFgColor(COLOR_BLACK);
		CLcdSetBgColor(COLOR_WHITE);
		ScrSpaceSet(0,0);
        ScrCls();
        SCR_PRINT(0, 3, 0x1, "关机中,请稍候...", "Shutting Down...");
        WlPowerOff(); // close wnet power
    }

	// 清屏
	s_LcdClear();
	// 关液晶背光
	s_ScrLightCtrl(2);
	// 关背光按键
	if(get_machine_type() == D200)
	{
		s_KbLock(0);
		s_TouchKeyStop();
	}
	else
	{
		s_KBlightCtrl(0);
	}
 
	//关蜂鸣器
	s_StopBeep();

    if(GetBatteryChargeProcess()<0)
	{
		// D210没有在充电，但是放在座机上面进入到假关机状态
		if(GetHWBranch() == D210HW_V2x && !OnBase())
		{
			OnBasePowerOffEntry();
		}
		else
		{
			gpio_set_drv_strength(POWER_HOLD, GPIO_DRV_STRENGTH_8mA);
			gpio_set_pin_val(POWER_HOLD, 0);
			while(1);
		}
	}
	else
	{
		k_Charging = 1;
		BatteryChargeEntry(2);
	}
}
// Power键关机函数,该函数在通用定时器中被调用
/* 当判断到关机键被按下时,首先开始计时,如果计时时间
   到两秒中,判断关机键是否仍处于被按下的状态,如果是
   则关机,否则计时清零,等待关机键被按下
*/ 
void s_KeyPowerOffHandler(void)
{
	uint uiTemp = 0,i;

	if(get_machine_type()!= S500)return;
	// 关机键被按下,开始计时
	if (gucPowerKeyDownFlag == 0) return;
	guiPowerKeyCount++;
	
	// 计时时间到,准备关机
	if (guiPowerKeyCount < 200) return;

	guiPowerKeyCount = 0;
	gucPowerKeyDownFlag = 0;
        
    uiTemp = gpio_get_pin_val(POWER_KEY) ;
    // 判断到关机键仍处于按下状态
    if (uiTemp) return;
	OsResume(g_taskpoweroff);
	#if 0
	//关蜂鸣器
	s_StopBeep();
	
    //停止打印机
    s_PrnStop();
	
    if(GetWlType() != 0)
    {
    	CLcdSetFgColor(COLOR_BLACK);
		CLcdSetBgColor(COLOR_WHITE);
		ScrSpaceSet(0,0);
        ScrCls();
        SCR_PRINT(0, 3, 0x1, "关机中,请稍候...", "Shutting Down...");
        WlPowerOff(); // close wnet power
    }

	// 清屏
	s_LcdClear();

	// 清屏
	ScrCls();
	// 关液晶背光
	s_ScrLightCtrl(2);
	// 关背光按键
	if(get_machine_type() == D200)
	{
		s_KbLock(0);
		s_TouchKeyStop();
	}
	else
	{
		s_KBlightCtrl(0);
	}
	// Output 0
	gpio_set_pin_val(POWER_HOLD, 0);
	while(1);
	#endif
}

void PowerOff(void)
{
	int i;

	if (get_machine_type()==D200)
	{
		s_AbortPowerOff();
	}
    if(get_machine_type()==S300)return;

	//s_Beep(BeepFreqTab[7],100,0);
    	//清图标
	for(i=0; i<ICON_NUMBER; i++)
	{
		if ((i+1) == ICON_BATTERY)
    	{
    		s_ScrSetIcon(ICON_BATTERY, 6);
			continue;
    	}
		ScrSetIcon(i+1, 0); 
	}	
    //停止打印机
    s_PrnStop();

    if(GetWlType() != 0)
    {
    	CLcdSetFgColor(COLOR_BLACK);
		CLcdSetBgColor(COLOR_WHITE);
		ScrSpaceSet(0,0);
        ScrCls();
        SCR_PRINT(0, 3, 0x1, "关机中,请稍候...", "Shutting Down...");
        WlPowerOff(); // close wnet power
    }

	//清屏幕
	s_LcdClear();
	//关屏幕背光
	s_ScrLightCtrl(2);
	//关键盘背光
	if(get_machine_type() == D200)
	{
		s_KbLock(0);
		s_TouchKeyStop();
	}
	else
	{
		s_KBlightCtrl(0);
	}

	if(GetBatteryChargeProcess()<0)
	{
		gpio_set_drv_strength(POWER_HOLD, GPIO_DRV_STRENGTH_8mA);
		gpio_set_pin_val(POWER_HOLD, 0);
		while(1);
	}
	else
	{
		k_Charging = 1;
		BatteryChargeEntry(2);
	}
}

// AbortHandler关机函数,该函数在Abort处理函数中被调用
void s_AbortPowerOff(void)
{
	uint i;

    if(get_machine_type()==S300)return;

	// 清图标
	for(i=0; i<ICON_NUMBER; i++)
	{
		if ((i+1) == ICON_BATTERY)
    	{
    		s_ScrSetIcon(ICON_BATTERY, 6);
			continue;
    	}
		ScrSetIcon(i+1, 0); 
	}   
    //停止打印机
    s_PrnStop();
    
    if(GetWlType() != 0)
    {
        CLcdSetFgColor(COLOR_BLACK);
		CLcdSetBgColor(COLOR_WHITE);
		ScrSpaceSet(0,0);
        ScrCls();
        SCR_PRINT(0, 3, 0x1, "关机中,请稍候...", "Shutting Down...");
        WlPowerOff(); // close wnet power
    }

	// 清屏
	s_LcdClear();
	// 关液晶背光
	s_ScrLightCtrl(2);
	// 关背光按键
	if(get_machine_type() == D200)
	{
		s_KbLock(0);
		s_TouchKeyStop();
	}
	else
	{
		s_KBlightCtrl(0);
	}
	//关蜂鸣器
	s_StopBeep();

	gpio_set_drv_strength(POWER_HOLD, GPIO_DRV_STRENGTH_8mA);
	gpio_set_pin_val(POWER_HOLD, 0);
	while(1);
}


/*********************************************/
//Just for D200 - Touch Keyboard
/*********************************************/
static  const char Key_Tab_D200[]={
	KEY0,			KEY1,		KEY2,		KEY3,
    KEY4,			KEY5,		KEY6,		KEY7,
    KEY8,			KEY9,		KEYF1,		KEYF2,
    KEYCLEAR,		KEYCANCEL,	KEYENTER,   KEYF5,
};  
volatile unsigned int guiCheckKeyFlag;
volatile unsigned int k_TouchLockTime; // = 3000;
/* 
 * touchkey_lock_mode = 0 : 按键锁定，短按电源按键、刷磁卡及IC卡可解锁
 * touchkey_lock_mode = 1 : 保持解锁状态30秒，30秒后自动锁定，或者短按电源键进行锁定和解锁的切换。
 * touchkey_lock_mode = 2 : 保持按键解锁状态，不再被短按电源键锁定
 */
volatile unsigned int touchkey_lock_mode;
volatile int giLastKey=-1;
uchar gucKeyBuf[2];
static T_SOFTTIMER touckb_timer;

/*
 * 与KbLock 不同之处：s_KbLock 只控制键盘，不控制键盘背光和屏幕背光，
 * 用法同KbLock。
 */
void s_KbLock(uchar mode)
{
	if (get_machine_type() != D200)
	{
		return;
	}
	
	if (mode > 2)
		return ;
	if (mode == 0) /* lock */
	{
		ScrSetIcon(5, 1);
		s_TouchKeyLockSwitch(1);
		touchkey_lock_mode = 1;
	}
	else if (mode == 1) /* unlock */
	{
		ScrSetIcon(5, 0);
		s_TouchKeyLockSwitch(0);
		k_TouchLockTime 	= k_LightTime;
		touchkey_lock_mode = 1;
	}
	else /* unlock always */
	{
		ScrSetIcon(5, 0);
		s_TouchKeyLockSwitch(0);
		touchkey_lock_mode = 2;
	}
}

/**
 * mode:
 * = 0 : 按键锁定，短按电源按键、刷磁卡及IC卡可解锁
 * = 1 : 保持解锁状态30秒，30秒后自动锁定，或者短按电源键进行锁定和解锁的切换。
 * = 2 : 保持按键解锁状态，不再被短按电源键锁定
 */
void KbLock(uchar mode)
{
	if (get_machine_type() != D200)
	{
		return;
	}
	
	if (mode > 2)
		return ;
	if (mode == 0) /* lock */
	{
		s_ScrLightCtrl(0); //Always turn off the light.
		s_KBlightCtrl(0);
	}
	else/* unlock */
	{
		if(k_ScrBackLightMode<2) ScrBackLight(1);
		else ScrBackLight(2);

		if(k_KBLightMode<2)kblight(1);
		else if(k_KBLightMode==2)ScrBackLight(2);
	}

	s_KbLock(mode);
}

extern int giTouchKeyLockFlag;
int KbCheck(int iCmd)
{
	if (iCmd == 0)
	{
		if (get_machine_type() == D200)
		{
			return giTouchKeyLockFlag;
		}
		else
		{
			return -1;
		}
	}
	else if (iCmd == 1)
	{
		if(kb_buf_over == 1)
			return KB_BUF_MAX;
		else
			return (kb_in_ptr-kb_out_ptr + KB_BUF_MAX)%KB_BUF_MAX;
	}
	else if (iCmd == 2)
	{
		return s_GetKbmuteStatus();
	}
	else if (iCmd == 3)
	{
		return kblightstat;
	}
	return -1;
}

void s_Kb_Touchkey_Isr(void)
{
	gpio_set_pin_interrupt(KB_INT_GPIO,0);
	guiCheckKeyFlag = 1;
}
// called by the system general timer
void s_Kb_Touchkey_handler(void)
{
	int iKeyCode;
	unsigned short usRegVal;

	if (get_machine_type() != D200)
		return;
	if(kblightstat != kblightctrl)
	{
		s_TouchKeyBLightCtrl(kblightctrl);
		kblightstat = kblightctrl;
	}
	if(guiCheckKeyFlag == 0)
	{
		return ;
	}

	gucKeyBuf[0] = KB_I2C_ADDR_TOUCHKEY_VALUE;
	kb_i2c_write_str(KB_I2C_SLAVER_WRITE,gucKeyBuf,1);
	kb_i2c_read_str(KB_I2C_SLAVER_READ, gucKeyBuf,2);

	usRegVal = ((unsigned short)gucKeyBuf[1] << 8) | gucKeyBuf[0];
	iKeyCode = fls(usRegVal);
	
	#ifdef DEBUG_TOUCHKEY	
	printk("key=0x%02x,0x%02x\r\n",gucKeyBuf[0],gucKeyBuf[1]);
	printk("Fck=%x, iCode=%d\n", usRegVal, iKeyCode);
	#endif

	if((usRegVal & (usRegVal-1)) || (usRegVal & (1<<15)))
	{
		guiCheckKeyFlag = 0;
		gpio_set_pin_interrupt(KB_INT_GPIO,1);
		return ;
	}

	#ifdef DEBUG_TOUCHKEY
	printk( "s_ulDrvTimerCheck=%d\r\n",s_TimerCheckMs(&touckb_timer));
	#endif
	if ((s_TimerCheckMs(&touckb_timer) != 0) && (iKeyCode != giLastKey))
	{
		//快速切换按键丢弃
		//快速按同一个按键时不丢弃
		guiCheckKeyFlag = 0;
		gpio_set_pin_interrupt(KB_INT_GPIO,1);
		return ;
	}
	
	if (iKeyCode != giLastKey)
	{
		s_TimerSetMs(&touckb_timer,200);//200ms内不允许切换按键
		giLastKey = iKeyCode;
	}

	//Push
	if((kb_buf_over == 0)&&(kb_Buffer_Busy == 0))
	{
		kb_buf[kb_in_ptr] = iKeyCode;
		kb_in_ptr=(kb_in_ptr+1)%KB_BUF_MAX;
		if(kb_in_ptr == kb_out_ptr)    
			kb_buf_over = 1;
	}
	
	if(k_ScrBackLightMode < 2)     ScrBackLight(1);
    if(k_KBLightMode < 2)             kblight(1);
    

	s_KbLock(touchkey_lock_mode);
	s_TimerKeySound(0);

	gpio_set_pin_interrupt(KB_INT_GPIO,1);
	guiCheckKeyFlag = 0;
}

void s_Kb_TouchKey_Init(void)
{
	int i;
	uint temp;
	
	memcpy(Key_Tab, Key_Tab_D200, sizeof(Key_Tab_D200));
	
	for(i = 0; i < KB_BUF_MAX; i++)
	{
		kb_buf[i] = NOKEY;
	}
	
	kb_in_ptr = 0;
	kb_out_ptr = 0;
	kb_buf_over = 0;

	kb_PressStatus = WAITFOR_PRESS;

	kb_TimerStep = 0;
	kb_OffTimerCount = 0;

	kb_Buffer_Busy = 0;
	kb_keyCombFlag = 0; 		//默认不启用组合键
	guiCheckKeyFlag = 0;

	gpio_set_pin_interrupt(KB_INT_GPIO,0);
	gpio_set_pin_type(KB_INT_GPIO, GPIO_INPUT);

	if (s_TouchKeyVersion() >= 0x04)//兼容V03版触摸按键驱动
	{
		s_TouchKeySetBaseLine();
	}
	s_TouchKeyStart();

	s_setShareIRQHandler(KB_INT_GPIO,INTTYPE_FALL,s_Kb_Touchkey_Isr);
	gpio_set_pin_interrupt(KB_INT_GPIO,1);
}
// 用于触摸按键的初始化
static void s_KbInit_wlankey(void);
static void s_KbInit_touchkey(void)
{
	k_LightTime         = 6000;//亮1分钟，自动熄灭
	k_ScrBackLightTime  = k_LightTime;//
	k_KBLightTime       = k_LightTime;//S80没有按键背光
	k_TouchLockTime		= k_LightTime;
	k_KBLightMode       = 1;

	s_Kb_TouchKey_Init();
	s_KbInit_wlankey();
	s_StopBeep();
}
// BT+WIFI 按键中断服务程序
static void s_WlanKey_Int(void)
{
	kb_TimerStep = 0;
	kb_OffTimerCount = 0;
    kb_OnTimerCount = 0;
	kb_PressStatus = WAITFOR_RELEASE;

	gpio_set_pin_interrupt(WLAN_EN_KEY,0);
	// 触发定时器事件，20ms后开始扫描按键
	StartTimerEvent(KEY_HANDLER,2);
}
static void s_TimerScanWlanKey(void)
{
    static unsigned int kbCode; 
    unsigned char uRet;

    kb_OnTimerCount++;

    switch(kb_TimerStep)
    {
        case 0:  /*初步分析按键*/
			if (gpio_get_pin_val(WLAN_EN_KEY) == 0)
				kbCode = 0x10;
			else
				kbCode = 0;
			
            if(kbCode == 0)
                kb_OffTimerCount++;
            else	/*扫描到有效按键*/
            {
				kb_OffTimerCount = 0; 
				kb_TimerStep =1;
            }

            if(kb_OffTimerCount > 2)
            {
				kb_TimerStep = 22;
				kb_OffTimerCount = 0;
            }
            break;

        case 1:      
			/*该阶段对有效键进行确认扫描，并获取按键值*/
			if (gpio_get_pin_val(WLAN_EN_KEY) == 0)
				kbCode = 0x10;
			else
				kbCode = 0;
            /*如果没有键值,退出扫描*/
            if(!kbCode)
            {
                kb_TimerStep = 22;/*只有普通按键按下，进入等待释放按键阶段*/
                break;
            }

            if((kb_buf_over == 0)&&(kb_Buffer_Busy == 0))
            {
                /*存放按键到缓冲区中*/
        		kb_buf[kb_in_ptr] = kbCode;
        		kb_in_ptr=(kb_in_ptr+1)%KB_BUF_MAX;
        		
        		if(kb_in_ptr == kb_out_ptr)    
        			kb_buf_over = 1;
            }

            s_TimerKeySound(0);
            if(k_ScrBackLightMode < 2)  ScrBackLight(1);
            if(k_KBLightMode < 2)	       kblight(1);
            s_KbLock(touchkey_lock_mode);
           
            kb_TimerStep = 22; /*只有普通按键按下，进入等待释放按键阶段*/
            break;	

            /*该阶段等待普通按键释放*/
        case 22:
            uRet = s_TimerKeySound(1);
			if (gpio_get_pin_val(WLAN_EN_KEY) == 0)
				kbCode = 0x10;
			else
				kbCode = 0;
			
            if(!kbCode && ++kb_OffTimerCount >= 2 && !uRet)
                goto quit_scan;		
            break;

        default:
            break;
    }

    return ;

    /*结束此次按键扫描，开中断并打开定时器*/
quit_scan:
    s_TimerKeySound(2);
    StopTimerEvent(KEY_HANDLER);
	gpio_set_pin_interrupt(WLAN_EN_KEY,1);
    kb_PressStatus = WAITFOR_PRESS; 
}
static void s_KbInit_wlankey(void)
{
	kb_TimerStep = 0;
	kb_OffTimerCount = 0;
	kb_OnTimerCount = 0;
	kb_PressStatus = WAITFOR_PRESS;
	
	gpio_set_pin_interrupt(WLAN_EN_KEY,0);
	gpio_set_pin_type(WLAN_EN_KEY, GPIO_INPUT);
	s_setShareIRQHandler(WLAN_EN_KEY,INTTYPE_FALL,s_WlanKey_Int);
	
	// 触发定时器事件，20ms后开始扫描按键
	s_SetTimerEvent(KEY_HANDLER,s_TimerScanWlanKey); //4
	
	gpio_set_pin_interrupt(WLAN_EN_KEY,1);
}
int get_touchkeylockmode(void)
{
	return touchkey_lock_mode;
}
int s_KbWaitRelease(void)
{
	int kb_in1;
	if (get_machine_type() == D200) return;
	while(1)	
	{
		kb_in1 = gpio_get_mpin_val(GPIOB,KEY_GPIOIN_MASK);
		if(kb_in1==KEY_GPIOIN_MASK) break;
	}
}
