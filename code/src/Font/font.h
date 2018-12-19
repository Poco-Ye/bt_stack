#ifndef _FONT_H
#define _FONT_H

#define MAX_FONT_NUMS       64
#define MAX_FONT_WIDTH      48
#define MAX_FONT_HEIGHT     48

#define CHARSET_WEST        0x01 
#define CHARSET_TAI         0x02 
#define CHARSET_MID_EUROPE  0x03     
#define CHARSET_VIETNAM     0x04     
#define CHARSET_GREEK       0x05     
#define CHARSET_BALTIC      0x06     
#define CHARSET_TURKEY      0x07     
#define CHARSET_HEBREW      0x08     
#define CHARSET_RUSSIAN     0x09     
#define CHARSET_GB2312      0x0A     
#define CHARSET_GBK         0x0B     
#define CHARSET_GB18030     0x0C     
#define CHARSET_BIG5        0x0D     
#define CHARSET_SHIFT_JIS   0x0E     
#define CHARSET_KOREAN      0x0F     
#define CHARSET_ARABIA      0x10   
#define CHARSET_DIY         0x11

typedef struct{
	int CharSet;
	int Width;
	int Height;
	int Bold;
	int Italic;
}ST_FONT;

//Init Interface
void s_FontInit(unsigned char *FontLibPtr);

//New API for Monitor
unsigned char s_GetFontVer(void);
void s_GetCurScrFont(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont);
int  s_GetPrnFontDot(int Reverse, int SingleDoubleWidth, int MultiDoubleWidth,unsigned char *str, unsigned char *dot, int *width, int *height);
int  s_GetLcdFontDot(int Reverse, unsigned char *str, unsigned char *buf, int *width, int *height);
//For compatible
void s_SetPrnFont(unsigned char Ascii, unsigned char Cfont);

//New API for Application
int  EnumFont(ST_FONT *Fonts, int MaxFontNums);
int  SelScrFont(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont);
int  SelPrnFont(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont);

//Old API for application
int  ReadFontLib(unsigned long Offset,unsigned char *FontData, int ReadLen);
int  PrnGetFontDot(int FontSize, unsigned char *str, unsigned char *OutDot);
unsigned char  ScrFontSet(unsigned char fontsize);

#endif
