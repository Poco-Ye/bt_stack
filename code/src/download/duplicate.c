/*-------------------------------------------------------------
* Copyright (c) 2001-2007 PAX Computer Technology (Shenzhen) Co., Ltd.
* All Rights Reserved.
*
* This software is the confidential and proprietary information of
* PAX ("Confidential Information"). You shall not
* disclose such Confidential Information and shall use it only in
* accordance with the terms of the license agreement you entered
* into with PAX.
*
* PAX makes no representations or warranties about the suitability of 
* the software, either express or implied, including but not limited to 
* the implied warranties of merchantability, fitness for a particular purpose, 
* or non-infrigement. PAX shall not be liable for any damages suffered 
* by licensee as the result of using, modifying or distributing this software 
* or its derivatives.
* 
*
*  FileName: Duplicate.c
*  Author:	PengRongshou	 
*  Version : 1.0		
*  Date:2007/7/12
*  Description: 对拷
	

*  Function List:	

*  History: 		// 历史修改记录
   <author>  <time>   <version >   <desc>
   PengRongshou	07/09/10	 1.0	 build this moudle
2010.10.26 
修改对拷文件时，等待写入文件的超时，由原先1M文件的25秒增加到85秒
修改根据如下：根据flash文档，最坏情况是写入两字节耗时100us,1MB则耗时50秒，加上擦除耗时8秒（一个扇区0.5秒），所以等待1MB写入超时大于60秒
---------------------------------------------------------------*/
 /** 
 * @addtogroup 
 * 
 * @{
 */
#include <stdio.h>
#include "base.h"
#include "../file/filedef.h"
#include "localdownload.h"

#include "posapi.h"
#include "../encrypt/rsa.h"
#include "../puk/puk.h"
#include "../comm/comm.h"

#ifndef NULL
#define NULL 0
#endif

#define	SHAKE_HAND_TIMEOUT_DEFAULT	300
#define	SHAKE_HAND_TIMEOUT_BACKGROUND	2000
#define	SHAKE_HAND_TIMEOUT	2000
#define	DOWNLOAD_TIMEOUT	5000// 1S
#define	DOWNLOAD_RECVPACK_TIMEOUT	500  // ms

//define RET CODE 
#define 	RET_DUP_OK	0
#define 	RET_DUP_ERR	-1
#define		RET_DUP_SHAKE_TIMEOUT -2
#define		RET_DUP_RECV_TIMEOUT	-3
#define		RET_DUP_RCV_DATA_TIMEOUT -4
#define		RET_DUP_LRC_ERR 	-5
#define		RET_DUP_RCV_ERR	-6
#define		RET_DUP_RCVBUFLEN_ERR	-7
#define		RET_DUP_CHANGEBAUD_ERR -8
#define 	RET_DUP_MSG_ERROR	-9
#define		RET_DUP_TERMINAL_NOT_MATCH	-10
#define		RET_DUP_CHECKTERMINAL_FAIL	-11
#define		RET_DUP_CHECKCONFTAB_FAIL	-12
#define		RET_DUP_CONFITTAB_NOT_MATCH	-13
#define		RET_DUP_COPY_PUK_FAIL	-14
#define		RET_DUP_COPY_FONT_FAIL	-16
#define		RET_DUP_COPY_FILE_FAIL	-17
#define		RET_DUP_DELALL_APP_FAIL	-18
#define		RET_DUP_DEL_PUB_FAIL	-19
#define		RET_DUP_COPY_PUB_FAIL	-20

enum {
	IDUPDOWNLOADINIT=0 ,
	IDUPDOWNLOADSHAKE,
	IDUPDOWNLOADCHECKTERMINFO,
	IDUPDOWNLOADSETBAUD,
	IDUPDOWNLOADCHECKCONFTAB,
	IDUPDOWNLOADDELALLAPP,
	IDUPDOWNLOADPUK,
	IDUPDOWNLOADFONTLIB,
	IDUPDOWNLOADAPPFILE,
    IDUPDOWNLOADDELPUBFILE,
	IDUPDOWNLOADPUBFILE,
	IDUPDOWNLOADFINISH,
	IDUPDOWNLOADDONE,
	IDUPDOWNLOADEXIT
};
typedef struct tagStatusTable{
	int iStatusNumber;
	int (*pFunction)( int * );
}STATUSTABLE;

static uchar gucComPort=0xff;
static const uchar gaucComPortArr[4]={COM1,COM2,PINPAD,P_USB_HOST};
static uchar gaucComPortList[sizeof(gaucComPortArr)];

#define RCV_BUF_LEN 20480
static uchar *gaucRcvBuf=(uchar *)MAPP_RUN_ADDR;
extern uchar gaucSndBuf[20480];

static short gsLastMsgNo=0;//上次包序号
static short gsLastCmd = -1;
static uchar gucCompressSuport=0;
static uint gusMaxDataLen=1024;
static uint guiMaxBaudRate=115200;
static uchar gucDownloadNeedPrompt = 0;
static int giShakeTimeOut=SHAKE_HAND_TIMEOUT_DEFAULT;
// Refer to uartDownload
//记录下载时的显示命令，避免频繁刷新屏幕产生闪烁
static int lastStep = 0xff;

static volatile uint isLargePackets=0;

static void vDupDownloadingPromt(int iStep,int iCmd);
static int iSndMsg(int *piResult,int iSndLen);
static int iRecvMsg(int *piResult);
static int iDupDownLoadFile(int iFileType,char* pszFileName,uchar* pucFileAttr,int iAttrAppNo,int*piResult);

#define	TM_DUP_DOWNLOAD_CMD		&tm_dup0

