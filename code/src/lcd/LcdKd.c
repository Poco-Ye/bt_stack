/************************************************************

20050119:
1）禁止GETSTRING中的STR为空指针

20050308:
1)根据新的PCI内存分配，修改了对比度值备份操作，从一字节扩展到4字节。
2)禁止通过画框函数显示出PIN字样

 ***********************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "posapi.h"
#include "base.h"
#include "../lcd/LcdDrv.h"

#define  ASCII                  0x00
#define  CFONT                  0x01
#define  MAX_KEYTABLE           10

const uchar k_KeyTable[10][5]={"0,*#","1QZ.","2ABC","3DEF","4GHI","5JKL","6MNO","7PRS","8TUV","9WXY"};
const uchar k_KeyTable_lower[10][5]={"0,*#","1qz.","2abc","3def","4ghi","5jkl","6mno","7prs","8tuv","9wxy"};

const uchar k_KeySymbol[40]="0\\*, #:;+-=?$&%!~@^()|/_[]{}<>`\'\"";


//extern ulong  k_PEDINPUTTIMEOUT;
ulong  k_PEDINPUTTIMEOUT = 120;//saint add it

extern void s_TimerSet(uchar TimerNo, ulong count);
extern ulong s_TimerCheck(uchar TimerNo);



//////////////////////////////////////////////////////////////
/*            GetString Code                                */
//////////////////////////////////////////////////////////////
void s_Kb_DataStrConvAmt(uchar *disp_buf, uchar *amt_buf, uchar len)
{
    char i;
    uchar new_len, bCount;
    uchar *ptmp;

    ptmp = amt_buf;
    for (i = 0; i < len; i++)
    {
        if (ptmp[i] != '0')
            break;
    }

    ptmp += i;
    new_len = len - (uchar) (ptmp - amt_buf);   /* this is 'i' value */
    amt_buf = ptmp;
    disp_buf[2] = '.';
    disp_buf[5] = 0x0;

    switch (new_len)
    {
    case 0:
        disp_buf[1] = '0';
        disp_buf[3] = '0';
        disp_buf[4] = '0';
        break;
    case 1:
        disp_buf[1] = '0';
        disp_buf[3] = '0';
        disp_buf[4] = *amt_buf;
        break;
    case 2:
        disp_buf[1] = '0';
        disp_buf[3] = *amt_buf;
        disp_buf[4] = *(amt_buf + 1);
        break;
    default:
        bCount = 0;
        for (i = 0; i < (new_len - 2); i++)
        {
            disp_buf[i + 1] = *(amt_buf + i);
        }
        disp_buf[new_len - 1] = '.';
        disp_buf[new_len] = *(amt_buf + new_len - 2);
        disp_buf[new_len + 1] = *(amt_buf + new_len - 1);
        disp_buf[new_len + 1 + 1] = '\0';

        break;
    }
    disp_buf[0] = (new_len < 3) ? (4) : (new_len + 1 + bCount);
    disp_buf[disp_buf[0] + 1] = 0x0;
}


