#include <string.h>
#include <Posapi.h>
#include <base.h>
#include "ModuleMenu.h"
extern unsigned char bIsChnFont; 

/*函数功能说明：显示一个菜单，此菜单包括菜单标题和菜单项。使用此函数，当页数超过一页时，会自动点亮上下翻页的箭头图标。如果未超过一页，则自动关闭箭头图标
参数说明：
in   menu   指向菜单的指针
in   
返回值： 。
举例：S78主菜单有三页，则返回值为3。 SP30主菜单有4页，则返回4。*/

int iShowMenu(Menu menu[], int totalitem, int delayms, int *selecteditem)
{
	int iLoop, jLoop;
	int iCurPage, iCurLine;
	int PAGELINE;
	int OFFSET;
    int iTotLine;
	int iTotPage;
	//记录每一页菜单之后多少个文本项
	int PAGETEXT;
	//记录每一页多少个菜单项
	int PAGEITEM;

	//记录此页面之前有多少项
	int iItemsPage;
	
	//记录是否存在logo项
    int bIsLogo;
	
	//记录是否存在输入项
	int bIsInput;
	unsigned char MODE;
	int iFontWidth, iFontHeight;
	int iLcdLineSpace, iLcdCharSpace;
	int iLcdWidth, iLcdHeight;
	int iLcdCharsPerLine, iLcdLinesPerScr;
	unsigned char ucKey, inputKeyNumbers;
	int bMenuFlag;
	int tmpmode;
	int tmp;
	int page[MENU_MAX_PAGE];
	int iLeftHeightPixel;
	unsigned char out_buf[30];
	//用来接收键值
    unsigned char KeyBuff[10];
   	//set var 0
	PAGETEXT = 0;
	PAGEITEM = 0;
	bIsLogo = 0;
	bIsInput = 0;
	iLcdLinesPerScr = 0;
	tmp = 0;
	iItemsPage = 0;
	memset ( page , 0, sizeof(page) );
	inputKeyNumbers = 0;

	// set var 1
	iCurPage = 0;
	//get totol lines
	iTotLine = totalitem;

    //count lines per screen13600094288
	ScrGetLcdSize( &iLcdWidth, &iLcdHeight);
	memset(out_buf, 0, sizeof(out_buf));
	GetTermInfo(out_buf);
	
    //leftpixel
	iLeftHeightPixel = iLcdHeight;
	if(iLcdHeight != 272)//非MT30
	{
		ScrFontSet( 1 );
	}
	if(bIsChnFont)
	{
		if ( 0 != strlen(menu[0].acChnName) ) 
		{
			bMenuFlag = 1;
		}
		else
		{
			bMenuFlag = 0;
		}
	}
	else
	{
		if ( 0 != strlen(menu[0].acEngName) )
		{
			bMenuFlag = 1;
		}
		else
		{
			bMenuFlag = 0;
		}
	}
	if (bMenuFlag)
	{
		if(iLcdHeight == 272)//MT30
		{
			if ( MENU_CFONT == (MENU_CFONT & menu[0].uiAttr) ) 
			{
				iLeftHeightPixel -= MENU_MT30_CFONT_HEIGHT;
			}
			else
			{
				iLeftHeightPixel -= MENU_MT30_ASCII_HEIGHT;
			}
		}
		else
		{
			if ( MENU_CFONT == (MENU_CFONT & menu[0].uiAttr) ) 
			{
				iLeftHeightPixel -= MENU_CFONT_HEIGHT;
			}
			else
			{
				iLeftHeightPixel -= MENU_ASCII_HEIGHT;
			}
		}
	}
	for ( iLoop = 1; iLoop < iTotLine; iLoop++)
	{
		//ScrPrint(0,0,0,"%d %d %d",iLeftHeightPixel,iLcdLinesPerScr,tmp);
		//while(kbhit()!=0);
		//kbflush();
		//ScrCls();
		if ( MENU_CFONT == (MENU_CFONT & menu[iLoop].uiAttr) ) 
		{
			if(iLcdHeight == 272)//MT30
			{
				iLeftHeightPixel -= MENU_MT30_CFONT_HEIGHT;
			}
			else
			{
				iLeftHeightPixel -= MENU_CFONT_HEIGHT;
			}
			if (iLeftHeightPixel >= 0)
			{
				iLcdLinesPerScr++;
			}
		}
		else
		{
			if(iLcdHeight == 272)//MT30
			{
				iLeftHeightPixel -= MENU_MT30_ASCII_HEIGHT;
			}
			else
			{
				iLeftHeightPixel -= MENU_ASCII_HEIGHT;
			}
			if (iLeftHeightPixel >= 0)
			{
				iLcdLinesPerScr++;
			}
		}
		if (0 >= iLeftHeightPixel)
		{
        	page[tmp] = iLcdLinesPerScr;
			tmp++;
			if ( ( iLoop == (iTotLine - 1) ) && (0 == iLeftHeightPixel) )
			{
					//ScrPrint(0,5,0,"12%d",tmp);
					//while(1);
				tmp--;
			}
			if (bMenuFlag)
			{
				if ( MENU_CFONT == (MENU_CFONT & menu[0].uiAttr) ) 
				{
					if(iLcdHeight == 272)//MT30
					{
						iLeftHeightPixel = iLcdHeight - MENU_MT30_CFONT_HEIGHT;
					}
					else
					{
						iLeftHeightPixel = iLcdHeight - MENU_CFONT_HEIGHT;
					}
				}
				else
				{
					if(iLcdHeight == 272)//MT30
					{
						iLeftHeightPixel = iLcdHeight - MENU_MT30_ASCII_HEIGHT;
					}
					else
					{
						iLeftHeightPixel = iLcdHeight - MENU_ASCII_HEIGHT;
					}
				}
			}
			else
			{
				iLeftHeightPixel = iLcdHeight;
			}
			iLcdLinesPerScr = 0;

	
		}
		//当不足新的一页时而剩余像素点时，也当然此页的行数填入
		else
		{
			//ScrPrint(49,0,0,"hi");
			if ( iLoop == (iTotLine - 1) )
			{
				page[tmp] = iLcdLinesPerScr;
			}
		}
	}
	iTotPage = tmp;
	//ScrCls();
	//ScrPrint(0,4,0,"34%d",tmp);
	//while(1);
	kbflush();
	while(1)
	{
		OFFSET = 0;
		PAGEITEM = 0;
        iItemsPage = 0;
		ScrCls();

		if(bIsChnFont)
		{
			if (bMenuFlag) 
			{
				if ( 0 == (MENU_HIDDEN & menu[0].uiAttr) )
				{
					ScrPrint( 0, 0, (unsigned char)menu[0].uiAttr | 0x0040 | MENU_REVERSE, menu[0].acChnName );
				}
				OFFSET = ( menu[0].uiAttr & MENU_CFONT )? 2: 1;
				PAGELINE = iLcdLinesPerScr - 1;
				if (iTotPage > 0)
				{
					ScrPrint( iLcdWidth - 6, 0, (unsigned char)(menu[0].uiAttr & 0xbe), "%d", iCurPage + 1);
				}
				if(iLcdHeight != 272)//非MT30
				{
					ScrFontSet( 1 );
				}
			}
			else
			{
				iTotLine = totalitem - 1;
				OFFSET = 0;
				bMenuFlag = 0;
				PAGELINE = iLcdLinesPerScr;
			}
	    }
		else
		{
			if (bMenuFlag)
			{
				if ( 0 == (MENU_HIDDEN & menu[0].uiAttr) )
				{
					ScrPrint( 0, 0, (unsigned char)menu[0].uiAttr | 0x0040 | MENU_REVERSE, menu[0].acEngName );
				}
				OFFSET = ( menu[0].uiAttr & MENU_CFONT )? 2: 1;
				PAGELINE = iLcdLinesPerScr - 1;
				if (iTotPage > 0)
				{
					ScrPrint( iLcdWidth - 6, 0, (unsigned char)(menu[0].uiAttr & 0xbe), "%d", iCurPage + 1);
				}
			}
			else
			{
				iTotLine = totalitem - 1;
				bMenuFlag = 0;
				OFFSET = 0;
				PAGELINE = iLcdLinesPerScr;
			}
		}
		for (jLoop = 0; jLoop < iCurPage; jLoop++)
		{
			iItemsPage += page[jLoop];
		}
		for (iLoop = 0; iLoop < page[iCurPage]; iLoop++)
		{

			/*判断各个项的类型*/
			//如果是子菜单项
			if ( MENU_MENUITEM == ( menu[iItemsPage + iLoop + 1].uiAttr & MENU_ITEM_TYPE_MASK) )
			{
				PAGEITEM++;
				//ScrPrint(0,6,0,"hiyun%d",page[iCurPage]);
				//while(1);
				tmp = ShowMenuItem( 0, OFFSET, 
							        menu[iItemsPage + iLoop + 1].uiAttr, 
							  		menu[iItemsPage + iLoop + 1].acChnName, 
							  		menu[iItemsPage + iLoop + 1].acEngName, PAGEITEM );
				OFFSET += tmp;
			}
			else if ( MENU_TEXT == (menu[iItemsPage + iLoop + 1].uiAttr & MENU_ITEM_TYPE_MASK) )
			{
			//	ScrPrint(0,7,0,"hello%d",OFFSET);
				//while(1);
				tmp = ShowMenuText( 0, OFFSET, 
									menu[iItemsPage + iLoop + 1].uiAttr, 
							  		menu[iItemsPage + iLoop + 1].acChnName, 
							  		menu[iItemsPage + iLoop + 1].acEngName );
				OFFSET += tmp;
				PAGETEXT++;

			}
			else if ( MENU_LOGO == (menu[iItemsPage + iLoop + 1].uiAttr & MENU_ITEM_TYPE_MASK) )
			{
				//ShowMenuLogo();
			}
			else
			{
				bIsInput = iItemsPage + iLoop + 1;
				tmp = ShowMenuText( 0, OFFSET, 
									menu[iItemsPage + iLoop + 1].uiAttr, 
							  		menu[iItemsPage + iLoop + 1].acChnName, 
							  		menu[iItemsPage + iLoop + 1].acEngName );
				OFFSET += tmp;
			}
		}
		if(iLcdHeight != 272)//非MT30
		{
			/*处理向上向下箭头*/
			ScrSetIcon(ICON_UP, CLOSEICON);
			ScrSetIcon(ICON_DOWN, CLOSEICON);	
			if (0 != iCurPage)
			{
				if (0 != page[iCurPage - 1])
				{
					ScrSetIcon( ICON_UP, OPENICON );
				}
			}
	  	  	if (iTotPage != iCurPage)	
			{
				if (0 != page[iCurPage + 1])
				{
					ScrSetIcon( ICON_DOWN, OPENICON );
				}
			}
		}
        /*   处理按键消息	*/
		while (2)
		{
			//这里时空闲时需要检测的工作
		/*  if (!s_ulSofTimerCheck( TM_DISPLAY_MENU ) )
			{
				return -6;
			} */
			if(selecteditem == NULL)//该菜单不需等待按键，显示后直接返回
			{
				return;
			}
			if ( 0 != kbhit() ) 
			{
				continue;
			}
			ucKey = getkey();
  			if (KEYFN == ucKey)
			{
				break;
			}
			else if (KEYUP == ucKey || KEYF1 == ucKey)
			{
				if (0 == iCurPage)
				{
					iCurPage = iTotPage;
				}
			    else	
				{
					iCurPage--;
				}
				PAGEITEM = 0;
				PAGETEXT = 0;
				break;
			}
			else if (KEYDOWN == ucKey || KEYF2 == ucKey)
			{
				if (iTotPage == iCurPage)
				{
					iCurPage = 0;
				}
				else
				{
					iCurPage ++;
				}
				PAGEITEM = 0;
				PAGETEXT = 0;
				break;				
			}
            else if (KEYCANCEL == ucKey)
			{
				if(iLcdHeight != 272)//非MT30
				{
					//退出菜单时需要做的清理
					ScrSetIcon(ICON_UP, CLOSEICON);
					ScrSetIcon(ICON_DOWN, CLOSEICON);
				}
				return KEYCANCEL;
			}
			else if (KEYENTER == ucKey)
			{
				if (bIsInput)
				{
					if (bIsChnFont)
					{
						if(strlen(menu[bIsInput].acChnName) - 1 > 10)
						{
							*selecteditem = -1;//防止输入的字符串过长
						}
						else
						{
							*selecteditem = atoi( menu[bIsInput].acChnName );
						}
					}
					else
					{
						if(strlen(menu[bIsInput].acEngName) - 1 > 10)
						{
							*selecteditem = -1;//防止输入的字符串过长
						}
						else
						{
							*selecteditem = atoi( menu[bIsInput].acEngName );
						}
					}
					menu[bIsInput].acChnName[0] = '_';
					menu[bIsInput].acChnName[1] = '\0';
					menu[bIsInput].acEngName[0] = '_';
					menu[bIsInput].acEngName[1] = '\0';

					return;
				}
				if((iLcdHeight == 272 || out_buf[0] == 0x86) && (bIsInput == 0))//MT30或T60
				{
					if (iTotPage == iCurPage)
					{
						iCurPage = 0;
					}
					else
					{
						iCurPage ++;
					}
					PAGEITEM = 0;
					PAGETEXT = 0;
				}
				break;
			}
			else if (KEYCLEAR == ucKey)
			{
				if (bIsInput)
				{
					inputKeyNumbers = 0;
					menu[bIsInput].acChnName[0] = '_';
					menu[bIsInput].acChnName[1] = '\0';
					menu[bIsInput].acEngName[0] = '_';
					menu[bIsInput].acEngName[1] = '\0';
				}
				if((iLcdHeight == 272 || out_buf[0] == 0x86) && (bIsInput == 0))//MT30或T60
				{
					if (0 == iCurPage)
					{
						iCurPage = iTotPage;
					}
			    	else	
					{
						iCurPage--;
					}
					PAGEITEM = 0;
					PAGETEXT = 0;
				}
				break;
			}
			else if (KEYMENU == ucKey)
			{
				break;
			}
			else if (KEY0 <= ucKey && ucKey <= KEY9)
			{
				if (!bIsInput && !bIsLogo)
				{
					if ( (ucKey > '0') && (ucKey <= '0' + PAGEITEM) )
					{
				    	ScrSetIcon(ICON_UP, CLOSEICON);
						ScrSetIcon(ICON_DOWN, CLOSEICON);
						*selecteditem = iItemsPage + ucKey- '0' + 1;
						menu[iItemsPage + ucKey - '0' + PAGETEXT].pFun();
						kbflush();
						if(iLcdHeight != 272)//非MT30
						{
							ScrFontSet( 1 );
						}

						break;
					}
					else
					{
						break;
					}
				}
				else if (bIsInput)
				{
					inputKeyNumbers++;
					menu[bIsInput].acChnName[inputKeyNumbers] = '_';
					menu[bIsInput].acChnName[inputKeyNumbers - 1] = ucKey; 
					menu[bIsInput].acChnName[inputKeyNumbers + 1] = '\0'; 
					menu[bIsInput].acEngName[inputKeyNumbers] = '_';
					menu[bIsInput].acEngName[inputKeyNumbers - 1] = ucKey; 
					menu[bIsInput].acEngName[inputKeyNumbers + 1] = '\0'; 
					break;
					/*while (KEY0 <= inputKey && inputKey <= KEY9)
					{
						jLoop = 0;
						KeyBuff[jLoop] = inputKey;
						jLoop++;
						inputKey = getkey();
					}				
					if (KEYENTER == inputKey)
					{
						
					}*/	
				}
				else
				{
					break;
				}	
			}
			else
			{
				break;
			}		
		}

	}
}

