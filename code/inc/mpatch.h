#ifndef __MPATCH_H__
#define __MPATCH_H__

//common functype
#define MPATCH_COMMON_FUNC     	0
//common funcno
#define MPATCH_INIT_NO      	0

//wifi functype
#define MPATCH_WIFI_FUNC			1
//wifi funcno
#define MPATCH_WIFI_UPDATE_FW		0
#define MPATCH_WIFI_GET_TADM2		1
#define MPATCH_WIFI_GET_PATCH_VER	2
#define MPATCH_WIFI_GET_FIRM_INFO   3

#define MPATCH_STARTLOGO_FUNC   2   //startlogo functype
#define MPATCH_UMS_STARTLOGO    0   //startlogo funcno

#define MPATCH_EXTNAME      ".MPATCH"

#define MPATCH_MIN_PARA     0
#define MPATCH_MAX_PARA     5//允许mpatch异常次数，包括程序异常和系统突然断电

//mpatch list
#define MPATCH_NAME_RS9100      "RS9100"
#define MPATCH_NAME_rs9100      "rs9100"
#define MPATCH_NAME_AP6181  "WIFI_03" //AP6181
#define MPATCH_NAME_STARTLOGO   "StartLogo"

#endif


