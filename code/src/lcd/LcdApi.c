#include <posapi.h>
#include "base.h"
#include "LcdDrv.h"
#include "LcdApi.h"
#include <stdarg.h>
#include "..\font\font.h"

extern LCD_T gtLcdInfo;
extern ST_FONT tBkMapSingleFont, tBkMapMultiFont;
extern app_origin_t gstAppScrOrg;

void s_RefreshIcons(void);
void s_ScrSetIcon(unsigned char icon_no,unsigned char mode);

void s_SetAppScrOrg(int x, int y)
{
    gstAppScrOrg.x = x;
    gstAppScrOrg.y = y;
}

void s_GetAppScrOrg(int *x, int *y)
{
    *x = gstAppScrOrg.x;
    *y = gstAppScrOrg.y;
}

void s_LcdAttrInit(void)
{
	int machType;

	memset(&gtLcdInfo, 0, sizeof(LCD_T));

 	machType = get_machine_type();
	if(machType == S300) 
	{
		/* parameter in mono role */
		gtLcdInfo.tLcdAttr[MONO_ROLE].cursor_x = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].cursor_y = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].reverse = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].width = 128;
		gtLcdInfo.tLcdAttr[MONO_ROLE].height = 110;
		gtLcdInfo.tLcdAttr[MONO_ROLE].lineCount = 13; 
		gtLcdInfo.tLcdAttr[MONO_ROLE].charSpace = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].lineSpace = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].numerator = 2;
		gtLcdInfo.tLcdAttr[MONO_ROLE].denominator = 1;
		gtLcdInfo.tLcdAttr[MONO_ROLE].backcolor = COLOR_WHITE;
        gtLcdInfo.tLcdAttr[MONO_ROLE].forecolor = COLOR_BLACK;

		/* parameter in color role */
		gtLcdInfo.tLcdAttr[COLOR_ROLE].width = 240;
		gtLcdInfo.tLcdAttr[COLOR_ROLE].height = 220;
        gtLcdInfo.tLcdAttr[COLOR_ROLE].backcolor = COLOR_WHITE;
        gtLcdInfo.tLcdAttr[COLOR_ROLE].forecolor = COLOR_BLACK;
	}
	else if(machType == S900)
	{
		/* parameter in mono role */
		gtLcdInfo.tLcdAttr[MONO_ROLE].cursor_x = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].cursor_y = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].reverse = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].width = 128;
		gtLcdInfo.tLcdAttr[MONO_ROLE].height = 140;
		gtLcdInfo.tLcdAttr[MONO_ROLE].lineCount = 17; 
		gtLcdInfo.tLcdAttr[MONO_ROLE].charSpace = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].lineSpace = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].numerator = 2;
		gtLcdInfo.tLcdAttr[MONO_ROLE].denominator = 1;
		gtLcdInfo.tLcdAttr[MONO_ROLE].backcolor = COLOR_WHITE;
        gtLcdInfo.tLcdAttr[MONO_ROLE].forecolor = COLOR_BLACK;

		/* parameter in color role */
		gtLcdInfo.tLcdAttr[COLOR_ROLE].width = 240;
		gtLcdInfo.tLcdAttr[COLOR_ROLE].height = 280;
        gtLcdInfo.tLcdAttr[COLOR_ROLE].backcolor = COLOR_WHITE;
        gtLcdInfo.tLcdAttr[COLOR_ROLE].forecolor = COLOR_BLACK;
	}
	else if(machType == S800 || machType == S500 || machType == D200 || machType == D210)
	{
		/* parameter in mono role */
		gtLcdInfo.tLcdAttr[MONO_ROLE].cursor_x = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].cursor_y = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].reverse = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].width = 128;
		gtLcdInfo.tLcdAttr[MONO_ROLE].height = 80;
		gtLcdInfo.tLcdAttr[MONO_ROLE].lineCount = 10; 
		gtLcdInfo.tLcdAttr[MONO_ROLE].charSpace = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].lineSpace = 0;
		gtLcdInfo.tLcdAttr[MONO_ROLE].numerator = 5;
		gtLcdInfo.tLcdAttr[MONO_ROLE].denominator = 2;
		gtLcdInfo.tLcdAttr[MONO_ROLE].backcolor = COLOR_WHITE;
        gtLcdInfo.tLcdAttr[MONO_ROLE].forecolor = COLOR_BLACK;

		/* parameter in color role */
		gtLcdInfo.tLcdAttr[COLOR_ROLE].width = 320;
		gtLcdInfo.tLcdAttr[COLOR_ROLE].height = 200;
        gtLcdInfo.tLcdAttr[COLOR_ROLE].backcolor = COLOR_WHITE;
        gtLcdInfo.tLcdAttr[COLOR_ROLE].forecolor = COLOR_BLACK;
	}
}

