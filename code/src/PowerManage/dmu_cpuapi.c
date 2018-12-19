#include "dmu_cpuapi.h"
#include "base.h"
#include "../comm/comm.h"

void LP_Module_Entry(uchar *DownCtrl);
void LP_Module_Exit(uchar *DownCtrl);

extern SLEEP_CTRL sleep_ctrl[3];

int dmu_cpuclk_sel(uint src, uint clk, uint clk_sel) 
{
	uint count=0,value;

	value = readl(DMU_clk_sel);
	value = (value & ~clk) | (src <<clk_sel);
	writel(value, DMU_clk_sel);

	for(count=0;count<10000;count++) 
	{
		if(readl(DMU_clk_sel) ==value) break;
	}
	if (count>=10000) return -1;
	return 0;
}

int dmu_pll_resync(int p1div, int p2div, int ndiv, int m1div,int m2div,
						int m3div, int m4div, int pll_num, int frac,int bclk_sel)
{
	uint config, status, count;
	uint vco_rng, ndiv_frac;

	if(pll_num==0)
	{
		dmu_cpuclk_sel(REFCLK, CPUCLK,CPUCLK_SHIFT);
		config = DMU_F_dmu_pll0_enb_clkout_MASK;
		writel_or(config, DMU_pll0_clk);
		config = (1 << DMU_F_dmu_pll0_dreset_R)     ; // dreset
		writel_or(config, DMU_pll0_clk);

		config = p1div; 		// p1div
		config |= p2div << 4; 	// p2div
		config |= ndiv << 8; 	// ndiv
		config |= (3 << 26); 	// enb_clkout,dreset
		writel(config,DMU_pll0_clk);


		writel(m1div,DMU_pll0_ch1);
		writel(m2div,DMU_pll0_ch2);
		writel(m3div,DMU_pll0_ch3);
		writel(m4div,DMU_pll0_ch4);

		writel(frac | (2 << 25), DMU_pll0_frac);
		writel(0x202c2820,DMU_pll0_ctrl1);

		/* Clear enb_clkout */
		writel_and(~DMU_F_dmu_pll0_enb_clkout_MASK, DMU_pll0_clk);

		/*wait for unlock only if u change vco frequency,Poll on pll lock bit */
		for(count=0;count<8000;count++)
		{
			if((readl(DMU_status) & 0x03) ==0x03) break;
		}
		if(count >=8000) return -1;

	 	/*clear dreset*/ 
    	writel_and(~DMU_F_dmu_pll0_dreset_MASK,DMU_pll0_clk);
		dmu_cpuclk_sel(PLLCLK_TAP1, CPUCLK,CPUCLK_SHIFT);

	}
	else
	{
		writel_or(0x04000000,DMU_pll1_clk);
	
		config  = p1div                             ; // p1div
		config |= p2div << 4    ; // p2div
		config |= ndiv << 8  ; // ndiv
		config |= (3<< 26) ; // enb_clkout, dreset
		writel(config, DMU_pll1_clk);

		writel(m1div,DMU_pll1_ch1);
		writel(m2div,DMU_pll1_ch2);

		writel(frac | (2 << 25), DMU_pll1_frac);
		writel(0x202c2820,DMU_pll1_ctrl1);

		for(count=0;count<8000;count++)
		{
			if((readl(DMU_status) & 0x0C) ==0x0C) break;
		}
		if(count >=8000) return -2;

		dmu_cpuclk_sel(bclk_sel, BCLK,BCLK_SHIFT);

		// Clear enb_clkout
		writel_and(~0x04000000,DMU_pll1_clk);
		// clear dreset
		writel_and(~0x08000000,DMU_pll1_clk);

	} 
    	return 0;

}

