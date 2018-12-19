#include  <stdio.h>
#include  <string.h>
#include  <stdarg.h>

#include <Base.h>

#include  "..\comm\comm.h"

//#undef x_debug
//#define x_debug

#define USB_ERR_NOTOPEN       3
#define USB_ERR_BUF           4
#define USB_ERR_NOTFREE       5
#define USB_ERR_CHIPVER       15
#define USB_ERR_DEV_ABSENT    16
#define USB_ERR_PORT_POWERED  17
#define USB_ERR_DEV_REMOVED   18
#define USB_ERR_DEV_QUERY     19
#define USB_ERR_TIMEOUT       0xff

#define rHC_REVISION                (*(volatile unsigned int*)0x000B1000)
#define rHC_CONTROL                 (*(volatile unsigned int*)0x000B1004)
#define rHC_COMMAND_STATUS          (*(volatile unsigned int*)0x000B1008)
#define rHC_INTERRUPT_STATUS        (*(volatile unsigned int*)0x000B100C)
#define rHC_INTERRUPT_ENABLE        (*(volatile unsigned int*)0x000B1010)
#define rHC_INTERRUPT_DISABLE       (*(volatile unsigned int*)0x000B1014)
#define rHC_HCCA                    (*(volatile unsigned int*)0x000B1018)
#define rHC_PERIOD_CURRENT_ED       (*(volatile unsigned int*)0x000B101C)
#define rHC_CONTROL_HEAD_ED         (*(volatile unsigned int*)0x000B1020)
#define rHC_CONTROL_CURRENT_ED      (*(volatile unsigned int*)0x000B1024)
#define rHC_BULK_HEAD_ED            (*(volatile unsigned int*)0x000B1028)
#define rHC_BULK_CURRENT_ED         (*(volatile unsigned int*)0x000B102C)
#define rHC_DONE_HEAD               (*(volatile unsigned int*)0x000B1030)
#define rHC_FM_INTERVAL             (*(volatile unsigned int*)0x000B1034)
#define rHC_FM_REMAINING            (*(volatile unsigned int*)0x000B1038)
#define rHC_FM_NUMBER               (*(volatile unsigned int*)0x000B103C)
#define rHC_PERIODIC_START          (*(volatile unsigned int*)0x000B1040)
#define rHC_LS_THRESHOLD            (*(volatile unsigned int*)0x000B1044)
#define rHC_RH_DESCRIPTORA          (*(volatile unsigned int*)0x000B1048)
#define rHC_RH_DESCRIPTORB          (*(volatile unsigned int*)0x000B104C)
#define rHC_RH_STATUS               (*(volatile unsigned int*)0x000B1050)
#define rHC_RH_PORT_STATUS1         (*(volatile unsigned int*)0x000B1054)

typedef struct _OHCI_ED_
{
    volatile unsigned int Info;
    volatile unsigned int TailP;
    volatile unsigned int HeadP;
    volatile unsigned int NextED;
}__attribute__((__packed__)) OHCI_ED;

typedef struct _OHCI_TD_
{
    volatile unsigned int Info;
    volatile unsigned int CBP;
    volatile unsigned int NextTD;
    volatile unsigned int BE;
}__attribute__((__packed__)) OHCI_TD;


typedef struct _OHCI_HCCA_
{
    volatile unsigned int HccaInterruptTable[32];
    volatile unsigned short HccaFrameNumber;
    volatile unsigned short HccaPad1;
    volatile unsigned int HccaDoneHead;
    volatile unsigned char reserved[116];
}__attribute__((__packed__)) OHCI_HCCA;

#define OHCI_HCCA_ADDR      0x40206000

#define OHCI_ED_ADDR        0x40207000
#define OHCI_TD_ADDR        0x40207100

#define OHCI_DATA_BUF       0x40208000
#define OHCI_DATA_LEN       4096

static OHCI_HCCA * ghcca = (OHCI_HCCA *)OHCI_HCCA_ADDR;
static OHCI_TD * gtd = (OHCI_TD *)OHCI_TD_ADDR;
static OHCI_ED * ged = (OHCI_ED *)OHCI_ED_ADDR;

static unsigned char DevAddr = 0;

/* TD info field */
#define TD_CC	    0xf0000000
#define TD_CC_GET(td_p) ((td_p >>28) & 0x0f)
#define TD_CC_SET(td_p, cc) (td_p) = ((td_p) & 0x0fffffff) | (((cc) & 0x0f) << 28)
#define TD_EC	    0x0C000000
#define TD_T	    0x03000000
#define TD_T_DATA0  0x02000000
#define TD_T_DATA1  0x03000000
#define TD_T_TOGGLE 0x00000000
#define TD_R	    0x00040000
#define TD_DI	    0x00E00000
#define TD_DI_SET(X) (((X) & 0x07)<< 21)
#define TD_DP	    0x00180000
#define TD_DP_SETUP 0x00000000
#define TD_DP_IN    0x00100000
#define TD_DP_OUT   0x00080000

#define TD_ISO	    0x00010000
#define TD_DEL	    0x00020000

/* CC Codes */
#define TD_CC_NOERROR	    0x00
#define TD_CC_CRC	        0x01
#define TD_CC_BITSTUFFING   0x02
#define TD_CC_DATATOGGLEM   0x03
#define TD_CC_STALL	        0x04
#define TD_DEVNOTRESP	    0x05
#define TD_PIDCHECKFAIL	    0x06
#define TD_UNEXPECTEDPID    0x07
#define TD_DATAOVERRUN	    0x08
#define TD_DATAUNDERRUN	    0x09
#define TD_BUFFEROVERRUN    0x0C
#define TD_BUFFERUNDERRUN   0x0D
#define TD_NOTACCESSED	    0x0F

//--constant parameters
#define RX_POOL_SIZE 65537
#define TX_POOL_SIZE 10241/*1025*/
#define BIO_MAX_RETRIES 5
#define EP_BUF_SIZE 64
#define EP_NUMS     16


enum DEV_TYPE{T_NULL=0,T_PAXDEV,T_UDISK,T_UDISK_FAST,T_DEV_LIMIT};
enum TXN_TYPE{T_IDLE=0,T_SETUP,T_BULK_IO};

#define P_SETUP 0
#define P_OUT   1
#define P_IN    2

enum DEV_STATE{S_DETACHED=0,S_ATTACHED,S_POWERED,S_DEFAULT,S_ADDRESS,S_CONFIGURED,S_SUSPENDED};
enum HCI_STATE{S_HCI_NOT_INIT=0,S_HCI_INITIALIZED,S_HCI_STARTED,S_HCI_DETECTED};

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
int usb_host_ohci_state=0;
static int port_open_state[T_DEV_LIMIT];
static unsigned char tx_pool[TX_POOL_SIZE],rx_pool[RX_POOL_SIZE];
static volatile int tx_read,tx_write,rx_read,rx_write;
static ST_BIO_STATE dev_state;

//--descriptor structs
static ST_DESCRIPTOR_DEVICE desc_device;
static ST_DESCRIPTOR_CONFIG desc_config;
static ST_DESCRIPTOR_INTERFACE desc_face;
static ST_DESCRIPTOR_ENDPOINT desc_ep[EP_NUMS];

