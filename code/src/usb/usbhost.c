#include  <stdio.h>
#include  <string.h>
#include  <stdarg.h>

#include <Base.h>

#include  "..\comm\comm.h"

#define HC_NUMS 1

#define USB_ERR_NOTOPEN       3
#define USB_ERR_BUF           4
#define USB_ERR_NOTFREE       5
#define USB_ERR_CHIPVER       15
#define USB_ERR_DEV_ABSENT    16
#define USB_ERR_PORT_POWERED  17
#define USB_ERR_DEV_REMOVED   18
#define USB_ERR_DEV_QUERY     19
#define USB_ERR_TIMEOUT       0xff

#define rG_AHB_CFG          (*(volatile unsigned int*)0x000A0008)
#define rG_USB_CFG          (*(volatile unsigned int*)0x000A000C)
#define rG_RST_CTL          (*(volatile unsigned int*)0x000A0010)
#define rG_INT_STS          (*(volatile unsigned int*)0x000A0014)
#define rG_INT_MSK          (*(volatile unsigned int*)0x000A0018)
#define rG_HW_CFG1          (*(volatile unsigned int*)0x000A0044)/*ep direction*/
#define rG_HW_CFG2          (*(volatile unsigned int*)0x000A0048)
#define rG_HW_CFG3          (*(volatile unsigned int*)0x000A004C)
#define rG_HW_CFG4          (*(volatile unsigned int*)0x000A0050)
#define rG_RXFIFO_SIZE      (*(volatile unsigned int*)0x000A0024)
#define rG_NP_TXFIFO_SIZE   (*(volatile unsigned int*)0x000A0028)

#define rHCFG               (*(volatile unsigned int*)0x000A0400)
#define rHAINT              (*(volatile unsigned int*)0x000A0414)
#define rHAINTMSK           (*(volatile unsigned int*)0x000A0418)
#define rHPRT               (*(volatile unsigned int*)0x000A0440)
#define rHCCHAR0            (*(volatile unsigned int*)0x000A0500)
#define rHCINT0             (*(volatile unsigned int*)0x000A0508)
#define rHCINTMSK0          (*(volatile unsigned int*)0x000A050C)
#define rHCTSIZ0            (*(volatile unsigned int*)0x000A0510)
#define rHCDMA0             (*(volatile unsigned int*)0x000A0514)
#define rPCGCCTL            (*(volatile unsigned int*)0x000A0E00)

#define rWC_PHY_CTL         (*(volatile unsigned int*)0x000B0000)
#define rWC_GEN_CTL         (*(volatile unsigned int*)0x000B0004)
#define rWC_VBUS_CTL         (*(volatile unsigned int*)0x000B0008)

#define USB_ADDR DMA_USB_BASE//0x40200000

unsigned char * hcx_buf0;
//--constant parameters
#define RX_POOL_SIZE 65537
#define TX_POOL_SIZE 10241/*1025*/
#define BIO_MAX_RETRIES 5
#define EP_BUF_SIZE 64
#define EP_NUMS     16


enum DEV_TYPE{T_NULL=0,T_PAXDEV,T_UDISK,T_UDISK_FAST,T_SCANNER,T_DEV_LIMIT};
enum TXN_TYPE{T_IDLE=0,T_SETUP,T_BULK_IO};
enum PID_TYPE{P_SETUP=0x0d,P_IN=0x09,P_OUT=0x01,P_SOF=0x05,P_DATA0=0x03,
				  P_DATA1=0x0b,P_ACK=0x02,P_NAK=0x0a,P_DATA_ERR=0x0f,P_TIME_OUT=0,
				  P_STALL=0x0e};
enum DEV_STATE{S_DETACHED=0,S_ATTACHED,S_POWERED,S_DEFAULT,S_ADDRESS,S_CONFIGURED,S_SUSPENDED};
enum HCI_STATE{S_HCI_NOT_INIT=0,S_HCI_INITIALIZED,S_HCI_STARTED,S_HCI_DETECTED,S_HCI_FTDI,S_HCI_HCD};

struct ST_PACKET
{
	int ep_no;//endpoint number,0~15
	int d0_d1;//0:data0 used,1:data1 used
	int pid;
	int dn;//data bytes
	int state;
}cur_pk;

struct ST_PIPE
{
	int rx_ep_no;
	int tx_ep_no;
	int rx_packet_size;
	int tx_packet_size;
	int speed_type;//0--low,1--full,2--high
	unsigned char seq_no;
	unsigned char device_id;
	unsigned short device_version;
	int sof_required;
	int io_timeout;
}cur_pipe;

typedef struct
{
	unsigned char seq_no;
	unsigned char req_type;//0:OUT,1:IN,2:STAT,3:RESET
	unsigned short len;
	unsigned char	 data[508];
}__attribute__((__packed__)) ST_BULK_IO;

typedef struct
{
	int tx_buf_size;
	int rx_buf_size;
	int tx_left;
	int rx_left;
}__attribute__((__packed__)) ST_BIO_STATE;

typedef struct
{
	unsigned char sig[4];
	int id;
	int dlen;
	unsigned char flag;
	unsigned char unit_no;
	unsigned char cmd_len;
	unsigned char cmd[16];
}__attribute__((__packed__)) ST_UFI_HEAD;

typedef struct
{
	unsigned char len;//Descriptor size in bytes:18
	unsigned char type;//DEVICE descriptor type:1
	unsigned short usb_version;//USB spec version in BCD, e.g. 0x0200
	unsigned char	class;//Class code, if 0 see interface
	unsigned char	sub_class;//Sub-Class code, 0 if class = 0
	unsigned char	protocol;//Protocol, if 0 see interface
	unsigned char	ep0_packet_size;//Endpoint 0 max. size
	unsigned short vendor_id;//Vendor ID per USB-IF
	unsigned short product_id;//Product ID per manufacturer
	unsigned short device_version;//Device release # in BCD
	unsigned char	manufacturer_index;//Index to manufacturer string
	unsigned char	product_index;//Index to product string
	unsigned char	serial_index;//Index to serial number string
	unsigned char	configs;//Number of possible configurations
}__attribute__((__packed__)) ST_DESCRIPTOR_DEVICE;

typedef struct
{
	unsigned char len;//Descriptor size in bytes:9
	unsigned char type;//CONFIGURATION type:2 or 7
	unsigned short total_len;//Length of concatenated descriptors
	unsigned char interfaces;//Number of interfaces, this config
	unsigned char config_id;//Value to set this config
	unsigned char string_index;//Index to configuration string
	unsigned char attrib;//Config. characteristics
	unsigned char  max_power;//Max.power from bus, 2mA units
}__attribute__((__packed__)) ST_DESCRIPTOR_CONFIG;

typedef struct
{
	unsigned char   len;//Descriptor size in bytes = 9
	unsigned char   type;//INTERFACE descriptor type = 4
	unsigned char   interface_id;//Interface no.
	unsigned char   alternate_no;//Value to select it
	unsigned char   endpoints;//Number of endpoints excluding 0
	unsigned char   class;//Class code, 0xFF = vendor
	unsigned char   sub_class;//Sub-Class code, 0 if class = 0
	unsigned char   protocol;//Protocol, 0xFF = vendor
	unsigned char   string_index;//Index to interface string
}__attribute__((__packed__)) ST_DESCRIPTOR_INTERFACE;

typedef struct
{
	unsigned char len;//Descriptor size in bytes = 7
	unsigned char type;//ENDPOINT descriptor type = 5
	unsigned char address;//Endpoint # 0-15, IN/OUT
	unsigned char attrib;//D1D0:transfer type
	unsigned short packet_size;//Bits 10:0 = max. packet size
	unsigned char interval;//Polling interval in (micro) frames
}__attribute__((__packed__)) ST_DESCRIPTOR_ENDPOINT;

static unsigned char tx_data_no[3],rx_data_no[3];
volatile int usb_host_state=0;
volatile int usbEnableVbusTick=0;
static int port_open_state[T_DEV_LIMIT];
static uchar last_device_id=T_NULL;
extern unsigned char usb0_tx_pool[],usb0_rx_pool[];
static volatile int tx_read,tx_write,rx_read,rx_write;
static ST_BIO_STATE dev_state;

//--descriptor structs
static ST_DESCRIPTOR_DEVICE desc_device;
static ST_DESCRIPTOR_CONFIG desc_config;
static ST_DESCRIPTOR_INTERFACE desc_face;
static ST_DESCRIPTOR_ENDPOINT desc_ep[EP_NUMS];

static const uchar USB_HOST_VER[]="89A,150403\0";

extern void reset_udev_state(void);
extern void ftdi_interrupt(int id);
extern volatile uint usb_to_device_task;

void GetUsbHostVersion(char *ver_str)
{
	strcpy(ver_str,USB_HOST_VER);
	return;
}

void flush_tx_fifo(const int num)
{
	int count = 0;

    rG_RST_CTL|=(1<<5)|(num<<6);

	while(rG_RST_CTL&(1<<5)) 
    {
		if (++count > 10000) 
        {
			//printk("%s() HANG! GRSTCTL=%0x GNPTXSTS=\n",__func__, rG_RST_CTL);
			break;
		}
		DelayUs(1);
	}

	/* Wait for 3 PHY Clocks */
	DelayUs(1);
}

