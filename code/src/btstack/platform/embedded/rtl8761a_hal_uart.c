#include <string.h>

#include "btstack_config.h"
#include "btstack_debug.h"
#include "hal_uart_dma.h"
#include "bt_init.h"
#include "queue1.h"

enum CHANNEL_NO {
	P_RS232A = 0,
	P_RS232B,
	P_WNET,
	P_PINPAD,
	P_MODEM,
	P_WIFI,
	P_BARCODE,
	P_BT,
	P_GPS,
	CHANNEL_LIMIT,
	P_USB = 10,
	P_USB_DEV,
	P_USB_HOST,
	P_USB_CCID,
	P_USB_HOST_A,
	P_BASE_DEV,
	P_BASE_HOST,
	P_BASE_A,
	P_BASE_B,
	P_PORT_MAX
};

static void dummy_handler(void);

extern volatile int download_fw_complete;


// handlers
static void (*rx_done_handler)(void) = &dummy_handler; // receive finish trriger
static void (*tx_done_handler)(void) = &dummy_handler; // send finish trriger

static void dummy_handler(void) {
	log_debug("dummy_handler 5892 called!!!!\r\n");
}

void hal_uart_dma_init(void) {
	log_info("Port open=%d\r\n", PortOpen(P_BT, "115200,8,E,1"));
	bt_disable_rts_cts();
}

/**
 * @brief Set callback for block received - can be called from ISR context
 * @param callback
 */
void hal_uart_dma_set_block_received(void (*callback)(void)) {
	rx_done_handler =
	    callback; // btstack_uart_block_received. receive finish trigger!!!!
	log_debug("set rx_done_handler:%x \r\n",
	          rx_done_handler); // btstack_uart_block_received
}

/**
 * @brief Set callback for block sent - can be called from ISR context
 * @param callback
 */
void hal_uart_dma_set_block_sent(void (*callback)(void)) {
	tx_done_handler =
	    callback; // btstack_uart_block_sent.  send finish trigger!!!!
	log_debug("set tx_done_handler:%x \r\n", tx_done_handler);
}

/**
 * @brief Set baud rate
 * @note During baud change, TX line should stay high and no data should be
 * received on RX accidentally
 * @param baudrate
 */
int hal_uart_dma_set_baud(uint32_t baud) {
	UNUSED(baud);
	return 0;
}

#ifdef HAVE_UART_DMA_SET_FLOWCONTROL
/**
 * @brief Set flowcontrol
 * @param flowcontrol enabled
 */
int hal_uart_dma_set_flowcontrol(int flowcontrol) {
}
#endif

extern volatile int download_fw_complete;

/**
 * @brief Send block. When done, callback set by hal_uart_set_block_sent must be
 * called
 * @param buffer
 * @param lengh
 */
void hal_uart_dma_send_block(const uint8_t* buffer, uint16_t len) {
	/*
	if(download_fw_complete)
	{
		log_info("\r\n<--Send[%d]-->\r\n", len);
		for(i=0; i < len && i < 16; i++)
		log_info("%02x, ",buffer[i]);
		log_info("\r\n<--Send End-->\r\n");
	}*/

	#if 1
	PortSends(P_BT, buffer, len);
	//log_debug("PortSends len:%d", len);
	(*tx_done_handler)();
	#endif
	
	#if 0
	int ret;
	DelayMs(60);// avoid sending too fast!!!
	
	ret = PortSends(P_BT, buffer, len);
	log_info("send block ret:%d", ret);
	(*tx_done_handler)();
	//log_info("send block 11 ret:%d", ret);
	#endif

}

/**
 * @brief Receive block. When done, callback set by
 * hal_uart_dma_set_block_received must be called
 * @param buffer
 * @param lengh
 */
void hal_uart_dma_receive_block(uint8_t* buffer, uint16_t len) {
#if 1
		uint16_t ret = 0, received = 0;
		memset(buffer, 0, len);
		
		while (received < len) {
			ret = PortRecvs(P_BT, buffer + received, len - received, 2);
			//log_debug("hal_uart_dma_receive_block: len=%d,ret=%d:\r\n", len,ret);
			if (ret > 0) {
				received += ret;
			} else {
				break;
			}
		}
		//log_debug("hal_uart_dma_receive_block: len=%d,ret=%d:\r\n", len,ret);
		
		(*rx_done_handler)();

#else

		//SHICS  TODO:
		int i, ret, received,count;
		received=0;
		extern volatile int download_fw_complete;
		if(download_fw_complete > 0) 
		{
			count = len /100 + 90;
			if(count >180) count= 180;
		}
		else
		{
	
			count = len /100 + 5;
			if(count >120) count= 120;
		}
		while(received < len && count > 0)
		{
			ret= PortRecvs_std(P_BT, buffer+received, len-received, 3000);
			log_debug("hal_uart_dma_receive_block: len=%d,ret=%d:\r\n", len,ret);
			if(ret > 0)
			{
				received +=ret;
			}
			else if(ret == -255 ||ret == 0)//timeout
			{	
				count--;
				continue;
			}
		}
		if(ret == len){
			DelayMs(10);
			log_debug("RX done!\r\n");
			log_debug("hal_uart_dma_receive_block: len=%d,ret=%d:\r\n", len,ret);
			(*rx_done_handler)();
			//rx_done_handler();//added by shics btstack_uart_block_received. trigger!!!
		}
	
	#if 0 //shics
		if(download_fw_complete)
		{
			log_info("\r\n<--Recv[%d][%d]-->\r\n", len, received);
			for(i=0; i < received && i <16; i++)
			log_info("%02x, ",buffer[i]);
			log_info("\r\n<--Recv End-->\r\n");
		}
	#endif

	
#endif

}

extern int gEnterRecvRunLoop;
extern tBtCtrlOpt *getBtOptPtr(void);

void rx_done_handler_ex() {
	tBtCtrlOpt *ptBtOpt = getBtOptPtr();

	
	if (rx_done_handler == dummy_handler) {
		return;
	}
	//log_debug("zf:rx_done_handler_ex");
	
	//if(ptBtOpt->Connected) {
		//gEnterRecvRunLoop = 1;
	//}

	rx_done_handler();
}

void tx_done_handler_ex() {
	if (tx_done_handler == dummy_handler) {
		return;
	}

	tx_done_handler();
}


/**
 * @brief Set or clear callback for CSR pulse - can be called from ISR context
 * @param csr_irq_handler or NULL to disable IRQ handler
 */
void hal_uart_dma_set_csr_irq_handler(void (*csr_irq_handler)(void)) {
	UNUSED(csr_irq_handler);
	log_debug("hal_uart_dma_set_csr_irq_handler called!\r\n");
}

/**
 * @brief Set sleep mode
 * @param block_received callback
 */
void hal_uart_dma_set_sleep(uint8_t sleep) {
	UNUSED(sleep);
	log_debug("hal_uart_dma_set_sleep called!\r\n");
}

#ifdef HAVE_HAL_UART_DMA_SLEEP_MODES

/**
 * @brief Set callback for block received - can be called from ISR context
 * @returns list of supported sleep modes
 */
int hal_uart_dma_get_supported_sleep_modes(void) {
	// SHICS  TODO:
	log_debug("hal_uart_dma_get_supported_sleep_modes called!\r\n");
}

/**
 * @brief Set sleep mode
 * @param sleep_mode
 */
void hal_uart_dma_set_sleep_mode(btstack_uart_sleep_mode_t sleep_mode) {
	// SHICS  TODO:
	log_debug("hal_uart_dma_set_sleep_mode called!\r\n");
}
#endif
