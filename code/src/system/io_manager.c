#include "Base.h"

/* GPIO group base, from bcm5892_reg.h */
/* There are 5 GPIO groups in 5892 */
#define	HW_GPIO0_PHY_BASE	GIO0_REG_BASE_ADDR
#define	HW_GPIO1_PHY_BASE	GIO1_REG_BASE_ADDR
#define	HW_GPIO2_PHY_BASE	GIO2_REG_BASE_ADDR
#define	HW_GPIO3_PHY_BASE	GIO3_REG_BASE_ADDR
#define	HW_GPIO4_PHY_BASE	GIO4_REG_BASE_ADDR

/* Register offsets */
#define GPIO_IOTR         0x000 /* GPIO Data in register */
#define GPIO_DOUT         0x004 /* GPIO Data out register */
#define GPIO_EN           0x008 /* GPIO driver enable register */
#define GPIO_INT_TYPE     0x00C /* GPIO Interrupt type register */
#define GPIO_INT_DE       0x010 /* GPIO Dual edge select register */
#define GPIO_INT_EDGE     0x014 /* GPIO Interrupt edge select register */
#define GPIO_INT_MSK      0x018 /* GPIO Interrupt mask register, 1 means INT allowed */
#define GPIO_INT_STAT     0x01C /* GPIO Interrupt Status register */
#define GPIO_INT_MSTAT    0x020 /* GPIO Interrupt masked status register */
#define GPIO_INT_CLR      0x024 /* GPIO Interrupt clear register */
#define GPIO_AUX_SEL      0x028 /* GPIO Auxiliary interface select register */
#define GPIO_SCR          0x02C /* GPIO security control register */
#define GPIO_INIT_VAL     0x030 /* GPIO Initial value register (unused) */
#define GPIO_PAD_RES      0x034 /* GPIO IO Programmable resistor direction */
#define GPIO_RES_EN       0x038 /* GPIO IO Programmable resister enable */
#define GPIO_TestInput    0x03C /* 28 Bit word sent to the GPIN[$CELL:0] bus */
#define GPIO_TestOutput   0x040 /* 28 Bit word from the GPOUT[$CELL:0] bus */
#define GPIO_TestEnable   0x044 /* 28 bit enable signal to drive GP_TestInput to the GP_DIN */
#define GPIO_PWR_TRISTATED	0x050 
#define GPIO_PWR_TRIEN		0x054 
#define GPIO_HYSEN_SW     0x058 /* When set this bit enables the Schmitt-trigger functionality option of the IO pad. */
#define GPIO_SLEW_SW      0x05C /* This register provides the GPIO IO pad slew controllability. Default is 1抌1. */
#define GPIO_DRV_SEL0_SW  0x060 /* used to specify the drive strength parameter of the corresponding GPIO pad. */
#define GPIO_DRV_SEL1_SW  0x064 /* used to specify the drive strength parameter of the corresponding GPIO pad. */
#define GPIO_DRV_SEL2_SW  0x068 /* used to specify the drive strength parameter of the corresponding GPIO pad. */
#define GPIO_AUX01_SEL    0x06C /* Mux select for each GPIO bit to select between two functional auxiliary inputs. */
#define GPIO_INT_POLARITY 0x070 /* Polarity definition for the GPIO interrupt detection logic. */
#define GPIO_GROUP0       0x074 /* Specifies the GPIOs which belong to GRP0. */

#define GPIO_OUT_SET       0 
#define GPIO_OUT_CLEAR   1 


#define HW_GPIOA_NUM_PIN    23
#define HW_GPIOB_NUM_PIN    32   // can config to extern interrupt port
#define HW_GPIOC_NUM_PIN    30
#define HW_GPIOD_NUM_PIN    18 
#define HW_GPIOE_NUM_PIN    10 

static void (*g_Port_func[HW_GPIOB_NUM_PIN])();  //端口1 A 中断函数表
#define ARCH_GPIO_PORT 5
static const uint GPIO_BASE_ADDR[]={
	HW_GPIO0_PHY_BASE+0x800,
	HW_GPIO1_PHY_BASE+0x800,
	HW_GPIO2_PHY_BASE+0x800,
	HW_GPIO3_PHY_BASE+0x800,
	HW_GPIO4_PHY_BASE+0x800
};

