#include "sl811.h"
#include <stdarg.h>
#include "..\comm\comm.h"
#include <Base.h>
#include "ftdi_sio.h"
#include "..\fat\fsapi.h"


extern int usb_host_state;

SL811_DEV usb_dev;
URB usb_urb;
static int send_flag = 0;
static uchar send_buf[512];
static uchar recv_buf[512];

#define TM_USB 9/*internal soft timer no for USB*/

//--error code define of API call
#define USB_ERR_NOTOPEN       3
#define USB_ERR_DEV_ID        10
#define USB_ERR_DEV_ABSENT    16

//--USB registers
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

#define USB_ADDR 0x40200000

#define TIMEOUT_RETRY 0x2 /*Maximum no. of timeout retry during USB xfer*/
#define SEND_TM 5
#define RECV_TM 3
#define CYCLE  (SEND_TM+RECV_TM*2)
#define SEND_LIMIT 10
#define RECV_LIMIT 64
#define FTDI_BUF_SIZE 8193
#define __u32 unsigned long

typedef struct xmit {
	int tail;
	int head;
	unsigned char *buf;
} circ_buf_t;

extern unsigned char usb0_tx_pool[],usb0_rx_pool[];
static circ_buf_t ftdi_write_buf;
static circ_buf_t ftdi_read_buf;
static int ftdi_port_state = 0;

void ftdi_interrupt(int id);

#define CIRC_CNT(head, tail, size) (((head) - (tail) + (size)) % (size))
#define CIRC_SPACE(head, tail, size) CIRC_CNT((tail),((head)+1),(size))
//extern volatile uint k_Timer4Count10MS;
static unsigned int backup_hcchar=0,backup_hctsiz=0;
static unsigned char *backup_data=0;

void retry_urb(void)
{
    int t0;

    //printk("\r%s\n",__func__);

    t0 = s_Get10MsTimerCount();
    
    rHCINT0=~0;
    if (rHCCHAR0&(1<<31))//channel enable
    {
        rHCCHAR0|=(1<<30);//bit30:channel disable
        //printk("\rHALT CHANNEL START\n");
        while(1)
        {
            if(rHCINT0&(1<<1)) break;//channel halted
            if((s_Get10MsTimerCount()-t0)*10 >500) return ;
        }
        rHCINT0=~0;
        //printk("\rHALT CHANNEL SUCCESS\n");
	}

    
    rHCDMA0=(unsigned int)backup_data;
    rHCTSIZ0=backup_hctsiz;
    rHCCHAR0=backup_hcchar;
}

void sl811_submit_urb(SL811_DEV *dev, URB *urb)
{
	unsigned char xferLen, cmd, reg_offset=0;
	unsigned char out[8];
	unsigned char *buffer = NULL;
    unsigned char * data;
    volatile unsigned int hcchar;
    volatile unsigned int hctsiz;
    int t0;

    t0 = s_Get10MsTimerCount();


	usb_debug("sl811_submit_urb addr=%d.............\n", urb->usbaddr);
	dev->retry_counter[0] = 0;
	if(dev->ep[urb->endpoint].wPayLoad==0)
	{
		usb_debug("EP[%d] wPayLoad %d bad!\n",
			urb->endpoint, dev->ep[urb->endpoint].wPayLoad);
		assert(0);
	}

	data=(uchar*)USB_ADDR;	// buffer address
	xferLen = 0;

	if(urb->buffer_len>0)
	{
		u32 len = urb->buffer_len-urb->actual_len;
		if(len<=0)
		{
			usb_debug("sl811_submit_urb: buffer len %d, actual len %d\n",
				urb->buffer_len, urb->actual_len);
			assert(0);
		}
		if(len > dev->ep[urb->endpoint].wPayLoad)
			xferLen = dev->ep[urb->endpoint].wPayLoad;
		else
			xferLen = (unsigned char)len;
		usb_debug("xferLen %d\n", xferLen);
		buffer = urb->buffer+urb->actual_len;
	}
    rHCINT0=~0;
    if (rHCCHAR0&(1<<31))//channel enable
    {
        rHCCHAR0|=(1<<30);//bit30:channel disable
        //printk("\rHALT CHANNEL START\n");
        while(1)
        {
            if(rHCINT0&(1<<1)) break;//channel halted
            if((s_Get10MsTimerCount()-t0)*10 >500)return ;
        }
        rHCINT0=~0;
        //printk("\rHALT CHANNEL SUCCESS\n");
	}
    hcchar=rHCCHAR0;
    hcchar &=~(0xfffff);//clear bit19~bit0
    
	hctsiz = 0;
	hctsiz |= 1<<19;//bit28~bit19:packet count


	// For IN token
	if (urb->pid==PID_IN)				// for current IN tokens
	{												//
		//cmd = sDATA0_RD;			// FS/FS on Hub, sync to sof
		if(usb_get_toggle(dev, urb->endpoint)) hctsiz|=2<<29;//bit30~bit29:set to data1
        hcchar|=1<<15;//bit15:set to in

        //printk("\rPID_IN:%d\n",xferLen);
	}
	// For OUT token
	else if(urb->pid==PID_OUT)				// for OUT tokens
	{	
        if(usb_get_toggle(dev, urb->endpoint)) hctsiz|=2<<29;//bit30~bit29:set to data1
		//cmd = sDATA0_WR;						// FS/FS on Hub, sync to sof
		if(xferLen) 								// only when there are	
		{
			//sl811_buf_write(data,buffer,xferLen);	// data to transfer on USB
			//w_mem(data,xferLen,buffer);
            memcpy(data,buffer,xferLen);
		}
	}
	//------------------------------------------------
	// For SETUP/OUT token
	//------------------------------------------------
	else if(urb->pid == PID_SETUP)// for current SETUP/OUT tokens
	{
		usb_set_toggle(dev, urb->endpoint, 0);//always start DATA0
		out[0]=urb->setup.bmRequest;
		out[1]=urb->setup.bRequest;
		out[2]=(unsigned char)urb->setup.wValue;
		out[3]=(unsigned char)(urb->setup.wValue>>8);
		out[4]=(unsigned char)urb->setup.wIndex;
		out[5]=(unsigned char)(urb->setup.wIndex>>8);
		out[6]=(unsigned char)urb->setup.wLength;
		out[7]=(unsigned char)(urb->setup.wLength>>8);
		//sl811_buf_write(data,out,8);
		//w_mem(data,8,out);
		memcpy(data,out,8);
		//cmd = sDATA0_WR;							// FS/FS on Hub, sync to sof
        hctsiz|=3<<29;//bit30~bit29:set to setup;
		xferLen = 8;
	}else
	{
		usb_debug("Unknow PID=%02x\n", urb->pid);
		assert(0);
	}
    hctsiz|=xferLen;
    if(urb->endpoint)hcchar|=0x2<<18;//ep bulk
    hcchar |=(urb->endpoint<<11)|dev->ep[urb->endpoint].wPayLoad|(1<<31);//set ep number,mps,hc enable

    DelayUs(2);

    backup_data=data;
    backup_hctsiz=hctsiz;
    backup_hcchar=hcchar;

    rHCDMA0=(unsigned int)data;
    rHCTSIZ0=hctsiz;
    rHCCHAR0=hcchar;
	urb->cmd = cmd;
	urb->xferLen = xferLen;
}


