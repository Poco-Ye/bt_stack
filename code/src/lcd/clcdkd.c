#include "base.h"
#include "lcdapi.h"
#include "..\font\font.h"

#define  ASCII                  0x00
#define  CFONT                  0x01
#define  MAX_KEYTABLE           10
#define  MAX_STR_SIZE           128   // GetString()可处理的最大字符个数(英文)

extern ulong  k_PEDINPUTTIMEOUT;
extern const uchar k_KeyTable[10][5];
extern const uchar k_KeySymbol[40];
extern const uchar k_KeyTable_lower[10][5];

#define GetStringWarn()    do { Beep(); \
							Beep(); } while(0);

// get_str_opt中new_mode的值
#define NEWMODE_DIGIT     0
#define NEWMODE_CHAR      1 
#define NEWMODE_PASSWD    2 
#define NEWMODE_AMOUNT    3 

typedef struct
{
	uchar new_mode;
	int fontwidth;
	int fontheight;
	uchar fFlushLeft;
	uchar LowerAlpha;
	int x;
	int y;
}GET_STR_OPT_T;

void s_Kb_DataStrConvAmt(uchar *disp_buf, uchar *amt_buf, uchar len);

static void ShiftKey(uchar *in, uchar dir, int lowerAlpha)
{
	int i, j, max_i, max_j;
	const uchar (*p)[5];
	
	if(*in == 0) return;
	max_j = strlen((char*)k_KeySymbol);
	
	for(j=0; j<max_j; j++)
	{
		if(k_KeySymbol[j] == *in)
		{
			j = (dir == KEYDOWN) ?  j + 1 : j - 1;
			if(j < 0)
				j = max_j - 1;
			if(j > max_j - 1)
				j = 0;

			*in = k_KeySymbol[j];
			return;
		}
	}

	p = (lowerAlpha == 0) ? k_KeyTable : k_KeyTable_lower;
	max_i = sizeof(k_KeyTable) / sizeof(k_KeyTable[0]);
	max_j = strlen((char*)(k_KeyTable[0]));
	for(i=0; i<max_i; i++)
	{
		for(j=0; j<max_j; j++)
		{
			if(p[i][j] == *in)
			{
				j = (dir == KEYDOWN) ?  j + 1 : j - 1;
				if(j < 0)
					j = max_j - 1;
				if(j > max_j - 1)
					j = 0;

				*in = p[i][j];
				return;
			}
		}
	}
}

uchar GetInput(uchar *prekey, GET_STR_OPT_T *gso)
{
	uchar key = getkey();
	switch (key)
	{
		case KEYUP:
		case KEYDOWN:
			if(gso->new_mode == NEWMODE_CHAR)
				ShiftKey(prekey, key, gso->LowerAlpha);
			else
				GetStringWarn();

			key = NOKEY;
			break;

		case KEYALPHA:
			if(gso->new_mode == NEWMODE_CHAR)
			{
				gso->LowerAlpha = ! gso->LowerAlpha;
				if((gso->LowerAlpha == 0) && (*prekey >= 'a') && (*prekey <= 'z'))
					*prekey -= 'a' - 'A';
				if((gso->LowerAlpha != 0) && (*prekey >= 'A') && (*prekey <= 'Z'))
					*prekey += 'a' - 'A';
			}
			else
			{
				GetStringWarn();
			}
			key = NOKEY;
			break;

		case KEYCANCEL:
		case KEYENTER:
		case KEYCLEAR:
		default:
			break;
	}
	return key;
}

/* Kb_DispString():
 *    inp_buf格式：
 *        inp_buf[0]为字符串长度
 *        inp_buf[1]开始为字符串
 */
static void Kb_DispString(uchar * inp_buf, GET_STR_OPT_T gso)
{
	int x, LcdWidth;
	ST_LCD_INFO lcdinfo;
	uchar disp_buf[MAX_STR_SIZE + 4];
	uchar max_len, offset = 0;
	int len = inp_buf[0];

	if (len > MAX_STR_SIZE)
	{
		GetStringWarn();
		return;
	}

	// 显示区域属性
	CLcdGetInfo(&lcdinfo);
	LcdWidth = lcdinfo.width;
	max_len = (LcdWidth - gso.x) / gso.fontwidth;   // 显示区域可显示的最大字符个数。

	// 清除显示区域
	memset(disp_buf, ' ', sizeof(disp_buf));
	disp_buf[max_len] = 0;
	CLcdTextOut(gso.x, gso.y, disp_buf);

	// 整理输入字符串准备显示
	memset(disp_buf, ' ', sizeof(disp_buf));
	if (gso.new_mode == NEWMODE_AMOUNT)
	{
		s_Kb_DataStrConvAmt(disp_buf, inp_buf + 1, len);
		if (len < 4)
			len = 5;
		else
			len += 2;              // 多了一个小数点
	}
	else
	{
		if (gso.new_mode == NEWMODE_PASSWD)
			memset(&disp_buf[1], '*', len);
		else
			memcpy(&disp_buf[1], inp_buf + 1, len);

		len++;
	}

	disp_buf[len] = '_';
	disp_buf[len + 1] = 0;

	// 显示字符串
	x = gso.x;

    if (gso.fFlushLeft)
	{
		if(max_len != 1)
		{
			if (len > max_len)
				offset = len - max_len;
		}
	}
	else
	{
		if(len >= max_len)
		{
            offset = len - max_len;
			x = LcdWidth - max_len * gso.fontwidth;
		}
		else
			x = LcdWidth - len * gso.fontwidth;
	}

    CLcdTextOut(x, gso.y, disp_buf + offset + 1);
}

