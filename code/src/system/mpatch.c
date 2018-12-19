#include "base.h"
#include "posapi.h"
#include "../dll/dll.h"
#include "../file/filedef.h"

extern uint *kernel_sym_start,*kernel_sym_end;
extern volatile uchar k_AppRunning;
static volatile uint text_addr[2]={0};


uchar GetMpatchSysPara(char *name)
{
    char mvalue[2];
    char uaUpperName[30];

    UpperCase(name, uaUpperName, 16);
    mvalue[0]=mvalue[1]=0;
    if(SysParaRead(uaUpperName, mvalue)==0)
        if(mvalue[0]>='0') return (uchar)(mvalue[0]-'0');
    
    return 0;
}

int SetMpatchSysPara(char *name,uchar nums)
{
    char mvalue[2];
    char uaUpperName[30];

    UpperCase(name, uaUpperName, 16);
    mvalue[0]='0'+nums;
    mvalue[1]=0;
    return SysParaWrite(uaUpperName, mvalue);
}

/*
mpatch调用,参数有param导入
返回值:表示是否成功运行mptach调用
0:表示成功进入mpatch调用
其它表示mpatch调用失败
*/
int s_MpatchEntry(char *name,PARAM_STRUCT *param)
{
    volatile void (*mpatchEntry)(void *param)=0;
    static volatile uchar mpatchBusy=0;
    PARAM_STRUCT initParam;
    uint tmpd,fileSize,x;
    uchar attr[2],para=0;
    int fd=-1,ret=0;
    char uaUpperName0[30],uaUpperName1[30],fName[64];
    SO_HEAD_INFO header={0};

    memset(&header,0,sizeof(header));
    memset(uaUpperName0,0,sizeof(uaUpperName0));
    memset(uaUpperName1,0,sizeof(uaUpperName1));
    memset(fName,0,sizeof(fName));
    strcpy(fName,name);
    strcat(fName,MPATCH_EXTNAME);
    
    attr[0]=FILE_ATTR_PUBSO;
    attr[1]=FILE_TYPE_SYSTEMSO;
    fd = s_open(fName, O_RDWR, attr);
    if(fd<0) return -2;
    
    fileSize=GetFileLen(fd);
    seek(fd,SOINFO_OFFSET,SEEK_SET);
    tmpd = read(fd,(uchar *)&header,sizeof(header));
    if(tmpd!=sizeof(header))
    {
        ret = -3;
        goto exit;
    }

    UpperCase(fName, uaUpperName0, 16);
    UpperCase(header.so_name, uaUpperName1, 16);
    if(memcmp(uaUpperName0,uaUpperName1,strlen(uaUpperName0)))
    {
        ret = -4;
        goto exit;
    }

    //mpatch para
    if(!k_AppRunning)//需要监控mpatch运行
    {
        para = GetMpatchSysPara(fName);
        if(para>=MPATCH_MAX_PARA)return -5;
    }

    //最多可以同时运行两个text_addr不同的mpatch
    irq_save(x);
    if(text_addr[0] && text_addr[1])
    {
        ret = -6;
        irq_restore(x);
        goto exit;
    }
    if(text_addr[0]==header.text_sec_addr ||
        text_addr[1]==header.text_sec_addr)
    {
        ret = -7;
        irq_restore(x);
        goto exit;
    }

    if(text_addr[0]==0)
    {
        text_addr[0]=header.text_sec_addr;
    }
    else if(text_addr[1]==0)
    {
        text_addr[1]=header.text_sec_addr;
    }
    irq_restore(x);

    if(header.lib_type!=LIB_TYPE_SYSTEM)
    {
        ret = -8;
        goto exit;
    }

    mpatchEntry = (void *)header.init_func;
    if(mpatchEntry==0)
    {
        ret = -9;
        goto exit;
    }

    //loading patch
    seek(fd,0,SEEK_SET);
    tmpd = read(fd,(uchar *)header.text_sec_addr,fileSize);
    if(tmpd!=fileSize)
    {
        ret = -10;
        goto exit;
    }

    if(!k_AppRunning)//需要监控mpatch运行
    {
        //write flag
        para+=1;
        ret = SetMpatchSysPara(fName,para);
        if(ret != 0)
        {
            ret = -11;
            goto exit;
        }
    }
    
    //init patch
    initParam.FuncType = MPATCH_COMMON_FUNC;
    initParam.FuncNo = MPATCH_INIT_NO;
    initParam.Addr1 = (uint *)&kernel_sym_start;
    initParam.Addr2 = (uint *)&kernel_sym_end;
    ClearIDCache();
    mpatchEntry(&initParam);

    //running function
    mpatchEntry(param);

    if(!k_AppRunning)//需要监控mpatch运行
    {
        //clear flag
        if(SetMpatchSysPara(fName, MPATCH_MIN_PARA)!=0)
            SetMpatchSysPara(fName, MPATCH_MIN_PARA);
    }

exit:
    if(fd>=0)close(fd);
    if(text_addr[0]&&(text_addr[0]==header.text_sec_addr))
        text_addr[0]=0;
    if(text_addr[1]&&(text_addr[1]==header.text_sec_addr))
        text_addr[1]=0;
    return ret;
}

//return 1:RS9110_mpatch
//return 2:AP6181_mpatch
//return 0:no_wifi_mpatch
int is_wifi_mpatch(char *name)
{
	if(strstr(name, MPATCH_EXTNAME)) 
	{
		if(strstr(name, MPATCH_NAME_RS9100) || strstr(name, MPATCH_NAME_rs9100))
			return 1;
		else if(strstr(name, MPATCH_NAME_AP6181))
			return 2;
		else 
			return 0;
	}
	else return 0;
}

