#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "base.h"
#include "posapi.h"
#include "../Font/font.h"
#include "printer.h"

typedef struct _PRN_HAL_FUNC
{
    void (*s_HalInit)(unsigned char prnType);
    unsigned char (*s_PrnStart)(void);
    unsigned char (*s_PrnStatus)(void);
    int (*s_GetPrnTemp)(void);
    void (*s_PrnStop)(void);
}PRN_HAL_FUNC;

static PRN_HAL_FUNC prn_hal_func;

volatile uchar k_PrnStatus = 0;        // 打印状态
static int k_PrnReverse;
static int k_CharSpace,k_LineSpace;                // 行列间距      
static int k_AscDoubleWidth,k_AscDoubleHeight;     // ASCII字体倍高倍宽标志
static int k_LocalDoubleWidth,k_LocalDoubleHeight; // 汉字倍高倍宽标志
int k_CurDotCol,k_LeftIndent,k_CurHeight;   // 当前列,左边界,当前字体高度变量
volatile int k_CurDotLine,k_CurPrnLine;              // 打印缓冲区行控制变量
uchar k_DotBuf[MAX_DOT_LINE+1][48];           // 打印缓冲区
static uchar k_ExBuf[2*MAX_FONT_HEIGHT][48];
unsigned int k_GrayLevel = 100;
unsigned int k_FeedStat = 0;
//printer API
uchar PrnInit_thermal(void)
{
    k_PrnReverse=0;

    PrnFontSet(1,1);
    k_CharSpace     = 0;
    k_LineSpace     = 0;
    k_CurDotCol     = 0;
    k_CurHeight     = 0;
    k_CurDotLine    = 0;
    k_LeftIndent    = 0;
    k_FeedStat      = 1;//默认有预走纸
    memset(k_DotBuf,0,sizeof(k_DotBuf));
    memset(k_ExBuf,0,sizeof(k_ExBuf));
    k_PrnStatus = PRN_OK;
    PrnSetGray(1);
    return 0;
}

#define COMPEN_MAX_DOTS  (240)
#define COMPEN_MIN_DOTS  (74)

void CompensatePapar(int pixel)
{
    uchar buff[48];
    uint i,max=COMPEN_MAX_DOTS;

    if(0 == pixel) return;
    memset(buff,0,sizeof(buff));
    if(k_CurDotLine<=COMPEN_MAX_DOTS) max = k_CurDotLine;
    for(i=0;i<max;i++)
    {
       if(memcmp(k_DotBuf[k_CurDotLine-(i+1)],buff,sizeof(buff))) break;    
    }
    if((i>=COMPEN_MIN_DOTS) && (i< 120)) PrnStep(pixel);
    return;
}

uchar PrnStart_thermal(void)
{
    return prn_hal_func.s_PrnStart();
}

void PrnDoubleWidth_thermal(int AscDoubleWidth, int LocalDoubleWidth)
{
    if ((AscDoubleWidth != 0) && (AscDoubleWidth != 1))
    {
        return;
    }
    if ((LocalDoubleWidth != 0) && (LocalDoubleWidth != 1))
    {
        return;
    }
	k_AscDoubleWidth=AscDoubleWidth;
	k_LocalDoubleWidth=LocalDoubleWidth;
}

void PrnDoubleHeight_thermal(int AscDoubleHeight, int LocalDoubleHeight)
{
    if (AscDoubleHeight != 0 && AscDoubleHeight != 1)
    {
        return;
    }
    if ((LocalDoubleHeight != 0) && (LocalDoubleHeight != 1))
    {
        return;
    }
	k_AscDoubleHeight = AscDoubleHeight; 
	k_LocalDoubleHeight = LocalDoubleHeight;
}
void PrnAttrSet_thermal(int Reverse)
{
    if ((Reverse != 0) && (Reverse != 1))
    {
        return;
    }
	k_PrnReverse=Reverse;
}

