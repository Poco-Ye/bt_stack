/*****************************************************/
/* filecore.c                                         */
/* For file system base on flash chip                */
/* For all PAX POS terminals                         */
/* Created by ZengYun at 12/11/2003                  */
/*****************************************************/
/******************************************************
2006年07月20日:
1、修改s_LogErase()函数，把最后2字节擦除状态改为8字节，并采用:擦标志为0-擦写-写标志为8字节的方法保证可检验擦除完整性
2、在初始化文件系统中按照新的擦写log方法判断擦写完整性
3、因文件系统不兼容，所以改fat1和fat2的8字节标志，改为更容易操作的带版本标志串


*******************************************************/
#include <string.h>
#include "filedef.h"
#include "base.h"

#include "..\download\usbhostdl.h"
#include "..\puk\puk.h"
#include "..\download\localdownload.h"
#include "..\font\font.h"
#include "..\flash\nand.h"

//#define     YCY_DEBUG_FS
extern int snprintf(char *str, size_t size, const char *format, ...); 
//#define  MAKE_CREATFILESYS

#define     ENCRYPT             1
#define     DECRYPT             0

#ifdef  EN_SECURY_FILE
#define     SECURE_KEY          0x0800E000      //  0x0050E000
#define     SECURE_DATA         0x0800E008      //  0x0050E008
#endif

#ifndef		NULL
#define     NULL                0
#endif

#define     FILE_EXIST          1
#define     FILE_NOEXIST        2
#define     MEM_OVERFLOW        3
#define     TOO_MANY_FILES      4
#define     INVALID_HANDLE      5
#define     INVALID_MODE        6
#define     FILE_NOT_OPENED     8
#define     FILE_OPENED         9
#define     END_OVERFLOW        10
#define     TOP_OVERFLOW        11
#define     NO_PERMISSION       12

#define     USED_BLOCK          0xff00
#define     NULL_BLOCK          0xffff

#define     CMD_OK              0x0000
#define     CMD_SET             0x55AA

#define     CREATE_FILE         0xff11
#define     WRITE_FILE          0xff22
#define     REMOVE_FILE         0xff44
#define     TRUNCATE_FILE       0xff88
#define     EXCHANGE_DATA       0xee11
#define     FWRITE_FAILED       0xDD22


extern uchar            bIsChnFont;
ushort g_LogSector[BLOCK_SIZE/2]; /*备份日子扇区*/


typedef struct
{
    char            file_name[16];
    unsigned char   attr;
    unsigned char   type;
    unsigned short  StartBlock;
    long            size;
}FILE_ITEM;


typedef struct{
    unsigned short OpenFlag;
    unsigned short fd;
    long           FilePtr;
    int            len;
    unsigned short UpBlockPtr;  /*当前块的前一块，可能是NULL_BLOCK*/
    unsigned short CurBlockPtr;
    unsigned short FilePtrOff;    /*FilePtr 在PAGE中的offset*/
    unsigned short CmdSet;
}FILE_STATUS;


extern uint              errno;

extern unsigned long    DATA_SECTORS;

volatile static unsigned long    FAT_SIZE;   //FAT扇区实际使用的长度
volatile static unsigned long    ALL_PAGES;

volatile  unsigned short   k_FreePageNum,k_UsedPageNum;
volatile static unsigned short   k_DataRecover;
volatile static unsigned long    k_ExchangeAddr;
volatile static unsigned short   k_FreePagePtr;
static unsigned short   k_FAT[0x20000];
static FILE_ITEM        k_Flist[MAX_FILES];
static FILE_STATUS      k_Fstatus[MAX_FILES];
#ifdef  EN_SECURY_FILE
static unsigned char    SecureFile[MAX_FILES];
#endif

volatile ushort   *k_LogPtr ;
volatile static unsigned short   vCMD_OK, vCMD_SET;

#define LOG_READ(addr)  g_LogSector[((uint)(addr) -LOG_ADDR)/2]

extern void des(unsigned char *input, unsigned char *output,unsigned char *deskey, int mode);
static uchar* s_ReadBlock(uint Addr,  int len);

//#define _DEBUG_F

#ifdef _DEBUG_F

void vMessageError(char *err)
{
    Lcdprintf(err);
    getkey();
    getkey();
}


void s_CheckFileSys(void)
{
	int i,j,k,sBlkTop,sBlkBot;
	unsigned short *BlkPtr,UsedBlk,FreeBlk,FileBlk,tmpPtr,upPtr;
	unsigned char *charPtr;

	BlkPtr=k_FAT;
	UsedBlk=0;
	j=(k_ExchangeAddr-DATA_ADDR)/DATA_SECTOR_SIZE;
	sBlkTop = j*BLOCK_PAGES;
	sBlkBot = sBlkTop+BLOCK_PAGES;
	for(i=0;i<DATA_SECTORS;i++)
	{
		if(i==j)
		{
			BlkPtr+=BLOCK_PAGES;
			continue;
		}
		for(k=0;k<BLOCK_PAGES;k++)
		{
			if(*BlkPtr++ == USED_BLOCK) UsedBlk++;
		}
	}
	if(UsedBlk!=k_UsedPageNum)
	{
		vMessageError("UsedBlk!=k_UsedPageNum");
		return;
	}
	tmpPtr=k_FreePagePtr;
	FreeBlk=0;
	while(tmpPtr!=NULL_BLOCK)
	{
		FreeBlk++;
		if(tmpPtr>=sBlkTop && tmpPtr<sBlkBot)
		{
			vMessageError("tmpPtr>=sBlkTop");
			return;
		}
		tmpPtr=k_FAT[tmpPtr];
	}
    
	if(k_FreePageNum!=FreeBlk) vMessageError("err30");
	FileBlk=0;
    for(i=0;i<MAX_FILES;i++){
        if(i==MAX_FILES-1)
            i=MAX_FILES-1;
        if(!k_Flist[i].file_name[0]){
            if(k_Fstatus[i].OpenFlag) vMessageError("err1");
            continue;
        }
        if(k_Fstatus[i].OpenFlag){
            if(k_Flist[i].size==0){
                if(k_Fstatus[i].CurBlockPtr!=NULL_BLOCK) vMessageError("err2");
                if(k_Fstatus[i].FilePtr!=0) vMessageError("err3");
                if(k_Fstatus[i].FilePtrOff!=PAGE_SIZE)
                    vMessageError("err4");
                if(k_Fstatus[i].UpBlockPtr!=NULL_BLOCK) vMessageError("err5");
                if(k_Flist[i].StartBlock!=NULL_BLOCK) vMessageError("err6");
            }
            else{
                if(k_Fstatus[i].FilePtrOff==PAGE_SIZE){
                    if(k_Fstatus[i].FilePtr%PAGE_SIZE) vMessageError("err7");
                }
                else if(k_Fstatus[i].FilePtrOff!=k_Fstatus[i].FilePtr%PAGE_SIZE) vMessageError("err7");
                tmpPtr=k_Flist[i].StartBlock;
                upPtr=NULL_BLOCK;
                if(k_Fstatus[i].FilePtr%PAGE_SIZE) k=k_Fstatus[i].FilePtr/PAGE_SIZE+1;
                else k=k_Fstatus[i].FilePtr/PAGE_SIZE;
                for(j=0;j<k-1;j++){
                    if(tmpPtr==NULL_BLOCK) vMessageError("err10");
                    upPtr=tmpPtr;
                    tmpPtr=k_FAT[tmpPtr];
                }
                if(k_Fstatus[i].FilePtrOff==0 && k_Fstatus[i].FilePtr){
                    upPtr=tmpPtr;
                    tmpPtr=k_FAT[tmpPtr];
                }
                if(tmpPtr!=k_Fstatus[i].CurBlockPtr){
                    vMessageError("err12");
                }
                if(upPtr!=k_Fstatus[i].UpBlockPtr) vMessageError("err13");
            }
        }
        tmpPtr=k_Flist[i].StartBlock;
        upPtr=NULL_BLOCK;
        if(k_Flist[i].size%PAGE_SIZE) k=k_Flist[i].size/PAGE_SIZE+1;
        else k=k_Flist[i].size/PAGE_SIZE;
        for(j=0;j<k;j++){
            if(tmpPtr==NULL_BLOCK) vMessageError("err17");
            upPtr=tmpPtr;
            tmpPtr=k_FAT[tmpPtr];
        }
        FileBlk+=k;
        if(tmpPtr!=NULL_BLOCK)  vMessageError("err16");
        if(k_Flist[i].size==0) continue;
		#if 0
        j=k_Flist[i].size%PAGE_SIZE;
        if(j){
            //charPtr=(unsigned char *)(DATA_ADDR+upPtr*PAGE_SIZE+j);
		charPtr=s_ReadBlock((DATA_ADDR+upPtr*PAGE_SIZE+j), (PAGE_SIZE-j));
		for(k=0;k<PAGE_SIZE-j;k++){
			if(*charPtr++ != 0xff) vMessageError("err21");
		}
		
		
        }
		#endif
    }
    if(FileBlk+UsedBlk+FreeBlk!=DATA_SECTORS*BLOCK_PAGES-BLOCK_PAGES) vMessageError("err20");
}
#endif

static void s_LogErase(void)
{
	unsigned char   flag[16];
	if(NAND_ID_HYNIX != GetNandType())OtherForceWrite();  /*确保LOG可以写入LOG区*/
	memset(flag, 0, sizeof(flag));
	s_WriteFlash(LOG_ADDR+LOG_SIZE-8,flag,8);
	s_WriteFlash(LOG_ADDR,&vCMD_OK,2);
	s_EraseSector(LOG_ADDR);
	s_WriteFlash(LOG_ADDR+LOG_SIZE-8,LOG_VERSION_FLAG,8);
	k_LogPtr = (unsigned short *)(LOG_ADDR+16);
}


