#include "wlt2564.h"
#include "stdlib.h"
#include "posapi.h"
#include "platform.h"
#include "string.h"
#include "base.h"
#include "LIB_Main.h"

//#define DEBUG_WLT2564
#ifdef DEBUG_WLT2564
#define Display(_x)            do { printf_uart _x; } while(0)
#define BtGetTimerCount()		GetTimerCount()
#else
#define Display(_x)			
#define BtGetTimerCount()		(0)
#endif

#define Null_return				return //do {return ;} while(0)
#define Err_return(_info, _err)	do {Display(("<line:%d> %s <Err:%d>\r\n", __LINE__, _info, _err)); return (_err);} while(0)
#define OK_return(_info, _ok)	do {Display(("<line:%d> %s <OK:%d>\r\n", __LINE__, _info, _ok)); return (_ok); } while(0)

#define BT_FIFO_SIZE			(8192+1)

static unsigned char gauMasterRecvFifo[BT_FIFO_SIZE];
static unsigned char gauMasterSendFifo[BT_FIFO_SIZE];
static unsigned char gauSlaveRecvFifo[BT_FIFO_SIZE];
static unsigned char gauSlaveSendFifo[BT_FIFO_SIZE];

static unsigned int gDebugT1, gDebugT2;
unsigned int gDebugScanTimeT1, gDebugScanTimeT2;

static int giBtMfiCpErr;
static int giBtPowerState;
static int giBtStackInited;
static int giBtDevInited;
static int giBtPortInited;
static volatile int giBtApiMutex = 1;
static volatile tBtCtrlOpt gtBtOpt;
static volatile tBtCtrlOpt *gptBtOpt = &gtBtOpt;
static volatile int gReconnectFlag;
static unsigned char gucBtName[64];
static unsigned char gucBtMac[6];
static unsigned char gucBtVer[8];

extern int MainThread(void *UserParameter);
static int s_BtConnectTimerDone(void);
volatile uchar g_UserBtLinkEnable = 0;

void SetBtLinkEnable(uchar value)
{
	g_UserBtLinkEnable = value;
}

uchar GetBtLinkEnable(void)
{
	return g_UserBtLinkEnable;
}
// 根据不同机型设置与MFi认证相关的token
int getFIDInfo(unsigned char fid[])
{
	struct fid_token{
		unsigned char len; // 长度不包括长度域本身
		unsigned char info[32];	
	};	
	struct fid_token *pToken;
	int i, retLen, tokenCnt;
	int m_type = get_machine_type();

	struct fid_token D200_token[] = {
		{0x0C, 0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x02,0x00},
		{0x0A, 0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00},	
		{0x0D, 0x00,0x02,0x01,'P','A','X',' ','D','2','0','0','A',0x00},
		{0x06, 0x00,0x02,0x04,0x01,0x00,0x00},
		{0x06, 0x00,0x02,0x05,0x01,0x00,0x00},
		{0x07, 0x00,0x02,0x06,'P','A','X',0x00},
		{0x09, 0x00,0x02,0x07,'D','2','0','0','A',0x00},
		{0x05, 0x00,0x02,0x09,0x00,0x80},
		{0x07, 0x00,0x02,0x0C,0x00,0x00,0x00,0x07},
		{0x12, 0x00,0x04,0x01,'c','o','m','.','p','a','x','s','z','.','i','P','O','S',0x00},
		{0x0D, 0x00,0x05,'L','P','J','7','C','N','3','3','R','E',0x00},
	};
	struct fid_token D210_token[] = {
		{0x0C, 0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x02,0x00},
		{0x0A, 0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00},	
		{0x0C, 0x00,0x02,0x01,'P','A','X',' ','D','2','1','0',0x00},
		{0x06, 0x00,0x02,0x04,0x01,0x00,0x00},
		{0x06, 0x00,0x02,0x05,0x01,0x00,0x00},
		{0x07, 0x00,0x02,0x06,'P','A','X',0x00},
		{0x08, 0x00,0x02,0x07,'D','2','1','0',0x00},
		{0x05, 0x00,0x02,0x09,0x00,0x80},
		{0x07, 0x00,0x02,0x0C,0x00,0x00,0x00,0x07},
		{0x12, 0x00,0x04,0x01,'c','o','m','.','p','a','x','s','z','.','i','P','O','S',0x00},
		{0x0D, 0x00,0x05,'L','P','J','7','C','N','3','3','R','E',0x00},
	};    
	struct fid_token S900_token[] = {
		{0x0C, 0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x02,0x00},
		{0x0A, 0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00},	
		{0x0C, 0x00,0x02,0x01,'P','A','X',' ','S','9','0','0',0x00},
		{0x06, 0x00,0x02,0x04,0x01,0x00,0x00},
		{0x06, 0x00,0x02,0x05,0x01,0x00,0x00},
		{0x07, 0x00,0x02,0x06,'P','A','X',0x00},
		{0x08, 0x00,0x02,0x07,'S','9','0','0',0x00},
		{0x05, 0x00,0x02,0x09,0x00,0x80},
		{0x07, 0x00,0x02,0x0C,0x00,0x00,0x00,0x07},
		{0x12, 0x00,0x04,0x01,'c','o','m','.','p','a','x','s','z','.','i','P','O','S',0x00},
		{0x0D, 0x00,0x05,'L','P','J','7','C','N','3','3','R','E',0x00},
	};
	
	switch(m_type)
	{
		case D200:
			pToken   = D200_token;
			tokenCnt = sizeof(D200_token)/sizeof(D200_token[0]);	
			break;
		case D210:
			pToken   = D210_token;
			tokenCnt = sizeof(D210_token)/sizeof(D210_token[0]);	
			break;	
		case S900:
			pToken	 = S900_token;
			tokenCnt = sizeof(S900_token)/sizeof(S900_token[0]);	
			break;	
		default:
			pToken   = D200_token;
			tokenCnt = sizeof(D200_token)/sizeof(D200_token[0]);	
			break;		
	}

	retLen  = 0;
	fid[0]  = tokenCnt;
	retLen += 1;
	for(i = 0; i < tokenCnt; i++)
	{
		memcpy(fid+retLen, (unsigned char *)&pToken[i], pToken[i].len+1);
		retLen += (pToken[i].len + 1);
	}

	return retLen;
}

int getIAP2Info(unsigned char ipa2[])
{
	struct iap2_token {
		unsigned char len[2]; // 长度包括长度域本身
		unsigned char info[64];
	};
	struct iap2_token *pToken;
	int i, tmp, retLen, tokenCnt;
	int m_type = get_machine_type();
	
	struct iap2_token D200_token[] = {
		{0x00,0x0E, 0x00,0x00,'P','A','X',' ','D','2','0','0','A',0x00},
		{0x00,0x0A, 0x00,0x01,'D','2','0','0','A',0x00},
		{0x00,0x08, 0x00,0x02,'P','A','X',0x00},
		{0x00,0x0D, 0x00,0x03,'0','0','0','0','0','0','0','0',0x00}, // 序列号
		{0x00,0x0A, 0x00,0x04,'1','.','0','.','0',0x00},
		{0x00,0x0A, 0x00,0x05,'1','.','0','.','0',0x00},
		{0x00,0x06, 0x00,0x06,0xEA,0x02},
		{0x00,0x08, 0x00,0x07,0xEA,0x00,0xEA,0x01},
		{0x00,0x05, 0x00,0x08,0x00},
		{0x00,0x06, 0x00,0x09,0x00,0x00},
		{0x00,0x21, 0x00,0x0A,0x00,
					0x05,0x00,0x00,0x01,0x00,
					0x13,0x00,0x01,'c','o','m','.','p','a','x','s','z','.','i','P','O','S',0x00,
					0x00,0x05,0x00,0x02,0x00,},
		{0x00,0x07, 0x00,0x0C,'e','n',0x00},
		{0x00,0x07, 0x00,0x0D,'e','n',0x00},
	};

	struct iap2_token D210_token[] = {
		{0x00,0x0D, 0x00,0x00,'P','A','X',' ','D','2','1','0',0x00},
		{0x00,0x09, 0x00,0x01,'D','2','1','0',0x00},
		{0x00,0x08, 0x00,0x02,'P','A','X',0x00},
		{0x00,0x0D, 0x00,0x03,'0','0','0','0','0','0','0','0',0x00}, // 序列号
		{0x00,0x0A, 0x00,0x04,'1','.','0','.','0',0x00},
		{0x00,0x0A, 0x00,0x05,'1','.','0','.','0',0x00},
		{0x00,0x06, 0x00,0x06,0xEA,0x02},
		{0x00,0x08, 0x00,0x07,0xEA,0x00,0xEA,0x01},
		{0x00,0x05, 0x00,0x08,0x00},
		{0x00,0x06, 0x00,0x09,0x00,0x00},
		{0x00,0x21, 0x00,0x0A,0x00,
					0x05,0x00,0x00,0x01,0x00,
					0x13,0x00,0x01,'c','o','m','.','p','a','x','s','z','.','i','P','O','S',0x00,
					0x00,0x05,0x00,0x02,0x00,},
		{0x00,0x07, 0x00,0x0C,'e','n',0x00},
		{0x00,0x07, 0x00,0x0D,'e','n',0x00},
	};
	
	struct iap2_token S900_token[] = {
		{0x00,0x0D, 0x00,0x00,'P','A','X',' ','S','9','0','0',0x00},
		{0x00,0x09, 0x00,0x01,'S','9','0','0',0x00},
		{0x00,0x08, 0x00,0x02,'P','A','X',0x00},
		{0x00,0x0D, 0x00,0x03,'0','0','0','0','0','0','0','0',0x00}, // 序列号
		{0x00,0x0A, 0x00,0x04,'1','.','0','.','0',0x00},
		{0x00,0x0A, 0x00,0x05,'1','.','0','.','0',0x00},
		{0x00,0x06, 0x00,0x06,0xEA,0x02},
		{0x00,0x08, 0x00,0x07,0xEA,0x00,0xEA,0x01},
		{0x00,0x05, 0x00,0x08,0x00},
		{0x00,0x06, 0x00,0x09,0x00,0x00},
		{0x00,0x21, 0x00,0x0A,0x00,
					0x05,0x00,0x00,0x01,0x00,
					0x13,0x00,0x01,'c','o','m','.','p','a','x','s','z','.','i','P','O','S',0x00,
					0x00,0x05,0x00,0x02,0x00,},
		{0x00,0x07, 0x00,0x0C,'e','n',0x00},
		{0x00,0x07, 0x00,0x0D,'e','n',0x00},
	};

	switch(m_type)
	{
		case D200:
			pToken   = D200_token;
			tokenCnt = sizeof(D200_token)/sizeof(D200_token[0]);	
			break;
		case D210:
			pToken   = D210_token;
			tokenCnt = sizeof(D210_token)/sizeof(D210_token[0]);	
			break;	
		case S900:
			pToken	 = S900_token;
			tokenCnt = sizeof(S900_token)/sizeof(S900_token[0]);	
			break;	
		default:
			pToken   = D200_token;
			tokenCnt = sizeof(D200_token)/sizeof(D200_token[0]);	
			break;		
	}

	retLen = 0;
	for(i = 0; i < tokenCnt; i++)
	{
		tmp = ((int)pToken[i].len[0] << 8) + pToken[i].len[1];
		memcpy(ipa2+retLen, (unsigned char *)&pToken[i], tmp);
		retLen += tmp;
	}
	
	return retLen;
}

/**
Bt profile save and load api
*/
#define BT_PROFILE "btprofile"
static uchar gucBtProfileTemp[2048];
volatile static int giBtProfileUpdateFlag = 0;
enum
{
	BT_NAME=0,
	BT_PIN,
	BT_PAIRDATA,
};


#define WLT_BT_PAIRDATA_MAX_LEN	(1024)
static uchar gucBtPairData[WLT_BT_PAIRDATA_MAX_LEN];

/**
 *two functions below just be called by lib 
 */
int WLT_HAL_NV_DataWrite(int Length, unsigned char *Buffer)
{
	if(Length < 0 || Length > WLT_BT_PAIRDATA_MAX_LEN || Buffer == NULL)
		return -1;
	
	Display(("%s, Len: %d\r\n", __FUNCTION__, Length));
	giBtApiMutex = 1;
	memcpy(gucBtPairData, Buffer, Length);
	giBtProfileUpdateFlag = 1;
	giBtApiMutex = 0;
	
	return Length;
}

int WLT_HAL_NV_DataRead(int Length, unsigned char *Buffer)
{
	if(Length < 0 || Length > WLT_BT_PAIRDATA_MAX_LEN || Buffer == NULL)
		return -1;

	
	Display(("%s, Len: %d\r\n", __FUNCTION__, Length));
	giBtApiMutex = 1;
	memcpy(Buffer, gucBtPairData,Length);
	giBtApiMutex = 0;
	
	return Length;
}

int s_WltSetConfigFile(int index, uchar *data)
{
	int fd;
	fd = s_open(BT_PROFILE, O_RDWR, (uchar *)"\xff\x05");
	if (fd < 0)
	{
		return -1;
	}
	if (index == BT_NAME)
	{
		memcpy(gucBtProfileTemp, data,BT_NAME_MAX_LEN);
	}
	else if (index == BT_PIN)
	{
		memcpy(gucBtProfileTemp+512, data, BT_PIN_MAX_LEN);
	}
	else if (index == BT_PAIRDATA)
	{
		memcpy(gucBtProfileTemp+1024, data, WLT_BT_PAIRDATA_MAX_LEN);
	}
	write(fd, gucBtProfileTemp, sizeof(gucBtProfileTemp));
	close(fd);
	return 0;
}
int s_WltGetConfigFile(int index, uchar *data)
{
	int fd;
	
	fd = s_open(BT_PROFILE, O_RDWR, (uchar *)"\xff\x05");
	if (fd < 0)
	{
		int iRet;
		char Temp[BT_NAME_MAX_LEN];
		char TermName[16];
		BD_ADDR_t Local_BD_ADDR;
		//file not exist
		fd = s_open(BT_PROFILE, O_CREATE, (uchar *)"\xff\x05");
		if (fd < 0)
			return -1;
		
		seek(fd, 0, SEEK_SET);
		memset(gucBtProfileTemp, 0, sizeof(gucBtProfileTemp));
		memset(TermName, 0, sizeof(TermName));
		memset(Temp, 0, sizeof(Temp));
		get_term_name(TermName);
		if((iRet = GetLocalAddress(&Local_BD_ADDR)) == 0x00)
		{
			sprintf(Temp, "PAX_%s_%02X%02X", TermName, Local_BD_ADDR.BD_ADDR1, Local_BD_ADDR.BD_ADDR0);
			memcpy(gucBtProfileTemp, Temp, sizeof(Temp));
		}
		else
		{
			sprintf(Temp, "PAX_%s_BT", TermName);
			memcpy(gucBtProfileTemp, Temp, sizeof(Temp));
		}
		memset(Temp, 0, sizeof(Temp));
		strcpy(Temp, "0000");//PIN
		memcpy(gucBtProfileTemp+512, Temp, sizeof(Temp));
		write(fd, gucBtProfileTemp, sizeof(gucBtProfileTemp));
		close(fd);
	}
	else
	{
		seek(fd, 0, SEEK_SET);
		read(fd, gucBtProfileTemp, sizeof(gucBtProfileTemp));
		close(fd);
	}
	if (index == BT_NAME)
	{
		memcpy(data, gucBtProfileTemp, BT_NAME_MAX_LEN);
	}
	else if (index == BT_PIN)
	{
		memcpy(data, gucBtProfileTemp+512, BT_PIN_MAX_LEN);
	}
	else if (index == BT_PAIRDATA)
	{
		memcpy(data, gucBtProfileTemp+1024, WLT_BT_PAIRDATA_MAX_LEN);
	}
	return 0;
}

typedef struct{
	int rst_port;
	int rst_pin;
	int pwr_port;
	int pwr_pin;
	void (*s_BtPowerSwitch)(int);
}T_BtDrvCfg;

void s_BtPowerSwitch_Sxxx(int on);
void s_BtPowerSwitch_D200(int on);
T_BtDrvCfg D210BtCfg = {
	.rst_port = GPIOB,
	.rst_pin = 11,
	.pwr_port = GPIOB,
	.pwr_pin = 9,
	.s_BtPowerSwitch = s_BtPowerSwitch_Sxxx,
};

T_BtDrvCfg D200BtCfg = {
	.rst_port = GPIOE,
	.rst_pin = 2,
	.pwr_port = -1,
	.pwr_pin = -1,
	.s_BtPowerSwitch = s_BtPowerSwitch_D200,
};

T_BtDrvCfg S900BtCfg = {
	.rst_port = GPIOA,
	.rst_pin = 3,
	.pwr_port = GPIOB,
	.pwr_pin = 7,
	.s_BtPowerSwitch = s_BtPowerSwitch_Sxxx,
};
T_BtDrvCfg S900V3BtCfg = {
	.rst_port = GPIOD,
	.rst_pin = 17,
	.pwr_port = GPIOD,
	.pwr_pin = 16,
	.s_BtPowerSwitch = s_BtPowerSwitch_Sxxx,
};


T_BtDrvCfg *pBtDrvCfg = NULL;
tBtCtrlOpt *getBtOptPtr(void);

// TODO: just for WLT2564
/*
#define BT_RESET_PORT 		GPIOA
#define BT_RESET_PIN		(3)
#define BT_POWER_PORT		GPIOB
#define BT_POWER_PIN		(7)
*/

#define BT_RESET_PORT 		pBtDrvCfg->rst_port
#define BT_RESET_PIN		pBtDrvCfg->rst_pin
#define BT_POWER_PORT		pBtDrvCfg->pwr_port
#define BT_POWER_PIN		pBtDrvCfg->pwr_pin

void s_BtRstSetPinVal(int val)
{
	int v = (val == 0 ? 0 : 1);
	gpio_set_pin_val(BT_RESET_PORT, BT_RESET_PIN, v);
}

void s_BtRstSetPinType(int type)
{
	GPIO_PIN_TYPE t = (type == 0 ? GPIO_INPUT : GPIO_OUTPUT);
	gpio_set_pin_type(BT_RESET_PORT, BT_RESET_PIN, t);
}

void s_BtPowerSwitch_Sxxx(int on)
{
	int flag = 0;
	int v = (on == 1 ? 0 : 1);
	
	gpio_set_pin_type(BT_RESET_PORT, BT_RESET_PIN, GPIO_OUTPUT);
	gpio_set_pin_val(BT_RESET_PORT, BT_RESET_PIN, 0);
	DelayMs(10);
	if(flag == 0)
	{
		gpio_set_pin_type(BT_POWER_PORT, BT_POWER_PIN, GPIO_OUTPUT);
		flag = 1;
	}
	gpio_set_pin_val(BT_POWER_PORT, BT_POWER_PIN, v);
}
void s_BtPowerSwitch_D200(int on)
{
	gpio_set_pin_type(BT_RESET_PORT, BT_RESET_PIN, GPIO_OUTPUT);
	gpio_set_pin_val(BT_RESET_PORT, BT_RESET_PIN, 0);
	DelayMs(10);
	s_PmuBtPowerSwitch(on);
}
void s_BtPowerSwitch(int on)
{
	if (pBtDrvCfg != NULL)
		pBtDrvCfg->s_BtPowerSwitch(on);
}

int is_wlt2564(void)
{
    return (get_bt_type() == 1);
}

int get_bt_type(void)
{
	char context[64];
	static int bt_type = 0;
	static int get_flag = 0;
	
	if(get_flag) return bt_type;
    memset(context,0,sizeof(context));
    if(ReadCfgInfo("BLUE_TOOTH",context) > 0)
    {
		if(strstr(context, "01"))
		{
			bt_type = 1;			// WLT2564
		}
		else if(strstr(context, "03"))
		{
			bt_type = 3;			// AP6212
		}
		else if(strstr(context, "04"))
		{
			bt_type = 4;			// AP6105
		}
		else
		{
			bt_type = 0;
		}
	}
	get_flag = 1;
    return bt_type;	
}

static void s_BtIconUpdate(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	ScrSetIcon(ICON_BT, giBtDevInited && (ptBtOpt->slaveConnected || ptBtOpt->masterConnected ==1));
//	ScrSetIcon(ICON_LOCK, giBtDevInited && ptBtOpt->masterConnected);
//	ScrSetIcon(ICON_ICCARD, ptBtOpt->masterConnected == 2);
}

void s_BtTimerDone(void)
{
	unsigned int t1, t2, t3;

	if (is_wlt2564() == 0) return;

	s_BtIconUpdate();

	if(giBtApiMutex == 1) return;
	
	t1 = BtGetTimerCount();
	BTPS_ProcessScheduler();
	t2 = BtGetTimerCount();
	
	s_BtConnectTimerDone();	
	
	if(t2 - t1 >= 10)
		Display(("++++++++++++ BTPS_ProcessScheduler <Warning: TimeDone(%dms)>\r\n", t2-t1));
}

tBtCtrlOpt *getBtOptPtr(void)
{
	return (tBtCtrlOpt *)gptBtOpt;
}

void s_DisplayWltAddr(char *title, BD_ADDR_t *addr)
{
	Display(("%s: %02x:%02x:%02x:%02x:%02x:%02x\r\n", title, 
		addr->BD_ADDR5, addr->BD_ADDR4, 
		addr->BD_ADDR3, addr->BD_ADDR2, 
		addr->BD_ADDR1, addr->BD_ADDR0));
}

