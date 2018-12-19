/*
	存放版本信息
	author: sunJH
	create: 2008-10-10
History:
081010 sunJH
net_version获取的版本信息只有一个字节
目前已经不能满足需求,
因此增加一个接口NetGetVersion读取更多
的版本信息
字符串如: 116-081010-D
含义为: 116           版本号,发布一次累加
                  081010      发布日期
                  D/R          D为Debug, R为Release
*
*/
#include "inet/inet.h"

#ifdef NET_DEBUG
#define LAST_VER "D"
#else
#define LAST_VER "R"
#endif
 
static char net_ver_str[30]="165-120605-"LAST_VER; 


void NetGetVersion_std(char ip_ver[30])
{
	strcpy(ip_ver, net_ver_str);
}

