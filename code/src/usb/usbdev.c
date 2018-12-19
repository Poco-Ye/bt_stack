//2016.11.17,Added the close of power interrupt in reset_udev_state() to avoid SysSleep() failure
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "base.h"
#include "usbdev.h"
#include  "..\comm\comm.h"

//#define USB_TEST

//static const uchar USB_DEV_VER[]="86A,120731\0";
//static const uchar USB_DEV_VER[]="86B,120903\0";
static const uchar USB_DEV_VER[]="86C,161117\0";

//--error code define of API call
#define USB_ERR_NOTOPEN    3
#define USB_ERR_BUF        4
#define USB_ERR_NOTFREE    5
#define USB_ERR_NOCONF     11
#define USB_ERR_DISCONN    12
#define USB_ERR_REATTACHED 13
#define USB_ERR_CLOSED     14
#define USB_ERR_TIMEOUT    0xff

//--register definition
#define rVIC_INT_ENABLE   (*(volatile unsigned int*)0x01029010)
#define rVIC0_INT_ENABLE  (*(volatile unsigned int*)0x0102A010)
#define rVIC0_INT_ENCLEAR (*(volatile unsigned int*)0x0102A014)
#define rGPB_DIN          (*(volatile unsigned int*)0x000CD800)
#define rGPB_INT_MSK      (*(volatile unsigned int*)0x000CD818)

#define rG_AHB_CFG        (*(volatile unsigned int*)0x000A0008)
#define rG_USB_CFG        (*(volatile unsigned int*)0x000A000C)
#define rG_RST_CTL        (*(volatile unsigned int*)0x000A0010)
#define rG_INT_STS        (*(volatile unsigned int*)0x000A0014)
#define rG_INT_MSK        (*(volatile unsigned int*)0x000A0018)
#define rG_HW_CFG1        (*(volatile unsigned int*)0x000A0044)/*ep direction*/
#define rG_HW_CFG2        (*(volatile unsigned int*)0x000A0048)
#define rG_HW_CFG3        (*(volatile unsigned int*)0x000A004C)
#define rG_HW_CFG4        (*(volatile unsigned int*)0x000A0050)
#define rG_RXFIFO_SIZE    (*(volatile unsigned int*)0x000A0024)
#define rG_NP_TXFIFO_SIZE (*(volatile unsigned int*)0x000A0028)

#define rWC_PHY_CTL      (*(volatile unsigned int*)0x000B0000)
#define rWC_GEN_CTL      (*(volatile unsigned int*)0x000B0004)

#define rD_CFG           (*(volatile unsigned int*)0x000A0800)
#define rD_CTL           (*(volatile unsigned int*)0x000A0804)
#define rD_STS           (*(volatile unsigned int*)0x000A0808)
#define rD_THR_CTL       (*(volatile unsigned int*)0x000A0830)
#define rD_OEP0_CTL      (*(volatile unsigned int*)0x000A0B00)
#define rD_IEP0_CTL      (*(volatile unsigned int*)0x000A0900)
#define rD_INT           (*(volatile unsigned int*)0x000A0818)
#define rD_INT_MSK       (*(volatile unsigned int*)0x000A081C)
#define rD_OEP_MSK       (*(volatile unsigned int*)0x000A0814)
#define rD_IEP_MSK       (*(volatile unsigned int*)0x000A0810)
#define rD_OEP0_TXSIZE   (*(volatile unsigned int*)0x000A0B10)
#define rD_OEP0_DMA      (*(volatile unsigned int*)0x000A0B14)
#define rD_OEP0_INT      (*(volatile unsigned int*)0x000A0B08)
#define rD_IEP0_TXSIZE   (*(volatile unsigned int*)0x000A0910)
#define rD_IEP0_DMA      (*(volatile unsigned int*)0x000A0914)
#define rD_IEP0_INT      (*(volatile unsigned int*)0x000A0908)

#define rD_OEP1_CTL      (*(volatile unsigned int*)0x000A0B20)
#define rD_IEP1_CTL      (*(volatile unsigned int*)0x000A0920)
#define rD_OEP1_TXSIZE   (*(volatile unsigned int*)0x000A0B30)
#define rD_OEP1_DMA      (*(volatile unsigned int*)0x000A0B34)
#define rD_OEP1_INT      (*(volatile unsigned int*)0x000A0B28)
#define rD_IEP1_TXSIZE   (*(volatile unsigned int*)0x000A0930)
#define rD_IEP1_DMA      (*(volatile unsigned int*)0x000A0934)
#define rD_IEP1_INT      (*(volatile unsigned int*)0x000A0928)

#define rHPRT              (*(volatile unsigned int*)0x000A0440)

//--constant parameters
#define EP0_BUF_SIZE 64
#define EP1_BUF_SIZE 64
#define EP_NUMS     3
#define RX_POOL_SIZE 65537
#define TX_POOL_SIZE 10241
#define DMA_OEP0_ADDR DMA_USB_BASE//0x40200000
#define DMA_OEP1_ADDR DMA_OEP0_ADDR+0x1000//0x40201000
#define DMA_IEP0_ADDR DMA_OEP1_ADDR+0x1000//0x40202000
#define DMA_IEP1_ADDR DMA_IEP0_ADDR+0x1000//0x40203000

enum PID_TYPE{P_SETUP=0x01,P_IN=0x02,P_OUT=0x04,P_SOF=0x08,P_DATA0=0x10,P_DATA1=0x20};

static volatile struct ST_UDEV_STATE
{
	int f_open;//flag for open
	int f_power;
	int f_config;
	int f_bulkio_reattach;//flag for bulkio and detach,1-bulkio done,2-detach done
	int f_close;
	int f_enable_hispeed;
	int cur_speed;
	int usb_need_watch;
	int enum_count;
	int reset_count;
}udev_state={-1,-1,-1,0,0,0,0,0,0,0};

static struct ST_TXN
{
	int ep_no;//endpoint number,0~15
	int d0_d1;//0:data0 used,1:data1 used
	int b0_b1;//parity,0:BD0 used,1:BD1 used
	int pid;
	int dn;//data bytes
	int sum_len;//optional for debug
}cur_txn;

struct ST_LAST_BIO
{
	int seq_no;
	int req_type;
	int data_len;
}last_bio;

#define MAXDATA     32763//2043
typedef struct
{
	unsigned char id;
	unsigned char req_type;//0:OUT,1:IN,2:STAT,3:RESET
	unsigned short len;
	unsigned char data[MAXDATA];
}__attribute__((__packed__)) ST_BULK_IO;

typedef struct
{
	int tx_buf_size;
	int rx_buf_size;
	int tx_left;
	int rx_left;
}__attribute__((__packed__)) ST_BIO_STATE;

static int task_p1[EP_NUMS],task_p2[EP_NUMS];
static int tx_data_no[EP_NUMS],rx_data_no[EP_NUMS];
uchar usb0_tx_pool[TX_POOL_SIZE],usb0_rx_pool[RX_POOL_SIZE];
static volatile int tx_read=0,tx_write=0,rx_read=0,rx_write=0;

static unsigned short maxdata=508;//default value
static unsigned int hostVer=0;
volatile uint usb_to_device_task=0;

extern int usb_host_state;//defined in usbhost.c
void watch_usb_interrupt(void);
static void interrupt_usbpower(void);
static void interrupt_usbdev(int id);
static int dev_setup(void);
static int dev_bulk_io(void);
static int dev_bulk_io_h(void);
static int step_tx_ep0(int dn,uchar *tx_data);
static int step_rx_ep0(int dn);
static int step_tx_ep1(int dn,unsigned char *tx_data);

//TODO: for D200
static int connect_flag = 0;//device connect flag,0- not connected ,1- connected
//TODO: for D200 <end

void GetUsbDevVersion(char *ver_str);
uchar UsbDevOpen(uchar EnableHighSpeed);
uchar UsbDevStop(void);
uchar UsbDevSends(char *buf,ushort size);
int UsbDevRecvs(uchar *buf,ushort want_len,ushort timeout_ms);
uchar UsbDevTxPoolCheck(void);
uchar UsbDevReset(void);

#ifdef USB_TEST
#define printk x_debug
extern volatile ushort port_used[];
uchar gpszPrintf[8193*2];
void x_debug(char * fmt,...)
{
	int i,iLen,iRet;
	va_list args;
	static int iPortOpen=0;

	if(iPortOpen==0 || !port_used[2])
	{
		iRet=PortOpen(0,"115200,8,N,1");//use COM1 port
		//iRet=PortOpen(3,"115200,8,N,1");//use PINPAD port
		 if(iRet)return;
		 iPortOpen = 1;
	}

	va_start(args,fmt);
	iLen=vsnprintf(gpszPrintf,sizeof(gpszPrintf)-1,fmt,args);
	va_end(args);

	if(iLen>sizeof(gpszPrintf))return;

	PortSends(0,gpszPrintf,iLen);
	//for(i=0;i<iLen;i++)PortSend(2,gpszPrintf[i]);
	return;
}
#endif

void GetUsbDevVersion(char *ver_str)
{
	strcpy(ver_str,USB_DEV_VER);
	return;
}

int verify_checksum(ST_BULK_IO *p_bio)
{
	uchar a,b;
	int i,dn;

	dn=p_bio->len+4;
	a=0;
	for(i=2;i<dn;i++)
	  a^=((uchar*)p_bio)[i];
	a^=p_bio->id&0x0f;
	a^=p_bio->req_type&0x0f;
	b=(p_bio->id&0xf0)+(p_bio->req_type>>4);
	if(a!=b)return 1;

	//--clear checksum field
	p_bio->id&=0x0f;
	p_bio->req_type&=0x0f;
	return 0;
}

void set_checksum(ST_BULK_IO *p_bio)
{
	uchar a;
	int i,dn;

	dn=p_bio->len+4;
	a=0;
	for(i=2;i<dn;i++)
	  a^=((uchar*)p_bio)[i];
	a^=p_bio->id&0x0f;
	a^=p_bio->req_type&0x0f;

	//--fill high 4 bits of checksum into high 4 bits of ID field
	p_bio->id=(p_bio->id&0x0f) | (a&0xf0);

	//--fill low 4 bits of checksum into high 4 bits of REQ_TYPE field
	p_bio->req_type|=(a<<4);
	return;
}

void reset_udev_state(void)
{
	memset((uchar*)&udev_state,0x00,sizeof(udev_state));

	//--close usbdev's power interrupt to avoid SysSleep() failure,2016.11.17
	gpio_set_pin_interrupt(GPIOB,14,0);
}

