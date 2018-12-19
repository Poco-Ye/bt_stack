#include <stdio.h>
#include <string.h>
#include "font.h"
#include "base.h"
#include "posapi.h"
#include "..\lcd\LcdDrv.h"

typedef struct{
	ST_FONT SingleCodeFont;
	unsigned char *SingleDotPtr;
	unsigned long SingleFontLibLen;
	ST_FONT MultiCodeFont;
	unsigned char *MultiDotPtr;
	unsigned long MultiFontLibLen;
}ST_CURRENT_FONT;

typedef struct{
	int CodeType;
	unsigned long FontLibLen;
	ST_FONT Font;
	unsigned char *FontDotPtr;
}ST_FONT_TABLE;

static int k_FontNum,k_FontVer;
static int k_OldScrFontSize,k_OldPrnFontSize; //For compatible
static unsigned char *k_FontLibPtr;
static ST_CURRENT_FONT k_LcdFont,k_PrnFont;
static ST_FONT_TABLE k_FontTable[MAX_FONT_NUMS];


#define MAX_DIFF_MAP_NUM	1
typedef struct { 
	ST_FONT font[MAX_DIFF_MAP_NUM];
	int	matched; 
}MAP_FONT_T;

/* used for judging whether a fontlib is font-mapping supporting */
MAP_FONT_T DIFF_MAP_FONT = {{1,   8,   16, 0, 0}, 0};
uint k_sFontMapStatus =0, k_mFontMapStatus =0;
ST_FONT tBkMapSingleFont, tBkMapMultiFont;

//for GBK font
static const struct{
	unsigned char ch;
	unsigned char endch;
	int CharNum;
	int flag;
}GBKPage[6]={
	{0x81,0xA0,6080,1},
	{0xA1,0xA7,658,2},
	{0xA8,0xA9,380,1},
	{0xAA,0xAF,576,3},
	{0xB0,0xF7,13680,1},
	{0xF8,0xFE,672,3}
};

//for setting Default LCD & printer font
static const unsigned char DefLcdSingleCodeFont[4][4]={
	{6,8,0,0},{8,16,0,0},{6,12,0,0},{0,0,0,1}
};
static const unsigned char DefLcdMultiCodeFont[4][4]={
	{16,16,1,0},{12,12,1,0},{14,14,1,0},{0,0,1,1}
};
static const unsigned char DefPrnSingleCodeFont[4][4]={
	{12,24,0,0},{8,16,0,0},{10,20,0,0},{0,0,0,1}
};
static const unsigned char DefPrnMultiCodeFont[4][4]={
	{24,24,1,0},{16,16,1,0},{20,20,1,0},{0,0,1,1}
};

static ST_FONT SelectedSingleCodeFont;
static ST_FONT SelectedMultiCodeFont;
static char SaveCurFontFlag;

extern const unsigned char DefASCFont6X8[];
extern const unsigned char DefASCFont8X16[];

unsigned long StrToLong(unsigned char *str)
{
	unsigned long tmp;

	tmp=str[0];
	tmp<<=8;
	tmp+=str[1];
	tmp<<=8;
	tmp+=str[2];
	tmp<<=8;
	tmp+=str[3];
	return tmp;
}

/*
Search for suited font
FontAttr[0]  width	
FontAttr[1]  height
FontAttr[2]  CodeType
FontAttr[3]  Only match CodeType
*/
static int FindFont(const unsigned char *FontAttr)
{
	int i;

	for(i=0;i<k_FontNum;i++)
	{
		if(k_FontTable[i].Font.Width==FontAttr[0] && k_FontTable[i].Font.Height==FontAttr[1] && 
			k_FontTable[i].CodeType==FontAttr[2]){
				if(!k_FontTable[i].Font.Bold && !k_FontTable[i].Font.Italic) break;
		}
		if(FontAttr[3] && k_FontTable[i].CodeType == FontAttr[2]) break;
	}
	return i;
}

// for set default font
static void SetDefaultFont(ST_CURRENT_FONT *Font, const unsigned char DefFontAttr[4][4], int SingleFlag)
{
    int i, j;

    for (i = 0; i < 4; i++)
    {
        j = FindFont(DefFontAttr[i]);
        if (j < k_FontNum)
            break;
    }

    if (i >= 4)
        return;

    if (SingleFlag)
    {
        memcpy(&Font->SingleCodeFont, &k_FontTable[j].Font, sizeof(ST_FONT));
        Font->SingleDotPtr = k_FontTable[j].FontDotPtr;
        Font->SingleFontLibLen = k_FontTable[j].FontLibLen;
    }
    else
    {
        memcpy(&Font->MultiCodeFont, &k_FontTable[j].Font, sizeof(ST_FONT));
        Font->MultiDotPtr = k_FontTable[j].FontDotPtr;
        Font->MultiFontLibLen = k_FontTable[j].FontLibLen;
    }
}

