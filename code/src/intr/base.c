#include "posapi.h"
#include "base.h"
#include "../lcd/lcdapi.h"
#include "../kb/kb.h"

typedef void 	ISR_FUNC( int id );
typedef void 	SOFTINT_FUNC(void);
ISR_FUNC *IsrTable[NR_IRQS];
SOFTINT_FUNC *Soft_int_Table[NR_IRQS];
void soft_interrupt(void);

volatile uint abort_lr=0;
volatile uint cur_cpsr=0;
volatile uint cur_spsr=0;
volatile uint bad_pc = 0;
volatile uint cur_sp = 0;
volatile uint IFSR,DFSR;
volatile uint g_interrupt_addr;


void DefaultHandler(int id);

void disable_irq(uint irq)
{
	if(irq<OVIC0_IRQ_START)
	{
		irq -= SVIC_IRQ_START;
		*(volatile uint*)(VIC0_REG_BASE_ADDR + VIC_INTENCLEAR) =1 <<irq;
	}
	else if(irq <OVIC1_IRQ_START)
	{
		irq -= OVIC0_IRQ_START;
		*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTENCLEAR) =1 <<irq;
	}
	else 
	{
		irq -= OVIC1_IRQ_START;
		*(volatile uint*)(VIC2_REG_BASE_ADDR + VIC_INTENCLEAR) =1 <<irq;
	}
}

void enable_irq(uint irq)
{
	if(irq<OVIC0_IRQ_START)
	{
		irq -= SVIC_IRQ_START;
		*(volatile uint*)(VIC0_REG_BASE_ADDR + VIC_INTENABLE) =1 <<irq;
	}
	else if(irq <OVIC1_IRQ_START)
	{
		irq -= OVIC0_IRQ_START;
		*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTENABLE) =1 <<irq;
	}
	else 
	{
		irq -= OVIC1_IRQ_START;
		*(volatile uint*)(VIC2_REG_BASE_ADDR + VIC_INTENABLE) =1 <<irq;
	}
	
}

void s_SetInterruptMode(uint IntNo, uchar Priority, uchar INT_Mode)
{
	uint irq;
	uchar mode;
	irq= IntNo;
	mode =INT_Mode & 0x01;
	
	if(irq<OVIC0_IRQ_START)
	{
		irq -= SVIC_IRQ_START;
		if(INTC_IRQ == mode)
			*(volatile uint*)(VIC0_REG_BASE_ADDR + VIC_INTSELECT) &= ~(1 <<irq);
		else
			*(volatile uint*)(VIC0_REG_BASE_ADDR + VIC_INTSELECT) = 1 <<irq;
			
		*(volatile uint*)(VIC0_R_VICVECTPRIORITY0_MEMADDR+irq*4) = Priority & 0x0F;
	}
	else if(irq <OVIC1_IRQ_START)
	{
		irq -= OVIC0_IRQ_START;
		if(INTC_IRQ == mode)
			*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTSELECT) &= ~(1 <<irq);
		else
			*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTSELECT) = 1 <<irq;
		*(volatile uint*)(VIC1_R_VICVECTPRIORITY0_MEMADDR+irq*4) = Priority & 0x0F;
		//printk("INTSELECT:%08X!\r\n",*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTSELECT));
	}
	else 
	{
		irq -= OVIC1_IRQ_START;
		if(INTC_IRQ == mode)
			*(volatile uint*)(VIC2_REG_BASE_ADDR + VIC_INTSELECT) &= ~(1 <<irq);
		else
			*(volatile uint*)(VIC2_REG_BASE_ADDR + VIC_INTSELECT) = 1 <<irq;
		
		*(volatile uint*)(VIC2_R_VICVECTPRIORITY0_MEMADDR+irq*4) = Priority & 0x0F;
	}

}

void s_SetIRQHandler(uint IntNo, void (*Handler)(int id))
{
	IsrTable[IntNo] = Handler;
	//printk("intNo:%d,handler:%08X!\r\n",IntNo,Handler);
}


void s_SetFIQHandler(uint IntNo, void (*Handler)(int id))
{
	s_SetInterruptMode(IntNo, INTC_PRIO_0, INTC_FIQ);
	IsrTable[IntNo] = Handler;
}

void s_SetSoftInterrupt(uint IntNo, void (*Handler)(void))
{
	Soft_int_Table[IntNo] = Handler;
}

uint  Swap_value(uint value,uint *dst)
{
    __asm("swp r0,r0,[r1]");
}  

