#ifndef __LINK_INFO_H__
#define __LINK_INFO_H__


#include "elf.h"

#define APP_TYPE_MAGIC 	"PAXMEAPP"
#define SONAME_MAX_LEN	64
#define STALIB_MAX_NUM	10
#define SO_MAX_NUM      255

#define FIRMWARE_SO_NAME        "PAXBASE"

#define LIB_TYPE_SYSTEM    0x01
#define LIB_TYPE_USER      0x02

typedef struct 
{
    char name[32];
}LIBINFO_T;

typedef struct 
{
    char  so_name[64];  //so 文件名
    uchar version[8];   //版本号
    uchar date[20];     //编译时间
    uint stalib_sum;    //静态库个数
    uint text_sec_addr;
    uint *init_func;                //初始化
    uint lib_type;			//LIB_TYPE_SYSTEM or LIB_TYPE_USER
    ushort security_level;
    ushort reserve;
    uint RFU[7];      //保留
}SO_HEAD_INFO;

typedef struct 
{
    SO_HEAD_INFO  head;                  //保留
    LIBINFO_T	 sublib_info[10];   //静态库版本号和名称
}SO_INFO;

typedef struct _PUBFILE_INFO{
    uchar name[17];
    uchar len[4];
}__attribute__ ((packed))PUBFILE_INFO;

#define SOINFO_OFFSET	0x200

#endif
