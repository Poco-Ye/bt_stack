/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */
 
// *****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>

#include "ble/sm.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "bluetooth_sdp.h"
#include "classic/rfcomm.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "classic/spp_server.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_stdin.h"
#include "btstack_debug.h"
#include "le_streamer.h"
#include "btstack.h"
#include "bt_init.h"
#include "posapi.h"
#include "platform.h"
// #define log_debug LogPrintf


static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);
static btstack_packet_callback_registration_t hci_event_callback_registration;

typedef enum {
	TC_OFF,
	TC_IDLE,
	TC_W4_SCAN_RESULT,
	TC_W4_CONNECT,
	TC_W4_SERVICE_RESULT,
	TC_W4_CHARACTERISTIC_RX_RESULT,
	TC_W4_CHARACTERISTIC_TX_RESULT,
	TC_W4_ENABLE_NOTIFICATIONS_COMPLETE,
	TC_W4_TEST_DATA
} gc_state_t;

static gc_state_t le_state = TC_OFF;

// addr and type of device with correct name
static bd_addr_t      le_streamer_addr;
static bd_addr_type_t le_streamer_addr_type;

// SPP / RFCOMM
#define RFCOMM_SERVER_CHANNEL 1
#define MAX_NR_CONNECTIONS 3

#define NUM_ROWS 25
#define NUM_COLS 40

static hci_con_handle_t connection_handle;

// On the GATT Server, RX Characteristic is used for receive data via Write, and TX Characteristic is used to send data via Notifications
// static uint8_t le_streamer_service_uuid[16]           = { 0x00, 0x00, 0xFF, 0x10, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
// static uint8_t le_streamer_characteristic_rx_uuid[16] = { 0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
// static uint8_t le_streamer_characteristic_tx_uuid[16] = { 0x00, 0x00, 0xFF, 0x12, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
static uint8_t le_streamer_service_uuid[16]           = { 0x49, 0x53, 0x53, 0x43, 0xFE, 0x7D, 0x4A, 0xE5, 0x8F, 0x9A, 0x9F, 0xAF, 0xD2, 0x05, 0xE4, 0x55};
static uint8_t le_streamer_characteristic_rx_uuid[16] = { 0x49, 0x53, 0x53, 0x43, 0x1E, 0x4D, 0x4B, 0xD9, 0xBA, 0x61, 0x23, 0xC6, 0x47, 0x24, 0x96, 0x16};
static uint8_t le_streamer_characteristic_tx_uuid[16] = { 0x49, 0x53, 0x53, 0x43, 0x88, 0x41, 0x43, 0xF4, 0xA8, 0xD4, 0xEC, 0xBE, 0x34, 0x72, 0x9B, 0xB3};

static gatt_client_service_t le_streamer_service;
static gatt_client_characteristic_t le_streamer_characteristic_rx;
static gatt_client_characteristic_t le_streamer_characteristic_tx;

static int listener_registered;
static gatt_client_notification_t notification_listener;


static bd_addr_t remote = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// static bd_addr_t remoteBleAddr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static bd_addr_t remoteAddr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static bd_addr_t remote_rfcomm = {0xA0, 0xCB, 0xFD, 0xC5, 0x0B, 0x6F};
static char counter_string[30];
static int counter_string_len;
static hci_con_handle_t att_con_handle;

static uint8_t rfcomm_channel_nr = 1;

static int gap_discoverable = 0;
static int gap_connectable = 0;
static int gap_bondable = 0;
static int gap_dedicated_bonding_mode = 0;
static int gap_mitm_protection = 0;
static uint8_t gap_auth_req = 0;
static char *gap_io_capabilities;

static int ui_passkey = 0;
static int ui_digits_for_passkey = 0;
static int ui_chars_for_pin = 0;
static uint8_t ui_pin[17];
static int ui_pin_offset = 0;

static uint16_t handle;
static uint16_t local_cid;

//static uint32_t  spp_service_buffer[150/4];    // implicit alignment to 4-byte memory address
static uint32_t  spp_service_buffer[150];    // implicit alignment to 4-byte memory address


static uint8_t  test_data[NUM_ROWS * NUM_COLS];
static uint16_t  rfcomm_mtu;
uint16_t  rfcomm_channel_id;
static uint16_t  spp_test_data_len;

//ble
static uint16_t         att_mtu;

extern int giBtDevInited;
extern int giBtPortInited;

int classicConnectComplete1;
int classicConnectComplete2;

//get config API
static ST_BT_CONFIG *devInfo;


typedef struct {
	char name;
	int le_notification_enabled;
	uint16_t value_handle;
	hci_con_handle_t connection_handle;
	int  counter;
	char test_data[200];
	int  test_data_len;
	uint32_t test_data_sent;
	uint32_t test_data_start;
} le_streamer_connection_t;
static le_streamer_connection_t le_streamer_connections[MAX_NR_CONNECTIONS];

/*
 * @section Advertisements
 *
 * @text The Flags attribute in the Advertisement Data indicates if a device is
 * in dual-mode or not. Flag 0x06 indicates LE General Discoverable, BR/EDR not
 * supported although we're actually using BR/EDR. In the past, there have been
 * problems with Anrdoid devices when the flag was not set. Setting it should
 * prevent the remote implementation to try to use GATT over LE/EDR, which is
 * not implemented by BTstack. So, setting the flag seems like the safer choice
 * (while it's technically incorrect).
 */
/* LISTING_START(advertisements): Advertisement data: Flag 0x06 indicates
 * LE-only device */
const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02,
    BLUETOOTH_DATA_TYPE_FLAGS,
    0x06,
    // Name
    0x05,
    BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'D',
    '2',
    '1',
    '0',
    // Incomplete List of 16-bit Service Class UUIDs -- FF10 - only valid for
    // testing!
    // 0x03,
    // BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS,
    // 0x10,
    // 0xff,
};
uint8_t adv_data_len = sizeof(adv_data);


static volatile tBtCtrlOpt gtBtOpt;
static volatile tBtCtrlOpt *gptBtOpt = &gtBtOpt;

tBtCtrlOpt *getBtOptPtr(void)
{
	return (tBtCtrlOpt *)gptBtOpt;
}

static int connection_index;
static void init_connections(void) {
	// track connections
	int i;

	for (i=0; i<MAX_NR_CONNECTIONS; i++) {
		le_streamer_connections[i].connection_handle = HCI_CON_HANDLE_INVALID;
		le_streamer_connections[i].name = 'A' + i;
	}
}

static le_streamer_connection_t* connection_for_conn_handle(hci_con_handle_t conn_handle) {
	int i;

	for (i=0; i<MAX_NR_CONNECTIONS; i++) {
		if (le_streamer_connections[i].connection_handle == conn_handle) {
			return &le_streamer_connections[i];
		}
	}

	return NULL;
}

static void next_connection_index(void) {
	connection_index++;

	if (connection_index == MAX_NR_CONNECTIONS) {
		connection_index = 0;
	}
}


#define INQUIRY_INTERVAL 0X30//5, 0X1--0X30
#define TEST_COD 0x020104

//struct device devices[MAX_DEVICES];
static struct device *devices;
static struct device deviceInfoLocal[MAX_DEVICES];
static int deviceCount = 0;
//ble client
static struct le_device *le_devices;
static struct le_device leDeviceInfoLocal[MAX_DEVICES];
static int le_deviceCount = 0;


static unsigned char *RecvsData;
static int RecvsLen = 0;

static uint16_t BtTestReady = 0;