void PrnFontSet_thermal(uchar Ascii, uchar Locale)
{
    int AscFont,LocaleFont;
/*
bAscii - Ascii码的字体设置
0 ' 	8x16字体[基本]
1 ' 	16x24字体[基本]
2 ' 	8x16字体纵向放大
3 ' 	16x24字体纵向放大
4 ' 	8x16字体横向放大
5 ' 	16x24字体横向放大
6 ' 	8x16字体整体放大
7 ' 	16x24字体整体放大
其他值'不改变原设置
bLocale - 扩展码的字体设置.
0 ' 	16x16字体[基本]
1 ' 	24x24字体[基本]
2 ' 	16x16字体纵向放大
3 ' 	24x24字体纵向放大
4 ' 	16x16字体横向放大
5 ' 	24x24字体横向放大
6 ' 	16x16字体整体放大
7 ' 	24x24字体整体放大
*/
    if(Ascii>=8 || Locale>=8) return;
    //  set ascii font
    if((Ascii==0) || (Ascii==2) || (Ascii==4) || (Ascii==6))
    {
        AscFont = SMALL_FONT;
    }
    else
    {
        AscFont = BIG_FONT;
    }
    if((Ascii==4) || (Ascii==5) || (Ascii==6) || (Ascii==7))
    {
        k_AscDoubleWidth = 1;
    }
    else
    {
        k_AscDoubleWidth = 0;
    }
    if((Ascii==2) || (Ascii==3) || (Ascii==6) || (Ascii==7))
    {
        k_AscDoubleHeight = 1;
    }
    else
    {
        k_AscDoubleHeight = 0;
    }
    //  set local font
    if((Locale==0) || (Locale==2) || (Locale==4) || (Locale==6))
    {
        LocaleFont = SMALL_FONT;
    }
    else
    {
        LocaleFont = BIG_FONT;
    }
    if((Locale==4) || (Locale==5) || (Locale==6) || (Locale==7))
    {
        k_LocalDoubleWidth = 1;
    }
    else
    {
        k_LocalDoubleWidth = 0;
    }
    if((Locale==2) || (Locale==3) || (Locale==6) || (Locale==7))
    {
        k_LocalDoubleHeight = 1;
    }
    else
    {
        k_LocalDoubleHeight = 0;
    }
    s_SetPrnFont(AscFont,LocaleFont);
}
// 设置行列间距
void PrnSpaceSet_thermal(uchar x, uchar y)
{
    k_CharSpace = x;
    k_LineSpace = y;
}
// 设置字符间距
static void  PrnSetSpCh(uchar x)
{
    k_CharSpace = x;
}
// 设置行间距
static void  PrnSetSpLi(uchar y)
{
    k_LineSpace = y;
}
void PrnLeftIndent_thermal(ushort x)
{
    if(x > 300)     // 336
        x = 300;    // 336
    if(k_LeftIndent != k_CurDotCol) s_NewLine();
    k_LeftIndent = (int)x;
    k_CurDotCol  = (int)x;
}

//  调用s_NewLine函数请注意判断是否可以成功填充到打印缓冲
int s_NewLine(void)
{
    int i,j;

    if(k_PrnStatus == PRN_OUTOFMEMORY) return -1;
    if(k_CurDotCol == k_LeftIndent)
    {
        //k_CurHeight = 16;
        if(k_CurDotLine+16+k_LineSpace > MAX_DOT_LINE)
        {
            k_PrnStatus  = PRN_OUTOFMEMORY;
            k_CurDotLine = MAX_DOT_LINE;
            return -1;
        }
        k_CurDotLine += (16+k_LineSpace);
        return 0;
    }
    if(k_CurDotLine+k_LineSpace+k_CurHeight > MAX_DOT_LINE)
    {
        k_PrnStatus  = PRN_OUTOFMEMORY;
        k_CurDotLine = MAX_DOT_LINE;
        return -1;
    }
	if(k_CurDotCol>384) k_CurDotCol=384;
    for(i=0; i<k_CurHeight; i++)
    {
        for(j=k_LeftIndent/8; j<(k_CurDotCol+7)/8; j++)   //  for(j=0;j<48;j++)
        {
            k_DotBuf[k_CurDotLine+i][j] |= k_ExBuf[2*MAX_FONT_HEIGHT-k_CurHeight+i][j];//写入打印缓冲
        }
    }
    k_CurDotLine += (k_LineSpace+k_CurHeight);
    k_CurDotCol   = k_LeftIndent;
    k_CurHeight   = 0;
    memset(k_ExBuf,0,sizeof(k_ExBuf));
    return 0;
}

