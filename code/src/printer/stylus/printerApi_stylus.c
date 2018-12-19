
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "base.h"
#include "printer_stylus.h"
#include "..\..\font\font.h" 

#define PRN_OK                  0x00
#define PRN_BUSY				0x01
#define PRN_PAPEROUT			0x02
#define PRN_WRONG_PACKAGE		0x03
#define PRN_FAULT				0x04
#define PRN_TOOHEAT				0x08
#define PRN_UNFINISHED			0xf0
#define PRN_NOFONTLIB			0xfc
#define PRN_OUTOFMEMORY			0xfe

#define MAX_PACK_BUFF   32*1024

#define PRN_END         0x00
#define PRN_LF          0x0a
#define PRN_FF          0x0c
#define PRN_STEP        0x19
#define PRN_SPACE_SET   0x20
#define PRN_FONT_SET    0x57
#define PRN_LEFT_INDENT 0x6c
#define PRN_STR         0x43
#define PRN_LOGO        0x59
#define PRN_FONT_SEL 	0x55

unsigned char k_PrnPackBuff[MAX_PACK_BUFF+10];
unsigned char *k_PrnPackPtr,*k_PackEnd;
static unsigned char k_PrnStrLen;
static char k_PrnStrBuff[256];
static int k_OverFlow;

extern void ps_PrnSmallFontInit(void);

void s_PrnInit_stylus(void)
{
    k_OverFlow   = 0;
    k_PrnPackPtr = k_PrnPackBuff;
    k_PackEnd    = k_PrnPackBuff + MAX_PACK_BUFF;
    ps_PrnInit();
}


uchar PrnInit_stylus(void)
{
    k_PrnPackPtr = k_PrnPackBuff;
    k_OverFlow   = 0;
    ps_PrnSmallFontInit();
    ps_PrnInitStatus();
	s_SetSPrnFont(0,1);
    return 0;
}

void PrnFontSet_stylus(unsigned char Ascii, unsigned char Cfont)
{
    if ( (Ascii >= 4) || (Cfont >= 4) ) return;
    if(k_PrnPackPtr+3 > k_PackEnd)
    {
        k_OverFlow = 1;
        return;
    }
    *k_PrnPackPtr++ = PRN_FONT_SET;
    *k_PrnPackPtr++ = Ascii;
    *k_PrnPackPtr++ = Cfont;
}

void PrnSpaceSet_stylus(unsigned char x, unsigned char y)
{
    if(k_PrnPackPtr+3 > k_PackEnd)
    {
         k_OverFlow = 1;
         return;
    }
    *k_PrnPackPtr++ = PRN_SPACE_SET;
    *k_PrnPackPtr++ = x;
    *k_PrnPackPtr++ = y;
}

int SelPrnFont1111_stylus(ST_FONT *SingleCodeFont, ST_FONT *MultiCodeFont)
{
	int rsl;


    if(k_PrnPackPtr+2*sizeof(SingleCodeFont) > k_PackEnd)
    {
        k_OverFlow = 1;
        return -3;
    }
    *k_PrnPackPtr++ = PRN_FONT_SEL;
    if(SingleCodeFont)	memcpy(k_PrnPackPtr,SingleCodeFont,sizeof(ST_FONT));
    else
    {
    	memset(k_PrnPackPtr,0,sizeof(ST_FONT));
    }
	k_PrnPackPtr+=sizeof(ST_FONT);
	if(MultiCodeFont) 	memcpy(k_PrnPackPtr,MultiCodeFont,sizeof(ST_FONT));
	else				memset(k_PrnPackPtr,0,sizeof(ST_FONT));

	k_PrnPackPtr+=sizeof(ST_FONT);
	return 0;
 	
}


void PrnStep_stylus(short pixel)
{
    if(!pixel) return;

    if(k_PrnPackPtr+3 > k_PackEnd)
    {
         k_OverFlow = 1;
         return;
    }
    
    if(pixel>255 || pixel<-255) return;

    *k_PrnPackPtr++ = PRN_STEP;
    *k_PrnPackPtr++ = (unsigned char)(pixel>>8);
    *k_PrnPackPtr++ = (unsigned char)pixel;
}

void PrnLeftIndent_stylus(ushort num)
{
    if(k_PrnPackPtr+2 > k_PackEnd)
    {
         k_OverFlow = 1;
         return;
    }
    
    if(num>240) num=240;
    *k_PrnPackPtr++ = PRN_LEFT_INDENT;
    *k_PrnPackPtr++ = num & 0xff;
}