static void s_SaveFAT(void)
{
	int i;
	unsigned short buff[4];
	unsigned char sector_buf[BLOCK_SIZE];
	if(NAND_ID_HYNIX != GetNandType())OtherForceWrite();  /*确保LOG可以写入LOG区*/
    
	s_WriteFlash((uchar*)FAT1_LOG_ADDR, &vCMD_SET, 2);
	memcpy(sector_buf,FAT_VERSION_FLAG,8);
	memcpy(sector_buf+8,(uchar*)&k_ExchangeAddr,4);
	memcpy(sector_buf+12,(uchar*)&k_FreePageNum,2);
	memcpy(sector_buf+14,(uchar*)&k_UsedPageNum,2);
	memcpy(sector_buf+16,(uchar*)&k_FreePagePtr,2);
	memcpy(sector_buf+18,(uchar*)&k_FAT,ALL_PAGES*2);
	memcpy(sector_buf+ALL_PAGES*2+18,k_Flist,MAX_FILES*sizeof(FILE_ITEM)+2);
	
	for(i=0;i<3;i++)
	{
		s_EraseSector(FAT1_ADDR);
		if(s_WriteFlash(FAT1_ADDR,sector_buf,FAT_SIZE)) continue;
		s_WriteFlash(FAT1_LOG_ADDR, &vCMD_OK, 2);
		break;
	}
	if(i==3) s_WriteFlashFailed();
	for(i=0;i<3;i++)
	{
		s_EraseSector(FAT2_ADDR);
		if(s_WriteFlash(FAT2_ADDR,sector_buf, FAT_SIZE)) continue;
		s_WriteFlash(FAT2_LOG_ADDR, &vCMD_OK, 2);
		break;
	}
	if(i==3) s_WriteFlashFailed();
	s_LogErase();
}

static void s_LogCmdOK(void)
{
    if(s_WriteFlash((uchar*)k_LogPtr,(uchar*)&vCMD_OK,2)) s_SaveFAT();
    else if((uint)++k_LogPtr>LOG_END_ADDR) s_SaveFAT();
}

static void k_LogTruncateFile(unsigned short fd, int len)
{
	unsigned short buff[5];

	buff[0]=TRUNCATE_FILE;
	buff[1]=fd;
	buff[2]=len & 0xFFFF;
	buff[3]=(len>>16) & 0xFFFF;
	buff[4]=CMD_SET;
	if(s_WriteFlash(k_LogPtr,buff,sizeof(buff)))
	{
		s_SaveFAT();
		if(s_WriteFlash(k_LogPtr,buff,sizeof(buff))) s_WriteFlashFailed();
	}
	k_LogPtr+=4;
}

static void k_LogRemoveFile(unsigned short fd)
{
	unsigned short buff[3];

	buff[0]=REMOVE_FILE;
	buff[1]=fd;
	buff[2]=CMD_OK;
	if(s_WriteFlash(k_LogPtr,buff,sizeof(buff))) s_SaveFAT();
	else
	{
		k_LogPtr+=3;
		if((uint)k_LogPtr>LOG_END_ADDR) s_SaveFAT();
	}
}

static void k_LogCeateFile(unsigned short fd,unsigned char *attr, char *filename)
{
	unsigned short buff[12];

	buff[0]=CREATE_FILE;
	buff[1]=fd;
	memcpy(buff+2,attr,2);
	memcpy(buff+3,filename,16);
	buff[11]=CMD_OK;
	if(s_WriteFlash(k_LogPtr,buff,sizeof(buff))) s_SaveFAT();
	else
	{
		k_LogPtr+=12;
		if((uint)k_LogPtr>LOG_END_ADDR) s_SaveFAT();
	}
}

static void s_LogExchangeData(unsigned long sAddr)
{
	unsigned short buff[4];

	buff[0]=EXCHANGE_DATA;
	buff[1]=sAddr & 0xFFFF;
	buff[2]=(sAddr>>16) &0xFFFF;
	buff[3]=CMD_OK;
	if(s_WriteFlash(k_LogPtr,buff,sizeof(buff))) s_SaveFAT();
	else
	{
		k_LogPtr+=4;
		if((uint)k_LogPtr>LOG_END_ADDR) s_SaveFAT();
	}
}

static unsigned short s_GetFreeBlock(void)
{
	unsigned int ptr;
	unsigned short tmpBlock;
	unsigned short pagebuff[1024];
	int i;

	tmpBlock=k_FreePagePtr;
	k_FreePagePtr=k_FAT[tmpBlock];
	k_FreePageNum--;

	return tmpBlock;
}
static void s_CheckUsedBlock(int BlockNum)
{
	int i,j;
	unsigned short upPtr,tmpPtr;
	unsigned short *datPtr;
	unsigned short buf[1024];

	tmpPtr=k_FreePagePtr;
	for(j=0;j<BlockNum;j++){
		if(tmpPtr==NULL_BLOCK) break;
		datPtr=(ushort *)(DATA_ADDR+tmpPtr*PAGE_SIZE);
		s_ReadFlash(datPtr, buf, PAGE_SIZE);
		for(i=0;i<PAGE_SIZE/2;i++) if(buf[i] != 0xffff) break;

		if(i<PAGE_SIZE/2){
			k_UsedPageNum++;
			k_FreePageNum--;
			if(tmpPtr==k_FreePagePtr){
				k_FreePagePtr=k_FAT[tmpPtr];
				k_FAT[tmpPtr]=USED_BLOCK;
				tmpPtr=k_FreePagePtr;
			}
			else{
				k_FAT[upPtr]=k_FAT[tmpPtr];
				k_FAT[tmpPtr]=USED_BLOCK;
				tmpPtr=k_FAT[upPtr];
			}
		}
		else{
			upPtr=tmpPtr;
			tmpPtr=k_FAT[tmpPtr];
		}
	}
}


static uchar* s_ReadBlock(uint Addr,  int len)
{
	static uchar page_buf[PAGE_SIZE];
	memset(page_buf,0,sizeof(page_buf));
	if(len>PAGE_SIZE) len=PAGE_SIZE;
	if(len==0) return page_buf;
	s_ReadFlash(Addr,page_buf,len);
	return page_buf;              
}

static void s_WriteBlock(unsigned short BlockPtr, int offset, unsigned char *dat, int len)
{
	unsigned int Addr;
	Addr=DATA_ADDR+BlockPtr*PAGE_SIZE+offset;
	s_WriteFlash(Addr,dat,len);              

}

int  s_SearchFile(char *filename,unsigned char *fattr)
{
    int     i,len;
    uchar   attr;
    APPINFO CurAppInfo;
    char    FName[17];

    if(GetCallAppInfo(&CurAppInfo))
    {
        return -1;
    }

    if(filename==NULL || !filename[0])
    {
        errno = FILE_NOEXIST;
        return -1;
    }

    memset(FName, 0x00, sizeof(FName));
    len = UpperCase(filename, FName, 16);

    if(fattr == NULL)
    {
        attr = CurAppInfo.AppNum;
    }
    else
    {
        attr = fattr[0];
    }
//    len = strlen(filename);
    for(i=0; i<MAX_FILES; i++)
    {
        if(attr == k_Flist[i].attr)
        {
            if(len < 16)
            {
                if(!strcmp(FName, k_Flist[i].file_name))
                    return i;
            }
            else if(!memcmp(FName, k_Flist[i].file_name, 16))
            {
                return i;
            }
        }
    }
    if(fattr == NULL)
    {
        errno = FILE_NOEXIST;
    }
    return -1;
}

static void s_CreateFile(unsigned short fd, char *filename, unsigned char *attr)
{
	FILE_ITEM   *fItemPtr;
	char        FName[17];

	memset(FName, 0x00, sizeof(FName));
	UpperCase(filename, FName, 16);
	fItemPtr = &k_Flist[fd];
	memcpy(fItemPtr->file_name, FName, 16);
	fItemPtr->attr = attr[0];
	fItemPtr->type = attr[1];
	fItemPtr->StartBlock = NULL_BLOCK;
	fItemPtr->size = 0;

	if(!k_DataRecover)
	{
		k_LogCeateFile(fd, attr, FName);
	}
}
static void s_LoadFAT(void)
{
	unsigned short sector_buf[BLOCK_SIZE/2];
	s_ReadFlash(FAT2_ADDR,sector_buf, sizeof(sector_buf));
	memcpy((uchar*)&k_ExchangeAddr,&sector_buf[8/2],4);	
	k_FreePageNum = sector_buf[12/2];
	k_UsedPageNum = sector_buf[14/2];
	k_FreePagePtr = sector_buf[16/2];
	memcpy(k_FAT,&sector_buf[18/2],ALL_PAGES*2);
	memcpy(k_Flist,&sector_buf[ALL_PAGES+18/2],MAX_FILES*sizeof(FILE_ITEM));
	memset(k_Fstatus, 0, MAX_FILES*sizeof(FILE_STATUS));
}

//修改一个文件的名称和类型
void s_RenameFile(unsigned short fd, char *filename, unsigned char *attr)
{
	char        FName[17];

	memset(FName, 0x00, sizeof(FName));
	UpperCase(filename, FName, 16);
	memcpy(k_Flist[fd].file_name, FName,16);
	k_Flist[fd].attr = attr[0];
	k_Flist[fd].type = attr[1];
	s_SaveFAT();
}

