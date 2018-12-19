#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "Lcdapi.h"
#include "base.h"
#include "LcdDrv.h"
#include "..\Cfgmanage\cfgmanage.h"
#include "clcdapi.h"

//for S500,S58Q,S90Q,D200,D210,S800
static LCDPARAM lcdparam_320X240 = {
	.PPL = 240,
	.HBP = 0x48,
	.HFP = 0x48,
	.HSW = 10,
	.LPP = 320,
	.VSW = 2,
	.VBP = 2,
	.VFP = 4,
	.LED = 5
};

//for S300,S900
static LCDPARAM lcdparam_240X320 = {
	.PPL = 320,
	.HBP = 69,
	.HFP = 18,
	.HSW = 1,
	.LPP = 240,
	.VSW = 1,
	.VBP = 13,
	.VFP = 10,
	.LED = 5
};

//for S500,S58Q,S90Q,D210,S800
static int gray_320X240[MAX_GRAY+1] = {0, 1, 2, 3, 5, 7, 9,13,18,24,28,34,40};
//for D200
static int gray_320X240_2[MAX_GRAY+1] = {40, 34, 28, 24, 18, 13, 9, 7, 5, 3, 2, 1, 0};
//for S300,S900
static int gray_240X320[MAX_GRAY+1] = {0,12,13,14,15,17,19,23,26,29,32,35,40};

typedef struct {
	void (*s_LcdPhyInit)(void);
	void (*s_LcdSetGram)(int,int,int,int);
	void (*s_LcdWrLData)(ushort);	
	void (*s_LcdReadPixel)(int,int,int *);
	void (*s_LcdFillColor)(int,int,int,int,ushort);	
	void (*s_LcdFillBuf)(int,int,int,int,ushort *, ushort *, uchar *);	
	void (*s_LcdReset)(void);
	void (*s_LcdSleepIn)(void);
	void (*s_LcdSleepOut)(void);
}T_LcdDrvConfig;

void s_S90357_LcdPhyInit(void);
void s_TM035KBH08_LcdPhyInit(void);
void s_ST7789VRGB_LcdPhyInit(void);
void s_ST7789VRGB_TM_YB_LcdPhyInit(void);
void s_DmaLcd_SetGram(int scol, int sline, int ecol, int eline);
void s_TM035KBH08_Lcd_WrLData(ushort data);
void s_TM035KBH08_Lcd_FillColor(int scol, int sline, int ecol, int eline, ushort color);
void s_TM035KBH08_Lcd_FillBuf(int scol, int sline, int ecol, int eline, ushort *ForeBuf,ushort *BackBuf,uchar *AlphaBuf);
void s_TM035KBH08_Lcd_ReadPixel(int x, int y, int *Color);
void s_TM035KBH08_Reset(void);
void s_S90357_Lcd_WrLData(ushort data);
void s_S90357_Lcd_FillColor(int scol, int sline, int ecol, int eline, ushort color);
void s_S90357_Lcd_FillBuf(int scol, int sline, int ecol, int eline, ushort *ForeBuf,ushort *BackBuf,uchar *AlphaBuf);
void s_S90357_Lcd_ReadPixel(int x, int y, int *Color);

void ili9341_reset(void);
void s_ST7789VRGB_Reset(void);
void s_ST7789VRGB_SleepIn(void);
void s_ST7789VRGB_SleepOut(void);

T_LcdDrvConfig S300_S900_Lcd_Drv =
{
	.s_LcdPhyInit = s_TM035KBH08_LcdPhyInit,
	.s_LcdSetGram = s_DmaLcd_SetGram,
	.s_LcdWrLData = s_TM035KBH08_Lcd_WrLData,
	.s_LcdReadPixel = s_TM035KBH08_Lcd_ReadPixel,
	.s_LcdFillColor = s_TM035KBH08_Lcd_FillColor,
	.s_LcdFillBuf = s_TM035KBH08_Lcd_FillBuf,
	.s_LcdReset = s_TM035KBH08_Reset,
	.s_LcdSleepIn = NULL,
	.s_LcdSleepOut = NULL,
};
T_LcdDrvConfig S800_Lcd_Drv =
{
	.s_LcdPhyInit = s_S90357_LcdPhyInit,
	.s_LcdSetGram = s_DmaLcd_SetGram,
	.s_LcdWrLData = s_S90357_Lcd_WrLData,
	.s_LcdReadPixel = s_S90357_Lcd_ReadPixel,	
	.s_LcdFillColor = s_S90357_Lcd_FillColor,
	.s_LcdFillBuf = s_S90357_Lcd_FillBuf,
	.s_LcdReset = ili9341_reset,
	.s_LcdSleepIn = NULL,
	.s_LcdSleepOut = NULL,
};
T_LcdDrvConfig D210_Lcd_Drv =
{
	.s_LcdPhyInit = s_ST7789VRGB_LcdPhyInit,
	.s_LcdSetGram = s_DmaLcd_SetGram,
	.s_LcdWrLData = s_S90357_Lcd_WrLData,
	.s_LcdReadPixel = s_S90357_Lcd_ReadPixel,
	.s_LcdFillColor = s_S90357_Lcd_FillColor,
	.s_LcdFillBuf = s_S90357_Lcd_FillBuf,
	.s_LcdReset = s_ST7789VRGB_Reset,
	.s_LcdSleepIn = s_ST7789VRGB_SleepIn,
	.s_LcdSleepOut = s_ST7789VRGB_SleepOut,
};

T_LcdDrvConfig Tft_ST7789V_TM_YB_Drv =
{
	.s_LcdPhyInit = s_ST7789VRGB_TM_YB_LcdPhyInit,
	.s_LcdSetGram = s_DmaLcd_SetGram,
	.s_LcdWrLData = s_S90357_Lcd_WrLData,
	.s_LcdReadPixel = s_S90357_Lcd_ReadPixel,
	.s_LcdFillColor = s_S90357_Lcd_FillColor,
	.s_LcdFillBuf = s_S90357_Lcd_FillBuf,
	.s_LcdReset = s_ST7789VRGB_Reset,
	.s_LcdSleepIn = s_ST7789VRGB_SleepIn,
	.s_LcdSleepOut = s_ST7789VRGB_SleepOut,
};

void s_HX8347_PhyInit(void);
void s_ILI9341_PhyInit(void);
void s_ILIXXXX_SleepIn(void);
void s_ILIXXXX_SleepOut(void);
void s_TM023KDZ06_PhyInit(void);
void s_H23B13_PhyInit(void);
void s_ST7789V_PhyInit(void);
void s_HX8347_SetGram(int scol, int sline, int ecol, int eline);
void s_ILIXXXX_SetGram(int scol, int sline, int ecol, int eline);
void s_ST7789V_SetGram(int scol, int sline, int ecol, int eline);
void s_Tft_WrLData(ushort data);
void s_Tft_FillColor(int scol, int sline, int ecol, int eline, ushort color);
void s_Tft_FillBuf(int scol, int sline, int ecol, int eline, ushort *ForeBuf,ushort *BackBuf,uchar *AlphaBuf);
void s_TftReadPixel(int x, int y, int * Color);
T_LcdDrvConfig Tft_HX8347_Drv =
{
	.s_LcdPhyInit = s_HX8347_PhyInit,
	.s_LcdSetGram = s_HX8347_SetGram,
	.s_LcdWrLData = s_Tft_WrLData,
	.s_LcdReadPixel = s_TftReadPixel,
	.s_LcdFillColor = s_Tft_FillColor,
	.s_LcdFillBuf = s_Tft_FillBuf,
	.s_LcdReset = NULL,
	.s_LcdSleepIn = NULL,
	.s_LcdSleepOut = NULL,
};
T_LcdDrvConfig Tft_ILI9341_Drv =
{
	.s_LcdPhyInit = s_ILI9341_PhyInit,
	.s_LcdSetGram = s_ILIXXXX_SetGram,
	.s_LcdWrLData = s_Tft_WrLData,
	.s_LcdReadPixel = s_TftReadPixel,
	.s_LcdFillColor = s_Tft_FillColor,
	.s_LcdFillBuf = s_Tft_FillBuf,
	.s_LcdReset = NULL,
	.s_LcdSleepIn = s_ILIXXXX_SleepIn,
	.s_LcdSleepOut = s_ILIXXXX_SleepOut,
};
T_LcdDrvConfig Tft_TM023KDZ06_Drv =
{
	.s_LcdPhyInit = s_TM023KDZ06_PhyInit,
	.s_LcdSetGram = s_ILIXXXX_SetGram,
	.s_LcdWrLData = s_Tft_WrLData,
	.s_LcdReadPixel = s_TftReadPixel,
	.s_LcdFillColor = s_Tft_FillColor,
	.s_LcdFillBuf = s_Tft_FillBuf,
	.s_LcdReset = NULL,
	.s_LcdSleepIn = s_ILIXXXX_SleepIn,
	.s_LcdSleepOut = s_ILIXXXX_SleepOut,
};
T_LcdDrvConfig Tft_H23B13_Drv =
{
	.s_LcdPhyInit = s_H23B13_PhyInit,
	.s_LcdSetGram = s_ILIXXXX_SetGram,
	.s_LcdWrLData = s_Tft_WrLData,
	.s_LcdReadPixel = s_TftReadPixel,
	.s_LcdFillColor = s_Tft_FillColor,
	.s_LcdFillBuf = s_Tft_FillBuf,
	.s_LcdReset = NULL,
	.s_LcdSleepIn = s_ILIXXXX_SleepIn,
	.s_LcdSleepOut = s_ILIXXXX_SleepOut,
};
T_LcdDrvConfig Tft_ST7789V_Drv =
{
	.s_LcdPhyInit = s_ST7789V_PhyInit,
	.s_LcdSetGram = s_ST7789V_SetGram,
	.s_LcdWrLData = s_Tft_WrLData,
	.s_LcdReadPixel = s_TftReadPixel,
	.s_LcdFillColor = s_Tft_FillColor,
	.s_LcdFillBuf = s_Tft_FillBuf,
	.s_LcdReset = NULL,
	.s_LcdSleepIn = NULL,
	.s_LcdSleepOut = NULL,
};

static T_LcdDrvConfig *gstLcdDrv = NULL;
static LCDPARAM *lcdpara=NULL;
static int *gpLcdGray = NULL;
uchar get_lcd_type(void)
{
    char context[64];
    static uchar lcd_type,get_flag=0;

    if (get_flag) return lcd_type;
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("LCD",context)<0) lcd_type = DMA_LCD;
    else if(!strcmp(context, "TFT_H24C117-00N")) lcd_type = TFT_H24C117_LCD;
	else if(!strcmp(context, "LCD_001")) lcd_type = TFT_H24C117_LCD;
	else if(!strcmp(context, "LCD_002")) lcd_type = TFT_S90401_LCD;
	else if(!strcmp(context, "01")) lcd_type = TFT_H23B13_LCD;
	else if(!strcmp(context, "04")) lcd_type = TFT_H28C60_LCD;
	else if(!strcmp(context, "07")) lcd_type = DMA_S90357_LCD;
	else if(!strcmp(context, "08")) lcd_type = DMA_TM035KBH08_LCD;
	else if(!strcmp(context, "09")) lcd_type = TFT_TM023KDZ06_LCD;
	else if(!strcmp(context, "10")) lcd_type = DMA_H28C69_00N_LCD;
	else if(!strcmp(context, "21")) lcd_type = DMA_ST7789V_TM_YB_LCD;// add 1
    else lcd_type = DMA_LCD;
    get_flag = 1;
    return lcd_type;
}