void s_DisplayWltMac(char *title, unsigned char mac[6])
{
	Display(("%s: %02x:%02x:%02x:%02x:%02x:%02x\r\n", title,
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
}

void s_WltSetAddrToMac(unsigned char mac[6], BD_ADDR_t *addr)
{
	mac[0] = addr->BD_ADDR5;
	mac[1] = addr->BD_ADDR4;
	mac[2] = addr->BD_ADDR3;
	mac[3] = addr->BD_ADDR2;
	mac[4] = addr->BD_ADDR1;
	mac[5] = addr->BD_ADDR0;
}

void s_WltSetMacToAddr(unsigned char mac[6], BD_ADDR_t *addr)
{
	addr->BD_ADDR5 = mac[0];
	addr->BD_ADDR4 = mac[1];
	addr->BD_ADDR3 = mac[2];
	addr->BD_ADDR2 = mac[3];
	addr->BD_ADDR1 = mac[4];
	addr->BD_ADDR0 = mac[5];
}

void s_WltSetAddrToAddr(BD_ADDR_t *dst, BD_ADDR_t *src)
{
	dst->BD_ADDR5 = src->BD_ADDR5;
	dst->BD_ADDR4 = src->BD_ADDR4;
	dst->BD_ADDR3 = src->BD_ADDR3;
	dst->BD_ADDR2 = src->BD_ADDR2;
	dst->BD_ADDR1 = src->BD_ADDR1;
	dst->BD_ADDR0 = src->BD_ADDR0;
}

void s_WltSetBtPin(char *pin)
{
	strcpy(gptBtOpt->LocalPin, pin);
}
void s_WltGetBtPin(char *pin)
{
	strcpy(pin, gptBtOpt->LocalPin);
}
void s_WltSetBtName(char *name)
{
	strcpy(gptBtOpt->LocalName, name);
}
void s_WltGetBtName(char *name)
{
	strcpy(name, gptBtOpt->LocalName);
}

int s_BtPowerOn(void)
{
	int i, err = 0;
	unsigned char ucVal = 0xff;
	unsigned char buf[100];
	memset(buf, 0xff, sizeof(buf));
	if(giBtPowerState == 0)
	{
		// TODO: poweron timing
		//MfiCpOpenPinSet();
		MfiCpDefaultPinSet();
		s_BtPowerSwitch(1);
		if(get_machine_type()==S900 && (GetHWBranch()==S900HW_V3x))//S900 V30以后不再支持Mfi
		{
			giBtPowerState = 1;
		}
		else
		{
			DelayMs(200);
			gDebugT1 = BtGetTimerCount();
			//DelayMs(200);
			for(i = 0; i < 3; i++)
			{
				if(MfiCpGetDevVer(&ucVal) == 0 && MfiCpGetDevID(buf) == 0)
				{
					Display(("DevVer:%02x, DevId:%02x %02x %02x %02x\r\n", ucVal, buf[0] , buf[1], buf[2], buf[3]));
					break;
				}
				else
					DelayMs(100);
			}
			if(i >= 3)
			{
				// FixMe
				Display(("<ERR:%d> Mfi CP Init Err!!\r\n", __LINE__));
				err = -1;
			}
			gDebugT2 = BtGetTimerCount();
			Display(("Mfi Cp Init <Time: %dms>\r\n", gDebugT2-gDebugT1));
			/* */
			giBtPowerState = 1;
		}
	}
	return err;
}

void s_BtPowerOff(void)
{
	if(giBtPowerState == 1)
	{
		// TODO: poweroff timing
		gDebugT1 = BtGetTimerCount();
		giBtApiMutex = 1;
		DeInitializeApplication();
		giBtApiMutex = 0;
		gDebugT2 = BtGetTimerCount();
		Display(("DeInitializeApplication <Time: %d ms>\r\n", gDebugT2-gDebugT1));
		DelayMs(100);
		s_BtRstSetPinType(0);
		s_BtRstSetPinVal(0);
		DelayMs(100);
		s_BtPowerSwitch(0);
		//MfiCpClosePinSet();
		MfiCpDefaultPinSet();
		/* */
		giBtPowerState = 0;
		giBtDevInited = 0;
		giBtStackInited = 0;
		giBtApiMutex = 1;
	}
}

int s_IsBtPowerOn(void)
{
	return (giBtPowerState == 1);
}

void s_WltBtInit(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
		
	if(!is_wlt2564())
		return ;

	switch(get_machine_type())
	{
	case D210:
		pBtDrvCfg = &D210BtCfg;
		break;
	case D200:
		pBtDrvCfg = &D200BtCfg;
		break;
	case S900:
		if(GetHWBranch() == S900HW_V3x) pBtDrvCfg = &S900V3BtCfg;
		else pBtDrvCfg = &S900BtCfg;
		break;
	default:
		return;
	}
	
	//s_BtPowerSwitch(1);
	
	MfiCpInit();

	giBtDevInited = 0;
	giBtPortInited = 0;
	
	memset((char *)ptBtOpt, 0x00, sizeof(tBtCtrlOpt));

	ptBtOpt->isServerEnd = 1;
	ptBtOpt->eConnectType = eConnected_None;

//	QueInit((T_Queue *)(&ptBtOpt->tRecvQue), gauBtRecvFifo, sizeof(gauBtRecvFifo));
//	QueInit((T_Queue *)(&ptBtOpt->tSendQue), gauBtSendFifo, sizeof(gauBtSendFifo));
// TODO:
	QueInit((T_Queue *)(&ptBtOpt->tMasterRecvQue), gauMasterRecvFifo, sizeof(gauMasterRecvFifo));
	QueInit((T_Queue *)(&ptBtOpt->tMasterSendQue), gauMasterSendFifo, sizeof(gauMasterSendFifo));
	QueInit((T_Queue *)(&ptBtOpt->tSlaveRecvQue), gauSlaveRecvFifo, sizeof(gauSlaveRecvFifo));
	QueInit((T_Queue *)(&ptBtOpt->tSlaveSendQue), gauSlaveSendFifo, sizeof(gauSlaveSendFifo));
}

int s_BtGetInfo(void)
{
	int iRet;
	Bluetooth_Stack_Version_t version;
	static char flag;

	if(flag) return 0;
	iRet = BtOpen();
	if(iRet) 
	{
		BtClose();
		return -1;
	}
	memset(gucBtName,0,sizeof(gucBtName));
	s_WltGetBtName(gucBtName);
	memset(gucBtMac,0,sizeof(gucBtMac));
	s_WltGetLocalMac(gucBtMac);
	memset(gucBtVer,0,sizeof(gucBtVer));
	GetBluetoothStackVersion(&version);
	sprintf(gucBtVer,"V%d.%d.%d",version.CustomerVersion[0],version.CustomerVersion[1],version.CustomerVersion[2]);
	BtClose();
	flag = 1;
	return 0;
}

int s_BtGetName(unsigned char *name)
{
	if(!s_BtGetInfo())
	{
		strcpy(name,gucBtName);
		return 0;
	}
	return -1;
}

int s_BtGetMac(unsigned char *mac)
{
	if(!s_BtGetInfo())
	{
		memcpy(mac,gucBtMac,6);
		return 0;
	}
	return -1;
}

int s_BtGetVer(unsigned char *ver)
{
	if(!s_BtGetInfo())
	{
		strcpy(ver,gucBtVer);
		return 0;
	}
	return -1;
}

int s_WltSetBtAbilityMode(int enable)
{
	int iRet;
	giBtApiMutex = 1;
	if(enable)
	{
		gDebugT1 = BtGetTimerCount();
		iRet = SetDiscoverabilityMode(2); /* 2:GeneralDiscoverableMode */
		gDebugT2 = BtGetTimerCount();
		if(iRet != 0)
		{
			// FixMe:
			Display(("<ERROR:%d>SetDiscoverabilityMode(2) <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
		}
		else
		{
			Display(("SetDiscoverabilityMode(2) <OK, TimeDone:%dms\r\n", gDebugT2-gDebugT1));
		}

		gDebugT1 = BtGetTimerCount();
		iRet = SetPairabilityMode(1); /* 0:pmNonPairableMode, 1:pmPairableMode, 2:pmPairableMode_EnableSecureSimplePairing */
		gDebugT2 = BtGetTimerCount();
		if(iRet != 0)
		{
			// FixMe:
			Display(("<ERROR:%d>SetPairabilityMode(1) <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
		}
		else
		{
			Display(("SetPairabilityMode(1) <OK, TimeDone:%dms\r\n", gDebugT2-gDebugT1));
		}

		gDebugT1 = BtGetTimerCount();
		iRet = SetConnectabilityMode(1); /* 1:ConnectableMode */
		gDebugT2 = BtGetTimerCount();
		if(iRet != 0)
		{
			// FixMe:
			Display(("<ERROR:%d>SetConnectabilityMode(1) <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
		}
		else
		{
			Display(("SetConnectabilityMode(1) <OK, TimeDone:%dms\r\n", gDebugT2-gDebugT1));
		}
	}
	else
	{
		gDebugT1 = BtGetTimerCount();
		iRet = SetDiscoverabilityMode(0); /* LimiteDiscoverableMode */
		gDebugT2 = BtGetTimerCount();
		if(iRet != 0)
		{
			// FixMe:
			Display(("<ERROR:%d>SetDiscoverabilityMode(0) <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
		}
		else
		{
			Display(("SetDiscoverabilityMode(0) <OK, TimeDone:%dms\r\n", gDebugT2-gDebugT1));
		}

		gDebugT1 = BtGetTimerCount();
		iRet = SetPairabilityMode(0); /* NonPairableMode */
		gDebugT2 = BtGetTimerCount();
		if(iRet != 0)
		{
			// FixMe:
			Display(("<ERROR:%d>SetPairabilityMode(0) <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
		}
		else
		{
			Display(("SetPairabilityMode(0) <OK, TimeDone:%dms\r\n", gDebugT2-gDebugT1));
		}
		gDebugT1 = BtGetTimerCount();
		iRet = SetConnectabilityMode(0); /* NonConnectableMode */
		gDebugT2 = BtGetTimerCount();
		if(iRet != 0)
		{
			// FixMe:
			Display(("<ERROR:%d>SetConnectabilityMode(0) <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
		}
		else
		{
			Display(("SetConnectabilityMode(0) <OK, TimeDone:%dms\r\n", gDebugT2-gDebugT1));
		}
	}
	giBtApiMutex = 0;
}

void s_WltProfileOpen(unsigned int mask)
{
	int iRet;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	giBtApiMutex = 1;
	if(mask & WLT_PROFILE_SPP)
	{
		if(ptBtOpt->SPPProfileOpenCnt == 0)
		{
			gDebugT1 = BtGetTimerCount();
			iRet = SPPProfileOpen(1);
			gDebugT2 = BtGetTimerCount();
			if(iRet > 0)
			{
				Display(("SPPProfileOpen <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
				ptBtOpt->LocalSerialPortID = iRet;
				ptBtOpt->SPPProfileOpenCnt++;
				Display(("SPPProfileOpen Success! <SerialPortID = %d>\r\n", iRet));
			}
			else
			{
				// FixMe
				Display(("<ERROR:%d>SPPProfileOpen <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
			}
		}
		else
		{
			Display(("<Warning:%d>gptBtOpt->SPPProfileOpenCnt = %d\r\n", __LINE__, ptBtOpt->SPPProfileOpenCnt));
		}
	}
	// SPPLE Profile 注册SPPLE Profile, 但是此时没有广播Service，此时，其他设备无法搜索到。
	if(mask & WLT_PROFILE_SPPLE)
	{
		if(ptBtOpt->SPPLEProfileRegisterCnt == 0)
		{
			gDebugT1 = BtGetTimerCount();
			iRet = SPPLEProfileRegister();
			gDebugT2 = BtGetTimerCount();
			if(iRet != 0)
			{
				// FixMe
				Display(("<ERROR:%d>SPPLEProfileRegister <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));	
			}
			else
			{
				Display(("SPPLEProfileRegister <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
				ptBtOpt->SPPLEProfileRegisterCnt++;
			}
		}
		else
		{
			Display(("<Warning:%d>gptBtOpt->SPPLEProfileRegisterCnt = %d\r\n", __LINE__, ptBtOpt->SPPLEProfileRegisterCnt));
		}
	}
	// BLE 打开BLE广播，可被其他设备搜到
	if(mask & WLT_PROFILE_BLE)
	{
		if(ptBtOpt->BLEAdvertiseStartCnt == 0)
		{
			gDebugT1 = BtGetTimerCount();
			iRet = BLEAdvertiseStart();
			gDebugT2 = BtGetTimerCount();
			if(iRet != 0)
			{
				// FixMe
				Display(("<ERROR:%d>BLEAdvertiseStart <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
			}
			else
			{
				Display(("BLEAdvertiseStart <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
				ptBtOpt->BLEAdvertiseStartCnt++;
			}
		}
		else
		{
			Display(("<Warning:%d>gptBtOpt->BLEAdvertiseStartCnt = %d\r\n", __LINE__, ptBtOpt->BLEAdvertiseStartCnt));	
		}
	}
	// ISPP profile
	if(mask & WLT_PROFILE_ISPP)
	{
		if(ptBtOpt->ISPPProfileOpenCnt == 0)
		{		
			gDebugT1 = BtGetTimerCount();
			iRet = ISPPProfileOpen(9);
			gDebugT2 = BtGetTimerCount();
			if(iRet != 0)
			{
				// FixMe
				Display(("<ERROR:%d>ISPPProfileOpen <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
			}
			else
			{
				Display(("ISPPProfileOpen <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
				ptBtOpt->ISPPProfileOpenCnt++;
			}
		}
		else
		{
			Display(("<Warning:%d>gptBtOpt->ISPPProfileOpenCnt = %d\r\n", __LINE__, ptBtOpt->ISPPProfileOpenCnt));	
		}
	}
	giBtApiMutex = 0;
}

void s_WltProfileClose(unsigned int mask)
{
	int iRet;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	giBtApiMutex = 1;
	// SPP profile
	if(mask & WLT_PROFILE_SPP)
	{
		if(ptBtOpt->SPPProfileOpenCnt > 0)
		{
			if(ptBtOpt->SPPProfileOpenCnt != 1)
			{
				Display(("<Warning:%d>gptBtOpt->SPPProfileOpenCnt = %d\r\n", __LINE__, ptBtOpt->SPPProfileOpenCnt));
			}
			gDebugT1 = BtGetTimerCount();
			iRet = SPPProfileCloseByNumbers(ptBtOpt->LocalSerialPortID);
			gDebugT2 = BtGetTimerCount();
			if(iRet != 0)
			{
				Display(("<ERROR:%d>SPPProfileCloseByNumbers(%d) <StackErr:%d,, TimeDone:%dms>\r\n", 
					__LINE__, ptBtOpt->LocalSerialPortID, iRet, gDebugT2-gDebugT1));
			}
			else
			{
				Display(("SPPProfileCloseByNumbers <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
				ptBtOpt->SPPProfileOpenCnt--;
			}
		}
		else
		{
			Display(("<Warning:%d>gptBtOpt->SPPProfileOpenCnt = %d\r\n", __LINE__, ptBtOpt->SPPProfileOpenCnt));
		}
	}
	// SPPLE profile 注销SPPLE Profile
	if(mask & WLT_PROFILE_SPPLE)
	{
		if(ptBtOpt->SPPLEProfileRegisterCnt > 0)
		{
			if(ptBtOpt->SPPLEProfileRegisterCnt != 1)
			{
				Display(("<Warning:%d>gptBtOpt->SPPLEProfileRegisterCnt = %d\r\n", __LINE__, ptBtOpt->SPPLEProfileRegisterCnt));
			}
			gDebugT1 = BtGetTimerCount();
			iRet = SPPLEProfileUnRegister();
			gDebugT2 = BtGetTimerCount();
			if(iRet != 0)
			{
				Display(("<ERROR:%d>SPPLEProfileUnRegister <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
			}
			else
			{
				Display(("SPPLEProfileUnRegister <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
				ptBtOpt->SPPLEProfileRegisterCnt--;
			}
		}
		else
		{
			Display(("<Warning:%d>gptBtOpt->SPPLEProfileRegisterCnt = %d\r\n", __LINE__, ptBtOpt->SPPLEProfileRegisterCnt));
		}
	}
	// BLE 关闭BLE设备广播,其他设备搜索不到
	// 一旦BLE被连接上之后，BLE广播就会自动关闭,这是蓝牙联盟规定这么做的。所以这里并不需要再关闭.
	// 但是为了s_WltProfileOpen能再打开BLE广播，这里把gptBtOpt->BLEAdvertiseStartCnt计数值减1
	// 需要时再调用BLEAdvertiseStart再次打开广播
	if(mask & WLT_PROFILE_BLE)
	{
		if(ptBtOpt->eConnectType == eConnected_SPPLE)
		{
			if(ptBtOpt->BLEAdvertiseStartCnt > 0)
			{
				if(ptBtOpt->BLEAdvertiseStartCnt != 1)
				{
					Display(("<Warning:%d>gptBtOpt->BLEAdvertiseStartCnt = %d\r\n", __LINE__, ptBtOpt->BLEAdvertiseStartCnt));
				}
				gDebugT1 = BtGetTimerCount();
				iRet = 0; //BLEAdvertiseStop();
				gDebugT2 = BtGetTimerCount();
				if(iRet != 0)
				{
					Display(("<ERROR:%d>BLEAdvertiseStop <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
				}
				else
				{
					Display(("BLEAdvertiseStop <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
					ptBtOpt->BLEAdvertiseStartCnt--;
				}
			}
			else
			{
				Display(("<Warning:%d>gptBtOpt->BLEAdvertiseStartCnt = %d\r\n", __LINE__, ptBtOpt->BLEAdvertiseStartCnt));
			}		

		}
		else
		{
			if(ptBtOpt->BLEAdvertiseStartCnt > 0)
			{
				if(ptBtOpt->BLEAdvertiseStartCnt != 1)
				{
					Display(("<Warning:%d>gptBtOpt->BLEAdvertiseStartCnt = %d\r\n", __LINE__, ptBtOpt->BLEAdvertiseStartCnt));
				}
				gDebugT1 = BtGetTimerCount();
				iRet = BLEAdvertiseStop();
				gDebugT2 = BtGetTimerCount();
				if(iRet != 0)
				{
					Display(("<ERROR:%d>BLEAdvertiseStop <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
				}
				else
				{
					Display(("BLEAdvertiseStop <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
					ptBtOpt->BLEAdvertiseStartCnt--;
				}
			}
			else
			{
				Display(("<Warning:%d>gptBtOpt->BLEAdvertiseStartCnt = %d\r\n", __LINE__, ptBtOpt->BLEAdvertiseStartCnt));
			}		
		}
	}
	// ISPP profile
	if(mask & WLT_PROFILE_ISPP)
	{
		if(ptBtOpt->ISPPProfileOpenCnt > 0)
		{
			if(ptBtOpt->ISPPProfileOpenCnt != 1)
			{
				Display(("<Warning:%d>gptBtOpt->ISPPProfileOpenCnt = %d\r\n", __LINE__, ptBtOpt->ISPPProfileOpenCnt));
			}
			gDebugT1 = BtGetTimerCount();
			iRet = ISPPProfileClose();
			gDebugT2 = BtGetTimerCount();
			if(iRet != 0)
			{
				Display(("<ERROR:%d>ISPPProfileClose <Stack_Err:%d, TimeDone:%dms>\r\n", __LINE__, iRet, gDebugT2-gDebugT1));
			}
			else
			{
				Display(("ISPPProfileClose <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
				ptBtOpt->ISPPProfileOpenCnt--;
			}
		}
		else
		{
			Display(("<Warning:%d>gptBtOpt->ISPPProfileOpenCnt = %d\r\n", __LINE__, ptBtOpt->ISPPProfileOpenCnt));
		}
	}
	giBtApiMutex = 0;
}


int s_WltGetConnectType(void)
{
	return gptBtOpt->eConnectType;
}
int s_WltGetISPPSessionStatus(void)
{
	return gptBtOpt->ISPP_Session_state;
}

int s_WltGetLocalMac(unsigned char *pLoaclMac)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(ptBtOpt->Local_BD_ADDR.BD_ADDR5 == 0x00 
		&& ptBtOpt->Local_BD_ADDR.BD_ADDR4 == 0x00
		&& ptBtOpt->Local_BD_ADDR.BD_ADDR3 == 0x00
		&& ptBtOpt->Local_BD_ADDR.BD_ADDR2 == 0x00
		&& ptBtOpt->Local_BD_ADDR.BD_ADDR1 == 0x00
		&& ptBtOpt->Local_BD_ADDR.BD_ADDR0 == 0x00)
		return -1;
	s_WltSetAddrToMac(pLoaclMac, &ptBtOpt->Local_BD_ADDR);

	return 0;
}

void s_WltClearInquiryCnt(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	ptBtOpt->tInqResult.iCnt = 0;
}
int s_WltGetInquiryCnt(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	return ptBtOpt->tInqResult.iCnt;
}

int s_WltGetInquiryDev(unsigned char *pMac, int iCount)
{
	int i;
	int iCnt = s_WltGetInquiryCnt();
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	Display(("s_WltGetInquiryCnt <iRet = %d>\r\n", iCnt));
	for(i = 0; i < iCnt && i < iCount; i++)
	{
		s_WltSetAddrToMac(pMac+i*6, &ptBtOpt->tInqResult.Device[i].BD_ADDR);
	}
	return i;
}

/*
 * iCmd:
 * 0 : Upload base bt info
 * 1 : SetDiscoverabilityMode
 * 2 : SetPairabilityMode
 * 3 : SetConnectabilityMode
 * 4 : ConnectionSetSSPParameters
 * 5 : UserConfirmationResponse
 * 6 : PassKeyResponse
 * 7 : GetPairStatus...@_@...
 * 8 : Clear Pair Data
 */
int WltBtCtrl(tBtCtrlOpt *pOpt, unsigned int iCmd, 
	void *pArgIn,  unsigned int iSizeIn,
	void *pArgOut, unsigned int iSizeOut)
{
	int iRet = -255;
	unsigned int mode;
	int passkey;
	BD_ADDR_t Remote_BD_ADDR;
	unsigned int *tmp = (unsigned int *)pArgOut;
	uchar *tmpc;

	switch(iCmd)
	{
		case 7:
			if(!is_hasbase())
			{
				iRet = BT_RET_ERROR_NOBASE;				// 没有座机
				break;
			}
			tmpc = (uchar *)pArgOut;
			iRet = 0;
			iRet = BtCtrl_GetBaseBt(tmpc,43);
			if(iRet) iRet = BT_RET_ERROR_BASEINFO;		// 获取座机蓝牙信息失败
			break;
		
		default:
			break;
	}
	return iRet;
}

// TODO: Bt API

static int s_BtConnectStep_00(tBtCtrlOpt *opt);
static int s_BtConnectStep_01(tBtCtrlOpt *opt);
static int s_BtConnectStep_02(tBtCtrlOpt *opt);
static int s_BtConnectStep_03(tBtCtrlOpt *opt);
static int s_BtConnectStep_04(tBtCtrlOpt *opt);
static int s_BtConnectStep_05(tBtCtrlOpt *opt);
static int s_BtConnectStep_06(tBtCtrlOpt *opt);
static int s_BtConnectStep_07(tBtCtrlOpt *opt);
static int s_BtConnectStep_08(tBtCtrlOpt *opt);
static int s_BtConnectStep_09(tBtCtrlOpt *opt);
static int s_BtConnectStep_10(tBtCtrlOpt *opt);
static int s_BtConnectStep_11(tBtCtrlOpt *opt);
static int s_BtConnectStep_12(tBtCtrlOpt *opt);
static int s_BtConnectStep_01_retry(tBtCtrlOpt* opt);
static int s_BtConnectStep_04_retry(tBtCtrlOpt* opt);
static int s_BtConnectStep_07_retry(tBtCtrlOpt* opt);
static int s_BtConnectStep_10_retry(tBtCtrlOpt* opt);


static int s_BtConnectTimerDone(void)
{
	int i, iCnt, iRet;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	T_StatusBlock gtBtConnectOptTab[] = {
		{eStateConnectDone, s_BtConnectStep_00},
		{eStateConnectPairStart, s_BtConnectStep_01},
		{eStateConnectPairRestart, s_BtConnectStep_01_retry},
		{eStateConnectPairing, s_BtConnectStep_02},
		{eStateConnectPairCompleted, s_BtConnectStep_03},
		{eStateServiceDiscoveryRestart, s_BtConnectStep_04_retry},
		{eStateServiceDiscoveryStart, s_BtConnectStep_04},
		{eStateServiceDiscovering, s_BtConnectStep_05},
		{eStateServiceDiscoveryCompleted, s_BtConnectStep_06},

		{eStateRemoteTypeObtainRestart, s_BtConnectStep_07_retry},
		{eStateRemoteTypeObtainStart, s_BtConnectStep_07},
		{eStateRemoteTypeObtaining, s_BtConnectStep_08},
		{eStateRemoteTypeObtained, s_BtConnectStep_09},
		
		{eStateSppConnectRestart, s_BtConnectStep_10_retry},
		{eStateSppConnectStart, s_BtConnectStep_10},
		{eStateSppConnecting, s_BtConnectStep_11},
		{eStateSppConnectCompleted, s_BtConnectStep_12},
	};
	
	iCnt = sizeof(gtBtConnectOptTab) / sizeof(gtBtConnectOptTab[0]);
	for(i = 0; i < iCnt; i++)
	{
		if(ptBtOpt->BtConnectOptStep == gtBtConnectOptTab[i].iStep 
			&& gtBtConnectOptTab[i].pFunc != NULL)
		{
			iRet = gtBtConnectOptTab[i].pFunc((tBtCtrlOpt *)ptBtOpt);
			break;
		}
	}
	if(i >= iCnt)
	{
		// FixMe
		Display(("Undefined Status <ConnectOptStep = %d>\r\n", ptBtOpt->BtConnectOptStep));
		ptBtOpt->BtConnectErrCode = -404;
		ptBtOpt->BtConnectOptStep = eStateConnectDone;
	}
	return 0;
}

extern unsigned int BluetoothStackID;

static int s_BtConnectStep_00(tBtCtrlOpt *opt)
{
	opt->BtConnectOptStep = eStateConnectDone;
	return 0;
}

static int s_BtConnectStep_01_retry(tBtCtrlOpt* opt)
{
	static int tmp = 0;

	if(tmp++ > 8)
	{
		opt->BtConnectOptStep = eStateConnectPairStart;
		tmp = 0;
	}

	return opt->BtConnectErrCode;
}
static int s_BtConnectStep_01(tBtCtrlOpt* opt)
{
	int i, iRet, iTmp = 0;
	//BD_ADDR_t BD_ADDR;
	static int iCnt = 0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	// 入口状态: opt->BtConnectOptStep = eStateConnectPairStart;
	opt->BtConnectErrCode = 0;

	/*
	gDebugT1 = BtGetTimerCount();
	iRet = ConnectionPairingCancel(BD_ADDR);
	gDebugT2 = BtGetTimerCount();
	Display(("ConnectionPairingCancel <DoneTime:%dms>\r\n", gDebugT2-gDebugT1));
	if(iRet != 0)
	{
		// FixMe
	}
	*/
	giBtApiMutex = 1;
	gDebugT1 = BtGetTimerCount();
	iRet = ConnectionPair(ptBtOpt->masterConnectedRemoteAddr);
	gDebugT2 = BtGetTimerCount();
	giBtApiMutex = 0;
	Display(("ConnectionPair <iCnt:%d, iRet:%d, DoneTime:%dms>\r\n", iCnt, iRet, gDebugT2-gDebugT1));
	if(iRet < 0)
	{
		giBtApiMutex = 1;
		gDebugT1 = BtGetTimerCount();
		iTmp = GAP_Disconnect_Link(BluetoothStackID, ptBtOpt->masterConnectedRemoteAddr);
		gDebugT2 = BtGetTimerCount();
		giBtApiMutex = 0;
		//DelayMs(40);
		if(iTmp != 0)
		{
			// FixMe
			Display(("GAP_Disconnect_Link <Stack_Err:%d, TimeDone:%d>\r\n", iTmp, gDebugT2-gDebugT1));
		}
		else
		{
			Display(("GAP_Disconnect_Link <OK, TimeDone:%d>\r\n", gDebugT2-gDebugT1));
		}

		if(iCnt++ > 5)
		{
			// FixMe
			Display(("ConnectionPair <Stack_Err:%d>\r\n", iRet));
			iCnt = 0;	
			opt->isServerEnd = 1;
			opt->BtConnectErrCode = -1;
			opt->PairComplete = -1;
			opt->BtConnectOptStep = eStateConnectDone;
			return opt->BtConnectErrCode;
		}
		Display(("<Warning!><Line:%d> %s, TODO: eStateConnectPairRestart\r\n", __LINE__, __FUNCTION__));
		opt->BtConnectOptStep = eStateConnectPairRestart;
		return opt->BtConnectErrCode;
	}
	
	iCnt = 0;
	opt->BtConnectOptStep = eStateConnectPairing;
	
	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_02(tBtCtrlOpt* opt)
{
	static int iCnt = 0;
	static int iTmp = 0;
	//Display(("***** %s ******\r\n", __FUNCTION__));
	// opt->BtConnectOptStep = eStateConnectPairing;
	if(opt->PairComplete == 1)
	{
		iCnt = 0;
		iTmp = 0;
		opt->BtConnectOptStep = eStateConnectPairCompleted;
	}
	else if (opt->PairComplete == -1)
	{
		// FixMe
		if(iTmp++ < 4) // 0 for debug
		{
			iCnt = 0;
			opt->PairComplete = 0;
			opt->BtConnectOptStep = eStateConnectPairRestart; // 重来2次??
			Display(("<Warning!><Line:%d> %s, TODO: eStateConnectPairRestart\r\n", __LINE__, __FUNCTION__));
		}
		else
		{
			iTmp = 0;
			iCnt = 0;
			opt->isServerEnd = 1;
			opt->BtConnectErrCode = -2;
			opt->BtConnectOptStep = eStateConnectDone;
			Display(("<Err!><Line:%d> %s\r\n", __LINE__, __FUNCTION__));
		}
	}
	else // (opt->PairComplete == 0)
	{
		opt->BtConnectOptStep = eStateConnectPairing;
		if(iCnt++ > 100*30) // 30s, ConnectionPair调用后可能没有事件返回，则需要超时来退出
		{
			iCnt = 0;
			iTmp = 0;
			opt->BtConnectErrCode = -3;
			opt->BtConnectOptStep = eStateConnectDone;
			Display(("<Err!><Line:%d> %s\r\n", __LINE__, __FUNCTION__));
		}
	}
	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_03(tBtCtrlOpt* opt)
{
	//Display(("***** %s ******\r\n", __FUNCTION__));
	// opt->BtConnectOptStep = eStateConnectPairCompleted;
	// to : eStateServiceDiscoveryStart
	opt->BtConnectOptStep = eStateServiceDiscoveryStart;
	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_04_retry(tBtCtrlOpt* opt)
{
	static int tmp = 0;

	if(tmp++ > 5)
	{
		opt->BtConnectOptStep = eStateServiceDiscoveryStart;
		tmp = 0;
	}

	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_04(tBtCtrlOpt* opt)
{
	int iRet;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	// 入口状态: opt->BtConnectOptStep = eStateServiceDiscoveryStart;
	giBtApiMutex = 1;
	gDebugT1 = BtGetTimerCount();
	iRet = ServiceDiscovery(ptBtOpt->masterConnectedRemoteAddr, 20);
	gDebugT2 = BtGetTimerCount();
	giBtApiMutex = 0;
	if(iRet != 0)
	{
		// FixMe
		Display(("ServiceDiscovery <Stack_Err:%d, TimeDone:%d>\r\n",iRet, gDebugT2-gDebugT1));
		opt->isServerEnd = 1;
		opt->BtConnectErrCode = -4;
		opt->BtConnectOptStep = eStateConnectDone;
		opt->ServiceDiscoveryComplete = -1;
		return opt->BtConnectErrCode;
	}
	else
	{
		Display(("ServiceDiscovery <OK, DoneTime:%dms>\r\n",gDebugT2-gDebugT1));
	}
	opt->BtConnectOptStep = eStateServiceDiscovering;
	
	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_05(tBtCtrlOpt* opt)
{
	static int iCnt = 0;
	static int iTmp = 0;
	//Display(("***** %s ******\r\n", __FUNCTION__));
	//opt->BtConnectOptStep = eStateServiceDiscovering;
	if(opt->ServiceDiscoveryComplete == 1)
	{
		iCnt = 0;
		iTmp = 0;
		opt->BtConnectOptStep = eStateServiceDiscoveryCompleted;
	}
	else if (opt->ServiceDiscoveryComplete == -1)
	{
		// FixMe
		if(iTmp++ < 2) // 0 for debug
		{
			iCnt = 0;
			opt->ServiceDiscoveryComplete = 0;
			opt->BtConnectOptStep = eStateServiceDiscoveryRestart;
			Display(("<Warning!><Line:%d> %s, TODO: eStateServiceDiscoveryRestart\r\n", __LINE__, __FUNCTION__));
		}
		else
		{
			iCnt = 0;
			iTmp = 0;
			opt->isServerEnd = 1;
			opt->BtConnectOptStep = eStateConnectDone;
			opt->BtConnectErrCode = -5;
			Display(("<Err!><Line:%d> %s\r\n", __LINE__, __FUNCTION__));
		}
	}
	else
	{
		opt->BtConnectOptStep = eStateServiceDiscovering;
		if(iCnt++ > 100*10) // ServiceDiscovery调用后可能没有事件返回，则需要超时来退出
		{
			iCnt = 0;
			iTmp = 0;
			opt->BtConnectErrCode = -6;
			opt->BtConnectOptStep = eStateConnectDone;
			Display(("<Err!><Line:%d> %s\r\n", __LINE__, __FUNCTION__));	
		}
	}
	return opt->BtConnectErrCode;

}

static int s_BtConnectStep_06(tBtCtrlOpt* opt)
{
	//Display(("***** %s ******\r\n", __FUNCTION__));
	// opt->BtConnectOptStep = eStateServiceDiscoveryCompleted;
	// to : eStateSppConnectStart
	static int tmp = 0;

	if(tmp++ > 4)
	{
		opt->BtConnectOptStep = eStateRemoteTypeObtainStart;
		tmp = 0;
	}

	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_07_retry(tBtCtrlOpt* opt)
{
	static int tmp = 0;

	if(tmp++ > 3)
	{
		opt->BtConnectOptStep = eStateRemoteTypeObtainStart;
		tmp = 0;
	}

	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_07(tBtCtrlOpt* opt)
{
	int iRet;
	static int retry = 0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	// 入口状态: opt->BtConnectOptStep = eStateSppConnectStart; 
	giBtApiMutex = 1;
	opt->RemoteServerType = -1;
	gDebugT1 = BtGetTimerCount();
	iRet = GetRemoteDeviceType(ptBtOpt->masterConnectedRemoteAddr);
	gDebugT2 = BtGetTimerCount();
	giBtApiMutex = 0;
	if(iRet <= 0)
	{
		// FixMe
		if(retry++ > 2)
		{
			Display(("GetRemoteDeviceType <Err:%d, DoneTime:%d>\r\n", iRet, gDebugT2-gDebugT1));
			opt->BtConnectErrCode = -7;
			opt->isServerEnd = 1;
			opt->BtConnectOptStep = eStateConnectDone;
			opt->SPPConnectComplete = -1;
			retry = 0;
			return opt->BtConnectErrCode;
		}
		else
		{
			opt->BtConnectOptStep = eStateRemoteTypeObtainRestart;//eStateSppConnectRestart;
			Display(("<Warning!><Line:%d> %s, TODO: eStateServiceDiscoveryRestart\r\n", __LINE__, __FUNCTION__));
			return opt->BtConnectErrCode;
		}
	}
	else
	{
		Display(("SPPConnect <OK, DoneTime:%dms>\r\n", gDebugT2-gDebugT1));
	}
	
	opt->BtConnectOptStep = eStateRemoteTypeObtaining; 
	retry = 0;

	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_08(tBtCtrlOpt *opt)
{
	static int iCnt = 0;

	// android
	if(opt->RemoteServerType == 0 || opt->RemoteServerType == 1)
	{
		opt->BtConnectOptStep = eStateRemoteTypeObtained;
		iCnt = 0;
	}

	if(iCnt ++ > 100*10)		// GetRemoteDeviceType调用后可能没有事件返回，需要超时处理
	{
		iCnt = 0;
		opt->isServerEnd = 0;
		opt->BtConnectOptStep = eStateConnectDone;
		Display(("<Err!><Line:%d> %s\r\n", __LINE__, __FUNCTION__));
	}
	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_09(tBtCtrlOpt *opt)
{
	opt->BtConnectOptStep = eStateSppConnectStart;
	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_10_retry(tBtCtrlOpt *opt)
{
	static int tmp = 0;

	if(tmp++ > 5)
	{
		opt->BtConnectOptStep = eStateSppConnectStart;
		tmp = 0;
	}

	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_10(tBtCtrlOpt *opt)
{
	int iRet;
	static int retry = 0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	// 入口状态: opt->BtConnectOptStep = eStateSppConnectStart; 
	giBtApiMutex = 1;
	gDebugT1 = BtGetTimerCount();
	if(opt->RemoteServerType == 0)		// non ios
	{
		opt->SPPConnectComplete = 0;
		iRet = SPPConnect(ptBtOpt->masterConnectedRemoteAddr, ptBtOpt->RemoteSeverPort);
		Display(("NON ios SPPConnect ret = %d <OK, DoneTime:%dms> port : %d\r\n",iRet, gDebugT2-gDebugT1,ptBtOpt->RemoteSeverPort));
	}
	else if(opt->RemoteServerType == 1)		// ios
	{
		opt->ISPPConnectComplete = 0;
		if(retry == 0)
		{
			s_WltProfileClose(WLT_PROFILE_ISPP);
		//	ISPPProfileClose();
		}
		opt->isServerEnd = 0;			// 主动连接
		iRet = ISPPConnect(ptBtOpt->masterConnectedRemoteAddr, ptBtOpt->RemoteSeverPort);
		Display(("ios ISPPConnect ret = %d <OK, DoneTime:%dms> port : %d\r\n",iRet, gDebugT2-gDebugT1,ptBtOpt->RemoteSeverPort));
	}
	gDebugT2 = BtGetTimerCount();
	giBtApiMutex = 0;
	if(iRet < 0)
	{
		// FixMe
		if(retry++ > 2)
		{
			Display(("SPPConnect <Stack_Err:%d, DoneTime:%d>\r\n", iRet, gDebugT2-gDebugT1));
			opt->BtConnectErrCode = -10;
			opt->isServerEnd = 1;
			opt->BtConnectOptStep = eStateConnectDone;
			opt->SPPConnectComplete = -1;
			retry = 0;
			return opt->BtConnectErrCode;
		}
		else
		{
			opt->BtConnectOptStep = eStateSppConnectRestart;
			Display(("<Warning!><Line:%d> %s, TODO: eStateServiceDiscoveryRestart\r\n", __LINE__, __FUNCTION__));
			return opt->BtConnectErrCode;
		}
	}
	else
	{
		Display(("SPPConnect <OK, DoneTime:%dms>\r\n", gDebugT2-gDebugT1));
	}
	
	opt->BtConnectOptStep = eStateSppConnecting; 
	retry = 0;

	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_11(tBtCtrlOpt *opt)
{
	static int iCnt = 0;
	if((opt->SPPConnectComplete == 1 && opt->RemoteServerType == 0) ||
	   (opt->ISPPConnectComplete == 1 && opt->RemoteServerType == 1)
	)
	{
		opt->BtConnectOptStep = eStateSppConnectCompleted;
		iCnt = 0;
	}
	if(iCnt++ > 100*20) // SPPConnect调用后可能没有事件返回，则需要用超时来退出
	{
		iCnt = 0;
		opt->isServerEnd = 1;
		opt->BtConnectErrCode = -11;
		opt->BtConnectOptStep = eStateSppConnectCompleted;
		Display(("<Err!><Line:%d> %s\r\n", __LINE__, __FUNCTION__));
	}
	return opt->BtConnectErrCode;
}

static int s_BtConnectStep_12(tBtCtrlOpt *opt)
{
	unsigned char mac[6];
	// 入口状态: opt->BtConnectOptStep = eStateSppConnectCompleted;
	opt->BtConnectOptStep = eStateConnectDone;
	if(opt->BtConnectErrCode == 0)
	{
		opt->isServerEnd = 0;
		s_WltSetAddrToMac(mac, &opt->masterConnectedRemoteAddr);
	//	s_WltProfileClose(WLT_PROFILE_ALL);
	//	s_WltProfileClose(WLT_PROFILE_SPP|WLT_PROFILE_SPPLE|WLT_PROFILE_BLE);
		opt->masterConnected = 1;
		gReconnectFlag = 0;

	}
	else
	{
		Display(("BtConnect Timer Done <Err:%d>\r\n", opt->BtConnectErrCode));
		opt->isServerEnd = 1;
		opt->eConnectType = eConnected_None;
		opt->masterConnected = 0;
	}
	return opt->BtConnectErrCode;
}

void BtUpdate(void)
{
	ScrPrint(0,7,0,"Not Support Update");
	getkey();
}

void s_BtInit(void)
{
	if (is_wlt2564())
	{
		s_WltBtInit();
		//s_BtPowerOff();
		MfiCpDefaultPinSet();
		s_BtPowerSwitch(0);
	}
}

void s_BtReset(void)
{
	s_BtRstSetPinType(1);
	s_BtRstSetPinVal(0);
	DelayMs(10);
	s_BtRstSetPinVal(1);
	DelayMs(500);
}

void BtRfTest(void)
{
	uchar ch;
	unsigned char ucRet;
	
	ScrCls();
	ScrPrint(0,3,0,"Waiting for test...");

	ucRet = PortOpen(0, "115200,8,n,1");
	//ScrPrint(0, 5, 0x00, "Port(0):%d", ucRet);
	ucRet = PortOpen(7, "115200,8,n,1");
	//ScrPrint(0, 6, 0x00, "Port(7):%d", ucRet);
	s_BtReset();
	while(1)
	{			
		if (PortRx(0, &ch)==0) PortTx(7, ch);
		if (PortRx(7, &ch)==0) PortTx(0, ch);
		if (!kbhit() && (getkey()==KEYCANCEL))break;
	}
	PortClose(0);
	PortClose(7);
}

int BtOpen(void)
{
	int iRet, err, tmp;
	char name[1024];
	char NewName[BT_NAME_MAX_LEN];
	char BleName[BT_NAME_MAX_LEN];
	char Pin[BT_PIN_MAX_LEN];
	char value[3] = {0};
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	if(!is_wlt2564()) return -255;
	
	if(giBtDevInited == 1)
	{
		if(giBtMfiCpErr)
			Err_return("Bt Mfi init Err", BT_RET_ERROR_MFI);
		else
			OK_return(__FUNCTION__, BT_RET_OK);
	}
	
	memset(name, 0x00, sizeof(name));
	memset(NewName, 0x00, sizeof(NewName));
	memset(BleName, 0x00, sizeof(BleName));
	memset(Pin, 0x00, sizeof(Pin));

	giBtMfiCpErr = s_BtPowerOn();
	giBtApiMutex = 1;	
	if(giBtStackInited == 0)
	{
		s_WltBtInit();
		//s_BtReset();\
		memset(gucBtPairData, 0, sizeof(gucBtPairData));
		s_WltGetConfigFile(BT_PAIRDATA, gucBtPairData);
		gDebugT1 = BtGetTimerCount();
		iRet = MainThread(NULL);
		gDebugT2 = BtGetTimerCount();
		if(iRet != 0)
		{
			giBtStackInited = 0;
			giBtDevInited = 0;
			Display(("MainThread <Stack_Err:%d, TimeDone:%dms>\r\n", iRet, gDebugT2-gDebugT1));
			Err_return("MainThread Init", BT_RET_ERROR_NODEV);
		}
		Display(("MainThread <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
		s_WltGetConfigFile(BT_NAME, NewName);
		memcpy(BleName, NewName, 14);//ble name max to 14
		if((iRet = SetLocalDeviceName(NewName)) == 0x00
			&& (tmp = SetBLELocalDeviceName(BleName)) == 0x00)
		{
			s_WltSetBtName(NewName);
		}
		else
		{
			// FixMe
			Display(("SetLocalDeviceName <Stack_Err:iRet=%d, tmp=%d>\r\n", 
				iRet, tmp));
			if((iRet = GetLocalDeviceName(name)) == 0x00)
			{
				Display(("LocalDviceName:%s\r\n", name));
				s_WltSetBtName(name);
			}
			else
			{
				// FixMe
				Display(("GetLocalDeviceName <Stack_Err:%d>\r\n", iRet));
			}
		}
		s_WltGetConfigFile(BT_PIN, Pin);
		
		s_WltSetBtPin(Pin);
		SetLinkSupervisionTimeout(500);
		s_WltProfileOpen(WLT_PROFILE_ALL);

		iRet = SysParaRead(SET_BT_PAIR_MODE, value);
		Display(("SysParaRead value:%s\r\n", value));
		if(iRet == 0 && value[0] == '1')
		{
			iRet = SetPairabilityMode(pmPairableMode_EnableSecureSimplePairing);
			Display(("SetPairabilityMode <iRet:%d>\r\n", iRet));	
			iRet = ConnectionSetSSPParameters(NoInputNoOutput, 0);
			Display(("ConnectionSetSSPParameters(%d,%d) <iRet:%d>\r\n", NoInputNoOutput, 0, iRet));			
		}
		
		giBtStackInited = 1;
	}
	//else
	//{
	//	s_WltSetBtAbilityMode(1);
	//	s_WltProfileOpen(WLT_PROFILE_ALL);
	//}

	ptBtOpt->isServerEnd = 1;
	ptBtOpt->eConnectType = eConnected_None;
	ptBtOpt->tInqResult.iCnt = 0;
	
//	QueInit((T_Queue *)(&ptBtOpt->tRecvQue), gauBtRecvFifo, sizeof(gauBtRecvFifo));
//	QueInit((T_Queue *)(&ptBtOpt->tSendQue), gauBtSendFifo, sizeof(gauBtSendFifo));

	QueInit((T_Queue *)(&ptBtOpt->tMasterRecvQue), gauMasterRecvFifo, sizeof(gauMasterRecvFifo));
	QueInit((T_Queue *)(&ptBtOpt->tMasterSendQue), gauMasterSendFifo, sizeof(gauMasterSendFifo));
	QueInit((T_Queue *)(&ptBtOpt->tSlaveRecvQue), gauSlaveRecvFifo, sizeof(gauSlaveRecvFifo));
	QueInit((T_Queue *)(&ptBtOpt->tSlaveSendQue), gauSlaveSendFifo, sizeof(gauSlaveSendFifo));

	giBtDevInited = 1;
	giBtApiMutex = 0;

	if(giBtMfiCpErr)
		Err_return("Bt Mfi init Err", BT_RET_ERROR_MFI);
	else
		OK_return(__FUNCTION__, BT_RET_OK);
}

int BtClose(void)
{
	if(!is_wlt2564()) return -255;
	
	if(giBtDevInited == 1)
	{
		BtPortClose();
		BtDisconnect();
		DelayMs(100);
		s_WltProfileClose(WLT_PROFILE_ALL);
		s_WltSetBtAbilityMode(0);

		giBtDevInited = 0;
		giBtPortInited = 0;

		if (giBtProfileUpdateFlag)
		{
			s_WltSetConfigFile(BT_PAIRDATA, gucBtPairData);
			giBtProfileUpdateFlag = 0;
		}
	}
	DelayMs(200);
	s_BtPowerOff();
	if(GetBtLinkEnable())
	{
		SetBtLinkEnable(0);				// 断开蓝牙需要关闭手座机蓝牙链路的使能
	}
	OK_return(__FUNCTION__, BT_RET_OK);
}

int BtGetConfig(ST_BT_CONFIG *pCfg)
{
	char name[512];
	char pin[16+1];
	char mac[6];
	
	if(!is_wlt2564()) return -255;

	if(giBtDevInited == 0)
	{
		Err_return("<BtGetConfig> BtDev Not Init", BT_RET_ERROR_NODEV);
	}
	if(pCfg == NULL)
	{
		Err_return("BtGetConfig Error Parameters", BT_RET_ERROR_PARAMERROR);
	}
	giBtApiMutex = 1;
	memset(pCfg, 0x00, sizeof(ST_BT_CONFIG));
	memset(name, 0x00, sizeof(name));
	memset(pin, 0x00, sizeof(pin));	
	if(GetLocalDeviceName(name) != 0)
	{	
		giBtApiMutex = 0;
		Err_return("GetLocalDeviceName", BT_RET_ERROR_STACK);
	}
	if(s_WltGetLocalMac(mac) != 0)
	{
		giBtApiMutex = 0;
		Err_return("s_WltGetLocalMac", BT_RET_ERROR_STACK);
	}
	s_WltGetBtPin(pin);
	
	s_WltSetBtName(name);
	memcpy(pCfg->name, name, 64);
	memcpy(pCfg->pin, pin, 16);
	memcpy(pCfg->mac, mac, 6);
	
	giBtApiMutex = 0;
	OK_return(__FUNCTION__, BT_RET_OK);
}

int BtSetConfig(ST_BT_CONFIG *pCfg)
{
	char ble_name[16];
	char name[BT_NAME_MAX_LEN+1];
	char pin[BT_PIN_MAX_LEN+1];
	int i, iTmp, iRet;
	int err = 0;
	int tmp;
	
	if(!is_wlt2564()) return -255;
	
	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", BT_RET_ERROR_NODEV);
	}
	if(pCfg == NULL)
	{
		Err_return("BtGetConfig Error Parameters", BT_RET_ERROR_PARAMERROR);
	}
	if (strlen(pCfg->name) <= 1) return BT_RET_ERROR_PARAMERROR;
	if (strlen(pCfg->pin) < BT_PIN_MIN_LEN) return BT_RET_ERROR_PARAMERROR;
	
	giBtApiMutex = 1;
	memset(name, 0x00, sizeof(name));
	for(i = 0; i < BT_NAME_MAX_LEN; i++)
		name[i] = pCfg->name[i];
	if((iRet = SetLocalDeviceName(name)) != 0)
	{
		// FixMe
		giBtApiMutex = 0;
		Display(("SetLocalDeviceName <Stack_Err:%d>\r\n", iRet));
		Err_return("SetLocalDeviceName", BT_RET_ERROR_STACK);
	}

	do {
		memset(ble_name, 0x00, sizeof(ble_name));
		for(i = 0; i < 14; i++)
			ble_name[i] = pCfg->name[i];
		s_WltProfileClose(WLT_PROFILE_BLE);
		if((iRet = SetBLELocalDeviceName(ble_name)) != 0)
		{
			s_WltProfileOpen(WLT_PROFILE_BLE);
			giBtApiMutex = 0;
			Display(("SetBLELocalDeviceName <Stack_Err:iRet=%d>\r\n", iRet));
			Err_return("SetBLELocalDeviceName", BT_RET_ERROR_STACK);		
		}
		s_WltProfileOpen(WLT_PROFILE_BLE);
		
	}while(0);
	s_WltSetBtName(name);
	s_WltSetConfigFile(BT_NAME, name);
	
	memset(pin, 0x00, sizeof(pin));
	for(i = 0; i < BT_PIN_MAX_LEN; i++)
		pin[i] = pCfg->pin[i];
	s_WltSetBtPin(pin);
	s_WltSetConfigFile(BT_PIN, pin);
	giBtApiMutex = 0;
	OK_return(__FUNCTION__, 0);
}

int BtScan_GetRemoteDeviceName(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int TimeOut)
{
	int i, iRet;
	uint t0;
	int iCnt = 0;
	ST_BT_SLAVE pBtSlave[BT_INQUIRY_MAX_NUM];
	BD_ADDR_t BD_ADDR;
	int err = 0;
	
	if(!is_wlt2564()) return -255;
	
	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", BT_RET_ERROR_NODEV);
	}
	if(pSlave == NULL)
	{
		Err_return("BtScan Error Parameters", BT_RET_ERROR_PARAMERROR);
	}
	if(TimeOut > MAXIMUM_INQUIRY_LENGTH || TimeOut < MINIMUM_INQUIRY_LENGTH
		|| Cnt > MAXIMUM_NUMBER_INQUIRY_RESPONSES || Cnt < MINIMUM_NUMBER_INQUIRY_RESPONSES)
	{
		Err_return("BtScan Error Parameters", BT_RET_ERROR_PARAMERROR);
	}
	if(Cnt > BT_INQUIRY_MAX_NUM)		Cnt = BT_INQUIRY_MAX_NUM;
	
	memset(pBtSlave, 0x00, sizeof(pBtSlave));

	// TODO: inquiry
	memset((char *)&gptBtOpt->tInqResult, 0x00, sizeof(gptBtOpt->tInqResult));
	giBtApiMutex = 1;
	gDebugT1 = BtGetTimerCount();
	iRet = ConnectionInquiry(TimeOut, Cnt);
	gDebugT2 = BtGetTimerCount();
	giBtApiMutex = 0;
	if(iRet != 0)
	{
		// FixMe
		Display(("ConnectionInquiry(%d,%d) <Stack_Err:%d, TimeDone:%dms>\r\n", TimeOut, Cnt, iRet, gDebugT2-gDebugT1));
		Err_return("ConnectionInquiry(Statck Err)", BT_RET_ERROR_STACK);
	}
	Display(("ConnectionInquiry(%d,%d) <OK, TimeDone:%dms>\r\n", TimeOut, Cnt, gDebugT2-gDebugT1));
	t0 = GetMs();
	while(1)
	{
		if(GetMs() - t0 > (TimeOut+1)*1000)
		{
			Err_return("ConnectionInquiry(Statck TimeOut)", BT_RET_ERROR_TIMEOUT);
		}
		if((iCnt = s_WltGetInquiryCnt()) != 0)
			break;
	}
	
	//iCnt = s_WltGetInquiryDev(pMac, iCnt);
	for(i = 0; i < iCnt; i++)
	{
		gptBtOpt->GetRemoteNameComplete = 0;
		giBtApiMutex = 1;
		gDebugT1 = BtGetTimerCount();
		iRet = GetRemoteDeviceName(gptBtOpt->tInqResult.Device[i].BD_ADDR);
		gDebugT2 = BtGetTimerCount();
		giBtApiMutex = 0;
		if(iRet != 0)
		{
			Display(("GetRemoteDeviceName(%02x:%02x:%02x:%02x:%02x:%02x) <Stack_Err:%d>\r\n", 
				gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR5, 
				gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR4, 
				gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR3,
				gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR2, 
				gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR1, 
				gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR0, iRet));
			//Err_return("GetRemoteDeviceName(Statck Err)", -3);
			continue;
		}
		Display(("GetRemoteDeviceName<Dev_%d:%02x:%02x:%02x:%02x:%02x:%02x> <OK, TimeDone:%dms>\r\n", i, 
			gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR5, 
			gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR4,
			gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR3, 
			gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR2,
			gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR1,
			gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR0,
			gDebugT2-gDebugT1));
		t0 = GetMs();
		while(1)
		{
			if(GetMs() - t0 > 8*1000)
			{
				// 即使获取名称错误也不做其他处理
				Display(("GetRemoteDeviceName(%02x:%02x:%02x:%02x:%02x:%02x) <Stack Timeout>\r\n", 
					gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR5, 
					gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR4, 
					gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR3,
					gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR2, 
					gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR1, 
					gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR0));
				break;
			}
			if(gptBtOpt->GetRemoteNameComplete != 0)
				break;
		}
	}
	gptBtOpt->GetRemoteNameComplete = 0;

	Display(("\r\n"));
	for(i = 0; i < iCnt; i++)
	{
		//memcpy(pSlave->mac, &gptBtOpt->tInqResult.Device[i].BD_ADDR, 6);
		pSlave->mac[0] = gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR5;
		pSlave->mac[1] = gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR4;
		pSlave->mac[2] = gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR3;
		pSlave->mac[3] = gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR2;
		pSlave->mac[4] = gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR1;
		pSlave->mac[5] = gptBtOpt->tInqResult.Device[i].BD_ADDR.BD_ADDR0;
		memcpy(pSlave->name, &gptBtOpt->tInqResult.Device[i].DeviceName, 64);

		Display(("pSlave[%d]: %02x:%02x:%02x:%02x:%02x:%02x, %s\r\n", i, 
			pSlave->mac[0], pSlave->mac[1], pSlave->mac[2], 
			pSlave->mac[3], pSlave->mac[4], pSlave->mac[5], pSlave->name));
		pSlave++;
	}
	return iCnt;
}

int BtScan(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int TimeOut)
{
	int i, iRet;
	uint t0;
	int iCnt = 0;
	ST_BT_SLAVE pBtSlave[BT_INQUIRY_MAX_NUM];
	BD_ADDR_t BD_ADDR;
	int err = 0;
	int retry;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(!is_wlt2564()) return -255;
	
	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", BT_RET_ERROR_NODEV);
	}
	if(pSlave == NULL)
	{
		Err_return("BtScan Error Parameters", BT_RET_ERROR_PARAMERROR);
	}
	if(TimeOut > MAXIMUM_INQUIRY_LENGTH || TimeOut < MINIMUM_INQUIRY_LENGTH
		|| Cnt > MAXIMUM_NUMBER_INQUIRY_RESPONSES || Cnt < MINIMUM_NUMBER_INQUIRY_RESPONSES)
	{
		Err_return("BtScan Error Parameters", BT_RET_ERROR_PARAMERROR);
	}
	if(Cnt > BT_INQUIRY_MAX_NUM)		
		Cnt = BT_INQUIRY_MAX_NUM;
	
	TimeOut = TimeOut * 3 / 4;
	if(TimeOut < MINIMUM_INQUIRY_LENGTH)
		TimeOut = MINIMUM_INQUIRY_LENGTH;
	
	// TODO: inquiry
	giBtApiMutex = 1;	
	memset(pBtSlave, 0x00, sizeof(pBtSlave));
	memset((char *)&ptBtOpt->tInqResult, 0x00, sizeof(ptBtOpt->tInqResult));
	ptBtOpt->InquiryComplete = 0;
	gDebugT1 = BtGetTimerCount();
	gDebugScanTimeT1 = gDebugT1;
	iRet = ConnectionInquiry(TimeOut, Cnt);
	gDebugT2 = BtGetTimerCount();
	giBtApiMutex = 0;
	if(iRet != 0)
	{
		// FixMe
		Display(("ConnectionInquiry(%d,%d) <Stack_Err:%d, TimeDone:%dms>\r\n", TimeOut, Cnt, iRet, gDebugT2-gDebugT1));
		Err_return("ConnectionInquiry(Statck Err)", BT_RET_ERROR_STACK);
	}
	Display(("ConnectionInquiry(%d,%d) <OK, TimeDone:%dms, t1:%d,t2:%d>\r\n", TimeOut, Cnt, gDebugT2-gDebugT1, gDebugT1, gDebugT2));
	t0 = GetMs();
	while(1)
	{
		if(GetMs() - t0 > TimeOut*1500)
		{
			giBtApiMutex = 1;
			ConnectionInquiryCancel();
			giBtApiMutex = 0;
			Err_return("ConnectionInquiry(Statck TimeOut)", BT_RET_ERROR_TIMEOUT);
		}
		//if((iCnt = s_WltGetInquiryCnt()) != 0)
		if(ptBtOpt->InquiryComplete != 0)
			break;
	}
	if(ptBtOpt->InquiryComplete == -1)
	{
		// 扫描时设备信息反馈事件和扫描结果事件不匹配
		Display(("<Warning:%d> ConnectionInquiry: InquiryComplete Result Err\r\n", __LINE__));
		DelayMs(200);
		memset((char *)&ptBtOpt->tInqResult, 0x00, sizeof(ptBtOpt->tInqResult));
	}
	
	Display(("\r\n"));
	//giBtApiMutex = 1;
	iCnt = ptBtOpt->tInqResult.iCnt;
	for(i = 0; i < iCnt; i++)
	{
		retry = 0;
re_GetRemoteName:
		if(strlen(ptBtOpt->tInqResult.Device[i].DeviceName) == 0)
		{
			giBtApiMutex = 1;
			ptBtOpt->InquiryGetRemoteNameComplete = 0;
			iRet = GetRemoteDeviceName(ptBtOpt->tInqResult.Device[i].BD_ADDR);
			giBtApiMutex = 0;
			if(iRet != 0)
			{
				// FixMe
				Display(("<ErrLine:%d> GetRemoteDeviceName <StackErr = %d>\r\n", __LINE__, iRet));
				s_DisplayWltAddr("GetRemoteDeviceName", &ptBtOpt->tInqResult.Device[i].BD_ADDR);
				continue ;
			}
			t0 = GetMs();
			while(1)
			{
				if(GetMs() - t0 > 6*1000)
				{
					// CancelGetRemoteDeviceName
					giBtApiMutex = 1;
					iRet = CancelGetRemoteDeviceName(gptBtOpt->tInqResult.Device[i].BD_ADDR);
					giBtApiMutex = 0;
					if(iRet != 0)
					{
						// FixMe: What the fck! We can do nothing.
						Display(("<ErrLine:%d> CancelGetRemoteDeviceName <StackErr = %d>\r\n", __LINE__, iRet));
						s_DisplayWltAddr("CancelGetRemoteDeviceName", &ptBtOpt->tInqResult.Device[i].BD_ADDR);
					}

					if(++retry < 2)
					{
						Display(("** GetRemoteDeviceName <TimerOut, Retry:%d>\r\n", retry));
						goto re_GetRemoteName;
					}
					else
						break ;
				}
				if(ptBtOpt->InquiryGetRemoteNameComplete != 0)
					break ;
			}
			if(ptBtOpt->InquiryGetRemoteNameComplete == -1)
			{
				// FixMe: CL_REMOTE_NAME_COMPLETE event err
				if(++retry < 2)
				{
					Display(("** CL_REMOTE_NAME_COMPLETE event err <Retry:%d>\r\n", retry));
					goto re_GetRemoteName;
				}
			}
		}
	}


	// TODO:
	giBtApiMutex = 1;
	iRet = 0;
	for(i = 0; i < iCnt; i++)
	{
		//memcpy(pSlave->mac, &gptBtOpt->tInqResult.Device[i].BD_ADDR, 6);
		if(strlen(ptBtOpt->tInqResult.Device[i].DeviceName) != 0)
		{
			s_WltSetAddrToMac(pSlave->mac, &ptBtOpt->tInqResult.Device[i].BD_ADDR);
			memcpy(pSlave->name, &ptBtOpt->tInqResult.Device[i].DeviceName, 64);
			//Display(("<%d> ", i));
			//s_DisplayWltMac("pSlave->Mac", pSlave->mac);
			pSlave++;
			iRet++;
		}
	}
	giBtApiMutex = 0;
	return iRet;
}

int BtConnect(ST_BT_SLAVE *Slave)
{
	int i, iRet;
	BD_ADDR_t BD_ADDR;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(!is_wlt2564()) return -255;

	if(Slave == NULL) return BT_RET_ERROR_PARAMERROR;
	
	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", BT_RET_ERROR_NODEV);
	}

	if(ptBtOpt->BtConnectOptStep != eStateConnectDone)
	{
		Err_return("BtConnect Err ...ing", BT_RET_ERROR_CONNECTED);
	}

	// 已有1个主连接,不管主连接的是座机还是其他设备,都不可以再建立主连接
	if(ptBtOpt->masterConnected != 0)
	{
		Err_return("BtConnect Err ...masterConnected", BT_RET_ERROR_CONNECTED);
	}

	// 如果存在1个从连接,则不可以再建立主连接
	if(ptBtOpt->slaveConnected == 1)
	{
		Err_return("BtConnect Err ...slaveConnected", BT_RET_ERROR_CONNECTED);
	}

	if(giBtApiMutex) 
	{
		Err_return("BtConnect Reconnect", BT_RET_ERROR_CONNECTED);
	}
	giBtApiMutex = 1;

	s_WltSetMacToAddr(Slave->mac, &ptBtOpt->masterConnectedRemoteAddr);

	s_DisplayWltAddr("ptBtOpt->masterConnectedRemoteAddr", &ptBtOpt->masterConnectedRemoteAddr);

	// 主动发起SPP连接,则不是Server端
	ptBtOpt->isServerEnd = 0;
	ptBtOpt->PairComplete = 0;
	ptBtOpt->ServiceDiscoveryComplete = 0;
	ptBtOpt->SPPConnectComplete = 0;
	ptBtOpt->BtConnectOptStep = eStateConnectPairStart;
	
	giBtApiMutex = 0;

	OK_return(__FUNCTION__, 0);
}

int BtBaseReconnect(ST_BT_SLAVE *Slave)
{
	int iRet;

	iRet = BtConnect(Slave);
	if(!iRet)
	{
		gReconnectFlag = 1;
	}
	return iRet;
}

int BtDisconnect(void)
{
	int iRet = 0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	int sppSerialPortID;
	uint t0;
	
	if(!is_wlt2564()) return -255;
	if (giBtProfileUpdateFlag)
	{
		s_WltSetConfigFile(BT_PAIRDATA, gucBtPairData);
		giBtProfileUpdateFlag = 0;
	}
	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", BT_RET_ERROR_NODEV);
	}
	if(GetBtLinkEnable())
	{
		SetBtLinkEnable(0);				// 断开蓝牙需要关闭手座机蓝牙链路的使能
	}

	if(gReconnectFlag)			// 只能终止手座机的重连
	{
		t0 = GetTimerCount();
		while(1)
		{
			if(GetTimerCount() > t0 + 2000) return BT_RET_ERROR_TIMEOUT;
			// 正在连接的过程中要求断开连接
			if(ptBtOpt->BtConnectOptStep == 0)
			{
				break;
			}
			else if(ptBtOpt->BtConnectOptStep > eStateConnectDone && ptBtOpt->BtConnectOptStep < eStateSppConnectStart)		
			{
				giBtApiMutex = 1;
				if((ptBtOpt->BtConnectOptStep == eStateConnectPairing || ptBtOpt->BtConnectOptStep == eStateConnectPairCompleted))
				{
					GAP_Disconnect_Link(BluetoothStackID, ptBtOpt->masterConnectedRemoteAddr);
				}
				ptBtOpt->BtConnectOptStep = eStateConnectDone;
				giBtApiMutex = 0;
				break;
			}
			else
			{
				if(ptBtOpt->slaveConnected == 1 || ptBtOpt->masterConnected == 1) 
				{
					break;
				}
			}
		}
	}
	
	switch(ptBtOpt->eConnectType)
	{
		case eConnected_None:
			OK_return("BtDisconnect", 0);
			break;
		case eConnected_SPP:
			giBtApiMutex = 1;
			gDebugT1 = BtGetTimerCount();
			// 如果是主连接且不是连接座机，则断开该连接
			if(ptBtOpt->masterConnected == 1)
			{
				sppSerialPortID = ptBtOpt->masterSerialPortID;
			}
			else if(ptBtOpt->slaveConnected == 1) // 如果是从连接，则断开该连接
			{
				sppSerialPortID = ptBtOpt->slaveSerialPortID;
			}
			else
			{
				// ((masterConnected == 2 || masterConnected == 0) && slaveConnected != 1)会进入此分支
				OK_return("BtDisconnect", 0);
			}
			iRet = SPPDisconnectByNumbers(sppSerialPortID);
			gDebugT2 = BtGetTimerCount();
			giBtApiMutex = 0;
			if(iRet != 0)
			{
				// FixMe
				Display(("<ERROR:%d> SPPDisconnectByNumbers(%d) <Stack_Err = %d>\r\n", 
					__LINE__, ptBtOpt->masterSerialPortID, iRet));	
			}
			Display(("SPPDisconnectByNumbers(%d) <OK, TimeDone:%dms>\r\n", 
				sppSerialPortID, gDebugT2-gDebugT1));
			break;
		case eConnected_ISPP:
			giBtApiMutex = 1;
			gDebugT1 = BtGetTimerCount();
			iRet = ISPPDisconnect();
			gDebugT2 = BtGetTimerCount();
			giBtApiMutex = 0;
			if(iRet != 0)
			{
				// FixMe
				Display(("<ERROR:%d> ISPPDisconnect <Stack_Err = %d>\r\n", __LINE__, iRet));	
			}
			Display(("ISPPDisconnect <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
			break;
		case eConnected_SPPLE:
			giBtApiMutex = 1;
			gDebugT1 = BtGetTimerCount();
			iRet = BLEDisconnect(ptBtOpt->SPPLE_Addr);
			gDebugT2 = BtGetTimerCount();
			giBtApiMutex = 0;
			if(iRet != 0)
			{
				// FixMe
				Display(("<ERROR:%d> BLEDisconnect <Stack_Err = %d>\r\n", __LINE__, iRet));	
			}
			Display(("BLEDisconnect <OK, TimeDone:%dms>\r\n", gDebugT2-gDebugT1));
			break;
		default:
			break;
	}
	
	if(iRet == 0)
	{
		DelayMs(500);
		giBtApiMutex = 1;
		if(ptBtOpt->eConnectType == eConnected_ISPP)
			s_WltProfileClose(WLT_PROFILE_ALL);
		s_WltProfileOpen(WLT_PROFILE_ALL);
		giBtApiMutex = 0;
		ptBtOpt->eConnectType = eConnected_None;
		// 如果不是Server端(即建立的SPP连接为D200主动发起的),则需要重新打开所有profile
		if(!ptBtOpt->isServerEnd)
		{
			ptBtOpt->isServerEnd = 1;
			if(1 == ptBtOpt->masterConnected)
			{
				ptBtOpt->masterConnected = 0;			// add 
			}
		}
		else
		{
			if(1 == ptBtOpt->slaveConnected)
			{
				ptBtOpt->slaveConnected = 0;
			}
		}
		OK_return(__FUNCTION__, 0);
	}

	Err_return("Bt Disconnect", BT_RET_ERROR_STACK);
}

int BtGetStatus(ST_BT_STATUS *pStatus)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(!is_wlt2564()) return -255;

	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", BT_RET_ERROR_NODEV);
	}
	if(pStatus == NULL)
	{
		Err_return("BtGetConfig Error Parameters", BT_RET_ERROR_PARAMERROR);
	}

	memset(pStatus, 0x00, sizeof(ST_BT_STATUS));

	// 如果没有从连接 并且 没有主连接其他设备
	if(ptBtOpt->slaveConnected != 1 && ptBtOpt->masterConnected != 1)
	{
		if(ptBtOpt->eConnectType == eConnected_None)		//
		{
			pStatus->status = 0;
		}
		else if(ptBtOpt->BtConnectOptStep != eStateConnectDone && ptBtOpt->BtConnectErrCode == 0)
		{
			pStatus->status = 2;			// bt正在链接中
		}
		OK_return(__FUNCTION__, BT_RET_OK);
	}
	
	if(ptBtOpt->eConnectType == eConnected_None)
	{
		pStatus->status = 0;
		OK_return(__FUNCTION__, BT_RET_OK);
	}

	pStatus->status = 1;
	if(ptBtOpt->isServerEnd) // 从连接
	{
		switch(ptBtOpt->eConnectType)
		{
			case eConnected_ISPP:
				s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->CurrentISPP_BD_ADDR);
				break;
			case eConnected_SPP:
				//s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->CurrentSPP_BD_ADDR);
				s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->slaveCOnnectedRemoteAddr);
				break;
			case eConnected_SPPLE:
				s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->CurrentSPPLE_BD_ADDR);
				break;
			default:
				break;
		}
	}
	else
	{
		//s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->Connect_BD_ADDR);
		s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->masterConnectedRemoteAddr);
	}
	if (giBtProfileUpdateFlag)
	{
		s_WltSetConfigFile(BT_PAIRDATA, gucBtPairData);
		giBtProfileUpdateFlag = 0;

	}
	s_DisplayWltMac("BtGetStatus>RemoteAddr", pStatus->mac);

	OK_return(__FUNCTION__, BT_RET_OK);
}

int BtCtrl(unsigned int iCmd, void *pArgIn, unsigned int iSizeIn ,void* pArgOut, unsigned int iSizeOut)
{
	return WltBtCtrl((tBtCtrlOpt *)gptBtOpt, iCmd, pArgIn, iSizeIn, pArgOut, iSizeOut);
}

// TODO:
unsigned char BtUserPortOpen(void)
{
	int iRet;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", 0x03);
	}
	
	QueReset((T_Queue*)(&ptBtOpt->tMasterRecvQue));
	QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
	
	QueReset((T_Queue*)(&ptBtOpt->tSlaveRecvQue));
	QueReset((T_Queue*)(&ptBtOpt->tSlaveSendQue));
	
	giBtPortInited = 1;
	if (giBtProfileUpdateFlag)
	{
		s_WltSetConfigFile(BT_PAIRDATA, gucBtPairData);
		giBtProfileUpdateFlag = 0;
	}
	OK_return("BtMasterOpen Success", 0x00);
}

unsigned char BtUserPortClose(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", 0x03);
	}
	
	QueReset((T_Queue*)(&ptBtOpt->tMasterRecvQue));
	QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
	QueReset((T_Queue*)(&ptBtOpt->tSlaveRecvQue));
	QueReset((T_Queue*)(&ptBtOpt->tSlaveSendQue));
	
	giBtPortInited = 0;

	OK_return("BtMasterClose Success!", 0x00);

}

int BtUserClose(void)
{
	if(!is_wlt2564()) return -255;

	if(giBtDevInited)
	{
		BtUserPortClose();
		BtDisconnect();
	}

	OK_return(__FUNCTION__, BT_RET_OK);
}

unsigned char BtUserPortSends(unsigned char *szData, unsigned short iLen)
{
	int iCapLen;
	uint t0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *sendQueue = NULL;

	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		Err_return("Bt Dev Not Init", 0x03);
	}

	// 主连接
	if(ptBtOpt->masterConnected == 1)
	{
		sendQueue = (T_Queue *)&ptBtOpt->tMasterSendQue;
	}
	else // 从连接
	{
		sendQueue = (T_Queue *)&ptBtOpt->tSlaveSendQue;
	}
	if (giBtProfileUpdateFlag)
	{
		s_WltSetConfigFile(BT_PAIRDATA, gucBtPairData);
		giBtProfileUpdateFlag = 0;
	}
	t0 = GetMs();
	while(QueIsFull(sendQueue))
	{
		if(GetMs() - t0 > 500)
		{
			Err_return("BtMasterSends TimeOut", 0x04);
		}
	}

	iCapLen = QueCheckCapability(sendQueue);
	if (iCapLen < iLen)
	{
		//return -4;
		Err_return("BtMasterSends TimeOut", 0x04);
	}
	QuePuts(sendQueue, szData, iLen);
	
	OK_return("BtMasterSends Success", 0x00);
}

int BtUserPortRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms)
{
	int iOffset = 0;
	T_SOFTTIMER Time;
	uint t0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *recvQueue = NULL;

	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		Err_return("Bt Dev Not Init", -0x03);
	}
	
	// 主连接且不是连接座机
	if(ptBtOpt->masterConnected == 1)
	{
		recvQueue = (T_Queue *)&ptBtOpt->tMasterRecvQue;
	}
	else // 从连接 或 无连接
	{
		recvQueue = (T_Queue *)&ptBtOpt->tSlaveRecvQue;
	}
	if (giBtProfileUpdateFlag)
	{
		s_WltSetConfigFile(BT_PAIRDATA, gucBtPairData);
		giBtProfileUpdateFlag = 0;
	}
	if(ms != 0)
	{
		t0 = GetMs();
	}
	
	while(iOffset < usLen)
	{
		iOffset += QueGets(recvQueue, szData+iOffset, usLen-iOffset);

		if(ms == 0)break;
		
		if(GetMs() - t0 < ms) continue;

		if(iOffset) break;
		
		Err_return("BtMasterRecvs Timeout", -0xff);
	}
	
	OK_return("BtMasterRecvs Data", iOffset);
}

unsigned char BtUserPortSend(unsigned char ch)
{
	return BtUserPortSends(&ch, 1);
}

unsigned char BtUserPortRecv(unsigned char *ch, unsigned int ms)
{
	int iRet;
	iRet = BtUserPortRecvs(ch, 1, ms);
	if(iRet == 1)
		return 0;
	else if(iRet == -3)
		return 0x03;
	else
		return 0xff;
}

unsigned char BtUserPortReset(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *recvQueue = NULL;
	
	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		Err_return("Bt Dev Not Init", 0x03);
	}
	
	// 主连接
	if(ptBtOpt->masterConnected == 1)
	{
		recvQueue = (T_Queue *)&ptBtOpt->tMasterRecvQue;
	}
	else // 从连接
	{
		recvQueue = (T_Queue *)&ptBtOpt->tSlaveRecvQue;
	}
	
	QueReset(recvQueue);

	OK_return("BtMasterReset Success", 0x00);
}

unsigned char BtUserPortTxPoolCheck(void)
{
	T_Queue *sendQueue = NULL;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		Err_return("Bt Dev Not Init", 0x03);
	}	

	// 主连接
	if(ptBtOpt->masterConnected == 1)
	{
		sendQueue = (T_Queue *)&ptBtOpt->tMasterSendQue;
	}
	else // 从连接
	{
		sendQueue = (T_Queue *)&ptBtOpt->tSlaveSendQue;
	}

	
	if (0 == QueIsEmpty(sendQueue)) // not empty
	{
		return 0x01;
	}
	else
	{
		return 0x00;
	}
}

int BtUserPortPeep(unsigned char *szData, unsigned short iLen)
{
	int iRet;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *recvQueue = NULL;
	
	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		Err_return("Bt Dev Not Init", -0x03);
	}
	
	if(szData == NULL)
	{
		return -1;
	}
	
	// 主连接
	if(ptBtOpt->masterConnected == 1)
	{
		recvQueue = (T_Queue *)&ptBtOpt->tMasterRecvQue;
	}
	else // 从连接 
	{
		recvQueue = (T_Queue *)&ptBtOpt->tSlaveRecvQue;
	}
	
	iRet = QueGetsWithoutDelete(recvQueue, szData, iLen);

	OK_return("BtUserPortPeep Data", iRet);
}

unsigned char BtPortOpen(void)
{
	return BtUserPortOpen();
}

unsigned char BtPortClose(void)
{
	return BtUserPortClose();
}

unsigned char BtPortReset(void)
{
	return BtUserPortReset();
}

unsigned char BtPortSends(unsigned char *szData, unsigned short iLen)
{
	return BtUserPortSends(szData, iLen);
}

unsigned char BtPortTxPoolCheck(void)
{
	return BtUserPortTxPoolCheck();
}

int BtPortRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms)
{
	return BtUserPortRecvs(szData, usLen, ms);
}

unsigned char BtPortSend(unsigned char ch)
{
	return BtUserPortSend(ch);
}

unsigned char BtPortRecv(unsigned char *ch, unsigned int ms)
{
	return BtUserPortRecv(ch, ms);
}

int BtPortPeep(unsigned char *szData, unsigned short iLen)
{
	return BtUserPortPeep(szData, iLen);
}

// for base
int BtBaseGetStatus(ST_BT_STATUS *pStatus)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(!is_wlt2564()) return -255;

	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", BT_RET_ERROR_NODEV);
	}
	if(pStatus == NULL)
	{
		Err_return("BtGetConfig Error Parameters", BT_RET_ERROR_PARAMERROR);
	}

	memset(pStatus, 0x00, sizeof(ST_BT_STATUS));

	if(ptBtOpt->masterConnected != 1)
	{
		if(ptBtOpt->eConnectType == eConnected_None)		//
		{
			pStatus->status = 0;
		}
		else if(ptBtOpt->BtConnectOptStep != eStateConnectDone && ptBtOpt->BtConnectErrCode == 0)
		{
			pStatus->status = 2;			// bt正在链接中
		}
		OK_return(__FUNCTION__, BT_RET_OK);
	}
	
	if(ptBtOpt->eConnectType == eConnected_None)
	{
		pStatus->status = 0;
		OK_return(__FUNCTION__, BT_RET_OK);
	}

	pStatus->status = 1;
	if(ptBtOpt->isServerEnd) // 从连接
	{
		switch(ptBtOpt->eConnectType)
		{
			case eConnected_ISPP:
				s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->CurrentISPP_BD_ADDR);
				break;
			case eConnected_SPP:
				//s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->CurrentSPP_BD_ADDR);
				s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->slaveCOnnectedRemoteAddr);
				break;
			case eConnected_SPPLE:
				s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->CurrentSPPLE_BD_ADDR);
				break;
			default:
				break;
		}
	}
	else
	{
		//s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->Connect_BD_ADDR);
		s_WltSetAddrToMac(pStatus->mac, &ptBtOpt->masterConnectedRemoteAddr);
	}
	
	s_DisplayWltMac("BtGetStatus>RemoteAddr", pStatus->mac);

	OK_return(__FUNCTION__, BT_RET_OK);
}

int BtBaseClose(void)
{
	if(!is_wlt2564()) return -255;

	if(giBtDevInited)
	{
		BtBasePortClose();
		BtDisconnect();
	}

	OK_return(__FUNCTION__, BT_RET_OK);
}

int BtBaseGetErrorCode(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	return ptBtOpt->BtConnectErrCode;
}

unsigned char BtBasePortOpen(void)
{
	int iRet;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	if(giBtDevInited == 0)
	{
		Err_return("Bt Dev Not Init", 0x03);
	}
	
	QueReset((T_Queue*)(&ptBtOpt->tMasterRecvQue));
	QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
	
	QueReset((T_Queue*)(&ptBtOpt->tSlaveRecvQue));
	QueReset((T_Queue*)(&ptBtOpt->tSlaveSendQue));
	
	giBtPortInited = 1;
	
	OK_return("BtMasterOpen Success", 0x00);
}

unsigned char BtBasePortClose(void)
{
	return BtUserPortClose();
}

unsigned char BtBasePortSends(unsigned char *szData, unsigned short iLen)
{
	int iCapLen;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *sendQueue = NULL;

	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		Err_return("Bt Dev Not Init", 0x03);
	}

	// 主连接且不是连接座机
	if(ptBtOpt->masterConnected == 1)
	{
		sendQueue = (T_Queue *)&ptBtOpt->tMasterSendQue;
	}
	else // 只可能是主连接
	{
		Err_return("BtBaseSends <SPP Not Connect>", 0x03);
	}
	
	if(QueIsFull(sendQueue))
	{
		Err_return("BtMasterSends TimeOut", 0x04);
	}

	iCapLen = QueCheckCapability(sendQueue);
	if (iCapLen < iLen)
	{
		//return -4;
		Err_return("BtMasterSends TimeOut", 0x05);
	}
	QuePuts(sendQueue, szData, iLen);
	
	OK_return("BtMasterSends Success", 0x00);
}

unsigned char BtBasePortTxPoolCheck(void)
{
	return BtUserPortTxPoolCheck();
}

int BtBasePortPeep(unsigned char *szData, unsigned short iLen)
{
	int iRet;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *recvQueue = NULL;
	
	if(ptBtOpt->masterConnected != 1)
	{
		Err_return("BtBasePeep <SPP Not Connect>", -0x03);
	}
	
	if(szData == NULL)
	{
		return -1;
	}
	
	recvQueue = (T_Queue *)&ptBtOpt->tMasterSendQue;
	iRet = QueGetsWithoutDelete(recvQueue, szData, iLen);

	OK_return("BtBasePeep Data", iRet);
}

int BtBasePortRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms)
{
	int iOffset = 0;
	T_SOFTTIMER Time;
	uint t0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *recvQueue = NULL;

	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		Err_return("Bt Dev Not Init", -0x03);
	}
	
	// 主连接且不是连接座机
	if(ptBtOpt->masterConnected == 1)
	{
		recvQueue = (T_Queue *)&ptBtOpt->tMasterRecvQue;
	}
	else // 只可能是主连接
	{
		Err_return("BtBaseRecvs <SPP Not Connect>", 0x03);
	}
	
	if(ms != 0)
	{
		t0 = GetMs();
	}
	
	while(iOffset < usLen)
	{
		iOffset += QueGets(recvQueue, szData+iOffset, usLen-iOffset);

		if(ms == 0)break;
		
		if(GetMs() - t0 < ms) continue;

		if(iOffset) break;
		
		Err_return("BtBaseRecvs Timeout", -0xff);
	}
	
	OK_return("BtBaseRecvs Data", iOffset);
}

unsigned char BtBaseSend(unsigned char ch)
{
	return BtBasePortSends(&ch, 1);
}

unsigned char BtBaseRecv(unsigned char *ch, unsigned int ms)
{
	int iRet;
	iRet = BtBasePortRecvs(ch, 1, ms);
	if(iRet == 1)
		return 0;
	else if(iRet == -3)
		return 0x03;
	else
		return 0xff;
}

unsigned char BtBasePortReset(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *recvQueue = NULL;
	
	if(ptBtOpt->masterConnected != 1)
	{
		Err_return("BtBaseReset <SPP Not Connect>", 0x03);
	}

	recvQueue = (T_Queue*)(&ptBtOpt->tMasterRecvQue);
	QueReset(recvQueue);

	OK_return("BtBaseReset Success", 0x00);
}

// TODO: just for bt test
#ifdef DEBUG_WLT2564

typedef void (*tMenuOptFun)(void);
typedef struct menu_opt {
	//int id;
	char str[32];
	tMenuOptFun pFun;
}ST_MENU_OPT;

static void x_MenuSelect(struct menu_opt *menu, int menu_size)
{
#define lines_per_page	(7)
#define key_down		(KEYDOWN)
#define key_up			(KEYUP)
#define key_exit		(KEYCANCEL)

	int i;
	unsigned char ucKey;
	int cur_page = 0;
	int all_lines = menu_size - 1;
	int all_pages = (all_lines + lines_per_page - 1 ) / lines_per_page; /* count from 1 */
	int lines_of_cur_page;

	ScrCls();
	
	if(all_pages <= 0)
		return ;
	while(1)
	{
		if(cur_page == all_pages - 1 && all_lines % lines_per_page != 0) /* the last page */
			lines_of_cur_page = (all_lines) % lines_per_page;
		else
			lines_of_cur_page = lines_per_page;

		ScrCls();
		ScrPrint(0, 0, 0x00, "%s", menu[0].str);
		for(i = 1; i <= lines_of_cur_page; i++)
		{
			ScrPrint(0, i, 0x00, "%d.%s", i, menu[cur_page*lines_per_page+i].str);
		}
		
		ucKey = getkey();
		if (ucKey == key_down)
		{
			if(++cur_page >= all_pages)
				cur_page = 0;
		}
		else if (ucKey == key_up)
		{
			if(--cur_page < 0)
				cur_page = all_pages-1;
		}
		else if (ucKey == key_exit)
		{
			return ;
		}
		else if (ucKey >= KEY1 && ucKey < KEY1 + lines_of_cur_page)
		{
			if(menu[cur_page * lines_per_page + ucKey - KEY0].pFun != NULL)
			{
				ScrCls();
				menu[cur_page * lines_per_page + ucKey - KEY0].pFun();
			}
		}
	}
}

static unsigned char gpInquiryDevMac[6*BT_INQUIRY_MAX_NUM];
static int gpInquiryDevCount = 0;

void logDataVal(char *data, int len)
{
	int i;
	for(i = 0; i < len; i++)
	{
		if(i != 0 && i % 16 == 0)
			Display(("\r\n"));
		Display(("%c ", data[i]));
	}
	Display(("\r\n"));
}

int showAllDevInfo(unsigned char *dev, int iCount)
{
#define MAX_LINE (7)
	int i, j;
	unsigned char ucKey;
	int index = 0;

	while(1)
	{
		ScrCls();
		//Lcdprintf("Select Bt Dev\n");
		ScrPrint(0, 0, 0x40, "Select Bt Dev");
		if(index >= iCount)
		{
			index = 0;
		}
		for(i = 0; i < MAX_LINE && index < iCount; i++, index++)
		{
			//Lcdprintf("%d.%s\n", i, finfo[index++].name);
			ScrPrint(0, i+1, 0x00, "%d.Dev%d:%02x%02x%02x%02x%02x%02x", i, index, 
				dev[index*6+0], dev[index*6+1], dev[index*6+2], 
				dev[index*6+3], dev[index*6+4], dev[index*6+5]);
		}
		while(1)
		{
			if(kbhit() == 0x00)
			{
				ucKey = getkey();
				if(ucKey >= KEY0 && ucKey < KEY0 + i)
				{
					return index - (i - (ucKey-KEY0));
				}
				if(ucKey == KEYCANCEL)
				{
					return -1;
				}
				if(ucKey == KEYENTER)
				{
					break;
				}
			}
		}
	}
}

void ApiTestBtOpen(void)
{
	int iRet;
	
	iRet = BtOpen();
	ScrPrint(0, 3, 0x00, "BtOpen = %d", iRet);
	ScrPrint(0, 5, 0x00, "getkey exit");
	getkey();
}
void ApiTestBtPortOpen(void)
{
	unsigned char ucRet;
	ucRet = BtPortOpen();
	ScrPrint(0, 3, 0x00, "PortOpen = %d", ucRet);
	ScrPrint(0, 5, 0x00, "getkey exit");
	getkey();
}
void ApiTestBtClose(void)
{
	int iRet;
	iRet = BtClose();
	ScrPrint(0, 3, 0x00, "BtClose = %d", iRet);
	ScrPrint(0, 5, 0x00, "getkey exit");
	getkey();
}
void ApiTestBtPortClose(void)
{
	unsigned char ucRet;
	ucRet = BtPortClose();
	ScrPrint(0, 3, 0x00, "PortClose = %d", ucRet);
	ScrPrint(0, 5, 0x00, "getkey exit");
	getkey();

}
void ApiTestBtGetStatus(void)
{
	int iRet;
	unsigned char ucStatus[7];
	ST_BT_STATUS pStatus;
	
	iRet = BtGetStatus(&pStatus);
	if(pStatus.status == 0 || iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "Bt Not Connect");
		ScrPrint(0, 4, 0x00, "iRet = %d", iRet);
	}
	else
	{
		ScrPrint(0, 3, 0x00, "BtConnected:");
		ScrPrint(0, 4, 0x00, "%02x%02x%02x%02x%02x%02x",
			pStatus.mac[0], pStatus.mac[1], pStatus.mac[2], 
			pStatus.mac[3], pStatus.mac[4], pStatus.mac[5]);
	}
	ScrPrint(0, 5, 0x00, "getkey exit");
	getkey();
}

void ApiTestBtGetConfig(void)
{
	ST_BT_CONFIG tBtCfg;
	int iRet;

	memset(&tBtCfg, 0x00, sizeof(ST_BT_CONFIG));
	iRet = BtGetConfig(&tBtCfg);
	if(iRet == 0x00)
	{
		ScrPrint(0, 2, 0x00, "Name:%s", tBtCfg.name);
		ScrPrint(0, 4, 0x00, "PIN:%s", tBtCfg.pin);
		ScrPrint(0, 5, 0x00, "MAC:%02x%02x%02x%02x%02x%02x",
			tBtCfg.mac[5], tBtCfg.mac[4], tBtCfg.mac[3], 
			tBtCfg.mac[2], tBtCfg.mac[1], tBtCfg.mac[0]);
	}
	else
	{
		ScrPrint(0, 3, 0x00, "GetCfg Err<%d>", iRet);
	}
	ScrPrint(0, 6, 0x00, "getkey exit");
	getkey();
}

void ApiTestBtSetConfig(void)
{
	char name[BT_NAME_MAX_LEN+2];
	char pin[BT_PIN_MAX_LEN+2];
	int iRet;
	ST_BT_CONFIG tBtCfg;
	
	memset(name, 0x00, sizeof(name));
	memset(pin, 0x00, sizeof(pin));
	ScrCls();
	ScrPrint(0, 2, 0x00, "input name:");
	iRet = GetString(name, 0x35, 1, 64);
	if(iRet == 0)
	{
		strcpy(tBtCfg.name, name+1);
	}
	ScrCls();
	ScrPrint(0, 2, 0x00, "input pin:");
	iRet = GetString(pin, 0x35, 1, 16);
	if(iRet == 0)
	{
		strcpy(tBtCfg.pin, pin+1);
	}
	getkey();
	ScrCls();
	ScrPrint(0, 2, 0x00, "name:%s", tBtCfg.name);
	ScrPrint(0, 5, 0x00, "pin:%s", tBtCfg.pin);
	if(getkey() == KEYCANCEL)
		return ;
	//strcpy(name, "PAX_D200_BT");
	//strcpy(pin, "0000");

	//strcpy(tBtCfg.name, name);
	//strcpy(tBtCfg.pin, pin);
	iRet = BtSetConfig(&tBtCfg);
	ScrCls();
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "SetConfig Success!");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "SetConfig Error!");
	}
	ScrPrint(0, 5, 0x00, "getkey exit");
	getkey();
}
void ApiTestBtScan(void)
{
	int i, iRet;
	ST_BT_SLAVE pSlave[30];
	unsigned char ucMac[6*BT_INQUIRY_MAX_NUM];
	unsigned int InquiryPeriodLength;
	unsigned int MaximumResponses;
	char str[100];
		
	ScrCls();
	ScrPrint(0, 0, 0x00, "InquiryPeriod:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	InquiryPeriodLength = atoi(str+1);

	ScrCls();
	ScrPrint(0, 0, 0x00, "MaximumResponses:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	MaximumResponses = atoi(str+1);

	ScrCls();
	ScrPrint(0, 1, 0x00, "Time:%d,Cnt:%d", InquiryPeriodLength, MaximumResponses);
	if(getkey() == KEYCANCEL)
		return ;
	//while(1)
	//{
		ScrCls();
		ScrPrint(0, 1, 0x00, "BtScan...");
		//if(kbhit() == 0x00 && getkey() == KEYCANCEL)
		//	break;
		
		iRet = BtScan(pSlave, MaximumResponses, InquiryPeriodLength);
		if(iRet <= 0)
		{
			ScrPrint(0, 3, 0x00, "Scan Err<%d>", iRet);
			ScrPrint(0, 5, 0x00, "getkey exit");
			getkey();
			return ; 
		}
		else
		{
			ScrPrint(0, 3, 0x00, "Scan success!");
			ScrPrint(0, 5, 0x00, "getkey show info");		
		}
	//}
	getkey();
	gpInquiryDevCount = iRet;
	for(i = 0; i < iRet; i++)
	{
		memcpy(ucMac+i*6, pSlave[i].mac, 6);
		memcpy(gpInquiryDevMac+i*6, pSlave[i].mac, 6);
		Display(("Devic[%d]: %02x%02x%02x%02x%02x%02x, %s\r\n", 
			i, pSlave[i].mac[0], pSlave[i].mac[1], pSlave[i].mac[2], 
			pSlave[i].mac[3], pSlave[i].mac[4], pSlave[i].mac[5], pSlave[i].name));
		
	}
	showAllDevInfo(ucMac, iRet);
/*
	for(i = 0; i < iRet; i++)
	{
		Display(("Deve_%d:%02x:%02x:%02x:%02x:%02x:%02x\r\n", i, 
			pSlave[i].mac[5], pSlave[i].mac[4], pSlave[i].mac[3], 
			pSlave[i].mac[2], pSlave[i].mac[1], pSlave[i].mac[0]));
	}
*/
}

void ApiTestConnect(void)
{
	int i, iRet;
	ST_BT_SLAVE slave;
	
	memset(&slave, 0x00, sizeof(slave));
	
	iRet = showAllDevInfo(gpInquiryDevMac, gpInquiryDevCount);
	if(iRet < 0)
	{
		ScrCls();
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 3, 0x00, "Connect To<%d>:", iRet);
	for(i = 0; i < 6; i++)
	{
		slave.mac[i] = gpInquiryDevMac[6*iRet+i];
	}
	ScrPrint(0, 5, 0x00, "%02x:%02x:%02x:%02x:%02x:%02x",
		slave.mac[0], slave.mac[1], slave.mac[2], 
		slave.mac[3], slave.mac[4], slave.mac[5]);
	
	if(getkey() == KEYCANCEL)
		return ;
	
	iRet = BtConnect(&slave);

	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "Connect Success");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "Connect Err <%d>", iRet);
	}
	ScrPrint(0, 5, 0x00, "getkey exit");
	getkey();
}

void ApiTestBtDisconnect(void)
{
	int iRet;
	ST_BT_SLAVE slave;
	
	iRet = BtDisconnect();

	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "Disconnect Success");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "Disconnect Err <%d>", iRet);
	}
	ScrPrint(0, 5, 0x00, "getkey exit");
	getkey();
}

void ApiTestBtCtrl_SetPairMode(void)
{
	int iRet;
	unsigned char ucKey;
	unsigned int pIn[10];
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "Select Mode");
	ScrPrint(0, 1, 0x00, "1.NonPairableMode");
	ScrPrint(0, 2, 0x00, "2.PairableMode");
	ScrPrint(0, 3, 0x00, "3.EnableSecureSimplePair");
	while(1)
	{
		ucKey = getkey();
		if(ucKey == KEY1 || ucKey == KEY2 || ucKey == KEY3)
			break;
		else if(ucKey == KEYCANCEL)
			return ;
	}
	pIn[0] = ucKey - KEY1;
	iRet = BtCtrl(2, pIn, 1, 0, 0);
	if(iRet == 0)
	{
		ScrPrint(0, 5, 0x00, "SetMode Success");
	}
	else
	{
		ScrPrint(0, 5, 0x00, "SetMode <Err:%d>", iRet);
	}
	getkey();
}


void ApiTestBtCtrl_SetSSPParam(void)
{
	unsigned char ucKey;
	int i, iRet;
	unsigned int pIn[10];
	
	struct spp_param{
		int mode;
		char info[32];
	} param[] = {
		0, "DisplayOnly",
		1, "DisplayYesNo",
		2, "KeyboardOnly",
		3, "NoInputNoOutput",
	};
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "Set SPP Param");
	for(i = 0; i < sizeof(param)/sizeof(param[0]); i++)
		ScrPrint(0, i+1, 0x00, "%d.%s", param[i].mode+1, param[i].info);
	while(1)
	{
		ucKey = getkey();
		if(ucKey == KEYCANCEL)
			return; 
		
		if (ucKey == KEY1 || ucKey == KEY2 || ucKey == KEY3 || ucKey == KEY4)
			break;
	}
	ScrCls();
	ScrPrint(0, 0, 0x00, "Set SSP Param");
	pIn[0] = ucKey-KEY1;
	iRet = BtCtrl(4, pIn, 1, 0, 0);
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "OK");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "Err: %d", iRet);
	}
	getkey();
}


void ApiTestBtCtrl_SSP_Pair(void)
{
	int iRet;
	unsigned int pOut[10];
	unsigned int pIn[10];
	unsigned char ucKey;
	unsigned char str_key[10];
	unsigned int passkey;
	
	memset(pIn, 0x00, sizeof(pIn));
	memset(pOut, 0x00, sizeof(pOut));
	ScrCls();
	ScrPrint(0, 0, 0x00, "Bt Ctrl");
	ScrPrint(0, 2, 0x00, "Check Pair Status...");
	while(1)
	{
		iRet = BtCtrl(7, NULL, 0, (unsigned int *)pOut, 1);
		if(pOut[0] == 0)
			ScrPrint(0, 3, 0x00, "Waiting..."); 
		if(pOut[0] == 1 || pOut[0] == 2)
			break;
		if(kbhit() == 0x00 && getkey() == KEYCANCEL)
			return ;
	}
	if(pOut[0] == 1) /* */
	{
		ScrCls();
		ScrPrint(0, 0, 0x00, "Response Yes/NO");
		ScrPrint(0, 1, 0x00, "1. Yes");
		ScrPrint(0, 2, 0x00, "2. No");
		ScrPrint(0, 4, 0x00, "KeyInfo: %06u", pOut[2]);
		while(1)
		{
			ucKey = getkey();
			if(ucKey == KEYCANCEL)
				return ;
			if(ucKey == KEY1 || ucKey == KEY0)
				break;
		}
		pIn[0] = ucKey - KEY0;
		iRet = BtCtrl(5, pIn, 1, 0, 0);
		if(iRet != 0)
			ScrPrint(0, 2, 0x00, "StackErr:%d", iRet);
		else
			ScrPrint(0, 2, 0x00, "Response:%d", pIn[0]);
		getkey();
	}
	else /* */
	{
		ScrCls();
		ScrPrint(0, 0, 0x00, "Response Passkey");
		ScrGotoxy(0, 1);
		memset(str_key, 0x00, sizeof(str_key));
		iRet = GetString(str_key, 0x24, 6, 10);
		if(iRet != 0)
		{
			ScrPrint(0, 3, 0x00, "GetPasskey Err");
			getkey();
			return ;
		}
		passkey = atoi(str_key+1);
		ScrCls();
		iRet = BtCtrl(6, (unsigned int *)&passkey, 1, 0, 0);
		if(iRet != 0)
			ScrPrint(0, 2, 0x00, "StackErr:%d", iRet);
		else
			ScrPrint(0, 2, 0x00, "Response OK");
		getkey();
	}
}

void ApiTestBtOpenClose(void)
{
	int i = 0;
	int iRet;
	ST_BT_SLAVE pSlave[30];
	//for(i = 0; i < 100; i++)
	while(1)
	{
		if(kbhit() == 0x00 && getkey() == KEYCANCEL)
			break;
		Display(("\r\n************* BtOpen -> BtScan -> BtClose <Times:%d>\r\n", i));
		iRet = BtOpen();
		iRet = BtScan(pSlave, 16, 60);
		iRet = BtClose();
		//DelayMs(1000);
		i++;
	}
	getkey();
}
void ApiTestBtScanLoop(void)
{
	int i, iRet;
	ST_BT_SLAVE pSlave[30];
	unsigned char ucMac[6*BT_INQUIRY_MAX_NUM];
	unsigned int InquiryPeriodLength;
	unsigned int MaximumResponses;
	char str[100];
	int CntErr, CntOK;
		
	ScrCls();
	ScrPrint(0, 0, 0x00, "InquiryPeriod:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	InquiryPeriodLength = atoi(str+1);

	ScrCls();
	ScrPrint(0, 0, 0x00, "MaximumResponses:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	MaximumResponses = atoi(str+1);

	ScrCls();
	ScrPrint(0, 1, 0x00, "Time:%d,Cnt:%d", InquiryPeriodLength, MaximumResponses);
	if(getkey() == KEYCANCEL)
		return ;
	i = 0;
	CntOK = 0;
	CntErr = 0;
	while(1)
	{
		ScrCls();
		ScrPrint(0, 1, 0x00, "BtScan...");
		if(kbhit() == 0x00 && getkey() == KEYCANCEL)
			break;
		Display(("\r\n-----------> <TimeDone:%d> BtScan <Cnt:%d>\r\n", BtGetTimerCount(), i));
		ScrPrint(0, 3, 0x00, "Time:%d,Cnt:%d", InquiryPeriodLength, MaximumResponses);
		ScrPrint(0, 5, 0x00, "Cnt:%d", i);
		ScrPrint(0, 7, 0x00, "OK:%d, Err:%d", CntOK, CntErr);
		iRet = BtScan(pSlave, MaximumResponses, InquiryPeriodLength);
		if(iRet <= 0)
		{
			ScrPrint(0, 3, 0x00, "Scan Err<%d>", iRet);
			ScrPrint(0, 5, 0x00, "getkey exit");
			CntErr++;
			Display(("********************* BtScan <Cnt:%d, Err:%d> *******************\r\n", i, iRet));
			//getkey();
			//return ; 
		}
		else
		{
			CntOK++;
			ScrPrint(0, 3, 0x00, "Scan success!<%d>", iRet);
			ScrPrint(0, 5, 0x00, "getkey show info");		
		}
		i++;
	}
	getkey();
	gpInquiryDevCount = iRet;
	for(i = 0; i < iRet; i++)
	{
		memcpy(ucMac+i*6, pSlave[i].mac, 6);
		memcpy(gpInquiryDevMac+i*6, pSlave[i].mac, 6);
		Display(("Devic[%d]: %02x%02x%02x%02x%02x%02x, %s\r\n", 
			i, pSlave[i].mac[0], pSlave[i].mac[1], pSlave[i].mac[2], 
			pSlave[i].mac[3], pSlave[i].mac[4], pSlave[i].mac[5], pSlave[i].name));
		
	}
	showAllDevInfo(ucMac, iRet);
/*
	for(i = 0; i < iRet; i++)
	{
		Display(("Deve_%d:%02x:%02x:%02x:%02x:%02x:%02x\r\n", i, 
			pSlave[i].mac[5], pSlave[i].mac[4], pSlave[i].mac[3], 
			pSlave[i].mac[2], pSlave[i].mac[1], pSlave[i].mac[0]));
	}
*/
}

void ApiTestBtProfileOpenCloseLoop(void)
{
	int i = 0;
	int iRet;
	while(1)
	{
		if(kbhit() == 0x00 && getkey() == KEYCANCEL)
			break;
		Display(("\r\n************* Profile Open <-> ProfileClose <Times:%d>\r\n", i));
		s_WltProfileOpen(WLT_PROFILE_ALL);
		DelayMs(500);
		s_WltProfileClose(WLT_PROFILE_ALL);
		DelayMs(500);
		i++;
	}
	getkey();	
}

void ApiTestBtPortSends(void)
{
	int i, iDataLen, iRet;
	int SerialPortID;
	char str[1024];

	ScrCls();
	ScrPrint(0, 0, 0x00, "DataLen:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 4);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	iDataLen = atoi(str+1);

	ScrCls();
	ScrPrint(0, 2, 0x00, "iDataLen:%d", iDataLen);
	if(getkey() == KEYCANCEL)
		return ;

	for(i = 0; i < iDataLen && i < sizeof(str); i++)
	{
		str[i] = i % 10 + '0';
	}

	iRet = BtPortSends(str, iDataLen);
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "BtPortSends Success!!");
		ScrPrint(0, 5, 0x00, "BtPortSends<Len:%d>", iDataLen);
	}
	else
	{
		ScrPrint(0, 3, 0x00, "BtPortSends Err <%d>", iRet);
	}
	getkey();
}

void ApiTestBtPortRecvs(void)
{
	int iRet;
	char str[8192];
	int iDataLen;
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "DataLen:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 4);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	iDataLen = atoi(str+1);

	ScrCls();
	ScrPrint(0, 2, 0x00, "iDataLen:%d", iDataLen);
	if(getkey() == KEYCANCEL)
		return ;
	
	iRet = BtPortRecvs(str, iDataLen, 3000);
	if(iRet > 0)
	{
		ScrPrint(0, 3, 0x00, "BtPortRecvs Success!!");
		ScrPrint(0, 5, 0x00, "BtPortRecvs<Len:%d>", iRet);		
	}
	else
	{
		ScrPrint(0, 3, 0x00, "BtPortRecvs Err<%d>", iRet);
	}
	getkey();
}

void ApiTestBtPortTxPoolCheck(void)
{
	int iRet;

	ScrCls();
	iRet = BtPortTxPoolCheck();
	ScrPrint(0, 3, 0x00, "Check:%d", iRet);
	getkey();
}

void ApiTestPortSendsRecvs(void)
{
	int i, iRet;
	int iSentBytes, iRecvBytes, iAllCnt;
	int iOK, iFailed, iSendErr, iRecvErr;
	char szSend[10000], szRecv[10000];
	unsigned char ucStatus[7];
	T_SOFTTIMER Time;
	uint t0;
	ST_BT_STATUS BtStatus;
	iOK = 0;
	iFailed = 0;
	iSendErr = 0;
	iRecvErr = 0;
	iAllCnt = 0;
	iSentBytes = 1024;
	
	/*
	iRet = WltBtIoCtrl(eBtCmdGetLinkStatus, NULL, 0, (unsigned char *)ucStatus, sizeof(ucStatus));
	if(iRet != 0 || ucStatus[0] != 1)
	{
		ScrPrint(0, 3, 0x00, "BtDevice Not Connect!");
		ScrPrint(0, 5, 0x00, "getkey exit");
		getkey();
		return ;
	}
	*/

	iRet = BtGetStatus(&BtStatus);
	if(iRet != 0 || BtStatus.status != 1)
	{
		ScrPrint(0, 3, 0x00, "BtDevice Not Connect!");
		ScrPrint(0, 5, 0x00, "getkey exit");
		getkey();
		return ;
	}

	while(1)
	{
		//if (kbhit() == 0 && getkey() == KEYCANCEL)
		//	break;
		
		ScrCls();
		iAllCnt++;
		for(i=0; i<iSentBytes; i++)
		{
			szSend[i] = iAllCnt % 10 + '0';
		}
			
		ScrPrint(0, 1, 0x00, "Cnt=%d,bytes=%d", iAllCnt, iSentBytes*iAllCnt);
		iRet = BtPortSends(szSend, iSentBytes);
		if (iRet != 0)
		{
			Display(("BtPortSends failed = %02x\r\n", iRet));
			iSendErr++;
			continue;
		}
		ScrPrint(0, 3, 0x00, "BtPortSend <DataLen: %d>", iSentBytes);
		//while(BtPortTxPoolCheck())
		//	;
	//	s_TimerSetMs(&Time, 30000);
		t0 = GetMs();
		iRecvBytes = 0;
		memset (szRecv, 0, sizeof(szRecv));
		while(1)
		{
			if (kbhit()==0 && getkey() == KEYCANCEL)
				goto end;

			if(GetMs() - t0 > 30000)
		//	if (s_TimerCheckMs(&Time) == 0)
			{
				Display(("Receive timeout\r\n"));
				iRecvErr++;
				break;
			}
			
			iRet = BtPortRecvs(szRecv+iRecvBytes, iSentBytes-iRecvBytes, 2000);
			if (iRet < 0)
			{
				Display(("BtPortRecvs failed = %d\r\n", iRet));
				iRecvErr++;
				continue;
			}
			else
			{
				Display(("BtPortRecvs <DataLen = %d>\r\n", iRet));
				iRecvBytes += iRet;

				if (iRecvBytes == iSentBytes)
				{
					if (memcmp(szSend, szRecv, iSentBytes) == 0)
					{
						Display(("data is same\r\n"));
						//Beep();
						iOK++;
					}
					else
					{
						Display(("data is NOT same!!!\r\n"));
						Display(("Send Data::\r\n"));
						logDataVal(szSend, 1024);
						Display(("Recv Data::\r\n"));
						logDataVal(szRecv, 1024);
						iFailed++;
					}
					break;
				}
			}
		}
		ScrPrint(0, 5, 0x00, "iOK = %d, iFailed = %d, iSendErr = %d, iRecvErr = %d\r\n", iOK, iFailed, iSendErr, iRecvErr);
		Display(("iOK = %d, iFailed = %d, iSendErr = %d, iRecvErr = %d\r\n", iOK, iFailed, iSendErr, iRecvErr));
		DelayMs(200);
	}
end:	
	ScrPrint(0, 4, 0x00, "iOK = %d, iFailed = %d, iSendErr = %d, iRecvErr = %d\r\n", iOK, iFailed, iSendErr, iRecvErr);
	ScrPrint(0, 7, 0x00, "getkey exit");
	Display(("iOK = %d, iFailed = %d, iSendErr = %d, iRecvErr = %d\r\n", iOK, iFailed, iSendErr, iRecvErr));
	getkey();
	getkey();
}

// TODO:
void LibTestMainThread(void)
{
	int iRet;

	iRet = MainThread(NULL);
	if(iRet == 0x00)
	{
		ScrPrint(0, 3, 0x00, "MainThread Success");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "MainThread <Err:%d>", iRet);		
	}
	getkey();
}
// TODO:
void LibTestSetLocalName(void)
{
	int iRet;
	char name[100];

	memset(name, 0x00, sizeof(name));
	ScrCls();
	ScrPrint(0, 0, 0x00, "input name:");
	ScrGotoxy(0, 1);
	iRet = GetString(name, 0x35, 1, 64);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	
	iRet = SetLocalDeviceName(name);
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "SetName Success!!");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "SetName Err <%d>!", iRet);		
	}
	getkey();
}

void LibTestGetLocalName(void)
{
	int iRet;
	unsigned char name[256+1];
	
	memset(name, 0x00, sizeof(name));
	ScrCls();
	iRet = GetLocalDeviceName(name);
	if(iRet == 0x00)
	{
		ScrPrint(0, 2, 0x00, "GetName Success!");
		ScrPrint(0, 4, 0x00, "Name:%s", name);
	}
	else
	{
		ScrPrint(0, 3, 0x00, "GetName Err <%d>!", iRet);
	}
	getkey();
}
void LibTestGetLocalAddress(void)
{
	int i, iRet;
	BD_ADDR_t BD_ADDR;

	ScrCls();
	iRet = GetLocalAddress(&BD_ADDR);
	if(iRet == 0)
	{
		ScrPrint(0, 2, 0x00, "GetLocalAddr :");
		ScrPrint(0, 4, 0x00, "%02x:%02x:%02x:%02x:%02x:%02x",
			BD_ADDR.BD_ADDR5, BD_ADDR.BD_ADDR4, BD_ADDR.BD_ADDR3,
			BD_ADDR.BD_ADDR2, BD_ADDR.BD_ADDR1, BD_ADDR.BD_ADDR0);
	}
	else
	{
		ScrPrint(0, 3, 0x00, "GetLocalAddr <Err:%d>", iRet);
	}
	getkey();
}
void LibTestGetRemoteName(void)
{
	int i, iRet;
	char name[100];
	unsigned char mac[6];
	BD_ADDR_t BD_ADDR;
	
	iRet = showAllDevInfo(gpInquiryDevMac, gpInquiryDevCount);
	if(iRet < 0)
	{
		ScrCls();
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 0, 0x00, "Connect To<%d>:", iRet);
	for(i = 0; i < 6; i++)
	{
		mac[i] = gpInquiryDevMac[6*iRet+i];
	}
	ScrPrint(0, 1, 0x00, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
	BD_ADDR.BD_ADDR5 = mac[5];
	BD_ADDR.BD_ADDR4 = mac[4];
	BD_ADDR.BD_ADDR3 = mac[3];
	BD_ADDR.BD_ADDR2 = mac[2];
	BD_ADDR.BD_ADDR1 = mac[1];
	BD_ADDR.BD_ADDR0 = mac[0];
	if(getkey() == KEYCANCEL)
		return ;
	
	iRet = GetRemoteDeviceName(BD_ADDR);
	if(iRet == 0)
	{
		ScrPrint(0, 4, 0x00, "GetRemoteName Success");
	}
	else
	{
		ScrPrint(0, 5, 0x00, "GetRemoteName <Err:%d>", iRet);
	}
	getkey();
}

// TODO: 
void LibTestSetDiscoveryMode(void)
{
	int iRet;
	unsigned char ucKey;
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "Select Mode");
	ScrPrint(0, 1, 0x00, "1.NonDiscoverable");
	ScrPrint(0, 2, 0x00, "2.LimitedDiscoverable");
	ScrPrint(0, 3, 0x00, "3.GeneralDiscoverable");
	while(1)
	{
		ucKey = getkey();
		if(ucKey == KEY1 || ucKey == KEY2 || ucKey == KEY3)
			break;
		else if(ucKey == KEYCANCEL)
			return ;
	}
	iRet = SetDiscoverabilityMode(ucKey-KEY1);
	if(iRet == 0)
	{
		ScrPrint(0, 5, 0x00, "SetMode Success");
	}
	else
	{
		ScrPrint(0, 5, 0x00, "SetMode <Err:%d>", iRet);
	}
	getkey();
}
void LibTestSetConnectMode(void)
{
	int iRet;
	unsigned char ucKey;
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "Select Mode");
	ScrPrint(0, 1, 0x00, "1.NonConnectableMode");
	ScrPrint(0, 2, 0x00, "2.ConnectableMode");
	while(1)
	{
		ucKey = getkey();
		if(ucKey == KEY1 || ucKey == KEY2)
			break;
		else if(ucKey == KEYCANCEL)
			return ;
	}
	iRet = SetDiscoverabilityMode(ucKey-KEY1);
	if(iRet == 0)
	{
		ScrPrint(0, 5, 0x00, "SetMode Success");
	}
	else
	{
		ScrPrint(0, 5, 0x00, "SetMode <Err:%d>", iRet);
	}
	getkey();	
}
void LibTestSetPairMode(void)
{
	int iRet;
	unsigned char ucKey;
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "Select Mode");
	ScrPrint(0, 1, 0x00, "1.NonPairableMode");
	ScrPrint(0, 2, 0x00, "2.PairableMode");
	ScrPrint(0, 3, 0x00, "3.EnableSecureSimplePair");
	while(1)
	{
		ucKey = getkey();
		if(ucKey == KEY1 || ucKey == KEY2 || ucKey == KEY3)
			break;
		else if(ucKey == KEYCANCEL)
			return ;
	}
	iRet = SetPairabilityMode(ucKey-KEY1);
	if(iRet == 0)
	{
		ScrPrint(0, 5, 0x00, "SetMode Success");
	}
	else
	{
		ScrPrint(0, 5, 0x00, "SetMode <Err:%d>", iRet);
	}
	getkey();
}

void LibTestSetModeLoop(void)
{
	int i = 0;
	int iRet;
	
	while(1)
	{
		if(kbhit() == 0x00 && getkey() == KEYCANCEL)
			break;
		
		Display(("****************** Cnt=%d ****************\r\n", i++));
		
		iRet = SetDiscoverabilityMode(2);
		Display(("SetDiscoverabilityMode(2) <%d>\r\n", iRet));
		iRet = SetPairabilityMode(1); 
		Display(("SetPairabilityMode(1) <%d>\r\n", iRet));
		iRet = SetConnectabilityMode(1);
		Display(("SetConnectabilityMode(1) <%d>\r\n", iRet));

		DelayMs(500);
		
		iRet = SetDiscoverabilityMode(0);
		Display(("SetDiscoverabilityMode(0) <%d>\r\n", iRet));
		iRet = SetPairabilityMode(0); 
		Display(("SetPairabilityMode(0) <%d>\r\n", iRet));
		iRet = SetConnectabilityMode(0);
		Display(("SetConnectabilityMode(0) <%d>\r\n", iRet));
		
		DelayMs(500);
	}
}

// TODO: GAP(Generic Access Profile) API
void LibTestConnectionInquiry(void)
{
	unsigned int InquiryPeriodLength;
	unsigned int MaximumResponses;
	int i, iRet;
	char str[100];
	unsigned char pMac[6*BT_INQUIRY_MAX_NUM];
	T_SOFTTIMER tTime;
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "InquiryPeriod:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	InquiryPeriodLength = atoi(str+1);

	ScrCls();
	ScrPrint(0, 0, 0x00, "MaximumResponses:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	MaximumResponses = atoi(str+1);

	ScrCls();
	ScrPrint(0, 1, 0x00, "Time:%d,Cnt:%d", InquiryPeriodLength, MaximumResponses);
	if(getkey() == KEYCANCEL)
		return ;
	
	ScrPrint(0, 2, 0x00, "Inquiry...");
	s_WltClearInquiryCnt();
	iRet = ConnectionInquiry(InquiryPeriodLength-1, MaximumResponses);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "Inquiry Err<%d>", iRet);
		ScrPrint(0, 4, 0x00, "getkey exit");
		getkey();
		return ;
	}
	//ScrPrint(0, 3, 0x00, "Inquiry Success");

	s_TimerSetMs(&tTime, InquiryPeriodLength * 1000);
	while(1)
	{
		if(s_TimerCheckMs(&tTime) == 0)
		{
			ScrPrint(0, 3, 0x00, "Inquiry Err<%d>", iRet);
			ScrPrint(0, 4, 0x00, "getkey exit");
			getkey();
			return ;
		}
		if((iRet = s_WltGetInquiryCnt()) != 0)
			break;
	}
	iRet = s_WltGetInquiryDev(pMac, iRet);
	Display(("s_WltGetInquiryDev <iRet = %d>\r\n", iRet));
	for(i = 0; i < iRet; i++)
	{
		Display(("Dev[%d]: %02x:%02x:%02x:%02x:%02x:%02x\r\n", i, 
			pMac[i*6+5], pMac[i*6+4], pMac[i*6+3], pMac[i*6+2], pMac[i*6+1], pMac[i*6+0]));
	}
	gpInquiryDevCount = iRet;
	memcpy(gpInquiryDevMac, pMac, sizeof(gpInquiryDevMac));

	ScrPrint(0, 4, 0x00, "Inquiry Complete");
	ScrPrint(0, 5, 0x00, "getkey show info");
	getkey();

	showAllDevInfo(gpInquiryDevMac, gpInquiryDevCount);	
}

void LibTestInquiryGetName(void)
{
	ST_BT_SLAVE pSlave[100];
	unsigned int InquiryPeriodLength;
	unsigned int MaximumResponses;
	int i, iRet;
	char str[100];
	unsigned char pMac[6*BT_INQUIRY_MAX_NUM];
	T_SOFTTIMER tTime;
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "InquiryPeriod:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	InquiryPeriodLength = atoi(str+1);

	ScrCls();
	ScrPrint(0, 0, 0x00, "MaximumResponses:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	MaximumResponses = atoi(str+1);

	ScrCls();
	ScrPrint(0, 1, 0x00, "Time:%d,Cnt:%d", InquiryPeriodLength, MaximumResponses);
	if(getkey() == KEYCANCEL)
		return ;
	ScrPrint(0, 2, 0x00, "Inquiry...");

	iRet = BtScan(pSlave, MaximumResponses, InquiryPeriodLength);
	if(iRet == 0x00)
	{
		ScrPrint(0, 3, 0x00, "Scan_ext Success");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "Scan_ext Err<%d>", iRet);
	}
	getkey();
}
void LibTestConnectionPair(void)
{
	int i, iRet;
	unsigned char mac[6];
	BD_ADDR_t BD_ADDR;
	
	iRet = showAllDevInfo(gpInquiryDevMac, gpInquiryDevCount);
	if(iRet < 0)
	{
		ScrCls();
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 0, 0x00, "Connect To<%d>:", iRet);
	for(i = 0; i < 6; i++)
	{
		mac[i] = gpInquiryDevMac[6*iRet+i];
	}
	ScrPrint(0, 1, 0x00, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
	BD_ADDR.BD_ADDR5 = mac[5];
	BD_ADDR.BD_ADDR4 = mac[4];
	BD_ADDR.BD_ADDR3 = mac[3];
	BD_ADDR.BD_ADDR2 = mac[2];
	BD_ADDR.BD_ADDR1 = mac[1];
	BD_ADDR.BD_ADDR0 = mac[0];
	if(getkey() == KEYCANCEL)
		return ;
	
	iRet = ConnectionPair(BD_ADDR);
	if(iRet == 0)
	{
		ScrPrint(0, 4, 0x00, "ConnectionPair Success");
	}
	else
	{
		ScrPrint(0, 5, 0x00, "ConnectionPair <Err:%d>", iRet);
	}
	getkey();
}
void LibTestConnectionPairCancel(void)
{
	int i, iRet;
	unsigned char mac[6];
	BD_ADDR_t BD_ADDR;
	
	iRet = showAllDevInfo(gpInquiryDevMac, gpInquiryDevCount);
	if(iRet < 0)
	{
		ScrCls();
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 0, 0x00, "Connect To<%d>:", iRet);
	for(i = 0; i < 6; i++)
	{
		mac[i] = gpInquiryDevMac[6*iRet+i];
	}
	ScrPrint(0, 1, 0x00, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
	BD_ADDR.BD_ADDR5 = mac[5];
	BD_ADDR.BD_ADDR4 = mac[4];
	BD_ADDR.BD_ADDR3 = mac[3];
	BD_ADDR.BD_ADDR2 = mac[2];
	BD_ADDR.BD_ADDR1 = mac[1];
	BD_ADDR.BD_ADDR0 = mac[0];
	if(getkey() == KEYCANCEL)
		return ;
	
	iRet = ConnectionPairingCancel(BD_ADDR);
	if(iRet == 0)
	{
		ScrPrint(0, 4, 0x00, "ConnectionPairingCancel Success");
	}
	else
	{
		ScrPrint(0, 4, 0x00, "ConnectionPairingCancel <Err:%d>", iRet);
	}
	getkey();
}

void LibTestBLEStartScanning(void)
{
	int iRet;

	iRet = BLEStartScanning();
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "BLEStartScanning Success");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "BLEStartScanning <Err:%d>", iRet);
	}
	getkey();
}

void LibTestBLEStopScanning(void)
{
	int iRet;

	iRet = BLEStopScanning();
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "BLEStopScanning Success");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "BLEStopScanning <Err:%d>", iRet);
	}
	getkey();
}

// TODO: SPP(Serial Port Profile) API
void LibTestSPPProfileOpen(void)
{
	int iRet;
	unsigned char str[100];
	int ServerPort;
	int SerialPortID;

	ScrCls();
	ScrPrint(0, 0, 0x00, "ServerPort:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ServerPort = atoi(str+1);
	ScrCls();
	ScrPrint(0, 2, 0x00, "ServerPort = %d", ServerPort);
	if(getkey() == KEYCANCEL)
		return ;
	
	SerialPortID = SPPProfileOpen(ServerPort);
	if(SerialPortID > 0)
	{
		ScrPrint(0, 4, 0x00, "SPPProfileOpen Success!");
		ScrPrint(0, 5, 0x00, "SPPProfileOpen(%d)", ServerPort);
		ScrPrint(0, 6, 0x00, "<Ret>SerialPortID = %d", SerialPortID);
	}
	else
	{
		ScrPrint(0, 4, 0x00, "SPPProfileOpen <Err:%d>", SerialPortID);
	}
	getkey();
}

void LibTestSPPProfileClose(void)
{
	int iRet;
	unsigned char str[100];
	int LocalSerialPortID;

	memset(str, 0x00, sizeof(str));
	ScrCls();
	ScrPrint(0, 0, 0x00, "LocalSerialPortID:");
	ScrGotoxy(0, 1);
	iRet = GetString(str, 0x35, 1, 6);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	LocalSerialPortID = atoi(str+1);
	ScrCls();
	ScrPrint(0, 2, 0x00, "LocalSerialPortID = %d", LocalSerialPortID);
	if(getkey() == KEYCANCEL)
		return ;
	
	iRet = SPPProfileCloseByNumbers(LocalSerialPortID);
	if(iRet == 0)
	{
		ScrPrint(0, 4, 0x00, "SPPProfileClose Success!");
		ScrPrint(0, 6, 0x00, "SPPProfileClose(%d)", LocalSerialPortID);
	}
	else
	{
		ScrPrint(0, 4, 0x00, "SPPProfileClose <Err:%d>", iRet);
	}
	getkey();
}

void LibTestSPPConnect(void)
{
	int i, iRet;
	ST_BT_SLAVE slave;
	BD_ADDR_t tRemoteAddr;
	int ServerPort;
	char str[100];

	memset(&slave, 0x00, sizeof(slave));
	
	iRet = showAllDevInfo(gpInquiryDevMac, gpInquiryDevCount);
	if(iRet < 0)
	{
		ScrCls();
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 3, 0x00, "Connect To<%d>:", iRet);
	for(i = 0; i < 6; i++)
	{
		slave.mac[i] = gpInquiryDevMac[6*iRet+i];
	}
	ScrPrint(0, 5, 0x00, "%02x:%02x:%02x:%02x:%02x:%02x",
		slave.mac[5], slave.mac[4], slave.mac[3], 
		slave.mac[2], slave.mac[1], slave.mac[0]);

	tRemoteAddr.BD_ADDR5 = slave.mac[5];
	tRemoteAddr.BD_ADDR4 = slave.mac[4];
	tRemoteAddr.BD_ADDR3 = slave.mac[3];
	tRemoteAddr.BD_ADDR2 = slave.mac[2];
	tRemoteAddr.BD_ADDR1 = slave.mac[1];
	tRemoteAddr.BD_ADDR0 = slave.mac[0];
	if(getkey() == KEYCANCEL)
		return ;

	ScrCls();
	ScrPrint(0, 0, 0x00, "ServerPort:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 6);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ServerPort = atoi(str+1);
	ScrCls();
	ScrPrint(0, 2, 0x00, "ServerPort = %d", ServerPort);
	if(getkey() == KEYCANCEL)
		return ;

	iRet = SPPConnect(tRemoteAddr, ServerPort);
	Display(("SPPConnect(%d) = %d\r\n", ServerPort, iRet));
	if(iRet > 0)
	{
		ScrPrint(0, 3, 0x00, "SPPConnect Success!");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "SPPConnect Err <%d>", iRet);
	}
	
	getkey();
}

void LibTestSPPDisconnect(void)
{
	int iRet;

	iRet = SPPDisconnect();
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "SPPDisconnect Success!!");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "SPPDisconnect <Err:%d>", iRet);
	}
	getkey();
}

void LibTestSPPDisconnectByNumbers(void)
{
	int iRet;
	int SerialPortID;
	char str[100];

	ScrCls();
	ScrPrint(0, 0, 0x00, "SerialPortID:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 6);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	SerialPortID = atoi(str+1);
	ScrCls();
	ScrPrint(0, 2, 0x00, "SerialPortID = %d", SerialPortID);
	if(getkey() == KEYCANCEL)
		return ;

	iRet = SPPDisconnectByNumbers(SerialPortID);
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "SPPDisconnectByNumbers Success!!");
		ScrPrint(0, 5, 0x00, "SerialPortID:%d", SerialPortID);
	}
	else
	{
		ScrPrint(0, 3, 0x00, "SPPDisconnectByNumbers Err <%d>", iRet);
	}
	getkey();
}

void LibTestServiceDiscovery(void)
{
	int i, iRet;
	ST_BT_SLAVE slave;
	BD_ADDR_t tRemoteAddr;

	memset(&slave, 0x00, sizeof(slave));
	
	iRet = showAllDevInfo(gpInquiryDevMac, gpInquiryDevCount);
	if(iRet < 0)
	{
		ScrCls();
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 3, 0x00, "Connect To<%d>:", iRet);
	for(i = 0; i < 6; i++)
	{
		slave.mac[i] = gpInquiryDevMac[6*iRet+i];
	}
	ScrPrint(0, 5, 0x00, "%02x:%02x:%02x:%02x:%02x:%02x",
		slave.mac[5], slave.mac[4], slave.mac[3], 
		slave.mac[2], slave.mac[1], slave.mac[0]);
	
	tRemoteAddr.BD_ADDR5 = slave.mac[5];
	tRemoteAddr.BD_ADDR4 = slave.mac[4];
	tRemoteAddr.BD_ADDR3 = slave.mac[3];
	tRemoteAddr.BD_ADDR2 = slave.mac[2];
	tRemoteAddr.BD_ADDR1 = slave.mac[1];
	tRemoteAddr.BD_ADDR0 = slave.mac[0];
	
	if(getkey() == KEYCANCEL)
		return ;
	
	iRet = ServiceDiscovery(tRemoteAddr, 20);
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "ServiceDiscovery Success!");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "ServiceDiscovery <Err=%d>!", iRet);
	}
	getkey();
}

void LibTestProfileOpenClose(void)
{
	int id1, id2;
	int ret1, ret2;
	int i;
	ScrPrint(0, 3, 0x00, "ProfileOpenClose");
	i = 0;
	while(1)
	{
		if(kbhit() == 0x00 && getkey() == KEYCANCEL)
			break;
		id1 = SPPProfileOpen(1);
		id2 = SPPProfileOpen(1);
		Display(("<%d>id1 = %d, id2 = %d\r\n", i, id1, id2));
		ret1 = SPPProfileCloseByNumbers(id1);
		ret2 = SPPProfileCloseByNumbers(id2);		
		Display(("<%d>ret1 = %d, ret2 = %d\r\n", i, ret1, ret2));
		i++;
		DelayMs(50);
	}
	getkey();
}

void LibTestSPPDataWrite(void)
{
	int i, iDataLen, iRet;
	int SerialPortID;
	char str[1024*10];
	unsigned char ucKey;

	ScrCls();
	ScrPrint(0, 0, 0x00, "ServerPortID:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 4);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	SerialPortID = atoi(str+1);

	ScrCls();
	ScrPrint(0, 0, 0x00, "DataLen:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 4);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	iDataLen = atoi(str+1);

	ScrCls();
	ScrPrint(0, 1, 0x00, "ServerPortId:%d", SerialPortID);
	ScrPrint(0, 2, 0x00, "Cnt:%d", iDataLen);
	if(getkey() == KEYCANCEL)
		return ;

	for(i = 0; i < iDataLen && i < sizeof(str); i++)
	{
		str[i] = i % 10 + '0';
	}
	while(1)
	{
		iRet = SPPDataWrite(str, i, SerialPortID);
		if(iRet >= 0)
		{
			ScrPrint(0, 3, 0x00, "SPPDataWrite Success!!");
			ScrPrint(0, 5, 0x00, "SPPDataWrite<Len:%d>", iRet);
		}
		else
		{
			ScrPrint(0, 3, 0x00, "SPPDataWrite Err <%d>", iRet);
		}

		ucKey = getkey();
		if(ucKey == KEYCANCEL)
			break;
	}

	getkey();
}

// TODO: ISPP(Iphone Serial Port Profile) API
void LibTestISPPProfileOpen(void)
{
	int iRet;
	unsigned char str[100];
	int ServerPort;
	int SerialPortID;

	ScrCls();
	ScrPrint(0, 0, 0x00, "ServerPort:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ServerPort = atoi(str+1);
	ScrCls();
	ScrPrint(0, 2, 0x00, "ServerPort = %d", ServerPort);
	if(getkey() == KEYCANCEL)
		return ;
	SerialPortID = ISPPProfileOpen(ServerPort);
	if(SerialPortID >= 0)
	{
		ScrPrint(0, 4, 0x00, "ISPPProfileOpen Success!");
		ScrPrint(0, 5, 0x00, "ISPPProfileOpen(%d)", ServerPort);
		ScrPrint(0, 6, 0x00, "SerialPortID = %d", SerialPortID);
	}
	else
	{
		ScrPrint(0, 4, 0x00, "ISPPProfileOpen <Err:%d>", SerialPortID);
	}
	getkey();
}

void LibTestISPPProfileClose(void)
{
	int iRet;

	iRet = ISPPProfileClose();
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "ISPPProfileOpen Success");
	}
	else
	{
		ScrPrint(0, 2, 0x00, "ISPPProfileOpen <Err:%d>", iRet);
	}
	getkey();
}

void LibTestISPPPConnect(void)
{
	int i, iRet;
	ST_BT_SLAVE slave;
	BD_ADDR_t tRemoteAddr;
	int ServerPort;
	char str[100];

	memset(&slave, 0x00, sizeof(slave));
	
	iRet = showAllDevInfo(gpInquiryDevMac, gpInquiryDevCount);
	if(iRet < 0)
	{
		ScrCls();
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 3, 0x00, "Connect To<%d>:", iRet);
	for(i = 0; i < 6; i++)
	{
		slave.mac[i] = gpInquiryDevMac[6*iRet+i];
	}
	ScrPrint(0, 5, 0x00, "%02x:%02x:%02x:%02x:%02x:%02x",
		slave.mac[5], slave.mac[4], slave.mac[3], 
		slave.mac[2], slave.mac[1], slave.mac[0]);

	tRemoteAddr.BD_ADDR5 = slave.mac[5];
	tRemoteAddr.BD_ADDR4 = slave.mac[4];
	tRemoteAddr.BD_ADDR3 = slave.mac[3];
	tRemoteAddr.BD_ADDR2 = slave.mac[2];
	tRemoteAddr.BD_ADDR1 = slave.mac[1];
	tRemoteAddr.BD_ADDR0 = slave.mac[0];
	if(getkey() == KEYCANCEL)
		return ;

	ScrCls();
	ScrPrint(0, 0, 0x00, "ServerPort:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 6);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	ServerPort = atoi(str+1);
	ScrCls();
	ScrPrint(0, 2, 0x00, "ServerPort = %d", ServerPort);
	if(getkey() == KEYCANCEL)
		return ;

	iRet = ISPPConnect(tRemoteAddr, ServerPort);
	Display(("ISPPConnect(%d) = %d\r\n", ServerPort, iRet));
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "ISPPConnect Success!");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "ISPPConnect Err <%d>", iRet);
	}
	
	getkey();
}

void LibTestISPPDisconnect(void)
{
	int iRet;

	iRet = ISPPDisconnect();
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "ISPPDisconnect Success!!");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "ISPPDisconnect <Err:%d>", iRet);
	}
	getkey();
}