void s_ScrInit(void)
{
    int i;

	s_LcdAttrInit();
    s_LcdDrvInit();

    ScrGray(4);
    for (i = 0; i < ICON_NUMBER; i++)
    {
    	if ((i+1) == ICON_BATTERY)
    	{
    		s_ScrSetIcon(ICON_BATTERY, 6);
			continue;
    	}
        ScrSetIcon(i + 1, 0);   /* 关闭所有图标 */
    }

    ScrBackLight(1);
}

int s_SetLcdInfo(int object, int ops)
{
    int machType;
	static int picc_ops=0, ts_ops=0;
    
	machType = get_machine_type();
    if(object == 0 && machType == S300)
    {
		if(ops == ts_ops) return 0;

		ts_ops = ops;
        if(ops == 0) /* TouchScreenClose */
        {
            gtLcdInfo.tLcdAttr[MONO_ROLE].height -= 30;
            gtLcdInfo.tLcdAttr[COLOR_ROLE].height -= 60;
        }
        else /* TouchScreenOpen */
        {
            gtLcdInfo.tLcdAttr[MONO_ROLE].height += 30;
            gtLcdInfo.tLcdAttr[COLOR_ROLE].height += 60;
        }
    }	
	else if (object == 1)
    {
        if(ops == picc_ops) return 0;

        picc_ops = ops;
        if (ops == 0) /* vled_close */
        {
            gstAppScrOrg.y -= 40;
            if (gtLcdInfo.tLcdAttr[COLOR_ROLE].width == WIDTH_FOR_TFT_S800)
            {
                gtLcdInfo.tLcdAttr[MONO_ROLE].height += 16;
            }
            else
            {
                gtLcdInfo.tLcdAttr[MONO_ROLE].height += 20;
            }
            
            gtLcdInfo.tLcdAttr[COLOR_ROLE].height += 40;
        }
        else /* vled_open */
        {
            gstAppScrOrg.y += 40;
            if (gtLcdInfo.tLcdAttr[COLOR_ROLE].width == WIDTH_FOR_TFT_S800)
            {
                gtLcdInfo.tLcdAttr[MONO_ROLE].height -= 16;
            }
            else
            {
                gtLcdInfo.tLcdAttr[MONO_ROLE].height -= 20;
            }
            gtLcdInfo.tLcdAttr[COLOR_ROLE].height -= 40;
        }
	}
    else if (object == 2)
    {
        if(ops == 0)
        {
            if(gtLcdInfo.tLcdAttr[COLOR_ROLE].isfull) return 0;
            gtLcdInfo.tLcdAttr[COLOR_ROLE].isfull = 1;
            s_SetAppScrOrg(0, 0);

            gtLcdInfo.tLcdAttr[COLOR_ROLE].height += 40; /* statusbar */
            if (gtLcdInfo.tLcdAttr[COLOR_ROLE].width == WIDTH_FOR_TFT_S800){
                gtLcdInfo.tLcdAttr[MONO_ROLE].height += 16;
            }
            else{
                gtLcdInfo.tLcdAttr[MONO_ROLE].height += 20;
            }            
        }
        else
        {
            if(!gtLcdInfo.tLcdAttr[COLOR_ROLE].isfull) return 0;
            gtLcdInfo.tLcdAttr[COLOR_ROLE].isfull = 0;
            s_SetAppScrOrg(0, 40);

            gtLcdInfo.tLcdAttr[COLOR_ROLE].height -= 40; /* statusbar */
            if (gtLcdInfo.tLcdAttr[COLOR_ROLE].width == WIDTH_FOR_TFT_S800){
                gtLcdInfo.tLcdAttr[MONO_ROLE].height -= 16;
            }
            else{
                gtLcdInfo.tLcdAttr[MONO_ROLE].height -= 20;
            }
            s_RefreshIcons();
        }
    }	
	gtLcdInfo.tLcdAttr[MONO_ROLE].lineCount = gtLcdInfo.tLcdAttr[MONO_ROLE].height / 8; 
}


