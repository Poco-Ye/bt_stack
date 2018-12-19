#include "base.h"
#include "../comm/comm.h"
#include "../lcd/lcdapi.h"
#include "../PUK/puk.h"
#include "../file/filedef.h"
#include <stdarg.h>
#include "../kb/kb.h"
#include "../pmu/pmu.h"

#define CHG_STAT_CHARGING			0
#define CHG_STAT_STOP				1
#define CHG_STAT_ERROR				2

#define ADC_POWER_VOL_CHANNEL   2//电压检测通道



extern uchar k_Charging;
extern volatile unsigned int    k_LightTime;
extern volatile unsigned char   k_KBLightMode;

int *gpLevelTab=NULL;
int giLevelTabSize=0;
// 测量电池的ADC电压基准为1.2V，电阻分压1:11，AD采样值x与电压V关系：
// V = x * 1.2 * 11 / 1023 (V)
//   = x * 13200 / 1023 (mV)    (精度约13mV)
//
                       /*{ flash,   0,     1,     2,     3,     4,      5,    6,      7,       8,     9,   10  ,外电}*/
                         /*flash,     0,     1,     2,     3,     4*/ 
const int S900_Level[] = {448, 500, 532, 560, 573, 587, 660};
const int D210_Level[] = {480, 500, 550, 578, 592, 615, 645};

void s_PowerBatteryChargeCtl(int en);
void EnableCharge(void)
{
	if (get_machine_type() == D210)
	{
		gpio_set_pin_type(GPIOE, 8, GPIO_OUTPUT);
		gpio_set_pin_val(GPIOE, 8, 0);
	}
	else if (get_machine_type() == D200)
	{
		s_PowerBatteryChargeCtl(1);
	}
}
void DisableCharge(void)
{	
	if (get_machine_type() == D210)
	{
		gpio_set_pin_type(GPIOE, 8, GPIO_OUTPUT);
		gpio_set_pin_val(GPIOE, 8, 1);
	}
	else if (get_machine_type() == D200)
	{
		s_PowerBatteryChargeCtl(0);
	}
}

int hasBatteryConfig(void)
{
	int machine;

	machine = get_machine_type();
	if(machine == D210 || machine == S900 || machine == D200)
		return 1;
	else
		return 0;
}

int is_ExPower()
{
	int machine;

	machine = get_machine_type();
	if (machine == D210)		//for D210, can get the power voltage through ADCSTA4
	{
		int val;

		val = GetExtVolt();
		if (val > 6000)
			return 1;
		else
			return 0;
		
	}
	else if (machine == S900)	//S900
	{
	    int temp;

	    temp = s_ReadAdc(ADC_POWER_VOL_CHANNEL);
		if (temp > S900_Level[6])
			return 1;
		else
			return 0;
	}
	else if (machine == D200)	//D200
	{
	    return s_PowerACPresent();
	}
	else
		return 1;
}

uint GetUsPeriod(uint last, uint now)
{
	if (now > last)
	{
		return now - last;
	}
	else
	{
		return now + 0xffffffff - last;
	}
}
#define CHG_BLINK_PERIOD	700	//ms
static uint k_ChgInvTime = 0;
static int k_ChargeLastStat = CHG_STAT_STOP;
static int k_ChgStatLastLevel=-1;

void s_UpdateChargeStat(void)
{
	static int needcheck = 0;
	static int init = 0;
	int level;
	uint now, period;

	if (init == 0)
	{
		if (get_machine_type()==S900 && (GetHWBranch()!=S900HW_Vxx))
		{
			needcheck = 1;
		}
		init = 1;
	}
	if (init == 1 && needcheck == 0)
		return;
	
	if (k_ChgStatLastLevel == -1)
	{
		k_ChgInvTime = GetUsClock();
	}

	level = gpio_get_pin_val(GPIOB, 3);
	now = GetUsClock();
	
	period = GetUsPeriod(k_ChgInvTime, now)/1000;

	if (level != k_ChgStatLastLevel)
	{
		if (period > CHG_BLINK_PERIOD && period <= CHG_BLINK_PERIOD*2)
		{
			k_ChargeLastStat = CHG_STAT_ERROR;
		}
		else
		{
			k_ChargeLastStat = (level == 0? CHG_STAT_CHARGING: CHG_STAT_STOP);
		}

		k_ChgInvTime = now;
	}
	else
	{
		if (period > CHG_BLINK_PERIOD*2)
		{
			k_ChargeLastStat = (level == 0? CHG_STAT_CHARGING: CHG_STAT_STOP);
		}
	}
	
	k_ChgStatLastLevel = level;
}