void LibTestISPPDataWrite(void)
{
	int i, iDataLen, iRet;
	char str[1024];

	ScrCls();
	ScrPrint(0, 0, 0x00, "DataLen:");
	ScrGotoxy(0, 1);
	memset(str, 0x00, sizeof(str));
	iRet = GetString(str, 0x35, 1, 2);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "User Cancel!!");
		getkey();
		return ;
	}
	iDataLen = atoi(str+1);
	ScrCls();
	ScrPrint(0, 2, 0x00, "DataLen:%d", iDataLen);
	if(getkey() == KEYCANCEL)
		return ;

	for(i = 0; i < iDataLen && i < sizeof(str); i++)
	{
		str[i] = i % 10 + '0';
	}

	iRet = ISPPDataWrite(str, i);
	if(iRet > 0)
	{
		ScrPrint(0, 3, 0x00, "ISPPDataWrite Success!!");
		ScrPrint(0, 5, 0x00, "ISPPDataWrite<Len:%d>", iRet);
	}
	else
	{
		ScrPrint(0, 3, 0x00, "ISPPDataWrite Err <%d>", iRet);
	}
	getkey();
}

void LibTestISPPProfileOpenClose(void)
{
	int id1, id2;
	int ret1, ret2;
	int i;
	ScrPrint(0, 3, 0x00, "ProfileOpenClose");
	i = 0;
	while(1)
	{
		if(kbhit() == 0x00 && getkey() == KEYCANCEL)
			break;
		id1 = ISPPProfileOpen(9);
		id2 = ISPPProfileOpen(9);
		Display(("<%d>id1 = %d, id2 = %d\r\n", i, id1, id2));
		ret1 = ISPPProfileClose();
		ret2 = ISPPProfileClose();		
		Display(("<%d>ret1 = %d, ret2 = %d\r\n", i, ret1, ret2));
		i++;
		DelayMs(50);
	}
	getkey();
}