void flush_rx_fifo(void)
{
	int count = 0;

    rG_RST_CTL|=1<<4;
	while(rG_RST_CTL&(1<<4))
    {
		if (++count > 10000) 
        {
			//printk("%s() HANG! GRSTCTL=%0x\n", __func__,rG_RST_CTL);
			break;
		}
		DelayUs(1);
	}

	/* Wait for 3 PHY Clocks */
	DelayUs(1);
}
#define USB_OTG_ID		(3)
static void usbotg_interrupt(void)
{
    //printk("%s\r\n",__func__);
    if((rG_INT_STS&1) ==0)//device
    {
        //printk("IsDevice\r\n");
        gpio_set_pin_interrupt(GPIOB,USB_OTG_ID,0);//disable otg interrupt

    	gpio_set_pin_type(GPIOB, 28, GPIO_OUTPUT);//USB0_HOST_ON
        gpio_set_pin_val(GPIOB, 28, 0);

        //turn off vbus
    	gpio_set_pin_type(GPIOB,12,GPIO_OUTPUT);
    	gpio_set_pin_val(GPIOB,12,0);
        return;
    }

    if(gpio_get_pin_val(GPIOB, USB_OTG_ID))//B connect
    {
        //printk("B connect\r\n");
    	gpio_set_pin_type(GPIOB, 28, GPIO_OUTPUT);//USB0_HOST_ON
        gpio_set_pin_val(GPIOB, 28, 0);

        //turn off vbus
    	gpio_set_pin_type(GPIOB,12,GPIO_OUTPUT);
    	gpio_set_pin_val(GPIOB,12,0);
		
        //gpio_enable_pull(GPIOB, 0);
        //gpio_set_pull(GPIOB, 0, 0);
        //s_setShareIRQHandler(GPIOB,USB_OTG_ID,INTTYPE_LOWLEVEL,usbotg_interrupt);//low level trigger

        gpio_set_pin_interrupt(GPIOB,USB_OTG_ID,0);//disable otg interrupt
        if(usb_to_device_task==0)
            usb_to_device_task = 1;//switch to usb device in softtimer handler
    }
    else//A connect
    {
        //printk("A connect\r\n");
        gpio_set_pin_type(GPIOB, 28, GPIO_OUTPUT);//USB0_HOST_ON
        gpio_set_pin_val(GPIOB, 28, 1);
            
		gpio_set_pin_type(GPIOB,12,GPIO_OUTPUT);
        gpio_set_pin_val(GPIOB,12,1);//turn on vbus
		
        //gpio_enable_pull(GPIOB, 0);
        //gpio_set_pull(GPIOB, 0, 0);
        s_setShareIRQHandler(GPIOB,USB_OTG_ID,INTTYPE_HIGHLEVEL,usbotg_interrupt);//low level trigger
    }    
}

//serve_dev_type: 0--not FTDI,1--FTDI
unsigned char UsbHostInit(int serve_dev_type)
{
    unsigned int i,tmpd;

    //printk("\r%s\n",__func__);

    usb_host_state=0;
    
    //1 Disable USB0 and initialized variable
    disable_irq(IRQ_OUSB0);

    rG_INT_STS=0;
    rG_INT_MSK=0;

    //4 Initlized otg phy
    tmpd = rWC_PHY_CTL;//usbwc_phy_ctrl
    tmpd = tmpd & 0xffffafff;

    tmpd |=  USBWC_F_otg_mode_MASK       | /* OTG mode */
                    USBWC_F_utmi_pwrdwnb_i_MASK | /* Power to utmi phy */
                    USBWC_F_afe_pwrdwnb_i_MASK  | /* Power to analog front end */
                    USBWC_F_soft_resetb_i_MASK;   /* Clear reset */
    rWC_PHY_CTL=tmpd;

	/* Set software VBUS control */
	rWC_GEN_CTL |= USBWC_F_sw_vbc_en_MASK;

	/* Enable otgID from connector pin */
	rWC_GEN_CTL |= USBWC_F_otgid_en_MASK ;

    /* Force valid device configuration */
    rWC_VBUS_CTL |= (     USBWC_F_sw_vbusvalid_MASK |
                            USBWC_F_sw_avalid_MASK |
                            USBWC_F_sw_bvalid_MASK);

    /* Set endianess */
    rWC_GEN_CTL &= ~( USBWC_F_otg_master_be_en_MASK |  USBWC_F_otg_slave_be_en_MASK);
    
//	otg_core_reset();
    i=0;
	while ((rG_RST_CTL&(1<<31))==0) //ahb master idle
    {
		DelayUs(1);
		if (++i > 1000000) return USB_ERR_DEV_ABSENT;
	}

	i = 0;
    rG_RST_CTL|=1;//core soft reset
	while (rG_RST_CTL&1) 
    {
		if (++i > 10000) break;
		DelayUs(1);
	}
	/* Wait for 3 PHY Clocks */
	DelayUs(1);

    rPCGCCTL = 0;
    //2 Force set to host mode
    rG_USB_CFG = 0x2000A440;//0x2000A420;//48MHz PHY
    //rG_USB_CFG = 0x20002400;//480MHz PHY
    i=0;
    while(1)//wait otg core host mode
    {
        DelayUs(1);
        if(rG_INT_STS&0x01) break;//host mode
        if(++i>=100000) return USB_ERR_DEV_ABSENT;//100 ms
    }
    
	rG_AHB_CFG = 0x00000026;//0x00000027;
    rHCFG=0x05;//48MHz
    //rHCFG=0x04;//60-30MHz
    
    if(serve_dev_type==1)//for ftdi
    {
        //set irq
	    //10-set IRQ type and priority
    	s_SetInterruptMode(IRQ_OUSB0, INTC_PRIO_2, INTC_IRQ);
    	s_SetIRQHandler(IRQ_OUSB0,ftdi_interrupt);//USB_INT
    }

    //3 Clean host channel 0
    rHCINTMSK0=0;
    rHCINT0=~0x00;

    /* Configure data FIFO sizes */
    if(rG_HW_CFG2&(1<<19))//Dynamic FIFO Sizing Enable
    {
        rG_RXFIFO_SIZE=1024;
        rG_NP_TXFIFO_SIZE=1024|(1024<<16);
    }
    
	flush_tx_fifo(0x10);
	flush_rx_fifo();

    tmpd =rHPRT;
    if((tmpd&(1<<12)) ==0)//bit12-0:power off 1:power on
    {
        if((get_machine_type()==S300)&&(GetVolt()>=5500))
            return USB_ERR_PORT_POWERED;
        /* bit1:PortConnect Detected bit2:PortEnable bit3:PortEnable change bit5:PortOvercurrentChange */
        tmpd&=~((1<<2)|(1<<1)|(1<<3)|(1<<5));
        tmpd|=1<<12;//Port on
        #if 0
        gpio_set_pin_val(GPIOB,12,0);
        gpio_set_pin_type(GPIOB,12,GPIO_OUTPUT);
        DelayUs(10);
        gpio_set_pin_type(GPIOB,14,GPIO_INPUT);
        if(gpio_get_pin_val(GPIOB,14)==0) return USB_ERR_PORT_POWERED;
        #endif
        gpio_set_pin_type(GPIOB,14,GPIO_INPUT);
        if(gpio_get_pin_val(GPIOB,14))usbEnableVbusTick = s_Get10MsTimerCount();//如果vbus无电才更新监测时间
		if (get_machine_type() != D200)
		{
        	gpio_set_pin_val(GPIOB,12,1);
        	gpio_set_pin_type(GPIOB,12,GPIO_OUTPUT);
		}
        rHPRT=tmpd;
    }

	if (get_machine_type() == D200)
	{
	    gpio_set_pin_type(GPIOB, USB_OTG_ID, GPIO_INPUT);
	    gpio_set_pull(GPIOB, USB_OTG_ID, 1);
	    gpio_enable_pull(GPIOB, USB_OTG_ID);

	    if(gpio_get_pin_val(GPIOB, USB_OTG_ID))//B connect
	    {
	        if(usb_to_device_task==0)
	            usb_to_device_task = 1;//switch to usb device in softtimer handler
	        return USB_ERR_DEV_ABSENT;
	    }
	    
	    s_setShareIRQHandler(GPIOB,USB_OTG_ID,INTTYPE_LOWLEVEL,usbotg_interrupt);//low level trigger
	}
    hcx_buf0  =(unsigned char *)USB_ADDR;
    usb_host_state = S_HCI_INITIALIZED;
    memset(port_open_state,0x00,sizeof(port_open_state));
    DelayMs(1);
    return 0;
}

