
#include  <stdio.h>
#include  <string.h>
#include  <stdarg.h>
#include <Base.h>
#include "../comm/Comm.h"
#include "../file/filedef.h"
#include "../DownLoad/LocalDL.h"
#include "../PUK/puk.h"
#include "../font/font.h"
#include "../encrypt/sha256.h"
//#include "..\comm\comm.h"

#define DELAYTIME	0

extern uint swi_addr(void);

#define PUK_FIRMWARE	0x81


extern const APPINFO    MonitorInfo;
extern void entry_point(void);

//
static ulong           k_RunAppStartAddr;

static uchar           k_CallAppValid[2];
static APPINFO         k_CallAppInfo[2];

volatile uchar k_AppRunning=0;
extern volatile unsigned int baseso_text_sec_addr;
extern void     RunStartAddr(ulong startaddr);
extern POS_AUTH_INFO *GetAuthParam(void);


uchar GetCallAppInfo(APPINFO *CallAppInfo)
{
	uint call_swiaddr;

	call_swiaddr=swi_addr();
	
    if((call_swiaddr >= MONITOR_RUN_ADDR) && (call_swiaddr < MONITOR_RUN_ADDR+MONITOR_AREA_SIZE))
    {
		*CallAppInfo=MonitorInfo;
    }
    else if((call_swiaddr >= baseso_text_sec_addr) && (call_swiaddr < baseso_text_sec_addr+BASESO_AREA_SIZE))
    {
        *CallAppInfo=MonitorInfo;
    }
    else if((call_swiaddr >= MAPP_RUN_ADDR) && (call_swiaddr < MAPP_RUN_ADDR+MAPP_AREA_SIZE))
    {
		*CallAppInfo=k_CallAppInfo[0];
    }
    else if((call_swiaddr >= SAPP_RUN_ADDR) && (call_swiaddr < SAPP_RUN_ADDR+SAPP_AREA_SIZE))
    {
		*CallAppInfo=k_CallAppInfo[1];
    }
    else if((call_swiaddr >= SO_START) && (call_swiaddr < SO_END))
    {
        uchar appnum;
        appnum=SoAppNumSync(call_swiaddr);
		if(appnum ==0) *CallAppInfo=k_CallAppInfo[0];
        else    *CallAppInfo=k_CallAppInfo[1];
    }
    else
    {
		memset((uchar *)CallAppInfo,0,sizeof(APPINFO));
        return(1);
    }
    
	return(0);
}

//  获取应用程序的入口地址,参考init.s
void GetAppEntry(uchar *AppHead, ulong *MainEntry, ulong *EventEntry, ulong *AppInfoEntry)
{
    ulong   tmp;

    tmp = 0;
    
	if(MainEntry != NULL)
    {
        memcpy(&tmp, AppHead, 3); //B指令，
        *MainEntry = tmp * 4 + 8;//main_entry，左偏移2位 + 8
    }
    
	if(EventEntry != NULL)
    {
        memcpy(&tmp, AppHead+4, 3);//event_main
        *EventEntry = tmp * 4 + 12;//左偏移2位 + 8 + 4，指令偏移
    }

    if(AppInfoEntry != NULL)
    {
        memcpy(&tmp, AppHead+8, 3);//appinfo
        *AppInfoEntry = tmp * 4 + 16;//左偏移2位 + 8 + 4 +4
    }
}


