#include "bcm5892_i2s.h"
#include "base.h"
#include "audio.h"
#include "queue.h"

volatile unsigned char IsAudioEnabled;
extern volatile unsigned char IsBeepEnabled;
extern QUEUE_T gtWaveQue;
extern WAVE_ATTR_T gtWaveAttr;
extern uint guiAudioVolumeValue;

void interrupt_i2s(void)
{
	QUEUE_T *ptAudioQue =&gtWaveQue;
	int audioData, regval, vol=guiAudioVolumeValue;
	int ret, flag, iTmp;
	uchar ucTmp;

	/* clear tx intr */
	writel_or((1<<I2S_DATXCTRL_TxIntStat_SHIFT), I2S_DATXCTRL_REG);

	do {
		if(!QueueGetc(ptAudioQue, &ucTmp)) goto AUDIO_EXIT;
		audioData = ucTmp;

		if(gtWaveAttr.usBitsPerSample == 16)
		{
			if(!QueueGetc(ptAudioQue, &ucTmp)) goto AUDIO_EXIT;
			audioData |= (ucTmp<<8);
		}

		ret = i2s_data_conv(audioData, &iTmp, gtWaveAttr.usBitsPerSample/8, vol);
		if(ret) goto AUDIO_EXIT;
		
		writel(iTmp & 0xffff, I2S_DAFIFO_DATA_REG);
		regval = readl(I2S_DADEBUG_REG);
		regval = regval & 0xff; 
	}while(regval <= I2S_MAX_FIFO_SIZE);

	return ;

AUDIO_EXIT:
	writel_and(~(1<<I2S_DATXCTRL_TxIntEn_SHIFT), I2S_DATXCTRL_REG); /* disable intr */
	writel_and(~(1<<I2S_DAI2S_TXEN_SHIFT), I2S_DAI2S_REG); /* i2s transmit Interface disable */
	writel_or((0x1<<16),	DMU_R_dmu_clkout_sel_MEMADDR); /* clkout1 disable */
	disable_irq(IRQ_OI2S);

	if(!IsBeepEnabled) gpio_set_pin_val(GPIOE, 8, 0); /* disable amplifier */
	IsAudioEnabled = 0;

	return ;
}

static int get_proper_sample_val(uint samp_rate, uint uiSampleArray[], uint iArrayLen)
{
	int cmp_val, prop_val, idx=0;
	int i;
	
	cmp_val = samp_rate - uiSampleArray[0];
	cmp_val = (cmp_val>0) ? cmp_val : -cmp_val;

	for(i=0; i<iArrayLen; i++)
	{
		prop_val = samp_rate - uiSampleArray[i];
		prop_val = (prop_val > 0) ? prop_val : -prop_val;

		if(prop_val < cmp_val) 
		{	
			cmp_val = prop_val;
			idx = i;
		}
	}

	return idx;
}

