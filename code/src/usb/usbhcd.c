#include  <stdio.h>
#include  <string.h>
#include  <stdarg.h>

#include <Base.h>

#include  "..\comm\comm.h"

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
#define rG_RXSTSR           (*(volatile unsigned int*)0x000A001C)
#define rG_HW_CFG1          (*(volatile unsigned int*)0x000A0044)/*ep direction*/
#define rG_HW_CFG2          (*(volatile unsigned int*)0x000A0048)
#define rG_HW_CFG3          (*(volatile unsigned int*)0x000A004C)
#define rG_HW_CFG4          (*(volatile unsigned int*)0x000A0050)
#define rG_RXFIFO_SIZE      (*(volatile unsigned int*)0x000A0024)
#define rG_NP_TXFIFO_SIZE   (*(volatile unsigned int*)0x000A0028)
#define rG_HPXFSIZ          (*(volatile unsigned int*)0x000A0100)

#define rHCFG               (*(volatile unsigned int*)0x000A0400)
#define rHFNUM              (*(volatile unsigned int*)0x000A0408)
#define rHPTXSTS            (*(volatile unsigned int*)0x000A0410)
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


typedef struct
{
	unsigned char   bLength;
	unsigned char   bDescriptorType;

	unsigned short  bcdUSB;
	unsigned char   bDeviceClass;
	unsigned char   bDeviceSubClass;
	unsigned char   bDeviceProtocol;
	unsigned char   bMaxPacketSize0;
	unsigned short  idVendor;
	unsigned short  idProduct;
	unsigned short  bcdDevice;
	unsigned char   iManufacturer;
	unsigned char   iProduct;
	unsigned char   iSerialNumber;
	unsigned char   bNumConfigurations;
}__attribute__((__packed__)) USB_DEVICE_DESCRIPTOR;


typedef struct
{
	unsigned char  bLength;
	unsigned char  bDescriptorType;

	unsigned short wTotalLength;
	unsigned char  bNumInterfaces;
	unsigned char  bConfigurationValue;
	unsigned char  iConfiguration;
	unsigned char  bmAttributes;
	unsigned char  bMaxPower;
}__attribute__((__packed__)) USB_CONFIG_DESCRIPTOR;

typedef struct
{
	unsigned char  bLength;
	unsigned char  bDescriptorType;

	unsigned char  bInterfaceNumber;
	unsigned char  bAlternateSetting;
	unsigned char  bNumEndpoints;
	unsigned char  bInterfaceClass;
	unsigned char  bInterfaceSubClass;
	unsigned char  bInterfaceProtocol;
	unsigned char  iInterface;
}__attribute__((__packed__)) USB_INTERFACE_DESCRIPTOR;

typedef struct
{
	unsigned char  bLength;
	unsigned char  bDescriptorType;

	unsigned char  bEndpointAddress;
	unsigned char  bmAttributes;
	unsigned short wMaxPacketSize;
	unsigned char  bInterval;
}__attribute__((__packed__)) USB_ENDPOINT_DESCRIPTOR;

typedef struct
{
    volatile unsigned char epNum;
    volatile unsigned char epType;
    volatile unsigned char epDir;
    volatile unsigned char pid;
    volatile unsigned short maxPkt;
    volatile unsigned char *transfer_buffer;//transfer_buffer >= ((transfer_buffer_length/maxpkt)+1)*mkpkt
    volatile unsigned int transfer_buffer_length;
    volatile unsigned int actual_length;
    volatile unsigned int cur_length;
    volatile int status;
    volatile void (*complete)(void *para);
    void *para;
    volatile unsigned char complete_flag;
}__attribute__((__packed__)) USB_TD;

///usb speed
#define HS_SPEED    0
#define FS_SPEED    1
#define LS_SPEED    2

//endpoint type
#define EP_CTRL     0
#define EP_ISO      1
#define EP_BULK     2
#define EP_INT      3

//endpoint dir
#define EP_IN       0
#define EP_OUT      1

//pid
#define PID_DATA0   0
#define PID_DATA1   1
#define PID_SETUP   2

typedef struct
{
#define MAX_EP_NUMS 10    
    unsigned char addr;
    unsigned char speed;
    USB_DEVICE_DESCRIPTOR dev_desc;
    USB_CONFIG_DESCRIPTOR cfg_desc;
    USB_INTERFACE_DESCRIPTOR int_desc;
    USB_ENDPOINT_DESCRIPTOR ep_in[MAX_EP_NUMS];
    USB_ENDPOINT_DESCRIPTOR ep_out[MAX_EP_NUMS];
    unsigned char ep_in_data[MAX_EP_NUMS];
    unsigned char ep_out_data[MAX_EP_NUMS];
    USB_TD *td;
}__attribute__((__packed__)) USB_DEVICE;

static USB_TD gTd;
static USB_DEVICE usb_device;

static volatile unsigned char kbPipe=0;
static unsigned char kbBuffer[64];
static unsigned char scannerKernelBuffer[2048],scannerUserBuffer[2048],scannerUserBusy=0;
static volatile unsigned int scannerKernelBufferLenght=0, scannerUserBufferLength=0;

#define T_NODEVICE     0
#define T_SCANNER    4
static volatile unsigned char usb_device_id=T_NODEVICE;

extern volatile unsigned int usbEnableVbusTick;

void UsbHcdInterrupt(int id);
static int StartTransfer(USB_TD *td);
static void UsbHubHalt(void);
unsigned char UsbHcdStop(void);

