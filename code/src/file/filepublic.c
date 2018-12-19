#include <stdio.h>
#include <string.h>
#include "filedef.h"
#include "base.h"

#include "..\download\usbhostdl.h"
#include "..\puk\puk.h"
#include "..\download\localdownload.h"
#include "..\font\font.h"
#include "..\flash\nand.h"

static ushort s_EnvId  = 0xFFFF;
static uchar s_EnvData[0x10000],s_EnvAppNum=0xFF;
static int s_EnvLen = 0;

//---------获取bin文件的信息
int GetFileBinInfo(uchar *file,uint len,APPINFO *info)
{
	int iRet;
	image_header_t hdr;
	uint *tptr,tmp[2],tt;

	memset(&hdr,0,sizeof(hdr));
	if(len < sizeof(image_header_t)) return -1;
	memcpy((uchar *)tmp,&file[sizeof(image_header_t)],8);
	tptr = (uint *)tmp;
	tptr += 1;
	if(*tptr < 0x20000000) return -1;
	tt = *tptr - 0x20000000;
	memcpy(info,&file[sizeof(image_header_t)+tt],sizeof(APPINFO));
	return 0;
}

void Invalid_Env_Cache(void)
{
    s_EnvId = 0xFFFF;
    s_EnvAppNum=0xFF;
    memset(s_EnvData,0,sizeof(s_EnvData));
}
unsigned char GetEnv(char *envname,unsigned char *value)
{
    int fd,offset,len;
    uchar buff[128],attr[2],name[8];
    APPINFO CurAppInfo;

    if(GetCallAppInfo(&CurAppInfo))    return -1;
    if(envname==NULL || envname[0]==0) return 0x01;
    read_env:    
    if((s_EnvId < 256) &&(s_EnvAppNum ==CurAppInfo.AppNum))
    {
        //--close the ENV file for compatibility
        close(s_EnvId);
    	        
        memcpy(name,envname,7);
        name[7]=0;
        for(offset=0;offset< s_EnvLen;offset+=128)
        {
            memcpy(buff,s_EnvData+offset,8);
            buff[7]=0;
            if(!strcmp(name,buff)){
                memcpy(buff,s_EnvData+offset+8,120);
                strcpy(value,buff);
                return 0;        
            }
        }
        return 0x01;
    }
    attr[0] = CurAppInfo.AppNum;
    attr[1] = 6;
    fd=s_open(ENV_FILE, O_RDWR, attr);
    if(fd<0) return 0x01;
    memset(s_EnvData,0,sizeof(s_EnvData));
    s_EnvLen = read(fd,s_EnvData,sizeof(s_EnvData));
    close(fd);
    if(s_EnvLen <= 0) return 0x01;
    s_EnvAppNum = CurAppInfo.AppNum;
    s_EnvId =fd;
    goto read_env;

    return 0;
}

unsigned char PutEnv(char *envname,unsigned char *value)
{
    int fd,len,offset=0;
    uchar buff[128],name[8];
    uchar attr[4];
    APPINFO CurAppInfo;
    if(GetCallAppInfo(&CurAppInfo))    return -1;
    if(envname[0]==0 || envname==NULL || value==NULL) return 0x01;
    attr[0] = CurAppInfo.AppNum;
    attr[1] = 6;
    s_EnvAppNum = CurAppInfo.AppNum;
    fd=s_open(ENV_FILE, O_RDWR|O_CREATE, attr);
    if(fd<0) return 0x02;
    if(s_EnvId != fd)
    {
        memset(s_EnvData,0,sizeof(s_EnvData));
        s_EnvId = 0xFFFF;
        s_EnvLen=read(fd,s_EnvData,sizeof(s_EnvData));
        if(s_EnvLen<0){
            close(fd);
            return 0x02;
        } 
        s_EnvId = fd;
    }
    memcpy(name,envname,7);
    name[7]=0;
    for(offset=0;offset< s_EnvLen;offset+=128)
    {
        memcpy(buff,s_EnvData+offset,8);
        buff[7]=0;
        if(!strcmp(name,buff))break;
    } 

    len =strlen(value);
    if(len>119) len=119;
    if(offset<s_EnvLen) /*modify value*/
    {
    		/*--check both the value and file size,to avoid a record with invalid length*/
        if(!strcmp(s_EnvData+offset+8,value) && (offset+128<=s_EnvLen)){
            close(fd);
            return 0;    	
        }/*put a same item,do nothing*/

        memset(buff,0,sizeof(buff));
        memcpy(buff,name,8);
        memcpy(buff+8,value,len);
        buff[127]=0;  /*add a '\0' for string end flag*/
        seek(fd,offset,SEEK_SET);
        if(write(fd,buff,128)!=128){
            s_EnvId = 0xFFFF;
            close(fd);
            return 0x02;	
        }
        memcpy(s_EnvData+offset,buff,128);
        close(fd);
        return 0;
    }
    /*put  a new item*/
    if (s_EnvLen >= sizeof(s_EnvData)){
        close(fd);
        return 0x02;
    }        
    seek(fd,0,SEEK_END);
    memset(buff,0,sizeof(buff));
    memcpy(buff,name,8);
    memcpy(buff+8,value,len);
    buff[127]=0;  /*add a '\0' for string end flag*/
    s_EnvId = 0xFFFF;
    memset(s_EnvData,0,sizeof(s_EnvData));
    if(write(fd,buff,128)!=128){
        close(fd);
        return 0x02;
    }    
    close(fd);
    return 0;
}