int  s_open(char *filename, unsigned char mode, unsigned char *attr)
{
	int fd;
	FILE_STATUS *fStatusPtr;

	if(filename==NULL || !filename[0])
	{
		errno=FILE_NOEXIST;
		return -1;
	}

	fd=s_SearchFile(filename,attr);

	if(fd>=0)
	{
		if(mode == O_CREATE || mode==(O_CREATE | O_ENCRYPT))
		{
			errno=FILE_EXIST;
			return -1;
		}
	}
	else
	{
		if(mode == O_RDWR || mode==(O_RDWR | O_ENCRYPT))
		{
			errno=FILE_NOEXIST;
			return -1;
		}

		for(fd=0;fd<MAX_FILES;fd++)
		{
			if(!k_Flist[fd].file_name[0]) break;
		}

		if(fd==MAX_FILES)
		{
			errno=TOO_MANY_FILES;
			return -1;
		}

		s_CreateFile((unsigned short)fd, filename, attr);
	}

	fStatusPtr=&k_Fstatus[fd];
	fStatusPtr->fd=fd;
	fStatusPtr->CmdSet=CMD_SET;
	fStatusPtr->OpenFlag=WRITE_FILE;
	fStatusPtr->CurBlockPtr=k_Flist[fd].StartBlock;
	fStatusPtr->UpBlockPtr=NULL_BLOCK;
	fStatusPtr->FilePtr=0;
	if(fStatusPtr->CurBlockPtr==NULL_BLOCK) fStatusPtr->FilePtrOff=PAGE_SIZE;
	else fStatusPtr->FilePtrOff=0;
	return fd;
}

void  s_removeId(int fd)
{
	unsigned short tmpPtr,BlockPtr;

    if(!strcmp(k_Flist[fd].file_name, ENV_FILE)) Invalid_Env_Cache();
    
	k_Flist[fd].file_name[0]=0;
	k_Flist[fd].attr=0xfe;
	k_Fstatus[fd].OpenFlag=0;
	BlockPtr=k_Flist[fd].StartBlock;
	while(BlockPtr!=NULL_BLOCK){
		tmpPtr=BlockPtr;
		BlockPtr=k_FAT[BlockPtr];
		k_FAT[tmpPtr]=USED_BLOCK;
		k_UsedPageNum++;
	}
	if(!k_DataRecover) k_LogRemoveFile((unsigned short)fd);
}

int  open(char *filename, unsigned char mode)
{
	int fd;
	unsigned char attr[2];
	APPINFO CurAppInfo;

	if(GetCallAppInfo(&CurAppInfo))    return -1;

	if(mode<1 || mode >6 || mode==O_ENCRYPT)
	{
		errno=INVALID_MODE;
		return -1;
	}
	attr[0]=CurAppInfo.AppNum;
	attr[1]=0x04;

	fd=s_open(filename, mode, attr);
#ifdef _DEBUG_F
	//  s_CheckFileSys();
#endif
	return fd;
}

static int s_CheckFid(int fd)
{
	if(fd<0 || fd>=MAX_FILES)
	{
		errno=INVALID_HANDLE;
		return -1;
	}
	if(!k_Fstatus[fd].OpenFlag){
		errno=FILE_NOT_OPENED;
		return -2;
	}
	return 0;
}

int  close(int fd)
{
	if(s_CheckFid(fd)) return -1;
	k_Fstatus[fd].OpenFlag=0;
	return 0;
}
/**应用关闭文件*/

int  closefile(int fd)
{

	if(s_CheckFid(fd)) return -1;
    
    switch(k_Flist[fd].attr){
        case FILE_ATTR_PUBSO:
        case FILE_ATTR_PUBFILE:
            break;
        default:
            if(k_Flist[fd].attr>APP_MAX_NUM) return -1;/*隐藏的工厂测试程序应用号为24，允许文件关闭操作*/
            break;
    }

	k_Fstatus[fd].OpenFlag=0;

	return 0;
}

long filesize(char *filename)
{
	int fd;

	fd=s_SearchFile(filename,NULL);
	if(fd<0) return -1;
	return k_Flist[fd].size;
}

long GetFileLen(int iFid)
{
	if(iFid<0) return -1;
	return k_Flist[iFid].size;
}

int s_remove(char *filename, unsigned char *attr)
{
	int fd;

	fd=s_SearchFile(filename,attr);
	if(fd<0) return -1;
	s_removeId(fd);
	return 0;
}

long s_filesize(char *filename, unsigned char *attr)
{
	int fd;

	fd=s_SearchFile(filename,attr);
	if(fd<0) return -1;
	return k_Flist[fd].size;
}

int  remove(char *filename)
{
	int fd;

	fd=s_SearchFile(filename,NULL);
	if(fd<0) return -1;
	if(k_Fstatus[fd].OpenFlag){
		errno=FILE_OPENED;
		return -1;
	}
	s_removeId(fd);
	#ifdef _DEBUG_F
	//  s_CheckFileSys();
	#endif
	return 0;
}

long freesize(void)
{
	return (k_FreePageNum+k_UsedPageNum-1-4)*PAGE_SIZE;
}

int  fexist(char *filename)
{
	return s_SearchFile(filename,NULL);
}

int  GetFileInfo(FILE_INFO* finfo)
{
	int i,j;
	for(i=0,j=0; i<MAX_FILES; i++)
	{
		if(k_Flist[i].file_name[0] == 0) continue;
		j++;
		finfo->fid=i;
		finfo->attr=k_Flist[i].attr;
		finfo->type=k_Flist[i].type;
		finfo->name[16]=0;
		memcpy(finfo->name,k_Flist[i].file_name,16);
		finfo->length=k_Flist[i].size;
		finfo++;
	}
	return j;
}

int  ex_open(char *filename, unsigned char mode,unsigned char* attr)
{
	APPINFO CurAppInfo;
	char pszAppName[32];
	uchar aucAttr[3];
	if(GetCallAppInfo(&CurAppInfo))    return -1;

	if(mode<1 || mode >6 || mode==O_ENCRYPT)
	{
		errno=INVALID_MODE;
		return -1;
	}

	if(attr[1]==4 && attr[0]!=0xFF)
	{
		// if((attr[0]<APP_MAX_NUM ) && (attr[0]==0 || CurAppInfo.AppNum==0 || attr[0] == CurAppInfo.AppNum))

		//pengrs 用户文件应用之间可以互相访问2008-09-16==> 

		snprintf(pszAppName,sizeof(pszAppName),"%s%d",APP_FILE_NAME_PREFIX,attr[0]);

		aucAttr[0]=0xff;

		if(attr[0]==0)aucAttr[1]=0;//应用管理器
		else aucAttr[1]=1;//子应用

		if(attr[0]<APP_MAX_NUM&&CurAppInfo.AppNum<APP_MAX_NUM//检查应用号是否合法
		&&s_SearchFile(pszAppName,aucAttr)>=0)//检查应用是否存在
		return s_open(filename,mode,attr);
	}

	errno = NO_PERMISSION;
	return -1;

}

int  s_read(int fd, unsigned char *dat, int len)
{
	int bakLen;
	unsigned short tmpLen,tmpBlock,upBlock;
	FILE_ITEM *fItemPtr;
	FILE_STATUS *fStatusPtr;
	fItemPtr=&k_Flist[fd];
	fStatusPtr=&k_Fstatus[fd];

	if(fStatusPtr->FilePtr+len > fItemPtr->size) len=fItemPtr->size - fStatusPtr->FilePtr;
	if(len <= 0) return 0;
	tmpLen=PAGE_SIZE-fStatusPtr->FilePtrOff;
	if(len <= tmpLen)
	{
		//memcpy(dat,(unsigned char *)(DATA_ADDR+fStatusPtr->CurBlockPtr*PAGE_SIZE+fStatusPtr->FilePtrOff),len);
		s_ReadFlash((DATA_ADDR+fStatusPtr->CurBlockPtr*PAGE_SIZE+fStatusPtr->FilePtrOff), dat,len);
		fStatusPtr->FilePtrOff+=len;
		fStatusPtr->FilePtr+=len;
		return len;
	}

	bakLen=len;

	if(tmpLen)
	{
		//memcpy(dat,(unsigned char *)(DATA_ADDR+fStatusPtr->CurBlockPtr*PAGE_SIZE+fStatusPtr->FilePtrOff),tmpLen);
		s_ReadFlash((DATA_ADDR+fStatusPtr->CurBlockPtr*PAGE_SIZE+fStatusPtr->FilePtrOff), dat,tmpLen);

		len-=tmpLen;
		dat+=tmpLen;
	}

	tmpBlock=fStatusPtr->CurBlockPtr;

	while(1)
	{
		upBlock=tmpBlock;
		tmpBlock=k_FAT[tmpBlock];
		if(len<=PAGE_SIZE)
		{
			//memcpy(dat,(unsigned char *)(DATA_ADDR+tmpBlock*PAGE_SIZE),len);
			s_ReadFlash(DATA_ADDR+tmpBlock*PAGE_SIZE, dat,len);
			break;
		}
		//memcpy(dat,(unsigned char *)(DATA_ADDR+tmpBlock*PAGE_SIZE),PAGE_SIZE);
		s_ReadFlash(DATA_ADDR+tmpBlock*PAGE_SIZE, dat,PAGE_SIZE);
		len-=PAGE_SIZE;
		dat+=PAGE_SIZE;
	}
	fStatusPtr->FilePtr += bakLen;
	fStatusPtr->FilePtrOff = len;
	fStatusPtr->CurBlockPtr = tmpBlock;
	fStatusPtr->UpBlockPtr = upBlock;
#ifdef _DEBUG_F
	//  s_CheckFileSys();
#endif
	return bakLen;
}

