#include "icc_hard_bcm5892.h"
#include "ncn8025.h"
#include "icc_queue.h"



extern struct emv_core gl_emv_devs[];
extern volatile struct emv_queue sci_device[2];


static void ncn8025_pins_controller(unsigned int pin, unsigned int val);


static void ncn8025_enable_controller(void)
{
	sci_gpio_set_pin_type(NCN8025_NCMDVCC, GPIO_OUTPUT);
	sci_gpio_set_pin_type(NCN8025_RSTIN,   GPIO_OUTPUT);
	sci_gpio_set_pin_type(NCN8025_AUX1,    GPIO_OUTPUT);
	sci_gpio_set_pin_type(NCN8025_AUX2,    GPIO_OUTPUT);
	sci_gpio_set_pin_type(NCN8025_VSEL0,   GPIO_OUTPUT);
	sci_gpio_set_pin_type(NCN8025_VSEL1,   GPIO_OUTPUT);

	sci_gpio_set_pin_val(NCN8025_RSTIN,   LOW);
	sci_gpio_set_pin_val(NCN8025_AUX1,    LOW);
	sci_gpio_set_pin_val(NCN8025_AUX2,    LOW);
}

static void ncn8025_power_on_reset(void)
{
	ncn8025_pins_controller(CARD_VCC_PIN, VCC_MODE_OFF);
}

static void ncn8025_init()
{
	ncn8025_enable_controller();
	ncn8025_power_on_reset();
	DelayMs(10); /* to assure the power reset had taken affect */ 
}


static void sam_card_pin_init()
{
	sci_gpio_set_pin_type(SAM_RST,  GPIO_OUTPUT);
	sci_gpio_set_pin_type(SAM1_PWR,  GPIO_OUTPUT);
	sci_gpio_set_pin_type(SAM2_PWR,  GPIO_OUTPUT);
	sci_gpio_set_pin_type(SAM3_PWR,  GPIO_OUTPUT);

	sci_gpio_set_pin_type(SAM_SEL0, GPIO_OUTPUT);
	sci_gpio_set_pin_type(SAM_SEL1, GPIO_OUTPUT);

	sci_gpio_set_pin_val(SAM_RST, LOW);
	sci_gpio_set_pin_val(SAM1_PWR, LOW);
	sci_gpio_set_pin_val(SAM2_PWR, LOW);
	sci_gpio_set_pin_val(SAM3_PWR, LOW);

	sci_gpio_set_pin_val(SAM_SEL0, LOW);
	sci_gpio_set_pin_val(SAM_SEL1, LOW);

	if(sci_device_resource.sam_interface.sam4_pwr_pin != -1)
	{
		sci_gpio_set_pin_type(SAM4_PWR,  GPIO_OUTPUT);
		sci_gpio_set_pin_val(SAM4_PWR, LOW);
	}

	if(sci_device_resource.sam_interface.sam_sel_en_pin != -1)
	{
		sci_gpio_set_pin_type(SAM_SEL_EN, GPIO_OUTPUT);
		sci_gpio_set_pin_val(SAM_SEL_EN, 0);//enable
	}
}

int controller_init_ncn8025(void)
{
	ncn8025_init();
	sam_card_pin_init();
	return 0;
}



static void ncn8025_pins_controller(unsigned int pin, unsigned int val)
{
	if( CARD_VCC_PIN & pin )
	{
		switch ( val )
		{
			case VCC_MODE_OFF:
				sci_gpio_set_pin_val(NCN8025_NCMDVCC, HIGH);
			break;

			case VCC_MODE_ON:
				sci_gpio_set_pin_val(NCN8025_NCMDVCC, LOW);
			break;

			case VCC_VAL_1V8:
				sci_gpio_set_pin_val(NCN8025_VSEL0, HIGH);
				sci_gpio_set_pin_val(NCN8025_VSEL1, HIGH);
			break;

			case VCC_VAL_3V:
				sci_gpio_set_pin_val(NCN8025_VSEL0, LOW);
				sci_gpio_set_pin_val(NCN8025_VSEL1, LOW);
			break;

			case VCC_VAL_5V:
				sci_gpio_set_pin_val(NCN8025_VSEL0, LOW);
				sci_gpio_set_pin_val(NCN8025_VSEL1, HIGH);
			break;
		}
	}

	if( CARD_RST_PIN & pin )
	{
		sci_gpio_set_pin_val(NCN8025_RSTIN, val);
	}

	if( CARD_IO_PIN & pin )
	{
		sci_gpio_set_pin_val(NCN8025_IO, val);
	}
}


