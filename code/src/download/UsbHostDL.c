
#include "UsbHostDL.h"
#include "..\fat\fsapi.h"

static int iPrnDLoadLog=0;//是否打印下载日志
static int iHadDLoadMonitor;//是否下载过监控
static int iHadDLoadFile;//标识是否下载过
static int iHadDLoadBaseso;// 下载了baseso, 需要重启
static int iHadDLoadPuk;// 下载了puk, 需要重启
static uchar szNodeInfo[(LONG_NAME_MAX + 4 + 14)*100];//支持一个U盘100个打包文件的读，文件越多肯定会比较慢
extern uchar            bIsChnFont;
extern uchar WaitKeyBExit(int usTimeout,uchar ucDefaultKey);
extern SetLineRever();
extern uchar GetKeyMs(uint Ms);
extern int s_iVerifySignature(int iType,uchar * pucAddr,long lFileLen,tSignFileInfo *pstSigInfo);
extern  int s_iGetAppName(char * pszAppName);

int GetAppId(uchar * ptrAppName)
{
	uchar ucAppId =0;
	APPINFO ai;
	
	while (1)
	{
		ReadAppInfo(ucAppId,&ai);

		if (memcmp(ai.AppName,ptrAppName,strlen(ptrAppName)) == 0 && strlen(ptrAppName) == strlen(ai.AppName))
		{
			return ai.AppNum;
		}

		if (ucAppId > 24)
		{
			return -1;
		}
		ucAppId++;
	}

}

void BeepOK()
{
	/*~~~~~~*/
	uchar	iCnt;
	/*~~~~~~*/

	for(iCnt = 0; iCnt < 3; iCnt++)
	{
		//Beepf(3, 60);
		DelayMs(80);
	}
}

/*********************
terminal beep for the fail operation
*********************/

void BeepFail()
{
	//Beepf(6, 200);
	DelayMs(200);	
}


extern void SCR_PRINT(uchar x, uchar y, uchar mode, uchar *HanziMsg, uchar *EngMsg);
static void ShowDLOver()
{
	ScrCls();
	SetLineRever(0,1);		
    SCR_PRINT(0,0,0x81,"     U盘下载    "," U-DISC-DOWNLOAD");
	SCR_PRINT(0,5,0x01,"  下载完成!",  "DOWNLOAD OVER!");	
	BeepOK();
	DelayMs(2000);

	if (iHadDLoadMonitor || iHadDLoadBaseso || iHadDLoadPuk)
	{
		ScrClrLine(2,7);
		SCR_PRINT(0,4,0x01,"已下载监控/公钥，请重新启动!","MONITOR/PUK HAD UPDATED,PLS RESTART!");
        kbflush();
		getkey();
		ScrCls();
		Soft_Reset();
	}
}
const struct T_ERRFS
{
	int iID;
	uchar * szErrStrEN;
	uchar * szErrStrCN;
}tErrFs[] = {
1,"TOO MANY FILES","文件数量超标",
2,"VERIFY MONT FAIL","监控验证失败",
//3,"VERIFY APP FAIL","应用验证失败",
3, "Verify SIG fail","验证签名失败",
4,"FILE TOO LARGE","文件大小超标",
5,"NEED SPACE","文件系统空间不足",
6,"APP TOO MORE","应用程序过多",
7,"READ UDISC FAIL","U盘读写失败",
8,"SAVE FILE FAIL","文件保存失败",
9,"LRC WRONG","LRC验证错误",
10,"APP DUPLICATED","不同类型应用重名",
11,"APP TYPE ERR.","应用类型错",
12,    "NO TERM EXIST","指定终端不存在",
13,    "PCK NO SW","指定终端无文件",
14,"APP PCK ERR","应用包错误",
15,"APP PCK BEYOND","应用包过多",
16,"INVALID PCK","非法下载包",
0,	"OPER OK"            	,"表示没有错误",
-5,	"DEVICE NOT EXIST"     	,"设备不存在",
-6,	"RE-OPEN"      	,"节点忙",
-7,	"PRAMA ERR."       	,"错误参数，非法参数",
-8,	"NO FILE FIND"       	,"未找到指定文件",
-9,	"NAME TOO LONG" 	,"名字太长",
-10,	"NAME EXISTED"	,"名字已存在",
-11,	"ILLEGAL CHAR"      	,"存在非法字符",
-13,	"NO SPACE" 	,"存储空间不够",
-14,	"ROOT FULL" 	,"根目录表项已满",
-16,	"PATH TOO DEEP"   	,"路径太深",
-17,	"NODE NO EXIST"  	,"不存在该节点",
-20,	"NEDD MEMEORY"  	,"内存不够",
-21,	"BAD HANDLE"  	,"错误句柄",
-22,	"EMPTY DIR"	,"目录不为空",
-23,	"WRITE ONLY"     	,"节点属性只写",
-24,	"READ ONLY"     	,"节点属性只读",
-25,	"NOT SUPPORT"    	,"系统不支持该功能",
0xffffffff,"UNKNOWN ERR.","未知错误"
};

uchar DatBinToAsc(uchar *ptr, ulong inBin, uchar len)
{
	ptr += len - 1;
	while(len-- > 0)
	{
		*ptr = (uchar) inBin % 10 + '0';
		inBin /= 10;
		ptr--;
	}

	return(uchar) inBin;
}

void DatBcdToAsc(uchar *Asc, uchar *Bcd, int Asc_len)
{
	/*~~~~~~~~~~~~~*/
	uchar	is_first;
	uchar	by;
	/*~~~~~~~~~~~~~*/

	is_first = (Asc_len % 2);				/* change by wxk 98.11.06 */

	while(Asc_len-- > 0)
	{
		if(is_first)
		{
			by = *Bcd & 0x0f;
			Bcd++;
		}
		else
		{
			by = *Bcd >> 4;
		}

		by += (by >= 0x0a) ? 0x37 : '0';	/* 0x37 = 'A' - 0x0a */

		*Asc++ = by;
		is_first = !is_first;
	}
}


void GetSysTime(uchar * ptrTime)
{
	/*~~~~~~~~~~~~~~~~~*/
	uchar	szDatetime[20];
	/*~~~~~~~~~~~~~~~~~*/

	GetTime((uchar *) szDatetime);

	DatBcdToAsc(ptrTime, (uchar *) szDatetime, 12);
}

//显示错误信息,记录错误日志udisodload_err
static void ShowFsErr(int iErrCode)
{
	int iLogFile;
	uchar szErrInfo[50],ucIdx,tmpbuf[128];
	ScrCls();
	SetLineRever(0,CFONT);
	SCR_PRINT(0,0,0x41|0x80,"    异常信息    "," EXCEPTION INFO ");
	memset(szErrInfo,0,sizeof(szErrInfo));
    memset(tmpbuf,0,sizeof(tmpbuf));

	for (ucIdx = 0; ucIdx < sizeof(tErrFs)/sizeof(struct T_ERRFS);ucIdx++)
	{//最后一位的数据拷贝
		if (tErrFs[ucIdx].iID == iErrCode|| (ucIdx + 1) == sizeof(tErrFs)/sizeof(struct T_ERRFS))
		{
			if (bIsChnFont)
			{
				strcpy(szErrInfo,tErrFs[ucIdx].szErrStrCN);
			}
			else
			{
				strcpy(szErrInfo,tErrFs[ucIdx].szErrStrEN);
			}
			break;
		}
	}
	sprintf(tmpbuf, "err:%s\n", szErrInfo);
	iLogFile = s_open(ERR_LOG_FILE,O_CREATE|O_RDWR,"\xff\x05");
	seek(iLogFile,0,SEEK_END);
	write(iLogFile,tmpbuf,strlen(tmpbuf));
	close(iLogFile);
	ScrPrint(0,4,CFONT,"%s[%d]",szErrInfo,iErrCode);
	BeepFail();
	WaitKeyBExit(600,NOKEY);
}

