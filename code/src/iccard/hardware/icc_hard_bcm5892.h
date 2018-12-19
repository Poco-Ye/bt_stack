#ifndef _ICC_HARD_BCM5892_H
#define _ICC_HARD_BCM5892_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"

#define	BIT0    (1<<0)
#define	BIT1    (1<<1)
#define BIT2    (1<<2)
#define	BIT3    (1<<3)
#define BIT4    (1<<4)
#define	BIT5    (1<<5)
#define	BIT6    (1<<6)
#define BIT7    (1<<7)

#define SCI0_BASE_ADDR	0xCE000
#define SCI1_BASE_ADDR	0xCF000

/*SCI0 REG*/
#define SC0_UART_CMD1_REG               (SCI0_BASE_ADDR + 0x00)
#define SCI_INV_PAR                     BIT7
#define SCI_GET_ATR                     BIT6
#define SCI_IO_EN                       BIT5
#define SCI_AUTO_RCV                    BIT4
#define	SCI_DIS_PAR                     BIT3
#define	SCI_TX_RX                       BIT2
#define SCI_XMIT_GO                     BIT1
#define SCI_UART_RST                    BIT0

#define SC0_IF_CMD1_REG                 (SCI0_BASE_ADDR + 0x04)
#define SCI_AUTO_CLK                    BIT7
#define SCI_AUTO_RST                    BIT5
#define SCI_AUTO_VCC                    BIT4
#define SCI_IO_NVAL                     BIT3
#define SCI_PRES_POL                    BIT2
#define SCI_RST_VAL                     BIT1
#define SCI_VCC_VAL                     BIT0

#define SC0_CLK_CMD_REG                 (SCI0_BASE_ADDR + 0x08)
#define	SCI_CLK_EN                      BIT7

#define SC0_PROTO_CMD_REG               (SCI0_BASE_ADDR + 0x0C)
#define SCI_CRC_LRC                     BIT7
#define SCI_EDC_EN                      BIT6
#define SCI_TBUF_RST                    BIT5
#define SCI_RBUF_RST                    BIT4

#define SC0_PRESCALE_REG                (SCI0_BASE_ADDR + 0x10)
#define SC0_TGUARD_REG                  (SCI0_BASE_ADDR + 0x14)
#define SC0_TRANSMIT_REG                (SCI0_BASE_ADDR + 0x18)
#define SC0_RECEIVE_REG                 (SCI0_BASE_ADDR + 0x1C)
#define SC0_TLEN2_REG                   (SCI0_BASE_ADDR + 0x20)
#define SC0_TLEN1_REG                   (SCI0_BASE_ADDR + 0x24)
#define SC0_FLOW_CMD_REG                (SCI0_BASE_ADDR + 0x28)
#define SC0_RLEN2_REG                   (SCI0_BASE_ADDR + 0x2C)
#define SCI_RPAR                        BIT7
#define SCI_ATRS                        BIT6
#define SCI_CWT                         BIT5
#define SCI_RLEN                        BIT4
#define SCI_WAIT                        BIT3
#define SCI_EVENT2                      BIT2
#define SCI_RCV                         BIT1
#define SCI_RREADY                      BIT0

#define SC0_RLEN1_REG                   (SCI0_BASE_ADDR + 0x30)
#define SCI_TPAR                        BIT7
#define SCI_TIMER                       BIT6
#define SCI_PRES                        BIT5
#define SCI_BGT                         BIT4
#define SCI_TDONE                       BIT3
#define SCI_RETRY                       BIT2
#define SCI_TEMPTY                      BIT1
#define SCI_EVENT1                      BIT0

#define SC0_STATUS1_REG                 (SCI0_BASE_ADDR + 0x34)
#define SCI_CARD_PRES                   BIT6

#define SC0_STATUS2_REG                 (SCI0_BASE_ADDR + 0x38)
#define SCI_RPAR_ERR                    BIT7
#define SCI_R_OVERFLOW                  BIT3
#define SCI_EDC_ERR                     BIT2
#define SCI_R_EMPTY                     BIT1
#define SCI_R_READY                     BIT0

