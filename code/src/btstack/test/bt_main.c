
// *****************************************************************************
//
// minimal setup for HCI code
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "btstack_config.h"

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_link_key_db_fs.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "bluetooth_company_id.h"
#include "hci.h"
#include "hci_dump.h"
#include "btstack_stdin.h"

//#include "btstack_chipset_bcm.h"
//#include "btstack_chipset_bcm_download_firmware.h"
#include "btstack_chipset_rtl8761a.h"
#include "hciattach.h"


#if 1//shics added
#include "stdlib.h"
#include "posapi.h"
#include "platform.h"
#include "string.h"
#include "base.h"
#include "../../multitask/paxos.h"
#include "../../comm/comm.h"

#include "bt_init.h"
#include "mfi_cp.h"
#include "simI2C.h"

#endif

//#define log_debug LogPrintf
volatile int download_fw_complete = 0;
//shics this function defined in classic_test.c
extern int btstack_main(int argc, const char * argv[]);

static int phase2(int status);
static int RtkPowerSwitch(int OP);

static hci_transport_config_uart_t transport_config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,// 921600,  // main baudrate
    0,       // flow control
    NULL,
};
static btstack_uart_config_t uart_config;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static btstack_uart_block_t * uart_driver;


OS_TASK *pBtTask=NULL;
uint8_t EventTypeTest;

static void local_version_information_handler(uint8_t* packet) {
	log_debug("Local version information:\n");
	uint16_t hci_version = packet[6];
	uint16_t hci_revision = little_endian_read_16(packet, 7);
	uint16_t lmp_version = packet[9];
	uint16_t manufacturer = little_endian_read_16(packet, 10);
	uint16_t lmp_subversion = little_endian_read_16(packet, 12);
	log_debug("- HCI Version    0x%04x\n", hci_version);
	log_debug("- HCI Revision   0x%04x\n", hci_revision);
	log_debug("- LMP Version    0x%04x\n", lmp_version);
	log_debug("- LMP Subversion 0x%04x\n", lmp_subversion);
	log_debug("- Manufacturer 0x%04x\n", manufacturer);
}

static unsigned char hci_cmd_send_data_send_step(void);
static unsigned char hci_cmd_send_data_read_resp_step(void);

typedef struct {
	volatile unsigned char step;
	unsigned char (*func)(void);
}BT_STATE_MACHINE_T;

typedef enum {
	STATE_MACHINE_SEND_DATA_STEP,
	STATE_MACHINE_READ_RESP_STEP,
}BT_STATE_MACHINE_SEND_DATA_STEP_T;

static const BT_STATE_MACHINE_T bt_send_data_state_machine[] =
{
	{STATE_MACHINE_SEND_DATA_STEP, hci_cmd_send_data_send_step},
	{STATE_MACHINE_READ_RESP_STEP, hci_cmd_send_data_read_resp_step},
};

#define SEND_DATA_STATE_MACHINE_STEP_CN (sizeof(bt_send_data_state_machine) / sizeof(bt_send_data_state_machine[0]))

typedef struct 
{
	signed char   sending_data_flag;               /*1:wainting response, 0:no data send, -1:error*/
	int sending_len;                     /*to store length of data to send now*/
	unsigned int  send_data_timeout;
	BT_STATE_MACHINE_SEND_DATA_STEP_T  send_data_state_machine_step;
	BT_STATE_MACHINE_SEND_DATA_STEP_T read_event_state_machine_step;
}BT_STATUS_T;

BT_STATUS_T bt_status;

