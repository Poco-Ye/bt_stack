#ifndef _LCD_API_H
#define _LCD_API_H

#define _RED(c)        (((unsigned short)c & 0x00f8) << 8)
#define _GREEN(c)      (((unsigned short)c & 0x00fc) << 3)
#define _BLUE(c)       (((unsigned short)c & 0x00f8) >> 3)
#define RGB(r, g, b)   (_RED(r) | _GREEN(g) | _BLUE(b))

#define RGB_RED(c)     (((unsigned short)c & 0xf800) >> 8)
#define RGB_GREEN(c)   (((unsigned short)c & 0x07e0) >> 3)
#define RGB_BLUE(c)    (((unsigned short)c & 0x001f) << 3)

#define COLOR_WHITE   0xffffff
#define COLOR_BLACK   0x000000

typedef struct {
	unsigned int width;
	unsigned int height;
	unsigned int PPL;   /*pixel per line*/
	unsigned int HBP;
	unsigned int HFP;
	unsigned int HSW;
	unsigned int LPP;   /*line per panel*/
	unsigned int VSW;
	unsigned int VBP;
	unsigned int VFP;
	unsigned int LED;
} LCDPARAM;           // LCD屏幕物理参数

typedef struct __ST_LCDINFO{
	unsigned int width;
	unsigned int height;
	unsigned int ppl;
	unsigned int ppc;
	unsigned int fgColor;
	unsigned int bgColor;
	int reserved[8];
}ST_LCD_INFO;


// app可用屏坐标转换为LCD逻辑坐标
#define APPX(x)      ((x) + ptLcdAttr->left)
#define APPY(y)      ((y) + ptLcdAttr->top)

// 检查app可用屏坐标是否有效
#define isValidX(x)  (((x) >= 0) && ((x) < ptLcdAttr->width))
#define isValidY(y)  (((y) >= 0) && ((y) < ptLcdAttr->height))
#define isValidPixel(x, y)  (isValidX(x) && isValidY(y))

//Init Interface
void s_ScrInit(void);

//Old API for application
void ScrCls(void);
void ScrClrLine(unsigned char startline, unsigned char endline);
void ScrGray(unsigned char mode);
void ScrBackLight(unsigned char mode);
void ScrGotoxy (unsigned char x,unsigned char y);
unsigned char ScrAttrSet(unsigned char Attr);
void ScrPlot (unsigned char x,unsigned char y,unsigned char Color);
void ScrPrint(unsigned char col,unsigned char row,unsigned char mode, char *str,...);
void s_ScrPrint(unsigned char col,unsigned char row,unsigned char mode, char *str);
void ScrDrLogo(unsigned char *logo);
void ScrSetIcon(unsigned char icon_no,unsigned char mode);
void ScrDrLogoxy(int left, int top, unsigned char *logo);
void ScrDrawBox(unsigned char y1,unsigned char x1,unsigned char y2,unsigned char x2);
unsigned char ScrRestore(unsigned char mode);
int Lcdprintf(unsigned char *str,...);
void s_Lcdprintf(unsigned char *str);

//New API for application
void ScrGotoxyEx (int pixel_X, int pixel_Y);
void ScrGetxyEx(int *pixel_X, int *pixel_Y);  
void ScrDrawRect(int left,int top, int right,int bottom);
void ScrClrRect(int left,int top, int right,int bottom);
void ScrSpaceSet(int CharSpace, int LineSpace);
void ScrGetLcdSize(int *width, int *height);
void ScrTextOut(int pixel_X, int pixel_Y, unsigned char *txt);

#endif