static int s_SelectFont(ST_CURRENT_FONT *CurFont,ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont)
{
    int i, isGBKExisted=0, iGBKIndex=0;

    if (SingleCodeFont != NULL)
    {
        if ((SingleCodeFont->Width > MAX_FONT_WIDTH) || 
			(SingleCodeFont->Height > MAX_FONT_HEIGHT))
        {
            return -3;
        }

        for (i = 0; i < k_FontNum; i++)
        {
            if (!memcmp(&k_FontTable[i].Font, SingleCodeFont, sizeof(ST_FONT)))
            {
                break;
            }
        }

        if (i < k_FontNum && !k_FontTable[i].CodeType)
        {
            memcpy(&CurFont->SingleCodeFont, SingleCodeFont, sizeof(ST_FONT));
            /* print the info */
            CurFont->SingleDotPtr = k_FontTable[i].FontDotPtr;
            CurFont->SingleFontLibLen = k_FontTable[i].FontLibLen;
        }
        else return -1;
    }

    if (MultiCodeFont != NULL)
    {
        if ((MultiCodeFont->Width > MAX_FONT_WIDTH) || 
			(MultiCodeFont->Height > MAX_FONT_HEIGHT))
        {
            return -4;
        }

        for (i = 0; i < k_FontNum; i++)
        {
            if (!memcmp(&k_FontTable[i].Font, MultiCodeFont, sizeof(ST_FONT)))
            {
                break;
            }

			if (k_FontTable[i].Font.CharSet == CHARSET_GBK && 
				MultiCodeFont->CharSet == CHARSET_GB2312)
			{
				if (k_FontTable[i].Font.Width == MultiCodeFont->Width &&
					k_FontTable[i].Font.Height == MultiCodeFont->Height &&	
					k_FontTable[i].Font.Bold == MultiCodeFont->Bold && 
					k_FontTable[i].Font.Italic == MultiCodeFont->Italic)
				{
					isGBKExisted = 1;
					iGBKIndex = i;
				}
			}
        }

        if (i < k_FontNum && k_FontTable[i].CodeType)
        {
            memcpy(&CurFont->MultiCodeFont, MultiCodeFont, sizeof(ST_FONT));
            CurFont->MultiDotPtr = k_FontTable[i].FontDotPtr;
            CurFont->MultiFontLibLen = k_FontTable[i].FontLibLen;
        }
		else if(isGBKExisted == 1)
		{
			memcpy(&CurFont->MultiCodeFont, MultiCodeFont, sizeof(ST_FONT));
			CurFont->MultiCodeFont.CharSet = CHARSET_GBK;
            CurFont->MultiDotPtr = k_FontTable[iGBKIndex].FontDotPtr;
            CurFont->MultiFontLibLen = k_FontTable[iGBKIndex].FontLibLen;
		}
        else return -2;
    }
    return 0;
}


//把横向排列的点阵转换为纵向
static void ConvertDot(unsigned char *inDot,unsigned char *outDot, int width, int height)
{
    int i, j, k, s, tsize;
    unsigned char ch, ch2, *tPtr;

    tsize = width * (height / 8);
    if (height % 8)
        tsize += width;

    memset(outDot, 0, tsize);

    tPtr = outDot;
    for (i = 0; i < height; i++)
    {
        if (i % 8 == 0)
        {
            outDot = tPtr;
            ch2 = 0x01;
        }
        else
        {
            ch2 <<= 1;
            tPtr = outDot;
        }
        for (j = 0; j < width; j += 8)
        {
            ch = *inDot++;
            if (j + 8 > width)
                s = width - j;
            else
                s = 8;
            for (k = 0; k < s; k++)
            {
                if (ch & 0x80)
                {
                    *tPtr |= ch2;
                }
                ch <<= 1;
                tPtr++;
            }
        }
    }
}

/*GB2312编码空间
  A1A1 - A9FE   846
  B0A1 - F7FE   6768
                7614
*/	
static int GetGB2312FontDot(unsigned char *str,unsigned char *DotPtr,unsigned char *OutDot, int tsize)
{
    unsigned char ch0, ch1;

    ch0 = str[0];
    ch1 = str[1];

    if (ch0 > 0xA0 && ch0 < 0xAA)
    {
        if (ch1 < 0xA1)
            return 0;           //invalid code
        DotPtr += ((ch0 - 0xA1) * 94L + (ch1 - 0xA1)) * tsize;
    }
    else if (ch0 > 0xAF && ch0 < 0xF8)
    {
        if (ch1 < 0xA1) return 0;
        DotPtr += ((ch0 - 0xB0) * 94L + (ch1 - 0xA1) + 846) * tsize;
    }
    else
        return 0;

    memcpy(OutDot, DotPtr, tsize);
    return 2;                   // code bytes
}

/*
  GBK编码规则:

  码段          未编码段       字数

  8140 - A0FE               190 * 32 = 6080
  A140 - A7FE    40 - A0    94  * 7  = 658
  A840 - A9FE               190 * 2  = 380
  AA40 - AFFE    A1 - FE    96  * 6  = 576
  B040 - F7FE               190 * 72 = 13680
  F840 - FEFE    A1 - FE    96  * 7  = 672
                                总共 = 22046
*/
//str      字符串
//DotPtr   字库字模指针
//OutDot   输出点阵指针
//tsize    输出点阵大小
static int GetGBKFontDot(unsigned char *str,unsigned char *DotPtr,unsigned char *OutDot, int tsize)
{
	int i;
	unsigned char ch0,ch1;

	ch0=str[0];
	ch1=str[1];

	for(i=0;i<6;i++){
		if(ch0>=GBKPage[i].ch && ch0<=GBKPage[i].endch) break;
		DotPtr+=(GBKPage[i].CharNum*tsize);
	}
	if(i>=6) return 0;
	if(GBKPage[i].flag==1){
		if(ch1<0x40) return 0;
		DotPtr+=(190*(ch0-GBKPage[i].ch)+ch1-0x40)*tsize;
		if(ch1>0x7f) DotPtr-=tsize;
	}
	else if(GBKPage[i].flag==2){
		if(ch1<0xA1) return 0;
		DotPtr+=(94*(ch0-GBKPage[i].ch)+ch1-0xA1)*tsize;
	}
	else{
		if(ch1<0x40) return 0;
		DotPtr+=(96*(ch0-GBKPage[i].ch)+ch1-0x40)*tsize;
		if(ch1>0x7f) DotPtr-=tsize;
	}
	memcpy(OutDot,DotPtr,tsize);
	return 2;
}

