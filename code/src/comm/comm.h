#ifndef _COMM_H_
#define _COMM_H_

//#define PPP 
#define PHY_UART  4
#define SIO_SIZE 8193
#define TM_SIO PHY_UART/* soft timer no for SIO*/

enum CHANNEL_NO{P_RS232A=0,P_RS232B,P_WNET,P_PINPAD,P_MODEM,P_WIFI,P_BARCODE,P_BT,P_GPS,CHANNEL_LIMIT,
				P_USB=10,P_USB_DEV,P_USB_HOST,P_USB_CCID,P_USB_HOST_A,P_BASE_DEV,P_BASE_HOST,P_BASE_A,P_BASE_B,P_PORT_MAX};

extern volatile ushort si_read[PHY_UART];   //  串口接收缓冲区读指针
extern volatile ushort si_write[PHY_UART];  // 串口接收缓冲区写指针
extern volatile ushort so_read[PHY_UART];   // 串口发送缓冲区读指针
extern volatile ushort so_write[PHY_UART];  // 串口发送缓冲区写指针
extern uchar  si_pool[PHY_UART][SIO_SIZE];  // 串口接收缓冲区
extern uchar  so_pool[PHY_UART][SIO_SIZE];  // 串口发送缓冲区
extern volatile ushort port_used[PHY_UART]; // 串口使用标志

//#define P_WIFI P_RS232B
void s_CommInit(void);

uchar PortTx(uchar channel,uchar ch);//for sys use only
uchar PortTxs(uchar channel,uchar *str,ushort str_len);//for sys use only
uchar PortRx(uchar channel,uchar *ch);//for sys use only
uchar PortStop(uchar channel);//for sys use only
//void interrupt_cts(void);
void LanSwitch(void);//let two ports not select LANPORT
int PortPeep(uchar channel,uchar *buf,ushort want_len);


#define UART_REF_CLK_FREQ   24000000
#define UART_BAUD_RATE      115200              /* Baud rate */
#define UART_DATABIT_8 (3 << 5)
#define UART_DATABIT_7 (2 << 5) 

#define UART_PARITY_ODD   (1 << 1 )
#define UART_PARITY_EVEN  ((1 << 1 ) | (1 << 2))
#define UART_PARITY_NONE  (0)

#define UART_STOPBIT_1    (0)
#define UART_STOPBIT_2    (1 << 3) 

#define UART_LCRH_FEN             (1 << 4) /* fifo enable */
#define UART_ENABLE_RX    ((1 << 0) |     0       | (1 << 9))
#define UART_ENABLE_TX    ((1 << 0) | (1 << 8) |      0     )
#define UART_ENABLE_RX_TX ((1 << 0) | (1 << 8) | (1 << 9))
#define UART_ENABLE_FLOWCTRL ((1 << 14) | (1 << 15))

#define UART_TXFIFO_EMPTY  1
#define UART_RXFIFO_EMPTY  2

#define UART_STATUS_OEIS            (1 << 10)       /* overrun error interrupt status */
#define UART_STATUS_BEIS            (1 << 9)        /* break error interrupt status */
#define UART_STATUS_PEIS            (1 << 8)        /* parity error interrupt status */
#define UART_STATUS_FEIS            (1 << 7)        /* framing error interrupt status */
#define UART_STATUS_RTIS            (1 << 6)        /* receive timeout interrupt status */
#define UART_STATUS_TXIS            (1 << 5)        /* transmit interrupt status */
#define UART_STATUS_RXIS            (1 << 4)        /* receive interrupt status */
#define UART_STATUS_DSRMIS          (1 << 3)        /* DSR interrupt status */
#define UART_STATUS_DCDMIS          (1 << 2)        /* DCD interrupt status */
#define UART_STATUS_CTSMIS          (1 << 1)        /* CTS interrupt status */
#define UART_STATUS_RIMIS           (1 << 0)        /* RI interrupt status */

/* -------------------------------------------------------------------------------
 *  From AMBA UART (PL010) Block Specification
 * -------------------------------------------------------------------------------
 *  UART Register Offsets.
 */
#define UART_DR              0x00    /* Data read or written from the interface. */
#define UART_RSR             0x04    /* Receive status register (Read). */
#define UART_ECR             0x04    /* Error clear register (Write). */
#define UART_FR              0x18    /* Flag register (Read only). */
#define UART_ILPR            0x20    /* IrDA low power counter register. */
#define UART_IBRD            0x24    /* Integer baud rate divisor register. */
#define UART_FBRD            0x28    /* Fractional baud rate divisor register. */
#define UART_LCRH            0x2c    /* Line control register. */
#define UART_CR              0x30    /* Control register. */
#define UART_IFLS            0x34    /* Interrupt fifo level select. */
#define UART_IMSC            0x38    /* Interrupt mask. */
#define UART_RIS             0x3c    /* Raw interrupt status. */
#define UART_MIS             0x40    /* Masked interrupt status. */
#define UART_ICR             0x44    /* Interrupt clear register. */
#define UART_DMACR           0x48    /* DMA control register. */


//--UART control register definition
#define rUART0CR          (*(volatile unsigned int*)0x000D0030)
#define rUART1CR          (*(volatile unsigned int*)0x000D1030)
#define rUART2CR          (*(volatile unsigned int*)0x000D2030)
#define rUART3CR          (*(volatile unsigned int*)0x000D3030)

//--CTS signal definition
#define rGPB_DIN          (*(volatile unsigned int*)0x000CD800)
#define rGPB_INT_MSK      (*(volatile unsigned int*)0x000CD818)
#define rGPB_INT_STAT     (*(volatile unsigned int*)0x000CD81C)
#define rGPB_INT_CLR      (*(volatile unsigned int*)0x000CD824)

#endif