static T_SOFTTIMER tm_dup0;
/**
状态机初始化函数

 @param[in,out]  *piResult,上一状态错误码，退出时返回本状态错误码
  
 @retval 下一状态
 \li #IDUPDOWNLOADDONE 
\li  #IDUPDOWNLOADSHAKE
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadInit( int *piResult)
{
	int	iRet,iLoop,iFailCount=0;
	uchar aucComAttr[32];

	// Refer to uartDownloadInit
	lastStep = 0xff;//刷新菜单标题
	if(gucDownloadNeedPrompt)
		vDupDownloadingPromt(IDUPDOWNLOADINIT,0);
	gsLastMsgNo=-1;//上次包序号
	gsLastCmd=-1;//上次命令
	isLargePackets = 0;

	memcpy(gaucComPortList, gaucComPortArr, sizeof(gaucComPortArr));
	snprintf(aucComAttr,sizeof(aucComAttr),"%d%s",DEFAULT_BAUDRATE,COMATTR);
	gucComPort = 0xff;
	//open all comport in gaucComPortList
	for(iLoop=0;iLoop<sizeof(gaucComPortList);iLoop++)
	{
		if(gaucComPortList[iLoop]==0xff) continue;
		PortClose(gaucComPortList[iLoop]);
		if(gaucComPortList[iLoop] == P_USB_HOST)
			iRet = PortOpen(gaucComPortList[iLoop], "PAXDEV");
		else
			iRet = PortOpen(gaucComPortList[iLoop], aucComAttr);
		if(iRet!=0)
		{
			gaucComPortList[iLoop]=0xff;
			iFailCount++;
		}
	}
	if(iFailCount<sizeof(gaucComPortList))
	{
		*piResult = RET_DUP_OK;
		
		return	IDUPDOWNLOADSHAKE;
	}
	else
	{
	
		//all comport open err, 
		*piResult = RET_DUP_ERR;
		return IDUPDOWNLOADDONE;
	}
}


/**
 握手，完成下载过程的握手

 @param[in,out]  *piResult,上一状态错误码，退出时返回本状态错误码
  
 @retval #IDUPDOWNLOADDONE 下一状态是下载结束
  @retval #IDUPDOWNLOADREQ	 下一状态是继续接收命令
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadShake( int *piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS 	IDUPDOWNLOADSHAKE+1
	uchar ucTmp;
	int	iLoop,iFindLoop,iTmp;
	if(gucDownloadNeedPrompt)
		vDupDownloadingPromt(IDUPDOWNLOADSHAKE,0);
	s_TimerSetMs(TM_DUP_DOWNLOAD_CMD,giShakeTimeOut);
	iFindLoop=-1;
	while(s_TimerCheckMs(TM_DUP_DOWNLOAD_CMD))
	{	
		iFindLoop=(iFindLoop+1)%sizeof(gaucComPortList);
		//iDPrintf("iFindLoop[%d]\n",iFindLoop);
		if(gaucComPortList[iFindLoop]==0xff) continue;

		if(0!=PortSend(gaucComPortList[iFindLoop], SHAKE_REQUEST))
		{
			continue;
		}
		if(0==PortRecv(gaucComPortList[iFindLoop], &ucTmp,50))
		{		
			if(ucTmp==SHAKE_REPLY||ucTmp==SHAKE_REPLY_H)
			{
				gucComPort = gaucComPortList[iFindLoop];
				for(iTmp=0;iTmp<sizeof(gaucComPortList);iTmp++)
				{
					if(iTmp==iFindLoop) continue;
					if(gaucComPortList[iTmp]==0xff) continue;
					PortClose(gaucComPortList[iTmp]);
				}
				*piResult = RET_DUP_OK;
                if(ucTmp==SHAKE_REPLY_H)isLargePackets=1;
				return  NEXT_STATUS;
			}
			
		}
		
	}

	for(iTmp=0;iTmp<sizeof(gaucComPortList);iTmp++)
	{
		if(gaucComPortList[iTmp]==0xff) continue;
		PortClose(gaucComPortList[iTmp]);
	}
	
	*piResult = RET_DUP_SHAKE_TIMEOUT;
	return IDUPDOWNLOADDONE;
	
	/****************************************************************/
	

}
/**
读取接收方终端信息，检查协议版本，监控版本，终端类型是否一致
读取接收方支持的最大波特率、数据长度等值
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadCheckTermInfo(int*piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	 IDUPDOWNLOADCHECKTERMINFO+1
	int iRcvLen;
	uchar aucTmp[16],sTermName[16];
	vDupDownloadingPromt(IDUPDOWNLOADCHECKTERMINFO,LD_CMDCODE_GETPOSINFO);
	gaucSndBuf[3]=LD_CMDCODE_GETPOSINFO;
	gaucSndBuf[4]=0;
	gaucSndBuf[5]=0;
    if(isLargePackets)
    {
        gaucSndBuf[6]=0;
        gaucSndBuf[7]=0;
    	iSndMsg(piResult,9);
    }
    else
    {
    	iSndMsg(piResult,7);
    }
	iRcvLen = iRecvMsg(piResult);
	if(iRcvLen<7)
	{
		return IDUPDOWNLOADDONE;
	}
	if(gaucRcvBuf[6]!=LD_OK)
	{
		*piResult = RET_DUP_CHECKTERMINAL_FAIL;
		return IDUPDOWNLOADDONE;
	}
	get_term_name(sTermName);
	if((memcmp(gaucRcvBuf+7,sTermName,strlen(sTermName))!=0)//终端类型
    	||(gaucRcvBuf[23]!=PROTOCOL_VERSION))//协议版本
	{
		*piResult = RET_DUP_TERMINAL_NOT_MATCH;
		return IDUPDOWNLOADDONE;
	}
	gucCompressSuport = gaucRcvBuf[40];//压缩是否支持标志

    if(isLargePackets)
    {
    	//数据域最大长度
    	gusMaxDataLen = (gaucRcvBuf[41]<<24)|
                    	(gaucRcvBuf[42]<<16)|
                    	(gaucRcvBuf[43]<<8)|
                        (gaucRcvBuf[44]);
    	if(gusMaxDataLen>LD_MAX_RECV_DATA_LEN)
    	    gusMaxDataLen = LD_MAX_RECV_DATA_LEN;

    	//取最大支持波特率
    	guiMaxBaudRate = (gaucRcvBuf[77]<<24)|
    					 (gaucRcvBuf[78]<<16)|
    					 (gaucRcvBuf[79]<<8)|
    					 (gaucRcvBuf[80]);
    }
    else
    {
    	//数据域最大长度
    	gusMaxDataLen = (gaucRcvBuf[41]<<8)|(gaucRcvBuf[42]);
    	if(gusMaxDataLen>LD_MAX_RECV_DATA_LEN)
    	    gusMaxDataLen = LD_MAX_RECV_DATA_LEN;

    	//取最大支持波特率
    	guiMaxBaudRate = (gaucRcvBuf[75]<<24)|
    					 (gaucRcvBuf[76]<<16)|
    					 (gaucRcvBuf[77]<<8)|
    					 (gaucRcvBuf[78]);

    }
	return NEXT_STATUS;

}
/**
设置新的波特率
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadSetBaud( int *piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	 IDUPDOWNLOADSETBAUD+1
	uchar aucComAttr[32];
	int iRcvLen,iRet;
	vDupDownloadingPromt(IDUPDOWNLOADSETBAUD, LD_CMDCODE_SETBAUTRATE);
	if(guiMaxBaudRate>LD_MAX_BAUDRATE) guiMaxBaudRate = LD_MAX_BAUDRATE;
	else if(guiMaxBaudRate==DEFAULT_BAUDRATE) return NEXT_STATUS;
	snprintf(aucComAttr,sizeof(aucComAttr),"%d%s",guiMaxBaudRate,COMATTR);
	gaucSndBuf[3]=LD_CMDCODE_SETBAUTRATE;
    if(isLargePackets)
    {
    	gaucSndBuf[4]=0;
    	gaucSndBuf[5]=0;
        gaucSndBuf[6]=0;
        gaucSndBuf[7]=4;
    	
    	gaucSndBuf[8] = (uchar)((guiMaxBaudRate>>24)&0xff);
    	gaucSndBuf[9] = (uchar)((guiMaxBaudRate>>16)&0xff);
    	gaucSndBuf[10] = (uchar)((guiMaxBaudRate>>8)&0xff);
    	gaucSndBuf[11] = (uchar)((guiMaxBaudRate>>0)&0xff);
    	iSndMsg(piResult,13);
    }
    else
    {
    	gaucSndBuf[4]=0;
    	gaucSndBuf[5]=4;
    	
    	gaucSndBuf[6] = (uchar)((guiMaxBaudRate>>24)&0xff);
    	gaucSndBuf[7] = (uchar)((guiMaxBaudRate>>16)&0xff);
    	gaucSndBuf[8] = (uchar)((guiMaxBaudRate>>8)&0xff);
    	gaucSndBuf[9] = (uchar)((guiMaxBaudRate>>0)&0xff);
    	iSndMsg(piResult,11);
    }
	iRcvLen = iRecvMsg(piResult);
	if(iRcvLen<7)
	{
		return IDUPDOWNLOADDONE;
	}
	if(gaucRcvBuf[6]!=LD_OK)
	{
		return NEXT_STATUS;
	}
	if(P_USB_HOST!=gucComPort)
	{	
		//设置新的波特率
		PortClose(gucComPort);
		iRet = PortOpen(gucComPort, aucComAttr);
		if(iRet !=0)
		{
			*piResult = RET_DUP_CHANGEBAUD_ERR;
			return IDUPDOWNLOADDONE;
		}
	}
	//延迟100ms
	DelayMs(100);//delay 100ms;
	return NEXT_STATUS;
}
/**
	从接收方读取配置表信息，并比较配置表是否想等，
配置表不等则不能对拷
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadCheckConfTab(int*piResult)
{

	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	 IDUPDOWNLOADCHECKCONFTAB+1
	int iRcvLen,iRet,iLen,iCfgTabLen;
	uchar aucTmp[CONFIG_TAB_LEN+8];
	gaucSndBuf[3]=LD_CMDCODE_GETCONFTABLE;

    if(isLargePackets)
    {
    	gaucSndBuf[4]=0;
    	gaucSndBuf[5]=0;
        gaucSndBuf[6]=0;
        gaucSndBuf[7]=0;
    	iSndMsg(piResult,9);
    }
    else
    {
    	gaucSndBuf[4]=0;
    	gaucSndBuf[5]=0;
    	iSndMsg(piResult,7);
    }
	iRcvLen = iRecvMsg(piResult);
	if(iRcvLen<7)
	{
		return IDUPDOWNLOADDONE;
	}
	if(gaucRcvBuf[6]!=LD_OK)
	{
		*piResult = RET_DUP_CHECKTERMINAL_FAIL;
		return IDUPDOWNLOADDONE;
	}
	iCfgTabLen = ReadSecuConfTab(aucTmp);
	if(0>=iCfgTabLen)
	{
		*piResult = RET_DUP_ERR;
		return IDUPDOWNLOADDONE;
	}
	
	if((memcmp(aucTmp,gaucRcvBuf+11,1)!=0))
	{     		
		*piResult = RET_DUP_CONFITTAB_NOT_MATCH;
		return IDUPDOWNLOADDONE;
	}

	return NEXT_STATUS;
}
/**
删除接收方所有应用程序、参数文件、字库、及dmr
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadDelAllApp(int*piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	 IDUPDOWNLOADDELALLAPP+1
	int iRcvLen,iRet,iLen;
	vDupDownloadingPromt(IDUPDOWNLOADDELALLAPP,LD_CMDCODE_DELALLAPP);
	gaucSndBuf[3]=LD_CMDCODE_DELALLAPP;
    if(isLargePackets)
    {
    	gaucSndBuf[4]=0;
    	gaucSndBuf[5]=0;
        gaucSndBuf[6]=0;
        gaucSndBuf[7]=0;
    	iSndMsg(piResult,9);
    }
    else
    {
    	gaucSndBuf[4]=0;
    	gaucSndBuf[5]=0;
    	iSndMsg(piResult,7);
    }
    
	iRcvLen = iRecvMsg(piResult);
	if(iRcvLen<7)
	{
		return IDUPDOWNLOADDONE;
	}
	if(gaucRcvBuf[6]!=LD_OK)
	{
		*piResult = RET_DUP_DELALL_APP_FAIL;
		return IDUPDOWNLOADDONE;
	}
	return NEXT_STATUS;
			
}
/**
 下载公钥
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadPuk(int*piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	  IDUPDOWNLOADPUK+1

	return NEXT_STATUS;
}
/**
 下载字库
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadFontLib(int*piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	 IDUPDOWNLOADFONTLIB+1 
	int iRet;
	
	iRet = iDupDownLoadFile(FILE_TYPE_FONT, FONT_FILE_NAME, "\xff\x02",0xff, piResult);
	if(iRet==0)
	{
		return NEXT_STATUS;
	}
	else
	{
		*piResult = RET_DUP_COPY_FONT_FAIL;
		return IDUPDOWNLOADDONE;
	}
}
/**
下载应用程序及用户文件、参数文件，私有库到接收方
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态是下载结束
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadAppFile(int*piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	 IDUPDOWNLOADAPPFILE+1
	int iLoop,iRet;
	int iFileType,iAttrAppNo;
	char pszFileName[33];
	uchar aucFileAttr[8];
	int iAllFileNum,iSearch;
	FILE_INFO astFileInfo[MAX_FILES];
	iAllFileNum=GetFileInfo(astFileInfo);

	for(iLoop=0;iLoop<MAX_APP_NUM;iLoop++)
	{
		//下载应用程序
		snprintf(pszFileName,sizeof(pszFileName),"%s%d",APP_FILE_NAME_PREFIX,iLoop);
		if(iLoop==0)
		{
			aucFileAttr[0]=0xff;
			aucFileAttr[1]=FILE_TYPE_MAPP;
			iFileType = FILE_TYPE_MAPP;
		}
		else
		{	
			aucFileAttr[0]=0xff;
			aucFileAttr[1]=FILE_TYPE_APP;
			iFileType = FILE_TYPE_APP;			
		}
		if(s_SearchFile(pszFileName, aucFileAttr)<0)continue;
		
		iAttrAppNo=iDupDownLoadFile(iFileType,pszFileName,aucFileAttr,0xff,piResult);
		if(iAttrAppNo<0)
		{
			return IDUPDOWNLOADDONE;
		}

		//下载应用附属的用户文件和参数文件
		for(iSearch=0;iSearch<iAllFileNum;iSearch++)
		{
			if(astFileInfo[iSearch].attr!=iLoop) continue;

			//modify by skx 增加应用中私有动态文件对拷	//080520讨论不对拷用户文件
			if((astFileInfo[iSearch].type!=FILE_TYPE_APP_PARA) &&
				(astFileInfo[iSearch].type!=FILE_TYPE_USERSO) && 
				(astFileInfo[iSearch].type!=FILE_TYPE_SYSTEMSO))
			continue;

			if( astFileInfo[iSearch].type == FILE_TYPE_USERSO ||
				astFileInfo[iSearch].type == FILE_TYPE_SYSTEMSO)
				iFileType = LD_PARA_SO;
			else
				iFileType = astFileInfo[iSearch].type;
			aucFileAttr[0]=iLoop;
			aucFileAttr[1]=astFileInfo[iSearch].type;
			memcpy(pszFileName,astFileInfo[iSearch].name,sizeof(astFileInfo[iSearch].name));
			iRet = iDupDownLoadFile(iFileType,pszFileName,aucFileAttr,iAttrAppNo,piResult);
			if(iRet!=0)
			{
				return IDUPDOWNLOADDONE;
			}
		}
	}
		
	return  NEXT_STATUS;
}



/**
下载公有文件及共享动态库(除paxbase)
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态是下载结束
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li songkx-----------------13/12/04-------1.0.0.0----------创建
 */	