static int GetBG5FontDot(unsigned char *str,unsigned char *DotPtr,unsigned char *OutDot, int tsize)
{
    //for Big5 font
    //static const uchar BIG5FONT[6]={0xA1,0xF1,0x40,0x7E,0xA1,0xFE};
    int i;
    unsigned char ch0, ch1;

    ch0 = str[0];
    ch1 = str[1];

    if (ch0 < 0xA1 || ch0 > 0xF9)
        return 0;

    if (ch1 < 0xA1)
    {
        if (ch1 < 0x40 || ch1 > 0x7E)
            return 0;
    }
    else
    {
        if (ch1 > 0xFE)
            return 0;

        ch1 = ch1 - 0x22;       //0x22=0xA0-0x7E,中间部分裁剪掉
    }

    DotPtr += (ch1 - 0x40 + (ch0 - 0xA1) * 157) * tsize;
    memcpy(OutDot, DotPtr, tsize);
    return 2;
}

/*日文编码空间
  8140 - 84Be   690
  889F - 9fFC   4418
  e040 - eaa4   1980
                7088
*/	
static int GetShiftJISFontDot(unsigned char *str,unsigned char *DotPtr,unsigned char *OutDot, int tsize)
{
	unsigned char ch0,ch1;

 ch0=str[0];
 ch1=str[1];
 
 if(ch1<0x40) return 0;
 if(ch0>0x80 && ch0<0x85){
  DotPtr+=((ch0-0x81)*188L+(ch1-0x40))*tsize;
 }
 else if(ch0>0x87 && ch0<0xa0){
  if(ch0==0x88){
   DotPtr+=(690+(ch1-0x9e))*tsize;
  }
  else DotPtr+=(784+(ch0-0x89)*188L+(ch1-0x40))*tsize;
 }
 else if(ch0>0xdf && ch0<0xeb){
  DotPtr+=(5108+(ch0-0xe0)*188L+(ch1-0x40))*tsize;
 }
 else return 0;
 if(ch1>0x7F) DotPtr -= tsize;
 memcpy(OutDot,DotPtr,tsize);
 return 2;

//	unsigned char ch0,ch1;
//
//	ch0=str[0];
//	ch1=str[1];
//
//	if(ch1<0x40) return 0;
//	if(ch0>0x80 && ch0<0x85){
//		DotPtr+=((ch0-0x81)*188L+(ch1-0x40))*tsize;
//	}
//	else if(ch0>0x87 && ch0<0x9a){
//		if(ch0==0x88){
//			DotPtr+=(690+(ch1-0x40))*tsize;
//		}
//		else DotPtr+=(785+(ch0-0x89)*188L+(ch1-0x40))*tsize;
//	}
//	else if(ch0>0xdf && ch0<0xeb){
//		DotPtr+=(5108+(ch0-0xe0)*188L+(ch1-0x40))*tsize;
//	}
//	else return 0;
//	memcpy(OutDot,DotPtr,tsize);
//	return 2;
}

static int GetKoreanFontDot(unsigned char *str,unsigned char *DotPtr,unsigned char *OutDot, int tsize)
{
	return 0;
}
/*
	GB18030编码空间:

	码段					字符数

双字节编码
	与GBK编码相同			22046
   
四字节编码
	8139EE39				1
	8139EF30 - 8139FE39		160
	82308130 - 82358738		6369
   
	总共 = 28576
*/
static int GetGB18030FontDot(unsigned char *str, unsigned char *DotPtr,
                             unsigned char *OutDot, int tsize)
{
	unsigned char ch0, ch1, ch2, ch3;

	if(str[1] < 0x40)		//Four byte code
	{
		DotPtr += 22046 * tsize;

		ch0 = str[0];
		ch1 = str[1];
		ch2 = str[2];
		ch3 = str[3];
		
		if (0x81!=ch0 && 0x82!=ch0) return 0;
		if(0x81 == ch0)
		{
			if(ch1 != 0x39)	return 0;	//Invalid code				
			if(0xEE != ch2 || 0x39 != ch3)
			{
				if(ch2 < 0xEF || ch2 > 0xFE || ch3 < 0x30 || ch3 > 0x39) return 0;	//Invalid code
					
				DotPtr += 1 * tsize;			//Add the code "8139EE39" dot size
				DotPtr += ((ch2 - 0xEF) * (0x39 - 0x30 + 1) + (ch3 - 0x30)) * tsize;
			}				
		}
		else if(0x82 == ch0 )
		{
			//Invalid code
			if(ch1 < 0x30 || ch1 > 0x35) return 0;
			if(ch2 < 0x81 || ch2 > 0xFE) return 0;
			if(ch3 < 0x30 || ch3 > 0x39) return 0;
			if((ch1 == 0x35 && ch2 > 0x87) || (ch2 == 0x87 && ch3 == 0x39)) return 0;

			DotPtr += tsize * 161;
			DotPtr += (ch1 - 0x30) * tsize * 0x4ec;//(0xFE - 0x81 + 1) * (0x39 - 0x30 + 1);
			DotPtr += (ch2 - 0x81)  * tsize * 0x0a;//(0x39 - 0x30 + 1);
			DotPtr += (ch3 - 0x30) * tsize;
		}

		memcpy(OutDot, DotPtr, tsize);
		return 4;
	}

	//two byte code
	GetGBKFontDot(str, DotPtr, OutDot, tsize);
	return 2;
}

