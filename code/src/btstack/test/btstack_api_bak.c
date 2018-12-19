#include "btstack_config.h"

#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_link_key_db_fs.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_stdin.h"
#include "hci.h"
#include "hci_dump.h"
#include "btstack.h"
#include "bt_init.h"

//#define MAX_DEVICES 20
// #define MAX_NAME_LEN 64

typedef void (*recv_callback_t)(void *data, size_t size);

recv_callback_t recv_callback = NULL;

// typedef struct {
	// uint8_t name[64];
	// unsigned char mac[6];
	// unsigned char RFU[10];
// }ST_BT_SLAVE;

// typedef struct {
	// unsigned char name[64];
	// unsigned char pin[16];
	// unsigned char mac[6];
	// unsigned char RFU[10];
// } ST_BT_CONFIG;

extern int bt_open(void);
extern int bt_close(void);

int BTOpen(void) {
	bt_open();
	
	return 0;
}

int BTClose(void) {
	bt_close();
}

//classic + ble scan
int BtScanCommon(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int Timeout) {
	int i = 0;
	struct device btScanDdevices[MAX_DEVICES];
	struct le_device bleScanDdevices[LE_MAX_DEVICES];
	int btRet, bleRet, comRet;
	unsigned int btCnt,bleCnt;
	unsigned int btTimeout, bleTimeout;

	if (Cnt > MAX_DEVICES) {
		Cnt = MAX_DEVICES;
	}
	
	btCnt = (Cnt/2) + (Cnt%2);
	bleCnt = Cnt - btCnt;
	
	btTimeout = (Timeout/2) + (Timeout%2);
	bleTimeout = Timeout - btTimeout;
	
	log_debug("BtScanCommon btCnt:%d bleCnt:%d btTimeout:%d bleTimeout:%d", btCnt, bleCnt, btTimeout, bleTimeout);
	btRet = bt_scan(btScanDdevices, btCnt, btTimeout);
	log_debug("btscan i:%d btRet:%d name:%s mac:%s", i, btRet, btScanDdevices[0].name, bd_addr_to_str(btScanDdevices[0].address));
	for (i = 0 ; i < btRet; i++) {
		memcpy(pSlave[i].name, btScanDdevices[i].name, MAX_NAME_LEN);
		bd_addr_copy(pSlave[i].mac, btScanDdevices[i].address);
		log_debug("btscan i:%d btRet:%d name:%s mac:%s", i, btRet, pSlave[i].name, bd_addr_to_str(pSlave[i].mac));
	}

	log_debug("btscan finish btRet:%d", btRet);
	
	bleRet = ble_scan(bleScanDdevices, bleCnt, 5);
	log_debug("blescan i:%d btRet:%d name:%s mac:%s", i, bleRet, bleScanDdevices[0].name, bd_addr_to_str(bleScanDdevices[0].address));
	for (i = 0 ; i < bleRet; i++) {
		// memcpy(pSlave[i].name, bleScanDdevices[i].name, LE_MAX_NAME_LEN);
		strcpy(pSlave[i+btRet].name, bleScanDdevices[i].name);
		bd_addr_copy(pSlave[i+btRet].mac, bleScanDdevices[i].address);
		log_debug("blescan i:%d btRet:%d name:%s mac:%s", i, bleRet, pSlave[i+btRet].name, bd_addr_to_str(pSlave[i+btRet].mac));
	}
	
	log_debug("blescan finish bleRet:%d", bleRet);

	comRet = btRet + bleRet;
	
	return comRet;
} 

int BtConnectCommon(ST_BT_SLAVE *Slave) {
	int iRet;
	
	iRet = getDeviceIndexForAddressExt(Slave->mac);
	log_debug("Slave->mac:%s", bd_addr_to_str(Slave->mac));
	log_debug("getDeviceIndexForAddressExt iRet:%d", iRet);
	if(0 <= iRet) {
		BTConnect(Slave);
	} else {
		BleConnect(Slave);
	}
	
	return 0;
}

int BtSendCommon(ST_BT_SLAVE *Slave, void* data, size_t size) {
	int iRet;
		
	log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	iRet = getDeviceIndexForAddressExt(Slave->mac);
	if(0 <= iRet) {
		bt_rfcomm_send(Slave->mac, data, size);
	} else {
		ble_send(Slave->mac, data, size);
	}

	return 0;
}



