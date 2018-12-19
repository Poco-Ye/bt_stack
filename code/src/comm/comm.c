/******************************************************************************
*  文件名称:
*               Comm.c
*  功能描述:
*               串口驱动
*  包含以下函数:
*               s_CommInit         串口初始化
*               interrupt_uart0     串口0中断服务程序
*               interrupt_uart1     串口1中断服务程序
*               interrupt_uart2     串口2中断服务程序
*               PortOpen             打开指定串口，并初始化相关通信参数
*               PortClose            关闭指定串口，如果串口还有数据未发送完毕，则
*                                        等待发送完毕后再关闭
*               PortStop              立即关闭串口
*               PortSend            发送一个字符
*               PortTx                发送一个字符
*               PortTxs              发送字符串
*               PortRecv            接收一个字符
*               PortRx               接收一个字符
*               PortTxPoolCheck    等待串口发送完毕
*               PortSends           发送字符串
*               PortReset           串口复位，复位接收发送缓冲区相关参数
* 历史记录:
*          修改人       修改日期                 修改内容                      版本号
2012.3.1,ported from S80 ZA9L0 platform
****************************************************************************/
#include  <string.h>
#include  <stdarg.h>

#include <Base.h>
#include "Comm.h"

// #include "btstack_slip.h"
// #include "queue1.h"
// #include "bt_init.h"

#define PRINTK_DEBUG 
//#define DEBUG_COMM 

// 外部变量定义
uint errno = 0;
// 全局变量定义
volatile ushort si_read[PHY_UART];  //  串口接收缓冲区读指针
volatile ushort si_write[PHY_UART]; // 串口接收缓冲区写指针
volatile ushort so_read[PHY_UART];  // 串口发送缓冲区读指针
volatile ushort so_write[PHY_UART]; // 串口发送缓冲区写指针

//add by zhufeng
volatile ushort si_read_bt[PHY_UART];  //  串口接收缓冲区读指针
volatile ushort si_write_bt[PHY_UART]; // 串口接收缓冲区写指针
uchar  si_pool_bt[PHY_UART][SIO_SIZE]; // 串口接收缓冲区

uchar  si_pool[PHY_UART][SIO_SIZE]; // 串口接收缓冲区
uchar  so_pool[PHY_UART][SIO_SIZE]; // 串口发送缓冲区
volatile ushort port_used[PHY_UART];// 串口使用标志
volatile ushort port_cts_watch[PHY_UART];
volatile ushort port_xtab[CHANNEL_LIMIT];
volatile ushort port_ppp_serve[PHY_UART];
volatile ushort port_link_watch[PHY_UART],ptr_carrier[PHY_UART],carrier_lost[PHY_UART];
uchar const NO_CARRIER[]="\x0d\x0aNO CARRIER\x0d\x0a";
static ulong galUartBaudTab[]={9600,14400,19200,28800,38400,57600,115200,230400};

//#define  BT_DEBUG_COMM//shics
#ifdef BT_DEBUG_COMM//shics
volatile uchar shics_debug_buff[8][64];
volatile int shics_bt_comm_flag=0;
#endif

static struct {
	uchar status;
	char  attr[23];
} s_PINPAD_info; /*PINPAD logic Port open/close status*/ 
uchar PortClose_std(uchar channel);
static void interrupt_cts(void);
static void interrupt_uart(int id);
static void UartSetRxInt(int port,uint enable);
static void UartSetTxInt(int port,uint enable);
static void uart_phy_sends(int cur_port);

uchar IsConfigPinpad(void)
{
    int type = 0;
    
    type = get_machine_type();    
    if (type == S78 || type==S500 || type==S58 || type==S800) return 1;
    if (type==S80 && !is_eth_module()) return 1;

    return 0;
}

void s_CommInit(void)
{
	errno = 0;
	//全局变量清零
	memset((uchar*)port_used,0x00,sizeof(port_used));
	memset((uchar*)port_cts_watch,0x00,sizeof(port_cts_watch));
	memset((uchar*)port_link_watch,0x00,sizeof(port_link_watch));
	memset((uchar*)port_ppp_serve,0x00,sizeof(port_ppp_serve));
	memset((uchar*)port_xtab, 0xff, sizeof(port_xtab));
	memset((uchar*)si_read, 0x00, sizeof(si_read));
	memset((uchar*)si_write, 0x00, sizeof(si_write));
	memset((uchar*)so_read, 0x00, sizeof(so_read));
	memset((uchar*)so_write, 0x00, sizeof(so_write));
	memset((uchar*)&s_PINPAD_info, 0x00, sizeof(s_PINPAD_info));
	port_xtab[P_MODEM] = 0;
	port_xtab[P_WNET] = 1;
	port_xtab[P_RS232A] = 2;
	port_xtab[P_PINPAD] = 3;
	
#ifdef BT_DEBUG_COMM//shics
memset(shics_debug_buff,0,sizeof(shics_debug_buff));
#endif

    switch(get_machine_type())
    {
	case D210:
		gpio_set_pin_type(GPIOB,5,GPIO_OUTPUT);//不用的UART0_CTS需要配置为输出低电平		
		gpio_set_pin_val(GPIOB,5,0);
		
		port_xtab[P_WNET] = 0;
		port_xtab[P_BT] = 1;
		port_xtab[P_WIFI] = 3;
		//--power on UART
        gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);//PWR_nSLP
        gpio_set_pin_val(GPIOB,31, 0);//power open
		break;
	case D200:
		port_xtab[P_WNET] = 3;
		port_xtab[P_BT] = 1;
		port_xtab[P_WIFI] = 3;
		//--power on UART
        gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);//PWR_nSLP
        gpio_set_pin_val(GPIOB,31, 0);//power open
		break;
    case S900:
    	//--power on UART
        gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);//PWR_nSLP
        gpio_set_pin_val(GPIOB,31, 0);//power open
        break;

    case S800:
	case S500:
        // set IRQ type and priority 2 for modem CTS(GPB5)
        //shared by:modem cts,
        gpio_set_pin_type(GPIOB,5,GPIO_INPUT);//set CTS as input
        s_setShareIRQHandler(GPIOB,5,INTTYPE_LOWLEVEL,interrupt_cts);//low level trigger
	    //s800 pinpad和232b复用选择控制端口
		//--GPD16 is COM_S0,GPD17 is COM_S1
		gpio_set_pin_type(GPIOD,16,GPIO_OUTPUT);//set COM_S0 as output
		gpio_set_pin_type(GPIOD,17,GPIO_OUTPUT);//set COM_S1 as output
		gpio_set_pin_val(GPIOD, 16, 0 );
		gpio_set_pin_val(GPIOD, 17, 0 );        
		break;

    default:break;
    }

	s_SetInterruptMode(IRQ_OUART0, INTC_PRIO_3, INTC_IRQ);
	s_SetIRQHandler(IRQ_OUART0,interrupt_uart);

	s_SetInterruptMode(IRQ_OUART1, INTC_PRIO_3, INTC_IRQ);
	s_SetIRQHandler(IRQ_OUART1,interrupt_uart);

	s_SetInterruptMode(IRQ_OUART2, INTC_PRIO_3, INTC_IRQ);
	s_SetIRQHandler(IRQ_OUART2,interrupt_uart);

	s_SetInterruptMode(IRQ_OUART3, INTC_PRIO_3, INTC_IRQ);
	s_SetIRQHandler(IRQ_OUART3,interrupt_uart);
}

void interrupt_cts(void)
{

//printk("CTS1:%08X,%08X.",rGPB_INT_STAT,rGPB_INT_MSK);
	if(!(rGPB_INT_STAT&0x20))//CTS(GPB5)
	{
		return;
	}

	//--enable UART0 TX interrupt
	UartSetTxInt(0,1);

	//--disable cts interrupt
	gpio_set_pin_interrupt(GPIOB,5,0);

	//--clear cts interrupt,it's done in IsrPort() already
	//rGPB_INT_CLR=0x20;

	//--re-trigger tx interrupt
	uart_phy_sends(0);
//printk("CTS2:%08X,%08X.",rGPB_INT_STAT,rGPB_INT_MSK);

	return;
}

