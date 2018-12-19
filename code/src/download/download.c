
#include "base.h"
#include "../file/filedef.h"

#include "posapi.h"
#include "localdownload.h"
#include "../encrypt/rsa.h"
#include "../fat/fsapi.h"
#include "../puk/puk.h"
#include "link_info.h"

extern const APPINFO MonitorInfo;
extern POS_AUTH_INFO *GetAuthParam(void);

int GetPubFile(uchar *OutBuf)
{
    int i, sum=0, total=0;
    FILE_INFO finfo[256];
    PUBFILE_INFO *PubFileInfo = (PUBFILE_INFO *)OutBuf;

    sum = GetFileInfo(finfo);
    for(i=0;i<sum;i++)
    {
        if(finfo[i].attr == FILE_ATTR_PUBFILE && finfo[i].type == FILE_TYPE_USER_FILE)
        {
            strcpy(PubFileInfo->name,finfo[i].name);
            PubFileInfo->len[0] = (finfo[i].length>>24)&0xff;
            PubFileInfo->len[1] = (finfo[i].length>>16)&0xff;
            PubFileInfo->len[2] = (finfo[i].length>>8)&0xff;
            PubFileInfo->len[3] = (finfo[i].length)&0xff;
            PubFileInfo++;
            total++;
        }
    }
    return total;
}

int  s_iDeleteApp(uchar ucAppNo)
{	
	uchar aucAppName[64],aucAttr[3];
	int iLoop,iRet;
	FILE_INFO astFileInfo[MAX_FILES];
    
	sprintf(aucAppName,"%s%d",APP_FILE_NAME_PREFIX,ucAppNo);
	aucAttr[0]=0xff;
	aucAttr[1]=0x01;

	if(ucAppNo==0)aucAttr[1]=0x00;
	
	if(s_SearchFile(aucAppName,aucAttr)<0)
		return -1;
	iRet=GetFileInfo(astFileInfo);
	
	if(iRet>0)
	{
		for(iLoop=0;iLoop<iRet;iLoop++)
		{
			aucAttr[0]=astFileInfo[iLoop].attr;
			aucAttr[1]=astFileInfo[iLoop].type;
			if(astFileInfo[iLoop].attr==ucAppNo)
			{
				s_remove(astFileInfo[iLoop].name,aucAttr);
			}
		}
	}
	s_remove(aucAppName,"\xff\x01");
	return 0;
}