static int send_wait_token(unsigned char token,unsigned char dlen,int timeout_ms)
{
    volatile unsigned int hcint;
    volatile unsigned int hcchar;
    volatile unsigned int hctsiz;
    //host_grxsts_data_t grxsts;
    unsigned char pid,ep_no;
    int t0;

    t0 = s_Get10MsTimerCount();

RETRY:
    //otg_hc_halt(0);
    //otg_flush_tx_fifo(0x10);

    pid = token>>4;
    ep_no = token&0x0f;

    rHCINT0=~0;

    if (rHCCHAR0&(1<<31))//channel enable
    {
        rHCCHAR0|=(1<<30);//bit30:channel disable
        //printk("\rHALT CHANNEL START\n");
        while(1)
        {
            if(rHCINT0&(1<<1)) break;//channel halted
            if((s_Get10MsTimerCount()-t0)*10 >timeout_ms)return 1;
        }
        rHCINT0=~0;
        //printk("\rHALT CHANNEL SUCCESS\n");
	}

    hcchar=rHCCHAR0;
    hcchar &=~(0xfffff);//clear bit19~bit0
    
	hctsiz = dlen;//xfersize
	hctsiz |= 1<<19;//bit28~bit19:packet count
    if(pid==P_SETUP)hctsiz|=3<<29;//bit30~bit29:set to setup;
	else if(pid==P_OUT) 
	{
        if(tx_data_no[ep_no]) hctsiz|=2<<29;//bit30~bit29:set to data1
	}
    else if(pid ==P_IN)
    {
        if(rx_data_no[ep_no]) hctsiz|=2<<29;//bit30~bit29:set to data1
        hcchar|=1<<15;//bit15:set to in
    }
    else return 2;
    
    if(ep_no) hcchar|=0x2<<18;//ep bulk

    hcchar |=(ep_no<<11)|64|(1<<31);//set ep number,mps,hc enable
    DelayUs(2);

    rHCDMA0=(unsigned int)hcx_buf0;
    rHCTSIZ0=hctsiz;
    rHCCHAR0=hcchar;
    
    while(1) 
	{
        hcint=rHCINT0;
        if((rHPRT&0x0F)!=0x05)
        {
//printk(" ABSENT:%02X\n",b);
			port_open_state[cur_pipe.device_id]=0;
			return 3;
		}

        if(hcint&(1<<1) && hcint&1 && hcint&(1<<5))//channel halt,transfer completed,ack
        {
            rG_INT_STS=rG_INT_STS;
            rHCINT0=hcint;
            break;
        }
        else if((s_Get10MsTimerCount()-t0)*10 >timeout_ms)
        {
            //printk("\r%s timeout\n",__func__);
            return 4;
        }
        else if(hcint&(1<<1))//channel halt
        {
            if(hcint&(1<<10) && ep_no)
            {
                //printk("\rdatatglerr\n ");
                return 5;//data error
            }
            goto RETRY;
        }
        
        continue;
	}

    if((pid==P_OUT)||(pid==P_SETUP)) cur_pk.dn=dlen;//hctsiz.b.xfersize no change for OUT and SETUP
    else//hctsiz.b.xfersize has change for IN
        cur_pk.dn=dlen-rHCTSIZ0&(0x7ffff);

    return 0;
}

static int host_in_ep0(int tx_len,unsigned char *io_data,int *rx_len)
{
	unsigned char token;
	int i,packet_size,want_len,tmpd,tx_retries,data_no=0,t0;

    want_len=(io_data[7]<<8)+io_data[6];
    token = P_SETUP<<4;
    memcpy(hcx_buf0,io_data,8);

	tmpd=send_wait_token(token,8,200);
	if(tmpd)return 1;

	i=0;
	packet_size=*rx_len;
    rx_data_no[0]=1;
	while(1)
	{
    	token=P_IN<<4;
    	tmpd=send_wait_token(token,EP_BUF_SIZE,3000);//200
    	if(tmpd)return 4;

        memcpy(io_data+i,hcx_buf0,cur_pk.dn);
    	i+=cur_pk.dn;
    	*rx_len=i;

    	rx_data_no[0]=(rx_data_no[0]+1)%2;

    	if(cur_pk.dn!=packet_size)break;
    	if(i>=want_len)break;
	}//while(1)

//printk("SETUP_IN status tx...\n");
	//5--set tx_bd for sending OUT token of status packet
	//DATA1 always used for status packet
	//51--fill P_OUT token for tx
	token=P_OUT<<4;
    tx_data_no[0]=1;

	//6--wait ACK for status TX
	tmpd=send_wait_token(token,0,200);
	if(tmpd)return 7;

    DelayUs(2000);//Wait a few times for control transfers
	return 0;
}

int host_in_epx(unsigned char *io_data,int *rx_len)
{
	unsigned char token;
	int i,want_len,tmpd;

	i=0;
	want_len=*rx_len;
	*rx_len=0;
	while(1)
	{
//printk("EPX_IN,I:%d\n",i);
	//3--fill IN token for tx
	token=(P_IN<<4)+cur_pipe.rx_ep_no;

	//4--send IN token and wait IN data
	tmpd=send_wait_token(token,cur_pipe.rx_packet_size,cur_pipe.io_timeout);//200,1000
	if(tmpd)
	{
//printk("EPX_IN:%d,I:%d\n",tmpd,i);
		return 1;
	}

	//42--fetch data
    memcpy(io_data+i,hcx_buf0,cur_pk.dn);
	i+=cur_pk.dn;
	*rx_len=i;

	//43--toggle count
	rx_data_no[cur_pipe.rx_ep_no]=(rx_data_no[cur_pipe.rx_ep_no]+1)%2;

	if(cur_pk.dn!=cur_pipe.rx_packet_size)break;
	if(i>=want_len)break;
	}//while(1)

	return 0;
}


static int host_out_ep0(int tx_len,unsigned char *tx_data)
{
	unsigned char token;
	int tn,i,tmpd;

		//10--fill data into BD buffer
//printk("SETUP_OUT tx ...\n");
	tn=8;
    memcpy(hcx_buf0,tx_data,8);

	//12--fill SETUP token for tx
	token=P_SETUP<<4;

	//2--wait ACK for tx
	tmpd=send_wait_token(token,tn,200);
	if(tmpd)return 1;

    rx_data_no[0]=1;
	//5--fill IN token for tx
	token=P_IN<<4;

//printk("SETUP_OUT rx ...\n");
	//6--wait for status RX
	tmpd=send_wait_token(token,EP_BUF_SIZE,200);//200
	if(tmpd)return 5;

    if(cur_pk.dn)return 6;

    DelayUs(2000);//Wait a few times for control transfers
	return 0;
}

int host_out_epx(int tx_len,unsigned char *tx_data)
{
	unsigned char token;
	int cn,i,bn,tmpd;

	bn=tx_len/cur_pipe.tx_packet_size;
	if(tx_len%cur_pipe.tx_packet_size)bn++;

	for(i=0;i<bn;i++)
	{
	if(i!=bn-1)cn=cur_pipe.tx_packet_size;
	else cn=tx_len-i*cur_pipe.tx_packet_size;
	//10--fill data into BD buffer,note BD0 is used for host mode
	memcpy(hcx_buf0,tx_data+i*cur_pipe.tx_packet_size,cn);

	//13--fill P_OUT token for tx
	token=(P_OUT<<4)+cur_pipe.tx_ep_no;

	//2--send and wait ACK for tx
	tmpd=send_wait_token(token,cn,cur_pipe.io_timeout);//200,1000
	if(tmpd)
	{
		return 1;
	}
//printk("EPX_OUT,DN:%d\n",cur_pk.dn);
	if(cur_pk.dn!=cn)return 4;
	//if(cur_pk.d0_d1!=tx_data_no)return 5;

	//22--toggle counter
	tx_data_no[cur_pipe.tx_ep_no]=(tx_data_no[cur_pipe.tx_ep_no]+1)%2;
	}//for(i)

	return 0;
}