enum STATE {INIT, W4_INQUIRY_MODE_COMPLETE, ACTIVE} ;
enum STATE state = INIT;

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t con_handle,
                                  uint16_t att_handle, uint16_t offset,
                                  uint8_t *buffer, uint16_t buffer_size) {
  UNUSED(con_handle);

  if (att_handle ==
      ATT_CHARACTERISTIC_49535343_1E4D_4BD9_BA61_23C647249616_01_VALUE_HANDLE) {
    return att_read_callback_handle_blob((const uint8_t *)counter_string,
                                         buffer_size, offset, buffer,
                                         buffer_size);
  }
  return 0;
}

// write requests
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t* buffer, uint16_t buffer_size) {
	UNUSED(offset);

	// log_debug("att_write_callback att_handle %04x, transaction mode %u\n", att_handle, transaction_mode);
	if (transaction_mode != ATT_TRANSACTION_MODE_NONE) {
		return 0;
	}

	le_streamer_connection_t* context = connection_for_conn_handle(con_handle);

	switch(att_handle) {
	case ATT_CHARACTERISTIC_49535343_1E4D_4BD9_BA61_23C647249616_01_CLIENT_CONFIGURATION_HANDLE:
	// case ATT_CHARACTERISTIC_0000FF12_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
		context->le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
		log_debug("%c: Notifications enabled %u\n", context->name, context->le_notification_enabled);

		if (context->le_notification_enabled) {
			switch (att_handle) {
			case ATT_CHARACTERISTIC_49535343_1E4D_4BD9_BA61_23C647249616_01_CLIENT_CONFIGURATION_HANDLE:
				context->value_handle = ATT_CHARACTERISTIC_49535343_1E4D_4BD9_BA61_23C647249616_01_VALUE_HANDLE;
				break;

			// case ATT_CHARACTERISTIC_0000FF12_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
				// context->value_handle = ATT_CHARACTERISTIC_0000FF12_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE;
				// break;

			default:
				break;
			}

			//att_server_request_can_send_now_event(context->connection_handle);
		}

		//test_reset(context);
		break;

	case ATT_CHARACTERISTIC_49535343_1E4D_4BD9_BA61_23C647249616_01_VALUE_HANDLE:
	case ATT_CHARACTERISTIC_49535343_8841_43F4_A8D4_ECBE34729BB3_01_VALUE_HANDLE:
	// case ATT_CHARACTERISTIC_0000FF12_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE:
		//test_track_sent(context, buffer_size);
		printf_hexdump(buffer, buffer_size);
		break;

	default:
		log_debug("Write to 0x%04x, len %u\n", att_handle, buffer_size);
	}

	return 0;
}
static void handle_found_service(const char* name, uint8_t port) {
	log_info("SDP: Service name: '%s', RFCOMM port %u\n", name, port);
	rfcomm_channel_nr = port;
}


int getDeviceIndexForAddressExt( bd_addr_t addr){
    int j;
    for (j=0; j< deviceCount; j++){
		if (bd_addr_cmp(addr, deviceInfoLocal[j].address) == 0){
            return j;
        }

    }
    return -1;
}

static int getDeviceIndexForAddress( bd_addr_t addr){
    int j;
    for (j=0; j< deviceCount; j++){
        if (bd_addr_cmp(addr, devices[j].address) == 0){
            return j;
        }
    }
    return -1;
}

int leGetDeviceIndexForAddressExt( bd_addr_t addr){
    int j;
    for (j=0; j< le_deviceCount; j++){
        if (bd_addr_cmp(addr, leDeviceInfoLocal[j].address) == 0){
            return j;
        }
    }
    return -1;
}

static int leGetDeviceIndexForAddress( bd_addr_t addr){
    int j;
    for (j=0; j< le_deviceCount; j++){
        if (bd_addr_cmp(addr, le_devices[j].address) == 0){
            return j;
        }
    }
    return -1;
}


static void start_scan(void){
    log_debug("Starting inquiry scan..\n");
    hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
}

static int has_more_remote_name_requests(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        if (devices[i].state == REMOTE_NAME_REQUEST) return 1;
    }
    return 0;
}

static void do_next_remote_name_request(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        // remote name request
        if (devices[i].state == REMOTE_NAME_REQUEST){
            devices[i].state = REMOTE_NAME_INQUIRED;
            log_debug("Get remote name of %s...\n", bd_addr_to_str(devices[i].address));
            hci_send_cmd(&hci_remote_name_request, devices[i].address,
                        devices[i].pageScanRepetitionMode, 0, devices[i].clockOffset | 0x8000);
            return;
        }
    }
}

static void continue_remote_names(void){
    // don't get remote names for testing
    if (has_more_remote_name_requests()){
        do_next_remote_name_request();
        return;
    } 
    // start_scan();
    // accept first device
    if (deviceCount){
        memcpy(remote, devices[0].address, 6);
		log_debug("deviceCount: %d devices[deviceCount].name:%s\n", deviceCount, devices[deviceCount].name);	
		set_inquiry_complete(1);
        log_debug("Inquiry scan over, using %s for outgoing connections\n", bd_addr_to_str(remote));
    } else {
		set_inquiry_complete(1);
        log_debug("Inquiry scan over but no devices found\n" );
    }
}

static int gScanSize;
static void inquiry_packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
    bd_addr_t addr;
    int i;
    int numResponses;
    int index;
	le_streamer_connection_t* context;

    log_debug("inquiry_packet_handler: pt: 0x%02x, packet[0]: 0x%02x\n", packet_type, packet[0]);
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = packet[0];

    switch(event){
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:{
            numResponses = hci_event_inquiry_result_get_num_responses(packet);
			
			// log_debug("11 scan result numResponses:%d gScanSize:%d", numResponses, gScanSize);
            int offset = 3;
            for (i=0; i<numResponses && deviceCount < MAX_DEVICES;i++) {
                reverse_bd_addr(&packet[offset], addr);
                offset += 6;
                index = getDeviceIndexForAddress(addr);
                if (index >= 0) continue;   // already in our list
                memcpy(devices[deviceCount].address, addr, 6);
				//devices is null when classic scan finish, so need local devices keep info 
				memcpy(deviceInfoLocal[deviceCount].address, addr, 6);

                devices[deviceCount].pageScanRepetitionMode = packet[offset];
                offset += 1;

                if (event == HCI_EVENT_INQUIRY_RESULT){
                    offset += 2; // Reserved + Reserved
                    devices[deviceCount].classOfDevice = little_endian_read_24(packet, offset);
                    offset += 3;
                    devices[deviceCount].clockOffset =   little_endian_read_16(packet, offset) & 0x7fff;
                    offset += 2;
                    devices[deviceCount].rssi  = 0;
                } else {
                    offset += 1; // Reserved
                    devices[deviceCount].classOfDevice = little_endian_read_24(packet, offset);
                    offset += 3;
                    devices[deviceCount].clockOffset =   little_endian_read_16(packet, offset) & 0x7fff;
                    offset += 2;
                    devices[deviceCount].rssi  = packet[offset];
                    offset += 1;
                }
                devices[deviceCount].state = REMOTE_NAME_REQUEST;
                log_debug("Device found: %s with COD: 0x%06x, pageScan %d, clock offset 0x%04x, rssi 0x%02x\n", bd_addr_to_str(addr),
                        devices[deviceCount].classOfDevice, devices[deviceCount].pageScanRepetitionMode,
                        devices[deviceCount].clockOffset, devices[deviceCount].rssi);
                deviceCount++;

				if(deviceCount >= gScanSize) {
					log_debug("deviceCount:%d >= gScanSize:%d gap_inquiry_stop", deviceCount, gScanSize);
					gap_inquiry_stop();
					break;
				}
					
            }

            break;
        }
        case HCI_EVENT_INQUIRY_COMPLETE:
            for (i=0;i<deviceCount;i++) {
                // retry remote name request
                if (devices[i].state == REMOTE_NAME_INQUIRED)
                    devices[i].state = REMOTE_NAME_REQUEST;
            }
            continue_remote_names();
            break;

        case DAEMON_EVENT_REMOTE_NAME_CACHED:
            reverse_bd_addr(&packet[3], addr);
            log_debug("Cached remote name for %s: '%s'\n", bd_addr_to_str(addr), &packet[9]);
            break;

        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            reverse_bd_addr(&packet[3], addr);
            index = getDeviceIndexForAddress(addr);
            if (index >= 0) {
                if (packet[2] == 0) {
                    log_debug("Name: '%s'\n", &packet[9]);
					// char name_buffer[240];
					// int name_len = gap_event_inquiry_result_get_name_len(packet);
					// memcpy(name_buffer, gap_event_inquiry_result_get_name(packet), name_len);
					// name_buffer[name_len] = 0;
					// log_info(", name '%s'", name_buffer);
					log_debug("name request addr:%s index:%d", bd_addr_to_str(addr), index);
					log_debug("name request addr:%s index:%d", bd_addr_to_str(addr), index);
					memcpy(deviceInfoLocal[index].name, &packet[9], MAX_NAME_LEN); 
					log_debug("name request addr:%s index:%d", bd_addr_to_str(addr), index);
					memcpy(devices[index].name, &packet[9], MAX_NAME_LEN); 
					log_debug("deviceCount: %d devices[deviceCount].name:%s deviceInfo[deviceCount].name:%s\n", deviceCount, devices[index].name, deviceInfoLocal[index].name);	
                    devices[index].state = REMOTE_NAME_FETCHED;
                } else {
                    log_debug("Failed to get name: page timeout\n");
                }
            }
            continue_remote_names();
            break;

        default:
            break;
    }
}


