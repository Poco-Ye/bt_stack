#ifndef __BUTTON_H__
#define __BUTTON_H__

#define MAX_BTN_CNT		4	

typedef enum
{
	BTN_NORMAL_EN=0,
	BTN_PRESS_EN,
	BTN_NORMAL_CN,
	BTN_PRESS_CN,
	BTN_STA_RES,
	BTN_STA_LIMIT,
}BTN_COVER_LIST;

typedef enum
{
	BTN_NORMAL=0,
	BTN_PRESS,
}BTN_STATUS;

enum BTN_COLOR_SCHEME
{
	BTN_DEFAULT_COVER=0,
	BTN_PURE_COVER,
	BTN_IMG_COVER,
};

enum ALIGNMENT
{
	ALIGN_MMID=0,
	ALIGN_LMID,
	ALIGN_RMID,
	ALIGN_LTOP,
	ALIGN_MTOP,
	ALIGN_RTOP,
	ALIGN_LBOM,
	ALIGN_MBOM,
	ALIGN_RBOM,
};

typedef struct 
{
	int width;
	int high;
	const unsigned char *data;
}IMG_INFO_T;

typedef struct 
{
	int tsObjectID;
	unsigned char tsKeyNum;
	char *szText;
	unsigned char alignment;
	int left;
	int top;
	int right;
	int bottom;
	int foreground;
	const IMG_INFO_T **background;
	int colorscheme; 
}TS_OBJECT_T;

typedef struct 
{
	int isOccupied;
	int isEnabled;
	int isRefresh;
	TS_OBJECT_T *ptTsObject;
	void (*TriggerHandler)(int idx);
	void (*DoingHandler)(int idx);
	void (*ReleaseHandler)(int idx);
}BUTTON_T;

typedef struct 
{
	BUTTON_T tBtn[MAX_BTN_CNT];
	int refresh;
	int lastBtnIdx;
}BTN_QUE_T;

#endif /* __BUTTON_H__ */