int dmu_pll_power(int pwrup) 
{
	if(pwrup) 
	{
		writel_and( ~0x10000000,DMU_pll0_frac);
		writel_and( ~0x02000000,DMU_pll0_clk);
		writel_and( ~0x00000100,DMU_pll0_ch1);
		writel_and( ~0x00000100,DMU_pll0_ch2);
		writel_and( ~0x00000100,DMU_pll0_ch3);
		writel_and( ~0x00000100,DMU_pll0_ch4);

		writel_and(~0x10000000,DMU_pll1_frac);
		writel_and(~0x02000000,DMU_pll1_clk);
		writel_and(~0x00000100,DMU_pll1_ch1);
		writel_and(~0x00000100,DMU_pll1_ch2);
   	}
	else 
	{
		writel_or(0x10000000,DMU_pll0_frac);
		writel_or(0x02000000,DMU_pll0_clk);
		writel_or(0x00000100,DMU_pll0_ch1);
		writel_or(0x00000100,DMU_pll0_ch2);
		writel_or(0x00000100,DMU_pll0_ch3);
		writel_or(0x00000100,DMU_pll0_ch4);

		writel_or(0x10000000,DMU_pll1_frac);
		writel_or(0x02000000,DMU_pll1_clk);
		writel_or(0x00000100,DMU_pll1_ch1);
		writel_or(0x00000100,DMU_pll1_ch2);
	} //power down
	return 0;
}

void Entry_Sleep();
	
/*
 * svic_value = 0xf989ffff
 * bit17: RF (EINT1 Group 1)
 * bit18: MagDav (EINT2 Group 2)
 * bit21: PowerKey (EINT8-15 Group 5)
 * bit22: KeyIn (EINT16-23 Group 6)
 * bit23: WlanKey (EINT24-27 Group 7)
 * bit25-26: SmartCard 
 */