int GetPosInfo(uchar* buff, int PckMaxLen)
{
    int i = 0, tmp; 
    uint val;
    FS_DISK_INFO disk_info;
    tSignFileInfo sig;
    POS_AUTH_INFO *authinfo;
    uchar tmpbuf[1024], index;
    SO_INFO soInfo;
    uchar tmpinfo[32];
    ushort SecLevelDisplay;
    uchar SecLevelIndex;

    memset(tmpbuf, 0x00, sizeof(tmpbuf));
	ReadCfgInfo("TERMINAL_NAME", tmpbuf);
    memcpy(buff, tmpbuf, 16);	
    i+=16;
    buff[i++]=PROTOCOL_VERSION&0xff;//下载协议版本
    strcpy(buff+i,MonitorInfo.AppVer);       //监控版本
    i+=16;
    buff[i++]=1;//支持压缩
    //支持的最大数据包
    if (PckMaxLen > 0xffff)
    {
        buff[i-1]=0;//不支持压缩
        buff[i++]=(PckMaxLen>>24)&0xff;
        buff[i++]=(PckMaxLen>>16)&0xff;
    }
    buff[i++]=(PckMaxLen>>8)&0xff;
    buff[i++]= PckMaxLen&0xff;
    ReadSN(buff+i);
    i+=8;
    EXReadSN(buff+i);
    i+=24;
    //支持的最大波特率
    buff[i++]=(LD_MAX_BAUDRATE>>24)&0xff;
    buff[i++]=(LD_MAX_BAUDRATE>>16)&0xff;            
    buff[i++]=(LD_MAX_BAUDRATE>>8)&0xff;         
    buff[i++]=LD_MAX_BAUDRATE&0xff;          
    //文件系统剩余空间
    val = freesize();
    buff[i++]=(val>>24)&0xff;            
    buff[i++]=(val>>16)&0xff;
    buff[i++]=(val>>8)&0xff;
    buff[i++]=(val)&0xff;            

    //flash大小
	tmp = CheckFlashSize()*1024*1024;
    buff[i++]=(tmp>>24)&0xff;         
    buff[i++]=(tmp>>16)&0xff;
    buff[i++]=(tmp>>8)&0xff;
    buff[i++]=(tmp)&0xff;         

    if(FsGetDiskInfo(1,&disk_info) != 0) val = 0;
    else  val = disk_info.free_space + disk_info.used_space;
    buff[i++]=(val>>24)&0xff;         
    buff[i++]=(val>>16)&0xff;
    buff[i++]=(val>>8)&0xff;
    buff[i++]=(val)&0xff;

    buff[i++]=IS_SO_SUPPORTED & 0x01;
    GetMsrVer(buff+i);//磁头固件版本，32Byte
    i+=32;

    /****ExtInfo TLV Format****/
    index = 0x02;

    authinfo=GetAuthParam();     
    if(0!=s_GetBootSecLevel())
    {
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        sprintf(tmpbuf,"L%d", (authinfo->SecMode)&0x01 + ((authinfo->SecMode&0x02)>>1));
        val = strlen(tmpbuf);
        buff[i++]= 0x02;/*index=0x02*/
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
        
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
		SecLevelDisplay = 0;
		for(SecLevelIndex=0;SecLevelIndex<16;SecLevelIndex++)
		{
			if(!((authinfo->security_level) & (1<<SecLevelIndex)))
			{
				break;
			}
			else
			{
				SecLevelDisplay++;
			}
		}
        sprintf(tmpbuf,"S%.2d",SecLevelDisplay);
        val = strlen(tmpbuf);
        buff[i++]= 0x03;/*index=0x03*/
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
        
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        sprintf(tmpbuf,"%d", authinfo->AuthDownSn);
        val = strlen(tmpbuf);
        buff[i++]= 0x04;/*index=0x04*/
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
        
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        sprintf(tmpbuf,"%d", authinfo->SnDownLoadSum);
        val = strlen(tmpbuf);
        buff[i++]= 0x05;/*index=0x05*/
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
        
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        sprintf(tmpbuf,"%d", CheckIfDevelopPos());
        val = strlen(tmpbuf);
        buff[i++]= 0x06;/*index=0x06*/
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
        
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        sprintf(tmpbuf,"%d", s_GetFwDebugStatus());
        val = strlen(tmpbuf);
        buff[i++]= 0x07;/*index=0x07*/
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
        
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        sprintf(tmpbuf,"%d", authinfo->UsPukLevel);
        val = strlen(tmpbuf);
        buff[i++]= 0x08;/*index=0x08*/
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;    
    }

    /*cfg file info*/
    memset(tmpbuf, 0x00, sizeof(tmpbuf));
    memset(&sig, 0x00, sizeof(sig));
    tmp = ReadCfgInfo("PN",tmpbuf);
    strcat(tmpbuf,"(");
    ReadCfgInfo("CONFIG_FILE_VER",tmpbuf+32);
    strcat(tmpbuf,tmpbuf+32);
    strcat(tmpbuf,")  ");
    GetCfgFileSig(&sig);
    if (sig.ucHeader == SIGNFORMAT1) strcat(tmpbuf,sig.owner);    
    val = strlen(tmpbuf);    
    buff[i++]= 0x09;/*index=0x09*/
    buff[i++]= (val>>8)&0xff;
    buff[i++]= (val)&0xff;
    memcpy(buff+i, tmpbuf, val);
    i+=val;

    /*us_puk owner*/
    memset(tmpbuf, 0x00, sizeof(tmpbuf));
    memset(&sig, 0x00, sizeof(sig));    
    tmp = GetPUKSig(ID_US_PUK, (uchar*)&sig, &val);
    if (!tmp && sig.ucHeader==SIGNFORMAT1)
    {
        memcpy(tmpbuf, sig.owner, 16);
        val = strlen(tmpbuf);
    	sprintf(tmpbuf+val, " %c%c/%c%c/20%c%c",
                (sig.aucDigestTime[1]>>4)+0x30,(sig.aucDigestTime[1]&0x0f)+0x30,
                (sig.aucDigestTime[2]>>4)+0x30,(sig.aucDigestTime[2]&0x0f)+0x30,
                (sig.aucDigestTime[0]>>4)+0x30,(sig.aucDigestTime[0]&0x0f)+0x30);
    }
    val = strlen(tmpbuf);                 
    buff[i++]= 0x0a;/*index=0x0a*/
    buff[i++]= (val>>8)&0xff;
    buff[i++]= (val)&0xff;    
    memcpy(buff+i, tmpbuf, val);
    i+=val;

    /*Monitor Signer*/
    memset(tmpbuf, 0x00, sizeof(tmpbuf));
    memset(&sig, 0x00, sizeof(sig));    
    tmp = GetMonSig((uchar*)&sig, &val);
    if (!tmp && sig.ucHeader==SIGNFORMAT1)
    {
        memcpy(tmpbuf, sig.owner, 16);
        val = strlen(tmpbuf);
    	sprintf(tmpbuf+val, " %c%c/%c%c/20%c%c--%c%c/%c%c/20%c%c",
                (sig.aucDigestTime[1]>>4)+0x30,(sig.aucDigestTime[1]&0x0f)+0x30,
                (sig.aucDigestTime[2]>>4)+0x30,(sig.aucDigestTime[2]&0x0f)+0x30,
                (sig.aucDigestTime[0]>>4)+0x30,(sig.aucDigestTime[0]&0x0f)+0x30,
    		    (sig.validdate[1]>>4) + 0x30, (sig.validdate[1]&0x0f) + 0x30,
    		    (sig.validdate[2]>>4) + 0x30, (sig.validdate[2]&0x0f) + 0x30,
    		    (sig.validdate[0]>>4) + 0x30, (sig.validdate[0]&0x0f) + 0x30);
    }
    val = strlen(tmpbuf);                 
    buff[i++]= 0x0b;/*index=0x0b*/
    buff[i++]= (val>>8)&0xff;
    buff[i++]= (val)&0xff;    
    memcpy(buff+i, tmpbuf, val);
    i+=val;
    
    /*tampered info*/
    memset(tmpbuf, 0x00, sizeof(tmpbuf));
    val = s_GetBBLStatus();
    if(val==0)
    {
        if(authinfo->SecMode==POS_SEC_L2 
            && authinfo->TamperClear==0) strcpy(tmpbuf, "Unauthorized");
        else strcpy(tmpbuf, "NO TAMPERED");
    }    
    else
    {        
        sprintf(tmpbuf,"%X ", val);
        if(val & (1<<6))  strcat(tmpbuf, "N1-P1 ");   
        if(val & (1<<7))  strcat(tmpbuf, "N2-P2 ");
        if(val & (1<<8))  strcat(tmpbuf, "N3-P3 ");   
        if(val & (1<<9))  strcat(tmpbuf, "N4-P4 ");
        if(val & (1<<20)) strcat(tmpbuf, "under-vol ");
        if(val & (1<<21)) strcat(tmpbuf, "over-vol ");
        if(val & (1<<19)) strcat(tmpbuf, "high-freq ");
        if(val & (1<<18)) strcat(tmpbuf, "low-freq ");
        if(val & (1<<17)) strcat(tmpbuf, "over-temp ");
        if(val & (1<<16)) strcat(tmpbuf, "under-temp ");
        if(val & (1<<0))  strcat(tmpbuf, "N0 ");   
        if(val & (1<<5))  strcat(tmpbuf, "P0 ");       
    }
    val = strlen(tmpbuf);
    buff[i++]= 0x0c;/*index=0x0c*/
    buff[i++]= (val>>8)&0xff;
    buff[i++]= (val)&0xff;        
    memcpy(buff+i, tmpbuf, val);        
    i+=val;

    /*Base So Info*/
    memset(tmpbuf, 0x00, sizeof(tmpbuf));    
    memset(&soInfo,0,sizeof(soInfo));
    if(GetSoInfo("PAXBASE.SO",&soInfo)>0)
    {
        strcat(tmpbuf, soInfo.head.version);
        strcat(tmpbuf, " ");                
        strcat(tmpbuf, soInfo.head.date);            
    }
    else strcat(tmpbuf, "NULL");
    
    val = strlen(tmpbuf);
    buff[i++]= 0x0d;/*index=0x0d*/
    buff[i++]= (val>>8)&0xff;
    buff[i++]= (val)&0xff;
    memcpy(buff+i, tmpbuf, val);
    i+=val; 

    memset(tmpbuf, 0x00, sizeof(tmpbuf));    
    memset(&soInfo,0,sizeof(soInfo));
    if(GetSoInfo("SXXAPI01",&soInfo)>0)
    {
        strcat(tmpbuf, soInfo.head.version);
        strcat(tmpbuf, " ");                
        strcat(tmpbuf, soInfo.head.date);            
    }
    else strcat(tmpbuf, "NULL");

    val = strlen(tmpbuf);
    buff[i++]= 0x0e;/*index=0x0e*/
    buff[i++]= (val>>8)&0xff;
    buff[i++]= (val)&0xff;
    memcpy(buff+i, tmpbuf, val);
    i+=val;

    /*WIFI info*/
    if(is_wifi_module())
    {
        index = 20;
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        s_WifiGetHVer(tmpbuf);
        val = strlen(tmpbuf);
        buff[i++]= index;
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
    }

    /*SM info*/
    if (0 != GetSMFirmwareVer())
    {
        index = 23;
        memset(tmpbuf, 0, sizeof(tmpbuf));
        sprintf(tmpbuf, "V%02d", GetSMFirmwareVer());
        val = strlen(tmpbuf);
        buff[i++]= index;
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
    }

    /*SN LEN>8 Byte*/
    memset(tmpbuf, 0, sizeof(tmpbuf));
    ReadSN(tmpbuf);
    if(strlen(tmpbuf)>8)
    {
        index = 24;
        val = strlen(tmpbuf);
        buff[i++]= index;
        buff[i++]= (val>>8)&0xff;
        buff[i++]= (val)&0xff;
        memcpy(buff+i, tmpbuf, val);
        i+=val;
    }
    
    /*Boot Version*/
    memset(tmpbuf, 0, sizeof(tmpbuf));
    sprintf(tmpbuf,"%02d(C%C)",GetBiosVer(),(s_GetFwDebugStatus() == 1? 'D':'R'));
	index = 17;
	val = strlen(tmpbuf);
	buff[i++]= index;
	buff[i++]= (val>>8)&0xff;
	buff[i++]= (val)&0xff;
	memcpy(buff+i, tmpbuf, val);
	i+=val;

    /*Font Info*/
    tmp = filesize(FONT_FILE_NAME);
    if(tmp>0)
    {
    	memset(tmpbuf, 0, sizeof(tmpbuf));
		sprintf(tmpbuf,"%dbyte", tmp);
		index = 21;
		val = strlen(tmpbuf);
		buff[i++]= index;
		buff[i++]= (val>>8)&0xff;
		buff[i++]= (val)&0xff;
		memcpy(buff+i, tmpbuf, val);
		i+=val;
    }

	if(GetWlType())
	{
		/*WNET firmware info*/
		memset(tmpbuf, 0, sizeof(tmpbuf));
		memset(tmpinfo,0,sizeof(tmpinfo));
		WlGetModuleInfo(tmpbuf, tmpinfo);
		index = 25;
		val = strlen(tmpbuf);
		buff[i++]= index;
		buff[i++]= (val>>8)&0xff;
		buff[i++]= (val)&0xff;
		memcpy(buff+i, tmpbuf, val);
		i+=val;

		/*IMEI or IMSI or MEID*/
		index = 26;
		val = strlen(tmpinfo);
		buff[i++]= index;
		buff[i++]= (val>>8)&0xff;
		buff[i++]= (val)&0xff;
		memcpy(buff+i, tmpinfo, val);
		i+=val;
	}

	/*Ethernet MAC info*/
	if(is_eth_module())
	{
		memset(tmpbuf, 0, sizeof(tmpbuf));
		memset(tmpinfo,0,sizeof(tmpinfo));
		ReadMacAddr(tmpinfo);
		sprintf(tmpbuf, "%02X%02X%02X%02X%02X%02X",
				tmpinfo[0],tmpinfo[1],tmpinfo[2],tmpinfo[3],tmpinfo[4],tmpinfo[5]);
		index = 27;
		val = strlen(tmpbuf);
		buff[i++]= index;
		buff[i++]= (val>>8)&0xff;
		buff[i++]= (val)&0xff;
		memcpy(buff+i, tmpbuf, val);
		i+=val;
	}

	/*support to get SNK info*/
	if(GetIDKeyState()==1 || GetIDKeyState()==0)  //0-未上传， 1-已上传
	{
        index = 38;
		buff[i++] = index;
		buff[i++] = 0;
		buff[i++] = 1;
		if(GetIDKeyState())
		    buff[i++] = '1';
	    else
		    buff[i++] = '0';
		index = 39;
		memset(tmpbuf, 0x00, sizeof(tmpbuf));
		tmp = s_PedGetKcv(0x45,1,tmpbuf,0);
		if(tmp)
			val = 0;
		else
			val = 3;
		buff[i++] = index;
		buff[i++] = (val>>8)&0xff;
		buff[i++] = (val)&0xff;
		memcpy(buff+i, tmpbuf, val);
		i+=val;
	}

	/*0x01表示艾体威尔客户机型*/
    memset(tmpbuf, 0, sizeof(tmpbuf));
	if(ReadCfgInfo("C_TERMINAL",tmpbuf)>0)
	{
	    index = 41;
		buff[i++] = index;
		buff[i++] = 0;
		buff[i++] = 1;
		buff[i++] = 0x01;		
	}

	/* TUSN */
    memset(tmpbuf, 0, sizeof(tmpbuf));
    val = ReadTUSN(tmpbuf, 64);
    if (val==0){
        val = 1;
        tmpbuf[0] = '1';//支持TUSN，但未写入TUSN
    }
    index = 42;
    buff[i++]= index;
    buff[i++]= (val>>8)&0xff;
    buff[i++]= (val)&0xff;
    memcpy(buff+i, tmpbuf, val);
    i+=val;
    
    return i+1;
}