void watch_usb_interrupt(void)
//1--watch the reset interrupt requested by the host
//2--watch the enumeration fall back
//3--watch the re-attach action through power_down and power_on trigger
{
	int i;

	if(!udev_state.usb_need_watch)return;

	//0--disable USB_INT interrupt to avoid rival risk
	rVIC0_INT_ENCLEAR=0x04000000;//d26:USB0

	//1--perform speed fall back following unsuccessful enumeration
	if(udev_state.usb_need_watch&0x80)
	{
		//--close all global USB interrupts
		rG_INT_MSK=0x00;

		//--wait for 10ms to initiate the following reset
//printk("  reset in timer:%d.\n",udev_state.reset_count);
		udev_state.reset_count++;
		if(udev_state.reset_count<2)return;

//printk("  start to reset in timer.\n");
		//11--set NAK bit of all out EPs
		for(i=0;i<16;i++)*(&rD_OEP0_CTL+i*8)|=0x08000000;//d27:NAK bit

		//12--select mode and interface
		rG_AHB_CFG|=0x21;//--d5:DMA enable,d0:interrupt enable
		if(!udev_state.f_enable_hispeed)rG_USB_CFG=0x4000A420;//device,PHY 48MHz,turnaround 9 clocks,USB2.0 HS UTMI,3 pins,UTMI,8 bits
		else rG_USB_CFG=0x40002420;//device,PHY 480MHz,turnaround 9 clocks,USB2.0 HS UTMI,3 pins,UTMI,8 bits

		//13--select mode and interface,use Full Speed for adaptability
		if(!udev_state.f_enable_hispeed)rD_CFG=0x03;//--d1d0:speed,01-Full Speed(60MHz,USB2.0),11-Full Speed(48MHz,USB1.1)
		else rD_CFG=0x00;//--d1d0:speed,0x00-High Speed(60MHz,USB2.0)

		rG_RXFIFO_SIZE=256;
		rG_NP_TXFIFO_SIZE=0x00100100;//d31-16:EP0 TxFifo size,d15-0:start address in fifo
		rD_OEP0_DMA=DMA_OEP0_ADDR;//DMA address for EP0 OUT
		rD_IEP0_DMA=DMA_IEP0_ADDR;//DMA address for EP0 IN
		rD_OEP0_TXSIZE=0x20080008;//d30d29:setup packet count,d19:packet count,d6-0:tx size

		if(rD_IEP0_CTL&0x80000000)//clear the last transfer
			rD_IEP0_CTL=0xc8000000;//d27:NAK bit,d25-22:TxFifo No
		else//--EP_ENABLE must not be set now;packet_size=64
			rD_IEP0_CTL=0x08000000;

		//14--clear device address
		rD_CFG&=0xfffff80f;//d10-4:device address

		//15--enable EP0 OUT and IN interrupt
		rD_INT_MSK=0x00010001;//d16:EP0 out,d0:EP0 IN
		rD_OEP_MSK=0x09;//d3:setup,d0:transfer
		rD_IEP_MSK=0x05;//d2:timeout,d0:transfer

		//16--initialize global system parameters
		memset(&cur_txn,0x00,sizeof(cur_txn));
		memset(tx_data_no,0x00,sizeof(tx_data_no));
		memset(rx_data_no,0x00,sizeof(rx_data_no));
		memset(task_p1,0x00,sizeof(task_p1));
		udev_state.f_config=0;
		udev_state.enum_count=0;
		udev_state.reset_count=0;
		if(!udev_state.f_enable_hispeed)udev_state.usb_need_watch=0;
		else udev_state.usb_need_watch=1;

		//17--enable EP IO interrupt
		rG_INT_MSK=0xc3002;//d13:speed detect done,d12:reset,d1:mode mismatch
								 //d19:OEP int,d18:IEP int

		//18--clear the RESET interrupt
		rG_INT_STS=0x1000;
	}//if(reset interrupt)

	//2--perform speed fall back following unsuccessful enumeration
	if(udev_state.usb_need_watch&0x01)
	{
//printk("  %d,D_STS:%08X.\n",udev_state.enum_count,rD_STS);
		udev_state.enum_count++;
		//--wait for 1s to fall back speed
		if(udev_state.enum_count>=100)//100
		{
//printk("  Speed falls back.\n");
			rD_CFG=0x01;//--d1d0:speed,01-Full Speed(60MHz,USB2.0),11-Full Speed(48MHz,USB1.1)
			udev_state.f_enable_hispeed=0;
			udev_state.usb_need_watch=0;
		}
	}//if(enumeration)

	//3--watch usb's power interrupt
	if(udev_state.usb_need_watch&0x10)
	{
		//--if power down
		if(rGPB_DIN&0x4000)//GPB14
		{
			//disable USB_INT interrupt
			rVIC0_INT_ENCLEAR=0x04000000;//d26:USB0

//printk("  WATCH,low level.\n");
			//select LOW level of trigger mode for POWER_INT
			s_setShareIRQHandler(GPIOB,14,INTTYPE_LOWLEVEL,interrupt_usbpower);//low level trigger

			udev_state.f_power=0;
			udev_state.f_config=0;
		}

//printk("  WATCH,power int on.\n");
		udev_state.usb_need_watch&=~0x10;

		//enable POWER interrupt
		gpio_set_pin_interrupt(GPIOB,14,1);
	}//if(power)

	//--enable USB interrupt
	rVIC0_INT_ENABLE=0x04000000;//d26:USB0
}

static void interrupt_usbpower(void)
{
	//--if connected to host and powered on
	if(!(rGPB_DIN&0x4000))//GPB14
	{
		//select LOW high of trigger mode for POWER_INT
		s_setShareIRQHandler(GPIOB,14,INTTYPE_HIGHLEVEL,interrupt_usbpower);//high level trigger

		//enable USB_INT interrupt
		rVIC0_INT_ENABLE=0x04000000;//d26:USB0

		udev_state.f_power=1;
		if(udev_state.f_bulkio_reattach==1)udev_state.f_bulkio_reattach=2;
	}
	else
	{
		//--if power down,start the power-down watch timer for debouncing confirmation
		udev_state.usb_need_watch|=0x10;//D4:power watch

		//disable POWER interrupt
		gpio_set_pin_interrupt(GPIOB,14,0);
	}
//printk("POWER INT:%08X,%d\n",rGPB_DIN,udev_state.f_power);
}

static void interrupt_usbdev(int id)
{
	//static uint loops=0;
	int status,dev_int,i,tmpd;

	status=rG_INT_STS;

//	printk("INT:%08X,%08X.\n",status,rD_INT_MSK);
	//loops++;

	//--process RESET interrupt
	if(status&0x1000)//1000
	{
//printk("  RESET...%u.\n",GetTimerCount());
		//1--disable watch timer interrupt
		udev_state.usb_need_watch=0;
        maxdata = 508;//default value
        hostVer=0;

		//1-close all global USB interrupts
		rG_INT_MSK=0x00;

		//2--reset usb PHY,and set to device and USB2.0
		rWC_PHY_CTL&=0x7fffffff;//b31:0-enter UTMI soft reset
		rWC_PHY_CTL|=0x80000000;//b31:1-exit UTMI soft reset

		rWC_PHY_CTL&=0xfbffafff;//d26:0-usb2.0,1-otg,d14:delay clock,d12:removing DP/DM pulls
		rWC_PHY_CTL|=0x02000000;//d25:0-host,1-device

		//3--reset USB hardware
		rG_RST_CTL=0x01;//d0:core soft reset
		for(i=0;i<100;i++)
			if(!(rG_RST_CTL&0x01))break;

		udev_state.usb_need_watch=0x80;
		//DelayMs(1);//--must wait for at least 800uS following the reset
	}

	//--process speed enumeration interrupt
	if(status&0x2000)
	{
		tmpd=(rD_STS>>1)&0x03;//d2d1:speed(00-High 60MHz,01-Full 60MHz,10-Low,11-Full 48MHz)
		udev_state.cur_speed=tmpd;
//printk("  %d,SPEED:%08X,%d,%u.\n",udev_state.usb_need_watch,rD_STS,udev_state.cur_speed,GetTimerCount());
//printk("  IEP0:%08X,OEP0:%08X\n",rD_IEP0_CTL,rD_OEP0_CTL);
//printk("  INT_MSK:%08X.\n",rG_INT_MSK);
		rD_IEP0_CTL=0x00;//d1d0:max packet size(00:64)
		rD_OEP0_CTL|=0x84000000;//d31:ep enable,d26:clear NAK

		//--clear the SPEED_ENUMERATION interrupt
		rG_INT_STS=0x2000;
	}

	if(!(status&0xc0000))//d19:OEP interrupt,d18:IEP interrupt
	{
		return;
	}

	dev_int=rD_INT;//D31-16:OutEpInt,D15-0:InEpInt
	if(!dev_int)
	{
		return;
	}

	//--process EP0 interrupt
	if(dev_int&0x10001)
	{
//printk("  D_INT:%08X,OEP0:%08X,IEP0:%08X.\n",dev_int,rD_OEP0_INT,rD_IEP0_INT);
		cur_txn.ep_no=0;
		cur_txn.pid=0;//invalid type

		if(dev_int&0x10000)
		{
			tmpd=rD_OEP0_INT;//d3:SETUP Phase Done
			if(tmpd&0x08)cur_txn.pid=P_SETUP;
			if(tmpd&0x01)cur_txn.pid|=P_OUT;
		}

		if(dev_int&0x0001)
		{
			tmpd=rD_IEP0_INT;//d7:TxFifo Empty,d1:disable,d0:transfer complete
			if(tmpd&0x01)cur_txn.pid|=P_IN;
		}
//printk("  pid:%02X.\n",cur_txn.pid);

		tmpd=dev_setup();
		//if(task_p1[0])goto INT_TAIL;
		if(tmpd)
		{
//printk("SETUP failed:%d\n",tmpd);
			task_p1[0]=0;

			//--reset EP0 to receive
			rD_OEP0_INT=rD_OEP0_INT;//clear the OEP0 interrupt
			rD_IEP0_INT=rD_IEP0_INT;//clear the IEP0 interrupt

			rD_OEP0_DMA=DMA_OEP0_ADDR;//DMA address for EP0 OUT
			rD_OEP0_TXSIZE=0x20080008;//d30d29:setup packet count,d19:packet count,d6-0:tx size
			rD_OEP0_CTL|=0x84000000;//d31:ep enable,d26:clear NAK
		}
	}//if(Ep0Int)

	//--process EP1 interrupt
	if(dev_int&0x20002)
	{
//printk("  D_INT:%08X,OEP1:%08X,IEP1:%08X.\n",dev_int,rD_OEP1_INT,rD_IEP1_INT);
		cur_txn.ep_no=1;
		cur_txn.pid=0;//invalid type
		cur_txn.dn=EP1_BUF_SIZE-(rD_OEP1_TXSIZE&0x3ffff);//d28d19:packet count,d18-0:tx size(64)

		if(dev_int&0x20000)
		{
			tmpd=rD_OEP1_INT;//d0:transfer complete
			if(tmpd&0x01)cur_txn.pid=P_OUT;
		}
		if(dev_int&0x0002)
		{
			tmpd=rD_IEP1_INT;//d7:TxFifo Empty,d0:transfer complete
			if(tmpd&0x01)cur_txn.pid|=P_IN;
		}

        if(hostVer>=0x300)
            tmpd=dev_bulk_io_h();
        else 
            tmpd=dev_bulk_io();
		if(tmpd)
		{
//printk("BULK_IO failed:%d\n",tmpd);
			task_p1[1]=0;
			rx_data_no[1]=0;
			tx_data_no[1]=0;

			//--pause EP1 receive
			rD_OEP1_CTL|=0x200000;//d21:stall handshake
			rD_OEP1_INT=rD_OEP1_INT;//clear the OEP1 interrupt
			rD_IEP1_INT=rD_IEP1_INT;//clear the IEP1 interrupt

//printk("IEP1_CTL:%08X.\n",rD_IEP1_CTL);
			if(rD_IEP1_CTL&0x80000000)
				rD_IEP1_CTL|=0x40200000;//clear the last transfer
			else rD_IEP1_CTL|=0x200000;//d21:stall handshake
		}
	}//if(Ep1Int)

	return;
}