int bcm5892_suspend_enter(uchar *DownCtrl)
{
	unsigned int vic0_en,vic1_en,vic2_en;
	unsigned int disable1,disable2,blk1,blk2;
	unsigned int svic_value=0xf999ffff;  /* note above */ 
    uint vic2_value = 0xffffffff;
	uint raw_interrupt_svic,raw_interrupt_ovic1,raw_interrupt_ovic2;
	uint irq_interrupt_svic,irq_interrupt_ovic1,irq_interrupt_ovic2,flag;
    int result=0;

	irq_save(flag);
	/*disable all interrupt in vic0,vic1,vic2,except gpio group7*/
	vic0_en = readl(IO_ADDRESS(VIC0_REG_BASE_ADDR + VIC_INTENABLE));
	vic1_en = readl(IO_ADDRESS(VIC1_REG_BASE_ADDR + VIC_INTENABLE));
	vic2_en = readl(IO_ADDRESS(VIC2_REG_BASE_ADDR + VIC_INTENABLE));

	if (get_machine_type() == D200)
		svic_value=0xf919ffff;
	
	if(sleep_ctrl[MODULE_RTC].wake_enable)
	{
		svic_value &= ~(0x01<<2);
	}
	if(sleep_ctrl[MODULE_BT].wake_enable)
	{
		if(GetPortPhyNmb(P_BT) == 1)
			vic2_value &= ~(0x01<<7);// uart1 wake
		else if(GetPortPhyNmb(P_BT) == 3)
			vic2_value &= ~(0x01<<9);// uart3 wake
	}

	writel(svic_value,IO_ADDRESS(VIC0_REG_BASE_ADDR + VIC_INTENCLEAR));
	writel(0xffffffff,IO_ADDRESS(VIC1_REG_BASE_ADDR + VIC_INTENCLEAR));
	writel(vic2_value,IO_ADDRESS(VIC2_REG_BASE_ADDR + VIC_INTENCLEAR));

	writel(0xffffffff,IO_ADDRESS(VIC0_REG_BASE_ADDR + VIC_SOFTINTCLEAR));
	writel(0xffffffff,IO_ADDRESS(VIC1_REG_BASE_ADDR + VIC_SOFTINTCLEAR));
	writel(0xffffffff,IO_ADDRESS(VIC2_REG_BASE_ADDR + VIC_SOFTINTCLEAR));

	/*change to reference clock,  turn blocks clock off */
	dmu_cpuclk_sel(REFCLK, CPUCLK,CPUCLK_SHIFT);
	dmu_pll_power(0);

	LP_Module_Entry(DownCtrl); /*each module low power consumption config*/
	
	blk1 = readl(DMU_pwd_blk1);
	blk2 = readl(DMU_pwd_blk2);

	if (get_machine_type() == S500 || get_machine_type() == D200)
	{
		if(DownCtrl==NULL || DownCtrl[0]=='1') /*power down icc*/
			disable1 = 0xf8f783b3;//must , PBX-Z & memmory & GPIO & cfg & bbl & lcd
		else 
			disable1 = 0x78f783b3;//must , PBX-Z & memmory & GPIO & cfg & bbl & sci0 & lcd
		disable2 = 0xffeffbff;//must ,SPL&uart2
	}
	else
	{
		if(DownCtrl==NULL || DownCtrl[0]=='1') /*power down icc*/
			disable1 = 0xf8d383b3;//must , PBX-Z & memmory & GPIO & cfg & bbl & lcd
		else 
			disable1 = 0x78d383b3;//must , PBX-Z & memmory & GPIO & cfg & bbl & sci0 & lcd
		disable2 = 0xffeefbff;//must ,SPL&uart2
	}

	if(sleep_ctrl[MODULE_LCD].sleep_mode == 2)
	{
		disable1 &= ~(0x01<<28); //Lcd backlight pwm on.
	}

	if(sleep_ctrl[MODULE_BT].wake_enable)
	{
		if(GetPortPhyNmb(P_BT) == 1)
			disable2 &= ~(0x01<<19);
		else if(GetPortPhyNmb(P_BT) == 3)
			disable2 &= ~(0x01<<21);
	}

	writel(disable1, DMU_pwd_blk1);
	writel(disable2, DMU_pwd_blk2);
	
	ClearIDCache();
	Entry_Sleep();
  	gpio_restore_regs();
	
    //ModemPowerOff();
	
	dmu_pll_resync(1, 1, 33, 3, 16, 32, 9, 0, 0x555555, 0); //266M
	writel(blk1, DMU_pwd_blk1);
	writel(blk2, DMU_pwd_blk2);
	raw_interrupt_svic = readl(VIC0_REG_BASE_ADDR + VIC_RAWINTR);
	raw_interrupt_ovic1 = readl(VIC1_REG_BASE_ADDR + VIC_RAWINTR);
	raw_interrupt_ovic2 = readl(VIC2_REG_BASE_ADDR + VIC_RAWINTR);
    //ScrPrint(0,5,0,"s:%x-%x-%x",raw_interrupt_svic,raw_interrupt_ovic1,raw_interrupt_ovic2);	
    
	if(raw_interrupt_svic&(1<<IRQ_EXT_GRP2)) result=1;//swipe card waken
	if(raw_interrupt_svic&(1<<IRQ_SMARTCARD0)) result=2;//insert card waken
	if(raw_interrupt_svic&((1<<IRQ_EXT_GRP7) | (1<<IRQ_EXT_GRP6) | (1<<IRQ_EXT_GRP5)))result=3;//kbhit waken
	if(raw_interrupt_svic&(1<<IRQ_BBL_RTC)) result=4;//BBL/RTC wakeup
	if((GetPortPhyNmb(P_BT) == 1 && (raw_interrupt_ovic2&(1<<7)))
			|| (GetPortPhyNmb(P_BT) == 3 && (raw_interrupt_ovic2&(1<<9))))
		result=5;//BT Uart wakeup

	LP_Module_Exit(DownCtrl);

	/* enable interrupt */
	writel(vic0_en,IO_ADDRESS(VIC0_REG_BASE_ADDR + VIC_INTENABLE));
	writel(vic1_en,IO_ADDRESS(VIC1_REG_BASE_ADDR + VIC_INTENABLE));
	writel(vic2_en,IO_ADDRESS(VIC2_REG_BASE_ADDR + VIC_INTENABLE));
	irq_restore(flag);

	return result;
}



