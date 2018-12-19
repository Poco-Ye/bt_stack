#ifndef _MODULE_MENU_H
#define _MODULE_MENU_H

//menu name buffer size 
#define MENU_MAX_NAME_SIZE   60

//menu max pages
#define MENU_MAX_PAGE 10

//menu font size
#define MENU_MT30_CFONT_HEIGHT  24
#define MENU_MT30_ASCII_HEIGHT   24
#define MENU_MT30_CFONT_WIDTH   24
#define MENU_MT30_ASCII_WIDTH 12

#define MENU_CFONT_HEIGHT  16
#define MENU_ASCII_HEIGHT   8
#define MENU_CFONT_WIDTH   16
#define MENU_SMALL_ASCII_WIDTH 6
#define MENU_BIG_ASCII_WIDTH 8

//menu attribute bit 9 & 8
#define MENU_LEFT_ALIGN       0x00 << 8
#define MENU_CENTER_ALIGN     0x01 << 8
#define MENU_RIGHT_ALIGN      0x02 << 8
#define MENU_LEFTRIGHT_ALIGN  0x03 << 8
#define MENU_ALIGN_MASK       0x03 << 8

//menu attribute bit 11
#define MENU_UPDATE    0x01 << 11

//menu attribute bit 10
#define MENU_HIDDEN    0x01 << 10

//menu attribute bit 13 & 12
#define MENU_MENUITEM  0x00 << 12
#define MENU_TEXT      0x01 << 12
#define MENU_INPUT     0x02 << 12
#define MENU_LOGO      0x03 << 12
#define MENU_ITEM_TYPE_MASK 0x03 << 12

//menu bit 1
#define MENU_CFONT 0x01 << 0

//men bit 8
#define MENU_REVERSE 0x01 << 8


typedef struct
{
	int id;
	int ucX; //此参数与ScrGotoxy()函数参数X意义与取值范围一致
	int ucY; //此参数与ScrGotoxy()函数参数Y意义与取值范围一致
	unsigned int uiAttr;
	char  acChnName[MENU_MAX_NAME_SIZE];
	char  acEngName[MENU_MAX_NAME_SIZE];
	void  (*pFun)();
}Menu;

/*
typedef struct
{
	int id;
	MenuItem *pMenuitem;
	int iItemNum;
	unsigned char ucX; //此参数与ScrGotoxy()函数参数X意义与取值范围一致
	unsigned char ucY; //此参数与ScrGotoxy()函数参数Y意义与取值范围一致
	unsigned int uiAttr;
	char acChnTitle[MAX_NAME_SIZE];
	char acEngTitle[MAX_NAME_SIZE];
	void  (*pFun)();
only used for system
	char buff[3][LCD_X_COL * LCD_Y_LINE];
}Menu; */

int iShowMenu(Menu menu[], int totalitem, int delayms, int *selecteditem);
static int ShowMenuItem(int x, int y, int mode, char *ChnName, char *EngName, int index);
static int ShowMenuText(int x, int y, int mode, char *ChnName, char *EngName);

#endif