static int UsbHubInit(void)
{
    volatile uint i,reg,hprt0,hprt0_modify,t;

    //printk("%s\n",__func__);
    if((rG_INT_STS&1) ==0)
    {
        //printk("no host mode\n");
        return USB_ERR_NOTFREE;//device mode
    }

    disable_irq(IRQ_OUSB0);
    rG_INT_MSK=0;
    rHAINTMSK=0;
    rG_AHB_CFG &=~1;//global interrupt mask
    
    UsbHubHalt();
    memset(&usb_device,0,sizeof(usb_device));
    memset(&gTd,0,sizeof(gTd));

    //printk("usb host power on tick:%d\n",s_Get10MsTimerCount()-usbEnableVbusTick);

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
    //printk("hprt0:%08X\n",rHPRT);
    if(hprt0&1)// a device is attached to the port
    {
        i=(hprt0>>17)&0x3;
        if(i==0)usb_device.speed = HS_SPEED;
        else if(i==1)usb_device.speed = FS_SPEED;
        else if(i==2)usb_device.speed = LS_SPEED;

        switch(usb_device.speed)
        {
        case HS_SPEED:
        case FS_SPEED:
            #if 0
            rG_USB_CFG = 0x20001408;//480MHz PHY
            rG_AHB_CFG = 0x00000026;
            rHCFG=5;//480MHz
            #endif
            #if 0
            rG_USB_CFG = 0x20002420;//480MHz PHY
            rG_AHB_CFG = 0x00000026;
            rHCFG=5;//480MHz
            #endif

            rG_USB_CFG = 0x2000A440;//48MHz PHY
            rG_AHB_CFG = 0x00000026;
            rHCFG=0x05;//48MHz
            
            usb_device.dev_desc.bMaxPacketSize0 = 64;
            break;

        case LS_SPEED:
            rG_USB_CFG = 0x20001440;//48MHz PHY
            rG_AHB_CFG = 0x00000022;
            rHCFG=0x06;//6MHz
            usb_device.dev_desc.bMaxPacketSize0 = 8;
            break;
        }
    }
    else return USB_ERR_DEV_ABSENT;

    
    hprt0_modify &=~((1<<2)|(1<<3));//set 0 to bit2:prtena bit3:prtenchng 
    if(hprt0&(1<<1))hprt0_modify |= 1<<1;//prtconndet
    if(hprt0&(1<<5))hprt0_modify |= 1<<5;//prtovrcurrchng
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
        //printk("prtenchng err rHprt:%08X\n",hprt0);
        return USB_ERR_DEV_ABSENT;
    }
        
    hprt0_modify|=1<<3;//prtenchng
    if((hprt0&(1<<2))==0)//prtena
    {
        //printk("prtena err rHprt:%08X\n",hprt0);
        return USB_ERR_DEV_ABSENT;
    }

    rHPRT=hprt0_modify;

    DelayMs(1);
    //flush channel 0
    reg=rHCCHAR0;
    reg&=~(1<<31);
    reg|=1<<30;
    rHCCHAR0=reg;

    //halt channel 0
    reg=rHCCHAR0;
    reg|=(1<<31)|(1<<30);
    reg&=~(1<<15);
    rHCCHAR0=reg;
    i=0;
	while(rHCCHAR0&(1<<31)) 
	{
		if (++i> 500) break;
	} 
    
    rHCCHAR0&=~(0x7f<<22);//set device addr to 0
    rHCINTMSK0=0;
    rHCINT0=~0x00;

    return 0;
}

void UsbHubReset(void)
{
    volatile uint hprt0;

//    printk("%s hprt:%08X   ",__func__,rHPRT);

    hprt0 = rHPRT;
    hprt0&=~(1<<7);//prtsusp
    hprt0|=1<<8;//prtrst
    rHPRT=hprt0;
    DelayMs(60);
    hprt0 &= ~(1<<8);//prtrst
    rHPRT=hprt0;
    DelayMs(10);
    usb_device.addr=0;
//    printk("%08X\n",rHPRT);
}

int HaltChannel(void)
{
    unsigned int t0 = GetUsClock();
    
    if(rHCCHAR0&(1<<31))//channel enable
    {
        rHCCHAR0|=(1<<30);//bit30:channel disable
        while(1)
        {
            if(rHCINT0&(1<<1)) break;//channel halted
            if((GetTimerCount()-t0)>30)return 1;
        }
        rHCINT0=~0;
	}
    return 0;
}

void UsbHcdInterrupt(int id)
{
    unsigned int int_sts,haint,hcint,tmpd,pid;
    USB_TD *td;

    int_sts = rG_INT_STS&rG_INT_MSK;
    haint = rHAINT&rHAINTMSK;
    hcint = rHCINT0;
    td=usb_device.td;

    if(td==0)
    {
        ///printk("%s error td==0\n",__func__);
        UsbHubHalt();
        return;
    }

    if(int_sts!=(1<<25) || haint!=1)//no channel interrupt
    {
        if(rHCCHAR0&(1<<31))//channel enable
        {
            rHCCHAR0|=(1<<30);//bit30:channel disable
    	}
        //printk("%s error interrupt\n",__func__);
        usb_device.td->status = 1;
        goto complete;
    }

    if((hcint&0x23)!=0x23)//channel halt,transfer completed,ack
    {
        /*
        if(usb_device.td->epNum==0)
        {
            printk("%s error transfer,hcint:%08X\n",__func__,hcint);
            printk("hctsiz:%08X,hcchar:%08X,hprt:%08X\n",rHCTSIZ0,rHCCHAR0,rHPRT);
        }
        */
        usb_device.td->status = 2;
        goto complete;
    }

    rHCINT0=hcint;
    rG_INT_STS=rG_INT_STS;
    //printk("interrupt,xfersize:%d,pktcnt:%d\n",rHCTSIZ0&(0x7ffff),(rHCTSIZ0>>19)&0x3FF);
    //printk("hctsiz:%08X,hcchar:%08X\n",rHCTSIZ0,rHCCHAR0);

    if(td->pid==PID_SETUP)
    {
        usb_device.td->status = 0;
        goto complete;
    }
    
    pid = (rHCTSIZ0>>29)&0x03;
    if(td->epDir==EP_OUT)
    {
        td->actual_length += td->cur_length;
        if(pid==0)//reg-data0
        {
           usb_device.ep_out_data[td->epNum]=0;//data0
        }
        else
        {
            usb_device.ep_out_data[td->epNum]=1;//data1  
        }
        

        if(td->cur_length!=td->maxPkt ||
            td->actual_length >= td->transfer_buffer_length)
        {
            td->status = 0;
            goto complete;
        }

        if(usb_device.ep_out_data[td->epNum])
            td->pid = PID_DATA1;
        else
            td->pid = PID_DATA0;
        
        if(StartTransfer(td))//failed!
        {
            usb_device.td->status = 4;
            goto complete;
        }

        return;
    }
    else//in
    {
        tmpd = td->cur_length-rHCTSIZ0&(0x7ffff);    

        if((tmpd+td->actual_length)>td->transfer_buffer_length)
        {
            usb_device.td->status = 5;
            goto complete;
        }
        
        memcpy((unsigned char *)(td->transfer_buffer+td->actual_length),
            (unsigned char *)DMA_USB_BASE,tmpd);
        td->actual_length +=tmpd;

        if(pid==0)//reg-data0
        {
            usb_device.ep_in_data[td->epNum]=0;//data0
        }
        else
        {
            usb_device.ep_in_data[td->epNum]=1;//data1  
        }

        if(tmpd!=td->maxPkt ||
            td->actual_length >= td->transfer_buffer_length)
        {
            td->status = 0;
            goto complete;
        }

        if(usb_device.ep_in_data[td->epNum])
            td->pid = PID_DATA1;
        else
            td->pid = PID_DATA0;
        
        if(StartTransfer(td))//failed!
        {
            usb_device.td->status = 8;
            goto complete;
        }

        return;
    }
    
complete:
    rHCINT0=hcint;
    rG_INT_STS=rG_INT_STS;
    rHAINTMSK=0;
    rG_INT_MSK=0;
    rG_AHB_CFG &=~1;//global interrupt mask
    usb_device.td = 0;

    td->complete_flag=1;
    if(td->complete)
        td->complete(td->para);
}