static int dev_setup(void)
{
	static uchar tmps[100];
	static const uchar *ptr_tx_data;
	static int i,tn,tmpd,req_type,rsp_direction;
	static unsigned int want_dlen;

	switch(task_p1[0])
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	}

STEP0:
	task_p2[0]=0;
	task_p1[0]=1;
STEP1:
	//--wait for a setup packet
	while(1)
	{
		//if(cur_txn.ep_no)return 1;
		if(!(cur_txn.pid&P_SETUP))return 2;

		//--fetch the setup packet
		memcpy(tmps,(void*)DMA_OEP0_ADDR,8);
		rD_OEP0_INT=rD_OEP0_INT;//clear the EP0 interrupt

//for(i=0;i<8;i++)printk("%02X ",tmps[i]);
//printk("\n");

        if(tmps[0]==0x40 && tmps[1]==0x01)
        {
            hostVer=tmps[2]|(tmps[3]<<8);
            tn=0;
			rsp_direction=0;//response is rx
			//printk("\rhostVer:0x%X\n",hostVer);
			goto ANSWER;
        }

		req_type=tmps[1];
		if(req_type==6)req_type=(req_type<<8)+tmps[3];
		want_dlen=tmps[6]+(tmps[7]<<8);

//printk("REQ:%04X\n",req_type);
		tx_data_no[0]=1;
		switch(req_type)
		{
		case 0x601://GET_DEVICE_DESCRIPTION request
			//req:80060001 00004000
			if(want_dlen<sizeof(DESCRIPTOR_DEVICE))tn=want_dlen;
			else tn=sizeof(DESCRIPTOR_DEVICE);//18
			ptr_tx_data=DESCRIPTOR_DEVICE;
			rsp_direction=1;//response is tx
			break;
		case 0x602://GET_CONFIG_DESCRIPTION request
			//req:80060002 00000900
			if(want_dlen<sizeof(DESCRIPTOR_CONFIG))tn=want_dlen;
			else tn=sizeof(DESCRIPTOR_CONFIG);
			ptr_tx_data=DESCRIPTOR_CONFIG;
			rsp_direction=1;//response is tx
			break;
		case 0x603://GET_STRING_DESCRIPTION request
			tmpd=tmps[2];//get str_index
			if(!tmpd)
			{
				//80 06 00 03 00 00 FF 00
				if(want_dlen<sizeof(DESCRIPTOR_STR_0))tn=want_dlen;
				else tn=sizeof(DESCRIPTOR_STR_0);
				ptr_tx_data=DESCRIPTOR_STR_0;
			}
			else if(tmpd==1)
			{
				//80 06 01 03 09 04 00 01
				if(want_dlen<sizeof(DESCRIPTOR_STR_1))tn=want_dlen;
				else tn=sizeof(DESCRIPTOR_STR_1);
				ptr_tx_data=DESCRIPTOR_STR_1;
			}
			else if(tmpd==2)
			{
				//80 06 02 03 09 04 00 01
				if(want_dlen<sizeof(DESCRIPTOR_STR_2))tn=want_dlen;
				else tn=sizeof(DESCRIPTOR_STR_2);
				ptr_tx_data=DESCRIPTOR_STR_2;
			}
			else if(tmpd==4)
			{
				//80 06 04 03 09 04 00 01
				if(want_dlen<sizeof(DESCRIPTOR_STR_4))tn=want_dlen;
				else tn=sizeof(DESCRIPTOR_STR_4);
				ptr_tx_data=DESCRIPTOR_STR_4;
			}
			rsp_direction=1;//response is tx
			break;
		case 1://CLEAR_FEATURE request
//printk("  CLR FEATURE:%02X,%u.\n",tmps[4],GetTimerCount());
			if(!tmps[2] && !tmps[3])//clear ENDPOINT_HALT
			{
				//--activate IEP0
				if(rD_IEP0_CTL&0x80000000)//clear the last transfer
					rD_IEP0_CTL=0xc8000800;//d27:NAK bit,d25-22:TxFifo No
				else//--EP_ENABLE must not be set now;packet_size=64
					rD_IEP0_CTL=0x08000800;

				//reset endpoint
				if(tmps[4]==0x01)//OEP
				{
					//--activate OEP1
					//02 01 00 00 01 00 00 00
					rD_OEP1_CTL&=0xffdfffff;//d21:stall handshake
					rx_data_no[1]=0;
					task_p1[1]=0;

					//assign DMA address for EP1 OUT
//printk(" clr,oep1_ctl:%08X,TXSIZE:%08X,%08X\n",rD_OEP1_CTL,rD_OEP1_TXSIZE,rD_OEP1_DMA);
					rD_OEP1_DMA=DMA_OEP1_ADDR;//DMA address for EP1 OUT
					rD_OEP1_TXSIZE=0x80000+EP1_BUF_SIZE;//d28d19:packet count,d18-0:tx size(64)
					rD_OEP1_CTL=0x94088200;

					//--clear the EP1's RxFifo
					rG_RST_CTL=0x10;//d4:RxFifo flush
//printk(" CLR,oep1_ctl:%08X,TXSIZE:%08X,%08X\n",rD_OEP1_CTL,rD_OEP1_TXSIZE,rD_OEP1_DMA);
				}
				else if(tmps[4]==0x81)//IEP
				{
//printk(" clear,iep1_ctl:%08X,TXSIZE:%08X,%08X\n",rD_IEP1_CTL,rD_IEP1_TXSIZE,rD_IEP1_DMA);

					//--activate IEP1
					if(rD_IEP1_CTL&0x80000000)//clear the last transfer
					{
						//d31,ep enable,d30:disable,d29:set DATA1,d28:set DATA0,d27:set NAK
						//d26:clear NAK,d25-22:TxFifo No,d19d18:EP type,10-BULK
						//d15:activate,d14-11:next ep_no,d10-0:max packet size
						rD_IEP1_CTL=0xd8088a00;//note:DATA0 and NAK bits must be set
					}
					else
					{
						//--EP_ENABLE must not be set now;packet_size=1024 or 64
						rD_IEP1_CTL=0x18088a00;
					}

					//--clear the EP1's TxFifo
					rG_RST_CTL=0x20;//d10-6:TxFifo No,d5:TxFifo flush
//printk(" CLEAR,iep1_ctl:%08X,TXSIZE:%08X,%08X\n",rD_IEP1_CTL,rD_IEP1_TXSIZE,rD_IEP1_DMA);

					//02 01 00 00 81 00 00 00
					tx_data_no[1]=0;
					task_p1[1]=0;
				}
			}
			tn=0;
			rsp_direction=0;//response is rx
			break;
		case 5://SET_ADDRESS request
			//--set USB address
			rsp_direction=0;//response is rx
			tn=0;
			// TODO: for D200
			connect_flag = 1;
			// TODO: for D200 <end
			//--if SetAddress request,update device address
			rD_CFG&=0xfffff80f;//d10-4:device address
			tmps[2]&=0x7f;
			rD_CFG|=tmps[2]<<4;
//printk("  SetAddr:%08X.\n",rD_CFG);
			break;
		case 9://SET_CONFIG request
			tmpd=tmps[2];//get config_index
			if(!tmpd);
			else if(tmpd==1)
			{
				//--activate EP1
				//d31,ep enable,d29:set DATA1,d28:set DATA0,
				//d26:clear NAK,d25-22:TxFifo No
				//d19d18:EP type,10-BULK,d15:activate,d10-0:max packet size
				//rD_IEP1_CTL=0x10088840;
				//--EP_ENABLE must not be set now;packet_size=1024 or 64
				rD_IEP1_CTL=0x10088a00;

				//assign DMA address for EP1 OUT
				rD_OEP1_DMA=DMA_OEP1_ADDR;//DMA address for EP1 OUT
				rD_OEP1_TXSIZE=0x80000+EP1_BUF_SIZE;//d28d19:packet count,d18-0:tx size(64)
				rD_OEP1_CTL=0x94088200;

				//--enable EP1 interrupt
				rD_INT_MSK|=0x00020002;//d17:EP1 out,d1:EP1 IN

				//if(udev_state.f_bulkio_reattach==2)udev_state.f_bulkio_reattach=0;

				if(udev_state.f_config!=1)
				{
                    maxdata=508;
					udev_state.f_config=1;
					rx_data_no[1]=0;
					tx_data_no[1]=0;
				}
//printk("  EP1 started.\n");
			}
			else
			{
				//stall endpoint0
				rD_IEP0_CTL|=0x200000;//d31,ep enable,d30:ep disable,d21:stall handshake

				//--re-trigger for receiving the next SETUP packet
				//re-assign DMA address for EP0 OUT
				rD_OEP0_DMA=DMA_OEP0_ADDR;//DMA address for EP0 OUT
				rD_OEP0_TXSIZE=0x20080008;//d30d29:setup packet count,d19:packet count,d6-0:tx size
				rD_OEP0_CTL|=0x84000000;//d31:ep enable,d26:clear NAK

				task_p1[0]=1;
				return 0;
			}
			rsp_direction=0;//response is rx
			break;
		case 0x0b://SET_INTERFACE request
			tn=0;
			rsp_direction=0;//response is rx
			break;
		default:
			//req:80060006 00000A00,get DEVICE_QUALIFIER
			rD_IEP0_CTL|=0x200000;//d31,ep enable,d30:ep disable,d21:stall handshake

			//--re-trigger for receiving the next SETUP packet
			//re-assign DMA address for EP0 OUT
			rD_OEP0_DMA=DMA_OEP0_ADDR;//DMA address for EP0 OUT
			rD_OEP0_TXSIZE=0x20080008;//d30d29:setup packet count,d19:packet count,d6-0:tx size
			rD_OEP0_CTL|=0x84000000;//d31:ep enable,d26:clear NAK

			task_p1[0]=1;
			return 0;
		}//switch(req_type)
ANSWER:
		//--send packet
		if(rsp_direction)
		{
			task_p1[0]=2;
STEP2:
			tmpd=step_tx_ep0(tn,(uchar*)ptr_tx_data);
		}
		else
		{
			task_p1[0]=3;
STEP3:
			tmpd=step_rx_ep0(tn);
		}

		if(!tmpd && task_p2[0])return 0;
		if(tmpd)
		{
//printk("RSP result:%d\n",tmpd);
			return 6;
		}

		//--re-trigger for receiving the next SETUP packet
		//re-assign DMA address for EP0 OUT
		rD_OEP0_DMA=DMA_OEP0_ADDR;//DMA address for EP0 OUT
		rD_OEP0_TXSIZE=0x20080008;//d30d29:setup packet count,d19:packet count,d6-0:tx size
		rD_OEP0_CTL|=0x84000000;//d31:ep enable,d26:clear NAK

		if(udev_state.f_config==1)break;

		task_p1[0]=1;
		return 0;
	}//while(1)

	task_p1[0]=0;
	return 0;
}