extern int gSendingData;	
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t addr;
    if (packet_type != HCI_EVENT_PACKET) 
    {
        log_debug("packet_handler: packet_type=%x\r\n, discard!",  packet_type);
	return;
    }
   // log_debug("Test main: packet_handler:%x, state:%02x\r\n",packet_handler, hci_event_packet_get_type(packet));
	EventTypeTest = hci_event_packet_get_type(packet);
	log_debug("zf:packet_handler:EventTypeTest:%d", EventTypeTest);
	if(1 == bt_status.sending_data_flag && EventTypeTest == 19) {
		bt_status.sending_data_flag = 0;
		// gSendingData = 0;
		EventTypeTest = 0;
		log_debug("zf:sending_data_flag:%d send_data_state_machine_step:%d", bt_status.sending_data_flag, bt_status.send_data_state_machine_step);
		// bt_status.send_data_state_machine_step = STATE_MACHINE_SEND_DATA_STEP;
	}
	
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
			log_debug("btstack_event_state_get_state(packet):%d HCI_STATE_WORKING:%d", btstack_event_state_get_state(packet), HCI_STATE_WORKING);
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            gap_local_bd_addr(addr);
            log_debug("BTstack up and running at %s\n",  bd_addr_to_str(addr));
            break;
		case HCI_EVENT_COMMAND_COMPLETE:
			if (HCI_EVENT_IS_COMMAND_COMPLETE(packet,
			                                  hci_read_local_version_information)) {
				local_version_information_handler(packet);
			}

			break;	
		case HCI_EVENT_DISCONNECTION_COMPLETE:
			log_debug("Disconnect, reason %02x\n", hci_event_disconnection_complete_get_reason(packet));
			break;
		
        default:			
            break;
    }
}


extern tBtCtrlOpt *getBtOptPtr(void);				
extern uint16_t  rfcomm_channel_id;

extern int gPortRecvs;
int gEnterRecvRunLoop = 0;		
int gGetRfcomData = 0;;	
int gAclDataPacket = 0;
int gProcessFrame = 0;
int gSendComplete = 0;
int gUIH_PF = 0;
	
		
#if 1		
typedef enum {
	RL_OFF,
	RL_START,
	RL_GETDATA,
	RL_SENDING,
	RL_SENDRESULT,
	RL_SENDNEXT
} gSend_state_t;		
			
static gSend_state_t send_state = RL_OFF;						
#endif

#if 0			
extern int hci_transport_h5_block_received_ext(void);			
int getDataPacketType(void) {
	int ret = 0;
	
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	if(ptBtOpt->Connected) {
		ret = hci_transport_h5_block_received_ext();
		log_debug("zf:hci_transport_h5_block_received_ext:%d", ret);
		return ret;
	}
	
	return 0;
}	
#endif
	