//BLE client service handle event
static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
	UNUSED(packet_type);
	UNUSED(channel);
	UNUSED(size);
	uint16_t mtu;
	uint16_t value_handle, value_length, value;

	switch(le_state) {
	case TC_W4_SERVICE_RESULT:
		switch(hci_event_packet_get_type(packet)) {
		case GATT_EVENT_SERVICE_QUERY_RESULT:
			// store service (we expect only one)
			gatt_event_service_query_result_get_service(packet, &le_streamer_service);
			break;

		case GATT_EVENT_QUERY_COMPLETE:
			if (packet[4] != 0) {
				log_debug("SERVICE_QUERY_RESULT - Error status %x.\n", packet[4]);
				gap_disconnect(connection_handle);
				break;
			}

			// service query complete, look for characteristic
			le_state = TC_W4_CHARACTERISTIC_RX_RESULT;
			log_debug("Search for LE Streamer RX characteristic.\n");
			gatt_client_discover_characteristics_for_service_by_uuid128(handle_gatt_client_event, connection_handle, &le_streamer_service, le_streamer_characteristic_rx_uuid);
			break;

		default:
			break;
		}

		break;

	case TC_W4_CHARACTERISTIC_RX_RESULT:
		switch(hci_event_packet_get_type(packet)) {
		case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
			gatt_event_characteristic_query_result_get_characteristic(packet, &le_streamer_characteristic_rx);
			break;

		case GATT_EVENT_QUERY_COMPLETE:
			if (packet[4] != 0) {
				log_debug("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
				gap_disconnect(connection_handle);
				break;
			}

			// rx characteristiic found, look for tx characteristic
			le_state = TC_W4_CHARACTERISTIC_TX_RESULT;
			log_debug("Search for LE Streamer TX characteristic.\n");
			gatt_client_discover_characteristics_for_service_by_uuid128(handle_gatt_client_event, connection_handle, &le_streamer_service, le_streamer_characteristic_tx_uuid);
			break;

		default:
			break;
		}

		break;

	case TC_W4_CHARACTERISTIC_TX_RESULT:
		switch(hci_event_packet_get_type(packet)) {
		case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
			gatt_event_characteristic_query_result_get_characteristic(packet, &le_streamer_characteristic_tx);
			break;

		case GATT_EVENT_QUERY_COMPLETE:
			if (packet[4] != 0) {
				log_debug("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
				gap_disconnect(connection_handle);
				break;
			}

			// register handler for notifications
			listener_registered = 1;
			gatt_client_listen_for_characteristic_value_updates(&notification_listener, handle_gatt_client_event, connection_handle, &le_streamer_characteristic_tx);
			// setup tracking
			//le_streamer_connection.name = 'A';
			//le_streamer_connection.test_data_len = ATT_DEFAULT_MTU - 3;
			//test_reset(&le_streamer_connection);
			gatt_client_get_mtu(connection_handle, &mtu);
			//le_streamer_connection.test_data_len = btstack_min(mtu - 3, sizeof(le_streamer_connection.test_data));
			//log_debug("%c: ATT MTU = %u => use test data of len %u\n", le_streamer_connection.name, mtu, le_streamer_connection.test_data_len);
			// enable notifications
#if (TEST_MODE & TEST_MODE_ENABLE_NOTIFICATIONS)
			log_debug("Start streaming - enable notify on test characteristic.\n");
			le_state = TC_W4_ENABLE_NOTIFICATIONS_COMPLETE;
			gatt_client_write_client_characteristic_configuration(handle_gatt_client_event, connection_handle,
			        &le_streamer_characteristic_tx, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
			break;
#endif
			le_state = TC_W4_TEST_DATA;
#if (TEST_MODE & TEST_MODE_WRITE_WITHOUT_RESPONSE)
			log_debug("Start streaming - request can send now.\n");
			gatt_client_request_can_write_without_response_event(handle_gatt_client_event, connection_handle);
#endif
			break;

		default:
			break;
		}

		break;

	case TC_W4_ENABLE_NOTIFICATIONS_COMPLETE:
		switch(hci_event_packet_get_type(packet)) {
		case GATT_EVENT_QUERY_COMPLETE:
			log_debug("Notifications enabled, status %02x\n", gatt_event_query_complete_get_status(packet));
			le_state = TC_W4_TEST_DATA;
#if (TEST_MODE & TEST_MODE_WRITE_WITHOUT_RESPONSE)
			log_debug("Start streaming - request can send now.\n");
			gatt_client_request_can_write_without_response_event(handle_gatt_client_event, connection_handle);
#endif
			break;

		default:
			break;
		}

		break;

	case TC_W4_TEST_DATA:
		switch(hci_event_packet_get_type(packet)) {
		case GATT_EVENT_NOTIFICATION:
			value_handle = little_endian_read_16(packet, 4);
			value_length = little_endian_read_16(packet, 6);
			value = &packet[8];
			log_debug("Notification handle 0x%04x, value: ", value_handle);
			printf_hexdump(value, value_length);
			//test_track_data(&le_streamer_connection, gatt_event_notification_get_value_length(packet));
			break;

		case GATT_EVENT_QUERY_COMPLETE:
			break;

		case GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE:
			//streamer(&le_streamer_connection);
			break;

		default:
			log_debug("Unknown packet type %x\n", hci_event_packet_get_type(packet));
			break;
		}

		break;

	default:
		log_debug("error\n");
		break;
	}
}

static unsigned int gScanTimeT1, gScanTimeT2, gScanTimeResult;
static unsigned int gBleTimeout;
static int gBleScanSize;
#if 1
static void handle_advertising_event(uint8_t* packet, int size) {
	bd_addr_t le_streamer_addr;
	char dataDefault[15] = "Unknown Device";
	int index, hasName = 0;
	
	if (le_deviceCount >= LE_MAX_DEVICES) {
		return;  // already full
	}
	
	if(le_deviceCount >= gBleScanSize) {
		log_debug("le_deviceCount:%d >= gBleScanSize:%d gap_stop_scan", le_deviceCount, gBleScanSize);
		gap_stop_scan();
		return;
	}
	
	gScanTimeT2 = GetTimerCount();
	gScanTimeResult = gScanTimeT2 - gScanTimeT1;
	//log_debug("time gScanTimeT2:%d gScanTimeT1:%d gScanTimeResult:%d gBleTimeout:%d", gScanTimeT2, gScanTimeT1, gScanTimeResult, gBleTimeout);
	
	if(gScanTimeResult > (gBleTimeout * 1000)) {
		// stop scanning, and connect to the device
		// le_state = TC_W4_CONNECT;
		gap_stop_scan();
		log_debug("Stop scan. scan result addr %s le_streamer_addr_type:%d le_deviceCount:%d\n", bd_addr_to_str(le_streamer_addr), le_streamer_addr_type, le_deviceCount);

		le_set_inquiry_complete(1);	
	}

	gap_event_advertising_report_get_address(packet, le_streamer_addr);
	index = leGetDeviceIndexForAddress(le_streamer_addr);
	if (index >= 0) {
		log_debug("ble addr:%s already in our list", bd_addr_to_str(le_streamer_addr));
		return;  // already in our list
	}	

	memcpy(le_devices[le_deviceCount].address, le_streamer_addr, 6);
	memcpy(leDeviceInfoLocal[le_deviceCount].address, le_streamer_addr, 6);
	uint8_t le_streamer_addr_type = gap_event_advertising_report_get_address_type(packet);
	log_debug("scan result addr %s le_streamer_addr_type:%d le_deviceCount:%d\n", bd_addr_to_str(le_streamer_addr), le_streamer_addr_type, le_deviceCount);
	// always request address resolution
	// sm_address_resolution_lookup(addr_type, addr);

	// uint8_t adv_event_type = gap_event_advertising_report_get_advertising_event_type(packet);
	// log_debug("Advertisement: %s - %s, ", bd_addr_to_str(le_streamer_addr), ad_event_types[adv_event_type]);

	
	int adv_size = gap_event_advertising_report_get_data_length(packet);
	const uint8_t* adv_data = gap_event_advertising_report_get_data(packet);
	// check flags
	ad_context_t context;
	
	//memset(le_devices[le_deviceCount].name, 0x00, LE_MAX_NAME_LEN);
	for (ad_iterator_init(&context, adv_size, (uint8_t*)adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
		uint8_t data_type = ad_iterator_get_data_type(&context);
		uint8_t data_size      = ad_iterator_get_data_len(&context);
		const uint8_t* data = ad_iterator_get_data(&context);

		log_debug("data_type:%d le_deviceCount:%d data_size:%d data:%s", data_type, le_deviceCount, data_size, data);
		switch (data_type) {
		case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
		case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
			log_debug("adv local name:%s", data);
			hasName = 1;
			strcpy(le_devices[le_deviceCount].name, data);
			//memcpy(le_devices[le_deviceCount].name, data, data_size);
			log_debug("name:%s addr:%s", le_devices[le_deviceCount].name, bd_addr_to_str(le_streamer_addr));
			break;

		default:
			break;
		}
	}

	if(1 != hasName) {
		strcpy(le_devices[le_deviceCount].name, dataDefault);
		//memcpy(le_devices[le_deviceCount].name, dataDefault, strlen(dataDefault));
		log_debug("no name:%s addr:%s", le_devices[le_deviceCount].name, bd_addr_to_str(le_streamer_addr));
	}

	// dump data
	// log_debug("Data: ");
	// printf_hexdump(adv_data, adv_size);
	
	le_deviceCount++;		
}
#endif

extern int gGetRfcomData;
int rfcomm_event_can_send_now = 1;
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    uint16_t psm;
    uint32_t passkey;
	uint16_t i;
	int mtu;
	le_streamer_connection_t* context;
	uint16_t conn_interval;
	bd_addr_t event_addr;
	int index;
	int iRet;

	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
#if 0
    int i;
    log_debug("\r\nClassic_test::packet_handler:packet_type=%02x, PACKET[]:\r\n",packet_type);
    for(i=0; i<16 && i<size; i++)
		log_debug("0x%02x, ",packet[i]);
	log_debug(" \r\n");
#endif

    if (packet_type == UCD_DATA_PACKET){
        log_debug("UCD Data for PSM %04x received, size %u\n", little_endian_read_16(packet, 0), size - 2);
    }

    switch (packet_type) {
	case HCI_EVENT_PACKET:
		switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
        case HCI_EVENT_INQUIRY_COMPLETE:
        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            inquiry_packet_handler(packet_type, packet, size);
            break;		

        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                log_error("BTstack Bluetooth Classic Test Ready\n");
                hci_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
				BtTestReady = 1;
            }
            break;			
		
        case GAP_EVENT_DEDICATED_BONDING_COMPLETED:
            log_debug("GAP Dedicated Bonding Complete, status %u\n", packet[2]);
            break;

        case HCI_EVENT_CONNECTION_COMPLETE:
            if (!packet[2]){
                handle = little_endian_read_16(packet, 3);
                reverse_bd_addr(&packet[5], remote);
                log_debug("HCI_EVENT_CONNECTION_COMPLETE: handle 0x%04x\n", handle);
            }
			
			// ptBtOpt->Connected = 1;
			
            break;

        case HCI_EVENT_USER_PASSKEY_REQUEST:
            reverse_bd_addr(&packet[2], remote);
            log_debug("GAP User Passkey Request for %s\nPasskey:", bd_addr_to_str(remote));
            fflush(stdout);
            ui_digits_for_passkey = 6;
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            reverse_bd_addr(&packet[2], remote);
            passkey = little_endian_read_32(packet, 8);
            log_debug("GAP User Confirmation Request for %s, number '%06u'\n", bd_addr_to_str(remote),passkey);
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
			log_info("Pin code request - using '0000'\n");
			hci_event_pin_code_request_get_bd_addr(packet, remote);
			gap_pin_code_response(remote, "0000");
            //hci_event_pin_code_request_get_bd_addr(packet, remote);
            //log_debug("GAP Legacy PIN Request for %s (press ENTER to send)\nPasskey:", bd_addr_to_str(remote));
           // fflush(stdout);
            //ui_chars_for_pin = 1;
            break;

        case L2CAP_EVENT_CHANNEL_OPENED:
            // inform about new l2cap connection
            reverse_bd_addr(&packet[3], remote);
            psm = little_endian_read_16(packet, 11); 
            local_cid = little_endian_read_16(packet, 13); 
            handle = little_endian_read_16(packet, 9);
            if (packet[2] == 0) {
                log_debug("L2CAP Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                       bd_addr_to_str(remote), handle, psm, local_cid,  little_endian_read_16(packet, 15));
            } else {
                log_debug("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(remote), packet[2]);
            }
            break;

        case L2CAP_EVENT_INCOMING_CONNECTION: 
            // data: event (8), len(8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16) 
           	psm = little_endian_read_16(packet, 10);
            uint16_t l2cap_cid  = little_endian_read_16(packet, 12);
            log_debug("L2CAP incoming connection request on PSM %u\n", psm); 
            l2cap_accept_connection(l2cap_cid);
            break;
        

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
            rfcomm_event_incoming_connection_get_bd_addr(packet, remote); 
			bd_addr_copy(remoteAddr, remote);
			log_debug("RFCOMM_EVENT_INCOMING_CONNECTION:remote_rfcomm:%s", bd_addr_to_str(remoteAddr));
			
            rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
            rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
            log_debug("RFCOMM channel %u requested for %s\n\r", rfcomm_channel_nr, bd_addr_to_str(remote));
            rfcomm_accept_connection(rfcomm_channel_id);
            break;
            
        case RFCOMM_EVENT_CHANNEL_OPENED:
            // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
            if (rfcomm_event_channel_opened_get_status(packet)) {
                log_debug("RFCOMM channel open failed, status %u\n\r", rfcomm_event_channel_opened_get_status(packet));
				gap_discoverable_control(1);
				gap_connectable_control(1);
            } else { 
				log_debug("RFCOMM_EVENT_CHANNEL_OPENED");
                rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
								log_debug("RFCOMM_EVENT_CHANNEL_OPENED");
                rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
				log_debug("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, rfcomm_mtu);
				spp_test_data_len = rfcomm_mtu;
				// spp_test_data_len = 8;

				// if (spp_test_data_len > sizeof(test_data)) {
					// spp_test_data_len = sizeof(test_data);
				// }
				log_debug("RFCOMM_EVENT_CHANNEL_OPENED");
				// disable page/inquiry scan to get max performance
				gap_discoverable_control(0);
				gap_connectable_control(0);
				
				// disable advertisements
				log_debug("RFCOMM_EVENT_CHANNEL_OPENED");
				gap_advertisements_enable(0);
				//ptBtOpt->Connected = 1;

				//add by zhufeng for connect ok info
				log_debug("l2cap_emit_channel_opened set classicConnectComplete2 = 1");
				classicConnectComplete1 = 1;				
                log_debug("\n\rRFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n\r", rfcomm_channel_id, mtu);
            }
            break;
			
        case RFCOMM_EVENT_CHANNEL_CLOSED:
			gap_discoverable_control(1);
			gap_connectable_control(1);
            rfcomm_channel_id = 0;
			ptBtOpt->Connected = 0;
            break;

		case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            handle_found_service(sdp_event_query_rfcomm_service_get_name(packet),
                                 sdp_event_query_rfcomm_service_get_rfcomm_channel(packet));
            break;

        case SDP_EVENT_QUERY_COMPLETE:
            log_debug("SDP SPP Query complete channel:%d\n", rfcomm_channel_nr);
			gap_drop_link_key_for_bd_addr(remote);
			rfcomm_create_channel(packet_handler, remote, rfcomm_channel_nr, NULL);
            break;
			
		//ble server
		case HCI_EVENT_DISCONNECTION_COMPLETE:
			context = connection_for_conn_handle(hci_event_disconnection_complete_get_connection_handle(packet));

			if (!context) {
				break;
			}
			
			if (listener_registered) {
				listener_registered = 0;
				gatt_client_stop_listening_for_characteristic_value_updates(&notification_listener);
			}

			// free connection
			log_debug("%c: Disconnect, reason %02x\n", context->name, hci_event_disconnection_complete_get_reason(packet));
			context->le_notification_enabled = 0;
			context->connection_handle = HCI_CON_HANDLE_INVALID;
			ptBtOpt->Connected = 0;
			break;


		case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
			log_debug("RFCOMM_EVENT_CHANNEL_OPENED");
			att_mtu = att_event_mtu_exchange_complete_get_MTU(packet);
			log_debug("ATT MTU = %u\n", att_mtu);
			context = connection_for_conn_handle(att_event_mtu_exchange_complete_get_handle(packet));

			if (!context) {
				break;
			}

			context->test_data_len = btstack_min(att_mtu - 3, sizeof(context->test_data));
			log_debug("%c: ATT MTU = %u => use test data of len %u\n", context->name, att_mtu, context->test_data_len);
			break;	
			
		case GAP_EVENT_ADVERTISING_REPORT:
			 handle_advertising_event(packet, size);
			 break;
		
			
		case HCI_EVENT_LE_META:
			// wait for connection complete
			if (hci_event_le_meta_get_subevent_code(packet) !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
				break;
			}

			// setup new
			context = connection_for_conn_handle(HCI_CON_HANDLE_INVALID);
			if (!context) {
					break;	
			}
			
			//store ble server connect addr
			hci_subevent_le_connection_complete_get_peer_address(packet, remoteAddr);
			// bd_addr_copy(remote_rfcomm, bleRemote);
			log_debug("HCI_EVENT_LE_META:store ble server connect addr:%s", bd_addr_to_str(remoteAddr));

			connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
			context->counter = 'A';
			context->test_data_len = ATT_DEFAULT_MTU - 3;
			context->connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
			// print connection parameters (without using float operations)
			conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
			log_debug("Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
			log_debug("Connection Latency: %u\n", hci_subevent_le_connection_complete_get_conn_latency(packet));
			// initialize gatt client context with handle, and add it to the list of active clients
			// query primary services
			log_debug("Search for LE Streamer service.\n");
			 if(le_state == TC_W4_CONNECT) {
				log_debug("Search for LE Streamer service 111.\n");
				le_state = TC_W4_SERVICE_RESULT;
				gatt_client_discover_primary_services_by_uuid128(handle_gatt_client_event, connection_handle, le_streamer_service_uuid);
			 }
			log_debug("Search for LE Streamer service. 222\n");
			break;	
		
		//zhufeng
		case RFCOMM_EVENT_CAN_SEND_NOW:
			rfcomm_event_can_send_now = 0;
			log_debug("RFCOMM_EVENT_CAN_SEND_NOW");
			break;
		
		default:
			break;
			}
			
			break;
			
		case RFCOMM_DATA_PACKET:
			log_debug("packet_type:%d RecvsLen:%d",packet_type, size);
			 //recv_callback(packet, size);
			//for (i = 0; i < size; i++) {
				//log_debug("%02x ", ((char*)packet)[i]);
		   // }
			//gGetRfcomData = 1;
			iRet = QuePuts((T_Queue *)(&ptBtOpt->tMasterRecvQue), packet, size);
			//log_debug("QuePuts iRet:%d",iRet);
			
			break;

        default:
            break;
    }
}

static void update_auth_req(void){
    gap_set_bondable_mode(gap_bondable);
    gap_auth_req = 0;
    if (gap_mitm_protection){
        gap_auth_req |= 1;  // MITM Flag
    }
    if (gap_dedicated_bonding_mode){
        gap_auth_req |= 2;  // Dedicated bonding
    } else if (gap_bondable){
        gap_auth_req |= 4;  // General bonding
    }
    log_debug("Authentication Requirements: %u\n", gap_auth_req);
    gap_ssp_set_authentication_requirement(gap_auth_req);
}



static void send_ucd_packet(void){
    l2cap_reserve_packet_buffer();
    int ucd_size = 50;
    uint8_t * ucd_buffer = l2cap_get_outgoing_buffer();
    little_endian_store_16(ucd_buffer, 0, 0x2211);
    int i; 
    for (i=2; i< ucd_size ; i++){
        ucd_buffer[i] = i;
    }
    l2cap_send_prepared_connectionless(handle, L2CAP_CID_CONNECTIONLESS_CHANNEL, ucd_size);
}


static void ble_server_streamer(void* data, size_t size) {
	// find next active streaming connection
	int old_connection_index = connection_index;

	log_debug("streamer con_handle:%d, noti_enable:%d connection_index:%d", le_streamer_connections[connection_index].connection_handle, le_streamer_connections[connection_index].le_notification_enabled, connection_index);
	while (1) {
		// active found?
		if ((le_streamer_connections[connection_index].connection_handle != HCI_CON_HANDLE_INVALID) &&
		        (le_streamer_connections[connection_index].le_notification_enabled)) {
			break;
		}

		// check next
		next_connection_index();

		// none found
		if (connection_index == old_connection_index) {
			return;
		}
	}

	 le_streamer_connection_t* context = &le_streamer_connections[connection_index];

	// send
	if(size > (context->test_data_len)) {
		size = context->test_data_len;
	}
	log_debug("att_server_notify size:%d", size);
	att_server_notify(context->connection_handle, context->value_handle, (uint8_t*) data, size);

	// check next
	next_connection_index();
}


//get config API
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


typedef int (*pairData_callback_t)(int size, void* data);
pairData_callback_t pairDataWrite_callback = NULL;
pairData_callback_t pairDataRead_callback = NULL;

void SetPairDataWriteCallback(pairData_callback_t callback) {
	pairDataWrite_callback = callback;
}

void SetPairDataReadCallback(pairData_callback_t callback) {
	pairDataRead_callback = callback;
}

#if 1
int Rtk_PairDataWrite(int Length, unsigned char *Buffer)
{
	// if(Length < 0 || Length > WLT_BT_PAIRDATA_MAX_LEN || Buffer == NULL)
		// return -1;
	
	log_debug("Rtk_PairDataWrite Len: %d\r\n", Length);

	memcpy(gucBtPairData, Buffer, Length);
	log_debug("gucBtPairData:%s", gucBtPairData);
	giBtProfileUpdateFlag = 1;

	
	return Length;
}

int Rtk_PairDataRead(int Length, unsigned char *Buffer)
{
	// if(Length < 0 || Length > WLT_BT_PAIRDATA_MAX_LEN || Buffer == NULL)
		// return -1;

	
	log_debug("Rtk_PairDataRead Len: %d\r\n", Length);
	memcpy(Buffer, gucBtPairData,Length);

	return Length;
}
#endif


int s_RtkSetConfigFile(int index, uchar *data)
{
	int fd;
	fd = s_open(BT_PROFILE, O_RDWR, (uchar *)"\xff\x05");
	if (fd < 0)
	{
		return -1;
	}
	if (index == BT_NAME)
	{
		memcpy(gucBtProfileTemp, data,MAX_NAME_LEN);
	}
	else if (index == BT_PIN)
	{
		memcpy(gucBtProfileTemp+512, data, MAX_PIN_LEN);
	}
	else if (index == BT_PAIRDATA)
	{
		memcpy(gucBtProfileTemp+1024, data, WLT_BT_PAIRDATA_MAX_LEN);
	}
	write(fd, gucBtProfileTemp, sizeof(gucBtProfileTemp));
	close(fd);
	return 0;
}
int s_RtkGetConfigFile(int index, uchar *data)
{
	int fd;
	
	fd = s_open(BT_PROFILE, O_RDWR, (uchar *)"\xff\x05");
	if (fd < 0)
	{
		int iRet;
		char Temp[MAX_NAME_LEN];
		char TermName[16];
		bd_addr_t Local_BD_ADDR;
		//file not exist
		fd = s_open(BT_PROFILE, O_CREATE, (uchar *)"\xff\x05");
		if (fd < 0)
			return -1;
		
		seek(fd, 0, SEEK_SET);
		memset(gucBtProfileTemp, 0, sizeof(gucBtProfileTemp));
		memset(TermName, 0, sizeof(TermName));
		memset(Temp, 0, sizeof(Temp));
		get_term_name(TermName);
		log_debug("term name:%s", TermName);
		gap_local_bd_addr(Local_BD_ADDR);
		
		sprintf(Temp, "PAX_%s_%02X", TermName, Local_BD_ADDR);
		log_debug("Temp:%s", Temp);
		memcpy(gucBtProfileTemp, Temp, sizeof(Temp));
		
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
		memcpy(data, gucBtProfileTemp, MAX_NAME_LEN);
		log_debug("BT_NAME:%s", data);
	}
	else if (index == BT_PIN)
	{
		memcpy(data, gucBtProfileTemp+512, MAX_PIN_LEN);
	}
	else if (index == BT_PAIRDATA)
	{
		memcpy(data, gucBtProfileTemp+1024, WLT_BT_PAIRDATA_MAX_LEN);
	}
	return 0;
}


void s_RtkSetBtName(char *name)
{
	strcpy(gptBtOpt->LocalName, name);
}
void s_RtkGetBtName(char *name)
{
	strcpy(name, gptBtOpt->LocalName);
}

void s_RtkSetBtPin(char *pin)
{
	strcpy(gptBtOpt->LocalPin, pin);
}
void s_RtkGetBtPin(char *pin)
{
	strcpy(pin, gptBtOpt->LocalPin);
}

static void btRunLoop_timer_handler(void) {
	btstack_run_loop_embedded_execute_once();
}


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	log_debug("bt_main start");
	char NewName[MAX_NAME_LEN];
	char Pin[MAX_PIN_LEN];
	 
	hci_event_callback_registration.callback = &packet_handler;
	hci_add_event_handler(&hci_event_callback_registration);
	l2cap_init();
	rfcomm_init();
	rfcomm_register_service(packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff);
	// init SDP, create record for SPP and register with SDP
	sdp_init();
	memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
	spp_create_sdp_record(spp_service_buffer, 0x10001, RFCOMM_SERVER_CHANNEL, "ZF210 Streamer");
	sdp_register_service(spp_service_buffer);
	// log_print("SDP service record size: %u\n", de_get_len(spp_service_buffer));
	// short-cut to find other SPP Streamer
	gap_set_class_of_device(TEST_COD);
	gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
	
	
	//set local name
	memset(NewName, 0x00, sizeof(NewName));
	s_RtkGetConfigFile(BT_NAME, NewName);
	gap_set_local_name(NewName);
	s_RtkSetBtName(NewName);
	
	//set pin code
	s_RtkGetConfigFile(BT_PIN, Pin);
	s_RtkSetBtPin(Pin);
	
	gap_discoverable_control(1);
	gap_connectable_control(1);
	
	
#if 1
	//ble server
	// setup le device db
	le_device_db_init();

	// setup SM: Display only
	sm_init();

	// setup ATT server
	att_server_init(profile_data, att_read_callback, att_write_callback);
	att_server_register_packet_handler(packet_handler);

	// setup advertisements
	uint16_t adv_int_min = 0x0030;
	uint16_t adv_int_max = 0x0030;
	uint8_t adv_type = 0;
	bd_addr_t null_addr;
	memset(null_addr, 0, 6);
	gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0,
                                null_addr, 0x07, 0x00);
	gap_advertisements_set_data(adv_data_len, (uint8_t *)adv_data);
	gap_advertisements_enable(1);
  
  	// init client state
	init_connections();

	//ble client
	gatt_client_init();
#endif	
	
	// enabled EIR
	hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);
	// turn on!
	hci_power_control(HCI_POWER_ON);
	log_debug("bt_main end");


	while(1) {
		btstack_run_loop_embedded_execute_once();
		if(BtTestReady > 0) break;
	}
	
	
	// SetPairDataWriteCallback(Rtk_PairDataWrite);
	// SetPairDataReadCallback(Rtk_PairDataRead);
	//s_SetSoftInterrupt(81, btRunLoop_timer_handler);
	create_bt_send_data_task();
	create_bt_task();

	
	ptBtOpt->Connected = 0;
	
	return BT_RET_OK;
}


//classic API
static uint8_t inquiry_complete = 0;

static int get_inquiry_size(void) {
	return deviceCount;
}

static void set_inquiry_results(struct device *device) {
	devices = device;
}

static int is_inquiry_complete(void) {
	return inquiry_complete;
}

void set_inquiry_complete(uint8_t status) {
	inquiry_complete = status;
}


int bt_scan(struct device* device, unsigned int size, unsigned int timeout) {
	//UNUSED(size);
	unsigned int scanStartTime = 0;
	deviceCount = 0;
	gScanSize = size;
	
	set_inquiry_complete(0);
	gap_inquiry_start(timeout);
	set_inquiry_results(device);

	scanStartTime = GetTimerCount();
	for (;;) {
		if (is_inquiry_complete()) {
			break;
		}

		if((GetTimerCount() - scanStartTime) > timeout*1500) {
			log_error("bt_scan timeout:%d", timeout);
			break;
		}
		
		btstack_run_loop_embedded_execute_once();
	}

		
	
	log_info("[%s] done.", __func__);
	return get_inquiry_size();
}

int hasConnect = 0;
extern void create_bt_task(void);
void bt_connect(bd_addr_t addr) {
	unsigned int connectTimeT1 = 0;
	unsigned int connectTimeT2 = 0;
	unsigned int connectTimeEnd = 0;
	
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	connectTimeT1 = GetTimerCount();
	
	log_debug("bt_connect:addr:%s", bd_addr_to_str(addr));
	bd_addr_copy(remoteAddr, addr);
	
	sdp_client_query_rfcomm_channel_and_name_for_uuid(&packet_handler, addr, 0x1101);
	
	#if 1
	log_debug("classicConnectComplete1:%d classicConnectComplete2:%d", classicConnectComplete1, classicConnectComplete2);		
	for (;;) {
		connectTimeT2 = GetTimerCount();
		connectTimeEnd = connectTimeT2 - connectTimeT1;
			
		if(connectTimeEnd > 30000) {
			hasConnect = 1;
			ptBtOpt->Connected = 1;
			log_debug("zf:classicConnectComplete1:%d connectTimeEnd:%d ptBtOpt->Connected:%d hasConnect:%d", classicConnectComplete1, connectTimeEnd, ptBtOpt->Connected, hasConnect);	
			break;
		}
	
		btstack_run_loop_embedded_execute_once();
	}	
	#endif
	
	//hasConnect = 1;
	//ptBtOpt->Connected = 1;
}

void bt_disconnect(void) {
	log_info("[%s] cid: %d\n", __func__, rfcomm_channel_id);
	rfcomm_disconnect(rfcomm_channel_id);
}

void bt_rfcomm_send(void* data, size_t size) {

	log_debug("111 bt_rfcomm_send size:%d", size);
	log_debug("[%s] cid: %d\n", __func__, rfcomm_channel_id);
	if(size > spp_test_data_len) {
		size = spp_test_data_len;
	}

	rfcomm_send(rfcomm_channel_id, data, size);

}


//BLE
static uint8_t le_inquiry_complete = 0;

static void le_streamer_client_start(unsigned int timeout) {
	
	log_debug("Start scanning!\n");
	gap_set_scan_parameters(0,0x0030, 0x0030);
	gap_start_scan();
}


static int le_get_inquiry_size(void) {
	return le_deviceCount;
}

static void le_set_inquiry_results(struct le_device* device) {
	le_devices = device;
}

static int le_is_inquiry_complete(void) {
	return le_inquiry_complete;
}

void le_set_inquiry_complete(uint8_t status) {
	le_inquiry_complete = status;
}


int ble_scan(struct le_device* device, unsigned int size, unsigned int timeout) {
	// UNUSED(size);
	
	// unsigned int leScanStartTime;
	le_deviceCount = 0;
	gBleScanSize = size;
	gBleTimeout = timeout;
	
	le_set_inquiry_complete(0);
	le_streamer_client_start(timeout);
	le_set_inquiry_results(device);
	
	// leScanStartTime = GetTimerCount();
	for (;;) {
		if (le_is_inquiry_complete()) {
			break;
		}

		// if((GetTimerCount() - leScanStartTime) > timeout*1500) {
			// log_error("ble_scan timeout:%d", timeout);
			// break;
		// }
		
		btstack_run_loop_embedded_execute_once();
	}

	log_info("[%s] done.", __func__);
	return le_get_inquiry_size();
}

void ble_connect(bd_addr_t addr) {
	//le_connection_set_bdaddr(addr);
	// uint16_t le_addr_type = get_le_addr_type(addr);
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	bd_addr_copy(remoteAddr, addr);
	
	log_info("ble client connect addr:%s %d", bd_addr_to_str(addr), le_streamer_addr_type);
	gap_connect(addr, 0);
	
	log_debug("ble classicConnectComplete:%d", classicConnectComplete1);		
	for (;;) {
		if(classicConnectComplete1) {
			break;
		}

		btstack_run_loop_embedded_execute_once();
	}	
}

void ble_disconnect(void) {
	//uint16_t connection_handle = get_connection_info(addr, 0);
	log_info("[%s] ble client disconnect connection_handle: %d\n", __func__, connection_handle);
	gap_disconnect(connection_handle);
}

void ble_send(void* data, size_t size) {
	log_info("ble client send data");
	gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle, le_streamer_characteristic_rx.value_handle, 10, (uint8_t *) "0123456789");
	// gatt_client_write_value_of_characteristic_without_response(connection_handle, value_handle, size, (uint8_t*) data);
	log_info("send success");
	//l2cap_le_send_data(cid, (uint8_t*) data, size);
}

