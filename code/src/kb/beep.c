#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "Base.h"
#include "kb.h"
#include "posapi.h"

volatile uint  BeepfUnblockCount=0;
volatile static int   kb_SoundFrequency;  //记录当前与beep发声的频率
volatile static int   kb_SoundDelay;         //记录当前用于按键中断中的BEEP发声时间
volatile static int   kb_SoundTempDelay; //用于临时存放kb_SoundDelay，并从kb_SoundDelay的
							      //值开始递减，当递减到0时，停止按键发声
volatile static int   kb_BeepIntBusy;   //按键中断中，正在发声
volatile static int	  BeepfFunBusy;      //判断蜂鸣器函数和SoundPlay是否正在发声
#define KEY_BEEP_TIME   60 //60毫秒
const uint BeepFreqTab[8] = {0,1680,1850,2020,2130,2380,2700,2750}; //频率表，单位HZ

extern volatile uchar    kb_in_ptr; 
extern volatile unsigned char IsAudioEnabled;
volatile unsigned char IsBeepEnabled=0;

struct t_BeepDrvConfig;
typedef struct t_BeepDrvConfig{
	int amplifier_flag;
	int amplifier_port;
	int amplifier_pin;
	int pwm_channel;
	int pwm_port;
	int pwm_pin;
}T_BeepDrvConfig;
T_BeepDrvConfig *Beep_Drv = NULL;

static void s_StopBeep_Common(struct t_BeepDrvConfig *pCfg)
{
	uint flag;

	// 设置蜂鸣器口设置为普通IO口，并输出低电平关闭蜂鸣器
	if (pCfg->amplifier_flag)
	{
		irq_save(flag);
		if(!IsAudioEnabled)
		{
			gpio_set_pin_val(pCfg->amplifier_port, pCfg->amplifier_pin, 0);//停止功放
		}
		IsBeepEnabled=0;
		irq_restore(flag);
	}

	pwm_enable(pCfg->pwm_channel, 0);
	gpio_set_pin_val(pCfg->pwm_port, pCfg->pwm_pin, 0);

	if (pCfg->amplifier_flag)
		gpio_set_pin_type(pCfg->pwm_port, pCfg->pwm_pin, GPIO_INPUT);
	else
	{
		gpio_set_pin_type(pCfg->pwm_port, pCfg->pwm_pin,GPIO_OUTPUT);
		gpio_set_pin_val(pCfg->pwm_port, pCfg->pwm_pin, 0);
	}
}

static void s_Beep_Common(struct t_BeepDrvConfig *pCfg, int freq,int ms,int closeflag)
{
	uint PeriodCnt,flag;

	irq_save(flag);
	IsBeepEnabled=1;
	irq_restore(flag);

	if (pCfg->amplifier_flag)
	{
		gpio_set_pin_type(pCfg->amplifier_port, pCfg->amplifier_pin, GPIO_OUTPUT);//使能功放芯片
		gpio_set_pin_val(pCfg->amplifier_port, pCfg->amplifier_pin, 1);
	}

	PeriodCnt =1000000/freq;
	pwm_config(pCfg->pwm_channel, PeriodCnt, 0, 0);
	pwm_duty(pCfg->pwm_channel, PeriodCnt/2);
	pwm_enable(pCfg->pwm_channel, 1);
		
	if(!closeflag)
	{
		// 蜂鸣器持续鸣叫100ms
		DelayMs(ms);
		// 设置TCLKOUT[0]普通GPIO口，并输出低电平关闭蜂鸣器
		s_StopBeep_Common(pCfg);
	}
}

static T_BeepDrvConfig Sxxx_Beep_Drv =
{
	.amplifier_flag = 1,/*有功放*/
	.amplifier_port = GPIOE,
	.amplifier_pin = 8,
	.pwm_channel = PWM2,
	.pwm_port = GPIOD,
	.pwm_pin = 2,
};
static T_BeepDrvConfig D210_Beep_Drv =
{
	.amplifier_flag = 0,/*无功放*/
	.amplifier_port = 0xff,
	.amplifier_pin = 0xff,
	.pwm_channel = PWM2,
	.pwm_port = GPIOD,
	.pwm_pin = 2,
};
static T_BeepDrvConfig Sxx_Beep_Drv =
{
	.amplifier_flag = 0,/*无功放*/
	.amplifier_port = 0xff,
	.amplifier_pin = 0xff,
	.pwm_channel = PWM1,
	.pwm_port = GPIOD,
	.pwm_pin = 1,
};
static T_BeepDrvConfig D200_Beep_Drv =
{
	.amplifier_flag = 0,/*无功放*/
	.amplifier_port = 0xff,
	.amplifier_pin = 0xff,
	.pwm_channel = PWM2,
	.pwm_port = GPIOD,
	.pwm_pin = 2,
};
/**************************************************
函数原型:static void s_Beep(int freq,int ms,int closeflag)
功能: 使蜂鸣器按照输入频率，在规定时间内发声
参数:
输入参数freq  表示发声频率，单位HZ
输入参数ms   表示发声的时间长度，单位毫秒,
			          此参数在closeflag 等于0时生效
输入参数closeflag表示发声后是否要关闭蜂鸣器
			   0 表示不关闭，1表示关闭
**************************************************/
void s_Beep(int freq,int ms,int closeflag)
{
	if (Beep_Drv != NULL)
		s_Beep_Common(Beep_Drv, freq, ms, closeflag);
}
/**************************************************
函数原型: void  s_StopBeep()
功能:用于关闭蜂鸣器发声
**************************************************/
void s_StopBeep(void)
{
	if (Beep_Drv != NULL)
		s_StopBeep_Common(Beep_Drv);
}