#define SC0_UART_CMD2_REG               (SCI0_BASE_ADDR + 0x40)
#define SCI_CONV                        6
#define SCI_RPAR_RETRY                  3
#define SCI_TPAR_RETRY                  0

#define SC0_BGT_REG                     (SCI0_BASE_ADDR + 0x44)
#define SC0_TIMER_CMD_REG               (SCI0_BASE_ADDR + 0x48)
#define SCI_TIMER_SRC                   BIT7
#define SCI_TIMER_MODE                  BIT6
#define SCI_TIMER_EN                    BIT5
#define SCI_CWT_EN                      BIT4
#define SCI_WAIT_MODE                   BIT1
#define SCI_WAIT_EN                     BIT0

#define SC0_IF_CMD2_REG                 (SCI0_BASE_ADDR + 0x4C)
#define SC0_INTR_EN1_REG                (SCI0_BASE_ADDR + 0x50)
#define SC0_INTR_EN2_REG                (SCI0_BASE_ADDR + 0x54)
#define SC0_INTR_STAT1_REG              (SCI0_BASE_ADDR + 0x58)
#define SC0_INTR_STAT2_REG              (SCI0_BASE_ADDR + 0x5C)
#define SC0_TIMER_CMP1_REG              (SCI0_BASE_ADDR + 0x60)
#define SC0_TIMER_CMP2_REG              (SCI0_BASE_ADDR + 0x64)
#define SC0_TIMER_CNT1_REG              (SCI0_BASE_ADDR + 0x68)
#define SC0_TIMER_CNT2_REG              (SCI0_BASE_ADDR + 0x6C)
#define SC0_WAIT1_REG                   (SCI0_BASE_ADDR + 0x70)
#define SC0_WAIT2_REG                   (SCI0_BASE_ADDR + 0x74)
#define SC0_WAIT3_REG                   (SCI0_BASE_ADDR + 0x78)
#define SC0_EVENT1_CNT_REG              (SCI0_BASE_ADDR + 0x80)
#define SC0_EVENT1_CMP_REG              (SCI0_BASE_ADDR + 0x88)
#define SC0_EVENT1_CMD1_REG             (SCI0_BASE_ADDR + 0x90)
#define SC0_EVENT1_CMD2_REG             (SCI0_BASE_ADDR + 0x94)
#define SC0_EVENT1_CMD3_REG             (SCI0_BASE_ADDR + 0x98)
#define SC0_EVENT1_CMD4_REG             (SCI0_BASE_ADDR + 0x9C)
#define SCI_EVENT1_EN                   BIT7
#define SCI_INTR_AFTER_CMP1             BIT2
#define SCI_RUN_AFTER_RESET1            BIT1

#define SC0_EVENT2_CMP_REG              (SCI0_BASE_ADDR + 0xA0)
#define SC0_EVENT2_CNT_REG              (SCI0_BASE_ADDR + 0xA8)
#define SC0_EVENT2_CMD1_REG             (SCI0_BASE_ADDR + 0xB0)
#define SC0_EVENT2_CMD2_REG             (SCI0_BASE_ADDR + 0xB4)
#define SC0_EVENT2_CMD3_REG             (SCI0_BASE_ADDR + 0xB8)
#define SC0_EVENT2_CMD4_REG             (SCI0_BASE_ADDR + 0xBC)
#define SCI_EVENT2_EN                   BIT7
#define SCI_INTR_AFTER_CMP2             BIT2

#define SC0_DMA_RX_INTF_REG             (SCI0_BASE_ADDR + 0xC0)
#define SC0_DMA_TX_INTF_REG             (SCI0_BASE_ADDR + 0xC4)
#define SC0_MODE_REGISTER_REG           (SCI0_BASE_ADDR + 0xC8)
#define SCI_IO_SEL                      BIT6
#define SCI_SYNC_DEC                    BIT5
#define SCI_SYNC_CKEN                   BIT4
#define SCI_SYNC_TYPE                   BIT3
#define SCI_MODE                        BIT1
#define SCI_ENABLE                      BIT0