LCD_ATTR_T *GetLcdInfo(int role)
{
	return &gtLcdInfo.tLcdAttr[role];
}

void s_ScrCursorSet(int x, int y)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
	ptLcdAttr->cursor_x = x;
	ptLcdAttr->cursor_y = y;
}

void s_ScrCursorGet(int *x, int *y)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
	*x = ptLcdAttr->cursor_x;
	*y = ptLcdAttr->cursor_y;
}

void ScrGray(unsigned char mode)
{
	s_LcdGray(mode);
}

void ScrGotoxy(unsigned char x, unsigned char y)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
	x = x % ptLcdAttr->width;
	y = y % ptLcdAttr->lineCount;
   	s_ScrCursorSet(x, y * 8); 
}

unsigned char ScrAttrSet(unsigned char Attr)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
    unsigned char tmp;

    tmp = ptLcdAttr->reverse;
    ptLcdAttr->reverse = Attr;
    return tmp;
}

unsigned char GetScrCurAttr()
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
	return ptLcdAttr->reverse;
}

void ScrPlot(unsigned char x,unsigned char y,unsigned char Color)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
    int line;
    unsigned char ch;

    if (x >= ptLcdAttr->width || y >= ptLcdAttr->height)
        return;

	if(Color)
		s_ScrFillRect(x, y, 1, 1, ptLcdAttr->forecolor, MONO_ROLE, FORE_LAYER);
	else
		s_ScrClrRect(x, y, 1, 1, MONO_ROLE);
}

void ScrSpaceGet(int* piCharSpace, int* piLineSpace)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);

     if(piCharSpace != NULL)
	 	 *piCharSpace = ptLcdAttr->charSpace;

     if(piLineSpace != NULL)
	 	 *piLineSpace = ptLcdAttr->lineSpace;
}

void ScrDrLogo(unsigned char *logo)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
	ScrDrLogoxy(ptLcdAttr->cursor_x, ptLcdAttr->cursor_y, logo);
}


static uchar iconUpdate[ICON_NUMBER] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
static uchar iconStatus[ICON_NUMBER] = 
		{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,0xff, 0xff,0xff};
// call in 10ms timer interrupt
void ScrUpdateIcon(void) 
{
	int i;

	if (s_GetBatteryEntryFlag() == 0)return;
	if(LcdCheckBusy())return;

	for(i=0; i<ICON_NUMBER; i++)
	{
		if(iconUpdate[i])
		{
			iconUpdate[i] = 0;
			s_LcdSetIcon(i+1, iconStatus[i]);//from 1 to 8
		}
	}
}

void s_RefreshIcons(void)
{
    int i;

    for(i = 0; i < ICON_NUMBER; i++)
    {
        iconUpdate[i] = 1;
    }

    return ;
}

/* call in app, many interrupt */
void ScrSetIcon(unsigned char icon_no,unsigned char mode)
{
    LCD_ATTR_T *ptLcdAttr =GetLcdInfo(COLOR_ROLE);
	uint x;

    if (ptLcdAttr->isfull) return;
	
	switch(icon_no)
	{
		case ICON_PHONE:
			if (mode > 2) return;
		break;
		case ICON_SIGNAL:
			if (mode > 6) return;
		break;
		case ICON_BATTERY:
			if (mode > 5) return;
		break;
        case ICON_PRINTER:
        case ICON_ICCARD:
        case ICON_LOCK:
		case ICON_UP:
		case ICON_DOWN:
		case ICON_BT:
		case ICON_ETH:
			if (mode > 1) return;
		break;
		case ICON_WIFI:
			if (mode > 3) return;
		break;
		default:
		return;
	}    

	/* disable interrupt,forbid timer interrupt call ScrUpdateIcon */
	irq_save(x);
	icon_no = icon_no - 1;//from 0 to 11
	if(iconStatus[icon_no] != mode)
	{
		iconStatus[icon_no] = mode;
		iconUpdate[icon_no] = 1;
	}
	irq_restore(x);
}
/* just for batter icon, and call in monitor */
void s_ScrSetIcon(unsigned char icon_no,unsigned char mode)
{
	uint x;
	switch(icon_no)
	{
		case ICON_BATTERY:
			if (mode > 6) return;
		break;
		default:
		return;
	}    

	/* disable interrupt,forbid timer interrupt call ScrUpdateIcon */
	irq_save(x);
	icon_no = icon_no - 1;//from 0 to 11
	if(iconStatus[icon_no] != mode)
	{
		iconStatus[icon_no] = mode;
		iconUpdate[icon_no] = 1;
	}
	irq_restore(x);
}


