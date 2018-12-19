/**
 * Copyright (C) PAX Computer Technology Co., Ltd. 2010-2012
 *
 * available for NXP TDA8026
 *
 * platform: BCM5892 + NXP TDA8026
 *
 * Date : Oct, 10th 2011
 * Email: Liubo (liubo@paxsz.com or liubo1234@126.com)
 * ===========================================================================
 * Version History List:
 * ---------------------------------------------------------------------------
 * 2011-11-29	liubo	initial revision
 * ---------------------------------------------------------------------------
 * 2012-02-02	liubo	last (?) version
 * ---------------------------------------------------------------------------
 *
 * ===========================================================================
 * $Id: tda8026.c 5688 2012-04-06 05:35:27Z danfe $
 */

#include "tda8026.h"
#include "icc_hard_bcm5892.h"
#include "icc_queue.h"



extern struct emv_core gl_emv_devs[];
/*add by wanls 20160810*/
extern SCI_RESOURCE sci_device_resource;
int devch_backup = 0;
/*add end*/

/**
 * using software mode to implement the i2c function.
 */
static void i2c_delay(int dly);
static int tda8026_i2c_init(void);
static int tda8026_i2c_start(void);
static int tda8026_i2c_wbyte(unsigned char byte);
static int tda8026_i2c_rbyte(int ack);
static int tda8026_i2c_stop(void);
static int tda8026_i2c_read(unsigned char reg);
static int tda8026_i2c_write(unsigned char reg, unsigned char val);
static int tda8026_enable(int);
static int tda8026_probe(void);
static void tda8026_tasklet_proc(void);

static void enable_sci_irq();
static void disable_sci_irq();

static int tda8026_init(void);
static int tda8026_irq_source(void);
static int tda8026_irq_clear(int);
static int tda8026_release(void);
static int tda8026_switch_slot(int);
static int tda8026_cold_reset(int, int, int);
static int tda8026_warm_reset(int, int);
static int tda8026_deactivate(int);
static void tda8026_reset(int slot, int level);

static unsigned char last_regv[5];
static int last_slot;

static unsigned char second_tda8026_last_regv[5];
static int second_tda8026_last_slot = 2;

#define TDA8026_CHANNEL_SIZE (5)
#define LAST_TDA8026_CHANNEL TDA8026_CHANNEL_SIZE
#define SAM_START_CHANNEL    (2)


static void i2c_delay(int dly)
{
	volatile int tt;

	for (tt = 0; tt < (dly * 80); tt++)
		continue;
}

static int tda8026_i2c_init()
{
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CLKEN, 1);
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, 0);
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, (CRSR_SCL | CRSR_SDA));
	while ((CRSR_SCL | CRSR_SDA) != (BCM5892_READ_I2C_REG(BCM5892_I2C_CRSR) & (CRSR_SCL | CRSR_SDA)))
		continue;

	return 0;
}

static int tda8026_i2c_start()
{
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SCL | CRSR_SDA);
	i2c_delay(5);
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SCL);
	i2c_delay(5);
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, 0);
	i2c_delay(5);

	return 0;
}

static int tda8026_i2c_wbyte(unsigned char byte)
{
	int i = 0;
	unsigned char regval;

	for (i = 0; i < 8; i++) {
		if (byte & (1 << (7 - i))) {
			BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SDA);
			i2c_delay(1);
			BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR,
			    CRSR_SDA | CRSR_SCL);
			i2c_delay(2);
			BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SDA);
			i2c_delay(1);
		}
		else {
			BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, 0);
			i2c_delay(1);
			BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SCL);
			i2c_delay(2);
			BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, 0);
			i2c_delay(1);
		}
	}
	i2c_delay(2);
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SCL);
	i2c_delay(2);
	regval = BCM5892_READ_I2C_REG(BCM5892_I2C_CRSR);
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, (regval & 0x80));
	i2c_delay(2);

	return 0;
}

static int tda8026_i2c_rbyte(int ack)
{
	int i = 0;
	unsigned char regval;
	unsigned char byte;

	byte = 0;
	/* enable receiver */
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SDA);
	i2c_delay(1);
	for (i = 0; i < 8; i++) {
		BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SDA | CRSR_SCL);
		i2c_delay(2);
		regval = BCM5892_READ_I2C_REG(BCM5892_I2C_CRSR);
		if (regval & 0x80) {
			byte |= (1 << (7 - i));
		}
		BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SDA);
		i2c_delay(2);
	}

	if (ack) {
		BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SDA);
		i2c_delay(2);
		BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SDA | CRSR_SCL);
		i2c_delay(2);
		BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SDA);
	}
	else {
		BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, 0);
		i2c_delay(2);
		BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SCL);
		i2c_delay(2);
		BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, 0);
	}
	i2c_delay(2);

	return byte;
}