static int bcm5892_i2s_set_sampling_rate(int nSamplingRate)
{
	uint value, ndiv_int, ndiv_frac, m2, i2s_txsr, i;   
	uint p1=1, p2=1;
	uint iArrayIdx;
	int flag;

	uint uiSampleRateArray[] = { 8000,	11025,	12000,	16000,  22050, 
								 24000,	32000,  44100,  48000       };

	iArrayIdx = get_proper_sample_val(nSamplingRate, uiSampleRateArray, sizeof(uiSampleRateArray)/sizeof(uiSampleRateArray[0]));
	switch(iArrayIdx)
	{
		case 0: /* 8000 */
		   ndiv_int = 32;
		   ndiv_frac = 319;
		   m2 = 125;
		   p1 = 1;
		   p2 = 3;
		   i2s_txsr = 0;              
		   break;

		case 1: /* 11025 */
		   ndiv_int = 6;            
		   ndiv_frac = 100; 
		   m2 = 51;
		   i2s_txsr = 2; 
		   break;

		case 2: /* 12000 */
		   ndiv_int = 16;            
		   ndiv_frac = 1279;
		   m2 = 125;
		   i2s_txsr = 3;
		   break;

		case 3: /* 16000 */
		   ndiv_int = 71;
		   ndiv_frac = 639;
		   m2 = 208;
		   i2s_txsr = 4;
		   break;

		case 4: /* 22050 */
		   ndiv_int = 23;            
		   ndiv_frac = 19; 
		   m2 = 98;
		   p1 = 1;
		   p2 = 1;
		   i2s_txsr = 5;
		   break;

		case 5: /* 24000 */
		   ndiv_int = 64;            
		   ndiv_frac = 639;
		   m2 = 250;
		   i2s_txsr = 6;
		   break;

		case 6: /* 32000 */
		   ndiv_int = 57;
		   ndiv_frac = 319;
		   m2 = 167;
		   i2s_txsr = 7;
		   break;

		case 7: /* 44100 */
		   ndiv_int = 23;            
		   ndiv_frac = 19; 
		   m2 = 49;
		   i2s_txsr = 8;
		   break;

		case 8: /* 48000 */
		   	ndiv_int = 64;            
			ndiv_frac = 1279;
			m2 = 125;
			i2s_txsr = 9;
		   break;

		default:
		   return -1;
	}

	/* clkout1 disable */
	writel_or((0x1<<16),	DMU_R_dmu_clkout_sel_MEMADDR);

	/* dmu pll1 power off*/
	writel_or(0x10000000, DMU_PLL1_FRAC_PARAM);
	writel_or(0x02000000, DMU_PLL1_CLK_PARAM);
	writel_or(0x00000100, DMU_PLL1_CH1_PARAM);
	writel_or(0x00000100, DMU_PLL1_CH2_PARAM);

	/* PLL digital reset */
	writel_or((1<<dmu_pll1_dreset_SHIFT), 	DMU_PLL1_CLK_PARAM);
	writel_and(~(1<<dmu_pll1_dreset_SHIFT), DMU_PLL1_CLK_PARAM);
	
	/* set p1, p2, and NDIV_INT */
	writel_and((~0xFFFF), DMU_PLL1_CLK_PARAM);

	writel_or(((p1&0x0f)<<4)|(p2&0x0f), DMU_PLL1_CLK_PARAM);
	writel_or((ndiv_int<<dmu_pll1_ndiv_int_SHIFT), DMU_PLL1_CLK_PARAM);

	/* set NDIV_FRAC */
	writel_and((~0xFFFFFF), DMU_PLL1_FRAC_PARAM);
	writel_or(ndiv_frac, DMU_PLL1_FRAC_PARAM);

	/* set M2 */
	writel_and((~0xFF), DMU_PLL1_CH2_PARAM);
	writel_or(m2, DMU_PLL1_CH2_PARAM);
	
	/* dmu pll1 power on*/
	writel_and(~0x10000000, DMU_PLL1_FRAC_PARAM);
	writel_and(~0x02000000, DMU_PLL1_CLK_PARAM);
	writel_and(~0x00000100, DMU_PLL1_CH2_PARAM);

	/* clkout1 enable */
	writel_and(~(0x1<<16),	DMU_R_dmu_clkout_sel_MEMADDR);

	/* config ws */
	writel_and(~(0xf<<I2S_DAI2S_TXSR_SHIFT), I2S_DAI2S_REG);
	writel_or((i2s_txsr<<I2S_DAI2S_TXSR_SHIFT), I2S_DAI2S_REG);
	
	return 0;
}