static int giLcdIndexX;
static int giLcdIndexY;
static int giLcdMinX;
static int giLcdMinY;
static int giLcdMaxX;
static int giLcdMaxY;

//s58 s90 s500 D200 D210 lcd模块不能读取颜色值，使用软件备份颜色
static uchar gucLcdMem[TFT_MAX_Y][TFT_MAX_X*2];

// DMA LCD 物理缓冲区
ushort * const dma_lcd_buf = (ushort*)DMA_LCD_BASE;


/****************************************************************/
/****************************For TFT LCD*************************/
/****************************************************************/
#define s_HX8347_WriteReg(idx, dat) {s_TFT_WrCmd((uchar)(idx));s_TFT_WrData((uchar)(dat));}
static uchar s_HX8347_ReadReg(uchar idx)
{
	uchar tmp;
	s_TFT_WrCmd(idx);
	s_TFT_ReadData(&tmp);
	return tmp;
}

void s_Tft_WrLData(ushort data) 
{ 
	if (giLcdIndexY > giLcdMaxY) return;
	s_TFT_WrData(RGB_H(data)); 
	s_TFT_WrData(RGB_L(data)); 
	gucLcdMem[giLcdIndexY][giLcdIndexX*2] = RGB_H(data); 
	gucLcdMem[giLcdIndexY][giLcdIndexX*2+1] = RGB_L(data); 
	giLcdIndexX++; 
	if (giLcdIndexX > giLcdMaxX) 
	{ 
		giLcdIndexX = giLcdMinX; 
		giLcdIndexY++; 
	} 
}
void s_LcdSetGram(int scol, int sline, int ecol, int eline);
void s_Tft_FillColor(int scol, int sline, int ecol, int eline, ushort color) 
{ 
	s_LcdSetGram(scol, sline, ecol, eline);

	for (giLcdIndexY = giLcdMinY;giLcdIndexY <= giLcdMaxY;giLcdIndexY++)
	{
		for (giLcdIndexX = giLcdMinX;giLcdIndexX <= giLcdMaxX;giLcdIndexX++)
		{
			gucLcdMem[giLcdIndexY][giLcdIndexX*2] = RGB_H(color); 
			gucLcdMem[giLcdIndexY][giLcdIndexX*2+1] = RGB_L(color); 
			s_TFT_WrData(RGB_H(color)); 
			s_TFT_WrData(RGB_L(color)); 
		}
	}
}

void s_Tft_FillBuf
(int scol, int sline, int ecol, int eline, ushort *ForeBuf,ushort *BackBuf,uchar *AlphaBuf) 
{ 
	ushort color;
	int bufoffsety,bufoffset;
	s_LcdSetGram(scol, sline, ecol, eline);
	for (giLcdIndexY = giLcdMinY;giLcdIndexY <= giLcdMaxY;giLcdIndexY++)
	{
		bufoffsety = giLcdIndexY * lcdpara->LPP;
		for (giLcdIndexX = giLcdMinX;giLcdIndexX <= giLcdMaxX;giLcdIndexX++)
		{
			bufoffset = bufoffsety+giLcdIndexX;
			color = AlphaBuf[bufoffset] ? ForeBuf[bufoffset] : BackBuf[bufoffset];
			gucLcdMem[giLcdIndexY][giLcdIndexX*2] = RGB_H(color); 
			gucLcdMem[giLcdIndexY][giLcdIndexX*2+1] = RGB_L(color); 
			s_TFT_WrData(RGB_H(color)); 
			s_TFT_WrData(RGB_L(color)); 
		}
	}
}

void s_HX8347_SetGram(int scol, int sline, int ecol, int eline)
{
	s_HX8347_WriteReg(0x02, (uchar)(scol >> 8));
	s_HX8347_WriteReg(0x03, (uchar)(scol & 0x00FF));

	s_HX8347_WriteReg(0x04, (uchar)(ecol >> 8));
	s_HX8347_WriteReg(0x05, (uchar)(ecol & 0x00FF));

	giLcdMinX = scol;
	giLcdIndexX = giLcdMinX;
	giLcdMaxX = ecol;//include this pixel

	s_HX8347_WriteReg(0x06, (uchar)(sline>> 8));
	s_HX8347_WriteReg(0x07, (uchar)(sline & 0x00FF));

	s_HX8347_WriteReg(0x08, (uchar)(eline >> 8));
	s_HX8347_WriteReg(0x09, (uchar)(eline & 0x00FF));

	giLcdMinY = sline;
	giLcdIndexY = giLcdMinY;
	giLcdMaxY = eline;//include this pixel

	s_TFT_WrCmd(0x22);//write memory start
}

void s_ILIXXXX_SetGram(int scol, int sline, int ecol, int eline)
{
	s_TFT_WrCmd(TFT_SET_COLUMN_ADDRESS);
	s_TFT_WrData((uchar)(scol >> 8));
	s_TFT_WrData((uchar)(scol & 0x00FF));
	s_TFT_WrData((uchar)(ecol >> 8));
	s_TFT_WrData((uchar)(ecol & 0x00FF));
	giLcdMinX = scol;
	giLcdIndexX = giLcdMinX;
	giLcdMaxX = ecol;//include this pixel
	s_TFT_WrCmd(TFT_SET_PAGE_ADDRESS);
	s_TFT_WrData((uchar)(sline>> 8));
	s_TFT_WrData((uchar)(sline & 0x00FF));
	s_TFT_WrData((uchar)(eline >> 8));
	s_TFT_WrData((uchar)(eline & 0x00FF));
	giLcdMinY = sline;
	giLcdIndexY = giLcdMinY;
	giLcdMaxY = eline;//include this pixel
	
	s_TFT_WrCmd(TFT_WRITE_MEMORY_CONTINUE);
	s_TFT_WrCmd(TFT_WRITE_MEMORY_START);
}
void s_ST7789V_SetGram(int scol, int sline, int ecol, int eline)
{
	// HKF屏驱动
	s_TFT_WrCmd(TFT_SET_COLUMN_ADDRESS);
	s_TFT_WrData((uchar)(scol >> 8));
	s_TFT_WrData((uchar)(scol & 0xff));
	s_TFT_WrData((uchar)(ecol >> 8));
	s_TFT_WrData((uchar)(ecol & 0xff));

	giLcdMinX = scol;
	giLcdIndexX = giLcdMinX;
	giLcdMaxX = ecol;//include this pixel

	s_TFT_WrCmd(TFT_SET_PAGE_ADDRESS);
	s_TFT_WrData((uchar)(sline >> 8));
	s_TFT_WrData((uchar)(sline & 0xff));
	s_TFT_WrData((uchar)(eline >> 8));
	s_TFT_WrData((uchar)(eline & 0xff));

	giLcdMinY = sline;
	giLcdIndexY = giLcdMinY;
	giLcdMaxY = eline;//include this pixel

	s_TFT_WrCmd(TFT_WRITE_MEMORY_START);
}
static void SMC_TFT_config()
{
	uint cycles,opmode,direct_cmd;
	uint temp;
	gpio_set_pin_type(GPIOC,0,GPIO_FUNC1);
	gpio_set_pin_type(GPIOC,1,GPIO_FUNC1);
	gpio_set_pin_type(GPIOC,2,GPIO_FUNC1);
	gpio_set_pin_type(GPIOC,3,GPIO_FUNC1);
	gpio_set_pin_type(GPIOC,4,GPIO_FUNC1);
	gpio_set_pin_type(GPIOC,5,GPIO_FUNC1);
	gpio_set_pin_type(GPIOC,10,GPIO_FUNC1);
	gpio_set_pin_type(GPIOC,11,GPIO_FUNC1);  //DATA[0-7]

	gpio_set_pin_type(GPIOC,9,GPIO_FUNC1);    //ADDR[7]
	gpio_set_pin_type(GPIOC,12,GPIO_FUNC1);   //WE_L 
	gpio_set_pin_type(GPIOC,13,GPIO_FUNC1);   //OE_L
	gpio_set_pin_type(GPIOC,23,GPIO_FUNC1);   //CS_L[1]
	
	/*-----------------------------------------------------------------------------
	Write to smc_set_cycles register with the following parameters: 
	Write to smc_set_opmode register with the following parameters: 
	set_burst_align = 0,set_bls = 1,set_adv = 0,set_baa = 0,set_wr_bl = 0,set_wr_sync = 0,set_rd_bl = 0,set_rd_sync = 0,set_mw = 0.

	Write to smc_direct_cmd register with the following parameters: chip_select = 0 if the device is hooked up with 
	SMC_CS_L[0]. This value has to be 1 if the device is connected to SMC_CS_L[1], or 2 if the device is connected 
	to SMC_CS_L[2].cmd_type = 2 [UpdateRegs],set_cre = 0,addr = 0.
	b00 = UpdateRegs and AXI
	b01 = ModeReg
	b10 = UpdateRegs
	b11 = ModeReg and UpdateRegs.	
	When cmd_type = UpdateRegs and AXI then:
		bits [15:0] are used to match wdata[15:0]
		bits [19:16] are reserved. Write as zero.
	When cmd_type = ModeReg or ModeReg and UpdateRegs, these bits map to the external 
	memory address bits [19:0].
	When cmd_type = UpdateRegs, these bits are reserved. Write as zero.
	-------------------------------------------------------------------------------*/	

	//cycles =(t6<<20)|(t5<<17) |(t4<<14)|(t3<<11)|(t2<<8) |(t1<<4)|(t0);
	cycles =(7<<20)|(7<<17) |(7<<14)|(7<<11)|(7<<8) |(15<<4)|(15);
	//cycles =(3<<20)|(1<<17) |(1<<14)|(6<<11)|(1<<8) |(10<<4)|(7);
	//opmode =(set_burst_align <<13) | (set_byte_lane_strobe <<12);
	opmode =(0 <<13) | (1 <<12);
	//direct_cmd =(mem_if <<25) |(chip_sel <<23)|(cmd_type<<21)|(set_cre<<20) | (address);	
	direct_cmd =(1 <<23)|(2<<21)|(0<<20) | (0);	

	writel(cycles,SMC_R_smc_set_cycles_MEMADDR);
	writel(opmode,SMC_R_smc_set_opmode_MEMADDR);
	writel(direct_cmd,SMC_R_smc_direct_cmd_MEMADDR);
}