void  s_seek(int fd, long offset, unsigned char origin)
{
    FILE_ITEM *fItemPtr;
    FILE_STATUS *fStatusPtr;
    unsigned short tmpBlock,upBlock;
    fItemPtr=&k_Flist[fd];
    fStatusPtr=&k_Fstatus[fd];
    if(origin==SEEK_SET){
        tmpBlock=fItemPtr->StartBlock;
        fStatusPtr->FilePtr=offset;
    }
    else if(origin==SEEK_END){
        tmpBlock=fItemPtr->StartBlock;
        offset+=fItemPtr->size;
        fStatusPtr->FilePtr=offset;
    }
    else{
        if(offset<=0){
            if(fStatusPtr->FilePtrOff+offset>=0){
                fStatusPtr->FilePtr += offset;
                fStatusPtr->FilePtrOff += (short)offset;
                return;
            }
            tmpBlock=fItemPtr->StartBlock;
            offset+=fStatusPtr->FilePtr;
            fStatusPtr->FilePtr=offset;
        }
        else{
            if(fStatusPtr->FilePtrOff+offset<=PAGE_SIZE){
                fStatusPtr->FilePtrOff += (short)offset;
                fStatusPtr->FilePtr += offset;
                return;
            }
            fStatusPtr->FilePtr+=offset;
            offset += fStatusPtr->FilePtrOff;
            tmpBlock=fStatusPtr->CurBlockPtr;
        }
    }
    upBlock=NULL_BLOCK;
    while(offset>PAGE_SIZE)
	{
        upBlock=tmpBlock;
        tmpBlock=k_FAT[tmpBlock];
        offset-=PAGE_SIZE;
    }
    fStatusPtr->UpBlockPtr = upBlock;
    fStatusPtr->CurBlockPtr = tmpBlock;
    if(tmpBlock!=NULL_BLOCK) fStatusPtr->FilePtrOff = (short)offset;
}

int  seek(int fd, long offset, unsigned char origin)
{
	long tmpLen;

	if(s_CheckFid(fd)) return -1;
	if(origin==SEEK_SET) tmpLen=0;
	else if(origin==SEEK_CUR) tmpLen=k_Fstatus[fd].FilePtr;
	else if(origin==SEEK_END) tmpLen=k_Flist[fd].size;
	else
	{
		errno=INVALID_MODE;
		return -1;
	}

	if(tmpLen+offset<0)
	{
		errno=TOP_OVERFLOW;
		return -1;
	}

	if(tmpLen+offset>k_Flist[fd].size)
	{
		errno=END_OVERFLOW;
		return -1;
	}
	s_seek(fd, offset, origin);

	return 0;
}

int read(int fd, unsigned char *dat, int len)
{
	int i,j,retLen,off,tmpLen,iret;
	unsigned char *SecureKey,*SecureData;
	unsigned char buff[8],tmpBuff[8];
	FILE_ITEM *fItemPtr;
	FILE_STATUS *fStatusPtr;
	iret = s_CheckFid(fd);
	if(iret == -1)
	{
		errno=INVALID_HANDLE;
		return -1;
	}
	else if (iret == -2)
	{
		errno=FILE_NOT_OPENED;
		return -1;
	}

	return(s_read(fd, dat, len));
}

/*sAddr指向一个全USED_BLOCK的sector,该扇区可以直接擦除*/
static void s_ExchangePage_Hynix(unsigned long sAddr)
{
	int i,j,k,sSecNo,dBlkTop,sBlkTop,sBlkBot;
	unsigned short *src,*dec,*BlkPtr;
	FILE_ITEM *fItemPtr;
	FILE_STATUS *fStatusPtr;
	uchar block_free_table[64];
	ushort FreePtr,FreeNum;	

	memset(block_free_table,0,sizeof(block_free_table));
	
	sSecNo = (sAddr-DATA_ADDR)/DATA_SECTOR_SIZE;
	dBlkTop = ((k_ExchangeAddr-DATA_ADDR)/DATA_SECTOR_SIZE)*BLOCK_PAGES;
	sBlkTop = sSecNo*BLOCK_PAGES;
	sBlkBot = sBlkTop+BLOCK_PAGES;
	memcpy(k_FAT+dBlkTop, k_FAT+sBlkTop, BLOCK_PAGES*2); /*将sAddr指向的sector的K_FAT信息替换为Exchange块的K_FAT*/

	FreePtr =k_FreePagePtr;
	FreeNum=k_FreePageNum;
	
	while(FreeNum)
	{
		if((FreePtr>=sBlkTop) &&  (FreePtr<sBlkBot) )	block_free_table[FreePtr-sBlkTop] =1; /*Free Page*/	
		FreePtr = k_FAT[FreePtr];
		FreeNum--;
	}

	k = dBlkTop-sBlkTop;
	BlkPtr = k_FAT;
	for(i=0;i<DATA_SECTORS;i++)
	{
		if(i==sSecNo) /*找待替换块时直接跳过该块*/ 
		{
			BlkPtr+=BLOCK_PAGES;
			continue;
		}
		for(j=0;j<BLOCK_PAGES;j++) /*在一块内找交换块，找到交换块，便于待交换块替换*/
		{
			if(*BlkPtr>=sBlkTop && *BlkPtr<sBlkBot) *BlkPtr+=k;  /*将FAT表中的blk部分与sAddr指向的所有页替换*/
			BlkPtr++;
		}
	}/*完成块FAT表中的块替换*/
	
	for(i=0;i<MAX_FILES;i++){
		if(k_Flist[i].file_name[0]){
			fItemPtr=&k_Flist[i];
			if(fItemPtr->StartBlock >= sBlkTop && fItemPtr->StartBlock < sBlkBot) fItemPtr->StartBlock += k;
			if(k_Fstatus[i].OpenFlag){
				fStatusPtr=&k_Fstatus[i];
				if(fStatusPtr->UpBlockPtr>=sBlkTop && fStatusPtr->UpBlockPtr < sBlkBot) fStatusPtr->UpBlockPtr += k;
				if(fStatusPtr->CurBlockPtr>=sBlkTop && fStatusPtr->CurBlockPtr < sBlkBot) fStatusPtr->CurBlockPtr += k;
			}
		}
	}/*完成文件中用到的待交换扇区的信息*/
	
	if(k_FreePagePtr>=sBlkTop && k_FreePagePtr < sBlkBot) k_FreePagePtr += k;

	for(j=0;j<3;j++){
		if(!k_DataRecover) s_EraseSector(k_ExchangeAddr);
		BlkPtr=k_FAT+dBlkTop;/*指向sAddr部分原来的K_FAT*/
		src=(unsigned short *)sAddr;
		dec=(unsigned short *)k_ExchangeAddr;
		for(i=0;i<BLOCK_PAGES;i++){
			if(*BlkPtr == USED_BLOCK){  /*将sAddr sector中的USED_BLOCK放入Free Block 链表中*/
				k_UsedPageNum--;
				k_FreePageNum++;
				*BlkPtr = k_FreePagePtr;
				k_FreePagePtr = dBlkTop+i;
			}
			else 
			{
				if(!block_free_table[i])
				{
					if(!k_DataRecover) 
						if (s_WriteFlash((uchar*)dec,s_ReadBlock(
							(uint)src,PAGE_SIZE),PAGE_SIZE)) break;
				}
				
			}
			BlkPtr++;
			dec+=PAGE_SIZE/2;
			src+=PAGE_SIZE/2;
		}

	
		if(i==BLOCK_PAGES) break;
	}

	if(j==3) s_WriteFlashFailed();
	k_ExchangeAddr=sAddr;
	if(!k_DataRecover){
		s_LogExchangeData(sAddr);
		#ifdef _DEBUG_F
		//      s_CheckFileSys();
		#endif
	}
}