//return    内码的字节数
static int s_GetFontDot(int LCDFlag, int Reverse, ST_CURRENT_FONT *CurFont, unsigned char *str, unsigned char *dot, int *width, int *height)
{
    int i, CodeByte, tsize, tmpWidth, tmpHeight;
    unsigned char ch, tbuf[(MAX_FONT_WIDTH / 8) * MAX_FONT_HEIGHT];

    ch = *str;
    if (ch >= 0x80 && CurFont->MultiDotPtr != NULL)
    {
        tmpWidth = CurFont->MultiCodeFont.Width;
        tmpHeight = CurFont->MultiCodeFont.Height;
        *width = tmpWidth;
        *height = tmpHeight;
        tsize = tmpHeight * (tmpWidth / 8);

        if (tmpWidth % 8)
            tsize += tmpHeight;

        switch (CurFont->MultiCodeFont.CharSet)
        {
        case CHARSET_GB2312:
            CodeByte = GetGB2312FontDot(str, CurFont->MultiDotPtr, tbuf, tsize);
            break;

        case CHARSET_GBK:
            CodeByte = GetGBKFontDot(str, CurFont->MultiDotPtr, tbuf, tsize);
            break;

        case CHARSET_GB18030:
            CodeByte = GetGB18030FontDot(str, CurFont->MultiDotPtr, tbuf, tsize);
            break;

        case CHARSET_BIG5:
            CodeByte = GetBG5FontDot(str, CurFont->MultiDotPtr, tbuf, tsize);
            break;

        case CHARSET_SHIFT_JIS:
            CodeByte = GetShiftJISFontDot(str, CurFont->MultiDotPtr, tbuf, tsize);
            break;

        case CHARSET_KOREAN:
            CodeByte = GetKoreanFontDot(str, CurFont->MultiDotPtr, tbuf, tsize);
            break;

        default:
            CodeByte = 0;
        }

        if (CodeByte)
        {
            if (LCDFlag)
            {
                ConvertDot(tbuf, dot, tmpWidth, tmpHeight);
                tsize = tmpWidth * ((tmpHeight + 7) / 8);
            }
            else
                memcpy(dot, tbuf, tsize);

            if (Reverse)
            {
                for (i = 0; i < tsize; i++)
                {
                    *dot = ~(*dot);
                    dot++;
                }
            }
            return CodeByte;
        }
    }

    tmpWidth = CurFont->SingleCodeFont.Width;
    tmpHeight = CurFont->SingleCodeFont.Height;

    *width = tmpWidth;
    *height = tmpHeight;

    tsize = tmpHeight * (tmpWidth / 8);
    if (tmpWidth % 8)
        tsize += tmpHeight;

    if (ch < 0x20)
    {
        memset(dot, (unsigned char)0xff, tsize);
    }
    else
    {
        i = (ch - 0x20) * tsize;
        if (i >= CurFont->SingleFontLibLen)
        {
            memset(dot, (unsigned char)0xff, tsize);
            return 1;
        }

        if (LCDFlag)
        {
            ConvertDot(CurFont->SingleDotPtr + i, dot, tmpWidth, tmpHeight);
            tsize = tmpWidth * ((tmpHeight + 7) / 8);
        }
        else
            memcpy(dot, CurFont->SingleDotPtr + i, tsize);

		 if (Reverse)
        {
            for (i = 0; i < tsize; i++)
                dot[i] = ~dot[i];
        }
    }
    return 1;
}

void FontRemapInit(ST_FONT_TABLE *ptFontTab, int num)
{
    int i, j;
	ST_FONT tTmpFont;
    int left,top,width,height;
    int den = 1;
    int nom = 2;

	s_ScrGetLcdArea(&left,&top,&width,&height);
	if (width == WIDTH_FOR_TFT_S800)
	{
		den = 2;
		nom = 5;
	}
	else
	{
		den = 1;
		nom = 2;
	}
	
    for (i = 0; i < sizeof(DIFF_MAP_FONT.font) / sizeof(DIFF_MAP_FONT.font[0]); i++)
    {
        for (j = 0; j < num; j++)
        {
            if (!memcmp (&ptFontTab[j].Font, &DIFF_MAP_FONT.font[i], sizeof(ST_FONT)))
                break;
        }

        if (j == num)
        {
            DIFF_MAP_FONT.matched = 1;
            return;
        }

		memcpy(&tTmpFont,  &DIFF_MAP_FONT, sizeof(ST_FONT));
		if (width == WIDTH_FOR_TFT_S800)
		{
			tTmpFont.Width = DIFF_MAP_FONT.font[i].Width * nom / den;
			tTmpFont.Height = DIFF_MAP_FONT.font[i].Height * nom / den;
			
		}
		else
		{
			tTmpFont.Width = DIFF_MAP_FONT.font[i].Width * nom / den;
			tTmpFont.Width = tTmpFont.Width - tTmpFont.Width / 16;
			tTmpFont.Height = DIFF_MAP_FONT.font[i].Height * tTmpFont.Width / DIFF_MAP_FONT.font[i].Width;
		}
		
        for (j = 0; j < num; j++)
        {
            if (!memcmp(&tTmpFont, &ptFontTab[j].Font, sizeof(ST_FONT)))
                break;
        }

        if (j == num)
        {
            DIFF_MAP_FONT.matched = 0;
            return;
        }
    }

    DIFF_MAP_FONT.matched = 1;
}

