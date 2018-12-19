#include "base.h"
#include "..\puk\puk.h"
#include "..\dll\dll.h"
#include "..\file\filedef.h"
#include "link_info.h"

static int (*baseso_entry_func)(void *param)=NULL; 
/*记录baseso的运行地址，从baseso文件信息中获取*/
volatile unsigned int baseso_text_sec_addr = 0xFFFFFFFF;

int soname_check(char *name)
{
    uchar buff[20];
	int str_len = strlen(name);
	
	if(str_len < 4) return -1;
    UpperCase(name + str_len - 3,buff, 3);
	if(memcmp(buff, ".SO", 3) == 0) return 0;

    return -1;
}

void *GetBaseSoAddr(void)
{
    int fd,len,num,i,ret;
    uint addr;
    uchar attr[2];
    FILE_INFO finfo[256];
    SO_HEAD_INFO head;

    num=GetFileInfo(finfo);
    for(i=0;i<num;i++)
    {
        if(strstr(finfo[i].name,FIRMWARE_SO_NAME) && !soname_check(finfo[i].name)) break;
    }
    if(i==num) return NULL;

    attr[0]=FILE_ATTR_PUBSO;
    attr[1]=FILE_TYPE_SYSTEMSO;

    fd = s_open(finfo[i].name, O_RDWR, attr);
    if(fd >= 0)
    {
        len = GetFileLen(fd);    
        (void)read(fd, (uchar*)&head, sizeof(head));

        baseso_text_sec_addr = head.text_sec_addr;
        addr = baseso_text_sec_addr;
        seek(fd,0,SEEK_SET);
        ret = read(fd,(uchar *)addr,len);
        close(fd);
        if(ret!=len)return NULL;
        ClearIDCache();
    
        return head.init_func;
    }
    return NULL;
}


/*Call Base Po APIs*/
extern uint *kernel_sym_start,*kernel_sym_end;
#define FIRMWARE_API_FUN          0x20
#define PED_API_FUN              0x22

#define FIRMWARE_EXPORTSYMTABLE   0x01
#define FIRMWARE_GETSOREALNAME    0x02
#define FIRMWARE_GETALLSONAME     0x03
#define FIRMWARE_REMOVESO         0x04
#define FIRMWARE_GETSOINFO        0x05
#define FIRMWARE_ELFGETNEEDSO     0x06
#define FIRMWARE_SOAPPNUMSYNC     0x07
#define FIRMWARE_PROTIMSUPDATE    0x08
#define FIRMWARE_PROTIMSVERSION   0x09
#define FIRMWARE_REMOTELOADAPP    0x0a
#define FIRMWARE_PROTIMSENTRY     0x0b
#define FIRMWARE_PEDGETVER        0x0c
#define FIRMWARE_PEDINIT          0x0d
#define FIRMWARE_PEDGETLIBVER     0x0e
#define FIRMWARE_PEDDOWNLOADMENU  0x0f
#define FIRMWARE_GETMSRROOTKEY  0x10
#define FIRMWARE_VTRACEATTACKINFO 0x11
#define FIRMWARE_PEDSELFCHECKUP     0x12
#define FIRMWARE_GETBASESOINFO      0x13
#define FIRMWARE_PEDGETMAC          0x15
#define FIRMWARE_PEDSETIDKEYSTATE   0x16
#define FIRMWARE_PEDGETIDKEYSTATE   0x17
#define FIRMWARE_PEDEXPORTIDKEY     0x18
#define FIRMWARE_PEDGETKCV          0x19

int s_BaseSoLoader(void)
{
    PARAM_STRUCT param;

    if(baseso_entry_func!=NULL)
        return (int)baseso_entry_func;

    baseso_entry_func = GetBaseSoAddr();
    if(baseso_entry_func == NULL)return NULL;
        
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_EXPORTSYMTABLE;
    param.Addr1= (void*)&kernel_sym_start;
    param.Addr2= (void*)&kernel_sym_end;
    baseso_entry_func(&param);

    return (int)baseso_entry_func;
}