void ScrDrLogoxy(int left, int top, unsigned char *logo)
{
    int i, lines;
    unsigned short len;

	if(left < 0 || top < 0) return ;

    lines = *logo++;
    for (i = 0; i < lines; i++)
    {
        len = *logo++;
        len <<= 8;
        len += *logo++;

        s_ScrWriteBitmap(left, top + i * 8, len, 8, logo, 1, MONO_ROLE, 1, FORE_LAYER);
        logo += len;
    }
}

unsigned char ScrRestore(unsigned char mode)
{
	return s_ScrRestore(mode);
}

void ScrDrawBox(unsigned char y1,unsigned char x1,unsigned char y2,unsigned char x2)
{
    unsigned char i, wn, m, n;

    if (y2 < y1)
        return;
    if (x2 < x1)
        return;

    if ((y2 - y1) < 2)
        return;                 //禁止通过画框函数显示出PIN字样
    if ((x2 - x1) < 5)
        return;                 //禁止通过画框函数显示出PIN字样

    wn = x2 * 6;
    m = y1 * 8 + 4;
    n = y2 * 8 - 4;

    for (i = x1 * 6 + 1; i < wn; i++)
    {
        ScrPlot(i, m, 1);
        ScrPlot(i, n, 1);
    }

    wn = y2 * 8 - 4;
    m = x1 * 6 + 1;
    n = x2 * 6 - 1;

    for (i = y1 * 8 + 5; i < wn; i++)
    {
        ScrPlot(m, i, 1);
        ScrPlot(n, i, 1);
    }
}

void ScrGotoxyEx(int pixel_X, int pixel_Y)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);

	if(pixel_X<0 || pixel_X>=ptLcdAttr->width) return;
	if(pixel_Y<0 || pixel_Y>=ptLcdAttr->height) return;
	s_ScrCursorSet(pixel_X, pixel_Y);
}

void ScrGetxyEx(int *pixel_X, int *pixel_Y)
{
	if(pixel_X == NULL || pixel_Y == NULL) return ;

	s_ScrCursorGet(pixel_X, pixel_Y);
}

void ScrSpaceSet(int CharSpace, int LineSpace)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);

    if (CharSpace < 0 || CharSpace >= ptLcdAttr->width)
        return;
    if (LineSpace < 0 || LineSpace >= ptLcdAttr->height)
        return;

    ptLcdAttr->charSpace = CharSpace;
    ptLcdAttr->lineSpace = LineSpace;
}

static int StringLenStat(uchar *txt)
{
	int sWidth, mWidth;
	int sNum=0, mNum=0, len=0;
    
	while(*txt)
	{
		if(*txt > 0x80)	
		{
			mNum++;
			txt += 2;
		}
		else
		{
			sNum++;
			txt++;
		}
	}

	s_GetLcdFontCharAttr(&sWidth, NULL, &mWidth, NULL);

	if(mWidth == 0 && mNum != 0)
		mWidth = sWidth * 2;

	len = sNum * sWidth + mNum * mWidth;
	return len;
}