static void s_HX8347_Init(void)
{
	uchar tmp;
	int i,j;
	s_HX8347_WriteReg(0x18,0x88);   //UADJ 70Hz 
	s_HX8347_WriteReg(0x19,0x01);   //OSC_EN='1', start Osc 
	s_HX8347_WriteReg(0x1A,0x04); //BT (VGH~15V,VGL~-10V,DDVDH~5V) 

    s_HX8347_WriteReg(0x1D,0x44);//66 
    s_HX8347_WriteReg(0x1E,0x44);//66 
    s_HX8347_WriteReg(0x2E,0x80); 
	//Power Voltage Setting 
	s_HX8347_WriteReg(0x1B,0x1C); //VRH=4.60V 
	s_HX8347_WriteReg(0x1C,0x04); //AP Crosstalk    04
	//s_HX8347_WriteReg(0x1A,0x01); //BT (VGH~15V,VGL~-10V,DDVDH~5V) 
	s_HX8347_WriteReg(0x24,0x83); //VMH 	  27
	s_HX8347_WriteReg(0x25,0x63); //VML 
	//VCOM offset 
	s_HX8347_WriteReg(0x23,0x7E); //aa for Flicker adjust

	s_HX8347_WriteReg(0xe8,0x56); 
	s_HX8347_WriteReg(0xe9,0x38); 
	s_HX8347_WriteReg(0xe2,0x0D); 
	s_HX8347_WriteReg(0xe3,0x03); 
	s_HX8347_WriteReg(0xe4,0x15); // 15 EQVCI_M0 
	s_HX8347_WriteReg(0xe5,0x01); //    EQVSSD_M0 
	s_HX8347_WriteReg(0xe6,0x15); // 15 EQVCI_M1 
	s_HX8347_WriteReg(0xe7,0x01);//    EQVSSD_M1 
	s_HX8347_WriteReg(0xE8,0x86);//add 
    s_HX8347_WriteReg(0xEB,0x26);//add 
    s_HX8347_WriteReg(0xec,0x28); // 28 STBA[15:8] 
	s_HX8347_WriteReg(0xed,0x0C); // 0C STBA[7;0] 

	s_HX8347_WriteReg(0x1F,0x88);// GAS=1, VOMG=00, PON=0, DK=1, XDK=0, DVDH_TRI=0, STB=0 
	DelayMs(5); 
	s_HX8347_WriteReg(0x1F,0x80);// GAS=1, VOMG=00, PON=0, DK=0, XDK=0, DVDH_TRI=0, STB=0 
	DelayMs(5); 
	s_HX8347_WriteReg(0x1F,0x90);// GAS=1, VOMG=00, PON=1, DK=0, XDK=0, DVDH_TRI=0, STB=0 
	DelayMs(5); 
	s_HX8347_WriteReg(0x1F,0xD0);// GAS=1, VOMG=10, PON=1, DK=0, XDK=0, DDVDH_TRI=0, STB=0 
	DelayMs(5);      

	s_HX8347_WriteReg(0x36,0x09);   //REV, BGR 
	//Gamma 2.2 Setting 
	s_HX8347_WriteReg(0x40,0x0A); // 
	s_HX8347_WriteReg(0x41,0x09); // 
	s_HX8347_WriteReg(0x42,0x01); // 
	s_HX8347_WriteReg(0x43,0x21); // 	  11
	s_HX8347_WriteReg(0x44,0x1E); // 	  0f
	s_HX8347_WriteReg(0x45,0x29); // 	  28
	s_HX8347_WriteReg(0x46,0x19); // 
	s_HX8347_WriteReg(0x47,0x72); // 	  53
	s_HX8347_WriteReg(0x48,0x03); // 
	s_HX8347_WriteReg(0x49,0x1B); // 
	s_HX8347_WriteReg(0x4A,0x1D); // 	  17
	s_HX8347_WriteReg(0x4B,0x1D); // 	  18
	s_HX8347_WriteReg(0x4C,0x1A); // 	  1a
	s_HX8347_WriteReg(0x50,0x16); // 	  17
	s_HX8347_WriteReg(0x51,0x21); // 	  30
	s_HX8347_WriteReg(0x52,0x1E); // 	  2e
	s_HX8347_WriteReg(0x53,0x3E); // 
	s_HX8347_WriteReg(0x54,0x36); // 
	s_HX8347_WriteReg(0x55,0x35); // 
	s_HX8347_WriteReg(0x56,0x0D); // 	  2c
	s_HX8347_WriteReg(0x57,0x66); // 
	s_HX8347_WriteReg(0x58,0x05); // 	  05
	s_HX8347_WriteReg(0x59,0x02); // 	  07
	s_HX8347_WriteReg(0x5A,0x02); // 	  08
	s_HX8347_WriteReg(0x5B,0x04); // 
	s_HX8347_WriteReg(0x5C,0x1C); // 
	s_HX8347_WriteReg(0x5D,0x88); // 

	// S200为横屏，该LCD模块默认为竖屏，需要设置x, y坐标互换
	tmp = s_HX8347_ReadReg(0x16);
	tmp &= 0x1f;
	tmp |= 0xa0;
	s_HX8347_WriteReg(0x16, tmp);
	tmp = s_HX8347_ReadReg(0x16);

	// 设置行、列的起始、结束值。
	s_HX8347_WriteReg(0x02, 0);
	s_HX8347_WriteReg(0x03, 0);
	s_HX8347_WriteReg(0x04, 1);
	s_HX8347_WriteReg(0x05, 0x3f);
	s_HX8347_WriteReg(0x06, 0);
	s_HX8347_WriteReg(0x07, 0);
	s_HX8347_WriteReg(0x08, 0);
	s_HX8347_WriteReg(0x09, 0xef);
	// 设置为16位色
	s_HX8347_WriteReg(0x17, 0x05);//06 for 262K
	/* clear screen */
	s_HX8347_SetGram(0, 0, TFT_MAX_X-1, TFT_MAX_Y-1);
	for (i=0;i<TFT_MAX_Y;i++)
	{
		for (j=0;j<TFT_MAX_X;j++)
		{
			s_Tft_WrLData(0xffff);
		}
	}
	//Display ON Setting 
	s_HX8347_WriteReg(0x28,0x38);   //GON=1, DTE=1, D=1000 
	DelayMs(40); 
	s_HX8347_WriteReg(0x28,0x3C);   //GON=1, DTE=1, D=1100    
	s_HX8347_WriteReg(0x1A,0x00); //BT (VGH~15V,VGL~-10V,DDVDH~5V) 
	
}
static void s_ST7789V_Init()
{
	int i,j;
	s_TFT_WrCmd(0x11);
	DelayMs(120);               //Delay 120ms
	//------------------------------display and color format setting-------------------------------
	s_TFT_WrCmd(0x36);
	s_TFT_WrData(0xa0);
	s_TFT_WrCmd(0x3a);
	s_TFT_WrData(0x55);
	//--------------------------------ST7789V Frame rate setting---------------------------------
	s_TFT_WrCmd(0xb2);
	s_TFT_WrData(0x0c);
	s_TFT_WrData(0x0c);
	s_TFT_WrData(0x00);
	s_TFT_WrData(0x33);
	s_TFT_WrData(0x33);
	s_TFT_WrCmd(0xb7);
	s_TFT_WrData(0x35);
	//---------------------------------ST7789V Power setting--------------------------------------
	s_TFT_WrCmd(0xbb);
	s_TFT_WrData(0x28);
	s_TFT_WrCmd(0xc0);
	s_TFT_WrData(0x2c);
	s_TFT_WrCmd(0xc2);
	s_TFT_WrData(0x01);
	s_TFT_WrCmd(0xc3);
	s_TFT_WrData(0x0b);
	s_TFT_WrCmd(0xc4);

	s_TFT_WrData(0x20);
	s_TFT_WrCmd(0xc6);
	s_TFT_WrData(0x0f);
	s_TFT_WrCmd(0xd0);
	s_TFT_WrData(0xa4);
	s_TFT_WrData(0xa1);
	//--------------------------------ST7789V gamma setting---------------------------------------//
	s_TFT_WrCmd(0xe0);
	s_TFT_WrData(0xd0);
	s_TFT_WrData(0x01);
	s_TFT_WrData(0x08);
	s_TFT_WrData(0x0f);
	s_TFT_WrData(0x11);
	s_TFT_WrData(0x2a);
	s_TFT_WrData(0x36);
	s_TFT_WrData(0x55);
	s_TFT_WrData(0x44);
	s_TFT_WrData(0x3a);
	s_TFT_WrData(0x0b);
	s_TFT_WrData(0x06);
	s_TFT_WrData(0x11);
	s_TFT_WrData(0x20);
	s_TFT_WrCmd(0xe1);
	s_TFT_WrData(0xd0);
	s_TFT_WrData(0x02);
	s_TFT_WrData(0x07);
	s_TFT_WrData(0x0a);
	s_TFT_WrData(0x0b);
	s_TFT_WrData(0x18);
	s_TFT_WrData(0x34);
	s_TFT_WrData(0x43);
	s_TFT_WrData(0x4a);
	s_TFT_WrData(0x2b);
	s_TFT_WrData(0x1b);
	s_TFT_WrData(0x1c);
	s_TFT_WrData(0x22);
	s_TFT_WrData(0x1f);

	/* clear screen */
	s_ST7789V_SetGram(0, 0, TFT_MAX_X-1, TFT_MAX_Y-1);
	for (i=0;i<TFT_MAX_Y;i++)
	{
		for (j=0;j<TFT_MAX_X;j++)
		{
			s_Tft_WrLData(0xffff);
		}
	}
	s_TFT_WrCmd(0x29);
}