static int StartTransfer(USB_TD *td)
{
    volatile unsigned int hcint;
    volatile unsigned int hcchar;
    volatile unsigned int hctsiz;

    rG_AHB_CFG &=~1;//global interrupt mask

    //clear interrupt status
    rHCINT0=~0;//channel0 interrupt status
    
    if (rHCCHAR0&(1<<31))return 1;

    hcchar=rHCCHAR0;
    hcchar &=~(0x1fffffff);//clear bit19~bit0

    //if(td->epDir==EP_IN)//in
    //{
     //   td->cur_length = td->maxPkt;
   // }
    //else 
    if((td->transfer_buffer_length-td->actual_length)>td->maxPkt)//out
    {
        td->cur_length = td->maxPkt;
    }
    else//out
    {
        td->cur_length = td->transfer_buffer_length-td->actual_length;
    }
    
	hctsiz = td->cur_length;//xfersize
	hctsiz |= 1<<19;//bit28~bit19:packet count

    switch(td->pid)
    {
    case PID_DATA0:
        hctsiz |=0<<29;
        break;

    case PID_DATA1:
        hctsiz |=2<<29;
        break;

    case PID_SETUP:
        hctsiz |=3<<29;
        break;

    default:return 2;
    }

    hcchar |=usb_device.addr<<22;//set address

    if(td->epDir==EP_IN)
        hcchar|=1<<15;//bit15:set to in
    else
        hcchar|=0<<15;//bit15:set to out

    switch(td->epType)
    {
    case EP_CTRL:
        hcchar|=0<<18;//ep bulk
        break;

    case EP_BULK:
        hcchar|=0x2<<18;//ep bulk
        break;

    case EP_INT:
        hcchar|=0x3<<18;//ep interrupt
        if((rHFNUM&1)==0)
            hcchar|=1<<29;//odd frame
        else
            hcchar&=~(1<<29);//even frame
        break;

    default:return 3;
    }

    if(usb_device.speed==LS_SPEED)
        hcchar|=(1<<17);//low speed

    hcchar |=(td->epNum<<11)|td->maxPkt|(1<<31);//set ep number,mps,hc enable

    if(td->epDir==EP_OUT)
    {
        memcpy((unsigned char *)DMA_USB_BASE,(unsigned char *)(td->transfer_buffer+td->actual_length),
        td->cur_length);
        //printk("dir out len:%d,%02X-%02X\n",td->cur_length,
        //    td->transfer_buffer[0],td->transfer_buffer[1]);
    }

    rG_INT_STS=rG_INT_STS;

    //printk("\nep%d hctsiz:%08X,hcchar:%08X\n",td->epNum,hctsiz,hcchar);

    //enable interrupt-channel0
    rHCINTMSK0=1<<1;//chhltd unmask
    rHAINTMSK=1;//host channel0 unmask
    rG_INT_MSK=1<<25;//host channels interrupt unmask
    rG_AHB_CFG |=1;//global interrupt unmask
    
    rHCDMA0=(unsigned int)DMA_USB_BASE;
    rHCTSIZ0=hctsiz;
    rHCCHAR0=hcchar;

    return 0;
}

static void UsbHubHalt(void)
{
    USB_TD *td;
    
    disable_irq(IRQ_OUSB0);

    td=usb_device.td;
    
    rG_INT_STS=rG_INT_STS;
    rHCINT0=rHCINT0;
    rHAINTMSK=0;
    rG_INT_MSK=0;
    rG_AHB_CFG &=~1;//global interrupt mask

    usb_device.td = 0;
    if(td)
    {
        td->complete_flag=1;
        if(td->complete)
            td->complete(td->para);
    }
}

static int UsbTransferDesc(USB_TD *td,unsigned char epNum,
    unsigned char epDir,unsigned char epType,unsigned char pid,
    unsigned char maxPkt,unsigned char *buffer,unsigned char size,
    void * callback,void *para)
{
    if(usb_device.td)return 5;//busy

    if(HaltChannel())return 6;

    usb_device.td = td;
    memset(td,0,sizeof(USB_TD));
    td->epNum=epNum;
    td->epDir=epDir;
    td->epType=epType;
    td->pid = pid;
    td->maxPkt = maxPkt;
    td->transfer_buffer = buffer;
    td->transfer_buffer_length = size;
    td->complete = callback;
    td->para = para;
    td->status = -1;

    return StartTransfer(td);
}

