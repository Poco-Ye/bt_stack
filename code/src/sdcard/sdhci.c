#include <stdio.h>
#include <string.h>
#include "mmc.h"
#include "sdhci.h"
#include "bcm5892_reg.h"
#include "base.h"

static unsigned long s_sd_baseaddr=0;

#define sdhci_readb(addr) (*(volatile unsigned char *) (addr+s_sd_baseaddr))
#define sdhci_readw(addr) (*(volatile unsigned short *) (addr+s_sd_baseaddr))
#define sdhci_readl(addr) (*(volatile unsigned int *) (addr+s_sd_baseaddr))
#define sdhci_writeb(b,addr) ((*(volatile unsigned char *) (addr+s_sd_baseaddr)) = (b))
#define sdhci_writew(b,addr) ((*(volatile unsigned short *) (addr+s_sd_baseaddr)) = (b))
#define sdhci_writel(b,addr) ((*(volatile unsigned int *) (addr+s_sd_baseaddr)) = (b))

static unsigned int sdhci_ver=0;
static unsigned int sdhci_clock=0;

static int fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

static void sdhci_reset(unsigned char mask)
{
    volatile unsigned int t0;

	sdhci_writeb(mask, SDHCI_SOFTWARE_RESET);
    t0=s_Get10MsTimerCount();
    //100ms timeout
	while (sdhci_readb(SDHCI_SOFTWARE_RESET) & mask) 
    {
        if((s_Get10MsTimerCount()-t0) >10)return;
	}
}

static void sdhci_cmd_done(struct mmc_cmd *cmd)
{
	int i;
    
	if (cmd->resp_type & MMC_RSP_136) 
    {
		/* CRC is stripped so we need to do some shifting. */
		for (i = 0; i < 4; i++) 
        {
			cmd->response[i] = sdhci_readl(SDHCI_RESPONSE + (3-i)*4) << 8;
			if (i != 3)
				cmd->response[i] |= sdhci_readb(SDHCI_RESPONSE + (3-i)*4-1);
		}
	} 
    else
    {
		cmd->response[0] = sdhci_readl(SDHCI_RESPONSE);
	}
}

static void sdhci_transfer_pio(struct mmc_data *data)
{
	int i;
	char *offs;

	for (i = 0; i < data->blocksize; i += 4) 
    {
		offs = data->dest + i;
		if (data->flags == MMC_DATA_READ)
			*(unsigned int *)offs = sdhci_readl(SDHCI_BUFFER);
		else
			sdhci_writel(*(unsigned int *)offs, SDHCI_BUFFER);
	}
}

static int sdhci_transfer_data(struct mmc_data *data,
				unsigned int start_addr)
{
	unsigned int stat, rdy, mask, block = 0,t0;

	rdy = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
	mask = SDHCI_DATA_AVAILABLE | SDHCI_SPACE_AVAILABLE;

    t0=s_Get10MsTimerCount();

	do 
    {
		stat = sdhci_readl(SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR) 
        {
//			printf("Error detected in status(0x%X)!\n", stat);
			return -1;
		}
		if (stat & rdy) 
        {
			if (!(sdhci_readl(SDHCI_PRESENT_STATE) & mask))
				continue;
			sdhci_writel(rdy, SDHCI_INT_STATUS);
			sdhci_transfer_pio(data);
			data->dest += data->blocksize;
			if (++block >= data->blocks)
                break;
		}
        
        //100ms timeout
        if((s_Get10MsTimerCount()-t0) >10)return -1;
	} while (!(stat & SDHCI_INT_DATA_END));
    
	return 0;
}

int sdhci_send_command(struct mmc *mmc, struct mmc_cmd *cmd,
		       struct mmc_data *data)
{
	unsigned int stat = 0;
	int ret = 0;
	int trans_bytes = 0, is_aligned = 1;
	unsigned int mask, flags, mode;
	unsigned int t0,start_addr = 0;

    DelayUs(100);

	sdhci_writel(~(SDHCI_INT_CARD_INSERT|SDHCI_INT_CARD_REMOVE), 
        SDHCI_INT_STATUS);
	mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;

	/* We shouldn't wait for data inihibit for stop commands, even
	   though they might use busy signaling */
	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		mask &= ~SDHCI_DATA_INHIBIT;

    t0=s_Get10MsTimerCount();
	while (sdhci_readl(SDHCI_PRESENT_STATE) & mask) 
    {
        //1000ms
        if((s_Get10MsTimerCount()-t0) >100)return COMM_ERR;
	}
    