static int tda8026_i2c_stop()
{
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, 0);
	i2c_delay(5);
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SCL);
	i2c_delay(5);
	BCM5892_WRITE_I2C_REG(BCM5892_I2C_CRSR, CRSR_SCL | CRSR_SDA);
	i2c_delay(5);

	return 0;
}

int tda8026_i2c_read(unsigned char reg)
{
	unsigned char val;

	tda8026_i2c_start();
	tda8026_i2c_wbyte(reg | 1);
	val = tda8026_i2c_rbyte(1);
	tda8026_i2c_stop();

	return (int)val;
}

int tda8026_i2c_write(unsigned char reg, unsigned char val)
{
	tda8026_i2c_start();
	tda8026_i2c_wbyte(reg & 0xfe);
	tda8026_i2c_wbyte(val);
	tda8026_i2c_stop();

	return 0;
}



/*add by wanls 20160810*/
static int second_tda8026_i2c_init()
{
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SCL, GPIO_OUTPUT);
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_OUTPUT);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);
	
	return 0;
}

static int second_tda8026_i2c_start()
{
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SCL, GPIO_OUTPUT);
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_OUTPUT);
	
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);
	i2c_delay(5);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 0);
	i2c_delay(5);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
	i2c_delay(5);

	return 0;
}

static int second_tda8026_i2c_wbyte(unsigned char byte)
{
	int i = 0;
	unsigned char regval;
	
    sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_OUTPUT);
	for (i = 0; i < 8; i++) {
		if (byte & (1 << (7 - i))) {
			
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);
			
			i2c_delay(1);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);
			i2c_delay(2);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);
			i2c_delay(1);
		}
		else {
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 0);
			i2c_delay(1);

			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 0);
			i2c_delay(2);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
			sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 0);
			i2c_delay(1);
		}
	}
    sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_INPUT); 
	i2c_delay(2);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
	i2c_delay(2);
	regval = sci_gpio_get_pin_val(SECOND_TDA8026_I2C_SDA);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_OUTPUT); 

	i2c_delay(2); 

	return 0;
}

static int second_tda8026_i2c_rbyte(int ack)
{
	int i = 0;
	unsigned char regval;
	unsigned char byte;

	byte = 0;
	/* enable receiver */
    sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_INPUT); 
	

	
	i2c_delay(1);
	for (i = 0; i < 8; i++) {
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
		i2c_delay(2);
		regval = sci_gpio_get_pin_val(SECOND_TDA8026_I2C_SDA);
		if (regval) {
			byte |= (1 << (7 - i));
		}
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
		i2c_delay(2);
	}

	if (ack) {

        sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
		sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_OUTPUT); 
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);

		i2c_delay(2);
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);
		i2c_delay(2);
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
	}
	else {
		
        sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
		sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_OUTPUT); 
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 0);
		i2c_delay(2);
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 0);
		i2c_delay(2);
		sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
	}
	i2c_delay(2);

	return byte;
}

static int second_tda8026_i2c_stop()
{
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 0);
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_OUTPUT);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 0);
	i2c_delay(5);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
	i2c_delay(5);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);
	i2c_delay(5);
	return 0;
}

int second_tda8026_i2c_read(unsigned char reg)
{
	unsigned char val;

	second_tda8026_i2c_start();
	second_tda8026_i2c_wbyte(reg | 1);
	val = second_tda8026_i2c_rbyte(1);
	second_tda8026_i2c_stop();

	return (int)val;
}

int second_tda8026_i2c_write(unsigned char reg, unsigned char val)
{
	second_tda8026_i2c_start();
	second_tda8026_i2c_wbyte(reg & 0xfe);
	second_tda8026_i2c_wbyte(val);
	second_tda8026_i2c_stop();

	return 0;
}

int second_tda8026_enable(int enable)
{
	unsigned char slot;

	if (enable) {
		sci_gpio_set_pin_val(SECOND_TDA8026_SDWN_PD, 1);
		DelayMs(25); //wait for TDA8026 power-up sequence
		second_tda8026_i2c_init();
		DelayMs(2);

		/* clearing all slots status */
		for (slot = 1; slot < 6; slot++) {
			second_tda8026_i2c_write(0x48, slot);
			second_tda8026_i2c_read(0x40);
			second_tda8026_i2c_write(0x40, 0);
		}

		/* set slew rate */
		second_tda8026_i2c_write(0x48, 0x06);
	}
	else {
		sci_gpio_set_pin_val(SECOND_TDA8026_SDWN_PD, 0);
		DelayMs(8);
	}

	return 0;
}