int s_GetChargeStat_D210(void)
{
	int stat;
	int bVol, eVol;

	bVol = GetBatVolt();
	eVol = GetExtVolt();

	if (bVol < 5000)	//no battery
		return CHG_STAT_ERROR;

	if (eVol < 5000)	//no ext power
		return CHG_STAT_STOP;
	
	stat = gpio_get_pin_val(GPIOB, 3);

	return (stat == 0? CHG_STAT_CHARGING : CHG_STAT_STOP);
}

int s_GetChargeStat_S900V1(void)
{
	int stat1, stat2;

	stat1 = gpio_get_pin_val(GPIOB, 3);
	stat2 = gpio_get_pin_val(GPIOB, 11);

	if (stat1 == 0 && stat2 == 1)
		return CHG_STAT_CHARGING;
	if (stat1 == 1 && stat2 == 0)
		return CHG_STAT_STOP;
	if (stat1 == 1 && stat2 == 1)	
		return CHG_STAT_ERROR;
}
int s_GetChargeStat_S900V2(void)
{
	return k_ChargeLastStat;
}

int s_GetChargeStat(void)
{
	if (get_machine_type() == D210)
	{
		return s_GetChargeStat_D210();
	}
	else if (get_machine_type() == S900)
	{
		if (GetHWBranch()!=S900HW_Vxx)
		{
			return s_GetChargeStat_S900V2();
		}
		else
		{
			return s_GetChargeStat_S900V1();
		}
	}
	else if (get_machine_type() == D200)
	{
		if (s_PowerBatteryPresent())
		{
			return s_PowerBatteryChargeOK();
		}
		else
		{
			return CHG_STAT_ERROR;
		}
	}
	return CHG_STAT_ERROR;	
}

void s_ChargingBaseLedInit(void)
{
	if (get_machine_type() != D200) return;
	gpio_set_pin_type(GPIOA, 20, GPIO_OUTPUT);
	gpio_set_pull(GPIOA, 20, 1);
}
void s_ChargingBaseLedCtrl(int charging)
{
	if (get_machine_type() != D200) return;
	if(charging == 1)
		gpio_set_pin_val(GPIOA, 20, 1);
	else
		gpio_set_pin_val(GPIOA, 20, 0);
}
void s_PowerInit(void);

static uchar gucBatteryLevel = 0;// 记录电池图标等级
extern volatile uchar k_PrnStatus;
void Battery_Check_Handler_D200(void);
void Battery_Check_Handler(void)
{
    int temp, machine, val1, val2, i;
    static uchar Count=0,Flash=0,BatIcon=1,offcnt=0;
	static int iBatteryChangeCounter=0;
    static uchar LastBatteryLevel = 0;
	int chargestate;

	if (hasBatteryConfig() == 0)
		return;
	if(k_Charging == 1) return;
	if (k_PrnStatus == PRN_BUSY) return;
	if (get_machine_type() == D200)
	{
		Battery_Check_Handler_D200();
		return;
	}

	s_UpdateChargeStat();	//Joshua _a for devlopment hardware

    if (Count++ < 100) return ;
    Count = 0;

   /*External power supply*/
   	if (is_ExPower() == 1)
	{
		chargestate = s_GetChargeStat();
	    //if(!gpio_get_pin_val(GPIOB, 3) && gpio_get_pin_val(GPIOB, 11)==1)/* battery charging */ 
	    if (chargestate == CHG_STAT_CHARGING)	//Joshua _c for compilate
		{
            ScrSetIcon(ICON_BATTERY,BatIcon+1);
            if (++BatIcon > 4) BatIcon = 1;
            //LastBatteryLevel = 0;
		}
        else if (chargestate == CHG_STAT_STOP)
        {
            gucBatteryLevel = 5; 
            ScrSetIcon(ICON_BATTERY, gucBatteryLevel);
            //LastBatteryLevel = gucBatteryLevel; 
        }       
		else
		{
			gucBatteryLevel = 0; 
            ScrSetIcon(ICON_BATTERY, gucBatteryLevel);
		}
		LastBatteryLevel = 0;
		return ;
	}
	
    temp = s_ReadAdc(ADC_POWER_VOL_CHANNEL);
	/*确保各个电压值都有一个最小的level*/    
	if(LastBatteryLevel > 0)
	{
		if(gpLevelTab[LastBatteryLevel] <= temp)
		{
			iBatteryChangeCounter = 0;
			gucBatteryLevel = LastBatteryLevel;
		}
		else
		{
			iBatteryChangeCounter++;
			if(iBatteryChangeCounter >= 15)/*15s*/
			{
				iBatteryChangeCounter = 0;
				gucBatteryLevel = LastBatteryLevel-1;
				LastBatteryLevel = gucBatteryLevel;
			}
		}
	}
    else
    {
        gucBatteryLevel = 0;
       // for(i=sizeof(S900_Level)/sizeof(S900_Level[0])-2; i>=1;  i--)
        for(i=giLevelTabSize-2; i>=1;  i--)
        {
            if(gpLevelTab[i] > temp) continue;
            gucBatteryLevel = i;
            LastBatteryLevel = gucBatteryLevel;
            break;            
        }    
    }
    if (temp >= gpLevelTab[0]) offcnt = 0;
	else  
	{
		offcnt++;
		if (offcnt > 30) PowerOff(); /* 关机标志已经设置直接关机*/
	}

    /* 如果大于1,则电池有电,根据电量显示图标*/
    if(gucBatteryLevel > 0)
    {
        ScrSetIcon(ICON_BATTERY, gucBatteryLevel);
    }    
    else /*等于0,则电池图标空格闪烁,提示充电*/
    {
    	if (Flash == 0)
    	{
    		s_ScrSetIcon(ICON_BATTERY, 6);
    	}
		else
		{
			ScrSetIcon(ICON_BATTERY, 1);
		}
		Flash ^= 0x01;
    }
}