int iDupDownloadPubFile(int*piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	 IDUPDOWNLOADPUBFILE+1
	int iLoop,iRet;
	int iFileType,iAttrAppNo;
	char pszFileName[33];
	uchar aucFileAttr[8];
	int iAllFileNum,iSearch;
	FILE_INFO astFileInfo[MAX_FILES];
	iAllFileNum=GetFileInfo(astFileInfo);//找出文件系统中所有文件

	*piResult = RET_DUP_OK;
	
	if( !iAllFileNum ) 
	{
		*piResult = RET_DUP_COPY_PUB_FAIL;
		return IDUPDOWNLOADDONE;
	}
	

	for(iSearch =0 ; iSearch< iAllFileNum ;iSearch++)
	{
		if(astFileInfo[iSearch].attr == FILE_ATTR_PUBSO ||
			astFileInfo[iSearch].attr == FILE_ATTR_PUBFILE)//公共文件或者共享动态库文件
		{
            //不对拷baseso以及WIFI_MPATCH
			if(astFileInfo[iSearch].attr == FILE_ATTR_PUBSO && 
				strstr(astFileInfo[iSearch].name,FIRMWARE_SO_NAME))continue;
			if( astFileInfo[iSearch].attr == FILE_ATTR_PUBSO && is_wifi_mpatch(astFileInfo[iSearch].name))continue;
			iFileType = astFileInfo[iSearch].type;
			strcpy(pszFileName , astFileInfo[iSearch].name);
			aucFileAttr[0] = astFileInfo[iSearch].attr;
			aucFileAttr[1] = astFileInfo[iSearch].type;
			iRet=iDupDownLoadFile(iFileType,pszFileName,aucFileAttr,0xff,piResult);
			if( iRet ) 
			{
				*piResult = RET_DUP_COPY_PUB_FAIL;
				return IDUPDOWNLOADDONE;
			}
		}
	}

	return NEXT_STATUS;
	
}
/**
删除所有公有文件及公有动态库(除PAXBASE)
 @param[in,out]  *piResult,上一状态错误码
 @retval # 下一状态是下载结束
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li songkx-----------------13/12/05-------1.0.0.0----------创建
 */	