	mask = SDHCI_INT_RESPONSE;
	if (!(cmd->resp_type & MMC_RSP_PRESENT))
	{
		flags = SDHCI_CMD_RESP_NONE;
	}
	else if (cmd->resp_type & MMC_RSP_136)
	{
		flags = SDHCI_CMD_RESP_LONG;
	}
	else if (cmd->resp_type & MMC_RSP_BUSY) 
    {
		flags = SDHCI_CMD_RESP_SHORT_BUSY;
		mask |= SDHCI_INT_DATA_END;
	} 
    else
    {
		flags = SDHCI_CMD_RESP_SHORT;
    }
    
	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= SDHCI_CMD_CRC;
	if (cmd->resp_type & MMC_RSP_OPCODE)
		flags |= SDHCI_CMD_INDEX;
	if (data)
		flags |= SDHCI_CMD_DATA;

	/*Set Transfer mode regarding to data flag*/
	if (data != 0) 
    {
		sdhci_writeb(0xe, SDHCI_TIMEOUT_CONTROL);
		mode = SDHCI_TRNS_BLK_CNT_EN;
		trans_bytes = data->blocks * data->blocksize;
		if (data->blocks > 1)
			mode |= SDHCI_TRNS_MULTI;

		if (data->flags == MMC_DATA_READ)
			mode |= SDHCI_TRNS_READ;

		sdhci_writew(SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,
				data->blocksize),
				SDHCI_BLOCK_SIZE);
		sdhci_writew(data->blocks, SDHCI_BLOCK_COUNT);
		sdhci_writew(mode, SDHCI_TRANSFER_MODE);
	}

	sdhci_writel(cmd->cmdarg, SDHCI_ARGUMENT);
	sdhci_writew(SDHCI_MAKE_CMD(cmd->cmdidx, flags), SDHCI_COMMAND);

    t0=s_Get10MsTimerCount();
	do
    {
		stat = sdhci_readl(SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR)
			break;

        //100ms
        if((s_Get10MsTimerCount()-t0) >10)return NO_CARD_ERR;
	} while ((stat & mask) != mask);

	if ((stat & (SDHCI_INT_ERROR | mask)) == mask) 
    {
		sdhci_cmd_done(cmd);
		sdhci_writel(mask, SDHCI_INT_STATUS);
	}
    else
    {
		ret = -1;
    }

	if (!ret && data)
		ret = sdhci_transfer_data(data, start_addr);

	stat = sdhci_readl(SDHCI_INT_STATUS);
	sdhci_writel(~(SDHCI_INT_CARD_INSERT|SDHCI_INT_CARD_REMOVE), SDHCI_INT_STATUS);
    if (!ret) return 0;

	sdhci_reset(SDHCI_RESET_CMD);
	sdhci_reset(SDHCI_RESET_DATA);
	if (stat & SDHCI_INT_TIMEOUT)return TIMEOUT;
	else return COMM_ERR;
}