int gRecvedExt;	
void create_bt_block_received_task(void);
void resume_bt_block_received_task(void);
#if 1
int zfTimeTest1,zfTimeTest2,zfTimeTest3,zfNum = 0;
extern int gRecvFrameFinish;
extern uint16_t gframe_size;
int gProcessFrameFinish;
extern int receive_start_ext;
//int timeOver0, timeOver;
//int timeOver1, timeOver2, timeOver3;
extern int gMutiSendingData;
static uint8_t temp_buffer[1024];
static int gRecvingData = 0;
static void run_bt_loop(void) {
	#if 0
	int iLen;
	int iRet = 1;
	unsigned char spp_data[110];
	static int iSPPSend = 0;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	send_state = RL_START;
	#endif
	// uint8_t temp_buffer[1024];
	int n = 0;
	memset(temp_buffer, 0, 1024);
	
	for (;;) {
	// log_debug("begin run_bt_loop..."); 
	//if(ptBtOpt->Connected) {
		//zfTimeTest1 = GetTimerCount();

	//}
	
	#if 1
	if(gRecvFrameFinish) {
		//timeOver1 = GetTimerCount();
		// gframe_size += 2;
		// PortRecvs(7, temp_buffer, gframe_size, 10);
		// for(n = 0; n < 30; n++) {
			// log_debug("n:%d gframe_size:%d temp_buffer:%x", n, gframe_size, temp_buffer[0]);
		// }
		// gframe_size -= 2;
		// gRecvingData = 1;
		// log_debug("zf:run_loop gframe_size:%d", gframe_size);
		hci_transport_h5_process_frame_ext(gframe_size);
		// gframe_size += 2;
		// PortRecvs(7, temp_buffer, gframe_size, 10);

		// for(n = 0; n < gframe_size; n++) {
			// log_debug("n:%d gframe_size:%d temp_buffer:%x", n, gframe_size, temp_buffer[0]);
		// }

		//memset(temp_buffer, 0, 1024);
		//gRecvFrameFinish = 0;
		// gframe_size = 0;
		// gRecvingData = 0;
		//gProcessFrameFinish = 1;
		//timeOver2 = GetTimerCount();
		//timeOver3 = timeOver2 - timeOver1;
		//timeOver = timeOver2 - receive_start_ext;
		//log_debug("time:%d receive_start_ext:%d time3:%d time2:%d time1:%d", timeOver, receive_start_ext, timeOver3, timeOver2, timeOver1);
		//log_debug("run_loop: gRecvFrameFinish:%d gProcessFrameFinish:%d", gRecvFrameFinish, gProcessFrameFinish);
	}
	#endif

	#if 0
	//hci_transport_h5_block_received_ext();
	if(ptBtOpt->Connected && gEnterRecvRunLoop && gAclDataPacket) {	//&& gSendingData == 0
		while(1) {
			
		if((20 == EventTypeTest && gGetRfcomData == 1) || gUIH_PF) {  //(110 == EventTypeTest) 
			log_debug("2222:zhufeng:EventTypeTest:%d gGetRfcomData:%d", EventTypeTest, gGetRfcomData);
			gEnterRecvRunLoop = 0;
			EventTypeTest = 0;
			gAclDataPacket = 0;
			gUIH_PF = 0;
			// resume_bt_block_received_task();
			break;
		}
		// log_debug("1111:EventTypeTest:%d gGetRfcomData:%d", EventTypeTest, gGetRfcomData);
			btstack_run_loop_embedded_execute_once();
		}
	}  
	#endif
	  
	 
	#if 0
	if(ptBtOpt->Connected && !QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue))) {
		while(1) {
			// log_debug("queue:%d",  QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue)));
			if(RL_START == send_state || (RL_SENDNEXT == send_state)) {
				iLen = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, sizeof(spp_data));
				send_state = RL_GETDATA;
			}

			//log_debug("QueGetsWithoutDelete = %d rfcomm_channel_id:%d EventTypeTest:%d data:%s\r\n", iLen, rfcomm_channel_id, EventTypeTest, spp_data);
			
			if((0 == iLen) && (EventTypeTest == 19)) {
				log_debug("zhufeng: out iLen:%d EventTypeTest:%d", iLen, EventTypeTest);
				EventTypeTest = 0;
				gSendComplete = 1;
				gSendingData = 0;
				send_state = RL_START;
				QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
				break;
			}

			if(iLen && (RL_GETDATA == send_state)) {
				iRet = rfcomm_send(rfcomm_channel_id, spp_data, iLen);
				if(iLen && (0 == iRet)) {
					//log_debug("rfcomm_send Seccuss <cnt:%d><len:%d> send_state:%d\r\n", iSPPSend++, iLen, send_state);
					QueGets((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, iLen);
				}	
				send_state = RL_SENDING;
				//log_debug("rfcomm_send iLen:%d rfcomm_channel_id:%d iRet:%d EventTypeTest:%d send_state:%d", iLen, rfcomm_channel_id, iRet, EventTypeTest, send_state);
				
			}
			
			if(RL_SENDING == send_state) {
				btstack_run_loop_embedded_execute_once();	
			}
			
			if(RL_SENDING == send_state && (19 == EventTypeTest)) {	
				send_state = RL_SENDNEXT;
			}


		}  
	} 
	#endif
	  
	// if(!gRecvingData){
		btstack_run_loop_embedded_execute_once();
	// }   


	//if(ptBtOpt->Connected) {
		//zfTimeTest2 = GetTimerCount();
		//zfTimeTest3 = zfTimeTest2 - zfTimeTest1;
		//log_debug("run loop time:%d num:%d", zfTimeTest3, zfNum);
		//zfNum++;

	//}
	OsSleep(1);

  }
}				

void create_bt_task()
{
	log_debug("create_bt_task!!!!!!!\r\n");
	pBtTask = OsCreate((OS_TASK_ENTRY)run_bt_loop,26,0x3000); //TASK_PRIOR_BT_LOOP
	if(pBtTask == NULL)
		log_debug("OsCreate==NULL\r\n");
}

void suspend_bt_task()
{
	log_debug("suspend_bt_task!!!");
	if (pBtTask != NULL) {
		log_debug("suspend BtTask != NULL");
		OsSuspend(pBtTask);
	}else {
		log_debug("suspend BtTask == NULL");
	}
}

void resume_bt_task()
{
	log_debug("resume_bt_task!!!");
	if (pBtTask != NULL) {
		log_debug("OsResume BtTask != NULL");
		OsResume(pBtTask);
	}else {
		log_debug("OsResume BtTask == NULL");
	}

}
#endif


