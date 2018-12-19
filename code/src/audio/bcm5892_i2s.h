#ifndef __BCM5892_H__
#define __BCM5892_H__

/* I2S Ports: gpio_D */
#define I2S_WS		6
#define I2S_SCK		7
#define I2S_SDI		8
#define I2S_SDO		9

/* I2S registers */
/*------------------------------------------------------*/
#define I2S_BASE_ADDR			0x000D8000

#define I2S_DATXCTRL_REG		(I2S_BASE_ADDR+0x00)
/* I2S_DATXCTRL_REG bit define */
#define I2S_DATXCTRL_TxIntPeriod_SHIFT 		(8) /* 1 - 127 */
#define I2S_DATXCTRL_TxIntEn_SHIFT  		(7)
#define I2S_DATXCTRL_TxIntStat_SHIFT		(6)
#define I2S_DATXCTRL_SRST_SHIFT				(3)
#define I2S_DATXCTRL_Flush_SHIFT			(1)
#define I2S_DATXCTRL_Enable_SHIFT			(0)

#define I2S_DARXCTRL_REG		(I2S_BASE_ADDR+0x04)
#define I2S_DAFIFO_DATA_REG		(I2S_BASE_ADDR+0x08)

#define I2S_DAI2S_REG			(I2S_BASE_ADDR+0x0C)
/* I2S_DAI2S_REG bit define */
#define I2S_DAI2S_TXSR_SHIFT				(8)
#define I2S_DAI2S_TX_START_SHIFT			(6)
#define I2S_DAI2S_TXEN_SHIFT				(5)
#define I2S_DAI2S_SCKSTOP_SHIFT				(3)
#define I2S_DAI2S_TXMode_SHIFT				(1)


#define I2S_DAI2SRX_REG			(I2S_BASE_ADDR+0x10)
#define I2S_DADMACTRL_REG		(I2S_BASE_ADDR+0x14)
/* register for debuging */
#define I2S_DADEBUG_REG			(I2S_BASE_ADDR+0x18)
#define I2S_DASTA_REG			(I2S_BASE_ADDR+0x1C)
#define I2S_TXFIFOPTR_REG		(I2S_BASE_ADDR+0x20)

#define I2S_MAX_FIFO_SIZE		120


/* PLL1 registers */
/*-----------------------------------------------------*/
#define DMU_BASE_ADDR			0x01024000
#define DMU_PLL1_CLK_PARAM 		(DMU_BASE_ADDR+0x20)
/* DMU_PPL1_CLK_PARAM bit define */
#define dmu_pll1_dreset_SHIFT				(27)
#define dmu_pll1_ndiv_int_SHIFT				(8)

#define DMU_PLL1_FRAC_PARAM		(DMU_BASE_ADDR+0x24)
#define DMU_PLL1_CH1_PARAM		(DMU_BASE_ADDR+0x28)
#define DMU_PLL1_CH2_PARAM		(DMU_BASE_ADDR+0x2c)

#endif /* end of define __BCM5892_H__ */