/**
ucType:
\li 1 整数
\li 2 字符
\li 3 字符串
*/
int  iGetInput(uchar ucType,uchar ucCol,uchar ucLine,char*Prompt,char*OutPut)
{
	uchar aucTmp[128];
	uchar ucTmp;
	uchar ucRet;
	ScrGotoxy(ucCol, ucLine);
	if(Prompt!=NULL)
	{
		Lcdprintf("%s\n",Prompt);
	}
	memset(aucTmp,0x00,sizeof(aucTmp));
	if(OutPut!=NULL&&ucType==3)
	{
		if(OutPut[0]!=0)
		snprintf(aucTmp,22,"%s",OutPut);
	}
	ucRet =GetString(aucTmp,0xB1,0,sizeof(aucTmp)-1);
	if(ucRet==0x00)
	{
		if((ucType==3)&&(OutPut!=NULL))
		{
			memcpy(OutPut,aucTmp+1,aucTmp[0]);
			OutPut[aucTmp[0]]=0;
			return aucTmp[0];
		}
		if(ucType==1)
		{
			return atol(aucTmp+1);
		}
		if(ucType==2)
		{
			return aucTmp[1];
		}
	}
//	Lcdprintf("[%02X]\n",ucRet);getkey();
	if(0xFF==ucRet)return -1;
	return -2;
}

extern void LoadFontLib(void);
int FileToApp(uchar *FileName)
{ 
	//uchar ptrAppData[MAX_APP_LEN + 32];// = 0x28600000;
	// 因为堆栈空间不够,暂时使用字库的空间作为存储,转换结束后重新加载字库
	uchar *ptrAppData=(uchar *)FILE_transfer_buff;

	uchar szAttr[3],szAppDataLen[5],szAppHead[20],szAppName[50],szAppFileName[50];
	int iFid,iRet,iAppType,iAppDataLen,iLoop,iTargetAppNo = -1,iFirstNullNo = -1;
	long lFileLen;
	ulong ulTemp = 0;
	tSignFileInfo stSigInfo;	
	APPINFO CurAppInfo;

	uchar aucAppFileTail[32];
    char pszAppName[33];
    uchar aucAttr[3];

	SaveCurFont();			// 保存当前选择的字体
//读取该应用
	if (FileName == NULL)
	{
		return FTO_RET_PARAM_ERR;
	}
	if (filesize(FileName) > (MAX_APP_LEN))
	{
		return FTO_RET_FILE_TOOBIG;
	}

    if(GetCallAppInfo(&CurAppInfo))
	{
		return FTO_RET_APPINFO_ERR;
	}

    szAttr[0]=CurAppInfo.AppNum;
    szAttr[1]=0x04;

    if((iFid=s_open(FileName, O_RDWR, szAttr)) < 0)
	{
		return FTO_RET_FILE_NO_EXIST;
	}
	if((lFileLen = filesize(FileName)) <= 16)
	{
        close(iFid);
		return FTO_RET_APP_TYPE_ERR;
	}

	seek(iFid,0,SEEK_SET);

	if (read(iFid,ptrAppData,lFileLen) != lFileLen)
	{
	    LoadFontLib();
		RestoreCurFont();
		close(iFid);
		return FTO_RET_READ_FILE_ERR;
	}
	
	if((iRet = s_iVerifySignature(SIGN_FILE_APP,ptrAppData,lFileLen,&stSigInfo)) != 0)
	{
	    LoadFontLib();
		RestoreCurFont();
		close(iFid);
		return FTO_RET_SIG_ERR;
	}
/*	else if (!CheckIfDevelopPos())
	{
		if(memcmp(ptrAppData+lFileLen-16,SIGN_FLAG,16)!=0)//因为有的监控没有去验证签名
		{
			return FTO_RET_SIG_ERR;
		}
	}*/
	
	if((SIGN_FILE_APP!=stSigInfo.ucFileType)&&(!CheckIfDevelopPos())) //debug版的monitor不验证签名
	{
		LoadFontLib();
		RestoreCurFont();
		close(iFid);
		return FTO_RET_SIG_ERR;
	}


	if(memcmp(ptrAppData+lFileLen-16,SIGN_FLAG,16)!=0)
	{
		//如果是未签名文件，文件末尾没有签名标志
		iAppDataLen = lFileLen;
	}
	else
	{
		seek(iFid,lFileLen-20,SEEK_SET);
		memset(szAppDataLen,0,5);
		///必须是签名的文件，所以要再次计算APP的长度，不加签名的信息
		if(read(iFid,szAppDataLen,4) != 4)
		{
		    LoadFontLib();
			RestoreCurFont();
			close(iFid);
			return FTO_RET_READ_FILE_ERR;
		}

		iAppDataLen =  ((uchar)szAppDataLen[0]) +((uchar)szAppDataLen[1]<<8)	+((uchar)szAppDataLen[2]<<16)	+((uchar)szAppDataLen[3]<<24);
	}
	
	seek(iFid,iAppDataLen-strlen(MAPP_FLAG),SEEK_SET);
	
	memset(szAppHead,0,20);

	if(read(iFid,szAppHead,strlen(MAPP_FLAG)) !=strlen(MAPP_FLAG))
	{
		LoadFontLib();
		RestoreCurFont();
		close(iFid);
		return FTO_RET_READ_FILE_ERR;
	}

	iAppType = iGetAppType(szAppHead);

	if((iAppType!=0)&&(iAppType!=1))
	{
		
	    LoadFontLib();
		RestoreCurFont();
		close(iFid);
		return FTO_RET_APP_TYPE_ERR;
	}

	memset(szAppName,0,sizeof(szAppName));
	memcpy(&ulTemp,ptrAppData+8,3);
	memcpy(szAppName,ptrAppData + ulTemp * 4 + 16,32);//get the app name

	//查找是否有存在重名的应用
	for(iLoop=0;iLoop<MAX_APP_NUM;iLoop++)
	{
		sprintf(szAppFileName,"%s%d",APP_FILE_NAME_PREFIX,iLoop);
		
		if((iRet = s_iGetAppName(szAppFileName))!=0)
		{	
			//找到第一个不存在的子应用号
			if((iFirstNullNo<0)&&(iLoop>0))iFirstNullNo= iLoop;
			continue;
		}

		if(strcmp(szAppFileName,szAppName)==0)
		{
			//找到重名的应用号
			iTargetAppNo = iLoop;
			break;
		}
	}

	if(iAppType==0)
	{
		//下载的是应用管理器,应该为0才是主应用
		if(iTargetAppNo>0)
		{
			LoadFontLib();
			RestoreCurFont();
			return FTO_RET_NAME_DEUPT;
		}
		else if(iTargetAppNo!=0)
		{
			//没同名的管理器 删除应用管理器参数文件 filetoapp文件不能删除
			s_iDeleteAppParaFile(0);
		}
		iTargetAppNo = 0;
	}
	else if(iAppType==1)//下载的是子应用
	{
		if(iTargetAppNo==0)
		{
			//子应用与已经存在的主应用重名，拒绝下载
            close(iFid);
			LoadFontLib();
			RestoreCurFont();
			return FTO_RET_NAME_DEUPT;
		}
		if(iTargetAppNo<1)
		{
			iTargetAppNo=iFirstNullNo;
		}
	}
	
	if(iTargetAppNo < 0)
	{
        close(iFid);
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_TOOMANY_APP;
	}
    SetAppSigInfo(iTargetAppNo, stSigInfo);                                    

	aucAttr[0]=0xff;
	aucAttr[2]=0;
	if(iTargetAppNo==0) aucAttr[1]=0x00;
	else aucAttr[1]=0x01;
	memset(aucAppFileTail,0x00,32);
	memcpy(aucAppFileTail+8,DATA_SAVE_FLAG,8);
	seek(iFid,lFileLen,SEEK_SET);
	write(iFid,aucAppFileTail,16);
	close(iFid);
	sprintf(pszAppName,"%s%d",APP_FILE_NAME_PREFIX,iTargetAppNo);
	s_remove(pszAppName,aucAttr);
	s_RenameFile(iFid,pszAppName,aucAttr);
	
	LoadFontLib();
	RestoreCurFont();
	return FTO_RET_OK;
}

