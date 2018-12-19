#ifndef __WLT2564_H__
#define __WLT2564_H__

#include "queue1.h"
#include "BTBTypes.h"
#include "GAPAPI.h"
#include "LIB_GAP.h"

#define BT_LIB_VERSION	"022-140616-R"
#define BT_MODULE_NAME	"WLT2564"
#define SET_BT_PAIR_MODE		 "SetBtPairMode"


#define BT_MAC_LEN			(6)
#define BT_PIN_MIN_LEN		(4)
#define BT_PIN_MAX_LEN		(16)
#define BT_NAME_MAX_LEN		(64)

#define BT_INQUIRY_MAX_NUM	(16)

// TODO: 
typedef struct {
	unsigned char name[64];
	unsigned char pin[16];
	unsigned char mac[6];
	unsigned char RFU[10];
} ST_BT_CONFIG;

typedef struct {
	unsigned char name[64];
	unsigned char mac[6];
	unsigned char RFU[10];
}ST_BT_SLAVE;

typedef struct {
	unsigned char status;
	unsigned char name[64];
	unsigned char mac[6];
}ST_BT_STATUS;

#define BT_RET_OK					(0)
#define BT_RET_ERROR_NODEV			(-1)
#define BT_RET_ERROR_PARAMERROR		(-2)
#define BT_RET_ERROR_STACK			(-3)
#define BT_RET_ERROR_TIMEOUT		(-4)
#define BT_RET_ERROR_CONNECTED		(-5)
#define BT_RET_ERROR_STATUSERROR	(-6)
#define BT_RET_ERROR_MFI			(-7)
#define BT_RET_ERROR_NOBASE			(-254)
#define BT_RET_ERROR_BASEINFO		(-255)
//#define BT_RET_ERROR_NOMODULE		(-255)

typedef enum e_connect_t{
	eConnected_None,
	eConnected_SPP,
	eConnected_SPPLE,
	eConnected_ISPP,
}E_ConnectType;

enum {
	eISPP_Session_Close,
	eISPP_Session_Open,
};

typedef char BoardStr_t[15];

typedef struct st_device_info{
	BD_ADDR_t BD_ADDR;
	char DeviceName[64];
}ST_DEVICE_INFO;

typedef struct bt_inqurity_result{
	int iCnt;
	//BD_ADDR_t pBD_ADDR[BT_INQUIRY_MAX_NUM];
	ST_DEVICE_INFO Device[BT_INQUIRY_MAX_NUM];
}ST_BT_INQUIRY_RESULT;

typedef struct {
	int BtConnectOptStep;
	int BtConnectErrCode;

	BD_ADDR_t Connect_BD_ADDR;
	
	E_ConnectType eConnectType;
	unsigned int RemoteSeverPort;
	int RemoteServerType;						// 0:spp  1:ispp
	int LocalSerialPortID;
	int ISPP_Session_state;

	int isServerEnd;
	
	BD_ADDR_t Local_BD_ADDR;
	char LocalName[BT_NAME_MAX_LEN+1];
	char LocalPin[BT_PIN_MAX_LEN+1];

	int SPPProfileOpenRet1;
	int SPPProfileOpenRet2;

	int SPPProfileOpenCnt;
	int ISPPProfileOpenCnt;
	int SPPLEProfileRegisterCnt;
	int BLEAdvertiseStartCnt;

	int InquiryComplete;
	int InquiryGetRemoteNameComplete;
	int PairComplete;
	int ServiceDiscoveryComplete;
	int SPPConnectComplete;
	int ISPPConnectComplete;
	int GetRemoteNameComplete;

	int SSP_TimerCount;
	int SSP_Capability_Type;
	int SSP_Key_Response;
	BD_ADDR_t SSP_PairRemote_BD_ADDR;

	BoardStr_t SPPLE_Addr;
	BD_ADDR_t CurrentSPPLE_BD_ADDR;
	BD_ADDR_t CurrentSPP_BD_ADDR;
	BD_ADDR_t CurrentISPP_BD_ADDR;
	BD_ADDR_t LastPair_BD_ADDR;
	char RemoteName[BT_PIN_MAX_LEN+1];
	
	ST_BT_INQUIRY_RESULT tInqResult;
	
//	T_Queue tRecvQue;		   //the payload will store in this queue
//	T_Queue tSendQue;		   //send payload will store in this queue


	// TODO:
	T_Queue tMasterRecvQue;
	T_Queue tMasterSendQue;
	T_Queue tSlaveRecvQue;
	T_Queue tSlaveSendQue;

	int masterConnected; // 0: ?T???¡§¡é??¡§?, 1: ???¡§¡é??¡§?????¡§|¡§¡§?¨¤?, 2:???¡§¡é??¡§??¨¢¡§¡ä?¡§2
	int slaveConnected;

	int masterSerialPortID;
	int slaveSerialPortID;

	BD_ADDR_t masterConnectedRemoteAddr;
	BD_ADDR_t slaveCOnnectedRemoteAddr;

}tBtCtrlOpt;