void s_BeepInit(void)
{
	int mach;
	mach = get_machine_type();
	switch(mach)
	{
	case S300:
	case S800:
	case S900:
		Beep_Drv = &Sxxx_Beep_Drv;
		break;
	case D210:
		Beep_Drv = &D210_Beep_Drv;
		break;
	case D200:
		Beep_Drv = &D200_Beep_Drv;
		break;
	case S500:
		Beep_Drv = &Sxx_Beep_Drv;
		break;
	default:
		return;
	}
	
	kb_SoundFrequency = BeepFreqTab[7];

	kb_SoundDelay = KEY_BEEP_TIME/20;
	kb_SoundTempDelay = 0;
	
	kb_BeepIntBusy = 0;
	BeepfFunBusy = 0;
}
static int s_SoundPlayAbort(void);
void Beep(void)
{
	while(kb_BeepIntBusy);//等待物理按键音结束
	if (BeepfFunBusy)
        s_SoundPlayAbort();//有SoundPlay时终止蜂鸣音
	BeepfFunBusy = 1;
	s_Beep(BeepFreqTab[7],100,0);
	BeepfFunBusy = 0;
}


void Beepf(unsigned char mode, unsigned short dlytime)
{
	unsigned char tempin_ptr;
	T_SOFTTIMER kb_timer;
    
	if(!dlytime) return;
		
	while(kb_BeepIntBusy);//等待物理按键音结束
	if (BeepfFunBusy)
        s_SoundPlayAbort();//有SoundPlay时终止蜂鸣音

	BeepfFunBusy = 1;
	mode%=7;
	
	s_Beep(BeepFreqTab[mode+1],0,1);

	tempin_ptr = kb_in_ptr;

	// 等待定时时间到
	s_TimerSetMs(&kb_timer, dlytime);
	while(s_TimerCheckMs(&kb_timer) && (tempin_ptr == kb_in_ptr));   //有按键，跳出循环关闭BEEP
	s_StopBeep();
	BeepfFunBusy = 0;
}

void Beef(unsigned char mode, unsigned short dlytime)
{
	Beepf(mode, dlytime);
}


#define MAX_FREQ 3000
#define MIN_FREQ 50
#define MAX_NUM 100

typedef struct
{
    int freq;
    int ms;
}NOTE_INFO;

static NOTE_INFO SoundDat[MAX_NUM];
static volatile int read_index=0, note_num=0;

/*
beep异步函数,定时器中计数并调用s_BeepUnblockAbort()函数终止
dlytime精度10毫秒，单位毫秒
mode:0~49 按固定模式播放; >=50 按照频率播放
*/
int s_BeepUnblock(unsigned short mode, unsigned short dlytime)
{    
    //printk("%s mode:%d,t:%d\n",__func__,mode,dlytime);
	if(!dlytime) return 0;

    
	BeepfFunBusy = 1;
    if(mode<50)
    {
        mode%=7;
        s_Beep(BeepFreqTab[mode+1],0,1);
    }
    else
    {
    	s_Beep(mode,0,1);
    }
    BeepfUnblockCount = (dlytime+9)/10;

    return 0;
}
int s_BeepCheckSoundBusy(void)
{
	return kb_BeepIntBusy;
}
//终止异步beep
void s_BeepUnblockAbort(void)
{
    s_StopBeep();
	BeepfFunBusy = 0;
    BeepfUnblockCount=0;
}

//终止beep
void s_BeepAbortAll(void)
{
    s_StopBeep();
	BeepfFunBusy = 0;
    BeepfUnblockCount=0;
    note_num = 0;
}

static int ParseParamList(char *param, NOTE_INFO *DataOut)
{
    int len, i, freq, ms, num = 0, flag = 0, last_i = 0;
    char *pt, buf[64], ch;

    len = strlen(param);
    if (len < 3) return -1;
    pt  = param;
    for (i = 0; i <= len; i++)
    {
        ch = *(pt+i);
        if (ch == ':')
        {
            if (flag != 0) return -1;
            if (i-last_i >= sizeof(buf)) return -2;
            memcpy(buf, pt+last_i, i-last_i);
            buf[i-last_i]=0x00;
            freq = atoi(buf);
            if (freq<MIN_FREQ || freq>MAX_FREQ) return -1;
            flag = 1;
            last_i = i+1;                
        }
        else if (ch == ';' || ch=='\0')
        {
            if (flag != 1) return -1;
            if (num >= MAX_NUM) return -1;
            if (i-last_i >= sizeof(buf)) return -2;
            memcpy(buf, pt+last_i, i-last_i);
            buf[i-last_i]=0x00;
            ms = atoi(buf);
            if (ms <= 0) return -1;
            DataOut[num].freq = freq;
            DataOut[num].ms = ms;
            num++;
            flag = 0; 
            last_i= i+1;
        }
    }
 
    if (flag != 0) return -1;
    if (num>MAX_NUM || num==0) return -1;

    return num;
}