int getStrToBD_ADDR(char *str, BD_ADDR_t *addr)
{
	int i;
	unsigned char tmp1, tmp2;
	unsigned char BD_mac[6];
	
	if(str == NULL || addr == NULL)
		return -1;
	if(strlen(str) != 12)
		return -2;

	for(i = 0; i < 12; i+=2)
	{
		if(str[i] >= '0' && str[i] <= '9')
			tmp1 = str[i] - '0';
		else if(str[i] >= 'a' && str[i] <= 'f')
			tmp1 = str[i] - 'a' + 10;
		else if(str[i] >= 'A' && str[i] <= 'F')
			tmp1 = str[i] - 'A' + 10;
		else
			return -3;

		if(str[i+1] >= '0' && str[i+1] <= '9')
			tmp2 = str[i+1] - '0';
		else if(str[i+1] >= 'a' && str[i+1] <= 'f')
			tmp2 = str[i+1] - 'a' + 10;
		else if(str[i+1] >= 'A' && str[i+1] <= 'F')
			tmp2 = str[i+1] - 'A' + 10;
		else
			return -3;
		
		BD_mac[i/2] = tmp1 * 16 + tmp2;
	}
	addr->BD_ADDR5 = BD_mac[0];
	addr->BD_ADDR4 = BD_mac[1];
	addr->BD_ADDR3 = BD_mac[2];
	addr->BD_ADDR2 = BD_mac[3];
	addr->BD_ADDR1 = BD_mac[4];
	addr->BD_ADDR0 = BD_mac[5];
	return 0;
}