int FileToParam(uchar *FileName,uchar *AppName,int iType)
{
	long lFileLen;
	int iFid,iAppId;
	uchar szAttr[3];
	// 因为堆栈空间不够,暂时使用字库的空间作为存储,转换结束后重新加载字库
	uchar *PtrParam=(uchar *)FILE_transfer_buff ;
	APPINFO CurAppInfo;
	
	SaveCurFont();
	if (FileName == NULL || AppName == NULL || (iType != 0 && iType != 1))
	{
		return FTO_RET_PARAM_ERR;
	}

	if (filesize(FileName) > (MAX_PARAM_LEN))
	{
		return FTO_RET_FILE_TOOBIG;
	}

//读取该应用
    if(GetCallAppInfo(&CurAppInfo))
	{
		return FTO_RET_APPINFO_ERR;
	}

    szAttr[0]=CurAppInfo.AppNum;
    szAttr[1]=0x04;

    if((iFid=s_open(FileName, O_RDWR, szAttr)) < 0)
	{
		return FTO_RET_FILE_NO_EXIST;
	}

	lFileLen = filesize(FileName);

	seek(iFid,0,SEEK_SET);

	if (read(iFid,PtrParam,lFileLen) != lFileLen)
	{
		close(iFid);
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_READ_FILE_ERR;
	}

	close(iFid);

	if((iAppId = GetAppId(AppName)) < 0)//获取应用ID
	{
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_NO_APP;
	}

	s_removeId(iFid);

	szAttr[0] = iAppId&0xff;

	if (iType == 0)
	{
		szAttr[1] = 0x06&0xff;
		s_remove(ENV_FILE, szAttr);
		iFid = s_open(ENV_FILE,O_CREATE|O_RDWR,szAttr);
	}
	else if(iType == 1)
	{
		szAttr[1] = 0x04&0xff;
		s_remove(FileName, szAttr);		
		iFid = s_open(FileName,O_CREATE|O_RDWR,szAttr);
	}

	if (iFid < 0)
	{
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_WRITE_FILE_ERR;
	}

	seek(iFid,0,SEEK_SET);

	if (write(iFid,PtrParam,lFileLen) != lFileLen)
	{
        close(iFid);
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_WRITE_FILE_ERR;
	}
	
	close(iFid);	
	LoadFontLib();
	RestoreCurFont();
	return FTO_RET_OK;
}