static unsigned char ohci_host_init(void)
{
    unsigned int i,tmpd;
    unsigned int fminterval;

    usb_host_ohci_state=0;
    
    //1 Disable USB1 and initialized variable
    disable_irq(IRQ_OUSBH1);
    
    ///* SMM owns the HC, request ownership */
    if(rHC_CONTROL & (1<<8))
    {
        rHC_COMMAND_STATUS = 1<<3;
        i=0;
        while(rHC_CONTROL & (1<<8))
        {
            DelayMs(1);
            i++;
            if(i>500)
            {
//                x_debug("\rUSB HC TakeOver failed!\n");
                return 1;
            }
        }
    }

    //rHC_INTERRUPT_DISABLE = 1<31;

    //x_debug("\rUSB HC reset_hc rHC_CONTROL:0x%X\n",rHC_CONTROL);
    rHC_CONTROL = 0;

    rHC_COMMAND_STATUS = 1;//HostControlReset
    i=0;
    while(rHC_COMMAND_STATUS & 1)
    {
        DelayUs(1);
        i++;
        if(i>50)
        {
            //x_debug("\rHostControlReset Failed!\n");
            return 2;
        }
    }

    //HC start

    rHC_CONTROL_HEAD_ED = 0;
    rHC_BULK_HEAD_ED = 0;
    memset((unsigned char *)OHCI_HCCA_ADDR,0,sizeof(OHCI_HCCA));
    rHC_HCCA = OHCI_HCCA_ADDR;
    ghcca= (OHCI_HCCA *)OHCI_HCCA_ADDR;
    OHCI_HCCA * ghcca= (OHCI_HCCA *)OHCI_HCCA_ADDR;
    OHCI_TD * gtd = (OHCI_TD *)OHCI_TD_ADDR;
    OHCI_ED * ged = (OHCI_ED *)OHCI_ED_ADDR;

    fminterval = 0x2edf;
    rHC_PERIODIC_START = (fminterval * 9) / 10;
    fminterval |= ((((fminterval - 210) * 6) / 7) << 16);
    rHC_FM_INTERVAL = fminterval;
    rHC_LS_THRESHOLD = 0x628;

    //start controller operation
    rHC_CONTROL = (2<<6)|3;//usb operational,control/bulk:4:1

    //rHC_INTERRUPT_DISABLE = ~(1<<1);
    rHC_INTERRUPT_STATUS = rHC_INTERRUPT_STATUS;
    //rHC_INTERRUPT_ENABLE = (1<<31);

    //x_debug("\rINT_DIS:0X%08X\n",rHC_INTERRUPT_DISABLE);
    //x_debug("\rINT_EN:0X%08X\n",rHC_INTERRUPT_ENABLE);

    DelayMs((rHC_RH_DESCRIPTORA >> 23) & 0x1fe);
    DelayMs(5);

    rHC_RH_STATUS = 0x10000;//HC is power on
    
    usb_host_ohci_state = S_HCI_INITIALIZED;
    memset(port_open_state,0x00,sizeof(port_open_state));
    DelayMs(1);

    return 0;
}

static int ctrl_send_wait(unsigned char *setup,
    unsigned char *data,int *len,unsigned int timeout_ms)
{
    int td_index=0;
    unsigned int tmpd=0,data0=0,isIn,want_len,i,intsts;
    int t0;

    DelayMs(2);

    t0 = s_Get10MsTimerCount();
    
    data0 = 0;
    td_index = 0;

    isIn= setup[0]&0x80;
    want_len =(setup[7]<<8)+setup[6];

    //fill setup packet in td
    gtd[td_index].Info = TD_CC|TD_DP_SETUP|TD_T_DATA0;
    gtd[td_index].CBP = OHCI_DATA_BUF;
    gtd[td_index].BE = gtd[td_index].CBP + 8 - 1;
    memcpy((unsigned char *)gtd[td_index].CBP,setup,8);
    gtd[td_index].NextTD = (unsigned int)&gtd[td_index+1];
    data0 ^= 1;
    td_index++;

   // x_debug("\rwant_len:%d,isIn:%d,data0:%d\n",want_len,isIn,data0);
    
    if(want_len && isIn)
    {
        for(i=0;i<want_len;)
        {
            gtd[td_index].Info = data0?TD_CC | TD_R | TD_DP_IN | TD_T_DATA1:
                TD_CC | TD_R | TD_DP_IN | TD_T_DATA0;
                
            gtd[td_index].CBP = OHCI_DATA_BUF+8+i;//td[td_index-1].BE+1;
            gtd[td_index].BE = gtd[td_index].CBP + EP_BUF_SIZE - 1;
            gtd[td_index].NextTD = (unsigned int)&gtd[td_index+1];
            i+= EP_BUF_SIZE;
            data0 ^= 1;
            td_index++;
        }
    }
    else if(want_len && (isIn==0))
    {
        for(i=0;i<want_len;)
        {
            gtd[td_index].Info = data0?TD_CC | TD_R | TD_DP_OUT | TD_T_DATA1:
                TD_CC | TD_R | TD_DP_OUT | TD_T_DATA0;
                
            gtd[td_index].CBP = OHCI_DATA_BUF+8+i;//td[td_index-1].BE+1;
            gtd[td_index].BE = gtd[td_index].CBP + EP_BUF_SIZE - 1;
            gtd[td_index].NextTD = (unsigned int)&gtd[td_index+1];
            memcpy((unsigned char *)gtd[td_index].CBP,
                (unsigned char *)(data+i),EP_BUF_SIZE);
            i+=EP_BUF_SIZE;
            data0 ^= 1;
            td_index++;
        }
    }
    //status 
    gtd[td_index].Info = isIn?TD_CC | TD_DP_OUT | TD_T_DATA1:
        TD_CC | TD_DP_IN | TD_T_DATA1;
    gtd[td_index].CBP = 0;
    gtd[td_index].BE = 0;
    gtd[td_index].NextTD = (unsigned int)&gtd[td_index+1]; 
    td_index++;

    //fill setup packet in ed
    ged[0].Info = EP_BUF_SIZE<<16 | DevAddr;
    ged[0].HeadP = (unsigned int)&gtd[0];//head td
    ged[0].TailP = (unsigned int)&gtd[td_index];//just one td
    ged[0].NextED = 0;//just one ed
/*
    x_debug("\rstart-ed:0x%X,0x%X,0x%X,0x%X\n",
        ged[0].Info,ged[0].TailP,ged[0].HeadP,ged[0].NextED);

    for(i=0;i<td_index;i++)
        x_debug("\rtd[%d]:0x%X,0x%X,0x%X,0x%X\n",i,
            gtd[i].Info,gtd[i].CBP,gtd[i].NextTD,gtd[i].BE);

    x_debug("\rhcca->donehead:0x%X\n",ghcca->HccaDoneHead);
*/

    rHC_INTERRUPT_STATUS = rHC_INTERRUPT_STATUS;
    rHC_CONTROL_HEAD_ED = (unsigned int)&ged[0];
    rHC_COMMAND_STATUS |=1<<1;//ControlListFilled
    rHC_CONTROL |= 1<<4;//ControlListEnable

    while(1)
    {
        if(rHC_INTERRUPT_STATUS & 0x10)//UnrecoverableError
            return 1;

        if(rHC_INTERRUPT_STATUS & (1<<6))//roothub status change
        {
			port_open_state[cur_pipe.device_id]=0;
			return 2;
		}

        if((ged[0].TailP>>2) == (ged[0].HeadP>>2))
        {
            //x_debug("\rctrl-0x%08X,rhc_ct:0x%08X,rhccmd:0x%08X\n",
            //    rHC_INTERRUPT_STATUS,rHC_CONTROL,rHC_COMMAND_STATUS);
            //x_debug("\rctrl-start-ed:0x%X,0x%X,0x%X,0x%X\n",
            //    ged[0].Info,ged[0].TailP,ged[0].HeadP,ged[0].NextED);
            break;
        }

        if((s_Get10MsTimerCount()-t0)*10 >timeout_ms)
        {
            //x_debug("\r%s timeout,sof:%d\n",__func__,sof);
            //x_debug("\rctrl-0x%08X,rhc_ct:0x%08X,rhccmd:0x%08X\n",
            //        rHC_INTERRUPT_STATUS,rHC_CONTROL,rHC_COMMAND_STATUS);
            //x_debug("\rctrl-start-ed:0x%X,0x%X,0x%X,0x%X\n",
              //      ged[0].Info,ged[0].TailP,ged[0].HeadP,ged[0].NextED);

            return 3;
        }
    }

    if(want_len && isIn)
    {
        for(i=0;i<want_len;)
        {
            if(((gtd[i/EP_BUF_SIZE+1].Info >>26) & 0x03)==2) 
            {
                //x_debug("\rTd error,info:0x%X\n",gtd[i/EP_BUF_SIZE+1].Info);
                return 4;
            }
            if(gtd[i/EP_BUF_SIZE+1].BE ==0)break;
            if(gtd[i/EP_BUF_SIZE+1].CBP ==0)tmpd = EP_BUF_SIZE;
            else
                tmpd = EP_BUF_SIZE - (gtd[i/EP_BUF_SIZE+1].BE+1-gtd[i/EP_BUF_SIZE+1].CBP);
            memcpy((unsigned char *)(data+i),
                (unsigned char *)(OHCI_DATA_BUF+8+i),tmpd);
            i+=tmpd;
            if(tmpd!=EP_BUF_SIZE) break;
        }

        *len = i;
    }
/*    
    x_debug("\rint_sts:0x%X\n",rHC_INTERRUPT_STATUS);

    x_debug("\red:0x%X,0x%X,0x%X,0x%X\n",
        ged[0].Info,ged[0].TailP,ged[0].HeadP,ged[0].NextED);
    for(i=0;i<td_index;i++)
        x_debug("\rtd[%d]:0x%X,0x%X,0x%X,0x%X\n",i,
            gtd[i].Info,gtd[i].CBP,gtd[i].NextTD,gtd[i].BE);
    x_debug("\rhcca->donehead:0x%X\n",ghcca->HccaDoneHead);
*/    

    //ged[0].HeadP = ged[0].TailP = (unsigned int)&gtd[0];
    //rHC_CONTROL &= ~(1<<4);//ControlListEnable
    //rHC_CONTROL_HEAD_ED = 0;
    return 0;
}