static int step_tx_ep0(int dn,uchar *tx_data)
{
	static int tn,i;
	static unsigned char cn;

	switch(task_p2[0])
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	}

STEP0:
	tn=dn;
	i=0;

	task_p2[0]=1;
STEP1:
	if(tn>=EP0_BUF_SIZE)cn=EP0_BUF_SIZE;
	else cn=tn;

	//--fill data into out DMA buffer
//printk("TX0 begins,cn:%d,tn:%d,%08X.\n",cn,tn,rD_IEP0_CTL);
	memcpy((void*)DMA_IEP0_ADDR,tx_data+i,cn);

	//1--set for sending
	//re-assign DMA address for EP0 IN
	rD_IEP0_DMA=DMA_IEP0_ADDR;//re-assign DMA address for EP0 IN
	rD_IEP0_TXSIZE=0x80000+cn;//d20d19:packet count,d6-0:transfer size
	rD_IEP0_CTL|=0x84000000;//d31:EP enable,d26:clear NAK

	task_p2[0]=2;return 0;
STEP2:
//printk("  TX0 middle...\n");
	//--wait ACK for tx
	if(!(cur_txn.pid&P_IN))return 1;
	//if(cur_txn.d0_d1!=1)return 2;

	rD_IEP0_INT=rD_IEP0_INT;//clear the IEP0 interrupt
	tx_data_no[0]=(tx_data_no[0]+1)%2;
	tn-=cn;
	i+=cn;

	if(tn){task_p2[0]=1;goto STEP1;}

	//--get ready for STATUS receive
	rD_OEP0_TXSIZE=0x80000+EP0_BUF_SIZE;//64,d20d19:packet count,d6-0:transfer size

	// EP enable
	rD_OEP0_CTL|=0x84000000;//d31:EP enable,d26:clear NAK

	task_p2[0]=3;return 0;
STEP3:
	//--wait STATUS packet
	if(!(cur_txn.pid&P_OUT))return 3;
	//if(cur_txn.d0_d1!=1)return 4;

	rD_OEP0_INT=rD_OEP0_INT;//clear the OEP0 interrupt

//printk("  TX0 END:%08X.\n",rD_OEP0_DMA);
	task_p2[0]=0;
	return 0;
}

static int step_rx_ep0(int dn)
{
	switch(task_p2[0])
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	}

STEP0:

	//--send status(a NULL packet in DATA1)
//printk("RX0 begins,%08X,",rD_IEP0_CTL);
	rD_IEP0_DMA=DMA_IEP0_ADDR;//re-assign DMA address for EP0 IN
	rD_IEP0_TXSIZE=0x80000;//d20d19:packet count,d6-0:transfer size
	rD_IEP0_CTL|=0x84000000;//d31:EP enable,d26:clear NAK

	task_p2[0]=1;return 0;
STEP1:
	//--wait ACK for status's tx
	if(!(cur_txn.pid&P_IN))return 11;
	//if(cur_txn.d0_d1!=1)return 12;

	rD_IEP0_INT=rD_IEP0_INT;//clear the IEP0 interrupt

//printk("RX0 ends\n");
	task_p2[0]=0;
	return 0;
}

static int dev_bulk_io(void)
{
	static uchar tmps[MAXDATA+10];
	static int i,rn,tn,tmpd;
	static ushort want_dlen;
	static ST_BULK_IO *ptr_bulk,bulk_out;
	static ST_BIO_STATE bio_state;

	switch(task_p1[1])
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	}

STEP0:
	task_p2[1]=0;
	task_p1[1]=1;
	bio_state.tx_buf_size=TX_POOL_SIZE-1;
	bio_state.rx_buf_size=RX_POOL_SIZE-1;
	rn=0;
STEP1:
	//--wait for a bulk_io packet
	while(1)
	{
		if(!(cur_txn.pid&P_OUT))return 1;
		if(cur_txn.dn<0)return 2;

		if(rD_OEP1_CTL&0x10000)cur_txn.d0_d1=0;//d16:DATA PID No
		else cur_txn.d0_d1=1;
//		if(cur_txn.d0_d1!=rx_data_no[1])return 3;

		//--fetch the packet
		memcpy(tmps+rn,(void*)DMA_OEP1_ADDR,cur_txn.dn);
		rD_OEP1_INT=rD_OEP1_INT;//clear the EP1 interrupt
		rx_data_no[1]=(rx_data_no[1]+1)%2;

		//--re-trigger the next rx
		//re-assign DMA address for EP1 OUT
		rD_OEP1_DMA=DMA_OEP1_ADDR;//DMA address for EP1 OUT
		rD_OEP1_TXSIZE=0x80000+EP1_BUF_SIZE;//d28d19:packet count,d18-0:tx size(64)

		//d31,ep enable,d29:set DATA1,d28:set DATA0,
		//d26:clear NAK,d25-22:TxFifo No
		//d19d18:EP type,10-BULK,d15:activate,d10-0:max packet size
		rD_OEP1_CTL|=0x84000000;
		//rD_IEP1_CTL|=0x04000000;//d26:clear NAK

//if(!rn)
//{
//for(i=0;i<cur_txn.dn;i++)printk("%02X ",tmps[rn+i]);
//printk("\n");
//}
		//--receive a full packet
		rn+=cur_txn.dn;
//printk("rn:%d ",rn);
		if(rn<4){task_p1[1]=1;return 0;}

		ptr_bulk=(ST_BULK_IO *)tmps;
		//if(!memcmp(tmps,"\xff\xff\xff\xff",4)){task_p1[1]=1;return 0;}//interrupt was blocked
		if(rn<(int)ptr_bulk->len+4){task_p1[1]=1;return 0;}
		if(verify_checksum(ptr_bulk))
		{
//printk("\nCHECKSUM false,REQ seq:%02X,type:%02X,RN:%d\n",ptr_bulk->id,ptr_bulk->req_type,rn);
			task_p1[1]=0;
			return 0;
		}

//printk("\nSEQ:%02X,type:%02X,RN:%d,O:%d ",ptr_bulk->id,ptr_bulk->req_type,rn,cur_txn.d0_d1);

		//--finish remaining work of last BULK_IN
		//  i.e. update usb0_tx_pool's pointer
		if(last_bio.req_type==1 &&
			(ptr_bulk->id!=last_bio.seq_no || ptr_bulk->req_type!=1))
		{
			tx_read=(tx_read+last_bio.data_len)%TX_POOL_SIZE;
//bio_state.tx_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;
//printk("%02X,TN:%d,left:%d\n",ptr_bulk->id,last_bio.data_len,bio_state.tx_left);
		}

		switch(ptr_bulk->req_type)
		{
        case 4://get data infomation from version 2.00
            rn=0;
			bulk_out.req_type=4;
			bulk_out.id=ptr_bulk->id;
			bulk_out.len=2;

            maxdata = sizeof(bulk_out.data);
            if(maxdata>508)maxdata=508;
			memcpy(bulk_out.data,&maxdata,2);

			tn=bulk_out.len+4;
			break;

		case 3://reset
			task_p1[1]=1;
			rn=0;
			return 0;
		case 0://BULK OUT
//printk("OUT-%02X,%u...",ptr_bulk->id,GetTimerCount());
			//memset(&bulk_out,0x00,sizeof(ST_BULK_IO));
			bulk_out.req_type=2;

			//--discard the repeated data coming from host
			if(last_bio.req_type==0 && ptr_bulk->id==last_bio.seq_no)goto ANSWER_STATUS;

			rn=0;
			//--move data from ep1_rx_buf to usb0_rx_pool
			tmpd=ptr_bulk->len;
			bio_state.rx_left=RX_POOL_SIZE-1-(rx_write+RX_POOL_SIZE-rx_read)%RX_POOL_SIZE;
			if(tmpd>bio_state.rx_left)
			{
				tmpd=0;//discard the rx data to avoid overflow
				bulk_out.req_type=0;//assign a false type to indicate system error
			}

			if(rx_write+tmpd<=RX_POOL_SIZE)
				memcpy(usb0_rx_pool+rx_write,ptr_bulk->data,tmpd);
			else
			{
				memcpy(usb0_rx_pool+rx_write,ptr_bulk->data,RX_POOL_SIZE-rx_write);
				memcpy(usb0_rx_pool,ptr_bulk->data+RX_POOL_SIZE-rx_write,tmpd+rx_write-RX_POOL_SIZE);
			}
/*
for(mismatch=0,i=0;i<tmpd;i++)
{
	a=usb0_rx_pool[(rx_write+i)%RX_POOL_SIZE];
	if(a && a!=last_cc+1)
	{
		printk("RX MISMATCH,dn:%d,i:%d-%02X\n",tmpd,i,a);
		mismatch=1;
	}
	last_cc=a;
}
*/
			rx_write=(rx_write+tmpd)%RX_POOL_SIZE;
//printk("%d,",tmpd);
//bio_state.rx_left-=tmpd;
//printk("%02X,RN:%d,RP:%d\n",ptr_bulk->id,tmpd,bio_state.rx_left);
		case 2://get status
//printk("GET_STATUS\n");
ANSWER_STATUS:
			udev_state.f_bulkio_reattach=1;//bulkio done

			rn=0;
			if(ptr_bulk->req_type)
			{
				//memset(&bulk_out,0x00,sizeof(ST_BULK_IO));
				bulk_out.req_type=2;
			}
			bulk_out.id=ptr_bulk->id;
			bulk_out.len=sizeof(bio_state);

			bio_state.rx_left=RX_POOL_SIZE-1-(rx_write+RX_POOL_SIZE-rx_read)%RX_POOL_SIZE;
			bio_state.tx_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;
			memcpy(bulk_out.data,&bio_state,sizeof(bio_state));

//printk("STATUS REPLY,rl:%d,tl:%d\n",bio_state.rx_left,bio_state.tx_left);
//for(i=0;i<20;i++)printk("%02X ",*((uchar *)&bulk_out+i));
//printk("\n");

			tn=bulk_out.len+4;
			break;
		case 1://BULK IN
//printk("IN-%02X,%u...",ptr_bulk->id,GetTimerCount());
			if(ptr_bulk->len<2){task_p1[1]=0;return 0;}//discard the invalid packet

			rn=0;
			//memset(&bulk_out,0x00,sizeof(ST_BULK_IO));
			bulk_out.id=ptr_bulk->id;
			bulk_out.req_type=ptr_bulk->req_type;

			want_dlen=ptr_bulk->data[0]+(ptr_bulk->data[1]<<8);
			tmpd=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;
			if(tmpd>want_dlen)tmpd=want_dlen;
			//if(tmpd>sizeof(bulk_out.data))tmpd=sizeof(bulk_out.data);
            if(tmpd>maxdata) tmpd=maxdata;

			last_bio.data_len=tmpd;
			if(!tmpd)goto ANSWER_STATUS;
			bulk_out.len=tmpd;

			if(tx_read+tmpd<=TX_POOL_SIZE)memcpy(bulk_out.data,usb0_tx_pool+tx_read,tmpd);
			else
			{
				memcpy(bulk_out.data,usb0_tx_pool+tx_read,TX_POOL_SIZE-tx_read);
				memcpy(bulk_out.data+TX_POOL_SIZE-tx_read,usb0_tx_pool,tmpd+tx_read-TX_POOL_SIZE);
			}

//printk("%d,",tmpd);
			tn=bulk_out.len+4;
			break;
		default:
//printk("\nREQ_TYPE false,REQ seq:%02X,type:%02X,RN:%d\n",ptr_bulk->id,ptr_bulk->req_type,rn);
			task_p1[1]=0;
			return 0;
		}
		//--backup last_bio for retry process
		last_bio.seq_no=ptr_bulk->id;
		last_bio.req_type=ptr_bulk->req_type;

		//--fill checksum field
		set_checksum(&bulk_out);

		//--send packet
		task_p1[1]=2;
STEP2:
		tmpd=step_tx_ep1(tn,(uchar*)&bulk_out);
		if(!tmpd && task_p2[1])return 0;
		if(tmpd)
		{
//printk("RSP failed:%d\n",tmpd);
			return 6;
		}

//if(ptr_bulk->req_type<2)printk(" end:%u.\n",GetTimerCount());
		task_p1[1]=1;return 0;
	}//while(1)

	task_p1[1]=0;
	return 0;
}