int UsbCtrlMsg(unsigned char *setup,unsigned char *buffer,
    unsigned int *act_size,unsigned int timeoutMs)
{
    unsigned char maxPkt;
    volatile unsigned int t0=GetTimerCount(),t1;
    volatile unsigned int dataLen,tmpd,bufferSize,pid,tsize;

    if(usb_device.td)return 1;//busy
    if((rHPRT&0x0F)!=0x05) return 2;

    dataLen = (setup[7]<<8)+setup[6];
    maxPkt=usb_device.dev_desc.bMaxPacketSize0;

RESTART:
    *act_size=0;

    //setup
    //printk("setup...\n");
    while(1)
    {
        if((rHPRT&0x0F)!=0x05)return 2;
        if((GetTimerCount()-t0)>timeoutMs)return 3;
        //DelayUs(2000);
        tmpd = UsbTransferDesc(&gTd, 0, EP_OUT, EP_CTRL, PID_SETUP, maxPkt, setup, 8, 0, 0);
        if(tmpd)
        {
            //printk("Send setup error:%d\n",tmpd);
            continue;
        }
        
        while(gTd.complete_flag==0)
        {
            if((rHPRT&0x0F)!=0x05)return 2;
            if((GetTimerCount()-t0)>timeoutMs)return 3;
        }

        if(gTd.status!=0)
        {
            //printk("setup error status:%d\n",gTd.status);
            continue;
        }
        break;
    }
    
    if(dataLen==0)//no data,In(data1)
    {
        //printk("data in zero..\n");
        t1=GetTimerCount();
        while(1)
        {
            if((rHPRT&0x0F)!=0x05)return 2;
            if((GetTimerCount()-t0)>timeoutMs)return 4;
            //DelayUs(2000);
            tmpd = UsbTransferDesc(&gTd, 0, EP_IN, EP_CTRL, PID_DATA1, maxPkt, 0, 0, 0, 0);
            if(tmpd)
            {
                //printk("send data in zero error:%d\n",tmpd);
                continue;
            }
            
            while(gTd.complete_flag==0)
            {
                if((rHPRT&0x0F)!=0x05)return 2;
                if((GetTimerCount()-t0)>timeoutMs)return 4;
            }
            if(gTd.status!=0)
            {
                //printk("data in zero error:%d\n",gTd.status);
                if((GetTimerCount()-t1)>200)goto RESTART;
                continue;
            }
            DelayUs(2000);
            return 0;
        }
    }
    else if(setup[0]&0x80)//data in
    {
        while(1)//data in 0/1
        {
            if((rHPRT&0x0F)!=0x05)return 2;
            if((GetTimerCount()-t0)>timeoutMs)return 5;
            //DelayUs(2000);
            usb_device.ep_in_data[0]=1;
            tmpd = UsbTransferDesc(&gTd, 0, EP_IN, EP_CTRL, PID_DATA1, maxPkt, 
                            buffer, dataLen, 0, 0);
            if(tmpd)
            {
                //printk("send data in error:%d\n",tmpd);
                continue;
            }
            while(gTd.complete_flag==0)
            {
                if((rHPRT&0x0F)!=0x05)return 2;
                if((GetTimerCount()-t0)>timeoutMs)return 5;
            }
            if(gTd.status!=0)
            {
                //printk("data in error:%d\n",gTd.status);
                continue;
            }
            *act_size=gTd.actual_length;
            break;
        }

        t1=GetTimerCount();
        while(1)//data out zero
        {
            //out data1
            ////printk("data out zero..\n");
            if((rHPRT&0x0F)!=0x05)return 2;
            if((GetTimerCount()-t0)>timeoutMs)return 6;
            //DelayUs(2000);
            tmpd = UsbTransferDesc(&gTd, 0, EP_OUT, EP_CTRL, PID_DATA1, maxPkt, 0, 0, 0, 0);
            if(tmpd)
            {
                //printk("send data out zero error:%d\n",tmpd);
                continue;
            }
            while(gTd.complete_flag==0)
            {
                if((rHPRT&0x0F)!=0x05)return 2;
                if((GetTimerCount()-t0)>timeoutMs)return 6;
            }
            if(gTd.status!=0)
            {
                //printk("data out zero error:%d\n",gTd.status);
                if((GetTimerCount()-t1)>200)goto RESTART;                
                continue;
            }
            DelayUs(2000);
            return 0;
        }
    }
    else//data out
    {
        while(1)//data out
        {
            if((rHPRT&0x0F)!=0x05)return 2;
            if((GetTimerCount()-t0)>timeoutMs)return 7;
            //DelayUs(2000);
            usb_device.ep_out_data[0]=1;
            tmpd = UsbTransferDesc(&gTd, 0, EP_OUT, EP_CTRL, PID_DATA1, maxPkt,
                            buffer, dataLen, 0, 0);
            if(tmpd)
            {
                //printk("send data out error:%d\n",tmpd);
                continue;
            }
            while(gTd.complete_flag==0)
            {
                if((rHPRT&0x0F)!=0x05)return 2;
                if((GetTimerCount()-t0)>timeoutMs)return 7;
            }
            if(gTd.status!=0)
            {
                //printk("data out error:%d\n",gTd.status);
                continue;
            }
            *act_size=gTd.actual_length;
            break;
        }

        t1=GetTimerCount();
        while(1)
        {
            //in data1
            ////printk("data in zero..\n");
            if((rHPRT&0x0F)!=0x05)return 2;
            if((GetTimerCount()-t0)>timeoutMs)return 8;
            //DelayUs(2000);
            tmpd = UsbTransferDesc(&gTd, 0, EP_IN, EP_CTRL, PID_DATA1, maxPkt, 0, 0, 0, 0);
            if(tmpd)
            {
                //printk("send data in zero error:%d\n",tmpd);
                continue;
            }
            while(gTd.complete_flag==0)
            {
                if((rHPRT&0x0F)!=0x05)return 2;
                if((GetTimerCount()-t0)>timeoutMs)return 8;
            }
            if(gTd.status!=0)
            {
                //printk("data in zero error:%d\n",gTd.status);
                if((GetTimerCount()-t1)>200)goto RESTART;
                continue;
            }
            DelayUs(2000);
            return 0;
        }
    }
}

int UsbBulkOrIntMsg(USB_TD *td,unsigned char pipe,unsigned char *buffer,unsigned int size,
    void *callBack,void *para)
{
    unsigned char epNum,epType,epDir;
    unsigned char maxPkt;
    volatile unsigned int tmpd,pid;
    
    if(usb_device.td)return 7;//busy
    if((rHPRT&0x0F)!=0x05)return 8;

    epNum = pipe&0x7f;
    if(pipe&0x80)//in
    {
        epDir=EP_IN;
        switch(usb_device.ep_in[epNum].bmAttributes)
        {
        case 0:
            epType=EP_CTRL;
            break;
        case 1:
            epType=EP_ISO;
            break;
        case 2:
            epType=EP_BULK;
            break;
        case 3:
            epType=EP_INT;
            break;
        }
        maxPkt = usb_device.ep_in[epNum].wMaxPacketSize;
        if(usb_device.ep_in_data[epNum])
            pid = PID_DATA1;
        else 
            pid = PID_DATA0;
        
    }
    else//out
    {
        epDir = EP_OUT;
        switch(usb_device.ep_out[epNum].bmAttributes)
        {
        case 0:
            epType=EP_CTRL;
            break;
        case 1:
            epType=EP_ISO;
            break;
        case 2:
            epType=EP_BULK;
            break;
        case 3:
            epType=EP_INT;
            break;
        }
        maxPkt = usb_device.ep_out[epNum].wMaxPacketSize;
        if(usb_device.ep_out_data[epNum])
            pid = PID_DATA1;
        else 
            pid = PID_DATA0;
        
    }
    tmpd = UsbTransferDesc(td, epNum, epDir, epType, pid, maxPkt, 
                buffer, size, callBack, para);
    if(tmpd)
    {
        //printk("send bulk or int error:%d\n",tmpd);
        return tmpd;
    }

    return 0;
}