int s_SoundPlayFreq(char *param)
{
    NOTE_INFO SoundDataTmp[MAX_NUM];
    int ret;
    uint x;
    
    if (param==NULL) return -1;

    if(kb_BeepIntBusy || BeepfFunBusy) return -4;/*设备忙*/
            
    ret = ParseParamList(param, SoundDataTmp);
    if (ret < 0) return ret;
    memcpy(SoundDat, SoundDataTmp, ret*sizeof(NOTE_INFO));

    irq_save(x);
    read_index = 0;
    note_num = ret;
	BeepfFunBusy=1;/*unlock beep 已就绪*/
    irq_restore(x);
	s_BeepUnblock(SoundDat[read_index].freq, SoundDat[read_index].ms);
	read_index++;

    return 0;
}

static int s_SoundPlayAbort(void)
{
    if(note_num)
    {
        s_BeepUnblockAbort();
        note_num = 0;
    }
    return 0;
}

int s_SoundPlayCheck(void)
{
    if(BeepfFunBusy)return 0;

    if(read_index>=note_num)
    {
        note_num=0;
        read_index=0;
        return 0;
    }

    s_BeepUnblock(SoundDat[read_index].freq, SoundDat[read_index].ms);
    read_index++;

    return 0;
}


/*
异步方式启动按键音，用于触摸屏虚拟按键音
*/
int s_KeySoundUnblock(void)
{
	if(kb_BeepIntBusy)return -1;//有按键音时直接返回
    
    if(BeepfFunBusy)     
    {
        s_SoundPlayAbort(); //有SoundPlay时终止蜂鸣音
        return 0;
    }
    
    if(kb_SoundDelay==0) //按键已经静音时需终止beep音
    {
        s_BeepAbortAll();
        return 0;
    }

    s_BeepUnblock(6, KEY_BEEP_TIME);

    return 0;
}

/*
返回值:0表示按键音停止状态，其他表示按键音播放状态
*/
unsigned char s_TimerKeySound(uchar mode)
{
	if (BeepfFunBusy)
	{
		kb_BeepIntBusy = 0;
        s_SoundPlayAbort();//有SoundPlay时终止蜂鸣音
		return 0;
	}

	if((kb_SoundDelay == 0)||(kb_SoundFrequency == 0))
	{
        s_BeepAbortAll();//按键已经静音时需终止beep音
		kb_BeepIntBusy = 0;
        
		return 0;
	}
	 
	if(mode == 0)
	{
	    kb_SoundTempDelay = kb_SoundDelay;
		s_Beep(kb_SoundFrequency,0,1);
		kb_BeepIntBusy = 1;
        
        return 1;
	}
	else if(mode == 1)
	{
		if(kb_SoundTempDelay > 0)
		{
		    kb_SoundTempDelay--;

            if(kb_SoundTempDelay==0)
            {
    			s_StopBeep();
    			kb_BeepIntBusy = 0;
                
                return 0;
            }
            
            return 1;              
		}
	    else
		{
            return 0;
		}
	}
	else
	{
		s_StopBeep();
		kb_BeepIntBusy = 0;
        
        return 0;
	}
}
void s_KeySoundCheck(void)
{
	static int cnt = 0;
	if (get_machine_type() != D200) return;
	cnt++;
	if (cnt < 2) return;
	cnt = 0;
	if((kb_BeepIntBusy== 1) && (kb_SoundTempDelay > 0))
	{
		if(--kb_SoundTempDelay == 0) s_TimerKeySound(2);
	}	
}

void kbsound(unsigned char mode, unsigned short dlytime)
{
	uchar dat0,dat1,dat2;
	uchar freq;

	while(kb_BeepIntBusy);
	freq = mode%8;
	if(freq == 0)
		kb_SoundDelay = 0;
	else
	{
		if(dlytime == 0)
			kb_SoundDelay = 0;
		else if(dlytime < 20)
			kb_SoundDelay = 1;  // 20
		else if(dlytime == 0xffff)
			kb_SoundDelay = dlytime;
		else
			kb_SoundDelay = dlytime/20;  //20
	}
	kb_SoundFrequency = BeepFreqTab[freq];
}
void  kbmute(unsigned char flag)
{
	if(flag == 0)
		kbsound(0,0);
	else
		kbsound(7,KEY_BEEP_TIME);
}

int s_GetKbmuteStatus()
{
	if((kb_SoundDelay == 0)||(kb_SoundFrequency == 0))
		return 0;
	else return 1;
}