static int dev_bulk_io_h(void)
{
	static uchar tmps[MAXDATA+10];
	static int i,rn,tn,tmpd;
	static ushort want_dlen;
	static ST_BULK_IO *ptr_bulk,bulk_out;
	static ST_BIO_STATE bio_state;
    static uchar check_sum;
    static uint check_i;

	switch(task_p1[1])
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	}

STEP0:
	task_p2[1]=0;
	task_p1[1]=1;
	bio_state.tx_buf_size=TX_POOL_SIZE-1;
	bio_state.rx_buf_size=RX_POOL_SIZE-1;
	rn=0;
    check_sum=0;
    check_i=0;
STEP1:
	//--wait for a bulk_io packet
	while(1)
	{
		if(!(cur_txn.pid&P_OUT))return 1;
		if(cur_txn.dn<0)return 2;

		if(rD_OEP1_CTL&0x10000)cur_txn.d0_d1=0;//d16:DATA PID No
		else cur_txn.d0_d1=1;

		//--fetch the packet
		memcpy(tmps+rn,(void*)DMA_OEP1_ADDR,cur_txn.dn);
		rD_OEP1_INT=rD_OEP1_INT;//clear the EP1 interrupt
		rx_data_no[1]=(rx_data_no[1]+1)%2;

		//--re-trigger the next rx
		//re-assign DMA address for EP1 OUT
		rD_OEP1_DMA=DMA_OEP1_ADDR;//DMA address for EP1 OUT
		rD_OEP1_TXSIZE=0x80000+EP1_BUF_SIZE;//d28d19:packet count,d18-0:tx size(64)

		//d31,ep enable,d29:set DATA1,d28:set DATA0,
		//d26:clear NAK,d25-22:TxFifo No
		//d19d18:EP type,10-BULK,d15:activate,d10-0:max packet size
		rD_OEP1_CTL|=0x84000000;
		//rD_IEP1_CTL|=0x04000000;//d26:clear NAK

//if(!rn)
//{
//for(i=0;i<cur_txn.dn;i++)printk("%02X ",tmps[rn+i]);
//printk("\n");
//}
		//--receive a full packet
		rn+=cur_txn.dn;
//printk("rn:%d ",rn);
		if(rn<4){task_p1[1]=1;return 0;}

		ptr_bulk=(ST_BULK_IO *)tmps;

        for(;check_i<rn;check_i++)
        {
            check_sum ^=((uchar *)ptr_bulk)[check_i];
            if(ptr_bulk->req_type==0 && check_i>=4 && check_i!=(ptr_bulk->len+4-1))
                usb0_rx_pool[(rx_write+check_i-4)%RX_POOL_SIZE] = ptr_bulk->data[check_i-4];
        }
        
		if(rn<(int)ptr_bulk->len+4)
        {
            task_p1[1]=1;
            return 0;
        }

        if(check_sum)
        {
            //printk("\rErr check_sum:0x%X,ptr_bulk->len:%d,seq:0x%X\n",check_sum,
            //    ptr_bulk->len,ptr_bulk->id);
            //for(i=0;i<rn;i++)printk("%02X ",tmps[i]);
            //printk("\n");
            task_p1[1]=0;
            return 0;
        }
        if(ptr_bulk->len==0)
        {
            //printk("\rErr len = 0\n");
            task_p1[1]=0;
            return 0;
        }
        ptr_bulk->len -=1;
		
//printk("\nSEQ:%02X,type:%02X,RN:%d,O:%d ",ptr_bulk->id,ptr_bulk->req_type,rn,cur_txn.d0_d1);
        
		//--finish remaining work of last BULK_IN
		//  i.e. update usb0_tx_pool's pointer
		if(last_bio.req_type==1 &&
			(ptr_bulk->id!=last_bio.seq_no || ptr_bulk->req_type!=1))
		{
			tx_read=(tx_read+last_bio.data_len)%TX_POOL_SIZE;
//bio_state.tx_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;
//printk("%02X,TN:%d,left:%d\n",ptr_bulk->id,last_bio.data_len,bio_state.tx_left);
		}

		switch(ptr_bulk->req_type)
		{
        case 4://get data infomation from version 2.00
            rn=0;
			bulk_out.req_type=4;
			bulk_out.id=ptr_bulk->id;
			bulk_out.len=2;

            maxdata = sizeof(bulk_out.data);

			memcpy(bulk_out.data,&maxdata,2);

			tn=bulk_out.len+4;
			break;

		case 3://reset
			task_p1[1]=0;
			rn=0;
			return 0;
            
		case 0://BULK OUT
			bulk_out.req_type=2;

			//--discard the repeated data coming from host
			if(last_bio.req_type==0 && ptr_bulk->id==last_bio.seq_no)goto ANSWER_STATUS;

			rn=0;
			//--move data from ep1_rx_buf to usb0_rx_pool
			tmpd=ptr_bulk->len;
			bio_state.rx_left=RX_POOL_SIZE-1-(rx_write+RX_POOL_SIZE-rx_read)%RX_POOL_SIZE;
			if(tmpd>bio_state.rx_left)
			{
				tmpd=0;//discard the rx data to avoid overflow
				bulk_out.req_type=0;//assign a false type to indicate system error
			}

			rx_write=(rx_write+tmpd)%RX_POOL_SIZE;

		case 2://get status
ANSWER_STATUS:
			udev_state.f_bulkio_reattach=1;//bulkio done

			rn=0;
			if(ptr_bulk->req_type)
			{
				bulk_out.req_type=2;
			}
			bulk_out.id=ptr_bulk->id;
			bulk_out.len=sizeof(bio_state);

			bio_state.rx_left=RX_POOL_SIZE-1-(rx_write+RX_POOL_SIZE-rx_read)%RX_POOL_SIZE;
			bio_state.tx_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;
			memcpy(bulk_out.data,&bio_state,sizeof(bio_state));

			tn=bulk_out.len+4;
			break;
            
		case 1://BULK IN
			if(ptr_bulk->len<2){task_p1[1]=0;return 0;}//discard the invalid packet

			rn=0;
			bulk_out.id=ptr_bulk->id;
			bulk_out.req_type=ptr_bulk->req_type;
            
			want_dlen=ptr_bulk->data[0]+(ptr_bulk->data[1]<<8);
			tmpd=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;
			if(tmpd>want_dlen)tmpd=want_dlen;
            if(tmpd>(maxdata-1)) tmpd=maxdata-1;

			last_bio.data_len=tmpd;
			if(!tmpd)goto ANSWER_STATUS;
			bulk_out.len=tmpd;
			tn=bulk_out.len+4;
			break;
		default:
//printk("\nREQ_TYPE false,REQ seq:%02X,type:%02X,RN:%d\n",ptr_bulk->id,ptr_bulk->req_type,rn);
			task_p1[1]=0;
			return 0;
		}
		//--backup last_bio for retry process
		last_bio.seq_no=ptr_bulk->id;
		last_bio.req_type=ptr_bulk->req_type;

		//--fill checksum field
        check_sum=0;
        check_i=0;
        bulk_out.len++;
        tn=bulk_out.len+4;
        
		//--send packet
		task_p1[1]=2;
STEP2:
        if((tn-1-check_i)>EP1_BUF_SIZE)
        {
            for(i=0;i<EP1_BUF_SIZE;i++,check_i++)
            {
                if(bulk_out.req_type==1 && check_i>=4 && check_i!=(bulk_out.len+4-1))
                    bulk_out.data[check_i-4]=usb0_tx_pool[(tx_read+check_i-4)%TX_POOL_SIZE];
                check_sum ^= ((uchar *)&bulk_out)[check_i];
            }
        }
        else if(tn-1-check_i)
        {
            for(;check_i<(tn-1);check_i++)
            {
                if(bulk_out.req_type==1 && check_i>=4 && check_i!=(bulk_out.len+4-1))
                    bulk_out.data[check_i-4]=usb0_tx_pool[(tx_read+check_i-4)%TX_POOL_SIZE];

                check_sum ^= ((uchar *)&bulk_out)[check_i];
            }
            bulk_out.data[bulk_out.len-1]=check_sum;
        }            

		tmpd=step_tx_ep1(tn,(uchar*)&bulk_out);
		if(!tmpd && task_p2[1])return 0;
		if(tmpd)
		{
//printk("RSP failed:%d\n",tmpd);
			return 6;
		}

        check_sum=0;
        check_i=0;
		task_p1[1]=1;return 0;
	}//while(1)

	task_p1[1]=0;
	return 0;
}