int UartOpen(int port_no,int baud_rate,uint data_bit,uchar parity,uint stop_bit)
{
	uint status = 0;
	uint temp;
	uint divider;
	uint remainder;
	uint fraction;
	uint UART_BASE;
	uint old_cr;

	switch (port_no)
	{
		case 0:
			UART_BASE =URT0_REG_BASE_ADDR;
			writel_or(0x180000, GIO0_R_GRPF0_AUX_SEL_MEMADDR);
			break;
		case 1:
			UART_BASE =URT1_REG_BASE_ADDR;
			break;
		case 2:
			UART_BASE =URT2_REG_BASE_ADDR;
			writel_or(0x0C00,GIO3_R_GRPF3_AUX_SEL_MEMADDR);
			break;

		case 3:
			UART_BASE =URT3_REG_BASE_ADDR;
			if (get_machine_type() == S900)
			{
				if((GetHWBranch()==S900HW_Vxx || GetHWBranch()==S900HW_V2x)&&is_bt_module())
					writel_or(0x3C000,GIO3_R_GRPF3_AUX_SEL_MEMADDR);
			}
			else writel_or(0xC000,GIO3_R_GRPF3_AUX_SEL_MEMADDR);
			break;

		default:
			return 1;
			break;
	}
	//--disable  UARTn interrupts
	disable_irq(IRQ_OUART0+port_no);
	//--disable UART0 TX interrupt
	UartSetTxInt(port_no,0);
	UartSetRxInt(port_no,0);

	/* disable before configure*/
	//*(volatile ushort*)(UART_BASE + UART_CR)  &=~0x0001; //URT_R_UARTCR_MEMADDR
	writel(0,(UART_BASE + UART_CR) );

	//--clear the FIFOs
	writel((data_bit | stop_bit | parity),UART_BASE + UART_LCRH); //URT_R_UARTLCR_MEMADDR

	/*** Set baud rate
	** IBRD = UART_CLK / (16 * BPS)
	** FBRD = ROUND((64 * MOD(UART_CLK,(16 * BPS))) / (16 * BPS))
	******/
	temp      = 16 * baud_rate;
	divider   = UART_REF_CLK_FREQ / temp;
	remainder = UART_REF_CLK_FREQ % temp;
	fraction  = (4 * remainder) / baud_rate;
	if(data_bit == 7) data_bit =UART_DATABIT_7;
	else data_bit =UART_DATABIT_8;

	if(stop_bit ==2) stop_bit =UART_STOPBIT_2;
	else 	 stop_bit =UART_STOPBIT_1;

	if(parity ==2) parity =UART_PARITY_ODD;
	else if(parity ==1) parity =UART_PARITY_EVEN;
	else parity =UART_PARITY_NONE;

	writel(divider,UART_BASE + UART_IBRD); //URT_R_UARTIBRD_MEMADDR
	writel(fraction,UART_BASE + UART_FBRD); //URT_R_UARTFBRD_MEMADDR
	writel((data_bit | stop_bit | parity | UART_LCRH_FEN),UART_BASE + UART_LCRH); //URT_R_UARTLCR_MEMADDR
	if (port_no == 3)writel(0x00,UART_BASE + UART_IFLS); // 1/8 tx/rx fifo trigger interrupt
	else writel(0x09,UART_BASE + UART_IFLS); // 1/4 tx/rx fifo trigger interrupt
	//writel(UART_ENABLE_RX_TX ,UART_BASE + UART_CR); //URT_R_UARTCR_MEMADDR
	if (port_no == 3 && is_bt_module() && get_machine_type() == S900)//uart for bluetooth
	{
		//enable CTS and RTS
		writel(UART_ENABLE_FLOWCTRL|UART_ENABLE_RX_TX ,UART_BASE + UART_CR); //URT_R_UARTCR_MEMADDR
	}
	else if (port_no == 3 && is_wifi_module() && get_machine_type() == D210)//uart for wifi
	{
		//enable CTS and RTS
		//writel(UART_ENABLE_FLOWCTRL|UART_ENABLE_RX_TX ,UART_BASE + UART_CR); //URT_R_UARTCR_MEMADDR
		writel(UART_ENABLE_RX_TX ,UART_BASE + UART_CR); //URT_R_UARTCR_MEMADDR
	}
	else if (port_no == 1 && is_bt_module() && (get_machine_type() == D210||(get_machine_type() == S900 && GetHWBranch() == S900HW_V3x)))	//bt , must enable flow control
	{
		//enable CTS and RTS
		writel(UART_ENABLE_FLOWCTRL|UART_ENABLE_RX_TX ,UART_BASE + UART_CR); //URT_R_UARTCR_MEMADDR
	}
	else if (port_no == 1 && is_bt_module() && get_machine_type() == D200)	//bt , must enable flow control
	{
		//enable CTS and RTS
		writel(UART_ENABLE_FLOWCTRL|UART_ENABLE_RX_TX ,UART_BASE + UART_CR); //URT_R_UARTCR_MEMADDR
	}
	else
	{
		writel(UART_ENABLE_RX_TX ,UART_BASE + UART_CR); //URT_R_UARTCR_MEMADDR
	}

	//--enable UART RX interrupt
	UartSetRxInt(port_no,1);
	//--enable UARTn TX interrupt
	UartSetTxInt(port_no,1);

	//--clear rx and tx buffers
	si_read[port_no]=0;
	si_write[port_no]=0;
	so_read[port_no]=0;
	so_write[port_no]=0;

	//--enable UART0 interrupt
	enable_irq(IRQ_OUART0+port_no);

	return 0;
}

static void uart_phy_sends(int cur_port)
{
	uint UART_BASE,i,flag;

	switch (cur_port)
	{
		case 0:
			UART_BASE =URT0_REG_BASE_ADDR;
			break;
		case 1:
			UART_BASE =URT1_REG_BASE_ADDR;
			break;
		case 2:
			UART_BASE =URT2_REG_BASE_ADDR;
			break;
		case 3:
			UART_BASE =URT3_REG_BASE_ADDR;
			break;
	}

	//--trigger the TX interrupt
	//--lock all the IRQ interrupts,write to TX_FIFO for maximum 32 times
	irq_save(flag);
	for(i=0;i<32;i++)//5
	{
		if(readl(UART_BASE+UART_FR) & 0x20) break; //fifo full
		if(so_read[cur_port]==so_write[cur_port])break;
		writel(so_pool[cur_port][so_read[cur_port]],(UART_BASE +UART_DR));
		so_read[cur_port]=(so_read[cur_port]+1)%SIO_SIZE;
	}
	irq_restore(flag);

	return;
}

static void UartSetTxInt(int port,uint enable)
{
	uint reg;
	uint base,flag;
	if((port<0) || (port >= PHY_UART) ) return ;
	
	base = URT0_REG_BASE_ADDR + 0x1000*port;
	irq_save(flag);
	reg=readl(base+UART_IMSC);
	if(enable) reg |=UART_STATUS_TXIS;
	else		 reg &=~UART_STATUS_TXIS;

	writel(reg,(base+UART_IMSC));
	irq_restore(flag);
}

static void UartSetRxInt(int port,uint enable)
{
	uint flag,reg;
	uint base;
	if((port<0) || (port>3) ) return ;
	
	base = URT0_REG_BASE_ADDR + 0x1000*port;
	irq_save(flag);
	reg=readl(base+UART_IMSC);
	if(enable) reg |=(UART_STATUS_RTIS |UART_STATUS_RXIS);
	else		 reg &=~(UART_STATUS_RTIS |UART_STATUS_RXIS);
	
	writel(reg,(base+UART_IMSC));
	irq_restore(flag);
}

static int UartCheck_txFIFO(int port)
{
	uint flag,reg;
	uint base;

	base = URT0_REG_BASE_ADDR + 0x1000*port;
	reg=readl(base +UART_FR);

	if(reg&(1<<3))return 0;//Uart Busy	
	return 1;//TX FIFO is empty
}

int PortCheckBaudValid(ulong ulBaudrate)
{
		  int iLoop;

		  for(iLoop=0;iLoop<sizeof(galUartBaudTab)/sizeof(long);iLoop++)
		  {
				if(galUartBaudTab[iLoop]==ulBaudrate)return 0;
		  }
		  return -1;
}

extern void *ppp_comm[3];
extern int ppp_xmit_char(void *ppp);
extern void ppp_recv_char(int, void *);
/*
** 逻辑端口号Port映射成物理端口号
**/
int ppp_get_comm(int port)
{
	return port_xtab[port];
}

/*
** 获取modem对应的物理端口号
**/
int modem_get_comm(void)
{
	return port_xtab[P_MODEM];
}

/*
** 关闭comm 中断
**
**/
void ppp_com_irq_disable(int comm)
{
	if(comm==1)
	{
		//--disable  UART1 interrupts
        disable_softirq_timer();
	}
	else if(comm==2)
	{
		//--disable  UART2 interrupts
        disable_softirq_timer();
	}
	else if(comm == 3)
	{
		disable_softirq_timer();
	}
}

/*
** 打开comm 中断
**
**/
void ppp_com_irq_enable(int comm)
{
	if(comm == 1)
	{
		//--enable UART1 interrupt
        enable_softirq_timer();
	}else if(comm == 2)
	{
		//--enable UART2 interrupt
        enable_softirq_timer();
	}
	else if(comm == 3)
	{
		enable_softirq_timer();
	}
}

/*
** 打开comm 发送中断
** 因为中断服务函数中，
** 当发现没有数据发送,
** 会关闭发送中断；
** 因此ppp要主动打开发送中断
**/
void ppp_com_irq_tx(int comm)
{
	int c;
    disable_softirq_timer();
	while (1) 
	{
		if (!ppp_comm[comm]) break;
		c=ppp_xmit_char(ppp_comm[comm]);
		if (c < 0) break;
		if (comm == port_xtab[P_WNET])
			PortTx(P_WNET, c&0xff);
		if (comm == port_xtab[P_MODEM])
			PortTx(P_MODEM, c&0xff);
	}
    enable_softirq_timer();
}