/*******************************************************************************************************
函数原型	void s_ScrTextOut(int x, int y, int align, int update_cursor, unsigned char *txt, int role)
函数功能	写数据到指定的位置
参数说明	入参	x -- 左上角x坐标
                    y -- 左上角y坐标
                    align -- 是否居中对齐，0x00左对齐，0x01居中对齐,0x02居右对齐
                    update_cursor -- 是否更新光标
                    txt -- 数据缓冲区
                    role -- COLOR_ROLE/MONO_ROLE 彩屏或者黑白屏
			出参	无
返回值		无
补充说明	
*********************************************************************************************************/
void s_ScrTextOut(int x, int y, uint align, int update_cursor, unsigned char *txt, int role)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(role);
	int charspace, linespace, reverse;
	int iSingleCharWidth;
	int cursor_x, cursor_y;
    int i,Zoom,rgb;
	int lcd_height, lcd_width;	
    int dot_w, dot_h;
    int displayed=0, width, height, lineHeight, charWidth, CodeBytes;
    unsigned char DotBuf[MAX_FONT_WIDTH / 8 * MAX_FONT_HEIGHT + MAX_FONT_HEIGHT];
	int color = ptLcdAttr->forecolor;

    if (txt == NULL) return;

	charspace = ptLcdAttr->charSpace;
	linespace = ptLcdAttr->lineSpace;
	reverse = ptLcdAttr->reverse;
	lcd_width = ptLcdAttr->width;
	lcd_height = ptLcdAttr->height;

	if (x >= lcd_width || y >= lcd_height) return;

	cursor_x = update_cursor ? ptLcdAttr->cursor_x : x;
	cursor_y = update_cursor ? ptLcdAttr->cursor_y : y;

	if(cursor_x < 0 || cursor_y < 0) return ;

    s_GetLcdFontCharAttr(&charWidth, &lineHeight, NULL, NULL);
    
	if(align == 0x01) //居中
	{
		if(role == MONO_ROLE)
		{
			int iTmp;
			iSingleCharWidth = charWidth + charspace;
			if(iSingleCharWidth == 0) iSingleCharWidth = 8;
			if(strlen(txt)*iSingleCharWidth < lcd_width-cursor_x)
			{
				iTmp = (lcd_width - cursor_x) / iSingleCharWidth;
				if(reverse)
					s_ScrFillRect(cursor_x, cursor_y, iTmp*iSingleCharWidth, lineHeight, color, role, FORE_LAYER);
				else
					s_ScrClrRect(cursor_x, cursor_y, iTmp*iSingleCharWidth, lineHeight, role);

				cursor_x += (lcd_width - cursor_x - strlen(txt)*iSingleCharWidth)/2;
			}
		}
		else  /* COLOR_ROLE */
		{
			int iTextLen;
			iTextLen = StringLenStat(txt);
			if(iTextLen < lcd_width - cursor_x)
				cursor_x += (lcd_width - cursor_x - iTextLen)/2;
		}
	}
    else if(align == 0x02) //右对齐
    {
		if(role == COLOR_ROLE)
		{
			int iTextLen;
			iTextLen = StringLenStat(txt);
			if(iTextLen < lcd_width - cursor_x)
				cursor_x += lcd_width - cursor_x - iTextLen;
		}
    }

    if(role == MONO_ROLE)
    {
        TryMapFont();
    }
            
    while (*txt)
    {
        if (*txt == '\r')
        {
            txt++;
            continue;
        }
        if (*txt == '\n')
        {
            txt++;
            cursor_x = 0;
            if (displayed && role == MONO_ROLE)
			{
				if(reverse)
					s_ScrFillRect(0, cursor_y+lineHeight, lcd_width, linespace, color, role, FORE_LAYER);
				else
					s_ScrClrRect(0, cursor_y+lineHeight, lcd_width, linespace, role);
			}

            cursor_y += (lineHeight + linespace);
            continue;
        }
        
        displayed = 1;

        /*获取字符点阵数据和字体宽度、高度*/
        CodeBytes = s_GetLcdFontDot(reverse, txt, DotBuf, &width, &height);
        dot_w = width; //dot_w 为实际点阵宽度,width为放大前字体宽度
        dot_h = height; //dot_h 为实际点阵高度,height为放大前字体高度

		if(role == MONO_ROLE)
		{
			if(!CheckZoom())
			{
				if(CodeBytes == 1)
				{
					width = tBkMapSingleFont.Width;
					height = tBkMapSingleFont.Height;
				}
				else 
				{
					width = tBkMapMultiFont.Width;
					height = tBkMapMultiFont.Height;
				}
			}

		}
			/*假如当前行不够显示该字符，则跳到下一行*/
		if (cursor_x + width > lcd_width)
		{
			if(role != MONO_ROLE) return ;

			if(reverse)
			{
				/*填充该行剩余的空间*/
				s_ScrFillRect(cursor_x, cursor_y, width, height, color, role, FORE_LAYER);
				/*填充两行之间的行间距矩形*/
				s_ScrFillRect(0, cursor_y+lineHeight, lcd_width, linespace, color, role, FORE_LAYER);
			}
			else
			{
				/*填充该行剩余的空间*/
				s_ScrClrRect(cursor_x, cursor_y, width, height, role);
				/*填充两行之间的行间距矩形*/
				s_ScrClrRect(0, cursor_y+lineHeight, lcd_width, linespace, role);
			}

			cursor_x = 0;
			cursor_y += (lineHeight + linespace);
		}

        if (cursor_y >= lcd_height) cursor_y = 0;

		if(role == MONO_ROLE)//add by zhaorh,20141211
		{
			if (dot_h != height)
			{
				if(reverse)
					s_ScrFillRect(cursor_x, cursor_y, width, height, color, role, FORE_LAYER);
				else
					s_ScrClrRect(cursor_x, cursor_y, width, height, role);
			}
		}
		s_ScrWriteBitmap(cursor_x, cursor_y, dot_w, dot_h, DotBuf, 1, role, CheckZoom(), FORE_LAYER);
        cursor_x += width;
        
		if(role == MONO_ROLE)
		{
			if (cursor_x == 0) lineHeight = height;

			for (i = 0; i < height / 8; i++)
			{
				if(reverse)
					s_ScrFillRect(cursor_x, cursor_y + i * 8, charspace, 8, color, role, FORE_LAYER);
				else
					s_ScrClrRect(cursor_x, cursor_y + i * 8, charspace, 8, role);
			}

			if (height % 8)
			{
				if(reverse)
					s_ScrFillRect(cursor_x, cursor_y + i * 8, charspace, height % 8, color, role, FORE_LAYER);
				else
					s_ScrClrRect(cursor_x, cursor_y + i * 8, charspace, height % 8, role);
			}
		}

        cursor_x += charspace;
        if (lineHeight < height) lineHeight = height;

        /*获取下一个字符地址*/
        txt += CodeBytes;
    }

	if(role == MONO_ROLE)
	{
		TryRestoreFont();
	}
	
	if (cursor_x >= lcd_width)
	{
		if(role != MONO_ROLE) return ;
		if(reverse)//add by zhaorh 20141211
		{
			/*填充两行之间的行间距矩形*/
			s_ScrFillRect(0, cursor_y+lineHeight, lcd_width, linespace, color, role, FORE_LAYER);
		}
		else
		{
			/*填充两行之间的行间距矩形*/
			s_ScrClrRect(0, cursor_y+lineHeight, lcd_width, linespace, role);
		}
		cursor_x = 0;
		cursor_y += (lineHeight + linespace);
		if (cursor_y >= lcd_height)
		{
			cursor_y = 0;
		}
	}

	if(update_cursor)
	{
		ptLcdAttr->cursor_x = cursor_x;
		ptLcdAttr->cursor_y = cursor_y;
	}
}