//device_id:1--U disk,2--SP30 USB,0--both U disk and SP30 USB
//reset:0--first detect,1--reset device
//SOF_out:1--output SOF,0--not output SOF
unsigned char UsbHostDetect(int device_id,int reset)
{
    unsigned char a,tmps[1100];
	int tmpd,tn,rn,i,cn;
	ST_BULK_IO bulk_out,*p_bulk_in;
	ST_UFI_HEAD ufi_head;
	volatile unsigned int hprt0;
	volatile unsigned int hprt0_modify,t;

	if(usb_host_state<S_HCI_INITIALIZED)return USB_ERR_NOTFREE;//not initialized
    if((rG_INT_STS&1) ==0)return USB_ERR_NOTFREE;//device mode

    last_device_id = T_NULL;
//1 Reset device

    i=0;
    if((s_Get10MsTimerCount()-usbEnableVbusTick)<300)
        t=3000000;
    else
        t=100000;
    while ((rG_INT_STS&(1<<24))==0)
    {
		DelayUs(1);
		if (++i > t) 
		{
            //printk("no hprt interrupt\n");
            return USB_ERR_DEV_ABSENT;
		}
	}
    DelayMs(100);    
    
	hprt0_modify=hprt0 = rHPRT;

    if((hprt0&1)==0)// a device is attached to the port
    {
        //printk("no device 0\n");
        return USB_ERR_DEV_ABSENT;
    }
    if(((hprt0>>17)&0x3)==2)//low speed
    {
        rG_USB_CFG = 0x20001440;//48MHz PHY
        rG_AHB_CFG = 0x00000022;
        rHCFG=0x06;//6MHz
    }
    else
    {
        rG_USB_CFG = 0x2000A440;//48MHz PHY
        rG_AHB_CFG = 0x00000026;
        rHCFG=0x05;//48MHz
    }
    //printk("\rhprt0-0x%X\n",hprt0);

    hprt0_modify &=~((1<<2)|(1<<3));//set 0 to bit2:prtena bit3:prtenchng 

    if(hprt0&(1<<1))hprt0_modify |= 1<<1;//prtconndet
    if(hprt0&(1<<5))hprt0_modify |= 1<<5;//prtovrcurrchng

    //DelayMs(50);
    rHPRT=hprt0_modify;//Clear HPRT

    DelayMs(10);

    hprt0 = rHPRT;
    hprt0&=~(1<<7);//prtsusp
    hprt0|=1<<8;//prtrst
    rHPRT=hprt0;
    DelayMs(60);
    hprt0 &= ~(1<<8);//prtrst
    rHPRT=hprt0;

    DelayUs(10);
	hprt0 = rHPRT;
    if((hprt0&(1<<3))==0)//prtenchng
    {
        //printk("\rNoDevice\n");
        return USB_ERR_DEV_ABSENT;
    }
        
    hprt0_modify|=1<<3;//prtenchng
    if((hprt0&(1<<2))==0)//prtena
    {
        //printk("\r!!!! Fatal Err  in hardware interrupt \n");
        return USB_ERR_DEV_ABSENT;
	}
#if 0
    if (hprt0.b.prtspd != CoreDev.speed)
	{
		CoreDev.speed = hprt0.b.prtspd;
		init_fslspclksel();
        printk("\rhprt0.b.prtspd != CoreDev.speed\n");
	}
#endif    
    //printk("\rPort Enable\n");

    rHPRT=hprt0_modify;

    //flush channel 0
    tmpd=rHCCHAR0;
    tmpd&=~(1<<31);
    tmpd|=1<<30;
    rHCCHAR0=tmpd;

    //halt channel 0
    tmpd=rHCCHAR0;
    tmpd|=(1<<31)|(1<<30);
    tmpd&=~(1<<15);
    rHCCHAR0=tmpd;
    i=0;
	while(rHCCHAR0&(1<<31)) 
	{
		if (++i> 500) 
		{
			//printk("%s: Unable to clear halt on channel %d\n",__func__, 0);
			break;
		}
	} 
    
    rHCCHAR0&=~(0x7f<<22);//set device addr to 0

	//2--reset relevant variables
	memset(&cur_pk,0x00,sizeof(cur_pk));
    memset(tx_data_no,0,sizeof(tx_data_no));
    memset(rx_data_no,0,sizeof(rx_data_no));
	memset(&desc_device,0x00,sizeof(desc_device));
	memset(&desc_config,0x00,sizeof(desc_config));
	memset(&desc_face,0x00,sizeof(desc_face));
	memset(&desc_ep,0x00,sizeof(desc_ep));
	memset(&dev_state,0x00,sizeof(dev_state));
	cur_pipe.rx_packet_size=64;cur_pipe.tx_packet_size=64;

	if(!reset)
	{
		usb_host_state=S_HCI_STARTED;
		tx_read=0;tx_write=0;rx_read=0;rx_write=0;
		memset(&cur_pipe,0x00,sizeof(cur_pipe));
		memset(port_open_state,0x00,sizeof(port_open_state));

		if(device_id==T_NULL || device_id==T_UDISK)cur_pipe.sof_required=1;
		else cur_pipe.sof_required=0;
	}

	cur_pipe.speed_type=0xff;

    if(((hprt0>>17)&0x3)==1)//full speed of device
        cur_pipe.speed_type=1;
    else if(((hprt0>>17)&0x3)==2)//low speed of device
        cur_pipe.speed_type=0;
    else if(((hprt0>>17)&0x3)==0)//high speed of device
        cur_pipe.speed_type =2;
    //printk("\rDeviceSpeed:%d\n",cur_pipe.speed_type);
#if 0
	//5--select host mode,full speed,1ms SOF counter
	w_reg(rUSB_HWV_SOF_L,0xe0);
	if(!cur_pipe.speed_type)a=0xee;
	else a=0xae;
	w_reg(rUSB_CTRL2_SOF_H,a);
	if(!cur_pipe.speed_type)a=0x20;//0x21
	else a=0x00;//0x01
	if(cur_pipe.sof_required)a|=0x01;
	w_reg(rUSB_CTRL1,a);//D5:speed,D0:SOF enable
	if(cur_pipe.sof_required)w_reg(rUSB_A_CTRL,0x07);//start SOF

#endif

	//6--fetch device descriptor
	tn=8;
	memcpy(tmps,"\x80\x06\x00\x01\x00\x00\x40\x00",tn);
	rn=8;
	tmpd=host_in_ep0(tn,tmps,&rn);
	if(tmpd)
	{
        //printk("\rError1:%d\n",tmpd);
        return 20+tmpd;
	}

    //printk("\rDevDesc-");
    //for(i=0;i<rn;i++)
    //{
      //  printk("0x%X ",tmps[i]);
    //}
    //printk("\n");    
    
	if(rn>sizeof(desc_device))rn=sizeof(desc_device);
	memset(&desc_device,0x00,sizeof(desc_device));
	memcpy(&desc_device,tmps,rn);

	//5--set device address
	tn=8;
	memcpy(tmps,"\x00\x05\x01\x00\x00\x00\x00\x00",tn);
	tmpd=host_out_ep0(tn,tmps);
	if(tmpd)
	{
        //printk("\rError2:%d\n",tmpd);
        return 30+tmpd;
	}
    tmpd=rHCCHAR0;
    tmpd&=~(0x7f<<22);//clear device addr
    tmpd|=1<<22;//set device addr to 1
    rHCCHAR0=tmpd;

	//6--fetch configuration descriptor
	tn=8;
	memcpy(tmps,"\x80\x06\x00\x02\x00\x00\xff\x00",tn);
	rn=desc_device.ep0_packet_size;
	tmpd=host_in_ep0(tn,tmps,&rn);
	if(tmpd)
	{
        //printk("\rError3:%d\n",tmpd);
        return 40+tmpd;
	}
	//--61,analyse the config packet
//printk("CONFIG,rn:%d\n",rn);
//for(i=0;i<rn;i++)printk("%02X ",tmps[i]);
//printk("\n");
	cn=rn;
	if(cn>sizeof(desc_config))cn=sizeof(desc_config);
	memset(&desc_config,0x00,sizeof(desc_config));
	memcpy(&desc_config,tmps,cn);
	tmpd=sizeof(desc_config);//rn-=sizeof(desc_config);
//printk("config_id:%d,interface:%d\n",desc_config.config_id,desc_config.interfaces);

	if(tmps[tmpd]==3)
        tmpd+=3;
	cn=rn-tmpd;//cn=rn;
	if(cn>sizeof(desc_face))cn=sizeof(desc_face);
	memset(&desc_face,0x00,sizeof(desc_face));
	//if(cn)memcpy(&desc_face,tmps+sizeof(desc_config),cn);
	if(cn)memcpy(&desc_face,tmps+tmpd,cn);
	tmpd+=sizeof(desc_face);//rn-=sizeof(desc_face);
//printk("interface id:%d-%d,protocol:%02X\n",
//			desc_face.interface_id,desc_face.alternate_no,desc_face.protocol);

	for(i=0;i<desc_face.endpoints;i++)
	{
		cn=rn-tmpd;
		if(cn>sizeof(desc_ep[0]))cn=sizeof(desc_ep[0]);
		memset(&desc_ep[i],0x00,sizeof(desc_ep[0]));
		//if(cn)memcpy(&desc_ep[i],tmps+sizeof(desc_config)+sizeof(desc_face)+
		//										i*sizeof(desc_ep[0]),cn);
		if(cn)memcpy(&desc_ep[i],tmps+tmpd,cn);

		tmpd+=sizeof(desc_ep[0]);//rn-=sizeof(desc_ep[0]);
//printk("ep%d,attrib:%02X\n",i,desc_ep[i].attrib);
	}

	//7--fetch string descriptor 0,language id list
	tn=8;
	memcpy(tmps,"\x80\x06\x00\x03\x00\x00\xff\x00",tn);
	rn=desc_device.ep0_packet_size;
	tmpd=host_in_ep0(tn,tmps,&rn);
//printk("Fetch str0:%d\n",tmpd);
	//if(tmpd)return 50+tmpd;//some UDISK may not support it

//for(i=0;i<rn;i++)printk("%02X ",tmps[i]);
//printk("\n");
//	memcpy(&desc_string,tmps,rn);

	//8--fetch string descriptor:manufacturer
	if(desc_device.manufacturer_index)
	{
		tn=8;
		memcpy(tmps,"\x80\x06\x00\x03\x00\x00\xff\x00",tn);
		tmps[2]=desc_device.manufacturer_index;
		rn=desc_device.ep0_packet_size;
		tmpd=host_in_ep0(tn,tmps,&rn);
		if(tmpd)return 50+tmpd;
//printk("MANUFACTURER:\n");
//for(i=0;i<rn;i++)printk("%02X ",tmps[i]);
//printk("\n");
//for(i=2;i<rn;i+=2)printk("%c",tmps[i]);
//printk("\n");
	}

	//9--fetch string descriptor:product
	if(desc_device.product_index)
	{
		tn=8;
		memcpy(tmps,"\x80\x06\x00\x03\x00\x00\xff\x00",tn);
		tmps[2]=desc_device.product_index;
		rn=desc_device.ep0_packet_size;
		tmpd=host_in_ep0(tn,tmps,&rn);
		if(tmpd)return 60+tmpd;
//printk("PRODUCT:\n");
//for(i=0;i<rn;i++)printk("%02X ",tmps[i]);
//printk("\n");
//for(i=2;i<rn;i+=2)printk("%c",tmps[i]);
//printk("\n");
	}

	//10--fetch string descriptor:serial
	if(desc_device.serial_index)
	{
		tn=8;
		memcpy(tmps,"\x80\x06\x00\x03\x00\x00\xff\x00",tn);
		tmps[2]=desc_device.serial_index;
		rn=desc_device.ep0_packet_size;
		tmpd=host_in_ep0(tn,tmps,&rn);
		if(tmpd)return 70+tmpd;
//printk("SERIAL:\n");
//for(i=0;i<rn;i++)printk("%02X ",tmps[i]);
//printk("\n");
//for(i=2;i<rn;i+=2)printk("%c",tmps[i]);
//printk("\n");
	}

	//11--select config
	tn=8;
	memcpy(tmps,"\x00\x09\x01\x00\x00\x00\x00\x00",tn);
	tmps[2]=desc_config.config_id;
	tmpd=host_out_ep0(tn,tmps);
	if(tmpd)return 80+tmpd;

	//12--select interface
	tn=8;
	memcpy(tmps,"\x01\x0b\x00\x00\x00\x00\x00\x00",tn);
	tmps[2]=desc_face.alternate_no;
	tmps[4]=desc_face.interface_id;
	tmpd=host_out_ep0(tn,tmps);
	//if(tmpd)return 90+tmpd;//deleted because some UDISK can't support it

	if((device_id==T_UDISK||device_id==T_UDISK_FAST) && desc_face.class!=0x08)return 89;//not a mass storage device
	if(device_id==T_PAXDEV && desc_face.class!=0xff)return 90;//not a PAXDEV device

	//15--configure related endpoints
	if(desc_face.endpoints<2)return 91;
	cur_pipe.device_version=desc_device.device_version;
	for(i=0;i<2;i++)
	{
		int n;

		n=desc_ep[i].address&0x0f;//cur_ep_no
		if(desc_ep[i].address&0x80)//IN endpoint
		{
			cur_pipe.rx_ep_no=n;
			cur_pipe.rx_packet_size=desc_ep[i].packet_size;

		}
		else
		{
			cur_pipe.tx_ep_no=n;
			cur_pipe.tx_packet_size=desc_ep[i].packet_size;
		}
	}
    //printk("\rTxPort:%d,RxPort:%d\n",cur_pipe.tx_ep_no,cur_pipe.rx_ep_no);

	cur_pk.state=S_CONFIGURED;
	tx_data_no[cur_pipe.tx_ep_no]=0;
	rx_data_no[cur_pipe.rx_ep_no]=0;

	if(desc_face.class==0x08)//mass storage
	{
	cur_pipe.io_timeout=4000;//4000;

	memset(&ufi_head,0x00,sizeof(ufi_head));
	memcpy(ufi_head.sig,"USBC",4);
	ufi_head.dlen=36;
	ufi_head.flag=0x80;
	ufi_head.cmd_len=12;
	memcpy(ufi_head.cmd,"\x12\x00\x00\x00\x24",5);
	tn=sizeof(ufi_head);
	tmpd=host_out_epx(tn,(unsigned char*)&ufi_head);
	if(tmpd)
	{
		tmpd=host_out_epx(tn,(unsigned char*)&ufi_head);
		if(tmpd)return 91+tmpd;
	}

	memset(tmps,0x00,sizeof(tmps));
	rn=36;
	tmpd=host_in_epx(tmps,&rn);
	if(tmpd)return 100+tmpd;
//printk("UFI inquiry:%d\n",rn);
//for(i=0;i<rn;i++)printk("%02X ",tmps[i]);
//printk("\n");

	memset(tmps,0x00,sizeof(tmps));
	rn=13;
	tmpd=host_in_epx(tmps,&rn);
	if(tmpd)return 110+tmpd;
//printk("UFI inquiry CSW:%d\n",rn);
//for(i=0;i<rn;i++)printk("%02X ",tmps[i]);
//printk("\n");
	if(device_id==T_NULL)cur_pipe.device_id=T_UDISK;
	else cur_pipe.device_id=device_id;

    usb_host_state=S_HCI_DETECTED;
	}
	else if(!reset)//don't perform query for reset operation to avoid change of last_bio
	{
	cur_pipe.io_timeout=10;//200,20

	//--close SOF
	if(cur_pipe.sof_required)
	{
		cur_pipe.sof_required=0;
		//if(!cur_pipe.speed_type)a=0x20;//0x21
		//else a=0x00;//0x01
		//w_reg(rUSB_CTRL1,a);//D5:speed,D0:SOF enable
	}

	memset(&bulk_out,0x00,sizeof(bulk_out));
	bulk_out.seq_no=cur_pipe.seq_no++;
	bulk_out.req_type=2;
	bulk_out.len=sizeof(bulk_out.data);
	tn=sizeof(bulk_out);
    for(i=0;i<bulk_out.len;i++)bulk_out.data[i]=i&0xff;
    
	if(cur_pipe.device_version>=0x0101)set_checksum(&bulk_out);
//printk("Enquiry,tn:%d\n",tn);
	tmpd=host_out_epx(tn,(unsigned char*)&bulk_out);
	if(tmpd)
	{
        //printk("\rError4:%d\n",tmpd);
        return 120+tmpd;
	}
	rn=sizeof(ST_BULK_IO);
	tmpd=host_in_epx(tmps,&rn);
	if(tmpd)
	{
        //printk("\rError5:%d\n",tmpd);
        return 129+tmpd;
	}
	p_bulk_in=(ST_BULK_IO *)tmps;
	if(rn<20)return 135;

	if(cur_pipe.device_version>=0x0101 && verify_checksum(p_bulk_in))return 136;
	if(p_bulk_in->req_type!=2)return 137;
	if(cur_pipe.device_version>=0x0101)bulk_out.seq_no&=0x0f;
	if(p_bulk_in->seq_no!=bulk_out.seq_no)return 138;//SEQ_NO mismatches
	memcpy(&dev_state,p_bulk_in->data,sizeof(dev_state));

//printk("size:%d-%d,left:%d,%d\n",dev_state.tx_buf_size,dev_state.rx_buf_size,dev_state.tx_left,dev_state.rx_left);
	usb_host_state=S_HCI_DETECTED;
	cur_pipe.device_id=T_PAXDEV;
	}

    return 0;
}