int i2s_param_set(uint channel_mode, uint sample_rate)
{
	int txsr_val;
	int i;
	
	/* disable tx intr */
	disable_irq(IRQ_OI2S);
	writel_and(~(1<<I2S_DATXCTRL_TxIntEn_SHIFT), I2S_DATXCTRL_REG); /* disable intr */
	writel_or((1<<I2S_DATXCTRL_TxIntStat_SHIFT), I2S_DATXCTRL_REG); /* clear intr flag */

	/* set soft reset and clear */
	writel_or((0x1<<I2S_DATXCTRL_SRST_SHIFT), 	I2S_DATXCTRL_REG);

	while(!(readl(I2S_DATXCTRL_REG) & (0x1<<I2S_DATXCTRL_SRST_SHIFT)));

	writel_and(~(0x1<<I2S_DATXCTRL_SRST_SHIFT),	I2S_DATXCTRL_REG);

	if(channel_mode == 2)
		writel_and(~(0x1<<I2S_DAI2S_TXMode_SHIFT),	I2S_DAI2S_REG); /* stereo */
	else
		writel_or((0x1<<I2S_DAI2S_TXMode_SHIFT),	I2S_DAI2S_REG); /* mono */

	writel_and(~(1<<I2S_DAI2S_TX_START_SHIFT), I2S_DAI2S_REG);

	/* set i2s sampling rate */
	bcm5892_i2s_set_sampling_rate(sample_rate);

	/* flush i2s fifo */
	writel_or((1<<I2S_DATXCTRL_Flush_SHIFT), I2S_DATXCTRL_REG);
	while(!(readl(I2S_DATXCTRL_REG) && (1<<I2S_DATXCTRL_Flush_SHIFT)));
	writel_and(~(1<<I2S_DATXCTRL_Flush_SHIFT), I2S_DATXCTRL_REG);

    /* enable sck */
    writel_and(~(1<<I2S_DAI2S_SCKSTOP_SHIFT), I2S_DAI2S_REG); 
	return 0;
}

void bcm5892_i2s_init(void)
{
	gpio_set_pin_type(GPIOD, I2S_WS, 	GPIO_FUNC0);
	gpio_set_pin_type(GPIOD, I2S_SCK, 	GPIO_FUNC0);
	gpio_set_pin_type(GPIOD, I2S_SDI, 	GPIO_FUNC0);
	gpio_set_pin_type(GPIOD, I2S_SDO, 	GPIO_FUNC0);

	/* set bclk to 12MHz */
	writel_and(~(0x7<<18),	DMU_R_dmu_clk_sel_MEMADDR); 
	writel_or((5<<18), 		DMU_R_dmu_clk_sel_MEMADDR);

	/* clkout1 disable */
	writel_or((0x1<<16),	DMU_R_dmu_clkout_sel_MEMADDR);

	/* select pll1_tap2 for clkout1 */
	writel_and(~(0xf<<18),	DMU_R_dmu_clkout_sel_MEMADDR);
	writel_or((5<<18),  	DMU_R_dmu_clkout_sel_MEMADDR);

	/* disable tx intr & clear tx intr */
	disable_irq(IRQ_OI2S);
	writel_and(~(1<<I2S_DATXCTRL_TxIntEn_SHIFT), I2S_DATXCTRL_REG);
	writel_or((1<<I2S_DATXCTRL_TxIntStat_SHIFT), I2S_DATXCTRL_REG);

	/* interrupt register */
	s_SetInterruptMode(IRQ_OI2S, INTC_PRIO_3, INTC_IRQ);
	s_SetIRQHandler(IRQ_OI2S, (void (*)(int))interrupt_i2s);
}