static void ftdi_status_packet(SL811_DEV *dev, URB *urb)
{
	usb_debug("SETUP-->%s(DATA Stage)-->%s(Status Stage)\n\r",
		urb->pid==PID_IN?"IN":"OUT",
		urb->next_pid==PID_IN?"IN":"OUT"
		);
	urb->pid = urb->next_pid;
	urb->next_pid = 0;
//	urb->buffer_len = 0;
	usb_set_toggle(dev, urb->endpoint, 1);

	sl811_submit_urb(dev, urb);
}

/*
** Control Transaction:
** 1.write : SETUP(data0)--->OUT(data1)*N--->IN STATUS(data1)
** 2.read  : SETUP(data0)--->IN(data1)*N----->OUT STATUS(data1)
** 3.no data: SETUP(data0)---->IN STATUS(data1)
** Bulk Transaction:
** Send: OUT(data0)--->OUT(data1)--->
** Rcv:   IN(data0)--->IN(data1)---->
**
**/
static void ftdi_done(SL811_DEV *dev, URB *urb)
{
	u8 selector = 0;
	//u8 result, remainder;
    u8 remainder;
	u8 len;
    volatile unsigned int hcint;
    volatile unsigned int hctsiz;
    unsigned char * data;

 //D7:STALL,D6:NAK,D5:overflow,D4:setup,D3:sequence,D2:TIMEOUT,D1:error,D0:ACK
	//result=r_reg(SL811AB(rUSB_A_PID_STATUS, selector));// read EP0status register
	//remainder = r_reg(SL811AB(rUSB_A_ADDR_COUNT, selector));// remainder value in last pkt xfer
    hcint=rHCINT0;
    hctsiz = rHCTSIZ0;
	data=(uchar*)USB_ADDR;	// buffer address
	remainder=(hctsiz&(0x7ffff));
	//usb_debug("sl811_done: Result %02x, remainder %02x\n\r", result, remainder);
	//usb_debug("sl811_done: Addr %02x EP[%d]\n\r",dev->uAddr, urb->endpoint);
	//if (result & 0x01)// Transmission ACK
    if((hcint&(1<<1)) && (hcint&1) && (hcint&(1<<5)))
    {
		usb_debug("sl811_done: result ACK\n\r");
		usb_do_toggle(dev, urb->endpoint);
		// SETUP TOKEN
		if(urb->pid == PID_SETUP)
		{
			if(urb->next_pid==PID_IN||urb->next_pid==PID_OUT)
			{
				usb_debug("sl811_done: SETUP---->");
				urb->prev_pid = PID_SETUP;
				urb->pid = urb->next_pid;
				if(urb->buffer_len>0)
				{
					urb->next_pid = (urb->pid==PID_IN)?PID_OUT:PID_IN;
					usb_debug("%s(Data stage)",
						urb->pid==PID_IN?"IN":"OUT"
						);
				}else 
				{
					urb->next_pid = PID_IN;
					usb_debug("IN(status stage)");
				}
				usb_debug("\n\r");
				sl811_submit_urb(dev, urb);
			}else
			{
				usb_debug("sl811_done: SETUP Done\n");
				urb->state = URB_DONE;
			}
			return;
		}	
		// OUT TOKEN				
		else if(urb->pid == PID_OUT)
		{
			urb->actual_len += urb->xferLen;
			usb_debug("sl811_done: OUT Data %d, %d\n\r", urb->buffer_len, urb->actual_len);
			if(urb->buffer_len>urb->actual_len)
			{
				sl811_submit_urb(dev, urb);
			}else
			{
				if(urb->next_pid == PID_IN)
				{
					//status Stage
					ftdi_status_packet(dev, urb);
				}else
				{
					usb_debug("sl811_done: OUT Data %d Done\n\r", urb->actual_len);
					urb->state = URB_DONE;
					if(dev->finish_entry)
						(*(dev->finish_entry))(dev, urb);
				}
			}
			return;
		}	
		// IN TOKEN
		else if(urb->pid == PID_IN)
		{	// for IN token only
		//printk("\rdone IN:%d,%d,0x%X\n",remainder,urb->xferLen,hcint);
			//------------------------------------------------	
			// If host requested for more data than the slave 
			// have, and if the slave's data len is a multiple
			// of its endpoint payload size/last xferLen. Do 
			// not overwrite data in previous buffer.
			//------------------------------------------------	
			//if(result&0x20)//overflow
			if(hcint&(1<<9))
			{
				usb_debug("sl811_done:  OVERFLOW\n");
				remainder = 0;
			}
			if(remainder < urb->xferLen)
			//if((hctsiz&(0x7ffff))< urb->xferLen)
			{
				len = urb->xferLen-remainder;
				usb_debug("sl811_done: IN len=%d\n\r", len);
				//sl811_buf_read(data, urb->buffer+urb->actual_len, len);
				//r_mem(data,len,urb->buffer+urb->actual_len);
                memcpy(urb->buffer+urb->actual_len,data,len);
				//force_urb_printf(urb->buffer+urb->actual_len, len, "IN Data");
				urb->actual_len += len;
                //printk("\r0x%X,0x%X\n",data[0],data[1]);
			}
			usb_debug("sl811_done: IN Data %d, %d\n\r", urb->buffer_len, urb->actual_len);
			
			if(remainder||(urb->actual_len==urb->buffer_len))
			{
				if(urb->next_pid == PID_OUT)
				{
					//status Stage
					ftdi_status_packet(dev, urb);
				}else
				{
					urb->state = URB_DONE;
					if (dev->finish_entry)
						(*(dev->finish_entry))(dev, urb);

					usb_debug("sl811_done: IN Data %d Done\n", urb->actual_len);
					if(dev->state == UDEV_FINISHED&&
						urb->actual_len != urb->request_len
					)
					{
						usb_debug("sl811_done: In data %d, but request %d\n",
							urb->actual_len,
							urb->request_len
							);
					}
				}
			}else
			{
				sl811_submit_urb(dev, urb);
			}
	
			return;
		}// PID IN		
		else
		{
			usb_debug("sl811_done: Unknow PID=%02x\n", urb->pid);
		}
		return;
	}

	//-----------------NAK ----------------------
	else
	//if (result & 0x40)
	if(hcint&(1<<4))
	{	
		//printk("sl811_done: result NAK\n");
		if (urb->endpoint != 0) {
			urb->state = URB_ERROR;
			urb->err_code = URB_NAK;
			return;
		}

		dev->retry_counter[0] ++;
		if(dev->retry_counter[0] <= 3)
		{
			//printk("EP[%d] retry %d\n", urb->endpoint, dev->retry_counter[selector]);
            retry_urb();
			//w_reg(rUSB_INT_STATUS,0xff);// clear interrupt status, need to
			//w_reg(SL811AB(rUSB_A_CTRL,selector),urb->cmd);
									// re-arm and request for last cmd again
		}else
		{
			urb->state = URB_ERROR;
			urb->err_code = URB_NAK;
		}
	}	
	//-----------------TIMEOUT ----------------------
	else
	//if (result & 0x04)
	if(hcint&(1<<7))
	{	
		//printk("sl811_done: result TIMEOUT\n");
		if(urb->endpoint == 0)
		{
			if(0&&dev->retry_counter[selector]<TIMEOUT_RETRY)
			{
				//printk("EP[%d] retry\n", urb->endpoint);
                retry_urb();
				//w_reg(rUSB_INT_STATUS,0xff);// clear interrupt status, need to
				//w_reg(SL811AB(rUSB_A_CTRL,selector),
				//		urb->cmd
				//		);					// re-arm and request for last cmd again
				dev->retry_counter[selector]++;
			}else
			{
				urb->state = URB_ERROR;
				//if(result&0x40)//NAK
				if(hcint&(1<<4))
					urb->err_code = URB_NAK;
				//else if(result&0x04)//TIMEOUT
                else if(hcint&(1<<7))
					urb->err_code = URB_TIMEOUT;
				urb->err_code = URB_NAK;
			}
		} else {
			urb->state = URB_ERROR;
			urb->err_code = URB_TIMEOUT;
		}
	}	
	
	//-----------------------STALL----------------------------
	else
	//if (result & 0x80)// STALL detected
	if(hcint&(1<<3))
	{
		//printk("sl811_done: result STALL\n\r");
		urb->state = URB_ERROR;
		urb->err_code = URB_STALL;
	}
																	
	//----------------------OVEFLOW---------------------------
	else
	//if (result & 0x20)//overflow
	if(hcint&(1<<9))
	{
		//printk("sl811_done: result OVERFLOW\n\r");
		urb->state = URB_ERROR;
		urb->err_code = URB_OVERFLOW;
	}
	//-----------------------ERROR----------------------------
	else
	//if (result & 0x02)
	if((hcint&(1<<2))||(hcint&(1<<6))||(hcint&(1<<8))||(hcint&(1<<10)))
	{
		//printk("sl811_done: result ERROR\n\r");
		urb->state = URB_ERROR;
		urb->err_code = URB_NAK;
	}
  return;
}