static int ShowMenuItem(int x, int y, int mode, char *ChnName, char *EngName, int index)
{
	int LcdWidth, LcdHeight;
	int MaxCharsPerLine;
	int lineuse;
	lineuse = 0;
	//get lcd width & height
	ScrGetLcdSize( &LcdWidth, &LcdHeight );

	if (bIsChnFont)
	{
		if(LcdHeight == 272)//MT30
		{
			MaxCharsPerLine = LcdWidth / MENU_MT30_CFONT_WIDTH;
		}
		else
		{
			MaxCharsPerLine = LcdWidth / MENU_CFONT_WIDTH;
		}
		
		ChnName[MaxCharsPerLine * 2] = '\0';
		if ( 0 == (MENU_HIDDEN & mode) )
		{
			ScrPrint( x, y, (unsigned char)mode, "%d-%s", index, ChnName);
		}
		lineuse = 2;
	}
	else
	{
		if(LcdHeight != 272)//非MT30
		{
			if ( MENU_CFONT == (mode & MENU_CFONT) )
			{
				MaxCharsPerLine = LcdWidth / MENU_BIG_ASCII_WIDTH;
				EngName[MaxCharsPerLine] = '\0';
				if ( 0 == (MENU_HIDDEN & mode) )
				{
					ScrPrint( x, y, (unsigned char)mode, "%d-%s", index, EngName);
				}
				lineuse = 2;
			}
			else
			{
				MaxCharsPerLine = LcdWidth / MENU_SMALL_ASCII_WIDTH;
				EngName[MaxCharsPerLine] = '\0';
				if ( 0 == (MENU_HIDDEN & mode) )
				{
					ScrPrint( x, y, (unsigned char)mode, "%d - %s", index, EngName);
				}
				lineuse = 1;
			}
		}
		else
		{
			MaxCharsPerLine = LcdWidth / MENU_MT30_ASCII_WIDTH;
			EngName[MaxCharsPerLine] = '\0';
			if ( 0 == (MENU_HIDDEN & mode) )
			{
				ScrPrint( x, y, (unsigned char)mode, "%d-%s", index, EngName);
			}
			lineuse = 2;
		}
		
	}
	return lineuse;
}

