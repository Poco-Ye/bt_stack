#include "base.h"
#include "platform.h"
#include "stdarg.h"
#include "ts_sample.h"
#include "button.h"
#include "ts_hal.h"
#include "../lcd/Lcdapi.h"

/* invalid button status */
#define	BUTTON_NONE  		-1
#define BTN_CHECK_EDGE  	2

extern uchar 		  bIsChnFont;
extern TS_DEV_T 	  gtTsDev;
	   BTN_QUE_T 	  gtSysBtnQue;
extern QUEUE_SAMPLE_T gtBtnPpSample;
extern QUEUE_SAMPLE_T gtAppPpSample;
extern volatile int giIsPiccDetected;

extern void s_TimerKeySound(uchar mode);
void ButtonEventProc(TS_DEV_T *ptTsDev);
void ButtonEventCheck(BTN_QUE_T *ptBtnQue, TS_SAMPLE_T  *s, int sta);
void GenericBtnTriggerEvent(int idx);
void GenericBtnReleaseEvent(int idx);

BTN_QUE_T *GetQueue(int owner)
{
	return &gtSysBtnQue;
}

int TsObjectRegister(TS_OBJECT_T *ptTsObject)
{
	int LCD_MAX_X=239, LCD_MAX_Y=319;
	const int 	RESERVED = 2;
	BTN_QUE_T 	*ptQue;
	int 		i;
	int 		flag;

	if(gtTsDev.currOwner != TS_OWNER_SYS) return -2;

	if( ptTsObject->left+BTN_CHECK_EDGE   >= LCD_MAX_X          || 
		ptTsObject->top+BTN_CHECK_EDGE    >= LCD_MAX_Y          ||
		ptTsObject->right-BTN_CHECK_EDGE <= 0                   ||
		ptTsObject->bottom-BTN_CHECK_EDGE <= 0                  ||
		ptTsObject->left+BTN_CHECK_EDGE+RESERVED >= ptTsObject->right  ||
		ptTsObject->top+BTN_CHECK_EDGE+RESERVED >= ptTsObject->bottom	) {
		return -3;
	}

	ptQue = GetQueue(gtTsDev.currOwner);

	/* check exist */
	for(i=0; i<MAX_BTN_CNT; i++)
	{
		if(ptQue->tBtn[i].isOccupied!=0 && ptQue->tBtn[i].ptTsObject == ptTsObject)
			return -3;
	}
	
	for(i=0; i<MAX_BTN_CNT; i++)
	{
		if(ptQue->tBtn[i].isOccupied == 0)
		{
			irq_save(flag);
			ptTsObject->tsObjectID = i;
			ptQue->tBtn[i].ptTsObject = ptTsObject;
			ptQue->tBtn[i].isOccupied = 1;
			irq_restore(flag);
			return 0;
		}
	}
		
	return -1; /* queue is full */
}


int TsObjectDeregister(TS_OBJECT_T *ptTsObject)
{
	BTN_QUE_T 	*ptQue;
	int 		flag;

	if(gtTsDev.currOwner != TS_OWNER_SYS) return -2;
	if(ptQue->tBtn[ptTsObject->tsObjectID].isEnabled) return -3;

	ptQue = GetQueue(gtTsDev.currOwner);
	irq_save(flag);
	ptQue->tBtn[ptTsObject->tsObjectID].isOccupied = 0;
	ptQue->tBtn[ptTsObject->tsObjectID].ptTsObject = NULL;
	irq_restore(flag);
	return 0;
}


int TsObjectEnable(TS_OBJECT_T *ptTsObject)
{
	BTN_QUE_T 	*ptQue;
	int			flag; 

	if(gtTsDev.currOwner != TS_OWNER_SYS) return -2;

	ptQue = GetQueue(gtTsDev.currOwner);
	if(ptQue->tBtn[ptTsObject->tsObjectID].isOccupied == 0) return -3;

	irq_save(flag);
	ptQue->tBtn[ptTsObject->tsObjectID].isEnabled = 1;
	ptQue->tBtn[ptTsObject->tsObjectID].isRefresh = 1;
	ptQue->refresh = 1;
	irq_restore(flag);
	return 0;
}

int TsObjectDisable(TS_OBJECT_T *ptTsObject)
{
	BTN_QUE_T 	*ptQue;
	int 		flag;

	if(gtTsDev.currOwner != TS_OWNER_SYS) return -2;

	ptQue = GetQueue(gtTsDev.currOwner);
	if(ptQue->tBtn[ptTsObject->tsObjectID].isOccupied == 0) return -3;

	irq_save(flag);
	ptQue->tBtn[ptTsObject->tsObjectID].isEnabled = 0;
	ptQue->tBtn[ptTsObject->tsObjectID].isRefresh = 1;
	ptQue->refresh = 1;
	irq_restore(flag);
	return 0;
}