/*用于Monitor调用BaseSo*/
void s_BaseSoEntry(PARAM_STRUCT *param)
{
    if(baseso_entry_func || (s_BaseSoLoader()!=NULL))
    {
        baseso_entry_func(param);    
    }
    else
    {
        ScrCls();
        ScrPrint(0,3,0,"can't find base so!");
        while(1);
    }
}

int RunUpdataRemoteDownload()
{
    PARAM_STRUCT param;

    if(!kbhit())
    {
        param.FuncType = FIRMWARE_API_FUN;
        param.FuncNo = FIRMWARE_PROTIMSUPDATE;
        param.u_char1 =kbcheck();
        s_BaseSoEntry(&param);
        return param.int1;
    }
    else
    {
        param.FuncType = FIRMWARE_API_FUN;
        param.FuncNo = FIRMWARE_PROTIMSUPDATE;
        param.u_char1 =NULL;
        s_BaseSoEntry(&param);
        return param.int1;
    }
}

void RemoteDload(void)
{
    PARAM_STRUCT param;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PROTIMSUPDATE;
    param.u_char1 = KEY3;
    s_BaseSoEntry(&param);
}

int RemoteLoadVer(unsigned char *ver)
{
    PARAM_STRUCT param;
	if(s_BaseSoLoader()==NULL)return -1;
	
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PROTIMSVERSION;
    param.u_str1 = ver;
    s_BaseSoEntry(&param);
    
    return param.int1;
}

int RemoteLoadApp(void *ptCommPara)
{
    PARAM_STRUCT param;
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_REMOTELOADAPP;
    param.Addr1 = ptCommPara;
    s_BaseSoEntry(&param);
    return param.int1; 
}

void ProTimsEntry(void *ptParam)
{
    PARAM_STRUCT param;
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PROTIMSENTRY;
    param.Addr1 = ptParam;
    s_BaseSoEntry(&param);    
}

int GetAllSoName(uchar *pOutPOsName, int *LenOut)
{
    PARAM_STRUCT param;

    if(s_BaseSoLoader()==NULL)return -1;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_GETALLSONAME;
    param.u_str1 = pOutPOsName;
    param.up_short1 = (ushort *)LenOut;
    s_BaseSoEntry(&param);
    
    return param.int1;  
}
int  RemoveSo(const char *pSoName,const char *pAppName)
{
    PARAM_STRUCT param;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_REMOVESO;
    param.u_str1 = (uchar *)pSoName;
    param.u_str2 = (uchar *)pAppName;
    s_BaseSoEntry(&param);
    
    return param.int1;  
}

int elf_get_needed_so(char *filename, char *soname)
{
    PARAM_STRUCT param;

    if(s_BaseSoLoader()==NULL)return 0;
    
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_ELFGETNEEDSO;
    param.u_str1 = (uchar *)filename;
    param.u_str2 = (uchar *)soname;
    s_BaseSoEntry(&param);
    
    return param.int1;      
}

int GetSoInfo(uchar *pszSoName, uchar *pszSoInfo)
{
    PARAM_STRUCT param;

    if(s_BaseSoLoader()==NULL)return 0;
    
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_GETSOINFO;
    param.u_str1 = (char *)pszSoName;
    param.u_str2 = (char *)pszSoInfo;

    s_BaseSoEntry(&param);
    
    return param.int1;  
}


uchar SoAppNumSync(uint addr)
{
    PARAM_STRUCT param;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_SOAPPNUMSYNC;
    param.u_int1 = addr;
    s_BaseSoEntry(&param);
    
    return param.u_char1;
}
/*
Buff:12字节,是12字节
Buf[0]:
         bit0: ped
         bit1: e2ee
         bit2: wlext
         bit3: sm
*/
int GetBasesoInfo(uchar *buff)
{
    PARAM_STRUCT param;

    if(s_BaseSoLoader()==NULL)return 0;
    
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_GETBASESOINFO;
    param.u_str1 = buff;

    s_BaseSoEntry(&param);    
    return 0;
}