#if 0
extern int hasConnect;
//hci_transport_h5_block_received_ext();
static void bt_block_received_loop(void) {
	int ret;

  for (;;) {
	// log_debug("bt_block_received_loop"); 
  	if(hasConnect && gEnterRecvRunLoop) { //&& gSendingData == 0

		ret = hci_transport_h5_block_received_ext();
		if(1 == ret) {
			gRecvedExt = 1;
			// suspend_bt_block_received_task();
			// resume_bt_task();
		}

	}

	OsSleep(1);
  }
}	

void create_bt_block_received_task(void) {
	log_debug("creat bt_block_received_loop\r\n");
	pBtRecvTask = OsCreate((OS_TASK_ENTRY)bt_block_received_loop,26,0x3000); //TASK_PRIOR_BT_LOOP
	if(pBtRecvTask == NULL)
		log_debug("bt_block_received_loop OsCreate==NULL\r\n");
}

void suspend_bt_block_received_task()
{
	log_debug("suspend_bt_block_received_task!!!");
	if (pBtRecvTask != NULL) {
		log_debug("suspend pBtRecvTask != NULL");
		OsSuspend(pBtRecvTask);
	}else {
		log_debug("suspend pBtRecvTask == NULL");
	}

}

void resume_bt_block_received_task()
{
	log_debug("resume_bt_block_received_task!!!");
	if (pBtRecvTask != NULL) {
		log_debug("OsResume pBtRecvTask != NULL");
		OsResume(pBtRecvTask);
	}else {
		log_debug("OsResume pBtRecvTask == NULL");
	}

}
#endif

#if 1

OS_TASK *pBtSendTask = NULL;

static unsigned int hci_send_data_timer_check(unsigned int timeout)
{
	unsigned int ret;
	// ret = hci_timer_check(bt_status.send_data_timeout, timeout);
	// if(ret == 0)
	// {
		// bt_status.send_data_state_machine_step = STATE_MACHINE_SEND_DATA_STEP;
		// bt_status.sending_data_flag = 0;
	// }

	return 1;
}

extern uint16_t  rfcomm_channel_id;
static unsigned char hci_cmd_send_spp_data(unsigned char buf[], unsigned char len)
{
    unsigned char ret;
	unsigned char length;
	
	length = len;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
    if(ptBtOpt->Connected == 0)
    {
        return 1;
    }
  
   	bt_status.sending_data_flag = 1;
	//log_debug("rfcomm_send begin len:%d length:%d ret:%d", len, length, ret);
    ret = rfcomm_send(rfcomm_channel_id, buf, length);
	log_debug("rfcomm_send end len:%d length:%d ret:%d", len, length, ret);
    if(ret != 0)
    {
		log_debug("rfcomm_send err len:%d ret:%d", len, ret);
		bt_status.sending_data_flag = 0;
        return ret;
    }

    return ret;
} 

static unsigned char hci_cmd_send_data_send_step(void)
{
	int ret;
	int len;
	unsigned char send_data[350];
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(0 == (ptBtOpt->Connected))
	{
		//log_debug("66666 sending_data_flag:%d", bt_status.sending_data_flag);
		QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
		//QueReset((T_Queue*)(&ptBtOpt->tMasterRecvQue));
		bt_status.send_data_state_machine_step = STATE_MACHINE_SEND_DATA_STEP;
		return 1;
	}

	if(bt_status.send_data_state_machine_step != STATE_MACHINE_SEND_DATA_STEP)
	{
		//log_debug("5555 sending_data_flag:%d", bt_status.sending_data_flag);
		return 1;
	}
	
	//log_debug("22 sending_data_flag:%d", bt_status.sending_data_flag);
	if(QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue)))
	{		
		//log_debug("4444 sending_data_flag:%d", bt_status.sending_data_flag);
		// gSendingData = 0;
		return 1;
	}
	
	// if(bt_status.connection_status == 2) // ble connected
	// {
		// len = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tMasterSendQue), (char *)send_data, bt_status.negotiate_le_mtu);
		// ret = hci_cmd_send_ble_data(send_data, len);
	// }
	// else
	// {
		//log_debug("queIsEmpty:%d", QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue)));
		len = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tMasterSendQue), (char *)send_data, sizeof(send_data));
		//log_debug("33 sending_data_flag:%d len:%d", bt_status.sending_data_flag, len);
		if(len) {
			gSendingData = 1;
		}
		//ret = rfcomm_send(rfcomm_channel_id, send_data, len);

		//ret = hci_cmd_send_spp_data(send_data, len);
		bt_status.sending_data_flag = 1;
		//log_debug("rfcomm_send begin len:%d ret:%d", len, ret);
		ret = rfcomm_send(rfcomm_channel_id, send_data, len);
		//log_debug("rfcomm_send end len:%d ret:%d", len, ret);
		if(ret != 0)
		{
			log_debug("rfcomm_send err len:%d ret:%d", len, ret);
			bt_status.sending_data_flag = 0;
			return ret;
		}
		
	// }

	// if(ret != 0)
	// {
		// log_debug("Send data failure\r\n");
		// return 1;
	// }

	bt_status.send_data_state_machine_step = STATE_MACHINE_READ_RESP_STEP;
	bt_status.sending_len = len;
	//log_debug("bt_status.sending_len:%d", bt_status.sending_len);
	//bt_status.send_data_timeout = GetTimerCount();
	return 0;
}