int iDupDownloadDelPubFile(int*piResult)
{
	#undef		NEXT_STATUS
	#define 	NEXT_STATUS	 IDUPDOWNLOADDELPUBFILE+1
	int iRcvLen,iRet,iLen;
	vDupDownloadingPromt(IDUPDOWNLOADDELALLAPP,LD_CMDCODE_DELPUBFILE);
	gaucSndBuf[3]=LD_CMDCODE_DELPUBFILE;
	gaucSndBuf[4]=0;
	gaucSndBuf[5]=0;
    if(isLargePackets)
    {
        gaucSndBuf[6]=0;
        gaucSndBuf[7]=0;
    	iSndMsg(piResult,9);
    }
    else
        iSndMsg(piResult,7);
	iRcvLen = iRecvMsg(piResult);

	if(iRcvLen<7)return IDUPDOWNLOADDONE;
    
	if(gaucRcvBuf[6]!=LD_OK)
	{
		*piResult = RET_DUP_DEL_PUB_FAIL;
		return IDUPDOWNLOADDONE;
	}
	return NEXT_STATUS;
	
}


/**
发送下载完成命令给接收方
 @param[in,out]  *piResult,上一状态错误码
 @retval #IDUPDOWNLOADEXIT 下一状态是下载结束
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadFinish(int *piResult)
{
	#undef		NEXT_STATUS
	#define NEXT_STATUS IDUPDOWNLOADFINISH+1
	int iRcvLen;

	vDupDownloadingPromt(IDUPDOWNLOADFINISH, 0);
	gucDownloadNeedPrompt=0;	

	gaucSndBuf[3]=LD_CMDCODE_DLCOMPLETE;
	gaucSndBuf[4]=0;
	gaucSndBuf[5]=0;
    if(isLargePackets)
    {
        gaucSndBuf[6]=0;
        gaucSndBuf[7]=0;
    	iSndMsg(piResult,9);
    }
    else
    {
    	iSndMsg(piResult,7);
    }
	iRcvLen = iRecvMsg(piResult);

	if(iRcvLen<7)
	{
			
	}
	*piResult = RET_OK;
	return NEXT_STATUS;
}
/**
 状态机最后的状态，状态机退出时，会进入本状态，
 在本状态，根据piResult值，进行相应处理。
 @param[in,out]  *piResult,上一状态错误码
 @retval #IDUPDOWNLOADEXIT 下一状态是下载结束
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int iDupDownloadDone( int *piResult)
{	
	int iTmp;
	giShakeTimeOut=SHAKE_HAND_TIMEOUT_DEFAULT;
	if(gucComPort!=0xFF)
	{
		if(P_USB_HOST == gucComPort)
		{
			PortClose(P_USB_HOST);
		}
		gucComPort = 0xff;
	}
	vDupDownloadingPromt(IDUPDOWNLOADDONE,*piResult);
	switch(*piResult)
	{
		case RET_DUP_OK:
		{
		}
		default:
		{
			break;
		}
	}
	gucDownloadNeedPrompt=0;	
	return IDUPDOWNLOADEXIT;
}


/**
 状态机主函数。运行状态机入口
 @param[in]iMode
	\li #FRONT_MODE 1 :前台模式
	\li #BACKGOUND_MODE  0 :后台模式，没有接收到命令前，不在lcd上作提示
@retval #RET_DUP_RECV_TIMEOUT 接收命令超时
@remarks
	\li 作者-----------------时间--------版本---------修改说明                    
 	\li PengRongshou---07/06/28--1.0.0.0-------创建
*/
int iDupDownloadMain(int iMode)
{
	STATUSTABLE aDupDStatus[] =
	{
		{ IDUPDOWNLOADINIT, iDupDownloadInit},
		{ IDUPDOWNLOADSHAKE, iDupDownloadShake},
		{IDUPDOWNLOADCHECKTERMINFO,iDupDownloadCheckTermInfo},
		{IDUPDOWNLOADSETBAUD,iDupDownloadSetBaud},
		{IDUPDOWNLOADCHECKCONFTAB,iDupDownloadCheckConfTab},
		{IDUPDOWNLOADDELALLAPP,iDupDownloadDelAllApp},
		
		{IDUPDOWNLOADPUK,iDupDownloadPuk},
		{IDUPDOWNLOADFONTLIB,iDupDownloadFontLib},
		{IDUPDOWNLOADAPPFILE,iDupDownloadAppFile},
		{IDUPDOWNLOADDELPUBFILE,iDupDownloadDelPubFile},
		{IDUPDOWNLOADPUBFILE,iDupDownloadPubFile},
		{IDUPDOWNLOADFINISH,iDupDownloadFinish},
		{IDUPDOWNLOADDONE, iDupDownloadDone},
		{ -1, 0}
	};
	STATUSTABLE *p;
	int iResult=-1;
	int iStatusNumber;
	if(iMode!=0)
	{
		gucDownloadNeedPrompt =1;
		giShakeTimeOut = SHAKE_HAND_TIMEOUT;
	}
	else
	{
		gucDownloadNeedPrompt =0;
		giShakeTimeOut = SHAKE_HAND_TIMEOUT_BACKGROUND;

	}
	
	iStatusNumber = IDUPDOWNLOADINIT;

	while ( iStatusNumber != IDUPDOWNLOADEXIT )
	{
		for ( p = ( STATUSTABLE *)aDupDStatus ; p->pFunction != 0 ; p++ )
		{
			if ( iStatusNumber == p->iStatusNumber )
			{
				iStatusNumber = ( *p->pFunction)( &iResult );
				break;
			}
		}
		
		if ( p->iStatusNumber == -1 || p->pFunction == NULL )
		{
			iStatusNumber = IDUPDOWNLOADINIT;
		}

	}

	return iResult;
}