int UsbHubEnum(unsigned char device_id)
{
    unsigned char buffer[1024],tmpc;
    unsigned int act_size;
    volatile unsigned int i,j,k;
    int ret;

    s_SetInterruptMode(IRQ_OUSB0, INTC_PRIO_2, INTC_IRQ);
	s_SetIRQHandler(IRQ_OUSB0,UsbHcdInterrupt);//USB_INT
    enable_irq(IRQ_OUSB0);

    //printk("%s \n",__func__);
    //get device desc
    //printk("get dev desc\n");
    ret = UsbCtrlMsg("\x80\x06\x00\x01\x00\x00\x40\x00", buffer, &act_size, 3000);
    if(ret)
    {
        //printk("get dev desc err:%d\n",ret);
        return 30+ret;
    }
    //printk("dev desc len:%d: ",act_size);
//    for(i=0;i<act_size;i++)
        //printk("%02X ",buffer[i]);
    //printk("\n");

    if(act_size>sizeof(usb_device.dev_desc))
        memcpy(&usb_device.dev_desc,buffer,sizeof(usb_device.dev_desc));
    else
        memcpy(&usb_device.dev_desc,buffer,act_size);


    //set address
    //printk("set addr\n");
    ret = UsbCtrlMsg("\x00\x05\x01\x00\x00\x00\x00\x00", 0, 0, 3000);
    if(ret)
    {
        //printk("Set Address Error:%d\n",ret);
        return 40+ret;
    }
    usb_device.addr=1;

    //get config desc
    //printk("get cfg desc\n");
    ret = UsbCtrlMsg("\x80\x06\x00\x02\x00\x00\xff\x00", buffer, &act_size, 3000);
    if(ret)
    {
        //printk("Get Config desc Error:%d\n",ret);
        return 50+ret;
    }
/*
    printk("cfg desc len:%d: ",act_size);
    for(i=0;i<act_size;i++)
        printk("%02X ",buffer[i]);
    printk("\n");
*/
    if(act_size>sizeof(usb_device.cfg_desc))
        memcpy(&usb_device.cfg_desc,buffer,sizeof(usb_device.cfg_desc));
    else
        memcpy(&usb_device.cfg_desc,buffer,act_size);

    i=usb_device.cfg_desc.bLength;
    while(buffer[i+1]!=4)//interface
    {
        if(i>=act_size)return 200;
        i+=buffer[i];
    }
    memcpy(&usb_device.int_desc,buffer+i,sizeof(usb_device.int_desc));
    i+=sizeof(usb_device.int_desc);

    if(usb_device.int_desc.bNumEndpoints)
    {
        while(buffer[i+1]!=5)//endpoint
        {
            if(i>=act_size)return 201;
            i+=buffer[i];
        }

        for(j=0;j<usb_device.int_desc.bNumEndpoints;j++)
        {
            k=buffer[i+2]&0x7f;//endpoint num
            if(buffer[i+2]&0x80)//in
            {
                memcpy(&usb_device.ep_in[k],buffer+i,sizeof(USB_ENDPOINT_DESCRIPTOR));
            }
            else//out
            {
                memcpy(&usb_device.ep_out[k],buffer+i,sizeof(USB_ENDPOINT_DESCRIPTOR));
            }
            i+=sizeof(USB_ENDPOINT_DESCRIPTOR);
            if(i>=act_size)break;
        }

    }
/*
    printk("dev.len:%02X,dev.bcdUSB:%04X,dev.NumCfg:%02X\n",
        usb_device.dev_desc.bLength,
        usb_device.dev_desc.bcdUSB,
        usb_device.dev_desc.bNumConfigurations);
    printk("cfg.len:%02X,cfg.power:%02X\n",
        usb_device.cfg_desc.bLength,
        usb_device.cfg_desc.bMaxPower);
    printk("int.len:%02X,int.iInt:%02X\n",
        usb_device.int_desc.bLength,
        usb_device.int_desc.iInterface);
    
    printk("numEndPoint:%d\n",usb_device.int_desc.bNumEndpoints);

    for(i=0;i<10;i++)
    {
        if(usb_device.ep_in[i].bLength)
        {
            printk("ep in:%d,len:%02X,addr:%02X,max:%04X\n",i,
                usb_device.ep_in[i].bLength,
                usb_device.ep_in[i].bEndpointAddress,
                usb_device.ep_in[i].wMaxPacketSize);
        }

        if(usb_device.ep_out[i].bLength)
        {
            printk("ep out:%d,len:%02X,addr:%02X,max:%04X\n",i,
                usb_device.ep_out[i].bLength,
                usb_device.ep_out[i].bEndpointAddress,
                usb_device.ep_out[i].wMaxPacketSize);
        }
    }
*/
    //select config
    //printk("select config\n");
    ret = UsbCtrlMsg("\x00\x09\x01\x00\x00\x00\x00\x00", 0, 0, 3000);
    if(ret)
    {
        //printk("select config error:%d\n",ret);
        return 60+ret;
    }

    if(usb_device.int_desc.bInterfaceClass ==0x03)//hid
    {
        //set idle
        //printk("set idle\n");
        ret = UsbCtrlMsg("\x21\x0a\x00\x00\x00\x00\x00\x00", 0, 0, 3000);
        //if(ret)
        //{
            //printk("set idle error:%d\n",ret);
            //return 70+ret;
        //}

        //get hid report
        //printk("get hid report\n");
        ret = UsbCtrlMsg("\x81\x06\x00\x22\x00\x00\xff\x00", buffer, &act_size, 3000);
        //if(ret)
        //{
            //printk("get hid report error:%d\n",ret);
            //return 37;
        //}
        //printk("hid report len:%d: ",act_size);
        //for(i=0;i<act_size;i++)
            //printk("%02X ",buffer[i]);
        //printk("\n");

        if(usb_device.int_desc.bInterfaceProtocol==1)//kb
        {
            for(i=0;i<sizeof(usb_device.ep_in)/sizeof(USB_ENDPOINT_DESCRIPTOR);i++)
            {
                if(usb_device.ep_in[i].bmAttributes==3)//endpoint interrupt transfer
                {
                    if(usb_device.ep_in[i].bEndpointAddress&0x80)//in
                        tmpc = usb_device.ep_in[i].bEndpointAddress;
                }
            }
            if(tmpc==0)
            {
                //printk("kb pipe error\n");
                return 202;
            }

            //printk("kb pipe:%02X\n",tmpc);
            if(device_id == T_SCANNER)
            {
                usb_device_id = T_SCANNER;
                scannerUserBufferLength = 0;
                scannerKernelBufferLenght = 0;
                scannerUserBusy = 0;
                kbPipe=tmpc;
                return 0;
            }
        }
        else if(usb_device.int_desc.bInterfaceProtocol==2)//mouse
        {
        }
    }

    return 205;
}

#define SCANNER_BREAK       0XFF
#define SCANNER_SHIFT       0XFE
#define SCANNER_00          0XFD
#define SCANNER_000         0XFC
#define SCANNER_X0          0XFB
#define SCANNER_X1          0XFA