static int bulk_send_wait(unsigned char isIn,
    unsigned char *data,int *len,unsigned int timeout_ms)
{
    unsigned int tmpd=0,want_len,i,intsts;
    int t1;

    want_len = *len;

    if(want_len==0)return 1;
    if(want_len>OHCI_DATA_LEN) return 2;

/*
    x_debug("\rwant_len:%d,%s\n",want_len,isIn?"In":"Out");
    if(isIn)x_debug("\rno:%d,pkt_size:%d,data%d\n",
        cur_pipe.rx_ep_no,cur_pipe.rx_packet_size,rx_data_no[cur_pipe.rx_ep_no]);
    else x_debug("\rno:%d,pkt_size:%d,data%d\n",
        cur_pipe.tx_ep_no,cur_pipe.tx_packet_size,tx_data_no[cur_pipe.rx_ep_no]);    
*/
    if(isIn)
    {
        gtd[0].Info = rx_data_no[cur_pipe.rx_ep_no]?
                TD_CC | TD_R | TD_DP_IN | TD_T_DATA1/*TD_T_TOGGLE*/:
                TD_CC | TD_R | TD_DP_IN | TD_T_DATA0;
                
        gtd[0].CBP = OHCI_DATA_BUF;
        gtd[0].BE = gtd[0].CBP + want_len - 1;
        gtd[0].NextTD = (unsigned int)&gtd[1];
        t1 = s_Get10MsTimerCount()+ (timeout_ms*((want_len+cur_pipe.rx_packet_size-1)/cur_pipe.rx_packet_size)+9)/10;
    }
    else
    {
        gtd[0].Info = tx_data_no[cur_pipe.tx_ep_no]?
            TD_CC | TD_DP_OUT | TD_T_DATA1/*TD_T_TOGGLE*/:
            TD_CC | TD_DP_OUT | TD_T_DATA0;
            
        gtd[0].CBP = OHCI_DATA_BUF;
        gtd[0].BE = gtd[0].CBP + want_len- 1;
        gtd[0].NextTD = (unsigned int)&gtd[1];
        memcpy((unsigned char *)gtd[0].CBP,
            (unsigned char *)(data),want_len);
        t1 = s_Get10MsTimerCount()+ (timeout_ms*((want_len+cur_pipe.rx_packet_size-1)/cur_pipe.tx_packet_size)+9)/10;
    }

    //fill packet in ed
    if(isIn)ged[0].Info = cur_pipe.rx_packet_size<<16 | DevAddr | (cur_pipe.rx_ep_no<<7);
    else ged[0].Info = cur_pipe.tx_packet_size<<16 | DevAddr | (cur_pipe.tx_ep_no<<7);
    ged[0].HeadP = (unsigned int)&gtd[0];//head td
    ged[0].TailP = (unsigned int)&gtd[1];//just one td
    ged[0].NextED = 0;//just one ed
/*
    //if(rn>57 && isIn)
    {
    x_debug("\rstart-ed:0x%X,0x%X,0x%X,0x%X\n",
        ged[0].Info,ged[0].TailP,ged[0].HeadP,ged[0].NextED);

    for(i=0;i<1;i++)
        x_debug("\rtd[%d]:0x%X,0x%X,0x%X,0x%X\n",i,
            gtd[i].Info,gtd[i].CBP,gtd[i].NextTD,gtd[i].BE);

    x_debug("\rhcca->donehead:0x%X\n",ghcca->HccaDoneHead);
    }
*/

    rHC_INTERRUPT_STATUS = rHC_INTERRUPT_STATUS;
    rHC_BULK_HEAD_ED = (unsigned int)&ged[0];
    rHC_COMMAND_STATUS |=1<<2;//BulklListFilled
    rHC_CONTROL |= 1<<5;//BulklListEnable

    while(1)
    {
        if(rHC_INTERRUPT_STATUS & 0x10)//Unrecoverable Error
            return 1;
        
        if(rHC_INTERRUPT_STATUS & (1<<6))//roothub status change
        {
			port_open_state[cur_pipe.device_id]=0;
			return 2;
		}
        
        if((ged[0].TailP>>2) == (ged[0].HeadP>>2))
        {
            break;
        }
        
        if(s_Get10MsTimerCount() > t1)
        {
            //x_debug("\r%s timeout\n",__func__);
            return 3;
        }
    }
/*
    {
    x_debug("\rint_sts:0x%X\n",rHC_INTERRUPT_STATUS);

    x_debug("\red:0x%X,0x%X,0x%X,0x%X\n",
        ged[0].Info,ged[0].TailP,ged[0].HeadP,ged[0].NextED);
    for(i=0;i<1;i++)
        x_debug("\rtd[%d]:0x%X,0x%X,0x%X,0x%X\n",i,
            gtd[i].Info,gtd[i].CBP,gtd[i].NextTD,gtd[i].BE);
    x_debug("\rhcca->donehead:0x%X\n",ghcca->HccaDoneHead);
    }
*/
    
    if(isIn)
    {
        if(((gtd[0].Info >>26) & 0x03)==2) 
        {
            //x_debug("\rTd error,info:0x%X\n",gtd[0].Info);
            return 4;
        }
        if(gtd[0].CBP ==0) tmpd = want_len;
        else tmpd = want_len -(gtd[0].BE+1-gtd[0].CBP);

        memcpy((unsigned char *)(data),
            (unsigned char *)(OHCI_DATA_BUF),tmpd);
        rx_data_no[cur_pipe.rx_ep_no] = (ged[0].HeadP&0x2)?1:0;

        *len = tmpd;
    }
    else
        tx_data_no[cur_pipe.tx_ep_no] = (ged[0].HeadP&0x2)?1:0;


    //ged[0].HeadP = ged[0].TailP = (unsigned int)&gtd[0];
    //rHC_CONTROL &= ~(1<<5);//ControlListEnable
    //rHC_BULK_HEAD_ED = 0;

    return 0;
}

