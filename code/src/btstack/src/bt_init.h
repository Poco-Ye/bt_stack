#ifndef __BT_INIT_H__
#define __BT_INIT_H__
#include "btstack.h"
#include "queue1.h"


#define MAX_DEVICES 16
#define MAX_NAME_LEN 64
#define MAX_PIN_LEN	16

#define BT_INQUIRY_MAX_NUM	(16)

#define LE_MAX_NAME_LEN 14
#define LE_MAX_DEVICES 16

#define MINIMUM_INQUIRY_LENGTH	2
#define MAXIMUM_INQUIRY_LENGTH	61

#define MINIMUM_NUMBER_INQUIRY_RESPONSES	1
#define MAXIMUM_NUMBER_INQUIRY_RESPONSES    255

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

enum DEVICE_STATE { REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, REMOTE_NAME_FETCHED };
struct device {
    bd_addr_t  address;
    uint16_t   clockOffset;
    uint32_t   classOfDevice;
    uint8_t    pageScanRepetitionMode;
	uint8_t            name[MAX_NAME_LEN];
    uint8_t    rssi;
    enum DEVICE_STATE  state; 
};


struct le_device {
	bd_addr_t          address;
	bd_addr_type_t	   addr_type;
	uint8_t            pageScanRepetitionMode;
	uint16_t           clockOffset;
	uint8_t            name[LE_MAX_NAME_LEN];
};

typedef struct {
	unsigned char name[64];
	bd_addr_t mac;
	unsigned char RFU[10];
}ST_BT_SLAVE;

typedef struct {
	uint8_t  name[64];
	unsigned char pin[16];
	bd_addr_t mac;
	unsigned char RFU[10];
} ST_BT_CONFIG;


typedef struct {
	unsigned char status;
	unsigned char name[64];
	unsigned char mac[6];
}ST_BT_STATUS;


typedef struct {
	bd_addr_t Connect_BD_ADDR;
	
	bd_addr_t Local_BD_ADDR;
	char LocalName[MAX_NAME_LEN+1];
	char LocalPin[MAX_PIN_LEN+1];
	
	int Connected;
	int SSP_TimerCount;

	char RemoteName[MAX_NAME_LEN+1];
	T_Queue tMasterRecvQue;
	T_Queue tMasterRecvQueTest;
	T_Queue tMasterRecvQueTestLen;
	T_Queue tMasterSendQue;
	
}tBtCtrlOpt;



int bt_scan(struct device* device, unsigned int size, unsigned int timeout);
void bt_connect(bd_addr_t addr);
void bt_disconnect(void);
void bt_rfcomm_send(void* data, size_t size);
// void bt_rfcomm_send(bd_addr_t addr, void* data, size_t size);

int ble_scan(struct le_device* device, unsigned int size, unsigned int timeout);
void ble_connect(bd_addr_t addr);
void ble_disconnect(void);
void ble_send(void* data, size_t size);
void ble_server_send(void* data, size_t size);
// void ble_send(bd_addr_t addr, void* data, size_t size);
// void ble_server_send(void);

int BtGetConfig(ST_BT_CONFIG *pCfg);
int BtSetConfig(ST_BT_CONFIG *pCfg);

int bt_get_status(ST_BT_STATUS *pStatus);

int getDeviceIndexForAddressExt( bd_addr_t addr);
int leGetDeviceIndexForAddressExt( bd_addr_t addr);

//comm API
unsigned char BtUserPortOpen(void);
unsigned char BtUserPortClose(void);

unsigned char BtPortSend(unsigned char ch);
unsigned char BtPortRecv(unsigned char *ch, unsigned int ms);

unsigned char BtUserPortSends(unsigned char *szData, unsigned short iLen);
int BtUserPortRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms);
int BtUserPortPeep(unsigned char *szData, unsigned short iLen);
unsigned char BtUserPortReset(void);
unsigned char BtUserPortTxPoolCheck(void);
#endif