SECTION_SRAM void Disable_mmu()
{
	__asm("MRC     	P15, 0, R0, C1, C0, 0");       
	__asm("BIC     	       R0, R0, #0x1 ");      	/* disable MMU */
	__asm("MCR     	P15, 0, R0, C1, C0, 0 ");     

	__asm("BIC     R0, R0, #0x00001000");        	/* ensure I Cache disabled */
	__asm("BIC     R0, R0, #0x00000004");         	/* ensure D Cache disabled */
	__asm("MCR     	P15, 0, R0, C1, C0, 0 ");    

}

SECTION_SRAM void Enable_mmu()
{
	__asm("MRC	P15, 0, R0, C1, C0, 0");
	__asm("ORR   R0, R0, #0x00000001");
	__asm("MCR   P15, 0, R0, C1, C0, 0");       /*enable MMU*/
	__asm("ORR   R0, R0, #0x00001000");
	__asm("ORR   R0, R0, #0x00000004");     /* enabled I/D */
	__asm("MCR   P15, 0, R0, C1, C0, 0");   
	__asm("mov     r0,#0x2f000000   ");
	__asm("ldr     r0,[r0]  ");
}

	
SECTION_SRAM void Sram_Entry_Sleep()
{
	uint val,ddr_ctrl;
	volatile int i;
	Disable_mmu();
	ddr_ctrl=readl(DDR_POWER_DOWN);
	writel((ddr_ctrl | 0x2000) ,DDR_POWER_DOWN);  //DDR deep sleep
	val=readl(GPIOB_INT_MSTAT);
	writel(val,GPIOB_INT_CLR);//清中断标志
	writel(1,DMU_R_dmu_pm_MEMADDR);//enable deep sleep
	__asm("STMFD     SP!, {R0}");
	__asm("MOV  R0, #0");
	__asm("mcr	p15, 0, r0, c7, c0, 4");  /*wait for interrupt*/
	__asm("LDMFD     SP!, {R0}");	
	writel(0,DMU_R_dmu_pm_MEMADDR);//disable deep sleep
	writel(ddr_ctrl,DDR_POWER_DOWN);  //DDR deep sleep
//	for(i=0;i<0x10000;i++);
	Enable_mmu();
}


uint sp_add,sp_add2,lr_add;
void Entry_Sleep()
{
	__asm("MOV R3,SP");
	__asm("LDR SP,=0x1BFFF0");
	__asm("STMFD     SP!, {R0-R12,LR}");

	__asm("LDR R0,=Sram_Entry_Sleep");
	__asm("MOV LR,PC");
	__asm("BX R0");
	__asm("LDMFD     SP!, {R0-R12,LR}");	

	__asm("MOV SP,R3");

}