/**
接收应答数据包，
@param[out]*piResult:错误码	
@retval >0接收到数据包长度
@retval <=0 错误
@remarks
	\li 作者-----------------时间--------版本---------修改说明                    
 	\li PengRongshou---07/09/18--1.0.0.0-------创建
*/
static int iRecvMsg(int *piResult)
{
	int iRet,iLoop,iTmp,iLen,iRcvLen;

	*piResult =RET_DUP_RECV_TIMEOUT;
	if(gucComPort!=0xff)
	{
		s_TimerSetMs(TM_DUP_DOWNLOAD_CMD,DOWNLOAD_TIMEOUT);
		while(s_TimerCheckMs(TM_DUP_DOWNLOAD_CMD))
		{
			iRet = PortRecv(gucComPort, gaucRcvBuf,  0);
			//iDPrintf("[%s %d]recv stx[%d]\n",FILELINE,iRet);
			if(iRet!=0) continue;
			if(gaucRcvBuf[0]== SHAKE_REQUEST)
			{
                if(isLargePackets)
                    PortSend(gucComPort, SHAKE_REPLY_H);
                else
    				PortSend(gucComPort, SHAKE_REPLY);
				continue;
			}
			if(gaucRcvBuf[0]!=STX)
			{
				continue;
			}
			RECV_PACK_SEQ:
			iRet = PortRecv(gucComPort, gaucRcvBuf+1, 20);
			if(iRet!=0) continue;			
			iRet = PortRecv(gucComPort, gaucRcvBuf+2, 20);
			if(iRet!=0) continue;
			if(gaucRcvBuf[2]==STX)
			{
				goto RECV_PACK_SEQ;	
			}
			else if(gaucRcvBuf[2]!=LD_CMD_TYPE)
			{
				continue;
			}
			iRet = PortRecv(gucComPort, gaucRcvBuf+3, 20);
			if(iRet!=0) continue;
			else
			{	
				*piResult = RET_DUP_OK;
				break;
			}
		}

	}
	else
	{
		return -1;
	}

	if(*piResult ==RET_DUP_RECV_TIMEOUT)
	{
		return  -2;
	}

	//recv msg length
	
	iRet = PortRecvs(gucComPort, gaucRcvBuf+4, 2, DOWNLOAD_TIMEOUT);

	if(iRet!=2)
	{
		*piResult =RET_DUP_RECV_TIMEOUT;
		return -3;
	}
	iLen = ((uchar)gaucRcvBuf[4]<<8)+(uchar)gaucRcvBuf[5];
	iLen++;
	if(iLen+6>RCV_BUF_LEN)
	{
		
		*piResult =RET_DUP_RCVBUFLEN_ERR;
		return -4;
	}
	iRcvLen=0;
	
	s_TimerSetMs(TM_DUP_DOWNLOAD_CMD,DOWNLOAD_RECVPACK_TIMEOUT);
	while((iLen>0)&&s_TimerCheckMs(TM_DUP_DOWNLOAD_CMD))
	{
		iTmp = PortRecvs(gucComPort, gaucRcvBuf+6+iRcvLen, iLen, DOWNLOAD_RECVPACK_TIMEOUT);
		if(iTmp>0)
		{
			iRcvLen+=iTmp;
			iLen -=iTmp;
			s_TimerSetMs(TM_DUP_DOWNLOAD_CMD,DOWNLOAD_RECVPACK_TIMEOUT);

		}
	}	
	if(iLen>0)
	{
		*piResult =RET_DUP_RCV_DATA_TIMEOUT;
		return -5;
	}
	if(gaucRcvBuf[6+iRcvLen-1]!=ucGenLRC(gaucRcvBuf+1, iRcvLen+6-2))
	{
		*piResult =RET_DUP_LRC_ERR;
		return -6;
	}
	
	*piResult =RET_DUP_OK	;
	if((gaucRcvBuf[1]!=(gsLastMsgNo%0x100))||(gaucRcvBuf[3]!=gsLastCmd))
	{
		*piResult =RET_DUP_MSG_ERROR;
		return -7;
	}
	return  iRcvLen+6;
	
}