static int sdhci_set_clock(struct mmc *mmc, unsigned int clock)
{
	unsigned int div, clk, t0;

	sdhci_writew(0, SDHCI_CLOCK_CONTROL);

	if (clock == 0)return 0;

	if (sdhci_ver >= SDHCI_SPEC_300) 
    {
		/* Version 3.00 divisors must be a multiple of 2. */
		if (mmc->f_max <= clock)div = 1;
		else 
        {
			for (div = 2; div < SDHCI_MAX_DIV_SPEC_300; div += 2) 
				if ((mmc->f_max / div) <= clock)break;
		}
	} 
    else 
    {
		/* Version 2.00 divisors must be a power of 2. */
		for (div = 1; div < SDHCI_MAX_DIV_SPEC_200; div *= 2) 
        {
			if ((mmc->f_max / div) <= clock)
				break;
		}
	}
	div >>= 1;

	clk = (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
	clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
		<< SDHCI_DIVIDER_HI_SHIFT;
	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(clk, SDHCI_CLOCK_CONTROL);

    t0=s_Get10MsTimerCount();
	while (!((clk = sdhci_readw(SDHCI_CLOCK_CONTROL))& SDHCI_CLOCK_INT_STABLE)) 
    {
		//50ms
		if((s_Get10MsTimerCount()-t0) >5)return -1;
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(clk, SDHCI_CLOCK_CONTROL);
    
	return 0;
}

static void sdhci_set_power(unsigned short power)
{
	unsigned char pwr = 0;

	if (power != (unsigned short)-1) 
    {
		switch (1 << power) 
        {
		case MMC_VDD_165_195:
			pwr = SDHCI_POWER_180;
			break;

        case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr = SDHCI_POWER_300;
			break;

        case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr = SDHCI_POWER_330;
			break;
		}
	}

	if (pwr == 0) 
    {
		sdhci_writeb(0, SDHCI_POWER_CONTROL);
		return;
	}

	pwr |= SDHCI_POWER_ON;

	sdhci_writeb(pwr, SDHCI_POWER_CONTROL);
}

void sdhci_set_ios(struct mmc *mmc)
{
	unsigned int ctrl;

	if (mmc->clock != sdhci_clock)
		sdhci_set_clock(mmc, mmc->clock);

	/* Set bus width */
	ctrl = sdhci_readb(SDHCI_HOST_CONTROL);
	if (mmc->bus_width == 8) 
    {
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		if (sdhci_ver >= SDHCI_SPEC_300)
			ctrl |= SDHCI_CTRL_8BITBUS;
	}
    else
    {
		if (sdhci_ver >= SDHCI_SPEC_300)
			ctrl &= ~SDHCI_CTRL_8BITBUS;
		if (mmc->bus_width == 4)
			ctrl |= SDHCI_CTRL_4BITBUS;
		else
			ctrl &= ~SDHCI_CTRL_4BITBUS;
	}

	if (mmc->clock > 26000000)
		ctrl |= SDHCI_CTRL_HISPD;
	else
		ctrl &= ~SDHCI_CTRL_HISPD;

	
	sdhci_writeb(ctrl, SDHCI_HOST_CONTROL);
}

int sdhci_init(struct mmc *mmc)
{
	/* Eable all state */
	sdhci_writel(SDHCI_INT_ALL_MASK, SDHCI_INT_ENABLE);
	sdhci_writel(SDHCI_INT_ALL_MASK, SDHCI_SIGNAL_ENABLE);

	sdhci_set_power(fls(mmc->voltages) - 1);

	return 0;
}

extern struct mmc sd0_mmc;

static int add_sdhci(struct mmc *mmc, unsigned int max_clk, unsigned int min_clk)
{
	unsigned int caps;
		
	memset(mmc,0,sizeof(struct mmc));
	
	mmc->send_cmd = sdhci_send_command;
	mmc->set_ios = sdhci_set_ios;
	mmc->init = sdhci_init;

	caps = sdhci_readl(SDHCI_CAPABILITIES);

	if (max_clk)
	{
		mmc->f_max = max_clk;
	}
	else
    {
		if (sdhci_ver >= SDHCI_SPEC_300)
			mmc->f_max = (caps & SDHCI_CLOCK_V3_BASE_MASK)
				>> SDHCI_CLOCK_BASE_SHIFT;
		else
			mmc->f_max = (caps & SDHCI_CLOCK_BASE_MASK)
				>> SDHCI_CLOCK_BASE_SHIFT;
		mmc->f_max *= 1000000;
	}
    
	if (mmc->f_max == 0) 
    {
		return -1;
	}
    
	if (min_clk)
	{
		mmc->f_min = min_clk;
	}
	else 
    {
		if (sdhci_ver >= SDHCI_SPEC_300)
			mmc->f_min = mmc->f_max / SDHCI_MAX_DIV_SPEC_300;
		else
			mmc->f_min = mmc->f_max / SDHCI_MAX_DIV_SPEC_200;
	}

	mmc->voltages = 0;
	if (caps & SDHCI_CAN_VDD_330)
		mmc->voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;
	if (caps & SDHCI_CAN_VDD_300)
		mmc->voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;
	if (caps & SDHCI_CAN_VDD_180)
		mmc->voltages |= MMC_VDD_165_195;
	mmc->host_caps = MMC_MODE_HS | MMC_MODE_HS_52MHz | MMC_MODE_4BIT;
	if (caps & SDHCI_CAN_DO_8BIT)
		mmc->host_caps |= MMC_MODE_8BIT;

	sdhci_reset(SDHCI_RESET_ALL);
	mmc_register(mmc);

	return 0;
}

unsigned char sdcard_is_attach(void)
{
    #if 0
    printk("\r%s:0x%X,0x%X\n",__func__,
        sdhci_readw(&sd0_host, SDHCI_INT_STATUS),
        sdhci_readl(&sd0_host, SDHCI_PRESENT_STATE));
    #endif
    return (!(sdhci_readw(SDHCI_INT_STATUS)&0x80));
}

int s_sdhci_config(void)
{
	if( (get_machine_type()==D210 && GetHWBranch()==D210HW_V2x) || 
	    (get_machine_type()==S900 && GetHWBranch()==S900HW_V3x)||
	    (get_machine_type()==S800 && GetHWBranch()==S800HW_V4x))
	{
		s_sd_baseaddr = SDM1_REG_BASE_ADDR;
		gpio_set_drv_strength(GPIOE,0,GPIO_DRV_STRENGTH_6mA);
		gpio_set_drv_strength(GPIOE,1,GPIO_DRV_STRENGTH_6mA);
		gpio_set_pin_type(GPIOE,0,GPIO_FUNC1);
		gpio_set_pin_type(GPIOE,1,GPIO_FUNC1);
	}
	else
	{
		s_sd_baseaddr = SDM0_REG_BASE_ADDR;
	}

	sdhci_ver = sdhci_readw(SDHCI_HOST_VERSION);
	return add_sdhci(&sd0_mmc, 0, 0);
}