static int ftdi_urb_enq(SL811_DEV *dev, URB *urb)
{
	int err;

	urb->state = URB_DOING;
	urb->err_code = 0;
	sl811_submit_urb(dev, urb);
	volatile uint time = s_Get10MsTimerCount() + 100;
	usb_debug("start wait\r\n");
	while (time > s_Get10MsTimerCount())//wait 1 Sec
	{
		if (urb->state == URB_DONE)
		{
			usb_debug("sl811_wait: URB DONE!\n\r");
			return FS_OK;
		}
		if (urb->state == URB_ERROR)
		{
			usb_debug("sl811_wait: urb error=%d\n", urb->err_code);
			return -1;
		}
	}
	usb_debug("time out\r\n");
	return FS_ERR_TIMEOUT;
}

//*****************************************************************************************
// Control Endpoint 0's USB Data Xfer
// ep0Xfer, endpoint 0 data transfer
//*****************************************************************************************
static int ftdi_control(SL811_DEV *dev, URB *urb)
{
	int err;
	usb_debug("--------- EP0 Control ------------\n\r");

	urb->next_pid = 0;
	urb->actual_len = 0;

	//----------------------------------------------------
	// SETUP token with 8-byte request on endpoint 0
	//----------------------------------------------------
	usb_debug("sl811_Control: SETUP\n\r");
	urb->endpoint=0;
	urb->pid=PID_SETUP;
	
	urb->next_pid  = PID_IN;
	//----------------------------------------------------
	// IN or OUT data stage on endpoint 0	
	//----------------------------------------------------
	urb->request_len = urb->setup.wLength;
	urb->buffer_len = urb->request_len;
   	if (urb->buffer_len)// if there are data for transfer
	{
		if (urb->setup.bmRequest & 0x80)		// host-to-device : IN token
		{			
			usb_debug("sl811_Control: IN %d\n\r", urb->buffer_len);
			urb->next_pid  = PID_IN;	
		}
		else											// device-to-host : OUT token
   		{
   			usb_debug("sl811_Control: OUT %d\n\r", urb->buffer_len);
			urb->next_pid  = PID_OUT;
		}
	}

	usb_debug(" control addr = %d\n\r", usb_urb.usbaddr);

	err = ftdi_urb_enq(dev, urb);
	if(err<0)
	{
		usb_debug("Fail in SETUP err = %d\n\r", err);
	}
	if(urb->actual_len != urb->request_len)
	{
		return FS_ERR_HW;
	}
	return err;
}