static void sam_power_signal( int devch, int vcc )
{
	switch(devch)
	{
		case SAM1_CH:		
			sci_gpio_set_pin_val(SAM1_PWR,!!vcc);
			break;
		case SAM2_CH:
			sci_gpio_set_pin_val(SAM2_PWR,!!vcc);
			break;
		case SAM3_CH:
			sci_gpio_set_pin_val(SAM3_PWR,!!vcc);
			break;
		case SAM4_CH:
			sci_gpio_set_pin_val(SAM4_PWR,!!vcc);
			break;	
		default:					
			break;					
	}
}


static void sci_convert_channel(int devch, int pin_mode)
{
	int ori_devch;
	unsigned long flag;
	if(USER_CH == devch)
		return;

	//!!!BCM592 CPU存在输出模式与输入模式GPIO值读取寄存器不同情况
	//ori_devch = sci_gpio_get_pin_val_output(sci_ncn8025.sam_interface->sam_sel0) + 
	//			sci_gpio_get_pin_val_output(sci_ncn8025.sam_interface->sam_sel1) * 2 + 1;
	//if(devch == ori_devch) 
	//{
	//	sci_gpio_set_pin_val(GPIOC, SAM_SEL_EN, LOW);
	//	return;
	//}

	sci_gpio_set_pin_val(SAM_SEL_EN,HIGH);//disable sel

	sci_gpio_set_pin_val(SAM_RST,HIGH);
	writel_and(~SCI_IO_NVAL, SC1_IF_CMD1_REG);
	DelayUs(5); /* to assure the logic level on  RST/IO pin had asserted */

	irq_save(flag); /* to assure SEL1 is synchronous with SEL0 */	
	switch(devch)
	{
		case SAM1_CH: 
			sci_gpio_set_pin_val(SAM_SEL0,LOW);
			sci_gpio_set_pin_val(SAM_SEL1,LOW);
		break;

		case SAM2_CH:
			sci_gpio_set_pin_val(SAM_SEL0,HIGH);
			sci_gpio_set_pin_val(SAM_SEL1,LOW);
		break;

		case SAM3_CH:	
			sci_gpio_set_pin_val(SAM_SEL0,LOW);
			sci_gpio_set_pin_val(SAM_SEL1,HIGH);
		break;

		case SAM4_CH:
			sci_gpio_set_pin_val(SAM_SEL0,HIGH);
			sci_gpio_set_pin_val(SAM_SEL1,HIGH);
		break;
	
		default:
		break;	
	}
	irq_restore(flag);

	if(pin_mode == 0)
	{
		//make sure SCI_IO_EN is disable when you call here
		sci_gpio_set_pin_val(SAM_RST,LOW);
		writel_or(SCI_IO_NVAL, SC1_IF_CMD1_REG);
	}

	sci_gpio_set_pin_val(SAM_SEL_EN,LOW);//enable sel
}


/* syncronous card interface by ncn8025 */