int ReadAppInfo(uchar AppNo, APPINFO* ai)
{
    int     fd;
    char    AppName[17];
    ulong   StartAddr;
    uchar   buff[20], sAppFile[20];
/*    
	APPINFO CurRunApp;

    if(GetCallAppInfo(&CurRunApp))
    {
        return(-1);
    }
	
    if((CurRunApp.AppNum!=0) && (CurRunApp.AppNum!=0xff))
    {
        return(-1);
	}*/

    if(AppNo >= APP_MAX_NUM)
	{
        return -2;
	}

    sprintf(AppName, "%s%d", APP_NAME, AppNo);
    fd = s_open(AppName, O_RDWR, (uchar *)"\xff\x01");

    if(fd < 0)
    {
        return -3;
    }

    memset(buff, 0, sizeof(buff));
    seek(fd, -8L, SEEK_END);
    read(fd, buff, 8);

    if(memcmp(buff,DATA_SAVE_FLAG,8))
    {
        close(fd);
        return -4;
    }

    seek(fd, 0, SEEK_SET);
    read(fd, (uchar *)AppName, 12);
    GetAppEntry(AppName, NULL, NULL, &StartAddr);
    seek(fd, StartAddr, SEEK_SET);
    read(fd, (uchar *)ai, sizeof(APPINFO));
	ai->AppNum = AppNo;//saint add it
    close(fd);
    return 0;
}

static short iHaveLoadManage=0;
uchar CheckIfManageExist()
{
	int     fd;
	char    AppName[17],buff[17];
	APPINFO CurRunApp;

	if(GetCallAppInfo(&CurRunApp))
	{
		return 2;
	}

	if(CurRunApp.AppNum == 0)
	{
		return 2;
	}   
	if (iHaveLoadManage)
	{
		return 1;
	}

	return 0;
}

uchar ReadAppState(uchar AppNo)
{
    int     fd;
    uchar   flag, buf[16];
    char    AppName[17];

	if(AppNo >= APP_MAX_NUM)
        return -1;

    sprintf(AppName, "%s%d", APP_NAME, AppNo);
	fd = s_open(AppName, O_RDWR, (uchar *)"\xff\x01");
	if(fd < 0)
        return -1;
	seek(fd, -16L, SEEK_END);
    memset(buf, 0x00, sizeof(buf));
	read(fd, buf, 16);
	close(fd);

    flag = 0x03;
    if(memcmp(buf+8, DATA_SAVE_FLAG, 8))
        return(-1);
    if(buf[0] == 0x00)
        flag |= 0x04;

	return(flag);
}

extern int s_iVerifySignature(int iType,uchar * pucAddr,long lFileLen,tSignFileInfo *pstSigInfo);
//  载入应用程序到对应的地址
int LoadApplication(uchar AppNo, ulong LoadAddr)
{
    int         i, j, fd, cnt, mlen;
    ulong       StartAddr, InfoAddr;
    char        AppName[17];
    APPINFO     CurRunApp;
    uchar       ucRet;

    if(GetCallAppInfo(&CurRunApp))
    {
        return(-1);
    }

    if((CurRunApp.AppNum==0) || (CurRunApp.AppNum==0xff))
    {
        if((CurRunApp.AppNum==0) && (AppNo==0))
        {
			return(-1);
		}
        if(AppNo >= APP_MAX_NUM)
		{
			return(-2);
		}
	}
    else
    {
        return(-1);
    }

    sprintf(AppName, "%s%d",APP_NAME, AppNo);
	fd = s_open(AppName, O_RDWR, (uchar *)"\xff\x01");
    if(fd < 0)
    {
        return(-3);
    }

    seek(fd, 0, SEEK_SET);
    if(AppNo == 0)
	{
        i = 0;
		mlen=MAPP_AREA_SIZE;
	}
    else
	{
        i = 1;
		mlen=SAPP_AREA_SIZE;
	}
	
	cnt=read(fd, (char *)LoadAddr, mlen);
    StartAddr = LoadAddr+cnt;	
    close(fd);

    //  back up app info
    GetAppEntry((uchar *)LoadAddr, NULL, NULL, &InfoAddr);
    memcpy((uchar *)&(k_CallAppInfo[i]), (uchar *)(LoadAddr+InfoAddr), sizeof(APPINFO));
    k_CallAppInfo[i].AppNum = AppNo;
    k_CallAppValid[i] = 1;

    if(memcmp((uchar *)(StartAddr-strlen(DATA_SAVE_FLAG)), DATA_SAVE_FLAG, strlen(DATA_SAVE_FLAG)))
    {
        ScrCls();
        ScrPrint(0, 2, 0x01, "NAME:%s", k_CallAppInfo[i].AppName);
        ScrPrint(0, 6, 0x01, "NO:%d", AppNo);
        SCR_PRINT(0, 0, 0x01, " 应用程序不完整", " APP FILE ERROR");
        while(1);
    }
    if(GetAppSigInfo(AppNo, NULL)<0)
    {
        if(s_GetTamperStatus()==0 ||
            s_GetBootSecLevel()==0)/*for compatibility :old boot + new monitor*/
        {
            ScrCls();
            ScrPrint(0, 2, 0x01, "NAME:%s", k_CallAppInfo[i].AppName);
            ScrPrint(0, 6, 0x01, "NO:%d", AppNo);
            SCR_PRINT(0, 0, 0x41, "应用程序签名错误", "APP SIGN ERROR");
            while(1);
        }
    
        return -4;
    }   

    StartAddr -= 16;
    if(memcmp((uchar *)(StartAddr-strlen(SIGN_FLAG)), SIGN_FLAG, strlen(SIGN_FLAG)))
    {
        cnt = StartAddr - LoadAddr;
    }
    else
    {
        memcpy((uchar *)&cnt, (uchar *)(StartAddr-strlen(SIGN_FLAG)-4), 4);
    }
	cnt-=12; //减去"PAX-S80-SAPP"的长度
    if(cnt < (SAPP_AREA_SIZE))
    {
        memset((uchar *)(LoadAddr+cnt), 0x00, SAPP_AREA_SIZE-cnt);
    }
    return(0);
}

