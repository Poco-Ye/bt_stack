#include "base.h"
#include "..\lcd\Lcdapi.h"
#include "..\lcd\LcdDrv.h"
#include "..\TouchScreen\button.h"

extern const IMG_INFO_T tPhoneEmpty, tPhoneHookOff, tPhoneHookOn;
extern const IMG_INFO_T tSignalLevelE, tSignalLevel0, tSignalLevel1, tSignalLevel2, tSignalLevel3, tSignalLevel4, tSignalLevel5;
extern const IMG_INFO_T tPrinter0, tPrinter1;
extern const IMG_INFO_T tIcCard0, tIcCard1;
extern const IMG_INFO_T tLock0, tLock1;
extern const IMG_INFO_T tUp0, tUp1, tDown0, tDown1;
extern const IMG_INFO_T tBatteryE, tBattery0, tBattery1, tBattery2, tBattery3, tBattery4,tAdapter;
extern const IMG_INFO_T vled_blue_data, vled_green_data, vled_red_data, vled_yellow_data, vled_off_data;
extern const IMG_INFO_T tStartLogo,tVanstone_logo;
extern const IMG_INFO_T tWifi0, tWifi1, tWifi2, tWifi3;
extern const IMG_INFO_T tBluetooth0, tBluetooth1;
extern const IMG_INFO_T tlinkoff_gray,tlinkup_blue;
extern const IMG_INFO_T tbattery_icon_240x320_1, tbattery_icon_240x320_2,tbattery_icon_240x320_3, tbattery_icon_240x320_4, tbattery_icon_240x320_5;
extern const IMG_INFO_T tbattery_icon_320x240_1, tbattery_icon_320x240_2,tbattery_icon_320x240_3, tbattery_icon_320x240_4, tbattery_icon_320x240_5;

static const IMG_INFO_T *icon_list_phone[] = {&tPhoneEmpty, &tPhoneHookOn, &tPhoneHookOff};
static const IMG_INFO_T *icon_list_printer[] = {&tPrinter0, &tPrinter1};
static const IMG_INFO_T *icon_list_iccard[] = {&tIcCard0, &tIcCard1};
static const IMG_INFO_T *icon_list_lock[] = {&tLock0, &tLock1};
static const IMG_INFO_T *icon_list_up[] = {&tUp0, &tUp1};
static const IMG_INFO_T *icon_list_down[] = {&tDown0, &tDown1};
static const IMG_INFO_T *icon_list_signal[] = {
    &tSignalLevelE, &tSignalLevel0, &tSignalLevel1, &tSignalLevel2, &tSignalLevel3, &tSignalLevel4, &tSignalLevel5	
};
static const IMG_INFO_T *icon_list_batt[] = {
    &tAdapter, &tBattery0, &tBattery1, &tBattery2, &tBattery3, &tBattery4, &tBatteryE
};

static const IMG_INFO_T *icon_list_wifi[] = {&tWifi0, &tWifi1, &tWifi2, &tWifi3};
static const IMG_INFO_T *icon_list_bt[] = {&tBluetooth0, &tBluetooth1};

static const IMG_INFO_T *icon_list_eth[] = {&tlinkoff_gray, &tlinkup_blue};

static const IMG_INFO_T** icon_list[]={
                                    icon_list_phone , 
                                    icon_list_signal, 
                                    icon_list_printer, 
                                    icon_list_iccard, 
                                    icon_list_lock, 
                                    icon_list_batt,
                                    icon_list_up,
                                    icon_list_down,
                                    NULL,
                                    NULL,
                                    icon_list_bt,
                                    icon_list_wifi,
                                    icon_list_eth,
                                };
static IMG_INFO_T* icon_list_batteryicon[]={NULL, NULL, NULL, NULL, NULL};
extern int lcd_logic_max_x;
extern int lcd_logic_max_y;

void DrawIcon(int x, int y, const IMG_INFO_T *icon)
{
	int height= icon->high;
	int width = icon->width;
	s_LcdDrawIcon(x, y, width, height, icon->data);
}