/************************************************************/
/* CLcdGetString(): 输入字符串                              */
/*  mode:                                                   */
/*              bit7: 1[0] 能[否]回车退出                   */
/*              bit6: 1[0] 大[小]字体 (不再支持)            */
/*              bit5: 1[0] 能[否]输数字                     */
/*              bit4: 1[0] 能[否]输字符                     */
/*              bit3: 1[0] 是[否]密码方式                   */
/*              bit2: 1[0] 左[右]对齐输入                   */
/*              bit1: 1[0] 有[否]小数点                     */
/*              bit0: 1[0] 正[反]显示(不再支持)             */
/************************************************************/
/*  返回值:
 *            0x00, 正常输入完成
 *            0x00, 操作超时
 *            0x0d, 还没输入时输入回车。
 *            0xfe, 参数值非法
 *            0xff, 用户取消
 */
uchar CLcdGetString(uint x, uint y, uchar * str, uchar mode, uchar minlen, uchar maxlen)
{
	GET_STR_OPT_T gso;
	uchar ret = 0;
	uint done = GetTimerCount() + k_PEDINPUTTIMEOUT * 1000;
	uchar key, len;
	uchar sBuf[MAX_STR_SIZE + 4];
	ST_LCD_INFO lcdinfo;

	CLcdGetInfo(&lcdinfo);
	if (minlen > maxlen || maxlen > MAX_STR_SIZE || maxlen == 0)
		return 0xfe;

	if ((str == NULL) || (mode & 0x38) == 0)     // 0011 1000
		return 0xfe;

	if ((x < 0) || (y < 0) || (x >= lcdinfo.width) || (y >= lcdinfo.height))
		return 0xfe;

	// 处理参数
	
	gso.LowerAlpha = 1;
	gso.fFlushLeft   = (mode & BIT2) ? 1 : 0;

	if (mode & BIT5)
		gso.new_mode = NEWMODE_DIGIT;

	if (mode & BIT4)
		gso.new_mode = NEWMODE_CHAR;

	if (mode & BIT3)
		gso.new_mode = NEWMODE_PASSWD;

	if ((gso.new_mode == NEWMODE_DIGIT) && (mode & BIT1))
		gso.new_mode = NEWMODE_AMOUNT;

	gso.x = x;
	gso.y = y;
	s_GetLcdFontCharAttr(&gso.fontwidth, &gso.fontheight, NULL, NULL);

	// 处理初始字符串
	if ((gso.new_mode == NEWMODE_PASSWD) || (gso.new_mode == NEWMODE_AMOUNT))
		len = 0;          // 密码模式和带小数点的数字模式不接受初始字符串
	else
		len = strlen((char *)str);

	if(len > maxlen)
		len = maxlen;

	sBuf[0] = len;
	memcpy(sBuf + 1, str, len);
	sBuf[len + 1] = 0;
	Kb_DispString(sBuf, gso);

	// 处理按键
	while (1)
	{
		if (done < GetTimerCount())
		{
			ret = 0x07;        // 超时
			goto getstring_end;
		}
		if (kbhit())
			continue;

		done = GetTimerCount() + k_PEDINPUTTIMEOUT * 1000;
		key = GetInput(&sBuf[len], &gso);
		switch (key)
		{
			case NOKEY:
				break;
			case KEYCANCEL:
				sBuf[0] = 0;
				ret = 0xff;
				goto getstring_end;
			case KEYENTER:
				if ((gso.new_mode == NEWMODE_AMOUNT) && (len == 0))
				{                   // 数字含小数点模式，无输入时返回0
					str[0] = 1;
					str[1] = '0';
					str[2] = 0;
					goto getstring_end;
				}
				if ((len == 0) && (mode & BIT7)) /* enter to exit */
				{
					sBuf[0] = 0;
					memcpy(str, sBuf, 2 + len);
					ret = 0x0d;
					goto getstring_end;
				}
				if (len >= minlen)
				{
					sBuf[0] = len;
					sBuf[1 + len] = 0;
					memcpy(str, sBuf, 2 + len);
					goto getstring_end;
				}
				GetStringWarn();
				break;
			case KEYCLEAR:
				if (len == 0)
				{
					GetStringWarn();
					break;
				}
				if (gso.new_mode == NEWMODE_PASSWD) //密码模式全清，其他模式退格
				{
					len = 0;
					memset(sBuf, 0, sizeof(sBuf));
					break;
				}
				else
				{
					len--;
					sBuf[0] = len;
					sBuf[1 + len] = 0;
					break;
				}
				break;
			default:
				if ( key < KEY0 || key >= KEY00)
				{
					GetStringWarn();
					break;
				}

				if((len >= MAX_STR_SIZE) || (len >= maxlen))
				{
					GetStringWarn();
					break;
				}

				len++;
				sBuf[0] = len;
				sBuf[len] = k_KeyTable[key - 0x30][0];
				sBuf[len + 1] = 0;
				if (gso.new_mode == NEWMODE_AMOUNT)
				{
					if((len==1) && (key==KEY0)) len = 0;
				}
				break;
		}
		Kb_DispString(sBuf, gso);
	}

getstring_end:
	return ret;
}

uchar CLcdGetStringEx(int col, int row, uchar * str, uchar mode, uchar minlen, uchar maxlen)
{
	int char_w, char_h, x, y;

	s_GetLcdFontCharAttr(&char_w, &char_h, NULL, NULL);
	y = char_h * row;
	x = char_w * col;
	return CLcdGetString(x, y, str, mode, minlen, maxlen);
}