static int second_tda8026_probe()
{
	unsigned char regval;

	second_tda8026_i2c_write(0x48, 0x00);
	regval = second_tda8026_i2c_read(0x40);	/* read product version */
	return (0xc2 == regval) ? 0 : -1;
}


int second_tda8026_init(void)
{
	int result;

	sci_gpio_set_pin_type(SECOND_TDA8026_SDWN_PD,GPIO_OUTPUT);
	sci_gpio_set_pin_val(SECOND_TDA8026_SDWN_PD,0);
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SCL, GPIO_OUTPUT);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SCL, 1);
	sci_gpio_set_pin_type(SECOND_TDA8026_I2C_SDA, GPIO_OUTPUT);
	sci_gpio_set_pin_val(SECOND_TDA8026_I2C_SDA, 1);

	
	second_tda8026_enable(1);
	second_tda8026_probe();


	memset(second_tda8026_last_regv, '\0', sizeof(second_tda8026_last_regv));
	second_tda8026_last_slot = 2;

	return 0;
}

/*slot 从1开始编码*/
static int second_tda8026_deactivate(int slot)
{
	second_tda8026_last_regv[slot - 1] = 0;

	/* start auto-deactivation procedure */
	second_tda8026_i2c_write(0x48, slot);
	second_tda8026_i2c_write(0x40, 0x00);

	return 0;
}


/*devch start from 1*/
int second_TDA8026_vcc(unsigned char slot, unsigned int mode)
{

	int tda8026_ch = slot;
	int i;
	unsigned char regval = 0;


	second_tda8026_last_regv[second_tda8026_last_slot - 1] &= ~0x40;
	second_tda8026_i2c_write(0x48, second_tda8026_last_slot);
	second_tda8026_i2c_write(0x40, second_tda8026_last_regv[second_tda8026_last_slot - 1]);

	/*clear second TDA8026 irq*/
    second_tda8026_i2c_read(0x40);
	
	second_tda8026_i2c_write(0x48, tda8026_ch);
	second_tda8026_i2c_write(0x40, 0x00);
	/*RSTIN is set to 0, use synchronous mode*/
	second_tda8026_i2c_write(0x42, 0x00);
	//DelayMs(11);
	
	//second_tda8026_i2c_write(0x48, tda8026_ch);

	switch (mode) {
	case 1800:
		regval = (1 << 7) |(1 << 6) | (1 << 0);
		break;

	case 3000:
		regval = (0 << 2) |(1 << 6) | (1 << 0);
		break;

	default:
		regval = (1 << 2) |(1 << 6) | (1 << 0);
		break;
	}
	second_tda8026_i2c_write(0x40, regval);

	second_tda8026_last_slot = tda8026_ch;
	second_tda8026_last_regv[second_tda8026_last_slot - 1] = regval;

	//DelayUs(100);
	
	return 0;
}


/*devch start from 1*/
void second_TDA8026_REST(unsigned char devch, unsigned char level)
{
	int tda8026_ch = devch;
	unsigned char regval;

	second_tda8026_i2c_write(0x48, tda8026_ch);
	regval = second_tda8026_i2c_read(0x42);

	if (level) {
		second_tda8026_i2c_write(0x42, regval | (1 << 6));
	}
	else {
		second_tda8026_i2c_write(0x42, regval & ~(1 << 6));
	}

	return;
}


/*slot start from 1*/
int second_tda8026_switch_slot(int slot)
{
	if (slot > 1 &&  second_tda8026_last_slot != slot) 
	{
		second_tda8026_last_regv[second_tda8026_last_slot - 1] &= ~0x40;
		second_tda8026_i2c_write(0x48, second_tda8026_last_slot);
		
		/*disable current slot I/OEN */
		second_tda8026_i2c_write(0x40, second_tda8026_last_regv[second_tda8026_last_slot - 1]);

		second_tda8026_last_regv[slot - 1] |= 0x40;
		second_tda8026_i2c_write(0x48, slot);
		
		/*enable new slot I/OEN */
		second_tda8026_i2c_write(0x40, second_tda8026_last_regv[slot - 1]);

		second_tda8026_last_slot = slot;
	}

	return 0;
}