static unsigned char hci_cmd_send_data_read_resp_step(void)
{
	unsigned char send_data[350];
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	if(bt_status.send_data_state_machine_step != STATE_MACHINE_READ_RESP_STEP)
	{
		return 1;
	}
	
	if(bt_status.sending_data_flag == 1)
	{
		// if(!hci_send_data_timer_check(3000))
		// {
			// log_debug("Wait resp timeout\r\n");
		// }
		return 1;
	}

	// if(bt_status.sending_data_flag == -1)
	// {
		// log_debug("Wait ble send data failure\r\n");
		// bt_status.send_data_state_machine_step = STATE_MACHINE_SEND_DATA_STEP;
		// return 1;
	// }

	bt_status.send_data_state_machine_step = STATE_MACHINE_SEND_DATA_STEP;
	QueGets((T_Queue *)(&ptBtOpt->tMasterSendQue), (char*)send_data, bt_status.sending_len);
	return 0;
}


static void bt_run_loop_send_data(void) {
	//int iLen, iRet, iSPPSend = 0;
	//unsigned char spp_data[110];
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	//send_state = RL_START;

	int i;
	int ret;
	
	while(1) {
	//if(ptBtOpt->Connected && !QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue))) {
		for(i=0; i<SEND_DATA_STATE_MACHINE_STEP_CN; i++) {
			// if(ptBtOpt->Connected) {
				// log_debug("CNUM:%d i:%d step:%d", SEND_DATA_STATE_MACHINE_STEP_CN, i, bt_status.send_data_state_machine_step );
			// }
			if(bt_status.send_data_state_machine_step == bt_send_data_state_machine[i].step) {
				ret = bt_send_data_state_machine[i].func();
				if(ret) {
					break;  //If this step failure, try to do it again in the next loop
				}
			}
		}	
	//}

	OsSleep(1);
		
	}

	
	
#if 0	
	while(1) {
		if(ptBtOpt->Connected && !QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue))) {
			EventTypeTest = 0;
			while(1) {
				// log_debug("queue:%d",  QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue)));
				if(RL_START == send_state || (RL_SENDNEXT == send_state)) {
					iLen = QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, sizeof(spp_data));
					log_debug("QueGetsWithoutDelete iLen:%d EventTypeTest:%d", iLen, EventTypeTest);
					send_state = RL_GETDATA;
				}

				//log_debug("QueGetsWithoutDelete = %d rfcomm_channel_id:%d EventTypeTest:%d data:%s\r\n", iLen, rfcomm_channel_id, EventTypeTest, spp_data);
					
				if((0 == iLen) && (EventTypeTest == 19)) {
					log_debug("zhufeng: out iLen:%d EventTypeTest:%d", iLen, EventTypeTest);
					EventTypeTest = 0;
					//gSendComplete = 1;
					//gSendingData = 0;
					send_state = RL_START;
					QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
					break;
				}

				if(iLen && (RL_GETDATA == send_state)) {
					iRet = rfcomm_send(rfcomm_channel_id, spp_data, iLen);
					if(iLen && (0 == iRet)) {
						log_debug("rfcomm_send Seccuss <cnt:%d><len:%d> send_state:%d\r\n", iSPPSend++, iLen, send_state);
						QueGets((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, iLen);
					}	
					send_state = RL_SENDING;
					log_debug("rfcomm_send iLen:%d rfcomm_channel_id:%d iRet:%d EventTypeTest:%d send_state:%d", iLen, rfcomm_channel_id, iRet, EventTypeTest, send_state);
						
				}
					
				//if(RL_SENDING == send_state) {
					//btstack_run_loop_embedded_execute_once();	
				//}
					
				if(RL_SENDING == send_state && (19 == EventTypeTest)) {	
					log_debug("sendding iLen:%d EventTypeTest:%d", iLen, EventTypeTest);
					send_state = RL_SENDNEXT;
				}
				
			}  
	}
		OsSleep(1);	
	}