void s_Kb_DispString(uchar flag, uchar new_mode, uchar *inp_buf,
                   uchar len, uchar fontsize, uchar fFlushLeft)
{
    uchar x, y;
    uchar max_len, bOffset, k_CurFontAttr;
    uchar disp_buf[130];
    int   pixel_X,pixel_Y;
	int LcdWidth, LcdHeight;
	
	ScrGetLcdSize(&LcdWidth, &LcdHeight);
	
	ScrGetxyEx(&pixel_X,&pixel_Y);
    x =pixel_X;
    y =pixel_Y;
    if (new_mode == 3)
        s_Kb_DataStrConvAmt(disp_buf, inp_buf, len);
    else
    {
        disp_buf[0] =len;
        if (new_mode == 2)
            memset(&disp_buf[1], '*', len);
        else
            memcpy(&disp_buf[1], inp_buf, len);
        disp_buf[1+len] =0;
    }

    bOffset =0;
    max_len =(LcdWidth-x)/fontsize;
    if (max_len == 0)
        return;

    k_CurFontAttr = GetScrCurAttr();

    if (fFlushLeft)
    {
        if (max_len != 1)
        {
            if (disp_buf[0] >= max_len)
            {
                bOffset =disp_buf[0] - max_len + 1;
                Lcdprintf("%s",(char *)&disp_buf[bOffset+1]);
                ScrAttrSet(0);
                Lcdprintf("_");
                ScrAttrSet(k_CurFontAttr);
            }
            else
            {
                Lcdprintf("%s",(char *)&disp_buf[bOffset+1]);
                ScrAttrSet(0);
                Lcdprintf("_");
                if (flag && (disp_buf[0] != max_len-1))
                    Lcdprintf(" ");
                ScrAttrSet(k_CurFontAttr);
            }
        }
    }
    else
    {
        if (disp_buf[0] >= max_len)
            bOffset =max_len;
        else
        {
            bOffset =disp_buf[0];
            if (flag)
            {
                ScrGotoxyEx(LcdWidth-(bOffset+1)*fontsize, y);
                ScrAttrSet(0);
                Lcdprintf(" ");
                ScrAttrSet(k_CurFontAttr);
            }
        }
        ScrGotoxyEx(LcdWidth-bOffset*fontsize, y);
        Lcdprintf("%s",(char *)&disp_buf[disp_buf[0]-bOffset+1]);
    }
}




/************************************************************/
/*  mode:                                                   */
/*              bit7: 1[0] 能[否]回车退出                   */
/*              bit6: 1[0] 大[小]字体                       */
/*              bit5: 1[0] 能[否]输数字                     */
/*              bit4: 1[0] 能[否]输字符                     */
/*              bit3: 1[0] 是[否]密码方式                   */
/*              bit2: 1[0] 左[右]对齐输入                   */
/*              bit1: 1[0] 有[否]小数点                     */
/*              bit0: 1[0] 正[反]显示                       */
/*                                                          */
/*  new_mode:                                               */
/*              0    --- input numeric  only                */
/*              1    --- input character                    */
/*              2    --- input password                     */
/*              3    --- input amount[radix point]          */
/************************************************************/
/*  2 返回值增加 0x0d， 还没输入时输入回车。
*/

