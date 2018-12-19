#include "base.h"

#define SPI_CPHA        0x01                    /* clock phase */
#define SPI_CPOL        0x02                    /* clock polarity */

//SPI mode 
#define SPI_MODE_0      (0|0)                   /* (original MicroWire) */
#define SPI_MODE_1      (0|SPI_CPHA)
#define SPI_MODE_2      (SPI_CPOL|0)
#define SPI_MODE_3      (SPI_CPOL|SPI_CPHA)
#define	SPI_3WIRE	0x10	/* SI/SO signals shared */

//registers for spi
#define PL022_CR0   (0x00)
#define PL022_CR1   (0x04)
#define PL022_DR    (0x08)
#define PL022_SR    (0x0c)
#define PL022_CPSR  (0x10)
#define PL022_IMSC  (0x14)
#define PL022_RIS   (0x18)
#define PL022_MIS   (0x1c)
#define PL022_CIS   (0x20)
#define PL022_DMACR (0x24)

//status
#define PL022_SR_BSY (0x10)
#define PL022_SR_RFF (0x08)
#define PL022_SR_RNE (0x04)
#define PL022_SR_TNF (0x02)
#define PL022_SR_TFE (0x01)


#define CPSR_MAX 254  /* prescale divisor */
#define CPSR_MIN 2    /* prescale divisor min (must be divisible by 2) */
#define SCR_MAX 255   /* other divisor */
#define SCR_MIN 0

#define SPI_INPUT_CLOCK (66000000)  /* better if we can get this from the board */
#define MAX_CALC_SPEED_HZ (SPI_INPUT_CLOCK / (CPSR_MIN * (1 + SCR_MIN)))
#define MIN_CALC_SPEED_HZ (SPI_INPUT_CLOCK / (254 * 256))
#define MAX_SUPPORTED_SPEED_HZ (12000000)

#define MAX_SPEED_HZ ( (MAX_SUPPORTED_SPEED_HZ < MAX_CALC_SPEED_HZ) ? MAX_SUPPORTED_SPEED_HZ : MAX_CALC_SPEED_HZ)
#define MIN_SPEED_HZ (MIN_CALC_SPEED_HZ)

#define PL022_REG(base, reg) readl((base) + (reg))
#define PL022_WRITE_REG(val, base, reg) writel((val),((base) + (reg)))

void spi_config(int channel, uint speed, int data_size, uchar mode)
{
	 /* half_divisor = clock / (2*speed), rounded up: */
	uint half_divisor = (SPI_INPUT_CLOCK + (speed * 2 - 1)) / (speed*2);
	uint best_err = half_divisor;
	uint best_scr = SCR_MAX;
	uint best_half_cpsr = CPSR_MAX/2;
	uint scr, half_cpsr, err;

	uint polarity,phase;
	uint base;

	(mode & SPI_CPOL) ? (polarity=1):(polarity=0);
	(mode & SPI_CPHA) ? (phase =1):(phase=0);
	
	if(channel ==0) base =START_SPI0;
	else if(channel ==1) base =START_SPI1;
	else if(channel ==2) base =START_SPI2;
	else if(channel ==3) base =START_SPI3_CFG;
	else base = SPI4_REG_BASE_ADDR;

	/* Loop over possible SCR values, calculating the appropriate CPSR and finding the best match
	 * For any SPI speeds above 260KHz, the first iteration will be it, and it will stop.
	 * The loop is left in for completeness */
	//printk("Setting up PL022 for: %dHz, mode %d, %d bits (target %d)\r\n",
	//       speed, mode, data_size, half_divisor);

	for (scr = SCR_MIN; scr <= SCR_MAX; ++scr) {
		/* find the right cpsr (rounding up) for the given scr */
		half_cpsr = ((half_divisor + scr) / (1+scr));

		if (half_cpsr < CPSR_MIN/2)
			half_cpsr = CPSR_MIN/2;
		if (half_cpsr > CPSR_MAX/2)
			continue;

		err = ((1+scr) * half_cpsr) - half_divisor;

		if (err < best_err) {
			best_err = err;
			best_scr = scr;
			best_half_cpsr = half_cpsr;
			if (err == 0)
				break;
		}
	}

	//printk("Actual clock rate: %dHz\r\n", SPI_INPUT_CLOCK / (2 * best_half_cpsr * (1+best_scr)));

	//printk("Setting PL022 config: %08x %08x %08x\r\n",
	//	(best_scr<<8) |(phase<<7) |(polarity<6) |(data_size-1), 2, best_half_cpsr * 2);

	/* Set CR0 params */
	writel((best_scr<<8) |(phase<<7) |(polarity<<6) |(data_size-1), (base+PL022_CR0));

	writel(2, (base+PL022_CR1));

	/* Set prescale divisor */
	writel(best_half_cpsr * 2, (base+PL022_CPSR));

}

