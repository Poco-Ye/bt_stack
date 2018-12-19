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
#include "posapi.h"
#include "platform.h"


extern int giBtDevInited;
extern int giBtPortInited;

extern int bt_open(void);
extern int bt_close(void);
extern tBtCtrlOpt *getBtOptPtr(void);

int BTOpen(void) {
	bt_open();
	// create_bt_task();
	
	return 0;
}

int BTClose(void) {
	int ret;
	ret = bt_close();
	return ret;
}

//classic + ble scan
int BtScanCommon(ST_BT_SLAVE *pSlave, unsigned int Cnt, unsigned int Timeout) {
	int i = 0;
	struct device btScanDdevices[MAX_DEVICES];
	struct le_device bleScanDdevices[LE_MAX_DEVICES];
	int btRet, bleRet, comRet;
	unsigned int btCnt,bleCnt;
	unsigned int btTimeout, bleTimeout;

	// if(!is_rtl8761()) return -255;
	
	if(giBtDevInited == 0) {
		log_error("Bt Dev Not Init");
		return BT_RET_ERROR_NODEV;
	}
	if(pSlave == NULL) {
		log_error("BtScan Error Parameters");
		return BT_RET_ERROR_PARAMERROR;
	}
	
	if(Timeout > MAXIMUM_INQUIRY_LENGTH || Timeout < MINIMUM_INQUIRY_LENGTH || Cnt > MAXIMUM_NUMBER_INQUIRY_RESPONSES || Cnt < MINIMUM_NUMBER_INQUIRY_RESPONSES) {
		log_error("BtScan Error Parameters");
		return BT_RET_ERROR_PARAMERROR;
	}
	
	if(Cnt > BT_INQUIRY_MAX_NUM)		
		Cnt = BT_INQUIRY_MAX_NUM;
	
	if(MINIMUM_INQUIRY_LENGTH > Timeout)
		Timeout = MINIMUM_INQUIRY_LENGTH;
	
	if (Cnt > MAX_DEVICES) {
		Cnt = MAX_DEVICES;
	}
	
	memset(btScanDdevices, 0x00, sizeof(btScanDdevices));
	memset(bleScanDdevices, 0x00, sizeof(bleScanDdevices));
	
	btCnt = (Cnt/2) + (Cnt%2);
	bleCnt = Cnt - btCnt;
	
	btTimeout = (Timeout/2) + (Timeout%2);
	bleTimeout = Timeout - btTimeout;
	
	
	//log_debug("BtScanCommon btCnt:%d bleCnt:%d btTimeout:%d bleTimeout:%d", btCnt, bleCnt, btTimeout, bleTimeout);
	btRet = bt_scan(btScanDdevices, btCnt, btTimeout);
	log_debug("btscan i:%d btRet:%d name:%s mac:%s", i, btRet, btScanDdevices[0].name, bd_addr_to_str(btScanDdevices[0].address));
	for (i = 0 ; i < btRet; i++) {
		memcpy(pSlave[i].name, btScanDdevices[i].name, MAX_NAME_LEN);
		bd_addr_copy(pSlave[i].mac, btScanDdevices[i].address);
		log_debug("btscan i:%d btRet:%d name:%s mac:%s", i, btRet, pSlave[i].name, bd_addr_to_str(pSlave[i].mac));
	}

	//log_debug("btscan finish btRet:%d", btRet);
	
	bleRet = ble_scan(bleScanDdevices, bleCnt, bleTimeout);
	//log_debug("blescan i:%d btRet:%d name:%s mac:%s", i, bleRet, bleScanDdevices[0].name, bd_addr_to_str(bleScanDdevices[0].address));
	for (i = 0 ; i < bleRet; i++) {
		// memcpy(pSlave[i].name, bleScanDdevices[i].name, LE_MAX_NAME_LEN);
		strcpy(pSlave[i+btRet].name, bleScanDdevices[i].name);
		bd_addr_copy(pSlave[i+btRet].mac, bleScanDdevices[i].address);
		log_debug("blescan i:%d btRet:%d name:%s mac:%s", i, bleRet, pSlave[i+btRet].name, bd_addr_to_str(pSlave[i+btRet].mac));
	}
	
	//log_debug("blescan finish bleRet:%d", bleRet);

	comRet = btRet + bleRet;
	
	//resume_bt_task();
	
	return comRet;
} 

static int classicRole;
static int bleClientRole;

int BtConnectCommon(ST_BT_SLAVE *Slave) {
	int iRet;
	int bleRet;
	
	#if 1
	iRet = getDeviceIndexForAddressExt(Slave->mac);
	bleRet = leGetDeviceIndexForAddressExt(Slave->mac);
	log_debug("Slave->mac:%s", bd_addr_to_str(Slave->mac));
	log_debug("getDeviceIndexForAddressExt iRet:%d bleRet:%d", iRet, bleRet);
	if(0 <= iRet) {
		BTConnect(Slave);
		classicRole = 1;		
	} 
	
	if(0 <= bleRet) {
		BleConnect(Slave);
		bleClientRole = 1;
	}
	#endif
	return BT_RET_OK;
}