static unsigned char reset_pipe_port(void)
{
	unsigned char a,tmps[100];
	int tn;

	tx_data_no[cur_pipe.tx_ep_no]=0;
	rx_data_no[cur_pipe.rx_ep_no]=0;

    flush_tx_fifo(0);
    flush_rx_fifo();

	//--clear OUT ENDPOINT_HALT
	tn=8;
	memcpy(tmps,"\x02\x01\x00\x00\x01\x00\x00\x00",tn);
	a=host_out_ep0(tn,tmps);

	if(!a)
	{
	//--clear IN ENDPOINT_HALT
	tn=8;
	memcpy(tmps,"\x02\x01\x00\x00\x81\x00\x00\x00",tn);
	a=host_out_ep0(tn,tmps);
	if(a)a+=10;
	}

	if(a)a=UsbHostDetect(T_PAXDEV,1);

	return a;
}

static unsigned char host_bulkio(unsigned char R_or_S)
{
	unsigned char a,tmps[2100];
	int tmpd,tn,rn,dn,rp_left,tp_left,i,err;
	ST_BULK_IO bulk_out,*p_bulk_in;
//static int loops=0;

    if((rG_INT_STS&1) ==0)return USB_ERR_DEV_REMOVED;//device mode
    if((rHPRT&0x0F)!=0x05)return USB_ERR_DEV_REMOVED;//device is removed

	switch(R_or_S)
	{
	case 'R':goto RECV_PROCESS;
	case 'S':goto SEND_PROCESS;
	}

RECV_PROCESS:
	do
	{
//printk("rx loops:%d...\n",dev_state.tx_left);
		//3--check if usb0_rx_pool has space
		rp_left=RX_POOL_SIZE-1-(rx_write+RX_POOL_SIZE-rx_read)%RX_POOL_SIZE;
		if(!rp_left)break;

		if(rp_left>sizeof(bulk_out.data))dn=sizeof(bulk_out.data);
		else dn=rp_left;

		//4.0--fill BULK_IN request
		memset(&bulk_out,0x00,sizeof(bulk_out));
		bulk_out.seq_no=cur_pipe.seq_no;
		bulk_out.req_type=1;//BULK_IN
		bulk_out.len=2;
		bulk_out.data[0]=dn&0xff;
		bulk_out.data[1]=dn>>8;
		if(cur_pipe.device_version>=0x0101)tn=bulk_out.len+4;
		else tn=sizeof(bulk_out);
		if(cur_pipe.device_version>=0x0101)set_checksum(&bulk_out);

		for(i=0;i<BIO_MAX_RETRIES;i++)
		{
			err=0;

			//4.1--send BULK_IN request
			tmpd=host_out_epx(tn,(unsigned char*)&bulk_out);
			if(tmpd){err=140+tmpd;goto R_TAIL;}

			//5--receive BULK_IN response
			rn=sizeof(ST_BULK_IO);
			tmpd=host_in_epx(tmps,&rn);
			if(tmpd){err=150+tmpd;goto R_TAIL;}

			p_bulk_in=(ST_BULK_IO *)tmps;
			if(cur_pipe.device_version>=0x0101 && verify_checksum(p_bulk_in))
				{err=155;goto R_TAIL;}//CHECK_SUM mismatches

			if(rn<4 || rn<p_bulk_in->len+4)
			{
//printk("\rERROR len,rn:%d,%02X%02X%02X%02X,sys_seq:%02X\n",rn,tmps[0],tmps[1],tmps[2],tmps[3],bulk_out.seq_no);
				err=156;
				goto BIO_TAIL;
			}

			//--if dev's usb0_rx_pool is empty
			if(p_bulk_in->req_type==2 && p_bulk_in->len>=sizeof(dev_state))
			{
//printk("RX POOL empty...\n",loops++);
				DelayUs(2000);//2000
				memcpy(&dev_state,p_bulk_in->data,sizeof(dev_state));
				cur_pipe.seq_no++;
				goto SEND_PROCESS;
			}
			if(p_bulk_in->req_type!=1){err=157;goto BIO_TAIL;}
			if(cur_pipe.device_version>=0x0101)bulk_out.seq_no&=0x0f;
			if(p_bulk_in->seq_no!=bulk_out.seq_no){err=158;goto BIO_TAIL;}//SEQ_NO mismatches
			if(p_bulk_in->len>dn){err=159;goto BIO_TAIL;}//too more data than requested

R_TAIL:
			if(!err)break;

            if((rHPRT&0x0F)!=0x05){err=160;goto BIO_TAIL;}//device is removed

			a=reset_pipe_port();
//printk("\r%d,RX RESET PIPE:%d,SEQ:%02X,err:%d\n",i,a,bulk_out.seq_no,err);
		}//for(i)
		if(err)goto BIO_TAIL;

		//6--store data into usb0_rx_pool
		dn=p_bulk_in->len;
		if(rx_write+dn<=RX_POOL_SIZE)
			memcpy(usb0_rx_pool+rx_write,p_bulk_in->data,dn);
		else
		{
			memcpy(usb0_rx_pool+rx_write,p_bulk_in->data,RX_POOL_SIZE-rx_write);
			memcpy(usb0_rx_pool,p_bulk_in->data+RX_POOL_SIZE-rx_write,dn+rx_write-RX_POOL_SIZE);
		}
		rx_write=(rx_write+dn)%RX_POOL_SIZE;
		dev_state.tx_left-=dn;
		cur_pipe.seq_no++;

//printk("RX,%02X,DN:%d,%02X\n",p_bulk_in->seq_no,p_bulk_in->len,p_bulk_in->data[0]);
	}while(dev_state.tx_left>0);//while(device has data to be sent)


SEND_PROCESS:
	do
	{
		//13--check if usb0_tx_pool has data to be sent
		tp_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;
		if(!tp_left)break;
//printk("tx loops:%d...\n",tp_left);

		if(tp_left>sizeof(bulk_out.data))dn=sizeof(bulk_out.data);
		else dn=tp_left;
		if(dn>dev_state.rx_left)dn=dev_state.rx_left;

		//14--send BULK_OUT request
		memset(&bulk_out,0x00,sizeof(bulk_out));
		bulk_out.seq_no=cur_pipe.seq_no;
		bulk_out.req_type=0;//BULK_OUT
		bulk_out.len=dn;

		//--move data from usb0_tx_pool to out_pool
		if(tx_read+dn<=TX_POOL_SIZE)memcpy(bulk_out.data,usb0_tx_pool+tx_read,dn);
		else
		{
			memcpy(bulk_out.data,usb0_tx_pool+tx_read,TX_POOL_SIZE-tx_read);
			memcpy(bulk_out.data+TX_POOL_SIZE-tx_read,usb0_tx_pool,dn+tx_read-TX_POOL_SIZE);
		}

		if(cur_pipe.device_version>=0x0101)tn=dn+4;
		else tn=sizeof(bulk_out);
		if(cur_pipe.device_version>=0x0101)set_checksum(&bulk_out);

		for(i=0;i<BIO_MAX_RETRIES;i++)
		{
			err=0;

			tmpd=host_out_epx(tn,(unsigned char*)&bulk_out);
			if(tmpd){err=160+tmpd;goto W_TAIL;}

			//15--receive STATE query response
			rn=sizeof(ST_BULK_IO);
			tmpd=host_in_epx(tmps,&rn);
			if(tmpd){err=170+tmpd;goto W_TAIL;}
			p_bulk_in=(ST_BULK_IO *)tmps;
			if(rn<sizeof(dev_state)+4){err=175;goto W_TAIL;}

			if(cur_pipe.device_version>=0x0101 && verify_checksum(p_bulk_in))
				{err=176;goto BIO_TAIL;}//CHECK_SUM mismatches

			if(p_bulk_in->req_type && p_bulk_in->req_type!=2){err=177;goto W_TAIL;}
			if(cur_pipe.device_version>=0x0101)bulk_out.seq_no&=0x0f;
			if(p_bulk_in->seq_no!=bulk_out.seq_no){err=178;goto W_TAIL;};//SEQ_NO mismatches
			memcpy(&dev_state,p_bulk_in->data,sizeof(dev_state));

			if(p_bulk_in->req_type==2)dn=bulk_out.len;
			else dn=0;//data is discarded by devive
W_TAIL:
			if(!err)break;

			//a=r_reg(rUSB_INT_STATUS);
			//if(a&0x60){err=179;goto BIO_TAIL;}//device is removed
			if((rHPRT&0x0F)!=0x05){err=179;goto BIO_TAIL;}//device is removed

			a=reset_pipe_port();
//printk("\r%d,TX RESET PIPE:%d,err:%d ",i,a,err);
//printk("\rSEQ:%02X,dn:%d\n",bulk_out.seq_no,bulk_out.len);
		}//for(i)
		if(err)goto BIO_TAIL;

		tx_read=(tx_read+dn)%TX_POOL_SIZE;
		cur_pipe.seq_no++;

//printk("TX,%02X,DN:%d,%02X\n",p_bulk_in->seq_no,dn,bulk_out.data[0]);
	}while(dev_state.rx_left>0);//while(device has space for receive)

	if(!dev_state.rx_left)DelayUs(2000);//2000

BIO_TAIL:
	return err;
}