void ble_server_send(void* data, size_t size) {
	log_debug("ble_server_send:data:%s size:%d", data, size);
	ble_server_streamer(data, size);
}




int BtGetConfig(ST_BT_CONFIG *pCfg) {
	char *name = NULL;
	char pin[16+1];
	bd_addr_t addr;
	
	// if(!is_rtl8761()) return -255;

	if(giBtDevInited == 0) {
		log_error("<BtGetConfig> BtDev Not Init");
		return BT_RET_ERROR_NODEV;
	}
	
	if(pCfg == NULL) {
		log_error("BtGetConfig Error Parameters");
		return BT_RET_ERROR_PARAMERROR;
	}
	
	memset(pCfg, 0x00, sizeof(ST_BT_CONFIG));
	memset(name, 0x00, sizeof(name));
	memset(pin, 0x00, sizeof(pin));	
	
	gap_local_bd_addr(addr);
	log_debug("bt_get_config mac:%s", bd_addr_to_str(addr));
	bd_addr_copy(pCfg->mac, addr);
	
	name = gap_get_local_name();
	log_debug("bt_get_config name:%s", name);
	memcpy(pCfg->name, name, 64);
	
	s_RtkGetBtPin(pin);
	log_debug("bt_get_config pin:%s", pin);
	memcpy(pCfg->pin, pin, 16);
	
	return BT_RET_OK;
}