const uint GPIO_OUT_OP[5][2]={
	{HW_GPIO0_PHY_BASE+0x888,HW_GPIO0_PHY_BASE+0x88C},  //GPIOA---SET,CLEAR
	{HW_GPIO1_PHY_BASE+0x894,HW_GPIO1_PHY_BASE+0x898},  //GPIOB---SET,CLEAR
	{HW_GPIO2_PHY_BASE+0x87C,HW_GPIO2_PHY_BASE+0x880},  //GPIOC---SET,CLEAR	
	{HW_GPIO3_PHY_BASE+0x890,HW_GPIO3_PHY_BASE+0x894},  //GPIOD---SET,CLEAR	
	{HW_GPIO4_PHY_BASE+0x87C,HW_GPIO4_PHY_BASE+0x880},  //GPIOE---SET,CLEAR
};


#define GPIO_PORT_TO_BASE(port)   GPIO_BASE_ADDR[port]

void s_PiccTypeInit(void);
void s_PhysicalPiccLightSet(uchar ucLedIndex,uchar ucOnOff);
extern void s_VirtualPiccLight(uchar led, uchar on);

typedef struct
{
	int	red_port;
	int red_subno;
	int	green_port;
	int green_subno;
	int	blue_port;
	int blue_subno;
	int	yellow_port;
	int yellow_subno;
	void (*PiccLightSet)(uchar,uchar);
}T_PiccLightCfg;

T_PiccLightCfg S500PiccLightCfg = {
	.red_port = GPIOC,
	.red_subno = 20,
	.green_port = GPIOC,
	.green_subno = 21,
	.blue_port = GPIOC,
	.blue_subno = 22,
	.yellow_port = GPIOC,
	.yellow_subno = 29,
	.PiccLightSet = s_PhysicalPiccLightSet,
};

T_PiccLightCfg S900PiccLightCfg = {
	.red_port = GPIOD,
	.red_subno = 5,
	.green_port = GPIOB,
	.green_subno = 10,
	.blue_port = GPIOB,
	.blue_subno = 5,
	.yellow_port = GPIOD,
	.yellow_subno = 4,
	.PiccLightSet = s_PhysicalPiccLightSet,
};

T_PiccLightCfg VirtualPiccLightCfg = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	.PiccLightSet = s_VirtualPiccLight,
};
T_PiccLightCfg *SxxxPiccLight = NULL;

static void IsrPort()
{
	int i;
	uint IAST_REG;
	IAST_REG=readl(GIO1_R_GRPF1_INT_MSTAT_MEMADDR);
//printk("IAST_REG:%08X!\r\n",g_IAST_REG);
	for(i=0;i<HW_GPIOB_NUM_PIN;i++)
	{
		if(!(IAST_REG&(1<<i))) continue;
//printk("isr port:%d!\r\n",i);		
		if(g_Port_func[i])
		{
			g_Port_func[i]();
			writel((1<<i),GIO1_R_GRPF1_INT_CLR_MEMADDR);//清中断标志
		}
	}

}

static int gpio_to_irq(int pin)
{
	if ((pin < 0) || (pin >= HW_GPIOB_NUM_PIN)) return -1;

	if (pin ==  0)     return IRQ_EXT_GRP0;
	if (pin == 1)  return IRQ_EXT_GRP1;
	if (pin == 2)  return IRQ_EXT_GRP2;
	if (pin == 3)  return IRQ_EXT_GRP3;
	if (pin >= 24) return IRQ_EXT_GRP7; /* gpio 24-31 */
	if (pin >= 16) return IRQ_EXT_GRP6; /* gpio 16-23 */
	if (pin >=  8) return IRQ_EXT_GRP5; /* gpio 08-15 */
	if (pin >=  4) return IRQ_EXT_GRP4; /* gpio 04-07 */

	return -2;
}