static void s_ExchangePage_New(unsigned long sAddr)
{
	int i,j,k,sSecNo,dSecNo,dBlkTop,sBlkTop,sBlkBot;
    int Ex_src_dist;  /*待交换块和交换块起始页之间的差值*/
	unsigned short *src,*dec,*BlkPtr;
	FILE_ITEM *fItemPtr;
	FILE_STATUS *fStatusPtr;
	//uchar block_free_table[64]; /*由于NAND不能完全拷贝，只有有效数据才进行平移，否则不拷贝*/
	ushort FreePtr,FreePtr_back,FreeNum;
    int reIndex[BLOCK_SIZE/PAGE_SIZE],blkFreeNum,blkFreeNum_rec,blkUseNum,blkDataNum;

	sSecNo = (sAddr-DATA_ADDR)/DATA_SECTOR_SIZE;
	dSecNo =  (k_ExchangeAddr-DATA_ADDR)/DATA_SECTOR_SIZE;
	dBlkTop = dSecNo*BLOCK_PAGES;
	sBlkTop = sSecNo*BLOCK_PAGES;
	sBlkBot = sBlkTop+BLOCK_PAGES;
	Ex_src_dist = dBlkTop-sBlkTop;  /*Exchange Block 与 待交换块之间的PAGE数量差异*/

	/*1、拷贝FAT表*/
	memcpy(k_FAT+dBlkTop, k_FAT+sBlkTop, BLOCK_PAGES*2); /*将sAddr指向的sector的K_FAT信息替换为Exchange块的K_FAT*/
 
    BlkPtr=k_FAT+sBlkTop;
    for(i=0,blkUseNum=0;i<BLOCK_PAGES;i++,BlkPtr++)
    {
        if(*BlkPtr == USED_BLOCK)blkUseNum++;/*计算待交换块内的UESED块*/ 
    }

    /*计算Free Page 与 Used Page的数量*/
	FreePtr =k_FreePagePtr;
	FreeNum=k_FreePageNum;
	blkFreeNum =0;

    for(i=0;i<FreeNum;i++)   
	{
		/*在sSec中到找到FreePage中的最开始项，若不能找到则将FreePtr指向FreePage链表的最后一项*/
		if((FreePtr>=sBlkTop)&&(FreePtr<sBlkBot)) break;
        FreePtr_back = FreePtr;
		FreePtr = k_FAT[FreePtr];
	}
	blkFreeNum_rec = i;
	if(i>= FreeNum) {
        FreePtr=FreePtr_back; 
        blkFreeNum=0;
    } /*case 3,4,5*/
	else blkFreeNum = BLOCK_PAGES -(FreePtr % BLOCK_PAGES); /*case 1+6,case 2+6*/

    k_UsedPageNum -= blkUseNum;
    k_FreePageNum += blkUseNum;
    /* Free ptr 处理结束，再进行FreePage处理前 FreePtr不能修改*/

    /*2、开始处理data page*/
	memset(reIndex,0,sizeof(reIndex));
    BlkPtr = k_FAT + sBlkTop;
	dec = k_FAT + dBlkTop;
    for(i=0,j=0;i<(BLOCK_PAGES- blkFreeNum);i++)
    {
		if(BlkPtr[i] == USED_BLOCK) continue;
		reIndex[i] = Ex_src_dist + j -i; /*计算data page 交换后与原位置的差值*/
		j++;

    }    
    blkDataNum = BLOCK_PAGES - blkUseNum - blkFreeNum; /*计算待交换块内的数据块*/ 
    BlkPtr = k_FAT;
	
	for(i=0;i<DATA_SECTORS;i++)
	{
		if(i==sSecNo) /*遇到sSec时直接跳过该块*/ 
		{
			BlkPtr+=BLOCK_PAGES;
			continue;
		}
		/*2.1、修改dSec块 FAT表*/
		if(i==dSecNo){
			for(j=0,k=0;j<BLOCK_PAGES;j++) /*在任意Block内找交换page，找到交换page，便于待交换page替换*/
			{
				/*2.1.1 移动并修改dSec块 FAT表*/
				if(*BlkPtr>=sBlkTop && *BlkPtr<(sBlkBot - blkFreeNum) ) {
					*(BlkPtr+k-j) = *BlkPtr+reIndex[*BlkPtr % BLOCK_PAGES];
					k++;
					BlkPtr++;
					continue;
				}

				/*2.1.2 移动dSec块FAT表中的指向sSec外或NULL_BLOCK的数据PAGE*/
				if((*BlkPtr!=USED_BLOCK) && (j < (BLOCK_PAGES - blkFreeNum))) {
					*(BlkPtr+k-j) = *BlkPtr;
					k++;
				}

				BlkPtr++;
			}		
			continue;
		}

		/*2.2、修改其他块指向sSec块的FAT表项*/
		for(j=0;j<BLOCK_PAGES;j++) 
		{
			if(*BlkPtr>=sBlkTop && *BlkPtr<sBlkBot ) 
                *BlkPtr+=reIndex[*BlkPtr % BLOCK_PAGES]; 
			BlkPtr++;
		}
	}
	/*3、将USED Page释放为Free Page 并将USED Page合入Free块的链表中*/
	/**********************************************
	*	case 3，4，5 整个sSec块中没有Free Page
	*	case 1，6(1)：K_FreePagePtr指向sSec 块
	*	case 2，6(2): K_FreePagePtr不指向sSec 块
	************************************************/
	if(blkFreeNum_rec>= FreeNum){ /*case 3,4,5*/
		k_FAT[FreePtr]=dBlkTop +(BLOCK_PAGES - blkUseNum); /*将Free Page链表最后一项指向dSec块中的第一个Free Page*/
		k_FAT[dBlkTop + BLOCK_PAGES-1] = NULL_BLOCK;  /*将dSec块中的最后一个Page指向NULL BLOCK*/
	}
	else{/*case 1+6,case 2+6*/
		if(k_FreePagePtr>=sBlkTop && k_FreePagePtr < sBlkBot) /*case 1+6*/     
			k_FreePagePtr = dBlkTop +(BLOCK_PAGES- blkFreeNum- blkUseNum);/*将全局k_FreePagePtr指向dSec块中的第一个Free Page*/  
		else /*case 2+6*/
			k_FAT[FreePtr_back] = dBlkTop +(BLOCK_PAGES- blkFreeNum- blkUseNum);/*将第一个指向sSec块的k_FAT表项，指向dSec块中的第一个Free Page*/
		k_FAT[dBlkTop + BLOCK_PAGES-1] = k_FAT[sBlkBot-1]; /*dSec块的最后一个Free Page的FAT表项直接使用sSec块中的最后一个 Free Page FAT表项 */
	}
	/*4、将sSec中的USED 及 Free Page进行转换和移动到dSec中,最后一个Page不做修改*/
    BlkPtr=k_FAT+dBlkTop;
    for(i = BLOCK_PAGES- blkFreeNum- blkUseNum;i<(BLOCK_PAGES-1);i++)
    {
        BlkPtr[i] = dBlkTop + i +1;
    }

    /*5、修改文件链表中，指向sSec 中的数据Page的文件相关指针*/    
	for(i=0;i<MAX_FILES;i++){/*完成文件中用到的待交换扇区的信息*/
		if(k_Flist[i].file_name[0]){
			fItemPtr=&k_Flist[i];
			if(fItemPtr->StartBlock >= sBlkTop && fItemPtr->StartBlock < sBlkBot) 
                fItemPtr->StartBlock += reIndex[fItemPtr->StartBlock % BLOCK_PAGES];
			if(k_Fstatus[i].OpenFlag){
				fStatusPtr=&k_Fstatus[i];
				if(fStatusPtr->UpBlockPtr>=sBlkTop && fStatusPtr->UpBlockPtr < sBlkBot) 
                    fStatusPtr->UpBlockPtr += reIndex[ fStatusPtr->UpBlockPtr % BLOCK_PAGES];
				if(fStatusPtr->CurBlockPtr>=sBlkTop && fStatusPtr->CurBlockPtr < sBlkBot) 
                    fStatusPtr->CurBlockPtr += reIndex[fStatusPtr->CurBlockPtr % BLOCK_PAGES];
			}
		}
	}
	
	/*6、将数据page写到dSec块中*/
	for(j=0;j<3;j++){
		if(!k_DataRecover) s_EraseSector(k_ExchangeAddr);
        BlkPtr = k_FAT + sBlkTop;
		src=(unsigned short *)sAddr;
		dec=(unsigned short *)k_ExchangeAddr;
		for(i=0,k=0;i<(blkDataNum + blkUseNum);i++){
            if(*BlkPtr != USED_BLOCK)
            {   
    		    if(!k_DataRecover) 
    			if (s_WriteFlash((uchar*)dec,s_ReadBlock(
    				(uint)src,PAGE_SIZE),PAGE_SIZE)) break;
                dec+=PAGE_SIZE/2;
				k++;
            }
            BlkPtr++;
			src+=PAGE_SIZE/2;
		}

		if(k==blkDataNum) break;
	}

	if(j==3) s_WriteFlashFailed();
	k_ExchangeAddr=sAddr;
	if(!k_DataRecover)	s_LogExchangeData(sAddr);

    FreePtr = k_FreePagePtr;
    for(i=0;i<k_FreePageNum;i++){
        if(k_FAT[FreePtr]== NULL_BLOCK)break;
        FreePtr = k_FAT[FreePtr];
    }
    if(i != (k_FreePageNum-1))s_ExchangeFailed();
        
    
}
static void s_ExchangePage(unsigned long sAddr)
{
	if(NAND_ID_HYNIX == GetNandType()) s_ExchangePage_Hynix(sAddr);
	else s_ExchangePage_New(sAddr);	
}

static void s_RecycleBlocks(void)
{
    int i,j,k,s,n,end,BlkNo;
    unsigned short *BlkPtr,UsedBlk;

    UsedBlk=0;
    j=(k_ExchangeAddr-DATA_ADDR)/DATA_SECTOR_SIZE;  /*j-->Exchange sector number*/
	
    for(n=0;n<2;n++){
        if(n){/*从第一个sector到ExchangeAddr指向的块开始找全USED_BLOCK的sector*/
            i=0;
            end=j;
            BlkPtr=k_FAT;
        }
        else{/*从Free 后一个sector开始找全USED_BLOCK的sector*/
            i=j+1;   /*i-->Exchange next sector number*/
            end=DATA_SECTORS; /*all sectors number*/
            BlkPtr=k_FAT+i*BLOCK_PAGES;  /*Exchange next sector 第一个page K_FAT中的位置*/
        }
		
        for(;i<end;i++){
            s=0;
            for(k=0;k<BLOCK_PAGES;k++){  /*在某个块内找USED PAGE,并记录个数*/
                if(*BlkPtr++ == USED_BLOCK) s++;
            }
            if(s>UsedBlk){ /*找到有USED_BLOCK最多的sector*/
                UsedBlk=s;
                BlkNo=i;
            }
            if(UsedBlk==BLOCK_PAGES) break; /*找到整个sector所有PAGE都为USED_BLOCK*/
        }
        if(UsedBlk==BLOCK_PAGES) break; /*找到整个sector所有PAGE都为USED_BLOCK*/
    }
    if(!UsedBlk) return; /*所有扇区都没有USED_BLOCK，即再无可用PAGE*/
    s_ExchangePage(DATA_ADDR+BlkNo*DATA_SECTOR_SIZE); /*用有最多USED_BLOCK的sector，替换Exchange sector*/
}