#define SC0_DB_WIDTH_REG                (SCI0_BASE_ADDR + 0xCC)
#define SC0_SYNCCLK_DIVIDER1_REG        (SCI0_BASE_ADDR + 0xD0)
#define SC0_SYNCCLK_DIVIDER2_REG        (SCI0_BASE_ADDR + 0xD4)
#define SC0_SYNC_XMIT_REG               (SCI0_BASE_ADDR + 0xD8)
#define SC0_SYNC_RCV_REG                (SCI0_BASE_ADDR + 0xDC)
#define SC0_PARAMETERS_REG              (SCI0_BASE_ADDR + 0xE0)
#define SC0_SETTINGS_REG                (SCI0_BASE_ADDR + 0xE4)
#define SC0_INTR_REG                    (SCI0_BASE_ADDR + 0xE8)

/*SCI1 REG*/
#define SC1_UART_CMD1_REG               (SCI1_BASE_ADDR + 0x00)
//#define SCI_INV_PAR                   BIT7
//#define SCI_GET_ATR                   BIT6
//#define SCI_IO_EN                     BIT5
//#define SCI_AUTO_RCV                  BIT4
//#define SCI_DIS_PAR                   BIT3
//#define SCI_TX_RX                     BIT2
//#define SCI_XMIT_GO                   BIT1
//#define SCI_UART_RST                  BIT0

#define SC1_IF_CMD1_REG                 (SCI1_BASE_ADDR + 0x04)
//#define SCI_AUTO_CLK                  BIT7
//#define SCI_AUTO_RST                  BIT5
//#define SCI_AUTO_VCC                  BIT4
//#define SCI_IO_NVAL                   BIT3
//#define SCI_PRES_POL                  BIT2
//#define SCI_RST_VAL                   BIT1
//#define SCI_VCC_VAL                   BIT0

#define SC1_CLK_CMD_REG                 (SCI1_BASE_ADDR + 0x08)
//#define SCI_CLK_EN                    BIT7

#define SC1_PROTO_CMD_REG               (SCI1_BASE_ADDR + 0x0C)
//#define SCI_CRC_LRC                   BIT7
//#define SCI_EDC_EN                    BIT6
//#define SCI_TBUF_RST                  BIT5
//#define SCI_RBUF_RST                  BIT4

#define SC1_PRESCALE_REG                (SCI1_BASE_ADDR + 0x10)
#define SC1_TGUARD_REG                  (SCI1_BASE_ADDR + 0x14)
#define SC1_TRANSMIT_REG                (SCI1_BASE_ADDR + 0x18)
#define SC1_RECEIVE_REG                 (SCI1_BASE_ADDR + 0x1C)
#define SC1_TLEN2_REG                   (SCI1_BASE_ADDR + 0x20)
#define SC1_TLEN1_REG                   (SCI1_BASE_ADDR + 0x24)
#define SC1_FLOW_CMD_REG                (SCI1_BASE_ADDR + 0x28)
#define SC1_RLEN2_REG                   (SCI1_BASE_ADDR + 0x2C)
//#define SCI_RPAR                      BIT7
//#define SCI_ATRS                      BIT6
//#define SCI_CWT                       BIT5
//#define SCI_RLEN                      BIT4
//#define SCI_WAIT                      BIT3
//#define SCI_EVENT2                    BIT2
//#define SCI_RCV                       BIT1
//#define SCI_RREADY                    BIT0

#define SC1_RLEN1_REG                   (SCI1_BASE_ADDR + 0x30)
//#define SCI_TPAR                      BIT7
//#define SCI_TIMER                     BIT6
//#define SCI_PRES                      BIT5
//#define SCI_BGT                       BIT4
//#define SCI_TDONE                     BIT3
//#define SCI_RETRY                     BIT2
//#define SCI_TEMPTY                    BIT1
//#define SCI_EVENT1                    BIT0

