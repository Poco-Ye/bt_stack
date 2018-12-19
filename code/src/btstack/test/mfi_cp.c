
#include "base.h"
#include "mfi_cp.h"
#include "simI2C.h"

//#define _MFI_DEBUG_
#ifdef _MFI_DEBUG_
#define log(fmt, ...) do{iDPrintf("<%s><%d>: ", __FUNCTION__, __LINE__);\
	iDPrintf(fmt, ##__VA_ARGS__);}while(0)
#else
#define log
#endif

#if 0
#define MFi_CP_20B
#else
#define MFi_CP_20C
#endif

#ifdef MFi_CP_20B
#define COMMUNICATION_MODE		1  /* CP's pin14 status */
#if (COMMUNICATION_MODE == 1) 
//#define CP_IIC_READ_ADD		(0x23)
#define CP_IIC_WRIET_ADD    	(0x22)
#define CP_IIC_READ_ADD      	(CP_IIC_WRIET_ADD)
#else
//#define CP_IIC_READ_ADD		(0x21)
#define CP_IIC_WRIET_ADD		(0x20)
#define CP_IIC_READ_ADD			(CP_IIC_WRIET_ADD)
#endif
#endif

#ifdef MFi_CP_20C
#define CP_IIC_WRIET_ADD		0x20
#define CP_IIC_READ_ADD			0x20
#endif

typedef struct{
	int sda_port;
	int sda_pin;
	int scl_port;
	int scl_pin;
	int rst_port;
	int rst_pin;
}T_MfiCpDrv;

T_MfiCpDrv *pMfiCpDrv = NULL;

T_MfiCpDrv S900MfiCpDrv = {
	.sda_port = GPIOE,
	.sda_pin = 1,
	.scl_port = GPIOE,
	.scl_pin = 0,
	.rst_port = GPIOB,
	.rst_pin = 25,
};

T_MfiCpDrv D210MfiCpDrv = {
	.sda_port = GPIOD,
	.sda_pin = 5,
	.scl_port = GPIOD,
	.scl_pin = 4,
	.rst_port = GPIOB,
	.rst_pin = 10,
};

T_MfiCpDrv D200MfiCpDrv = {
	.sda_port = GPIOB,
	.sda_pin = 8,
	.scl_port = GPIOB,
	.scl_pin = 7,
	.rst_port = GPIOB,
	.rst_pin = 6,
};

T_MfiCpDrv D200QMfiCpDrv = {
	.sda_port = GPIOD,
	.sda_pin = 13,
	.scl_port = GPIOD,
	.scl_pin = 12,
	.rst_port = GPIOB,
	.rst_pin = 30,
};

/*
#define IPOD_CP_SDA_PORT		(GPIOE)
#define IPOD_CP_SDA_PIN			(1)
#define IPOD_CP_SCL_PORT		(GPIOE)
#define IPOD_CP_SCL_PIN			(0)
#define IPOD_CP_RST_PORT		(GPIOB)
#define IPOD_CP_RST_PIN			(25)
*/
	
#define IPOD_CP_SDA_PORT		pMfiCpDrv->sda_port
#define IPOD_CP_SDA_PIN			pMfiCpDrv->sda_pin
#define IPOD_CP_SCL_PORT		pMfiCpDrv->scl_port
#define IPOD_CP_SCL_PIN			pMfiCpDrv->scl_pin
#define IPOD_CP_RST_PORT		pMfiCpDrv->rst_port
#define IPOD_CP_RST_PIN			pMfiCpDrv->rst_pin


static T_SimI2CHdl tHdlMfiCp;
static T_SimI2CHdl *ptHdlMfiCp = &tHdlMfiCp;

void MfiCpInit(void)
{
	log("SimI2C init...\r\n");
	switch(get_machine_type())
	{
	case D210:
		pMfiCpDrv = &D210MfiCpDrv;
		break;
	case S900:
		if(GetHWBranch() == S900HW_V3x) return;//S900 V30以后不再支持Mfi
		else	pMfiCpDrv = &S900MfiCpDrv;
		break;
	case D200:
		if (ReadBoardVer("MAIN_BOARD") >= 0x30)
		{
			pMfiCpDrv = &D200QMfiCpDrv;
		}
		else
		{
			pMfiCpDrv = &D200MfiCpDrv;
		}
		break;
	default:
		return;
	}
	
	SimI2CInit(ptHdlMfiCp, (IPOD_CP_SDA_PORT<<8)|IPOD_CP_SDA_PIN, (IPOD_CP_SCL_PORT<<8)|IPOD_CP_SCL_PIN);	
	SimI2CSetRetry(ptHdlMfiCp, 10000);
}

void MfiCpClose(void)
{

}

void MfiCpDefaultPinSet(void)
{
	if (pMfiCpDrv == NULL)
		return;
	gpio_set_pin_type(IPOD_CP_SDA_PORT, IPOD_CP_SDA_PIN, GPIO_OUTPUT);	
	gpio_set_pin_type(IPOD_CP_SCL_PORT, IPOD_CP_SCL_PIN, GPIO_OUTPUT);
	gpio_set_pin_type(IPOD_CP_RST_PORT, IPOD_CP_RST_PIN, GPIO_OUTPUT);
	
	gpio_disable_pull(IPOD_CP_SCL_PORT, IPOD_CP_SCL_PIN);	
	gpio_disable_pull(IPOD_CP_SDA_PORT, IPOD_CP_SDA_PIN);	

	gpio_set_pin_val(IPOD_CP_SDA_PORT, IPOD_CP_SDA_PIN, 0);
	gpio_set_pin_val(IPOD_CP_SCL_PORT, IPOD_CP_SCL_PIN, 0);
	gpio_set_pin_val(IPOD_CP_RST_PORT, IPOD_CP_RST_PIN, 0);
}

void MfiCpReset(void)
{
	if (pMfiCpDrv == NULL)
		return;
	gpio_set_pin_type(IPOD_CP_SDA_PORT, IPOD_CP_SDA_PIN, GPIO_OUTPUT);	
	gpio_set_pin_type(IPOD_CP_SCL_PORT, IPOD_CP_SCL_PIN, GPIO_OUTPUT);
	gpio_set_pin_type(IPOD_CP_RST_PORT, IPOD_CP_RST_PIN, GPIO_OUTPUT);

	gpio_disable_pull(IPOD_CP_SCL_PORT, IPOD_CP_SCL_PIN);	
	gpio_disable_pull(IPOD_CP_SDA_PORT, IPOD_CP_SDA_PIN);	

	gpio_set_pin_val(IPOD_CP_SDA_PORT, IPOD_CP_SDA_PIN, 1);
	gpio_set_pin_val(IPOD_CP_SCL_PORT, IPOD_CP_SCL_PIN, 1);
	gpio_set_pin_val(IPOD_CP_RST_PORT, IPOD_CP_RST_PIN, 0);
	DelayUs(10);
	gpio_set_pin_val(IPOD_CP_RST_PORT, IPOD_CP_RST_PIN, 0);
	DelayUs(500);
	gpio_set_pin_val(IPOD_CP_RST_PORT, IPOD_CP_RST_PIN, 0);
	DelayMs(50);	
}

/*
get register data
input:
	addr - register address
	iLen - the data length
output:
	szData - the data buffer
return:
	0 - OK
	<0 - error no
*/
//int MfiCpGetRegData(char addr, char *szData, int iLen)
int MfiCpGetRegData(unsigned char addr, unsigned char *szData, unsigned char iLen)
{
	int iRet;
	
	if (pMfiCpDrv == NULL)
		return -1;
	iRet = SimI2CReadDataFromAddr(ptHdlMfiCp, CP_IIC_READ_ADD, addr, szData, iLen);
	if (iRet != iLen)
	{
		log("read register data failed<%d>, addr = %d", iRet, addr);
		iRet = MFIERR_OPERATE_HW;
	}
	else
	{
		iRet = 0;
	}
	return iRet;
}

/*
set register data
input:
	addr - the register address
	szData - the data buffer
	iLen - the data length
return:
	0 - OK
	<0 - error no
*/
//int MfiCpSetRegData(char addr, char *szData, int iLen)
int MfiCpSetRegData(unsigned char addr, unsigned char *szData, unsigned char iLen)
{
	int iRet;
	
	if (pMfiCpDrv == NULL)
		return -1;
	iRet = SimI2CWriteDataToAddr(ptHdlMfiCp, CP_IIC_WRIET_ADD, addr, szData, iLen);
	if (iRet != iLen)
	{
		log("write register data failed<%d>, addr = %d", iRet, addr);
		iRet = MFIERR_OPERATE_HW;
	}
	else
	{
		iRet = 0;
	}
	return iRet;
}

int MfiCpGetDevVer(char *szBuf)
{
	return MfiCpGetRegData(0x00, szBuf, 1);
}
int MfiCpGetFirmwareVer(char *szBuf)
{
	return MfiCpGetRegData(0x01, szBuf, 1);
}
int MfiCpGetDevID(char *szBuf)
{
	return MfiCpGetRegData(0x04, szBuf, 4);
}
int MfiCpGetMajorVer(char *szBuf)
{
	return MfiCpGetRegData(0x02, szBuf, 1);
}
int MfiCpGetMinorVer(char *szBuf)
{
	return MfiCpGetRegData(0x03, szBuf, 1);
}

// TODO:
#ifdef _MFI_DEBUG_
extern void s_BtPowerSwitch(int on);
void MfiCpTest(void)
{
	unsigned char ucVerId[4] = {0};
	unsigned char ucVal = 0x00;
	int iRet;
	int cnt = 0;
	unsigned char ucKey;

	ScrCls();
	PortOpen(0, "115200,8,n,1");
	ScrPrint(0, 0, 0x01, "CP Test");
	ScrPrint(0, 2, 0x00, "Device Version");
	ScrPrint(0, 3, 0x00, "Firmware Version");
	MfiCpInit();

	s_BtPowerSwitch(1);
	DelayMs(2000);
	
	MfiCpReset();
	if(getkey() == 0x1b)
		return ;
	while(1)
	{
		iDPrintf("\r\n***************** %d *****************\r\n", cnt++);
		iRet = MfiCpGetDevVer(&ucVal);
		iDPrintf("MfiCpGetDevVer <iRet = %d> <Ver = %d>\r\n", iRet, ucVal);
		iRet = MfiCpGetFirmwareVer(&ucVal);
		iDPrintf("MfiCpGetFirmwareVer <iRet = %d> <Ver = %d>\r\n", iRet, ucVal);
		iRet = MfiCpGetMajorVer(&ucVal);
		iDPrintf("MfiCpGetMajorVer <iRet = %d> <Ver = %d>\r\n", iRet, ucVal);
		iRet = MfiCpGetMinorVer(&ucVal);
		iDPrintf("MfiCpGetMinorVer <iRet = %d> <Ver = %d>\r\n", iRet, ucVal);
		iRet = MfiCpGetDevID(ucVerId);
		iDPrintf("MfiCpGetDevID <iRet = %d> <ID: %d %d %d %d>\r\n", iRet, ucVerId[0], ucVerId[1], ucVerId[2], ucVerId[3]);
		//DelayMs(5000);
		if(getkey() == 0x1b)
			break;
	}
	
	getkey();
	
}
#endif
// TODO:


