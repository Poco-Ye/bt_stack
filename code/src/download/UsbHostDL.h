
#ifndef _USBHOSTDL_
#define _USBHOSTDL_

#include "base.h"
#include "posapi.h"
#include "..\file\filedef.h"
#include "..\download\localdownload.h"
#include "..\PUK\puk.h"


enum FILE_TYPE{
MONITOR_TYPE =1,
FONT_TYPE,
USPUK_TYPE,
UAPUK_TYPE,
APPCK_TYPE,//应用包
PUBF_TYPE,//公有文件
SOF_TYPE,//公有库
PRG_TYPE,//程序包文件
BASEDRIVER_TYPE,//座机固件
};

enum TERM_TYPE
{
	TERML_P70_S=1,
	TERML_P60_S1,
	TERML_P60_S1_BASE,
	TERML_P80,
	TERML_P90,
	TERML_P78,
	TERML_S80,
	TERML_SP30,
	TERML_S90,
	TERML_R50,
	TERML_S60,
	TERML_S60_BASE,
	TERML_S300,
	TERML_S800,
	TERML_S900
};

typedef struct
{
	uchar ucFileType;
	uchar szFileName[17];//显示的字符,记录文件名，比如公有文件名
	uint uiFileAddr;
	uint uiFileLen;
}TERM_DESCRIB_TABLE;

#define PRG_FILE_STATIC_BIN     1
#define PRG_FILE_DYNAMIC_BIN    2
#define PRG_FILE_DYNAMIC_SO     3

#define MAX_MON_LEN    3*1024*1024    // Monitor程序最大3M


#define ERR_LOG_FILE "udisodload_err"

#endif