int Mc_Vcc_NCN8025( unsigned char devch, unsigned int mode )
{
	if (devch != USER_CH)
		return 0;

	sci_gpio_set_pin_val(NCN8025_NCMDVCC, HIGH);
	DelayMs( 10 ); /* to free the charge in chip */ 
	if(mode == 0)	return 0;

	switch(mode)
	{
		case 5000:
			ncn8025_pins_controller(CARD_VCC_PIN, VCC_VAL_5V);
			break;
		case 3000:
			ncn8025_pins_controller(CARD_VCC_PIN, VCC_VAL_3V);
			break;
		case 1800:
			ncn8025_pins_controller(CARD_VCC_PIN, VCC_VAL_1V8);		
			break;
		default:
			ncn8025_pins_controller(CARD_VCC_PIN, VCC_VAL_5V);
			break;
	}

	sci_gpio_set_pin_val(NCN8025_RSTIN, LOW);
	sci_gpio_set_pin_val(NCN8025_NCMDVCC, LOW);

	return 0;
}


void Mc_Reset_NCN8025( unsigned char devch, unsigned char level )
{
	if (devch != USER_CH)
		return;

	sci_gpio_set_pin_val(NCN8025_RSTIN, level);
}

void Mc_C4_Write_NCN8025( unsigned char devch, unsigned char level )
{
	if (devch != USER_CH)
		return;
	
	sci_gpio_set_pin_val(NCN8025_AUX1, level);
}

void Mc_C8_Write_NCN8025( unsigned char devch, unsigned char level )
{
	if (devch != USER_CH)
		return;
	sci_gpio_set_pin_val(NCN8025_AUX2, level);
}


/* asyncronous card interface by ncn8025 */

int sci_card_xmit_setting_ncn8025(int devch)
{
	if(USER_CH == devch)
		return 0;

	sci_convert_channel(devch, 1);
	return 0;
}


/* only offer RST pin control, this interface is used in intr */
void sci_card_pin_control_ncn8025(unsigned int pin, unsigned int val)
{
	/* if we want current sam slot info on reset, record it in cold_reset and warm_reset function */
	if( SAM_RST_PIN & pin )
	{
		sci_gpio_set_pin_val(SAM_RST,!!val);
	}

	if( CARD_RST_PIN & pin )
	{
		ncn8025_pins_controller(CARD_RST_PIN,!!val);
	}
}


/**
 * ===========================================================================
 * start ncn8025 auto-activation sequence
 *
 * parameters:
 *	slot         : [in] slot number
 *	voltage      : [in] output voltage (by mV)
 *	reset_timing : [in] reset maintain LOW level time
 * retval:
 *	0, successful
 *	others, unsuccessful
 */


int  sci_card_cold_reset_ncn8025(int devch, int voltage, int reset_timing)
{
	unsigned long flag;
	if (devch == USER_CH) {
		/* set voltage, and output power in synchronous mode */
		switch (voltage) {
		case 5000:
			ncn8025_pins_controller(CARD_VCC_PIN, VCC_VAL_5V);
			break;
		case 3000:
			ncn8025_pins_controller(CARD_VCC_PIN, VCC_VAL_3V);
			break;
		case 1800:
			ncn8025_pins_controller(CARD_VCC_PIN, VCC_VAL_1V8);		
			break;
		default:
			ncn8025_pins_controller(CARD_VCC_PIN, VCC_VAL_5V);
			break;
		}

		writel_and(~SCI_CLK_EN,SC0_CLK_CMD_REG);//disable CLK
		ncn8025_pins_controller(CARD_RST_PIN, 0);
		ncn8025_pins_controller(CARD_VCC_PIN, VCC_MODE_ON);


		irq_save(flag); /* to assure clk en is synchronous with counter start */
		writel_or(SCI_CLK_EN,SC0_CLK_CMD_REG);
		writel_or(SCI_TIMER_EN,SC0_TIMER_CMD_REG);
		irq_restore(flag);
	}
	else { //SAM card
		sci_convert_channel(devch, 0);

		writel_and(~SCI_IO_NVAL, SC1_IF_CMD1_REG);
		sam_power_signal(devch, 5000);

		writel_or(SCI_CLK_EN,SC1_CLK_CMD_REG);
		writel_or(SCI_TIMER_EN,SC1_TIMER_CMD_REG);
	}

	return 0;
}