/*
** 检查comm和modem状态
** 由ppp_login调用
**/
int ppp_com_check(int comm)
{
	int status;

	if(port_xtab[P_MODEM]!=comm)
	{
		return -1;
	}
	status = ModemCheck();
	if((status!=0x00)&&(status!=0x08)&&(status!=0x09)&&(status!=0x01))
		return -1;

    disable_softirq_timer();
	port_ppp_serve[comm]=1;/* start ppp snd and rcv */
    enable_softirq_timer();
	return 0;
}

int ppp_com_clr(int comm)
{
	int cur_port = comm;
	if(cur_port<0||cur_port>2)
		return -1;
	si_read[cur_port] = si_write[cur_port];
	return 0;
}
/*
** 检查modem是否hang_off
** 由ppp的定时器调用
**/
int ppp_modem_check(int comm, long ppp_state)
{
	static uchar counter = 0;
    uchar status;
    
	if(port_xtab[P_MODEM]!=comm)
		return -1;
    
 	if(port_ppp_serve[port_xtab[P_MODEM]]==0)
		return -1;   
   
    status=ModemCheck();
    if((status!=0x00)&&(status!=0x08)&&(status!=0x09)&&(status!=0x01))
    {       
        port_ppp_serve[port_xtab[P_MODEM]]=0;
        return -1;
    }
        

	#if 0
	ScrPrint(0,4,0, "[%02x] %02x, %d, %d, %d, %d",
		counter++,
		ppp_state,
		port_xtab[P_MODEM],
		port_ppp_serve[1], port_ppp_serve[2],
		ret
		);
	#endif

	return 0;
}

int GetPortPhyNmb(uchar channel)
{
	return port_xtab[channel%CHANNEL_LIMIT];
}


// extern int gAclDataPacket;
// extern tBtCtrlOpt *getBtOptPtr(void);
// static uint8_t	hci_packet_with_pre_buffer_ext[520];

// static void hci_transport_slip_init_ext(void) {
	// btstack_slip_decoder_init(&hci_packet_with_pre_buffer_ext[6], 520);
// }

// static void hci_transport_h5_process_frame_ext(uint16_t frame_size) {
	// tBtCtrlOpt *ptBtOpt = getBtOptPtr();
	
	// if (frame_size < 4) {
		// return;
	// }
	
	// uint8_t *slip_header  = &hci_packet_with_pre_buffer_ext[6];
	// uint8_t  link_packet_type = slip_header[1] & 0x0f;
	// if(link_packet_type == HCI_ACL_DATA_PACKET && ptBtOpt->Connected) {
		// log_debug("comm.c connected link_packet_type:%d", link_packet_type);
		// gAclDataPacket = 1;
	// }
	
// }
extern int hasConnect;
extern int gAclDataPacket;

static uint16_t  decoder_pos;

typedef enum {
	SLIP_DECODER_UNKNOWN = 1,
	SLIP_DECODER_ACTIVE,
	SLIP_DECODER_X_C0,
	SLIP_DECODER_COMPLETE
} uart_decoder_state_t;

//static uart_decoder_state_t decoder_state = SLIP_DECODER_UNKNOWN;


extern void rx_done_handler_ex();
int gTimeBegin;
extern int hasConnect;
extern int gRecvStart;
int gRecvFrameFinish;
//uint8_t   pre_buffer_ext[220];
int receive_start_ext, receive_time_ext;

//void hci_transport_slip_init_ext(void) {
	//btstack_slip_decoder_init_ext(&pre_buffer_ext[6], 220);
//}

uint16_t gframe_size;
extern int gProcessFrameFinish;
extern int gMutiSendingData;
extern int gSendingData;
extern int hci_transport_h5_process_frame_ext(uint16_t frame_size);
static void interrupt_uart(int id)
{
	uint i,base,status,fifo,reg_fr;
	int c,uartid;
	int ppp_tx_finished = 1;
	int tmp_last,n;
	int link_packet_type;
	//uint16_t frame_size;

	uartid = id -IRQ_OUART0;
	if((uartid<0) || (uartid>3) ) return ;
	base = URT0_REG_BASE_ADDR + 0x1000*uartid;

	status =readl(base + UART_MIS);
	writel(status,(base+UART_ICR));  //Clear interrupt flag

	//if(hasConnect) {
		//printk("status:%d \r\n", status);
	//}

	if(status & (UART_STATUS_RTIS|UART_STATUS_RXIS))
	{
		//if(hasConnect) {
			//printk("status:%d \r\n", status);
		//}

		//--receive data if ready,64 for UART3,others 16
		for(i = 0; i < 64; i++)
		{
			if(readl(base +UART_FR) & (1<<4)) break;
			c=readl(base +UART_DR) &0xff;
			si_pool[uartid][si_write[uartid]]=c;
			si_write[uartid]=(si_write[uartid]+1)%SIO_SIZE;

			if(ppp_comm[uartid]) ppp_recv_char(c,ppp_comm[uartid]);

			//if ((receive_start_ext == 0) && (c != 192) && (hasConnect)) {
				//receive_start_ext = GetTimerCount();
			//}

			#if 1
			if((uartid == 1) && (hasConnect)) { //(gMutiSendingData == 0)
				printk("decoder_process_ext:%x \r\n", c);
				btstack_slip_decoder_process_ext(c);
				gframe_size = btstack_slip_decoder_frame_size_ext();
				if (gframe_size) {
					//gProcessFrameFinish = 0;
					//receive_time_ext = GetTimerCount() - receive_start_ext;
					//printk("receive_time_ext:%d ms frame_size:%d\r\n", receive_time_ext, gframe_size);
					printk("receive_time_ext frame_size:%d\r\n", gframe_size);
					
					gRecvFrameFinish = 1;

					//DelayMs(250);
					hci_transport_slip_init_ext();
					//receive_start_ext = 0;

				}

			}
			#endif


			
			
			#if 0
			if((uartid == 1) && hasConnect) {
				//printk("AAA char:%x gTimeBegin:%d \r\n", c, gTimeBegin);
				if((gRecvStart == 0) && (c == 192)) {
					gTimeBegin = GetTimerCount();
					printk("BBB char:%x gTimeBegin:%d \r\n", c, gTimeBegin);
				}
			}
			#endif

			
			#if 0
			if(uartid == 1 && hasConnect) {
				printk("char:%x decoder_state:%d \r\n", c, decoder_state);
				switch(decoder_state) {
					case SLIP_DECODER_UNKNOWN:				
						if (c != 192) 	{
							break;
						}
					decoder_state = SLIP_DECODER_UNKNOWN;
					decoder_pos = 0;	
					decoder_state = SLIP_DECODER_X_C0;
					break;	
					
					case SLIP_DECODER_X_C0:
						switch(c) {
						case 192:
							break;

						default:
							decoder_pos++;
							decoder_state = SLIP_DECODER_ACTIVE;
							printk("11 char:%x decoder_pos:%d \r\n", c, decoder_pos);
							break;
						}
					break;	
					
					case SLIP_DECODER_ACTIVE:
						switch(c) {
						case 192:
							decoder_state = SLIP_DECODER_UNKNOWN;
							decoder_pos = 0;	
						break;

						default:
							decoder_pos++;
							printk("22 char:%x decoder_pos:%d \r\n", c, decoder_pos);
							if(2 == decoder_pos) {
								link_packet_type = c & 0x0f;
								if(2 == link_packet_type) {
									printk("#####char:%x decoder_pos:%d \r\n", c, decoder_pos);
									gAclDataPacket = 1;
								}
							}
						break;
						}

					break;
				}	
				
			}
			#endif
			
			//if(uartid == 1 && i > 0) rx_done_handler_ex(); //added by shics
			
			#if 0
			if(uartid == 1 && hasConnect) {
				si_pool_bt[uartid][si_write_bt[uartid]]=c;
				si_write_bt[uartid]=(si_write_bt[uartid]+1)%SIO_SIZE;
			}
			#endif
		}		
	}

		

	//--Send data if TX FIFO empty,32 for uart3,others 16
	if(status & UART_STATUS_TXIS)
	{
		for(i=0;i<32;i++)
		{
			reg_fr =readl(base+UART_FR);
			if(reg_fr & (1<<5)) break;
			if(so_read[uartid]==so_write[uartid])break;

			if(!uartid&&port_cts_watch[uartid]&& rGPB_DIN&0x20)//CTS off
			{
				UartSetTxInt(uartid,0);//--disable UART TXD interrupt
				gpio_set_pin_interrupt(GPIOB,5,1);//--enable cts interrupt
				break;
			}
			writel(so_pool[uartid][so_read[uartid]],(base +UART_DR));
			so_read[uartid]=(so_read[uartid]+1)%SIO_SIZE;
		}
	}
}