uint FontRemap(ST_FONT *pInFont, ST_FONT *pOutFont)
{
	int left,top,width,height;
    int den = 1;
    int nom = 2;
    ST_FONT tTmpFont;
    int i, isGBKExisted=0, isGBKNormExisted=0, isGBKStyleMatched=0,isNormExisted=0;

	s_ScrGetLcdArea(&left,&top,&width,&height);
	if (width == WIDTH_FOR_TFT_S800)
	{
		den = 2;
		nom = 5;
	}
	else
	{
		den = 1;
		nom = 2;
	}
	
    if (DIFF_MAP_FONT.matched == 0)
        return 0;

    memcpy(&tTmpFont, pInFont, sizeof(ST_FONT));
    if (width == WIDTH_FOR_TFT_S800)
    {
        tTmpFont.Width = pInFont->Width * nom / den;
        tTmpFont.Height = pInFont->Height * nom / den;
    }
    else
    {        
		if(pInFont->Width == 12 && pInFont->Height == 12)
		{
			tTmpFont.Width = 24;
			tTmpFont.Height = 24;
		}
		else
		{
			tTmpFont.Width = pInFont->Width * nom / den;
			tTmpFont.Width = tTmpFont.Width - tTmpFont.Width / 16;
			tTmpFont.Height = pInFont->Height * tTmpFont.Width / pInFont->Width;
		}
    }

    for (i = 0; i < k_FontNum; i++)
    {
        if (k_FontTable[i].Font.Width == tTmpFont.Width &&
            k_FontTable[i].Font.Height == tTmpFont.Height)//高宽匹配
        {
			if (k_FontTable[i].Font.CharSet == tTmpFont.CharSet && 
			    k_FontTable[i].Font.Bold == tTmpFont.Bold && 
		  		k_FontTable[i].Font.Italic == tTmpFont.Italic)//全匹配
			{
				memcpy(pOutFont, &tTmpFont, sizeof(ST_FONT));
				return 1;
			}
			
			if (tTmpFont.CharSet == CHARSET_GB2312 && k_FontTable[i].Font.CharSet == CHARSET_GBK)
			{
				isGBKExisted = 1;

				if (k_FontTable[i].Font.Bold==tTmpFont.Bold && k_FontTable[i].Font.Italic==tTmpFont.Italic)
					isGBKStyleMatched = 1;
				else if(k_FontTable[i].Font.Bold==0 && k_FontTable[i].Font.Italic==0)
					isGBKNormExisted = 1;
			}
        
			if (k_FontTable[i].Font.CharSet == tTmpFont.CharSet && 
				k_FontTable[i].Font.Bold == 0 && k_FontTable[i].Font.Italic == 0)
			{
				isNormExisted = 1;
			}
				
		}
    }

	if(isNormExisted)
	{
		tTmpFont.Bold = 0;
		tTmpFont.Italic = 0;
		memcpy(pOutFont, &tTmpFont, sizeof(ST_FONT));
		return 1;
	}

	
	if(isGBKExisted)
	{
		if(isGBKStyleMatched)
		{
			tTmpFont.CharSet = CHARSET_GBK;
			memcpy(pOutFont, &tTmpFont, sizeof(ST_FONT));
			return 1;
		}

		if(isGBKNormExisted)
		{
			tTmpFont.CharSet = CHARSET_GBK;
			tTmpFont.Bold = 0;
			tTmpFont.Italic = 0;
			memcpy(pOutFont, &tTmpFont, sizeof(ST_FONT));
			return 1;
		}
	}
	
    return 0;
}


int TryMapFont(void)
{
	ST_FONT tTmpSingleCodeFont, tTmpMultiCodeFont;

	if(k_LcdFont.MultiDotPtr == NULL) 
	{
		k_sFontMapStatus = FontRemap(&k_LcdFont.SingleCodeFont, &tTmpSingleCodeFont);

		if(k_sFontMapStatus)
		{
			memcpy(&tBkMapSingleFont,&k_LcdFont.SingleCodeFont,sizeof(ST_FONT));
			return s_SelectFont(&k_LcdFont, &tTmpSingleCodeFont, NULL);
		}
	}
	else
	{
		k_sFontMapStatus = FontRemap(&k_LcdFont.SingleCodeFont, &tTmpSingleCodeFont);
		k_mFontMapStatus = FontRemap(&k_LcdFont.MultiCodeFont, &tTmpMultiCodeFont);

		if(k_sFontMapStatus && k_mFontMapStatus)
		{
			memcpy(&tBkMapSingleFont,&(k_LcdFont.SingleCodeFont),sizeof(ST_FONT));
			memcpy(&tBkMapMultiFont,&(k_LcdFont.MultiCodeFont),sizeof(ST_FONT));
			return s_SelectFont(&k_LcdFont, &tTmpSingleCodeFont, &tTmpMultiCodeFont);
		}
	}
}

int TryRestoreFont()
{
	if (k_LcdFont.MultiDotPtr == NULL)
	{
	 	if (k_sFontMapStatus) 
			s_SelectFont(&k_LcdFont, &tBkMapSingleFont, NULL);
	}
	else
	{
		if (k_sFontMapStatus && k_mFontMapStatus)
			s_SelectFont(&k_LcdFont, &tBkMapSingleFont, &tBkMapMultiFont);
	}
}

/* return 1 indicating zoom needed */
int CheckZoom()
{
	if ((k_LcdFont.MultiDotPtr==NULL && k_sFontMapStatus==1) || 
		(k_LcdFont.MultiDotPtr!=NULL && k_sFontMapStatus==1 && k_mFontMapStatus==1)) 
		return 0; 
	else 
		return 1; 
}