void ScrGetLcdSize(int *width, int *height)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);

	if(width == NULL || height == NULL) return ;

	*width  = ptLcdAttr->width;
	*height = ptLcdAttr->height;
}

/**************************************************************************************
函数原型	void ScrClrLine(unsigned char startline, unsigned char endline)
函数功能	黑白屏幕接口，清除行，一行8个像素点
参数说明	入参	startline -- 起始行
                    endline -- 结束行
			出参	无
返回值		无
补充说明	
***************************************************************************************/
void ScrClrLine(unsigned char startline, unsigned char endline)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
	int width, totalLineIndex;

	width = ptLcdAttr->width;
	totalLineIndex = ptLcdAttr->lineCount - 1;

    if (startline > totalLineIndex && endline > totalLineIndex)
        return;

    if (startline > endline)
        return;

    if (endline > totalLineIndex)
        endline = totalLineIndex;

   	s_ScrClrRect(0, startline*8, width, (endline-startline+1)*8, MONO_ROLE);
	s_ScrCursorSet(0, startline*8);
}

/**************************************************************************************
函数原型	void ScrDrawRect(int left,int top, int right,int bottom)
函数功能	黑白屏幕接口，画矩形
参数说明	入参	left -- 左上角x坐标
                    top -- 左上角y坐标
                    right -- 右下角x坐标
                    bottom -- 右下角y坐标
			出参	无
返回值		无
补充说明	
***************************************************************************************/
void ScrDrawRect(int left,int top, int right,int bottom)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
	int MAX_X = ptLcdAttr->width;
	int MAX_Y = ptLcdAttr->height;
	int color = ptLcdAttr->forecolor;

    if (left > right || top > bottom)
        return;

    if (left < -MAX_X) 	left = -MAX_X;
    if (top < -MAX_Y) 	top = -MAX_Y;
    if (right > MAX_X) 	right = MAX_X;
    if (bottom > MAX_Y) bottom = MAX_Y;

    s_ScrFillRect(left, top, 1, bottom - top + 1, color, MONO_ROLE, FORE_LAYER);
    s_ScrFillRect(right, top, 1, bottom - top + 1, color, MONO_ROLE, FORE_LAYER);
    s_ScrFillRect(left, top, right - left+ 1, 1, color, MONO_ROLE, FORE_LAYER);
    s_ScrFillRect(left, bottom, right - left+ 1, 1,color, MONO_ROLE, FORE_LAYER);
}