/******************************* ped api/*******************************/
void PedFun(PARAM_STRUCT *glParam)
{
    PARAM_STRUCT param;

    memcpy(&param,glParam,sizeof(PARAM_STRUCT));
    param.FuncType = PED_API_FUN;
    s_BaseSoEntry(&param);

    param.FuncType = glParam->FuncType;
    memcpy(glParam,&param,sizeof(PARAM_STRUCT));
}

int PedGetVer(uchar * VerInfoOut)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL)return 0;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDGETVER;
    param.u_str1 = VerInfoOut;
    s_BaseSoEntry(&param);
    
    return param.int1;
}

int PedGetLibVer(uchar * VerInfoOut)
{
    PARAM_STRUCT param;
	if(s_BaseSoLoader()==NULL)return -1;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDGETLIBVER;
    param.u_str1 = VerInfoOut;
    s_BaseSoEntry(&param);
    
    return param.int1;   
}

void vPedDownloadMenu(void)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL)return ;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDDOWNLOADMENU;
    s_BaseSoEntry(&param);
    
    return ;       
}

int GetMsrRootKey(uchar *MsrRootKey1,uchar *MsrRootKey2)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL)return 0;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_GETMSRROOTKEY;
    param.u_str1= MsrRootKey1;
    param.u_str2= MsrRootKey2;

    s_BaseSoEntry(&param);
    
    return param.int1;       
}

void vTraceAttackInfo(int iAttackType,uint uiAttackInfo)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL)return ;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_VTRACEATTACKINFO;
    param.int1 = iAttackType;
    param.u_int1 = uiAttackInfo;

    s_BaseSoEntry(&param);
    
    return;   
}

int iPedInit(void)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL)return -1;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDINIT;
    s_BaseSoEntry(&param);
    
    return param.int1;  
}

int  PedSelfCheckup(void)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL)return -1;

    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDSELFCHECKUP;
    s_BaseSoEntry(&param);
    
    return param.int1;  
}
/******************************* ped api end/*******************************/

/*
*设置银联SN KEY (ID key)上传状态
* KeyIndex [Input]	:   IDkey索引, 目前仅支持1。
* State [Input]     :   0--未上传,1--已上传。IdKey产生时的默认状态是0x00。
* 返回              :   0--success, -1 -- SO不支持 , 其他<0 error
*/
int s_PedSetIdKeyState(uchar KeyIndex, uchar State)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL) return -1;

    param.int1 = -1;
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDSETIDKEYSTATE;
    param.u_char1 = KeyIndex;
    param.u_char2 = State;
    s_BaseSoEntry(&param);
    
    return param.int1;  
}

/*
*获取银联SN KEY (ID key)上传状态
* KeyIndex [Input]	:   IDkey索引, 目前仅支持1。
* State [Output]    :   0--未上传,1--已上传。IdKey产生时的默认状态是0x00。
* 返回              :   0--success, -1 -- SO不支持 , 其他<0 error
*/
int s_PedGetIdKeyState(uchar KeyIndex, uchar *State)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL) return -1;

    param.int1 = -1;
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDGETIDKEYSTATE;
    param.u_char1 = KeyIndex;
    param.u_str1 = State;
    s_BaseSoEntry(&param);
    
    return param.int1;  
}

/*
*采用可信任的RSA公钥导出SN KEY (ID key)
*KeyIndex [Input]	        :IDkey索引。 目前仅支持1。
*publicRsa [Input]	        :用于导出key的公钥，该公钥必须由monitor本身确认可以信任。
*Mode[input]	            :目前只支持0模式，即PKCS1模式。模式规定输出的数据格式。
*EncryptedKeyBlock [Output]	:输出的key，为公钥模长
*返回值                     :0--success, -1 -- SO不支持 , 其他<0 error
*/
int s_PedExportIdKey(uchar KeyIndex, ST_RSA_KEY *publicRsa, uchar *  EncryptedKeyBlock, uchar Mode)
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL) return -1;

    param.int1 = -1;
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDEXPORTIDKEY;
    param.u_char1 = KeyIndex;
    param.Addr1 = publicRsa;
    param.u_str1 = EncryptedKeyBlock;
    param.u_char2 = Mode;    
    s_BaseSoEntry(&param);
    
    return param.int1; 

}

