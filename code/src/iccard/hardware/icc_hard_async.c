/**
 * =============================================================================
 * Process hardware driver in this document.
 * This document must be portable when change hardware platform.
 *
 * Author            : xuwt
 * Date              : 2011-10-9
 * Hardware platform : S80
 * Email             : xuwt@paxsz.com
 * =============================================================================
 */


#include "posapi.h"
#include "icc_hard_async.h"
#include "icc_hard_bcm5892.h"
#include "platform.h"
#include "..\protocol\icc_errno.h"
#include "icc_queue.h"

#include "icc_device_configure.h"

#include "ncn8025.h"
#include "tda8026.h"




enum PTYPE
{
	T0=0,
	T1,
};

enum SPEC
{
	EMV=0,
	ISO7816,
};

enum CONV
{
	DIRECT=0,
	REVERSE,
};


extern struct emv_core gl_emv_devs[];
extern volatile int tda8026_tasklet_enable;

#define RST_LOW_TIMER 0
#define TS_TIMER 1
#define TWT_TIMER 2
static unsigned int sci0_gen_timer_mode, sci1_gen_timer_mode;
static unsigned int sci0_ts_time_length, sci1_ts_time_length;


volatile unsigned int fifo0_time_status, fifo1_time_status;
volatile struct emv_queue sci_device[2]; 

static void sci_bcm5892_enable(int slot, int enable);
static int sci_bcm5892_set_etu(int slot, int fi, int di);
static void sci_bcm5892_set_atr_timing(int slot, struct emv_core *dcb);
static void sci_bcm5892_set_t0_timing(int slot, struct emv_core *dcb);
static void sci_bcm5892_set_t1_timing(int slot, struct emv_core *dcb);
static void sci0_irq_handler(void);
static void sci1_irq_handler(void);
       void sci_emv_hard_fifo_spill(int channel, int length);



/* card controller interface */
int (*controller_init)(void);
int (*controller_enable)(int enable);
int (*sci_card_xmit_setting)(int devch);
void (*sci_card_pin_control)(unsigned int pin, unsigned int val);
int  (*sci_card_cold_reset)(int devch, int voltage, int reset_timing);
int  (*sci_card_warm_reset)(int devch, int reset_timing);
int (*sci_card_powerdown)(int devch);
void (*sci_usercard_detect)(void);
void (*controller_intr_handler)(unsigned long data);





static void sci_device_reset( volatile struct emv_queue *device )
{
	device->txin_index = 0;
	device->txout_index = 0;
	device->rxin_index = 0;
	device->rxout_index = 0;
	device->parity_index = -1;
	device->timeout_index = -1;
	device->wait_counter = 0;
}

static void user_card_init()
{
	sci_gpio_set_pin_type(GPIOA, SCI0_INTPD, GPIO_FUNC0);
	sci_gpio_set_pin_type(GPIOA, SCI0_CLK, GPIO_FUNC0);
	sci_gpio_set_pin_type(GPIOA, SCI0_IO, GPIO_FUNC0);

	s_SetIRQHandler(IRQ_SMARTCARD0, (void (*)(int))sci0_irq_handler);
	sci_bcm5892_enable(SMARTCARD0, ENABLE);
	enable_irq(IRQ_SMARTCARD0);
}

static void sam_card_init()
{	
	sci_gpio_set_pin_type(GPIOA, SCI1_CLK,  GPIO_FUNC1);  
	sci_gpio_set_pin_type(GPIOA, SCI1_IO,   GPIO_FUNC1);

	s_SetIRQHandler(IRQ_SMARTCARD1, (void(*)(int))sci1_irq_handler);
	sci_bcm5892_enable(SMARTCARD1, ENABLE);	
	enable_irq(IRQ_SMARTCARD1);	
}

extern int sync_interface_init(unsigned int icc_controller_type);
static int sci_device_configure()
{
	int ret;
	ret = sci_resource_configure();
	if(ret < 0) return ret;

	/* interface select */
	sync_interface_init(sci_device_resource.icc_interfaceIC_type);
	if(sci_device_resource.icc_interfaceIC_type == TDA8026)
	{
		controller_init = controller_init_tda8026;
		controller_enable = controller_enable_tda8026;
		sci_card_xmit_setting = sci_card_xmit_setting_tda8026;
		sci_card_pin_control = sci_card_pin_control_tda8026;
		sci_card_cold_reset = sci_card_cold_reset_tda8026;
		sci_card_warm_reset = sci_card_warm_reset_tda8026;
		sci_card_powerdown = sci_card_powerdown_tda8026;
		sci_usercard_detect = sci_usercard_detect_tda8026;
		controller_intr_handler = controller_intr_handler_tda8026;
	}
	else if(sci_device_resource.icc_interfaceIC_type == NCN8025)
	{
		controller_init = controller_init_ncn8025;
		controller_enable = controller_enable_ncn8025;
		sci_card_xmit_setting = sci_card_xmit_setting_ncn8025;
		sci_card_pin_control = sci_card_pin_control_ncn8025;
		sci_card_cold_reset = sci_card_cold_reset_ncn8025;
		sci_card_warm_reset = sci_card_warm_reset_ncn8025;
		sci_card_powerdown = sci_card_powerdown_ncn8025;
		sci_usercard_detect = sci_usercard_detect_ncn8025;
		controller_intr_handler = controller_intr_handler_ncn8025;
	}
	else
	{
		ScrCls();
        ScrPrint(0, 0, 0, "ICC Card    ");
        ScrPrint(0, 3, 0, "Invalid IC_CARD");
        getkey();
		return -1;
	}

	return 0;
}