/**************************************************************************************
函数原型	void ScrClrRect(int left,int top, int right,int bottom)
函数功能	黑白屏幕接口，清除矩形
参数说明	入参	left -- 左上角x坐标
                    top -- 左上角y坐标
                    right -- 右下角x坐标
                    bottom -- 右下角y坐标
			出参	无
返回值		无
补充说明	
***************************************************************************************/
void ScrClrRect(int left,int top, int right,int bottom)
{
	LCD_ATTR_T *ptLcdAttr =GetLcdInfo(MONO_ROLE);
	int MAX_X = ptLcdAttr->width;
	int MAX_Y = ptLcdAttr->height;

    if (left < 0) left = 0;
    if (top < 0) top = 0;

    if (right > MAX_X) right = MAX_X;
    if (bottom > MAX_Y) bottom = MAX_Y;

    if (right < left) return;
    if (bottom < top) return;
    
    s_ScrClrRect(left,  top, 1, bottom - top + 1, MONO_ROLE);
    s_ScrClrRect(right, top, 1, bottom - top + 1, MONO_ROLE);
    s_ScrClrRect(left, top, right - left+ 1, 1, MONO_ROLE);
    s_ScrClrRect(left, bottom, right - left+ 1, 1, MONO_ROLE);    
    s_ScrClrRect(left, top, right - left + 1, bottom - top + 1, MONO_ROLE);
}

void ScrTextOut(int x, int y, unsigned char *txt)
{
	LCD_ATTR_T *ptLcdAttr;
	int sumLen, totalLen;
	int sWidth, sHeight, mWidth, mHeight,width;

    if(x>= 0 && y>=0)
    {
        s_ScrTextOut(x, y, 0, 0, txt, MONO_ROLE);
        return ;
    }
    
    ptLcdAttr =GetLcdInfo(MONO_ROLE);
	s_GetLcdFontCharAttr(&sWidth, &sHeight, &mWidth, &mHeight);

	if (x < 0)
	{
		sumLen = -x;
		x = 0;
		totalLen = 0;

		while(*txt)
		{
			if (sumLen <= totalLen) break;

			if(*txt & 0x80)
			{
				txt += 2;
				totalLen += mWidth + ptLcdAttr->charSpace;
			}
			else if(*txt == '\r')
			{
				txt++;
			}
			else if(*txt == '\n')
			{
				txt++;
				totalLen += ptLcdAttr->width - (totalLen % ptLcdAttr->width);
			}
			else
			{
				txt++;
				totalLen += sWidth + ptLcdAttr->charSpace;
			}
            if(*txt & 0x80) width = mWidth;
            else width = sWidth;
            
			if(ptLcdAttr->width - (totalLen % ptLcdAttr->width) < width)
				totalLen += ptLcdAttr->width - (totalLen % ptLcdAttr->width);
		}
	}

	if(y < 0)
	{
		sumLen = (-y + (sHeight + ptLcdAttr->lineSpace - 1)) / (sHeight + ptLcdAttr->lineSpace); 
        sumLen = sumLen * ptLcdAttr->width - x;
		y = 0;
		totalLen = 0;
    
        while(*txt)
		{
			if(sumLen <= totalLen) break;
			if(*txt & 0x80)
			{
				txt += 2;
				totalLen += mWidth + ptLcdAttr->charSpace;
			}
			else if(*txt == '\r')
			{
				txt++;
			}
			else if(*txt == '\n')
			{
				txt++;
				totalLen += ptLcdAttr->width - (totalLen % ptLcdAttr->width);
			}
			else
			{
				txt++;
				totalLen += sWidth + ptLcdAttr->charSpace;
			}

            if(*txt & 0x80) width = mWidth;
            else width = sWidth;
            
			if(ptLcdAttr->width - (totalLen % ptLcdAttr->width) < width)
				totalLen += ptLcdAttr->width - (totalLen % ptLcdAttr->width);
		}
	}
    if(*txt == 0x00) return;
	s_ScrTextOut(x, y, 0, 0, txt, MONO_ROLE);
}