void LibTestConnectionSetSSPParameters(void)
{
	unsigned char ucKey;
	int i, iRet;
	struct spp_param{
		int mode;
		char info[32];
	} param[] = {
		0, "DisplayOnly",
		1, "DisplayYesNo",
		2, "KeyboardOnly",
		3, "NoInputNoOutput",
	};
	
	ScrCls();
	ScrPrint(0, 0, 0x00, "Set SPP Param");
	for(i = 0; i < sizeof(param)/sizeof(param[0]); i++)
		ScrPrint(0, i+1, 0x00, "%d.%s", param[i].mode+1, param[i].info);
	while(1)
	{
		ucKey = getkey();
		if(ucKey == KEYCANCEL)
			return; 
		
		if (ucKey == KEY1 || ucKey == KEY2 || ucKey == KEY3 || ucKey == KEY4)
			break;
	}
	ScrCls();
	ScrPrint(0, 0, 0x00, "Set SSP Param");
	iRet = ConnectionSetSSPParameters(ucKey-KEY1, 1);
	if(iRet == 0)
	{
		ScrPrint(0, 3, 0x00, "OK");
	}
	else
	{
		ScrPrint(0, 3, 0x00, "Err: %d", iRet);
	}
	getkey();
}

void LibTestUserConfirmationResponse(void)
{
	int iRet;
	BD_ADDR_t Remote_BD_ADDR;
	unsigned short mode;
	unsigned char str[100]="38BC1A048F5B";
	unsigned char ucKey;
#if 0	
	ScrCls();
	ScrPrint(0, 0, 0x00, "GetRemote_BD_ADDR");
	ScrGotoxy(0, 1);
	iRet = GetString(str, 0x34|0x80, 12,12);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "GetRemoteAddr Err");
		getkey();
		return ;
	}