static int ftdi_usb_control_msg(SL811_DEV *dev, URB *urb, uchar bmRequestType, uchar bmRequest, ushort wValue, 
						   ushort wIndex, ushort wLength, uchar *data)
{
	urb->usbaddr = dev->uAddr;
	urb->setup.bmRequest = bmRequestType;
	urb->setup.bRequest = bmRequest;
	urb->setup.wValue = wValue;
	urb->setup.wIndex = wIndex;
	urb->setup.wLength = wLength;
	urb->buffer = data;
	return ftdi_control(dev, urb);
}

static int ftdi_set_configs(SL811_DEV *dev, URB *urb)
{
	uchar buf[128];
	u8 i, epLen;
	u8 uAddr = dev->uAddr;
	u8 bIntfSet;   /* interface setting */
	u8 bIntfIndex; /* interface index   */
	u16 len;
	pCfgDesc cfg;
	pIntfDesc intf;
	pEPDesc ep;
	int err;

	usb_debug("*********************Set Configs********************\n\r");
	cfg = (pCfgDesc)buf;
	memset(buf, 0x00, sizeof(buf));
	err = ftdi_usb_control_msg(dev, urb, 0x80, GET_DESCRIPTOR, 0x0200, 0, 9, buf);
	if (err)
	{
		usb_debug("Fail in Get Slave USB Standard Configuration Descriptors\n\r");
		return err;
	}
	len = cfg->wLength;
	usb_debug("USB Configuration Descriptors total len = %d\n\r", len);
	usb_debug("interface = %d\n\r", cfg->bNumIntf);

	DelayMs(2);

	usb_debug("********** Get Slave USB All Configuration Descriptors ********\n\r");
	memset(buf, 0x00, sizeof(buf));
	err = ftdi_usb_control_msg(dev, urb, 0x80, GET_DESCRIPTOR, 0x0200, 0, len, buf);
	if (err)
	{
		usb_debug("Fail in Get Slave USB All Configuration Descriptors\n\r");
		return err;
	}

	for (i = 1; i < MAX_EP; i++)
		memset(dev->ep+i, 0x00, sizeof(SL811_EP));

	i = 1;
	for (cfg = (pCfgDesc)buf; cfg < (pCfgDesc)(buf+len); )
	{
		switch(cfg->bType)
		{
			case CONFIGURATION:
				break;
			case INTERFACE:
				intf = (pIntfDesc)cfg;
				bIntfIndex = intf->iNum;
				bIntfSet = intf->iAltString;
				break;
			case ENDPOINT:
				ep = (pEPDesc)cfg;
				if(i >= MAX_EP)break;

				dev->ep[i].bAddr = ep->bEPAdd;		  	// Ep address and direction
				dev->ep[i].bAttr = ep->bAttr;		  	// Attribute of Endpoint			
				dev->ep[i].wPayLoad	= ep->wPayLoad;   	//
				dev->ep[i].bInterval = ep->bInterval; 	//
				usb_set_toggle(dev, i, 0);            	// init data toggle
				
				if (ep->bAttr == 0x2)//bulk transfer
				{
					if (ep->bEPAdd&0x80)dev->epbulkin = i;//(ep->bEPAdd)&0xf;
					else dev->epbulkout = i;			  //(ep->bEPAdd)&0xf;
				}
				//printk("\rep[%d]:addr = %d, attr = %d\r\n", i, dev->ep[i].bAddr, dev->ep[i].bAttr);
				//printk("\rep[%d]:payload = %d, interval = %d\r\n", i, dev->ep[i].wPayLoad, dev->ep[i].bInterval);
				i++;
				break;
			default:
				break;
		}
		cfg = (pCfgDesc)((u8*)cfg+cfg->bLength);
	}

	//printk("\repbulkin %02x, epbulkout %02x\n\r",dev->epbulkin, dev->epbulkout);

	usb_debug("********** Set configuration ********\n\r");
	err = ftdi_usb_control_msg(dev, urb, 0x00, SET_CONFIG, DEVICE, 0, 0, NULL);
	if (err)
	{
		usb_debug("Fail in Set Configuration!\n\r");
		return err;								
	}
	
	usb_debug("********** Set interface ********\n\r");
	err = ftdi_usb_control_msg(dev, urb, 0x01, SET_INTERFACE, bIntfSet, bIntfIndex, 0, NULL);
	if (err)
	{
		usb_debug("Fail in Set Interface!\n\r");
		return err;								
	}
	
	usb_dev.state = UDEV_FINISHED;
	return FS_OK;
}