//  载入应用程序并返回运行地址
int RunApp(uchar AppNo)
{
    int             ret,i;
    ulong           StartAddr;
    ulong           runaddr;
    ST_EVENT_MSG    *param;
    APPINFO         CurRunApp;

    if(GetCallAppInfo(&CurRunApp))
    {
        return(-1);
    }

    if((CurRunApp.AppNum != 0) || (AppNo==0))
    {
        return(-1);
	}

    if(AppNo>=APP_MAX_NUM)
    {
        return -2;
    }
	
    ret = LoadApplication(AppNo, SAPP_RUN_ADDR);
    
	if(ret)
    {
        return ret;
    }

    GetAppEntry((unsigned char *)SAPP_RUN_ADDR, &StartAddr, NULL, NULL);

    k_RunAppStartAddr = StartAddr + SAPP_RUN_ADDR;
    return 0;
}

//  载入事件处理程序并返回运行地址
int DoEvent(uchar AppNo, ST_EVENT_MSG *param)
{
    int         ret;
    ulong       StartAddr;
    APPINFO     CurRunApp;

    if(GetCallAppInfo(&CurRunApp))
    {
        return(-1);
    }

    if((CurRunApp.AppNum != 0) || (AppNo==0))
    {
        return(-1);
	}

    if(AppNo>=APP_MAX_NUM)
    {
        return -2;
    }

    ret = LoadApplication(AppNo, SAPP_RUN_ADDR);

    if(ret)
    {
        return ret;
    }
    GetAppEntry((uchar *)SAPP_RUN_ADDR, NULL, &StartAddr, NULL);
    k_RunAppStartAddr = StartAddr + SAPP_RUN_ADDR;

	return 0;
}

//  判断应用程序是否第一次运行
uchar CheckFirst(void)
{
    int         fd;
    uchar       ch;
    APPINFO     CurRunApp;
    uchar       AppName[17];
    uchar       buf[17];

    if(GetCallAppInfo(&CurRunApp))
    {
        return(0xff);
    }

    memset(AppName, 0x00, sizeof(AppName));
    sprintf(AppName, "%s%d", APP_NAME, CurRunApp.AppNum);

    fd = s_open(AppName, O_RDWR, (uchar *)"\xff\x01");
    
	if(fd < 0)
    {
        return(0);
    }

    memset(buf, 0x00, sizeof(buf));
    seek(fd, -16L, SEEK_END);
    read(fd, buf, 16);

    if(memcmp(buf+8, DATA_SAVE_FLAG, 8))
    {
        close(fd);
        return(0);
    }

    if(buf[0] & 0x01)      //  not first RunApp
    {
        close(fd);
        return(0xff);
    }

    buf[0] |= 0x01;
    seek(fd, -16L, SEEK_END);
    write(fd, buf, 1);

    close(fd);


    return 0x00;
}