/*
*获取密钥的KCV值,以供对话双方进行密钥验证,用指定的密钥及算法对一段数据进行加密,并返回部分数据密文。
* 用法同API: PedGetKcv
*KeyType   [Input]      :  PED_TLK,PED_TMK,PED_TAK,PED_TPK,PED_TDK,PED_TIK
*KeyIndex  [Input]      :  密钥的索引号
*Mode      [Input]      :  0
*Kcv       [Output]     :   3BYTE kcv
*/
int s_PedGetKcv(uchar KeyType, uchar KeyIndex, uchar* Kcv, uchar Mode) 
{
    PARAM_STRUCT param;
    if(s_BaseSoLoader()==NULL) return -1;

    param.int1 = -1;
    param.FuncType = FIRMWARE_API_FUN;
    param.FuncNo = FIRMWARE_PEDGETKCV;
    param.u_char1 = KeyType;
    param.u_char2 = KeyIndex;
    param.u_char3 = Mode;
    param.u_str1 = Kcv;    
    s_BaseSoEntry(&param);
  
    return param.int1;    
}

static void SetSoPte(uint virtual_addr,uint phy_addr,uint len,uint ttb)
{
	uint addr,offset,pte;
	if(virtual_addr <SO_START) return;
	if((virtual_addr +len) >=SO_END) return;

	pte=phy_addr/0x1000;
	for(addr=virtual_addr;addr <(virtual_addr+len);addr+=0x1000)
	{
		offset=((addr-SO_START)/0x1000)*4;
		/*enable I/D cache,Domain15,4K page,AP_RW_RW*/
		//*(volatile uint *)(ttb+offset) = (pte<<12) |(0xFFE); 
		*(volatile uint *)(ttb+offset) = (pte<<12) |(0xFFE); 
		pte++;
	}
	
	/*clear and invaild TLB */
	__asm("mov r0 ,#0");
	__asm("mcr p15 ,0,r0,c8,c7,0");
    __asm("mcr  p15, 0, r0, c7, c10, 4");  //@ drain write buffer
}


int SoMemSwitch()
{
    volatile uint i,*PT;
    static uint ttb_buf[32],first=1;
    ClearIDCache();
    if(first)
    {
        for(i=0;i<(SO_END-SO_START)/0x100000;i++)
        {
           /* L1 PT 4KB Section entry,DOMAIN15*/ 
           ttb_buf[i]= (SO_L2_BASE+ i*0x400) | 0x1F1; 
           ttb_buf[i]+= 0x8000; 
        }
        first=0;
    }   
    
    PT = (uint*)(TTB_BASE + (SO_START>>20)*4);
    for(i=0;i<(SO_END-SO_START)/0x100000;i++)
    {
        PT[i]=ttb_buf[i];
    }
    /*clear and invaild TLB */
	__asm("mov r0 ,#0");
	__asm("mcr p15 ,0,r0,c8,c7,0");
    __asm("mcr  p15, 0, r0, c7, c10, 4");  //@ drain write buffer
    return 0;
}


int CfgSoMem(uint va,int len,uchar reset)
{
    static uint sub_last_phy=SO_PHY_TOP;
    uint temp_len;

    va = (va/0x1000)*0x1000;
    temp_len = (len + (va%0x1000));
    if(temp_len % 0x1000) temp_len = ((temp_len/0x1000)+1)*0x1000;
    
    if(reset)sub_last_phy=SO_PHY_TOP;
    sub_last_phy-=temp_len;
    SetSoPte(va,sub_last_phy,temp_len,SO_L2_BASE+0x8000); 

    return 0;
}