/**
将存在全局变量gaucSndBuf中的数据发送
@param[in]iSndLen发送的长度。
@param[out]piResult 错误码	
@retval 0成功
@retval 非0--发送失败
@remarks
	\li 作者-----------------时间--------版本---------修改说明                    
 	\li PengRongshou---07/06/28--1.0.0.0-------创建
*/
static int iSndMsg(int *piResult,int iSndLen)
{
	uchar ucRet;
	int iLen;
	gsLastMsgNo++;
	gaucSndBuf[0]=STX;
	gaucSndBuf[1]=gsLastMsgNo;
	gaucSndBuf[2]=LD_CMD_TYPE;
	gaucSndBuf[iSndLen-1]=ucGenLRC(gaucSndBuf+1,iSndLen-2);
	gsLastCmd = gaucSndBuf[3];

	if(iSndLen>8192)
	{
		iLen = 4096;
		ucRet = PortSends(gucComPort,gaucSndBuf, iLen);
		if(ucRet!=0)
		{			
			*piResult = RET_DUP_ERR;		
			return -1;
		}
		
	}
	else
	{
		iLen =0;
	}

    s_TimerSetMs(TM_DUP_DOWNLOAD_CMD,DOWNLOAD_TIMEOUT);
	while(PortTxPoolCheck(gucComPort))
	{
		if(!s_TimerCheckMs(TM_DUP_DOWNLOAD_CMD))
		{
            *piResult = RET_DUP_ERR;		
			return -1;
        }
	}
	ucRet = PortSends(gucComPort,gaucSndBuf+iLen, iSndLen-iLen);
	if(ucRet!=0)
	{
		*piResult = RET_DUP_ERR;		
		return -1;
	}
	
	*piResult = RET_DUP_OK;
	return 0;
}
/**

发送文件，
 
 @param[in]pucFileAttr 文件属性"\xAA\xBB",AA:文件属主，BB:文件类型
 @param[in]pszFileName文件名 
 @param[out]  返回的错误码
  
 @retval 
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/09/18--1.0.0.0-------创建
 */
static int iSndFile(uchar * pucFileAttr,char * pszFileName,int *piResult)
{
	int iFileID,iPercent,iFileSize,iHaveSnd=0;
	int iLen,iRcvLen,iRet=0;
    
	iFileID = s_open(pszFileName,O_RDWR,pucFileAttr);
	if(iFileID<0) return -1;
	iFileSize = s_filesize(pszFileName, pucFileAttr);
	gaucSndBuf[3]=LD_CMDCODE_DLFILEDATA;
	
	for(iHaveSnd=0;iHaveSnd<iFileSize;)
	{
        if(isLargePackets)
        {
    		iLen = read(iFileID,gaucSndBuf+8,gusMaxDataLen);
    		if(iLen<=0) 
    		{
    			iRet = -2;
    			break;
    		}

            gaucSndBuf[4]=0;
            gaucSndBuf[5]=0;
    		gaucSndBuf[6]=(iLen>>8)&0xff;
    		gaucSndBuf[7]=iLen&0xff;
    		//vDupDownloadingPromt(IDUPDOWNLOADAPPFILE,LD_CMDCODE_DLFILEDATA);
    		iPercent = 100*iHaveSnd/iFileSize;
    		if(iPercent>100) iPercent =100;
    		vDupDownloadingPromt(-1,iPercent);
    		iHaveSnd+=iLen;
    		iSndMsg( piResult, iLen+9);
        }
        else
        {
    		iLen = read(iFileID,gaucSndBuf+6,gusMaxDataLen);
    		if(iLen<=0) 
    		{
    			iRet = -2;
    			break;
    		}

    		gaucSndBuf[4]=(iLen>>8)&0xff;
    		gaucSndBuf[5]=iLen&0xff;
    		//vDupDownloadingPromt(IDUPDOWNLOADAPPFILE,LD_CMDCODE_DLFILEDATA);
    		iPercent = 100*iHaveSnd/iFileSize;
    		if(iPercent>100) iPercent =100;
    		vDupDownloadingPromt(-1,iPercent);
    		iHaveSnd+=iLen;
    		iSndMsg( piResult, iLen+7);
        }

        s_TimerSetMs(TM_DUP_DOWNLOAD_CMD,DOWNLOAD_TIMEOUT);
		while(PortTxPoolCheck(gucComPort))
		{
			if(!s_TimerCheckMs(TM_DUP_DOWNLOAD_CMD))
			{
                iRet = -3;
                goto exit;
            }
		}
        
		iRcvLen = iRecvMsg(piResult);
		if(iRcvLen<7)
		{
			 iRet = -4;
			 break;
		}
		if(gaucRcvBuf[6]!=LD_OK)
		{
			*piResult = gaucRcvBuf[6];
			iRet =  -5;
			break;
		}
	}

exit:    
	close(iFileID);
	return iRet;
}

/**
下载文件
 @param[in] iFileType 文件类型
 @param[in] pszFileName 文件名
 @param[in] pucFileAttr文件属性
 @param[in] iAttrAppNo,文件所属应用号,对方的应用号,由下载应用程序文件时返回 
 			只在文件类型为参数文件和用户文件,dmr文件时有效
 @param[out] piResult 下载结果
 @retval 
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/09/18--1.0.0.0-------创建
 */
