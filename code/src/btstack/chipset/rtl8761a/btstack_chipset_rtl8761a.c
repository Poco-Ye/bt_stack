#include "btstack_config.h"
#include "btstack_chipset_rtl8761a.h"
#include "btstack_debug.h"

#define HCI_UART_H4 0
#define HCI_UART_BCSP 1
#define HCI_UART_3WIRE  2
#define HCI_UART_H4DS 3
#define HCI_UART_LL 4

// globals
static void (*download_complete)(int result);
static const btstack_uart_block_t* the_uart_driver;

extern int rtk_init(int fd, int proto, int speed);
static int rtl8761a_start(void) {
	int fd = 7;
// #ifdef ENABLE_UART_H4
	// return rtk_init(fd, HCI_UART_H4, 115200);
// #elif defined ENABLE_UART_H5
	return rtk_init(fd, HCI_UART_3WIRE, 115200);
// #else
// #error "Need to ENABLE_UART_H4 or ENABLE_UART_H5"
// #endif
}

void btstack_chipset_rtl8761a_download_firmware(const btstack_uart_block_t* uart_driver, void (*done)(int result)) {
	the_uart_driver   = uart_driver;
	download_complete = done;
	int res = the_uart_driver->open();

	if (res) {
		log_error("uart_block init failed %u", res);
		download_complete(res);
	}

	rtl8761a_start();
	#if 0//deleted by shics
           // notify main
          download_complete(0);// download completed		
	#endif
	extern volatile int download_fw_complete;
		
	DelayMs(3000);////////////////////SHICS
	download_fw_complete =1;
}

// not used currently

static const btstack_chipset_t btstack_chipset_rtl8761a = {
	"RTL8761A",
	NULL, // chipset_init not used
	NULL, // chipset_next_command not used
	NULL, // chipset_set_baudrate_command not needed as we're connected via SPI
	NULL, // chipset_set_bd_addr not provided
};

// MARK: public API
const btstack_chipset_t* btstack_chipset_rtl8761a_instance(void) {
	return &btstack_chipset_rtl8761a;
}