static void s_ILI9341_Init()
{
	int i,j;
    //************* Start Initial Sequence **********// 
    s_TFT_WrCmd(0xCF);  //Power control B
    s_TFT_WrData(0x00); 
    s_TFT_WrData(0xC1); //(0xD9); 展讯会竖线
    s_TFT_WrData(0X30); 
     
    s_TFT_WrCmd(0xED);  //Power on sequence control
    s_TFT_WrData(0x64); 
    s_TFT_WrData(0x03); 
    s_TFT_WrData(0X12); 
    s_TFT_WrData(0X81); 
     
    s_TFT_WrCmd(0xE8);  //Driver timing control B
    s_TFT_WrData(0x85); 
    s_TFT_WrData(0x00); 
    s_TFT_WrData(0x78); 
     
    s_TFT_WrCmd(0xCB);  //Power control A
    s_TFT_WrData(0x39); 
    s_TFT_WrData(0x2C); 
    s_TFT_WrData(0x00); 
    s_TFT_WrData(0x34); 
    s_TFT_WrData(0x02); 
     
    s_TFT_WrCmd(0xF7);  //Pumb ratio control
    s_TFT_WrData(0x20); 
     
    s_TFT_WrCmd(0xEA);  //Driver timing control A
    s_TFT_WrData(0x00); 
    s_TFT_WrData(0x00); 
     
    s_TFT_WrCmd(0xC0);    //Power control 1
    s_TFT_WrData(0x1B);   //VRH[5:0] 
     
    s_TFT_WrCmd(0xC1);    //Power control 2
    s_TFT_WrData(0x12);   //SAP[2:0];BT[3:0] 
     
    s_TFT_WrCmd(0xC5);    //VCM control 1
    s_TFT_WrData(0x30); //0x32
    s_TFT_WrData(0x3C); 
     
    s_TFT_WrCmd(0xC7);    //VCM control 2 
    s_TFT_WrData(0X95); //(0X9D); 
     
    s_TFT_WrCmd(0x36);    // Memory Access Control 
    s_TFT_WrData(0xa8); //0x08
     
    s_TFT_WrCmd(0x3A);   //Pixel format set
    s_TFT_WrData(0x55); 

    s_TFT_WrCmd(0xB1);   //Frame rate control
    s_TFT_WrData(0x00);   
    s_TFT_WrData(0x16); //   (0x1B); 
     
    s_TFT_WrCmd(0xB6);    // Display Function Control 
    s_TFT_WrData(0x0A); 
    s_TFT_WrData(0xA2); 

    s_TFT_WrCmd(0xF6);   //Interface control 
    s_TFT_WrData(0x01); 
    s_TFT_WrData(0x30); 
     
    s_TFT_WrCmd(0xF2);    // 3Gamma Function Disable 
    s_TFT_WrData(0x00); 
     
    s_TFT_WrCmd(0x26);    //Gamma curve selected 
    s_TFT_WrData(0x01); 
     
    s_TFT_WrCmd(0xE0);    //Set Gamma 
    s_TFT_WrData(0x0F); 
    s_TFT_WrData(0x24); 
    s_TFT_WrData(0x1F); 
    s_TFT_WrData(0x0B); 
    s_TFT_WrData(0x0F); 
    s_TFT_WrData(0x05); 
    s_TFT_WrData(0x4A); 
    s_TFT_WrData(0X96); 
    s_TFT_WrData(0x39); 
    s_TFT_WrData(0x07); 
    s_TFT_WrData(0x11); 
    s_TFT_WrData(0x03); 
    s_TFT_WrData(0x11); 
    s_TFT_WrData(0x0D); 
    s_TFT_WrData(0x04); 
     
    s_TFT_WrCmd(0XE1);    //Set Gamma 
    s_TFT_WrData(0x00); 
    s_TFT_WrData(0x1B); 
    s_TFT_WrData(0x20); 
    s_TFT_WrData(0x04); 
    s_TFT_WrData(0x10); 
    s_TFT_WrData(0x02); 
    s_TFT_WrData(0x35); 
    s_TFT_WrData(0x23); 
    s_TFT_WrData(0x46); 
    s_TFT_WrData(0x04); 
    s_TFT_WrData(0x0E); 
    s_TFT_WrData(0x0C); 
    s_TFT_WrData(0x2E); 
    s_TFT_WrData(0x32); 
    s_TFT_WrData(0x05); 
     
    s_TFT_WrCmd(0x11);    //Exit Sleep 
    DelayMs(120); 
	/* clear screen */
	s_ILIXXXX_SetGram(0, 0, TFT_MAX_X-1, TFT_MAX_Y-1);
	for (i=0;i<TFT_MAX_Y;i++)
	{
		for (j=0;j<TFT_MAX_X;j++)
		{
			s_Tft_WrLData(0xffff);
		}
	}
    s_TFT_WrCmd(0x29);    //Display on 
}
void s_ILIXXXX_SleepIn(void)
{
	s_TFT_WrCmd(TFT_ENTER_SLEEP_MODE);
}
void s_ILIXXXX_SleepOut(void)
{
	s_TFT_WrCmd(TFT_EXIT_SLEEP_MODE);
	DelayMs(200);
}
static void s_ILI9342_Init(void)
{
	int i,j;
	//以下初始化命令为天马提供的ILI9342初始化命令
	s_TFT_WrCmd(0xB9);//Set EXTCommand
	s_TFT_WrData(0xFF);
	s_TFT_WrData(0x93);
	s_TFT_WrData(0x42);
	s_TFT_WrCmd(0x21);//Display Inversion ON
	s_TFT_WrCmd(0x36);//MemoryAccess Control
	s_TFT_WrData(0xC8);
	s_TFT_WrCmd(0x3A); // set CSEL,PIX Format Set
	s_TFT_WrData(0x05); // CSEL/IFPF_DAI=0x05, 16bit-color
	s_TFT_WrCmd(0x2D); //Look up table//ColorSet
	for(i=0;i<32;i++)
		s_TFT_WrData(2*i);//RED
	for(i=0;i<64;i++)
		s_TFT_WrData(1*i);//Green
	for(i=0;i<32;i++)
		s_TFT_WrData(2*i);//Blue
	s_TFT_WrCmd(0xC0);//Power Control 1
	s_TFT_WrData(0x25);//VRH=100101B,4.70V,Set the VREG1OUT level, 
								//which is a reference level for the VCOM level and the grayscale voltage level
	s_TFT_WrData(0x0A);//VC=1010B,2.80V,Sets VCI1 internal reference regulator voltage.
	s_TFT_WrCmd(0xC1);//Power Control 2
	s_TFT_WrData(0x01);//SAP=0,BT=1
	s_TFT_WrCmd(0xC5);//VCOM Control 1
	s_TFT_WrData(0x2F);//VMH=0x2F,VCOMH=3.875V
	s_TFT_WrData(0x27);//VML=0x27,VCOML=-1.525V
	s_TFT_WrCmd(0xC7);//VCOM Control 2
	s_TFT_WrData(0xD3);//VMF=0xD3,Set the VCOM offset voltage.VCOMH=VMH+19,VCOML=VML+19
	s_TFT_WrCmd(0xB8);//Oscillator Control
	s_TFT_WrData(0x0B);//FOSC=1011B,Frame Rate=93HZ
	s_TFT_WrCmd(0xE0);//Positive Gamma Correction
	//Set the gray scale voltage to adjust the gamma characteristics of the TFT panel.
	s_TFT_WrData(0x0F);
	s_TFT_WrData(0x22);
	s_TFT_WrData(0x1D);
	s_TFT_WrData(0x0B);
	s_TFT_WrData(0x0F);
	s_TFT_WrData(0x07);
	s_TFT_WrData(0x4C);
	s_TFT_WrData(0x76);
	s_TFT_WrData(0x3C);
	s_TFT_WrData(0x09);
	s_TFT_WrData(0x16);
	s_TFT_WrData(0x07);
	s_TFT_WrData(0x12);
	s_TFT_WrData(0x0B);
	s_TFT_WrData(0x08);
	s_TFT_WrCmd(0xE1);//Negative Gamma Correction
	//Set the gray scale voltage to adjust the gamma characteristics of the TFT panel.
	s_TFT_WrData(0x08);
	s_TFT_WrData(0x1F);
	s_TFT_WrData(0x24);
	s_TFT_WrData(0x03);
	s_TFT_WrData(0x0E);
	s_TFT_WrData(0x03);
	s_TFT_WrData(0x35);
	s_TFT_WrData(0x23);
	s_TFT_WrData(0x45);
	s_TFT_WrData(0x01);
	s_TFT_WrData(0x0B);
	s_TFT_WrData(0x07);
	s_TFT_WrData(0x2F);
	s_TFT_WrData(0x36);
	s_TFT_WrData(0x0F);
	s_TFT_WrCmd(0xF2);//??
	s_TFT_WrData(0x00);
	s_TFT_WrCmd(0x11); //Exit Sleep
	DelayMs(5);
	/* clear screen */
	s_ILIXXXX_SetGram(0, 0, TFT_MAX_X-1, TFT_MAX_Y-1);
	for (i=0;i<TFT_MAX_Y;i++)
	{
		for (j=0;j<TFT_MAX_X;j++)
		{
			s_Tft_WrLData(0xffff);
		}
	}
	s_TFT_WrCmd(0x29); //Display On
}

static void s_ILI9342C_Init(void)
{
	int i,j;
	//以下初始化命令为天马提供的ILI9342C初始化命令
	s_TFT_WrCmd(0xC8);//Set EXTCommand
	s_TFT_WrData(0xFF);
	s_TFT_WrData(0x93);
	s_TFT_WrData(0x42);
	//s_TFT_WrCmd(0x21);//Display Inversion ON
	s_TFT_WrCmd(0x36);//MemoryAccess Control
	s_TFT_WrData(0xC8);
	s_TFT_WrCmd(0x3A); // set CSEL,PIX Format Set
	s_TFT_WrData(0x55); // CSEL/IFPF_DAI=0x05, 16bit-color

	s_TFT_WrCmd(0xC0);//Power Control 1
	s_TFT_WrData(0x09);//VRH1=1001B,4.171V,Set the VREG1OUT level, 
								//which is a reference level for the VCOM level and the grayscale voltage level
	s_TFT_WrData(0x09);//VRH2=1001B,-4.17V,Sets VREG2OUT level.
	
	s_TFT_WrCmd(0xC1);//Power Control 2
	s_TFT_WrData(0x01);//SAP=0,BT=1
	s_TFT_WrCmd(0xC5);//VCOM Control 1
	s_TFT_WrData(0xF7);//VCOM=VREG2OUT*0.129
	
	s_TFT_WrCmd(0xE0);//Positive Gamma Correction
	//Set the gray scale voltage to adjust the gamma characteristics of the TFT panel.
	s_TFT_WrData(0x00);
	s_TFT_WrData(0x07);
	s_TFT_WrData(0x0C);
	s_TFT_WrData(0x05);
	s_TFT_WrData(0x12);
	s_TFT_WrData(0x07);
	s_TFT_WrData(0x33);
	s_TFT_WrData(0x76);
	s_TFT_WrData(0x43);
	s_TFT_WrData(0x07);
	s_TFT_WrData(0x0D);
	s_TFT_WrData(0x0B);
	s_TFT_WrData(0x17);
	s_TFT_WrData(0x18);
	s_TFT_WrData(0x0F);
	
	s_TFT_WrCmd(0xE1);//Negative Gamma Correction
	//Set the gray scale voltage to adjust the gamma characteristics of the TFT panel.
	s_TFT_WrData(0x00);
	s_TFT_WrData(0x25);
	s_TFT_WrData(0x27);
	s_TFT_WrData(0x03);
	s_TFT_WrData(0x0F);
	s_TFT_WrData(0x06);
	s_TFT_WrData(0x3C);
	s_TFT_WrData(0x37);
	s_TFT_WrData(0x4D);
	s_TFT_WrData(0x05);
	s_TFT_WrData(0x0E);
	s_TFT_WrData(0x0B);
	s_TFT_WrData(0x31);
	s_TFT_WrData(0x34);
	s_TFT_WrData(0x0F);
	
	s_TFT_WrCmd(0xB4);//inversion //2-dots
	s_TFT_WrData(0x02);

	s_TFT_WrCmd(0xB1);//frame rate 63
	s_TFT_WrData(0x00);
	s_TFT_WrData(0x1C);
	
	s_TFT_WrCmd(0x11); //Exit Sleep
	DelayMs(120);
	/* clear screen */
	s_ILIXXXX_SetGram(0, 0, TFT_MAX_X-1, TFT_MAX_Y-1);
	for (i=0;i<TFT_MAX_Y;i++)
	{
		for (j=0;j<TFT_MAX_X;j++)
		{
			s_Tft_WrLData(0xffff);
		}
	}
	s_TFT_WrCmd(0x29); //Display On
	s_TFT_WrCmd(0x2C);
}