static int enum_dev_ftdi(SL811_DEV *dev, URB *urb)
{
	uchar buf[128];
	u8 a,epLen,config_num;
	pDevDesc dev_desc;
	int err,tmpd,i;
	volatile unsigned int hprt0;
	volatile unsigned int hprt0_modify;


//1 Reset device
    i=0;
    while ((rG_INT_STS&(1<<24))==0)
    {
		DelayUs(1);
		if (++i > 100000) return USB_ERR_DEV_ABSENT;
	}

	hprt0_modify=hprt0 = rHPRT;

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
	while((rHCCHAR0&(1<<31))==1) 
	{
		if (++i> 500) 
		{
			//printk("%s: Unable to clear halt on channel %d\n",__func__, 0);
			break;
		}
	} 
    
    rHCCHAR0&=~(0x7f<<22);//set device addr to 0

	//3--reset relevant variables
	usb_dev.fail_state = usb_dev.state;
	usb_dev.state = UDEV_NOPRESENT;
	usb_urb.state = URB_ERROR;
	usb_urb.err_code = URB_STALL;
	ftdi_port_state = 0;

    rHCINTMSK0=0x02;
    rHAINTMSK=0x01;
    rG_INT_MSK=(1<<24)|(1<<25)|(1<<3);
    rG_AHB_CFG|=1;
    rHCINT0=~0;
    rG_INT_STS=~0;

    enable_irq(IRQ_OUSB0);

	DelayMs(10);
//x_debug("********** Get USB Device Descriptors ********\n\r");
	memset(dev, 0x00, sizeof(SL811_DEV));
	memset(urb, 0x00, sizeof(URB));
	dev->ep[0].wPayLoad = 8;
	dev_desc = (pDevDesc)buf;
	memset(buf, 0x00, sizeof(buf));
	err = ftdi_usb_control_msg(dev, urb, 0x80, GET_DESCRIPTOR, 0x0100, 0, 18, buf);
	if (err)
	{
        //printk("\rFail in Get wPayload of Endpoint 0:%d\r\n",err);
		return 100;
	}
	dev->ep[0].wPayLoad = dev_desc->bMaxPacketSize0;
	//printk("\rEP0 PayLoad = %02x, idVendor = 0x%04x, idProduct = 0x%04x\r\n", dev->ep[0].wPayLoad, dev_desc->idVendor, dev_desc->idProduct);
	if (dev_desc->idVendor != FTDI_VID || dev_desc->idProduct != FTDI_8U232AM_PID)
	{
		return USB_ERR_DEV_ID;
	}

	usb_debug("********** Set USB Address ********\n\r");
	err = ftdi_usb_control_msg(dev, urb, 0x00, SET_ADDRESS,3, 0x00, 0x00, NULL);
	if (err)
	{
		usb_debug("Fail in SetAddress!\n");
		return 110;
	}
	dev->uAddr = 3;
    
    tmpd=rHCCHAR0;
    tmpd&=~(0x7f<<22);//clear device addr
    tmpd|=3<<22;//set device addr to 3
    rHCCHAR0=tmpd;


	err = ftdi_set_configs(dev, urb);
	if (err)
	{
		return 120;
	}
	usb_debug("Enum USB DEV OK!\n\r");

	return 0;
}

static void ftdi_bulk_out_callback(struct sl811_dev *dev, struct urb_s *urb)
{
	int ret = usb_urb.actual_len;
	if (ret > 0) {
		ftdi_write_buf.tail = (ftdi_write_buf.tail + ret) % FTDI_BUF_SIZE;
	}
}

static void ftdi_bulk_in_callback(struct sl811_dev *dev, struct urb_s *urb)
{	
	uchar *buf = usb_urb.buffer;
	int temp;
	int ret = usb_urb.actual_len;

	if (ret >= 2)
	{
		if(buf[1] & FTDI_RS_TEMT)send_flag = 1;
		else send_flag = 0;
	}

	if (ret - 2 > 0 )
	{
		ret -= 2;
		int space = CIRC_SPACE(ftdi_read_buf.head, ftdi_read_buf.tail, FTDI_BUF_SIZE);
		if (space < ret)//cover old data
		{
			ftdi_read_buf.tail = (ftdi_read_buf.tail + ret - space) % FTDI_BUF_SIZE;
		}

		if (ftdi_read_buf.head + ret > FTDI_BUF_SIZE)
		{
			temp = FTDI_BUF_SIZE - ftdi_read_buf.head;
			memcpy(ftdi_read_buf.head+ftdi_read_buf.buf, buf+2, temp);
			memcpy(ftdi_read_buf.buf, buf+2+temp, ret-temp);
		}
		else
		{
			memcpy(ftdi_read_buf.head+ftdi_read_buf.buf, buf+2, ret);
		}
		ftdi_read_buf.head = (ftdi_read_buf.head + ret) % FTDI_BUF_SIZE;
	} 
}

static int ftdi_bulk_out(SL811_DEV *dev, URB *urb, void *buffer, int len)
{
	int ret;

	if (dev->state != UDEV_FINISHED)
	{
		usb_debug("ftdi_bulk_out: no such dev!\n");
		return FS_ERR_NODEV;
	}
	if(usb_urb.state==URB_DOING)
        return FS_ERR_NODEV;
	
	memset(urb, 0, sizeof(URB));
	urb->usbaddr = dev->uAddr;
	urb->endpoint = dev->epbulkout;
	urb->pid = PID_OUT;
	urb->next_pid = 0;
	urb->request_len = len;
	urb->buffer_len  = len;
	urb->buffer = buffer;
	urb->actual_len  = 0;
	
	dev->nak_delay = 1;
	dev->finish_entry = ftdi_bulk_out_callback;

	urb->state = URB_DOING;
	urb->err_code = 0;
	sl811_submit_urb(dev, urb);

	return 0;
}