static int ShowMenuText(int x, int y, int mode, char *ChnName, char *EngName)
{
	char *str_pos = NULL;
	int tmp = 0;
	int tmpmode, tmpmodefont, tmpmodehidden;
	int LcdWidth, LcdHeight;
    int MaxCharsPerLine;
	int lineused;
	
	//get align attr
	tmpmode = mode & MENU_ALIGN_MASK;
    //get cfont attr
	tmpmodefont = mode & MENU_CFONT;
	//get hidden attr
	tmpmodehidden = mode & MENU_HIDDEN;

	//get lcd width & height
	ScrGetLcdSize( &LcdWidth, &LcdHeight );

	if(LcdHeight != 272)//非MT30
	{
		if (MENU_CFONT == tmpmodefont)
		{
			MaxCharsPerLine = LcdWidth / MENU_BIG_ASCII_WIDTH;
			lineused = 2;
		}
		else
		{
			MaxCharsPerLine = LcdWidth / MENU_SMALL_ASCII_WIDTH;
			lineused = 1;
		}
	}
	else
	{
		MaxCharsPerLine = LcdWidth / MENU_MT30_ASCII_WIDTH;
		lineused = 2;
	}
	ChnName[MaxCharsPerLine] = '\0';
	EngName[MaxCharsPerLine] = '\0';

	if (MENU_LEFT_ALIGN == tmpmode)
	{
		if (0 == tmpmodehidden)
		{
			if (bIsChnFont)
			{
				ScrPrint( x, y, (unsigned char)mode, "%s", ChnName );
			}
			else
			{
				ScrPrint( x, y, (unsigned char)mode, "%s", EngName );
			}
		}
	}
	else if (MENU_RIGHT_ALIGN == tmpmode)
	{
        if (0 == tmpmodehidden)
		{	
			if (bIsChnFont)
			{
				if(LcdHeight != 272) //非MT30
				{
					if (MENU_CFONT == tmpmodefont)	
					{
						x = LcdWidth - 1 - strlen( ChnName ) * MENU_CFONT_WIDTH / 2;
					}
					else
					{
						x = LcdWidth - 1 - strlen( ChnName ) * MENU_SMALL_ASCII_WIDTH;
					}
				}
				else
				{
					x = LcdWidth - 1 - strlen( ChnName ) * MENU_MT30_CFONT_WIDTH / 2;
				}
				ScrPrint( x, y, (unsigned char)mode, "%s", ChnName );
			}
			else
			{
				if(LcdHeight != 272) //非MT30
				{
					if (MENU_CFONT == tmpmodefont)	
					{
						x = LcdWidth - 1 - strlen( EngName ) * MENU_BIG_ASCII_WIDTH;
					}
					else
					{
						x = LcdWidth - 1 - strlen( EngName ) * MENU_SMALL_ASCII_WIDTH;
					}
				}
				else
				{
					x = LcdWidth - 1 - strlen( EngName ) * MENU_MT30_ASCII_WIDTH;
				}
				ScrPrint( x, y, (unsigned char)mode, "%s", EngName );
			}
		}
	}
	else if ( MENU_CENTER_ALIGN == tmpmode )
	{
		mode |= 0x40;
        if (0 == tmpmodehidden)
		{
			if (bIsChnFont)
			{
				ScrPrint( x, y, (unsigned char)mode, "%s", ChnName );
			}
			else
			{
				ScrPrint( x, y, (unsigned char)mode, "%s", EngName);
			}
		}
	}
	else
	{
		if( 0 ==tmpmodehidden)
		{
			if (bIsChnFont)
			{
				str_pos = strchr( ChnName, '\x1c');
				if (NULL == str_pos)
				{
					//mode |= 0x40;
					ScrPrint( x, y, (unsigned char)mode, "%s", ChnName );
				}
				else
				{
					//首先处理靠左对齐
					tmp = strlen(ChnName) - strlen( str_pos ) ;
					ChnName[tmp] = '\0';
					ScrPrint( x, y, (unsigned char)mode, "%s", ChnName );
					ChnName[tmp] = '\x1c';
					//再处理靠右对齐
					if(LcdHeight != 272) //非MT30
					{
						if (MENU_CFONT == tmpmodefont)	
						{
							x = LcdWidth - 1 - (strlen(str_pos) * MENU_BIG_ASCII_WIDTH);
						}
						else
						{
							x = LcdWidth - 1 - (strlen( str_pos ) * MENU_SMALL_ASCII_WIDTH);
						}
					}
					else
					{
						x = LcdWidth - 1 - (strlen(str_pos) * MENU_MT30_ASCII_WIDTH);
					}
					ScrPrint( x, y, (unsigned char)mode, "%s", &ChnName[tmp+1]);
				}		
			}
			else
			{
				str_pos = strchr( EngName, '\x1c' );
				if (NULL == str_pos)
				{
					//mode |= 0x40;
					ScrPrint( x, y, (unsigned char)mode, "%s", EngName );
				}
				else
				{
					//首先处理靠左对齐
					tmp = strlen(EngName) - strlen(str_pos);
					EngName[tmp] = '\0';
					ScrPrint( x, y, (unsigned char)mode, "%s", EngName );
					EngName[tmp] = '\x1c';

					//再处理靠右对齐
					if(LcdHeight != 272) //非MT30
					{
						if (MENU_CFONT == tmpmodefont)	
						{
							x = LcdWidth - 1 - (strlen( str_pos ) * MENU_BIG_ASCII_WIDTH);
						}
						else
						{
							x = LcdWidth - 1 - (strlen( str_pos ) * MENU_SMALL_ASCII_WIDTH);
						}
					}
					else
					{
						x = LcdWidth - 1 - (strlen( str_pos ) * MENU_MT30_ASCII_WIDTH);
					}
					ScrPrint( x, y, (unsigned char)mode, "%s", &EngName[tmp+1] );
				}
			}	
		}
	}

	return lineused;
}