int fs_mount_udisk(void)
{
	uchar a;

	if(!port_open_state[T_UDISK] && !port_open_state[T_UDISK_FAST])
	{
		set_udisk_absent();
		return USB_ERR_NOTOPEN;
	}

	//a=r_reg(rUSB_INT_STATUS);//UDISK absent
	//if(a&0x60)
	if((rHPRT&0x0F)!=0x05)
	{
		set_udisk_absent();
		port_open_state[T_UDISK]=0;
		port_open_state[T_UDISK_FAST]=0;
	}
	return 0;
}


unsigned char UsbHostSends(char *buf,unsigned short size)
{
	unsigned char a,tmpc;
	int i,tx_left;

    if(usb_host_state==5)return UsbHcdSends(buf,size);

	if(usb_host_state!=S_HCI_DETECTED)return USB_ERR_NOTOPEN;//not opened

	//--check if enough tx buffer
	tx_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;

	if(size>=TX_POOL_SIZE || size+tx_left>=TX_POOL_SIZE)
		return USB_ERR_BUF;//buffer occupied

	for(i=0;i<size;i++)
		usb0_tx_pool[(tx_write+i)%TX_POOL_SIZE]=buf[i];
	tx_write=(tx_write+size)%TX_POOL_SIZE;

	tmpc=host_bulkio('S');
	if(tmpc)
	{
//printk("tx bio:%d\n",tmpc);
		//a=r_reg(rUSB_INT_STATUS);
		if((rHPRT&0x0F)!=0x05)return USB_ERR_DEV_REMOVED;//device is removed

		return tmpc;
	}

	return 0;
}