int  PrnPreFeedSet_thermal(unsigned int cmd)
{
    k_FeedStat = cmd;
    return 0;
}

void PrnStep_thermal(short pixel)
{
	if(pixel==0) return ;
	if(k_CurDotLine+pixel<0)
	{
		k_PrnStatus = PRN_OUTOFMEMORY;
		return ;
	}
	if(k_PrnStatus==PRN_OUTOFMEMORY) return;
	if(k_CurDotCol!=k_LeftIndent)
	{
		if(s_NewLine()) return;
	}
	if(k_CurDotLine+pixel>MAX_DOT_LINE)
	{
		k_PrnStatus = PRN_OUTOFMEMORY;
	//  pixel = MAX_DOT_LINE-k_CurDotLine;
		k_CurDotLine = MAX_DOT_LINE;
		return;
	}
	k_CurDotLine += pixel;
}
static void FillSpace(unsigned char *tDotPtr, int Height, int tmpHeight)
{
    int i,j,shift;
    int tmpSpace;
    uchar ch,ch1,*tmPtr,*tmPtr2;

    if(k_CurDotCol+k_CharSpace > 384) tmpSpace = 384-k_CurDotCol;
	else tmpSpace=k_CharSpace;
	if(!tmpSpace) return;
  
    shift     = k_CurDotCol % 8;

    for(i=0; i<Height; i++)
    {
        tmPtr  = tDotPtr;
        tmPtr2 = tDotPtr + 48;
        // 填充点阵
        for(j=0; j<tmpSpace; j+=8)
        {
            ch  = 0xff;
			if(j+8>tmpSpace){
				ch &= (0xff<<(j+8-tmpSpace));
			}
            ch1 = ch>>shift;
            *tmPtr++ |= ch1;
            ch <<= (8-shift);
            *tmPtr |= ch;
           // 倍高填充
            if(tmpHeight)
            {
                *tmPtr2++ |= ch1;
                *tmPtr2   |= ch;
            }
        }
        if(tmpHeight) tDotPtr += 96;
        else tDotPtr += 48;
    }
    k_CurDotCol += k_CharSpace;
}

