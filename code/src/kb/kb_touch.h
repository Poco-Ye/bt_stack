#ifndef _KB_TOUCH_KEY_H_
#define _KB_TOUCH_KEY_H_

#define KB_INT_GPIO    GPIOB,5
#define WLAN_EN_KEY GPIOB,24

//#define DEBUG_TOUCHKEY

#define KB_I2C_SLAVER_WRITE			0x0A
#define KB_I2C_SLAVER_READ			0x0B
#define KB_I2C_ADDR_TOUCH_ENABLE	0x00
#define KB_I2C_ADDR_KBBL			0x01
#define KB_I2C_ADDR_TOUCHKEY_VALUE	0x02
#define KB_I2C_ADDR_VERSION			0x04
#define KB_I2C_ADDR_BASELINE		0x05
#define KB_I2C_ADDR_INITBASELINE	0x14
#define KB_I2C_VALUE_TOUCH_ENABLE	0x01
#define KB_I2C_VALUE_TOUCH_DISABLE	0x00
#define KB_I2C_VALUE_KBBL_ON		0x01	
#define KB_I2C_VALUE_KBBL_OFF		0x00

void s_TouchKeySetBaseLine(void);
void s_TouchKeyStart(void);
void s_TouchKeyStop(void);
void s_TouchKeyInitBaseLine(void);
void s_TouchKeyBLightCtrl(unsigned char mode);
unsigned char s_TouchKeyVersion(void);


#endif