int DeleteApp(uchar AppNo, uchar SubFile)
{
    char        AppName[17];
    FILE_INFO   FileInfo[MAX_FILES];
    uint        MaxFileNum, i;
    int         DelNum = 0;
    uchar       Attr[2];

    if(AppNo >= APP_MAX_NUM)
	{
        return -1;
	}

    sprintf(AppName, "%s%d",APP_NAME, AppNo);

    if(s_filesize(AppName, (uchar *)"\xff\x01") < 0)return 0;

    if(SubFile)
    {
        memset((char *)&FileInfo, 0x00, sizeof(FileInfo));
        MaxFileNum = GetFileInfo(FileInfo);
        for(i=0; i<MaxFileNum; i++)
        {
            if(FileInfo[i].attr == AppNo)     //  属于这个应用的附属文件
            {
                Attr[0] = FileInfo[i].attr;
                Attr[1] = FileInfo[i].type;
                s_remove(FileInfo[i].name, Attr);
                DelNum++;
            }
        }
    }
    s_remove(AppName, (uchar *)"\xff\x01");
    DelNum++;
    return(DelNum);
}

//  计算应用以及子文件的大小
int CalcAppSize(uchar AppNo, uchar SubFile)
{
    int         TotalLen;
    char        AppName[17];
    FILE_INFO   FileInfo[MAX_FILES];
    uint        MaxFileNum, i;
    uchar       Attr[2];

    if(AppNo >= APP_MAX_NUM)
	{
        return 0;
	}
    TotalLen = 0;
    sprintf(AppName, "%s%d",APP_NAME, AppNo);
    TotalLen = s_filesize(AppName, (uchar *)"\xff\x01");
    if(TotalLen < 0)
    {
        return 0;
    }

    if(SubFile)
    {
        memset((char *)&FileInfo, 0x00, sizeof(FileInfo));
        MaxFileNum = GetFileInfo(FileInfo);
        for(i=0; i<MaxFileNum; i++)
        {
            if(FileInfo[i].attr == AppNo)     //  属于这个应用的附属文件
            {
                Attr[0]   = FileInfo[i].attr;
                Attr[1]   = FileInfo[i].type;
                TotalLen += FileInfo[i].length;
            }
        }
    }
    return(TotalLen);
}

int SaveApp(uchar AppNo, uchar *buf, uint Len)
{
    int     iRet, FileID;
    uchar   Append[17], AppName[17];

    if(AppNo >= APP_MAX_NUM)
        return(-1);
    memset(Append, 0xff, sizeof(Append));
    memcpy(Append+16-strlen(DATA_SAVE_FLAG), DATA_SAVE_FLAG, strlen(DATA_SAVE_FLAG));
    memset(AppName, 0x00, sizeof(AppName));
    sprintf(AppName, "%s%d", APP_NAME, AppNo);
    FileID = s_open(AppName, O_CREATE|O_RDWR, (uchar *)"\xff\x01");
    if(FileID < 0)
        return(FileID);
    seek(FileID, 0, SEEK_SET);
    iRet = write(FileID, buf, Len);
    if(iRet != Len)
    {
        close(FileID);
        return(-1);
    }
    write(FileID, Append, 16);
    close(FileID);
    return(0);
}