/*
ucAppNo:期望查询的应用的编号，
实际查询结果可能为该编号后存在的第一应用
*/
int s_iGetAppInfo(uchar ucAppNo,uchar  * pszAppInfo)
{
	uchar aucAppName[64],aucTmp[64],ucFindAppNo,aucAttr[3];
	int iLoop,iRet,iFileId,iStartAddr,ret;
	FILE_INFO astFileInfo[MAX_FILES];
	APPINFO stAppInfo;
	uchar uaAppDepFile[SO_MAX_NUM][64]; 
	int iAppHeadPos, iGotSoCount=0;
    tSignFileInfo sig;
            
	for(iLoop=ucAppNo;iLoop<MAX_APP_NUM;iLoop++)
	{
		sprintf(aucAppName,"%s%d",APP_FILE_NAME_PREFIX,iLoop);
		if(s_SearchFile(aucAppName,"\xff\x01")>=0) break;
	}

	if(iLoop>=MAX_APP_NUM)//没有应用
	{
		return -1;
	}
	else
	{
		ucFindAppNo = iLoop;
	}
	
	/*when pos tampered , can't get the invalid sig app info*/
    if (s_GetTamperStatus())
    {
        if (GetAppSigInfo(ucFindAppNo, NULL)<0 && 
            s_GetBootSecLevel()!=0)/*for compatibility :old boot + new monitor*/
        return -3;
    }
	
	/* try to extract so info from appfile */
	iGotSoCount = elf_get_needed_so(aucAppName, uaAppDepFile);
	if(iGotSoCount<=0)  iGotSoCount=0;
	/* get end */
	
	iFileId=s_open(aucAppName,O_RDWR,(unsigned char *)"\xff\x01");
	if(iFileId<0) return -2;	

	
	seek(iFileId,APP_ENTRY_OFFSET,SEEK_SET);
	read(iFileId,(unsigned char *)aucTmp,64);	
	GetAppEntry(aucTmp, NULL, NULL,(ulong*) &iStartAddr);
	if(ucFindAppNo==0)
	{
		seek(iFileId,iStartAddr,SEEK_SET);
	}
	else
	{	
		seek(iFileId,iStartAddr,SEEK_SET);
	}

	read(iFileId,(uchar*)&stAppInfo,sizeof(APPINFO));	
	close(iFileId);
	
	pszAppInfo[0]=ucFindAppNo;
	iRet = s_filesize(aucAppName,(uchar*)"\xff\x01");
	pszAppInfo[1]=(iRet>>24)&0xff;
	pszAppInfo[2]=(iRet>>16)&0xff;
	pszAppInfo[3]=(iRet>>8)&0xff;
	pszAppInfo[4]=(iRet)&0xff;

	if(stAppInfo.AppName[31]!=0)
		pszAppInfo[5]=32;//应用名长度
	else pszAppInfo[5]=strlen(stAppInfo.AppName);
	#define	APP_NAME_LEN	pszAppInfo[5]
	memcpy(pszAppInfo+6,stAppInfo.AppName,pszAppInfo[5]);//应用名

	if(stAppInfo.AppVer[15]!=0)//应用版本长度。
		pszAppInfo[6+APP_NAME_LEN]=16;
	else pszAppInfo[6+APP_NAME_LEN]=strlen(stAppInfo.AppVer);
	#define	APP_VER_LEN	 pszAppInfo[6+APP_NAME_LEN]
	memcpy(pszAppInfo+6+APP_NAME_LEN+1,stAppInfo.AppVer,APP_VER_LEN);//应用版本

	if(stAppInfo.AppProvider[31]!=0)//应用提供商长度
		pszAppInfo[6+APP_NAME_LEN+1+APP_VER_LEN]=32;
	else pszAppInfo[6+APP_NAME_LEN+1+APP_VER_LEN]= strlen(stAppInfo.AppProvider);
	#define	APP_PRO_LEN	  pszAppInfo[6+APP_NAME_LEN+1+APP_VER_LEN]
	//应用提供商
	memcpy(pszAppInfo+6+APP_NAME_LEN+1+APP_VER_LEN+1,stAppInfo.AppProvider,APP_PRO_LEN);

	if(stAppInfo.Descript[63]!=0)//应用描述长度
		pszAppInfo[6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN]=64;
	else pszAppInfo[6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN]=strlen(stAppInfo.Descript);
	#define	APP_DECRIPT_LEN		pszAppInfo[6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN]
	memcpy(pszAppInfo+6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN+1,stAppInfo.Descript,APP_DECRIPT_LEN);

	pszAppInfo[6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN+1+APP_DECRIPT_LEN]=14;
	memcpy(pszAppInfo+6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN+1+APP_DECRIPT_LEN+1,stAppInfo.DownloadTime,14);
	
	pszAppInfo[6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN+1+APP_DECRIPT_LEN+1+14]=73;
	memcpy(pszAppInfo+6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN+1+APP_DECRIPT_LEN+1+14+1,stAppInfo.RFU,73);
	#define	APPINFO_LEN  (6+APP_NAME_LEN+1+APP_VER_LEN+1+APP_PRO_LEN+1+APP_DECRIPT_LEN+1+14+1+73)

	pszAppInfo[APPINFO_LEN]=0;
	int iAllFileNum,iSubFiles=0;
	uchar aucSubInfo[32];
	iAllFileNum=GetFileInfo(astFileInfo);
	if(iAllFileNum==0)  return	APPINFO_LEN;
	for(iLoop=0,iSubFiles=0;iLoop<iAllFileNum;iLoop++)
	{
		if(astFileInfo[iLoop].attr!=ucFindAppNo) continue;
		memset(aucSubInfo,0x00,sizeof(aucSubInfo));
		aucSubInfo[0]=astFileInfo[iLoop].type;
		aucSubInfo[1]=(astFileInfo[iLoop].length>>24)&0xff;
		aucSubInfo[2]=(astFileInfo[iLoop].length>>16)&0xff;
		aucSubInfo[3]=(astFileInfo[iLoop].length>>8)&0xff;
		aucSubInfo[4]=(astFileInfo[iLoop].length)&0xff;
		aucSubInfo[5]=16;
		iRet = strlen(astFileInfo[iLoop].name);
		if(iRet>16)iRet=16;
		memcpy(aucSubInfo+6,astFileInfo[iLoop].name,iRet);
		memcpy(pszAppInfo+APPINFO_LEN+1+iSubFiles*22,aucSubInfo,22);
		iSubFiles++;
	}	

	pszAppInfo[APPINFO_LEN] = iSubFiles&0xff;

	/* export SO Info */
	pszAppInfo[APPINFO_LEN+1+iSubFiles*22] = iGotSoCount;

	for(iLoop=0; iLoop<iGotSoCount; iLoop++)
	{
		memcpy(pszAppInfo+APPINFO_LEN+1+iSubFiles*22+1+iLoop*sizeof(uaAppDepFile[0]), 
				uaAppDepFile[iLoop], sizeof(uaAppDepFile[0]));
	}
	
	if(iGotSoCount != 0)
		iRet = APPINFO_LEN+1+iSubFiles*22+1+iGotSoCount*sizeof(uaAppDepFile[0]);
	else
		iRet = APPINFO_LEN+1+iSubFiles*22+1;

    memset(aucTmp, 0x00, sizeof(aucTmp));
    memset(&sig, 0x00, sizeof(sig));
    ret = GetAppSigInfo(ucFindAppNo, &sig);
    if (!ret && sig.ucHeader==SIGNFORMAT1)
    {
        aucTmp[0] = 29;
        memcpy(aucTmp+1, sig.owner, 16);
        /*
                sprintf(aucTmp+17,"%c%c/%c%c/20%c%c",
                        (sig.validdate[1]>>4) + 0x30, (sig.validdate[1]&0x0f) + 0x30,
                        (sig.validdate[2]>>4) + 0x30, (sig.validdate[2]&0x0f) + 0x30,
                        (sig.validdate[0]>>4) + 0x30, (sig.validdate[0]&0x0f) + 0x30);
            */
        aucTmp[28] = sig.SecLevel + 0x30;
        memcpy(pszAppInfo+iRet, aucTmp, aucTmp[0]);
        iRet += (aucTmp[0]);
    }
    
    return iRet;		
}