static int AddStrToPack(void)
{
    if(!k_PrnStrLen) return 0;
    if(k_PrnPackPtr+k_PrnStrLen+2 > k_PackEnd)
    {
        k_OverFlow = 1;
        return -1;
    }

    *k_PrnPackPtr++ = PRN_STR;
    *k_PrnPackPtr++ = k_PrnStrLen;
    memcpy(k_PrnPackPtr, k_PrnStrBuff, k_PrnStrLen);
    k_PrnPackPtr += k_PrnStrLen;
    k_PrnStrLen   = 0;
    return 0;
}
static uchar PrnStr0(char *buff)
{
    unsigned short len,i;
    len = strlen(buff);
    
    if(!len) return 0;
    if(k_PrnPackPtr+len+2 > k_PackEnd)
    {
        k_OverFlow = 1;
        return 0xfe;
    }
	
    k_PrnStrLen = 0;
    for(i=0; i<len; i++)
    {
        if(buff[i] == '\n')
        {
            if(AddStrToPack())
            {
               k_OverFlow = 1;
               return 0xfe;
            }

            if(k_PrnPackPtr+2 > k_PackEnd)
            {
               k_OverFlow = 1;
               return 0xfe;
            }

            *k_PrnPackPtr++ = PRN_LF;
        }
        else if(buff[i]=='\f')
        {
            if(AddStrToPack())
            {
               k_OverFlow = 1;
               return 0xfe;
            }

            if(k_PrnPackPtr+2 > k_PackEnd)
            {
               k_OverFlow = 1;
               return 0xfe;
            }

            *k_PrnPackPtr++ = PRN_FF;
        }
        else
        {
            k_PrnStrBuff[k_PrnStrLen++] = buff[i];

            if(k_PrnStrLen == 255)
            {
                if(AddStrToPack())
                {
                    k_OverFlow = 1;
                    return 0xfe;
                }
            }
        }
    }

    if(AddStrToPack())
    {
         k_OverFlow = 1;
         return 0xfe;
    }

    return 0;
}

uchar s_PrnStr_stylus(uchar *s)
{
	if(k_OverFlow) return 0xfe;

	if(strlen(s)>2048)
	{
	 	k_OverFlow=1;
	    return 0xfe;
	}

	return PrnStr0(s);
}
uchar PrnStr_stylus(char *fmt,...)
{
    va_list marker;
    char    buff[2050];
    int     cnt;
    buff[0] = 0;
    va_start(marker, fmt);
    cnt = vsnprintf(buff, sizeof(buff)-2, fmt, marker);
	if(cnt>= sizeof(buff)-2) 
	{
	    k_OverFlow=1;
    	   return 0xfe;
	}
    va_end(marker);

	return PrnStr0(buff);
}


uchar PrnLogo_stylus(uchar *logo)
{
    int i,j,len,maxlen;

    if(!logo[0]) return 0;
    maxlen = k_PackEnd - k_PrnPackPtr;
    for(i=0,len=2; i<logo[0]; i++)
    {
        len += ((logo[len-1]<<8) + logo[len] + 1);
        len++;
        if(len > maxlen)
        {
            k_OverFlow = 1;
            return 0xfe;
        }
    }
    len--;
    *k_PrnPackPtr++ = PRN_LOGO;
    memcpy(k_PrnPackPtr, logo, len);
    k_PrnPackPtr += len;
	return 0;
}

extern volatile ushort k_PrinterBusy;
uchar PrnStart_stylus(void)
{
    unsigned char status;

	if(k_OverFlow) return  0xfe;

	ScrSetIcon(3, 1);	/* set printer icon */
	
    status = 0;
    if(k_PrnPackPtr!=k_PrnPackBuff)
    {
        *k_PrnPackPtr++ = PRN_END;
        ps_ProcData(k_PrnPackBuff, (int)(k_PrnPackPtr-k_PrnPackBuff), 1);
        ps_GetPrnResult(&status);
		k_PrnPackPtr--; 
    }
    
    if((!status)&& k_OverFlow)
    {
		ScrSetIcon(3, 0);	/* close printer icon */
	    return 0xfe;
    }
    while(k_PrinterBusy != 0);
    ScrSetIcon(3, 0);	/* close printer icon */

    return status;
}

unsigned char s_PrnStart_stylus(unsigned char mode)
{
    unsigned char status;

    status = 0;
    if(k_PrnPackPtr != k_PrnPackBuff)
    {
         *k_PrnPackPtr++ = PRN_END;
         if(mode)
            ps_ProcData(k_PrnPackBuff, (int)(k_PrnPackPtr-k_PrnPackBuff), 1);
         else
            ps_ProcData(k_PrnPackBuff, (int)(k_PrnPackPtr-k_PrnPackBuff), 0);
         ps_GetPrnResult(&status);
		k_PrnPackPtr--;
    }
    
    if((!status)&& k_OverFlow)return 0xfe;

    return status;
}

uchar PrnStatus_stylus(void)
{
    unsigned char status;

    if(k_OverFlow) return 0xfe;

    status=ps_GetStatus();
    if(!status)
	{
        if(k_OverFlow) return 0xfe;
		if (s_GetFontVer() == 0)return PRN_NOFONTLIB;
    }
    
    return status;
}

