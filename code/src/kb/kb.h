#ifndef  _KB_H
#define  _KB_H


/*
//GPIO0[6]	        KEY-O0£º°´¼üÉ¨ÃèÊä³ö¶Ë¿Ú1
//GPIO0[7]	        KEY-O1£º°´¼üÉ¨ÃèÊä³ö¶Ë¿Ú2
//GPIO0[8]	        KEY-O2£º°´¼üÉ¨ÃèÊä³ö¶Ë¿Ú3
//GPIO0[9]	        KEY-O3£º°´¼üÉ¨ÃèÊä³ö¶Ë¿Ú4
//GPIO0[1]	        KEY-I0£º°´¼üÉ¨ÃèÊäÈë¶Ë¿Ú1
//GPIO0[2]	        KEY-I1£º°´¼üÉ¨ÃèÊäÈë¶Ë¿Ú2
//GPIO0[3]	        KEY-I2£º°´¼üÉ¨ÃèÊäÈë¶Ë¿Ú3
//GPIO0[4]	        KEY-I3£º°´¼üÉ¨ÃèÊäÈë¶Ë¿Ú4
//GPIO0[5]	        KEY-I4£º°´¼üÉ¨ÃèÊäÈë¶Ë¿Ú5
//GPIO0[10]          LCD-LED: ¿ØÖÆ±³¹âÐÅºÅ
*/

//Defined for input key values
//#define KEYF1		0x01
//#define KEYF2		0x02
//#define KEYF3		0x03
//#define KEYF4		0x04
//#define KEYF5	    0x09
//#define KEYF6	    0x0a
//
//#define KEY1        0x31
//#define KEY2        0x32
//#define KEY3        0x33
//#define KEY4        0x34
//#define KEY5        0x35
//#define KEY6        0x36
//#define KEY7        0x37
//#define KEY8        0x38
//#define KEY9        0x39
//#define KEY0        0x30
//#define KEY00       0x3a
//
//#define KEYCLEAR    0x08
//#define KEYALPHA    0x07
//#define KEYUP       0x05
//#define KEYDOWN     0x06
//#define KEYFN       0x15
//#define KEYMENU     0x14
//#define KEYENTER    0x0d
//#define KEYCANCEL   0x1b
//
//#define KEYSETTLE   0x16
//#define KEYVOID     0x17
//#define KEYREPRINT  0x18
//#define KEYPRNUP    0x19
//#define KEYPRNDOWN  0x1a
//#define NOKEY       0xff


void  s_KbInit(void);
void s_KeyCombConfig(int value);
void s_EnterKeyCombConfig(int value);

typedef struct {
	int pwr_key_port;
	int pwr_key_pin;
	int pwr_hold_port;
	int pwr_hold_pin;
}T_PowerDrvConfig;
extern T_PowerDrvConfig *gzPwrDrvCfg;
#define POWER_KEY 		gzPwrDrvCfg->pwr_key_port, gzPwrDrvCfg->pwr_key_pin
#define POWER_HOLD		gzPwrDrvCfg->pwr_hold_port, gzPwrDrvCfg->pwr_hold_pin
#endif