/**
 获取应用名称
 @param[in,out]  pszAppName,输入调用时，传入应用程序的文件名，
 如果调用成功，返回应用的名称，必须保证pszAppName的缓冲
 大于等于33字节。
  
 @retval 0 成功
 @retval 其他，表示调用失败。
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/07/23--1.0.0.0-------创建
 */
 int s_iGetAppName(char * pszAppName)
{
	int iLoop,iRet,iFileId,iStartAddr;
	APPINFO stAppInfo;
	iFileId=s_open(pszAppName,O_RDWR,(unsigned char *)"\xff\x01");
	if(iFileId<0) 
	{
		return -1;	
	}
	seek(iFileId,APP_ENTRY_OFFSET,SEEK_SET);
	
	read(iFileId,(unsigned char *)pszAppName,32);	
	GetAppEntry(pszAppName, NULL, NULL, (ulong*)&iStartAddr);
	if(seek(iFileId,iStartAddr,SEEK_SET)<0)
	{
		close(iFileId);	
		return -2;
	}
	read(iFileId,(uchar*)&stAppInfo,sizeof(APPINFO));	
	close(iFileId);
	
	//strcpy(pszAppName,stAppInfo.AppName);
	memcpy(pszAppName,stAppInfo.AppName,sizeof(stAppInfo.AppName));
	pszAppName[sizeof(stAppInfo.AppName)]=0;
	
	return 0;
	
}