void UDiscDLlog(uchar *str)
{
	int iLogFile;
	uchar tmpbuf[64];

	memset(tmpbuf, 0x00, sizeof(tmpbuf));	
	iLogFile = s_open(ERR_LOG_FILE,O_CREATE|O_RDWR,"\xff\x05");
	seek(iLogFile,0,SEEK_END);
	sprintf(tmpbuf, "%s\n", str);
	write(iLogFile,tmpbuf,strlen(tmpbuf));
	close(iLogFile);
}

static void WriteLogtoUDisc(uchar * ptrPSWName)
{
	FS_W_STR FsStr;	
	uchar * ptrReadLog = (uchar *)MAPP_RUN_ADDR, tmpbuf[64],ucRet;
	int iUsbId,iFileId,iRet,len;
	int iFileNameLen = strlen(ptrPSWName);
	int iFileSize = filesize(ERR_LOG_FILE);

	if (iFileNameLen < 3) return;
	if (iFileSize <= 0) return;

    len=0;
    memset(tmpbuf,0x00, sizeof(tmpbuf));
    sprintf(tmpbuf, "%s\n",ptrPSWName);
    len = strlen(tmpbuf);
    memcpy(ptrReadLog, tmpbuf, len);
    /*read local log*/
	if((iFileId = s_open(ERR_LOG_FILE,O_RDWR,"\xff\x05")) < 0)
	{
		return;
	}

	seek(iFileId,0,SEEK_SET);
	if (read(iFileId,ptrReadLog+len,iFileSize) != iFileSize)
	{
        close(iFileId);
		return;
	}
	close(iFileId);
	len += iFileSize;
	
    /*add current time*/
    memset(tmpbuf, '*', 24);
    GetSysTime(tmpbuf+6);
    tmpbuf[24] = '\n';
    tmpbuf[25] = '\n';
    memcpy(ptrReadLog+len, tmpbuf, 26);
    len += 26;

    /*write download log into Udisk*/	
	FsStr.fmt = NAME_FMT_ASCII;
	FsStr.size = iFileNameLen;
	FsStr.str = ptrPSWName;
	memcpy(FsStr.str + iFileNameLen - 3,"log",3);
	
	if((iUsbId = FsOpen(&FsStr,FS_ATTR_C | FS_ATTR_W)) < 0)
	{
		ShowFsErr(iUsbId);
		return;
	}
    FsFileSeek(iUsbId,0,FS_SEEK_END);

	if (( iRet = FsFileWrite(iUsbId,ptrReadLog,len)) < 0)
	{
		ShowFsErr(iRet);
		return;
	}

	if ((iRet = FsClose(iUsbId)) < 0)
	{
		ShowFsErr(iRet);
		return;
	}

	/*print the download log*/
	iRet =0;
	if (iPrnDLoadLog)
	{
	    while(1)
	    {
            iRet = PrnUdiskDLInfo(ptrReadLog,len);
            if(0==iRet) break;
        	ScrCls();
        	SetLineRever(0,1);		
            SCR_PRINT(0,0,0x81,"  打印下载日志 ","PRN Download Log");	
            SCR_PRINT(0,2,0,  "   取消打印?  ","Cancle Prn Log?");
            SCR_PRINT(0,4,0,  "【确认】 - 是 ","[ENTER] - YES");
            SCR_PRINT(0,6,0,  " 其他键  - 否 ","Other Key - NO");
            kbflush();
            ucRet = GetKeyMs(60000);
            if(ucRet== KEYENTER) break;
	    }
	}    
}


uchar DatCalLrc(uchar *ptr, int len)
{
	/*~~~~~~~~*/
	uchar	lrc;
	/*~~~~~~~~*/

	lrc = 0;

	while(len-- > 0)
	{
		lrc ^= *ptr++;
	}

	return lrc;
}

static int CheckIfValidPck(uchar * ptrScrName,uchar * ptrDstName)
{
	FS_W_STR fsWStr;
	int iFid,iRet;

	fsWStr.fmt = NAME_FMT_ASCII;
	fsWStr.size = strlen(ptrScrName);
	fsWStr.str = ptrScrName;

	if((iFid = FsOpen(&fsWStr,FS_ATTR_R)) < 0) 
	{
	    FsClose(iFid);
		return iFid;
	}

	if ((iRet = FsFileSeek(iFid,0x00,FS_SEEK_SET)) != 0)
	{
		FsClose(iFid);
		return iRet;
	}

	if((iRet = FsFileRead(iFid,ptrDstName,12)) != 12)
	{
		FsClose(iFid);
		return iRet;
	}
	
	if (memcmp(ptrDstName,"PAX-LOAD-PKG",12) == 0)
	{
		if ((iRet = FsFileSeek(iFid,0x0e,FS_SEEK_SET)) != 0)
		{
			FsClose(iFid);
			return iRet;
		}

		if((iRet = FsFileRead(iFid,ptrDstName,14)) != 14)
		{
			FsClose(iFid);
			return iRet;
		}	
		FsClose(iFid);
		return 0;
	}

	FsClose(iFid);
	return 1;
}


//获取了个数，并把对应的所有内容读到一个内存区域
static int GetPSWNumber(int iFid)
{
	int iReadNum,iRetNum = 0,iRealOne,iCnt;
	uchar szAttr[5],szShowPckName[15];
	FS_INODE_INFO fsInodeInfo[10];//一次读10个
	long lOffset = 0;
	memset(szNodeInfo,0,(LONG_NAME_MAX + 5)*100);
	memset((char *)&fsInodeInfo,0,sizeof(fsInodeInfo));

	while (1)
	{
		iReadNum = FsDirRead(iFid,fsInodeInfo,10);//按顺序读取
		if (iReadNum <= 0) break;
        iRealOne = 0;
        for (iCnt = 0; iCnt < iReadNum;iCnt++)
        {        
            memset(szAttr,0,5);
            if(strlen(fsInodeInfo[iCnt].name) <= 4) continue;
            memcpy(szAttr,fsInodeInfo[iCnt].name + strlen(fsInodeInfo[iCnt].name) - 4,4);
            UpperCase((char*)szAttr, (char*)szAttr, 4);
            if (memcmp(szAttr,".PCK",4) == 0 && fsInodeInfo[iCnt].size < 0x2000000)//必须小于32M
            {
                memset(szShowPckName,0,15);
                if (!CheckIfValidPck(fsInodeInfo[iCnt].name,szShowPckName))
                {
                    memcpy(szNodeInfo + lOffset,fsInodeInfo[iCnt].name,LONG_NAME_MAX + 4);//读到内存中
                    memcpy(szNodeInfo + lOffset + LONG_NAME_MAX + 4,szShowPckName,14);//读到内存中
                    iRealOne++;
                    lOffset += (LONG_NAME_MAX + 5 + 14);
                }
            }
        }
        iRetNum += iRealOne;

	}

	if (iRetNum == 0)
	{
		return iReadNum;
	}
	
	return iRetNum;
}

static int s_IsSoFile(uint addr)
{
	Elf32_Ehdr head;

    memcpy(&head,(uchar *)addr,sizeof(head));
	if(memcmp(head.e_ident, ELFMAG, SELFMAG) == 0 && head.e_type == ET_DYN)
		return 1;
	return 0;
}

static int s_WriteUserFile(char *szFileName,uchar *addr,uint size,uchar attr)
{
    uchar szAttr[2];
    int iFileId;
    
	szAttr[0] = attr;
	szAttr[1] = 0x04&0xff;

	s_remove(szFileName, szAttr);

	iFileId = s_open(szFileName,O_CREATE|O_RDWR,szAttr);

	if (iFileId <= 0)return 8;

	seek(iFileId,0,SEEK_SET);

	if (write(iFileId,(uchar*)addr,size) != size)
	{
        close(iFileId);
		return 8;
	}
	close(iFileId);
    
    return 0;
}