#endif
	if(getStrToBD_ADDR(str, &Remote_BD_ADDR) != 0)
	{
		ScrPrint(0, 3, 0x00, "StrToAddr Err");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 0, 0x00, "Response Yes/No");
	ScrPrint(0, 1, 0x00, "1. Yes");
	ScrPrint(0, 2, 0x00, "0. No");
	while(1)
	{
		ucKey = getkey();
		if(ucKey == KEYCANCEL)
			return ;
		if(ucKey == KEY1 || ucKey == KEY0)
			break;
	}
	ScrCls();
	iRet = UserConfirmationResponse(Remote_BD_ADDR, ucKey-KEY0);
	if(iRet != 0)
		ScrPrint(0, 2, 0x00, "StackErr:%d", iRet);
	else
		ScrPrint(0, 2, 0x00, "Response:%d", ucKey-KEY0);
	
	ScrPrint(0, 4, 0x00, "Add: %02x:%02x:%02x:%02x:%02x:%02x",
		Remote_BD_ADDR.BD_ADDR5, Remote_BD_ADDR.BD_ADDR4, Remote_BD_ADDR.BD_ADDR3,
		Remote_BD_ADDR.BD_ADDR2, Remote_BD_ADDR.BD_ADDR1, Remote_BD_ADDR.BD_ADDR0);
	getkey();
}