static int ohci_host_in_ep0(int tx_len,unsigned char *io_data,int *rx_len)
{
    int ret;
    
    io_data[6] = (unsigned char)(*rx_len);
    io_data[7] = (unsigned char)((*rx_len)>>8);        
    ret = ctrl_send_wait(io_data, io_data, rx_len, 3000);
    DelayUs(2000);
    return ret;
}

int ohci_host_in_epx(unsigned char *io_data,int *rx_len)
{
    int len,ret,tmpd,size,i;

    size = (*rx_len+OHCI_DATA_LEN-1)/OHCI_DATA_LEN;
    
    for(len=0,i=0;i<size;i++)
    {
        if((*rx_len-len)>OHCI_DATA_LEN) tmpd = OHCI_DATA_LEN;
        else tmpd = *rx_len-len;

        ret = bulk_send_wait(1, (unsigned char*)(io_data+len), &tmpd, cur_pipe.io_timeout);
        if(ret)return ret;
        len+=tmpd;
        if(tmpd!=OHCI_DATA_LEN) break;
    }

    *rx_len = len;
    
    return 0;
}


static int ohci_host_out_ep0(int tx_len,unsigned char *tx_data)
{
    int ret;
    ret = ctrl_send_wait(tx_data, tx_data, &tx_len, 3000);
    DelayUs(2000);
    return ret;
}

int ohci_host_out_epx(int tx_len,unsigned char *tx_data)
{
    int len,ret,tmpd,size,i;

    size = (tx_len+OHCI_DATA_LEN-1)/OHCI_DATA_LEN;

    for(len=0,i=0;i<size;i++)
    {
        if((tx_len-len)>OHCI_DATA_LEN) tmpd = OHCI_DATA_LEN;
        else tmpd = tx_len-len;
        
        ret = bulk_send_wait(0, (unsigned char *)(tx_data+len), &tmpd, cur_pipe.io_timeout);
        if(ret) return ret;
        len+=tmpd;
    }
    
    return 0;
}

//device_id:1--U disk,2--SP30 USB,0--both U disk and SP30 USB
//reset:0--first detect,1--reset device
//SOF_out:1--output SOF,0--not output SOF
static unsigned char ohci_host_detect(int device_id,int reset)
{
    unsigned char a,tmps[1100];
	int tmpd,tn,rn,i,cn;
	ST_BULK_IO bulk_out,*p_bulk_in;
	ST_UFI_HEAD ufi_head;

	if(usb_host_ohci_state<S_HCI_INITIALIZED)return USB_ERR_NOTFREE;//not initialized

//1 Reset device

    i=0;
    while ((rHC_INTERRUPT_STATUS&(1<<6))==0)//bit6-RootHubStatusChange
    {
		DelayUs(1);
		if (++i > 100000) 
		{
            //x_debug("\rNo Hub Interrupt\n");
            return USB_ERR_DEV_ABSENT;
		}
	}
    rHC_INTERRUPT_STATUS = rHC_INTERRUPT_STATUS;

    if((rHC_RH_PORT_STATUS1 & (1<<16))==0) //bit16-ConnectStatusChange
    {
        //x_debug("\rNo ConnectStatusChange 0x%X\n",rHC_RH_PORT_STATUS1);
        return USB_ERR_DEV_ABSENT;
    }
    
    if((rHC_RH_PORT_STATUS1 & 1) == 0) //bit0-CurrentConnectStatus
    {
        //x_debug("\rNo CurrentConnectStatus 0x%X\n",rHC_RH_PORT_STATUS1);
        return USB_ERR_DEV_ABSENT;
    }

    rHC_RH_PORT_STATUS1 = 0x00010000;
    DelayMs(200);

    i=0;
    while(1)
    {
        if ((rHC_RH_PORT_STATUS1) & 0x00000001)
            rHC_RH_PORT_STATUS1 = 0x00000010;

        DelayMs(200);

        if((rHC_RH_PORT_STATUS1) & 0x00000002)
            break;

        DelayMs(200);

        i++;
        if(i>5) 
        {
            //x_debug("\rPort Disable\n");
            return 16;
        }
    }
    rHC_RH_PORT_STATUS1 = 0x00100000;


    //x_debug("\rPort Rest Complete:0x%X\n",rHC_RH_PORT_STATUS1);

	//2--reset relevant variables
	DevAddr = 0;
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
		usb_host_ohci_state=S_HCI_STARTED;
		tx_read=0;tx_write=0;rx_read=0;rx_write=0;
		memset(&cur_pipe,0x00,sizeof(cur_pipe));
		memset(port_open_state,0x00,sizeof(port_open_state));

		if(device_id==T_NULL || device_id==T_UDISK)cur_pipe.sof_required=1;
		else cur_pipe.sof_required=0;
	}

	cur_pipe.speed_type=0xff;

    if(rHC_RH_PORT_STATUS1&(1<<9))//low speed of device
        cur_pipe.speed_type=0;
    else //full speed of device
        cur_pipe.speed_type=1;
    //x_debug("\rDeviceSpeed:%d\n",cur_pipe.speed_type);
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
	rn=EP_BUF_SIZE;
	tmpd=ohci_host_in_ep0(tn,tmps,&rn);
	if(tmpd)
	{
        //x_debug("\rError1:%d\n",tmpd);
        return 20+tmpd;
	}
/*
    x_debug("\rDevDesc-");
    for(i=0;i<rn;i++)
    {
        x_debug("0x%X ",tmps[i]);
    }
    x_debug("\n");    
*/    
	if(rn>sizeof(desc_device))rn=sizeof(desc_device);
	memset(&desc_device,0x00,sizeof(desc_device));
	memcpy(&desc_device,tmps,rn);

	//5--set device address
	tn=8;
	memcpy(tmps,"\x00\x05\x01\x00\x00\x00\x00\x00",tn);
	tmpd=ohci_host_out_ep0(tn,tmps);
	if(tmpd)
	{
        //x_debug("\rError2:%d\n",tmpd);
        return 30+tmpd;
	}
    DevAddr = 1;

	//6--fetch configuration descriptor
	tn=8;
	memcpy(tmps,"\x80\x06\x00\x02\x00\x00\xff\x00",tn);
	rn=desc_device.ep0_packet_size;
	tmpd=ohci_host_in_ep0(tn,tmps,&rn);
	if(tmpd)
	{
        //x_debug("\rError3:%d\n",tmpd);
        return 40+tmpd;
	}
	//--61,analyse the config packet
//for(i=0;i<rn;i++)x_debug("%02X ",tmps[i]);
//x_debug("\n");
	cn=rn;
	if(cn>sizeof(desc_config))cn=sizeof(desc_config);
	memset(&desc_config,0x00,sizeof(desc_config));
	memcpy(&desc_config,tmps,cn);
	tmpd=sizeof(desc_config);//rn-=sizeof(desc_config);
//x_debug("config_id:%d,interface:%d\n",desc_config.config_id,desc_config.interfaces);

	if(tmps[tmpd]==3)
        tmpd+=3;
	cn=rn-tmpd;//cn=rn;
	if(cn>sizeof(desc_face))cn=sizeof(desc_face);
	memset(&desc_face,0x00,sizeof(desc_face));
	//if(cn)memcpy(&desc_face,tmps+sizeof(desc_config),cn);
	if(cn)memcpy(&desc_face,tmps+tmpd,cn);
	tmpd+=sizeof(desc_face);//rn-=sizeof(desc_face);