static int s_WriteEnvFile(uchar *addr,uint size,uchar attr)
{
    uchar szAttr[2];
    int iFileId;

	szAttr[0] = attr;
	szAttr[1] = 0x06&0xff;

	s_remove(ENV_FILE, szAttr);
	
	iFileId = s_open(ENV_FILE,O_CREATE|O_RDWR,szAttr);

	if (iFileId <= 0)
	{
		close(iFileId);	
		return 8;
	}

	seek(iFileId,0,SEEK_SET);
	if (write(iFileId,(uchar*)addr,size) != size)
	{
		close(iFileId);	
		return 8;
	}
	close(iFileId);		

    return 0;
}

static int s_WriteSoFile(uchar *addr,uint size,uchar attr)
{
    uchar soname[17],szAttr[2];
    SO_HEAD_INFO head;
    int ret,SignType,iFileId;

    memcpy(&head,(uchar *)(addr+SOINFO_OFFSET),sizeof(head));
    memcpy(soname,head.so_name,sizeof(soname)-1);
    soname[16] = '\0';

    if(head.lib_type == LIB_TYPE_SYSTEM)
    {
    	SignType = SIGN_FILE_MON;
    }
    else
    {
    	SignType = SIGN_FILE_APP;
    }

    if(!CheckIfDevelopPos())
    {
    	ret = s_iVerifySignature(SignType, (uchar*)addr, size, NULL);	
    	if(ret)return 3;
    }

    szAttr[0] = attr;
    if(head.lib_type == LIB_TYPE_SYSTEM)
    	szAttr[1] = FILE_TYPE_SYSTEMSO;
    else
    	szAttr[1] = FILE_TYPE_USERSO;

    ret = s_remove(soname, szAttr);
    iFileId = s_open(soname,O_CREATE|O_RDWR,szAttr);
    if(iFileId < 0)return 8;
    if (write(iFileId,(uchar*)addr,size) != size)
    {
        close(iFileId);
    	return 8;
    }
    close(iFileId);

    return 0;
}

static int s_ParsePrgPck(uchar *BaseAddr,TERM_DESCRIB_TABLE*tab)
{
    uchar szGetBuf[5],ucCnt,tbuf[100];
    uchar *ptrLoadBaleAddr;
    uint fnums,uiTemp;

#define BYTE2LONG(buf)	\
		((unsigned long)buf[0]<<24 | (unsigned long)buf[1] << 16 | (unsigned long)buf[2] << 8 | buf[3])


    ptrLoadBaleAddr = BaseAddr;
    if(memcmp(ptrLoadBaleAddr,"PAX-APP-PKG",strlen("PAX-APP-PKG")))
    {
        ShowFsErr(14);
        return -1;
    }
    
    memcpy(szGetBuf,ptrLoadBaleAddr+0x14,2);
	fnums = (szGetBuf[0]<<8)|szGetBuf[1];//file nums
    if(fnums==0)
    {
        ShowFsErr(14);
        return -2;
    }

    ptrLoadBaleAddr+=0x40;
    for(ucCnt=0;ucCnt<fnums;ucCnt++)
    {
        memset(&tab[ucCnt],0,sizeof(tab[ucCnt]));
        if(memcmp(ptrLoadBaleAddr,"tagstart",strlen("tagstart")))
        {
            ShowFsErr(14);
            return -3;
        }
        ptrLoadBaleAddr+=16+4;

        if(memcmp(ptrLoadBaleAddr,"filename",strlen("filename")))
        {
            ShowFsErr(14);
            return -4;
        }
        ptrLoadBaleAddr+=16;//+filename
        memcpy(szGetBuf,ptrLoadBaleAddr,4);
        uiTemp = BYTE2LONG(szGetBuf);
        ptrLoadBaleAddr+=4;//+lenght
        memcpy(tab[ucCnt].szFileName,ptrLoadBaleAddr,uiTemp);
        ptrLoadBaleAddr+=uiTemp;//+value

        if(memcmp(ptrLoadBaleAddr,"filetype",strlen("filetype")))
        {
            ShowFsErr(14);
            return -5;
        }
        ptrLoadBaleAddr+=16;//filetype
        memcpy(szGetBuf,ptrLoadBaleAddr,4);
        uiTemp = BYTE2LONG(szGetBuf);
        ptrLoadBaleAddr+=4;
        memcpy(tbuf,ptrLoadBaleAddr,uiTemp);
        tbuf[uiTemp]=0;
        if(memcmp(tbuf,"static_bin",strlen("static_bin"))==0)
        {
            tab[ucCnt].ucFileType = PRG_FILE_STATIC_BIN;
        }
        else if(memcmp(tbuf,"dynamic_bin",strlen("dynamic_bin"))==0)
        {
            tab[ucCnt].ucFileType = PRG_FILE_DYNAMIC_BIN;
        }
        else if(memcmp(tbuf,"dynamic_so",strlen("dynamic_so"))==0)
        {
            tab[ucCnt].ucFileType = PRG_FILE_DYNAMIC_SO;
        }
        ptrLoadBaleAddr+=uiTemp;//+value
        
        if(memcmp(ptrLoadBaleAddr,"offset",strlen("offset")))
        {
            ShowFsErr(14);
            return -5;
        }
        ptrLoadBaleAddr+=16;//offset
        ptrLoadBaleAddr+=4;//length
        memcpy(szGetBuf,ptrLoadBaleAddr,4);
        tab[ucCnt].uiFileAddr = BYTE2LONG(szGetBuf)+(uint)BaseAddr;
        ptrLoadBaleAddr+=4;//value

        if(memcmp(ptrLoadBaleAddr,"filelen",strlen("filelen")))
        {
            ShowFsErr(14);
            return -6;
        }
        ptrLoadBaleAddr+=16;//filelen
        ptrLoadBaleAddr+=4;//length
        memcpy(szGetBuf,ptrLoadBaleAddr,4);
        tab[ucCnt].uiFileLen = BYTE2LONG(szGetBuf);
        ptrLoadBaleAddr+=4;//value
        if(tab[ucCnt].uiFileLen==0)
        {
            ShowFsErr(14);
            return -7;
        }

        if(memcmp(ptrLoadBaleAddr,"tagend",strlen("tagend")))
        {
            ShowFsErr(14);
            return -8;
        }
        ptrLoadBaleAddr+=16+4;
    }

    return fnums;
}