int FileToFont(uchar *FileName)
{
	long lFileLen;
	int iFid,iAppId;
	uchar szAttr[3],szCheckData[10];//* PtrFont = 0x28600000;
	APPINFO CurAppInfo;
//读取该应用

	if (FileName == NULL)
	{
		return FTO_RET_PARAM_ERR;
	}

    if(GetCallAppInfo(&CurAppInfo))
	{
		return FTO_RET_APPINFO_ERR;
	}
//
    szAttr[0]=CurAppInfo.AppNum;
    szAttr[1]=0x04;

    if((iFid=s_open(FileName, O_RDWR, szAttr)) < 0)
	{
		return FTO_RET_FILE_NO_EXIST;
	}

	//判断字库的格式是否正确
	seek(iFid,0,SEEK_SET);

	if (read(iFid,szCheckData,10) != 10)//小于100个字节，肯定是非法的字库文件
	{
		close(iFid);
		return FTO_RET_FONT_ERR;
	}

	if (szCheckData[9]>MAX_FONT_NUMS || memcmp(szCheckData,"PAX-FONT",8) || (szCheckData[8] < 10 || szCheckData[8] > 99))
	{
		close(iFid);
		return FTO_RET_FONT_ERR;
	}	


	s_remove(FONT_FILE_NAME,(uchar *)"\xff\x02");	
	s_RenameFile(iFid,FONT_FILE_NAME,"\xff\x02");
	close(iFid);
	LoadFontLib();//重新加载字库	
	return FTO_RET_OK;
}

int FileToPuk(int PukType, int PukIdx,uchar *FileName)
{ 
	uchar ptrAppData[USER_PUK_LEN], szAttr[3];
	int iFid,iRet;
	long lFileLen;
	APPINFO CurAppInfo;

	if(PukType!=1||PukIdx!=1)return -16;
	if (FileName == NULL) return -2;
	if (filesize(FileName) > (USER_PUK_LEN)) return -14;
    if(GetCallAppInfo(&CurAppInfo)) return -1;

    szAttr[0]=CurAppInfo.AppNum;
    szAttr[1]=0x04;
    if((iFid=s_open(FileName, O_RDWR, szAttr)) < 0)
	{
		return -2;
	}
	lFileLen = filesize(FileName);

	seek(iFid,0,SEEK_SET);
	if (read(iFid,ptrAppData,lFileLen) != lFileLen)
	{
	    close(iFid);
		return -8;
	}
	iRet=s_WritePuk(ID_US_PUK,ptrAppData,lFileLen);
	close(iFid);	
	s_removeId(iFid);//删除用户文件
	if(iRet)return -3;
	return 0;
}

#define MON_BLOCK_VALID_FLAG	"MON VALID BLOCK!"
#define MON_BLOCK_SIZE			(BLOCK_SIZE-PAGE_SIZE)
static int check_bad_block(uint block_num)
{
	uchar rbuf[PAGE_SIZE+64];
	int i,j;

	for (i = 0; i < BLOCK_SIZE/PAGE_SIZE; i++)
	{
		umc_nand_page_rd((block_num*BLOCK_SIZE+i*PAGE_SIZE)*2, rbuf, PAGE_SIZE+64);
		for(j=0;j<PAGE_SIZE+64;j++)
		{
			if(rbuf[j]!=0xff) 	return -1;
		}
	}

	return 0;
}

static int monitor_nand_erase(uint block_num)
{
	int ret,i;
    if (block_num >= MONITOR_END_ADDR/BLOCK_SIZE) return -2;
    for(i=0;i<3;i++){
        nand_phy_erase(block_num);
    	ret=check_bad_block(block_num);
    	if(!ret) return ret;
    }
	return -1;
}
static int monitor_nand_phy_page_read(uint page_addr, uchar* dst_addr)
{
	uint i,j;
	uchar rbuf[(PAGE_SIZE+64)];
	int ret;
	if(page_addr<MONITOR_BASE_ADDR*2 || page_addr >MONITOR_END_ADDR*2) return -1;

	umc_nand_page_rd(page_addr, rbuf,PAGE_SIZE +64);
	for(i=PAGE_SIZE;i<(PAGE_SIZE+64);i++) 
	{
		if(rbuf[i] !=0xFF) break;
	}

	if(i ==PAGE_SIZE+64) /*空块不进行ecc纠错*/
	{
		memcpy(dst_addr,rbuf,PAGE_SIZE);
		return 0;
	}

	return ecc_correct((uint *)rbuf, dst_addr);
}