static int ftdi_bulk_in(SL811_DEV *dev, URB *urb, void *buffer, int len)
{
	int ret;

	if(usb_dev.state != UDEV_FINISHED)
	{
		usb_debug("ftdi_bulk_in: no such dev!\n");
		return FS_ERR_NODEV;
	}
	if(usb_urb.state==URB_DOING)
        return FS_ERR_NODEV;

	memset(urb, 0, sizeof(URB));
	urb->usbaddr = dev->uAddr;
	urb->endpoint = dev->epbulkin;
	urb->pid = PID_IN;
	urb->next_pid = 0;
	urb->request_len = len;
	urb->buffer_len  = len;
	urb->buffer = buffer;
	urb->actual_len  = 0;
	
	dev->nak_delay = 1;
	dev->finish_entry = ftdi_bulk_in_callback;

	urb->state = URB_DOING;
	urb->err_code = 0;
	sl811_submit_urb(dev, urb);
}

static void ftdi_comm(void)
{
	static uint tick = 0;
	int temp;
	int count;

	if (ftdi_port_state == 0)
		return;

	tick ++;
	if(tick % CYCLE == 0 || tick % CYCLE == (SEND_TM+RECV_TM))//receive process
	{
		memset(recv_buf, 0x00, sizeof(recv_buf));
		ftdi_bulk_in(&usb_dev, &usb_urb, recv_buf, RECV_LIMIT);
	}

	if(tick % CYCLE == SEND_TM)//send process
	{
		count = CIRC_CNT(ftdi_write_buf.head, ftdi_write_buf.tail, FTDI_BUF_SIZE);
		if (count > 0 && send_flag == 1)
		{
			memset(send_buf, 0x00, sizeof(send_buf));
			if (count > SEND_LIMIT)count = SEND_LIMIT;

			if (ftdi_write_buf.tail + count > FTDI_BUF_SIZE)
			{
				temp = FTDI_BUF_SIZE - ftdi_write_buf.tail;
				memcpy(send_buf, ftdi_write_buf.buf+ftdi_write_buf.tail, temp);
				memcpy(send_buf+temp, ftdi_write_buf.buf, count-temp);
			}
			else
			{
				memcpy(send_buf, ftdi_write_buf.buf+ftdi_write_buf.tail, count);
			}
			ftdi_bulk_out(&usb_dev, &usb_urb, send_buf, count);
		}
		else//receive process
		{
			memset(recv_buf, 0x00, sizeof(recv_buf));
			ftdi_bulk_in(&usb_dev, &usb_urb, recv_buf, RECV_LIMIT);
		}
	}
}

void ftdi_interrupt(int id)
{
	//u8 intr;
    u32 gintsts;
    unsigned int tmpd;
    gintsts = rG_INT_STS;

//	intr = r_reg(rUSB_INT_STATUS);
	//if ((intr & USB_RESET) || (intr & INSERT_REMOVE))
	//printk("\rgintsts:0x%X\n",gintsts);
    if(gintsts&(1<<24))
    {
        if((rHPRT&0x0F)!=0x05)
        {
    		//GPIO1_IEN_CLR_REG = BIT29; //disable interrupt
            disable_irq(IRQ_OUSB0);

    		usb_dev.fail_state = usb_dev.state;
    		usb_dev.state = UDEV_NOPRESENT;
    		usb_urb.state = URB_ERROR;
    		usb_urb.err_code = URB_STALL;
    		ftdi_port_state = 0;
        }
        else
        {
            tmpd = rHPRT;
        	tmpd&=~((1<<1)|(1<<2)|(1<<3));
        	rHPRT = tmpd;
        }
		//w_reg(rUSB_INT_STATUS, DEVICE_DETECT|INSERT_REMOVE);
	}

	//if (intr & 0x01)//A_DONE
	if(gintsts&(1<<25))
	{
        if(rHCINT0&(1<<1))
        {
    		//printk("chhld:0x%X\n\r",rHCINT0);
    		//w_reg(rUSB_INT_STATUS, 0x01);
    		if (usb_urb.state == URB_DOING)
    		{
    			ftdi_done(&usb_dev, &usb_urb);
    		}
            rHCINT0=~0;
        }
	}

	//if (intr & 0x10)//SOF_TIMER
    if(gintsts&(1<<3))
    {
		//w_reg(rUSB_INT_STATUS, 0x10);
		ftdi_comm();			
	}

	//w_reg(rUSB_INT_STATUS, 0xff);
	rG_INT_STS=gintsts;
}

static __u32 ftdi_232bm_baud_base_to_divisor(int baud, int base)
{
	unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
	__u32 divisor;
	int divisor3 = base / 2 / baud; // divisor shifted 3 bits to the left

	divisor = divisor3 >> 3;
	divisor |= (__u32)divfrac[divisor3 & 0x7] << 14;
	/* Deal with special cases for highest baud rates. */
	if (divisor == 1) divisor = 0; else	// 1.0
	if (divisor == 0x4001) divisor = 1;	// 1.5

	return divisor;
}

static __u32 ftdi_232bm_baud_to_divisor(int baud)
{
	 return(ftdi_232bm_baud_base_to_divisor(baud, 48000000));
}

