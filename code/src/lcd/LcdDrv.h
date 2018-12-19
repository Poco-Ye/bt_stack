#ifndef _LCD_DRV_H
#define _LCD_DRV_H

#define  TFT_MAX_X    320
#define  TFT_MAX_Y    240

#define WIDTH_FOR_TFT_S800 320
#define WIDTH_FOR_S300_S900 240

#define  LCD_CONST_FLAG         0x5A
#define	 MAX_GRAY				12
#define	 MIN_GRAY				1

#define BYTE_PER_PIXEL		2
#define TOTAL_PIXEL_CNT		(320*240)

//lcd type
//for S300,S800,S900,include the S90357 and TM035KBH08
#define DMA_LCD 0
//for S58Q,S90Q
#define TFT_H24C117_LCD 1
//for S500
#define TFT_S90401_LCD 2
//for S800
#define DMA_S90357_LCD 3
//for S300,S900
#define DMA_TM035KBH08_LCD 4
//for S500
#define TFT_H28C60_LCD 5

//for D200
#define TFT_TM023KDZ06_LCD 6
#define TFT_H23B13_LCD 7

//for D210
#define DMA_H28C69_00N_LCD 10

//for D210&S800
#define DMA_ST7789V_TM_YB_LCD 11

#define ALPHA_MODE_USEFORE 0xff
#define ALPHA_MODE_USEBACK 0

//定义命令宏
#define TFT_ENTER_SLEEP_MODE 0x10 
#define TFT_EXIT_SLEEP_MODE 0x11 
#define TFT_SET_COLUMN_ADDRESS 0x2A  //Set the column address
#define TFT_SET_PAGE_ADDRESS 0x2B // Set the page address
#define TFT_WRITE_MEMORY_START 0x2C //Transfer image information from the host processor interface to the SSD1960 starting at the location provided by set_column_address and set_page_address
#define TFT_WRITE_MEMORY_CONTINUE 0x3C // Transfer image information from the host processor

#define RGB_H(rgb) ((uchar)((rgb)>>8))
#define RGB_L(rgb) ((uchar)(rgb))

//for tft
#define  LCD_COMMAND   	0x3d000000		//SMC CS1
#define  LCD_DATA      	0x3d000080   		//SMC CS1 +ADDR[7]

#define s_TFT_WrCmd(data_in)    *(volatile uchar*)(LCD_COMMAND) = (uchar)(data_in)
#define s_TFT_WrData(data_in)   *(volatile uchar*)(LCD_DATA) = (uchar)(data_in)
#define s_TFT_ReadData(data_out) *(uchar *)(data_out) = *(volatile uchar*)(LCD_DATA)


enum PAINT_LAYER
{
	FORE_LAYER,
	BACK_LAYER,
};	

enum LCD_ROLE
{ 	MONO_ROLE=0, 
	COLOR_ROLE,
	LCD_ROLE_LIMIT,
};

typedef struct 
{
	int cursor_x;
	int cursor_y;
	int width;
	int height;
	int lineCount;
	int charSpace;
	int lineSpace;
	int brightness;
	int grayScale;
	int reverse;
	int forecolor;
	int backcolor;
	int denominator;
	int numerator;
    int isfull;	
}LCD_ATTR_T;

typedef struct 
{
	LCD_ATTR_T tLcdAttr[LCD_ROLE_LIMIT];
	int role;
}LCD_T;

// 定义兼容模式下app可操作屏幕范围的原点(左上角)在LCD屏幕上的坐标。
typedef struct {
	int x;
	int y;
} app_origin_t;

void s_LcdInit(void);
void s_LcdWrite(unsigned int col, unsigned int line,unsigned char *dat, unsigned int datLen);
void s_LcdSetIcon(unsigned char icon_no, unsigned char mode);
unsigned char ReadLCDGray(); /*  LCD灰度读写操作 */
int  WriteLCDGray(unsigned char LcdGrayVal);

#endif /* endof __LCD_DRV_H__ */