/**************************************************************
SubNo    	:	GPIOB 端口的子通道号
TrigType 	:	触发类型
Handler  	:	中断向量,为NULL即取消注册
***************************************************************/
int s_setShareIRQHandler(int port,uint SubNo,int TrigType,void(*Handler)(void) )
{
	int intnum;
	uint type,edge,mask,enable;
	uint flag,value;

	if(SubNo>= HW_GPIOB_NUM_PIN) return -1;
	irq_save(flag);
	type= readl(GIO1_R_GRPF1_INT_TYPE_MEMADDR);
	edge= readl(GIO1_R_GRPF1_INT_EDGE_MEMADDR);
	mask=readl(GIO1_R_GRPF1_INT_MSK_MEMADDR);
	enable=readl(GIO1_R_GRPF1_OUT_ENABLE_MEMADDR);
	type &= ~(1<<SubNo);
	edge &= ~(1<<SubNo);
	mask |= (1<<SubNo);     //enable gpio irq
	enable &= ~(1<<SubNo);

	switch(TrigType)
	{
		case INTTYPE_LOWLEVEL:
			type |= (1<<SubNo);
			break;
		case INTTYPE_HIGHLEVEL:
			type |= (1<<SubNo);
			edge |= (1<<SubNo);
			break;
		case INTTYPE_FALL:
			break;
		case INTTYPE_RISE:
			edge |= (1<<SubNo);
			break;
	}

	g_Port_func[SubNo]=Handler;
	writel(type,GIO1_R_GRPF1_INT_TYPE_MEMADDR);
	writel(edge,GIO1_R_GRPF1_INT_EDGE_MEMADDR);	
	writel(mask,GIO1_R_GRPF1_INT_MSK_MEMADDR);
	writel(enable,GIO1_R_GRPF1_OUT_ENABLE_MEMADDR);
	irq_restore(flag);
	
	intnum =gpio_to_irq(SubNo);
//	printk("setShareIRQHandler type:%08X,edge:%08X,mask:%08X,enable:%08X,intnum:%d!\r\n"
//		,type,edge,mask,enable,intnum);
	enable_irq(intnum);

	return 0;	
}

void s_GPIOInit(void)
{
	uchar i=0;
	int m_type;

	writel(0x00,GIO1_R_GRPF1_INT_MSK_MEMADDR);
	for(i=0;i<HW_GPIOB_NUM_PIN;i++)
	{	
		g_Port_func[i]=NULL;
	}
	for(i=IRQ_EXT_GRP0;i<=IRQ_EXT_GRP7;i++)
	{
		s_SetIRQHandler(i, IsrPort);
		s_SetInterruptMode(i, INTC_PRIO_10, INTC_IRQ);
	}
	m_type = get_machine_type();
	gpio_set_pin_type(GPIOB,3,GPIO_INPUT);  /*config battery check port to input port*/
	if(m_type == D210) //D210 V15 之后的版本不可以上下拉。
	{
		gpio_disable_pull(GPIOB, 3);
	}
	if (m_type != D210)
	{
    	gpio_set_pin_type(GPIOB,11,GPIO_INPUT); 
	}
	gpio_set_pull(GPIOB, 4, 0);   /*WLAN_CTS_DTR*/
	if ( m_type == D200)
	{
		gpio_set_pin_type(GPIOA, 20, GPIO_OUTPUT);
		gpio_set_pull(GPIOA, 20, 1);
		gpio_set_pin_val(GPIOA, 20, 1);
	}

	if(GetHWBranch() == D210HW_V2x)		// 底座检测脚
	{
		gpio_set_pin_type(GPIOB, 7, GPIO_INPUT);
		gpio_set_pull(GPIOB, 7, 1);
	}

	s_PiccTypeInit();			// 按键灯初始化	
}

/*-------------------------------------------------------------
int gpio_get_pin_type(int port, int subno)
功能 : 获取单个GPIO口线的类型
输入参数:
port   		:   	GPIO的端口号
subno  		:   	对应GPIO子口线的编号
返回值	:	类型值
----------------------------------------------------------------*/
int gpio_get_pin_type(int port, int subno)
{
	uint  nBitMask;
	uint base ;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	base = GPIO_PORT_TO_BASE(port);
	if (readl(base+GPIO_AUX_SEL) & nBitMask)
	{
		/* AUX bit set, now check AUX01 bit */
		if (readl(base+GPIO_AUX_SEL) & nBitMask) return GPIO_FUNC1;
		else	return GPIO_FUNC0;
	}
	else
	{
		/* Not AUX, now check Input/Output */
		if (readl(base+GPIO_EN) & nBitMask)	return GPIO_OUTPUT;
		else	return GPIO_INPUT; 
	}

}