static void s_ILI9342C_HSD2_3_Init(void)
{
	int i,j;
	// VCI=2.8V
	//************* Start Initial Sequence **********//
	s_TFT_WrCmd(0xC8);       //Set EXTC
	s_TFT_WrData(0xFF);
	s_TFT_WrData(0x93);
	s_TFT_WrData(0x42);
	  
	s_TFT_WrCmd(0x36);       //Memory Access Control
	s_TFT_WrData(0xC8); //MY,MX,MV,ML,BGR,MH
	s_TFT_WrCmd(0x3A);       //Pixel Format Set
	s_TFT_WrData(0x55); //DPI [2:0],DBI [2:0]
	
	s_TFT_WrCmd(0xC0);       //Power Control 1
	s_TFT_WrData(0x1f); //VRH[5:0] 0x10
	s_TFT_WrData(0x1f); //VC[3:0] 0x10
	
	s_TFT_WrCmd(0xC1);       //Power Control 2
	s_TFT_WrData(0x11); //SAP[2:0],BT[3:0]
	  
	s_TFT_WrCmd(0xC5);       //VCOM
	s_TFT_WrData(0xCD); // 0xCD 0xC2
	
	s_TFT_WrCmd(0xB1);      
	s_TFT_WrData(0x00);     
	s_TFT_WrData(0x1B);
	s_TFT_WrCmd(0xB4);      
	s_TFT_WrData(0x02);

	s_TFT_WrCmd(0xE0);
	s_TFT_WrData(0x0F);//P01-VP63   
	s_TFT_WrData(0x14);//P02-VP62   
	s_TFT_WrData(0x17);//P03-VP61   
	s_TFT_WrData(0x07);//P04-VP59   
	s_TFT_WrData(0x16);//P05-VP57   
	s_TFT_WrData(0x0A);//P06-VP50   
	s_TFT_WrData(0x3F);//P07-VP43   
	s_TFT_WrData(0x68);//P08-VP27,36
	s_TFT_WrData(0x4C);//P09-VP20   
	s_TFT_WrData(0x06);//P10-VP13   
	s_TFT_WrData(0x0F);//P11-VP6    
	s_TFT_WrData(0x0D);//P12-VP4    
	s_TFT_WrData(0x18);//P13-VP2    
	s_TFT_WrData(0x1A);//P14-VP1    
	s_TFT_WrData(0x00);//P15-VP0    
	
	s_TFT_WrCmd(0xE1);
	s_TFT_WrData(0x00);//P01
	s_TFT_WrData(0x29);//P02
	s_TFT_WrData(0x29);//P03
	s_TFT_WrData(0x04);//P04
	s_TFT_WrData(0x0F);//P05
	s_TFT_WrData(0x04);//P06
	s_TFT_WrData(0x3C);//P07
	s_TFT_WrData(0x24);//P08
	s_TFT_WrData(0x4B);//P09
	s_TFT_WrData(0x02);//P10
	s_TFT_WrData(0x0B);//P11
	s_TFT_WrData(0x09);//P12
	s_TFT_WrData(0x32);//P13
	s_TFT_WrData(0x37);//P14
	s_TFT_WrData(0x0F);//P15
	
	s_TFT_WrCmd(0x11);//Exit Sleep
	DelayMs(120);
	/* clear screen */
	s_ILIXXXX_SetGram(0, 0, TFT_MAX_X-1, TFT_MAX_Y-1);
	for (i=0;i<TFT_MAX_Y;i++)
	{
		for (j=0;j<TFT_MAX_X;j++)
		{
			s_Tft_WrLData(0xffff);
		}
	}
	s_TFT_WrCmd(0x29);//Display On
}

void s_TftReadPixel(int x, int y, int *Color) 
{
     *((COLORREF *)(Color)) = gucLcdMem[y][(x)*2]*256 + gucLcdMem[y][(x)*2+1];
}
void s_ST7789V_PhyInit(void)
{
	SMC_TFT_config();
	s_ST7789V_Init();
}
void s_ILI9341_PhyInit(void)
{
	SMC_TFT_config();
	s_TFT_WrCmd(0x01);
	DelayMs(10);
	s_ILI9341_Init(); 
}
void s_HX8347_PhyInit(void)
{
	SMC_TFT_config();
	s_HX8347_Init();
}
void s_TM023KDZ06_PhyInit(void)
{
	uchar ucReadData[4];

	memset(ucReadData, 0, sizeof(ucReadData));
	SMC_TFT_config();
	
	s_TFT_WrCmd(0x01);
	DelayMs(10);
		
	s_TFT_WrCmd(0xD3);//Get ID4 for ILI9341
	s_TFT_ReadData(ucReadData);//dummy data
	s_TFT_ReadData(ucReadData+1);//0x00
	s_TFT_ReadData(ucReadData+2);//0x93
	s_TFT_ReadData(ucReadData+3);//0x41
		
	s_TFT_WrCmd(0xDA);
	s_TFT_ReadData(ucReadData);
	s_TFT_ReadData(ucReadData+1);
	if(ucReadData[1] == 0x00)
	{
		s_ILI9342_Init();
	}
	else
	{
		s_ILI9342C_Init();
	}
}
void s_H23B13_PhyInit(void)
{
	SMC_TFT_config();
	s_ILI9342C_HSD2_3_Init();
}

/****************************************************************/
/****************************For DMA LCD*************************/
/****************************************************************/
#define LCD_SPI_ENABLE()    gpio_set_pin_val(GPIOC, 29, 0)
#define LCD_SPI_DISABLE()   gpio_set_pin_val(GPIOC, 29, 1)

#define SPI_WR_COMMAND_ID0(cmd) \
    LCD_SPI_DISABLE(); \
    DelayMs(1); \
    LCD_SPI_ENABLE(); \
    spi_txrx(3, (ushort)cmd, 16)

#define SPI_COMMAND_END() \
    LCD_SPI_DISABLE(); \
    DelayMs(1)

#define SPI_WR_DATA_ID0(data) \
    (spi_txrx(3, (ushort)(data | 0x0100), 16)&0xff)

void ili9341_reset(void)
{
	spi_config(3, 1000000, 9, 0);
	DelayMs(20);
		
	SPI_WR_COMMAND_ID0(0x028); //Display off
	SPI_COMMAND_END();
	DelayMs(1);
	SPI_WR_COMMAND_ID0(0x01); // Soft reset
	SPI_COMMAND_END();
	DelayMs(120);
}

void ili9341_init(void)
{
	SPI_WR_COMMAND_ID0(0x028); //Display off
	SPI_COMMAND_END();
	DelayMs(1);
	SPI_WR_COMMAND_ID0(0x01); // Soft reset
	SPI_COMMAND_END();
	DelayMs(120);

	/* Start Initial Sequence */
	SPI_WR_COMMAND_ID0(0xCB);
	SPI_WR_DATA_ID0(0x39);
	SPI_WR_DATA_ID0(0x2C);
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0x34);
	SPI_WR_DATA_ID0(0x02);
	SPI_COMMAND_END();

	SPI_WR_COMMAND_ID0(0xCF); // power control B
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0xC1);
	SPI_WR_DATA_ID0(0x30);		/* VGH*7 VGL*4 */
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xE8); // Driver timing control A
	SPI_WR_DATA_ID0(0x85);
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0x78);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xEA); // Driver timing control B
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0x00);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xED); // Power on sequence control
	SPI_WR_DATA_ID0(0x64);
	SPI_WR_DATA_ID0(0x03);
	SPI_WR_DATA_ID0(0x12);
	SPI_WR_DATA_ID0(0x81);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xF7); // Pump ratio control
	SPI_WR_DATA_ID0(0x20);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xC0);	/* Power control 1 */
	SPI_WR_DATA_ID0(0x1b);		/* VRH[5:0] */
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xC1);	/* Power control 2 */
	SPI_WR_DATA_ID0(0x10);		/* SAP[2:0];BT[3:0] */
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xC5);	/* VCOM control 1 */
	SPI_WR_DATA_ID0(0x27);
	SPI_WR_DATA_ID0(0x3a);    // old(33)
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0x36);	/* Memory Access Control */
	SPI_WR_DATA_ID0(0x48);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xB1);   // Frame rate control
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0x1d);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xB6);	/* Display Function Control */
	SPI_WR_DATA_ID0(0x0A);
	SPI_WR_DATA_ID0(0x82);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xF2);	/* 3Gamma Function Disable */
	SPI_WR_DATA_ID0(0x00);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0x26);	/* Gamma curve selected */
	SPI_WR_DATA_ID0(0x01);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xE0);	/* Positive Gamma Correction */
	SPI_WR_DATA_ID0(0x0F);
	SPI_WR_DATA_ID0(0x3a);
	SPI_WR_DATA_ID0(0x36);
	SPI_WR_DATA_ID0(0x0b);
	SPI_WR_DATA_ID0(0x0d);
	SPI_WR_DATA_ID0(0x06);
	SPI_WR_DATA_ID0(0x4c);
	SPI_WR_DATA_ID0(0x91);
	SPI_WR_DATA_ID0(0x31);
	SPI_WR_DATA_ID0(0x08);
	SPI_WR_DATA_ID0(0x10);
	SPI_WR_DATA_ID0(0x04);
	SPI_WR_DATA_ID0(0x11);
	SPI_WR_DATA_ID0(0x0c);
	SPI_WR_DATA_ID0(0x00);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xE1);	/* Negative Gamma Correction */
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0x06);
	SPI_WR_DATA_ID0(0x0a);
	SPI_WR_DATA_ID0(0x05);
	SPI_WR_DATA_ID0(0x12);
	SPI_WR_DATA_ID0(0x09);
	SPI_WR_DATA_ID0(0x2c);
	SPI_WR_DATA_ID0(0x92);
	SPI_WR_DATA_ID0(0x3f);
	SPI_WR_DATA_ID0(0x08);
	SPI_WR_DATA_ID0(0x0e);
	SPI_WR_DATA_ID0(0x0b);
	SPI_WR_DATA_ID0(0x2e);
	SPI_WR_DATA_ID0(0x33);
	SPI_WR_DATA_ID0(0x0F);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0x3a);	/* Pixel format set: 16bit */
	SPI_WR_DATA_ID0(0x55);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xB0);	/* RGB interface signal control */
	SPI_WR_DATA_ID0(0x40);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xB1);	/* Frame Rate Control (In Normal Mode/Full Colors) */
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0x1b);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xF6);   // Interface control
	SPI_WR_DATA_ID0(0x01);
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0x06);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xB4);	/* Display Inversion Control */
	SPI_WR_DATA_ID0(0x00);		/* Line inversion */
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0x11);	/* Exit Sleep */
	SPI_COMMAND_END();
	DelayMs(120);
	SPI_WR_COMMAND_ID0(0x29);	/* Display on */
	SPI_COMMAND_END();
}

#define SPI_COM_ID0(idx, data) \
    LCD_SPI_DISABLE(); \
    DelayMs(1); \
    LCD_SPI_ENABLE(); \
    spi_txrx(3, (ushort)(((idx << 2 | 0x02) << 8) | data), 16); \
    LCD_SPI_DISABLE()

void nt39016d_init(void)
{
	SPI_COM_ID0(0x00, 0x0A);	 /* RESET CHIP */
	DelayMs(40);

	SPI_COM_ID0(0x00, 0x07);	  /* R00 */
	SPI_COM_ID0(0x01, 0x00);	  /* R01 */
	SPI_COM_ID0(0x02, 0x03);	  /* R02 */
	SPI_COM_ID0(0x03, 0xCC);	 /* R03 */
	SPI_COM_ID0(0x04, 0x46);	  /* R04 */
	SPI_COM_ID0(0x05, 0x0D);	 /* R05 */
	SPI_COM_ID0(0x06, 0x00);	  /* R06 */
	SPI_COM_ID0(0x07, 0x00);	  /* R07 */
	SPI_COM_ID0(0x08, 0x08);	  /* R08 */
	SPI_COM_ID0(0x09, 0x40);	  /* R09 */
	SPI_COM_ID0(0x0A, 0x88);	  /* R0A */
	SPI_COM_ID0(0x0B, 0x88);	  /* R0B */
	SPI_COM_ID0(0x0C, 0x20);	  /* R0C */
	SPI_COM_ID0(0x0D, 0x20);	 /* R0D */
	SPI_COM_ID0(0x0E, 0x6b);	  /* R0E old(68). */
	SPI_COM_ID0(0x0F, 0xA4);	  /* R0F */
	SPI_COM_ID0(0x10, 0x04);	  /* R10 */
	SPI_COM_ID0(0x11, 0x24);	  /* R11 */
	SPI_COM_ID0(0x12, 0x24);	  /* R12 */
	SPI_COM_ID0(0x1E, 0x00);	  /* R1E */
	SPI_COM_ID0(0x20, 0x00);	  /* R20 */
}



