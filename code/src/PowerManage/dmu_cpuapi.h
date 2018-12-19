#ifndef _DMU_CPUAPI_H
#define _DMU_CPUAPI_H

#include "base.h"

#define REFCLK		0x0
#define PLLCLK_TAP1	0x1
#define BBLCLK		0x2
#define SPLCLK		0x3
#define CPUCLK		0x3	
#define MCLK			0xc
#define HCLK			0x30	
#define PCLK			0xc0	
#define CPUCLK_SHIFT  0
#define BCLK			0x1c0000
#define BCLK_SHIFT   18


#define DMU_pll0_frac		IO_ADDRESS(DMU_R_dmu_pll0_frac_param_MEMADDR)
#define DMU_pll0_clk		IO_ADDRESS(DMU_R_dmu_pll0_clk_param_MEMADDR)
#define DMU_pll0_ctrl1	IO_ADDRESS(DMU_R_dmu_pll0_ctrl1_MEMADDR)
#define DMU_pll0_ch1		IO_ADDRESS(DMU_R_dmu_pll0_ch1_param_MEMADDR)
#define DMU_pll0_ch2		IO_ADDRESS(DMU_R_dmu_pll0_ch2_param_MEMADDR)
#define DMU_pll0_ch3		IO_ADDRESS(DMU_R_dmu_pll0_ch3_param_MEMADDR)
#define DMU_pll0_ch4		IO_ADDRESS(DMU_R_dmu_pll0_ch4_param_MEMADDR)

#define DMU_pll1_frac		IO_ADDRESS(DMU_R_dmu_pll1_frac_param_MEMADDR)
#define DMU_pll1_clk		IO_ADDRESS(DMU_R_dmu_pll1_clk_param_MEMADDR)
#define DMU_pll1_ctrl1	IO_ADDRESS(DMU_R_dmu_pll1_ctrl1_MEMADDR)
#define DMU_pll1_ch1		IO_ADDRESS(DMU_R_dmu_pll1_ch1_param_MEMADDR)
#define DMU_pll1_ch2		IO_ADDRESS(DMU_R_dmu_pll1_ch2_param_MEMADDR)
#define DMU_pwd_blk1	IO_ADDRESS(DMU_R_dmu_pwd_blk1_MEMADDR)
#define DMU_pwd_blk2	IO_ADDRESS(DMU_R_dmu_pwd_blk2_MEMADDR)
#define DMU_clk_sel		IO_ADDRESS(DMU_R_dmu_clk_sel_MEMADDR)
#define DMU_status		IO_ADDRESS(DMU_R_dmu_status_MEMADDR)

#define DDR_POWER_DOWN	IO_ADDRESS(DDR_R_POWER_DOWN_MODE_MEMADDR)          
#define GPIOB_INT_MSTAT	IO_ADDRESS(GIO1_R_GRPF1_INT_MSTAT_MEMADDR)        
#define GPIOB_INT_CLR		IO_ADDRESS(GIO1_R_GRPF1_INT_CLR_MEMADDR)            


#endif // __DMU_CPUAPI_H