uchar s_PortOpen_std(uchar channel, uchar *Attr)
{
	char data_bits,stop_bits,parity,tmps[100];
	ushort i,j, cur_port, AttrLen = 0;
	long baud_rate, rBaudDiv = 0;
	int device_id,type = 0;
	static const long baud_tab[] = {1200,2400,4800,9600,14400,19200,28800,38400,
									57600,115200,230400,380400,460800,921600};

    type = get_machine_type();
	if(channel==P_USB_DEV)
	{
		if(!strcmp(Attr,"FAST"))return UsbDevOpen(1);
		if (Attr==NULL)return UsbDevOpen(0);
	}

	if(channel==P_USB_HOST)
	{
		if(Attr==NULL)device_id=0;//0 for auto detect of device
		else
		{
			memcpy(tmps,Attr,9);
			tmps[9]=0;
			strupr(tmps);
			if(!strcmp(tmps,"PAXDEV"))device_id=1;
			else if(!strcmp(tmps,"UDISK"))device_id=2;
			else if(!strcmp(tmps,"UDISKFAST"))device_id=2;//device_id=3;
			else if(!strcmp(tmps,"SCANNER"))device_id=4;
			else return 0xfe;//invalid device id
		}
		return UsbHostOpen(device_id);
	}

	if(channel==P_USB_HOST_A && type==S800)
	{
		if(Attr==NULL)device_id=0;//0 for auto detect of device
		else
		{
			memcpy(tmps,Attr,9);
			tmps[9]=0;
			strupr(tmps);
			if(!strcmp(tmps,"PAXDEV"))device_id=1;
			else if(!strcmp(tmps,"UDISK"))device_id=2;
			else if(!strcmp(tmps,"UDISKFAST"))device_id=2;//device_id=3;
			else return 0xfe;//invalid device id
		}
		return OhciOpen(device_id);
	}

	if(channel>=CHANNEL_LIMIT && channel!=P_USB && channel!=P_USB_DEV) return 0x02;
	//1--check validality of channel no
	
	switch(channel)
	{
		case P_RS232A:
		break;		
		case P_RS232B:
			if((type==S900) || (type == S90) || (type ==SP30) || (type ==D210) || (type ==D200)) return 0x02;
			if(type == S80){
				if(is_eth_module())return 0x05;
			}
		break;
		case P_WNET:
			if(type==S300 || type==SP30) return 0x02;			
		break;
		case P_PINPAD:
			if(type==S300 || type == S900 || type ==SP30 || type ==D210 || (type ==D200)) return 0x02;
			/*PINPAD for S90 uses to BAR_CODE*/
		break;
		case P_MODEM:
			if(type==S300 || type ==SP30 || type ==D210 || type==D200 || type==S900) return 0x02;
			if(modem_occupied())return 0xf0;
		break;
		case P_WIFI:
			if(type==S300 || type ==SP30) return 0x02;
			if(!is_wifi_module()) return 0x05;
		break;
		case P_BARCODE:
			if(type != S900 && type != S300) return 0x02;
			if(!is_barcode_module()) return 0x05;
			break;
		case P_BT:
			if(type != S900 && type != D210 && type != D200) return 0x02;
			if (!is_bt_module()) return 0x05;
			break;
		case P_GPS:
			if(type != S900) return 0x02;
		break;
	}
	//2.1--fetch baud rate from input string
	AttrLen = strlen(Attr);
	
	for(i = 0, j = 0; i < AttrLen; i++)
	{
		if(Attr[i] == ',') break;
		if(Attr[i] != 0x20) tmps[j++] = Attr[i];
		if(j >= 7) return 0xfe;
	}

	if(i >= AttrLen) return 0xfe;
	i++;  //skip over ','
	tmps[j] = 0;
	baud_rate = atol(tmps);

	//2.2--check validality of baud rate
	for(j = 0;j < sizeof(baud_tab)/sizeof(baud_tab[0]); j++)
		if(baud_rate == baud_tab[j])break;

	if(j >= sizeof(baud_tab)/sizeof(long))return 0xfe;
	
	if (channel != P_WIFI)
	{
		if (baud_rate > 230400) return 0xfe;//other channel needn't the high baudrate
	}
	

	//3.1--fetch data bits from input string
	for(; i < AttrLen; i++)
		if(Attr[i] != 0x20)
			{data_bits = Attr[i];break;}

	if(i >= AttrLen) return 0xfe;
	//3.2--check validality of data bits
	if(data_bits != '7' && data_bits != '8') return 0xfe;
	data_bits -= '0';
	i++;//skip over data bits

	// 4--search for ',' from input string
	for(; i < AttrLen; i++)
		if(Attr[i] != 0x20)break;

	if(i >= AttrLen) return 0xfe;
	if(Attr[i] != ',') return 0xfe;
	i++;//skip over ',' following data bits

	//5.1--fetch parity from input string
	for(; i < AttrLen; i++)
		if(Attr[i] != 0x20){parity = Attr[i];break;}

	if(i >= AttrLen)return 0xfe;
	// 5.2--check validality of data bits
	if(parity == 'e')parity = 'E'; // 偶校验
	else if(parity == 'n')parity = 'N'; // 无校验
	else if(parity == 'o')parity = 'O'; // 奇校验
	else if(parity!='N' && parity!='E' && parity!='O') return 0xfe;

	i++;//skip over parity

	//6--search for ',' from input string
	for(; i < AttrLen;i++)
		if(Attr[i] != 0x20) break;

	if(i >= AttrLen) return 0xfe;
	if(Attr[i] != ',') return 0xfe;

	i++;//skip over ',' following parity

	//7.1--fetch stop bits from input string
	for(; i < AttrLen; i++)
		if(Attr[i] != 0x20){stop_bits = Attr[i];break;}
	if(i >= AttrLen) return 0xfe;
	//7.2--check validality of stop bits
	if((stop_bits != '1') && (stop_bits != '2')) return 0xfe;
	stop_bits -= '0';
	
	if(channel==P_USB_DEV)	return UsbDevOpen(0);

	//use usb_to_serial port
	if(channel == P_USB)
		return ftdi_port_open(baud_rate, data_bits, parity, stop_bits);
	switch(channel)
	{
		case P_RS232A:
			cur_port=2;
		break;
		case P_RS232B:
			if(port_xtab[channel]==0xff && port_used[3])return 0x05;
			if (type == S800 || type == S500)
			{
				gpio_set_pin_val(GPIOD, 16,1); //UART_SEL0-HIGH LEVEL select RS232B
			}
			cur_port=3;
		break;
		case P_WNET:
			if(type == D210)
			{
				if(port_xtab[channel]==0xff && port_used[0]) return 0x05;
				cur_port = 0;
			}
			else if(type == D200|| (type == S900 && GetHWBranch() == S900HW_V3x))
			{
				if(port_xtab[channel]==0xff && port_used[3]) return 0x05;
				cur_port = 3;
			}
			else
			{
				if(port_xtab[channel]==0xff && port_used[1]) return 0x05;
				cur_port = 1;
			}
		break;
		case P_PINPAD:
			if(port_xtab[channel]==0xff && port_used[3])return 0x05;
			if (type == S800 || type == S500)
			{
				gpio_set_pin_val(GPIOD, 16,0);	//UART_SEL0-LOW LEVEL select PINPAD
			}
			cur_port=3;
		break;
		case P_MODEM:
			cur_port=0;
			port_cts_watch[0] = 1;
		break;
		case P_WIFI:
			if (type == S900 || type == D210 || type == D200)
			{
				if(port_xtab[channel]==0xff && port_used[3])return 0x05;
				cur_port = 3;
			}
			else if (type == S500 || type == S800)
			{
				if(port_xtab[channel]==0xff && port_used[1])return 0x05;
				cur_port = 1;
			}
			else
			{
				return 0x05;
			}
		break;
		case P_BT:
			if(type == D210 || type == D200|| (type == S900 && GetHWBranch() == S900HW_V3x))
			{
				if(port_xtab[channel]==0xff && port_used[1])return 0x05;
				cur_port = 1;
			}
			else
			{
				if(port_xtab[channel]==0xff && port_used[3])return 0x05;
				cur_port = 3;
			}
		break;
		case P_BARCODE:
			if(type == S300) 
			{
				if(port_xtab[channel]==0xff && port_used[1]) return 0x05;
				cur_port = 1;
			}
			else 
			{	
				if(port_xtab[channel]==0xff && port_used[0]) return 0x05;
				cur_port = 0;
			}
		break;
		case P_GPS:
			if(type == S900 && GetHWBranch() == S900HW_V3x)
			{
				if(port_xtab[channel]==0xff && port_used[1]) return 0x05;
				cur_port = 1;
			}
			else
			{
    			if(port_xtab[channel]==0xff && port_used[3]) return 0x05;
    			cur_port = 3;
			}
		break;
	}
	//--disable old port assign
	for(i = 0; i < CHANNEL_LIMIT; i++)
		if(port_xtab[i] == cur_port)port_xtab[i]=0xff;

	port_used[cur_port]=1;
	port_xtab[channel]=cur_port;

	if(parity == 'O')		parity =2;
	else if(parity == 'E')	parity =1;
	else 				parity =0;
	
	UartOpen(cur_port,baud_rate,data_bits,parity,stop_bits);

	return 0;
}
/*
* 函数名称:
	uchar PortOpen(uchar channel, uchar *Attr)
* 功能描述:
*            打开串口
* 输入参数:
*           channel    串口逻辑通道号
*           Attr         串口属性参数，具体组成如下:
*                        波特率+ 数据位+奇偶标志+停止位
* 输出参数:
*          无
* 返回值:
*           0      打开串口成功
*         其他值   打开串口失败
*/
uchar PortOpen_std(uchar channel, uchar *Attr)
{
	uchar ret;

	if((channel==P_WNET)&&(wnet_ppp_used())){
		return 0xf0;
	}
	if(IsConfigPinpad()){
		if(channel == P_RS232B && s_PINPAD_info.status==1){
		    PortClose_std(P_PINPAD);
		    s_PINPAD_info.status=1;
		}    
	}
	ret = s_PortOpen_std(channel,Attr);
	if(IsConfigPinpad()){
		if(channel == P_PINPAD && ret == 0){
			s_PINPAD_info.status=1;
			strncpy(s_PINPAD_info.attr,Attr,sizeof(s_PINPAD_info.attr)-1);
		}
		if( channel == P_RS232B && ret != 0){
			if(s_PINPAD_info.status ==1) s_PortOpen_std(P_PINPAD,s_PINPAD_info.attr);
		}
	}
	return ret;
}