static int ftdi_set_termios(ulong speed, uint data_bits, uchar parity, uint stop_bits)
{
	ushort urb_value = 0;
	ushort urb_index = 0;
	ulong  urb_index_value;
	int ret;
	
	if (stop_bits == 2)urb_value |= FTDI_SIO_SET_DATA_STOP_BITS_2;
	else urb_value |= FTDI_SIO_SET_DATA_STOP_BITS_1;

	switch (data_bits)
	{
		case 5:
			urb_value |= 5;
			break;
		case 6:
			urb_value |= 6;
			break;
		case 7:
			urb_value |= 7;
			break;
		case 8:
		default:
			urb_value |= 8;
			break;
	}

	switch (parity)
	{
		case 'O':
			urb_value |= FTDI_SIO_SET_DATA_PARITY_ODD;
			break;
		case 'E':
			urb_value |= FTDI_SIO_SET_DATA_PARITY_EVEN;
			break;
		case 'N':
		default:
			urb_value |= FTDI_SIO_SET_DATA_PARITY_NONE;
			break;
	}

	usb_debug("set databits, parity, stopbits ...\r\n");
	ret = ftdi_usb_control_msg(&usb_dev, &usb_urb, FTDI_SIO_SET_DATA_REQUEST_TYPE, FTDI_SIO_SET_DATA_REQUEST, urb_value, 0, 0, NULL);
	if (ret)
	{
		usb_debug("set databits, parity, stopbits error\n\r");
		return ret;
	}

	urb_index_value = ftdi_232bm_baud_to_divisor(speed);
	urb_value = (ushort)urb_index_value;
	urb_index = (ushort)(urb_index_value >> 16);
	usb_debug("set baudrate ...\r\n");
	ret = ftdi_usb_control_msg(&usb_dev, &usb_urb, FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE, FTDI_SIO_SET_BAUDRATE_REQUEST, urb_value, urb_index,
							0, NULL);
	if (ret)
	{
		usb_debug("set baudrate error\r\n");
		return ret;
	}

	usb_debug("trun off flow control ...\r\n");
	ret = ftdi_usb_control_msg(&usb_dev, &usb_urb, FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE, FTDI_SIO_SET_FLOW_CTRL_REQUEST, 0, 0, 0, NULL);
	if (ret)
	{
		usb_debug("turn off flow control error\r\n");
		return ret;
	}	

	return 0;
}

uchar ftdi_port_open(ulong speed, uint data_bits, uchar parity, uint stop_bits)
{
	uchar resp = 0;
	int ret;

    if((usb_host_state<0)||(usb_host_state==3)) return 5;

    reset_udev_state();
    
    disable_irq(IRQ_OUSB0);
	//s_SetInterruptMode(IRQ_OUSB0, INTC_PRIO_2, INTC_IRQ);
	//s_SetIRQHandler(IRQ_OUSB0,ftdi_interrupt);//USB_INT

	UsbHostInit(1);
	ftdi_read_buf.tail =0; ftdi_read_buf.head = 0;
    ftdi_read_buf.buf=usb0_rx_pool;
	ftdi_write_buf.tail =0; ftdi_write_buf.head = 0;
    ftdi_write_buf.buf=usb0_tx_pool;

	ftdi_port_state = 0;
	//GPIO1_IEN_CLR_REG = BIT29; //disable interrupt

	ret=enum_dev_ftdi(&usb_dev, &usb_urb);
	if(ret)
	{
		//printk("\rdevice mount error:%d\n\r",ret);
        //printk("\rgintsts:%x,gintmsk:%x,haintmsk:%x,hcint0:%x,hcintmsk:%x\n",
//            rG_INT_STS,rG_INT_MSK,rHAINTMSK,rHCINT0,rHCINTMSK0);
		if(ret<100)resp=ret;
		else resp=0xef;
		goto open_failed;
	}

	ret = ftdi_usb_control_msg(&usb_dev, &usb_urb, FTDI_SIO_RESET_REQUEST_TYPE, FTDI_SIO_RESET_REQUEST, FTDI_SIO_RESET_SIO, 0, 0, NULL);
	if (ret)
	{
		//printk("reset error:%d\n\r",ret);
		resp = 0xee;
		goto open_failed;
	}

	usb_debug("set termios ...\n\r");
	ret = ftdi_set_termios(speed, data_bits, parity, stop_bits);
	if (ret)
	{
		//printk("set termios error:%d\r\n",ret);
		resp = 0xed;
		goto open_failed;
	}

open_failed:
	if(resp)
	{
		//GPIO1_IEN_CLR_REG = BIT29; //disable interrupt
		disable_irq(IRQ_OUSB0);
		usb_host_state=0;
		return resp;
	}

	ftdi_port_state = 1;
	usb_host_state=4;///1;

	return 0;
}

uchar ftdi_port_close(void)
{
	uchar resp = 0;
    int t0;

    if(usb_host_state==4)usb_host_state=1;

	if(ftdi_port_state == 0)
	{
		//GPIO1_IEN_CLR_REG = BIT29; //disable interrupt
        disable_irq(IRQ_OUSB0);
		return 0x03;
	}

	//usb_host_state=0;

	//s_TimerSet(TM_USB, 50);//500 ms
	t0=s_Get10MsTimerCount();
	int last_read = ftdi_write_buf.tail;
	while (ftdi_write_buf.tail != ftdi_write_buf.head)
	{
		//if(!s_TimerCheck(TM_USB))
		if((s_Get10MsTimerCount()-t0)>=50)
		{
			resp = 0xff;
			break;
		}
		if (last_read == ftdi_write_buf.tail)
			continue;
		last_read = ftdi_write_buf.tail;
		//s_TimerSet(TM_USB,50);//set timer again
		t0=s_Get10MsTimerCount();
	}

	ftdi_port_state = 0;
	//GPIO1_IEN_CLR_REG = BIT29; //disable interrupt
    disable_irq(IRQ_OUSB0);

	return resp;
}

uchar ftdi_port_send(uchar ch)
{
	int space,t0;

	if (ftdi_port_state == 0)
		return 0x03;

	//s_TimerSet(TM_USB, 50);//500 ms
	t0=s_Get10MsTimerCount();
	do
	{
		//if(!s_TimerCheck(TM_USB))
		if((s_Get10MsTimerCount()-t0)>=50)
			return 0x04;

		space = CIRC_SPACE(ftdi_write_buf.head, ftdi_write_buf.tail, FTDI_BUF_SIZE);
	} while (space == 0);
	
	ftdi_write_buf.buf[ftdi_write_buf.head] = ch;
	ftdi_write_buf.head = (ftdi_write_buf.head + 1) % FTDI_BUF_SIZE;
	
	return 0;
}