void ShowButton(TS_OBJECT_T *btn, BTN_STATUS btnSta)
{
	uint i,j;
	int rgb;
	uchar color_r, color_g, color_b;
	int imgH, imgW;
	int realH, realW;
	int coverIndex;

	if(btn->colorscheme == BTN_PURE_COVER)
	{
		imgH = btn->bottom - btn->top;
		imgW = btn->right - btn->left;
		realW = imgW;
		realH = imgH;

		if(imgH<0 || imgW<0 || realH<0 || realW<0) return;
		
	    rgb = (uint)(btn->background[btnSta]);
		s_ScrRect(btn->left, btn->top, btn->left + realW - 1, btn->top + realH - 1, rgb, 1);
	}
	else if(btn->colorscheme == BTN_IMG_COVER)
	{
		if(bIsChnFont)
			coverIndex = btnSta + BTN_NORMAL_CN;
		else 
			coverIndex = btnSta + BTN_NORMAL_EN;

		const uchar *pImg = btn->background[coverIndex]->data;
		imgH = btn->background[coverIndex]->high;
		imgW = btn->background[coverIndex]->width;

		if(btn->right - btn->left + 1 >= btn->background[coverIndex]->width)
			realW = btn->background[coverIndex]->width;
		else
			realW = btn->right - btn->left + 1;

		if(btn->bottom - btn->top + 1 >= btn->background[coverIndex]->high)
			realH = btn->background[coverIndex]->high;
		else
			realH = btn->bottom - btn->top + 1;

		if(imgH<0 || imgW<0 || realH<0 || realW<0) return;

		for(i=0; i<realH; i++)
		{
			for(j=0; j<realW; j++)
			{
				rgb = (pImg[(imgH-i-1)*imgW*2+j*2]<<8) | pImg[(imgH-i-1)*imgW*2+j*2+1];
				s_LcdSetDot(btn->left+j, btn->top+i, RGB_RED(rgb)<<16 | RGB_GREEN(rgb)<<8 | RGB_BLUE(rgb));
			}
		}
	}
}

/* BTN_LEFT_TOP_Y : 在RF载波干扰的情况下，多样机平均统计最左边按钮左上角的y值 
 * NOISE_Y_OFFSET : 多样机平均统计按钮区域y值偏差, 有RF载波干扰y0, 没RF载波干扰y1, 
 * 					NOISE_Y_OFFSET = y1 - y0 。
 * XMAX           : x 最大值.
 * 其中 数值 "70", "10", 为多样机采点统计后修正关系式需要的值。
 */
static int SampleAntiRFCorrection(TS_SAMPLE_T *pstIn, TS_SAMPLE_T *pstOut)
{
	TS_SAMPLE_T tTmpS;
	const int BTN_LEFT_TOP_Y = 210; 
	const int NOISE_Y_OFFSET = 40; 
	const int XMAX = 240; 

	tTmpS = *pstIn;

	if(giIsPiccDetected == 1) 
	{
		if(pstIn->y > pstIn->x / 10 + BTN_LEFT_TOP_Y + (XMAX - pstIn->x) / 70)
			tTmpS.y = (tTmpS.y + NOISE_Y_OFFSET) + (XMAX - tTmpS.x)/10;
	}

	*pstOut  = tTmpS;
	return 0;
}

int CheckButton(TS_SAMPLE_T *s, BTN_QUE_T *ptBtnQue)
{
	int i;
	TS_SAMPLE_T tTmpS;
	
	tTmpS = *s;
	SampleAntiRFCorrection(&tTmpS, s);

	for (i=0; i<MAX_BTN_CNT; i++)
	{
		if (ptBtnQue->tBtn[i].isOccupied == 0)	 continue;
		if (ptBtnQue->tBtn[i].isEnabled == 0) 	continue;

		if ((s->x >= ptBtnQue->tBtn[i].ptTsObject->left+BTN_CHECK_EDGE) && 
			(s->x <= ptBtnQue->tBtn[i].ptTsObject->right-BTN_CHECK_EDGE) && 
		    (s->y >= ptBtnQue->tBtn[i].ptTsObject->top+BTN_CHECK_EDGE) && 
			(s->y <= ptBtnQue->tBtn[i].ptTsObject->bottom-BTN_CHECK_EDGE)) {
			return i;
		}
	}
 	return -1;
}