/*add end*/



/**
 * ===========================================================================
 * enable TDA8026 chip
 *
 * parameters:
 *	enable : switcher
 * retval:
 *	0, successful
 *	others, unsuccessful
 */
int tda8026_enable(int enable)
{
	unsigned char slot;

	if (enable) {
		sci_gpio_set_pin_val(TDA8026_SDWN_PD, 1);
		DelayMs(25); //wait for TDA8026 power-up sequence
		tda8026_i2c_init();
		DelayMs(2);

		/* clearing all slots status */
		for (slot = 1; slot < 6; slot++) {
			tda8026_i2c_write(0x48, slot);
			tda8026_i2c_read(0x40);
			tda8026_i2c_write(0x40, 0);
		}

		/* set slew rate */
		tda8026_i2c_write(0x48, 0x06);
		tda8026_i2c_write(0x48, 0x06);
	}
	else {
		sci_gpio_set_pin_val(TDA8026_SDWN_PD, 0);
		DelayMs(8);
	}

	return 0;
}

/**
 * clearing all slots interrupts and deactivating all slots
 *
 * parameters:
 *	no parameters
 * retval:
 *	0, successful
 *	others, unsuccessful
 */
static int tda8026_probe()
{
	unsigned char regval;

	tda8026_i2c_write(0x48, 0x00);
	regval = tda8026_i2c_read(0x40);	/* read product version */
	return (0xc2 == regval) ? 0 : -1;
}

/**
 * ===========================================================================
 * initialzing TDA8026 interface chip
 *
 * parameters:
 *	no parameters
 * retval:
 *	0, successful
 *	others, unsuccessful
 */
int tda8026_init(void)
{
	int result;

	s_SetSoftInterrupt(IRQ_SMARTCARD0, tda8026_tasklet_proc);
	sci_gpio_set_pin_type(TDA8026_SDWN_PD,GPIO_OUTPUT);
	sci_gpio_set_pin_val(TDA8026_SDWN_PD,0);
	sci_gpio_set_pin_type(TDA8026_I2C_SCL,GPIO_FUNC1);
	sci_gpio_set_pin_type(TDA8026_I2C_SDA,GPIO_FUNC1);
	
	tda8026_enable(1);
	tda8026_probe();

	memset(last_regv, '\0', 5);
	last_slot = 2;

	//printk("card %02x\r\n", tda8026_irq_clear(1));

	return 0;
}

/**
 * read the interrupt slot number
 *
 * parameters:
 *	no parameters
 * retval:
 *	0, successful
 *	others, unsuccessful
 */
int tda8026_irq_source()
{
	tda8026_i2c_write(0x48, 0x00);

	return tda8026_i2c_read(0x42);
}

/**
 * clearing assigned slot interrupts
 * parameters:
 *	ch : slot number
 * retval:
 *	interrupt information
 */
int tda8026_irq_clear(int slot)
{
	unsigned char status;

	tda8026_i2c_write(0x48, slot);
	status = tda8026_i2c_read(0x40);
	last_regv[slot - 1] = 0;

	return status;

}

/**
 * convert slot number in tda8026
 *
 * parameters:
 *	slot : [in] slot number
 * retval:
 *	0, successful
 *	others, unsuccessful
 */
int tda8026_switch_slot(int slot)
{
	disable_sci_irq();

	if (slot > 1 && last_slot != slot) {
		last_regv[last_slot - 1] &= ~0x40;
		tda8026_i2c_write(0x48, last_slot);
		tda8026_i2c_write(0x40, last_regv[last_slot - 1]);
		last_regv[slot - 1] |= 0x40;
		tda8026_i2c_write(0x48, slot);
		tda8026_i2c_write(0x40, last_regv[slot - 1]);
		last_slot = slot;
	}

	enable_sci_irq();

	return 0;
}

static void tda8026_reset(int slot,int level)
{
	tda8026_i2c_write(0x48, 1);
	if (level) {
		tda8026_i2c_write(0x42, (1 << 6));
	}
	else {
		tda8026_i2c_write(0x42, 0x00);
	}
}


extern volatile struct emv_queue sci_device[2]; 
extern volatile unsigned int fifo0_time_status, fifo1_time_status;
volatile int tda8026_tasklet_enable=0;
volatile int tda8026_i2c_mutex=0;
extern struct emv_core gl_emv_devs[];