const unsigned char scannerMap[256][2]=
{
{SCANNER_BREAK,SCANNER_BREAK},//N/A
{SCANNER_BREAK,SCANNER_BREAK},//N/A
{SCANNER_BREAK,SCANNER_BREAK},//N/A
{SCANNER_BREAK,SCANNER_BREAK},//N/A
{'a','A'},
{'b','B'},
{'c','C'},
{'d','D'},
{'e','E'},
{'f','F'},
{'g','G'},
{'h','H'},
{'i','I'},
{'j','J'},
{'k','K'},
{'l','L'},
{'m','M'},
{'n','N'},
{'o','O'},
{'p','P'},
{'q','Q'},
{'r','R'},
{'s','S'},
{'t','T'},
{'u','U'},
{'v','V'},
{'w','W'},
{'x','X'},
{'y','Y'},
{'z','Z'},
{'1','!'},
{'2','@'},
{'3','#'},
{'4','$'},
{'5','%'},
{'6','^'},
{'7','&'},
{'8','*'},
{'9','('},
{'0',')'},
{SCANNER_BREAK,SCANNER_BREAK},//enter
{SCANNER_BREAK,SCANNER_BREAK},//esc
{SCANNER_BREAK,SCANNER_BREAK},//del
{SCANNER_BREAK,SCANNER_BREAK},//tab
{' ',' '},//space
{'-','_'},
{'=','+'},
{'[','{'},
{']','}'},
{0x5C,'|'},
{SCANNER_BREAK,'~'},
{';',':'},
{0X27,0X22},
{'`','~'},
{',','<'},
{'.','>'},
{'/','?'},
{SCANNER_BREAK,SCANNER_BREAK},//CAP LOCK
{SCANNER_BREAK,SCANNER_BREAK},//F1
{SCANNER_BREAK,SCANNER_BREAK},//F2
{SCANNER_BREAK,SCANNER_BREAK},//F3
{SCANNER_BREAK,SCANNER_BREAK},//F4
{SCANNER_BREAK,SCANNER_BREAK},//F5
{SCANNER_BREAK,SCANNER_BREAK},//F6
{SCANNER_BREAK,SCANNER_BREAK},//F7
{SCANNER_BREAK,SCANNER_BREAK},//F8
{SCANNER_BREAK,SCANNER_BREAK},//F9
{SCANNER_BREAK,SCANNER_BREAK},//F10
{SCANNER_BREAK,SCANNER_BREAK},//F11
{SCANNER_BREAK,SCANNER_BREAK},//F12
{SCANNER_BREAK,SCANNER_BREAK},//PrintScreen
{SCANNER_BREAK,SCANNER_BREAK},//Scroll lock
{SCANNER_BREAK,SCANNER_BREAK},//pause
{SCANNER_BREAK,SCANNER_BREAK},//insert
{SCANNER_BREAK,SCANNER_BREAK},//home
{SCANNER_BREAK,SCANNER_BREAK},//pageup
{SCANNER_BREAK,SCANNER_BREAK},//del
{SCANNER_BREAK,SCANNER_BREAK},//end
{SCANNER_BREAK,SCANNER_BREAK},//pagedown
{SCANNER_BREAK,SCANNER_BREAK},//right
{SCANNER_BREAK,SCANNER_BREAK},//left
{SCANNER_BREAK,SCANNER_BREAK},//down
{SCANNER_BREAK,SCANNER_BREAK},//up
{SCANNER_BREAK,SCANNER_BREAK},//num lock
{'/','/'},//keypad /
{'*','*'},
{'-','-'},
{'+','+'},
{SCANNER_BREAK,SCANNER_BREAK},//keypad enter
{'1',SCANNER_BREAK},
{'2',SCANNER_BREAK},
{'3',SCANNER_BREAK},
{'4',SCANNER_BREAK},
{'5',SCANNER_BREAK},
{'6',SCANNER_BREAK},
{'7',SCANNER_BREAK},
{'8',SCANNER_BREAK},
{'9',SCANNER_BREAK},
{'0',SCANNER_BREAK},
{'.',SCANNER_BREAK},
{SCANNER_BREAK,'|'},
{SCANNER_BREAK,SCANNER_BREAK},//keyboard application
{SCANNER_BREAK,SCANNER_BREAK},//keyboard power
{'=','='},//keypad =
{SCANNER_BREAK,SCANNER_BREAK},//f13
{SCANNER_BREAK,SCANNER_BREAK},//f14
{SCANNER_BREAK,SCANNER_BREAK},//f15
{SCANNER_BREAK,SCANNER_BREAK},//f16
{SCANNER_BREAK,SCANNER_BREAK},//f17
{SCANNER_BREAK,SCANNER_BREAK},//f18
{SCANNER_BREAK,SCANNER_BREAK},//f19
{SCANNER_BREAK,SCANNER_BREAK},//f20
{SCANNER_BREAK,SCANNER_BREAK},//f21
{SCANNER_BREAK,SCANNER_BREAK},//f22
{SCANNER_BREAK,SCANNER_BREAK},//f23
{SCANNER_BREAK,SCANNER_BREAK},//f24
{SCANNER_BREAK,SCANNER_BREAK},//keyboard execute
{SCANNER_BREAK,SCANNER_BREAK},//keyboard help
{SCANNER_BREAK,SCANNER_BREAK},//keyboard menu
{SCANNER_BREAK,SCANNER_BREAK},//keyboard select
{SCANNER_BREAK,SCANNER_BREAK},//keyboard stop
{SCANNER_BREAK,SCANNER_BREAK},//keyboard again
{SCANNER_BREAK,SCANNER_BREAK},//keyboard undo
{SCANNER_BREAK,SCANNER_BREAK},//keyboard cut
{SCANNER_BREAK,SCANNER_BREAK},//keyboard copy
{SCANNER_BREAK,SCANNER_BREAK},//keyboard paste
{SCANNER_BREAK,SCANNER_BREAK},//keyboard find
{SCANNER_BREAK,SCANNER_BREAK},//keyboard mute
{SCANNER_BREAK,SCANNER_BREAK},//keyboard volume up
{SCANNER_BREAK,SCANNER_BREAK},//keyboard volume down
{SCANNER_BREAK,SCANNER_BREAK},//keyboard locking caps lock
{SCANNER_BREAK,SCANNER_BREAK},//keyboard locking num lock
{SCANNER_BREAK,SCANNER_BREAK},//keyboard locking scroll lock
{SCANNER_BREAK,SCANNER_BREAK},//keypad comma
{SCANNER_BREAK,SCANNER_BREAK},//keypad equal sign
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international1
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international2
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international3
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international4
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international5
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international6
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international7
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international8
{SCANNER_BREAK,SCANNER_BREAK},//keyboard international9
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG1
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG2
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG3
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG4
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG5
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG6
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG7
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG8
{SCANNER_BREAK,SCANNER_BREAK},//keyboard LANG9
{SCANNER_BREAK,SCANNER_BREAK},//keyboard alternate erase
{SCANNER_BREAK,SCANNER_BREAK},//keyboard sysreq
{SCANNER_BREAK,SCANNER_BREAK},//keyboard cancel
{SCANNER_BREAK,SCANNER_BREAK},//keyboard clear
{SCANNER_BREAK,SCANNER_BREAK},//keyboard prior
{SCANNER_BREAK,SCANNER_BREAK},//keyboard return
{SCANNER_BREAK,SCANNER_BREAK},//keyboard separator
{SCANNER_BREAK,SCANNER_BREAK},//keyboard out
{SCANNER_BREAK,SCANNER_BREAK},//keyboard oper
{SCANNER_BREAK,SCANNER_BREAK},//keyboard clear/again
{SCANNER_BREAK,SCANNER_BREAK},//keyboard crsel/props
{SCANNER_BREAK,SCANNER_BREAK},//keyboard exsel
{SCANNER_BREAK,SCANNER_BREAK},//reserved 165
{SCANNER_BREAK,SCANNER_BREAK},//reserved 166
{SCANNER_BREAK,SCANNER_BREAK},//reserved 167
{SCANNER_BREAK,SCANNER_BREAK},//reserved 168
{SCANNER_BREAK,SCANNER_BREAK},//reserved 169
{SCANNER_BREAK,SCANNER_BREAK},//reserved 170
{SCANNER_BREAK,SCANNER_BREAK},//reserved 171
{SCANNER_BREAK,SCANNER_BREAK},//reserved 172
{SCANNER_BREAK,SCANNER_BREAK},//reserved 173
{SCANNER_BREAK,SCANNER_BREAK},//reserved 174
{SCANNER_BREAK,SCANNER_BREAK},//reserved 175
{SCANNER_00,SCANNER_00},//keypad 00
{SCANNER_000,SCANNER_000},//keypad 000
{SCANNER_BREAK,SCANNER_BREAK},//thousands separator
{SCANNER_BREAK,SCANNER_BREAK},//decimal separator
{SCANNER_BREAK,SCANNER_BREAK},//currency unit
{SCANNER_BREAK,SCANNER_BREAK},//currency sub-uint
{'(','('},//keypad (
{')',')'},//keypad )
{'{','{'},//keypad {
{'}','}'},//keypad }
{SCANNER_BREAK,SCANNER_BREAK},//keypad tab
{SCANNER_BREAK,SCANNER_BREAK},//keypad backspace
{'A','A'},//keypad A
{'B','B'},//keypad B
{'C','C'},//keypad C
{'D','D'},//keypad D
{'E','E'},//keypad E
{'F','F'},//keypad F
{SCANNER_BREAK,SCANNER_BREAK},//keypad xor
{'^','^'},//keypad ^
{'%','%'},//keypad %
{'<','<'},//keypad <
{'>','>'},//keypad >
{'&','&'},//keypad &
{SCANNER_X0,SCANNER_X0},//keypad &&
{'|','|'},//keypad |
{SCANNER_X1,SCANNER_X1},//keypad ||
{':',':'},//keypad :
{'#','#'},//keypad #
{' ',' '},//keypad space
{'@','@'},//keypad @
{'!','!'},//keypad !
{SCANNER_BREAK,SCANNER_BREAK},//keypad memory store
{SCANNER_BREAK,SCANNER_BREAK},//keypad memory recall
{SCANNER_BREAK,SCANNER_BREAK},//keypad memory clear
{SCANNER_BREAK,SCANNER_BREAK},//keypad memory add
{SCANNER_BREAK,SCANNER_BREAK},//keypad memory subtract
{SCANNER_BREAK,SCANNER_BREAK},//keypad memory multiply
{SCANNER_BREAK,SCANNER_BREAK},//keypad memory divide
{'+','-'},//keypad +/-
{SCANNER_BREAK,SCANNER_BREAK},//keypad clear
{SCANNER_BREAK,SCANNER_BREAK},//keypad clear entry
{SCANNER_BREAK,SCANNER_BREAK},//keypad binary
{SCANNER_BREAK,SCANNER_BREAK},//keypad octal
{SCANNER_BREAK,SCANNER_BREAK},//keypad decimal
{SCANNER_BREAK,SCANNER_BREAK},//keypad hexadecimal
{SCANNER_BREAK,SCANNER_BREAK},//reserved 222
{SCANNER_BREAK,SCANNER_BREAK},//reserved 223
{SCANNER_BREAK,SCANNER_BREAK},//keyboard leftcontrol
{SCANNER_SHIFT,SCANNER_SHIFT},//keyboard leftshift
{SCANNER_BREAK,SCANNER_BREAK},//keyboard left alt
{SCANNER_BREAK,SCANNER_BREAK},//keyboard left gui
{SCANNER_BREAK,SCANNER_BREAK},//keyboard right control
{SCANNER_SHIFT,SCANNER_SHIFT},//keyboard right shift
{SCANNER_BREAK,SCANNER_BREAK},//keyboard right alt
{SCANNER_BREAK,SCANNER_BREAK},//keyboard right gui
{SCANNER_BREAK,SCANNER_BREAK},//reserved 232
{SCANNER_BREAK,SCANNER_BREAK},//reserved 233
{SCANNER_BREAK,SCANNER_BREAK},//reserved 234
{SCANNER_BREAK,SCANNER_BREAK},//reserved 235
{SCANNER_BREAK,SCANNER_BREAK},//reserved 236
{SCANNER_BREAK,SCANNER_BREAK},//reserved 237
{SCANNER_BREAK,SCANNER_BREAK},//reserved 238
{SCANNER_BREAK,SCANNER_BREAK},//reserved 239
{SCANNER_BREAK,SCANNER_BREAK},//reserved 240
{SCANNER_BREAK,SCANNER_BREAK},//reserved 241
{SCANNER_BREAK,SCANNER_BREAK},//reserved 242
{SCANNER_BREAK,SCANNER_BREAK},//reserved 243
{SCANNER_BREAK,SCANNER_BREAK},//reserved 244
{SCANNER_BREAK,SCANNER_BREAK},//reserved 245
{SCANNER_BREAK,SCANNER_BREAK},//reserved 246
{SCANNER_BREAK,SCANNER_BREAK},//reserved 247
{SCANNER_BREAK,SCANNER_BREAK},//reserved 248
{SCANNER_BREAK,SCANNER_BREAK},//reserved 249
{SCANNER_BREAK,SCANNER_BREAK},//reserved 250
{SCANNER_BREAK,SCANNER_BREAK},//reserved 251
{SCANNER_BREAK,SCANNER_BREAK},//reserved 252
{SCANNER_BREAK,SCANNER_BREAK},//reserved 253
{SCANNER_BREAK,SCANNER_BREAK},//reserved 254
{SCANNER_BREAK,SCANNER_BREAK},//reserved 255
};