int BtSetConfig(ST_BT_CONFIG *pCfg) {
	char name[MAX_NAME_LEN+1];
	char pin[MAX_PIN_LEN+1];
	int i;
	
	// if(!is_rtl8761()) return -255;
	
	if(giBtDevInited == 0) {
		log_error("Bt Dev Not Init");
		return BT_RET_ERROR_NODEV;
	}
	
	if(pCfg == NULL) {
		log_error("BtGetConfig Error Parameters");
		BT_RET_ERROR_PARAMERROR;
	}
	
	if (strlen(pCfg->name) <= 1) return BT_RET_ERROR_PARAMERROR;
	if (strlen(pCfg->pin) > MAX_PIN_LEN) return BT_RET_ERROR_PARAMERROR;
	
	//set local name
	memset(name, 0x00, sizeof(name));
	for(i = 0; i < MAX_NAME_LEN; i++) {
		name[i] = pCfg->name[i];
	}	
	
	log_debug("bt set config name:%s", name);
	s_RtkSetBtName(name);
	s_RtkSetConfigFile(BT_NAME, name);
	
	//set pin code 
	memset(pin, 0x00, sizeof(pin));
	for(i = 0; i < MAX_PIN_LEN; i++)
		pin[i] = pCfg->pin[i];
	
	log_debug("bt set config pin:%s", pin);
	s_RtkSetBtPin(pin);
	s_RtkSetConfigFile(BT_PIN, pin);
	
	return BT_RET_OK;
}


