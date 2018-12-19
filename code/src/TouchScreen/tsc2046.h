#ifndef __TSC2046_H__
#define __TSC2046_H__

#define TS_CMD_X               5
#define TS_CMD_Y               1
#define TS_CMD_Z1              3
#define TS_CMD_Z2              4
#define TS_CMD_TEMP0           0
#define TS_CMD_TEMP1           7
#define TS_CMD_BAT             2
#define TS_CMD_AUX             6
#define Color_R(c)		(((ushort)(c) & 0x00f8) << 8)
#define Color_G(c)		(((ushort)(c) & 0x00fc) << 3)
#define Color_B(c)		(((ushort)(c) & 0x00f8) >> 3)

typedef struct{
	int clk_port;
	int miso_port;
	int mosi_port;
	int cs_port;
	int irq_port;
	int busy_port;
	
	int clk_pin;
	int miso_pin;
	int mosi_pin;
	int cs_pin;
	int irq_pin;
	int busy_pin;
}T_TsDrvConfig;

/*  port definition of ts2046 chip */
#define  TS2046_CLK	Sxxx_Ts_Config->clk_port,Sxxx_Ts_Config->clk_pin
#define  TS2046_MISO	Sxxx_Ts_Config->miso_port,Sxxx_Ts_Config->miso_pin
#define  TS2046_MOSI	Sxxx_Ts_Config->mosi_port,Sxxx_Ts_Config->mosi_pin
#define  TS2046_nCS	Sxxx_Ts_Config->cs_port,Sxxx_Ts_Config->cs_pin
#define  TS2046_nPENIRQ	Sxxx_Ts_Config->irq_port,Sxxx_Ts_Config->irq_pin
#define  TS2046_BUSY	Sxxx_Ts_Config->busy_port,Sxxx_Ts_Config->busy_pin

#define  TS_CTRL_BIT_START  7
#define  TS_CTRL_BIT_ADDR	4

#define TS_TM035KBH08 1
#define TS_S98037A 2

#endif /* end of __TS_H__ */