unsigned char BatteryCheck_D200(void);
/* BatteryCheck API */
uchar BatteryCheck(void)
{
    uchar battery_level;

	if (hasBatteryConfig() == 0)
		return 0;
	if (get_machine_type() == D200)
	{
		return BatteryCheck_D200();
	}
	if (is_ExPower() == 1)
	{
		if (s_GetChargeStat() == CHG_STAT_CHARGING)
			return 5; /* ing */
		else 
			return 6; /* full */
	}

    if (gucBatteryLevel) battery_level =  gucBatteryLevel - 1;
    else battery_level = 0;

    return battery_level;
}

/* battery charge code */
int  GetBatteryChargeProcess()
{
	int stat;
	
	if (hasBatteryConfig() == 0)
		return -1;

	stat = s_GetChargeStat();
    //充电中电池拔出
	if (stat == CHG_STAT_ERROR)
    {
        return -1;
    }
	else if (is_ExPower() == 1)
	{
		if (stat == CHG_STAT_STOP)
			return 6; /* full */
		else 
			return 5; /* ing */
	}

	return -2;  /* power get rid off */
}

void DisplayBattery(void)
{
	static int shell=0;
	static int index=-1;
	static int last_counter=0, counter=0;
	int charge_sta;
	int i;

	counter = GetTimerCount();
	if(last_counter > counter) last_counter = counter; /* avoid rolling over */
	if(counter - last_counter < 1000) return;
	last_counter = counter;


	charge_sta = GetBatteryChargeProcess();
	if(charge_sta == 5)
	{	
		s_ChargingBaseLedCtrl(1);
		index += 1;
		if(index > 3) index = 0;
	}
	else if(charge_sta == 6)
	{
		s_ChargingBaseLedCtrl(0);
		index = 3;
	}
	else return ;
	
	if(shell == 0) /* draw shell */
	{
		DrawBatteryIcon(0);
		DrawBatteryIcon(1);
		DrawBatteryIcon(2);
		DrawBatteryIcon(3);
		shell = 1;
	}
	
	if (charge_sta == 5)
	{
		if (index == 0)
		{
			s_FillBatteryIcon();
		}
		DrawBatteryIcon(4+index);
	}
	else if(charge_sta == 6)
	{
		for (i=0; i<=index; i++)
		{
			DrawBatteryIcon(4+i);
		}
	}
	
}

void ProcessBattery(void)
{
    static uchar first=0;
    volatile static unsigned int t0;
	int ret;
    
	/* Power rid off */
	if(GetBatteryChargeProcess() < 0)
	{
		ret = gpio_get_pin_val(POWER_KEY);
		
		if (ret == 0)
			Soft_Reset();
		else
		{
			gpio_set_pin_type(POWER_HOLD, GPIO_OUTPUT);
			gpio_set_pin_val(POWER_HOLD, 0);	
			while(1); 
		}
	}

    if(!first)
    {
        first=1;
        t0 = GetTimerCount();
        s_ScrLightCtrl(1);
    }
	
	if (!gpio_get_pin_val(POWER_KEY))
	{
		t0 = GetTimerCount();
		s_ScrLightCtrl(1);
		while(!gpio_get_pin_val(POWER_KEY))
			if((GetTimerCount()-t0)>1000)Soft_Reset();
	}
	else if((GetTimerCount()-t0)>30000)//30s
		s_ScrLightCtrl(2);
	
}

