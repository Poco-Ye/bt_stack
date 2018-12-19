/*  BCM5892 platform header file. */
#ifndef __ASM_ARCH_PLATFORM_H
#define __ASM_ARCH_PLATFORM_H


//#define  SDRAM_SIZE  0x4000000
//#define  FLASH_SIZE   0x8000000
//#define  BPK_SIZE	0x800
#define  BPK_SIZE	0x20

/* Memory definitions */
#define BCM5892_DRAM_BASE	0x40000000
#define BCM5892_DRAM_SIZE	0x4000000
#define BCM5892_SCRATCH_BASE	START_SCRATCH
#define BCM5892_SCRATCH_SIZE	0x40000
#define BCM5892_SRAM_BASE	START_SCRATCH
#define BCM5892_PERIPH_BASE	0x00080000
#define BCM5892_DMA_BASE	0x40200000
#define DMA_USB_BASE        BCM5892_DMA_BASE
#define DMA_USB_LEN         0x10000
#define DMA_ETH_BASE        (DMA_USB_BASE + DMA_USB_LEN) /* NOTE: MUST ALIGNED WITH 8 BYTES!!! */
#define DMA_ETH_LEN         0x60000
#define DMA_LCD_BASE        (DMA_ETH_BASE + DMA_ETH_LEN)
#define DMA_LCD_LEN         (320*240*2)
#define DMA_SDIO_BASE       (DMA_LCD_BASE + DMA_LCD_LEN + 1024)
#define DMA_SDIO_LEN        (64 * 1024)
#define DMA_RESERVE_BASE    (DMA_SDIO_BASE + DMA_SDIO_LEN)



/*--------Memory 区域分配信息*/
/*********************************   DDR 分配********************************
---------系统空间-----------------------------------------------------------------------------------------
0x00000000-0x00000200     Vector区，只有512Bytes，物理与逻辑地址相同
0x401FC000-0x40200000     系统一级页表空间，物理与逻辑地址相同,映射4G
0x401E0000-0x401FC000     系统二级级页表空间，物理与逻辑地址相同，可映射112M
0x40200000-0x40300000     DMA空间，内存空间，预留1M，物理与逻辑地址相同
0x00180000-0x001BFFFF     DMA空间，256KB，物理与逻辑地址相同
0x1EE00000-0x1EFE0000     系统栈空间，物理内存0x40000000-0x401E0000

0x20000000-0x21000000    Monitor  ，物理内存0x4030000-0x41000000，共13M
0x21000000-0x24000000    Driver 空间共48M，物理地址复用Monitor，每个模块2M线性地址，采用4K的页表。
--------应用空间-----------------------------------------------------------------------------------------
0x28000000-0x2B000000    Application  物理内存0x41000000-0x43000000
0x28000000-0x28200000   主应用,物理内存为0x41000000-0x41200000，物理内存2M
0x28200000-0x2A000000   子应用，物理内存为0x41200000-0x43000000，物理内存30M
0x2F000000-0x30000000   Malloc区及栈区，前12M为Malloc，后4M为栈区。物理内存0x41000000-0x43000000
************************************************************************************
/*************************************FLASH 分配**********************************
System Flash space address is 8MB,详细分配信息参照Nand.h
Offset 0~0x20000  				128KB  Boot 空间
Offset 0x20000~0x800000          	配置扇区128KB*2 需要进行主备备份，
								每次存储的最小单位为2KB。	
Offset 0x80000~0x400000			3.5MB
Offset 0x800000~0x8000000              文件系统120MB
************************************************************************************/
//#define MONITOR_STORE_ADDR 0x80000

/*  Interrupt Controller */
#define VIC_IRQSTATUS		0x00
#define VIC_RAWINTR			0x008
#define VIC_INTSELECT		0x00c
#define VIC_INTENABLE		0x010
#define VIC_INTENCLEAR		0x014
#define VIC_SOFTINTCLEAR    0x01c

/*  System Timer */
#define TICKS_PER_mSEC		(6000)	/* REFCLK / 4 *1000*/
#define TICKS_PER_uSEC		(6)	/* REFCLK / 4 *1000*/


/* Watchdog offsets */
#define WDOG_LOAD	0x00
#define WDOG_VAL	0x04
#define WDOG_CTL	0x08
#define WDOG_INTCLR	0x0c
#define WDOG_MIS	0x14

/* Timer Control Register Bits */
#define TIMER_CTRL_16BIT	(0 << 1)	/* 16-bit counter mode */
#define TIMER_CTRL_32BIT	(1 << 1)	/* 32-bit counter mode */
#define TIMER_CTRL_IE		(1 << 5)	/* Interrupt enable */
#define TIMER_CTRL_EN		(1 << 7)	/* Timer enable */		