static int s_write(int fd, unsigned char *dat)
{
	int             len,tmpLen,needBlocks;
	FILE_ITEM       *fItemPtr;
	FILE_STATUS     *fStatusPtr;
	unsigned short  oldPtr,newPtr,upPtr,tmpPtr;
	unsigned char   *charPtr,*readBlockPtr;

	fItemPtr   = &k_Flist[fd];
	fStatusPtr = &k_Fstatus[fd];
	len        = fStatusPtr->len;
	tmpLen     = fStatusPtr->FilePtr%PAGE_SIZE;    //  当前指针在page位置，0等效于page size
	if(tmpLen)      //  不在块起始位置
	{
		tmpLen = len - (PAGE_SIZE-tmpLen);
		if(tmpLen <= 0) needBlocks = 1;     //  本块空间够写
		else if(tmpLen % PAGE_SIZE) needBlocks = tmpLen/PAGE_SIZE + 2;
		else needBlocks = tmpLen/PAGE_SIZE + 1;
	}
	else
	{
		if(len % PAGE_SIZE) needBlocks = len/PAGE_SIZE + 2;
		else needBlocks = len/PAGE_SIZE + 1;
	}
	needBlocks +=4 ;  /*NAND Need*/
//	k_FreePageNum,k_UsedPageNum,PAGE_SIZE);	
	if(needBlocks > k_FreePageNum)
	{
		if(needBlocks > k_FreePageNum+k_UsedPageNum)
		{
			errno = MEM_OVERFLOW;
			return -1;
		}
		while(needBlocks > k_FreePageNum) s_RecycleBlocks();
	}

	if(!k_DataRecover)
	{
		if(s_WriteFlash(k_LogPtr,fStatusPtr,sizeof(FILE_STATUS)))
		{
			s_SaveFAT();
			if(s_WriteFlash(k_LogPtr,fStatusPtr,sizeof(FILE_STATUS))) s_WriteFlashFailed();
		}
		k_LogPtr    += 9;
	}

	/****** 文件指针在文件的末尾，追加写****/	
	if(fStatusPtr->FilePtr == fItemPtr->size)   
	{
		if(fItemPtr->size == 0)     //  该文件还没有分配block
		{
			newPtr        = s_GetFreeBlock();
			k_FAT[newPtr] = NULL_BLOCK;
			fItemPtr->StartBlock    = newPtr;
			fStatusPtr->FilePtrOff  = 0;
			fStatusPtr->UpBlockPtr  = NULL_BLOCK;
			fStatusPtr->CurBlockPtr = newPtr;
		}
		fStatusPtr->FilePtr += len;
		fItemPtr->size      += len;

		tmpLen = PAGE_SIZE - fStatusPtr->FilePtrOff;
		if(fStatusPtr->FilePtrOff)
		{
			newPtr       = s_GetFreeBlock();
			k_FAT[newPtr] =k_FAT[fStatusPtr->CurBlockPtr];
			if(fStatusPtr->UpBlockPtr == NULL_BLOCK) fItemPtr->StartBlock = newPtr;
			else 	k_FAT[fStatusPtr->UpBlockPtr] = newPtr;        //  让上一块指向新块，用来替换旧的
			k_FAT[fStatusPtr->CurBlockPtr] = USED_BLOCK;
			k_UsedPageNum++;
		}
		if(!k_DataRecover)
		{
			readBlockPtr=s_ReadBlock(DATA_ADDR+fStatusPtr->CurBlockPtr*PAGE_SIZE,fStatusPtr->FilePtrOff);
			if(len<=tmpLen) tmpLen =len;
			memcpy(readBlockPtr+fStatusPtr->FilePtrOff,dat,tmpLen);
			s_WriteBlock(newPtr,0, readBlockPtr, fStatusPtr->FilePtrOff+tmpLen);
		}
		if(fStatusPtr->FilePtrOff)	fStatusPtr->CurBlockPtr=newPtr;
		fStatusPtr->FilePtrOff += tmpLen;

		dat += tmpLen;
		len -= tmpLen;
		if(len<=0)
		{
			if(!k_DataRecover)s_LogCmdOK();
			return 0;
		}


		upPtr = fStatusPtr->CurBlockPtr;
		while(1)        //  分配新块写
		{
			newPtr       = s_GetFreeBlock();
			k_FAT[upPtr] = newPtr;
			if(len <= PAGE_SIZE)
			{
				k_FAT[newPtr] = NULL_BLOCK;
				if(!k_DataRecover)
				{
					s_WriteBlock(newPtr, 0, dat, len);
					s_LogCmdOK();
				}
				fStatusPtr->CurBlockPtr = newPtr;
				fStatusPtr->UpBlockPtr  = upPtr;
				fStatusPtr->FilePtrOff  = len;
				return 0;
			}
			if(!k_DataRecover) s_WriteBlock(newPtr, 0, dat, PAGE_SIZE);
			dat  += PAGE_SIZE;
			len  -= PAGE_SIZE;
			upPtr = newPtr;
		}
	    }


	if(fStatusPtr->FilePtrOff == PAGE_SIZE)        //  若定位到该位置则移到下一个block
	{
		fStatusPtr->UpBlockPtr  = fStatusPtr->CurBlockPtr;
		fStatusPtr->CurBlockPtr = k_FAT[fStatusPtr->CurBlockPtr];
		fStatusPtr->FilePtrOff  = 0;
	}
	
	/*****************改写+追加*******************/
	if(fStatusPtr->FilePtr+len >= fItemPtr->size)   
	{
		fItemPtr->size      = fStatusPtr->FilePtr+len;
		fStatusPtr->FilePtr = fItemPtr->size;

		newPtr = s_GetFreeBlock();
		if(fStatusPtr->UpBlockPtr == NULL_BLOCK) fItemPtr->StartBlock = newPtr;
		else k_FAT[fStatusPtr->UpBlockPtr] = newPtr;        //  让上一块指向新块，用来替换旧的

		tmpLen = PAGE_SIZE - fStatusPtr->FilePtrOff;
		if(tmpLen >= len)   //  本块够写，而且由上一个改写+追加条件判断本块一定是最后一块
		{
			k_FAT[fStatusPtr->CurBlockPtr] = USED_BLOCK;
			k_UsedPageNum++;
			k_FAT[newPtr] = NULL_BLOCK;
			if(!k_DataRecover)
			{
				readBlockPtr =s_ReadBlock(DATA_ADDR+fStatusPtr->CurBlockPtr*PAGE_SIZE,fStatusPtr->FilePtrOff);
				memcpy(readBlockPtr+fStatusPtr->FilePtrOff,dat,len);
				s_WriteBlock(newPtr,0,readBlockPtr,fStatusPtr->FilePtrOff+len);
				s_LogCmdOK();
			}
			fStatusPtr->FilePtrOff += len;
			fStatusPtr->CurBlockPtr = newPtr;
			return 0;
		}
		if(!k_DataRecover)
		{
			readBlockPtr =s_ReadBlock(DATA_ADDR+fStatusPtr->CurBlockPtr*PAGE_SIZE,fStatusPtr->FilePtrOff);
			memcpy(readBlockPtr+fStatusPtr->FilePtrOff,dat,tmpLen);
			s_WriteBlock(newPtr,0,readBlockPtr,fStatusPtr->FilePtrOff+tmpLen);
		}
		dat += tmpLen;
		len -= tmpLen;

		while(1)
		{
			upPtr  = newPtr;
			newPtr = s_GetFreeBlock();
			k_FAT[upPtr] = newPtr;
			if(len <= PAGE_SIZE) break;
			if(!k_DataRecover) s_WriteBlock(newPtr, 0, dat, PAGE_SIZE);
			dat += PAGE_SIZE;
			len -= PAGE_SIZE;
		}
		k_FAT[newPtr] = NULL_BLOCK;
		
		fStatusPtr->UpBlockPtr  = upPtr;
		fStatusPtr->FilePtrOff  = len;
		tmpPtr = fStatusPtr->CurBlockPtr;
		fStatusPtr->CurBlockPtr = newPtr;
		while(tmpPtr!=NULL_BLOCK)       //将被替换的旧块进行回收  
		{
			upPtr        = tmpPtr;
			tmpPtr       = k_FAT[tmpPtr];
			k_FAT[upPtr] = USED_BLOCK;
			k_UsedPageNum++;
		}
		if(!k_DataRecover)
		{
			s_WriteBlock(newPtr, 0, dat, len);
			s_LogCmdOK();
		}
		return 0;
	}
	
	/* ************* 仅仅是改写数据*********************/
	fStatusPtr->FilePtr += len;
	newPtr = s_GetFreeBlock();
	if(fStatusPtr->UpBlockPtr == NULL_BLOCK) fItemPtr->StartBlock = newPtr;
	else k_FAT[fStatusPtr->UpBlockPtr] = newPtr;

	tmpPtr = k_FAT[fStatusPtr->CurBlockPtr];
	k_FAT[fStatusPtr->CurBlockPtr] = USED_BLOCK;
	k_UsedPageNum++;
	tmpLen = PAGE_SIZE - fStatusPtr->FilePtrOff;
	if(tmpLen >= len)   //  本块够写
	{
		k_FAT[newPtr] = tmpPtr;
		if(!k_DataRecover)
		{
			charPtr = (unsigned char *)(DATA_ADDR + fStatusPtr->CurBlockPtr * PAGE_SIZE);
			readBlockPtr =s_ReadBlock((uint)charPtr,PAGE_SIZE);
			memcpy(readBlockPtr+fStatusPtr->FilePtrOff,dat,len);
			s_WriteBlock(newPtr,0 , readBlockPtr, fStatusPtr->FilePtrOff+tmpLen);	
			len    += fStatusPtr->FilePtrOff;		
			s_LogCmdOK();
		}
		fStatusPtr->CurBlockPtr = newPtr;
		fStatusPtr->FilePtrOff  = len;
		return 0;
	}
	if(!k_DataRecover)
	{
		readBlockPtr =s_ReadBlock(DATA_ADDR + fStatusPtr->CurBlockPtr * PAGE_SIZE,fStatusPtr->FilePtrOff);
		memcpy(readBlockPtr+fStatusPtr->FilePtrOff,dat,tmpLen);
		s_WriteBlock(newPtr, 0, readBlockPtr, fStatusPtr->FilePtrOff+tmpLen);
	}
	len -= tmpLen;
	dat += tmpLen;

	upPtr = newPtr;
	while(len >= PAGE_SIZE)
	{
		newPtr = s_GetFreeBlock();
		k_FAT[upPtr] = newPtr;
		if(!k_DataRecover) s_WriteBlock(newPtr, 0, dat, PAGE_SIZE);
		dat   += PAGE_SIZE;
		len   -= PAGE_SIZE;
		upPtr  = newPtr;
		oldPtr = tmpPtr;
		tmpPtr = k_FAT[tmpPtr];
		k_FAT[oldPtr] = USED_BLOCK;
		k_UsedPageNum++;
	}
	if(len)
	{
		newPtr = s_GetFreeBlock();
		k_FAT[upPtr] = newPtr;
		if(!k_DataRecover)
		{
			readBlockPtr =s_ReadBlock(DATA_ADDR+tmpPtr*PAGE_SIZE,PAGE_SIZE);
			memcpy(readBlockPtr,dat,len);
			s_WriteBlock(newPtr,0,readBlockPtr,PAGE_SIZE);
		}
		k_FAT[newPtr] = k_FAT[tmpPtr];
		k_FAT[tmpPtr] = USED_BLOCK;
		k_UsedPageNum++;
		fStatusPtr->CurBlockPtr = newPtr;
		fStatusPtr->UpBlockPtr  = upPtr;
		fStatusPtr->FilePtrOff  = len;
	}
	else
	{
		k_FAT[newPtr] = tmpPtr;
		fStatusPtr->CurBlockPtr = tmpPtr;
		fStatusPtr->UpBlockPtr  = newPtr;
		fStatusPtr->FilePtrOff  = 0;
	}
	if(!k_DataRecover)	s_LogCmdOK();
	return 0;
}