int UsbHcdTask(void)
{
    static volatile unsigned char step=0;
    static volatile unsigned int t0;
    volatile unsigned int i,isShift=0;
    int ret;
    unsigned char tmpc,key;
    
    if(kbPipe==0)
    {
        step=0;
        return 0;
    }

    if(UsbHcdState())
    {
        UsbHubHalt();
        kbPipe = 0;
        return 0;
    }

Restart:
    switch(step)
    {
    case 0:
        if(usb_device.td==0)
        {
            ret = UsbBulkOrIntMsg(&gTd,kbPipe, kbBuffer, 8, 0, 0);
            if(ret==0)step=1;
        }

        break;

    case 1:
        if(gTd.complete_flag==0)break;
        //printk("gTd.status:%d\n",gTd.status);
        if(gTd.status==0)
        {
            t0 = s_Get10MsTimerCount();
            //for(i=0;i<gTd.actual_length;i++)
                //printk("%02X ",kbBuffer[i]);
            //printk("\n");
            
            if(kbBuffer[0]&0x22)isShift=1;
            else isShift=0;
            
            for(tmpc=0,i=2;i<8;i++)
            {
                if(kbBuffer[i])
                {
                    tmpc = kbBuffer[i];
                    key=scannerMap[tmpc][isShift];
                    if(key<SCANNER_X1)
                    {
                        scannerKernelBuffer[scannerKernelBufferLenght]=key;
                        scannerKernelBufferLenght++;
                    }
                    else
                    {
                    switch(key)
                    {
                    case SCANNER_BREAK:
                        if(scannerKernelBufferLenght)
                        {
                            if(scannerUserBusy)
                            {
                                step=2;
                                return;
                            }

                            memcpy(scannerUserBuffer,scannerKernelBuffer,scannerKernelBufferLenght);
                            scannerUserBufferLength=scannerKernelBufferLenght;
                            scannerKernelBufferLenght=0;
                            step=0;
                            goto Restart;
                        }
                        break;
                        
                    case SCANNER_SHIFT:
                        isShift=1;
                        break;

                    case SCANNER_00:
                        scannerKernelBuffer[scannerKernelBufferLenght]='0';
                        scannerKernelBufferLenght++;
                        scannerKernelBuffer[scannerKernelBufferLenght]='0';
                        scannerKernelBufferLenght++;
                        break;

                    case SCANNER_000:
                        scannerKernelBuffer[scannerKernelBufferLenght]='0';
                        scannerKernelBufferLenght++;
                        scannerKernelBuffer[scannerKernelBufferLenght]='0';
                        scannerKernelBufferLenght++;
                        scannerKernelBuffer[scannerKernelBufferLenght]='0';
                        scannerKernelBufferLenght++;
                        break;

                    case SCANNER_X0:
                        scannerKernelBuffer[scannerKernelBufferLenght]='&';
                        scannerKernelBufferLenght++;
                        scannerKernelBuffer[scannerKernelBufferLenght]='&';
                        scannerKernelBufferLenght++;
                        break;

                    case SCANNER_X1:
                        scannerKernelBuffer[scannerKernelBufferLenght]='|';
                        scannerKernelBufferLenght++;
                        scannerKernelBuffer[scannerKernelBufferLenght]='|';
                        scannerKernelBufferLenght++;
                        break;
                    }
                    }
                }
            }
        }
        else
        {
            if((s_Get10MsTimerCount()-t0)>10 && scannerKernelBufferLenght)
            {
                if(scannerUserBusy)
                {
                    step=2;
                    return;
                }

                memcpy(scannerUserBuffer,scannerKernelBuffer,scannerKernelBufferLenght);
                scannerUserBufferLength=scannerKernelBufferLenght;
                scannerKernelBufferLenght=0;
            }
        }
        step=0;
        goto Restart;

    case 2:
        if(scannerUserBusy)break;
        memcpy(scannerUserBuffer,scannerKernelBuffer,scannerKernelBufferLenght);
        scannerUserBufferLength=scannerKernelBufferLenght;
        scannerKernelBufferLenght=0;
        step=0;
        goto Restart;
    }
}