void BSP_INT_Init(void)
{
	volatile unsigned int i = 0;
    // 关闭IRQ/FIQ使能
    __asm( "MRS		R1, CPSR" );
    __asm( "ORR		R0, R1, #0xC0" );
    __asm( "MSR 	CPSR_c, R0" );
	/* Set all interrupts for irq*/
	*(volatile uint*)(VIC0_REG_BASE_ADDR + VIC_INTSELECT) =0;
	*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTSELECT) =0;
	*(volatile uint*)(VIC2_REG_BASE_ADDR + VIC_INTSELECT) =0;

	/* Disable  all interrupts */
	*(volatile uint*)(VIC0_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;
	*(volatile uint*)(VIC1_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;
	*(volatile uint*)(VIC2_REG_BASE_ADDR + VIC_INTENCLEAR) = 0xFFFFFFFF;

	for (i = 0; i < NR_IRQS; i++) 
	{
		IsrTable[i] =DefaultHandler;
		Soft_int_Table[i] =NULL;
	}	
	/* RNG interrupt is cleared, noisy it is.*/
	*(volatile uint*)(RNG_R_RNG_CTRL_MEMADDR) =0x00; 
	*(volatile uint*)(RNG_R_RNG_INT_MASK_MEMADDR) =0x01;
	*(volatile uint*)(RNG_R_RNG_CTRL_MEMADDR) =0x01;
}

void s_InitINT(void)
{
    // 打开IRQ/FIQ使能
    __asm( "MRS		R1, CPSR" );
    __asm( "BIC		R0, R1, #0xC0" );
    __asm( "MSR 	CPSR_c, R0" );
    OsCreate(soft_interrupt,TASK_PRIOR_Firmware0,0x3000);
}


//关闭打印机
static void ShutdownPrnPower(void)
{
	s_PrnStop();
}

void DefaultHandler(int id)
{
	//关闭打印机电源
	ShutdownPrnPower();
	//关闭蜂鸣器声音
	s_StopBeep();
	ScrPrint(0,2,0,"INVALID IRQ/FIQ INTERRUPT!\n");
    while(1);
}

void AbortHandler(void)
{
	__asm("LDR 		r0, =cur_cpsr");
	__asm("MRS		r1, CPSR");
	__asm("STR 		r1, [r0]");

	__asm("ORR 		R1,R1, #0xC0"); //关中断
	__asm("MSR 		CPSR_c, r1");

	__asm("LDR 		r0, =abort_lr");
	__asm("MOV		r1, LR");
	__asm("STR 		r1, [r0]");
	__asm("LDR 		r0, =cur_spsr");
	__asm("MRS		r1, SPSR");
	__asm("STR 		r1, [r0]");
	__asm("LDR 		r0, =bad_pc");
	__asm("MOV		r1, PC");
	__asm("STR 		r1, [r0]");
	__asm("LDR 		r0, =cur_sp");
	__asm("MOV		r1, SP");
	__asm("STR 		r1, [r0]");

	__asm("LDR 		r0, =IFSR");
	__asm("MRC p15, 0, R1, c5, c0, 1");
	__asm("STR 		r1, [r0]");

	__asm("LDR 		r0, =DFSR");
	__asm("MRC p15, 0, R1, c5, c0, 0");
	__asm("STR 		r1, [r0]");

	//关闭打印机电源
	ShutdownPrnPower();
	//关闭蜂鸣器声音
	s_StopBeep();
	CLcdSetFgColor(COLOR_BLACK);
	CLcdSetBgColor(COLOR_WHITE);

	ScrCls();

	SCR_PRINT(0,0,0x01,"     警 告      ", "    WARNING!\n");
	SCR_PRINT(0,2,0X41,"程序出错了","APP EXCEPTION");//居中显示

	ScrPrint(0,4,0x00,"DFSR:%08x",DFSR);//靠右显示，ARM打印LR值 ;MIPS 打印EPC值
	ScrPrint(0,5,0x00,"IFSR:%08x",IFSR);//靠右显示，ARM打印LR值 ;MIPS 打印EPC值
	ScrPrint(0,6,0x00,"%08x",abort_lr);//靠右显示，ARM打印LR值 ;MIPS 打印EPC值
	ScrPrint(0,7,0x00,"%08x,%08x",cur_spsr,cur_cpsr);//靠右显示ARM 打印SP_SR值  ;MIPS打印RA值

	if (get_machine_type() == S300 || get_machine_type() == S800 
			|| get_machine_type() == S900 || get_machine_type() == D210)
	{
	    //S300,S800,S900长按取消键关机
		gpio_set_pin_type(POWER_KEY,GPIO_INPUT);
	    gpio_set_pull(POWER_KEY,1);
		gpio_enable_pull(POWER_KEY);

		gpio_set_mpin_type(GPIOB,0x000F8000,GPIO_INPUT);
	    gpio_set_pin_type(GPIOB, 19, GPIO_OUTPUT);
		gpio_set_pin_val(GPIOB, 19, 0);
	        
		while(1)
		{
			if (!gpio_get_pin_val(POWER_KEY))
			{
				DelayMs(1000);
				if (!gpio_get_pin_val(POWER_KEY)) 
					s_AbortPowerOff();
			}
		}
	}
	else
	{
		while(1)
		{
			if (!gpio_get_pin_val(POWER_KEY))
			{
				DelayMs(1000);
				if (!gpio_get_pin_val(POWER_KEY)) 
					PowerOff();
			}
		}
	}

//	ExceptionHandler();
}

void soft_interrupt(void)
{
	int i;
    while(1)
    {
    	for(i=0;i<NR_IRQS;i++)
    	{
    		if(Soft_int_Table[i] ==NULL) continue;
    		Soft_int_Table[i]();
    	}
        OsSleep(1);
    }
}

void set_pte(uint virtual_addr,uint phy_addr,uint len)
{
	uint addr,offset,pte;
	if(virtual_addr <DRIVER_START) return;
	if((virtual_addr +len) >=DRIVER_END) return;

	pte=phy_addr/0x1000;
	for(addr=virtual_addr;addr <(virtual_addr+len);addr+=0x1000)
	{
		offset=((addr-DRIVER_START)/0x1000)*4;
		/*enable I/D cache,Domain15,4K page,AP_RW_NA*/
		*(volatile uint *)(DRIVER_L2_BASE+offset) = (pte<<12) |(0x55E); 
		pte++;
	}
	
	/*clear and invaild TLB */
	__asm("mov r0 ,#0");
	__asm("mcr p15 ,0,r0,c8,c7,0");
}

/***************************************************
	clear L1 vector table and L2 page table base addr context
****************************************************/

void ClearTTB()
{
	memset((uchar*)DRIVER_L2_BASE,0,0x20000);
}


int entry_irq(int id,int xx,int handler)
{
	static int irq_cnt=0;
	//printk("entry irq:%d,id:%d,handler:%08X!\r\n",irq_cnt++,id,handler);
	return id;
}