int i2s_data_conv(int dataIn, int *dataOut, int bytes, int vol)
{
	int regval, i, tmp;

	if(bytes == 2)
	{
		if(vol > 4 && vol <= MAX_AUDIO_VOLUME) /* turn up */
		{
			if(dataIn >= 0x8000)
			{
				tmp = (0x10000 - dataIn) * (vol-4);
				if(tmp > 0x8000) tmp = 0x8000;	
				else tmp = 0x10000 - tmp;
			}
			else 
			{
				tmp = dataIn * (vol-4);
				if(tmp > 0x7fff) tmp = 0x7fff;
			}
		}
		else if(vol <= 4 && vol > MIN_AUDIO_VOLUME) /* turn down */
		{
			if(dataIn >= 0x8000)
			{
				tmp = ((0x10000 - dataIn) * vol + 4) / 5;
				tmp = 0x10000 - tmp;
			}
			else 
				tmp = (dataIn * vol + 4) / 5;
		}
		else
			return -1;
	}
	else if(bytes == 1)
	{
		if(vol > 4 && vol <= MAX_AUDIO_VOLUME) /* turn up */
		{
			if(dataIn >= 0x80)
			{
				tmp = (dataIn - 0x80) * (vol-4);
				if(tmp > 0x7f) tmp = 0x7f;
				tmp = (tmp&0x7f) * 258;
			}
			else
			{
				tmp = (0x80 - dataIn) * (vol-4);
				if(tmp > 0x7f)  tmp = 32768;
				else tmp = 65536 - tmp*258;
			}
		}
		else if(vol <= 4 && vol > MIN_AUDIO_VOLUME) /* turn down */
		{
			if(dataIn >= 0x80)
			{
				tmp = ((dataIn - 0x80) * vol + 4) / 5;
				tmp = (tmp&0x7f) * 258;
			}
			else
			{
				tmp = ((0x80 - dataIn) * vol + 4) / 5;
				tmp = 65536 - tmp*258;
			}
		}
		else
			return -1;
	}

	*dataOut = tmp;

	return 0;
}

int i2s_phy_write(QUEUE_T *ptAudioQue, int bitsPerSample)
{
	int regval, audioData, flag, ret, vol=guiAudioVolumeValue;
	int iTmp, i=0;
	uchar ucTmp;
	
	if(bitsPerSample != 8 && bitsPerSample != 16) return 0;

	irq_save(flag);	
	IsAudioEnabled = 1;
	irq_restore(flag);	
    gpio_set_pin_type(GPIOE, 8, GPIO_OUTPUT); /* enable amplifier */
    gpio_set_pin_val(GPIOE, 8, 1);

	/* enable interrupt here */
	writel((1<<I2S_DATXCTRL_Enable_SHIFT)|(1<<I2S_DATXCTRL_TxIntEn_SHIFT), I2S_DATXCTRL_REG);
	writel_or((32<<I2S_DATXCTRL_TxIntPeriod_SHIFT), I2S_DATXCTRL_REG);

	do {
		if(!QueueGetc(ptAudioQue, &ucTmp)) goto ERR_EXIT;
		audioData = ucTmp;

		if(bitsPerSample == 16)
		{
			if(!QueueGetc(ptAudioQue, &ucTmp)) goto ERR_EXIT;
			audioData |= (ucTmp<<8);
		}

		ret = i2s_data_conv(audioData, &iTmp, bitsPerSample/8, vol);
		if(ret) goto ERR_EXIT;

		writel(iTmp&0xffff, I2S_DAFIFO_DATA_REG);

		regval = readl(I2S_DADEBUG_REG);
		regval = regval & 0xff; 
	}while(regval < I2S_MAX_FIFO_SIZE);	

	/* i2s transmit Interface enable */
	writel_or((1<<I2S_DAI2S_TXEN_SHIFT), I2S_DAI2S_REG);
	DelayMs(250);
	enable_irq(IRQ_OI2S);
	return 0;

ERR_EXIT:
	writel_and(~(1<<I2S_DATXCTRL_TxIntEn_SHIFT), I2S_DATXCTRL_REG); /* disable intr */
	writel_and(~(1<<I2S_DAI2S_TXEN_SHIFT), I2S_DAI2S_REG); /* i2s transmit Interface disable */
	writel_or((0x1<<16),	DMU_R_dmu_clkout_sel_MEMADDR); /* clkout1 disable */
	disable_irq(IRQ_OI2S);

	irq_save(flag);	
	if(!IsBeepEnabled) gpio_set_pin_val(GPIOE, 8, 0); /* disable amplifier */
	IsAudioEnabled = 0;
	irq_restore(flag);	
	return 0;
}

/* end of bcm5892_i2s.c */