int sci_hard_init(void)
{
	int ret;
	ret = sci_device_configure();
	if(ret < 0)
	{
		/* 配置失败，阻止icc API的使用 */
		sci_device_resource.user_slot_num = 0;
		sci_device_resource.sam_slot_num = 0;
		return ret;
	}
	
	controller_init();
	
	/*若通道不存在，则不启动对应的控制器。TDA8026若只有SAM卡也应启动SMARTCARD0 */
	if(sci_device_resource.sam_slot_num != 0)
	{
		sam_card_init();
	}
	if((sci_device_resource.user_slot_num != 0) || (sci_device_resource.icc_interfaceIC_type == TDA8026))
	{
		user_card_init();
	}
	if(sci_device_resource.user_slot_num != 0)
	{
		sci_usercard_detect();
	}
	//printk("dev %d\r\n", sci_device[0].dev_sta);

	return 0;
}

static void sci_bcm5892_enable(int devNum, int enable)
{
	if (SMARTCARD0 == devNum) 
	{
		if (enable) 
		{
			writel(SCI_ENABLE, SC0_MODE_REGISTER_REG); 
			writel(SCI_TBUF_RST | SCI_RBUF_RST, SC0_PROTO_CMD_REG); 
			while (readl(SC0_PROTO_CMD_REG) & (SCI_TBUF_RST | SCI_RBUF_RST))
				continue;

			writel(0x00, SC0_CLK_CMD_REG);
			writel(SCI_UART_RST, SC0_UART_CMD1_REG); 
			writel(SCI_PRES_POL, SC0_IF_CMD1_REG);

			writel(0x00,	     SC0_INTR_EN2_REG);
			writel(SCI_PRES,     SC0_INTR_EN1_REG);
		}
		else 
		{
			writel(0x00,	 SC0_INTR_EN2_REG);
			writel(0x00,     SC0_INTR_EN1_REG);
			writel(0x00, SC0_MODE_REGISTER_REG); /* disable sci chip */
		}
	}
	else if (SMARTCARD1 == devNum) 
	{
		if (enable) 
		{
			writel(SCI_ENABLE, SC1_MODE_REGISTER_REG);
			writel(SCI_TBUF_RST | SCI_RBUF_RST, SC1_PROTO_CMD_REG); 
			while (readl(SC1_PROTO_CMD_REG) & (SCI_TBUF_RST | SCI_RBUF_RST))
				continue;

			writel(0x00, SC1_CLK_CMD_REG);
			writel(SCI_UART_RST, SC1_UART_CMD1_REG);
			writel(SCI_PRES_POL, SC1_IF_CMD1_REG);

			writel(0x00, SC1_INTR_EN2_REG);
			writel(0x00, SC1_INTR_EN1_REG);
		}
		else 
		{
			writel(0x00, SC1_INTR_EN2_REG);
			writel(0x00, SC1_INTR_EN1_REG);
			writel(0x00, SC1_MODE_REGISTER_REG);  /* disable sci chip */
		}
	}
}

/**
 * set etu divider value
 *
 * parameters:
 *	devch : [in] slot number
 *	fi    : [in] frequency adjust integer
 *	di    : [in] speed adjust integer
 * return:
 *	 0 - success
 *	-1 - failure
 * 
 * formulation:
 * we fill SC_CLK_CMD[etu_clkdiv] with 1, SC_CLK_CMD[sc_clkdiv] with 8
 * then d1 = (Fi / Di) * (d0 / d2) - 1
 * and the d2 only select with in 31 or 32
 * sc_clkdiv : d0, prescale: d1, bauddiv: d2
 *
 */
static int sci_bcm5892_set_etu(int channel, int fi, int di)
{
	int sc_clkdiv = 8;
	int prescale = 0;
	int bauddiv = 31;
	int regval=0x80;

	if(8 == fi/di)
	{
		regval = SCI_CLK_EN | (0x05 << 4); /* SC_CLK divider = 8 */
		sc_clkdiv = 8;
		bauddiv = 31;
	}
	/*更改时钟频率为4MHZ    2016-12-7 */
	else 	
	{
		/* SC_CLK_CMD[etu_clkdiv] = 3 SC_CLK_CMD[sc_clkdiv] = 2 SC_CLK_CMD[bauddiv]= 32 */
		regval = SCI_CLK_EN | (0x01<< 4 ) | (0x02 << 1);
		sc_clkdiv = 2;
		bauddiv = 31;
	}
	if ((fi * sc_clkdiv) % (di * bauddiv)) 
	{
		bauddiv = 32;
		regval |= 1;  /* divide by 32 */
	}
	prescale = (fi * sc_clkdiv) / (di * bauddiv) - 1;
		
	if (prescale > 255 || prescale < 0)
		return -1;
	
	if (USER_CH == channel)
	{
		writel(regval,   SC0_CLK_CMD_REG);  
		writel(prescale, SC0_PRESCALE_REG); 
	}
	else
	{	
		writel(regval,   SC1_CLK_CMD_REG);
		writel(prescale, SC1_PRESCALE_REG);
	}

	return 0;
}