void spi_tx(int channel,uchar*tx_buf ,int len,int data_bits)
{
	int i;
	uint base;
	ushort *tx_buf16;
	
	if(channel ==0) base =START_SPI0;
	else if(channel ==1) base =START_SPI1;
	else if(channel ==2) base =START_SPI2;
	else if(channel ==3) base =START_SPI3_CFG;
	else base = SPI4_REG_BASE_ADDR;

	if(data_bits <=8) 		
    {
    	/* bytes or smaller */
    	for (i = 0; i < len; ++i) 
        {
    		/* wait until txfifo is not full */
    		while (!(readl(base+PL022_SR) & PL022_SR_TNF));
    		writel(tx_buf[i],(base+PL022_DR));
    	}
    }
    else
    {
        tx_buf16=(ushort *)tx_buf;
        for (i = 0; i < len; ++i) 
        {
    		/* wait until txfifo is not full */
    		while (!(readl(base+PL022_SR) & PL022_SR_TNF));
    		writel(tx_buf16[i],(base+PL022_DR));
    	}
	}
}

int spi_txcheck(int channel)
{
	uint base;
	if(channel ==0) base = START_SPI0;
	else if(channel ==1) base =START_SPI1;
	else if(channel ==2) base =START_SPI2;
	else if(channel ==3) base =START_SPI3_CFG;
	else base = SPI4_REG_BASE_ADDR;

	if(readl(base+PL022_SR) & PL022_SR_BSY)
	{
		return 1;
	}
	return 0;
}

int spi_rx(int channel,uchar *rx_buf, int len,int data_bits)
{
	int i,num_rxd;
 	uint base;
	ushort *rx_buf16;

	if(channel ==0) base = START_SPI0;
	else if(channel ==1) base =START_SPI1;
	else if(channel ==2) base =START_SPI2;
	else if(channel ==3) base =START_SPI3_CFG;
	else base = SPI4_REG_BASE_ADDR;

	num_rxd = 0;
    if(data_bits<=8)
    {
        while (readl(base+PL022_SR) & PL022_SR_RNE) 
    	{
    		rx_buf[num_rxd] =readl(base+PL022_DR);    			
    		num_rxd++;
    		if(num_rxd >= len) break;
    	}
    }
    else
    {
        rx_buf16=(ushort*)rx_buf;
    	while (readl(base+PL022_SR) & PL022_SR_RNE) 
    	{
    		rx_buf16[num_rxd] =readl(base+PL022_DR);    			
    		num_rxd++;
    		if(num_rxd >= len) break;
    	}
    }

	return num_rxd;
}

ushort spi_txrx(int channel,ushort data,int data_bits) 
{
	ushort value;

	spi_tx(channel,(uchar*)&data,1,data_bits);
	while(spi_txcheck(channel));
	spi_rx(channel, (uchar*)&value,1,data_bits);

	return value;
}



/* setup mode and clock, etc (spi driver may call many times) */
static int spi_setup (int channel)
{
	int bits_per_word = 8;
	uint max_speed_hz = 20000000;
	uchar spimode = SPI_3WIRE | SPI_MODE_3;
	int initial_cs_pin_state = 1;

	//printk("PL022-SPI:	configuring hw\n");

	spi_config(channel,max_speed_hz, bits_per_word,spimode);

	//printk("PL022-SPI: pl022_spi_setup() done.\n");

	return 0;
}