void SetOtherLP(void)
{
	int mask;
/*
	mask = 1<<13;
	gpio_set_mpin_type(GPIOA, mask, GPIO_INPUT);
*/
	mask = 1<<31|1<<28|1<<29|1<<30|1<<12/*|1<<9*/|1<<7;
	gpio_set_mpin_type(GPIOB, mask, GPIO_OUTPUT);
/*	
	mask = 1<<26|1<<27|1<<24;
	gpio_set_mpin_type(GPIOB, mask, GPIO_INPUT);
	mask = 1<<8;
	gpio_set_mpin_type(GPIOC, mask, GPIO_OUTPUT);
	*/

	mask = 1<<2;
	gpio_set_mpin_type(GPIOD, mask, GPIO_OUTPUT);

	mask = 1<<2|1<<8;
	gpio_set_mpin_type(GPIOE, mask, GPIO_OUTPUT);
/*
	gpio_set_pull(GPIOB, 26, 1);
	gpio_enable_pull(GPIOB, 26);
	gpio_set_pull(GPIOB, 27, 1);
	gpio_enable_pull(GPIOB, 27);
	gpio_set_pull(GPIOB, 24, 1);
	gpio_enable_pull(GPIOB, 24);
	gpio_set_pull(GPIOA, 13, 1);
	gpio_enable_pull(GPIOA, 13);
*/
	mask =1<<31/*|1<<9*/|1<<7;
	gpio_set_mpin_val(GPIOB, mask, 1);
	mask =1<<28|1<<29|1<<30|1<<12;
	gpio_set_mpin_val(GPIOB, mask, 0);
	
//	gpio_set_pin_val(GPIOC, 8, 0);
	gpio_set_pin_val(GPIOE, 8, 1);
	gpio_set_pin_val(GPIOD, 2, 0);

	
}
//TODO D200
void LP_Module_Entry(uchar *DownCtrl)
{
	writel_and(~(0x1<<20|0x1<<19),USBWC_R_usb_phy_control_MEMADDR);	 
    //Always put ETH PHY in IDDQ mode 
    writel_or(0x07,ETH_R_phyctrl_MEMADDR);

    gpio_save_regs();
	
	//UART
    if(rUART2CR&0x001)
    	rUART2CR&=~0x300; //disable tx/rx
	//gpio_set_pin_type(GPIOD,10,GPIO_INPUT);//解决休眠唤醒后误报收到数据0
	//gpio_set_pin_val(GPIOD, 10, 0);/*UART2 RX*/
	gpio_set_pin_type(GPIOD,11,GPIO_INPUT);
	gpio_set_pin_val(GPIOD, 11, 0);/*UART2 TX*/
	if(!(get_machine_type() == S900 && (GetHWBranch()!=S900HW_Vxx)))
	{
		gpio_set_pin_type(GPIOB,31,GPIO_OUTPUT);
		gpio_set_pin_val(GPIOB, 31, 1);/*PWR*/
	}

	//MODEM
    //ModemPowerOff();

	
	//IC&SAM
	if(DownCtrl==NULL || DownCtrl[0]=='1') /*power down icc*/
	{
		sci_bcm5892_suspend();
	}

	//USB
	gpio_set_pin_type(GPIOB,12,GPIO_OUTPUT);
	gpio_set_pin_val(GPIOB, 12, 0);

	if (get_machine_type() == S900)
	{
		//BAR CODE
		//gpio_set_pin_type(GPIOA, 19, GPIO_INPUT);//解决休眠唤醒后误报收到数据0
		//gpio_set_pin_val(GPIOA, 19, 0);	/*UART RX*/
		gpio_set_pin_type(GPIOA, 20, GPIO_INPUT);
		gpio_set_pin_val(GPIOA, 20, 0);	/*UART TX*/
		gpio_set_pin_type(GPIOC,6,GPIO_OUTPUT);
		gpio_set_pin_val(GPIOC, 6, 1);/*UART PWR*/
	}

	if (get_machine_type() != D200)
	{
	    //LCD back light  
	    gpio_set_pin_type(GPIOD,3,GPIO_OUTPUT);    
		gpio_set_pin_val(GPIOD,3, 1);
	}

	//keyboard back light
	gpio_set_pin_type(GPIOD,0,GPIO_OUTPUT);    
	gpio_set_pin_val(GPIOD,0, 1);

	if (get_machine_type() == S300 || get_machine_type() == S800
			|| get_machine_type() == S900 || get_machine_type() == D210)
	{
	    //lcd back light disable
	    gpio_set_pin_type(GPIOC, 7, GPIO_OUTPUT);    
		gpio_set_pin_val(GPIOC, 7, 0);
		
	    //lcd disable
		writel_and(~BIT0, LCD_R_LCDControl_MEMADDR);
	}

	
	if (get_machine_type() == D210)
	{
		SetOtherLP();
	}
}