/*-------------------------------------------------------------
void gpio_set_pin_type(int port, int subno, GPIO_PIN_TYPE pinType )
功能 : 设置单个GPIO口线的类型
输入参数:
port   	:   GPIO的端口号
subno  	:   对应GPIO子口线的编号
pinType  	:   待设置口线的类型，可参考GPIO_PIN_TYPE
----------------------------------------------------------------*/
void gpio_set_pin_type(int port, int subno, GPIO_PIN_TYPE pinType )
{
	uint  nBitMask,base,flag;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	base = GPIO_PORT_TO_BASE(port);
	irq_save(flag);
	switch(pinType)
	{
		case GPIO_INPUT:
		case GPIO_OUTPUT:
			/* Clear AUX bit */
			reg32(base+GPIO_AUX_SEL) &= ~nBitMask;
			/* Set input/output */
			if (pinType == GPIO_OUTPUT) reg32(base+GPIO_EN) |= nBitMask;
			else    reg32(base+GPIO_EN) &= ~nBitMask;
		break;

		case GPIO_FUNC0:
			/* Set AUX bit */
			reg32(base+GPIO_AUX_SEL) |= nBitMask;
			/* Clear AUX01 bit */
			reg32(base+GPIO_AUX01_SEL) &= ~nBitMask;
		break;

		case GPIO_FUNC1:
			/* Set AUX bit */
			reg32(base+GPIO_AUX_SEL) |= nBitMask;
			/* Set AUX01 bit */
			reg32(base+GPIO_AUX01_SEL) |= nBitMask;
		break;

		case GPIO_FUNC_DIS:
			/* Clear AUX bit */
			reg32(base+GPIO_AUX_SEL) &= ~nBitMask;
		break;
	}
	irq_restore(flag);
}

void gpio_set_mpin_type(int port, uint Mask, GPIO_PIN_TYPE pinType )
{
	uint  nBitMask,base,flag;
	if (port >ARCH_GPIO_PORT) return;
	base = GPIO_PORT_TO_BASE(port);
	irq_save(flag);
	switch(pinType)
	{
		case GPIO_INPUT:
		case GPIO_OUTPUT:
			/* Clear AUX bit */
			reg32(base+GPIO_AUX_SEL) &= ~Mask;
			/* Set input/output */
			if (pinType == GPIO_OUTPUT) reg32(base+GPIO_EN) |= Mask;
			else    reg32(base+GPIO_EN) &= ~Mask;
		break;

		case GPIO_FUNC0:
			/* Set AUX bit */
			reg32(base+GPIO_AUX_SEL) |= Mask;
			/* Clear AUX01 bit */
			reg32(base+GPIO_AUX01_SEL) &= ~Mask;
		break;

		case GPIO_FUNC1:
			/* Set AUX bit */
			reg32(base+GPIO_AUX_SEL) |= Mask;
			/* Set AUX01 bit */
			reg32(base+GPIO_AUX01_SEL) |= Mask;
		break;

		case GPIO_FUNC_DIS:
			/* Clear AUX bit */
			reg32(base+GPIO_AUX_SEL) &= ~Mask;
		break;
	}
	irq_restore(flag);
}
/*---------------------------------------------
void gpio_set_pin_val(int port, int subno, int val )
功能 : 设置单个GPIO口线的值
输入参数:
port   	:   GPIO的端口号
subno  	:   对应GPIO子口线的编号
val  		:   待设置口线的值
------------------------------------------------*/
void gpio_set_pin_val(int port, int subno, int val )
{
	uint  nBitMask,base;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	if ( val) writel(nBitMask,GPIO_OUT_OP[port][GPIO_OUT_SET]);  /* Set the pin to 1 */
	else 	 writel(nBitMask,GPIO_OUT_OP[port][GPIO_OUT_CLEAR]); /* Set the pin to 0 */
}

/*---------------------------------------------
void gpio_set_mpin_val(int port, uint mask, uint bitval )
功能 : 设置同一组内多个GPIO口线的值
输入参数:
port   	:   GPIO的端口号
mask  	:   对应GPIO子口线的位掩码
bitval  	:   需要设置的对应口线的值
------------------------------------------------*/
void gpio_set_mpin_val(int port, uint mask, uint bitval )
{
	uint  base;
	uint  temp;
	if (port >ARCH_GPIO_PORT) return;
	writel((bitval & mask),GPIO_OUT_OP[port][GPIO_OUT_SET]) ;  /* Set the pins to 1 */
	writel((~bitval & mask),GPIO_OUT_OP[port][GPIO_OUT_CLEAR]);	  /* Set the pins to 0 */
}

