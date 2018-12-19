/*****************************************************/
/* userdef.h                                         */
/* For file system base on flash chip                */
/* For all PAX POS terminals     		             */
/* Created by ZengYun at 20/11/2003                  */
/*
2005年3月18日：
为了扩充监控程序空间从304K扩到432K，须将文件系统向后推一个128K空间，也即文件系统
会减少一个128K空间。
同时须修改FILEDEF－C中的DATA＿SECTOR值，将其值减2。

*/

/*****************************************************/

#ifndef _FILEDEF_H
#define _FILEDEF_H

#include "posapi.h"

//For debug
//#define _DEBUG_F

extern unsigned long   DATA_SECTORS;
extern unsigned long BLOCK_PAGES	;			  //每个数据扇区包含的数据块数
extern unsigned long LOG_SIZE 	;				  //日志大小

extern unsigned long FAT_SECTOR_SIZE	;			      //FAT表所在扇区的大小
extern unsigned long LOG_SECTOR_SIZE   ;                 //日志表所在扇区的大小
extern unsigned long	DATA_SECTOR_SIZE    ;               //数据扇区的大小

extern unsigned long	FAT1_ADDR;	  //FAT1表的地址
extern unsigned long	FAT2_ADDR;	  //FAT2表的地址
extern unsigned long	LOG_ADDR;     //日志表的地址
extern unsigned long	DATA_ADDR;     //数据扇区的起始地址
extern ushort g_LogSector[0x10000]; /*备份日子扇区*/


#define     LOG_END_ADDR   (LOG_ADDR+LOG_SIZE-0x40)  /*防止写越界*/
#define     FAT1_LOG_ADDR       (LOG_ADDR+2)  // 2Bytes
#define     FAT2_LOG_ADDR       (LOG_ADDR+4)  // 2Bytes
#define     INIT_LOG_ADDR       (LOG_ADDR+6)   // 8Bytes

#define     FAT_VERSION_FLAG    "FFAT_V20"
#define     LOG_VERSION_FLAG    "FLOG_V20"



#define MAX_FILES        256                      //最大文件数

#define MAX_APP_LEN		2048*1024 //2M
#define MAX_PARAM_LEN	512*3*1024//1.5M
//注释：
//如果FAT扇区太小,空间不够的情况下,MAX_FILES可能会被系统重定义,
//所以实际的最大文件数可能小于你的定义值

//#define for file attr
#define FILE_ATTR_MON       0XFF
/*attr 0...23 : APP0...APP23*/
#define FILE_ATTR_PUBSO   0xE0
#define FILE_ATTR_PUBFILE 0xE1


//define for file type
#define	FILE_TYPE_MAPP	    0//用程序管理器
#define	FILE_TYPE_APP	    1//应用程序
#define	FILE_TYPE_FONT	    2//字库
#define	FILE_TYPE_LIBFUN	3//库函数
#define	FILE_TYPE_USER_FILE	4//用户文件
#define	FILE_TYPE_SYS_PARA	5//系统参数文件
#define	FILE_TYPE_APP_PARA	6//应用参数文件
#define	FILE_TYPE_PED		8//PED文件
#define	FILE_TYPE_PUK		9//用户公钥文件
#define FILE_TYPE_USERSO	10 //用户动态库
#define FILE_TYPE_SYSTEMSO  11 //系统动态库
//----define file convert return value

#define FTO_RET_OK					0
#define FTO_RET_APPINFO_ERR			-1
#define FTO_RET_FILE_NO_EXIST		-2
#define FTO_RET_SIG_ERR				-3
#define FTO_RET_TOOMANY_APP			-4
#define FTO_RET_NAME_DEUPT			-5
#define FTO_RET_APP_TYPE_ERR		-6
#define FTO_RET_WRITE_FILE_ERR		-7
#define FTO_RET_READ_FILE_ERR		-8
#define FTO_RET_WITHOUT_APP_NAME	-9
#define FTO_RET_TOOMANY_FILE		-10
#define FTO_RET_NO_APP				-11
#define FTO_RET_PARAM_ERR			-12
#define FTO_RET_FONT_ERR			-13
#define FTO_RET_FILE_TOOBIG			-14
#define FTO_RET_NO_SPACE			-15
#define FTO_RET_NO_BASE				-16			// 没有座机
#define FTO_RET_OFF_BASE			-17			// 座机不在位

// -- FOR SO & Public file
#define FTO_RET_SOINFO_ERR			-20
#define	FTO_RET_PARAMETER_INVALID	-21
#define FTO_RET_PARAMETER_NULL		-22
#define FTO_RET_FILE_HAS_EXISTED	-23
#define FTO_RET_TOO_MANY_HANDLES	-24

//--------------

#endif