/**
 写入应用程序。
 @param[in] iAppNo，应用程序编号
 @param[in] pucAddr,文件存放起始地址
 @param[in]lFileLen,文件长度  
 @retval  0 成功
 @retval  -1 空间不够
 @retval -2 打开文件失败
  @retval -3写入文件失败
 @remarks
	 \li 作者-----------------时间--------版本---------修改说明 				   
	 \li PengRongshou---07/06/28--1.0.0.0-------创建
 */
int s_iWriteApp(int iAppNo,uchar * pucAddr,long lFileLen)
{
	char pszAppName[33];
	uchar aucAppFileTail[32];
	uchar aucAttr[3];
	int iFileId;
	long lTmp=0;

	if (lFileLen > MAX_APP_LEN)
	{
		return -1;
	}
	
	aucAttr[0]=0xff;
	aucAttr[2]=0;
	if(iAppNo==0) aucAttr[1]=0x00;
	else aucAttr[1]=0x01;
	sprintf(pszAppName,"%s%d",APP_FILE_NAME_PREFIX,iAppNo);

	lTmp = s_filesize(pszAppName,aucAttr);
	if(lTmp<0)
	{
		lTmp=0;
	}

	if(lTmp+freesize()<lFileLen)
	{
		return -1;
	}

	s_remove(pszAppName,aucAttr);

	iFileId = s_open(pszAppName,O_CREATE|O_RDWR,aucAttr);
	if(iFileId<0)
	{
		return -2;
	}
	lTmp = write(iFileId,pucAddr,lFileLen);
	if(lTmp!=lFileLen)
	{
        close(iFileId);
		return -3;		
	}
	memset(aucAppFileTail,0x00,32);
	memcpy(aucAppFileTail+8,DATA_SAVE_FLAG,8);
	write(iFileId,aucAppFileTail,16);
	close(iFileId);
	return 0;
}