void s_S90357_LcdPhyInit(void)
{
	char    i;
	unsigned int tmp, clk_div ;

    //1.1 初始化变量
    memset(dma_lcd_buf, 0xff, DMA_LCD_LEN);
    
    clk_div = 12;//9.5MHz

	//1.2 bcm5892 internal LCD controller
	tmp = reg32(LCD_R_LCDControl_MEMADDR);
	tmp &= ~(BIT3 | BIT2 | BIT1);
	tmp |= BIT5 | BIT3 | BIT2;
	writel(tmp, LCD_R_LCDControl_MEMADDR); // TFT, 16 bits, 5:6:5 mode

	tmp = ((lcdpara->HBP - 1) << 24) | ((lcdpara->HFP - 1) << 16)
		| ((lcdpara->HSW - 1) << 8) | ((lcdpara->PPL / 16 - 1) << 2);
	writel(tmp, LCD_R_LCDTiming0_MEMADDR);

	tmp = ((lcdpara->VBP - 1) << 24) | ((lcdpara->VFP - 1) << 16)
		| ((lcdpara->VSW - 1) << 10) | (lcdpara->LPP -1);
	writel(tmp, LCD_R_LCDTiming1_MEMADDR);

	// 10-bit clk_div 被分为两部分写入寄存器, 各5 bits.
	tmp = ((clk_div >> 5) << 27) | (clk_div & 0x1f);
	tmp |= ((lcdpara->PPL - 1) << 16)
		| LCD_F_IHS_MASK | LCD_F_IVS_MASK | LCD_F_IPC_MASK;
	writel(tmp, LCD_R_LCDTiming2_MEMADDR);

    //1.3  初始化LCD DATA IO
    
	tmp = BIT2 | BIT3 | BIT4 | BIT5           // control signals
		| BIT9 | BIT10 | BIT11 | BIT12 | BIT13          // Red signals
		| BIT14 | BIT15 | BIT16 | BIT17 | BIT18 | BIT19 // Green signals
		| BIT21 | BIT22 | BIT23 | BIT24 | BIT25;        // Blue signals
	/* LCD RGB interface */
	gpio_set_mpin_type(GPIOC, tmp, GPIO_FUNC0);

    //1.4初始化LCD SPI和LCD 模块控制器
	/* LCD SPI interface */
	gpio_set_mpin_type(GPIOC, BIT26|BIT27|BIT28, GPIO_FUNC1);
    //LCD SPI 使能信号
	gpio_set_pin_type(GPIOC, 29, GPIO_OUTPUT);
    LCD_SPI_DISABLE();

	
	spi_config(3, 1000000, 9, 0);
	ili9341_init();
	

    //1.5使能BCM5892 LCD控制器
	writel(BIT16 | (lcdpara->LED - 1), LCD_R_LCDTiming3_MEMADDR); // Line end delay
	writel((uint)DMA_LCD_BASE, LCD_R_LCDUPBASE_MEMADDR);
	writel(0, LCD_R_LCDIMSC_MEMADDR); // No interrupt.

	tmp = reg32(LCD_R_LCDControl_MEMADDR);
	tmp |= BIT0;                           // LCD enable
	writel(tmp, LCD_R_LCDControl_MEMADDR);
	DelayMs(10);
	tmp |= BIT11;                          // LCD power on
	writel(tmp, LCD_R_LCDControl_MEMADDR);
	
}


void s_ST7789VRGB_Init_HKF(void)
{ 
	/*
	//-----------------------------------ST7789V reset sequence-----------------------------------
	LCD_RESET=1; 
	DelayMs(1); 				 //Delay 1ms 
	LCD_RESET=0; 
	DelayMs(10);				  //Delay 10ms 
	LCD_RESET=1; 
	DelayMs(120);				 //Delay 120ms 
	*/
	
	//---------------------------------------------------------------------------------------------------
	SPI_WR_COMMAND_ID0(0x11); 
	SPI_COMMAND_END();
	DelayMs(120);				 //Delay 120ms 
	//------------------------------display and color format setting-------------------------------
	SPI_WR_COMMAND_ID0(0x36); 
	SPI_WR_DATA_ID0(0x40); //0xa0
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0x3a); 
	SPI_WR_DATA_ID0(0x55); 
	SPI_COMMAND_END();

	//Joshua _a 
	SPI_WR_COMMAND_ID0(0xb0); 
	SPI_WR_DATA_ID0(0x11); 
	//SPI_WR_DATA_ID0(0x00); 
	SPI_COMMAND_END();
	
	SPI_WR_COMMAND_ID0(0xb1); 
	SPI_WR_DATA_ID0(0x40); 
	//SPI_WR_DATA_ID0(0x7f); 
	//SPI_WR_DATA_ID0(0x1f); 
	SPI_COMMAND_END();
	//--------------------------------ST7789V Frame rate setting---------------------------------
	SPI_WR_COMMAND_ID0(0xb2); 
	SPI_WR_DATA_ID0(0x0c); 
	SPI_WR_DATA_ID0(0x0c); 
	SPI_WR_DATA_ID0(0x00); 
	SPI_WR_DATA_ID0(0x33); 
	SPI_WR_DATA_ID0(0x33); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xb7); 
	SPI_WR_DATA_ID0(0x35); 
	SPI_COMMAND_END();
	//------------------)--------------ST7789V Power setting--------------------------------------
	SPI_WR_COMMAND_ID0(0xbb); 
	SPI_WR_DATA_ID0(0x28); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc0); 
	SPI_WR_DATA_ID0(0x2c); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc2); 
	SPI_WR_DATA_ID0(0x01); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc3); 
	SPI_WR_DATA_ID0(0x0b); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc4);
	SPI_WR_DATA_ID0(0x20); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc6); 
	SPI_WR_DATA_ID0(0x0f); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xd0); 
	SPI_WR_DATA_ID0(0xa4); 
	SPI_WR_DATA_ID0(0xa1); 
	SPI_COMMAND_END();
	//-------------------------------ST7789V gamma setting---------------------------------------// 
	SPI_WR_COMMAND_ID0(0xe0); 
	SPI_WR_DATA_ID0(0xd0); 
	SPI_WR_DATA_ID0(0x01); 
	SPI_WR_DATA_ID0(0x08); 
	SPI_WR_DATA_ID0(0x0f); 
	SPI_WR_DATA_ID0(0x11); 
	SPI_WR_DATA_ID0(0x2a); 
	SPI_WR_DATA_ID0(0x36); 
	SPI_WR_DATA_ID0(0x55); 
	SPI_WR_DATA_ID0(0x44); 
	SPI_WR_DATA_ID0(0x3a); 
	SPI_WR_DATA_ID0(0x0b); 
	SPI_WR_DATA_ID0(0x06); 
	SPI_WR_DATA_ID0(0x11); 
	SPI_WR_DATA_ID0(0x20); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xe1); 
	SPI_WR_DATA_ID0(0xd0); 
	SPI_WR_DATA_ID0(0x02); 
	SPI_WR_DATA_ID0(0x07); 
	SPI_WR_DATA_ID0(0x0a); 
	SPI_WR_DATA_ID0(0x0b); 
	SPI_WR_DATA_ID0(0x18); 
	SPI_WR_DATA_ID0(0x34); 
	SPI_WR_DATA_ID0(0x43); 
	SPI_WR_DATA_ID0(0x4a); 
	SPI_WR_DATA_ID0(0x2b); 
	SPI_WR_DATA_ID0(0x1b); 
	SPI_WR_DATA_ID0(0x1c); 
	SPI_WR_DATA_ID0(0x22); 
	SPI_WR_DATA_ID0(0x1f); 
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0x29);
	SPI_COMMAND_END();
}

void s_ST7789VRGB_Init_TM_YB(void)//tm028hdhg25-
{
	//---------------------------------------------------------------------------------------------------
	SPI_WR_COMMAND_ID0(0x11);
	SPI_COMMAND_END();
	DelayMs(120);				 //Delay 120ms
	//------------------------------display and color format setting-------------------------------
	SPI_WR_COMMAND_ID0(0x36);
	SPI_WR_DATA_ID0(0x40); //0xa0
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0x3a);
	SPI_WR_DATA_ID0(0x55);
	SPI_COMMAND_END();

	SPI_WR_COMMAND_ID0(0xb0);
	SPI_WR_DATA_ID0(0x11);
	SPI_COMMAND_END();

	SPI_WR_COMMAND_ID0(0xb1);
	SPI_WR_DATA_ID0(0x40);
	SPI_COMMAND_END();
	//--------------------------------ST7789V Frame rate setting---------------------------------
	SPI_WR_COMMAND_ID0(0xb2);
	SPI_WR_DATA_ID0(0x0c);
	SPI_WR_DATA_ID0(0x0c);
	SPI_WR_DATA_ID0(0x00);
	SPI_WR_DATA_ID0(0x33);
	SPI_WR_DATA_ID0(0x33);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xb7);
	SPI_WR_DATA_ID0(0x35);
	SPI_COMMAND_END();
	//------------------)--------------ST7789V Power setting--------------------------------------
	SPI_WR_COMMAND_ID0(0xbb);
	SPI_WR_DATA_ID0(0x15); //28h
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc0);
	SPI_WR_DATA_ID0(0x2c);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc2);
	SPI_WR_DATA_ID0(0x01);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc3);
	SPI_WR_DATA_ID0(0x0b);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc4);
	SPI_WR_DATA_ID0(0x20);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xc6);
	SPI_WR_DATA_ID0(0x0f);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xd0);
	SPI_WR_DATA_ID0(0xa4);
	SPI_WR_DATA_ID0(0xa1);
	SPI_COMMAND_END();
	//-------------------------------ST7789V gamma setting---------------------------------------//
	SPI_WR_COMMAND_ID0(0xe0);
	SPI_WR_DATA_ID0(0xd0);
	SPI_WR_DATA_ID0(0x01);
	SPI_WR_DATA_ID0(0x08);
	SPI_WR_DATA_ID0(0x0f);
	SPI_WR_DATA_ID0(0x11);
	SPI_WR_DATA_ID0(0x2a);
	SPI_WR_DATA_ID0(0x36);
	SPI_WR_DATA_ID0(0x55);
	SPI_WR_DATA_ID0(0x44);
	SPI_WR_DATA_ID0(0x3a);
	SPI_WR_DATA_ID0(0x0b);
	SPI_WR_DATA_ID0(0x06);
	SPI_WR_DATA_ID0(0x11);
	SPI_WR_DATA_ID0(0x20);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0xe1);
	SPI_WR_DATA_ID0(0xd0);
	SPI_WR_DATA_ID0(0x02);
	SPI_WR_DATA_ID0(0x07);
	SPI_WR_DATA_ID0(0x0a);
	SPI_WR_DATA_ID0(0x0b);
	SPI_WR_DATA_ID0(0x18);
	SPI_WR_DATA_ID0(0x34);
	SPI_WR_DATA_ID0(0x43);
	SPI_WR_DATA_ID0(0x4a);
	SPI_WR_DATA_ID0(0x2b);
	SPI_WR_DATA_ID0(0x1b);
	SPI_WR_DATA_ID0(0x1c);
	SPI_WR_DATA_ID0(0x22);
	SPI_WR_DATA_ID0(0x1f);
	SPI_COMMAND_END();
	SPI_WR_COMMAND_ID0(0x29);
	SPI_COMMAND_END();
}