static int s_ProcChar(uchar *str)
{
    int i,j,shift,tmpHeight,CodeBytes;
    int Width,Height,tmpHeightBak;
    uchar ch,ch1,*tmPtr,*tDotPtr,*tmPtr2,*Font;
    uchar tmpBuf[(MAX_FONT_WIDTH/8)*MAX_FONT_HEIGHT*2+300];

	CodeBytes = s_GetPrnFontDot(k_PrnReverse, k_AscDoubleWidth, k_LocalDoubleWidth,str, tmpBuf, &Width, &Height);

    if(k_CurDotCol+Width > 384)
    {
        if(s_NewLine()) return CodeBytes;
    }

    if(CodeBytes==1 && k_AscDoubleHeight || CodeBytes==2 && k_LocalDoubleHeight){
        tmpHeight = Height*2;
	}
    else tmpHeight = Height;
	
	tmpHeightBak=tmpHeight;
	if(k_CurHeight < tmpHeight) k_CurHeight = tmpHeight;

    tDotPtr   = k_ExBuf[2*MAX_FONT_HEIGHT-tmpHeight] + k_CurDotCol/8;
    shift     = k_CurDotCol % 8;
    Font      = tmpBuf;

	if(tmpHeight==Height) tmpHeight=0;
	else tmpHeight=1;

    for(i=0; i<Height; i++)
    {
        tmPtr  = tDotPtr;
        tmPtr2 = tDotPtr + 48;
        // 填充点阵
        for(j=0; j<Width; j+=8)
        {
            ch  = *Font++;
			if(j+8>Width){
				ch &= (0xff<<(j+8-Width));
			}
            ch1 = ch>>shift;
            *tmPtr++ |= ch1;
            ch <<= (8-shift);
            *tmPtr |= ch;
           // 倍高填充
           if(tmpHeight)
            {
                *tmPtr2++ |= ch1;
                *tmPtr2   |= ch;
            }
        }
        if(tmpHeight) tDotPtr += 96;
        else tDotPtr += 48;
    }
	if(k_PrnReverse && k_CharSpace)
	{
		k_CurDotCol += Width;
		tDotPtr  = k_ExBuf[2*MAX_FONT_HEIGHT-tmpHeightBak] + k_CurDotCol/8;
		FillSpace(tDotPtr,Height,tmpHeight);
	}
    else k_CurDotCol += (Width+k_CharSpace);
	
	return CodeBytes;
}

uchar s_PrnStr_thermal(uchar *str)
{
    int i,CodeBytes;
    
    i = 0;
    while(1)
    {

        if(k_PrnStatus == PRN_OUTOFMEMORY)
            return(k_PrnStatus);
        if(!str[i])
            return 0;
        if(str[i] == '\n')
        {
            if(s_NewLine()) return(k_PrnStatus);
            i++;
            continue;
        }
        if(str[i] == '\f')
        {
            if(k_CurDotLine != k_LeftIndent)
            {
                if(s_NewLine()) return(k_PrnStatus);
            }
            //k_FontDotLine=k_CurDotLine;
            PrnStep(PAGE_LINES);
            i++;
            continue;
        }
        else
        {
            CodeBytes=s_ProcChar(str+i);
            i += CodeBytes;
        }
    }
    return 0;
}

uchar PrnStr_thermal(char *str,...)
{
    va_list marker;
    char glBuff[2048];
    ushort len;

    glBuff[0] = 0;
    va_start(marker, str);
    vsprintf(glBuff,str,marker);
    va_end( marker );
    len = strlen(glBuff);
    if(!len)        return 0;
    if(len>2048)    return 0xfe;
    return(s_PrnStr_thermal(glBuff));
}
uchar PrnLogo_thermal(uchar *logo)
{
    uchar   *DotPtr,*tPtr;
    int     i,j,len,BmpLen,LeftBytes,LeftBits;

    if(k_PrnStatus == PRN_OUTOFMEMORY)
        return(k_PrnStatus);
    if(k_LeftIndent != k_CurDotCol)
        if(s_NewLine()) return(k_PrnStatus);

    LeftBytes = k_CurDotCol/8;
    LeftBits  = k_CurDotCol%8;
    tPtr      = logo + 1;
    for(i=0; i<logo[0]; i++)
    {
        DotPtr = k_DotBuf[k_CurDotLine];
        len    = tPtr[0] * 256 + tPtr[1];
        if(len > 48)
            BmpLen = 48;
        else
            BmpLen = len;
        tPtr += 2;
        for(j=0; LeftBytes+j<48 && j<BmpLen; j++)
        {
            DotPtr[LeftBytes+j] |= (tPtr[j]>>LeftBits);
            if(LeftBytes+j<47)
                DotPtr[LeftBytes+j+1] |= (tPtr[j]<<(8-LeftBits));
        }
        tPtr += len;
        k_CurDotLine++;
        if(k_CurDotLine >= MAX_DOT_LINE)
        {
            k_PrnStatus = PRN_OUTOFMEMORY;
            return(k_PrnStatus);
        }
    }
    return 0;
}