/*-------------------------------------------------------------
uint gpio_get_pin_val_output(int port, int subno )
功能 : 获取单个GPIO口线输出的值
输入参数:
port   		:   	GPIO的端口号
subno  		:   	对应GPIO子口线的编号
返回值	:	输出值
----------------------------------------------------------------*/
uint gpio_get_pin_val_output(int port, int subno )
{
	uint  nBitMask,base;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	base = GPIO_PORT_TO_BASE(port);

	return ( reg32(base+GPIO_DOUT) & nBitMask ) != 0;
}

/*-------------------------------------------------------------
uint gpio_get_pin_val_output(int port, int subno )
功能 : 获取单个GPIO口线输入的值
输入参数:
port   		:   	GPIO的端口号
subno  		:   	对应GPIO子口线的编号
返回值	:	输入值
----------------------------------------------------------------*/
uint gpio_get_pin_val(int port, int subno)
{
	uint  nBitMask,base;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	base = GPIO_PORT_TO_BASE(port);

	return ( readl(base+ GPIO_IOTR) & nBitMask ) != 0;
}

uint gpio_get_mpin_val(int port, uint mask)
{
	uint  base;
	if (port >ARCH_GPIO_PORT) return;
	base = GPIO_PORT_TO_BASE(port);

	return readl(base+ GPIO_IOTR) & mask ;
}

/*---------------------------------------------
void gpio_set_pin_interrupt(uint subno, int enable )
功能 : 设置单个GPIO口线中断的关闭和使能
输入参数:
port		:	GPIO的端口号
subno   	:   	GPIOB的子口线号
enable  	:   	1--打开中断，0--关闭中断
------------------------------------------------*/
void gpio_set_pin_interrupt(int port,uint subno, int enable)
{
	uint mask,flag;
	if(subno >31) return;
	irq_save(flag);
	mask = readl(GIO1_R_GRPF1_INT_MSK_MEMADDR);
	if(enable) mask |= (1<<subno);
	else		 mask &= ~(1<<subno);	
	writel(mask,GIO1_R_GRPF1_INT_MSK_MEMADDR);
	irq_restore(flag);
}

/*---------------------------------------------
void gpio_set_pull(int port, int subno, int val )
功能 : 设置单个GPIO口线的上下拉功能
输入参数:
port   	:   GPIO的端口号
subno  	:   对应GPIO子口线的编号
val  		:   1为上拉；0为下拉
------------------------------------------------*/
void gpio_set_pull(int port, int subno, int val )
{
	uint  nBitMask,base,flag;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	base = GPIO_PORT_TO_BASE(port);
	irq_save(flag);
	if( val ) reg32(base+GPIO_PAD_RES) |= nBitMask; /* Set pull up */
	else	reg32(base+GPIO_PAD_RES) &= ~nBitMask; 	/* Set pull down */
	irq_restore(flag);
}

/*---------------------------------------------
void gpio_enable_pull(int port, int subno)
功能 : 使能单个GPIO口线的上下拉功能
输入参数:
port   	:   GPIO的端口号
subno  	:   对应GPIO子口线的编号
------------------------------------------------*/
void gpio_enable_pull(int port, int subno)
{
	uint  nBitMask,base,flag;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	base = GPIO_PORT_TO_BASE(port);
	irq_save(flag);
	reg32(base+GPIO_RES_EN) |= nBitMask; /* Enable PAD resister */
	irq_restore(flag);
}
/*---------------------------------------------
void gpio_disable_pull(int port, int subno)
功能 : 关闭单个GPIO口线的上下拉功能
输入参数:
port   	:   GPIO的端口号
subno  	:   对应GPIO子口线的编号
------------------------------------------------*/
void gpio_disable_pull(int port, int subno)
{
	uint  nBitMask,base,flag;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	base = GPIO_PORT_TO_BASE(port);

	irq_save(flag);
	reg32(base+GPIO_RES_EN) &= ~nBitMask; /* Enable PAD resister */
	irq_restore(flag);
}