void MonitorLoadApp(void)
{
    uchar   AppNo, ch=NOKEY;
    int     iRet, i, fd;
    uint     status;
    ulong   EntryAddr, MainFuncAddr;
    POS_AUTH_INFO *authinfo;
    
    AppNo = 0;
    memset((uchar *)k_CallAppInfo, 0x00, sizeof(k_CallAppInfo));
    k_CallAppValid[0] = 0;
    k_CallAppValid[1] = 0;
	iHaveLoadManage = 0;

    while(AppNo < APP_MAX_NUM)
    {
        if(AppNo == 0)
        {
			EntryAddr = MAPP_RUN_ADDR;
			iHaveLoadManage = 1;
		}
        else
        {
			EntryAddr = SAPP_RUN_ADDR;
			iHaveLoadManage = 0;
		}

        iRet = LoadApplication(AppNo, EntryAddr);

		if(!iRet)
        {
            break;
        }
        AppNo++;
    }

    if(AppNo < APP_MAX_NUM)
    {
    	ScrCls();
		PortClose(P_USB_DEV);
		PortClose(P_USB_HOST);
        k_AppRunning=1;
        GetAppEntry((uchar *)EntryAddr, &MainFuncAddr, NULL, NULL);
        EntryAddr += MainFuncAddr;
		while(1)
        {
        	ClearIDCache();
            RunStartAddr(EntryAddr);
        }
    }

    if(0!=s_GetBootSecLevel())
    {
		authinfo=GetAuthParam();
		status =s_GetBBLStatus();
		if(status)
		{
			s_DisplayTamperInfo(status);
			ch = getkey();
			if(ch==KEYFN || ch==KEYF1) ShowSensorInfo(status);
		}
		else if ((authinfo->SecMode==POS_SEC_L2) && (authinfo->TamperClear==0))
		{
			ScrClrLine(0,5);
			SCR_PRINT(0, 1, 0x01, "\n 请重启，并授权", "Pos tampered,\npls reboot and authority.");
			getkey();
		}
    }
    RunFtestApp();
    SCR_PRINT(0, 3, 0x01, "    无 应 用    ", " No Application ");
}


//DLL返回时使用
uint app_even_start(void)
{
	return k_RunAppStartAddr;
}

void * GetAppInfoAddr()
{
    return (void*)k_CallAppInfo;
}

/*
*工厂测试程序常驻文件系统、按需显现/隐藏
*1.将MF_PVK签名的工厂测试程序以模块驱动方式下载
*2.只有当机器无其他应用时，机器可直接加载上述下载的工厂测试程序
*3.可在版本信息界面，1s内完成输入"4321"，机器将直接进入上述下载的工厂测试程序
*4.可在版本信息界面，1s内完成输入"1234",可删除常驻的工厂测试程序
*/
int IsFtestFile(uchar *AppData,uint len)
{
	uchar szAppName[33];
	uint tmp=0;
		
	memset(szAppName, 0, sizeof(szAppName));
	memcpy(&tmp,(uchar*)(AppData+8),3);//71 6e 61 63	
	memcpy(szAppName,(uchar*)(AppData + tmp * 4 + 16),32);//get the app name
	if(!strstr(szAppName, "FTESTHIDE")) return 0;//不是工厂测试程序
	
	return 1;
}


int WriteFtestAppFile(uchar *AppData, uint len)
{
    int ret,iAppType;
    uint tmp=0;
    uchar szAppName[33], tmpbuf[33];

    ret = s_iVerifySignature(SIGN_FILE_MON,AppData,len,NULL);
	if(ret==PUK_RET_SIG_TYPE_ERR) return -8;
    else if(ret!=0) return -1;

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
                
    if(iAppType!=0) return -2;//不是主应用
    if (len > MAX_APP_LEN) return -6;

    ret = s_iWriteApp(APP_MAX_NUM,(uchar*)AppData,len);
    if(ret!= 0) return -7;

    return 0;
}