int iDupDownLoadFile(int iFileType,char* pszFileName,uchar* pucFileAttr,int iAttrAppNo,int*piResult)
{

	int iLen,iTmp,iLoop;
	int iRcvLen,iRet;
	uchar ucCmdCode;
	//发起下载文件请求
	iLen = s_filesize(pszFileName,pucFileAttr);

	if(iLen<=0)
	{
		return 0;
	}

	iTmp = 4;//用来计算不同命令报文的数据域长度,
	//add by skx for public file 
	if(pucFileAttr[0]==FILE_ATTR_PUBFILE || pucFileAttr[0]==FILE_ATTR_PUBSO )
	{
		ucCmdCode = LD_CMDCODE_DLPUBFILE;
        if(isLargePackets)
        {
    		gaucSndBuf[12]=0x00;//appno
    		if( pucFileAttr[0] == FILE_ATTR_PUBSO )
    			gaucSndBuf[13]=LD_PUB_SO;//SO文件专属下载类型
    		else
    			gaucSndBuf[13]=iFileType;
    		
    		iTmp+=2;
    		memset(gaucSndBuf+14,0x00,16);
    		strcpy(gaucSndBuf+14,pszFileName);
        }
        else
        {
    		gaucSndBuf[10]=0x00;//appno
    		if( pucFileAttr[0] == FILE_ATTR_PUBSO )
    			gaucSndBuf[11]=LD_PUB_SO;//SO文件专属下载类型
    		else
    			gaucSndBuf[11]=iFileType;
    		
    		iTmp+=2;
    		memset(gaucSndBuf+12,0x00,16);
    		strcpy(gaucSndBuf+12,pszFileName);
        }
		iTmp+=16;
	}
	else
	{
		switch(iFileType)
		{
			case FILE_TYPE_FONT:
				ucCmdCode = LD_CMDCODE_DLFONTLIB;
				break;
			case FILE_TYPE_APP:
			case FILE_TYPE_MAPP:
				ucCmdCode = LD_CMDCODE_DLAPP;
				break;
			case FILE_TYPE_USER_FILE:
			case FILE_TYPE_APP_PARA:
			//增加用户私有动态文件对拷
			case LD_PARA_SO://文件名从文件中提取
				ucCmdCode = LD_CMDCODE_DLPARA;
                if(isLargePackets)
                {
    				gaucSndBuf[12]=iAttrAppNo;
    				gaucSndBuf[13]=iFileType;
    				iTmp+=2;
    				if(iFileType==FILE_TYPE_USER_FILE)
    				{	
    					memset(gaucSndBuf+14,0x00,16);
    					strcpy(gaucSndBuf+14,pszFileName);
    					iTmp+=16;
    				}
                }
                else
                {
    				gaucSndBuf[10]=iAttrAppNo;
    				gaucSndBuf[11]=iFileType;
    				iTmp+=2;
    				if(iFileType==FILE_TYPE_USER_FILE)
    				{	
    					memset(gaucSndBuf+12,0x00,16);
    					strcpy(gaucSndBuf+12,pszFileName);
    					iTmp+=16;
    				}
                }
				break;
			default:
				return -1;
		}
		
	}

	
	vDupDownloadingPromt(IDUPDOWNLOADAPPFILE,ucCmdCode);
	gaucSndBuf[3]=ucCmdCode;
    if(isLargePackets)
    {
        gaucSndBuf[4]=0;
        gaucSndBuf[5]=0;
    	gaucSndBuf[6]=(iTmp>>8)&0xFF;
    	gaucSndBuf[7]=iTmp&0xFF;
    	gaucSndBuf[8]=(iLen>>24)&0xFF;
    	gaucSndBuf[9]=(iLen>>16)&0xFF;
    	gaucSndBuf[10]=(iLen>>8)&0xFF;
    	gaucSndBuf[11]=(iLen>>0)&0xFF;
    	iTmp+=9;//报文总长度
    }
    else
    {
    	gaucSndBuf[4]=(iTmp>>8)&0xFF;
    	gaucSndBuf[5]=iTmp&0xFF;
    	gaucSndBuf[6]=(iLen>>24)&0xFF;
    	gaucSndBuf[7]=(iLen>>16)&0xFF;
    	gaucSndBuf[8]=(iLen>>8)&0xFF;
    	gaucSndBuf[9]=(iLen>>0)&0xFF;
    	iTmp+=7;//报文总长度
    }
	
	iSndMsg( piResult, iTmp);
	iRcvLen = iRecvMsg(piResult);

	if(iRcvLen<7)
	{
		return -2;
	}
	if(gaucRcvBuf[6]!=LD_OK)
	{
		*piResult = gaucRcvBuf[6];
		return -3;
	}

	//下载文件数据
	if(0!=iSndFile(pucFileAttr,pszFileName,piResult))
	{
		*piResult = RET_DUP_COPY_FILE_FAIL;
		return -4;
	}

	// FontLib updating, update prompt then.
	if (ucCmdCode == LD_CMDCODE_DLFONTLIB)
		lastStep = 0xff;
	vDupDownloadingPromt(IDUPDOWNLOADAPPFILE,LD_CMDCODE_WRITEFILE);
	
	//写入文件
	gaucSndBuf[3]=LD_CMDCODE_WRITEFILE;
	gaucSndBuf[4]=0;
	gaucSndBuf[5]=0;
    if(isLargePackets)
    {
        gaucSndBuf[6]=0;
        gaucSndBuf[7]=0;
    	iSndMsg(piResult,9);
    }
    else
    {
    	iSndMsg(piResult,7);
    }
	for(iLoop=0;iLoop<1+iLen/(64*1024);iLoop++)//大文件写入时间比较长
	{
		iRcvLen = iRecvMsg(piResult);
		if(*piResult==RET_DUP_RECV_TIMEOUT)
		{
			continue;
		}
		if(iRcvLen>0)
		{
			break;
		}
	}
	
	if(iRcvLen<7)
	{
		return -5;
	}
	
	if(gaucRcvBuf[6]!=LD_OK)
	{
		*piResult = gaucRcvBuf[6];
		return -5;
	}

	if(ucCmdCode==LD_CMDCODE_DLAPP)
	{
		return gaucRcvBuf[7];
	}
	else
	{
		return 0;
	}
}

/**
对拷过程界面提示函数。
@param[in]iStep 步骤
@param[in]iCmd 命令
@retval 
@remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/09/18--1.0.0.0-------创建
 */