static void sci_bcm5892_set_atr_timing(int channel, struct emv_core *dcb)
{
	dcb->terminal_ptype = T1;//parity error deal is the same with T = 1 protocol
	if (USER_CH == channel) 
	{
		writel(0x00,	     SC0_INTR_EN2_REG);
		writel(SCI_PRES,     SC0_INTR_EN1_REG);

		writel(SCI_UART_RST, SC0_UART_CMD1_REG);
		while (readl(SC0_UART_CMD1_REG) & SCI_UART_RST)
			continue;

		writel(SCI_TBUF_RST | SCI_RBUF_RST, SC0_PROTO_CMD_REG);
		while (readl(SC0_PROTO_CMD_REG) & (SCI_TBUF_RST | SCI_RBUF_RST))
			continue;

		fifo0_time_status = TIME_NORMAL;
		writel(0x00,   SC0_UART_CMD2_REG);	/* disable parity error retransmission */
		writel(0x00,   SC0_TIMER_CMD_REG);	/* init timer */
		writel(SCI_GET_ATR, SC0_UART_CMD1_REG); /* indicate the next bit rev is TS */
		
		writel(10081 & 0xFF,             SC0_WAIT1_REG);   /* ATR: cwt=10080 etu */
		writel((10081 & 0xFF00) >> 8,    SC0_WAIT2_REG);
		writel((10081 & 0xFF0000) >> 16, SC0_WAIT3_REG);

		sci0_gen_timer_mode = RST_LOW_TIMER;
		if (dcb->terminal_spec == EMV)
		{
			sci0_ts_time_length = 125 * dcb->terminal_di;
		}
		else
		{
			sci0_ts_time_length = 240 * dcb->terminal_di;
		}
		writel((115 * dcb->terminal_di)& 0xFF, SC0_TIMER_CMP1_REG);  /* ATR: rst low time=115 etu */
		writel(((115 * dcb->terminal_di) & 0xFF00) >> 8, SC0_TIMER_CMP2_REG);
		/* wait time enable, general-purpose timer(now is rst low timer) will  enable in card rst function */
		writel(SCI_WAIT_EN, SC0_TIMER_CMD_REG);

#ifdef TDA8026_ASYNC_MODE_L1_TWT 		
		/* when using TDA8026's asyncronous mode, reset low timer and ts timer count is catched by tda8026, so using 
		  *general purpose timer as twt count here in EMV L1 test.*/
		sci0_gen_timer_mode = TWT_TIMER;
		writel(20160 & 0xFF, 	      SC0_TIMER_CMP1_REG);  /* twt = 20160 etu */
		writel((20160 & 0xFF00) >> 8, SC0_TIMER_CMP2_REG);
		writel(SCI_TIMER_MODE | SCI_TIMER_EN | SCI_WAIT_EN, SC0_TIMER_CMD_REG);
#endif

		readl(SC0_INTR_STAT1_REG); /* clear pending */
		readl(SC0_INTR_STAT2_REG);

		writel(SCI_TIMER | SCI_PRES,          SC0_INTR_EN1_REG);
		writel(SCI_RPAR | SCI_WAIT | SCI_RCV, SC0_INTR_EN2_REG);
	}
	else
	{
		writel(0x00,	 SC1_INTR_EN2_REG);
		writel(0x00,     SC1_INTR_EN1_REG);

		writel(SCI_UART_RST, SC1_UART_CMD1_REG);
		while (readl(SC1_UART_CMD1_REG) & SCI_UART_RST)
			continue;
		
		writel(SCI_TBUF_RST | SCI_RBUF_RST, SC1_PROTO_CMD_REG);
		while (readl(SC1_PROTO_CMD_REG) & (SCI_TBUF_RST | SCI_RBUF_RST))
			continue;
	
		fifo1_time_status = TIME_NORMAL;
		writel(0x00,   SC1_UART_CMD2_REG); /* disable parity error retransmission */
		writel(0x00,	SC1_TIMER_CMD_REG);
		writel(SCI_GET_ATR, SC1_UART_CMD1_REG); /* next bit received is TS */
		
		writel((10080 + 256) & 0xFF, 	       	 SC1_WAIT1_REG); 	/* ATR: cwt=10080 etu */
		writel(((10080 + 256) & 0xFF00) >> 8, 	 SC1_WAIT2_REG);
		writel(((10080 + 256) & 0xFF0000) >> 16, SC1_WAIT3_REG);

		sci1_gen_timer_mode = RST_LOW_TIMER;
		if (dcb->terminal_spec == EMV)
		{
			sci1_ts_time_length = 125 * dcb->terminal_di;
		}
		else
		{
			sci1_ts_time_length = 240 * dcb->terminal_di;
		}
		writel((115 * dcb->terminal_di)& 0xFF, SC1_TIMER_CMP1_REG);  /* ATR: rst low time=115 etu */
		writel(((115 * dcb->terminal_di) & 0xFF00) >> 8, SC1_TIMER_CMP2_REG);

		/* wait time enable, general-purpose timer(now is rst low timer) will  enable in card rst function */
		writel(SCI_WAIT_EN, SC1_TIMER_CMD_REG);

		readl(SC1_INTR_STAT1_REG); 
		readl(SC1_INTR_STAT2_REG);

		writel(SCI_TIMER, 					  SC1_INTR_EN1_REG);
		writel(SCI_RPAR | SCI_WAIT | SCI_RCV, SC1_INTR_EN2_REG);
	}
}