static int tda8026_tirq_proc()
{
	unsigned char intsrc;
	unsigned char regval;
	unsigned char intr_state=0;

	/* tda8026 interrupts source, per slot one bit */
	/*
	 * when in this handler,some other tda8026 interrupts may happen.
	 * then TDA8026's INT pin will remain low and disable tda8026 intr.
	 * make sure that TDA8026' INT pin is in high level.
	 */
	while (1) {
		intsrc = tda8026_irq_source();
		if (intsrc == 0)	/* TDA8026's INT pin is high */
			break;

		if (intsrc & 1) {	/* slot1 */
			regval = tda8026_irq_clear(1);

			//if (regval & 0x02) {	/* card extracted or inserted */
				if (regval & 0x01) {
					sci_device[0].dev_sta = CARD_IN_SOCKET;
				}
				else {
					sci_device[0].dev_sta = CARD_OUT_SOCKET;
				}
			//}
			if (regval & 0x02) {	/* card extracted or inserted */
				gl_emv_devs[0].terminal_state = EMV_IDLE;
				if (sci_device[0].dev_sta == CARD_IN_SOCKET) {
					ScrSetIcon(ICON_ICCARD, ENABLE);
					insert_card_action();
				}
				else {
					ScrSetIcon(ICON_ICCARD, DISABLE);
				}
			}
			intr_state |= (1<<0);
		}

		if (intsrc & 2) {	/* slot2 */
			regval = tda8026_irq_clear(2);
			intr_state |= (1<<2);
		}

		if (intsrc & 4) {	/* slot3 */
			regval = tda8026_irq_clear(3);
			intr_state |= (1<<3);
		}

		if (intsrc & 8) {	/* slot4 */
			regval = tda8026_irq_clear(4);
			intr_state |= (1<<4);
		}

		if (intsrc & 0x10) {	/* slot5 */
			regval = tda8026_irq_clear(5);
			intr_state |= (1<<5);
		}

	}

	return intr_state;
}

static void tda8026_tasklet_proc(void)
{
	int ret, flag;

	if(tda8026_tasklet_enable == 0) return ;
	if(tda8026_i2c_mutex != 0) return ;

	tda8026_tasklet_enable = 0;

	ret = tda8026_tirq_proc();
	if(ret & 0x01) fifo0_time_status = TIME_OUT;

	/*  2015-3-10 xuwt:
	  * For compatible with un-standard sam card, sam-card TS timeout process by bam5892-controller now   */
	//if(ret & 0xFE) fifo1_time_status = TIME_OUT;
}

static void enable_sci_irq()
{
	tda8026_i2c_mutex = 0;
	enable_irq(IRQ_SMARTCARD0);
}

static void disable_sci_irq()
{
	disable_irq(IRQ_SMARTCARD0);
	tda8026_i2c_mutex = 1;
}

/**
 * ===========================================================================
 * start tda8026 auto-activation sequence
 *
 * parameters:
 *	slot         : [in] slot number
 *	voltage      : [in] output voltage (by mV)
 *	reset_timing : [in] reset maintain LOW level time
 * retval:
 *	0, successful
 *	others, unsuccessful
 */
static int tda8026_cold_reset(int slot, int voltage, int reset_timing)
{
	unsigned char regval;
	unsigned char status;

	disable_sci_irq();
	
	if (slot > 1) {
		last_regv[last_slot - 1] &= ~0x40;
		tda8026_i2c_write(0x48, last_slot);
		tda8026_i2c_write(0x40, last_regv[last_slot - 1]);
		last_slot = slot;
	}

	/* deactivation and set asynchronous mode */
	tda8026_i2c_write(0x48, slot); 	/* select slot */
	tda8026_i2c_write(0x40, 0x00); 	/* set reg[1:0] */
	tda8026_i2c_write(0x42, (1 << 6));
	
	/* the start of ATR must be not less than 200 + 0xAA */
	tda8026_i2c_write(0x40, (1 << 4));
	tda8026_i2c_write(0x42, 0xAA);

	/* set 'reset' signal timing */
	tda8026_i2c_write(0x40, (3 << 4)); 
	tda8026_i2c_write(0x42, (reset_timing & 0xFF));

	tda8026_i2c_write(0x40, (2 << 4));
	tda8026_i2c_write(0x42, (reset_timing & 0xFF00) >> 8);

	/* set voltage, and output power in synchronous mode */
	switch (voltage) {
	case 5000:
		regval = 0x44;
		break;
	case 3000:
		regval = 0x40;
		break;
	case 1800:
		regval = 0xC0;
		break;
	default:
		enable_sci_irq();
		return -1;
	}

	/* start auto-activation procedure */
	tda8026_i2c_write(0x40, regval | 1);
	last_regv[slot - 1] = regval | 1;

	/* For compatible with un-standard sam card, sam-card TS timeout process by bam5892-controller now   */
	if(slot > 1)
	{
		writel_or(SCI_TIMER_EN,SC1_TIMER_CMD_REG);
	}

	enable_sci_irq();

	return 0;
}