extern uint* cal_ecc(uint *datain);
static uint monitor_nand_phy_page_write(uint page_addr, uchar* src_addr)
{
	uint i,j;
	uchar hash_buf[32];
	uint *ecc_buf;
	uchar rbuf[(PAGE_SIZE+64)];

	if(page_addr<MONITOR_BASE_ADDR*2 || page_addr >MONITOR_END_ADDR*2) return -2;

	ecc_buf = cal_ecc((uint *)src_addr);
	sha1(src_addr,PAGE_SIZE,hash_buf);
	memcpy(src_addr+PAGE_SIZE,ecc_buf,32);
	memcpy(&src_addr[(PAGE_SIZE+32)],hash_buf,HASH_SIZE);
	memcpy(&src_addr[(PAGE_SIZE+32+HASH_SIZE)],hash_buf,HASH_SIZE);

	umc_nand_page_prg( page_addr, src_addr, PAGE_SIZE + 64);

	if (monitor_nand_phy_page_read(page_addr, rbuf) || memcmp(src_addr, rbuf, PAGE_SIZE))
		return -1;
	
	return 0;
}

static int monitor_nand_read(uint offset, uchar *data, uint data_len)
{
	volatile uint i;
	uint start_sector, start_offset, end_offset,end_sector,blk_num,blk_info;
	uint number_of_sectors, leave_bytes, page_addr;
	uchar rbuf[PAGE_SIZE];
	int ret;
//printk("s_nand_read block:%x,phyPage:%x,len:%d!\r\n",offset/BLOCK_SIZE,(offset%BLOCK_SIZE)/PAGE_SIZE,data_len);

	start_sector = offset/PAGE_SIZE;
	start_offset = offset%PAGE_SIZE;
	if((start_offset + data_len)<=PAGE_SIZE) leave_bytes =data_len;
	else leave_bytes =PAGE_SIZE - start_offset;
	
	end_sector = (offset + data_len)/PAGE_SIZE;
	end_offset = (offset + data_len)%PAGE_SIZE;

	if (end_offset )  number_of_sectors = end_sector - start_sector + 1;
	else			  number_of_sectors = end_sector - start_sector;

	page_addr = start_sector * 2 * PAGE_SIZE;
	
	for( i = 0 ; i < number_of_sectors ; i++ )
	{
		if(i == 0)
		{
			/* offset is not the begining of the first page to read */
			ret = monitor_nand_phy_page_read(page_addr, rbuf);
			if(ret <0) return ret;
			memcpy(data, rbuf + start_offset,leave_bytes);
			data += leave_bytes;
			data_len -= leave_bytes;
		}
		else if(i == (number_of_sectors-1))
		{
			/* last page to read */
			ret = monitor_nand_phy_page_read(page_addr, rbuf);
			if(ret <0) return ret;
			memcpy(data,rbuf,data_len);
			return ret;
		}
		else
		{
			/* pages between first and last pages to read */
			ret = monitor_nand_phy_page_read(page_addr, data);
			if(ret <0) return ret;
			data += PAGE_SIZE;
			data_len -= PAGE_SIZE;
		}
		page_addr += 2 *PAGE_SIZE;
	}
	return 0;
}