void LibTestPassKeyResponse(void)
{
	int iRet;
	BD_ADDR_t Remote_BD_ADDR;
	unsigned int passkey;
	unsigned char str[100]="38BC1A048F5B";
	unsigned char str_key[10];
	unsigned char ucKey;
#if 0	
	ScrCls();
	ScrPrint(0, 0, 0x00, "GetRemote_BD_ADDR");
	ScrGotoxy(0, 1);
	iRet = GetString(str, 0x34|0x80, 12,12);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "GetRemoteAddr Err");
		getkey();
		return ;
	}
#endif
	if(getStrToBD_ADDR(str, &Remote_BD_ADDR) != 0)
	{
		ScrPrint(0, 3, 0x00, "StrToAddr Err");
		getkey();
		return ;
	}
	ScrCls();
	ScrPrint(0, 0, 0x00, "Get Passkey");
	ScrGotoxy(0, 1);
	memset(str_key, 0x00, sizeof(str_key));
	iRet = GetString(str_key, 0x24, 6, 10);
	if(iRet != 0)
	{
		ScrPrint(0, 3, 0x00, "GetPasskey Err");
		getkey();
		return ;
	}
	passkey = atoi(str_key+1);
	ScrCls();
	iRet = PassKeyResponse(Remote_BD_ADDR, passkey);
	if(iRet != 0)
		ScrPrint(0, 2, 0x00, "StackErr:%d", iRet);
	else
		ScrPrint(0, 2, 0x00, "Response OK");
	ScrPrint(0, 3, 0x00, "key: %d", passkey);
	ScrPrint(0, 4, 0x00, "Add: %02x:%02x:%02x:%02x:%02x:%02x",
		Remote_BD_ADDR.BD_ADDR5, Remote_BD_ADDR.BD_ADDR4, Remote_BD_ADDR.BD_ADDR3,
		Remote_BD_ADDR.BD_ADDR2, Remote_BD_ADDR.BD_ADDR1, Remote_BD_ADDR.BD_ADDR0);
	getkey();
}