/*---------------------------------------------
void gpio_set_pull_updown(int port, int subno, int val )
功能 : 设置单个GPIO口线的驱动电流值
输入参数:
port   	:   GPIO的端口号
subno  	:   对应GPIO子口线的编号
val  		:   4mA;6mA;8mA,可参考相应的宏值
------------------------------------------------*/
void gpio_set_drv_strength(int port, int subno, GPIO_DRV_STRENGTH val )
{
	uint  nBitMask,base,flag;
	if (port >ARCH_GPIO_PORT) return;
	nBitMask= (1 << subno); /* this is not for pin number indicates AUX since that will be too big */
	base = GPIO_PORT_TO_BASE(port);

	irq_save(flag);
	switch(val) 
	{
		case GPIO_DRV_STRENGTH_4mA:
	        	reg32(base+GPIO_DRV_SEL0_SW) &= ~nBitMask; /* 0 */
	        	reg32(base+GPIO_DRV_SEL1_SW) |= nBitMask; /* 1  */
	        	reg32(base+GPIO_DRV_SEL2_SW) &= ~nBitMask; /* 0 */
			break;
		case GPIO_DRV_STRENGTH_6mA:
	        	reg32(base+GPIO_DRV_SEL0_SW) &= ~nBitMask; /* 0 */
	        	reg32(base+GPIO_DRV_SEL1_SW) &= ~nBitMask; /* 0 */
	        	reg32(base+GPIO_DRV_SEL2_SW) |= nBitMask; /* 1  */
			break;
		case GPIO_DRV_STRENGTH_8mA:
	        	reg32(base+GPIO_DRV_SEL0_SW) |= nBitMask; /* 1 */
	        	reg32(base+GPIO_DRV_SEL1_SW) &= ~nBitMask; /* 0 */
	        	reg32(base+GPIO_DRV_SEL2_SW) |= nBitMask; /* 1  */
			break;
	}
	irq_restore(flag);
} 

#define	PICC_LED_RED_INDEX			(1<<0)
#define	PICC_LED_GREEN_INDEX		(1<<1)
#define	PICC_LED_YELLOW_INDEX		(1<<2)
#define	PICC_LED_BLUE_INDEX			(1<<3)

void s_PhysicalPiccLightSet(uchar ucLedIndex,uchar ucOnOff)
{
	if (ucLedIndex & PICC_LED_RED_INDEX)
	{
		if (ucOnOff) gpio_set_pin_val(SxxxPiccLight->red_port,SxxxPiccLight->red_subno,0);
		else	gpio_set_pin_val(SxxxPiccLight->red_port,SxxxPiccLight->red_subno,1);
	}

	if (ucLedIndex & PICC_LED_GREEN_INDEX)
	{
		if (ucOnOff) gpio_set_pin_val(SxxxPiccLight->green_port,SxxxPiccLight->green_subno,0);
		else	gpio_set_pin_val(SxxxPiccLight->green_port,SxxxPiccLight->green_subno,1);
	}


	if (ucLedIndex & PICC_LED_BLUE_INDEX)
	{
		if (ucOnOff) gpio_set_pin_val(SxxxPiccLight->blue_port,SxxxPiccLight->blue_subno,0);
		else	gpio_set_pin_val(SxxxPiccLight->blue_port,SxxxPiccLight->blue_subno,1);
	}

	if (ucLedIndex & PICC_LED_YELLOW_INDEX)
	{
		if (ucOnOff) gpio_set_pin_val(SxxxPiccLight->yellow_port,SxxxPiccLight->yellow_subno,0);
		else	gpio_set_pin_val(SxxxPiccLight->yellow_port,SxxxPiccLight->yellow_subno,1);
	}
}

// 非接灯初始化，默认为虚拟非接灯
void s_PiccTypeInit(void)
{
	int RfStyle;

	RfStyle = GetTerminalRfStyle();

	if(RfStyle > 0)
	{
		if(RfStyle == 1)		// S500-R
		{
			SxxxPiccLight = &S500PiccLightCfg;
		}
		else if(RfStyle == 2)
		{
			SxxxPiccLight = &S900PiccLightCfg;
		}
		else
		{
			SxxxPiccLight = NULL;
		}

		if(SxxxPiccLight)
		{
			gpio_set_pin_type(SxxxPiccLight->red_port,SxxxPiccLight->red_subno,GPIO_OUTPUT);
			gpio_set_pin_type(SxxxPiccLight->green_port,SxxxPiccLight->green_subno,GPIO_OUTPUT);
			gpio_set_pin_type(SxxxPiccLight->blue_port,SxxxPiccLight->blue_subno,GPIO_OUTPUT);
			gpio_set_pin_type(SxxxPiccLight->yellow_port,SxxxPiccLight->yellow_subno,GPIO_OUTPUT);

			gpio_set_pin_val(SxxxPiccLight->red_port,SxxxPiccLight->red_subno,1);
			gpio_set_pin_val(SxxxPiccLight->green_port,SxxxPiccLight->green_subno,1);
			gpio_set_pin_val(SxxxPiccLight->blue_port,SxxxPiccLight->blue_subno,1);
			gpio_set_pin_val(SxxxPiccLight->yellow_port,SxxxPiccLight->yellow_subno,1);
		}
	}
	else
	{
		SxxxPiccLight = &VirtualPiccLightCfg;
	}	
}