static void sci_bcm5892_set_t0_timing(int channel, struct emv_core *dcb)
{
	if (USER_CH == channel)
	{
		writel(0x00,	     SC0_INTR_EN2_REG);
		writel(SCI_PRES,     SC0_INTR_EN1_REG);

		writel(SCI_UART_RST, SC0_UART_CMD1_REG);
		while (readl(SC0_UART_CMD1_REG) & SCI_UART_RST)
			continue;
		
		writel(SCI_TBUF_RST | SCI_RBUF_RST, SC0_PROTO_CMD_REG);
		while (readl(SC0_PROTO_CMD_REG) & (SCI_TBUF_RST | SCI_RBUF_RST))
			continue;
		
		if(dcb->terminal_spec == EMV)
		{
			DelayUs(((dcb->terminal_fi / dcb->terminal_di) * (6 + 2)) / ICC_FREQ + 1);
		}
		else //if(dcb->terminal_spec == ISO7816)
		{
		/* 
		 * If N = 0 to 254, then before being ready to receive the next character, the card requires the following delay
		 * from the leading edge of the previous character (transmitted by the card or the interface device).
		 */

		/* When using D = 64, the interface device shall ensure a delay of at least 16 etu between the leading edge of
		 * the last received character and the leading edge of the character transmitted for initiating a command
		 */
			DelayUs(((dcb->terminal_fi / dcb->terminal_di) * (dcb->terminal_cgt - 10 + 6)) / ICC_FREQ + 1);
		}
		
		if (dcb->terminal_conv == DIRECT) 
		{   	
			writel(DIRECT << SCI_CONV, SC0_UART_CMD2_REG); 
		}
		else
		{  			
			writel(REVERSE << SCI_CONV, SC0_UART_CMD2_REG);
		}

		fifo0_time_status = TIME_NORMAL;
		writel(0x00,	SC0_TIMER_CMD_REG);

		writel(dcb->terminal_cgt - 12,	SC0_TGUARD_REG);
		/*
		 * 2015-08-21:xuwt
		 * 为兼容新加坡wire card，增加cgt的值，为了不影响正常的卡，该改动只针对高速卡
		 */
		if((dcb->terminal_fi / dcb->terminal_di < 93) && (dcb->terminal_cgt < 16))
		{
			writel(16 - 12,	SC0_TGUARD_REG);
		}

		writel(dcb->terminal_wwt & 0xFF,	     SC0_WAIT1_REG); 
		writel((dcb->terminal_wwt & 0xFF00) >> 8,    SC0_WAIT2_REG);
		writel((dcb->terminal_wwt & 0xFF0000) >> 16, SC0_WAIT3_REG);
		
		writel(SCI_WAIT_EN, SC0_TIMER_CMD_REG);
		writel_or( (4 << SCI_RPAR_RETRY) | (4 << SCI_TPAR_RETRY), SC0_UART_CMD2_REG); /* rx retry times: 4, tx retry times: 4 */
		
		readl(SC0_INTR_STAT1_REG); /* clear pending */
 		readl(SC0_INTR_STAT2_REG);
		writel(SCI_PRES | SCI_RETRY, 	      SC0_INTR_EN1_REG); 
		writel(SCI_RPAR | SCI_WAIT | SCI_RCV, SC0_INTR_EN2_REG);
	}
	else
	{
		writel(0x00,	 SC1_INTR_EN2_REG);
		writel(0x00,     SC1_INTR_EN1_REG);

		writel(SCI_UART_RST, SC1_UART_CMD1_REG);
		while (readl(SC1_UART_CMD1_REG) & SCI_UART_RST)
			continue;
		
		writel(SCI_TBUF_RST | SCI_RBUF_RST, SC1_PROTO_CMD_REG);
		while (readl(SC1_PROTO_CMD_REG) & (SCI_TBUF_RST | SCI_RBUF_RST))
			continue;

		if(dcb->terminal_spec == EMV)
		{
			DelayUs(((dcb->terminal_fi / dcb->terminal_di) * (6 + 2)) / ICC_FREQ + 1);
		}
		else //if(dcb->terminal_spec == ISO7816)
		{
		/* 
		 * If N = 0 to 254, then before being ready to receive the next character, the card requires the following delay
		 * from the leading edge of the previous character (transmitted by the card or the interface device).
		 */

		/* When using D = 64, the interface device shall ensure a delay of at least 16 etu between the leading edge of
		 * the last received character and the leading edge of the character transmitted for initiating a command
		 */
			DelayUs(((dcb->terminal_fi / dcb->terminal_di) * (dcb->terminal_cgt - 10 + 6)) / ICC_FREQ + 1);
		}

		if (dcb->terminal_conv == DIRECT)
		{  
			writel(DIRECT << SCI_CONV, SC1_UART_CMD2_REG); 
		}
		else /* REVERSE */
		{   
			writel(REVERSE << SCI_CONV, SC1_UART_CMD2_REG); 
		}

		fifo1_time_status = TIME_NORMAL;
		writel(0x00, SC1_TIMER_CMD_REG);

		writel(dcb->terminal_cgt - 12, SC1_TGUARD_REG);
		/*
		 * 2015-08-21:xuwt
		 * 为兼容新加坡wire card，增加cgt的值，为了不影响正常的卡，该改动只针对高速卡
		 */
		if((dcb->terminal_fi / dcb->terminal_di < 93) && (dcb->terminal_cgt < 16))
		{
			writel(16 - 12,	SC1_TGUARD_REG);
		}

		writel(dcb->terminal_wwt & 0xFF,	     SC1_WAIT1_REG);
		writel((dcb->terminal_wwt & 0xFF00) >> 8,    SC1_WAIT2_REG);
		writel((dcb->terminal_wwt & 0xFF0000) >> 16, SC1_WAIT3_REG);

		writel(SCI_WAIT_EN, SC1_TIMER_CMD_REG);
		writel_or( (4 << SCI_RPAR_RETRY) | (4 << SCI_TPAR_RETRY), SC1_UART_CMD2_REG); /* rx retry times: 4, tx retry times: 4 */
		
		readl(SC1_INTR_STAT1_REG); 
		readl(SC1_INTR_STAT2_REG);
		writel(SCI_RETRY, 	   SC1_INTR_EN1_REG);
		writel(SCI_WAIT | SCI_RCV, SC1_INTR_EN2_REG);
	}
}