/*UART REF clock - fixed at 24Mhz in BCM5892*/
#define REFCLK_FREQ				24000000	
#define BCM5892_WDOG_CLK      	(REFCLK_FREQ/4)


/*-----CPU register operation*/
#define readl(addr)  (*(volatile uint *)(addr))
#define reg32(addr) (*(volatile uint *)(addr))
#define writel(value,addr)  (*(volatile uint *)(addr) = (value))
#define writel_or(value,addr) *((volatile uint *)(addr)) |= (value)      /*get register value then or it,then write to the register*/
#define writel_and(value,addr)  *((volatile uint *)(addr)) &= (value)  	/*get register value then and it,then write to the register*/

/* IRQ operation*/
void s_SetInterruptMode(uint IntNo, uchar Priority, uchar INT_Mode);
void s_SetIRQHandler(uint IntNo, void (*Handler)(int id));
void s_SetFIQHandler(uint IntNo, void (*Handler)(int id));
void s_SetSoftInterrupt(uint IntNo, void (*Handler)(void));


#define irq_save(x) irq_save_asm(&x)
#define irq_restore(x) irq_restore_asm(&x)
void disable_irq(uint irq);
void enable_irq(uint irq);
uint  Swap_value(uint value,uint *dst); /*先将dst指针内容作为返回值，随后将value写入dst对应的地址，整个过程为原子操作*/

/*---------soft timer & Event-----------------------------------*/
#define PRINTER_HANDLER 0         //目前预留
#define MODEM_HANDLER 1         
#define KEY_HANDLER  2          //按键事件序号
enum TIMER{TM_SIO=0,TM_MDM, TM_FSK,TM_ICC,TM_MAG,TM_KB,TM_SYS,TM_REMOTE_DOWN,TM_PRN,TM_LCD,TM_USB,TM_USB_DRV,TM_LIMIT};
int s_SetTimerEvent(int handle,void (*TimerFunc)(void));
void StartTimerEvent(int handle,ushort uElapse);
void StopTimerEvent(int handle);

typedef struct
{
     uint start;       
     uint timeout;    	
} T_SOFTTIMER;
void s_TimerSetMs(T_SOFTTIMER *tm,uint lms);
uint s_TimerCheckMs(T_SOFTTIMER *tm);


/*------------timer operation*/
#define TIMER_PERIODIC	(1 << 6)	/* Periodic mode */
#define TIMER_ONCE		(0 << 6)	/*Only one time */

struct platform_timer_param {
	uint timeout;            	/* uint is 1us */
	uint mode;			/*timer periodic mode. TIMER_PERIODIC or TIMER_ONCE*/
	void (*handler)(void);    /* interrupt handler function pointer */
	uint interrupt_prior;	/*timer's interrupt priority*/
	uint id;            		/* timer id number */
};
struct platform_timer {
	int id;
	uint mode;
	uint reload;               	/* uint is 1us*/
	void (*handler)(void);
	uint interrupt_prior;	/*timer's interrupt priority*/
};
/****************************************
    Hard timer resource distribute
    TMR0	IC card
    TMR1	IC card
    TMR2	system timer,unit 10ms 
    TMR3	OS schedule timer 
    TMR4	TouchScreen
    TMR5	printer1 IRDA
    TMR6	printer2
    TMR7	MODEM
    PWM0	KB_LED
    PWM1	PRN
    PWM2	BEEP
    PWM3	LCD_BL
****************************************/
//Hard timer resource distribute
#define TIMER_IC_0  0   //IC card
#define TIMER_IC_1  1   //IC card
#define TIMER_TICK  2   //tick
#define TIMER_OS    3   //os schedule timer
#define TIMER_TS    4   //TouchScreen
#define TIMER_PRN_0 5   //print 0
#define TIMER_IRDA  7   //irda  D210不带modem
#define TIMER_PRN_1 6   //print 1
#define TIMER_MDM   7   //modem
struct platform_timer *platform_get_timer(struct platform_timer_param *param);
int platform_start_timer(struct platform_timer *timer);
int platform_stop_timer(struct platform_timer *timer);
void platform_timer_IntClear(struct platform_timer *timer);

typedef enum {PWM0=0,PWM1,PWM2,PWM3}PWM_TYPE;
void pwm_enable(PWM_TYPE pwm,unsigned char isEnalbe);
void pwm_duty(PWM_TYPE pwm,unsigned int dutyUs);
void pwm_config(PWM_TYPE pwm,unsigned int periodUs,
    unsigned char isOpenDrain,unsigned char isHighPolarity);