int RunFtestApp()
{
    ulong   EntryAddr, MainFuncAddr,StartAddr, InfoAddr;
    int         fd, cnt, ret;
    char        AppName[17];
    APPINFO     CurRunApp;

    if(GetCallAppInfo(&CurRunApp)) return -1;
    if(CurRunApp.AppNum!=0xff) return -2;

    memset((uchar *)k_CallAppInfo, 0x00, sizeof(k_CallAppInfo));
    k_CallAppValid[0] = 0;
    k_CallAppValid[1] = 0;

    EntryAddr = MAPP_RUN_ADDR;
    iHaveLoadManage = 1;

    //Load App File, app file name="AppFile24"
    sprintf(AppName, "%s%d",APP_NAME, APP_MAX_NUM);
	fd = s_open(AppName, O_RDWR, (uchar *)"\xff\x01");
    if(fd < 0) return(-3);  
    seek(fd, 0, SEEK_SET);
	cnt=read(fd, (char *)EntryAddr, MAPP_AREA_SIZE);
    StartAddr = EntryAddr+cnt;	
    close(fd);

    if(memcmp((uchar *)(StartAddr-strlen(DATA_SAVE_FLAG)), DATA_SAVE_FLAG, strlen(DATA_SAVE_FLAG))) return -5;
    cnt -= 16;
    ret = s_iVerifySignature(SIGN_FILE_MON,(uchar *)EntryAddr,cnt,NULL);
    if(ret) return -6;
    
    //  back up app info
    GetAppEntry((uchar *)EntryAddr, NULL, NULL, &InfoAddr);
    memcpy((uchar *)&(k_CallAppInfo[0]), (uchar *)(EntryAddr+InfoAddr), sizeof(APPINFO));
    k_CallAppInfo[0].AppNum = APP_MAX_NUM;
    k_CallAppValid[0] = 1;

    StartAddr -= 16;
    if(memcmp((uchar *)(StartAddr-strlen(SIGN_FLAG)), SIGN_FLAG, strlen(SIGN_FLAG)))
    {
        cnt = StartAddr - EntryAddr;
    }
    else
    {
    	memcpy((uchar *)&cnt, (uchar *)(StartAddr-strlen(SIGN_FLAG)-4), 4);
    }
	cnt-=12; //减去"PAX-S80-SAPP"的长度
    if(cnt < (MAPP_AREA_SIZE))
    {
        memset((uchar *)(EntryAddr+cnt), 0x00, SAPP_AREA_SIZE-cnt);
    }

    ScrCls();
    PortClose(P_USB_DEV);
    PortClose(P_USB_HOST);
    k_AppRunning=1;     
    GetAppEntry((uchar *)EntryAddr, &MainFuncAddr, NULL, NULL);
    EntryAddr += MainFuncAddr;
    while(1)
    {
        ClearIDCache();
        RunStartAddr(EntryAddr);
    } 
}


void WakeUpFtestApp(uchar key)
{
    static int key_num=0;
    static uchar pswd[8]={0};
    static T_SOFTTIMER tm;
    uchar Name[33];
    
    sprintf(Name, "%s%d",APP_NAME, APP_MAX_NUM);
    if(filesize(Name)<0) return;
    if(key_num==0){
        s_TimerSetMs(&tm, 1000);
        memset(pswd, 0x00, sizeof(pswd));
    }
    else if(0==s_TimerCheckMs(&tm)){
        key_num = 0;
        s_TimerSetMs(&tm, 1000);
        memset(pswd, 0x00, sizeof(pswd));
    }

    pswd[key_num] = key;
    key_num++;
    if(key_num==4)
    {
        if(0==memcmp(pswd, "4321", key_num)) RunFtestApp();
        else if(0==memcmp(pswd, "1234", key_num))
        {
            ScrCls();
            kbflush();
            SCR_PRINT(0,2,0,"确定删除?   ","Delete Ftest?");
            SCR_PRINT(0,4,0,"[确认] - 是 ","[ENTER] - YES");
            SCR_PRINT(0,6,0,"其他键 - 否 ","Other Key - NO");
            if(GetKeyMs(5000)== KEYENTER) s_iDeleteApp(APP_MAX_NUM);
        }    
        key_num = 0;
        memset(pswd, 0x00, sizeof(pswd));
    }
}