/**************************************************************************
* 函数名称:
*           uchar PortClose(uchar channel)
* 功能描述:
*           关闭指定逻辑串口
* 输入参数:
*           channel  串口逻辑号
* 返回值:
*          0              关闭成功
*       其他值  关闭失败
****************************************************************************/
uchar PortClose_std(uchar channel)
{
	uchar resp;
	ushort cur_port,last_read;
	T_SOFTTIMER uart_timer;
	
	if(channel==P_USB_DEV) return UsbDevStop();
	if(channel==P_USB) return ftdi_port_close();
	if(channel==P_USB_HOST) return UsbHostClose();
    if(channel==P_USB_HOST_A && get_machine_type()==S800)
        return OhciClose();

	resp = 0;
	errno = 0;
	//1--check validality of channel no
	if(channel>=CHANNEL_LIMIT)
	{
		errno = 601;//invalid channel no
		return 2;
	}

	if((channel == P_MODEM) && modem_occupied())
	{
		errno = 620;//modem is running
		return 0xf0;
	}
	if((channel==P_WNET)&&(wnet_ppp_used())) return 0xf0;
	if ((channel==P_WIFI) && (!is_wifi_module())) return 0xf0;
	
	cur_port = port_xtab[channel];
	if(cur_port >= PHY_UART)
	{
		errno = 602;//channel not open
		return 3;
	}

	if(!port_used[cur_port])//已经关闭port_used[cur_port] == 0
	{
	     return 0;
	}

	//--check if pool is empty
	s_TimerSetMs(&uart_timer,500);//unit:1ms
	last_read = so_read[cur_port];
	while(so_read[cur_port] != so_write[cur_port])
	{
	    if(!s_TimerCheckMs(&uart_timer))
	    {
		    errno = 604;//tx_pool timeout
//			ScrPrint(0,3,0,"port=%02x,read=%02x,write=%02x",cur_port,so_read[cur_port],so_write[cur_port]);
		    resp = 0xff;
		    goto C_TAIL;
	    }

		 if(last_read == so_read[cur_port])continue;

	    last_read = so_read[cur_port];
	    s_TimerSetMs(&uart_timer,500);//set timer again

	}//while(1)

	//--check if TX_FIFO is empty
	s_TimerSetMs(&uart_timer,500);//unit:1ms
	while(2)
	{
		if(!s_TimerCheckMs(&uart_timer))
		{
			//errno = 605;  // TX_FIFO timeout
			resp = 0xff;
			goto C_TAIL;
		}

		// 等待发送缓冲区数据为空，即发送完成
		if(UartCheck_txFIFO(cur_port)) break;
	}//while(2)

C_TAIL:
	port_used[cur_port] = 0;

	//--disable UARTn interrupt
	disable_irq(IRQ_OUART0+cur_port);
	switch(cur_port)
	{
	case 0:
		rUART0CR&=~0x301;//d9:RX enable,d8:TX enable,d0:UART enable
		break;
	case 1:
		rUART1CR&=~0x301;
		break;
	case 2:
		rUART2CR&=~0x301;
		break;
	case 3:
		rUART3CR&=~0x301;
		break;
	}

	if (is_wifi_module() && get_machine_type() == D210 && cur_port == 3)
	{
		writel_and(~0x0C000,GIO3_R_GRPF3_AUX_SEL_MEMADDR);
		gpio_set_pin_type(GPIOD, 14, GPIO_INPUT);
		gpio_set_pin_type(GPIOD, 15, GPIO_INPUT);
		gpio_set_pull(GPIOD, 14, 0);
		gpio_set_pull(GPIOD, 15, 0);
		gpio_enable_pull(GPIOD, 14);
		gpio_enable_pull(GPIOD, 15);
	}

	if(port_cts_watch[cur_port])
	{
		//--disable cts interrupt
		gpio_set_pin_interrupt(GPIOB,5,0);
	}
	port_cts_watch[cur_port] = 0;

	if(IsConfigPinpad()){
		if(channel == P_RS232B && s_PINPAD_info.status==1) {
			PortOpen_std(P_PINPAD,s_PINPAD_info.attr);
		};
		if(resp == 0 && channel == P_PINPAD)s_PINPAD_info.status=0;
	}
	return resp;
}

/**************************************************************************
* 函数名称:
*           uchar PortStop(uchar channel)
* 功能描述:
*           停止指定逻辑串口
* 输入参数:
*           channel  串口逻辑号
* 返回值:
*          0             成功
*       其他值  失败
****************************************************************************/
uchar PortStop(uchar channel)
{
	ushort cur_port;

	cur_port = port_xtab[channel];
	
	if(cur_port>=PHY_UART)
	{
		errno = 601;//invalid channel no
		return 2;
	}
	
	port_used[cur_port] = 0;
	//--disable UARTn TX interrupt
	//UartSetTxInt(cur_port,0);

	if(port_cts_watch[cur_port])
	{
		//--disable EINT23/cts interrupt
		gpio_set_pin_interrupt(GPIOB,5,0);
	}

	port_cts_watch[cur_port] = 0;

	return 0;
}

/**************************************************************************
* 函数名称:
*           uchar PortSend(uchar channel, uchar ch)
* 功能描述:
*           从指定串口发送一个字符
* 输入参数:
*           channel  串口逻辑号
*           ch          要发送的字符
* 返回值:
*          0             成功
*       其他值  失败
****************************************************************************/
uchar PortSend_std(uchar channel, uchar ch)
{
	ushort cur_port;
	T_SOFTTIMER uart_timer;

	if(channel==P_USB_DEV) return UsbDevSends(&ch,1);
	if (channel == P_USB) return ftdi_port_send(ch);
	if(channel==P_USB_HOST) return UsbHostSends(&ch,1);
    if(channel==P_USB_HOST_A && get_machine_type()==S800)
        return OhciSends(&ch, 1);

	errno = 0;
	//1--check validality of channel no
	if(channel >= CHANNEL_LIMIT)
	{
		errno = 101;//invalid channel no
		return 2;
	}

	cur_port = port_xtab[channel];
	if((cur_port >= PHY_UART) || !port_used[cur_port])
	{
		errno = 102;//channel not open
		return 3;
	}

	if((channel == P_MODEM) && modem_occupied())
	{
		errno = 120;//modem is running
		return 0xf0;
	}

	if((channel==P_WNET)&&(wnet_ppp_used())) return 0xf0;
	if ((channel==P_WIFI) && (!is_wifi_module())) return 0xf0;
	s_TimerSetMs(&uart_timer,500);//500 ms

	while(so_read[cur_port] == (so_write[cur_port]+1)%SIO_SIZE)
	{
	    if(!s_TimerCheckMs(&uart_timer))
	    {
		    errno = 103;//overflowed
		    return 4;
	    }
	}

	so_pool[cur_port][so_write[cur_port]] = ch;
	so_write[cur_port] = (so_write[cur_port]+1)%SIO_SIZE;

	uart_phy_sends(cur_port);

	return 0;
}


uchar PortTx(uchar channel, uchar ch)//for internal use only
{
	ushort cur_port;

	cur_port = port_xtab[channel];
	if(cur_port>=PHY_UART)return 2;

	so_pool[cur_port][so_write[cur_port]] = ch;
	so_write[cur_port] = (so_write[cur_port]+1)%SIO_SIZE;

	uart_phy_sends(cur_port);
	return 0;
}