int bt_get_status(ST_BT_STATUS *pStatus)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	// if(!is_wlt2564()) return -255;

	if(giBtDevInited == 0)
	{
		log_debug("Bt Dev Not Init");
		return BT_RET_ERROR_NODEV;
	}
	if(pStatus == NULL)
	{
		log_debug("BtGetConfig Error Parameters");
		return BT_RET_ERROR_PARAMERROR;
	}

	memset(pStatus, 0x00, sizeof(ST_BT_STATUS));

	if(ptBtOpt->Connected == 1) {
		pStatus->status = 1;
	} else {
		pStatus->status = 0;
	}

	//remote device info
	// log_debug("bt_get_status deviceInfo[deviceCount].name:%s", deviceInfoLocal[deviceCount].name);
	// memcpy(devInfo[0].name, deviceInfoLocal[deviceCount].name, 64);
	log_debug("bt_get_status mac:%s", bd_addr_to_str(remoteAddr));
	bd_addr_copy(pStatus->mac, remoteAddr);


	return BT_RET_OK;
}

//com API
unsigned char BtUserPortOpen(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(giBtDevInited == 0) {
		log_error("Bt Dev Not Init");
		return 0x03;
	}
	
	QueReset((T_Queue*)(&ptBtOpt->tMasterRecvQue));
	QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
	
	giBtPortInited = 1;
	
	return BT_RET_OK;
}