/**************************************************************************************
函数原型	int Lcdprintf(unsigned char *fmt,...)
函数功能	黑白屏幕接口，格式化字符串到屏幕
参数说明	入参	fmt -- 显示输出的字符串及格式
			出参	无
返回值		无
补充说明	
***************************************************************************************/
int Lcdprintf(unsigned char *fmt,...)
{
    int i;
    char sbuffer[1028];
    va_list varg;

    va_start( varg, fmt );
    i = vsnprintf(sbuffer, sizeof(sbuffer)-4,  fmt,  varg);
    va_end( varg );

    if(i == -1) i = sizeof(sbuffer) - 4;
	sbuffer[i]=0;

    s_ScrTextOut(-1, -1, 0, 1, sbuffer, MONO_ROLE); 
	return 0;
}

void s_Lcdprintf(unsigned char *str)
{
	s_ScrTextOut(-1, -1, 0, 1, str, MONO_ROLE); 
}

/**************************************************************************************************
函数原型	void ScrPrint(unsigned char col, unsigned char row, unsigned char mode, char *str, ...)
函数功能	黑白屏幕接口，格式化字符串到屏幕
参数说明	入参	col -- 列起始位置
                    row -- 行地址
                    mode -- Bit0:字体设置,0-ASCII,1-CFONT; 
                    Bit6: 对齐属性,0-左对齐,1-居中
                    Bit7: 显示属性,0-正常显示,1-反显
                    fmt -- 显示输出的字符串及格式
			出参	无
返回值		无
补充说明	
**************************************************************************************************/
void ScrPrint(unsigned char col, unsigned char row, unsigned char mode, char *str, ...)
{
    unsigned char attr, FontSize;
    va_list varg;
    int retv;
    char sbuffer[1028];

    va_start(varg, str);
    retv = vsnprintf(sbuffer, sizeof(sbuffer) - 4, str, varg);
    va_end(varg);

    if (retv == -1) retv = sizeof(sbuffer) - 4;
    sbuffer[retv] = 0;
    attr = ScrAttrSet(mode & 0x80);
    FontSize = ScrFontSet(mode & 0x01);

    if (mode & 0x20)  /* clear content of current line */
    {
		ScrClrLine(row, !mode ? row : row+1);
    }

	s_ScrTextOut(col, row * 8, mode & 0x40 ? 1 : 0, 0, sbuffer, MONO_ROLE);
 
    ScrAttrSet(attr);
    ScrFontSet(FontSize);
}

/* attention: This function is providing for app without supporting text alignment */
void s_ScrPrint(unsigned char col, unsigned char row, unsigned char mode, char *str)
{
    unsigned char attr, FontSize;

    attr = ScrAttrSet((unsigned char)(mode & 0x80));
    FontSize = ScrFontSet((unsigned char)(mode & 0x01));
    s_ScrTextOut(col, row * 8, 0, 0, str, MONO_ROLE);
    ScrAttrSet(attr);
    ScrFontSet(FontSize);
}

/* end of lcdapi.c */