#endif
	
	
	#if 0
	while(1) {
		if(ptBtOpt->Connected && !QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue))) {
			iLen =  QueGetsWithoutDelete((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, sizeof(spp_data));
			iRet = rfcomm_send(rfcomm_channel_id, spp_data, iLen);
			if(0 == iRet) {
				QueGets((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, iLen);
			} else {
				log_error("rfcomm_send err:%d", iRet);
			}
		}
		
		OsSleep(1);	
	}
	#endif
}				

void create_bt_send_data_task(void) {
	log_debug("create_bt_data_task\r\n");
	pBtSendTask = OsCreate((OS_TASK_ENTRY)bt_run_loop_send_data,25,0x3000); //TASK_PRIOR_BT_LOOP
	if(pBtSendTask == NULL)
		log_debug("create_bt_data_task==NULL\r\n");
}
#endif

//GPIO
#define PWR_PORT	GPIOB
#define PWR_PIN		9
#define RST_PORT	GPIOB
#define RST_PIN		11

void bt_enable_uart()
{

	int flag; 
	flag = readl(URT1_REG_BASE_ADDR +UART_CR);
	
	flag |= 1;
	writel(flag, (URT1_REG_BASE_ADDR +UART_CR));

	flag = readl(URT1_REG_BASE_ADDR +UART_CR);
	//printk("eCR::CTSE:%d, RTSE:%d, RTS:%d, RxE:%d,TxE:%d, UE:%d \r\n", 
		//(flag >>15) & 1,(flag >>14) & 1,(flag >>11) & 1,(flag >>9) & 1,(flag >>8) & 1,flag & 1);
	DelayMs(1);
}

void bt_disable_uart()
{

	int flag; 
	flag = readl(URT1_REG_BASE_ADDR +UART_CR);
	flag &= ~1;
	writel(flag, (URT1_REG_BASE_ADDR +UART_CR));

	flag = readl(URT1_REG_BASE_ADDR +UART_CR);
	//printk("dCR::CTSE:%d, RTSE:%d, RTS:%d, RxE:%d,TxE:%d, UE:%d \r\n", 
		//(flag >>15) & 1,(flag >>14) & 1,(flag >>11) & 1,(flag >>9) & 1,(flag >>8) & 1,flag & 1);
	DelayMs(1);
}

void bt_disable_rts_cts()
{

	int flag; 
	bt_disable_uart();
	DelayMs(2);//current char

	flag = readl(URT1_REG_BASE_ADDR +UART_LCRH);
	flag &= ~(1<<4);
	writel(flag, (URT1_REG_BASE_ADDR +UART_LCRH));//Flush the transmit FIFO
		
	flag = readl(URT1_REG_BASE_ADDR +UART_CR);

	flag &=  ~((1<<15) |(1<<14));	//15--CTS, 14 --RTS

	writel(flag, (URT1_REG_BASE_ADDR +UART_CR));
	bt_enable_uart();

	flag = readl(URT1_REG_BASE_ADDR +UART_CR);
	//printk("eRC::CTSE:%d, RTSE:%d, RTS:%d, RxE:%d,TxE:%d, UE:%d \r\n", 
		//(flag >>15) & 1,(flag >>14) & 1,(flag >>11) & 1,(flag >>9) & 1,(flag >>8) & 1,flag & 1);
	DelayMs(1);
}