int  k_write(int fd, unsigned char *dat, int len)
{
	int ret;
	
    if(!strcmp(k_Flist[fd].file_name, ENV_FILE)) Invalid_Env_Cache();

	k_Fstatus[fd].len=len;
	ret=s_write(fd, dat);
	if(ret<0) return ret;
	return len;
}

int  write(int fd, unsigned char *dat, int len)
{
	int i,retLen,off,tmpLen,iret;
	unsigned char *SecureKey,*SecureData;
	unsigned char buff[1024],tmpBuff[8];
	FILE_ITEM *fItemPtr;
	FILE_STATUS *fStatusPtr;

	iret = s_CheckFid(fd);
	if(iret == -1)
	{
		errno=INVALID_HANDLE;
		return -1;
	}
	else if (iret == -2)
	{
		errno=FILE_NOT_OPENED;
		return -1;
	}
	if(len <= 0) return 0;
    return(k_write(fd, dat, len));
}


static void  s_truncate(int fd,long len)
{
	unsigned short headPtr,tmpPtr,newPtr;
	FILE_ITEM *fItemPtr;
	FILE_STATUS *fStatusPtr;
	uchar *readBlockPtr;

	fItemPtr=&k_Flist[fd];
	fStatusPtr=&k_Fstatus[fd];

    if(!strcmp(k_Flist[fd].file_name, ENV_FILE)) Invalid_Env_Cache();

	if(!k_DataRecover)	k_LogTruncateFile((unsigned short)fd,len);

	if(len==0) 
	{
		headPtr=fItemPtr->StartBlock;
		fStatusPtr->FilePtrOff=PAGE_SIZE;
		fItemPtr->StartBlock=NULL_BLOCK;
		fStatusPtr->CurBlockPtr=NULL_BLOCK;
		fStatusPtr->UpBlockPtr=NULL_BLOCK;
	}
	else
	{
		s_seek(fd,len,SEEK_SET);
		headPtr=k_FAT[fStatusPtr->CurBlockPtr];
		k_FAT[fStatusPtr->CurBlockPtr]=NULL_BLOCK;
	}

	/*从尾往前找*/
	while(headPtr!=NULL_BLOCK)
	{ 
		tmpPtr=headPtr;
		headPtr=k_FAT[headPtr];
		k_FAT[tmpPtr]=USED_BLOCK;
		k_UsedPageNum++;
	}
	fItemPtr->size=len;
	fStatusPtr->FilePtr=len;
	if(!k_DataRecover)     s_LogCmdOK();

#ifdef _DEBUG_F
//      s_CheckFileSys();
#endif
}

int  truncate(int fd,long len)
{
    int tmpLen,iret;
    unsigned char buff[8],tmpBuff[8];
    unsigned char *SecureKey,*SecureData;

	iret = s_CheckFid(fd);
	if(iret == -1)
	{
		errno=INVALID_HANDLE;
		return -1;
	}
	else if (iret == -2)
	{
		errno=FILE_NOT_OPENED;
		return -1;
	}

	if(len<0)
	{
		errno = TOP_OVERFLOW;
		return -1;
	}

	if(len>k_Flist[fd].size)
	{
		errno = END_OVERFLOW;
		return -1;
	}
	if(len==k_Flist[fd].size) return 0;

	s_truncate(fd,len);
	return 0;
}

void CreateFileSys(void)
{
	int i,ret;
	unsigned short *tmpPtr,buff[4];
	unsigned char sector_buf[0x20000]; // 128KB
	unsigned char log_buf[10];

	ret=s_CreateBlockMapTable();
	ScrRestore(0);
	errno = 0;
	vCMD_OK = CMD_OK;
	vCMD_SET = CMD_SET;
	k_DataRecover=0;
	s_fInitStart();
	k_FreePagePtr = BLOCK_PAGES; /*指向数据区的第二个扇区的起始地址*/
	k_ExchangeAddr = DATA_ADDR;
	k_FreePageNum = (DATA_SECTORS-1)*BLOCK_PAGES;//92*256
	k_UsedPageNum = 0;
	tmpPtr = &k_FAT[BLOCK_PAGES];
	for(i=BLOCK_PAGES+1;i<ALL_PAGES;i++) *tmpPtr++ = (unsigned short)i; /*建立时从Free Page开始依次指向下一个PAGE*/
	*tmpPtr = NULL_BLOCK;

	memset((unsigned char *)(&k_Flist[0]), 0, MAX_FILES*sizeof(FILE_ITEM));
	for(i=0;i<MAX_FILES;i++) k_Flist[i].attr=0xfe;
    
	if(NAND_ID_HYNIX != GetNandType())OtherForceWrite();  /*确保LOG可以写入LOG区*/
    memcpy(log_buf,&LOG_READ(INIT_LOG_ADDR), 8);
	if(!memcmp(log_buf,"\xff\xff\xff\xff\xff\xff\xff\xff",8))//0X20000
	{
		s_WriteFlash((uchar *)INIT_LOG_ADDR,(uchar*)"INITFILE",8); //8个字节
	}

	s_EraseSector(FAT1_ADDR);
	ShowEraseProcess(FAT1_ADDR);//显示进度
	s_EraseSector(FAT2_ADDR);
	ShowEraseProcess(FAT2_ADDR);

	for(i=0;i<DATA_SECTORS;i++)
	{
		s_EraseSector(DATA_ADDR+i*DATA_SECTOR_SIZE);
		ShowEraseProcess(DATA_ADDR+i*DATA_SECTOR_SIZE);
	}

	memcpy(sector_buf+8,(uchar*)&k_ExchangeAddr,4);
	memcpy(sector_buf+12,(uchar*)&k_FreePageNum,2);
	memcpy(sector_buf+14,(uchar*)&k_UsedPageNum,2);
	memcpy(sector_buf+16,(uchar*)&k_FreePagePtr,2);
	memcpy(sector_buf+18,(uchar*)&k_FAT,ALL_PAGES*2);
	memcpy(sector_buf+ALL_PAGES*2+18,k_Flist,MAX_FILES*sizeof(FILE_ITEM)+2);
	memcpy(sector_buf,FAT_VERSION_FLAG,8);
	
	for(i=0;i<3;i++)
	{
		if(i)//非第一次写时，需要重新擦出扇区
		{
			s_EraseSector(FAT1_ADDR);
			s_EraseSector(FAT2_ADDR);
		}
		if(s_WriteFlash(FAT2_ADDR,sector_buf,FAT_SIZE)) continue;
		if(s_WriteFlash(FAT1_ADDR,sector_buf,FAT_SIZE)) continue;

		break;
	}
	if(i==3) s_WriteFlashFailed();
	s_LogErase();
	ScrRestore(1);

}