/*
mode:
    0:没有充电，直接返回
    1:系统上电后直接进入充电状态
    2:从monitor关机后进入充电状态
    其它:不能识别的状态，直接返回
*/
void BatteryChargeEntry(int mode)
{
    uint flag;
	int w, h;
	int iRet;
	int auto_power_flag;
	unsigned char buf[256];
	memset(buf, 0, sizeof(buf));

    if (hasBatteryConfig() == 0)
		return;
	s_ChargingBaseLedInit();
	auto_power_flag = mode;
	if (get_machine_type()==D200)
	{
		//Step 1. 判断是否有适配器误触发，注意要先于第二步
		iRet = GetBatteryChargeProcess();
		if (iRet < 0)
		{
			k_Charging = 0;
			if (auto_power_flag != 0 && iRet == -2)
			{
				//需要进入充电状态，但未检测到适配器,属于误触发,重新下电
				gpio_set_pin_type(POWER_HOLD, GPIO_OUTPUT);
				gpio_set_pin_val(POWER_HOLD, 0);	
				while(1); 
			}
			return;
		}
		//Step 2. 判断配置文件
		iRet = ReadCfgInfo("BAT_DECT",buf);
		if (iRet > 0 && strcmp(buf, "FALSE") == 0)
		{
			k_Charging = 0;
			if(mode == 2) Soft_Reset();
			return;
		}
	}

	if (gpLevelTab == NULL)
	{
		if (get_machine_type()==S900)
		{
			gpLevelTab = (int *)S900_Level;
			giLevelTabSize = sizeof(S900_Level)/sizeof(S900_Level[0]);
		}
		else if (get_machine_type()==D210)
		{
			gpLevelTab = (int *)D210_Level;
			giLevelTabSize = sizeof(D210_Level)/sizeof(D210_Level[0]);
		}
	}
	EnableCharge();
    switch(mode)
    {
    case 1:

        s_AdcInit();
        s_LcdDrvInit();
    	s_InitINT();
    	s_GPIOInit();
        break;

    case 2:
        
        irq_save(flag);
		/* Disable  all interrupts */
		*(volatile uint*)(VIC0_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;
		*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;
		*(volatile uint*)(VIC2_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;
		irq_restore(flag);//重新开启中断
		
        break;

    default:
        return;
    }
	s_LcdGetAreaSize(&w, &h);
	if(mode == 2)
    {
    	// 如果事先将屏幕显示只输出到缓冲中的话，关机后充电图标将不会显示
    	if(get_machine_type() == D210)
    	{
    		ScrSetOutput(1);
    	}
    }
	s_ScrRect(0, 0, w, h, COLOR_BLACK, 1);
	s_TimerInit();
	s_KbInit();
	kbmute(0);
	k_LightTime = 0;
	k_KBLightMode = 2;
	s_KBlightCtrl(0);
	kbflush();
    DelayMs(1000);	
	while(1)
	{
		s_UpdateChargeStat();
		DisplayBattery();
		ProcessBattery();
	}
}

//for D200
void s_BatteryCheckInit(void);


#define BATTERY_LEVEL_NEVERCHK		(-2)
#define BATTERY_LEVEL_BAK_MAX		(6)


enum
{
	BAT_SAMPLE_COUNT=0,
	BAT_RESISTER_VCC,
	BAT_RESISTER_GND,
	BAT_ADC_REF_VOL,
	BAT_ADC_MAX,
};

enum
{
	CHARGE_STATE_NOADAPTER=1,
	CHARGE_STATE_NOBATTERY,
	CHARGE_STATE_CHARGING,
	CHARGE_STATE_CHARGOK,
};

extern int s_UsbCheckConnect(void);
extern void s_UsbSetConnectFlag(int flag);

static void s_PowerOffIsr(int gpio_num, int event);
static void s_PowerOffTimerFunc(void);

/* 格数对:4: 25%, 3: 25%, 2: 30%, 1: 15%, 0: 5% 
 * 防止显示抖动区间大约为50%.
 */
/* battery voltage * 1000 */
static const uint vol_level[10] = {
	5000, 3893, 3840, 3789, 3746, 3704, 3667, 3631, 3543, 3450
};
/*
static const uint vol_level[10] = {
	5000, 3949, 3884, 3820, 3734, 3648, 3639, 3631, 3540, 3450
};
*/

/*Param:SAMPLE_COUNT,RESISTER_VCC,RESISTER_GND,ADC_REF_VOL,ADC_MAX*/
static const uint BatVolCheckParam[5] = {7, 100000, 22000, 1200, 1023};

static int giBatteryLevelBaks[BATTERY_LEVEL_BAK_MAX]={0,};
static int giReadIndex=0;
static int giWriteIndex=0;
static volatile int gBatteryLevelAvrgMutex=0;

static int power_off_counter;

/* update battery icon in timer event function */
static int ChargeState = 0;
//for D200
static int ChargeStateBak = 0;
static int UsbFlag=0;
static int ChargingTime=0;
static int slowChargeCount = 3;
static int keycount=0;

volatile int gBatteryCheckBusy;
T_PowerDrvConfig *gzPwrDrvCfg = NULL;
static T_PowerDrvConfig S500_PowerDrvConfig = 
{
	.pwr_key_port = GPIOB,
	.pwr_key_pin = 15,
	.pwr_hold_port = GPIOB,
	.pwr_hold_pin = 13,
};
static T_PowerDrvConfig D200_PowerDrvConfig = 
{
	.pwr_key_port = GPIOB,
	.pwr_key_pin = 11,
	.pwr_hold_port = GPIOB,
	.pwr_hold_pin = 13,
};
static T_PowerDrvConfig Sxxx_PowerDrvConfig = 
{
	.pwr_key_port = GPIOB,
	.pwr_key_pin = 20,
	.pwr_hold_port = GPIOB,
	.pwr_hold_pin = 13,
};
void s_PowerGpioInit(void)
{
	int mach = get_machine_type();
	if(mach == D200)
	{
		gzPwrDrvCfg = &D200_PowerDrvConfig;
	}
	else if (mach == S300 || mach == S800 || mach == S900 || mach == D210)
	{
		gzPwrDrvCfg = &Sxxx_PowerDrvConfig;
	}
	else if (mach == S500)
	{
		gzPwrDrvCfg = &S500_PowerDrvConfig;
	}
	else
	{
		gzPwrDrvCfg = &Sxxx_PowerDrvConfig;
	}
}
static int giBatteryEntryFlag = 0;
void s_SetBatteryEntry(void)
{
	giBatteryEntryFlag = 1;
}
int s_GetBatteryEntryFlag(void)
{
	return giBatteryEntryFlag;
}
void s_PowerInit(void)
{	
	if (get_machine_type() != D200) return;
	
	gpio_set_pin_type(GPIOB, 12, GPIO_OUTPUT);
	gpio_set_pin_type(GPIOB, 28, GPIO_OUTPUT);
	gpio_set_pin_type(GPIOB, 29, GPIO_OUTPUT);
	
	gpio_set_pin_type(POWER_HOLD,GPIO_OUTPUT);
	gpio_set_pin_val(POWER_HOLD, 1);
	gpio_set_pin_interrupt(POWER_KEY,0);
	gpio_set_pin_type(POWER_KEY,GPIO_INPUT);
	gpio_set_pin_type(GPIOB,24,GPIO_INPUT);
	s_PmuInit();
	gpio_set_pin_val(GPIOB, 12, 0);
	gpio_set_pin_val(GPIOB, 29, 1);
	
	s_setShareIRQHandler(POWER_KEY, INTTYPE_FALL, (void *)s_PowerOffIsr);
	gpio_set_pin_interrupt(POWER_KEY, 1);
	
}

/* Battery check:
 * return 0: Battery is not present, 1: Battery is present
 */
int s_PowerBatteryPresent(void)
{
	unsigned char status;
	s_PmuRead(PMU_ADDR_CHGSTATE,&status, 1);
	if((status & 0x0f) == 0x09)
		return 0;
	else
		return 1;
}
/* AC adpter check:
 * return 0: AC adpter is not present, 1: AC adpter is present
 */
int s_PowerACPresent(void)
{
	unsigned char status;
	s_PmuRead(PMU_ADDR_CHGMONI,&status, 1);
	if(status & 0x01)
		return 1;
	else
		return 0;
}
/*battery charge status:
 * return 0: charge not finish,1: charge finish
 */
int s_PowerBatteryChargeOK(void)
{
	unsigned char status;
	s_PmuRead(PMU_ADDR_CHGSTATE, &status, 1);
	if(status == 0x05)
		return 1;
	else 
		return 0;
}
/*battery charge control
 *en 0: disable battery charge, 1: enable battery charge
 */
void s_PowerBatteryChargeCtl(int en)
{
	unsigned char status;
	if(en)
		status = 0x01;
	else
		status = 0;
	s_PmuWrite(PMU_ADDR_CHGSTART,&status, 1);
}

/*  check power key status:
 * return 0: power key not press, 1: power key is being pressed
 */
int s_PowerCheckPowerKeyStatus(void)
{
	unsigned int status;
	status = gpio_get_pin_val(POWER_KEY);
	if(status)
		return 0;
	else
		return 1;
}

int s_PowerReadBatteryVol(int pause_charge)
{
	
	int i;	
	volatile int j;
	unsigned int a,adc = 0;
	unsigned int min = 0xfff;
	unsigned int max = 0;
	int vol;

	if(pause_charge)
		s_PowerBatteryChargeCtl(0);
		
	for(i = 0; i < BatVolCheckParam[BAT_SAMPLE_COUNT]; i++)
	{
		a = s_ReadAdc(ADC_POWER_VOL_CHANNEL);
		for(j = 0; j < 20; j++)
			;
		adc += a;
		if(a < min)
			min = a;
		if(a > max)
			max = a;
	}
	if(pause_charge)
		s_PowerBatteryChargeCtl(1);
	adc -= min + max;
	adc = adc/(BatVolCheckParam[BAT_SAMPLE_COUNT] - 2);

	vol = ((unsigned long long)(BatVolCheckParam[BAT_RESISTER_GND] + BatVolCheckParam[BAT_RESISTER_VCC]) \
			* adc * BatVolCheckParam[BAT_ADC_REF_VOL]) \
			/ (BatVolCheckParam[BAT_RESISTER_GND] * BatVolCheckParam[BAT_ADC_MAX]);

	return vol;
}

int s_BatteryLevelAvrg(int nowLevel)
{
	int count,read_index_temp;
	int max=-1,min=0x0fffffff,total,i,avrg;
	
	if (gBatteryLevelAvrgMutex)
	{
		return nowLevel;
	}
	gBatteryLevelAvrgMutex = 1;
	
	//insert
	giBatteryLevelBaks[giWriteIndex] = nowLevel;
	giWriteIndex = (giWriteIndex + 1)%BATTERY_LEVEL_BAK_MAX;

	//check full
	if (giWriteIndex == giReadIndex)
	{
		giReadIndex = (giReadIndex+1)%BATTERY_LEVEL_BAK_MAX;
	}

	//avrg
	count = (giWriteIndex+BATTERY_LEVEL_BAK_MAX-giReadIndex)%BATTERY_LEVEL_BAK_MAX;
	read_index_temp = giReadIndex;
	total = 0;
	if (count == BATTERY_LEVEL_BAK_MAX-1)
	{
		for (i = 0;i < count;i++)
		{
			total += giBatteryLevelBaks[read_index_temp];
			if (giBatteryLevelBaks[read_index_temp]>max)
			{
				max = giBatteryLevelBaks[read_index_temp];
			}
			if (giBatteryLevelBaks[read_index_temp]<min)
			{
				min = giBatteryLevelBaks[read_index_temp];
			}
			read_index_temp = (read_index_temp+1)%BATTERY_LEVEL_BAK_MAX;
		}
		avrg = (total-max-min)/(count-2);
	}
	else
	{
		for (i = 0;i < count;i++)
		{
			total += giBatteryLevelBaks[read_index_temp];
			read_index_temp = (read_index_temp+1)%BATTERY_LEVEL_BAK_MAX;
		}
		if (count)
		{
			avrg = total/count;
		}
		else
		{
			gBatteryLevelAvrgMutex = 0;
			return nowLevel;
		}
	}
	gBatteryLevelAvrgMutex = 0;
	return avrg;
}
void s_BatteryLevelAvrgReset(void)
{
	giReadIndex = 0;
	giWriteIndex = 0;
}

/* return battery level 0~4 
 * 0: */
int s_PowerReadBatteryLevel(int force_update)
{
	unsigned int vol;
	int i;
	static int pre_level = 100;
	int level = 4;

	vol = s_PowerReadBatteryVol(0);
	//printk("s_PowerReadBatteryVol(0) = %d\r\n", vol);
	vol = s_BatteryLevelAvrg(vol);
	//printk("s_BatteryLevelAvrg = %d\r\n", vol);
	
	for(i=0; i<sizeof(vol_level)/sizeof(vol_level[0]); i += 2)
	{
		if(vol >= vol_level[i+1])
			break;
		level--;
	}

	if(i == sizeof(vol_level)/sizeof(vol_level[0]))
	{
		pre_level = -1;
		return -1;
	}

	if(force_update)
	{
		pre_level = level;
		return level;
	}
	else
	{
		if(vol > vol_level[i] && \
			(pre_level == level || pre_level == (level + 1)))
		{
			return pre_level;
		}
		else
		{
			pre_level = level;
			return level;
		}
	}
}

void s_PowerBattaryIcon(void)
{
	static int battery_charge = 2;
	static int battery_flash = 0;
	static int battery_low_power_off = 0;
	int battery_level;
	int TermType;
	int charging_level;
	static int battery_level_bak=BATTERY_LEVEL_NEVERCHK;

	if(!SystemInitOver())
		return ;
	if(gBatteryCheckBusy == 1)
		return ;
	
	if (ChargingTime > 0)
	{
		ChargingTime--;
	}
	if (slowChargeCount > 0)
	{
		slowChargeCount--;
	}

	if(s_PowerACPresent())
	{
		s_BatteryLevelAvrgReset();
		battery_level_bak=BATTERY_LEVEL_NEVERCHK;
		battery_low_power_off = 0;
		if(s_PowerBatteryPresent()) 
		{
			//printk("have ac, have battery\r\n");
			if (ChargeStateBak == CHARGE_STATE_NOBATTERY)
			{
				keycount = 1;
				
			}
			if (keycount)
			{
				keycount++;
				if (keycount == 3)
				{
					s_TouchKeyInitBaseLine();
					keycount = 0;
				}
			}
			
			if(s_PowerBatteryChargeOK())
			{
				s_ChargingBaseLedCtrl(0);
				ChargeState = CHARGE_STATE_CHARGOK;
			}
			else
			{
				s_ChargingBaseLedCtrl(1);
				ChargeState = CHARGE_STATE_CHARGING;
			}
			
			if(ChargeState == CHARGE_STATE_CHARGOK)
			{
				ChargeStateBak = ChargeState;
				s_ChargingBaseLedCtrl(0);
				ScrSetIcon(ICON_BATTERY, 5); //charge complete
			}
			else 
			{	
				s_ChargingBaseLedCtrl(1);
				if (ChargeStateBak != ChargeState || UsbFlag != s_UsbCheckConnect())
				{
					//from no charge state change to charge state
					//then init the pmu
					ChargingTime = 0;
				}
				ChargeStateBak = ChargeState;
				if(ChargingTime == 0)
				{
					if (slowChargeCount > 0)//刚插入适配器的时候用小电流充电，给足够时间给USB的检测
					{
						s_PmuSetChargeCurrent(0);
					}
					else
					{
						UsbFlag = s_UsbCheckConnect();
						if (UsbFlag)
						{
							//connect computer
							//init the pmu to slow charging mode
							s_PmuSetChargeCurrent(0);
							ChargingTime = 10*60; //10min don't check
						}
						else
						{
							//init the pmu to rapid charging mode
							s_PmuSetChargeCurrent(1);
							ChargingTime = 10*60; //10min don't check
						}

						s_PmuSetChargeMode(1);
					}
					
				}
				ScrSetIcon(ICON_BATTERY, battery_charge);
				if(++battery_charge > 5)
					battery_charge = 2;
			}
		}
		else
		{
			ChargeState = CHARGE_STATE_NOBATTERY;
			if (ChargeStateBak != ChargeState)
			{
				s_PmuSetChargeCurrent(1);
			}
			ChargeStateBak = ChargeState;
			battery_charge = 2;
			s_ChargingBaseLedCtrl(0);
			ScrSetIcon(ICON_BATTERY, 0); //no battery
		}
	} 
	else 
	{	
		s_ChargingBaseLedCtrl(1);
		ChargeState = CHARGE_STATE_NOADAPTER;
		ChargeStateBak = ChargeState;
		slowChargeCount = 3;
		s_UsbSetConnectFlag(0);//usb disconnect
		s_PmuSetChargeMode(0);
		
		/* ac not present, only battery plug in */
		battery_level = s_PowerReadBatteryLevel(0);
		//add by zhaorh,20130706
		if (battery_level_bak == BATTERY_LEVEL_NEVERCHK)
		{
			battery_level_bak = battery_level;
		}
		else
		{
			if (battery_level > battery_level_bak)
			{
				if ((battery_level - battery_level_bak) >= 2)
				{
					battery_level = battery_level_bak + 1;
				}
			}
			else if (battery_level < battery_level_bak)
			{
				if ((battery_level_bak - battery_level) >= 2)
				{
					battery_level = battery_level_bak - 1;
				}
			}
			battery_level_bak = battery_level;
		}
		if(battery_level >= 0)
			battery_low_power_off = 0;

		if(battery_level > 0) 
		{
			ScrSetIcon(ICON_BATTERY, battery_level + 1);
			battery_charge = battery_level + 1;
		} 
		else if(battery_level <= 0) 
		{
			battery_charge = 2;	
			/*flash battery icon to notify user,battery is low*/
			if (battery_flash == 0)
			{
				s_ScrSetIcon(ICON_BATTERY, 6);
			}
			else
			{
				ScrSetIcon(ICON_BATTERY, 1);
			}
			battery_flash ^= 1;

			if(battery_level == -1)
			{
				/* battery is too low, we check 30 times, then power off system */
				if(battery_low_power_off++ == 3)
					PowerOff();				
			}
		} 
	}
}
extern int giSysSleepFlag;
static void s_PowerOffIsr(int gpio_num, int event)
{
	gpio_set_pin_interrupt(POWER_KEY,0);
	if (giSysSleepFlag)/*PowerKey WakeUp Sleep*/
	{
		gpio_set_pin_interrupt(POWER_KEY,1);
		return;
	}
	power_off_counter = 0;
	s_SetSoftInterrupt(IRQ_OTIMER8, s_PowerOffTimerFunc);
}

extern OS_TASK *g_taskpoweroff;
static void s_PowerOffTimerFunc(void)
{
    static unsigned int timer10ms = 0;
	unsigned int ReadData;
	unsigned int power_key_release;
	int i;
	unsigned int flag;	
	extern int giTouchKeyLockFlag;
	
	if(timer10ms == s_Get10MsTimerCount()) return;
	timer10ms = s_Get10MsTimerCount();

	if (power_off_counter < 120)/* 短按,小于2s */
	{
		if (s_PowerCheckPowerKeyStatus()) /* 电源按键被按下 */
		{
			power_off_counter++;
		}
		else
		{
			/* power key release, see if need to reverse 
			 *lcd backlight status(D210/D200), keyboard lock status(D200) 
			 */
			unsigned char ret;
			/* reverse kb lock status */
			if ((ret = get_touchkeylockmode()) != 2)
			{
				if(giTouchKeyLockFlag)//Lock TouchKey and turn off the BackLight
				{
					KbLock(1);
				}
				else
				{
					KbLock(0);
				}
			}
			//PortPrintPoll("power off cancel!\r\n");
			//StopTimerEvent(ONOFF_HANDLER);
			s_SetSoftInterrupt(IRQ_OTIMER8, NULL);
			gpio_set_pin_interrupt(POWER_KEY,1);
			return;
		}
		//PortPrintPoll("power_off_counter:%d\r\n",power_off_counter);
	}
	else /* 长按,超过2s */
	{
		power_off_counter = 0;
		OsResume(g_taskpoweroff);
	}
}


int GetVolt_D200(void)
{
	return s_PowerReadBatteryVol(1);
}

unsigned char BatteryCheck_D200(void)
{
	int level;
	
	gBatteryCheckBusy = 1;
	if(s_PowerACPresent())
	{
		if (s_PowerBatteryPresent() == 0)
		{
			//no battery
			gBatteryCheckBusy = 0;
			return 6;
		}
		else if(s_PowerBatteryChargeOK())
		{
			gBatteryCheckBusy = 0;
			return 6;
		}
		else
		{
			gBatteryCheckBusy = 0;
			return 5;
		}
	}
	else
	{
		level =  s_PowerReadBatteryLevel(0);
		if(level <= 0)
		{
			gBatteryCheckBusy = 0;
			return 0;
		}
		else
		{
			gBatteryCheckBusy = 0;
			return level;
		}
	}
}


void Battery_Check_Handler_D200(void)
{
	static unsigned int cnt = 0;
	if(cnt++ == 100)
	{
		cnt = 0;
		s_PowerBattaryIcon();
	}
}