#if 1
static int giBtStackInited;
int giBtDevInited;
int giBtPortInited;
static int giBtMfiCpErr;
static int giBtPowerState;


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
	
	//add by zhufeng 20180828 begin
	if(flag == 0)
	{
		gpio_set_pin_type(PWR_PORT, PWR_PIN, GPIO_OUTPUT);
		flag = 1;
	}	
	gpio_set_pin_val(PWR_PORT, PWR_PIN, v);//prechage 	
	DelayMs(5);
	
	gpio_set_pin_type(RST_PORT, RST_PIN, GPIO_OUTPUT);
	gpio_set_pin_val(RST_PORT, RST_PIN, 0);
	DelayMs(250);
	gpio_set_pin_val(RST_PORT, RST_PIN, 1);
	
	DelayMs(1+8+10);//ready
	DelayMs(550);

}
void s_BtPowerSwitch_D200(int on)
{
	gpio_set_pin_type(BT_RESET_PORT, BT_RESET_PIN, GPIO_OUTPUT);
	gpio_set_pin_val(BT_RESET_PORT, BT_RESET_PIN, 0);
	DelayMs(10);
	s_PmuBtPowerSwitch(on);
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
    	log_debug("get_bt_type context:%c", context);
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

int is_rtk8761a(void)
{
    return (get_bt_type() == 1);
}

#define BT_FIFO_SIZE			(8192+1)
static unsigned char gauMasterRecvFifo[BT_FIFO_SIZE];
static unsigned char gauMasterRecvFifoTest[BT_FIFO_SIZE];
static unsigned char gauMasterRecvFifoTestLen[BT_FIFO_SIZE];
static unsigned char gauMasterSendFifo[BT_FIFO_SIZE];

extern tBtCtrlOpt *getBtOptPtr(void);

void s_RTKBtInit(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	//if(!is_rtk8761a())
		//return ;

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

	//ptBtOpt->isServerEnd = 1;
	//ptBtOpt->eConnectType = eConnected_None;

	QueInit((T_Queue *)(&ptBtOpt->tMasterRecvQue), gauMasterRecvFifo, sizeof(gauMasterRecvFifo));
	QueInit((T_Queue *)(&ptBtOpt->tMasterRecvQueTest), gauMasterRecvFifoTest, sizeof(gauMasterRecvFifoTest));
	QueInit((T_Queue *)(&ptBtOpt->tMasterRecvQueTestLen), gauMasterRecvFifoTestLen, sizeof(gauMasterRecvFifoTestLen));
	QueInit((T_Queue *)(&ptBtOpt->tMasterSendQue), gauMasterSendFifo, sizeof(gauMasterSendFifo));
}


void s_BtPowerSwitch(int on)
{
	if (pBtDrvCfg != NULL)
		pBtDrvCfg->s_BtPowerSwitch(on);
}

void s_BtPowerOff(void)
{
	if(giBtPowerState == 1)
	{
		// TODO: poweroff timing
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
	}
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
		
		//for test return 0 directly
		return 0;
		
		if(get_machine_type()==S900 && (GetHWBranch()==S900HW_V3x))//S900 V30以后不再支持Mfi
		{
			giBtPowerState = 1;
		}
		else
		{
			DelayMs(200);
			//gDebugT1 = BtGetTimerCount();
			//DelayMs(200);
			for(i = 0; i < 3; i++)
			{
				if(MfiCpGetDevVer(&ucVal) == 0 && MfiCpGetDevID(buf) == 0)
				{
					log_debug("DevVer:%d, DevId:%d %d %d %d\r\n", ucVal, buf[0] , buf[1], buf[2], buf[3]);
					break;
				}
				else
					DelayMs(100);
			}
			if(i >= 3)
			{
				// FixMe
				log_debug("<ERR> Mfi CP Init Err!!\r\n");
				err = -1;
			}
			//gDebugT2 = BtGetTimerCount();
			//log_debug(("Mfi Cp Init <Time: %dms>\r\n", gDebugT2-gDebugT1));
			/* */
			giBtPowerState = 1;
		}
	}
	//return err;
	return 0;
}
#endif