static void sci_bcm5892_set_t1_timing(int channel, struct emv_core *dcb)
{
	if (USER_CH == channel) 
	{
		writel(0x00,	     SC0_INTR_EN2_REG);
		writel(SCI_PRES,     SC0_INTR_EN1_REG);

		writel(SCI_UART_RST, SC0_UART_CMD1_REG);
		while (readl(SC0_UART_CMD1_REG) & SCI_UART_RST)
			continue;
		
		writel(SCI_TBUF_RST | SCI_RBUF_RST, SC0_PROTO_CMD_REG);
		while (readl(SC0_PROTO_CMD_REG) & (SCI_TBUF_RST | SCI_RBUF_RST))
			continue;

		DelayUs(((dcb->terminal_fi / dcb->terminal_di) * (22 - 10 + 2)) / ICC_FREQ + 1);
		
		if (dcb->terminal_conv == DIRECT) 
		{   
			writel(DIRECT << SCI_CONV, SC0_UART_CMD2_REG); 
		}
		else /* REVERSE */
		{   
			writel(REVERSE << SCI_CONV, SC0_UART_CMD2_REG);
		}
		
		if (dcb->terminal_cgt == 11) 
		{		
			writel(0xFF, SC0_TGUARD_REG); 
		}
		else 
		{
			writel(dcb->terminal_cgt - 12, SC0_TGUARD_REG);
		}
		
		fifo0_time_status = TIME_NORMAL;
		writel(0x00, SC0_TIMER_CMD_REG); /* init timer */
		writel((dcb->terminal_bwt & 0xFF) + 20,	     SC0_WAIT1_REG);
		writel((dcb->terminal_bwt & 0xFF00) >> 8,    SC0_WAIT2_REG);
		writel((dcb->terminal_bwt & 0xFF0000) >> 16, SC0_WAIT3_REG);

		writel(SCI_WAIT_MODE | SCI_WAIT_EN | SCI_CWT_EN, SC0_TIMER_CMD_REG);
		writel(6, SC0_PROTO_CMD_REG); 	/* T1:cwt=6 */
		
		readl(SC0_INTR_STAT1_REG); 
		readl(SC0_INTR_STAT2_REG);

		writel(SCI_PRES,	SC0_INTR_EN1_REG);
		writel(SCI_CWT | SCI_WAIT | SCI_RCV, SC0_INTR_EN2_REG);
	}
	else 
	{
		writel(0x00,	 SC1_INTR_EN2_REG);
		writel(0x00,     SC1_INTR_EN1_REG);

		writel(SCI_UART_RST, SC1_UART_CMD1_REG);
		while (readl(SC1_UART_CMD1_REG) & SCI_UART_RST)
			continue;
		
		writel(SCI_TBUF_RST | SCI_RBUF_RST, SC1_PROTO_CMD_REG);
		while (readl(SC1_PROTO_CMD_REG) & (SCI_TBUF_RST | SCI_RBUF_RST))
			continue;

		DelayUs(((dcb->terminal_fi / dcb->terminal_di) * ( 22 - 10 + 2)) / ICC_FREQ + 1);
		
		if (dcb->terminal_conv == DIRECT)
		{   	
			writel(DIRECT << SCI_CONV, SC1_UART_CMD2_REG);
		}
		else /* REVERSE */
		{  	
			writel(REVERSE << SCI_CONV, SC1_UART_CMD2_REG);
		}

		if (dcb->terminal_cgt == 11)
		{	
			writel(0xFF, SC1_TGUARD_REG); 
		}
		else 
		{
			writel(dcb->terminal_cgt - 12, SC1_TGUARD_REG);
		}
		
		fifo1_time_status = TIME_NORMAL;
		writel(0x00, SC1_TIMER_CMD_REG); /* init timer */
		writel(dcb->terminal_bwt & 0xFF, 	     SC1_WAIT1_REG); 	
		writel((dcb->terminal_bwt & 0xFF00) >> 8,    SC1_WAIT2_REG);
		writel((dcb->terminal_bwt & 0xFF0000) >> 16, SC1_WAIT3_REG);

		writel(SCI_WAIT_MODE | SCI_WAIT_EN | SCI_CWT_EN, SC1_TIMER_CMD_REG);
		writel(6, SC1_PROTO_CMD_REG); /* T1: cwt = 6 etu */

		readl(SC1_INTR_STAT1_REG); 
		readl(SC1_INTR_STAT2_REG);
		writel(0x00,	SC1_INTR_EN1_REG);
		writel(SCI_CWT | SCI_WAIT | SCI_RCV, SC1_INTR_EN2_REG);
	}
}

/**
 * cold reset ICC
 *
 * parameters:
 *	devch	: the number of ICC socket
 *	dcb	: the pointer of sci DCB
 * retval:
 *	0, successfully
 *	others, unsuccessfully
 */
static int sci_bcm5892_cold_reset(struct emv_core *dcb)
{
	int channel = dcb->terminal_ch;

	sci_bcm5892_set_etu(channel, dcb->terminal_fi, dcb->terminal_di);
	sci_bcm5892_set_atr_timing(channel, dcb);	
	
	if(USER_CH == channel)
	{
		/* avoid confliction between async card and sync card*/
		sci_gpio_set_pin_type(GPIOA, SCI0_CLK, GPIO_FUNC0); 
		sci_gpio_set_pin_type(GPIOA, SCI0_IO, GPIO_FUNC0);

		sci_device_reset(&sci_device[0]);		
		sci_core_init(&(gl_emv_devs[0]), (struct emv_queue *)(&sci_device[0]));

		if(sci_device[0].dev_sta == CARD_OUT_SOCKET)
		{
			fifo0_time_status = TIME_OUT;
			return 0;
		}
	}
	else
	{	
		sci_device_reset(&sci_device[1]);
		sci_core_init(&(gl_emv_devs[channel]), (struct emv_queue *)(&sci_device[1]));
	}
	sci_card_cold_reset(channel,dcb->terminal_vcc, 42100);
	
	return 0;
}

/**
 * warm reset ICC
 *
 * parameters:
 *	devch	: the number of ICC socket (starting from 1)
 *	dcb	: the pointer of sci DCB
 * retval:
 *	0, successfully
 *	others, unsuccessfully
 */