static int WriteAppPack(TERM_DESCRIB_TABLE tAimFileInfo,uchar PckType)
{
	uchar * ptrLoadBaleAddr = (uchar*)MAPP_RUN_ADDR;
	uchar ucAppNum,szGetBuf[5],ucCnt,ucDelFileFlag = 0,szFileName[17];
	uchar ucInCnt,ucMaxNum,szAttr[2],ucUseFNum,fNums,tbuf[100];
	int  iTargetAppNo,iAppLnOffSet = 0,iRet,i,index;
	uint uiWriteAddr,uiWriteLen;
	FILE_INFO   FileInfo[MAX_FILES];
    TERM_DESCRIB_TABLE tab[MAX_FILES];
	ucAppNum = *(uchar *)(ptrLoadBaleAddr + tAimFileInfo.uiFileAddr);

	for (ucCnt = 0; ucCnt < ucAppNum; ucCnt++)
	{
		iTargetAppNo = -1;
		uiWriteAddr = 0;
		uiWriteLen = 0;
		//得到应用的地址，应用的名称
		memset(szGetBuf,0,5);
		memcpy(szGetBuf,ptrLoadBaleAddr + tAimFileInfo.uiFileAddr + iAppLnOffSet + 0x10,4);
		uiWriteAddr = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		if (uiWriteAddr == 0)
		{
			ShowFsErr(14);
			return -1;//错误包
		}
		uiWriteAddr += (uint)ptrLoadBaleAddr;//最终在内存里的偏移地址
		
		memset(szGetBuf,0,5);
		memcpy(szGetBuf,ptrLoadBaleAddr + tAimFileInfo.uiFileAddr + iAppLnOffSet + 0x14,4);
		uiWriteLen = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		if (uiWriteLen == 0)
		{
			ShowFsErr(14);
			return -1;//错误包
		}

        if(PckType==PRG_TYPE)
		{
            iRet = s_ParsePrgPck((uchar *)uiWriteAddr, tab);
            if(iRet<=0)
            {
                ShowFsErr(14);
                return -1;
            }
            fNums = iRet;
            GetTermInfo(tbuf);
            for(i=0,index=-1;i<fNums;i++)
            {
                if(tab[0].ucFileType==PRG_FILE_STATIC_BIN)
                {
                    if(index<0)index=i;
                    if((tbuf[19]&0x04)==0)//monitor
                        break;
                }

                if(tab[i].ucFileType==PRG_FILE_DYNAMIC_BIN)
                {
                    if(tbuf[19]&0x04)//monitor plus
                    {
                        index=i;
                        break;
                    }
                }
            }
        	if(index<0)
        	{
                ShowFsErr(14);
                return -1;
            }
            uiWriteAddr = tab[index].uiFileAddr;
            uiWriteLen = tab[index].uiFileLen;
        }
        iTargetAppNo = s_iDLWriteApp((uchar*)uiWriteAddr, uiWriteLen);
		if(iTargetAppNo < 0)
		{		
    		if(iTargetAppNo==-1 || iTargetAppNo==-8) ShowFsErr(3);
    		else if(iTargetAppNo == -2) ShowFsErr(11);
    		else if(iTargetAppNo==-3 || iTargetAppNo==-4) ShowFsErr(10);
    		else if(iTargetAppNo == -5) ShowFsErr(6);						
    		else ShowFsErr(8);
    		
    		return -2;
        }        
		//是否删除其参数文件和用户文件

		ucDelFileFlag = *(char *)(ptrLoadBaleAddr + tAimFileInfo.uiFileAddr + 0x18 + iAppLnOffSet);

		if(ucDelFileFlag)
		{
			ScrClrLine(4,7);
			SCR_PRINT(0,4,1,"删除用户文件...","DEL USER FILE...");	
			memset((char *)&FileInfo, 0x00, sizeof(FileInfo));
			ucMaxNum = GetFileInfo(FileInfo);
			for(ucInCnt = 0; ucInCnt<ucMaxNum; ucInCnt++)
			{
				if(FileInfo[ucInCnt].attr == iTargetAppNo)     //  属于这个应用的附属文件
				{
					szAttr[0] = FileInfo[ucInCnt].attr;
					szAttr[1] = FileInfo[ucInCnt].type;
					s_remove(FileInfo[ucInCnt].name, szAttr);
				}
			}
		}
		//下载参数文件

		memset(szGetBuf,0,5);
		memcpy(szGetBuf,ptrLoadBaleAddr + iAppLnOffSet + tAimFileInfo.uiFileAddr + 0x19,4);
		uiWriteAddr = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		if (uiWriteAddr != 0)
		{
			uiWriteAddr += (uint)ptrLoadBaleAddr;//最终在内存里的偏移地址
		}

		memset(szGetBuf,0,5);
		memcpy(szGetBuf,ptrLoadBaleAddr + iAppLnOffSet + tAimFileInfo.uiFileAddr + 0x1d,4);
		uiWriteLen = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		
		if (uiWriteLen != 0 && uiWriteAddr !=0) //不存在
		{
			ScrClrLine(4,7);
			SCR_PRINT(0,4,1,"写入参数文件...","WRITE ENV FILE...");	
            iRet = s_WriteEnvFile((uchar *)uiWriteAddr, uiWriteLen, iTargetAppNo);
            if(iRet)
            {
                ShowFsErr(iRet);
                return -3;
            }
		}		

		//下载用户文件
		ScrClrLine(4,5);
		SCR_PRINT(0,4,1,"写入用户文件...","WRITE USER FILE...");	

		ucUseFNum = *(char *)(ptrLoadBaleAddr + tAimFileInfo.uiFileAddr + 0x21 + iAppLnOffSet);

		for (ucInCnt = 0; ucInCnt < ucUseFNum;ucInCnt ++)
		{
			memset(szFileName,0,17);
			memcpy(szFileName,ptrLoadBaleAddr + tAimFileInfo.uiFileAddr + 0x22 + iAppLnOffSet,16);
			memset(szGetBuf,0,sizeof(szGetBuf));
			memcpy(szGetBuf,ptrLoadBaleAddr + iAppLnOffSet + tAimFileInfo.uiFileAddr + 0x32,4);
			uiWriteAddr = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
			if (uiWriteAddr == 0)
			{
				ShowFsErr(14);
				return -1;//错误包
			}
			uiWriteAddr += (uint)ptrLoadBaleAddr;//最终在内存里的偏移地址

			memset(szGetBuf,0,5);
			memcpy(szGetBuf,ptrLoadBaleAddr + iAppLnOffSet + tAimFileInfo.uiFileAddr + 0x36,4);
			uiWriteLen = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];

			if(s_IsSoFile(uiWriteAddr))
			{
                iRet = s_WriteSoFile((uchar *)uiWriteAddr, uiWriteLen, iTargetAppNo);
                if(iRet)
                {
                    ShowFsErr(iRet);
                    return -4;
                }
            }
			else
			{
                iRet = s_WriteUserFile((uchar *)szFileName, (uchar *)uiWriteAddr, uiWriteLen, iTargetAppNo);
                if(iRet)
                {
                    ShowFsErr(iRet);
                    return -5;
                }
            }
			iAppLnOffSet += 24;
		}
		//--移动偏移
		iAppLnOffSet +=  0x12;//第二个是0x22+
		//FIXME！！
        if(tab[index].ucFileType==PRG_FILE_DYNAMIC_BIN)
        {
            for(i=0;i<fNums;i++)
            {
                if(tab[i].ucFileType==PRG_FILE_DYNAMIC_SO)
                {
                    iRet = s_WriteSoFile((uchar *)tab[i].uiFileAddr, tab[i].uiFileLen, iTargetAppNo);
                    if(iRet)
                    {
                        ShowFsErr(iRet);
                        return -4;
                    }
                }
            }
        }

	}

	ScrClrLine(2,7);
	SCR_PRINT(0,4,1,"下载应用包成功","LOAD APPCK OK");	
	iHadDLoadFile = 1;
	return 0;
}

