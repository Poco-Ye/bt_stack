#include "base.h"
#include "../comm/comm.h"
#include "..\lcd\lcdapi.h"
#include "../PUK/puk.h"
#include "../file/filedef.h"
#include <stdarg.h>

extern APPINFO      MonitorInfo;
static int SystemInit_flag=0;
extern uchar k_Charging;


void get_term_name(unsigned char *name_out);
void BatteryChargeEntry(int mode);


void SetMacAddr()
{
	uchar szMacAddr[8],SN[8],TimeBuf[9],szGetMac[32];

	memset(szMacAddr,0,8);
	memset(SN,0,8);
	memset(TimeBuf,0,9);
	memset(szGetMac,0,8);
	
	ReadMacAddr(szMacAddr);
	eth_mac_set(szMacAddr);
}

extern void geth_halt(void);
extern int geth_init(void);
extern void s_initNet(void);
extern int geth_rx (void);

static uchar s_RFType = 0;
uchar get_rftype(void)
{
	return s_RFType;
}

//must be called after the s_WlInit
void s_SetDefaultRoute(void)
{
	if(is_eth_module())
	{
		s_RouteSetDefault(0);
	}
	else if (is_gprs_module() || is_cdma_module() || is_wcdma_module())
	{
		s_RouteSetDefault(11);
	}
	else if(is_wifi_module())
	{
		s_RouteSetDefault(12);
	}
	else
	{
		s_RouteSetDefault(0);
	}
}

unsigned char SystemInit_quick()
{
	uchar ucRet = 0,ucUdisk;	
	uchar pedver[16],TermName[16],ucRfPara[31]; 
	int ret,cfg_init,ret1;
	uint status;
	
	ret = s_GetBootInfo();//读取Boot 版本
    k_Charging = 0;

	nand_init();
	cfg_init = s_CfgInfo_Init();	
	GetHardWareVer();
	s_PowerGpioInit();
	SystemApiInit();
	BatteryChargeEntry(k_Charging);
	s_ScrInit();
	s_SetBatteryEntry();
	//StartLogo();	
	s_FontInit(NULL);
	s_InitINT();
	s_GPIOInit();
	s_AdcInit();	
	s_TimerInit();
	s_CommInit();

	LoadDll();	  
	s_KbInit();
	KbLock(1);
	s_fs_init();
	s_ModemInit(0);
	InitFileSys();/*!!!禁止变动位置*/
	AdjustCfg(cfg_init);
	s_PrnInit();
	s_MagInit();
	s_IccInit();
	//ts初始化必须在文件系统之后
	s_ts_init();
	if(!is_eth_module()) writel_or(0x07,ETH_R_phyctrl_MEMADDR);
	s_initNet();
	s_WlInit();
	s_WifiInit();
	s_SetDefaultRoute();
	//s_BtInit(); //deleted by shics
	memset(ucRfPara, 0x00, sizeof(ucRfPara));	
	ucRet = PcdInit(&s_RFType, ucRfPara);
	if(ucRet)
	{	
		ScrPrint(5, 7, 0x00, "INIT PICC(RF):%x	 ",ucRet);
		DelayMs(1000);
	}
	//AudioVolumeInit();	
	s_ScanInit();
	Tiltsenor_Init();	
	s_ReadSN();
	s_LoadPuk();
	ScrFontSet(1);
	s_GetTamperStatus();
	iPedInit();	
	
	s_ModemInit(1); 
	s_SMInit();
	//s_basecheckinit();	 //deleted by shics
	SystemInit_flag=1;

}