#define SC1_STATUS1_REG                 (SCI1_BASE_ADDR + 0x34)
//#define SCI_CARD_PRES                 BIT6

#define SC1_STATUS2_REG                 (SCI1_BASE_ADDR + 0x38)
//#define SCI_RPAR_ERR                  BIT7
//#define SCI_R_OVERFLOW                BIT3
//#define SCI_EDC_ERR                   BIT2
//#define SCI_R_EMPTY                   BIT1
//#define SCI_R_READY                   BIT0

#define SC1_UART_CMD2_REG               (SCI1_BASE_ADDR + 0x40)
//#define SCI_CONV                      6
//#define SCI_RPAR_RETRY                3
//#define SCI_TPAR_RETRY                0

#define SC1_BGT_REG                     (SCI1_BASE_ADDR + 0x44)
#define SC1_TIMER_CMD_REG               (SCI1_BASE_ADDR + 0x48)
//#define SCI_TIMER_SRC                 BIT7
//#define SCI_TIMER_MODE                BIT6
//#define SCI_TIMER_EN                  BIT5
//#define SCI_CWT_EN                    BIT4
//#define SCI_WAIT_MODE                 BIT1
//#define SCI_WAIT_EN                   BIT0

#define SC1_IF_CMD2_REG                 (SCI1_BASE_ADDR + 0x4C)
#define SC1_INTR_EN1_REG                (SCI1_BASE_ADDR + 0x50)
#define SC1_INTR_EN2_REG                (SCI1_BASE_ADDR + 0x54)
#define SC1_INTR_STAT1_REG              (SCI1_BASE_ADDR + 0x58)
#define SC1_INTR_STAT2_REG              (SCI1_BASE_ADDR + 0x5C)
#define SC1_TIMER_CMP1_REG              (SCI1_BASE_ADDR + 0x60)
#define SC1_TIMER_CMP2_REG              (SCI1_BASE_ADDR + 0x64)
#define SC1_TIMER_CNT1_REG              (SCI1_BASE_ADDR + 0x68)
#define SC1_TIMER_CNT2_REG              (SCI1_BASE_ADDR + 0x6C)
#define SC1_WAIT1_REG                   (SCI1_BASE_ADDR + 0x70)
#define SC1_WAIT2_REG                   (SCI1_BASE_ADDR + 0x74)
#define SC1_WAIT3_REG                   (SCI1_BASE_ADDR + 0x78)
#define SC1_EVENT1_CNT_REG              (SCI1_BASE_ADDR + 0x80)
#define SC1_EVENT1_CMP_REG              (SCI1_BASE_ADDR + 0x88)
#define SC1_EVENT1_CMD1_REG             (SCI1_BASE_ADDR + 0x90)
#define SC1_EVENT1_CMD2_REG             (SCI1_BASE_ADDR + 0x94)
#define SC1_EVENT1_CMD3_REG             (SCI1_BASE_ADDR + 0x98)
#define SC1_EVENT1_CMD4_REG             (SCI1_BASE_ADDR + 0x9C)
//#define SCI_EVENT1_EN                 BIT7
//#define SCI_INTR_AFTER_CMP1           BIT2

#define SC1_EVENT2_CMP_REG              (SCI1_BASE_ADDR + 0xA0)
#define SC1_EVENT2_CNT_REG              (SCI1_BASE_ADDR + 0xA8)
#define SC1_EVENT2_CMD1_REG             (SCI1_BASE_ADDR + 0xB0)
#define SC1_EVENT2_CMD2_REG             (SCI1_BASE_ADDR + 0xB4)
#define SC1_EVENT2_CMD3_REG             (SCI1_BASE_ADDR + 0xB8)
#define SC1_EVENT2_CMD4_REG             (SCI1_BASE_ADDR + 0xBC)
//#define SCI_EVENT2_EN                 BIT7
//#define SCI_INTR_AFTER_CMP2           BIT2