static int WriteAimFile(TERM_DESCRIB_TABLE tAimFileInfo)
{
	uchar * ptrLoadBaleAddr = (uchar*)MAPP_RUN_ADDR;
	tSignFileInfo stSigInfo;
	int iFileId, ret, fd, SignType;
	uchar ucCnt, szAttr[2], soname[17], tmpbuf[64];
	SO_HEAD_INFO head;

	ScrCls();
	SetLineRever(0,1);	

    memset(tmpbuf, 0x00, sizeof(tmpbuf));
	if (tAimFileInfo.uiFileAddr == 0 && tAimFileInfo.uiFileLen == 0 && tAimFileInfo.ucFileType == 0 )
	{
		SCR_PRINT(0,0,0x81,"    删除     ","   DELETE FILE  ");		

		for (ucCnt = 0; ucCnt < 24; ucCnt++)
		{
			DeleteApp(ucCnt,1);
		}
		SCR_PRINT(0,4,1,"删除应用成功","DELETE APP OK");	
		return 0;
	}

	
	SCR_PRINT(0,0,0x81,"    下载文件    ","  DOWNLOAD FILE ");		

	switch(tAimFileInfo.ucFileType) 
	{
	case MONITOR_TYPE:
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"验证签名...","VERIFY SIGN...");

		//验证监控签名
        if (s_iVerifySignature(SIGN_FILE_MON,ptrLoadBaleAddr + tAimFileInfo.uiFileAddr,tAimFileInfo.uiFileLen, NULL))
		{
			ShowFsErr(2);
			return -1;
		}
		SCR_PRINT(0,4,1,"保存文件... ","SAVE FILE...");
		ret = WriteMonitor(ptrLoadBaleAddr + tAimFileInfo.uiFileAddr, tAimFileInfo.uiFileLen);
		if (ret)
		{
		    ShowFsErr(8);
		    return -1;
		}   

		iHadDLoadMonitor = 1;

		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"下载监控成功","LOAD MONITOR OK");	
		iHadDLoadFile = 1;
		break;
	case FONT_TYPE:

		if((freesize()+s_filesize(FONT_FILE_NAME,(uchar *)"\xff\x02"))< tAimFileInfo.uiFileLen)
		{
            ShowFsErr(5);
			return -2;
		}

		s_remove(FONT_FILE_NAME,(uchar *)"\xff\x02");
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"保存文件...","SAVE FILE...");
		iFileId = s_open((char*)FONT_FILE_NAME,O_CREATE|O_RDWR,(uchar *)"\xff\x02");
		
		if(iFileId<0)
		{
            ShowFsErr(0xffffffff);
			return -3;
		}

		seek(iFileId,0,SEEK_SET);
		
		if(tAimFileInfo.uiFileLen!=write(iFileId,ptrLoadBaleAddr + tAimFileInfo.uiFileAddr,tAimFileInfo.uiFileLen))
		{
			close(iFileId);
			s_remove((char*)FONT_FILE_NAME,(uchar *)"\xff\x02");
			return -4;
		}
		
		close(iFileId);
		LoadFontLib();//重新加载字库

		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"下载字库成功","LOAD FONT OK");	
		iHadDLoadFile = 1;
		break;
	case UAPUK_TYPE:
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"保存文件...","SAVE FILE...");
		ret = s_WritePuk(ID_UA_PUK,ptrLoadBaleAddr + tAimFileInfo.uiFileAddr,tAimFileInfo.uiFileLen);
        if(ret) return -5;
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"下载UA_PUK成功","LOAD UA_PUK OK");	
		iHadDLoadFile = 1;
		iHadDLoadPuk = 1;
		break;
	case USPUK_TYPE:
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"保存文件...","SAVE FILE...");
		ret = s_WritePuk(ID_US_PUK,ptrLoadBaleAddr + tAimFileInfo.uiFileAddr,tAimFileInfo.uiFileLen);
        if(ret) return -6;
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"下载US_PUK成功","LOAD US_PUK OK");	
		iHadDLoadFile = 1;
		iHadDLoadPuk = 1;
		break;
	case APPCK_TYPE://下载应用包
		ret = WriteAppPack(tAimFileInfo,APPCK_TYPE);
		if(ret) return -7;
		break;
    case PRG_TYPE:
        ret = WriteAppPack(tAimFileInfo,PRG_TYPE);
        if(ret) return -8;
        break;
	case SOF_TYPE:
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"验证文件...","VERIFY FILE...");

        memcpy(&head,ptrLoadBaleAddr +tAimFileInfo.uiFileAddr,sizeof(head));
		if(!strstr(head.so_name, FIRMWARE_SO_NAME))
		{
            memcpy(&head,ptrLoadBaleAddr+tAimFileInfo.uiFileAddr+SOINFO_OFFSET,sizeof(head));
		}

		if(head.lib_type == LIB_TYPE_SYSTEM)
		{
			SignType = SIGN_FILE_MON;
		}
		else
		{
			SignType = SIGN_FILE_APP;
		}

		if(!CheckIfDevelopPos())
		{
			ret = s_iVerifySignature(SignType, (uchar*)(ptrLoadBaleAddr +tAimFileInfo.uiFileAddr), tAimFileInfo.uiFileLen, NULL);	
			if(ret)
			{
				ShowFsErr(3);
				return -9;
			}
		}

		memcpy(soname,head.so_name,sizeof(soname)-1);
		soname[16] = '\0';

		if(strstr(soname, FIRMWARE_SO_NAME))
		{
			iHadDLoadBaseso = 1;
		}

		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"保存文件...","SAVE FILE...");

		szAttr[0] = FILE_ATTR_PUBSO;
		if(head.lib_type == LIB_TYPE_SYSTEM)
			szAttr[1] = FILE_TYPE_SYSTEMSO;
		else
			szAttr[1] = FILE_TYPE_USERSO;

		s_remove(soname, szAttr);
		fd = s_open(soname, O_CREATE|O_RDWR, szAttr);
		if(fd < 0)
		{
			ShowFsErr(8);
			return -10;
		}
		ret = write(fd, (uchar*)(ptrLoadBaleAddr +tAimFileInfo.uiFileAddr), tAimFileInfo.uiFileLen);
		close(fd);
		if(ret != tAimFileInfo.uiFileLen)
		{
			ShowFsErr(8);
			return -11;
		}

		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"下载公有库成功","LOAD PUBLIC SO OK");	
		iHadDLoadFile = 1;
		break;
	case PUBF_TYPE:
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"保存文件...","SAVE FILE...");

		szAttr[0] = FILE_ATTR_PUBFILE;
		szAttr[1] = FILE_TYPE_USER_FILE;

		s_remove(tAimFileInfo.szFileName, szAttr);

		fd = s_open(tAimFileInfo.szFileName, O_CREATE|O_RDWR, szAttr);
		if(fd < 0)
		{
			ShowFsErr(8);
			return -12;
		}
        
		ret = write(fd, (uchar*)(ptrLoadBaleAddr +tAimFileInfo.uiFileAddr), tAimFileInfo.uiFileLen);
		close(fd);
		if(ret != tAimFileInfo.uiFileLen)
		{
			ShowFsErr(8);
			return -13;
		}

		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"下载公有文件成功","LOAD PUBLIC FILE OK");	
		iHadDLoadFile = 1;
		break;
	case BASEDRIVER_TYPE:	
		if(!is_hasbase())
		{
			ScrClrLine(2,7);
			ShowFsErr(-25);				// 不支持该功能
			return;
		}
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"验证签名...","VERIFY SIGN...");

		//验证监控签名
		if (s_iVerifySignature(SIGN_FILE_MON,ptrLoadBaleAddr + tAimFileInfo.uiFileAddr,tAimFileInfo.uiFileLen,NULL))
		{
			ShowFsErr(2);
			return;
		}
		SCR_PRINT(0,4,1,"发送文件... ","SEND FILE...");
		ret = WriteBaseDriver(ptrLoadBaleAddr + tAimFileInfo.uiFileAddr, tAimFileInfo.uiFileLen,1);
		if(ret)
		{
			ShowFsErr(8);
			return;
		}
		ScrClrLine(2,7);
		SCR_PRINT(0,4,1,"下载驱动成功","LOAD DRIVER OK");	
		DelayMs(1000);
		iHadDLoadFile = 1;
		break;
	default:
		break;
	}
	return 0;
}