uchar PrnStatus_thermal(void)
{
    return prn_hal_func.s_PrnStatus();
}

int PrnTemperature_thermal(void)
{
    return(prn_hal_func.s_GetPrnTemp());
}

int PrnGetDotLine_thermal(void)
{
	return k_CurDotLine;
}

void PrnSetGray_thermal(int Level)
{
    if(Level==0)//70%
    {
        k_GrayLevel = 70;
    }
    else if(Level==1)//100%100
    {
        k_GrayLevel = 100;
    }
    else if(Level==3)//300%100
    {
        k_GrayLevel = 300;
    }
    else if(Level==4)//400%100
    {
        k_GrayLevel = 400;
    }
    else if((Level>=50)&&(Level<=500))//50%100~500%100
    {
        k_GrayLevel = Level;
    }
}

void s_PrnStop_thermal(void)
{
    prn_hal_func.s_PrnStop();
}

 //读取打印机类型					    
unsigned char read_prn_type(void)
{
	uchar buff[20];
	int ret;
    static uchar type=0xFF;
    
    if(type != 0xFF) return type;
    
	memset(buff, 0x00, sizeof(buff));
	ReadCfgInfo("PRINTER", buff);
    if (!memcmp(buff, "PRT-48F", strlen("PRT-48F")))type=TYPE_PRN_PRT48F;
	else if(!memcmp(buff, "PT486F", strlen("PT486F"))) type=TYPE_PRN_PRT486F;
	else if(!memcmp(buff, "01", strlen("01"))) type=TYPE_PRN_PT48D;
    else type=TYPE_PRN_LTPJ245G;
    
    return type;
}
 
/*精工打印机*/
extern void s_PrnHalInit_SII(uchar prnType);
extern int s_GetPrnTemp_SII(void);
extern uchar s_PrnStart_SII(void);
extern uchar s_PrnStatus_SII(void);
extern void s_PrnStop_SII(void);
/*普瑞特打印机*/
extern void s_PrnHalInit_PRT(uchar prnType);
extern int s_GetPrnTemp_PRT(void);
extern uchar s_PrnStart_PRT(void);
extern uchar s_PrnStatus_PRT(void);
extern void s_PrnStop_PRT(void);

//==============================================================================
//系统的打印机初始化
void s_PrnInit_thermal(void)
{
    uchar type = read_prn_type();

    switch(type)
    {
    case TYPE_PRN_PRT48F:
	case TYPE_PRN_PRT486F:
	case TYPE_PRN_PT48D:
        prn_hal_func.s_HalInit = s_PrnHalInit_PRT;
        prn_hal_func.s_GetPrnTemp = s_GetPrnTemp_PRT;
        prn_hal_func.s_PrnStart = s_PrnStart_PRT;
        prn_hal_func.s_PrnStatus = s_PrnStatus_PRT;
        prn_hal_func.s_PrnStop = s_PrnStop_PRT;
        break;
        
    case TYPE_PRN_LTPJ245G:
    /*
    默认使用精工打印机控制算法，因为精工打印机算法单次加热时间短。
    普瑞特算法单次加热时间长，有可能烧毁精工打印机。
    */
    default:
        prn_hal_func.s_HalInit = s_PrnHalInit_SII;
        prn_hal_func.s_GetPrnTemp = s_GetPrnTemp_SII;
        prn_hal_func.s_PrnStart = s_PrnStart_SII;
        prn_hal_func.s_PrnStatus = s_PrnStatus_SII;
        prn_hal_func.s_PrnStop = s_PrnStop_SII;
        break;
    }
    
    prn_hal_func.s_HalInit(type);
    
    PrnInit();
	ScrSetIcon(ICON_PRINTER,0);
}