static int sci_bcm5892_warm_reset(struct emv_core *dcb)
{
	int channel = dcb->terminal_ch;
	
	sci_bcm5892_set_etu(channel, dcb->terminal_fi, dcb->terminal_di);
	sci_bcm5892_set_atr_timing(channel, dcb);
	
	if(USER_CH == channel)
	{
		sci_device_reset(&sci_device[0]);
		if(sci_device[0].dev_sta == CARD_OUT_SOCKET)
		{
			fifo0_time_status = TIME_OUT;
			return 0;
		}
	}
	else
	{
		sci_device_reset(&sci_device[1]);
	}
	
	sci_card_warm_reset(channel, 42100);
	
	return 0;
}

/**
 * sci_bcm5892_xmit
 *
 * parameters:
 *	dcb	: the pointer of sci DCB
 * retval:
 *	0, successfully
 *	others, unsuccessfully
 */
static int sci_bcm5892_xmit(struct emv_core *dcb)
{
	int           	i;
	int           	channel  = dcb->terminal_ch;
	unsigned char 	*buff   = dcb->queue->txr_buff;
	int		n       = dcb->queue->txin_index;
	
	sci_bcm5892_set_etu(channel, dcb->terminal_fi, dcb->terminal_di);
	if (dcb->terminal_ptype == T0)
	{
		sci_bcm5892_set_t0_timing(channel, dcb);
	}
	else if (dcb->terminal_ptype == T1)
	{
		sci_bcm5892_set_t1_timing(channel, dcb);
	}

	sci_card_xmit_setting(channel);
	
	if (USER_CH == channel)
	{
		sci_device_reset(&sci_device[0]);
		
		sci_device[0].txin_index = n;
		for (i = 0; i < n; i++) 
		{
			writel(buff[i], SC0_TRANSMIT_REG);
		}

		sci_device[0].txout_index = n;
		writel(SCI_IO_EN | SCI_AUTO_RCV | SCI_TX_RX | SCI_XMIT_GO, SC0_UART_CMD1_REG);
	}
	else
	{
		sci_device_reset(&sci_device[1]);
		
		sci_device[1].txin_index = n;
		for (i = 0; i < n; i++) 
		{
			writel(buff[i], SC1_TRANSMIT_REG);
		}
		
		sci_device[1].txout_index = n;
		writel(SCI_IO_EN | SCI_AUTO_RCV | SCI_TX_RX | SCI_XMIT_GO, SC1_UART_CMD1_REG);
	}

	return 0;
}

/* interrupts service function for smartcard0 interface */
static void sci0_irq_handler(void)
{
	unsigned char intsta1, intsta2;
	int ret;

	intsta1 = readl(SC0_INTR_STAT1_REG); 
	intsta2 = readl(SC0_INTR_STAT2_REG);  
	intsta1 &= readl(SC0_INTR_EN1_REG);
	intsta2 &= readl(SC0_INTR_EN2_REG);
	
	if (intsta1 & SCI_RETRY) 
	{
		writel(0x00, SC0_UART_CMD1_REG); 
		sci_device[0].parity_index = sci_device[0].rxin_index;
	}

	if (intsta1 & SCI_PRES)
	{
		controller_intr_handler(0);
	}

	//if (intsta2 & SCI_ATRS) {}
	
	/* when T=0, we pay attention on retry intr but not this bit */
	//if (intsta1 & SCI_TPAR){}

	//if (intsta2 & SCI_RCV) {}
	
	if ((intsta2 & SCI_WAIT) || (intsta2 & SCI_CWT)) 
	{
		fifo0_time_status = TIME_OUT; /* T0: BWT or WWT, T1: BWT */
	}
	
	if (intsta1 & SCI_TIMER)
	{
		//printk("Gen Timer int SC0, mode:%d\r\n",sci0_gen_timer_mode);

		if(sci0_gen_timer_mode == RST_LOW_TIMER)
		{
			writel_or(SCI_IO_EN, SC0_UART_CMD1_REG);
			sci_card_pin_control(CARD_RST_PIN, 1);

			writel_and(~SCI_TIMER_EN, SC0_TIMER_CMD_REG);
			sci0_gen_timer_mode = TS_TIMER;
			writel(sci0_ts_time_length & 0xFF, SC0_TIMER_CMP1_REG);
			writel((sci0_ts_time_length & 0xFF00) >> 8, SC0_TIMER_CMP2_REG);
			writel_or(SCI_TIMER_EN, SC0_TIMER_CMD_REG);
		}
		else if(sci0_gen_timer_mode == TS_TIMER)
		{
			fifo0_time_status = TS_TIME_OUT;
			
			sci0_gen_timer_mode = TWT_TIMER;
			writel_and(~SCI_TIMER_EN, SC0_TIMER_CMD_REG);
			writel(20160 & 0xFF, SC0_TIMER_CMP1_REG);
			writel((20160 & 0xFF00) >> 8, SC0_TIMER_CMP2_REG);
			writel_or(SCI_TIMER_EN, SC0_TIMER_CMD_REG);
		}
		else
		{
			writel_and(~SCI_TIMER_EN, SC0_TIMER_CMD_REG);
			fifo0_time_status = TIME_OUT;
		}
	}
} 

