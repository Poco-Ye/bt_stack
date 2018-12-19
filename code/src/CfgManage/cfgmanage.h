#ifndef CFG_MANAGE_H
#define CFG_MANAGE_H

#include <stdarg.h>
#include <string.h>

#define ITEM_SIZE  (20)

typedef struct 
{
	char item[ITEM_SIZE+1];
}S_ITEMS_VALUE;

typedef struct 
{
	int type;
	S_ITEMS_VALUE * items;
	int num_per_item;

}S_VERSION_CONFIG;

typedef struct 
{
	int type;  
	char keyword[ITEM_SIZE+1];
	char context[ITEM_SIZE+1];
}S_HARD_INFO;

#define FILE_FORMAT_ERR		-1 //文件格式错误
#define KEYWORD_ERR			-2 //关键字不存在 
#define CONTEXT_ERR			-3 //关键字对应的项，不合法
#define FILL_ERR				-4 //配置设置错误
#define NO_CONTEXT_ERR		-5 //有关键，但没有内容 
#define ONLY_ONE_QUOTE_ERR	-6 //只有一个单引号
#define NO_CFG_FILE                      -7 //没有配置文件或者配置文件下载不完整 
#define CFGFILE_SIG_ERR		-8	//配置文件签名错误
#define FILL_ITEM(flag,x) {flag,x,sizeof(x)/sizeof(S_ITEMS_VALUE)}

//参数类型
#define CFG_TYPE 		1
#define BOARD_TYPE 	2
#define PARA_TYPE   		3
#define OTHER_TYPE    	4

/************************************************************************
函数原型：void CfgInfo_Init()           
参数：
	        无
返回值：
	<0      初始化错误
	其他    初始化成功
************************************************************************/
void CfgInfo_Init();
/************************************************************************
函数原型：int ReadCfgInfo(char *keyword,char *context)           
参数：
	keyword(input): 模块用于查询硬件配置信息的关键字，该关键字见相关文档				 
	context(ouput): 输出关键字对应的配置信息的字符串
返回值：
	<0      失败
	其他    成功
************************************************************************/
int ReadCfgInfo(char *keyword,char *context);

//版本信息的总个数
int CfgTotalNum();

//通过序号取得关键字及关键字对应的内容
int CfgGet(int num,int type,char *keyword,char *context);

/**************************获取指定信息***************/
#define INDEX_SN        		0
#define INDEX_EXSN      		1
#define INDEX_MACADDR   		2
#define INDEX_CFGFILE   		3 
#define INDEX_LCDGRAY   		4
#define INDEX_UAPUK     		5
#define INDEX_USPUK     		6 
#define INDEX_BEEPVOL     		7 
#define INDEX_KEYTONE     		8 
#define INDEX_RFPARA     		9
#define INDEX_USPUK1     		10
#define INDEX_USPUK2     		11
#define INDEX_CSN     			12
#define INDEX_TUSN     			13
#define MAX_INDEX                 20

#define MAX_BASE_CFGITEM 			(16)			// 座机配置信息个数


#endif