int iGetAppType(unsigned char *AppHead)
{
    if(!memcmp(AppHead, MAPP_FLAG, strlen(MAPP_FLAG)))
    {
        return  0;        //  main app
    }
    else if(!memcmp(AppHead, SAPP_FLAG,strlen(SAPP_FLAG)))
    {
        return 1;        //  sub app
    }
	else
    {
        return -1;
    }
}

int s_iDLWriteApp(uchar *AppData, uint len)
{
	int ret,iAppType,iLoop,iTargetAppNo = -1,iFirstNullNo = -1;
    uint tmp=0;
    uchar szAppName[33], tmpbuf[33];
    tSignFileInfo SigInfo;

    ret = s_iVerifySignature(SIGN_FILE_APP,AppData,len,&SigInfo);
    if(ret==PUK_RET_SIG_TYPE_ERR) return -8;
    else if(ret!=0) return -1;

    memset(szAppName, 0, sizeof(szAppName));
    memcpy(&tmp,(uchar*)(AppData+8),3);//71 6e 61 63    
    memcpy(szAppName,(uchar*)(AppData + tmp * 4 + 16),32);//get the app name
    if(memcmp((uchar *)(AppData+len-16),SIGN_FLAG,16)!=0)
    {
        iAppType = iGetAppType((uchar*)AppData+len-strlen(MAPP_FLAG));
    }
    else
    {
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        memcpy(tmpbuf,(uchar *)(AppData+len-16-4),4);
        tmp =  tmpbuf[0]+(tmpbuf[1]<<8)
                        +(tmpbuf[2]<<16)+(tmpbuf[3]<<24);
    
        iAppType = iGetAppType((uchar*)AppData+tmp-strlen(MAPP_FLAG));
    }   
                
    if((iAppType!=0)&&(iAppType!=1)) return -2;
    
    //查找是否有存在重名的应用
    for(iLoop=0;iLoop<MAX_APP_NUM;iLoop++)
    {
        memset(tmpbuf, 0x00, sizeof(tmpbuf));
        sprintf(tmpbuf,"%s%d",APP_FILE_NAME_PREFIX,iLoop);
        
        if((ret = s_iGetAppName(tmpbuf))!=0)
        {
            //找到第一个不存在的子应用号
            if((iFirstNullNo<0)&&(iLoop>0))iFirstNullNo= iLoop;
            continue;
        }
        if(strcmp(tmpbuf,szAppName)==0)
        {
            //找到重名的应用号
            iTargetAppNo = iLoop;
            break;
        }
    }
    
    if(iAppType==0)/*下载的是主应用*/
    {
        if(iTargetAppNo>0) return -3;/*与现有子应用同名*/
        else if(iTargetAppNo!=0)
        {
            //没同名的管理器 删除应用管理器参数文件
            s_iDeleteApp(0);
        }
    
        iTargetAppNo = 0;
    }
    else if(iAppType==1)//下载的是子应用
    {
        if(iTargetAppNo==0) return -4;/*与现有主应用同名*/
        if(iTargetAppNo<1) iTargetAppNo=iFirstNullNo;
    }    
    if(iTargetAppNo<0) return -5;/*无可用应用编号*/
    if (len > MAX_APP_LEN) return -6;
    ScrClrLine(4,7);
    SCR_PRINT(0,4,1,"保存应用文件...","SAVE APP FILE...");
    
    ret = s_iWriteApp(iTargetAppNo,(uchar*)AppData,len);
    if(ret!= 0) return -7;
    SetAppSigInfo(iTargetAppNo, SigInfo);                               
	
    return iTargetAppNo;
}

extern volatile uchar k_SafeConfig[8];
int ReadSecuConfTab(uchar* pucOut)
{
	memcpy(pucOut,(uchar *)k_SafeConfig,sizeof(k_SafeConfig));
	return sizeof(k_SafeConfig);
}