static int monitor_nand_write(uint offset,uchar *data, uint data_len)
{
	volatile uint i;
	uint start_sector, start_offset, end_sector, end_offset;
	uint number_of_sectors, leave_bytes, page_addr;
	uchar rbuf[PAGE_SIZE + 64];
	int ret;
	

	start_sector = offset/PAGE_SIZE;
	start_offset = offset%PAGE_SIZE;
	if((start_offset + data_len)<=PAGE_SIZE) leave_bytes =data_len;
	else leave_bytes =PAGE_SIZE - start_offset;
		
	end_sector = (offset + data_len)/PAGE_SIZE;
	end_offset = (offset + data_len)%PAGE_SIZE;

	if (end_offset )  number_of_sectors = end_sector - start_sector + 1;
	else		      number_of_sectors = end_sector - start_sector;

	page_addr = start_sector * 2 * PAGE_SIZE;
	//umc_ecc_config(ECC_NOJUMP, ECC_RD_END, ECC_ENABLE, ECC_PAGE_2048); 

	for( i = 0 ; i < number_of_sectors ; i++ )
	{
		memset(rbuf,0xff,sizeof(rbuf));
		if(i == 0) {
			/* offset is not the begining of the first page to write */
			memcpy(rbuf + start_offset, data,leave_bytes);
			ret=monitor_nand_phy_page_write(page_addr, rbuf);
			if(ret !=0) return ret;
			data += leave_bytes;
			data_len -= leave_bytes;
		}
		else if(i == (number_of_sectors-1)) {
			/* last page to write */
			memcpy((void *)rbuf, data, data_len);
			ret=monitor_nand_phy_page_write(page_addr, rbuf);
			return ret;
		}
		else {
			/* pages between first and last pages to write */
			memcpy(rbuf, data, PAGE_SIZE);
			ret=monitor_nand_phy_page_write(page_addr, rbuf);
			if(ret !=0) return ret;
			
			data += PAGE_SIZE;
			data_len -= PAGE_SIZE;
		}
		page_addr += PAGE_SIZE*2;
	}

	return 0;
}
int WriteMonitor(uchar *PtrMonitor, long len)
{
	int i,cnt,len2,ret,block_num;
	APPINFO MonitorInfo;

	if (PtrMonitor == NULL) return FTO_RET_PARAM_ERR;

	if(s_GetBootSecLevel())//新安全方案下限制安全等级低与boot安全等级的monitor下载
	{
		//---------获取所要写入的Monitor的信息
		GetFileBinInfo(PtrMonitor,len,&MonitorInfo);
		//---------如果所要升级的Monitor的安全等级比Boot低则不允许写入，并返回参数错误
		if(MonitorInfo.security_level<s_GetBootSecLevel()) return FTO_RET_PARAM_ERR;
	}
	block_num= len/MON_BLOCK_SIZE + (len%MON_BLOCK_SIZE ? 1:0);
	i = MONITOR_BASE_ADDR / BLOCK_SIZE;//起始块
	cnt = 0;
	while(cnt < block_num)
	{
		if (i == MONITOR_END_ADDR/BLOCK_SIZE) return FTO_RET_WRITE_FILE_ERR;
	
		if(len > MON_BLOCK_SIZE)	 len2 = MON_BLOCK_SIZE;
		else	 len2 = len;

		if (monitor_nand_erase(i))
		{	
			i++;
			continue;
		}
		
		ret = monitor_nand_write(i*BLOCK_SIZE, (unsigned char *)PtrMonitor, len2);
		if (ret)
		{
			i++;
			continue;
		}
		ret = monitor_nand_write(i*BLOCK_SIZE+MON_BLOCK_SIZE, (uchar *)(MON_BLOCK_VALID_FLAG), strlen(MON_BLOCK_VALID_FLAG));
		if (ret)
		{
			i++;
			continue;
		}
	
		len -= len2;
		PtrMonitor += len2;
		cnt++;
		i++;
	}

	return FTO_RET_OK;
}
int ReadMonitor(uint addr, uchar *data, uint data_len)
{
	uint block,off,target_block;
	int ret,i;
	uchar buff[32];
	uint flag_len=strlen(MON_BLOCK_VALID_FLAG);
	block = MONITOR_BASE_ADDR/BLOCK_SIZE;              /*start block of Monitor*/
	off =(addr - MONITOR_BASE_ADDR) % MON_BLOCK_SIZE;   /*offset in the block of last block*/
	target_block =(addr- MONITOR_BASE_ADDR)/MON_BLOCK_SIZE; /*target block number,start block is 0*/
         
	for (i=0;block < DATA_START_NUM; block++)
	{                 
		ret = monitor_nand_read(block*BLOCK_SIZE+MON_BLOCK_SIZE, buff,flag_len);
		if(ret) continue; 
		if(memcmp(buff, MON_BLOCK_VALID_FLAG,flag_len)) continue;
		if(i == target_block) break; /*vaild block number equals to target block number*/
		i++;
	}        

	if (block == DATA_START_NUM) return -2;
	if((data_len+off) <= MON_BLOCK_SIZE)  /*Only need one block*/
	{
		ret=monitor_nand_read(block*BLOCK_SIZE +off,data,data_len);
		return ret;
	}

	ret=monitor_nand_read(block*BLOCK_SIZE +off,data,MON_BLOCK_SIZE-off); /*read first block*/
	if(ret) return ret;
	data_len -= MON_BLOCK_SIZE-off;
	data += MON_BLOCK_SIZE-off;
         
	for(block+=1;block < DATA_START_NUM;block++) /*read last block */
	{
		ret = monitor_nand_read(block*BLOCK_SIZE+MON_BLOCK_SIZE, buff,flag_len);
		if (ret || memcmp(buff, MON_BLOCK_VALID_FLAG,flag_len)) continue;
                   
		if(data_len > MON_BLOCK_SIZE) /*middle blocks*/
		{
			ret=monitor_nand_read(block*BLOCK_SIZE, data, MON_BLOCK_SIZE);
			if(ret) return ret;
			data_len -= MON_BLOCK_SIZE;
			data += MON_BLOCK_SIZE;
			continue;                             
		}
		else /*the last block*/
		{
			ret=monitor_nand_read(block*BLOCK_SIZE,data,data_len);
			return ret;                                    
		}
	}

	return 0;
}
int FileToMonitor(uchar *FileName)
{
	long lFileLen;
	int iFid, ret;//最大1.5M
	uchar szAttr[3];
	// 因为堆栈空间不够,暂时使用字库的空间作为存储,转换结束后重新加载字库
	uchar *PtrMonitor=(uchar *)FILE_transfer_buff ;
	APPINFO CurAppInfo;

	SaveCurFont();
	if (FileName == NULL)
	{
		return FTO_RET_PARAM_ERR;
	}	
		
	if (filesize(FileName) > (MAX_MON_LEN))
	{
		return FTO_RET_FILE_TOOBIG;
	}

	//读取该应用
    if(GetCallAppInfo(&CurAppInfo))
	{
		return FTO_RET_APPINFO_ERR;
	}
//
    szAttr[0]=CurAppInfo.AppNum;
    szAttr[1]=0x04;

    if((iFid=s_open(FileName, O_RDWR, szAttr)) < 0)
	{
		return FTO_RET_FILE_NO_EXIST;
	}

	if((lFileLen = filesize(FileName)) <= 16)
	{
		close(iFid);
		return FTO_RET_APP_TYPE_ERR;
	}

	seek(iFid,0,SEEK_SET);

	if (read(iFid,PtrMonitor,lFileLen) != lFileLen)
	{
		close(iFid);
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_READ_FILE_ERR;
	}

	close(iFid);

	if (s_iVerifySignature(SIGN_FILE_MON,PtrMonitor,lFileLen, NULL))
	{
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_SIG_ERR;
	}

	s_removeId(iFid);
	ret = WriteMonitor(PtrMonitor, lFileLen);/*写入monitor*/

	LoadFontLib();
	RestoreCurFont();
	return ret;
}
int FileToDriver(uchar *FileName,uint Type)
{
	long lFileLen;
	int iFid, ret;//最大2M
	uchar szAttr[3];
	// 因为堆栈空间不够,暂时使用字库的空间作为存储,转换结束后重新加载字库
	uchar *PtrDriver=(uchar *)FILE_transfer_buff ;
	APPINFO CurAppInfo,DriverInfo;

	SaveCurFont();
	if(!is_hasbase()) return FTO_RET_NO_BASE;
	if (FileName == NULL || 0 != Type)			// 目前只支持座机Driver
	{
		return FTO_RET_PARAM_ERR;
	}	

	if (filesize(FileName) > (MAX_MON_LEN))
	{
		return FTO_RET_FILE_TOOBIG;
	}

	//读取该应用
    if(GetCallAppInfo(&CurAppInfo))
	{
		return FTO_RET_APPINFO_ERR;
	}
		
    szAttr[0]=CurAppInfo.AppNum;
    szAttr[1]=0x04;
    
    if((iFid=s_open(FileName, O_RDWR, szAttr)) < 0)
	{
		return FTO_RET_FILE_NO_EXIST;
	}

	if((lFileLen = filesize(FileName)) <= 16)
	{
		close(iFid);
		return FTO_RET_APP_TYPE_ERR;
	}

	seek(iFid,0,SEEK_SET);

	if (read(iFid,PtrDriver,lFileLen) != lFileLen)
	{
		close(iFid);
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_READ_FILE_ERR;
	}

	memset(&DriverInfo,0,sizeof(DriverInfo));
	if((GetFileBinInfo(PtrDriver,lFileLen,&DriverInfo)) || memcmp(DriverInfo.Descript,"Driver",6))
	{
		close(iFid);
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_APP_TYPE_ERR;
	}
	
	close(iFid);

	if (s_iVerifySignature(SIGN_FILE_MON,PtrDriver,lFileLen, NULL))
	{
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_SIG_ERR;
	}

//	s_removeId(iFid);
	ret = WriteBaseDriver(PtrDriver, lFileLen,0);/*写入driver*/
	if(!ret)
	{
		s_removeId(iFid);
	}

	LoadFontLib();
	RestoreCurFont();
	if(ret)
	{
		if(FTO_RET_PARAM_ERR == ret) return FTO_RET_PARAM_ERR;
		else if(-1 == ret) return FTO_RET_NO_BASE;
		else if(-2 == ret) return FTO_RET_OFF_BASE;
		else if(-3 == ret) return FTO_RET_WRITE_FILE_ERR;
	}
	return 0;
}