/**
 * start tda8026 warm reset sequence
 *
 * parameters:
 *	slot         : [in] slot number
 *	reset_timing : [in] reset maintain LOW level time
 * retval:
 *	0, successful
 *	others, unsuccessful
 */
static int tda8026_warm_reset(int slot, int reset_timing)
{
	unsigned char regval;

	disable_sci_irq();

	if (slot > 1) {
		last_regv[last_slot - 1] &= ~0x40;
		tda8026_i2c_write(0x48, last_slot); /* disable last IO when switching channel */
		tda8026_i2c_write(0x40, last_regv[last_slot - 1]);
		last_slot = slot;
	}

	tda8026_i2c_write(0x48, slot);
	regval = last_regv[slot - 1] | 0x40;
	last_regv[slot - 1] |= 0x40;

	/* the start of ATR must be not less than 200 + 0xAA */
	tda8026_i2c_write(0x40, regval | (1 << 4));
	tda8026_i2c_write(0x42, 0xAA);

	/* set reset signal timing */
	tda8026_i2c_write(0x40, regval | (2 << 4));
	tda8026_i2c_write(0x42, (reset_timing & 0xFF00) >> 8);
	tda8026_i2c_write(0x40, regval | (3 << 4));
	tda8026_i2c_write(0x42, (reset_timing & 0xFF));

	/* start warm reset procedure */
	tda8026_i2c_write(0x40, regval | 2);

	/* 2015-3-10 xuwt:
	  * For compatible with un-standard sam card, sam-card TS timeout process by bam5892-controller now   */
	if(slot > 1)
	{
		writel_or(SCI_TIMER_EN,SC1_TIMER_CMD_REG);
	}

	enable_sci_irq();

	return 0;
}

/**
 * start tda8026 auto-deactivation sequence
 *
 * parameters:
 *	slot : [in] slot number
 * retval:
 *	0, successful
 *	others, unsuccessful
 */
static int tda8026_deactivate(int slot)
{
	disable_sci_irq();

	last_regv[slot - 1] = 0;

	/* start auto-deactivation procedure */
	tda8026_i2c_write(0x48, slot);
	tda8026_i2c_write(0x40, 0x00);

	enable_sci_irq();

	return 0;
}

/* syncronous card interface by tda8026 */

int Mc_Vcc_TDA8026(unsigned char devch, unsigned int mode)
{
	/* 2015-3-10 xuwt:
	  * 修正同步卡读卡错误问题 */

	int tda8026_ch = devch + 1;
	int i;

	if (devch != USER_CH)
		return 0;

	disable_sci_irq();

	tda8026_i2c_write(0x48, tda8026_ch);
	tda8026_i2c_write(0x40, 0x00);
	tda8026_i2c_write(0x42, 0x00);

	enable_sci_irq();

	DelayMs(11);

	if(mode == 0) return 0;

	disable_sci_irq();

	tda8026_i2c_write(0x48, tda8026_ch);

	switch (mode) {
	case 1800:
		tda8026_i2c_write(0x40, (1 << 7) |(1 << 6) | (1 << 0));
		break;

	case 3000:
		tda8026_i2c_write(0x40, (0 << 2) |(1 << 6) | (1 << 0));
		break;

	default:
		tda8026_i2c_write(0x40, (1 << 2) |(1 << 6) | (1 << 0));
		break;
	}

	enable_sci_irq();

	/* 2015-3-10 xuwt:
	  *If CLKDIV[1:0] = 00, the first four clock cycles are not transferred to CLK(n). When CLKDIV[1:0] is not 00,
            * the first five clock cycles are not transferred to CLK(n). */
	DelayUs(100);
	for(i = 0; i < 4; i++)
	{
		Mc_Clk(USER_CH, 1);
		DelayUs(10);
		Mc_Clk(USER_CH, 0);
		DelayUs(10);
	}

	return 0;
}