int UsbHcdState(void)
{
    if(usb_device_id==T_NODEVICE)return USB_ERR_NOTOPEN;
    if((rG_INT_STS&1) ==0)return USB_ERR_NOTOPEN;//device
    if((rHPRT&0x0F)!=0x05)return USB_ERR_DEV_ABSENT;
    
    return 0;
}

unsigned char UsbHcdOpen(int device_id)
{
    int ret,ret1;
    volatile uint t0=GetTimerCount();

    //printk("%s 002 tick:%d\n",__func__,s_Get10MsTimerCount());

    if(usb_device_id==device_id && UsbHcdState()==0)
    {
        return 0;
    }
    UsbHcdStop();

//    printk("%s hprt:%08X\n",__func__,rHPRT);

    if((rHPRT&0x0F)==0x05)UsbHubReset();

    ret = UsbHubInit();
    if(ret)goto RETRY;

    DelayMs(200);
    ret = UsbHubEnum(device_id);
    if(ret && ret<200)
    {
        UsbHubHalt();
        while(1)
        {
RETRY:            
            if((GetTimerCount()-t0)>7000)break;
            UsbHubReset();
            while((rG_INT_STS&(1<<24))==0)
            {
                if((GetTimerCount()-t0)>7000)break;
            }
            ret1=UsbHubInit();
            if(ret1)continue;
            ret1 = UsbHubEnum(device_id);
            if(ret1==0)return 0;
            else if(ret1>=200)return ret1;
            UsbHubHalt();
        }
    }

    return ret;
}

unsigned char UsbHcdStop(void)
{
    switch(usb_device_id)
    {
    case T_SCANNER:
        //printk("%s\n",__func__);
        UsbHubHalt();
        kbPipe=0;
        scannerUserBufferLength=0;
        scannerKernelBufferLenght=0;
        scannerUserBusy = 0;
        break;
    }
    usb_device_id=T_NODEVICE;

    return 0;
}

unsigned char UsbHcdSends(char *buf,unsigned short size)
{
    int ret;

    ret = UsbHcdState();
    if(ret)return ret;
    
    return 0;
}

int UsbHcdRecvs(unsigned char *buf,unsigned short want_len,
    unsigned short timeout_ms)
{
    int ret=-USB_ERR_NOTOPEN;
    uint x,t0;

    switch(usb_device_id)
    {
    case T_SCANNER:
        t0=GetTimerCount();
        while(1)
        {
            ret = UsbHcdState();
            if(ret)return -ret;
            
            if(scannerUserBufferLength==0)
            {
                if(timeout_ms==0)return 0;
                if((GetTimerCount()-t0)<timeout_ms)continue;
                else return -USB_ERR_TIMEOUT;
            }
            else
            {
                irq_save(x);
                scannerUserBusy=1;
                irq_restore(x);
                if(scannerUserBufferLength>want_len)
                    ret = -USB_ERR_BUF;
                else
                {
                    memcpy(buf,scannerUserBuffer,scannerUserBufferLength);
                    ret = scannerUserBufferLength;
                    scannerUserBufferLength=0;
                }
                irq_save(x);
                scannerUserBusy=0;
                irq_restore(x);

                return ret;
            }
        }
        
        break;
    }
    return ret;
}

unsigned char UsbHcdTxPoolCheck(void)
{
    int ret;

    ret = UsbHcdState();
    if(ret)return (unsigned char)ret;

    return 0;
}

unsigned char UsbHcdReset(void)
{
    int ret;
    uint flag;

    ret = UsbHcdState();
    if(ret)return (unsigned char)ret;

    switch(usb_device_id)
    {
    case T_SCANNER:
        irq_save(flag);
        scannerUserBufferLength = 0;
        irq_restore(flag);
        break;
    }

    return 0;
}

#if 0
void UsbTest(void)
{
    uchar key,buffer[300];
    unsigned int act_size;
    int tmpd;
    
    while(1)
    {
        ScrCls();
        ScrPrint(0,0,0,"1-CoreInit   2-HcdEnum");
        kbflush();
        key = getkey();

        ScrCls();
        switch(key)
        {
        case KEY1:
            tmpd = UsbCoreInit();
            ScrPrint(0,0,0,"UsbCoreInit ret:%d",tmpd);
            getkey();
            break;

        case KEY2:
            tmpd = UsbHubInit();
            ScrPrint(0,0,0,"UsbHubInit ret:%d",tmpd);
            ScrPrint(0,1,0,"hrpt:0x%08X",rHPRT);
            ScrPrint(0,2,0,"speed:%d",usb_device.speed);

            if(tmpd==0)
            {
                tmpd = UsbHubEnum(T_SCANNER);
                ScrPrint(0,3,0,"UsbHubEnum:%d",tmpd);
            }
            getkey();
            break;

        case KEY3:
            tmpd = UsbHcdOpen(4);
            ScrPrint(0,0,0,"UsbHcdOpen:%d",tmpd);
            getkey();
            break;
        }
    }
}
#endif