//x_debug("interface id:%d-%d,protocol:%02X\n",
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
//x_debug("ep%d,attrib:%02X\n",i,desc_ep[i].attrib);
	}

	//7--fetch string descriptor 0,language id list
	tn=8;
	memcpy(tmps,"\x80\x06\x00\x03\x00\x00\xff\x00",tn);
	rn=desc_device.ep0_packet_size;
	tmpd=ohci_host_in_ep0(tn,tmps,&rn);
//x_debug("\rFetch str0:%d\n",tmpd);
	//if(tmpd)return 50+tmpd;//some UDISK may not support it

//for(i=0;i<rn;i++)x_debug("%02X ",tmps[i]);
//x_debug("\n");
//	memcpy(&desc_string,tmps,rn);

	//8--fetch string descriptor:manufacturer
	if(desc_device.manufacturer_index)
	{
		tn=8;
		memcpy(tmps,"\x80\x06\x00\x03\x00\x00\xff\x00",tn);
		tmps[2]=desc_device.manufacturer_index;
		rn=desc_device.ep0_packet_size;
		tmpd=ohci_host_in_ep0(tn,tmps,&rn);
		if(tmpd)return 50+tmpd;
//x_debug("\rMANUFACTURER:\n");
//for(i=0;i<rn;i++)x_debug("%02X ",tmps[i]);
//x_debug("\n");
//for(i=2;i<rn;i+=2)x_debug("%c",tmps[i]);
//x_debug("\n");
	}

	//9--fetch string descriptor:product
	if(desc_device.product_index)
	{
		tn=8;
		memcpy(tmps,"\x80\x06\x00\x03\x00\x00\xff\x00",tn);
		tmps[2]=desc_device.product_index;
		rn=desc_device.ep0_packet_size;
		tmpd=ohci_host_in_ep0(tn,tmps,&rn);
		if(tmpd)return 60+tmpd;
//x_debug("\rPRODUCT:\n");
//for(i=0;i<rn;i++)x_debug("%02X ",tmps[i]);
//x_debug("\n");
//for(i=2;i<rn;i+=2)x_debug("%c",tmps[i]);
//x_debug("\n");
	}

	//10--fetch string descriptor:serial
	if(desc_device.serial_index)
	{
		tn=8;
		memcpy(tmps,"\x80\x06\x00\x03\x00\x00\xff\x00",tn);
		tmps[2]=desc_device.serial_index;
		rn=desc_device.ep0_packet_size;
		tmpd=ohci_host_in_ep0(tn,tmps,&rn);
		if(tmpd)return 70+tmpd;
        //x_debug("\rrn3:%d\n",rn);
//x_debug("SERIAL:\n");
//for(i=0;i<rn;i++)x_debug("%02X ",tmps[i]);
//x_debug("\n");
//for(i=2;i<rn;i+=2)x_debug("%c",tmps[i]);
//x_debug("\n");
	}

	//11--select config
	tn=8;
	memcpy(tmps,"\x00\x09\x01\x00\x00\x00\x00\x00",tn);
	tmps[2]=desc_config.config_id;
	tmpd=ohci_host_out_ep0(tn,tmps);
	if(tmpd)return 80+tmpd;

	//12--select interface
	tn=8;
	memcpy(tmps,"\x01\x0b\x00\x00\x00\x00\x00\x00",tn);
	tmps[2]=desc_face.alternate_no;
	tmps[4]=desc_face.interface_id;
	tmpd=ohci_host_out_ep0(tn,tmps);
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
    //x_debug("\rTxPort:%d,RxPort:%d\n",cur_pipe.tx_ep_no,cur_pipe.rx_ep_no);

	cur_pk.state=S_CONFIGURED;
	tx_data_no[cur_pipe.tx_ep_no]=0;
	rx_data_no[cur_pipe.rx_ep_no]=0;

	if(desc_face.class==0x08)//mass storage
	{
	cur_pipe.io_timeout=2000;//1000;

	memset(&ufi_head,0x00,sizeof(ufi_head));
	memcpy(ufi_head.sig,"USBC",4);
	ufi_head.dlen=36;
	ufi_head.flag=0x80;
	ufi_head.cmd_len=12;
	memcpy(ufi_head.cmd,"\x12\x00\x00\x00\x24",5);
	tn=sizeof(ufi_head);
	tmpd=ohci_host_out_epx(tn,(unsigned char*)&ufi_head);
	if(tmpd)return 91+tmpd;

	memset(tmps,0x00,sizeof(tmps));
	rn=36;
	tmpd=ohci_host_in_epx(tmps,&rn);
	if(tmpd)return 100+tmpd;
//x_debug("UFI inquiry:%d\n",rn);
//for(i=0;i<rn;i++)x_debug("%02X ",tmps[i]);
//x_debug("\n");

	memset(tmps,0x00,sizeof(tmps));
	rn=13;
	tmpd=ohci_host_in_epx(tmps,&rn);
	if(tmpd)return 110+tmpd;
//x_debug("UFI inquiry CSW:%d\n",rn);
//for(i=0;i<rn;i++)x_debug("%02X ",tmps[i]);
//x_debug("\n");
	if(device_id==T_NULL)cur_pipe.device_id=T_UDISK;
	else cur_pipe.device_id=device_id;
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
//x_debug("Enquiry,tn:%d\n",tn);
	tmpd=ohci_host_out_epx(tn,(unsigned char*)&bulk_out);
	if(tmpd)
	{
        //x_debug("\rError4:%d\n",tmpd);
        return 120+tmpd;
	}
	rn=sizeof(ST_BULK_IO);
	tmpd=ohci_host_in_epx(tmps,&rn);
	if(tmpd)
	{
        //x_debug("\rError5:%d\n",tmpd);
        return 129+tmpd;
	}
	p_bulk_in=(ST_BULK_IO *)tmps;
	if(rn<20){/*x_debug("\rrn:%d\n",rn);*/return 135;}

	if(cur_pipe.device_version>=0x0101 && verify_checksum(p_bulk_in))return 136;
	if(p_bulk_in->req_type!=2)return 137;
	if(cur_pipe.device_version>=0x0101)bulk_out.seq_no&=0x0f;
	if(p_bulk_in->seq_no!=bulk_out.seq_no)return 138;//SEQ_NO mismatches
	memcpy(&dev_state,p_bulk_in->data,sizeof(dev_state));

//x_debug("size:%d-%d,left:%d,%d\n",dev_state.tx_buf_size,dev_state.rx_buf_size,dev_state.tx_left,dev_state.rx_left);
	usb_host_ohci_state=S_HCI_DETECTED;
	cur_pipe.device_id=T_PAXDEV;
	}

    return 0;
}


static unsigned char ohci_reset_pipe_port(void)
{
	unsigned char a,tmps[100];
	int tn;

	tx_data_no[cur_pipe.tx_ep_no]=0;
	rx_data_no[cur_pipe.rx_ep_no]=0;

	//--clear OUT ENDPOINT_HALT
	tn=8;
	memcpy(tmps,"\x02\x01\x00\x00\x01\x00\x00\x00",tn);
	a=ohci_host_out_ep0(tn,tmps);

	if(!a)
	{
	//--clear IN ENDPOINT_HALT
	tn=8;
	memcpy(tmps,"\x02\x01\x00\x00\x81\x00\x00\x00",tn);
	a=ohci_host_out_ep0(tn,tmps);
	if(a)a+=10;
	}

	if(a)a=ohci_host_detect(T_PAXDEV,1);

	return a;
}