void Mc_Reset_TDA8026(unsigned char devch, unsigned char level)
{
	int tda8026_ch = devch + 1;
	unsigned char regval;

	if (devch != USER_CH)
		return;

	disable_sci_irq();

	tda8026_i2c_write(0x48, tda8026_ch);
	regval = tda8026_i2c_read(0x42);

	if (level) {
		tda8026_i2c_write(0x42, regval | (1 << 6));
	}
	else {
		tda8026_i2c_write(0x42, regval & ~(1 << 6));
	}

	enable_sci_irq();

	return;
}


void Mc_C4_Write_TDA8026(unsigned char devch, unsigned char level)
{
	int tda8026_ch = devch + 1;
	unsigned char regval;

	if (devch != USER_CH)
		return;

	disable_sci_irq();

	tda8026_i2c_write(0x48, tda8026_ch);
	regval = tda8026_i2c_read(0x42);

	if (level) {
		tda8026_i2c_write(0x42, regval | (1 << 4));
	}
	else {
		tda8026_i2c_write(0x42, regval & ~(1 << 4));
	}

	enable_sci_irq();

	return;
}


void Mc_C8_Write_TDA8026(unsigned char devch, unsigned char level)
{
	unsigned char regval;
	int tda8026_ch = devch + 1;
	
	if (devch != USER_CH)
		return;

	disable_sci_irq();

	tda8026_i2c_write(0x48, tda8026_ch);
	regval = tda8026_i2c_read(0x42);

	if (level) {
		tda8026_i2c_write(0x42, regval | (1 << 5));
	}
	else {
		tda8026_i2c_write(0x42, regval & ~(1 << 5));
	}

	enable_sci_irq();

	return;
}


/* asyncronous card interface by tda8026 */


int sci_card_xmit_setting_tda8026(int devch)
{
	int tda8026_ch = devch + 1;
	devch_backup = devch;

	if(USER_CH == devch)
		return 0;
	/*add by wanls 20160810*/
    /*if sam slot in sencond tda8026*/
	if((sci_device_resource.sam_slot_num > 4) && (tda8026_ch >= TDA8026_CHANNEL_SIZE))
	{
	    second_tda8026_switch_slot(tda8026_ch - TDA8026_CHANNEL_SIZE + SAM_START_CHANNEL);
		tda8026_ch = LAST_TDA8026_CHANNEL;
	}
	/*add end*/

	tda8026_switch_slot(tda8026_ch);
	return 0;
}

/* only offer RST pin control, this interface is used in intr */
void sci_card_pin_control_tda8026(unsigned int pin, unsigned int val)
{
    /*change by wanls 20160810*/
    int tda8026_ch = devch_backup + 1;
	/* tda8026 use asyncronous mode, so  rst pin control is auto controlled by tda8026 */
	/* if we want current sam slot info on reset, record it in cold_reset and warm_reset function */

	if((sci_device_resource.sam_slot_num > 4) && (tda8026_ch >= TDA8026_CHANNEL_SIZE))
	{
	    second_TDA8026_REST(tda8026_ch - TDA8026_CHANNEL_SIZE + SAM_START_CHANNEL, val);
	}
	/*add end*/
	return;
}

/*
第二个TDA8026配置成同步模式，第一个TDA8026配置成异步模式
如果sam卡在第二个TDA8026上，则先给第二个TDA8026启动上电，在给第一个TDA8026的第五通道启动
冷复位。如果第一个TDA8026的第五通道已经启动过冷复位，则不能再次启动冷复位，否则会断了clk，
影响第二个TDA8026上的其他已经上电的sam卡
*/
int sci_card_cold_reset_tda8026(int devch, int voltage, int reset_timing)
{
	int tda8026_ch = devch + 1;
	int dev_num = 0;
	int FirstTda8026RestTag = 1;
	devch_backup = devch;
	/* using tda8026's asyncronous reset mode, so enable IO_EN before reset */
	if (devch == USER_CH) {
		writel_or(SCI_IO_EN, SC0_UART_CMD1_REG);
	}
	else {
		writel_or(SCI_IO_EN, SC1_UART_CMD1_REG);
	}

    /*add by wanls 20160810*/
	if((sci_device_resource.sam_slot_num > 4) && (tda8026_ch >= TDA8026_CHANNEL_SIZE))
    {
        /*Open the second tda8026, offer power to sam5-sam7*/

        second_TDA8026_vcc(tda8026_ch - TDA8026_CHANNEL_SIZE + SAM_START_CHANNEL, voltage);
		tda8026_ch = LAST_TDA8026_CHANNEL;


		for (dev_num = TDA8026_CHANNEL_SIZE - 1; dev_num < EMV_TERMINAL_NUM; dev_num++) 
		{
		    if (EMV_READY == gl_emv_devs[dev_num].terminal_state)
				break;
		}
		if (dev_num < EMV_TERMINAL_NUM) 
		{
		   FirstTda8026RestTag = 0;
		   tda8026_switch_slot(TDA8026_CHANNEL_SIZE);
		   writel_or(SCI_TIMER_EN,SC1_TIMER_CMD_REG);
		}	
	}
	
	if(FirstTda8026RestTag)
	{
		return tda8026_cold_reset(tda8026_ch, voltage, reset_timing);
	}

	return 0;
	/*add end*/
}

