#ifndef __BTSTACK_CHIPSET_RTL8761A_H
#define __BTSTACK_CHIPSET_RTL8761A_H

#if defined __cplusplus
extern "C" {
#endif

#include "btstack_chipset.h"
#include "btstack_uart_block.h"

	void btstack_chipset_rtl8761a_download_firmware(const btstack_uart_block_t* uart_driver, void (*done)(int result));
	const btstack_chipset_t* btstack_chipset_rtl8761a_instance(void);

#if defined __cplusplus
}
#endif

#endif // __BTSTACK_CHIPSET_RTL8761A_H