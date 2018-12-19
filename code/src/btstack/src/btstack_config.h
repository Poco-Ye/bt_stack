//
// btstack_config.h for most tests
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

//for debug
#if 1
extern void printk(const char *str, ...);

#ifndef LogPrintf
#define LogPrintf printk
#endif
#endif

// Port related features
#define HAVE_MALLOC
#if 0//shics
#define HAVE_POSIX_TIME
#define HAVE_POSIX_FILE_IO  
#define HAVE_BTSTACK_STDIN
#else //this is added by shics
//#define HAVE_EMBEDDED_TIME_MS
//#define HAVE_EMBEDDED_TICK 
#endif

#if 1//shics added
#define ENABLE_CLASSIC                  // Enable Classic related code in HCI and L2CAP
#define ENABLE_BLE                      // Enable BLE related code in HCI and L2CAP
//#define ENABLE_EHCILL                   // Enable eHCILL low power mode on TI CC256x/WL18xx chipsets
//#define ENABLE_LOG_DEBUG                // Enable log_debug messages
//#define ENABLE_LOG_ERROR                // Enable log_error messages
//#define ENABLE_LOG_INFO                 // Enable log_info messages
//#define ENABLE_SCO_OVER_HCI             //Enable SCO over HCI for chipsets (only TI CC256x/WL18xx, CSR + Broadcom H2/USB))
//#define ENABLE_HFP_WIDE_BAND_SPEECH     // Enable support for mSBC codec used in HFP profile for Wide-Band Speech
#define ENABLE_LE_PERIPHERAL            // Enable support for LE Peripheral Role in HCI and Security Manager
#define ENABLE_LE_CENTRAL               // Enable support for LE Central Role in HCI and Security Manager
//#define ENABLE_LE_SECURE_CONNECTIONS    // Enable LE Secure Connections
//#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS  //Use [micro-ecc library](https://github.com/kmackay/micro-ecc) for ECC operations
//#define ENABLE_LE_DATA_CHANNELS         // Enable LE Data Channels in credit-based flow control mode
//#define ENABLE_LE_DATA_LENGTH_EXTENSION   //Enable LE Data Length Extension support
#define ENABLE_LE_SIGNED_WRITE          // Enable LE Signed Writes in ATT/GATT
#define ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE   //Enable L2CAP Enhanced Retransmission Mode. Mandatory for AVRCP Browsing
//#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL   // Enable HCI Controller to Host Flow Control, see below
//#define ENABLE_CC256X_BAUDRATE_CHANGE_FLOWCONTROL_BUG_WORKAROUND   // Enable workaround for bug in CC256x Flow Control during baud rate change, see chipset docs
#endif

#if 0 //shics delete
#define ENABLE_SDP_DES_DUMP
#define ENABLE_SDP_EXTRA_QUERIES
#endif

#if 1 //shics added
#define HCI_HOST_ACL_PACKET_NUM 2 //Max number of ACL packets
#define HCI_HOST_ACL_PACKET_LEN  1021//Max size of HCI Host ACL packets
// #define HCI_HOST_SCO_PACKET_NUM  2//Max number of ACL packets
// #define HCI_HOST_SCO_PACKET_LEN 1021 //Max size of HCI Host SCO packets
#endif

#if 0 //shics added 
#define HCI_ACL_PAYLOAD_SIZE  1021  //Max size of HCI ACL payloads
#define MAX_SPP_CONNECTIONS 1
//#define MAX_NR_BNEP_CHANNELS  // Max number of BNEP channels
//#define MAX_NR_BNEP_SERVICES  // Max number of BNEP services
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 3 // Max number of link key entries cached in RAM
//#define MAX_NR_GATT_CLIENTS  // Max number of GATT clients
#define MAX_NR_HCI_CONNECTIONS 1 // Max number of HCI connections
//#define MAX_NR_HFP_CONNECTIONS  // Max number of HFP connections
#define MAX_NR_L2CAP_CHANNELS 2  //  Max number of L2CAP connections
#define MAX_NR_L2CAP_SERVICES 2  //  Max number of L2CAP services
//#define MAX_NR_RFCOMM_CHANNELS  // Max number of RFOMMM connections
#define MAX_NR_RFCOMM_MULTIPLEXERS 1  // Max number of RFCOMM multiplexers, with one multiplexer per HCI connection
#define MAX_NR_RFCOMM_SERVICES 1  // Max number of RFCOMM services
#define MAX_NR_SERVICE_RECORD_ITEMS  1// Max number of SDP service records
//#define MAX_NR_SM_LOOKUP_ENTRIES  // Max number of items in Security Manager lookup queue
#define MAX_NR_WHITELIST_ENTRIES  1 // Max number of items in GAP LE Whitelist to connect to
#define MAX_NR_LE_DEVICE_DB_ENTRIES  2// Max number of items in LE Device DB
#endif


//#define HCI_ACL_PAYLOAD_SIZE 1021
#define MAX_SPP_CONNECTIONS 1
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS MAX_SPP_CONNECTIONS
#define MAX_NR_L2CAP_SERVICES  2
#define MAX_NR_L2CAP_CHANNELS  (1+MAX_SPP_CONNECTIONS)
#define MAX_NR_RFCOMM_MULTIPLEXERS MAX_SPP_CONNECTIONS
#define MAX_NR_RFCOMM_SERVICES 1
#define MAX_NR_RFCOMM_CHANNELS MAX_SPP_CONNECTIONS
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 4
#define MAX_NR_BNEP_SERVICES 0
#define MAX_NR_BNEP_CHANNELS 0
#define MAX_NR_HFP_CONNECTIONS 0
#define MAX_NR_WHITELIST_ENTRIES 0
#define MAX_NR_SM_LOOKUP_ENTRIES 0
#define MAX_NR_SERVICE_RECORD_ITEMS 1
#define MAX_NR_LE_DEVICE_DB_ENTRIES 4
#define HCI_INCOMING_PRE_BUFFER_SIZE 6
#define NVM_NUM_LINK_KEYS 4

#define HCI_INCOMING_PRE_BUFFER_SIZE 14 // sizeof benep heade, avoid memcpy
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)

#if 1 //shics added
//#define NVM_NUM_LINK_KEYS        2 // Max number of Classic Link Keys that can be stored 
//#define NVM_NUM_DEVICE_DB_ENTRIES  4// Max number of LE Device DB entries that can be stored
//#define NVN_NUM_GATT_SERVER_CCC   // Max number of 'Client Characteristic Configuration' values that can be stored by GATT Server
#endif

// BTstack configuration. buffers, sizes, ...
//#define HCI_INCOMING_PRE_BUFFER_SIZE 6

// #define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL

#if 1 //shics
#define  HAVE_EMBEDDED_TICK
#endif

#define HAVE_MONITOR_FILE_IO
#define  ENABLE_LOG_ERROR
#define  ENABLE_LOG_INFO
#define  ENABLE_LOG_DEBUG

//#define DEBUG_FUNC
typedef struct 
{
	int call_num;
	int * func_addr;
} FUNC_CALL_INFO_T;

extern FUNC_CALL_INFO_T  func_call_info[128];
#endif