int sci_card_warm_reset_tda8026(int devch, int reset_timing)
{
	int tda8026_ch = devch + 1;
	devch_backup = devch;
	/* using tda8026's asyncronous reset mode, so enable IO_EN before reset */
	if (devch == USER_CH) {
		writel_or(SCI_IO_EN, SC0_UART_CMD1_REG);
	}
	else {
		writel_or(SCI_IO_EN, SC1_UART_CMD1_REG);
	}
	/*change by wanls 20160810*/
	if((sci_device_resource.sam_slot_num > 4) && (tda8026_ch >= TDA8026_CHANNEL_SIZE))
    {
        disable_sci_irq();
        /* set rest pin to low level */
        second_TDA8026_REST(tda8026_ch - TDA8026_CHANNEL_SIZE + SAM_START_CHANNEL, 0);
        tda8026_ch = LAST_TDA8026_CHANNEL;
		writel_or(SCI_TIMER_EN,SC1_TIMER_CMD_REG);
		enable_sci_irq();
		
		return 0;
	}
	else
	{
	    return tda8026_warm_reset(tda8026_ch, reset_timing);
	}
	/*change end*/
}

int sci_card_powerdown_tda8026(int devch)
{
	int dev_num;
	int tda8026_ch = devch + 1;

    /*change by wanls 20160810*/
    if((sci_device_resource.sam_slot_num > 4) && (tda8026_ch >= TDA8026_CHANNEL_SIZE))
    {
        second_tda8026_deactivate(tda8026_ch - TDA8026_CHANNEL_SIZE + SAM_START_CHANNEL);

		for (dev_num = TDA8026_CHANNEL_SIZE - 1; dev_num < EMV_TERMINAL_NUM; dev_num++) {
			if ((EMV_IDLE != gl_emv_devs[dev_num].terminal_state) && (dev_num != (devch)))
				break;
		}
		if (dev_num == EMV_TERMINAL_NUM) 
		{
		    tda8026_deactivate(LAST_TDA8026_CHANNEL);  
		}
    }
	else
	{
	    tda8026_deactivate(tda8026_ch);
	}
	/*add end*/

	if (devch == USER_CH) {
		/* stop SC_CLK */
		writel_and(~SCI_CLK_EN, SC0_CLK_CMD_REG);
	}
	else {
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


/* global interface on TDA8026 */

void sci_usercard_detect_tda8026(void)
{
	int dev_sta;

	disable_sci_irq();
	dev_sta = (tda8026_irq_clear(1) & 0x01) ?	CARD_IN_SOCKET : CARD_OUT_SOCKET;

	if (dev_sta != sci_device[0].dev_sta) {
		if (dev_sta == CARD_IN_SOCKET) {
			ScrSetIcon(ICON_ICCARD, ENABLE);
		}
		else {
			ScrSetIcon(ICON_ICCARD, DISABLE);
		}
		sci_device[0].dev_sta = dev_sta;
	}
	enable_sci_irq();
}


void controller_intr_handler_tda8026(unsigned long data)
{
	/* 采用下半部机制， 实际的处理函数为tda8026_tasklet_proc */
	tda8026_tasklet_enable = 1;
}

int controller_enable_tda8026(int enable)
{
    /*change by wanls 20160810*/
    int iRet = 0;
	iRet = tda8026_enable(enable);
	if(iRet)return iRet;
	if(sci_device_resource.sam_slot_num > 4)
	{
	   iRet = second_tda8026_enable(enable);
	}
	return iRet;
	/*add end*/
}

int controller_init_tda8026(void)
{
    /*change by wanls 20160810*/
    int iRet = 0;
	iRet = tda8026_init();
	if(iRet)return iRet;
	if(sci_device_resource.sam_slot_num > 4) 
	{
	    iRet = second_tda8026_init();
	}
	return iRet;
	/*add end*/
}