extern R_RSA_PUBLIC_KEY MF_PubKey;
int GetMonSig(uchar *Sig, int *SigLen)
{
    int ret, len;
    uchar buf[512], outdata[512];
   	image_header_t hdr ;	

	ret = ReadMonitor((uint)MONITOR_BASE_ADDR, (uchar*)(&hdr), sizeof(image_header_t));
    if(ret) return -1;
	len = SWAP_LONG(hdr.ih_size)+sizeof(image_header_t);
	len += 32;
	ReadMonitor((uint)(MONITOR_BASE_ADDR+len+284-16), buf, 16);
	if(memcmp(buf, SIGN_FLAG, 16)) return -1;
	ret = ReadMonitor((uint)(MONITOR_BASE_ADDR+len), buf, MF_PubKey.uiBitLen/8);
    if (ret) return -1;
    ret= RSAPublicDecrypt(outdata,&len,buf,MF_PubKey.uiBitLen/8, &MF_PubKey);	
    if (ret) return -1;

    *SigLen = sizeof(tSignFileInfo);
    memcpy(Sig, outdata, sizeof(tSignFileInfo));
    
    return 0;
}

int DelAppFile(uchar *AppName)
{
	int iAppId,DelNum;

	if (AppName == NULL)
	{
		return FTO_RET_PARAM_ERR;
	}

	if((iAppId = GetAppId(AppName)) < 0)//获取应用ID
	{
		return FTO_RET_NO_APP;
	}

    DelNum = DeleteApp(iAppId, 1);

    if(DelNum < 0)
    {
		return FTO_RET_NO_APP;
    }
    else if(DelNum == 0)
    {
		return FTO_RET_NO_APP;
    }

	return FTO_RET_OK;
}

void DelFontFile()
{
	s_remove(FONT_FILE_NAME,(uchar *)"\xff\x02");	
	LoadFontLib();//重新加载字库	
}