int BtSendCommon(void* data, size_t size) {
	int iRet, bleRet;	

	// create_bt_task();
	
	if(1 == classicRole) {
		//classic server/client
		log_debug("bt_rfcomm_send begin");
		bt_rfcomm_send(data, size);
	} 
	
	if(1 == bleClientRole) {
		//ble client 
		log_debug("ble_send begin");
		ble_send(data, size);
	} else {
		//ble server
		log_debug("ble_server_send begin");
		// ble_server_send(data, size);
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

int BtDisconnect(void) {
	bt_disconnect();
	ble_disconnect();
}

int BTDisconnect(void) {
	// log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	bt_disconnect();
	return 0;
}


int BTSend(void* data, size_t size) {
	// log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	//create_bt_task();  //error:rfcomm_send_internal: l2cap cannot send now
	// resume_bt_task();
	log_debug("BTsend size:%d", size);
	bt_rfcomm_send(data, size);
	return 0;
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
	BTRunOnce();
	
	return 0;
}

int BleDisconnect(void) {
	// log_info("[%s] mac: %s\n", __func__, bd_addr_to_str(Slave->mac));
	ble_disconnect();
	return 0;
}

int BleSend(void* data, size_t size) {
	log_info("BleSend begin");
	ble_send(data, size);
	return 0;
}

void BleServerSend(void* data, size_t size) {
	ble_server_send(data, size);
}



//com API
unsigned char BtPortOpen(void)
{
	return BtUserPortOpen();
}


unsigned char BtPortClose(void)
{
	return BtUserPortClose();
}


unsigned char BtPortSend(unsigned char ch)
{
	return BtUserPortSend(ch);
}

unsigned char BtPortRecv(unsigned char *ch, unsigned int ms)
{
	return BtUserPortRecv(ch, ms);
}

extern void create_bt_send_data_task(void);
extern int gSendComplete;
int num_send = 0;
unsigned char BtPortSends(unsigned char *szData, unsigned short iLen)
{
	
	//gSendComplete = 0;
	// create_bt_data_task();
	// create_bt_timer_task();
	// if(num_send == 0) {
		// create_bt_send_data_task();
		// num_send++;
		// log_debug("zf:create_bt_send_data_task end num_send:%d", num_send);
	// }
	return BtUserPortSends(szData, iLen);
}

int BtPortRecvs(unsigned char *szData, unsigned short usLen, unsigned short ms)
{
	// if(num_send == 0) {
		// create_bt_task();
		// num_send++;
	// }
	
	return BtUserPortRecvs(szData, usLen, ms);
}

unsigned char BtPortReset(void)
{
	return BtUserPortReset();
}

int BtPortPeep(unsigned char *szData, unsigned short iLen)
{
	return BtUserPortPeep(szData, iLen);
}

unsigned char BtPortTxPoolCheck(void)
{
	return BtUserPortTxPoolCheck();
}


//test
#if 1
static bd_addr_t gpInquiryDevMac[BT_INQUIRY_MAX_NUM];
static int gpInquiryDevCount = 0;


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


void logDataVal(char *data, int len)
{
	int i;
	for(i = 0; i < len; i++)
	{
		if(i != 0 && i % 16 == 0)
		log_debug("%c ", data[i]);
	}
}


void getAllFileInfo(void)
{
	int i, iRet;
	FILE_INFO file_info[256];

	memset(file_info, 0x00, sizeof(file_info));
	iRet = GetFileInfo(file_info);
	if(iRet > 0)
	{
		log_debug("GetFileInfo <iRet: %d>\r\n", iRet);
		for(i = 0; i < iRet; i++)
		{
			log_debug("%d, name: %s, len: %d\r\n", i, file_info[i].name, file_info[i].length);
		}
	}
}

void ApiTestBtOpen(void)
{
	int iRet;
	
	iRet = BTOpen();
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
	iRet = BTClose();
	ScrPrint(0, 3, 0x00, "BTClose = %d", iRet);
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
	
	iRet = bt_get_status(&pStatus);
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
		ScrPrint(0, 5, 0x00, "MAC:%s", bd_addr_to_str(tBtCfg.mac));
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
	char name[MAX_NAME_LEN+2];
	char pin[MAX_PIN_LEN+2];
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


int showAllDevInfo(bd_addr_t *dev, int iCount)
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
			ScrPrint(0, i+1, 0x00, "%d.Dev%d:%s", i, index, bd_addr_to_str(dev[i]));
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

void ApiTestBtScan(void)
{
	int i, iRet;
	ST_BT_SLAVE pSlave[30];
	bd_addr_t ucMac[BT_INQUIRY_MAX_NUM];
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
		
		iRet = BtScanCommon(pSlave, MaximumResponses, InquiryPeriodLength);
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
		// memcpy(ucMac+i*6, pSlave[i].mac, 6);
		// memcpy(gpInquiryDevMac+i*6, pSlave[i].mac, 6);
		bd_addr_copy(ucMac[i], pSlave[i].mac);
		bd_addr_copy(gpInquiryDevMac[i], pSlave[i].mac);
		log_debug("Devic[%d]: %s, %s\r\n", i, bd_addr_to_str(pSlave[i].mac), pSlave[i].name);
		
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

	bd_addr_copy(slave.mac, gpInquiryDevMac[iRet]);
	ScrPrint(0, 5, 0x00, "%s", bd_addr_to_str(slave.mac));
	
	if(getkey() == KEYCANCEL)
		return ;
	
	iRet = BtConnectCommon(&slave);

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

#if 1
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

	iRet = bt_get_status(&BtStatus);
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
		//iRet = BtPortSends("hello", sizeof("hello"));
		if (iRet != 0)
		{
			log_debug("BtPortSends failed = %02x\r\n", iRet);
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

			if(GetMs() - t0 > 3000000)
		//	if (s_TimerCheckMs(&Time) == 0)
			{
				log_debug("Receive timeout\r\n");
				iRecvErr++;
				break;
			}
			
			iRet = BtPortRecvs(szRecv+iRecvBytes, iSentBytes-iRecvBytes, 2000000);
			if (iRet < 0)
			{
				log_debug("BtPortRecvs failed = %d\r\n", iRet);
				iRecvErr++;
				continue;
			}
			else
			{
				log_debug("BtPortRecvs <DataLen = %d>\r\n", iRet);
				iRecvBytes += iRet;

				if (iRecvBytes == iSentBytes)
				{
					if (memcmp(szSend, szRecv, iSentBytes) == 0)
					{
						log_debug("data is same\r\n");
						//Beep();
						iOK++;
					}
					else
					{
						log_debug("data is NOT same!!!\r\n");
						log_debug("Send Data::\r\n");
						logDataVal(szSend, 1024);
						log_debug("Recv Data::\r\n");
						logDataVal(szRecv, 1024);
						iFailed++;
					}
					break;
				}
			}
		}
		ScrPrint(0, 5, 0x00, "iOK = %d, iFailed = %d, iSendErr = %d, iRecvErr = %d\r\n", iOK, iFailed, iSendErr, iRecvErr);
		log_debug("iOK = %d, iFailed = %d, iSendErr = %d, iRecvErr = %d\r\n", iOK, iFailed, iSendErr, iRecvErr);
		DelayMs(200);
	}
end:	
	ScrPrint(0, 4, 0x00, "iOK = %d, iFailed = %d, iSendErr = %d, iRecvErr = %d\r\n", iOK, iFailed, iSendErr, iRecvErr);
	ScrPrint(0, 7, 0x00, "getkey exit");
	log_debug("iOK = %d, iFailed = %d, iSendErr = %d, iRecvErr = %d\r\n", iOK, iFailed, iSendErr, iRecvErr);
	getkey();
	getkey();
}
#endif


void ApiTestBtPortSends(void)
{
	int i, iDataLen, iRet;
	int SerialPortID;
	char str[1024];
	int testNum = 0;

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
		ScrPrint(0, 5, 0x00, "BtPortSends<Len:%d> testNum:%d", iDataLen, testNum);
	}
	else
	{
		ScrPrint(0, 3, 0x00, "BtPortSends Err <%d> testNum:%d", iRet, testNum);
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
		"BtPortRecvs", ApiTestBtPortRecvs,
		"BtPortSends", ApiTestBtPortSends,
		"DataRxTxLoop", ApiTestPortSendsRecvs,
		"BtDisconnect", ApiTestBtDisconnect,
		// "BtOpen_CLose", ApiTestBtOpenClose,
		// "ProOpenClose", ApiTestBtProfileOpenCloseLoop,
		// "BtPortRecvs", ApiTestBtPortRecvs,
		// "BtPortTxPoolCheck", ApiTestBtPortTxPoolCheck,
		// "BtScanLoop", ApiTestBtScanLoop,
		//MENU_ALL,"Profile_xx", "Profile_xx", BtTestOpenClose,
	};
	x_MenuSelect(tBtApiTestMenu, sizeof(tBtApiTestMenu)/sizeof(tBtApiTestMenu[0]));
}


void WltBtTest(void)
{
	struct menu_opt tWltBtTestMenu[] =
	{
		"Bt Test", NULL,
		"Bt API", TestBtApi,
		// "SPP API", LibTestSPPApi,
		// "GAP API", LibTestGAPApi,
		// "ISPP API", LibTestISPPApi,
		// "Other API", LibTestOtherApi,
	};
	
	PortOpen(0, "115200,8,n,1");

	log_debug("WltBtTest begin");
	x_MenuSelect(tWltBtTestMenu, sizeof(tWltBtTestMenu)/sizeof(tWltBtTestMenu[0]));
}
#endif
