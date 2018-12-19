
#ifndef PRINTER_T
#define PRINTER_T


#define MAXPRNBUFFER	500000
#define ONELINEMARK		1
#define ONELINEBYTES	49
#define MAX_PRNLINES	MAXPRNBUFFER/ONELINEBYTES

#ifndef     ON
#define     ON          1
#endif

#ifndef     OFF
#define     OFF         0
#endif

#ifndef     LF
#define     LF          0x0a
#endif
#ifndef     FF
#define     FF          0x0c
#endif

/******   error no   **********************************************/
#define PRN_OK                  0x00
#define PRN_BUSY				0x01
#define PRN_PAPEROUT			0x02
#define PRN_WRONG_PACKAGE		0x03
#define PRN_FAULT				0x04
#define PRN_TOOHEAT				0x08
#define PRN_UNFINISHED			0xf0
#define PRN_NOFONTLIB			0xfc
#define PRN_OUTOFMEMORY			0xfe

#define TYPE_PRN_UNKNOW 0xff

#define TYPE_PRN_PRT48A		1
#define TYPE_PRN_PRT488A	2
#define TYPE_APS_SS205		3
#define TYPE_PRN_PRT486F	4
#define TYPE_PRN_PRT48F    5
#define TYPE_PRN_LTPJ245G   6
#define TYPE_PRN_PT48D		11


#define PAGE_LINES          200
#define MAX_DOT_LINE        5000

#define SMALL_FONT          0
#define BIG_FONT            1

extern volatile int k_CurDotLine,k_CurPrnLine;              // 打印缓冲区行控制变量
extern uchar k_DotBuf[MAX_DOT_LINE+1][48];           // 打印缓冲区
extern unsigned int k_GrayLevel;
extern volatile uchar       k_PrnStatus;        // 打印状态
extern int k_CurDotCol,k_LeftIndent,k_CurHeight;   // 当前列,左边界,当前字体高度变量
extern unsigned int k_FeedStat;

extern int s_NewLine(void);
extern void CompensatePapar(int pixel);
extern unsigned char read_prn_type(void);

#endif //PRINTER_T

