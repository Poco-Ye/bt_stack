#include "base.h"
#include "platform.h"
#include "ts_sample.h"
#include "ts_hal.h"
#include "tsc2046.h"
#include "../lcd/lcdapi.h"


T_TsDrvConfig S300_Ts_Config = {
	.clk_port = GPIOA,
	.miso_port = GPIOA,
	.mosi_port = GPIOA,
	.cs_port = GPIOA,
	.irq_port = GPIOB,
	.busy_port = GPIOB,
	
	.clk_pin = 4,
	.miso_pin = 5,
	.mosi_pin = 6,
	.cs_pin = 2,
	.irq_pin = 6,
	.busy_pin = 8,
};

T_TsDrvConfig S500_Ts_Config = {
	.clk_port = GPIOA,
	.miso_port = GPIOA,
	.mosi_port = GPIOA,
	.cs_port = GPIOB,
	.irq_port = GPIOB,
	.busy_port = GPIOB,
	
	.clk_pin = 4,
	.miso_pin = 5,
	.mosi_pin = 6,
	.cs_pin = 8,
	.irq_pin = 6,
	.busy_pin = 7,
};

T_TsDrvConfig *Sxxx_Ts_Config = NULL;

void ts_irq_handler();
void ts_init_timer(int period_us);
void ts_clear_buf();

char SPI0Send(uchar in)
{
    char val;

    spi_tx(0,(uchar*)&in,1,8);
    while(spi_rx(0, (uchar*)&val,1,8)==0);

    return val;
}

void SPI0CS(int a)
{
    while(spi_txcheck(0));
    if(a != 0) gpio_set_pin_val(TS2046_nCS, 1);
    else gpio_set_pin_val(TS2046_nCS, 0);
}

int ts_busy()
{
    return gpio_get_pin_val(TS2046_nPENIRQ);
}

ushort ts_cmd(uchar cmd)
{
	uchar hi, lo;
	
	SPI0CS(0);
	SPI0Send((1<<TS_CTRL_BIT_START) | (cmd<<TS_CTRL_BIT_ADDR));

	while(ts_busy());

	hi = SPI0Send(0); /* write zero only for reading data */
	lo = SPI0Send(0); /* write zero only for reading data */
	SPI0CS(1);

	return ((hi << 8) | lo) >> 4; /* 8 bits one time, read twice for combining 12 bits*/
}

void ts_clear_irq()
{
	writel(BIT6,GIO1_R_GRPF1_INT_CLR_MEMADDR);//清中断标志
}

void ts_enable_irq()
{
	ts_clear_irq();
	gpio_set_pin_interrupt(TS2046_nPENIRQ, 1);
}

void ts_disable_irq()
{
	gpio_set_pin_interrupt(TS2046_nPENIRQ, 0);
	
}

/*----------------- ts timer definition start --------------------- */

extern QUEUE_SAMPLE_T gtSysRawSample;
extern TS_DEV_T gtTsDev;
struct platform_timer *ts_timer = NULL; /* touch screen */  

void ts_timer_stop()
{
	platform_stop_timer(ts_timer);
}

void ts_timer_start()
{
	platform_start_timer(ts_timer);
}

void ts_irq_timer()
{
	TS_SAMPLE_T s;
	int ret;
	static int counter=0;

	ret = read_raw(&s);
	if(ret == 0) return ;

	DelayUs(80); /* sequence needed for nPENDIRQ checking */

	if(gpio_get_pin_val(TS2046_nPENIRQ))
	{
		if(s.pressure == 0)  /* pen may be still on the screen */
		{
			counter++;
			if(counter >= 3)
			{
				counter = 0;
				ts_timer_stop();
				ts_enable_irq();

				if(!gpio_get_pin_val(TS2046_nPENIRQ))
				{
					ts_disable_irq();
					ts_timer_start();
					return ;
				}

				memset(&s, 0, sizeof(TS_SAMPLE_T));
				ts_put(&s, &gtSysRawSample);
			}
		}
	}
	else
	{
		if(counter)counter = 0;
		ts_put(&s, &gtSysRawSample);
	}
}