int  sci_card_warm_reset_ncn8025(int devch, int reset_timing)
{
	unsigned long flag;

	if (devch == USER_CH) {
		irq_save(flag); /* to assure rst pulling low is synchronous with counter start  */
		ncn8025_pins_controller(CARD_RST_PIN, 0);
		writel_or(SCI_TIMER_EN,SC0_TIMER_CMD_REG); 
		irq_restore(flag);
	}
	else { //SAM card
		sci_convert_channel(devch, 1);
		
		/*xuwt 2015-11-05: 修正warm_reset问题，拉低RST无效*/
		sci_gpio_set_pin_val(SAM_RST, LOW); 
		writel_or(SCI_TIMER_EN,SC1_TIMER_CMD_REG); 
	}

	return 0;
}



int sci_card_powerdown_ncn8025(int devch)
{
	int dev_num;

	sci_convert_channel(devch, 1);
	

	if (devch == USER_CH) {
		ncn8025_pins_controller(CARD_VCC_PIN, VCC_MODE_OFF);
		/* stop SC_CLK */
		writel_and(~SCI_CLK_EN,SC0_CLK_CMD_REG);
		
	}
	else {
		sci_gpio_set_pin_val(SAM_RST, LOW);
		writel_or(SCI_IO_NVAL, SC1_IF_CMD1_REG);		
		DelayUs(5);
		sam_power_signal(devch, 0);

		/* stop SC_CLK */
		for (dev_num = SAM1_CH; dev_num < EMV_TERMINAL_NUM; dev_num++) {
			if ((EMV_IDLE != gl_emv_devs[dev_num].terminal_state) && (dev_num != (devch)))
				break;
		}
		if (dev_num == EMV_TERMINAL_NUM) {
			writel_and(~SCI_CLK_EN,SC1_CLK_CMD_REG);
		}
	}
	return 0;
}


/* global interface on ncn8025 */

void sci_usercard_detect_ncn8025(void)
{
	int card_pres_sta;
	
	card_pres_sta = (readl(SC0_STATUS1_REG) & SCI_CARD_PRES) ? 
					    CARD_IN_SOCKET : CARD_OUT_SOCKET;


	if(sci_device[0].dev_sta == card_pres_sta) //normal
	{
		return;
	}

	/* sci_device[0].dev_sta and readl(SC0_STATUS1_REG) are not the same.
	synchronize them. */
	disable_irq(IRQ_SMARTCARD0);
	if (readl(SC0_STATUS1_REG) & SCI_CARD_PRES)
	{
		if(sci_device[0].dev_sta != CARD_IN_SOCKET)
		{
			sci_device[0].dev_sta = CARD_IN_SOCKET;
			ScrSetIcon(ICON_ICCARD, ENABLE);
		}
	}
	else
	{
		if(sci_device[0].dev_sta != CARD_OUT_SOCKET)
		{
			sci_device[0].dev_sta = CARD_OUT_SOCKET;
			ScrSetIcon(ICON_ICCARD, DISABLE);
		}
	}
	enable_irq(IRQ_SMARTCARD0);
}

extern volatile unsigned int fifo0_time_status;
extern struct emv_core gl_emv_devs[];

void controller_intr_handler_ncn8025(unsigned long data)
{
	ncn8025_pins_controller(CARD_VCC_PIN, VCC_MODE_OFF);
	
	gl_emv_devs[0].terminal_state = EMV_IDLE;

	if (readl(SC0_STATUS1_REG) & SCI_CARD_PRES)  /* clr int sta bit*/
	{
		sci_device[0].dev_sta = CARD_IN_SOCKET;
		ScrSetIcon(ICON_ICCARD, ENABLE);
		insert_card_action();
		

	}
	else
	{
		sci_device[0].dev_sta = CARD_OUT_SOCKET;
		ScrSetIcon(ICON_ICCARD, DISABLE);
	}
	fifo0_time_status = TIME_OUT;
}

int controller_enable_ncn8025(int enable)
{
	return 0;
}