/**************************************************************************
* 函数名称:
*           uchar PortTxs(uchar channel,uchar *str,ushort str_len)
* 功能描述:
*           从指定串口发送一串字符
* 输入参数:
*           channel  串口逻辑号
*           str          要发送的字符串
*           str_len   要发送的字符串长度
* 返回值:
*          0             成功
*       其他值  失败
****************************************************************************/
uchar PortTxs(uchar channel,uchar *str,ushort str_len)
{
	ushort cur_port,xlen,o_read,o_write;

	cur_port=port_xtab[channel];
    if(cur_port>=PHY_UART)return 2;

	o_write=so_write[cur_port];
	o_read=so_read[cur_port];
	if(o_read<=o_write)xlen=o_write-o_read;
	else xlen=SIO_SIZE-o_read+o_write;

	if(str_len+xlen >= SIO_SIZE)//overflow,SIO_SIZE
	{
		//errno = 406;//overflowed after appending
		return 4;
	}

	if(str_len+o_write < SIO_SIZE)
		memcpy((uchar*)so_pool[cur_port]+o_write,str,str_len);
	else
	{
		xlen=SIO_SIZE-o_write;
		memcpy((uchar*)so_pool[cur_port]+o_write,str,xlen);
		memcpy((uchar*)so_pool[cur_port],str+xlen,str_len-xlen);
	}
	so_write[cur_port]=(so_write[cur_port]+str_len)%SIO_SIZE;

	uart_phy_sends(cur_port);
	return 0;
}

/**************************************************************************
* 函数名称:
*           uchar PortRecv(uchar channel, uchar *ch, uint ms)
* 功能描述:
*           从指定串口接收一个字符
* 输入参数:
*           channel  串口逻辑号
*           ms        超时等待时间
* 输出参数:
*          ch         接收到的字符
* 返回值:
*          0             成功
*       其他值  失败
****************************************************************************/
uchar PortRecv_std(uchar channel, uchar *ch, uint ms)
{
	ushort cur_port;
	int tmpd;
	T_SOFTTIMER uart_timer;

	if(channel==P_USB_DEV)
	{
		tmpd=UsbDevRecvs(ch,1,ms);
		if(tmpd>0)return 0;
		if(!tmpd)return 0xff;
		return -tmpd;
	}
	if(channel == P_USB)
		return ftdi_port_recv(ch, ms);
	if(channel==P_USB_HOST)
	{
		tmpd=UsbHostRecvs(ch,1,ms);
		if(tmpd>0)return 0;
		if(!tmpd)return 0xff;
		return -tmpd;
	}

	if(channel==P_USB_HOST_A && get_machine_type()==S800)
	{
		tmpd=OhciRecvs(ch,1,ms);
		if(tmpd>0)return 0;
		if(!tmpd)return 0xff;
		return -tmpd;
	}


	errno = 0;
	//1--check validality of channel no
	if(channel >= CHANNEL_LIMIT)
	{
		errno = 201; //invalid channel no
		return 2;
	}

	cur_port = port_xtab[channel];
	if(cur_port >= PHY_UART || !port_used[cur_port])
	{
		errno = 202; //channel not open
		return 3;
	}

	if((channel == P_MODEM) && modem_occupied())
	{
		errno = 220; //modem is running
		return 0xf0;
	}
	
	if((channel==P_WNET)&&(wnet_ppp_used())) return 0xf0;
	if ((channel==P_WIFI) && (!is_wifi_module())) return 0xf0;

	if(ms)s_TimerSetMs(&uart_timer,ms);

	while(si_read[cur_port] == si_write[cur_port])
	{
	    if(!ms || !s_TimerCheckMs(&uart_timer))
	    {
		    errno = 204;
		    return 0xff;//timeout
	    }	
	}

	ch[0] = si_pool[cur_port][si_read[cur_port]];
	si_read[cur_port] = (si_read[cur_port] + 1) % SIO_SIZE;

	return 0;
}

int PortPeep_std(uchar channel,uchar *buf,ushort want_len)
{
	ushort cur_port,i,j;

	errno=0;
	if(buf == NULL)
	{
		return -1;
	}
	
	//1--check validality of channel no
	if(channel>=CHANNEL_LIMIT)
	{
		errno=801; //invalid channel no
		return -2;
	}

	cur_port=port_xtab[channel];
	if(cur_port>=PHY_UART || !port_used[cur_port])
	{
		errno=802; //channel not open
		return -3;
	}

	if(channel==P_MODEM && modem_occupied())
	{
		errno=820; //modem is running
		return -0xf0;
	}

	if((channel==P_WNET)&&(wnet_ppp_used())) return 0xf0;
	if ((channel==P_WIFI) && (!is_wifi_module())) return 0xf0;

	i=0;
	while(i<want_len)
	{
		j=(si_read[cur_port]+i)%SIO_SIZE;
		if(j==si_write[cur_port])break;

		buf[i]=si_pool[cur_port][j];
		i++;
	}//while(tx)

	return i;
}

int PortRecvs_std(uchar channel,uchar *pszBuf,ushort usRcvLen,ushort usTimeOut)
{
	ushort cur_port,i;
	T_SOFTTIMER uart_timer;

	if(channel==P_USB_DEV) return UsbDevRecvs(pszBuf,usRcvLen,usTimeOut);
	if(channel==P_USB) return ftdi_port_recvs(pszBuf,usRcvLen,usTimeOut);
	if(channel==P_USB_HOST) return UsbHostRecvs(pszBuf,usRcvLen,usTimeOut);
    if(channel==P_USB_HOST_A && get_machine_type()==S800) 
        return OhciRecvs(pszBuf, usRcvLen,usTimeOut);

	errno=0;
	//1--check validality of channel no
	if(channel>=CHANNEL_LIMIT)
	{
		errno=701; //invalid channel no
		return -2;
	}

	cur_port=port_xtab[channel];
	if(cur_port>=PHY_UART || !port_used[cur_port])
	{
		errno=702; //channel not open
		return -3;
	}

	if(channel==P_MODEM && modem_occupied())
	{
		errno=720; //modem is running
		return -0xf0;
	}

	if((channel==P_WNET)&&(wnet_ppp_used())) return 0xf0;
	if ((channel==P_WIFI) && (!is_wifi_module())) return 0xf0;
	if(usTimeOut)	 s_TimerSetMs(&uart_timer,usTimeOut);
	i=0;
	while(i<usRcvLen)
	{
		if(si_read[cur_port]==si_write[cur_port])
		{
			if(!usTimeOut)break;

			if(s_TimerCheckMs(&uart_timer))continue;

			if(i)break;

			errno=704;
			return -0xff;//timeout
		}

		pszBuf[i]=si_pool[cur_port][si_read[cur_port]];
		si_read[cur_port]=(si_read[cur_port]+1)%SIO_SIZE;
		i++;
	}//while(tx)

	return i;
}

#if 0
int PortRecvs_std_bt(uchar channel,uchar *pszBuf,ushort usRcvLen,ushort usTimeOut)
{
	ushort cur_port,i;
	T_SOFTTIMER uart_timer;

	if(channel==P_USB_DEV) return UsbDevRecvs(pszBuf,usRcvLen,usTimeOut);
	if(channel==P_USB) return ftdi_port_recvs(pszBuf,usRcvLen,usTimeOut);
	if(channel==P_USB_HOST) return UsbHostRecvs(pszBuf,usRcvLen,usTimeOut);
    if(channel==P_USB_HOST_A && get_machine_type()==S800) 
        return OhciRecvs(pszBuf, usRcvLen,usTimeOut);

	errno=0;
	//1--check validality of channel no
	if(channel>=CHANNEL_LIMIT)
	{
		errno=701; //invalid channel no
		return -2;
	}

	cur_port=port_xtab[channel];
	if(cur_port>=PHY_UART || !port_used[cur_port])
	{
		errno=702; //channel not open
		return -3;
	}

	if(channel==P_MODEM && modem_occupied())
	{
		errno=720; //modem is running
		return -0xf0;
	}

	if((channel==P_WNET)&&(wnet_ppp_used())) return 0xf0;
	if ((channel==P_WIFI) && (!is_wifi_module())) return 0xf0;
	if(usTimeOut)	 s_TimerSetMs(&uart_timer,usTimeOut);
	i=0;
	while(i<usRcvLen)
	{
		if(si_read[cur_port]==si_write[cur_port])
		{
			if(!usTimeOut)break;

			if(s_TimerCheckMs(&uart_timer))continue;

			if(i)break;

			errno=704;
			return -0xff;//timeout
		}

		pszBuf[i]=si_pool_bt[cur_port][si_read_bt[cur_port]];
		//printk("pszBuf[%d]:%x  ", i, pszBuf[i]);
		si_read_bt[cur_port]=(si_read_bt[cur_port]+1)%SIO_SIZE;
		i++;
	}//while(tx)

	return i;
}
#endif