int  s_iDeleteAppParaFile(int ucAppNo)
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
				ex_remove(astFileInfo[iLoop].name,aucAttr);
			}
		}
	}
	return 0;
}

int MpConvertToSo(const char *filename, const char *appname)
{
	long lFileLen;
	int fd, newfd, iAppId;
	uchar szAttr[2];
    uchar fname[17], tmpname[17];
	// 因为堆栈空间不够,暂时使用字库的空间作为存储,转换结束后重新加载字库
	uchar *PtrParam=(uchar *)FILE_transfer_buff ;
	APPINFO CurAppInfo;
    FILE_INFO finfo[256];
    int num,i,iType;
    SO_HEAD_INFO *head;

	SaveCurFont();
	if (filename == NULL) 
		return FTO_RET_PARAMETER_NULL;

	// 读取该应用
    if (GetCallAppInfo(&CurAppInfo)) {
		return FTO_RET_APPINFO_ERR;
	}

	szAttr[0] = CurAppInfo.AppNum;
	szAttr[1] = FILE_TYPE_USER_FILE;
	fd = s_open(filename, O_RDWR, szAttr);
	if (fd < 0)
		return FTO_RET_FILE_NO_EXIST;
	
    seek(fd, 0, SEEK_END);
	lFileLen = tell(fd);
	
    seek(fd, 0, SEEK_SET);
	if (read(fd, PtrParam, lFileLen) != lFileLen) {
		close(fd);
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_READ_FILE_ERR;
	}   
    
	memset(fname, 0x00, sizeof(fname));	
	if (!memcmp(PtrParam, FIRMWARE_SO_NAME, strlen(FIRMWARE_SO_NAME))) //paxbase
	{
        strcpy(fname, PtrParam);
        szAttr[1] = FILE_TYPE_SYSTEMSO;
        iType = SIGN_FILE_MON;
	}
	else 
	{
		memset(tmpname, 0x00, sizeof(tmpname));
        head = (SO_HEAD_INFO*)(PtrParam+SOINFO_OFFSET);
        memcpy(tmpname, head->so_name, sizeof(tmpname)-1);
		UpperCase(tmpname, fname, 16);				   
	    if(strstr(head->so_name, MPATCH_EXTNAME) && head->lib_type==LIB_TYPE_SYSTEM)//mpatch file
		{
			 //限制非本机配置的wifi模块的mpatch的转换
		    if((is_wifi_mpatch(head->so_name)==1 && is_wifi_module()!=1) ||
				 (is_wifi_mpatch(head->so_name)==2 && is_wifi_module()!=3) ||
								   strlen(head->so_name)>16)
			{
			    LoadFontLib();
			    RestoreCurFont();//还原字体
			    close(fd);
	            return FTO_RET_PARAM_ERR;
			}
			iType = SIGN_FILE_MON;  
			szAttr[1] = FILE_TYPE_SYSTEMSO; 
		}
		else //so
		{
	        if(head->lib_type == LIB_TYPE_SYSTEM) 
	        {
	            szAttr[1] = FILE_TYPE_SYSTEMSO;
	            iType = SIGN_FILE_MON;
	        }
	        else 
	        {
	            szAttr[1] = FILE_TYPE_USERSO;
	            iType = SIGN_FILE_APP;
	        }
	    } 
    }
  
    if (appname != NULL) 
    {
        iAppId = GetAppId(appname);
        if (iAppId < 0)	
        {
            close(fd);
            return FTO_RET_NO_APP;
        }
        szAttr[0] = iAppId & 0xff;
    }
    else szAttr[0] = FILE_ATTR_PUBSO;
    
	if (s_iVerifySignature(iType, PtrParam, lFileLen, NULL) != 0) {
		LoadFontLib();
		RestoreCurFont();
		close(fd);
		return FTO_RET_SIG_ERR;
	}

	if(strstr(head->so_name, MPATCH_EXTNAME) && head->lib_type==LIB_TYPE_SYSTEM)//mpatch file	
	{	
	    if(GetMpatchSysPara(fname)!=0)
		{
			if(SetMpatchSysPara(fname, MPATCH_MIN_PARA)!=0)  
			    SetMpatchSysPara(fname, MPATCH_MIN_PARA);    //如果写para错误就重新写一次
		}
	}
	
	s_removeId(fd);
	close(fd);

    if (strstr(fname, FIRMWARE_SO_NAME)) {
        num = GetFileInfo(finfo);
        for (i=0; i<num; i++) {
            if (strstr(finfo[i].name, FIRMWARE_SO_NAME))
				s_remove(finfo[i].name,szAttr);
        }
    }
    else {
		s_remove(fname, szAttr);
    }
    
	newfd = s_open(fname, O_CREATE|O_RDWR, szAttr);
	if (newfd < 0) {
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_WRITE_FILE_ERR;
	}

	seek(newfd, 0, SEEK_SET);
	if (write(newfd, PtrParam, lFileLen) != lFileLen) {
		LoadFontLib();
		RestoreCurFont();
		return FTO_RET_WRITE_FILE_ERR;
	}

	close(newfd);
	LoadFontLib();
	RestoreCurFont();
	return FTO_RET_OK;
}