/*
对外接口
FontLibPtr 字库在内存中的地址，如果字库是压缩的，该指针指向的数据必须是解压后的数据
           没有字库时为NULL
		   在监控刚启动时，可能需要显示英文字符,此时可把FontLibPtr设置为NULL，
		   字库加载到内存后，再调用一次该函数
*/
void s_FontInit(unsigned char *FontLibPtr)
{
    int i;
    unsigned char *tPtr;
    ST_FONT AscFont;

    k_FontNum = 0;
    k_OldScrFontSize = 0;
    k_OldPrnFontSize = -1;

    if (FontLibPtr != NULL)
    {
        k_FontLibPtr = FontLibPtr;
        k_FontVer = k_FontLibPtr[8];
        k_FontNum = k_FontLibPtr[9];
        memset(&k_LcdFont, 0, sizeof(k_LcdFont));
        memset(&k_PrnFont, 0, sizeof(k_PrnFont));

        if (k_FontNum > MAX_FONT_NUMS || memcmp(k_FontLibPtr, "PAX-FONT", 8))
        {
            k_FontNum = 0;
        }

        if (k_FontNum)
        {
            tPtr = k_FontLibPtr + 16;
            for (i = 0; i < k_FontNum; i++)
            {
                k_FontTable[i].Font.CharSet = tPtr[0];
                k_FontTable[i].Font.Width = tPtr[1];
                k_FontTable[i].Font.Height = tPtr[2];
                k_FontTable[i].Font.Bold = tPtr[3];
                k_FontTable[i].Font.Italic = tPtr[4];
                k_FontTable[i].CodeType = tPtr[5];
                k_FontTable[i].FontDotPtr = FontLibPtr;
                k_FontTable[i].FontDotPtr += StrToLong(tPtr + 8);
                k_FontTable[i].FontLibLen = StrToLong(tPtr + 12);
                tPtr += 16;
            }
        }
    }

    if (!k_FontNum)//如果无字库则启用默认的仅英文字库(6*8和8*16两个)
    {
        memset(&k_LcdFont, 0, sizeof(k_LcdFont));
        memset(&k_PrnFont, 0, sizeof(k_PrnFont));

        k_FontVer = 0;
        k_FontNum = 2;
        AscFont.CharSet = CHARSET_WEST;
        AscFont.Width = 6;
        AscFont.Height = 8;
        AscFont.Bold = 0;
        AscFont.Italic = 0;
        memcpy(&k_FontTable[0].Font, &AscFont, sizeof(ST_FONT));
        AscFont.Width = 8;
        AscFont.Height = 16;
        memcpy(&k_FontTable[1].Font, &AscFont, sizeof(ST_FONT));
        k_FontTable[0].CodeType = 0;
        k_FontTable[1].CodeType = 0;
        k_FontTable[0].FontLibLen = 96 * 8;
        k_FontTable[1].FontLibLen = 96 * 16;
        k_FontTable[0].FontDotPtr = (unsigned char *)DefASCFont6X8;
        k_FontTable[1].FontDotPtr = (unsigned char *)DefASCFont8X16;
    }

    //Search for LCD single code font
    SetDefaultFont(&k_LcdFont, DefLcdSingleCodeFont, 1);//设置最初始的屏幕单字库
    //Search for LCD Multi code font
    SetDefaultFont(&k_LcdFont, DefLcdMultiCodeFont, 0);//设置最初始的屏幕双字库
    //Search for printer single code font
    SetDefaultFont(&k_PrnFont, DefPrnSingleCodeFont, 1);
    //Search for printer Multi code font
    SetDefaultFont(&k_PrnFont, DefPrnMultiCodeFont, 0);

	FontRemapInit(k_FontTable, k_FontNum);//初始化重映射(兼容老屏)
    SelScrFont(&k_LcdFont.SingleCodeFont,&k_LcdFont.MultiCodeFont);
}


int EnumFont(ST_FONT *Fonts, int MaxFontNums)
{
	int i,num;

	if (Fonts == NULL)
	{
		return 0;
	}
	if(MaxFontNums<k_FontNum) num=MaxFontNums;
	else num=k_FontNum;
	for(i=0;i<num;i++){
		memcpy(&Fonts[i],&k_FontTable[i].Font,sizeof(ST_FONT));
	}
	return num;
}

void s_GetCurScrFont(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont)
{
	memcpy(SingleCodeFont,&k_LcdFont.SingleCodeFont,sizeof(ST_FONT));
	memcpy(MultiCodeFont,&k_LcdFont.MultiCodeFont,sizeof(ST_FONT));
}

int SelScrFont(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont)
{
    return s_SelectFont(&k_LcdFont, SingleCodeFont, MultiCodeFont);
}

void  s_GetMatrixDot(uchar *str,uchar *MatrixDot,ushort *len, uchar flag)
{
	int width,height;

	ScrFontSet(flag);
	s_GetFontDot(1, 0, &k_LcdFont, str, MatrixDot, &width, &height);

	if(width == 16)
	{
		* len = 32;
	}
	else if (height == 16)
	{
		* len = 16;	
	}
	else
		*len = 8;
}

int s_GetLcdFontDot(int Reverse, unsigned char *str, unsigned char *dot, int *width, int *height)
{
	return s_GetFontDot(1, Reverse, &k_LcdFont, str, dot, width, height);
}

//Printer
int SelPrnFont(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont)
{
    int ret;

    ret = s_SelectFont(&k_PrnFont,SingleCodeFont,MultiCodeFont);
    
    if(get_machine_type()==S78 && ret==0)
		ret=SelPrnFont1111_stylus(SingleCodeFont,MultiCodeFont);
    
	return ret; 
}

int s_GetPrnFontDot(int Reverse, int SingleDoubleWidth, int MultiDoubleWidth,unsigned char *str, unsigned char *dot, int *width, int *height)
{
	int i,j,LineBytes,ret,twidth,theight,tsize;
	register unsigned char ch,ch1,*tPtr,*tPtr1;
	unsigned char buff[MAX_FONT_WIDTH/8*MAX_FONT_HEIGHT];
	unsigned char buff1[MAX_FONT_WIDTH/2*MAX_FONT_HEIGHT];

	ret=s_GetFontDot(0, Reverse, &k_PrnFont, str, buff, &twidth, &theight);
	*height=theight;
	tsize=(twidth/8)*theight;
	if(twidth%8) tsize+=theight;
	if(ret==1 && !SingleDoubleWidth || ret>=2 && !MultiDoubleWidth)
	{
		memcpy(dot,buff,tsize);
		*width=twidth;
		return ret;
	}

	*width=twidth*2;
	LineBytes=twidth/8;
	twidth=twidth%8;
	if(twidth) LineBytes++;

	tPtr=buff;
	tPtr1=buff1;

	for(i=0; i<theight; i++)
	{
		for(j=0; j<LineBytes; j++)
		{
			ch  = *tPtr++;
			ch1 = 0;
			if(ch & 0x080) ch1 |= 0xc0;
			if(ch & 0x040) ch1 |= 0x30;
			if(ch & 0x020) ch1 |= 0x0c;
			if(ch & 0x010) ch1 |= 0x03;
			*tPtr1++ = ch1;
			ch1 = 0;
			if(ch & 0x08) ch1 |= 0xc0;
			if(ch & 0x04) ch1 |= 0x30;
			if(ch & 0x02) ch1 |= 0x0c;
			if(ch & 0x01) ch1 |= 0x03;
			*tPtr1++ = ch1;
		}
		if(twidth>0 && twidth<=4) tPtr1--;
	}
	memcpy(dot,buff1,tPtr1-buff1);
	return ret;
}