static unsigned char ohci_host_bulkio(unsigned char R_or_S)
{
	unsigned char a,tmps[2100];
	int tmpd,tn,rn,dn,rp_left,tp_left,i,err;
	ST_BULK_IO bulk_out,*p_bulk_in;
//static int loops=0;

	switch(R_or_S)
	{
	case 'R':goto RECV_PROCESS;
	case 'S':goto SEND_PROCESS;
	}

RECV_PROCESS:
	do
	{
//x_debug("rx loops:%d...\n",dev_state.tx_left);
		//3--check if rx_pool has space
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
			tmpd=ohci_host_out_epx(tn,(unsigned char*)&bulk_out);
			if(tmpd){err=140+tmpd;goto R_TAIL;}

			//5--receive BULK_IN response
			rn=sizeof(ST_BULK_IO);
			tmpd=ohci_host_in_epx(tmps,&rn);
			if(tmpd){err=150+tmpd;goto R_TAIL;}

			p_bulk_in=(ST_BULK_IO *)tmps;
			if(cur_pipe.device_version>=0x0101 && verify_checksum(p_bulk_in))
				{err=155;goto R_TAIL;}//CHECK_SUM mismatches

			if(rn<4 || rn<p_bulk_in->len+4)
			{
//x_debug("\rERROR len,rn:%d,%02X%02X%02X%02X,sys_seq:%02X\n",rn,tmps[0],tmps[1],tmps[2],tmps[3],bulk_out.seq_no);
				err=156;
				goto BIO_TAIL;
			}

			//--if dev's rx_pool is empty
			if(p_bulk_in->req_type==2 && p_bulk_in->len>=sizeof(dev_state))
			{
//x_debug("RX POOL empty...\n",loops++);
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

            if((rHC_RH_PORT_STATUS1&0x103)!=0x103){err=160;goto BIO_TAIL;}//device is removed

			a=ohci_reset_pipe_port();
//x_debug("\r%d,RX RESET PIPE:%d,SEQ:%02X,err:%d\n",i,a,bulk_out.seq_no,err);
		}//for(i)
		if(err)goto BIO_TAIL;

		//6--store data into rx_pool
		dn=p_bulk_in->len;
		if(rx_write+dn<=RX_POOL_SIZE)
			memcpy(rx_pool+rx_write,p_bulk_in->data,dn);
		else
		{
			memcpy(rx_pool+rx_write,p_bulk_in->data,RX_POOL_SIZE-rx_write);
			memcpy(rx_pool,p_bulk_in->data+RX_POOL_SIZE-rx_write,dn+rx_write-RX_POOL_SIZE);
		}
		rx_write=(rx_write+dn)%RX_POOL_SIZE;
		dev_state.tx_left-=dn;
		cur_pipe.seq_no++;

//x_debug("RX,%02X,DN:%d,%02X\n",p_bulk_in->seq_no,p_bulk_in->len,p_bulk_in->data[0]);
	}while(dev_state.tx_left>0);//while(device has data to be sent)


SEND_PROCESS:
	do
	{
		//13--check if tx_pool has data to be sent
		tp_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;
		if(!tp_left)break;
//x_debug("tx loops:%d...\n",tp_left);

		if(tp_left>sizeof(bulk_out.data))dn=sizeof(bulk_out.data);
		else dn=tp_left;
		if(dn>dev_state.rx_left)dn=dev_state.rx_left;

		//14--send BULK_OUT request
		memset(&bulk_out,0x00,sizeof(bulk_out));
		bulk_out.seq_no=cur_pipe.seq_no;
		bulk_out.req_type=0;//BULK_OUT
		bulk_out.len=dn;

		//--move data from tx_pool to out_pool
		if(tx_read+dn<=TX_POOL_SIZE)memcpy(bulk_out.data,tx_pool+tx_read,dn);
		else
		{
			memcpy(bulk_out.data,tx_pool+tx_read,TX_POOL_SIZE-tx_read);
			memcpy(bulk_out.data+TX_POOL_SIZE-tx_read,tx_pool,dn+tx_read-TX_POOL_SIZE);
		}

		if(cur_pipe.device_version>=0x0101)tn=dn+4;
		else tn=sizeof(bulk_out);
		if(cur_pipe.device_version>=0x0101)set_checksum(&bulk_out);

		for(i=0;i<BIO_MAX_RETRIES;i++)
		{
			err=0;

			tmpd=ohci_host_out_epx(tn,(unsigned char*)&bulk_out);
			if(tmpd){err=160+tmpd;goto W_TAIL;}

			//15--receive STATE query response
			rn=sizeof(ST_BULK_IO);
			tmpd=ohci_host_in_epx(tmps,&rn);
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
			if((rHC_RH_PORT_STATUS1&0x103)!=0x103){err=179;goto BIO_TAIL;}//device is removed

			a=ohci_reset_pipe_port();
//x_debug("\r%d,TX RESET PIPE:%d,err:%d ",i,a,err);
//x_debug("\rSEQ:%02X,dn:%d\n",bulk_out.seq_no,bulk_out.len);
		}//for(i)
		if(err)goto BIO_TAIL;

		tx_read=(tx_read+dn)%TX_POOL_SIZE;
		cur_pipe.seq_no++;

///x_debug("TX,%02X,DN:%d,%02X\n",p_bulk_in->seq_no,dn,bulk_out.data[0]);
	}while(dev_state.rx_left>0);//while(device has space for receive)

	if(!dev_state.rx_left)DelayUs(2000);//2000

BIO_TAIL:
	return err;
}

int ohci_fs_mount_udisk(void)
{
	uchar a;

	if(!port_open_state[T_UDISK] && !port_open_state[T_UDISK_FAST])
	{
		set_udisk_a_absent();
		return USB_ERR_NOTOPEN;
	}

	//a=r_reg(rUSB_INT_STATUS);//UDISK absent
	//if(a&0x60)
	if((rHC_RH_PORT_STATUS1&0x103)!=0x103)
	{
		set_udisk_a_absent();
		port_open_state[T_UDISK]=0;
		port_open_state[T_UDISK_FAST]=0;
	}
	return 0;
}


unsigned char OhciSends(char *buf,unsigned short size)
{
	unsigned char a,tmpc;
	int i,tx_left;

	if(usb_host_ohci_state!=S_HCI_DETECTED)return USB_ERR_NOTOPEN;//not opened

	//--check if enough tx buffer
	tx_left=(tx_write+TX_POOL_SIZE-tx_read)%TX_POOL_SIZE;

	if(size>=TX_POOL_SIZE || size+tx_left>=TX_POOL_SIZE)
		return USB_ERR_BUF;//buffer occupied

	for(i=0;i<size;i++)
		tx_pool[(tx_write+i)%TX_POOL_SIZE]=buf[i];
	tx_write=(tx_write+size)%TX_POOL_SIZE;

	tmpc=ohci_host_bulkio('S');
	if(tmpc)
	{
//x_debug("tx bio:%d\n",tmpc);
		//a=r_reg(rUSB_INT_STATUS);
		if((rHC_RH_PORT_STATUS1&0x103)!=0x103)return USB_ERR_DEV_REMOVED;//device is removed

		return tmpc;
	}

	return 0;
}

int OhciRecvs(unsigned char *buf,unsigned short want_len,unsigned short timeout_ms)
{
	unsigned char tmpc,a;
	int i,j,t0;

	if(usb_host_ohci_state!=S_HCI_DETECTED)return -USB_ERR_NOTOPEN;//not opened

	if(rx_read==rx_write)
	{
		tmpc=ohci_host_bulkio('R');
		if(tmpc)
		{
			//a=r_reg(rUSB_INT_STATUS);
			if((rHC_RH_PORT_STATUS1&0x103)!=0x103)return -USB_ERR_DEV_REMOVED;//device is removed

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
            tmpc=ohci_host_bulkio('R');
    		if(tmpc)
    		{
    			//a=r_reg(rUSB_INT_STATUS);
    			if((rHC_RH_PORT_STATUS1&0x103)!=0x103)return -USB_ERR_DEV_REMOVED;//device is removed

    			return -tmpc;
    		}

			if(!timeout_ms)break;
			if((s_Get10MsTimerCount()-t0)<timeout_ms)continue;
			if(i)break;

			return -USB_ERR_TIMEOUT;
		}

		buf[i]=rx_pool[j];
		i++;
	}//while()
	rx_read=(rx_read+i)%RX_POOL_SIZE;

	return i;
}