void getAllFileInfo(void)
{
	int i, iRet;
	FILE_INFO file_info[256];

	memset(file_info, 0x00, sizeof(file_info));
	iRet = GetFileInfo(file_info);
	if(iRet > 0)
	{
		Display(("GetFileInfo <iRet: %d>\r\n", iRet));
		for(i = 0; i < iRet; i++)
		{
			Display(("%d, name: %s, len: %d\r\n", i, file_info[i].name, file_info[i].length));
		}
	}
}

void TestBtApi(void)
{
	struct menu_opt tBtApiTestMenu[] =
	{
		"BtDevApiTest", NULL,
		"getfile", getAllFileInfo,
		"BtOpen", ApiTestBtOpen,
		"BtPortOpen", ApiTestBtPortOpen,
		"BtClose", ApiTestBtClose,
		"BtPortClose", ApiTestBtPortClose,
		"BtGetStatus", ApiTestBtGetStatus,
		"BtGetConfig", ApiTestBtGetConfig,
		"BtSetConfig", ApiTestBtSetConfig,
		"BtScan", ApiTestBtScan,
		"BtConnect", ApiTestConnect,
		"BtDisconnect", ApiTestBtDisconnect,
		"BtOpen_CLose", ApiTestBtOpenClose,
		"DataRxTxLoop", ApiTestPortSendsRecvs,
		"ProOpenClose", ApiTestBtProfileOpenCloseLoop,
		"BtPortSends", ApiTestBtPortSends,
		"BtPortRecvs", ApiTestBtPortRecvs,
		"BtPortTxPoolCheck", ApiTestBtPortTxPoolCheck,
		"BtScanLoop", ApiTestBtScanLoop,
		//MENU_ALL,"Profile_xx", "Profile_xx", BtTestOpenClose,
	};
	x_MenuSelect(tBtApiTestMenu, sizeof(tBtApiTestMenu)/sizeof(tBtApiTestMenu[0]));
}

void LibTestOtherApi(void)
{
	struct menu_opt tOtherApiTestMenu[] =
	{
		"OtherApiTest", NULL,
		"SetLocalName", LibTestSetLocalName,
		"GetLocalName", LibTestGetLocalName,
		"GetLocalADDR", LibTestGetLocalAddress,
		"GetRemoteName", LibTestGetRemoteName,
		"SetDiscoveryMode", LibTestSetDiscoveryMode,
		"SetConnectMode", LibTestSetConnectMode,
		"SetPairMode", LibTestSetPairMode,
		"SetModeLoop", LibTestSetModeLoop,
		"SetSPPParam", LibTestConnectionSetSSPParameters,
		"ResponseYes/No", LibTestUserConfirmationResponse,
		"Response Key", LibTestPassKeyResponse,

	};
	x_MenuSelect(tOtherApiTestMenu, sizeof(tOtherApiTestMenu)/sizeof(tOtherApiTestMenu[0]));
}

void LibTestGAPApi(void)
{
	struct menu_opt tGAPApiTestMenu[] =
	{
		"GAP_ApiTest", NULL,
		"MainThread", LibTestMainThread,
		"ConnecteInquiry", LibTestConnectionInquiry,
		"InquriyName", LibTestInquiryGetName,
		"ConnectePair", LibTestConnectionPair,
		"PairCancel", LibTestConnectionPairCancel,
		"ServiceDiscovery", LibTestServiceDiscovery,
		"GetLocalName", LibTestGetLocalName,
		"SetLocalName", LibTestSetLocalName,
		"GetLocalMac", LibTestGetLocalAddress,
	};
	x_MenuSelect(tGAPApiTestMenu, sizeof(tGAPApiTestMenu)/sizeof(tGAPApiTestMenu[0]));
}

void LibTestISPPApi(void)
{
	struct menu_opt tISPPApiTestMenu[] =
	{
		"ISPP_ApiTest", NULL,
		"ISPPProfileOpen", LibTestISPPProfileOpen,
		"ISPPProfileClose", LibTestISPPProfileClose,
		"ConnectionPair", LibTestConnectionPair,
		"ISPPConnect", LibTestSPPConnect,
		"ISPPDisconnect", LibTestISPPDisconnect,
		"ISPPDataWrite", LibTestISPPDataWrite,
		"Open & Close", LibTestISPPProfileOpenClose,
	};
	x_MenuSelect(tISPPApiTestMenu, sizeof(tISPPApiTestMenu)/sizeof(tISPPApiTestMenu[0]));
}

void LibTestSPPApi(void)
{
	struct menu_opt tSppApiTestMenu[] =
	{
		"SPP_ApiTest", NULL,
		"MainThread", LibTestMainThread,
		"SPPProfileOpen", LibTestSPPProfileOpen,
		"SPPProfileClose", LibTestSPPProfileClose,
		"ConnectionPair", LibTestConnectionPair,
		"SPPConnect", LibTestSPPConnect,
		//"SPPDisconnect", LibTestSPPDisconnect,
		"DisconnectByID", LibTestSPPDisconnectByNumbers,
		"ServiceDiscovery", LibTestServiceDiscovery,
		"SPPDataWrite", LibTestSPPDataWrite,
		"Open & Close", LibTestProfileOpenClose,
	};
	x_MenuSelect(tSppApiTestMenu, sizeof(tSppApiTestMenu)/sizeof(tSppApiTestMenu[0]));
}

void WltBtTest(void)
{
	struct menu_opt tWltBtTestMenu[] =
	{
		"Bt Test", NULL,
		"Bt API", TestBtApi,
		"SPP API", LibTestSPPApi,
		"GAP API", LibTestGAPApi,
		"ISPP API", LibTestISPPApi,
		"Other API", LibTestOtherApi,
	};
	
	PortOpen(0, "115200,8,n,1");
	
	x_MenuSelect(tWltBtTestMenu, sizeof(tWltBtTestMenu)/sizeof(tWltBtTestMenu[0]));
}

#endif