/**************************************************************************
* 函数名称:
*           uchar PortRx(uchar channel,uchar *ch)
* 功能描述:
*           从指定串口接收一个字符
* 被以下函数调用:
*            内部函数
* 调用以下函数:
*           无
* 输入参数:
*           channel  串口逻辑号
* 输出参数:
*          ch         接收到的字符
* 返回值:
*          0             成功
*       其他值  失败
* 历史记录:
*          修改人       修改日期                 修改内容              版本号
*         潘平彬          2007-05-22                        创建                    01-01-01
****************************************************************************/
uchar PortRx(uchar channel,uchar *ch)//for internal use only
{
	ushort cur_port;

	cur_port = port_xtab[channel];

	if(si_read[cur_port] == si_write[cur_port])
		return 1;

	ch[0] = si_pool[cur_port][si_read[cur_port]];
	si_read[cur_port] = (si_read[cur_port] + 1)%SIO_SIZE;

	return 0;
}

/**************************************************************************
* 函数名称:
*           uchar PortTxPoolCheck(uchar channel)
* 功能描述:
*           检测发送缓冲区是否为空
* 被以下函数调用:
*            上层函数
* 调用以下函数:
*           无
* 输入参数:
*           channel  串口逻辑号
* 输出参数:
*           无
* 返回值:
*          0             成功
*       其他值  失败
* 历史记录:
*          修改人       修改日期                 修改内容              版本号
*         潘平彬          2007-05-22                        创建                    01-01-01
****************************************************************************/
uchar PortTxPoolCheck_std(uchar channel)
//--check if it's finished
{
	ushort cur_port;

	if(channel==P_USB_DEV) return UsbDevTxPoolCheck();
	if(channel==P_USB) return ftdi_port_pool_check();
	if(channel==P_USB_HOST) return UsbHostTxPoolCheck();
    if(channel==P_USB_HOST_A && get_machine_type()==S800)
        return OhciTxPoolCheck();

	errno = 0;
	//1--check validality of channel no
	if(channel >= CHANNEL_LIMIT)
	{
		errno = 301;//invalid channel no
		return 2;
	}

	cur_port = port_xtab[channel];
	if(cur_port >= PHY_UART || !port_used[cur_port])
	{
		errno = 302;//channel not open
		return 3;
	}

	if(so_read[cur_port] != so_write[cur_port])
		return 1;//not finished

	return 0;
}

/**************************************************************************
* 函数名称:
*           uchar PortSends(uchar channel, uchar *str,ushort str_len)
* 功能描述:
*          从串口发送一串数据
* 被以下函数调用:
*            上层函数
* 调用以下函数:
*           无
* 输入参数:
*           channel  串口逻辑号
*           str          发送的数据
*           str_len   发送的数据长度
* 输出参数:
*           无
* 返回值:
*          0             成功
*       其他值  失败
* 历史记录:
*          修改人       修改日期                 修改内容              版本号
*         潘平彬          2007-05-22                        创建                    01-01-01
****************************************************************************/
uchar PortSends_std(uchar channel, uchar *str,ushort str_len)
{
	ushort cur_port = 0, xlen = 0, o_read = 0, o_write = 0;
	uint UART_BASE;

	if(channel==P_USB_DEV) return UsbDevSends(str,str_len);
	if(channel==P_USB) return ftdi_port_sends(str, str_len);
	if(channel==P_USB_HOST) return UsbHostSends(str,str_len);
    if(channel==P_USB_HOST_A && get_machine_type()==S800)
        return OhciSends(str, str_len);

	errno = 0;
	//1--check validality of channel no
	if(channel >= CHANNEL_LIMIT)
	{
		errno = 401;//invalid channel no
		return 2;
	}

	cur_port = port_xtab[channel];
	if(cur_port >= PHY_UART|| !port_used[cur_port])
	{
		errno = 402;//channel not open
		return 3;
	}

	if((channel == P_MODEM) && modem_occupied())
	{
		errno = 420;//modem is running
		return 0xf0;
	}

	if((channel==P_WNET)&&(wnet_ppp_used())) return 0xf0;
	if ((channel==P_WIFI) && (!is_wifi_module())) return 0xf0;

	//--check if it will be overflowed
	if(str_len >= SIO_SIZE)
	{
		errno = 403;//overflowed for too long input
		return 4;
	}


	o_write = so_write[cur_port];
	o_read = so_read[cur_port];
	
	if(o_read <= o_write)
	{
		xlen = o_write - o_read;
	}
	else
	{
	    xlen = SIO_SIZE - o_read + o_write;
	}

	if((str_len + xlen) >= SIO_SIZE)//overflow,SIO_SIZE
	{
		errno = 406;//overflowed after appending
		return 4;
	}


	if((str_len + o_write) < SIO_SIZE)
	{
		memcpy((uchar*)so_pool[cur_port] + o_write, str, str_len);
	}
	else
	{
		xlen = SIO_SIZE-o_write;
		memcpy((uchar*)so_pool[cur_port]+o_write,str,xlen);
		memcpy((uchar*)so_pool[cur_port],str+xlen,str_len-xlen);
	}
	so_write[cur_port] = (so_write[cur_port] + str_len)%SIO_SIZE;

	uart_phy_sends(cur_port);

	return 0;
}

uchar PortReset_std(uchar channel)
{
	ushort cur_port;

	if(channel==P_USB_DEV) return UsbDevReset();
	if (channel == P_USB) return ftdi_port_reset();
	if(channel==P_USB_HOST) return UsbHostReset();
    if(channel==P_USB_HOST_A && get_machine_type()==S800)
        return OhciReset();

	errno = 0;
	//1--check validality of channel no
	if(channel >= CHANNEL_LIMIT)
	{
		errno = 501;//invalid channel no
		return 2;
	}

	cur_port = port_xtab[channel];
	if(cur_port >= PHY_UART || !port_used[cur_port])
	{
		errno = 502;//channel not open
		return 3;
	}

	if((channel == P_MODEM) && modem_occupied())
	{
		errno = 520;//modem is running
		return 0xf0;
	}

	if((channel==P_WNET)&&(wnet_ppp_used())) 
	{ 
		return 0xf0;
	}

	//--disable  UART interrupts
	si_read[cur_port] =0;
	si_write[cur_port] =0;
	//UartSetTxInt(cur_port,0);

	
	return 0;
}

/*only for BASE*/

int LinkComm_UartOpen(int baud_rate)
{
	return UartOpen(3,baud_rate,8,0,1);   
}

void LinkComm_UartStop(void)
{
	//--disable  UART3 interrupt
	disable_irq(IRQ_OUART3);
} 

uchar LinkCommTx(uchar ch)//for internal use only
{
	ushort cur_port =3;
	if(so_write[cur_port]>SIO_SIZE)return -1;
	so_pool[cur_port][so_write[cur_port]] = ch;
	so_write[cur_port] = (so_write[cur_port]+1)%SIO_SIZE;
	uart_phy_sends(cur_port);
	return 0;
}

uchar LinkCommTxs(uchar *str,ushort str_len)
{
	ushort cur_port,xlen,o_read,o_write;
	cur_port=3;

	o_write=so_write[cur_port];
	o_read=so_read[cur_port];
	if(o_read<=o_write)xlen=o_write-o_read;
	else xlen=SIO_SIZE-o_read+o_write;

	if(str_len+xlen >= SIO_SIZE) return 4;//overflow,SIO_SIZE
	if(str_len+o_write < SIO_SIZE)
		memcpy((uchar*)so_pool[cur_port]+o_write,str,str_len);
	else
	{
		xlen=SIO_SIZE-o_write;
		memcpy((uchar*)so_pool[cur_port]+o_write,str,xlen);
		memcpy((uchar*)so_pool[cur_port],str+xlen,str_len-xlen);
	}
	so_write[cur_port]=(so_write[cur_port]+str_len)%SIO_SIZE;
	uart_phy_sends(cur_port);
	return 0;
}

uchar LinkCommTxPoolCheck(void)
{
	if(so_write[3]!=so_read[3])return 1;//not empty

	return 0;//tx pool is empty
}


uchar LinkCommRx(uchar *ch)//for internal use only
{
	ushort cur_port =3;
	if(si_read[cur_port] == si_write[cur_port])	return 1;
	ch[0] = si_pool[cur_port][si_read[cur_port]];
	si_read[cur_port] = (si_read[cur_port] + 1)%SIO_SIZE;
	return 0;
}

uchar LinkCommReset(void)
{
	ushort cur_port=3;
	si_read[cur_port] =0;
	si_write[cur_port] =0;
	return 0;
}