void ButtonRefresh(BTN_QUE_T *ptBtnQue)
{
	int i;

	for (i=0; i<MAX_BTN_CNT; i++)
	{
		if(ptBtnQue->tBtn[i].isOccupied == 0) continue;
		if(ptBtnQue->tBtn[i].isRefresh)
		{
			if(ptBtnQue->tBtn[i].isEnabled == 0) 
			{
				s_ScrRect(ptBtnQue->tBtn[i].ptTsObject->left, ptBtnQue->tBtn[i].ptTsObject->top,
					  ptBtnQue->tBtn[i].ptTsObject->right, ptBtnQue->tBtn[i].ptTsObject->bottom, COLOR_WHITE, 0);
			}
			else ShowButton(ptBtnQue->tBtn[i].ptTsObject, BTN_NORMAL);
		}
		ptBtnQue->tBtn[i].isRefresh = 0;
	}
}

void ButtonEventCheck(BTN_QUE_T *ptBtnQue, TS_SAMPLE_T *s, int sta)
{
	int PRESS_CONFIRM_CNT = 6;
	int i, ret, iCurBtnIdx;
	static int pressCount = 0;
	static int process = 0;

	if(giIsPiccDetected) PRESS_CONFIRM_CNT = 1;

	if(ptBtnQue->lastBtnIdx == BUTTON_NONE)
		process = 0;

	if(process == 0) /* state wait for press */
	{
		if(sta == TS_TRIGGER)
		{
			iCurBtnIdx = CheckButton(s, ptBtnQue);
			if(iCurBtnIdx >= 0)
			{
				ptBtnQue->lastBtnIdx = iCurBtnIdx;
				pressCount = 0;
				process = 1;
			}
		}
	}
	else if(process == 1) /* state for confirmation */
	{
		iCurBtnIdx = CheckButton(s, ptBtnQue);
		if(sta == TS_PRESSING)
		{
			if(iCurBtnIdx == ptBtnQue->lastBtnIdx)
			{
				pressCount++;
				if(pressCount >= PRESS_CONFIRM_CNT)
				{
					ptBtnQue->lastBtnIdx = iCurBtnIdx;
					ShowButton(ptBtnQue->tBtn[iCurBtnIdx].ptTsObject, BTN_PRESS);
					if (ptBtnQue->tBtn[iCurBtnIdx].TriggerHandler != NULL)
						ptBtnQue->tBtn[iCurBtnIdx].TriggerHandler(iCurBtnIdx);
					pressCount = 0;
					process = 2;
				}
			}
			else 
			{
				pressCount = 0;
				process = 0;
			}
		}
		else if(sta == TS_TRIGGER)
		{
			if(iCurBtnIdx == ptBtnQue->lastBtnIdx)
			{
				pressCount = 0;
				process = 1;
			}
			else if(iCurBtnIdx >= 0)
			{
				ptBtnQue->lastBtnIdx = iCurBtnIdx;
				pressCount = 0;
				process = 1;
			}
		}
		else if(sta == TS_RELEASE)
		{
			pressCount = 0;
			process = 0;
		}
	}
	else if(process == 2) /* state for waiting release or slide */
	{
		iCurBtnIdx = CheckButton(s, ptBtnQue);
		if(sta == TS_PRESSING || sta == TS_TRIGGER)
		{
			if(iCurBtnIdx != ptBtnQue->lastBtnIdx)
			{
				ShowButton(ptBtnQue->tBtn[ptBtnQue->lastBtnIdx].ptTsObject, BTN_NORMAL);	
				ptBtnQue->lastBtnIdx = BUTTON_NONE;
				process = 0;
				pressCount = 0;
			}
		}
		else if(sta == TS_RELEASE)
		{
			ShowButton(ptBtnQue->tBtn[ptBtnQue->lastBtnIdx].ptTsObject, BTN_NORMAL);
			if(ptBtnQue->tBtn[ptBtnQue->lastBtnIdx].ReleaseHandler != NULL)
				ptBtnQue->tBtn[ptBtnQue->lastBtnIdx].ReleaseHandler(ptBtnQue->lastBtnIdx);

			ptBtnQue->lastBtnIdx = BUTTON_NONE;
			process = 0;
			pressCount = 0;
		}
	}
}