//CLASSIC API
int BTScan(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int Timeout) {
	int ret = 0;
	int i = 0;
	struct device scan_devices[MAX_DEVICES];

	if (Cnt > MAX_DEVICES) {
		Cnt = MAX_DEVICES;
	}

	ret = bt_scan(scan_devices, Cnt, Timeout);
	log_debug("btscan i:%d btRet:%d name:%s mac:%s", i, ret, scan_devices[0].name, bd_addr_to_str(scan_devices[0].address));
	for (i = 0 ; i < ret; i++) {
		memcpy(pSlave[i].name, scan_devices[i].name, MAX_NAME_LEN);
		bd_addr_copy(pSlave[i].mac, scan_devices[i].address);
		log_debug("btscan i:%d btRet:%d name:%s mac:%s", i, ret, pSlave[i].name, bd_addr_to_str(pSlave[i].mac));
	}

	return ret;
}

int BTStopScan(void) {
	gap_inquiry_stop();
	return 0;
}

int BTConnect(ST_BT_SLAVE *Slave) {
	log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	bt_connect(Slave->mac);
	return 0;
}

int BTDisconnect(void) {
	// log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	bt_disconnect();
	return 0;
}


int BTSend(ST_BT_SLAVE *Slave, void* data, size_t size) {
	log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	bt_rfcomm_send(Slave->mac, data, size);
	return 0;
}

void BTSetRecvCallback(recv_callback_t callback) {
	recv_callback = callback;
}

void BTRunOnce(void) {
	btstack_run_loop_embedded_execute_once();
}

//BLE API
int BleScan(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int Timeout) {
	struct le_device devices[LE_MAX_DEVICES];

	if (Cnt > LE_MAX_DEVICES) {
		Cnt = LE_MAX_DEVICES;
	}

	int ret = ble_scan(devices, Cnt, Timeout);
	int i = 0;
	log_debug("num:%d pSlave[i].name:%s pSlave[i].mac:%s", ret, devices[0].name, bd_addr_to_str(devices[0].address));
	
	for (i = 0 ; i < ret; i++) {
		//memcpy(pSlave[i].name, devices[i].name, LE_MAX_NAME_LEN);
		strcpy(pSlave[i].name, devices[i].name);
		bd_addr_copy(pSlave[i].mac, devices[i].address);
		log_debug("num:%d pSlave[i].name:%s pSlave[i].mac:%s", ret, pSlave[i].name, bd_addr_to_str(pSlave[i].mac));
	}

	return ret;
}

int BleStopScan(void) {
	gap_stop_scan();
	return 0;
}

int BleConnect(ST_BT_SLAVE *Slave) {
	log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	ble_connect(Slave->mac);
	return 0;
}

int BleDisconnect(void) {
	// log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	ble_disconnect();
	return 0;
}

int BleSend(ST_BT_SLAVE *Slave, void* data, size_t size) {
	log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	ble_send(Slave->mac, data, size);
	return 0;
}

void BleServerSend(void) {
	ble_server_send();
}

#if 0
int BtGetConfig(ST_BT_CONFIG *pCfg) {
	ST_BT_CONFIG deviceInfo[2];
	
	bt_get_config(deviceInfo);
	
	memcpy(pCfg[0].name, deviceInfo[0].name, MAX_NAME_LEN);
	bd_addr_copy(pCfg[0].mac, deviceInfo[0].mac);
	
	return 0;
}


int BtSetConfig(ST_BT_CONFIG *pCfg) {
	bt_set_cofig(pCfg);
	
	return 0;
}
#endif


// int BtClose(void)
// int BtGetConfig(ST_BT_CONFIG *pCfg)
// int BtSetConfig(ST_BT_CONFIG *pCfg)

// int BtConnect(ST_BT_SLAVE *Slave)
// int BtDisconnect(void)
// int BtGetStatus(ST_BT_STATUS *pStatus)
// int BtCtrl(unsigned int iCmd, void *pArgIn, unsigned int iSizeIn, void *pArgOut, unsigned int iSizeOut)