static int step_tx_ep1(int dn,unsigned char *tx_data)
{
	static int tn,i;
	static unsigned char cn;

	switch(task_p2[1])
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	}

STEP0:
	tn=dn;
	i=0;
	task_p2[1]=1;

STEP1:
	if(tn>=EP1_BUF_SIZE)cn=EP1_BUF_SIZE;
	else cn=tn;

	//1--fill data into DMA_IN buffer
	memcpy((void*)DMA_IEP1_ADDR,tx_data+i,cn);
	rD_IEP1_DMA=DMA_IEP1_ADDR;//re-assign DMA address for EP1 IN
//printk("  STEP_TX1 begins,cn:%d,tn:%d\n",cn,tn);
//if((tx_data[1]&0x0f)<2)printk("TX1-%02X,%08X-%08X,cn:%d-dn:%d\n",tx_data[0]&0x0f,rD_IEP1_CTL,rD_IEP0_CTL,cn,dn);
//if(loops>=100)printk(" %d,IEP1_CTL:%08X,TXSIZE:%08X\n",loops,rD_IEP1_CTL,rD_IEP1_TXSIZE);

	//2--set for sending
	rD_IEP1_TXSIZE=0x80000+cn;//d28d19:packet count,d18-0:tx size
	rD_IEP1_CTL|=0x84000000;//d31:EP enable,d26:clear NAK
//if(loops>=100)printk(" IEP1_CTL:%08X,TXSIZE:%08X\n",rD_IEP1_CTL,rD_IEP1_TXSIZE);

	task_p2[1]=2;return 0;
STEP2:
//if(loops>=100)printk(" %d,STEP_TX1 middle.\n",loops);
	//3--wait ACK for tx
	if(!(cur_txn.pid&P_IN))return 1;

	if(rD_IEP1_CTL&0x10000)cur_txn.d0_d1=0;//d16:DATA PID No
	else cur_txn.d0_d1=1;
	if(cur_txn.d0_d1!=tx_data_no[1])return 2;

	rD_IEP1_INT=rD_IEP1_INT;//clear the IEP1 interrupt
	tx_data_no[1]=(tx_data_no[1]+1)%2;

	tn-=cn;
	i+=cn;

	//4--to send a zero-length packet if a short packet with n*EP1_BUF_SIZE length
	//if((dn<sizeof(ST_BULK_IO)&&cn==EP1_BUF_SIZE) ||
		//(dn==sizeof(ST_BULK_IO)&&tn)){task_p2[1]=1;goto STEP1;}
    if(dn%512 && cn==EP1_BUF_SIZE ||
        (dn%512==0) && tn){task_p2[1]=1;goto STEP1;}


	task_p2[1]=0;
	return 0;
}

uchar UsbDevOpen(uchar EnableHighSpeed)
{
	int i;
    unsigned int tmpd;

	 if(usb_host_state>2)return USB_ERR_NOTFREE;//occupied by host

	 //--if opened already, then return at once
	 if(udev_state.f_open==1)//opened
	 {
//printk("dev re-open,120731A...\r");
		 //1--disable USB_INT interrupt
		rVIC0_INT_ENCLEAR=0x04000000;//d26:USB0

		memset(&last_bio,0xff,sizeof(last_bio));

		//--clear tx pool
		tx_read=0;tx_write=0;

		//--clear rx pool
		rx_read=0;rx_write=0;
		//--enable USB interrupt
		rVIC0_INT_ENABLE=0x04000000;//d26:USB0

		udev_state.f_close=0;
		usb_host_state=-1;//inhibit HOST
		return 0;
	 }
//printk("FULL dev open,120731A...\r");

	UsbHcdStop();
	if (get_machine_type() == D200)
	{
	    //disable otg_id interrupt
	    gpio_set_pin_type(GPIOB, 3, GPIO_INPUT);
	    gpio_set_pin_interrupt(GPIOB,3,0);

	    if(usb_to_device_task||((rG_INT_STS&1) ==0))
	    {
	        while(usb_to_device_task);

			 //1--disable USB_INT interrupt
			rVIC0_INT_ENCLEAR=0x04000000;//d26:USB0

			memset(&last_bio,0xff,sizeof(last_bio));

			//--clear tx pool
			tx_read=0;tx_write=0;

			//--clear rx pool
			rx_read=0;rx_write=0;
			//--enable USB interrupt
			rVIC0_INT_ENABLE=0x04000000;//d26:USB0

	        udev_state.f_open=1;//opened
			udev_state.f_close=0;
			usb_host_state=-1;//inhibit HOST
			return 0;
	    }

		//turn off vbus
		gpio_set_pin_type(GPIOB, 28, GPIO_OUTPUT);//USB0_HOST_ON
	    gpio_set_pin_val(GPIOB, 28, 0);
	}
    
	gpio_set_pin_val(GPIOB,12,0);
	gpio_set_pin_type(GPIOB,12,GPIO_OUTPUT);
	tmpd = rHPRT;
	tmpd&=~((1<<2)|(1<<1)|(1<<3)|(1<<5)|(1<<12));
	rHPRT = tmpd;

	 //1--disable USB_INT interrupt
	rVIC0_INT_ENCLEAR=0x04000000;//d26:USB0
	rG_INT_MSK=0;//close all usb interrupts

	//2--reset usb PHY,and set to device and USB2.0
	rWC_PHY_CTL&=0x7fffffff;//b31:0-enter UTMI soft reset
	DelayMs(1);
	rWC_PHY_CTL|=0x80000000;//b31:1-exit UTMI soft reset
	rWC_PHY_CTL&=0xfbffafff;//d26:0-usb2.0,1-otg,d14:delay clock,d12:removing DP/DM pulls
	rWC_PHY_CTL|=0x02000000;//d25:0-host,1-device

	//3--reset USB hardware
//printk(" RST_CTL:%08X.\n",rG_RST_CTL);
	rG_RST_CTL=0x01;//d0:core soft reset
	for(i=0;i<100;i++)
		if(!(rG_RST_CTL&0x01))break;
	if(i>=100)return USB_ERR_TIMEOUT;//device reset timeout

	//4--must wait for at least 800uS following the reset
	DelayMs(1);

	 //10-set IRQ type and priority
	s_SetInterruptMode(IRQ_OUSB0, INTC_PRIO_2, INTC_IRQ);
	s_SetIRQHandler(IRQ_OUSB0,interrupt_usbdev);//USB_INT
	//interrupt_usbdev_ftdi=interrupt_usbdev;

	//11--set NAK bit of all out EPs
	for(i=0;i<16;i++)*(&rD_OEP0_CTL+i*8)|=0x08000000;//d27:NAK bit

	//12--select mode and interface
	rG_AHB_CFG|=0x21;//--d5:DMA enable,d0:interrupt enable
	//d30:device mode,d29:host mode
	//d15:PHY frequency,0-480MHz,1-48Mhz
	//d13-10:turnaound time in PHY clock,1001 for 8-bit UTMI
	//d9:HNP,d8:SRP
	//d6:PHY select,0-USB2.0 HS UTMI,1-USB1.1 FS
	//d5:FS interface,0-6 pin unidirection,1-3 pin bidirection
	//d4:UTMI/ULPI select,0-UTMI,1-ULPI
	//d3:PHY interface width,0-8 bits,1-16 bits
	//d2-0:HS/FS timeout adding
	if(!EnableHighSpeed)rG_USB_CFG=0x4000A420;//device,PHY 48MHz,turnaround 9 clocks,USB2.0 HS UTMI,3 pins,UTMI,8 bits
	else rG_USB_CFG=0x40002420;//device,PHY 480MHz,turnaround 9 clocks,USB2.0 HS UTMI,3 pins,UTMI,8 bits
	DelayMs(30);//delay for at least 25ms

	if(!EnableHighSpeed)rD_CFG=0x03;//--d1d0:speed,01-Full Speed(60MHz,USB2.0),11-Full Speed(48MHz,USB1.1)
	else rD_CFG=0x00;//--d1d0:speed,00-High Speed(60MHz,USB2.0)

	//rD_THR_CTL|=0x0;//threshold control

	//13--close and clear interrupts
	rD_INT_MSK=0;//close all EP OUT and IN interrupts
	rG_INT_STS=rG_INT_STS;//clear all USB interrupts

	//14--configure EP0's buf_addr and RX mode
	rG_RXFIFO_SIZE=256;
	rG_NP_TXFIFO_SIZE=0x00100100;//d31-16:EP0 TxFifo size,d15-0:start address in fifo
	rD_OEP0_DMA=DMA_OEP0_ADDR;//DMA address for EP0 OUT
	rD_IEP0_DMA=DMA_IEP0_ADDR;//DMA address for EP0 IN
	rD_OEP0_TXSIZE=0x20080008;//d30d29:setup packet count,d19:packet count,d6-0:tx size
	rD_IEP0_CTL=0x00;//d1d0:max packet size(00:64 bytes)

	//15--reset relevant variables
	memset(&last_bio,0xff,sizeof(last_bio));
	memset(task_p1,0x00,sizeof(task_p1));
	memset((uchar*)&udev_state,0x00,sizeof(udev_state));
	memset(&cur_txn,0x00,sizeof(cur_txn));
	memset(rx_data_no,0x00,sizeof(rx_data_no));
	memset(tx_data_no,0x00,sizeof(tx_data_no));
	tx_read=0;tx_write=0;rx_read=0;rx_write=0;
	udev_state.f_open=1;//opened
	udev_state.f_enable_hispeed=EnableHighSpeed;

	//16--enable device interrupts:reset,speed detect done,sof
	//rG_INT_MSK=0x300a;//d13:speed detect done,d12:reset,d3:sof,d1:mode mismatch
	rG_INT_MSK=0x3002;//d13:speed detect done,d12:reset,d1:mode mismatch
	rVIC0_INT_ENABLE=0x04000000;//d26:USB0

	//--perform a soft disconnect and let HOST can enumerate
	rD_CTL|=0x02;//d1:SoftDisconnect
	DelayMs(10);//3
	rD_CTL&=~0x02;

	usb_host_state=-1;//inhibit HOST

	//17--set sub-interrupt for USBDEV_POWER(GPB14)
	gpio_set_pin_type(GPIOB,14,GPIO_INPUT);//set USB_POWER as input
	s_setShareIRQHandler(GPIOB,14,INTTYPE_LOWLEVEL,interrupt_usbpower);//low level trigger

	return 0;
}