unsigned char OhciTxPoolCheck(void)
{
	unsigned char a,tmpc;

	if(usb_host_ohci_state!=S_HCI_DETECTED)return USB_ERR_NOTOPEN;//not opened

	tmpc=ohci_host_bulkio('R');
	if(tmpc>1)
	{
		//a=r_reg(rUSB_INT_STATUS);
		if((rHC_RH_PORT_STATUS1&0x103)!=0x103)return USB_ERR_DEV_REMOVED;//device is removed

		return tmpc;
	}

	if(tx_write!=tx_read)
		return 1;//not finished

	return 0;
}

unsigned char OhciReset(void)
{
    unsigned char tmpc;
    
	if(usb_host_ohci_state!=S_HCI_DETECTED)return USB_ERR_NOTOPEN;//not opened

    if(cur_pipe.device_id==T_PAXDEV)
	    tmpc=ohci_host_bulkio('R');
    
	rx_read=rx_write;//clear rx pool
	return 0;
}

unsigned char OhciOpen(int device_id)
{
	unsigned char a,tmpc;
	int i,tmpd;

	if((usb_host_ohci_state<0)||(usb_host_ohci_state==4))return USB_ERR_NOTFREE;//occupied by device

	//reset_udev_state();

    //x_debug("\r%s-usb_host_ohci_state:%d\n",__func__,usb_host_ohci_state);

	if(usb_host_ohci_state<S_HCI_INITIALIZED)
	{        
		a=ohci_host_init();
		if(a)
		{
            //x_debug("\rohci_host_init failed!-%d\n",a);
            return a;
		}
	}
	//1--if it's already opened
	//a=r_reg(rUSB_INT_STATUS);//device absent
	//if(!(a&0x60))

    //Port Enable and A device is attached to the port
    //and Port Enable/Disable No Change
    if((rHC_RH_PORT_STATUS1&0x103)==0x103)
	{
		if(device_id!=T_NULL && port_open_state[device_id])return 0;
		if(device_id==T_NULL && (port_open_state[T_UDISK] ||
			port_open_state[T_UDISK_FAST] || port_open_state[T_PAXDEV]))return 0;
	}

	//--detect the device up to twice
	tmpc=ohci_host_detect(device_id,0);

	if(tmpc)
	{
        //x_debug("\rReDetect...\n");
        if(device_id == T_UDISK || device_id == T_UDISK_FAST ||
            device_id == T_NULL)
        {
            ohci_host_init();
            DelayMs(1000);
        }
        if(ohci_host_detect(device_id,0)==0)tmpc=0;
	}

//x_debug("Detect result:%d,id:%d\n",tmpc,cur_pipe.device_id);
	if(tmpc)
	{
		usb_host_ohci_state=0;
		return tmpc;
	}
	if(cur_pipe.device_id==T_PAXDEV)
	{
		port_open_state[cur_pipe.device_id]=1;
		return 0;
	}

	tmpd=usb_enum_massdev_udiskA();
//x_debug("Enum UDISK result:%d\n",tmpd);
	if(tmpd)
	{
		usb_host_ohci_state=0;
		return USB_ERR_DEV_QUERY;
	}
	port_open_state[cur_pipe.device_id]=1;
//x_debug("device_id:%d\n",cur_pipe.device_id);
	return 0;
}

unsigned char OhciClose(void)
{
	unsigned char tmpc;

	if((usb_host_ohci_state<0)||(usb_host_ohci_state==4))return USB_ERR_NOTFREE;//occupied by device

	usb_host_ohci_state=0;

	if(cur_pipe.device_id!=T_PAXDEV && cur_pipe.device_id!=T_UDISK
	&& cur_pipe.device_id!=T_UDISK_FAST)return USB_ERR_NOTOPEN;//not opened

    if((cur_pipe.device_id==T_PAXDEV)&&(usb_host_ohci_state==S_HCI_DETECTED))
	    tmpc=ohci_host_bulkio('R');

	rx_read=rx_write;//clear rx pool

	port_open_state[cur_pipe.device_id]=0;
	if(GetHWBranch() == D210HW_V2x )
	{
		usb_deinit_massdev_udiskA();
	}
	return 0;
}

//#define USB_TEST
#ifdef USB_TEST

#if 0
void dump_ohci(void)
{
    x_debug("\r%s\n",__func__);

    x_debug("\rrHC_REVISION:%X\n",rHC_REVISION);
    x_debug("\rrHC_CONTROL:%X\n",rHC_CONTROL);
    x_debug("\rrHC_COMMAND_STATUS:%X\n",rHC_COMMAND_STATUS);
    x_debug("\rrHC_INTERRUPT_STATUS:%X\n",rHC_INTERRUPT_STATUS);
    x_debug("\rrHC_INTERRUPT_ENABLE:%X\n",rHC_INTERRUPT_ENABLE);
    x_debug("\rrHC_INTERRUPT_DISABLE:%X\n",rHC_INTERRUPT_DISABLE);
    x_debug("\rrHC_HCCA:%X\n",rHC_HCCA);
    x_debug("\rrHC_PERIOD_CURRENT_ED:%X\n",rHC_PERIOD_CURRENT_ED);
    x_debug("\rrHC_CONTROL_HEAD_ED:%X\n",rHC_CONTROL_HEAD_ED);
    x_debug("\rrHC_CONTROL_CURRENT_ED:%X\n",rHC_CONTROL_CURRENT_ED);
    x_debug("\rrHC_BULK_HEAD_ED:%X\n",rHC_BULK_HEAD_ED);
    x_debug("\rrHC_BULK_CURRENT_ED:%X\n",rHC_BULK_CURRENT_ED);
    x_debug("\rrHC_DONE_HEAD:%X\n",rHC_DONE_HEAD);
    x_debug("\rrHC_FM_INTERVAL:%X\n",rHC_FM_INTERVAL);
    x_debug("\rrHC_FM_REMAINING:%X\n",rHC_FM_REMAINING);
    x_debug("\rrHC_FM_NUMBER:%X\n",rHC_FM_NUMBER);
    x_debug("\rrHC_PERIODIC_START:%X\n",rHC_PERIODIC_START);
    x_debug("\rrHC_LS_THRESHOLD:%X\n",rHC_LS_THRESHOLD);
    x_debug("\rrHC_RH_DESCRIPTORA:%X\n",rHC_RH_DESCRIPTORA);
    x_debug("\rrHC_RH_DESCRIPTORB:%X\n",rHC_RH_DESCRIPTORB);
    x_debug("\rrHC_RH_STATUS:%X\n",rHC_RH_STATUS);
    x_debug("\rrHC_RH_PORT_STATUS1:%X\n",rHC_RH_PORT_STATUS1);
}
#else

void dump_ohci(void)
{
    unsigned int *reg = (unsigned int *)0x000B1000;
    unsigned int i;
    x_debug("\r%s\n",__func__);

    for(i=0;i<22;i++)
    {
        x_debug("\raddr-0x%X v:0x%X\n",reg+i,*(reg+i));
    }

    x_debug("\n");
}
#endif