unsigned char s_GetFontVer(void)
{
	return (unsigned char)k_FontVer;
}

/**获取当前显示单个字符占用象素宽度和高度信息*/
int s_GetLcdFontCharAttr(int *piSingleCodeWidth, int *piSingleCodeHeight,
								int *piMultiCodeWidth, int *piMultiCodeHeight)
{
    if (piSingleCodeWidth != NULL)
        *piSingleCodeWidth = k_LcdFont.SingleCodeFont.Width;

    if (piSingleCodeHeight != NULL)
        *piSingleCodeHeight = k_LcdFont.SingleCodeFont.Height;

    if (piMultiCodeWidth != NULL)
        *piMultiCodeWidth = k_LcdFont.MultiCodeFont.Width;

    if (piMultiCodeHeight != NULL)
        *piMultiCodeHeight = k_LcdFont.MultiCodeFont.Height;

    return 0;
}

//For compatible
unsigned char  ScrFontSet(unsigned char fontsize)
{
    int i;
    int iRet;
    unsigned char tmp;
    ST_FONT sfont, mfont;
    ST_FONT *psfont, *pmfont;

    tmp = k_OldScrFontSize;

    sfont.Bold = 0;
    sfont.Italic = 0;
    sfont.CharSet = -1;

    if (fontsize)
    {
        sfont.Width = 8;
        sfont.Height = 16;
    }
    else
    {
        sfont.Width = 6;
        sfont.Height = 8;
    }

    mfont.Bold = 0;
    mfont.Italic = 0;
    mfont.CharSet = -1;
    mfont.Width = 16;
    mfont.Height = 16;

    psfont = NULL;
    pmfont = NULL;

    if (k_LcdFont.SingleCodeFont.Width == sfont.Width
        && k_LcdFont.SingleCodeFont.Height == sfont.Height)
    {
        memcpy(&sfont, &k_LcdFont.SingleCodeFont, sizeof(ST_FONT));
        psfont = &sfont;
    }

    if (k_LcdFont.MultiCodeFont.Width == mfont.Width
        && k_LcdFont.MultiCodeFont.Height == mfont.Height)
    {
        memcpy(&mfont, &k_LcdFont.MultiCodeFont, sizeof(ST_FONT));
        pmfont = &mfont;
    }

    for (i = 0; i < k_FontNum; i++)
    {
        if (psfont == NULL
            && !k_FontTable[i].CodeType
            && k_FontTable[i].Font.Width == sfont.Width
            && k_FontTable[i].Font.Height == sfont.Height)
        {
            sfont.CharSet = k_FontTable[i].Font.CharSet;
            psfont = &sfont;
        }

        if (pmfont == NULL
            && k_FontTable[i].CodeType
            && k_FontTable[i].Font.Width == mfont.Width
            && k_FontTable[i].Font.Height == mfont.Height)
        {
            mfont.CharSet = k_FontTable[i].Font.CharSet;
            pmfont = &mfont;
        }
    }

    iRet = SelScrFont(psfont, pmfont);
    k_OldScrFontSize = fontsize;
    return tmp;
}


//For compatible
int PrnGetFontDot(int FontSize, unsigned char *str, unsigned char *OutDot)
{
	int width, height,ret;
	ST_CURRENT_FONT FontBak;
	if (str == NULL || OutDot == NULL)return -2;
	if (FontSize != 0 && FontSize != 1) return -3;

	if(s_SearchFile(FONT_NAME_S80, (uchar *)"\xff\x02") < 0) return -1;
	FontBak = k_PrnFont;
	s_SetPrnFont((unsigned char)FontSize,(unsigned char)FontSize);
	s_GetFontDot(0, 0, &k_PrnFont, str, OutDot, &width, &height);
	k_PrnFont = FontBak;
	return 0;
}

void  s_SetPrnFont(unsigned char Ascii, unsigned char Cfont)
{
	int i,width16Flag;
	ST_FONT sfont, mfont;

	for(i=0;i<k_FontNum;i++){
		if(!k_FontTable[i].CodeType) break;
	}
	if(i<k_FontNum) sfont.CharSet=k_FontTable[i].Font.CharSet;
	else sfont.CharSet=CHARSET_WEST;

	width16Flag=0;
	for(i=0;i<k_FontNum;i++){
		if(!k_FontTable[i].CodeType){
			if(k_FontTable[i].Font.Width==16 && sfont.CharSet==k_FontTable[i].Font.CharSet){
				width16Flag=1;
				break;
			}
		}
	}

	for(i=0;i<k_FontNum;i++){
		if(k_FontTable[i].CodeType) break;
	}
	if(i<k_FontNum) mfont.CharSet=k_FontTable[i].Font.CharSet;
	else mfont.CharSet=CHARSET_GB2312;

	sfont.Bold=0;
	mfont.Bold=0;
	sfont.Italic=0;
	mfont.Italic=0;
	if(Ascii){
		if(width16Flag) sfont.Width=16;
		else sfont.Width=12;
		sfont.Height=24;
	}
	else{
		sfont.Width=8;
		sfont.Height=16;
	}
	if(Cfont){
		mfont.Width=24;
		mfont.Height=24;
	}
	else{
		mfont.Width=16;
		mfont.Height=16;
	}

	SelPrnFont(&sfont,&mfont);
}

//是否中文字库 0-非中文字库 其他值-中文字库 
extern uchar            bIsChnFont;
//字库文件长度
static ulong			ulFontFileLen=0;
extern uint              errno;

void SaveCurFont(void)
{
	s_GetCurScrFont(&SelectedSingleCodeFont,&SelectedMultiCodeFont);
	SaveCurFontFlag = 1;
}

int RestoreCurFont(void)
{
	if(SaveCurFontFlag)
	{
		SaveCurFontFlag = 0;
		SelScrFont(&SelectedSingleCodeFont,&SelectedMultiCodeFont);
		return 0;
	}
	SaveCurFontFlag = 0;
	return -1;
}