unsigned char SystemInit(void)
{
	uchar ucRet = 0,ucUdisk;	
	uchar pedver[16],TermName[16],ucRfPara[31];	
	int ret,cfg_init,ret1;
    uint status;
    
	ret = s_GetBootInfo();//读取Boot 版本

	nand_init();
	cfg_init = s_CfgInfo_Init();	
	GetHardWareVer();
	s_PowerGpioInit();
	SystemApiInit();
    BatteryChargeEntry(k_Charging);
	s_ScrInit();
	s_SetBatteryEntry();
    StartLogo();	
	s_FontInit(NULL);
	s_InitINT();
	s_GPIOInit();
	s_AdcInit();	
	s_TimerInit();
	s_CommInit();

	LoadDll();    
	s_KbInit();
	KbLock(1);
	if (ret < 0){
		ScrCls();
		ScrPrint(0,3,0, "BOOT VER ERROR");
	}

    if((get_machine_type()==S300)&&(GetVolt()>5500))
    {
        ScrCls();
        ScrPrint(0, 3, 0, "POWER ERROR");
        while(1);
    }

	s_fs_init();
	PortClose(P_USB_DEV);
	ucUdisk=s_uDiskInit();
	s_ModemInit(0);
	InitFileSys();/*!!!禁止变动位置*/
	AdjustCfg(cfg_init);
	s_PrnInit();
	s_MagInit();
	s_IccInit();
	//ts初始化必须在文件系统之后
	s_ts_init();
	if(!is_eth_module()) writel_or(0x07,ETH_R_phyctrl_MEMADDR);
	s_initNet();
	s_WlInit();
	s_WifiInit();
	s_SetDefaultRoute();
	//s_BtInit();//shics deleted
	memset(ucRfPara, 0x00, sizeof(ucRfPara));	
	ucRet = PcdInit(&s_RFType, ucRfPara);
	if(ucRet)
	{	
		ScrPrint(5, 7, 0x00, "INIT PICC(RF):%x   ",ucRet);
		DelayMs(1000);
	}
	AudioVolumeInit();	
	s_ScanInit();
	Tiltsenor_Init();	
    s_ReadSN();
    s_LoadPuk();

    memset(pedver,0x00,sizeof(pedver));
    GetPciVer(pedver);
    memset(TermName, 0x00, sizeof(TermName));
	get_term_name(TermName);
	ScrPrint(5, 6, 0x40, "%s PCI PED %s", TermName,pedver);
	ScrPrint(5,7,0x40,"PCI CERTIFIED");
	status = CheckUpSensor();
	if (!status) s_BBL_init();/*bbl unlock*/
	ScrCls();
	ScrPrint(0,2,0x61,"SELF-TEST");	
	ScrFontSet(1);

	ret1 = s_GetTamperStatus();
	ret = iPedInit();	
	if(ret==0 && ret1==0)
	{
		ret = PedSelfCheckup();
		ScrPrint(0,5,0x61,"SUCCESS");
	}
	s_ModemInit(1); 
	if(ucUdisk) s_uDiskInit();
    mmc_open();
	//testcase();
	s_SMInit();
	//s_basecheckinit();	 //deleted by shics
    SystemInit_flag=1;

}

int SystemInitOver(void)
{
	return SystemInit_flag;
}

int GetLastError(void)
{
	return errno;
}

/*
   S900: 使用动态Adc 检测
   D210: 使用静态Adc 检测外电,使用动态Adc 检测电池 
   S500: 使用动态Adc 检测(  预留)
*/
int GetBatVolt(void)
{
    int vol_value;
    
	vol_value = s_ReadAdc(2);
	return ((vol_value*1200*11)/1024);			/*Vp ,voltage,mv*/	
}
int GetExtVolt(void)
{
	int val, total;
	int i, j;

	total = 0;
	for(i=0,j=0; i<3&&j<10; i++,j++)
	//for(i=0; i<3; i++)
	{
		val = s_ReadStaticAdc(4);
		if (val == 0)
			i--;
		else
			total += val;
		
	}
	if (i <= 0)
		val = 0;
	else
		val = total/i;
	return ((val*1200*11)/1024);
}

int GetVolt(void)
{
    int vol_value;

	if(get_machine_type() == D210)
	{
		vol_value = GetExtVolt();
		if (vol_value < 5000)
			vol_value = GetBatVolt();

		return vol_value;
	}
	else if(get_machine_type() == D200)
	{
		return s_PowerReadBatteryVol(1);
	}
	else
	{    
		vol_value = s_ReadAdc(2);
		return ((vol_value*1200*11)/1024);			/*Vp ,voltage,mv*/	
	}
}