void vDupDownloadingPromt(int iStep,int iCmd)
{
	static char pszDisplay[]="...............";
	static uchar ucDisplayCnt=0;
	static char pszChnMsg[32];
	static char pszEngMsg[32];
	uchar aucTitle[17];

	if (lastStep == 0xff) 
    {
		ScrCls();
		SCR_PRINT(0, 0, 0xC1, "      对拷      ", "   DUPLICATE    ");
	}
	
	switch(iStep)
	{
		case IDUPDOWNLOADINIT:
		{
            ScrClrLine(4, 5);
			SCR_PRINT(0, 4, 0x01,"初始化...","INITIALIZING...\n");
			break;
		}
        
		case IDUPDOWNLOADSHAKE:
		{
            ScrClrLine(4, 5);
			SCR_PRINT(0, 4, 0x01,"连接中...","CONNECTING...\n");
			break;
		}
        
		case IDUPDOWNLOADCHECKTERMINFO:
		case IDUPDOWNLOADSETBAUD:
		case IDUPDOWNLOADCHECKCONFTAB:
		case IDUPDOWNLOADPUK:
		case IDUPDOWNLOADFONTLIB:
		case IDUPDOWNLOADAPPFILE:
		{
			switch(iCmd)
			{
				case LD_CMDCODE_SETBAUTRATE :{strcpy(pszChnMsg,"设置波特率...");strcpy(pszEngMsg,"Set Baudrate...");break;}
				case LD_CMDCODE_GETPOSINFO  :{strcpy(pszChnMsg,"读取终端信息");strcpy(pszEngMsg,"Read TermInfo");break;}
				case LD_CMDCODE_GETAPPSINFO :{strcpy(pszChnMsg,"读取应用信息...");strcpy(pszEngMsg,"Read AppInfo...");break;}
				case LD_CMDCODE_REBUILDFS   :{strcpy(pszChnMsg,"重建文件系统...");strcpy(pszEngMsg,"Rebuild FS...");break;}
				case LD_CMDCODE_SETTIME     :{strcpy(pszChnMsg,"设置终端时间...");strcpy(pszEngMsg,"Set Time...");break;}
				case LD_CMDCODE_DLUPK       :{strcpy(pszChnMsg,"复制公钥...");strcpy(pszEngMsg,"Copying PUK...");break;}
				case LD_CMDCODE_DLAPP       :{strcpy(pszChnMsg,"复制应用...");strcpy(pszEngMsg,"Copying APP...");break;}
				case LD_CMDCODE_DLFONTLIB   :{strcpy(pszChnMsg,"复制字库...");strcpy(pszEngMsg,"Copying FONT...");break;}
				case LD_CMDCODE_DLPARA      :{strcpy(pszChnMsg,"复制参数...");strcpy(pszEngMsg,"Copying PARAM...");break;}
				case LD_CMDCODE_DLPUBFILE	:{strcpy(pszChnMsg,"复制公共文件...");strcpy(pszEngMsg,"Copying Public...");break;}
				case LD_CMDCODE_DELPUBFILE	:{strcpy(pszChnMsg,"删除公共文件...");strcpy(pszEngMsg,"Del Public...");break;}
				case LD_CMDCODE_DLFILEDATA :{break;}
				case LD_CMDCODE_DLFILEDATAC :{break;}
				case LD_CMDCODE_WRITEFILE   :{break;}//{strcpy(pszChnMsg,"保存文件...");strcpy(pszEngMsg,"Save File...");break;}
				case LD_CMDCODE_DELAPP      :
				case LD_CMDCODE_DELALLAPP:{strcpy(pszChnMsg,"删除应用...");strcpy(pszEngMsg,"Del App...");break;}
				case LD_CMDCODE_GETCONFTABLE:
				default:{strcpy(pszChnMsg,"正在复制...");strcpy(pszEngMsg,"Copying...");break;}
			}

            if(iCmd != gsLastCmd || lastStep!=iStep)
            {
    			pszChnMsg[16]=0;
    			pszEngMsg[16]=0;
                ScrClrLine(4, 5);
    			SCR_PRINT(0, 4, 0x01,pszChnMsg,pszEngMsg);
            }
            
			if(LD_CMDCODE_WRITEFILE==iCmd)
			{
                ScrClrLine(6, 7);
				SCR_PRINT(0, 6, 0x01,"正在保存...","Save File...");
			}
			else
			{
                if(iCmd != gsLastCmd || lastStep!=iStep)
                    ucDisplayCnt=0;
                
                if(ucDisplayCnt==0)
                    ScrClrLine(6, 7);
                else
    				ScrPrint(0, 6, 0x01,pszDisplay+(5-ucDisplayCnt)*3);
				ucDisplayCnt = (ucDisplayCnt+1)%6;
			}
			break;
		}

		case IDUPDOWNLOADFINISH:
		{
            ScrClrLine(4, 5);
			SCR_PRINT(0, 4, 0x01,"对拷成功!","Duplicate OK!");
			DelayMs(1000);//Delay 1s to display the success msg 

			break;
		}

		case IDUPDOWNLOADDONE:
		{
			strcpy(pszChnMsg,"对拷未完成!");
			strcpy(pszEngMsg,"Duplicate Fail!");

			switch(iCmd)
			{
				case RET_DUP_OK	                 :{return;}
				case RET_DUP_ERR	               :{break;}
				case RET_DUP_SHAKE_TIMEOUT       :{break;}
				case RET_DUP_RECV_TIMEOUT	       :
				case RET_DUP_RCV_DATA_TIMEOUT    :{strcpy(pszChnMsg,"接收响应超时");strcpy(pszEngMsg,"Rcv msg timeout");break;}
				case RET_DUP_LRC_ERR 	           :{strcpy(pszChnMsg,"接收报文LRC错");strcpy(pszEngMsg,"Rcv Lrc error.");break;}
				case RET_DUP_RCV_ERR	           :{break;}
				case RET_DUP_RCVBUFLEN_ERR	     :{break;}
				case RET_DUP_CHANGEBAUD_ERR      :{strcpy(pszChnMsg,"设置新波特率失败");strcpy(pszEngMsg,"Set new baud fail.");break;}
				case RET_DUP_MSG_ERROR	         :{break;}
				case RET_DUP_TERMINAL_NOT_MATCH :{strcpy(pszChnMsg,"终端信息不匹配");strcpy(pszEngMsg,"Term not match");break;}
				case RET_DUP_CHECKTERMINAL_FAIL	 :{strcpy(pszChnMsg,"读终端信息失败");strcpy(pszEngMsg,"Get terminfo fail");break;}
				case RET_DUP_CHECKCONFTAB_FAIL	 :{strcpy(pszChnMsg,"取配置表失败");strcpy(pszEngMsg,"Get cfgtab fail");break;}
				case RET_DUP_CONFITTAB_NOT_MATCH :	{strcpy(pszChnMsg,"配置表不一致");strcpy(pszEngMsg,"Cfgtab not match");break;}
				case RET_DUP_COPY_PUK_FAIL	     :{strcpy(pszChnMsg,"复制PUK失败");strcpy(pszEngMsg,"Copy PUK fail");break;}
				case RET_DUP_COPY_FONT_FAIL	 :{strcpy(pszChnMsg,"复制字库失败");strcpy(pszEngMsg,"Copy font fail");break;}
				case RET_DUP_COPY_FILE_FAIL	 :{strcpy(pszChnMsg,"复制应用文件失败");strcpy(pszEngMsg,"copy app fail");break;}
				case RET_DUP_DELALL_APP_FAIL	   :{strcpy(pszChnMsg,"删除应用失败");strcpy(pszEngMsg,"del app fail");break;}
				case RET_DUP_COPY_PUB_FAIL	 :{strcpy(pszChnMsg,"复制公共文件失败");strcpy(pszEngMsg,"copy Pub fail");break;}
			}
			ScrClrLine(4, 5);
			SCR_PRINT(0, 4, 0x01,pszChnMsg,pszEngMsg);
			DelayMs(1000);
			break;
		}

		case -1://display percent
		{
			char aucPercent[17];
			sprintf(aucPercent,"      %d%%",iCmd);
            if(lastStep!=iStep)
                ScrClrLine(6, 7);
			SCR_PRINT(0, 4, 0x01,pszChnMsg,pszEngMsg);
			SCR_PRINT(0, 6, 0x01,aucPercent,aucPercent);
			break;
		}
        
		default:
		{
			break;
		}
	}

	lastStep = iStep;
	return;
}

/**


 
 @param[in]
	 \li
	 \li
	 \li
 @param[out] 
 @param[in,out]  
  
 @retval 
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/08/18--1.0.0.0-------创建
 */



/**  @} */
