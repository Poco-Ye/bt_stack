#ifndef _LOCALDOWNLOAD_H_
#define _LOCALDOWNLOAD_H_	

#include "base.h"
#define IS_SO_SUPPORTED     1
#define	PROTOCOL_VERSION	11
#define	LD_MAX_RECV_DATA_LEN	8192
#define	LD_MAX_BAUDRATE	230400

#define    DEFAULT_BAUDRATE 9600
#define	   COMATTR	",8,N,1"

//define data 
#define	SHAKE_REQUEST		'Q'
#define	SHAKE_REPLY			'S'	
#define	SHAKE_REPLY_H		'H'	
//#define	ACK		0x06
#define	STX		0x02

//define cmd list 
#define LD_CMD_TYPE             0x80
#define LD_CMDCODE_SETBAUTRATE  0xA1
#define LD_CMDCODE_GETPOSINFO   0xA2
#define LD_CMDCODE_GETAPPSINFO  0xA3
#define LD_CMDCODE_REBUILDFS    0xA4
#define LD_CMDCODE_SETTIME      0xA5
#define LD_CMDCODE_DLUPK        0xA6
#define LD_CMDCODE_DLAPP        0xA7
#define LD_CMDCODE_DLFONTLIB    0xA8
#define LD_CMDCODE_DLPARA       0xA9
//#define LD_CMDCODE_DLDMR      0xAA
#define LD_CMDCODE_DLFILEDATA   0xAB          // download file data in normal mode.
#define LD_CMDCODE_DLFILEDATAC  0xAC         // download file data in compress mode.
#define LD_CMDCODE_WRITEFILE    0xAD
#define LD_CMDCODE_DELAPP       0xAE          // delete application
#define LD_CMDCODE_DELALLAPP	0xAF		//del all app

#define LD_CMDCODE_DLMAGDRIVER  0xB4//download Magcard Driver
#define LD_CMDCODE_DLPAUSE		0xBE
#define LD_CMDCODE_DLCOMPLETE   0xBF
#define LD_CMDCODE_GETCONFTABLE	0XC1//read config tab
#define LD_CMDCODE_WRITECSN	    0XC4//write CSN
#define LD_CMDCODE_SHUTDOWN	    0xC6

#define LD_CMDCODE_DLPUBFILE    0xB0
#define LD_CMDCODE_GETSOINFO    0xB1
#define LD_CMDCODE_GET_ALL_SONAME  0xB2
#define LD_CMDCODE_DELSO	    0xB3
#define LD_CMDCODE_GETPUBFILE   0xB5
#define LD_CMDCODE_DELPUBFILE   0xB6//for duplicate 
#define LD_CMDCODE_GETCFGINFO   0xB7
#define LD_CMDCODE_SETCFGINFO   0xB8

#define LD_CMDCODE_SHUTDOWN	    0xC6
#define LD_CMDCODE_GETSNKEYINFO	0xC7
#define LD_CMDCODE_SETSNKEYSTATE 0xC8
#define LD_CMDCODE_SETTUSNFLAG  0xCF

//define	reply code to PC Tools
typedef enum
{
    LD_OK              = 0x00 ,
    LDERR_GENERIC      = 0x01 ,
    LDERR_HAVEMOREDATA = 0x02 ,
    LDERR_UNSUPPORTEDBAUDRATE = 0x03 ,
    LDERR_INVALIDTIME  = 0x04 ,
    LDERR_CLOCKHWERR   = 0x05 ,
    LDERR_INVALIDSIGNATURE      = 0x06 ,
	LDERR_TOOMANYAPPLICATIONS   = 0x07 ,
    LDERR_TOOMANYFILES          = 0x08 ,
    LDERR_APPLCIATIONNOTEXIST   = 0x09 ,
    LDERR_UNKNOWNAPPTYPE	    =0x0A,
	LDERR_SIGTYPEERR        =0x0B,
	LDERR_SIGAPPERR	        =0x0C,
	LDERR_REPEAT_APPNAME    = 0x0D,//下载的子应用与主应用重名或者下载的主应用与子应用重名
    LDERR_WRITEFILEFAIL     = 0x10 ,
    LDERR_NOSUFFICIENTSPACE = 0x11 ,
    LDERR_NOPERMIT			= 0x12 , // 系统动态库，不允许删除
    LDERR_BOOT_TOO_OLD      = 0x13 , // BOOT版本太低
    LDERR_TIMEOUT           = 0xFE ,
    LDERR_UNSUPPORTEDCMD    = 0xFF ,
	LDERR_DECOMPRESS_ERR	=0XA0,
	LDERR_APP_TOO_BIG		=0XA1,
	
	LDERR_FWVERSION			=0x14,		// 下载的固件版本不正确
} LDERR;

#define FONT_FILE_NAME          FONT_NAME_S80
#define MAX_APP_NUM             APP_MAX_NUM
#define APP_FILE_NAME_PREFIX    APP_NAME
#define CONFIG_TAB_LEN          8
#define APP_ENTRY_OFFSET        0

#define	LD_PUB_SO	    0x10
#define LD_PUB_USERFILE 0X0F

#define LD_PARA_SO      0X0F

#endif