void InitFileSys(void)
{
	int             i;
	unsigned long   len;
	unsigned char   tmpBuf[10];
	unsigned short   tmpBuf1[5];
	unsigned short   tmpBuf2[5];
	unsigned short   pagebuff[1024];
	unsigned short  *tmpPtr,*oldPtr,*headPtr,fd,step,logSize,saveFAT;
	ushort sector_buf[BLOCK_SIZE/2];

	ALL_PAGES = DATA_SECTORS * BLOCK_PAGES;//一个扇区256B
	FAT_SIZE   = MAX_FILES*sizeof(FILE_ITEM) + ALL_PAGES*2 + 32;//29984
	
	/*将g_LogSector与FLASH 数据同步*/
	s_ReadFlash(LOG_ADDR,(uchar*)g_LogSector,sizeof(g_LogSector));
	/*恢复LOG数据,将两份一样的数据保存到g_logSector*/
	for(i=0;i<LOG_SIZE/2;i++)
	{
		if(g_LogSector[i] ==g_LogSector[LOG_SIZE/2+i]) continue;
		if(g_LogSector[i] ==g_LogSector[LOG_SIZE+i])  continue;
		
		if(g_LogSector[LOG_SIZE/2+i] ==g_LogSector[LOG_SIZE+i])
		{
			g_LogSector[i] =g_LogSector[LOG_SIZE+i];
			continue;
		}
		g_LogSector[i] |=g_LogSector[LOG_SIZE/2+i];
	}

	

#ifdef _DEBUG_F
	if(sizeof(FILE_STATUS) != 20)
	{
		Lcdprintf("sizeof(FILE_STATUS)!=20");
		getkey();
	}
	if(sizeof(FILE_ITEM) != 24)
	{
		Lcdprintf("sizeof(FILE_ITEM)!=24");
		getkey();
	}
#endif

	errno    = 0;
	vCMD_OK  = CMD_OK;
	vCMD_SET = CMD_SET;
#ifdef MAKE_CREATFILESYS
	CreateFileSys();
#endif
	s_ReadFlash(FAT1_ADDR, tmpBuf1,8);
	//    if(memcmp(FAT_VERSION_FLAG, (unsigned char *)FAT1_ADDR, 8))
	if(memcmp(FAT_VERSION_FLAG, tmpBuf1, 8))  /*fail*/
	{
		s_ReadFlash(FAT2_ADDR, tmpBuf1,8);
		//if(memcmp(FAT_VERSION_FLAG, (unsigned char *)FAT2_ADDR, 8))
		if(memcmp(FAT_VERSION_FLAG, tmpBuf1, 8)) /*fail*/
		{
			CreateFileSys();
		}
		else if(LOG_READ(FAT1_LOG_ADDR) == 0xffff) /*fat2 ok,FAT1_LOG == 0xFFFF*/
		{
			CreateFileSys();
		}
	}

	if(!memcmp("INITFILE", &LOG_READ(INIT_LOG_ADDR), 8))
	{
		CreateFileSys();
	}
	k_DataRecover = 0;
	
	if(LOG_READ(LOG_ADDR)!=0xffff || memcmp(&LOG_READ(LOG_ADDR+LOG_SIZE-8),LOG_VERSION_FLAG,8)!=0)
	{
		s_LogErase();
		s_LoadFAT();
		return;
	}
	if(LOG_READ(FAT1_LOG_ADDR) == CMD_OK)
	{
        if(NAND_ID_HYNIX != GetNandType())OtherForceWrite();  /*确保LOG可以写入LOG区*/
		if(LOG_READ(FAT2_LOG_ADDR) != CMD_OK)
		{
			for(i=0; i<3; i++)
			{
				s_EraseSector(FAT2_ADDR);
                		//if(s_WriteFlash((unsigned short *)FAT2_ADDR, (unsigned short *)FAT1_ADDR, FAT_SIZE/2)) continue;
				s_ReadFlash(FAT1_ADDR, sector_buf, FAT_SIZE);
				if(s_WriteFlash(FAT2_ADDR, sector_buf, FAT_SIZE)) continue;              
				s_WriteFlash((uchar *)FAT2_LOG_ADDR, &vCMD_OK, 2);
				break;
			}
			if(i == 3) s_WriteFlashFailed();
		}
		s_LogErase();
		s_LoadFAT();
		return;
	}

	s_LoadFAT();
	k_LogPtr = (unsigned short *)(LOG_ADDR+16);
	if(LOG_READ(k_LogPtr)  == 0xffff) return;

	tmpPtr = (ushort*)LOG_END_ADDR;
	while(tmpPtr > k_LogPtr)
	{
		if(LOG_READ(tmpPtr)  != 0xffff) break;
		tmpPtr--;
	}

	oldPtr  = (ushort*)k_LogPtr;
	headPtr = (ushort*)k_LogPtr;
	logSize = tmpPtr - k_LogPtr + 2;
	step    = logSize / 20;
	k_DataRecover = 1;
	s_fStartPercent(0);
	
	if(logSize > LOG_SIZE/10) saveFAT = 1;
	else saveFAT = 0;
	TimerSet(0,100);
	
	while(k_DataRecover)
	{
		switch(LOG_READ(k_LogPtr) )
		{
			case CREATE_FILE:
			if(LOG_READ(k_LogPtr+11) == 0xffff)
			{
				saveFAT       = 1;
				k_DataRecover = 0;
				break;
			}
			s_CreateFile(LOG_READ(k_LogPtr+1), (char*)&LOG_READ(k_LogPtr+3), (uchar*)&LOG_READ(k_LogPtr+2));
			k_LogPtr += 12;
			break;
			
			case WRITE_FILE:
				fd = LOG_READ(k_LogPtr+1);
				if(fd < MAX_FILES) memcpy(&k_Fstatus[fd], &LOG_READ(k_LogPtr), 18);
				k_LogPtr += 9;
				if((LOG_READ(k_LogPtr) == 0xffff)||(LOG_READ(k_LogPtr) != CMD_OK))
				{
					saveFAT       = 1;
					k_DataRecover = 0;
					s_CheckUsedBlock(k_Fstatus[fd].len/PAGE_SIZE + 4);
					break;
				}

				/*日志没有问题时，恢复s_write操作的全局变量*/
				s_write(fd, tmpBuf);
				k_LogPtr++;
			break;
			
			case REMOVE_FILE:
				if(LOG_READ(k_LogPtr+2) == 0xffff)
				{
					saveFAT       = 1;
					k_DataRecover = 0;
					break;
				}

				s_removeId(LOG_READ(k_LogPtr+1));
				k_LogPtr += 3;
			break;

			case TRUNCATE_FILE:
				if((LOG_READ(k_LogPtr+4) == 0xffff) || (LOG_READ(k_LogPtr+4) != CMD_OK) )
				{
					saveFAT       = 1;
					k_DataRecover = 0;
					break;
				} /*最后一次未操作成功，可以不进行处理*/
				len  =  LOG_READ(k_LogPtr+3);
				len  = (len<<16) + LOG_READ(k_LogPtr+2);


				s_truncate(LOG_READ(k_LogPtr+1), len); /*k_LogPtr+1 -->fd*/
				k_LogPtr += 5;
			break;
			
			case EXCHANGE_DATA:
				if(LOG_READ(k_LogPtr+3) == 0xffff)
				{
					saveFAT       = 1;
					k_DataRecover = 0;
					break;
				}
				len =LOG_READ(k_LogPtr+2);
				len =(len<<16) +  LOG_READ(k_LogPtr+1);
				
				s_ExchangePage(len);  /*len == Saddr*/
				k_LogPtr += 4;
			break;

			default:
				k_DataRecover = 0;
				if(LOG_READ(k_LogPtr) != 0xffff)
				{
					saveFAT = 1;
					#ifdef _DEBUG_F
					Lcdprintf("Log error!");
					getkey();
					#endif
				}
			break;
		}
		
		if(k_LogPtr-oldPtr > step)
		{
			oldPtr = (ushort*)k_LogPtr;
			s_fStartPercent((ushort)((k_LogPtr-headPtr)*100L/logSize));
		}
	}

	s_fStartPercent(100);
	if(!TimerCheck(0)) saveFAT=1;
	if(saveFAT) s_SaveFAT();
	memset((unsigned char *)(&k_Fstatus[0]), 0, MAX_FILES*sizeof(FILE_STATUS));
	#ifdef _DEBUG_F
	s_CheckFileSys();
	#endif
}


/*
	功能:
	当文件系统可立即写入的空间小于等于NeedSize时,启动回收垃圾块
	避免文件系统频繁的进行垃圾回收,增加flash擦写次数.NeedSize越小越好

	参数:NeedSize:1~32K(uchar),其他值无效.
*/
void FsRecycle(int NeedSize)
{
	if(NeedSize<0)	return;
	if(NeedSize < 2*1024)	NeedSize = 2*1024;
	if(NeedSize > 32*1024)	NeedSize = 32*1024;

	if((uint)k_LogPtr>LOG_END_ADDR-4*1024)
	{
		s_SaveFAT();
	}
	if(k_FreePageNum*PAGE_SIZE>=NeedSize) return;
	if((k_FreePageNum+k_UsedPageNum)*PAGE_SIZE<NeedSize)
	{
		if(k_UsedPageNum) s_RecycleBlocks();
		return;
	}
	while(k_FreePageNum*PAGE_SIZE<NeedSize) s_RecycleBlocks();
}

//返回文件指针位置
long tell(int fd)
{
    if(s_CheckFid(fd)) return -1;
    return k_Fstatus[fd].FilePtr;
}

int ex_remove(char *filename, unsigned char *attr)
{
    int fd;

    fd=s_SearchFile(filename,attr);
    if(fd<0) return -1;
    if(k_Fstatus[fd].OpenFlag){
        errno=FILE_OPENED;
        return -1;
    }
    s_removeId(fd);
    return 0;
}