#define WLT_PROFILE_SPP		(1<<0)
#define WLT_PROFILE_ISPP	(1<<1)
#define WLT_PROFILE_SPPLE	(1<<2)
#define WLT_PROFILE_BLE		(1<<3)
#define WLT_PROFILE_NULL	(0)
#define WLT_PROFILE_ALL		(WLT_PROFILE_SPP|WLT_PROFILE_ISPP|WLT_PROFILE_SPPLE|WLT_PROFILE_BLE)

typedef struct{
	int iStep;
	int (*pFunc)(tBtCtrlOpt* opt);
}T_StatusBlock;

typedef enum {
	eStateConnectDone = 0,

	eStateConnectPairRestart,
	
	eStateConnectPairStart,
	eStateConnectPairing,
	eStateConnectPairCompleted,

	eStateServiceDiscoveryRestart,
	eStateServiceDiscoveryStart,
	eStateServiceDiscovering,
	eStateServiceDiscoveryCompleted,

	eStateRemoteTypeObtainRestart,
	eStateRemoteTypeObtainStart,
	eStateRemoteTypeObtaining,
	eStateRemoteTypeObtained,
	
	eStateSppConnectRestart,
	eStateSppConnectStart,
	eStateSppConnecting,
	eStateSppConnectCompleted,
}E_BtConnectOptStep;

typedef enum{
	eErrCodeConnectPair = 1,
	eErrCodeServiceDiscovery,
	eErrCodeSppConnect,
}E_BtConnectErrCode;

// TODO:
#if 0
unsigned char BtPortOpen(void);
unsigned char BtPortClose(void);
unsigned char BtPortSend(unsigned char ch);
unsigned char BtPortRecv(unsigned char *ch, unsigned int ms);
unsigned char BtPortReset(void);
unsigned char BtPortSends(unsigned char *szData, unsigned short usLen);
unsigned char BtPortTxPoolCheck(void);
int BtPortPeep(unsigned char *szData, unsigned short usLen);
int BtPortRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms);
#endif

#if 0
unsigned char BtMasterOpen(void);
unsigned char BtMasterClose(void);
unsigned char BtMasterSend(unsigned char ch);
unsigned char BtMasterRecv(unsigned char *ch, unsigned int ms);
unsigned char BtMasterReset(void);
unsigned char BtMasterSends(unsigned char *szData, unsigned short usLen);
unsigned char BtMasterTxPoolCheck(void);
int BtMasterPeep(unsigned char *szData, unsigned short usLen);
int BtMasterRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms);
#endif

#if 0
unsigned char BtSlaveOpen(void);
unsigned char BtSlaveClose(void);
unsigned char BtSlaveSend(unsigned char ch);
unsigned char BtSlaveRecv(unsigned char *ch, unsigned int ms);
unsigned char BtSlaveReset(void);
unsigned char BtSlaveSends(unsigned char *szData, unsigned short usLen);
unsigned char BtSlaveTxPoolCheck(void);
int BtSlavePeep(unsigned char *szData, unsigned short usLen);
int BtSlaveRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms);
#endif

// user
//unsigned char BtUserPortClose(void);
unsigned char BtPortOpen(void);
unsigned char BtPortClose(void);
unsigned char BtPortReset(void);
unsigned char BtPortSends(unsigned char *szData, unsigned short iLen);
unsigned char BtPortTxPoolCheck(void);
int BtPortRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms);
unsigned char BtPortSend(unsigned char ch);
unsigned char BtPortRecv(unsigned char *ch, unsigned int ms);
int BtPortPeep(unsigned char *szData, unsigned short iLen);

// base
unsigned char BtBasePortClose(void);
int BtBaseGetStatus(ST_BT_STATUS *pStatus);
int BtBaseGetErrorCode(void);
unsigned char BtBasePortOpen(void);
unsigned char BtBasePortClose(void);
unsigned char BtBasePortSends(unsigned char *szData, unsigned short iLen);
unsigned char BtBasePortTxPoolCheck(void);
int BtBasePortPeep(unsigned char *szData, unsigned short iLen);
int BtBasePortRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms);
unsigned char BtBaseSend(unsigned char ch);
unsigned char BtBaseRecv(unsigned char *ch, unsigned int ms);
unsigned char BtBasePortReset(void);


// TODO:
int BtOpen(void);
int BtClose(void);
int BtGetConfig(ST_BT_CONFIG *pCfg);
int BtSetConfig(ST_BT_CONFIG *pCfg);
int BtScan(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int TimeOut);
int BtConnect(ST_BT_SLAVE *Slave);
int BtDisconnect(void);
int BtGetStatus(ST_BT_STATUS *pStatus);

//int BtBaseGetStatus(void);

int s_WltGetConnectType(void);
int s_WltGetISPPSessionStatus(void);

int BtCtrl(unsigned int iCmd, void *pArgIn, unsigned int iSizeIn, void *pArgOut, unsigned int iSizeOut);

int s_BtGetVer(unsigned char *Version);

tBtCtrlOpt *getBtOptPtr(void);

#endif