int bt_open(void) {
	int ret;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	log_debug("bt_open begin");
	
	//if(!is_rtk8761a()) return -255;
	
	if(giBtDevInited == 1) {
		if(giBtMfiCpErr) {
			log_error("Bt Mfi init Err");
			return BT_RET_ERROR_MFI;		
		}

		else {
			log_debug("giBtDevInited");
			return BT_RET_OK;		
		}

	}
	
	s_RTKBtInit();

	giBtMfiCpErr = s_BtPowerOn();
	if(giBtMfiCpErr) {
		log_error("Bt Mfi init Err");
		return BT_RET_ERROR_MFI;
	} else {
		log_debug("s_BtPowerOn success");
	}

	//RtkPowerSwitch(1);
	if(giBtStackInited == 0) {
		btstack_memory_init();
		
		const btstack_run_loop_t* run_loop = (const btstack_run_loop_t*)btstack_run_loop_embedded_get_instance();
		btstack_run_loop_init(run_loop);
		transport_config.device_name = "rtl8761a";
		
		uart_driver = btstack_uart_block_embedded_instance();
		uart_config.baudrate	= transport_config.baudrate_init;
		uart_config.flowcontrol = transport_config.flowcontrol;
		uart_config.device_name = transport_config.device_name;
		uart_driver->init(&uart_config);//set tigger function and init baudrate
		
		const hci_transport_t* transport = hci_transport_h5_instance(uart_driver);
		hci_init(transport, (void*) &transport_config);//regiseter packet handler, etc
		// we use filesystem, don't access flash directly. shics	
		const btstack_link_key_db_t *link_key_db = btstack_link_key_db_memory_instance();
		hci_set_link_key_db(link_key_db);
		
		const btstack_chipset_t* chipset_driver = (const btstack_chipset_t*)btstack_chipset_rtl8761a_instance();
		hci_set_chipset(chipset_driver);
		
		
		// inform about BTstack state
		hci_event_callback_registration.callback = &packet_handler;//??? this handler is correct?
		hci_add_event_handler(&hci_event_callback_registration);
		
		
		btstack_chipset_rtl8761a_download_firmware(uart_driver, NULL);
		log_debug("wait for download complete!!\r\n");
		while(1)//wait for download complete!
		{
			if(download_fw_complete > 0) break;
			DelayMs(500*3);
		}
		log_debug("Downlaod finish!");
		ret = phase2(0);
		if(0 != ret) {
			return BT_RET_ERROR_STACK;
		}
		
		giBtStackInited = 1;

	}

	QueInit((T_Queue *)(&ptBtOpt->tMasterRecvQue), gauMasterRecvFifo, sizeof(gauMasterRecvFifo));
	QueInit((T_Queue *)(&ptBtOpt->tMasterSendQue), gauMasterSendFifo, sizeof(gauMasterSendFifo));
	
  	giBtDevInited = 1;
	giBtPortInited = 1; //temp test
	
    return BT_RET_OK;
}


static int phase2(int status){
	int ret;

    if (status){
        log_debug("Download firmware failed\n");
        return BT_RET_ERROR_STACK;
    }

    log_debug("Phase 2: Main app\n");

   ret = btstack_main(0, NULL); // use classic_test.c 

	return ret;
}


int bt_close(void) {
	//classic
	bt_disconnect();
	//ble
	ble_disconnect();

	DelayMs(200);
	s_BtPowerOff();
	
	return BT_RET_OK;
}


extern void DelayMs(uint ms);
int RtkPowerSwitch(int OP)
{
	int flag = 0;
	int v = (OP == 1 ? 0 : 1);
	
	//add by zhufeng 20180828 begin
	if(flag == 0)
	{
		gpio_set_pin_type(PWR_PORT, PWR_PIN, GPIO_OUTPUT);
		flag = 1;
	}	
	gpio_set_pin_val(PWR_PORT, PWR_PIN, v);//prechage 	
	DelayMs(5);
	
	gpio_set_pin_type(RST_PORT, RST_PIN, GPIO_OUTPUT);
	gpio_set_pin_val(RST_PORT, RST_PIN, 0);
	DelayMs(250);
	gpio_set_pin_val(RST_PORT, RST_PIN, 1);
	
	DelayMs(1+8+10);//ready
	DelayMs(550);
}


int uart_init_default(btstack_uart_block_t * uart_driver)
{
	return uart_driver->init(&uart_config);
}