uchar ftdi_port_sends(uchar *buf, ushort count)
{
	if (ftdi_port_state == 0)
		return 0x03;

	int space = CIRC_SPACE(ftdi_write_buf.head, ftdi_write_buf.tail, FTDI_BUF_SIZE);	
	if (space < count)
		return 0x04;
	
	if (ftdi_write_buf.head + count > FTDI_BUF_SIZE) {
		int temp = FTDI_BUF_SIZE - ftdi_write_buf.head;
		memcpy(ftdi_write_buf.buf+ftdi_write_buf.head, buf, temp);
		memcpy(ftdi_write_buf.buf, buf+temp, count-temp);
	} else {
		memcpy(ftdi_write_buf.buf+ftdi_write_buf.head, buf, count);
	}

	ftdi_write_buf.head = (ftdi_write_buf.head + count) % FTDI_BUF_SIZE;

	return 0;
}

uchar ftdi_port_recv(uchar *ch, uint ms)
{
    int t0;

	if (ftdi_port_state == 0)
		return 0x03;

	if(ms)
	{
		ms=(ms+9)/10;
		//s_TimerSet(TM_USB, ms);
		t0=s_Get10MsTimerCount();
	}
	int count = CIRC_CNT(ftdi_read_buf.head, ftdi_read_buf.tail, FTDI_BUF_SIZE);
	if (count == 0 )
	{
		if (!ms)return 0xff;

		do
		{
			//if (!s_TimerCheck(TM_USB))return 0xff;
			if((s_Get10MsTimerCount()-t0)>=ms) return 0xff;

			count = CIRC_CNT(ftdi_read_buf.head, ftdi_read_buf.tail, FTDI_BUF_SIZE);
		} while (count == 0);
	}
		
	*ch = ftdi_read_buf.buf[ftdi_read_buf.tail];
	ftdi_read_buf.tail = (ftdi_read_buf.tail + 1) % FTDI_BUF_SIZE;

	return 0;
}

int ftdi_port_recvs(uchar *buf,ushort want_len,ushort timeout_ms)
{
	int i,j,t0;

	if(ftdi_port_state == 0)
		return -0x03;

	if(timeout_ms)
	{
		timeout_ms=(timeout_ms+9)/10;
		//s_TimerSet(TM_USB, timeout_ms);
		t0=s_Get10MsTimerCount();
	}

	i=0;
	while(i<want_len)
	{
		j=(ftdi_read_buf.tail+i)%FTDI_BUF_SIZE;
		if(j==ftdi_read_buf.head)
		{
			if(!timeout_ms)break;
			//if(s_TimerCheck(TM_USB))continue;
			if((s_Get10MsTimerCount()-t0)<=timeout_ms)continue;
			if(i)break;

			return -0xff;//time outs
		}

		buf[i]=ftdi_read_buf.buf[j];
		i++;
	}//while()
	ftdi_read_buf.tail=(ftdi_read_buf.tail+i)%FTDI_BUF_SIZE;

	return i;
}

uchar ftdi_port_reset(void)
{
	if (ftdi_port_state == 0)
		return 0x03;

	ftdi_port_state = 0; 

	//sleep 5 ms until usb finish send or receive packet 
	DelayMs(5);

	ftdi_read_buf.head = ftdi_read_buf.tail = 0;
	ftdi_port_state = 1;

	return 0;
}

uchar ftdi_port_pool_check(void)
{
	if (ftdi_port_state == 0)
		return 0x03;

	int count = CIRC_CNT(ftdi_write_buf.head, ftdi_write_buf.tail, FTDI_BUF_SIZE);
	if (count > 0)return 0x01;

	return 0;
}

#if 0
void ftdi_test(void)
{
	uchar tmpc,fn,port_no,tmps[10300],xstr[10300];
	int tmpd,rn,tn,i;
	unsigned long byte_count,speed,elapse,cur_loop;

	printk("\rftdi_test 20120405_01\n");

    //UsbCoreInit();
    //DelayMs(10);

    while(1)
    {
    	DelayMs(1000);
    	//tmpd=ftdi_port_open(115200,8,'N',1);
    	tmpd = PortOpen(P_USB,"115200,8,n,1");
    	printk("\r DEV OPEN:%d.\n",tmpd);

        if(tmpd==0)break;
    }

	for(i=0;i<sizeof(tmps);i++)tmps[i]=i&0xff;
	cur_loop=0;
	tn=1;
	while(1)
	{
		cur_loop++;
//		printk("LOOPS:%d...\n",cur_loop);

		tmps[0]=tn%0xff;
		//tmpd=ftdi_port_sends(tmps,tn);
		tmpd=PortSends(P_USB,tmps,tn);
		if(tmpd)
		{
			printk("\r FTDI TX FAILED:%d,tn:%d.\n",tmpd,tn);
			while(1);
		}

		rn=0;

		//tmpd=ftdi_port_recvs(xstr,tn,20000);//2000
		tmpd=PortRecvs(P_USB,xstr,tn,20000);//2000
		if(tmpd)rn=tmpd;
		if(tmpd<=0)
		{
			printk("\r FTDI RX FAILED:%d,rn:%d,tn:%d,%u.\n",tmpd,rn,tn,GetTimerCount());
			while(1);
		}
		if(rn!=tn)
		{
			printk("\r FTDI DLEN MISMATCH,tn:%d,rn:%d,%u.\n",tn,rn,GetTimerCount());
            printk("\r");
            for(i=0;i<rn;i++)
                printk("%X ",xstr[i]);
            
			while(1);
		}
		if(memcmp(tmps,xstr,tn))
		{
			while(1)
			{
				printk("\r DATA MISMATCH:\n");
				for(i=0;i<tn;i++)printk("%02X ",xstr[i]);
				printk("\n");
				DelayMs(3000);
			}
		}

		tn=(tn+1)%10241;
		if(!tn)tn=1;
	}//while(1)
}
#endif