//  载入字库文件
void LoadFontLib(void)
{
	int     i, Len;
	ST_FONT sfont,mfont;
	ScrFontSet(CFONT);

	bIsChnFont = 0;

	/* 读取字库描述信息，并将字库从FLASH中复制到SDRAM中   */
	i = s_open(FONT_NAME_S80, O_RDWR, (uchar *)"\xff\x02");

	if(i >= 0)
	{	
		Len = GetFileLen(i);
		ulFontFileLen = Len;
		(void)read(i, (uchar *)FONTLIB_ADDR, Len);
		close(i);
		s_FontInit((uchar *)FONTLIB_ADDR);
		s_GetCurScrFont(&sfont,&mfont);

		if(mfont.CharSet==CHARSET_GB2312 || mfont.CharSet==CHARSET_GBK || mfont.CharSet==CHARSET_GB18030)
		{
			bIsChnFont = 1;
		}
		else
		{
			bIsChnFont = 0;
			//判断是否存在west charset
		}
	}
	else
	{
		s_FontInit(NULL);
		ulFontFileLen = 0;
	}
}

int is_chn_font(void)
{
	return bIsChnFont;
}

//For compatible
int ReadFontLib(unsigned long Offset,unsigned char *FontData, int ReadLen)
{
	if(k_FontLibPtr==NULL || FontData == NULL)
	{
		return -1;
	}
	if (ulFontFileLen == 0)
	{
		return -1;
	}
	if (ReadLen <= 0)
	{
		return -1;
	}
	
	if ((Offset > ulFontFileLen) || (Offset + ReadLen > ulFontFileLen))
	{
		return -1;
	}

	memcpy(FontData,k_FontLibPtr+Offset,ReadLen);
	return 0;
}

char * GetCharSetName(int iCharSet)
{
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

	switch(iCharSet) {
	case CHARSET_WEST:
		return "WEST";
	case CHARSET_TAI:
		return "WEST";
	case CHARSET_MID_EUROPE:
		return "MID EUROPE";
	case CHARSET_VIETNAM:
		return "VIETNAM";
	case CHARSET_GREEK:
		return "GREEK";
	case CHARSET_BALTIC:
		return "BALTIC";
	case CHARSET_TURKEY:
		return "TURKEY";	
	case CHARSET_HEBREW:
		return "HEBREW";	
	case CHARSET_RUSSIAN:
		return "RUSSIAN";	
	case CHARSET_GB2312:
		return "GB2312";	
	case CHARSET_GBK:
		return "GBK";
	case CHARSET_GB18030:
		return "GB18030";
	case CHARSET_BIG5:
		return "BIG5";	
	case CHARSET_KOREAN:
		return "KOREAN";
	case CHARSET_ARABIA:
		return "ARABIA";
	case CHARSET_DIY:
		return "DIY";
	default:
		return "UNKNOW CHARSET";
	}
}

void PrnFontInfo()//打印字库信息，必须在前面初始化打印机，在调用后调用打印
{
	int iCnt;
	PrnStr("VERSION:%d\n",k_FontVer);
	PrnStr("NUMBER:%d\n",k_FontNum);

	for (iCnt = 0; iCnt < k_FontNum; iCnt++)
	{
		PrnStr("%d-%s [%d*%d]\n",iCnt + 1,GetCharSetName(k_FontTable[iCnt].Font.CharSet),k_FontTable[iCnt].Font.Height,k_FontTable[iCnt].Font.Width);
	}
}

//针打字库
static ST_CURRENT_FONT 	k_SPrnFont; /* current font for stylus printer */
int s_SelPrnFont(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont)
{
	return s_SelectFont(&k_PrnFont,SingleCodeFont,MultiCodeFont);
}

int SelSPrnFont(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont)
{
	return s_SelectFont(&k_SPrnFont,SingleCodeFont,MultiCodeFont);
}

int GetLcdFontDot(unsigned char *str, unsigned char *dot, int *width, int *height)
{
	return s_GetFontDot(1, 0, &k_LcdFont, str, dot, width, height);
}

int s_GetSPrnFontDot(int Reverse, unsigned char *str, unsigned char *dot, int *width, int *height)
{
	return s_GetFontDot(1, Reverse, &k_SPrnFont, str, dot, width, height);
}

void  s_SetSPrnFont(unsigned char Ascii, unsigned char Cfont)
{
	int i;
	ST_FONT sfont, mfont;
	for(i=0;i<k_FontNum;i++){
		if(!k_FontTable[i].CodeType) break;
	}
	if(i<k_FontNum) sfont.CharSet=k_FontTable[i].Font.CharSet;
	else sfont.CharSet=CHARSET_WEST;

	for(i=0;i<k_FontNum;i++){
		if(!k_FontTable[i].CodeType){
			if(k_FontTable[i].Font.Width==16 && sfont.CharSet==k_FontTable[i].Font.CharSet){
				break;
			}
		}
	}

	for(i=0;i<k_FontNum;i++){
		if(k_FontTable[i].CodeType) break;
	}
	if(i<k_FontNum) mfont.CharSet=k_FontTable[i].Font.CharSet;
	else mfont.CharSet=CHARSET_GB2312;

	sfont.Bold=0;
	mfont.Bold=0;
	sfont.Italic=0;
	mfont.Italic=0;
	switch(Ascii)
	{
	case 0:sfont.Width=6; sfont.Height=8;break;
	case 1:sfont.Width=8; sfont.Height=16;break;
	case 2:sfont.Width=6; sfont.Height=12;break;
	case 3:sfont.Width=7; sfont.Height=14;break;
	default:
		break;
	}
	switch(Cfont)
	{
	case 0:
	case 1:mfont.Width=16;mfont.Height=16;break;
	case 2:mfont.Width=12;mfont.Height=12;break;
	case 3:mfont.Width=14;mfont.Height=14;break;
	default:	
		break;
	}

	SelSPrnFont(&sfont,&mfont);
}