static void sci1_irq_handler(void)
{
	unsigned char intsta1, intsta2;

	intsta1 = readl(SC1_INTR_STAT1_REG); 
	intsta2 = readl(SC1_INTR_STAT2_REG);  
	intsta1 &= readl(SC1_INTR_EN1_REG);
	intsta2 &= readl(SC1_INTR_EN2_REG);

	if (intsta1 & SCI_RETRY) 
	{
		writel(0x00, SC1_UART_CMD1_REG);
		sci_device[1].parity_index = sci_device[1].rxin_index;
	}

	//if (intsta1 & SCI_TPAR){}
	

	//if (intsta2 & SCI_RCV) {}

	if ((intsta2 & SCI_WAIT) || (intsta2 & SCI_CWT))
	{
		fifo1_time_status = TIME_OUT; /* T0 : wwt & bwt, T1 : bwt */
	}

	if (intsta1 & SCI_TIMER)
	{
		//printk("Gen Timer int SC1, mode:%d\r\n",sci1_gen_timer_mode);

		if(sci1_gen_timer_mode == RST_LOW_TIMER)
		{
			writel_or(SCI_IO_EN, SC1_UART_CMD1_REG);
			sci_card_pin_control(SAM_RST_PIN, 1);

			writel_and(~SCI_TIMER_EN, SC1_TIMER_CMD_REG);
			sci1_gen_timer_mode = TS_TIMER;
			writel(sci1_ts_time_length & 0xFF, SC1_TIMER_CMP1_REG);
			writel((sci1_ts_time_length & 0xFF00) >> 8, SC1_TIMER_CMP2_REG);
			writel_or(SCI_TIMER_EN, SC1_TIMER_CMD_REG);
		}
		else if(sci1_gen_timer_mode == TS_TIMER)
		{
			fifo1_time_status = TS_TIME_OUT;
			
			sci1_gen_timer_mode = TWT_TIMER;
			writel_and(~SCI_TIMER_EN, SC1_TIMER_CMD_REG);
			writel(20160 & 0xFF, SC1_TIMER_CMP1_REG);
			writel((20160 & 0xFF00) >> 8, SC1_TIMER_CMP2_REG);
			writel_or(SCI_TIMER_EN, SC1_TIMER_CMD_REG);
		}
		else
		{
			writel_and(~SCI_TIMER_EN, SC1_TIMER_CMD_REG);
			fifo1_time_status = TIME_OUT;
		}
	}

	//if (intsta2 & SCI_RPAR) {}
}

void sci_emv_hard_fifo_spill(int channel, int length)
{
	unsigned char regval;
	unsigned char status_regval;
	unsigned int temp_time_out_status;

	if( USER_CH == channel)
	{
		while(sci_device[0].rxin_index - sci_device[0].rxout_index < length)
		{
			temp_time_out_status = fifo0_time_status;
			status_regval = readl(SC0_STATUS2_REG);
			
			if (status_regval & SCI_R_EMPTY)
			{
				if (temp_time_out_status == TS_TIME_OUT)
				{
					if(sci_device[0].rxin_index == 0)
					{
						sci_device[0].timeout_index = sci_device[0].rxin_index;
						break;
					}
				}
			
				if (temp_time_out_status == TIME_OUT)
				{
					sci_device[0].timeout_index = sci_device[0].rxin_index;
					break;
				}		

				/*  retry error when T=0, parity_index is set in retry intr */
				if ((sci_device[0].parity_index != -1) && (gl_emv_devs[0].terminal_ptype == T0))
				{
					break;
				}
				
				continue;
			}

			regval = readl(SC0_RECEIVE_REG) & 0xFF;
			status_regval = readl(SC0_STATUS2_REG);

			if (!(status_regval & SCI_RPAR_ERR))
			{
				/* without parity err
				 * If txr_buff is full, we ignore the new data
				 * (only wait for time out).
				 */
				if (sci_device[0].rxin_index < 511)
				{
					sci_device[0].txr_buff[sci_device[0].rxin_index] = regval;
					sci_device[0].rxin_index++;
				}	
			}
			else if (gl_emv_devs[0].terminal_ptype == T1) 
			{
				/* with parity error 
				 * If txr_buff is full, we ignore the new data
				 * (only wait for time out).
				 */
				if (sci_device[0].rxin_index < 511)
				{
					sci_device[0].txr_buff[sci_device[0].rxin_index] = regval;
					sci_device[0].parity_index = sci_device[0].rxin_index++;
				}
			}
			else 
			{
				/* get a parity error or continue, parity error is set in retry intr when T=0 */
				if(sci_device[0].parity_index != -1)
					break;
			}
		}
	}
	else
	{
		while(sci_device[1].rxin_index - sci_device[1].rxout_index < length)
		{
			temp_time_out_status = fifo1_time_status;
			status_regval = readl(SC1_STATUS2_REG);
			
			if (status_regval & SCI_R_EMPTY) 
			{
				if (temp_time_out_status == TS_TIME_OUT) 
				{
					if(sci_device[1].rxin_index == 0)
					{
						sci_device[1].timeout_index = sci_device[1].rxin_index;
						break;
					}
				}
			
				if (temp_time_out_status == TIME_OUT) 
				{
					sci_device[1].timeout_index = sci_device[1].rxin_index;
					break;
				}

				/*  retry error when T=0, parity_index is set in retry intr */
				if ((sci_device[1].parity_index != -1) && (gl_emv_devs[channel].terminal_ptype == T0))
				{
					break;
				}
				
				continue;
			}

			regval = readl(SC1_RECEIVE_REG) & 0xFF;
			status_regval = readl(SC1_STATUS2_REG);
			
			if (!(status_regval & SCI_RPAR_ERR)) 
			{	 
				/* without parity error 
				 * If txr_buff is full, we ignore the new data
				 * (only wait for time out).
				*/
				if (sci_device[1].rxin_index < 511) 
				{
					sci_device[1].txr_buff[sci_device[1].rxin_index] = regval;
					sci_device[1].rxin_index++;
				}	
			}
			else if (gl_emv_devs[channel].terminal_ptype == T1) 
			{
				/* with parity error 
				 * If txr_buff is full, we ignore the new data
				 * (only wait for time out).
				 */
				if (sci_device[1].rxin_index < 511)
				{
					sci_device[1].txr_buff[sci_device[1].rxin_index] = regval;
					sci_device[1].parity_index = sci_device[1].rxin_index++;
				}
			}
			else 
			{
				/* get a parity error or continue, parity error is set in retry intr when T=0 */
				if(sci_device[1].parity_index != -1)
					break;
			}
		}
	}
}