#ifdef DEBUG_COMM
//for debug
static int DebugUartOpen(int port_no,int baud_rate,uint data_bit,uchar parity,uint stop_bit)
{
	uint status = 0;    
	uint temp;
	uint divider;
	uint remainder;
	uint fraction;
	uint UART_BASE; 

	switch (port_no)
	{
		case 0:
			UART_BASE =URT0_REG_BASE_ADDR;
			*(volatile uint*)GIO0_R_GRPF0_AUX_SEL_MEMADDR |=0x00180000; /*gpio_uart_enable */
			break;
		case 1:
			UART_BASE =URT1_REG_BASE_ADDR;
			break;
		case 2:
			UART_BASE =URT2_REG_BASE_ADDR;
			*(volatile uint*)GIO3_R_GRPF3_AUX_SEL_MEMADDR |=0x0C00; /*gpio_uart_enable */
			break;
		
		case 3:
			UART_BASE =URT3_REG_BASE_ADDR;
			*(volatile uint*)GIO3_R_GRPF3_AUX_SEL_MEMADDR |=0xC000; /*gpio_uart_enable */
			break;
		
		default:
			return ;
			break;
	}


	/* disable before configure*/
	*(volatile ushort*)(UART_BASE + UART_CR)  &=~0x0001; //URT_R_UARTCR_MEMADDR

	/*** Set baud rate
	** IBRD = UART_CLK / (16 * BPS)
	** FBRD = ROUND((64 * MOD(UART_CLK,(16 * BPS))) / (16 * BPS))
	******/
	temp      = 16 * baud_rate;
	divider   = UART_REF_CLK_FREQ / temp;
	remainder = UART_REF_CLK_FREQ % temp;
	fraction  = (4 * remainder) / baud_rate;
	if(data_bit == 7) data_bit =UART_DATABIT_7;
	else data_bit =UART_DATABIT_8;

	if(stop_bit ==2) stop_bit =UART_STOPBIT_2;
	else 	 stop_bit =UART_STOPBIT_1;

	if(parity ==2) parity =UART_PARITY_ODD;
	else if(parity ==1) parity =UART_PARITY_EVEN;
	else parity =UART_PARITY_NONE;

	*(volatile uint*)(UART_BASE + UART_IBRD) =divider; //URT_R_UARTIBRD_MEMADDR
	*(volatile uint*)(UART_BASE + UART_FBRD) =fraction; //URT_R_UARTFBRD_MEMADDR
	*(volatile uint*)(UART_BASE + UART_LCRH) =(data_bit | stop_bit | parity | UART_LCRH_FEN); //URT_R_UARTLCR_MEMADDR
	*(volatile ushort*) (UART_BASE + UART_CR) |=UART_ENABLE_RX_TX; //URT_R_UARTCR_MEMADDR

}

unsigned char DebugRecv ()
{
	unsigned char data;
	while (*((volatile uint *)(URT2_REG_BASE_ADDR+ UART_FR)) & 0x10) ;
	
	data = *((volatile uint *)(URT2_REG_BASE_ADDR)) & 0xFF;
	return data;
}

int DebugTstc()
{
     return (*((volatile uint *)(URT2_REG_BASE_ADDR+ UART_FR)) & 0x10);
}


void DebugSend (uchar c)
{
	//--UARTFR:D5-TX Fifo Full,D7-TX Fifo Empty
	while (*((volatile uint*)(URT2_REG_BASE_ADDR+ UART_FR)) & 0x20);
	(*((volatile uint *)URT2_REG_BASE_ADDR) = c);
	
	//while (*((volatile uint *)(URT2_REG_BASE_ADDR+ UART_FR)) & 0x20);

}

void DebugSends(uchar *str, int len)
{
	int i;
	for(i=0;i<len;i++)
		DebugSend(str[i]);
}


#if 1
void printk(const char *str, ...)
{
	va_list varg;
	int retv;
	volatile int i,j;
    char sbuffer[200];  
	static int first_flag=0;
	if(!first_flag )
	{
		//--power on UART
//		gpio_set_pin_type(GPIOD,13,GPIO_OUTPUT);
//		gpio_set_pin_val(GPIOD, 13, 0 );

		if (get_machine_type() == S900 || get_machine_type() == D210 
			|| get_machine_type() == D200)
		{
	        gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);//PWR_nSLP
	        gpio_set_pin_val(GPIOB,31, 0);//power open
		}

		DebugUartOpen(2,115200,8,0,1);//use COM1 port
		//first_flag =1;
	}
    memset(sbuffer, 0, sizeof(sbuffer));
    va_start( varg, str );                  
    retv=vsprintf(sbuffer,  str,  varg); 
    va_end( varg );
	DebugSends((unsigned char*)sbuffer, strlen((char *)sbuffer));
}

void print_hex(char *buf, int len)
{
	int i;

	for(i=0; i<len; i++)
		printk("%02x ", buf[i]);
}

#else /*only for S60*/
void printk(const char *str, ...)
{
	va_list varg;
	int retv;
	char sbuffer[20*1024];  
	int i,j,len;
	static int first_flag=0;
	if(!first_flag )
	{
		PortOpen_std(1,"115200,8,N,1");//use PINPAD port
		disable_irq(IRQ_OUART3);
		first_flag =1;
	}
    memset(sbuffer, 0, sizeof(sbuffer));
    va_start( varg, str );                  
    retv=vsprintf(sbuffer,  str,  varg); 
    va_end( varg );
    len = strlen(sbuffer);
	for(i=0;i<len;i++)
    {   
        while (readl(URT3_REG_BASE_ADDR+ UART_FR) & 0x20);
        writel(sbuffer[i],URT3_REG_BASE_ADDR);
    }
}
#endif

void ComTest(void)
{
	uchar tmpc,fn,port_no,tmps[10300],xstr[10300];
	int tmpd,rn,tn,i;
	unsigned long byte_count,speed,elapse,cur_loop;

	printk("COM TEST,120331b\n");

	port_no=3;//3--PINPAD
	tmpd=PortOpen(port_no,"115200,8,n,1");
	//PortClose(port_no);

	DelayMs(10000);
	tmpd=PortOpen(port_no,"115200,8,n,1");
	printk(" PORT%d OPEN:%d.\n",port_no,tmpd);
/*
	tmpd=PortRecvs(port_no,xstr,8192,0);
	printk(" PORT%d RECV:%d.\n",port_no,tmpd);
	while(1);
*/
	for(i=0;i<sizeof(tmps);i++)tmps[i]=i&0xff;
	cur_loop=0;
	tn=1;
	while(1)
	{
		cur_loop++;
		printk("LOOPS:%d...\n",cur_loop);

		tmps[0]=tn%0xff;
		tmpd=PortSends(port_no,tmps,tn);
		if(tmpd)
		{
			while(1)
			{
				printk(" TX FAILED:%d,tn:%d.\n",tmpd,tn);
				DelayMs(3000);
			}
		}

		rn=0;
		tmpd=PortRecvs(port_no,xstr,tn,20000);//2000
		if(tmpd)rn=tmpd;
		if(tmpd<=0)
		{
			while(1)
			{
				printk(" RX FAILED:%d,rn:%d,%u.\n",tmpd,rn,GetTimerCount());
				DelayMs(3000);
			}
		}
		if(rn!=tn)
		{
			while(1)
			{
				printk(" DLEN MISMATCH,tn:%d,rn:%d,%u.\n",tn,rn,GetTimerCount());
				DelayMs(1000);
			}
		}
		if(memcmp(tmps,xstr,tn))
		{
			while(1)
			{
				printk(" DATA MISMATCH:\n");
				for(i=0;i<tn;i++)printk("%02X ",xstr[i]);
				printk("\n");
				DelayMs(3000);
			}
		}

		tn=(tn+1)%8193;
		if(!tn)tn=1;
	}//while(1)
}

#endif

int iDPrintf(char * fmt,...)
{
	int iLen,iRet,iRetry=3;
	va_list args;
	unsigned char szPrintf[1024*8+1];
	
	if(port_used[2]==0)
	{
		PortClose(0);
		iRet=PortOpen(0,"115200,8,N,1");
		 if(iRet)return 1;
	}

	va_start(args,fmt);
	iLen = vsnprintf(szPrintf,sizeof(szPrintf)-1, fmt, args);
	va_end(args);

	if(iLen>sizeof(szPrintf))
	{
		PortSends(0, "Monitor", 8);
		PortSends(0,"iPortPrintf:pszPrintf over flow!halt!",37);
		while(1)
		{
		}
	}
	iRet = PortSends(0,(uchar *)szPrintf,iLen);
	while(iRet!=0)
	{
		DelayMs(20);
		iRet = PortSends(0,(uchar *)szPrintf,iLen);
		if(iRetry--<=0) return -2;
	}
	//while(PortTxPoolCheck(DBGCOM));

	return 0;
}

#ifdef PRINTK_DEBUG
void printk(const char *str, ...)
{
	va_list varg;
	int retv;
	char sbuffer[2100];  
	volatile int i,j;
	static int first_flag=0;

	if(!first_flag )
	//if(first_flag < 10 )
	{
		//--power on UART
		gpio_set_pin_type(GPIOD,13,GPIO_OUTPUT);
		gpio_set_pin_val(GPIOD, 13, 0 );

		PortOpen_std(0,"115200,8,n,1");//use COM1 port
		first_flag =1;
	}
    memset(sbuffer, 0, sizeof(sbuffer));
    va_start( varg, str );                  
    retv=vsprintf(sbuffer,  str,  varg); 
    va_end( varg );
	PortSends_std(0,(unsigned char*)sbuffer, strlen((char *)sbuffer));
}
#endif

