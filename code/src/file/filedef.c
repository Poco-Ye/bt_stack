/*****************************************************/
/* filedef.c                                         */
/* For file system base on flash chip                */
/* For all PAX POS terminals                         */
/* Created by ZengYun at 20/11/2003                  */
/*****************************************************/
#include "base.h"
#include "filedef.h"
#include "..\flash\nand.h"


unsigned long DATA_SECTORS;		//total sectors  100*8

unsigned long BLOCK_PAGES;	//每个数据扇区包含的数据块数   64
unsigned long LOG_SIZE;			//日志内容的大小
unsigned long LOG_SECTOR_SIZE;  	//日志表所在扇区的大小

unsigned long FAT_SECTOR_SIZE;	//FAT表所在扇区的大小
unsigned long DATA_SECTOR_SIZE; 	//数据扇区的大小

unsigned long	FAT1_ADDR;	  	//FAT1表的地址
unsigned long	FAT2_ADDR;	  	//FAT2表的地址
unsigned long	LOG_ADDR;     		//日志表的地址
unsigned long	DATA_ADDR;     	//数据扇区的起始地址

void s_fInitStart(void)
{
	ScrCls();
	ScrFontSet(0);
	Lcdprintf("Create File System...");
}


void ShowProcess(int iProcess)
{
	uchar wn,m,n,i,j;
	ScrDrawBox(6,1,8,20);
	wn = iProcess*6/5;
	m = 6*8 + 4;
	n = 8*8 - 4;

	if(wn < 8) wn = 8;
	if(wn > 118) wn = 118;
	for (i = m;i < n;i++)
	{
		ScrPlot(wn,i,1);
		ScrPlot(wn+1,i,1);
	}
}

void ShowEraseProcess(unsigned long Addr)
{
    int i;

    ScrPrint(5, 4, 0x00, "Build File System  ");
    if(Addr == FAT1_ADDR)
    {
        ScrPrint(5, 5, 0x00, "Erase (001%%)..OK  ");
		i = 1;
    }
    else if(Addr==FAT2_ADDR)
    {
        ScrPrint(5, 5, 0x00, "Erase (002%%)..OK  ");
		i = 2;
    }
    else
    {
        i=(Addr-DATA_ADDR)/DATA_SECTOR_SIZE;
		i=(i+1)*97/DATA_SECTORS;
        ScrPrint(5, 5, 0x00, "Erase (%03d%%)..OK  ", i+3);
    }

	ShowProcess(i+3);
}

void s_fStartPercent(unsigned short per)
{
	static int DispCnt;
	static const char DispSSS[10]="........";

	if(per==0) DispCnt=5;
	else
	{
		ScrGotoxy(5,7);
		if(per>100) per=100;

		if(per==100)
		{
			ScrFontSet(0);
			Lcdprintf("PLEASE WAIT...      ");
			ScrFontSet(1);
		}
		else
		{
			Lcdprintf("%d%% COMPLETED%s",per,DispSSS+DispCnt);
			if(--DispCnt ==0) DispCnt=5;//7
		}
    }
}


int s_WriteFlash(unsigned char *des, unsigned char *src, int len)
{
	int ret;
	if((uint)des >=(LOG_ADDR) && ((uint)des< (LOG_ADDR +LOG_SIZE)))
	{
		if(((uint)des+len)>(LOG_ADDR +LOG_SIZE)) return -101;
		/*LOG 扇区进行3次备份*/
		memcpy((uchar*)((uint)g_LogSector+(des-LOG_ADDR)),src,len); 
        if(NAND_ID_HYNIX == GetNandType()){
    		nand_write((uint)des,src,len);
    		nand_write((uint)des+LOG_SIZE,src,len);
    		nand_write((uint)des+LOG_SIZE*2,src,len);
            return 0;       
        }
        else{
            ret=OtherWriteLog((uint)des-LOG_ADDR, src, len);
            return ret;
        }
	}
	
	ret=nand_write((uint)des,src,len);
	return ret;
}


int s_ReadFlash(uint des, uchar *buff, int wlen)
{
	int ret;
    if(NAND_ID_HYNIX != GetNandType()){
        if((uint)des >=(LOG_ADDR) && ((uint)des< (LOG_ADDR +LOG_SIZE)))
        {
            ret = OtherReadLog(LOG_ADDR,(uchar*)g_LogSector,LOG_SIZE);
            memcpy((uchar*)g_LogSector + LOG_SIZE,(uchar*)g_LogSector,LOG_SIZE);
            memcpy((uchar*)g_LogSector + LOG_SIZE*2,(uchar*)g_LogSector,LOG_SIZE);
            return ret;
        }
    }
	ret=nand_read(des, buff, wlen);
	return ret;
}

void s_EraseSector(unsigned long Addr)
{
	if(Addr >=(LOG_ADDR) && (Addr  < LOG_END_ADDR))
	{
		memset((uchar*)g_LogSector,0xFF,sizeof(g_LogSector));
        if(NAND_ID_HYNIX != GetNandType()){
            OtherEraseLog(Addr);
            return;
        }
	}
	
	nand_erase(Addr,DATA_SECTOR_SIZE);
}

void s_WriteFlashFailed(void)
{
    ScrCls();
    Lcdprintf("Write(F) failed!\n");
    for(;;);
}

void s_ExchangeFailed(void)
{
    ScrCls();
    Lcdprintf("Exchange(F) failed!\n");
    for(;;);
}


#ifdef _DEBUG_F
int TestFileSystem2()
{
	int i,j,k,n,loop,iMaxFile,aiFid[256],iFid,iLen,iLoop,iWriteLen=4;
	char aucFileName[128];
	uchar aucTmp[1024*256];
	uchar recv_buf[1024*256];
	#define	MAX_WRITE_LEN (1024*50)
	uint uiCount=0,uiFail=0;
	uint ret;
	
	iLoop=0;
	loop = 257;
	for(n=0;n<loop;n++)
	{
		sprintf(aucFileName,"TestFile%d",n);
		iFid = open(aucFileName,O_RDWR);
		
		if(iFid<0)
			iFid = open(aucFileName,O_CREATE);
		
		printk("iFid:%d!\r\n",iFid);
		iLen=filesize(aucFileName);

		seek(iFid,0,SEEK_END);

		if(iWriteLen==0)
		{
			i=rand();
			i = i%(MAX_WRITE_LEN);
			if(i<0)i=i*-1;
		}
		else i=iWriteLen;
		
		ret=write(iFid,aucTmp,sizeof(aucTmp));
		printk("Write ret:%d, len:%d\r\n",ret,sizeof(aucTmp));
		seek(iFid,0, SEEK_SET);
		ret = read(iFid,recv_buf,sizeof(aucTmp));
		printk("read ret:%d \r\n", ret);
		for(k=0;k<sizeof(aucTmp);k++)
		{
			if(aucTmp[k] != recv_buf[k])
			{
				printk("data error\r\n");
				break;
			}
		}
		close(iFid);
	}
}
#endif