unsigned char BtUserPortClose(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(giBtDevInited == 0) {
		log_error("Bt Dev Not Init");
		return 0x03;
	}
	
	QueReset((T_Queue*)(&ptBtOpt->tMasterRecvQue));
	QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
		
	giBtPortInited = 0;
	
	return BT_RET_OK;
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

typedef enum {
	RL_OFF,
	RL_START,
	RL_GETDATA,
	RL_SENDING,
	RL_SENDRESULT,
	RL_SENDNEXT
} gSend_state_t;		
			
static gSend_state_t send_state = RL_OFF;	
extern int gSendComplete;
extern uint8_t EventTypeTest;
int gSendingData;
extern void hci_transport_link_send_ack_packet(void);

int gMutiSendingData = 0;
unsigned char BtUserPortSends(unsigned char *szData, unsigned short iLen)
{
	int iCapLen;
	uint t0;
	T_Queue *sendQueue = NULL;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	send_state = RL_START;
	int iRet = 1;
	unsigned char spp_data[110];
	static int iSPPSend = 0;
	int flag;
	//unsigned int t00,t1,t2;

	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		log_error("Bt Dev Not Init");
		return 0x03;
	}

	// 主连接
	if(ptBtOpt->Connected == 1)
	{
		sendQueue = (T_Queue *)&ptBtOpt->tMasterSendQue;
	}

	
	t0 = GetMs();
	while(QueIsFull(sendQueue))
	{
		if(GetMs() - t0 > 500)
		{
			log_error("BtMasterSends TimeOut");
			return 0x04;
		}
	}

	iCapLen = QueCheckCapability(sendQueue);
	if (iCapLen < iLen)
	{
		//return -4;
		log_error("QueCheckCapability err");
		return 0x04;
	}
	
	gMutiSendingData = 1;
	
	QuePuts(sendQueue, szData, iLen);

	log_debug("zf:BtMasterSends Success");
	gSendingData = 1;
	// DelayMs(10);
	// suspend_bt_task();
	// suspend_bt_block_received_task();
	#if 0
	// irq_save(flag);
	if(ptBtOpt->Connected && !QueIsEmpty((T_Queue *)(&ptBtOpt->tMasterSendQue))) {
		EventTypeTest = 0;
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
				gSendingData = 0;
				send_state = RL_START;
				QueReset((T_Queue*)(&ptBtOpt->tMasterSendQue));
				// resume_bt_block_received_task();
				
				// irq_restore(flag);
				break;
			}

			if(iLen && (RL_GETDATA == send_state)) {
				//hci_transport_link_send_ack_packet();
				iRet = rfcomm_send(rfcomm_channel_id, spp_data, iLen);
				if(iLen && (0 == iRet)) {
					//log_debug("rfcomm_send Seccuss <cnt:%d><len:%d> send_state:%d\r\n", iSPPSend++, iLen, send_state);
					QueGets((T_Queue *)(&ptBtOpt->tMasterSendQue), spp_data, iLen);
				}	
				send_state = RL_SENDING;
				//log_debug("rfcomm_send iLen:%d rfcomm_channel_id:%d iRet:%d EventTypeTest:%d send_state:%d", iLen, rfcomm_channel_id, iRet, EventTypeTest, send_state);	
			}
			
			// if(RL_SENDING == send_state) {
				//t00 = GetTimerCount();
				btstack_run_loop_embedded_execute_once();	
				//t1 = GetTimerCount();
				//t2 = t1 - t00;
				//log_debug("t0:%d t1:%d t2:%d\r\n", t00,t1,t2);
			// }
			
			if(RL_SENDING == send_state && (19 == EventTypeTest)) {	
				send_state = RL_SENDNEXT;
			}


		}  
	} 
	#endif
	
	
	return 0x00;
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
		log_error("Bt Dev Not Init");
		return -0x03;
	}
	
	// 主连接且不是连接座机
	if(ptBtOpt->Connected == 1)
	{
		recvQueue = (T_Queue *)&ptBtOpt->tMasterRecvQue;
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
		
		log_error("BtMasterRecvs Timeout");
		return -0xff;
	}
	
	log_debug("BtMasterRecvs Data");
	return iOffset;
}