void s_ST7789VRGB_Reset(void)
{
	spi_config(3, 1000000, 9, 0);
	DelayMs(20);
		
	SPI_WR_COMMAND_ID0(0x01); // Soft reset
	SPI_COMMAND_END();
	DelayMs(10);
	SPI_WR_COMMAND_ID0(0x028); //Display off
	SPI_COMMAND_END();
	DelayMs(1);
}
void s_ST7789VRGB_SleepIn(void)
{
	spi_config(3, 1000000, 9, 0);
	DelayMs(10);	
	SPI_WR_COMMAND_ID0(0x10); 
	SPI_COMMAND_END();
	DelayMs(5);	
}

void s_ST7789VRGB_SleepOut(void)
{
	spi_config(3, 1000000, 9, 0);
	DelayMs(10);
	SPI_WR_COMMAND_ID0(0x11); 
	SPI_COMMAND_END();
	DelayMs(120);
}

void s_ST7789VRGB_LcdPhyInit(void)
{
	char    i;
	unsigned int tmp, clk_div ;

    //1.1 初始化变量
    memset(dma_lcd_buf, 0xff, DMA_LCD_LEN);
    
    clk_div = 12;//9.5MHz

	//1.2 bcm5892 internal LCD controller
	tmp = reg32(LCD_R_LCDControl_MEMADDR);
	tmp &= ~(BIT3 | BIT2 | BIT1);
	tmp |= BIT5 | BIT3 | BIT2;
	writel(tmp, LCD_R_LCDControl_MEMADDR); // TFT, 16 bits, 5:6:5 mode

	tmp = ((lcdpara->HBP - 1) << 24) | ((lcdpara->HFP - 1) << 16)
		| ((lcdpara->HSW - 1) << 8) | ((lcdpara->PPL / 16 - 1) << 2);
	writel(tmp, LCD_R_LCDTiming0_MEMADDR);

	tmp = ((lcdpara->VBP - 1) << 24) | ((lcdpara->VFP - 1) << 16)
		| ((lcdpara->VSW - 1) << 10) | (lcdpara->LPP -1);
	writel(tmp, LCD_R_LCDTiming1_MEMADDR);

	// 10-bit clk_div 被分为两部分写入寄存器, 各5 bits.
	tmp = ((clk_div >> 5) << 27) | (clk_div & 0x1f);
	tmp |= ((lcdpara->PPL - 1) << 16)
		| LCD_F_IHS_MASK | LCD_F_IVS_MASK | LCD_F_IPC_MASK;
	writel(tmp, LCD_R_LCDTiming2_MEMADDR);

    //1.3  初始化LCD DATA IO
    
	tmp = BIT2 | BIT3 | BIT4 | BIT5           // control signals
		| BIT9 | BIT10 | BIT11 | BIT12 | BIT13          // Red signals
		| BIT14 | BIT15 | BIT16 | BIT17 | BIT18 | BIT19 // Green signals
		| BIT21 | BIT22 | BIT23 | BIT24 | BIT25;        // Blue signals
	/* LCD RGB interface */
	gpio_set_mpin_type(GPIOC, tmp, GPIO_FUNC0);

    //1.4初始化LCD SPI和LCD 模块控制器
	/* LCD SPI interface */
	gpio_set_mpin_type(GPIOC, BIT26|BIT27|BIT28, GPIO_FUNC1);
    //LCD SPI 使能信号
	gpio_set_pin_type(GPIOC, 29, GPIO_OUTPUT);
    LCD_SPI_DISABLE();

	
	spi_config(3, 1000000, 9, 0);
	s_ST7789VRGB_Init_HKF();
	

    //1.5使能BCM5892 LCD控制器
	writel(BIT16 | (lcdpara->LED - 1), LCD_R_LCDTiming3_MEMADDR); // Line end delay
	writel((uint)DMA_LCD_BASE, LCD_R_LCDUPBASE_MEMADDR);
	writel(0, LCD_R_LCDIMSC_MEMADDR); // No interrupt.

	tmp = reg32(LCD_R_LCDControl_MEMADDR);
	tmp |= BIT0;                           // LCD enable
	writel(tmp, LCD_R_LCDControl_MEMADDR);
	DelayMs(10);
	tmp |= BIT11;                          // LCD power on
	writel(tmp, LCD_R_LCDControl_MEMADDR);
	
}

void s_ST7789VRGB_TM_YB_LcdPhyInit(void)
{
	char    i;
	unsigned int tmp, clk_div ;

    //1.1 初始化变量
    memset(dma_lcd_buf, 0xff, DMA_LCD_LEN);

    clk_div = 12;//9.5MHz

	//1.2 bcm5892 internal LCD controller
	tmp = reg32(LCD_R_LCDControl_MEMADDR);
	tmp &= ~(BIT3 | BIT2 | BIT1);
	tmp |= BIT5 | BIT3 | BIT2;
	writel(tmp, LCD_R_LCDControl_MEMADDR); // TFT, 16 bits, 5:6:5 mode

	tmp = ((lcdpara->HBP - 1) << 24) | ((lcdpara->HFP - 1) << 16)
		| ((lcdpara->HSW - 1) << 8) | ((lcdpara->PPL / 16 - 1) << 2);
	writel(tmp, LCD_R_LCDTiming0_MEMADDR);

	tmp = ((lcdpara->VBP - 1) << 24) | ((lcdpara->VFP - 1) << 16)
		| ((lcdpara->VSW - 1) << 10) | (lcdpara->LPP -1);
	writel(tmp, LCD_R_LCDTiming1_MEMADDR);

	// 10-bit clk_div 被分为两部分写入寄存器, 各5 bits.
	tmp = ((clk_div >> 5) << 27) | (clk_div & 0x1f);
	tmp |= ((lcdpara->PPL - 1) << 16)
		| LCD_F_IHS_MASK | LCD_F_IVS_MASK | LCD_F_IPC_MASK;
	writel(tmp, LCD_R_LCDTiming2_MEMADDR);

    //1.3  初始化LCD DATA IO

	tmp = BIT2 | BIT3 | BIT4 | BIT5           // control signals
		| BIT9 | BIT10 | BIT11 | BIT12 | BIT13          // Red signals
		| BIT14 | BIT15 | BIT16 | BIT17 | BIT18 | BIT19 // Green signals
		| BIT21 | BIT22 | BIT23 | BIT24 | BIT25;        // Blue signals
	/* LCD RGB interface */
	gpio_set_mpin_type(GPIOC, tmp, GPIO_FUNC0);

    //1.4初始化LCD SPI和LCD 模块控制器
	/* LCD SPI interface */
	gpio_set_mpin_type(GPIOC, BIT26|BIT27|BIT28, GPIO_FUNC1);
    //LCD SPI 使能信号
	gpio_set_pin_type(GPIOC, 29, GPIO_OUTPUT);
    LCD_SPI_DISABLE();


	spi_config(3, 1000000, 9, 0);
	s_ST7789VRGB_Init_TM_YB();


    //1.5使能BCM5892 LCD控制器
	writel(BIT16 | (lcdpara->LED - 1), LCD_R_LCDTiming3_MEMADDR); // Line end delay
	writel((uint)DMA_LCD_BASE, LCD_R_LCDUPBASE_MEMADDR);
	writel(0, LCD_R_LCDIMSC_MEMADDR); // No interrupt.

	tmp = reg32(LCD_R_LCDControl_MEMADDR);
	tmp |= BIT0;                           // LCD enable
	writel(tmp, LCD_R_LCDControl_MEMADDR);
	DelayMs(10);
	tmp |= BIT11;                          // LCD power on
	writel(tmp, LCD_R_LCDControl_MEMADDR);

}

void s_TM035KBH08_LcdPhyInit(void)
{
	char    i;
	unsigned int tmp, clk_div ;

    //1.1 初始化变量
    memset(dma_lcd_buf, 0xff, DMA_LCD_LEN);
  
    clk_div = 17;//7.0MHz
	//1.2 bcm5892 internal LCD controller
	tmp = reg32(LCD_R_LCDControl_MEMADDR);
	tmp &= ~(BIT3 | BIT2 | BIT1);
	tmp |= BIT5 | BIT3 | BIT2;
	writel(tmp, LCD_R_LCDControl_MEMADDR); // TFT, 16 bits, 5:6:5 mode

	tmp = ((lcdpara->HBP - 1) << 24) | ((lcdpara->HFP - 1) << 16)
		| ((lcdpara->HSW - 1) << 8) | ((lcdpara->PPL / 16 - 1) << 2);
	writel(tmp, LCD_R_LCDTiming0_MEMADDR);

	tmp = ((lcdpara->VBP - 1) << 24) | ((lcdpara->VFP - 1) << 16)
		| ((lcdpara->VSW - 1) << 10) | (lcdpara->LPP -1);
	writel(tmp, LCD_R_LCDTiming1_MEMADDR);

	// 10-bit clk_div 被分为两部分写入寄存器, 各5 bits.
	tmp = ((clk_div >> 5) << 27) | (clk_div & 0x1f);
	tmp |= ((lcdpara->PPL - 1) << 16)
		| LCD_F_IHS_MASK | LCD_F_IVS_MASK | LCD_F_IPC_MASK;
	writel(tmp, LCD_R_LCDTiming2_MEMADDR);

    //1.3  初始化LCD DATA IO
    
	tmp = BIT2 | BIT3 | BIT4 | BIT5           // control signals
		| BIT9 | BIT10 | BIT11 | BIT12 | BIT13          // Red signals
		| BIT14 | BIT15 | BIT16 | BIT17 | BIT18 | BIT19 // Green signals
		| BIT21 | BIT22 | BIT23 | BIT24 | BIT25;        // Blue signals
	/* LCD RGB interface */
	gpio_set_mpin_type(GPIOC, tmp, GPIO_FUNC0);

    //1.4初始化LCD SPI和LCD 模块控制器
	/* LCD SPI interface */
	gpio_set_mpin_type(GPIOC, BIT26|BIT27|BIT28, GPIO_FUNC1);
    //LCD SPI 使能信号
	gpio_set_pin_type(GPIOC, 29, GPIO_OUTPUT);
    LCD_SPI_DISABLE();

	
	spi_config(3, 1000000, 16, 0);
	nt39016d_init();

    //1.5使能BCM5892 LCD控制器
	writel(BIT16 | (lcdpara->LED - 1), LCD_R_LCDTiming3_MEMADDR); // Line end delay
	writel((uint)DMA_LCD_BASE, LCD_R_LCDUPBASE_MEMADDR);
	writel(0, LCD_R_LCDIMSC_MEMADDR); // No interrupt.

	tmp = reg32(LCD_R_LCDControl_MEMADDR);
	tmp |= BIT0;                           // LCD enable
	writel(tmp, LCD_R_LCDControl_MEMADDR);
	DelayMs(10);
	tmp |= BIT11;                          // LCD power on
	writel(tmp, LCD_R_LCDControl_MEMADDR);

    //1.6初始化背光控制
	gpio_set_pin_type(GPIOC, 7, GPIO_OUTPUT); // 背光使能
	gpio_set_pin_val(GPIOC, 7, 1);
	
}