void s_LcdSetIcon(uchar icon_no, uchar mode)
{
    int distX, height, width;

  	if ((icon_no == ICON_DOWN || icon_no == ICON_UP) &&
		(is_wifi_module() || is_bt_module()))	//has wifi or bt, don't update the down & up
  	{
		return;
	}
	if ((icon_no == ICON_BT || icon_no == ICON_WIFI) &&
		!(is_wifi_module() || is_bt_module()))	//no wifi and bt, don't update the wifi & bt
  	{
		return;
	}
	if((icon_no == ICON_LOCK) && (is_hasbase() || is_eth_module()))		// has eth, don't update the lock
	{
		return;
	}
	if((icon_no == ICON_ETH) && ((!is_hasbase()) && (!is_eth_module())))		// no eth, don't update the eth
	{
		return;
	}
	if (icon_no == ICON_BT)
		distX = (ICON_UP-1)*(lcd_logic_max_x)/(8);      //bt use the offset of up,
	else if (icon_no == ICON_WIFI)
    	distX = (ICON_DOWN-1)*(lcd_logic_max_x)/(8);	  //wifi use the offset of udown,
    else if (icon_no == ICON_ETH)
    	distX = (ICON_LOCK-1)*(lcd_logic_max_x)/(8);	// eht use the offset of lock
	else
    	distX = (icon_no-1)*(lcd_logic_max_x)/(8);
	
	if (icon_list[icon_no-1] == NULL) 
		return;
	
    height = icon_list[icon_no-1][mode]->high;
    width = icon_list[icon_no-1][mode]->width;
    s_LcdDrawIcon(distX, 10, width, height, icon_list[icon_no-1][mode]->data);
}

/////////////////////////////////////////////////////////////
// 下面以图标的形式显示虚拟led灯。

struct vled_status_t {
	int red;
	int green;
	int blue;
	int yellow;
	int opened;
};

static struct vled_status_t vled_sta = {0, 0, 0, 0, 0};

static void vled_blue(int on)
{
    int x,y;
	const IMG_INFO_T *p = (on) ? &vled_blue_data : &vled_off_data;

    if(WIDTH_FOR_TFT_S800 == lcd_logic_max_x)
    {
        x = 20;
        y = 40;
    }
    else
    {
        x = 10;
        y = 40;
    }
	DrawIcon(x, y, p);
}

static void vled_green(int on)
{
    int x,y;
	const IMG_INFO_T *p = (on) ? &vled_green_data : &vled_off_data;

    if(WIDTH_FOR_TFT_S800 == lcd_logic_max_x)
    {
        x = 180;
        y = 40;
    }
    else
    {
        x = 130;
        y = 40;
    }
    
	DrawIcon(x, y, p);
}

static void vled_red(int on)
{
    int x,y;
	const IMG_INFO_T *p = (on) ? &vled_red_data : &vled_off_data;

    if(WIDTH_FOR_TFT_S800 == lcd_logic_max_x)
    {
        x = 260;
        y = 40;
    }
    else
    {
        x = 190;
        y = 40;
    }
	DrawIcon(x, y, p);
}

static void vled_yellow(int on)
{
    int x,y;
	const IMG_INFO_T *p = (on) ? &vled_yellow_data : &vled_off_data;

    if(WIDTH_FOR_TFT_S800 == lcd_logic_max_x)
    {
        x = 100;
        y = 40;
    }
    else
    {
        x = 70;
        y = 40;
    }
	DrawIcon(x, y, p);
}

static void vled_draw_background(int color)
{
	s_ScrRect(0, 40, lcd_logic_max_x-1, 79, color, 1);
}

void vled_open()
{
	if(vled_sta.opened != 0) return;

	vled_sta.opened = 1;

	// 窗口下移
	ScrRestore(0);
	ScrCls();
	s_SetLcdInfo(1, 1);
	ScrRestore(1);

	// led弹出
	vled_draw_background(COLOR_WHITE);
	vled_blue(vled_sta.blue);
	vled_green(vled_sta.green);
	vled_yellow(vled_sta.yellow);
	vled_red(vled_sta.red);
}

void vled_close()
{
	if(vled_sta.opened == 0)
		return;

	vled_sta.opened = 0;

	ScrRestore(0);
	ScrCls();
	s_SetLcdInfo(1, 0);
	vled_draw_background(COLOR_WHITE);
	ScrRestore(1);
}

/*
 * PiccLight():
 *     ucLedIndex: 灯索引，每位代表一种颜色的灯，
 *         BIT0:red   BIT1:green   BIT2:yellow   BIT3:blue   BIT4~BIT7 保留
 *
 *     ucOnOff: 点亮或熄灭标志, 0: 熄灭 非0:点亮.
 */
void s_VirtualPiccLight(uchar led, uchar on)
{
	vled_open();
	
	if((led & BIT0) != 0) vled_sta.red = 0;
	if((led & BIT1) != 0) vled_sta.green= 0;
	if((led & BIT2) != 0) vled_sta.yellow = 0;
	if((led & BIT3) != 0) vled_sta.blue = 0;

	if(vled_sta.opened != 0)
	{
		if((led & BIT0) != 0) vled_red(on);
		if((led & BIT1) != 0) vled_green(on);
		if((led & BIT2) != 0) vled_yellow(on);
		if((led & BIT3) != 0) vled_blue(on);
	}
}