uchar UsbDevStop(void)
{
	if(udev_state.f_open!=1)return USB_ERR_NOTOPEN;

	rx_read=rx_write;//--clear rx pool
	udev_state.f_close=1;
	usb_host_state=0;//enable HOST

	return 0;
}

uchar UsbDevReset(void)
{
	if(udev_state.f_close)return USB_ERR_CLOSED;
	if(udev_state.f_open!=1)return USB_ERR_NOTOPEN;
	if(udev_state.f_bulkio_reattach==2)return USB_ERR_REATTACHED;

	rx_read=rx_write;//--clear rx pool
	return 0;
}

uchar UsbDevTxPoolCheck(void)
{
	if(udev_state.f_close)return USB_ERR_CLOSED;
	if(udev_state.f_open!=1)return USB_ERR_NOTOPEN;
	if(udev_state.f_power!=1)return USB_ERR_DISCONN;
	if(udev_state.f_bulkio_reattach==2)return USB_ERR_REATTACHED;
	if(udev_state.f_config!=1)return USB_ERR_NOCONF;

	if(tx_read!=tx_write)return 1;//TX not finished

	return 0;
}

uchar UsbDevSends(char *buf,ushort size)
{
	ushort i;
	int tx_left;

	if(udev_state.f_close)return USB_ERR_CLOSED;
	if(udev_state.f_open!=1)return USB_ERR_NOTOPEN;
	if(udev_state.f_power!=1)return USB_ERR_DISCONN;
	if(udev_state.f_bulkio_reattach==2)return USB_ERR_REATTACHED;

	//--check if enough tx buffer
	tx_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;

	if(size>=TX_POOL_SIZE || size+tx_left>=TX_POOL_SIZE)
		return USB_ERR_BUF;//buffer occupied

	for(i=0;i<size;i++)
		usb0_tx_pool[(tx_write+i)%TX_POOL_SIZE]=buf[i];
	tx_write=(tx_write+size)%TX_POOL_SIZE;

	return 0;
}

int UsbDevRecvs(uchar *buf,ushort want_len,ushort timeout_ms)
{
	int i,j;
	uint t0,tout;

	if(udev_state.f_close)return -USB_ERR_CLOSED;
	if(udev_state.f_open!=1)return -USB_ERR_NOTOPEN;

	t0=GetTimerCount();
	tout=(timeout_ms+9)/10*10;//to be multiples of 10ms for compatibility

	i=0;
	while(i<want_len)
	{
		j=(rx_read+i)%RX_POOL_SIZE;
		if(j==rx_write)
		{
			if(udev_state.f_power!=1)
			{
				if(i)break;
				return -USB_ERR_DISCONN;
			}
			if(udev_state.f_bulkio_reattach==2)
			{
				if(i)break;
				return -USB_ERR_REATTACHED;
			}

			if(!timeout_ms)break;
			if(GetTimerCount()-t0<tout)continue;

			if(i)break;
			return -USB_ERR_TIMEOUT;
		}

		buf[i]=usb0_rx_pool[j];
		i++;
	}//while()
	rx_read=(rx_read+i)%RX_POOL_SIZE;

	return i;
}

// TODO: for D200
int s_UsbCheckConnect(void)
{
	return connect_flag;
}
void s_UsbSetConnectFlag(int flag)
{
	connect_flag = flag;
}
// TODO: for D200 <end