int sci_isvalid_channel( int slot )
{	
	slot &= 0x07;

	if((slot == 0) && sci_device_resource.user_slot_num) return 1;
	if((slot > 1) && (slot < sci_device_resource.sam_slot_num + 2)) return 1;

	return 0;
}


/* just get slot number(apis) */
int sci_select_slot( int slot )
{
    return ( ( slot & 0x07 ) ? ( ( slot & 0x07 ) - 1 ) : 0 );
}

/* Detect whether the card is in socket or not(apis) */
int sci_hard_detect( struct emv_core *pdev )
{    
	unsigned char ch;
	int state;

	if ( USER_CH == pdev->terminal_ch )
	{
		/* sci_device[0].dev_sta is set by intr(pres_intr). in some worst condition, int may not work.if it is
		NCN8025, synchronize it by call  sci_usercard_detect. we can't do this in TDA8026, for it will clear
		tda8026's intr flag. */
		if(sci_device_resource.icc_interfaceIC_type == NCN8025)
		{
			sci_usercard_detect();
		}
		return (sci_device[0].dev_sta == CARD_IN_SOCKET) ? 1 : 0 ;
	}
	else
	{
		pdev->terminal_state = EMV_IDLE;
		sci_emv_hard_power_pump(pdev);
		sci_emv_hard_cold_reset(pdev);
		state = sci_queue_spill(pdev, &ch, 1);
		sci_emv_hard_power_dump(pdev);
	
		if(!state || ICC_ERR_PARS == state) return 1;
	}

	return 0;
}

/* power up(apis) */
int sci_emv_hard_power_pump( struct emv_core *pdev )
{  
	return 0;
}

/* executing power down procedure(apis) */
int sci_emv_hard_power_dump( struct emv_core *pdev )
{
	pdev->terminal_state = EMV_IDLE;
	pdev->terminal_open  = 0;

	if (pdev->terminal_ch == USER_SLOT)
	{
		writel(0x00,	     SC0_INTR_EN2_REG);
		writel(SCI_PRES,     SC0_INTR_EN1_REG);
		writel_and(~SCI_IO_EN, SC0_UART_CMD1_REG);
	}
	else
	{
		writel(0x00,	 SC1_INTR_EN2_REG);
		writel(0x00,     SC1_INTR_EN1_REG);
		writel_and(~SCI_IO_EN, SC1_UART_CMD1_REG);
	}

	sci_card_powerdown(pdev->terminal_ch);

	DelayMs(10); /* Error will happen in some special cards while without proper delay time here */
	
	return 0;
	
}


/* compatible with old version */
int sci_alarm_count()
{
	return 0;
}

/* for lower power consumption */
int sci_bcm5892_suspend()
{
	struct emv_core terminal;

	if(sci_device_resource.user_slot_num != 0)
	{
		terminal.terminal_ch = USER_CH;
		sci_emv_hard_power_dump(&terminal);
	}
	
	if(sci_device_resource.sam_slot_num != 0)
	{
		terminal.terminal_ch = SAM1_CH;
		sci_emv_hard_power_dump(&terminal);
		
		terminal.terminal_ch = SAM2_CH;
		sci_emv_hard_power_dump(&terminal);
	
		terminal.terminal_ch = SAM3_CH;
		sci_emv_hard_power_dump(&terminal);
	
		terminal.terminal_ch = SAM4_CH;
		sci_emv_hard_power_dump(&terminal);
	}
	
	sci_bcm5892_enable(SMARTCARD0, DISABLE);
	sci_bcm5892_enable(SMARTCARD1, DISABLE);

	controller_enable(DISABLE);

	sci_device[0].dev_sta = CARD_OUT_SOCKET;
	ScrSetIcon(ICON_ICCARD, DISABLE);

	return 0;
}

/* for lower power consumption */
int sci_bcm5892_resume()
{
	controller_enable(ENABLE);
	
	/*若通道不存在，则不启动对应的控制器。TDA8026若只有SAM卡也应启动SMARTCARD0 */
	if(sci_device_resource.sam_slot_num != 0)
	{
		sci_bcm5892_enable(SMARTCARD1, ENABLE);
	}
	if((sci_device_resource.user_slot_num != 0) || (sci_device_resource.icc_interfaceIC_type == TDA8026))
	{
		sci_bcm5892_enable(SMARTCARD0, ENABLE);
	}
	if(sci_device_resource.user_slot_num != 0)
	{
		sci_usercard_detect();
	}
}

/* Apis cold reset */
int sci_emv_hard_cold_reset( struct emv_core *pdev )
{	
	pdev->terminal_state = EMV_COLD_RESET;	
	return sci_bcm5892_cold_reset(pdev);
}

/* Apis warm reset */
int sci_emv_hard_warm_reset( struct emv_core *pdev )
{
	pdev->terminal_state = EMV_WARM_RESET;
	pdev->terminal_fi = pdev->terminal_implict_fi;
	pdev->terminal_di = pdev->terminal_implict_di;
	return sci_bcm5892_warm_reset(pdev);
}


/* Apis start data transmit */
int sci_emv_hard_xmit( struct emv_core *pdev )
{		
	return sci_bcm5892_xmit(pdev);
}

int sci_disturb_interruption( struct emv_core *pdev, int enable )
{
	if(enable)
	{
		writel(0x00, SC1_TIMER_CMD_REG); 
		writel(0x00, SC0_TIMER_CMD_REG);
	}
		
	return 0;
}


