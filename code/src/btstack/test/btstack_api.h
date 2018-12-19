#ifndef __BTSTACK_APIZF_H__
#define __BTSTACK_APIZF_H__
#include "btstack.h"


int BTOpen(void);
int BTClose(void);

int BtGetConfig(ST_BT_CONFIG *pCfg);
int BtSetConfig(ST_BT_CONFIG *pCfg);

int BtScanCommon(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int Timeout);
int BtConnectCommon(ST_BT_SLAVE *Slave);
int BtSendCommon(ST_BT_SLAVE *Slave, void* data, size_t size);


int BTScan(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int Timeout);

int BTStopScan(void);
int BTConnect(ST_BT_SLAVE *Slave);
int BTDisconnect(void);
int BTSend(ST_BT_SLAVE *Slave, void *data, size_t size);
// void BTSetRecvCallback(recv_callback_t callback);
void BTRunOnce(void);

int BleScan(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int Timeout);
int BleStopScan(void);
int BleConnect(ST_BT_SLAVE *Slave);
int BleDisconnect(void);
int BleSend(ST_BT_SLAVE *Slave, void* data, size_t size);
void BleServerSend(void);


#endif