void ButtonEventProc(TS_DEV_T *ptTsDev)
{
	static int  lastSta=TS_RELEASE;
	int 		sta;
	BTN_QUE_T 	*ptBtnQue;
	TS_SAMPLE_T s;
	int 		i;
	int 		ret;
	static int lang = 0;
	
	if(ptTsDev->isOwnerSwitch == 1)
	{
		/* clear ruins of old buttons */
		if(ptTsDev->lastOwner == TS_OWNER_SYS && ptTsDev->currOwner != TS_OWNER_SHARE)
		{
			ptBtnQue = GetQueue(ptTsDev->lastOwner);
			ptBtnQue->refresh = 1;
			ptBtnQue->lastBtnIdx = BUTTON_NONE;
		
			for(i=0; i<MAX_BTN_CNT; i++) 
			{
				if(ptBtnQue->tBtn[i].isOccupied == 0) continue ;
				ptBtnQue->tBtn[i].isEnabled = 0;
				ptBtnQue->tBtn[i].isRefresh = 1;	
			}
			ButtonRefresh(ptBtnQue);
			ptBtnQue->refresh = 0;
			s_SetLcdInfo(0, 1); /* extend the size of screen for app */
		}

		/* recover new buttons */
		if(ptTsDev->currOwner == TS_OWNER_SYS && ptTsDev->lastOwner != TS_OWNER_SHARE)
		{
			ptBtnQue = GetQueue(ptTsDev->currOwner);
			ptBtnQue->refresh = 1;
			ptBtnQue->lastBtnIdx = BUTTON_NONE;

			for(i=0; i<MAX_BTN_CNT; i++)
			{
				if(ptBtnQue->tBtn[i].isOccupied == 0) continue ;
				ptBtnQue->tBtn[i].isEnabled = 1;
				ptBtnQue->tBtn[i].isRefresh = 1;
			}
			ButtonRefresh(ptBtnQue);
			ptBtnQue->refresh = 0;
			s_SetLcdInfo(0, 0); /* trim the size of screen for app */
			lastSta = TS_RELEASE;
		}
	}
    
	if(ptTsDev->currOwner != TS_OWNER_SYS && ptTsDev->currOwner != TS_OWNER_SHARE) return ;
    
    if(lang != bIsChnFont) /* button refresh when font update */
	{
		lang = bIsChnFont;
		if(ptTsDev->currOwner == TS_OWNER_SYS || ptTsDev->currOwner == TS_OWNER_SHARE)
		{
			ptBtnQue = GetQueue(ptTsDev->currOwner);
			ptBtnQue->refresh = 1;
			ptBtnQue->lastBtnIdx = BUTTON_NONE;

			for(i=0; i<MAX_BTN_CNT; i++)
			{
				if(ptBtnQue->tBtn[i].isOccupied == 0) continue ;
				ptBtnQue->tBtn[i].isEnabled = 1;
				ptBtnQue->tBtn[i].isRefresh = 1;
			}
			ButtonRefresh(ptBtnQue);
			ptBtnQue->refresh = 0;
		}
	}
    
	ptBtnQue = GetQueue(ptTsDev->currOwner);
	if(ptBtnQue->refresh) /* btn state changed */
	{
		ptBtnQue->lastBtnIdx = BUTTON_NONE;
		ButtonRefresh(ptBtnQue);
		ptBtnQue->refresh = 0;
	}

	ret = ts_get(&s, &gtBtnPpSample);
	if(ret == 0) return ;  /* fifo empty */

	if(s.pressure != 0) 
	{
		if(lastSta == TS_RELEASE) sta = TS_TRIGGER;
		else sta = TS_PRESSING;
	}
	else sta = TS_RELEASE;

	/* back up state for discriminating the first point */
	lastSta = sta;
	ButtonEventCheck(ptBtnQue, &s, sta);
	return ;
}


/* do all the ts event here */
int s_TsCheck(void)
{
	TS_DEV_T *ptTsDev = &gtTsDev;
	TS_SAMPLE_T 		s;
	uchar				isValidSamp;
	int 				ret;

	/* sample out here ought to be a perfect point */
	ret = ts_read(&s, 0);
	if(ret > 0) isValidSamp = 1; 
	else isValidSamp = 0;
	
	if(isValidSamp)
	{
		if(ptTsDev->isSKeySupported == 1)
		{
			if(ptTsDev->currOwner == TS_OWNER_SYS || ptTsDev->currOwner == TS_OWNER_SHARE)
				ts_put(&s, &gtBtnPpSample);
		}

		if(ptTsDev->lock == 1) ts_put(&s, &gtAppPpSample);
	}

	if(ptTsDev->isSKeySupported == 1) ButtonEventProc(&gtTsDev);

	/* owner switch flag clear here */
	if(ptTsDev->isOwnerSwitch == 1)	ptTsDev->isOwnerSwitch = 0;
    return isValidSamp;
}

