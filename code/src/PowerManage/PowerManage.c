#include "base.h"
#include <stdarg.h>
#include "../kb/kb.h"
#include "../lcd/lcdapi.h"

extern unsigned char    k_ScrBackLightMode;
extern volatile unsigned char    k_KBLightMode;
extern volatile unsigned int touchkey_lock_mode;
extern volatile unsigned int    k_LightTime;
extern uint g_irq_num;
static int scanIsOpen=0;

SLEEP_CTRL sleep_ctrl[3]={
		//LCD
		{1,0,0,},
		//BT
		{0,0,0,},
		//BBL/RTC
		{0,0,0,}
};

extern int s_KbWaitRelease(void);
int giSysSleepFlag = 0;
int SysSleep(uchar *DownCtrl)
{
	static int last_result=100,last_sleep_time;
	int tmpd,result=4,j,kb_in1;
	uint raw_interrupt_svic,raw_interrupt_ovic1,raw_interrupt_ovic2;
	uint irqstatus_interrupt_svic,irqstatus_interrupt_ovic1,irqstatus_interrupt_ovic2;

	unsigned char c,*p = DownCtrl;

	if(get_machine_type() != S900 && get_machine_type() != D210 && get_machine_type() != D200) return -2;
    if(is_ExPower()) return -2;
	
    if(DownCtrl!=NULL && strlen(DownCtrl)!=2)        
		return -1;    

	if(DownCtrl != NULL)/*check array is valid*/    
	{        
		while((c = *p++) != '\0')        
		{           
			if(c != '0' && c != '1')           
			{               
				return -1;            
			}        
		}    
	}
    if(GetTimerCount() - last_sleep_time <= 2000)           
		return -3;
	giSysSleepFlag = 1;
	while(s_BeepCheckSoundBusy());
	s_BeepAbortAll();
	
	//2--power down ICC slots if required 
	if(DownCtrl==NULL || DownCtrl[0]!='0')
	{
		IccClose(0);
		IccClose(1);
		IccClose(2);
		IccClose(3);
	}
	
	//3--power down PICC if required 
	if(DownCtrl==NULL || DownCtrl[1]!='0')PiccClose();
    //wireless sleep    
    WlSleep(1, NULL);

  	//BAR CODE
	scanIsOpen = s_ScanIsOpen();
    if(scanIsOpen)ScanClose();
    
	//4--power down modem
	OnHook();
	//5--wifi module sleep
	//if(is_wifi_module()) s_WifiInSleep();
	if(is_wifi_module() && s_IsWifiOpened()) 
		WifiClose();
	GpsClose();
	
	//7--wait for key release
	s_KbWaitRelease();
	
	//8--wait for all uart ports' tx fifo empty
	while(PortTxPoolCheck(0)==1);
	while(PortTxPoolCheck(1)==1);
	while(PortTxPoolCheck(2)==1);
	while(PortTxPoolCheck(3)==1);
	while(PortTxPoolCheck(4)==1);

	//--wair for kb interrupt server finish
	while(!KbReleased());

	if (get_machine_type() == D200)
	{
		s_KbLock(0);
		s_TouchKeyBLightCtrl(0);
	}
	if(sleep_ctrl[MODULE_LCD].sleep_mode==1)
	{
		s_ScrLightCtrl(2);//关lcd背光,因为cpu频率降低后lcd刷新慢，会看到屏幕闪烁
		Lcd_Sleep_In();
	}
	else // sleep_mode == 2;
	{
		s_ScrLightCtrl(0);
	}
	
	if(sleep_ctrl[MODULE_RTC].wake_enable)
	{
		bbl_match_setting(sleep_ctrl[MODULE_RTC].param*60);
	}

	result=bcm5892_suspend_enter(DownCtrl);
	
	last_sleep_time = GetTimerCount();
	last_result=result;

	if(sleep_ctrl[MODULE_LCD].sleep_mode==1)
	{
		Lcd_Sleep_Out();
	}

    if(scanIsOpen)ScanOpen();

    /*restore screen backlight,keyboard backlight status*/
	ScrBackLight(k_ScrBackLightMode);
	kblight(k_KBLightMode);
	s_KbLock(touchkey_lock_mode);

	if(sleep_ctrl[MODULE_RTC].wake_enable)
	{
		sleep_ctrl[MODULE_RTC].wake_enable = 0;
		bbl_match_cls();
	}
	if(sleep_ctrl[MODULE_BT].wake_enable)
	{
		sleep_ctrl[MODULE_BT].wake_enable = 0; //当次有效。
	}

	//if(is_wifi_module()) s_WifiOutSleep();
	WlSleep(2, NULL);//wireless wakeup
	giSysSleepFlag = 0;
	return result;
	
}

void SysIdle(void)
{
	__asm("STMFD     SP!, {R0}");
	__asm("MOV  R0, #0");
	__asm("mcr	p15, 0, r0, c7, c0, 4");  /*wait for interrupt*/
	__asm("LDMFD     SP!, {R0}");	
}

int SetWakeupChannel(uint index, uchar *attr)
{
    int min;

    if(get_machine_type()!=S900 && get_machine_type()!=D210 && get_machine_type()!=D200)
    	return -2;
    if(attr==NULL) return -1;

    switch(index)
    {
        case 1:
            min = atoi(attr);
            if (min<1 || min>1440) return -1;/*invalid param*/
            sleep_ctrl[MODULE_RTC].wake_enable = 1;
            sleep_ctrl[MODULE_RTC].param = min;
            break;
        case 2:
        	if(is_bt_module()==0) return -2;
        	if(strcmp(attr,"1")==0)
        	{
        		//蓝牙唤醒
        		sleep_ctrl[MODULE_BT].sleep_mode = 0;
        		sleep_ctrl[MODULE_BT].wake_enable = 1;
        	}
        	else
        	{
        		return -1;
        	}
        	break;
        default:
        	return -2;
        	break;
    }
    return 0;
}


void s_SetCLcdSleepMode(int mode)
{

	if(get_machine_type()!=D200) //RGB 接口屏幕无法支持休眠后保持屏幕显示。
		return;

	if(mode==1)
	{
		sleep_ctrl[MODULE_LCD].sleep_mode = 2;
	}
	else
	{
		sleep_ctrl[MODULE_LCD].sleep_mode = 1;
	}

}

// 进入到假关机状态但cpu不下电，避免出现座机串口电流的回灌
void OnBasePowerOffEntry()
{
	uint flag;
	int w, h;
    volatile static unsigned int t0;
	
	irq_save(flag);
	/* Disable	all interrupts */
	*(volatile uint*)(VIC0_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;
	*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;
	*(volatile uint*)(VIC2_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;
	irq_restore(flag);//重新开启中断

	s_LcdGetAreaSize(&w, &h);
	s_ScrRect(0, 0, w, h, COLOR_BLACK, 1);
	s_TimerInit();
	s_KbInit();
	kbmute(0);
	k_LightTime = 0;
	k_KBLightMode = 2;
	k_ScrBackLightMode = 3;
	s_KBlightCtrl(0);
	s_ScrLightCtrl(2);
	kbflush();
	DelayMs(1000);	
	while(1)
	{
		if(!gpio_get_pin_val(POWER_KEY))			// 有按键
		{
			kbflush();
			t0 = GetTimerCount();
			while(!gpio_get_pin_val(POWER_KEY))
				if((GetTimerCount()-t0)>1000)Soft_Reset();
		}
	}
}