int UsbHostRecvs(unsigned char *buf,unsigned short want_len,unsigned short timeout_ms)
{
	unsigned char tmpc,a;
	int i,j,t0;

    if(usb_host_state==5)return UsbHcdRecvs(buf,want_len,timeout_ms);

	if(usb_host_state!=S_HCI_DETECTED)return -USB_ERR_NOTOPEN;//not opened

	if(rx_read==rx_write)
	{
		tmpc=host_bulkio('R');
		if(tmpc)
		{
			//a=r_reg(rUSB_INT_STATUS);
			if((rHPRT&0x0F)!=0x05)return -USB_ERR_DEV_REMOVED;//device is removed

			return -tmpc;
		}
	}

	if(timeout_ms)
	{
		timeout_ms=(timeout_ms+9)/10;
		t0=s_Get10MsTimerCount();
	}
	i=0;
	while(i<want_len)
	{
		j=(rx_read+i)%RX_POOL_SIZE;
		if(j==rx_write)
		{
			//a=r_reg(rUSB_INT_STATUS);
			//if(a&0x60)return -USB_ERR_DEV_REMOVED;//device is removed
            tmpc=host_bulkio('R');
    		if(tmpc)
    		{
    			//a=r_reg(rUSB_INT_STATUS);
    			if((rHPRT&0x0F)!=0x05)return -USB_ERR_DEV_REMOVED;//device is removed

    			return -tmpc;
    		}

			if(!timeout_ms)break;
			if((s_Get10MsTimerCount()-t0)<timeout_ms)continue;
			if(i)break;

			return -USB_ERR_TIMEOUT;
		}

		buf[i]=usb0_rx_pool[j];
		i++;
	}//while()
	rx_read=(rx_read+i)%RX_POOL_SIZE;

	return i;
}

unsigned char UsbHostTxPoolCheck(void)
{
	unsigned char a,tmpc;

    if(usb_host_state==5) return UsbHcdTxPoolCheck();
	if(usb_host_state!=S_HCI_DETECTED)return USB_ERR_NOTOPEN;//not opened

	tmpc=host_bulkio('R');
	if(tmpc>1)
	{
		//a=r_reg(rUSB_INT_STATUS);
		if((rHPRT&0x0F)!=0x05)return USB_ERR_DEV_REMOVED;//device is removed

		return tmpc;
	}

	if(tx_write!=tx_read)
		return 1;//not finished

	return 0;
}

unsigned char UsbHostReset(void)
{
    unsigned char tmpc;

    if(usb_host_state==5)return UsbHcdReset();
	if(usb_host_state!=S_HCI_DETECTED)return USB_ERR_NOTOPEN;//not opened

    if(cur_pipe.device_id==T_PAXDEV)
	    tmpc=host_bulkio('R');
    
	rx_read=rx_write;//clear rx pool
	return 0;
}

//switch to usb host,and id must be A type!
unsigned char UsbHostOpen(int device_id)
{
	unsigned char a,tmpc;
	int i,tmpd;
    volatile unsigned int t0=0,t;

	if((usb_host_state<0)||(usb_host_state==4))return USB_ERR_NOTFREE;//occupied by device

	if (get_machine_type() == D200)
	{
	    /*if id is A type return error code*/
	    gpio_set_pin_type(GPIOB, USB_OTG_ID, GPIO_INPUT);
	    gpio_set_pull(GPIOB, USB_OTG_ID, 1);
	    gpio_enable_pull(GPIOB, USB_OTG_ID);
	    if(gpio_get_pin_val(GPIOB, USB_OTG_ID)) //B type
	        return USB_ERR_DEV_ABSENT;
	}
	
	reset_udev_state();

    //printk("\r%s-usb_host_state:%d\n",__func__,usb_host_state);

	if(usb_host_state<S_HCI_INITIALIZED||
        ((rG_INT_STS&1) ==0))//device mode
	{   
		a=UsbHostInit(0);
		if(a)
		{
            //printk("\rUsbHostInit failed!-%d\n",a);
            return a;
		}
	}

    if(device_id>=T_SCANNER)
    {
        last_device_id = 0;
        tmpd = UsbHcdOpen(device_id);
        if(tmpd==0)usb_host_state = 5;
        return tmpd;
    }
    UsbHcdStop();
    
	//1--if it's already opened
	//a=r_reg(rUSB_INT_STATUS);//device absent
	//if(!(a&0x60))

    //Port Enable and A device is attached to the port
    //and Port Enable/Disable No Change
    if(((rHPRT&0x0F)==0x05)&&last_device_id)
	{
        //printk("device_id:%d,last_device_id:%d\n",device_id,last_device_id);
        if((device_id!=T_NULL)&&(device_id!=last_device_id))return USB_ERR_DEV_ABSENT;
        port_open_state[last_device_id]=1;
        usb_host_state=S_HCI_DETECTED;
        return 0;        
	}

	//--detect the device up to twice
	tmpc=UsbHostDetect(device_id,0);
	if(tmpc)
	{
        //printk("%s hprt:%08X\n",__func__,rHPRT);
        if(device_id==T_PAXDEV)t=50;//500ms
        else t=200;///最长2秒钟等待
        if((rHPRT&0x0F)==0x05)
        {
            UsbHubReset();
            t0 = s_Get10MsTimerCount();
            while((rG_INT_STS&(1<<24))==0)
            {
                if((s_Get10MsTimerCount()-t0)>t)
                {
//                        printk("timeout hprt:%08X,sts:%08X\n",rHPRT,rG_INT_STS);
                    return tmpc;
                }
            }
        }
        else
        {
            UsbHostInit(0);
            t0 = s_Get10MsTimerCount();
            while((rHPRT&0x0F)!=0x03)
            {
                if((s_Get10MsTimerCount()-t0)>t)
                {
//                        printk("timeout hprt:%08X,sts:%08X\n",rHPRT,rG_INT_STS);
                    return tmpc;
                }
            }
        }
        if(UsbHostDetect(device_id,0)==0)tmpc = 0;
	}
//printk("Detect result:%d,id:%d\n",tmpc,cur_pipe.device_id);
	if(tmpc)
	{
		//usb_host_state=0;
		return tmpc;
	}
	if(cur_pipe.device_id==T_PAXDEV)
	{
        last_device_id = cur_pipe.device_id;
		port_open_state[cur_pipe.device_id]=1;
		return 0;
	}

	tmpd=usb_enum_massdev();
//printk("Enum UDISK result:%d\n",tmpd);
	if(tmpd)
	{
//		usb_host_state=0;
		return USB_ERR_DEV_QUERY;
	}
    last_device_id = cur_pipe.device_id; 
	port_open_state[cur_pipe.device_id]=1;
//printk("device_id:%d\n",cur_pipe.device_id);
	return 0;
}

//switch to usb host and enum udisk
uchar s_uDiskInit()
{
	unsigned char a,tmpc;
	int i,tmpd;
	int device_id =2;
	if((usb_host_state<0)||(usb_host_state==4))return USB_ERR_NOTFREE;//occupied by device

	reset_udev_state();
	if(usb_host_state<S_HCI_INITIALIZED)
	{        
		a=UsbHostInit(0);
        usbEnableVbusTick=-300;//使usb vbus上电监测失效
		if(a) return a;
	}
    if((rHPRT&0x0F)==0x05)
	{
		if(device_id!=T_NULL && port_open_state[device_id])return 0;
		if(device_id==T_NULL && (port_open_state[T_UDISK] ||
			port_open_state[T_UDISK_FAST] || port_open_state[T_PAXDEV]))return 0;
	}

	//--detect the device up to twice
	tmpc=UsbHostDetect(device_id,0);
	if(tmpc)
	{
		usb_host_state=0;
		return tmpc;
	}
	if(cur_pipe.device_id==T_PAXDEV)
	{
		port_open_state[cur_pipe.device_id]=1;
		return 0;
	}

	tmpd=usb_enum_massdev();
	if(tmpd)
	{
		usb_host_state=0;
		return USB_ERR_DEV_QUERY;
	}
	port_open_state[cur_pipe.device_id]=1;
	return 0;
}

/*
return:
0:In Host
Other:No Device or Device mode
*/
uint s_UsbHostState(void)
{
    if(usb_host_state!=S_HCI_DETECTED)return USB_ERR_NOTOPEN;
    if((rG_INT_STS&1) ==0)return USB_ERR_NOTOPEN;//device

    if((rHPRT&0x0F)!=0x05)return USB_ERR_DEV_ABSENT;
    return 0;
}

unsigned char UsbHostClose(void)
{
	unsigned char tmpc;

	if((usb_host_state<0)||(usb_host_state==4))return USB_ERR_NOTFREE;//occupied by device

	if(usb_host_state==5)
	{
		usb_host_state=S_HCI_INITIALIZED;
		return 0;
	}

    if(usb_host_state>0)
    	usb_host_state=S_HCI_INITIALIZED;

	if(cur_pipe.device_id!=T_PAXDEV && cur_pipe.device_id!=T_UDISK
	&& cur_pipe.device_id!=T_UDISK_FAST)return USB_ERR_NOTOPEN;//not opened

    if((cur_pipe.device_id==T_PAXDEV)&&(usb_host_state==S_HCI_DETECTED))
	    tmpc=host_bulkio('R');

	rx_read=rx_write;//clear rx pool
	port_open_state[cur_pipe.device_id]=0;
	return 0;
}

//#define USB_TEST
#ifdef USB_TEST

