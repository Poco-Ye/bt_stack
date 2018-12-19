#ifndef _PMU_H
#define _PMU_H
#include "base.h"
#include "pmui2c.h"
#define PMU_SLAVER 0x64
#define PMU_ADDR_PCCNT 0x00
#define PMU_ADDR_PCST 0x01
#define PMU_ADDR_VDCTRL 0x02
#define PMU_ADDR_LDOON 0x03
#define PMU_ADDR_LDO2DAC 0x04
#define PMU_ADDR_LDO3DAC 0x05
#define PMU_ADDR_LDO4DAC 0x06
#define PMU_ADDR_LDO5DAC 0x07
#define PMU_ADDR_LDO6DAC 0x08
#define PMU_ADDR_LDO7DAC 0x09
#define PMU_ADDR_LDO8DAC 0x0A
#define PMU_ADDR_DDCTL1 0x10
#define PMU_ADDR_DDCTL2 0x11
#define PMU_ADDR_RAMP1CTL 0x12
#define PMU_ADDR_RAMP2CTL 0x13
#define PMU_ADDR_DD1DAC 0x14
#define PMU_ADDR_DD2DAC 0x15
#define PMU_ADDR_CHGSTART 0x20
#define PMU_ADDR_FET1CNT 0x21
#define PMU_ADDR_FET2CNT 0x22
#define PMU_ADDR_TSET 0x23
#define PMU_ADDR_CMPSET 0x24
#define PMU_ADDR_SUSPEND 0x25
#define PMU_ADDR_CHGSTATE 0x26
#define PMU_ADDR_CHGEN1 0x28
#define PMU_ADDR_CHGIR1 0x29
#define PMU_ADDR_CHGMONI 0x2A
#define PMU_ADDR_CHGEN2 0x2C
#define PMU_ADDR_CHGIR2 0x2D

#define PMU_LDO2ON		(1<<1) // VOUT2
#define PMU_LDO3ON		(1<<2)
#define PMU_LOD4ON		(1<<3)
#define PMU_LOD5ON		(1<<4)
#define PMU_LOD6ON		(1<<5)
#define PMU_LOD7ON		(1<<6) // VOUT7

//#define PMU_BEEP_POWER				PMU_LOD5ON
#define PMU_WIFI_POWER				PMU_LOD5ON
#define PMU_TOUCHKEY_POWER			PMU_LOD6ON
//#define PMU_LCD_BACKLIGHT_POWER		PMU_LOD7ON
#define PMU_BT_POWER				PMU_LOD7ON

#define PMU_CHGSTATE_OFF		0x00
#define PMU_CHGSTATE_RAPID		0x04
#define PMU_CHGSTATE_COMPLETE	0x05
#define PMU_CHGSTATE_NOBATTERY	0x09
int s_PmuWrite(uchar addr, uchar *data, int len);
int s_PmuRead(uchar addr, uchar *data, int len);
void s_PowerSwitch(uchar vout, uchar OnOff);
void s_PmuWifiPowerSwitch(int OnOff);
void s_PmuInit(void);
void s_PmuSetChargeCurrent(int mode);

#endif