void PiccLight(uchar ucLedIndex,uchar ucOnOff)
{
	SxxxPiccLight->PiccLightSet(ucLedIndex,ucOnOff);
}

static uint s_gpio_regs[5][4];
void gpio_save_regs()
{
    uint port,base;

    for(port=0;port<5;port++)
    {
        base = GPIO_PORT_TO_BASE(port);
        s_gpio_regs[port][0]=reg32(base+GPIO_AUX_SEL);
        s_gpio_regs[port][1]=reg32(base+GPIO_EN);
        s_gpio_regs[port][2]=reg32(base+GPIO_AUX01_SEL);
        s_gpio_regs[port][3]=reg32(base+GPIO_DOUT);
    }
}

void gpio_restore_regs()
{
    uint port,base;

    for(port=0;port<5;port++)
    {
        base = GPIO_PORT_TO_BASE(port);
        reg32(base+GPIO_AUX_SEL)= s_gpio_regs[port][0];
        reg32(base+GPIO_EN)=s_gpio_regs[port][1];
        reg32(base+GPIO_AUX01_SEL)=s_gpio_regs[port][2];
        reg32(base+GPIO_DOUT)=s_gpio_regs[port][3];
    }

}



#if 0
typedef enum /* It's ok to have defines have same value */
{
	GPIO_AUX_TS         , /* Group 0, subgroup 0 */
	GPIO_AUX_SPI0       , /* Group 0, subgroup 1 */
	GPIO_AUX_SPI1       , /* Group 0, subgroup 2 */
	GPIO_AUX_SCI0       , /* Group 0, subgroup 3 */
	GPIO_AUX_I2C0       , /* Group 0, subgroup 3, AUX=1 */
	GPIO_AUX_UART0      , /* Group 0, subgroup 4 */

	GPIO_AUX_EINT0      , /* Group 1, subgroup 0 */
	GPIO_AUX_EINT1      , /* Group 1, subgroup 1 */
	GPIO_AUX_EINT2      , /* Group 1, subgroup 2 */
	GPIO_AUX_EINT3      , /* Group 1, subgroup 3 */
	GPIO_AUX_EINT4_7    , /* Group 1, subgroup 4 */
	GPIO_AUX_EINT8_15   , /* Group 1, subgroup 5 */
	GPIO_AUX_EINT16_19  , /* Group 1, subgroup 6 */
	GPIO_AUX_EINT20_23  , /* Group 1, subgroup 7 */

	GPIO_AUX_SMC        , /* Group 2, subgroup 0,   AUX=1 */
	GPIO_AUX_SPI3       , /* Group 2, subgroup 1,   AUX=1 */
	GPIO_AUX_LCD        , /* Group 2, subgroup 0&1, AUX=0 */

	GPIO_AUX_PWM        , /* Group 3, subgroup 0,   AUX=0 */
	GPIO_AUX_LED        , /* Group 3, subgroup 0,   AUX=1 */
	GPIO_AUX_I2C1       , /* Group 3, subgroup 1 */
	GPIO_AUX_I2S        , /* Group 3, subgroup 2 */
	GPIO_AUX_UART2  , /* Group 3, subgroup 3&4, AUX=0 */
	GPIO_AUX_IRDA       , /* Group 3, subgroup 3,   AUX=1 */
	GPIO_AUX_UART3      , /* Group 3, subgroup 5,   AUX=0 */
	GPIO_AUX_D1W        , /* Group 3, subgroup 5,   AUX=1 */

	GPIO_AUX_PRINTER    , /* Group 4, subgroup 0&1 */
	GPIO_AUX_SPI4       , /* Group 4, subgroup 0 */
} GPIO_SUBGROUP_TYPE;
#endif