/*-------IO part -----------------------------------------*/
enum GPIO_PORT_NUM{GPIOA=0,GPIOB,GPIOC,GPIOD,GPIOE};
typedef enum
{
    GPIO_INPUT= 0x00,
    GPIO_OUTPUT,     
    GPIO_FUNC0,       
    GPIO_FUNC1,
    GPIO_FUNC_DIS, /* change from alternative function back to GPIO */
} GPIO_PIN_TYPE;
typedef enum
{
    GPIO_DRV_STRENGTH_4mA		= 0x04,
    GPIO_DRV_STRENGTH_6mA		= 0x06,
    GPIO_DRV_STRENGTH_8mA		= 0x08
} GPIO_DRV_STRENGTH;
int gpio_get_pin_type(int port, int subno);
void gpio_set_pin_type(int port, int subno, GPIO_PIN_TYPE pinType );
void gpio_set_mpin_type(int port, uint Mask, GPIO_PIN_TYPE pinType );
void gpio_set_pin_val(int port, int subno, int val );
void gpio_set_mpin_val(int port, uint mask, uint bitval );
uint gpio_get_pin_val_output(int port, int subno );
uint gpio_get_pin_val(int port, int subno);
uint gpio_get_mpin_val(int port, uint mask);
void gpio_set_pin_interrupt(int port,uint subno, int enable);
void gpio_set_pull(int port, int subno, int val );
void gpio_enable_pull(int port, int subno);
void gpio_disable_pull(int port, int subno);
void gpio_set_drv_strength(int port, int subno, GPIO_DRV_STRENGTH val );

enum {INTTYPE_LOWLEVEL=0,INTTYPE_HIGHLEVEL,INTTYPE_FALL,INTTYPE_RISE}; /*TrigType 的类型*/ 
int s_setShareIRQHandler(int port,uint SubNo,int TrigType,void(*Handler)(void));

/*--------spi operation---------------------------------------*/
void spi_config(int channel, uint speed, int data_size, uchar mode);
void spi_tx(int channel,uchar*tx_buf ,int len,int data_bits);
int spi_rx(int channel,uchar *rx_buf, int len,int data_bits);
ushort spi_txrx(int channel,ushort data,int data_bits); 
int spi_txcheck(int channel);

/*--------nand flash operation---------------------------------*/
uint nand_init(void);
int nand_erase(uint offset, uint size);
int nand_write(uint offset,uchar *data, uint data_len);
int nand_read(uint offset,uchar *data, uint data_len);

/*--------------------RTC operation--------------------------*/
uchar SetTime(uchar *str);
void GetTime(uchar *str);
uint bbl_get_rtc_secs(); //uint is 1s
uint bbl_get_rtc_div();    //uint is 1/32768s ->3.05us

/*--------------------BBL operation--------------------------*/
void GetRandom(uchar *pucDataOut);
void UnlockSecRAM(void);
void UnlockSecRAM_E(void);
int WriteSecRam(uint uiOffset, uint len, uchar *pucData);
int ReadSecRam(uint uiOffset, uint len, uchar *pucData);
//#define printk

/*reserve*/
#define TASK_PRIOR_Firmware0   10 /*soft interrupt*/
#define TASK_PRIOR_Firmware1   (TASK_PRIOR_Firmware0+1)
#define TASK_PRIOR_Firmware2   (TASK_PRIOR_Firmware0+2)
#define TASK_PRIOR_Firmware3   (TASK_PRIOR_Firmware0+3)
#define TASK_PRIOR_Firmware4   (TASK_PRIOR_Firmware0+4)
#define TASK_PRIOR_Firmware5   (TASK_PRIOR_Firmware0+5)
#define TASK_PRIOR_Firmware6   (TASK_PRIOR_Firmware0+6)
#define TASK_PRIOR_Firmware7   (TASK_PRIOR_Firmware0+7)
#define TASK_PRIOR_Firmware8   (TASK_PRIOR_Firmware0+8) /*touch screen task*/
#define TASK_PRIOR_Firmware9   (TASK_PRIOR_Firmware0+9) /*poweroff task*/
#define TASK_PRIOR_Firmware10  (TASK_PRIOR_Firmware0+10) /*gif*/

#define TASK_PRIOR_BASE   23
#define TASK_PRIOR_MC509_LINK  24
#define TASK_PRIOR_BT_LOOP 25

/*reserve*/
#define TASK_PRIOR_USER0       26
#define TASK_PRIOR_USER1       (TASK_PRIOR_USER0+1)
#define TASK_PRIOR_USER2       (TASK_PRIOR_USER0+2)
#define TASK_PRIOR_USER3       

#define TASK_PRIOR_WIFI_DRV    21
#define TASK_PRIOR_WIFI_LINK   22
#define TASK_PRIOR_APP         30
#define TASK_PRIOR_IDLE        31

#endif /* __ASM_ARCH_PLATFORM_H */  