static void ts_timer_proc()
{
	platform_timer_IntClear(ts_timer);
	ts_irq_timer();
}

void ts_init_timer(int period_us)
{
	struct platform_timer_param param;
	
	param.handler = ts_timer_proc;
	param.id = TIMER_TS;
	param.timeout = period_us;
	param.mode = TIMER_PERIODIC;
    ts_timer = platform_get_timer(&param);
    platform_stop_timer(ts_timer);
}

void ts_samp_rate_change(void)
{
	static int lastSampRateModel=-1;

	/* change sample rate for nice signature */
	if(gtTsDev.tAttr.sampleRateModel != lastSampRateModel)
	{
		if(gtTsDev.tAttr.sampleRateModel == SIGN_SP_SPEED) 
			ts_init_timer(400);
		else 
			ts_init_timer(1000);
		lastSampRateModel = gtTsDev.tAttr.sampleRateModel;
	}
}

void ts_irq_handler()
{
	ts_samp_rate_change();
	ts_clear_irq();
	ts_clear_buf();
	ts_disable_irq();
	ts_timer_start();
}

void s_ts_init()
{
    int flag;
    if (GetTsType() <= 0) return;
	if (GetTsType() == TS_TM035KBH08)
	{
		Sxxx_Ts_Config = &S300_Ts_Config;
	}
	else
	{
		Sxxx_Ts_Config = &S500_Ts_Config;
	}
	
	/* SPI PIN configuration */
    gpio_set_pin_type(TS2046_CLK,  GPIO_FUNC0); 
    gpio_set_pin_type(TS2046_MISO, GPIO_FUNC0);
    gpio_set_pin_type(TS2046_MOSI, GPIO_FUNC0);
    gpio_set_pin_type(TS2046_nCS,  GPIO_OUTPUT);
    gpio_set_pin_val(TS2046_nCS,  1);
	/* spi mode config */
	spi_config(0, 150*1000, 8, 0);
	
	/* tsc2046 config */
	gpio_set_pin_type(TS2046_nPENIRQ,	GPIO_INPUT);
	gpio_set_pin_type(TS2046_BUSY, 		GPIO_INPUT);
	gpio_disable_pull(TS2046_nPENIRQ);
	gpio_disable_pull(TS2046_BUSY);

    irq_save(flag);
    s_setShareIRQHandler(TS2046_nPENIRQ, INTTYPE_LOWLEVEL, ts_irq_handler);
    gpio_set_pin_interrupt(TS2046_nPENIRQ, 0);//disable ts irq;
    writel(BIT6,GIO1_R_GRPF1_INT_CLR_MEMADDR);//清中断标志
    irq_restore(flag);
    
    TsCalibrationParamInit();

	/* init touchscreen dev */
	TsDevInit();
}

/*  for spi share */
void ts_spi_init(void)
{
    gpio_set_pin_type(TS2046_CLK, GPIO_FUNC0);
    gpio_set_pin_type(TS2046_MISO, GPIO_FUNC0);
    gpio_set_pin_type(TS2046_MOSI, GPIO_FUNC0);
    spi_config(0, 150*1000, 8, 0);
}

void  disable_ts_spi()
{
    int flag;
    if(gtTsDev.lock != 1 && gtTsDev.isSKeySupported != 1) return;

    SPI0CS(1);
    irq_save(flag);
    writel(BIT6,GIO1_R_GRPF1_INT_CLR_MEMADDR);//清中断标志
    gpio_set_pin_interrupt(TS2046_nPENIRQ, 0);//disable ts irq;
    platform_stop_timer(ts_timer);//ts timer stop;
    irq_restore(flag);
}

void enable_ts_spi()
{
	if(gtTsDev.lock != 1 && gtTsDev.isSKeySupported != 1) return;
	ts_spi_init();
    ts_enable_irq();
	ts_timer_start();
}

/* file end tsc2046.c */