void s_DmaLcd_SetGram(int scol, int sline, int ecol, int eline)
{
	
	giLcdMinX = scol;
	giLcdIndexX = giLcdMinX;
	giLcdMaxX = ecol;//include this pixel

	giLcdMinY = sline;
	giLcdIndexY = giLcdMinY;
	giLcdMaxY = eline;//include this pixel
	
}
void s_TM035KBH08_Lcd_WrLData(ushort data) 
{ 
	int offset;
	if (giLcdIndexY > giLcdMaxY) return;
	offset = (lcdpara->PPL-1-giLcdIndexY)+giLcdIndexX*lcdpara->PPL;
	dma_lcd_buf[offset] = data;
	giLcdIndexX++; 
	if (giLcdIndexX > giLcdMaxX) 
	{ 
		giLcdIndexX = giLcdMinX; 
		giLcdIndexY++;
	}
}
void s_TM035KBH08_Lcd_FillColor(int scol, int sline, int ecol, int eline, ushort color) 
{ 
	int offset;
	giLcdMinX = scol;
	giLcdIndexX = giLcdMinX;
	giLcdMaxX = ecol;//include this pixel

	giLcdMinY = sline;
	giLcdIndexY = giLcdMinY;
	giLcdMaxY = eline;//include this pixel

	for (giLcdIndexX = giLcdMinX;giLcdIndexX <= giLcdMaxX;giLcdIndexX++)
	{
		offset = giLcdIndexX*lcdpara->PPL;
		for (giLcdIndexY = giLcdMinY;giLcdIndexY <= giLcdMaxY;giLcdIndexY++)
		{
			dma_lcd_buf[lcdpara->PPL-1-giLcdIndexY+offset] = color;
		}
	}

}
void s_TM035KBH08_Lcd_FillBuf
(int scol, int sline, int ecol, int eline, ushort *ForeBuf,ushort *BackBuf,uchar *AlphaBuf) 
{ 
	int offset;
	int bufoffsety,bufoffset;
	giLcdMinX = scol;
	giLcdIndexX = giLcdMinX;
	giLcdMaxX = ecol;//include this pixel

	giLcdMinY = sline;
	giLcdIndexY = giLcdMinY;
	giLcdMaxY = eline;//include this pixel
	
	for (giLcdIndexX = giLcdMinX;giLcdIndexX <= giLcdMaxX;giLcdIndexX++)
	{
		offset = giLcdIndexX*lcdpara->PPL;
		for (giLcdIndexY = giLcdMinY;giLcdIndexY <= giLcdMaxY;giLcdIndexY++)
		{
			bufoffsety = giLcdIndexY * lcdpara->LPP;
			bufoffset = bufoffsety+giLcdIndexX;
			dma_lcd_buf[lcdpara->PPL-1-giLcdIndexY+offset] = AlphaBuf[bufoffset] ? ForeBuf[bufoffset] : BackBuf[bufoffset];
		}
	}

}

void s_TM035KBH08_Reset(void)
{
	spi_config(3,1000000,16,0);
	DelayMs(20);

	SPI_COM_ID0(0x00,0x17);
	DelayMs(150);
}

void s_TM035KBH08_Lcd_ReadPixel(int x, int y, int *Color) 
{
     *((COLORREF *)(Color)) = dma_lcd_buf[(lcdpara->PPL-1-y)+x*lcdpara->PPL];
}

void s_S90357_Lcd_WrLData(ushort data) 
{ 
	int offset;
	if (giLcdIndexY > giLcdMaxY) return;
	offset = (lcdpara->PPL-1-giLcdIndexY)+(lcdpara->LPP-1-giLcdIndexX)*lcdpara->PPL;
	dma_lcd_buf[offset] = data;
	giLcdIndexX++; 
	if (giLcdIndexX > giLcdMaxX) 
	{ 
		giLcdIndexX = giLcdMinX; 
		giLcdIndexY++;
	}
}
void s_S90357_Lcd_FillColor(int scol, int sline, int ecol, int eline, ushort color) 
{ 
	int offset;
	giLcdMinX = scol;
	giLcdIndexX = giLcdMinX;
	giLcdMaxX = ecol;//include this pixel

	giLcdMinY = sline;
	giLcdIndexY = giLcdMinY;
	giLcdMaxY = eline;//include this pixel

	for (giLcdIndexX = giLcdMinX;giLcdIndexX <= giLcdMaxX;giLcdIndexX++)
	{
		offset = (lcdpara->LPP-1-giLcdIndexX)*lcdpara->PPL;
		for (giLcdIndexY = giLcdMinY;giLcdIndexY <= giLcdMaxY;giLcdIndexY++)
		{
			dma_lcd_buf[lcdpara->PPL-1-giLcdIndexY+offset] = color;
		}
	}

}
void s_S90357_Lcd_FillBuf
(int scol, int sline, int ecol, int eline, ushort *ForeBuf,ushort *BackBuf,uchar *AlphaBuf)
{
	int offset;
	int bufoffsety,bufoffset;
	giLcdMinX = scol;
	giLcdIndexX = giLcdMinX;
	giLcdMaxX = ecol;//include this pixel

	giLcdMinY = sline;
	giLcdIndexY = giLcdMinY;
	giLcdMaxY = eline;//include this pixel
	
	for (giLcdIndexX = giLcdMinX;giLcdIndexX <= giLcdMaxX;giLcdIndexX++)
	{
		offset = (lcdpara->LPP-1-giLcdIndexX)*lcdpara->PPL;
		for (giLcdIndexY = giLcdMinY;giLcdIndexY <= giLcdMaxY;giLcdIndexY++)
		{
			bufoffsety = giLcdIndexY * lcdpara->LPP;
			bufoffset = bufoffsety+giLcdIndexX;
			dma_lcd_buf[lcdpara->PPL-1-giLcdIndexY+offset] = AlphaBuf[bufoffset] ? ForeBuf[bufoffset] : BackBuf[bufoffset];
		}
	}
}
void s_S90357_Lcd_ReadPixel(int x, int y, int *Color) 
{
     *((COLORREF *)(Color)) = dma_lcd_buf[(lcdpara->PPL-1-y)+(lcdpara->LPP-1-x)*lcdpara->PPL];
}

/*LCD Hardware API*/
void s_LcdPhyInit(void)
{
	if (gstLcdDrv != NULL)
		gstLcdDrv->s_LcdPhyInit();
}
void s_LcdSetGram(int scol, int sline, int ecol, int eline)
{
	if (gstLcdDrv != NULL)
		gstLcdDrv->s_LcdSetGram(scol,sline,ecol,eline);
}
void s_LcdWrLData(ushort data)
{
	if (gstLcdDrv != NULL)
		gstLcdDrv->s_LcdWrLData(data);
}
void s_LcdFillColor(int scol, int sline, int ecol, int eline, ushort color)
{
	if (gstLcdDrv != NULL)
		gstLcdDrv->s_LcdFillColor(scol,sline,ecol,eline,color);
}
void s_LcdFillBuf(int scol, int sline, int ecol, int eline, ushort *ForeBuf,ushort *BackBuf,uchar *AlphaBuf)
{
	if (gstLcdDrv != NULL)
		gstLcdDrv->s_LcdFillBuf(scol, sline, ecol, eline, ForeBuf, BackBuf, AlphaBuf);
}

void s_LcdReadPixel(int x, int y, int *Color) 
{
	if (gstLcdDrv != NULL)
		return gstLcdDrv->s_LcdReadPixel(x,y,Color);
}
void s_LcdReset(void)
{
	if (gstLcdDrv && gstLcdDrv->s_LcdReset)
		gstLcdDrv->s_LcdReset();
}

void Lcd_Sleep_In()
{
	if (gstLcdDrv && gstLcdDrv->s_LcdSleepIn)
		gstLcdDrv->s_LcdSleepIn();
}
void s_LcdGetAreaSize(int *lcd_x, int *lcd_y)
{
	*lcd_x = lcdpara->LPP;
	*lcd_y = lcdpara->PPL;
}
int s_LcdGetGray(int level)
{
	return gpLcdGray[level];
}
void Lcd_Sleep_Out()
{	
	if (gstLcdDrv && gstLcdDrv->s_LcdSleepOut)
		gstLcdDrv->s_LcdSleepOut();
}
void s_LcdDrvInit(void)
{
	switch(get_lcd_type())
	{
		case TFT_S90401_LCD:
		{
			gstLcdDrv = &Tft_HX8347_Drv;
			lcdpara = &lcdparam_320X240;
			gpLcdGray = gray_320X240;
		}
		break;
		case TFT_H24C117_LCD:
		{
			gstLcdDrv = &Tft_ILI9341_Drv;
			lcdpara = &lcdparam_320X240;
			gpLcdGray = gray_320X240;
		}
		break;
		case TFT_H23B13_LCD:
		{
			gstLcdDrv = &Tft_H23B13_Drv;
			lcdpara = &lcdparam_320X240;
			gpLcdGray = gray_320X240_2;
		}
		break;
		case TFT_TM023KDZ06_LCD:
		{
			gstLcdDrv = &Tft_TM023KDZ06_Drv;
			lcdpara = &lcdparam_320X240;
			gpLcdGray = gray_320X240_2;
		}
		break;
		case TFT_H28C60_LCD:
		{
			gstLcdDrv = &Tft_ST7789V_Drv;
			lcdpara = &lcdparam_320X240;
			gpLcdGray = gray_320X240;
		}
		break;
		case DMA_S90357_LCD:
		{
			gstLcdDrv = &S800_Lcd_Drv;
			lcdpara = &lcdparam_320X240;
			gpLcdGray = gray_320X240;
		}
		break;
		case DMA_TM035KBH08_LCD:
		{
			gstLcdDrv = &S300_S900_Lcd_Drv;
			lcdpara = &lcdparam_240X320;
			gpLcdGray = gray_240X320;
		}
		break;
		case DMA_H28C69_00N_LCD:
		{
			gstLcdDrv = &D210_Lcd_Drv;
			lcdpara = &lcdparam_320X240;
			gpLcdGray = gray_320X240;
		}
		break;
		case DMA_ST7789V_TM_YB_LCD:
		{
			gstLcdDrv = &Tft_ST7789V_TM_YB_Drv;
			lcdpara = &lcdparam_320X240;
			gpLcdGray = gray_320X240;
		}
		break;
		default:
		{
			if (get_machine_type() == S800)
			{
				gstLcdDrv = &S800_Lcd_Drv;
				lcdpara = &lcdparam_320X240;
				gpLcdGray = gray_320X240;
			}
			else if ((get_machine_type() == S300) || (get_machine_type() == S900))
			{
				gstLcdDrv = &S300_S900_Lcd_Drv;
				lcdpara = &lcdparam_240X320;
				gpLcdGray = gray_240X320;
			}
			else if (get_machine_type() == D210)
			{
				gstLcdDrv = &D210_Lcd_Drv;
				lcdpara = &lcdparam_320X240;
				gpLcdGray = gray_320X240;
			}
			else
			{
				return;
			}
		}
		break;
	}
	
	giLcdIndexX = 0;
	giLcdIndexY = 0;
	giLcdMinX = 0;
	giLcdMinY = 0;
	giLcdMaxX = 0;
	giLcdMaxY = 0;

	
	s_LcdBufInit();
	s_LcdPhyInit();
	s_LcdConfigInit();
}
/*  end of clcddrv.c */