void s_UsbState(void)
{
	static uint tcnt=0;	
	int i;
    unsigned int tmpd;
	if (get_machine_type() != D200) return;
	switch(usb_to_device_task)
	{
	case 1:
	    if((rG_INT_STS&1) ==0)//device
   		{
			usb_to_device_task = 0;
        	break;
    	}

		//turn off vbus
		gpio_set_pin_type(GPIOB, 28, GPIO_OUTPUT);//USB0_HOST_ON
    	gpio_set_pin_val(GPIOB, 28, 0);
    
		gpio_set_pin_val(GPIOB,12,0);
		gpio_set_pin_type(GPIOB,12,GPIO_OUTPUT);
		tmpd = rHPRT;
		tmpd&=~((1<<2)|(1<<1)|(1<<3)|(1<<5)|(1<<12));
		rHPRT = tmpd;
		
	 	//disable otg_id interrupt
		gpio_set_pin_type(GPIOB, 3, GPIO_INPUT);
    	gpio_set_pin_interrupt(GPIOB,3,0);

	 	//1--disable USB_INT interrupt
		rVIC0_INT_ENCLEAR=0x04000000;//d26:USB0
		rG_INT_MSK=0;//close all usb interrupts

		//2--reset usb PHY,and set to device and USB2.0
		rWC_PHY_CTL&=0x7fffffff;//b31:0-enter UTMI soft reset
		DelayMs(1);
		rWC_PHY_CTL|=0x80000000;//b31:1-exit UTMI soft reset
		rWC_PHY_CTL&=0xfbffafff;//d26:0-usb2.0,1-otg,d14:delay clock,d12:removing DP/DM pulls
		rWC_PHY_CTL|=0x02000000;//d25:0-host,1-device

		//3--reset USB hardware
//printk(" RST_CTL:%08X.\n",rG_RST_CTL);
		rG_RST_CTL=0x01;//d0:core soft reset
		for(i=0;i<100;i++)
			if(!(rG_RST_CTL&0x01))break;
		if(i>=100)//device reset timeout
		{
			usb_to_device_task = 0;
			return;
		}

		//4--must wait for at least 800uS following the reset
		DelayMs(1);

	 	//10-set IRQ type and priority
		s_SetInterruptMode(IRQ_OUSB0, INTC_PRIO_2, INTC_IRQ);
		s_SetIRQHandler(IRQ_OUSB0,interrupt_usbdev);//USB_INT
		//interrupt_usbdev_ftdi=interrupt_usbdev;

		//11--set NAK bit of all out EPs
		for(i=0;i<16;i++)*(&rD_OEP0_CTL+i*8)|=0x08000000;//d27:NAK bit

		//12--select mode and interface
		rG_AHB_CFG|=0x21;//--d5:DMA enable,d0:interrupt enable
		//d30:device mode,d29:host mode
		//d15:PHY frequency,0-480MHz,1-48Mhz
		//d13-10:turnaound time in PHY clock,1001 for 8-bit UTMI
		//d9:HNP,d8:SRP
		//d6:PHY select,0-USB2.0 HS UTMI,1-USB1.1 FS
		//d5:FS interface,0-6 pin unidirection,1-3 pin bidirection
		//d4:UTMI/ULPI select,0-UTMI,1-ULPI
		//d3:PHY interface width,0-8 bits,1-16 bits
		//d2-0:HS/FS timeout adding
		rG_USB_CFG=0x4000A420;//device,PHY 48MHz,turnaround 9 clocks,USB2.0 HS UTMI,3 pins,UTMI,8 bits
		usb_to_device_task = 2;
		tcnt=0;
		break;

	case 2:		
		tcnt++;
		if(tcnt<4)return;//delay for at least 25ms

		rD_CFG=0x03;//--d1d0:speed,01-Full Speed(60MHz,USB2.0),11-Full Speed(48MHz,USB1.1)

		//rD_THR_CTL|=0x0;//threshold control

		//13--close and clear interrupts
		rD_INT_MSK=0;//close all EP OUT and IN interrupts
		rG_INT_STS=rG_INT_STS;//clear all USB interrupts

		//14--configure EP0's buf_addr and RX mode
		rG_RXFIFO_SIZE=256;
		rG_NP_TXFIFO_SIZE=0x00100100;//d31-16:EP0 TxFifo size,d15-0:start address in fifo
		rD_OEP0_DMA=DMA_OEP0_ADDR;//DMA address for EP0 OUT
		rD_IEP0_DMA=DMA_IEP0_ADDR;//DMA address for EP0 IN
		rD_OEP0_TXSIZE=0x20080008;//d30d29:setup packet count,d19:packet count,d6-0:tx size
		rD_IEP0_CTL=0x00;//d1d0:max packet size(00:64 bytes)

		//15--reset relevant variables
		memset(&last_bio,0xff,sizeof(last_bio));
		memset(task_p1,0x00,sizeof(task_p1));
		memset((uchar*)&udev_state,0x00,sizeof(udev_state));
		memset(&cur_txn,0x00,sizeof(cur_txn));
		memset(rx_data_no,0x00,sizeof(rx_data_no));
		memset(tx_data_no,0x00,sizeof(tx_data_no));
		tx_read=0;tx_write=0;rx_read=0;rx_write=0;
		udev_state.f_enable_hispeed=0;

		//16--enable device interrupts:reset,speed detect done,sof
		//rG_INT_MSK=0x300a;//d13:speed detect done,d12:reset,d3:sof,d1:mode mismatch
		rG_INT_MSK=0x3002;//d13:speed detect done,d12:reset,d1:mode mismatch
		rVIC0_INT_ENABLE=0x04000000;//d26:USB0

		//--perform a soft disconnect and let HOST can enumerate
		rD_CTL|=0x02;//d1:SoftDisconnect

		usb_to_device_task = 3;
		tcnt=0;
		break;

	case 3:		
		tcnt++;
		if(tcnt<1)return;//delay 10 ms
		rD_CTL&=~0x02;

		//17--set sub-interrupt for USBDEV_POWER(GPB14)
		gpio_set_pin_type(GPIOB,14,GPIO_INPUT);//set USB_POWER as input
		s_setShareIRQHandler(GPIOB,14,INTTYPE_LOWLEVEL,interrupt_usbpower);//low level trigger
		usb_to_device_task = 0;
		break;

	default:
		usb_to_device_task = 0;
		break;
	}
}
#ifdef USB_TEST
void UsbDevTests(void)
{
	uchar tmpc,fn,port_no,tmps[10300],xstr[10300];
	int tmpd,rn,tn,i,loops;
	unsigned long byte_count,speed,elapse;

	//s_ModemInit(2);
	ScrCls();

	PortOpen(0,"115200,8,n,1");
	while(1)
	{
T_BEGIN:
	ScrCls();
	ScrPrint(0,0,0,"USB DEV TEST,120724A");
	ScrPrint(0,1,0,"1-OpenDev 2-OpenHost");
	ScrPrint(0,2,0,"3-CHECK  4-CLEAR");
	ScrPrint(0,3,0,"5-STOP/START");
	ScrPrint(0,4,0,"6-TX/RX");
	ScrPrint(0,5,0,"7-RESTART");
	ScrPrint(0,6,0,"8-RAM_TEST 9-CLOSE");
	ScrPrint(0,7,0,"0-UDISK");

	fn=getkey();
	if(fn==KEYCANCEL)return;

	ScrCls();
	switch(fn)
	{
	case '0':
		UdiskTest();
		break;
	case '1':
		tmpd=PortOpen(P_USB_DEV,NULL);
		ScrPrint(0,7,0,"OPEN DEV RESULT:%d",tmpd);
		getkey();
		break;
	case '2':
		tmpd=PortOpen(P_USB_HOST,NULL);
		ScrPrint(0,7,0,"OPEN HOST RESULT:%d",tmpd);
		getkey();
		break;
	case '3':
		tmpd=UsbDevTxPoolCheck();
		ScrPrint(0,0,0,"RESULT:%d",tmpd);
		//ScrPrint(0,1,0,"WATCH:%02X",usb_need_watch);
		//ScrPrint(0,2,0,"IEN:%08X",INTC_EN_REG);

		ScrPrint(0,3,0,"F_INIT_START:%d",udev_state.f_open);
		ScrPrint(0,4,0,"F_POWER:%d",udev_state.f_power);
		ScrPrint(0,5,0,"F_CONFIG:%d",udev_state.f_config);
		ScrPrint(0,6,0,"F_BIO_DETACH:%d",udev_state.f_bulkio_reattach);
		ScrPrint(0,7,0,"F_CLOSE:%d",udev_state.f_close);
		getkey();
		break;
	case '4':
		break;
	case '5':
		PortClose(P_USB_DEV);
		PortOpen(P_USB_DEV,NULL);
		break;
	case '7':
		PortOpen(P_USB_HOST,NULL);
		PortClose(P_USB_HOST);
		PortOpen(P_USB_DEV,NULL);
		break;
	case '9':
		PortClose(P_USB_DEV);
		break;
	case '8':
		break;
	case '6'://TX/RX
		ScrCls();
		ScrPrint(0,0,0,"USB DEV TX/RX");

		ScrPrint(0,2,0,"SELECT PORT NO:");
		ScrPrint(0,3,0,"1-P_USB_DEV");
		ScrPrint(0,4,0,"2-P_USB_FTDI");
		tmpc=getkey();
		if(tmpc==KEYCANCEL)break;
		if(tmpc=='2')port_no=P_USB;
		else port_no=P_USB_DEV;

		ScrPrint(0,2,0,"SELECT TX LEN:");
		ScrPrint(0,3,0,"1-1  2-1025");
		ScrPrint(0,4,0,"3-2048 4-5000");
		ScrPrint(0,5,0,"5-8000 6-9000");
		ScrPrint(0,6,0,"7-10240 8-10241");
		ScrPrint(0,7,0,"9-61    0-62");
		tmpc=getkey();
		if(tmpc==KEYCANCEL)break;

		if(tmpc=='2')tn=1025;
		else if(tmpc=='3')tn=2048;
		else if(tmpc=='4')tn=5000;
		else if(tmpc=='5')tn=8000;
		else if(tmpc=='6')tn=9000;
		else if(tmpc=='7')tn=10240;
		else if(tmpc=='8')tn=10241;
		else if(tmpc=='9')tn=61;
		else if(tmpc=='0')tn=62;
		else tn=1;
		loops=0;
		ScrClrLine(2,7);

		byte_count=0;
		TimerSet(0,60000);
		while(61)
		{
			loops++;

			for(i=0;i<sizeof(tmps);i++)tmps[i]=i&0xff;
			if(!(loops%100))ScrPrint(0,1,0,"%d,SENDING %d ",loops,tn);
			tmpd=PortSends(port_no,tmps,tn);
			if(tmpd)
			{
				//Beep();
				ScrPrint(0,3,0,"%d,TN:%d,RET:%d     ",loops,tn,tmpd);
				getkey();
			}

			if(!(loops%100))ScrPrint(0,4,0,"%d,RECVING  ...",loops);
			rn=0;
			while(611)
			{
				if(!TimerCheck(0))
				{
					TimerSet(0,60000);
					byte_count=0;
				}
				if(!kbhit())
				{
					tmpc=getkey();
					if(tmpc==KEYCANCEL)
					{
						ScrPrint(0,6,0,"WAIT FOR CLEAR...");
						DelayMs(1000);
						PortReset(P_USB_DEV);
						goto T_BEGIN;
					}

					if(tmpc=='1')PortSends(0,xstr,rn);

					ScrPrint(0,6,0,"%d,RN:%d ",loops,rn);
					ScrPrint(0,3,0,"TM:%d ",TimerCheck(0));
					elapse=60000-TimerCheck(0);
					if(!elapse)elapse=1;
					speed=byte_count/elapse;
					speed*=10;
					ScrPrint(0,7,0,"SPEED:%d,%lu/%lu  ",speed,byte_count,elapse);
				}
				tmpd=PortRecvs(port_no,xstr+rn,10240,0);
				if(tmpd<0 && tmpd!=-0xff)
				{
					//Beep();
					ScrPrint(0,6,0,"RCV FAILED:%d,RN:%d",tmpd,rn);
					getkey();
					goto T_BEGIN;
				}
				rn+=tmpd;
				if(rn>=tn)break;
			}//while(611)

			if(!(loops%100))ScrPrint(0,6,0,"%d,RN:%d ",loops,rn);
			if(memcmp(tmps,xstr,tn))
			{
				//Beep();
				ScrPrint(0,6,0,"%d,DATA MISMATCH,DN:%d ",loops,rn);
				tmpc=getkey();
				PortSends(0,"AAAAA",5);
				for(i=0;i<tn;i++)
				{
					sprintf(tmps,"%02X",xstr[i]);
					PortSends(0,tmps,2);

					if(i && !(i%1000))
					{
						ScrPrint(0,7,0,"i:%d,left:%d...",i,tn-i-1);
						getkey();
					}
				}
				PortSends(0,"BBBBB",5);
				PortReset(port_no);
				if(tmpc==KEYCANCEL)goto T_BEGIN;

				ScrClrLine(2,7);
			}

			tn++;
			byte_count+=tn*2;
			//if(tn>=1025)tn=1;
			if(tn>=10240)tn=1;
		}
		break;
	}//switch()
	}//while(1)
}

void UsbDevTest(void)
{
	uchar tmpc,fn,port_no,tmps[10300],xstr[10300];
	int tmpd,rn,tn,i;
	unsigned long byte_count,speed,elapse,cur_loop;

T_BEGIN:
	ScrCls();
	ScrPrint(0,0,0,"STARTED,120731A\n");

	tmpd=UsbDevOpen(0);//1
	printk(" DEV OPEN1:%d.\n",tmpd);
	ScrPrint(0,1,0," DEV OPEN1:%d.\n",tmpd);

getkey();
/*
//tmpd=UsbDevOpen(0);//1
UsbDevStop();
tmpd=PortOpen(P_USB_HOST,"PaxDev");
ScrPrint(0,2,0," HOST OPEN:%d  \n",tmpd);
getkey();
tmpd=UsbDevOpen(0);//1
ScrPrint(0,3,0," DEV OPEN2:%d  \n",tmpd);
getkey();
*/
	for(i=0;i<sizeof(tmps);i++)tmps[i]=i&0xff;
	cur_loop=0;

	tn=10240;
	while(1)
	{
		cur_loop++;
//printk("LOOPS:%d...\n",cur_loop);
ScrPrint(0,2,0,"LOOPS:%d,TX...\n",cur_loop);

		//tmps[0]=tn>>8;
		//tmps[1]=tn&0xff;
		//tmpd=UsbDevSends(tmps,tn);
		for(i=0;i<tn;i++)
		{
			tmpd=UsbDevSends(tmps+i,1);
			ScrPrint(0,3,0,"-%02X ",tmps[i]);
			if(tmpd)break;
		}
		if(tmpd)
		{
			printk(" TX FAILED:%d,tn:%d.\n",tmpd,tn);
			ScrPrint(0,3,0,"TX FAILED:%d,tn:%d.\n",tmpd,tn);
			getkey();
			UsbDevStop();
			PortOpen(P_USB_HOST,"PaxDev");
			goto T_BEGIN;

		}

ScrPrint(0,4,0,"LOOPS:%d,RX...\n",cur_loop);
		rn=0;
		memset(xstr,0x00,sizeof(xstr));
		tmpd=UsbDevRecvs(xstr,tn,20000);//2000
		if(tmpd)rn=tmpd;
		if(tmpd<=0)
		{
			printk("RX FAILED:%d,rn:%d,%u.\n",tmpd,rn,GetTimerCount());
			ScrPrint(0,5,0,"RX FAILED:%d,rn:%d,%u.\n",tmpd,rn,GetTimerCount());
			getkey();
         UsbDevStop();
			PortOpen(P_USB_HOST,"PaxDev");
			goto T_BEGIN;
		}
		if(rn!=tn)
		{
			//Beep();
			getkey();

			tmpd=UsbDevRecvs(xstr+rn,tn,20000);//2000
			ScrPrint(0,6,0," LAST:%d,rn:%u.\n",tmpd,rn);
			getkey();

			/*
			while(1)
			{
				printk(" DLEN MISMATCH,tn:%d,rn:%d,%u.\n",tn,rn,GetTimerCount());
				ScrPrint(0,6,0," DLEN MISMATCH,tn:%d,rn:%d,%u.\n",tn,rn,GetTimerCount());
				DelayMs(1000);
			}
			*/
		}
		if(memcmp(tmps,xstr,tn))
		{
			printk(" DATA MISMATCH:\n");
			ScrPrint(0,6,0," DATA MISMATCH:\n");
			for(i=0;i<tn;i++)printk("%02X ",xstr[i]);
			printk("\n");
			getkey();
		}

		tn=(tn+1)%10241;
		if(!tn)tn=1;

		if(cur_loop==1)
		{
			//UsbDevStop();
//			tmpd=UsbDevOpen(1);
//			ScrPrint(0,7,0,"REOPEN:%d ",tmpd);

//			tmpd=UsbDevRecvs(xstr,1000,3000);
//			ScrPrint(0,7,0,"REV:%d ",tmpd);
//			DelayMs(3000);
		}

	}//while(1)
}
#endif