static int DLoadBaleFile(char * ptrFileName)
{
#define TAB_NUMS    0xff    
#define READ_SIZE   (32*1024)
#define BYTE2LONG(buf)	\
            ((unsigned long)buf[0]<<24 | (unsigned long)buf[1] << 16 | (unsigned long)buf[2] << 8 | buf[3])

	int iFid,iRet,index;
	uchar * ptrLoadBaleAddr = (uchar*)MAPP_RUN_ADDR;
	FS_W_STR fsWStr;
	FS_INODE_INFO fsCurNode;
	TERM_DESCRIB_TABLE tDescribTab[TAB_NUMS];
	volatile uint iOffsetTerm = 0,iFirstFileOffset=0,iOffset;
	uchar ucAppPckNum = 0,ucPubFileNum=0,ucPubSoNum=0,ucPrgPckNum=0,ucBaseBinNum=0,szGetBuf[6],ucTermNum,ucCnt=0;
	uchar ucSel,ucCurPage,ucPageNum,ucRemain,ucWriteTimes,tmpbuf[64];
	uint iRemainRead,i,start,end;

	iHadDLoadFile = 0;
	iHadDLoadMonitor = 0;
	iHadDLoadBaseso = 0;
    iHadDLoadPuk = 0;

	fsWStr.fmt = NAME_FMT_ASCII;
	fsWStr.size = strlen(ptrFileName);
	fsWStr.str = ptrFileName;
	
	if((iFid = FsOpen(&fsWStr,FS_ATTR_R)) < 0) 
	{
		ShowFsErr(iFid);
		FsClose(iFid);
		return 1;
	}

	if (iRet = FsGetInfo(iFid,&fsCurNode))
	{
		ShowFsErr(iRet);
		FsClose(iFid);
		return 1;	
	}

	ScrCls();
	SetLineRever(0,1);		
    SCR_PRINT(0,0,0x81,"     U盘下载    "," U-DISC-DOWNLOAD");		

	FsFileSeek(iFid,0,FS_SEEK_SET);

    memset(tDescribTab,0,sizeof(tDescribTab));
	//将Pack文件内容全部读取到内存中
	if (fsCurNode.size < READ_SIZE)
	{
		ucWriteTimes = 1;
		iRemainRead = 0;
	}
	else
	{
		ucWriteTimes = fsCurNode.size/READ_SIZE;
		iRemainRead = fsCurNode.size - ucWriteTimes*READ_SIZE;
	}
	for (ucCnt = 0; ucCnt < ucWriteTimes; ucCnt++)
	{
		if ((iRet = FsFileRead(iFid,ptrLoadBaleAddr + ucCnt*READ_SIZE,READ_SIZE)) < 0 )
		{
			ShowFsErr(iRet);
			FsClose(iFid);
			return 1;
		}

		if (ucCnt == 0)//查找机型描述表
		{
			ucTermNum = *(ptrLoadBaleAddr+0x0d);//多少个机型
			
			for (ucCnt = 0; ucCnt < ucTermNum; ucCnt++)
			{	
				memcpy(szGetBuf,ptrLoadBaleAddr + ucCnt*5+0x20,5);
				iOffsetTerm = szGetBuf[1]*256*256*256 + szGetBuf[2]*256*256 + szGetBuf[3]*256 + szGetBuf[4];//终端表偏移地址
				break;
			}
			
			ucCnt = 0;
			
			if (iOffsetTerm == 0)
			{
				FsClose(iFid);
				ShowFsErr(12);
				return 1;//没有找到对应的终端
			}			
		}
		
		switch(ucCnt%4) {
		case 0:
			SCR_PRINT(0,4,1,"读取中   ","READING   ");
			break;
		case 1:
			SCR_PRINT(0,4,1,"读取中.  ","READING.  ");
			break;
		case 2:
			SCR_PRINT(0,4,1,"读取中.. ","READING.. ");
			break;
		case 3:
			SCR_PRINT(0,4,1,"读取中...","READING...");
			break;
		default:
			break;
		}
	}

	if(iRemainRead)
	{
		if ((iRet = FsFileRead(iFid,ptrLoadBaleAddr + ucCnt*READ_SIZE,iRemainRead)) < 0 )
		{
			ShowFsErr(iRet);
			FsClose(iFid);
			return 1;
		}
	}
	
	FsClose(iFid);
	
	//读取描述表
	ucCnt = 0;
//-------------------------
	tDescribTab[ucCnt].uiFileAddr = 0;
    tDescribTab[ucCnt].uiFileLen = 0;
    tDescribTab[ucCnt].ucFileType = 0;
	tDescribTab[ucCnt+1].uiFileAddr = 0;
    tDescribTab[ucCnt+1].uiFileLen = 0;
    tDescribTab[ucCnt+1].ucFileType = 0;

	if(bIsChnFont)
    {
        strcpy(tDescribTab[ucCnt].szFileName,"删除所有应用");
		strcpy(tDescribTab[ucCnt+1].szFileName,"下载所有文件");
    }
    else
    {
        strcpy(tDescribTab[ucCnt].szFileName,"DELETE ALL APP");
		strcpy(tDescribTab[ucCnt+1].szFileName,"DOWNLOAD ALL");
    }	
	ucCnt+=2;
//-------monitor
    iOffset=iOffsetTerm;
	memcpy(szGetBuf,ptrLoadBaleAddr + iOffset,4);
	tDescribTab[ucCnt].uiFileAddr = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
				
	if (tDescribTab[ucCnt].uiFileAddr != 0)
	{			
		memcpy(szGetBuf,ptrLoadBaleAddr + iOffset + 4,4);
		tDescribTab[ucCnt].uiFileLen = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		if (tDescribTab[ucCnt].uiFileLen != 0)
		{
			tDescribTab[ucCnt].ucFileType = MONITOR_TYPE;//monitor
			strcpy(tDescribTab[ucCnt].szFileName,"MONITOR");
            iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
			ucCnt++;
		}		
	}

//-------------font
	
    iOffset+=8 ;
	memcpy(szGetBuf,ptrLoadBaleAddr + iOffset,4);
	tDescribTab[ucCnt].uiFileAddr = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
	if (tDescribTab[ucCnt].uiFileAddr != 0)
	{			
		memcpy(szGetBuf,ptrLoadBaleAddr + iOffset+ 4,4);
		tDescribTab[ucCnt].uiFileLen = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		if (tDescribTab[ucCnt].uiFileLen != 0)
		{
			tDescribTab[ucCnt].ucFileType = FONT_TYPE;//font
			strcpy(tDescribTab[ucCnt].szFileName,"FONT");
            if(iFirstFileOffset==0 || iFirstFileOffset > tDescribTab[ucCnt].uiFileAddr)iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
			ucCnt++;
		}
	}

    //uspuk
    iOffset+=8;
	memcpy(szGetBuf,ptrLoadBaleAddr + iOffset, 4);
	tDescribTab[ucCnt].uiFileAddr  = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
	if (tDescribTab[ucCnt].uiFileAddr  != 0)
	{			
		memcpy(szGetBuf,ptrLoadBaleAddr + iOffset+4,4);
		tDescribTab[ucCnt].uiFileLen = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		if (tDescribTab[ucCnt].uiFileLen != 0)
		{
			tDescribTab[ucCnt].ucFileType = USPUK_TYPE;//puk
			strcpy(tDescribTab[ucCnt].szFileName,"US_PUK");
            if(iFirstFileOffset==0 || iFirstFileOffset > tDescribTab[ucCnt].uiFileAddr)iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
			ucCnt++;
		}
	}
			
    //uapuk
    iOffset+=8;
	memcpy(szGetBuf,ptrLoadBaleAddr + iOffset,4);
	tDescribTab[ucCnt].uiFileAddr = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];

	if (tDescribTab[ucCnt].uiFileAddr != 0)
	{
		memcpy(szGetBuf,ptrLoadBaleAddr + iOffset+4,4);
		tDescribTab[ucCnt].uiFileLen = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		if (tDescribTab[ucCnt].uiFileLen != 0)
		{
			tDescribTab[ucCnt].ucFileType = UAPUK_TYPE;//puk
			strcpy(tDescribTab[ucCnt].szFileName,"UA_PUK");
            if(iFirstFileOffset==0 || iFirstFileOffset > tDescribTab[ucCnt].uiFileAddr)iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
			ucCnt++;
		}
	}

	ucAppPckNum = *(ptrLoadBaleAddr+0x30 + iOffsetTerm);

	if (ucAppPckNum > 24)
	{
		ShowFsErr(15);
		return 1;		
	}

    iOffset=0x31 + iOffsetTerm;
	//将应用包信息读取
	for (i = 0;i < ucAppPckNum;i++)
	{
		memcpy(szGetBuf,ptrLoadBaleAddr + iOffset + i*18,4);
		tDescribTab[ucCnt].uiFileAddr = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
		if(tDescribTab[ucCnt].uiFileAddr==0)
        {
            ShowFsErr(16);
    		return 1;
        }
		tDescribTab[ucCnt].ucFileType = APPCK_TYPE;//puk
		memcpy(tDescribTab[ucCnt].szFileName,ptrLoadBaleAddr + iOffset + i*18 + 4,14);
        if(iFirstFileOffset==0 || iFirstFileOffset > tDescribTab[ucCnt].uiFileAddr) iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
		ucCnt++;
		}		

    //pub file
    iOffset+=ucAppPckNum*18;
    if(iFirstFileOffset!=iOffset)//有pub file定义
    {
        ucPubFileNum = *(ptrLoadBaleAddr +iOffset);
        if((ucCnt+ucPubFileNum)>TAB_NUMS)
        {
            ShowFsErr(1);
            return 1;
        }
        iOffset++;
        for(i = 0; i < ucPubFileNum; i++)
		{
            //file addr
			memcpy(szGetBuf,ptrLoadBaleAddr + iOffset+ i*24 + 16,4);
			tDescribTab[ucCnt].uiFileAddr = BYTE2LONG(szGetBuf);
            //file size
			memcpy(szGetBuf,ptrLoadBaleAddr + iOffset + i*24 + 20,4);
			tDescribTab[ucCnt].uiFileLen = BYTE2LONG(szGetBuf);
            if(tDescribTab[ucCnt].uiFileAddr==0 || tDescribTab[ucCnt].uiFileLen==0)
            {
                ShowFsErr(17);
        		return 1;
            }

            //file name
			memcpy(tDescribTab[ucCnt].szFileName, ptrLoadBaleAddr + iOffset+ i*24, 16);

            
			tDescribTab[ucCnt].ucFileType = PUBF_TYPE;
            if(iFirstFileOffset==0 || iFirstFileOffset > tDescribTab[ucCnt].uiFileAddr)iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
			ucCnt++;
		}
    }    

    //pub so
    iOffset+=ucPubFileNum*24;
    if(iFirstFileOffset!=iOffset)//有pub so定义
    {
        ucPubSoNum = *(ptrLoadBaleAddr +iOffset);
        if((ucCnt+ucPubSoNum)>TAB_NUMS)
        {
            ShowFsErr(1);
            return 1;
        }
        iOffset++;
        for(i=0;i<ucPubSoNum;i++)
        {
            //file addr
			memcpy(szGetBuf,ptrLoadBaleAddr + iOffset+ i*24 + 16,4);
			tDescribTab[ucCnt].uiFileAddr = BYTE2LONG(szGetBuf);
            //file size
			memcpy(szGetBuf,ptrLoadBaleAddr + iOffset + i*24 + 20,4);
			tDescribTab[ucCnt].uiFileLen = BYTE2LONG(szGetBuf);
            if(tDescribTab[ucCnt].uiFileAddr==0 || tDescribTab[ucCnt].uiFileLen==0)
            {
                ShowFsErr(18);
        		return 1;
            }

            //file name
			memcpy(tDescribTab[ucCnt].szFileName, ptrLoadBaleAddr + iOffset+ i*24, 16);

            
			tDescribTab[ucCnt].ucFileType = SOF_TYPE;
            if(iFirstFileOffset==0 || iFirstFileOffset > tDescribTab[ucCnt].uiFileAddr)iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
			ucCnt++;
		}
    }
    
    //prg pack
    iOffset+=ucPubSoNum*24;
    if(iFirstFileOffset!=iOffset)
    {
        ucPrgPckNum = *(ptrLoadBaleAddr +iOffset);
        if((ucCnt+ucPrgPckNum)>TAB_NUMS)
        {
            ShowFsErr(1);
            return 1;
        }
        iOffset++;
        for(i=0;i<ucPrgPckNum;i++)
        {
            //file addr
			memcpy(szGetBuf,ptrLoadBaleAddr + iOffset+ i*18,4);
			tDescribTab[ucCnt].uiFileAddr = BYTE2LONG(szGetBuf);
            if(tDescribTab[ucCnt].uiFileAddr==0)
            {
                ShowFsErr(19);
        		return 1;
            }

            //file name
			memcpy(tDescribTab[ucCnt].szFileName, ptrLoadBaleAddr + iOffset+ i*18+4, 14);

            
			tDescribTab[ucCnt].ucFileType = PRG_TYPE;
            if(iFirstFileOffset==0 || iFirstFileOffset > tDescribTab[ucCnt].uiFileAddr)iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
			ucCnt++;
		}
    }

	//------- base driver
    iOffset+=ucPrgPckNum*18;
	if(iFirstFileOffset!=iOffset)
    {
        ucBaseBinNum = *(ptrLoadBaleAddr +iOffset);
        if((ucCnt+ucBaseBinNum)>TAB_NUMS)
        {
            ShowFsErr(1);
            return 1;
        }
        iOffset++;
        for(i=0;i<ucBaseBinNum;i++)
        {
			memcpy(szGetBuf,ptrLoadBaleAddr + iOffset+ i*264+256,4);
			tDescribTab[ucCnt].uiFileAddr = BYTE2LONG(szGetBuf);
            if(tDescribTab[ucCnt].uiFileAddr==0)
            {
                ShowFsErr(20);
        		return 1;
            }				
			memcpy(szGetBuf,ptrLoadBaleAddr + iOffset + i*264+256+4,4);
			tDescribTab[ucCnt].uiFileLen = szGetBuf[0]*256*256*256 + szGetBuf[1]*256*256 + szGetBuf[2]*256 + szGetBuf[3];
			if (tDescribTab[ucCnt].uiFileLen != 0)
			{
				tDescribTab[ucCnt].ucFileType = BASEDRIVER_TYPE;
				memcpy(tDescribTab[ucCnt].szFileName, ptrLoadBaleAddr + iOffset+ i*264, 14);
	            if(iFirstFileOffset==0 || iFirstFileOffset > tDescribTab[ucCnt].uiFileAddr) iFirstFileOffset=tDescribTab[ucCnt].uiFileAddr;
				ucCnt++;
			}		
		}
    }

	if (ucCnt == 0)
	{
		ShowFsErr(13);
		return 1;//没有任何文件
	}


	ucPageNum = (ucCnt%3 == 0)?ucCnt/3:(ucCnt/3+1);
	ucCurPage = 1;
	ucRemain = ucCnt%3;//最后一页有几个显示，ucRemain=0时
	
	while (1)
	{
		ScrCls();
		SetLineRever(0,1);		
		SCR_PRINT(0,0,0x81,"    选择文件     ","   SELECT FILE  ");		
	
		if (ucPageNum > 1)
		{
			ScrSetIcon(ICON_UP, OPENICON);
			ScrSetIcon(ICON_DOWN, OPENICON);
		}

		//中英文字库切换后前两行需重新写入
		if(bIsChnFont)
	    {
	        strcpy(tDescribTab[0].szFileName,"删除所有应用");
			strcpy(tDescribTab[1].szFileName,"下载所有文件");
	    }
	    else
	    {
	        strcpy(tDescribTab[0].szFileName,"DELETE ALL APP");
			strcpy(tDescribTab[1].szFileName,"DOWNLOAD ALL");
	    }	
		//列出对应机型的所有的下载软件：监控，公钥，字库，应用包1，应用包2....
		if (ucCurPage == ucPageNum && ucRemain != 0)//最后一页,而且有剩余
		{
			for(i = 0; i < ucRemain;i++)
			{
				ScrPrint(0,2+i*2,1,"%d-%s",i+1,tDescribTab[i + 3*(ucCurPage - 1)].szFileName);
			}	
		}
		else //显示3行
		{
			for(i = 0; i < 3;i++)
			{
				ScrPrint(0,2+i*2,1,"%d-%s",i+1,tDescribTab[i + 3*(ucCurPage - 1)].szFileName);
			}				
		}
		//选择下载的软件
		kbflush();

		switch(ucSel = getkey()) 
		{
		case KEY1:
		case KEY2:
		case KEY3:
			ScrSetIcon(ICON_UP, CLOSEICON);
			ScrSetIcon(ICON_DOWN, CLOSEICON);
			if((ucCurPage==ucPageNum) && (ucRemain!=0))//最后一页不足3个条目
			{
				if(ucRemain==1&&ucSel!=KEY1)break;
				if(ucRemain==2&&ucSel==KEY3)break;
			}

			index = ucSel - KEY1 + 3*(ucCurPage - 1);
			if(index==1)//下载所有文件
			{
			    start = 2;//注意这里从2开始，因为前2行没有对应的文件
			    end = ucCnt-1;
			}
			else 
			{
			    start = index;
			    end = index;
			}
			for(i=start; i<=end; i++)
			{
                iRet = WriteAimFile(tDescribTab[i]);//下载目标文件
                memset(tmpbuf, 0x00, sizeof(tmpbuf));
                if(!iRet)
                {
                    sprintf(tmpbuf, "LOAD %s OK", tDescribTab[i].szFileName);                   
                    BeepOK();
                }
                else  sprintf(tmpbuf, "LOAD %s fail", tDescribTab[i].szFileName);
                UDiscDLlog(tmpbuf);                 			
			}
            DelayMs(1000);			          
			break;
		case KEYCANCEL:
			ScrSetIcon(ICON_UP, CLOSEICON);
			ScrSetIcon(ICON_DOWN, CLOSEICON);		
		return 2;
		case KEYUP://向前
		case KEYF1:
			if (ucCurPage <= 1) ucCurPage = ucPageNum;
			else ucCurPage--;
	    break;
		case KEYDOWN://向后
		case KEYF2:
			if (ucCurPage >= ucPageNum) ucCurPage = 1;
			else ucCurPage++;
		break;
		default:
			break;
		}
		//下载软件	
	}
	return 0;
}