void UsbOhciTest(void)
{
	uchar tmpc,fn,tmps[10300],xstr[10300];
	int tmpd,rn,tn,i,loops;
	unsigned long byte_count,speed,elapse;
	uchar last_cc,a,b;

	OhciClose();
	ScrCls();
	PortOpen(0,"115200,8,n,1");
	while(1)
	{
T_BEGIN:
	ScrCls();
	ScrPrint(0,0,0,"USB HOST TEST,130417B");
	ScrPrint(0,1*2,0,"1-OPEN UDISK");
	ScrPrint(0,2*2,0,"2-OPEN UDISKFAST");
	ScrPrint(0,3*2,0,"3-OPEN PAXDEV");
	ScrPrint(0,4*2,0,"4-OPEN P_USB");
	ScrPrint(0,5*2,0,"5-Recvs  6-RX/TX");
	ScrPrint(0,6*2,0,"7-Sends   9-CLOSE");
	ScrPrint(0,7*2,0,"8-OPEN NULL 0-DEVICE");

	fn=getkey();
	if(fn==KEYCANCEL)return;

	ScrCls();
	switch(fn)
	{
	case '1':
		//tmpc=OhciOpen(T_UDISK);
		tmpc=PortOpen(P_USB_HOST,"Udisk");
		ScrPrint(0,0*2,0,"OPEN:%d,SPEED:%d",tmpc,cur_pipe.speed_type);
		ScrPrint(0,1*2,0,"DEV VER:%04X",cur_pipe.device_version);
		getkey();
		break;
	case '2':
    case 5:
		//tmpc=OhciOpen(T_UDISK_FAST);
		tmpc=PortOpen(P_USB_HOST,"UdiskFast");
		ScrPrint(0,1*2,0,"OPEN RESULT:%d,SPEED:%d",tmpc,cur_pipe.speed_type);
		ScrPrint(0,3*2,0,"DEVICE VERSION:%04X",cur_pipe.device_version);
		getkey();
		break;
	case '3':
    case 6:
		//tmpc=OhciOpen(T_PAXDEV);
		tmpc=PortOpen(P_USB_HOST,"PaxDev");
		ScrPrint(0,1*2,0,"OPEN RESULT:%d,SPEED:%d",tmpc,cur_pipe.speed_type);
		ScrPrint(0,3*2,0,"DEVICE VERSION:%04X",cur_pipe.device_version);
		getkey();
		break;
	case '4':
		tmpc=PortOpen(P_USB,"115200,8,n,1");
		ScrPrint(0,1*2,0,"OPEN RESULT:%d",tmpc);
		getkey();
		break;
	case '8':
		//tmpc=OhciOpen(T_NULL);
		tmpc=PortOpen(P_USB_HOST,NULL);
		ScrPrint(0,1*2,0,"OPEN RESULT:%d,SPEED:%d",tmpc,cur_pipe.speed_type);
		ScrPrint(0,3*2,0,"DEVICE VERSION:%04X",cur_pipe.device_version);
		getkey();
		break;
	case '5':

        tmpd=PortRecvs(P_USB_HOST,xstr+rn,10240,0);
        ScrPrint(0,1*2,0,"PortRecvs ret:%d",tmpd);
        tmpd = PortTxPoolCheck(P_USB_HOST);
        ScrPrint(0,2*2,0,"PortTxPoolCheck ret:%d",tmpd);
        getkey();
/*
        tmpc=r_reg(rUSB_INT_STATUS);
		ScrPrint(0,1,0,"INSERT RESULT:%02X",tmpc);
		ScrPrint(0,2,0,"SOF:%02X",cur_pipe.sof_required);
		ScrPrint(0,3,0,"1-CLEAR STATUS");
		ScrPrint(0,4,0,"2-CLEAR WITH FF");
		fn=getkey();
		if(fn=='1')w_reg(rUSB_INT_STATUS,tmpc);//clear status
		if(fn=='2')w_reg(rUSB_INT_STATUS,0xff);//clear status
		*/
		break;
	case '7':

        tmpc=PortSends(P_USB_HOST,xstr,512);
        ScrPrint(0,1*2,0,"PortSends 512 ret:%d",tmpc);
        getkey();
/*        
		tmpc=ohci_reset_pipe_port();
		ScrPrint(0,1*2,0,"RESET RESULT:%02X",tmpc);
		getkey();
*/        
		break;
	case '9':
        tmpc = PortReset(P_USB_HOST);
        ScrPrint(0,1*2,0,"RESET RESULT:%02X",tmpc);

		tmpc = PortClose(P_USB_HOST);//OhciClose();
		ScrPrint(0,2*2,0,"CLOSE 12 RESULT:%02X",tmpc);

		tmpc = PortClose(P_USB);//OhciClose();
		ScrPrint(0,4*2,0,"CLOSE 10 RESULT:%02X",tmpc);

        getkey();
		break;
	case '6'://RX/TX
		ScrPrint(0,0*2,0,"USB HOST RX/TX");

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
              ScrPrint(0,1*2,0,"CHECK RESLUT:%d",tmpc);
              
			  ScrPrint(0,2*2,0,"%d,RCV %d ",loops,tn);
			  ScrPrint(0,4*2,0,"%d,SENDING %d ",loops,tn);

				ScrPrint(0,5*2,0,"TM:%d ",TimerCheck(0));
				elapse=60000-TimerCheck(0);
				if(!elapse)elapse=1;
				speed=byte_count/elapse;
				speed*=10;
				ScrPrint(0,6*2,0,"SPEED:%d,%lu/%lu  ",speed,byte_count,elapse);
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
				ScrPrint(0,6*2,0,"%d,RECV FAILED:%d Port:0x%X,0x%X",loops,tmpd,rHC_RH_PORT_STATUS1,rHC_INTERRUPT_STATUS);
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
					ScrPrint(0,6*2,0,"%d,RECV MISMATCHED,i:%d-%02X ",loops,i,a);
					getkey();
					for(i=0;i<rn;i++)PortSend(0,xstr[i]);
					goto T_BEGIN;
				}
				last_cc=a;
			}

			tn=rn;
			if(!(loops%100))ScrPrint(0,2*2,0,"%d,SENDING %d ",loops,tn);
			//tmpc=OhciSends(xstr,tn);
			tmpc=PortSends(P_USB_HOST,xstr,tn);
			if(tmpc)
			{
				ScrPrint(0,6*2,0,"%d,SEND FAILED:%d Port:0x%X,0x%X",loops,tmpc,rHC_RH_PORT_STATUS1,rHC_INTERRUPT_STATUS);
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
void UsbTest(void)
{
    uchar tmpc,fn,tmps[10300],xstr[10300];
	int tmpd,tn,i,loops;
	unsigned long byte_count,speed,elapse;
	uchar last_cc,a,b;
    
    x_debug("\rUsbHostTest 20120507_01\n");

    while(1)
    {
        //tmpd = OhciOpen(T_PAXDEV);
        tmpd = PortOpen(P_USB_HOST, "PAXDEV");
        DelayMs(1000);
        if(tmpd) x_debug("\rOhciOpen:%d\n",tmpd);
        else break;
    }
    x_debug("\rDetect a device\n");

	loops=0;
	while(61)
	{
		rn=0;
		while(611)
		{
			//tmpc=UsbHostRecv(xstr+rn,0);
			//tmpc=PortRecv(P_USB_HOST,xstr+rn,0);
			//tmpd=OhciRecvs(xstr+rn,10240,0);
			tmpd = PortRecvs(P_USB_HOST,xstr+rn,10240,0);
			if(tmpd<0)break;
			//rn+=tmpd;
			//if(rn>=10240)break;
			if(tmpd){rn=tmpd;break;}
		}
		if(tmpd<0)
		{
            x_debug("OhciRecvs Failed!-%d\n",tmpd);
			while(1);
		}
		//if(!rn)continue;
        x_debug("\rReceived:%d\n",rn);
		tn=rn;
		//tmpc=OhciSends(xstr,tn);
        tmpc=PortSends(P_USB_HOST,xstr,tn);
		if(tmpc)
		{
			x_debug("%d,SEND FAILED:%d ",loops,tmpc);
            while(1);
		}
		//if(loops%100 ==0)
            x_debug("\r%d,SENDING %d ",loops,tn);

		loops++;
	}

    while(1);
}
#endif