void LP_Module_Exit(uchar *DownCtrl)
{
	uint mask;
	if (get_machine_type() == S300 || get_machine_type() == S800 
			|| get_machine_type() == S900 || get_machine_type() == D210)
	{
	    //lcd enable
	    writel_or(BIT0, LCD_R_LCDControl_MEMADDR);

		//lcd back light enable
	    gpio_set_pin_type(GPIOC, 7, GPIO_OUTPUT);    
		gpio_set_pin_val(GPIOC, 7, 1);
	}
	
    //uart2 power on
	gpio_set_pin_type(GPIOB,31,GPIO_OUTPUT);
	gpio_set_pin_val(GPIOB, 31, 0);/*PWR*/
	if(rUART2CR&0x001)
		rUART2CR |= 0x300; //enable tx/rx
    
	
	//writel_and(0x04,UMC_R_NVSRAM_MEMC_CFG_CLR_MEMADDR);
	writel_or((0x1<<20|0x1<<19),USBWC_R_usb_phy_control_MEMADDR);	 
	if(DownCtrl==NULL || DownCtrl[0]=='1') /*power down icc*/
	{
		sci_bcm5892_resume();
	}
	s_AdcInit();

}

#if 0
SECTION_SRAM void sDebugSend (uchar c)
{
	volatile int i, j;
    for(j=0;j<23500;j++);
	//--UARTFR:D5-TX Fifo Full,D7-TX Fifo Empty
	while (*((volatile uint*)(URT2_REG_BASE_ADDR+ 0x18)) & 0x20);
	(*((volatile uint *)URT2_REG_BASE_ADDR) = c);
		for(j=0;j<23500;j++);

}

uint getMclk()
{
    uint p1div,p2div,ndiv_int,ndiv_frac;
    uint cpuclk,hclk,pclk,mclk;
    uint cpuclk_sel,hclk_sel,pclk_sel,mclk_sel;
    uint vcofreq,sel,div,m1div;

    sel = readl(DMU_R_dmu_clk_sel_MEMADDR);
    div = readl(DMU_R_dmu_pll0_clk_param_MEMADDR);

    cpuclk_sel = ((sel & DMU_F_dmu_cpuclk_sel_MASK));
    mclk_sel   = ((sel & DMU_F_dmu_mclk_sel_MASK) >> DMU_F_dmu_mclk_sel_R);
    hclk_sel   = ((sel & DMU_F_dmu_hclk_sel_MASK) >> DMU_F_dmu_hclk_sel_R);
    pclk_sel   = ((sel & DMU_F_dmu_pclk_sel_MASK) >> DMU_F_dmu_pclk_sel_R);
 	printk ("clk_sel = %08x\r\n",sel);
    if (cpuclk_sel == 1)
    {
      
		p1div     = ((div & DMU_F_dmu_pll0_p1div_MASK));
		p2div     = ((div & DMU_F_dmu_pll1_p2div_MASK)    >> DMU_F_dmu_pll1_p2div_R);
		ndiv_int  = ((div & DMU_F_dmu_pll1_ndiv_int_MASK) >> DMU_F_dmu_pll1_ndiv_int_R);
		ndiv_frac = (readl(DMU_R_dmu_pll0_frac_param_MEMADDR) & DMU_F_dmu_pll0_ndiv_frac_MASK);
		m1div     = (readl(DMU_R_dmu_pll0_ch1_param_MEMADDR)  & DMU_F_dmu_pll0_m1div_MASK);
		vcofreq   = (24 * ndiv_int+ (24*ndiv_frac)/0x1000000) * p2div / p1div;
 
		cpuclk = (vcofreq/m1div);
		mclk = cpuclk/(mclk_sel+1);
		hclk = cpuclk/(hclk_sel+1);
		pclk = hclk/(pclk_sel+1);

    }

    else
    {
    	vcofreq =0;
    	cpuclk = 24;
    	hclk   = 24;
    	mclk   = 24;
    	pclk   = 24;
    }
    printk ("Calculated PLL lock frequency = %d MHz \r\n",vcofreq);
    printk ("Calculated cpuclk frequency = %d MHz \r\n",cpuclk);
    printk ("Calculated mclk frequency = %d MHz \r\n",mclk);
    printk ("Calculated hclk frequency = %d MHz \r\n",hclk);
    printk ("Calculated pclk frequency = %d MHz \r\n",pclk);
    printk ("Calculated mclk_sel = %d  \r\n",mclk_sel);

    return mclk;

}
#endif