static int FsDirReadPSW(int fd, FS_INODE_INFO *fs_inode, int num,int iCurPage,uchar * ptrShowLine)
{
	int iCnt = num;
	
	while (num > 0)
	{
		memcpy(fs_inode[iCnt-num].name,szNodeInfo + (3 * iCurPage + iCnt-num)*(LONG_NAME_MAX + 5 + 14),(LONG_NAME_MAX + 4));
		memcpy(ptrShowLine + (iCnt - num)*15,szNodeInfo + (3 * iCurPage + iCnt-num )*(LONG_NAME_MAX + 5 + 14) + LONG_NAME_MAX + 4,14);		
		num--;
	}
	return iCnt;
}


//返回成功下载文件数目
uchar UDiscDload(void)
{
	int iFd,iFileNum,iRet,OkCnt,len,iPage,iCurPage = 0;
	FS_W_STR fsWStr;
	FS_INODE_INFO fsInodeInfoPage[4];//每页显示
	uchar szShowLine[3][15], tmpbuf[LONG_NAME_MAX+64];
	uchar ucRemainItem = 0,ucGetKey;

    OkCnt = 0;
    
	fsWStr.fmt = NAME_FMT_ASCII;
	fsWStr.size = 7;
	fsWStr.str = "/udisk/.";
	
	ScrCls();
	SetLineRever(0,1);		
    SCR_PRINT(0,0,0x81,"     U盘下载    "," U-DISC-DOWNLOAD");

	if((iFd = FsOpen(&fsWStr,FS_ATTR_R|FS_ATTR_W)) < 0)
	{
		return 0;
	}
	
    SCR_PRINT(0,4,0x01,"扫描下载文件...","SCANNING...");
	//-------重复显示，每屏显示3个，1-2-3
	if((iRet = FsSetWorkDir(&fsWStr)) != 0)
	{
		ShowFsErr(iRet);
		FsClose(iFd);
		return 0;
	}
    iFileNum = GetPSWNumber(iFd);//获取PSW文件的数目
	if(iFileNum <= 0)
	{
		FsClose(iFd);
		return 0;
	}

	FsDirSeek(iFd, 0, FS_SEEK_SET);
	iPage = iFileNum/3;
	ucRemainItem = iFileNum%3;
	iPage += (ucRemainItem != 0)? 1:0;
	ucRemainItem = (ucRemainItem == 0)? 3:ucRemainItem;

	while (1)
	{
		ScrCls();
	    SetLineRever(0,1);
        SCR_PRINT(0,0,0x81,"  选择打包文件","  SELECT FILE");

		ScrSetIcon(ICON_UP,CLOSEICON);
		ScrSetIcon(ICON_DOWN,CLOSEICON);

		if ((iCurPage + 1) < iPage) ScrSetIcon(ICON_DOWN, OPENICON);
		if (iCurPage != 0) ScrSetIcon(ICON_UP, OPENICON);			
		
		memset(&fsInodeInfoPage,0,sizeof(fsInodeInfoPage));
		memset(szShowLine,0,3*15);
		
		if ((iCurPage + 1) < iPage)//不是最后一页
		{	
			if((iFileNum = FsDirReadPSW(iFd,fsInodeInfoPage,3,iCurPage,szShowLine[0])) != 3)
			{
				ShowFsErr(iFileNum);
				FsClose(iFd);				
				return OkCnt;
			}
		}		
		else
		{
			if((iFileNum = FsDirReadPSW(iFd,fsInodeInfoPage,ucRemainItem,iCurPage,szShowLine[0])) != ucRemainItem)
			{
				if (iFileNum == 0)
				{
					ShowFsErr(-8);
				}
				else
				{
					ShowFsErr(iFileNum);
				}
				FsClose(iFd);		
				return OkCnt;
			}
		}

		kbflush();

		if (strlen(szShowLine[0]) != 0)
		{
			ScrPrint(0,2,0x01,"1-%s",szShowLine[0]);
		}

		if (strlen(szShowLine[1]) != 0)
		{
			ScrPrint(0,4,0x01,"2-%s",szShowLine[1]);
		}
		
		if (strlen(szShowLine[2]) != 0)
		{
			ScrPrint(0,6,0x01,"3-%s",szShowLine[2]);
		}
		
		SetLineRever(0,1);
        SCR_PRINT(0,0,0x81,"  选择打包文件","  SELECT FILE");

		switch(ucGetKey = GetKeyMs(60000)) 
		{
		case KEY0:
        	ScrCls();
        	SetLineRever(0,1);		
            SCR_PRINT(0,0,0x81,"     设  置    ","    Settings   ");	
            SCR_PRINT(0,2,0,  "完成后打印日志?","Prn Download Log?");
            SCR_PRINT(0,4,0,  "【确认】 - 是 ","[ENTER] - YES");
            SCR_PRINT(0,6,0,  " 其他键  - 否 ","Other Key - NO");
            kbflush();
            ucGetKey = GetKeyMs(60000);
            if(ucGetKey== KEYENTER) iPrnDLoadLog = 1;
            else iPrnDLoadLog = 0;
		break;
		case KEY1:
		case KEY2:
		case KEY3:
			if (strlen(fsInodeInfoPage[ucGetKey - 0x31].name) ==  0) break;
            ScrSetIcon(ICON_UP, CLOSEICON);
            ScrSetIcon(ICON_DOWN, CLOSEICON);
            s_remove(ERR_LOG_FILE,"\xff\x05");
            iRet = DLoadBaleFile(fsInodeInfoPage[ucGetKey - 0x31].name);//下载Bale          
            memset(tmpbuf, 0x00, sizeof(tmpbuf));
            get_term_name(tmpbuf);
            sprintf(tmpbuf+strlen(tmpbuf), "_");
            ReadSN(tmpbuf+strlen(tmpbuf));
            sprintf(tmpbuf+strlen(tmpbuf), "_%s", fsInodeInfoPage[ucGetKey - 0x31].name);
            WriteLogtoUDisc(tmpbuf);

            if (iRet)
            {
                FsClose(iFd); 
                if (iHadDLoadFile) ShowDLOver();//显示下载结束
                return OkCnt;
            }
            OkCnt++;

			break;
		case KEYUP:
		case KEYF1:
			iCurPage = (iCurPage == 0)? 0 : (iCurPage-1);
			break;
		case KEYENTER:			
		case KEYDOWN:
		case KEYF2:
			iCurPage = ((iCurPage + 1) == iPage)? iCurPage : (iCurPage+1);
			break;
		case KEYCANCEL:
			FsClose(iFd);
            if (iHadDLoadFile)
            {
                ShowDLOver();//显示下载结束
            }			
			return OkCnt;//一分钟没有按键就退出
		default:
			break;
  		}
	}
}