void StartLogo(void)
{
    IMG_INFO_T *pt;
	if(s_CheckVanstoneOEM()) pt = (IMG_INFO_T *)&tVanstone_logo;
	else pt = (IMG_INFO_T *)&tStartLogo;
	
	if (lcd_logic_max_x == WIDTH_FOR_S300_S900)
	{
		DrawIcon((lcd_logic_max_x-pt->width)/2,(lcd_logic_max_y-pt->high)/2-20-60, pt);
	}
	else
	{
		DrawIcon((lcd_logic_max_x-pt->width)/2,(lcd_logic_max_y-pt->high)/2-20, pt);
	}
	
}
void s_IconConfig(int val)
{
    if (val) s_LcdClear();

    s_SetLcdInfo(2, val);
    return ;
}

typedef struct {
	int x;
	int y;
}ST_ICON_LOCATION;
ST_ICON_LOCATION BatteryIconLoc[10];
static int icon_inited = 0;
void BatterIconInit(int LcdType)
{
	if (LcdType == 0)
	{
		//LCDTYPE:320X240
		icon_list_batteryicon[0] = (IMG_INFO_T* )(&tbattery_icon_320x240_1);
		icon_list_batteryicon[1] = (IMG_INFO_T* )(&tbattery_icon_320x240_2);
		icon_list_batteryicon[2] = (IMG_INFO_T* )(&tbattery_icon_320x240_3);
		icon_list_batteryicon[3] = (IMG_INFO_T* )(&tbattery_icon_320x240_4);
		icon_list_batteryicon[4] = (IMG_INFO_T* )(&tbattery_icon_320x240_5);
		BatteryIconLoc[0].x = 80;
		BatteryIconLoc[0].y = 70;
		BatteryIconLoc[1].x = 80;
		BatteryIconLoc[1].y = 80;
		BatteryIconLoc[2].x = 91;
		BatteryIconLoc[2].y = 161;
		BatteryIconLoc[3].x = 219;
		BatteryIconLoc[3].y = 80;
		BatteryIconLoc[4].x = 91;
		BatteryIconLoc[4].y = 80;
		BatteryIconLoc[5].x = 91+32;
		BatteryIconLoc[5].y = 80;
		BatteryIconLoc[6].x = 91+32*2;
		BatteryIconLoc[6].y = 80;
		BatteryIconLoc[7].x = 91+32*3;
		BatteryIconLoc[7].y = 80;
		BatteryIconLoc[8].x = 91;
		BatteryIconLoc[8].y = 80;
		BatteryIconLoc[9].x = 91+32*4-1;
		BatteryIconLoc[9].y = 160;
	}
	else
	{
		//LCDTYPE:240X320
		icon_list_batteryicon[0] = (IMG_INFO_T* )(&tbattery_icon_240x320_1);
		icon_list_batteryicon[1] = (IMG_INFO_T* )(&tbattery_icon_240x320_2);
		icon_list_batteryicon[2] = (IMG_INFO_T* )(&tbattery_icon_240x320_3);
		icon_list_batteryicon[3] = (IMG_INFO_T* )(&tbattery_icon_240x320_4);
		icon_list_batteryicon[4] = (IMG_INFO_T* )(&tbattery_icon_240x320_5);
		BatteryIconLoc[0].x = 70;
		BatteryIconLoc[0].y = 80;
		BatteryIconLoc[1].x = 80;
		BatteryIconLoc[1].y = 229;
		BatteryIconLoc[2].x = 161;
		BatteryIconLoc[2].y = 80;
		BatteryIconLoc[3].x = 80;
		BatteryIconLoc[3].y = 80;
		BatteryIconLoc[4].x = 80;
		BatteryIconLoc[4].y = 197;
		BatteryIconLoc[5].x = 80;
		BatteryIconLoc[5].y = 197-32;
		BatteryIconLoc[6].x = 80;
		BatteryIconLoc[6].y = 197-32*2;
		BatteryIconLoc[7].x = 80;
		BatteryIconLoc[7].y = 197-32*3;
		BatteryIconLoc[8].x = 80;
		BatteryIconLoc[8].y = 197-32*3;
		BatteryIconLoc[9].x = 160;
		BatteryIconLoc[9].y = 228;
	}
	icon_inited = 1;
}
void DrawBatteryIcon(uint index)
{
	if (!icon_inited) return;
	if (index < 4)
	{
		s_ScrDrawBmp(BatteryIconLoc[index].x, BatteryIconLoc[index].y,
			icon_list_batteryicon[index]->data);
	}
	else
	{
		s_ScrDrawBmp(BatteryIconLoc[index].x, BatteryIconLoc[index].y,
			icon_list_batteryicon[4]->data);
	}
}
void s_FillBatteryIcon(void)
{
	if (!icon_inited) return;
	s_ScrRect(BatteryIconLoc[8].x, BatteryIconLoc[8].y, 
				BatteryIconLoc[9].x, BatteryIconLoc[9].y, COLOR_BLACK, 1);
}
