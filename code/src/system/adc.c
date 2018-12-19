#include "base.h"
#include "bcm5892-adc.h"

#define TDM_SIZE    0x10001  /*256Bytes*/
#define TDM_OFFSET  0x0000  /*start from sram*/

void s_AdcInit()
{
	// 动态ADC初始化
	writel(ADC_REG_SC_CONFIG_DIV_SRST, ADC_REG_SC_CONFIG);/*enable soft reset of divisor logic,REFCLK/8 */	
	writel(ADC_REG_SC_CONFIG_DIV_EN, ADC_REG_SC_CONFIG); /*enable REFCLKDIV: */ 
	writel_or(ADC_REG_SC_CONFIG_CHAN3_EN, ADC_REG_SC_CONFIG);/*all data via TDM channel 3*/    
	writel(ADC_REG_SC_SCAN_RATE_VAL(14,14,14), ADC_REG_SC_SCAN_RATE);
	writel_or(ADC_REG_SC_CONFIG_CONTINUOUS, ADC_REG_SC_CONFIG);/*continuous sample*/ 

	/*disable all TDM Channel*/
	writel(0,MEM_R_MEM_CH_CTRL0_MEMADDR);
	writel(0,MEM_R_MEM_CH_CTRL1_MEMADDR);
	writel(0,MEM_R_MEM_CH_CTRL2_MEMADDR);
	writel(0,MEM_R_MEM_CH_CTRL3_MEMADDR);
	writel(TDM_SIZE,MEM_R_TDM_CHBUF3_MEMADDR);          /*256Bytes Buffer*/
	writel(TDM_OFFSET/0x100,MEM_R_TDM_CHPTR3_MEMADDR);  /*SRAM offset address*/
	writel(0x01,MEM_R_MEM_CH_CTRL3_MEMADDR); /*refresh channel point to inital location*/
	writel(0x02,MEM_R_MEM_CH_CTRL3_MEMADDR); /*enable TDM channel3*/

	/* power on adc */  
	writel_or(ADC_REG_SC_CONFIG_PWR_ALL, ADC_REG_SC_CONFIG);
	writel_or(ADC_REG_SC_CONFIG_RESET,ADC_REG_SC_CONFIG);
	writel_and(~ADC_REG_SC_CONFIG_RESET,ADC_REG_SC_CONFIG); /*reset scan ADC*/
	writel_or(0x07, ADC_REG_SC_CONFIG); /*enable channel 0-2*/ 
}

/*
channel:
0,1,2,动态adc
*/
int s_ReadAdc(uint channel)
{
    int i = 0,j;    
    static int channel_val[3]={0,0,0};
    ushort *pt,temp;

    pt=(ushort*)(BCM5892_SRAM_BASE +TDM_OFFSET);

	temp=0;   	
	for(i=0,j=0;i<128;i++)/*scan adc*/
	{
		if((pt[i]>>12) != channel) continue;
		temp+=pt[i] & 0x3ff;
		j++;              
	}

	if(j>0) return temp/j; 

    return 0;
}

/*
channel:
0,1,2,动态adc
*/
int s_ReadAdcLatest(uint channel)
{
    return readl(ADC_REG_SC_DATA0+channel*4)&0x3ff;
}

int s_ReadStaticAdc(int channel)
{
	int val;

	
	writel(ADC_REG_ST_CONFIG_RESET, ADC_REG_ST_CONFIG);
	writel(ADC_REG_ST_CONFIG_DIV_SRST|ADC_REG_ST_CONFIG_CLKDIV16, ADC_REG_ST_CONFIG);
	writel(ADC_REG_ST_CONFIG_DIV_EN, ADC_REG_ST_CONFIG);

	writel_or(ADC_REG_ST_CONFIG_PWR, ADC_REG_ST_CONFIG);
	writel_or(ADC_REG_ST_CONFIG_CHANNEL(channel), ADC_REG_ST_CONFIG);
	writel(0x03, ADC_REG_ST_INT_CLR);
	writel_or(ADC_REG_ST_CONFIG_EN, ADC_REG_ST_CONFIG);

	while(!( readl(ADC_REG_ST_RAW_INT_STA) & ADC_REG_ST_INT_CONV_DONE));
	val = readl(ADC_REG_ST_DATA);
	
	return val;
	
}