int s_iRebuildFileSystem(void)
{
	int iSector;

	FILE_INFO astFileInfo[MAX_FILES];
	int fNums,iLoop;
	int iFileID,iFileLen;
	uchar aucBackupData[512*1024],aucAttr[2],aucFileName[33];
	ulong ulFileLength=0;
	int iBackFiles[MAX_FILES][2];//[0]用于保存astFileInfo中的下标；[1]用于保存aucBackupData的起始位置
	int iPedFileCount=0;
    
    uchar bpAttr[2];
    uchar *bpBuf = (uchar *)SAPP_RUN_ADDR;
    int bpFid,bpFile[50],bpNums=0,bpfSize=0,tmpd,i,j;
    
	//将ped文件拷贝到内存备份
	fNums=GetFileInfo(astFileInfo);
	if(fNums>0)
	{
		for(iLoop=0;iLoop<fNums;iLoop++)
		{
			if((astFileInfo[iLoop].attr==0xff)&&
			(astFileInfo[iLoop].type==FILE_TYPE_PED || astFileInfo[iLoop].type==FILE_TYPE_SYS_PARA ))
			{
				iBackFiles[	iPedFileCount][0] = iLoop;
				iPedFileCount++;
				ulFileLength +=astFileInfo[iLoop].length;
			}
		}
	}

	if(ulFileLength<=sizeof(aucBackupData))//如果PED文件总大小小于备份缓冲区，则备份文件
	{
		iBackFiles[0][1]=0;
		for(iLoop=0;iLoop<iPedFileCount;iLoop++)
		{
			aucAttr[0]=astFileInfo[iBackFiles[iLoop][0]].attr;
			aucAttr[1] = astFileInfo[iBackFiles[iLoop][0]].type;
			iBackFiles[iLoop+1][1]=iBackFiles[iLoop][1]+astFileInfo[iBackFiles[iLoop][0]].length;
			memset(aucFileName,0x00,sizeof(aucFileName));
			memcpy(aucFileName,astFileInfo[iBackFiles[iLoop][0]].name,sizeof(astFileInfo[iBackFiles[iLoop][0]].name));
			iFileID = s_open(aucFileName,O_RDWR,aucAttr);
			iFileLen =s_read(iFileID,aucBackupData+iBackFiles[iLoop][1],astFileInfo[iBackFiles[iLoop][0]].length);
			if(iFileLen!=astFileInfo[iBackFiles[iLoop][0]].length)
{
				//ScrCls();
				//ScrPrint(0,0,0x01,"backup ped\file fail!");getkey();getkey();
				iPedFileCount=0;//有个PED文件读取失败，不再备份所有PED文件
				break;
			}
			close(iFileID);
			//ScrPrint(0,0,0x00,"id:%d len:%d %d %s]",iFileID,iPedFiles[iLoop+1][1],iFileLen,astFileInfo[iPedFiles[iLoop][0]].name);getkey();

		}
	}
	else iPedFileCount =0;

    bpAttr[0] = FILE_ATTR_PUBSO;
    bpAttr[1] = FILE_TYPE_SYSTEMSO;
    for(i=0,bpNums=0,bpfSize=0;i<fNums;i++)
    {
        if(astFileInfo[i].attr==FILE_ATTR_PUBSO &&
            astFileInfo[i].type==FILE_TYPE_SYSTEMSO)
        {
                bpFid = s_open(astFileInfo[i].name, O_RDWR, bpAttr);
                if(bpFid>=0)
                {
                    tmpd = read(bpFid, bpBuf+bpfSize, astFileInfo[i].length);
                    close(bpFid);
                    if(tmpd==astFileInfo[i].length)
                    {
                        bpfSize+=astFileInfo[i].length;
                        bpFile[bpNums++]=i;
                    }
                }
        }
    }
    

    
	CreateFileSys();
	InitFileSys();
	LoadFontLib();

	for(iLoop=0;iLoop<iPedFileCount;iLoop++)
	{
		aucAttr[0]=astFileInfo[iBackFiles[iLoop][0]].attr;
		aucAttr[1] = astFileInfo[iBackFiles[iLoop][0]].type;
		memset(aucFileName,0x00,sizeof(aucFileName));
		memcpy(aucFileName,astFileInfo[iBackFiles[iLoop][0]].name,sizeof(astFileInfo[iBackFiles[iLoop][0]].name));

		iFileID = s_open(aucFileName,O_CREATE|O_RDWR,aucAttr);

		iFileLen =write(iFileID,aucBackupData+iBackFiles[iLoop][1],astFileInfo[iBackFiles[iLoop][0]].length);
		if(iFileLen!=astFileInfo[iBackFiles[iLoop][0]].length)
		{
			close(iFileID);
			s_remove(aucFileName,aucAttr);
			continue;
		}
		close(iFileID);
	}

    for(i=0,bpfSize=0;i<bpNums;i++)
    {
        j = bpFile[i];
        bpFid = s_open(astFileInfo[j].name, O_RDWR|O_CREATE, bpAttr);
        if(bpFid >= 0)
        {
            tmpd = write(bpFid, bpBuf+bpfSize, astFileInfo[j].length);
            close(bpFid);
            if(tmpd!=astFileInfo[j].length)
            {
                s_remove(astFileInfo[j].name, bpAttr);
            }
            bpfSize+=astFileInfo[j].length;
        }
    }
    
	return 0;
}

uchar ucGenLRC(char * pszBuf,int iLen)
{

	uchar ucTmp;
	int iLoop;
	for(iLoop=1,ucTmp=pszBuf[0];iLoop<iLen;iLoop++)
	{
		ucTmp ^= pszBuf[iLoop]; 
	}
	return ucTmp;
}

