#include "base.h"
#include "ts_sample.h"
#include "tsc2046.h"
#include "ts_hal.h"
#include "button.h"

extern QUEUE_SAMPLE_T gtAppPpSample;
TS_DEV_T gtTsDev;

extern void GlobalTsEventCheck();

void TsDevInit(void)
{
    gtTsDev.tAttr.sampleRateModel = BTN_SP_SPEED;
    gtTsDev.currOwner = TS_OWNER_SYS;
    gtTsDev.lastOwner = TS_OWNER_SYS;
    gtTsDev.isOwnerSwitch = 1;
    gtTsDev.lock = 0;

    if(get_machine_type() == S300) 
    	gtTsDev.isSKeySupported = 1;
    else
    	gtTsDev.isSKeySupported = 0;

    if(gtTsDev.isSKeySupported == 1) SoftKeyInit();

    ts_clear_buf();
    OsCreate((OS_TASK_ENTRY)GlobalTsEventCheck,TASK_PRIOR_Firmware8,0x1000);

    /* enable ts interrupt */
    if (gtTsDev.isSKeySupported)
    	ts_enable_irq();
}

/*************************************************************/
/*                                                           */ 
/*               TouchScreen API Definition                  */ 
/*															 */
/*************************************************************/

/* mode == 1  indicating the system and app share ts resource 
 * mode == 0  indicating the app occupy the ts resource solely 
 */
int TouchScreenOpen(int mode)
{
    APPINFO     CurRunApp;
	int 		flag;

    if (GetTsType() <= 0) return TS_RET_NO_MODULE;
    if(GetCallAppInfo(&CurRunApp)) return TS_RET_SYS_ERR;
	if(gtTsDev.lock == 1) return TS_RET_DEV_BUSY;
	if(mode != 0 && mode != 1) return TS_RET_INVALID_PARAM;

	irq_save(flag);
	gtTsDev.lastOwner = gtTsDev.currOwner;
	gtTsDev.currOwner = mode ? TS_OWNER_SHARE : CurRunApp.AppNum;
	gtTsDev.isOwnerSwitch = 1;
	gtTsDev.lock = 1;
    gtAppPpSample.head = 0;
    gtAppPpSample.nbr = 0;
	irq_restore(flag);
	
	if(gtTsDev.isSKeySupported) 
	{
		DelayMs(20); /* make sure that the system button is clear */
	}
	else
	{
	    ts_spi_init();
	    ts_enable_irq();
	}    

	return TS_RET_SUCCESS;
}

void TouchScreenAttrSet(TS_ATTR_T *ptTsAttr)
{
	if(gtTsDev.lock == 0) return ;
	
	if(ptTsAttr->sampleRateModel < SPEED_LIMIT)
		gtTsDev.tAttr.sampleRateModel = ptTsAttr->sampleRateModel;
	
	return ;
}

void TouchScreenFlush(void)
{
	int flag;

	if(gtTsDev.lock == 0) return ;
	
	irq_save(flag);
	gtAppPpSample.head = 0;
	gtAppPpSample.nbr = 0;
	irq_restore(flag);
}

#define PRESS_FLAG	100
int TouchScreenRead(TS_POINT_T *tTsPoint, uint num)
{
	const int 		TS_TIMEOUT =  20; /* unit: ms */
	T_SOFTTIMER		ts_timer;
	TS_SAMPLE_T 	s;
	int 			lcd_left, lcd_top, lcd_width, lcd_height, ret, idx = 0;

	if(gtTsDev.lock == 0) return TS_RET_DEV_NOT_OPENED;
	if(tTsPoint == NULL || num == 0) return TS_RET_INVALID_PARAM;

    s_ScrGetLcdArea(&lcd_left, &lcd_top, &lcd_width, &lcd_height);

	s_TimerSetMs(&ts_timer, TS_TIMEOUT);
	while(s_TimerCheckMs(&ts_timer))
	{
		ret = ts_get(&s, &gtAppPpSample);
		if(ret == 0) continue;

		tTsPoint[idx].pressure = s.pressure ? PRESS_FLAG : 0;

		if(s.x <= lcd_left) s.x = 0;
		else if(s.x >= lcd_left+lcd_width-1) s.x = lcd_width-1;
		else s.x = s.x - lcd_left;

		if(s.y <= lcd_top) s.y = 0;
		else if(s.y >= lcd_top+lcd_height-1) s.y = lcd_height-1;
		else s.y = s.y - lcd_top;
			
		tTsPoint[idx].x = s.x;
		tTsPoint[idx].y = s.y;

		idx++;
		if(idx >= num) break;
	}

	return idx;
}

void TouchScreenClose(void)
{
	int	flag;

	if(gtTsDev.lock == 0) return ;

	irq_save(flag);
	gtTsDev.lastOwner = gtTsDev.currOwner;
	gtTsDev.currOwner = TS_OWNER_SYS; 		/* system take over */
	gtTsDev.isOwnerSwitch = 1;
	gtTsDev.lock = 0;
	gtTsDev.tAttr.sampleRateModel = BTN_SP_SPEED;
	if(gtTsDev.isSKeySupported == 0) 
	{
		ts_disable_irq();
		ts_timer_stop();
	}

	irq_restore(flag);

	/* make sure that the system button is recover */
	if(gtTsDev.isSKeySupported) 
		DelayMs(20); 
}
int TouchScreenCalibration(void)
{
	int ret;

	ret = TSCalibration();
	if (gtTsDev.lock != 0)
	{
		ts_spi_init();
		ts_enable_irq(); 
	}

	return ret;
}
/* end of ts_hal.c */