void GlobalTsEventCheck(void)
{
    int ret;
    while(1)
    {
        ret = s_TsCheck();
        if(ret == 0) OsSleep(1);
    }
}


//--------------------------------------------------------------------
//------------------------ Instantiate   buttons  --------------------
//--------------------------------------------------------------------

/* lcd size */
#define LCD_Y				320
#define LCD_X				240

extern const IMG_INFO_T tUpArrowNor;
extern const IMG_INFO_T tUpArrowPre;
extern const IMG_INFO_T tDwnArrowNor;
extern const IMG_INFO_T tDwnArrowPre;
extern const IMG_INFO_T tMenuNor;
extern const IMG_INFO_T tMenuPre;
extern const IMG_INFO_T tCMenuNor;
extern const IMG_INFO_T tCMenuPre;

const IMG_INFO_T* tUpBtn_ALL[BTN_STA_LIMIT] = { &tUpArrowNor, &tUpArrowPre, 	&tUpArrowNor, &tUpArrowPre };
const IMG_INFO_T* tDwnBtn_ALL[BTN_STA_LIMIT] = { &tDwnArrowNor, &tDwnArrowPre, &tDwnArrowNor, &tDwnArrowPre };
const IMG_INFO_T* tMenuBtn_ALL[BTN_STA_LIMIT] = { &tMenuNor, 	&tMenuPre, 		&tCMenuNor,    &tCMenuPre };

static TS_OBJECT_T button_up = {
	.tsKeyNum = KEYUP,
	.szText = NULL,
	.alignment = ALIGN_MMID,
	.left = 0,
	.top = LCD_Y-60,
	.right = 80-1,
	.bottom = LCD_Y-1,
	.foreground = RGB(0x00, 0x00, 0x00),
	.background = tUpBtn_ALL,
	.colorscheme = BTN_IMG_COVER,
};

static TS_OBJECT_T button_down = {
	.tsKeyNum = KEYDOWN,
	.szText = NULL,
	.alignment = ALIGN_MMID,
	.left = 80,
	.top = LCD_Y-60,
	.right = 80+80-1,
	.bottom = LCD_Y-1,
	.foreground = RGB(0x00, 0x00, 0x00),
	.background = tDwnBtn_ALL,
	.colorscheme = BTN_IMG_COVER,
};

static TS_OBJECT_T button_menu = {
	.tsKeyNum = KEYMENU,
	.szText = NULL,
	.alignment = ALIGN_MMID,
	.left = 80+80,
	.top = LCD_Y-60,
	.right = 80+80+80-1,
	.bottom = LCD_Y-1,
	.foreground = RGB(0x00, 0x00, 0x00),
	.background = tMenuBtn_ALL,
	.colorscheme = BTN_IMG_COVER,
};

extern unsigned char    k_ScrBackLightMode;
extern volatile unsigned char   k_KBLightMode;

void GenericBtnTriggerEvent(int idx)
{
	BTN_QUE_T	*ptQue;

	if(k_ScrBackLightMode < 2)    ScrBackLight(1);
	if(k_KBLightMode < 2)         kblight(1);

	ptQue = GetQueue(gtTsDev.currOwner);
	putkey(ptQue->tBtn[idx].ptTsObject->tsKeyNum);

	/* start timer right now */
	s_KeySoundUnblock();
}

void GenericBtnReleaseEvent(int idx)
{
	
}

void SoftKeyInit(void)
{
	int i;

	/* queue init */
	memset(&gtSysBtnQue, 0, sizeof(BTN_QUE_T));
	for(i=0; i<MAX_BTN_CNT; i++)
	{
		gtSysBtnQue.tBtn[i].TriggerHandler = GenericBtnTriggerEvent;
		gtSysBtnQue.tBtn[i].ReleaseHandler = GenericBtnReleaseEvent;
		gtSysBtnQue.tBtn[i].isRefresh = 0;
		gtSysBtnQue.tBtn[i].isEnabled = 0;
		gtSysBtnQue.tBtn[i].isOccupied = 0;
	}	

	/* sys queue register and enable */
	TsObjectRegister(&button_up);
	TsObjectRegister(&button_down);
	TsObjectRegister(&button_menu);
	TsObjectEnable(&button_up);
	TsObjectEnable(&button_down);
	TsObjectEnable(&button_menu);
	return ;
}

/* the end of button.c */