R_RSA_PUBLIC_KEY SN_KEY_PubKey =
{
   2048,
  {
   0xC9,0x92,0xFB,0xD4,0x60,0x3E,0xC5,0x2A,0x8B,0x69,0x15,0x3D,
   0x8A,0x6E,0xA7,0xA6,0x53,0x87,0xF4,0x1E,0x35,0x00,0x78,0x09,0x4C,0xA0,0xAF,0xE6,
   0x97,0x6C,0xC1,0xEB,0xA8,0xEA,0x68,0x33,0x74,0x7F,0x93,0x07,0x97,0xEB,0xC7,0xC0,
   0x59,0xA7,0x81,0x99,0x1D,0xE5,0x82,0xEF,0x25,0xA3,0xE1,0xDB,0x66,0xD1,0x4C,0x6C,
   0x48,0x7C,0xCC,0xA0,0xED,0xF8,0xC7,0x2A,0xC2,0x08,0x27,0x82,0x28,0x90,0xAB,0xAB,
   0x7B,0xAE,0x18,0x7E,0x2E,0xD3,0xAE,0x23,0x42,0x9B,0xC9,0x9D,0x03,0x2E,0x3B,0xF6,
   0x7F,0xB6,0xBB,0xEC,0x3E,0x5B,0x7A,0x0E,0x35,0x27,0x86,0xAB,0xE3,0xEE,0x1C,0x24,
   0xF5,0x0F,0xC0,0x4A,0x84,0xCF,0xCC,0x42,0xDD,0x59,0x79,0x1C,0xE7,0x31,0xD6,0xB5,
   0x22,0x87,0xCE,0x07,0xA6,0xE3,0x8D,0xA8,0x81,0x48,0x9C,0x83,0x4A,0x1E,0x92,0x92,
   0x23,0x5C,0x41,0x51,0xE0,0x4C,0x1C,0x1E,0x16,0xBF,0x46,0x9B,0x64,0xE2,0x7E,0x71,
   0x61,0x91,0xB5,0xA7,0xD2,0x8D,0xA9,0xE3,0x5F,0x28,0xF2,0xF8,0xC4,0x12,0xB9,0xF7,
   0x91,0xED,0x0B,0x13,0x34,0xF4,0x8D,0xB1,0x72,0x88,0xE9,0xFF,0xC0,0xF1,0x6B,0x5F,
   0x8F,0xBE,0x3F,0x79,0x70,0x33,0xA2,0x97,0x7C,0x6C,0x1D,0xE6,0x42,0xAB,0xBF,0x7F,
   0x32,0xC5,0x9E,0xF2,0xA4,0x45,0x2C,0x0C,0x75,0xFA,0x53,0xA6,0x65,0x5F,0xF8,0x37,
   0x63,0x36,0x61,0xCB,0x3B,0x18,0xC5,0xE3,0xC9,0x2E,0x1B,0x73,0x00,0x55,0x37,0x43,
   0x7E,0xA9,0x05,0xD5,0x89,0x41,0x67,0x48,0x89,0x72,0xA0,0x0E,0x61,0x46,0x86,0xF6,
   0x69,0xE2,0x6D,0x6F
  },
  {
    0x00,0x01,0x00,0x01
  }
};

int DLGetSNKeyInfo(uchar* outdata)
{
    uchar tmpbuf[512];
	uchar snkbuf[512];
    int i, ret;

    i = 0;

    //terminal name
    memset(tmpbuf, 0x00, sizeof(tmpbuf));
    get_term_name(tmpbuf);
    outdata[i++] = strlen(tmpbuf);
    memcpy(outdata+i, tmpbuf, strlen(tmpbuf));
    i += strlen(tmpbuf);
    
    //SN
    memset(tmpbuf, 0x00, sizeof(tmpbuf));
    ReadSN(tmpbuf);
    outdata[i++] = strlen(tmpbuf);
    memcpy(outdata+i, tmpbuf, strlen(tmpbuf));
    i += strlen(tmpbuf);

	//GET SNK
    ST_RSA_KEY stRsapukkey;
    memset(&stRsapukkey, 0x00, sizeof(ST_RSA_KEY));
    stRsapukkey.iModulusLen = SN_KEY_PubKey.uiBitLen;
    memcpy(&stRsapukkey.aucModulus[512 - SN_KEY_PubKey.uiBitLen/8], &SN_KEY_PubKey.aucModulus[0], SN_KEY_PubKey.uiBitLen/8);
    stRsapukkey.iExponentLen = 32;
    memcpy(&stRsapukkey.aucExponent[512 - 4], &SN_KEY_PubKey.aucExponent[0], 4);
    memset(snkbuf, 0x00, sizeof(snkbuf));
    ret = s_PedExportIdKey(1, &stRsapukkey,snkbuf, 0);
    if (ret != 0) return ret;
	

    //SN KEY KCV
    memset(tmpbuf, 0x00, sizeof(tmpbuf));
    ret = s_PedGetKcv(0x45, 1, tmpbuf, 0); //PED_TIDK 0x45 ,identity key
    outdata[i++] = 3;//pci4规定
    memcpy(outdata+i, tmpbuf, 3);
    i += 3;


	//SN KEY INFO
    outdata[i++] = ((stRsapukkey.iModulusLen/8)>>8)&0xff;//
    outdata[i++] = (stRsapukkey.iModulusLen/8)&0xff;
    memcpy(outdata+i, snkbuf, stRsapukkey.iModulusLen/8);
    i += stRsapukkey.iModulusLen/8;

    return i;
}

int DLSetSNKeyState(uchar* data)
{
    int ret;
    
    ret = s_PedSetIdKeyState(1, data[0]);

    return ret;
}

int DLSetTUSNFlag(uchar* data)
{
    int ret;

    if (data[0] != 0x01) return -1;
    ret = WriteTUSN();
    if (ret) return -2;
    
    return 0;
}