unsigned char BtUserPortReset(void)
{
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	T_Queue *recvQueue = NULL;
	
	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		log_error("Bt Dev Not Init");
		return 0x03;
	}
	
	// 主连接
	if(ptBtOpt->Connected == 1)
	{
		recvQueue = (T_Queue *)&ptBtOpt->tMasterRecvQue;
	}

	QueReset(recvQueue);

	log_debug("BtMasterReset Success");
	return 0x00;
}


unsigned char BtUserPortTxPoolCheck(void)
{
	T_Queue *sendQueue = NULL;
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	if(giBtDevInited == 0 || giBtPortInited == 0)
	{
		log_error("Bt Dev Not Init");
		return 0x03;
	}	

	// 主连接
	if(ptBtOpt->Connected == 1)
	{
		sendQueue = (T_Queue *)&ptBtOpt->tMasterSendQue;
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
		log_error("Bt Dev Not Init");
		return -0x03;
	}
	
	if(szData == NULL)
	{
		return -1;
	}
	
	// 主连接
	if(ptBtOpt->Connected == 1)
	{
		recvQueue = (T_Queue *)&ptBtOpt->tMasterRecvQue;
	}
	
	iRet = QueGetsWithoutDelete(recvQueue, szData, iLen);

	log_debug("BtUserPortPeep Data");
	return iRet;
}