uchar GetString(uchar *str, uchar mode, uchar minlen, uchar maxlen)
{
	uchar i,j,keystatu,prekey,key,line,len,fonttype,fontsize;
	uchar new_mode, old_fonttype, old_fontattr;
	uchar fFlushLeft, fReverseEcho, fEnterReturn;
	uchar sBuf[260],temp[2];
	uchar LowerAlpha;
	uchar   DispReg;
	int     iRet,pixel_X,pixel_Y,x,y;
	int LcdWidth, LcdHeight;
	T_SOFTTIMER lcdkd;
	
	
	ScrGetLcdSize(&LcdWidth, &LcdHeight);

    LowerAlpha=0;

	if(minlen > maxlen || maxlen > 128)
		return 0xfe;

	if(maxlen == 0)
		return 0xfe;

	if(str == NULL)
		return 0xfe;

    memset(sBuf, 0, sizeof(sBuf));
    temp[1] = 0;

	ScrGetxyEx(&pixel_X,&pixel_Y);
    x =pixel_X;
    y =pixel_Y;
    keystatu =0;
    line =0;
    i =0;

	if ((mode & 0x38) == 0)
	{
		return 0xfe;
	}

	if (mode & 0x40)
	{
		fonttype = CFONT;
		fontsize = 8;
	}
	else
	{
		fonttype = ASCII;
		fontsize = 6;
	}

	if (mode & 0x20)
		new_mode = 0;

	if (mode & 0x10)
		new_mode = 1;

	if (mode & 0x08)
		new_mode = 2;

	if ((new_mode == 0) && (mode & 0x02))
		new_mode = 3;

	fEnterReturn = 0;

	if (mode & 0x80)
		fEnterReturn = 1;

	fFlushLeft = 0;

	if (mode & 0x04)
		fFlushLeft = 1;

	fReverseEcho = 1;

	if (mode & 0x01)
		fReverseEcho = 0;

    prekey =0;
    len =0;
    sBuf[0] =0;
    sBuf[1] =NOKEY;

    if ((new_mode==0 || new_mode==1)&&fEnterReturn)  // 数字, 字符
    {
        len =(strlen((char*)str)>maxlen) ? maxlen : strlen((char *)str);
        if (len && fEnterReturn)
        {
            sBuf[0] =len;
            memcpy(&sBuf[1], str, len);
            sBuf[1+len] =0;

            keystatu = 0;
            prekey =0;
			if ((sBuf[len] >= '0') && (sBuf[len] <= '9'))
			{
				prekey = sBuf[len];
			}
            else if ((sBuf[len]>='A' && sBuf[len]<='Z') || sBuf[len]=='.')
            {
                 for (i=1; i<MAX_KEYTABLE; i++)
                 {
                     for (j=0; j<4; j++)
                     {
                         if (sBuf[len]==k_KeyTable[i][j])
                         {
                             prekey = k_KeyTable[i][0];
                             keystatu = j;
                              break;
                         }
                     }
                }
            }
            else if ((sBuf[len]>='a' && sBuf[len]<='z'))
            {
                for (i=1; i<MAX_KEYTABLE; i++)
                {
                    for (j=0; j<4; j++)
                    {
                        if (sBuf[len]==k_KeyTable_lower[i][j])
                        {
                            prekey = k_KeyTable_lower[i][0];
                            keystatu = j;
                            break;
                        }
                    }
                }
            }
            else
            {
                for (j=0; j<strlen((char *)k_KeySymbol); j++)
                {
                    if (sBuf[len]==k_KeySymbol[j])
                    {
                        prekey = k_KeySymbol[0];
                        keystatu = j;
                        break;
                    }
                }
            }

            if (prekey ==0)
            {
                len =0;
                sBuf[0] =0;
                sBuf[1] =NOKEY;
            }
        }
    }

    ScrFontSet(fonttype);
    ScrAttrSet(fReverseEcho);

    if (new_mode ==0x03)
    {
		s_Kb_DispString(0, new_mode, &sBuf[1], 0, fontsize, fFlushLeft);
	}
    else
    {
		if (len)
		{
			s_Kb_DispString(0, new_mode, &sBuf[1], len, fontsize, fFlushLeft);
		}
		else
		{
			if ((x + fontsize) <= LcdWidth && fFlushLeft)
			{
				Lcdprintf("_");
			}
		}
	}

    if(k_PEDINPUTTIMEOUT)    s_TimerSetMs(&lcdkd,k_PEDINPUTTIMEOUT*1000);

	while (1)
	{
		if(k_PEDINPUTTIMEOUT)
		{
			if(!s_TimerCheckMs(&lcdkd))	return 0x07;
		}
		if(kbhit()) continue;

		key=getkey();

		if(k_PEDINPUTTIMEOUT) s_TimerSetMs(&lcdkd,k_PEDINPUTTIMEOUT*1000); //2min

        //key = _getkey();
        switch(key)
        {
            case KEYCANCEL:
                sBuf[0] = 0;
                sBuf[1] = KEYCANCEL;
                sBuf[2] = 0x0;
                //ScrFontSet(old_fonttype);
               // ScrAttrSet(old_fontattr);
                return 0xff;
            case KEYENTER:
                if ((new_mode==0x03) && (len==0) && (prekey==KEY0))
                {
                    str[0] = 1;
                    str[1] = '0';
                    str[2] = 0;
                    //ScrFontSet(old_fonttype);
                    //ScrAttrSet(old_fontattr);
                    return 0x00;
                }
                if ((len==0) && fEnterReturn)
                {
                   // ScrFontSet(old_fonttype);
                   // ScrAttrSet(old_fontattr);
                    return 0x0d;
                }

                if ( len >= minlen )
                {
                    sBuf[0] = len;
                    sBuf[1+len] = 0;
                    memcpy(str, sBuf, 2+len);
                    //ScrFontSet(old_fonttype);
                    //ScrAttrSet(old_fontattr);
                    return 0x00;
                }
                //Beep();
                //Beep();
                continue;

               case KEYCLEAR:
                if (new_mode == 0x02)
                {
					if (len == 0)
					{
						//Beep();
						//Beep();
						continue;
					}

					while(len)
					{
						 len--;
						 sBuf[1+len] = 0;
						 ScrGotoxyEx(x,y);
						 s_Kb_DispString(1, new_mode, &sBuf[1], len, fontsize, fFlushLeft);
					}
					break;
                }
			case KEYF5:
				
				if (len == 0)
				{
					//Beep();
					//Beep();
					continue;
				}				
				len --;

				sBuf[1+len] = 0;

				ScrGotoxyEx(x,y);
				s_Kb_DispString(1, new_mode, &sBuf[1], len, fontsize, fFlushLeft);

				keystatu = 0;
				if ((sBuf[len] >= '0') && (sBuf[len] <= '9'))
				{
					prekey = sBuf[len];
				}
				else
				{
					if ((sBuf[len] >= 'A' && sBuf[len] <= 'Z') || sBuf[len] == '.')
					{
						for (i = 1; i < MAX_KEYTABLE; i++)
						{
							for (j = 0; j < 4; j++)
							{
								if (sBuf[len] == k_KeyTable[i][j])
								{
									prekey = k_KeyTable[i][0];
									keystatu = j;
									break;
								}
							}
						}
					}
					else if ((sBuf[len] >= 'a' && sBuf[len] < 'z'))
					{
						for (i = 1; i < MAX_KEYTABLE; i++)
						{
							for (j = 0; j < 4; j++)
							{
								if (sBuf[len] == k_KeyTable_lower[i][j])
								{
									prekey = k_KeyTable_lower[i][0];
									keystatu = j;
									break;
								}
							}
						}
					}
					else
					{
						for (j = 0; j < strlen((char *)k_KeySymbol); j++)
						{
							if (sBuf[len] == k_KeySymbol[j])
							{
								prekey = k_KeySymbol[0];
								keystatu = j;
								break;
							}
						}
					}
				}
				break;

			case KEYFN:
			case KEYF1:
			     if(LowerAlpha)
				 	LowerAlpha = 0;
			     else
				 	LowerAlpha = 1;
			     break;
			     
			case KEYALPHA:
			case KEYF2:
				if ((new_mode != 0x01) || (len == 0) || prekey == KEY00)
				{
					//Beep();
					//Beep();
					continue;
				}

				keystatu = keystatu + 1;

				if (prekey == '0')
				{
					if (keystatu >= strlen((char *)k_KeySymbol))
					{
						keystatu = 0;
					}
                    temp[0] = k_KeySymbol[keystatu];
				}
				else
				{
					if (keystatu >= 4)
						keystatu = 0;

					if(LowerAlpha)
					    temp[0] = k_KeyTable_lower[prekey-0x30][keystatu];
					else
					    temp[0] = k_KeyTable[prekey-0x30][keystatu];
				}

				sBuf[len] = temp[0];
				sBuf[len + 1] = 0;
				ScrGotoxyEx(x,y);
				s_Kb_DispString(0, new_mode, &sBuf[1], len, fontsize, fFlushLeft);
				break;
			case KEYUP:
			case KEYDOWN:
				if (SP30 == get_machine_type())
				{
					if(key==KEYDOWN)
					{
						keystatu=keystatu+1;
						if(prekey =='0')
						{
							if (keystatu>=strlen((char *)k_KeySymbol))
							keystatu=0;
						}
						else
						{
							if (keystatu>=4)	keystatu=0;
						}
					}
					else
					{
						if(keystatu==0)
						{
							if(prekey =='0') keystatu = strlen((char *)k_KeySymbol)-1;
							else keystatu=3;
						}
						else	keystatu=keystatu-1;
				
					}
				
					if (prekey =='0')
					{
						temp[0]=k_KeySymbol[keystatu];
					}
					else
					{
						if(LowerAlpha)
							 temp[0]=k_KeyTable_lower[prekey-0x30][keystatu];
						else
							 temp[0]=k_KeyTable[prekey-0x30][keystatu];
					}
					sBuf[len]=temp[0];
					sBuf[len+1]=0;
					ScrGotoxyEx(x,y);
					s_Kb_DispString(0, new_mode, &sBuf[1], len, fontsize, fFlushLeft);
				
					break;				
				}
				
			default:
				if ( key < KEY0 || key > KEY00 || len >= 128)
				{
					//Beep();
					//Beep();
					continue;
				}

				if (len >= maxlen || (key == KEY00 && len >= (maxlen-1)))
				{
					//Beep();
					//Beep();
					continue;
				}
				
				len++;
				keystatu = 0x00;
				if (key == KEY00)
				{
					sBuf[len++] = '0';
					sBuf[len] = '0';
					prekey = KEY0;
				}
				else
				{
					temp[0] = k_KeyTable[key - 0x30][0];
					sBuf[len] = temp[0];
					prekey = key;
				}
				sBuf[len+1] = 0;
				ScrGotoxyEx(x,y);

				if (new_mode == 0x03)
				{
					if (((len == 1) && (key == KEY0)) || ((len == 2) && (key == KEY00)))
					{
						len = 0;
						continue;
					}
				}
				s_Kb_DispString(0, new_mode, &sBuf[1], len, fontsize, fFlushLeft);

				break;
		} //switch(key)
	} //while
	return 0x00;
}