#define SC1_DMA_RX_INTF_REG             (SCI1_BASE_ADDR + 0xC0)
#define SC1_DMA_TX_INTF_REG             (SCI1_BASE_ADDR + 0xC4)
#define SC1_MODE_REGISTER_REG           (SCI1_BASE_ADDR + 0xC8)
//#define SCI_IO_SEL                    BIT6
//#define SCI_SYNC_DEC                  BIT5
//#define SCI_SYNC_CKEN                 BIT4
//#define SCI_SYNC_TYPE                 BIT3
//#define SCI_MODE                      BIT1
//#define SCI_ENABLE                    BIT0

#define SC1_DB_WIDTH_REG                (SCI1_BASE_ADDR + 0xCC)
#define SC1_SYNCCLK_DIVIDER1_REG        (SCI1_BASE_ADDR + 0xD0)
#define SC1_SYNCCLK_DIVIDER2_REG        (SCI1_BASE_ADDR + 0xD4)
#define SC1_SYNC_XMIT_REG               (SCI1_BASE_ADDR + 0xD8)
#define SC1_SYNC_RCV_REG                (SCI1_BASE_ADDR + 0xDC)
#define SC1_PARAMETERS_REG              (SCI1_BASE_ADDR + 0xE0)
#define SC1_SETTINGS_REG                (SCI1_BASE_ADDR + 0xE4)

/* bcm5892 SCI resource GPIOA */
#define SCI0_IO               sci_device_resource.sci_controller.sci_user_io
#define SCI0_CLK              sci_device_resource.sci_controller.sci_user_clk
#define SCI0_INTPD            sci_device_resource.sci_controller.sci_ex_intr
#define SCI1_IO               sci_device_resource.sci_controller.sci_sam_io
#define SCI1_CLK              sci_device_resource.sci_controller.sci_sam_clk

#define IRQ_SMARTCARD0                  25
#define IRQ_SMARTCARD1                  26
#define SMARTCARD0                      0
#define SMARTCARD1                      1

#define USER_CH                         0
#define SAM1_CH                         1
#define SAM2_CH                         2
#define SAM3_CH                         3
#define SAM4_CH                         4
#define SAM5_CH                         5
#define SAM6_CH                         6
	
#define USER_SLOT                       0
#define SAM1_SLOT                       2
#define SAM2_SLOT                       3
#define SAM3_SLOT                       4
#define SAM4_SLOT                       5
#define SAM5_SLOT                       6
#define SAM6_SLOT                       7

#define HIGH                            1
#define LOW                             0

#define ENABLE                          1
#define DISABLE                         0

#define CARD_IN_SOCKET		1
#define CARD_OUT_SOCKET		0

#define ICC_FREQ	    3  /* Icc card main clock frequency:3MHz */
#define TS_TIME_OUT 	2
#define TIME_OUT   	1
#define TIME_NORMAL	0



/* PIN define */
#define 	CARD_IO_PIN             ( 0x01 )
#define 	CARD_RST_PIN            ( 0x02 )
#define		CARD_VCC_PIN            ( 0x04 )
#define  	CARD_CLK_PIN            ( 0x08 )	
#define  	SAM_RST_PIN            	( 0x10 )


/* 2015-3-10 xuwt:
 * SCI的GPIO资源允许配置为-1，表示没有该口线。*/
/* GPIO function in sci */
static inline void sci_gpio_set_pin_type(int port, int subno, GPIO_PIN_TYPE pinType)
{ 
	if (subno != -1)
		gpio_set_pin_type(port, subno, pinType);
}

static inline void sci_gpio_set_pin_val(int port, int subno, int val)
{
	if (subno != -1)
		gpio_set_pin_val(port, subno, val);
}

static inline unsigned int sci_gpio_get_pin_val(int port, int subno)
{
	if (subno != -1)
		return gpio_get_pin_val(port, subno);

	return 0;
}


#ifdef __cplusplus
}
#endif

#endif 