void dump_host(void)
{
    printk("\r%s\n",__func__);

    printk("\rrG_AHB_CFG:%X\n",rG_AHB_CFG);
    printk("\rrG_USB_CFG:%X\n",rG_USB_CFG);
    printk("\rrG_RST_CTL:%X\n",rG_RST_CTL);
    printk("\rrG_INT_STS:%X\n",rG_INT_STS);
    printk("\rrG_INT_MSK:%X\n",rG_INT_MSK);
    printk("\rrG_HW_CFG1:%X\n",rG_HW_CFG1);
    printk("\rrG_HW_CFG2:%X\n",rG_HW_CFG2);
    printk("\rrG_HW_CFG3:%X\n",rG_HW_CFG3);
    printk("\rrG_HW_CFG4:%X\n",rG_HW_CFG4);
    printk("\rrHCFG:%X\n",rHCFG);
    printk("\rrHAINT:%X\n",rHAINT);
    printk("\rrHAINTMSK:%X\n",rHAINTMSK);
    printk("\rrHPRT:%X\n",rHPRT);
    printk("\rrPCGCCTL:%X\n",rPCGCCTL);
    printk("\rrWC_PHY_CTL:%X\n",rWC_PHY_CTL);
    printk("\rrWC_GEN_CTL:%X\n",rWC_GEN_CTL);
    printk("\rrWC_VBUS_CTL:%X\n",rWC_VBUS_CTL);
}


void UsbTest(void)
{
	uchar tmpc,fn,tmps[10300],xstr[10300];
	int tmpd,rn,tn,i,loops;
	unsigned long byte_count,speed,elapse;
	uchar last_cc,a,b;

	UsbHostClose();
	ScrCls();
	PortOpen(0,"115200,8,n,1");
	while(1)
	{
T_BEGIN:
	ScrCls();
	ScrPrint(0,0,0,"USB HOST TEST,120509A");
	ScrPrint(0,1,0,"1-OPEN UDISK");
	ScrPrint(0,2,0,"2-OPEN SCANER");
	ScrPrint(0,3,0,"3-OPEN PAXDEV");
	ScrPrint(0,4,0,"4-OPEN P_USB");
	ScrPrint(0,5,0,"5-RX  6-RX/TX");
	ScrPrint(0,6,0,"7-RESET   9-CLOSE");
	ScrPrint(0,7,0,"8-OPEN NULL 0-DEVICE");

	fn=getkey();
	if(fn==KEYCANCEL)return;

	ScrCls();
	switch(fn)
	{
	case '1':
		//tmpc=UsbHostOpen(T_UDISK);
		tmpc=PortOpen(P_USB_HOST,"Udisk");
		ScrPrint(0,0,0,"OPEN:%d,SPEED:%d",tmpc,cur_pipe.speed_type);
		ScrPrint(0,1,0,"DEV VER:%04X",cur_pipe.device_version);
		getkey();
		break;
	case '2':
    case 5:
		//tmpc=UsbHostOpen(T_UDISK_FAST);
		tmpc=PortOpen(P_USB_HOST,"SCANER");
		ScrPrint(0,1,0,"OPEN RESULT:%d",tmpc);
		getkey();
		break;
	case '3':
    case 6:
		tmpc=PortOpen(P_USB_HOST,"PAXDEV");
		ScrPrint(0,1,0,"OPEN RESULT:%d,SPEED:%d",tmpc,cur_pipe.speed_type);
		ScrPrint(0,3,0,"DEVICE VERSION:%04X",cur_pipe.device_version);
		getkey();
		break;
	case '4':
		tmpc=PortOpen(P_USB,"115200,8,n,1");
		ScrPrint(0,1,0,"OPEN RESULT:%d",tmpc);
		getkey();
		break;
	case '8':
		//tmpc=UsbHostOpen(T_NULL);
		tmpc=PortOpen(P_USB_HOST,NULL);
		ScrPrint(0,1,0,"OPEN RESULT:%d,SPEED:%d",tmpc,cur_pipe.speed_type);
		ScrPrint(0,3,0,"DEVICE VERSION:%04X",cur_pipe.device_version);
		getkey();
		break;
	case '5':
        ScrPrint(0,0,0,"Recvs....");
        tmpd = PortRecvs(P_USB_HOST, tmps, 200, 5000);
        if(tmpd<0)
        {
            ScrPrint(0,0,0,"Error:%d",tmpd);
        }
        else
        {
            ScrCls();
            for(i=0;i<tmpd;i++)
                Lcdprintf("%c",tmps[i]);
        }
        getkey();
		break;
	case '7':
		tmpc=reset_pipe_port();
		ScrPrint(0,1,0,"RESET RESULT:%02X",tmpc);
		getkey();
		break;
	case '9':
        tmpc = PortReset(P_USB_HOST);
        ScrPrint(0,1,0,"RESET RESULT:%02X",tmpc);

		tmpc = PortClose(P_USB_HOST);//UsbHostClose();
		ScrPrint(0,2,0,"CLOSE 12 RESULT:%02X",tmpc);

		tmpc = PortClose(P_USB);//UsbHostClose();
		ScrPrint(0,4,0,"CLOSE 10 RESULT:%02X",tmpc);

        getkey();
		break;
	case '6'://RX/TX
		ScrPrint(0,0,0,"USB HOST RX/TX");

		loops=0;
		byte_count=0;
		TimerSet(0,60000);
		last_cc=0;
		while(61)
		{
			if(!TimerCheck(0))
			{
				TimerSet(0,60000);
				byte_count=0;
			}
			if(!kbhit())
			{
			  tmpc=getkey();
			  if(tmpc==KEYCANCEL)break;

              tmpc = PortTxPoolCheck(P_USB_HOST);
              ScrPrint(0,1,0,"CHECK RESLUT:%d",tmpc);
              
			  ScrPrint(0,2,0,"%d,RCV %d ",loops,tn);
			  ScrPrint(0,4,0,"%d,SENDING %d ",loops,tn);

				ScrPrint(0,5,0,"TM:%d ",TimerCheck(0));
				elapse=60000-TimerCheck(0);
				if(!elapse)elapse=1;
				speed=byte_count/elapse;
				speed*=10;
				ScrPrint(0,6,0,"SPEED:%d,%lu/%lu  ",speed,byte_count,elapse);
			}

			rn=0;
			while(611)
			{
				//tmpc=UsbHostRecv(xstr+rn,0);
				//tmpc=PortRecv(P_USB_HOST,xstr+rn,0);
				tmpd=PortRecvs(P_USB_HOST,xstr+rn,10240,0);
				if(tmpd<=0)break;
				rn+=tmpd;
				if(rn>=10240)break;
			}
			if(tmpd<0)
			{
				ScrPrint(0,6,0,"%d,RECV FAILED:%d ",loops,tmpd);
				//Beep();
				getkey();
				break;
			}
			if(!rn)continue;
//x_debug("Received:%d\n",rn);
			for(i=0;i<rn;i++)
			{
				a=xstr[i];
				if(a!=last_cc+1 && a)
				{
					//Beep();
					ScrPrint(0,6,0,"%d,RECV MISMATCHED,i:%d-%02X ",loops,i,a);
					getkey();
					for(i=0;i<rn;i++)PortSend(0,xstr[i]);
					goto T_BEGIN;
				}
				last_cc=a;
			}

			tn=rn;
			ScrPrint(0,2,0,"%d,SENDING %d ",loops,tn);
			//tmpc=UsbHostSends(xstr,tn);
			tmpc=PortSends(P_USB_HOST,xstr,tn);
			if(tmpc)
			{
				ScrPrint(0,6,0,"%d,SEND FAILED:%d ",loops,tmpc);
				//Beep();
				getkey();
				break;
			}

			byte_count+=tn*2;
			loops++;
		}
		break;
	case '0':
		UsbDevTests();
		break;
	}//switch(fn)
	}//while(1)
}
#endif

#if 0
void UsbHostTest(void)
{
    uchar tmpc,fn,tmps[10300],xstr[10300];
	int tmpd,rn,tn,i,loops;
	unsigned long byte_count,speed,elapse;
	uchar last_cc,a,b;
    
    printk("\rUsbHostTest 20120507_01\n");

    while(1)
    {
        //tmpd = UsbHostOpen(T_PAXDEV);
        tmpd = PortOpen(P_USB_HOST, "PAXDEV");
        DelayMs(1000);
        if(tmpd) printk("\rUsbHostOpen:%d\n",tmpd);
        else break;
    }
    printk("\rDetect a device\n");

	loops=0;
	while(61)
	{
		rn=0;
		while(611)
		{
			//tmpc=UsbHostRecv(xstr+rn,0);
			//tmpc=PortRecv(P_USB_HOST,xstr+rn,0);
			//tmpd=UsbHostRecvs(xstr+rn,10240,0);
			tmpd = PortRecvs(P_USB_HOST,xstr+rn,10240,0);
			if(tmpd<=0)break;
			rn+=tmpd;
			if(rn>=10240)break;
		}
		if(tmpd<0)
		{
            printk("UsbHostRecvs Failed!-%d\n",tmpd);
			while(1);
		}
		if(!rn)continue;
        //printk("Received:%d\n",rn);
		tn=rn;
		//tmpc=UsbHostSends(xstr,tn);
        tmpc=PortSends(P_USB_HOST,xstr,tn);
		if(tmpc)
		{
			printk("%d,SEND FAILED:%d ",loops,tmpc);
            while(1);
		}
		if(loops%100 ==0)printk("\r%d,SENDING %d ",loops,tn);

		loops++;
	}

    while(1);
}
#endif