extern uchar bIsChnFont;
// 调节对比度
void AdjustLCDGray(void)
{
    uchar   ret;
    uchar   gray;
	uchar   grayval[2];
    uchar   logo[256];
	uchar   LcdGrayVal,LcdGrayValSave;

	LcdGrayValSave=LcdGrayVal = ReadLCDGray();

	while(1)
    {
        ScrClrLine(2, 7);
        if(bIsChnFont)
        {
            ScrPrint(0,3,0x41,"屏幕亮度:%d%%",LcdGrayVal*100/MAX_GRAY);
        }
        else
        {
            ScrPrint(0,3,0x41,"Brightness:%d%%",LcdGrayVal*100/MAX_GRAY);
        }
        
        switch(getkey())
        {
        case KEYCANCEL:
            s_ScrSetGrayBase(LcdGrayValSave);
			ScrGray(4);
            return;

        case KEYENTER:
            if(ReadLCDGray() != LcdGrayVal)
			{
	            if(WriteLCDGray(LcdGrayVal))
				{
                    if(bIsChnFont)
                    {
                        ScrPrint(0,6,0x41,"保存失败!");
                    }
                    else
                    {
    					ScrPrint(0,6,0x41,"Save Fail!");
                    }
				}
				else
				{
                    if(bIsChnFont)
                    {
                        ScrPrint(0,6,0x41,"保存成功");
                    }
                    else
                    {
    					ScrPrint(0,6,0x41,"Save Success");
                    }
				}
				WaitKeyBExit(3,0);
			}
			return;

        case KEYUP:
		case KEYF1:
            if (LcdGrayVal >= MAX_GRAY)
			{
				LcdGrayVal = MAX_GRAY;
			}
			else
			{
                LcdGrayVal++;
            }
			s_ScrSetGrayBase(LcdGrayVal);
			ScrGray(4);
            break;

        case KEYDOWN:
		case KEYF2:
            if (LcdGrayVal <= MIN_GRAY)
			{
				LcdGrayVal = MIN_GRAY;
			}	
            else
            {
                LcdGrayVal--;
            }
            s_ScrSetGrayBase(LcdGrayVal);
            ScrGray(4);
            break;
            
        default:break;
        }
    }
}
