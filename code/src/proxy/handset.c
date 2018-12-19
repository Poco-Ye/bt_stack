//2014.4.1--Added F_MDM_POLL_TX_POOL for FSK's TxPool check
//        --Added clear and lock process when fetching SYNC data
//        --Added mdm_rx_has state return for F_CHECK to do rx after line-broken
//        --Added mdm_rx_has check before link-off to enable rx after line-broken
//        --Adjusted base_modem_occupied() dut to rx enabling after line-broken
//        --Changed GetTimerCount() to GetMs() in step_recv_packet()
//2014.5.23-Revised F_MDM_CHECK output from 1 byte to 4 bytes
//     6.12-Shifted SYNC data clear in IrdaSoftIrq()
//     6.17-Added data clear upon modem OnHook()
//         -Adjusted max rx count for async ModemRxd()
//         -Revised SYNC rx free and required len to enable SYNC overwriting
//         -Shifted SYNC mo_read update in IrdaSoftIrq(),and added modem_status refresh
//     6.25-Added mdm_status refresh after sync ModemRxd()

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "base.h"

#include  "..\..\comm\comm.h"
#include  "..\..\modem\modem.h"
//#include  "..\..\bluetooth\wlt2564.h" //deleted by shics
#include  "..\modulecheck\ModuleMenu.h"

//#define HANDSET_TEST

#define TASK_PAYLOAD 2049//32768
#define MODEM_TMP_POOL	4097
#define LINK_PAYLOAD 1024
#define HEARTBEAT_PAYLOAD 256
#define COM_IO_SIZE (10241) //(8193)
#define MDM_IO_SIZE 2054
#define LAN_IO_SIZE (4097)
#define UDISK_IO_SIZE 4097
#define PRN_IO_SIZE (32*1024+20)
#define TIMEOUT_RX  (150*2)  /*unit:ms*/
#define TIMEOUT_RX_BT  2500  /*unit:ms*/

#define TEST_DATA  		(0X68)
#define TEST_DATA_ACK  	(uchar)(~TEST_DATA)
#define F_GET_BASE_BT	14
#define F_NET_LISTEN    35
#define F_NET_ACCEPT	(F_NET_LISTEN + 1)
#define BASE_STATE_RESERVE_LEN	(512 - 159)
// 0: no base    1:charge only base  2:uart link base    3:bt one to one base
int g_hasbase = 0;				
OS_TASK *g_taskbase=NULL;

enum MODULE_NO{M_COM=1,M_MODEM,M_PRINT,M_LAN,M_SYS,M_OHCI,M_BASECHECK};
enum FUNC_NO{F_CLOSE=0,F_OPEN,F_SEND,F_RECV,F_RESET,F_PEEP,F_CHECK,F_GET,
			 F_MDM_START_RX=11,F_MDM_POLL_TX_POOL=13,FS_GETINFOCHECK=18};

static struct ST_SYS_STATE
{
	uchar cur_seq;
	uchar need_reset;
}sys_state;

static struct ST_TASK
{
	int pending;//request/response transfer not finished yet
	int module_no;
	int func_no;
	uchar channel_no;
	int dlen;
	uchar data[TASK_PAYLOAD];
}cur_task;

typedef struct
{
	uchar stx;
	uchar len[2];
	uchar seq;
	uchar type[2];
	uchar data[LINK_PAYLOAD+2];//last 2 for CRC filling
}__attribute__((__packed__)) ST_IO_PACK;

#if 0
struct ST_BASE_STATE
{
	ushort com_rx_has[4]; 
	ushort com_tx_free[4];
	ushort mdm_rx_has;
	ushort mdm_tx_free;
	uchar com_channel[4];
	uchar lan_event[10];
	ushort lan_rx_has[10];
	ushort lan_tx_free[10];
	int lan_status;
	uchar mdm_status;
}__attribute__((__packed__));// base_state;
struct ST_BASE_STATE base_state;
#endif

//底座的状态查询包扩展包
struct ST_BASE_STATE
{
	ushort com_rx_has[4];// 4// 2
	ushort com_tx_free[4];// 4 //2
	ushort mdm_rx_has; // 2
	ushort mdm_tx_free; // 2
	uchar com_channel[4];// 2 // 4
	uchar lan_event[10];// 10
	ushort lan_rx_has[10];//20
	ushort lan_tx_free[10];// 20
	int lan_status;// 4
	uchar mdm_status;// 1
	int lan_accept_count[10];//10 //4
	int lan_err_value[10];	 //10 //4
	uchar reserved[BASE_STATE_RESERVE_LEN];
}__attribute__((__packed__)) base_state;
//struct ST_BASE_STATE_EXT base_state_ext;


static struct ST_HEARTBEAT
{
	uchar enable;
	uchar channel_no;
	ushort interval_100ms;
	ushort msg_len;
	uchar msg_data[HEARTBEAT_PAYLOAD];
}__attribute__((__packed__)) heart_beat;

extern uchar base_xtab[P_PORT_MAX];

volatile uint io_start;
static int task_p0,task_p1;
//--com pool related variables
static uchar com_channel_xtab[4],com_pool_xtab[P_PORT_MAX],ci_pool[4][COM_IO_SIZE],co_pool[4][COM_IO_SIZE];
static uchar com_attrib[4][20];
static volatile ushort ci_read[4],ci_write[4],co_read[4],co_write[4];
//--mdm pool related variables
static uchar mdm_channel,mdm_icon_dial,mdm_icon_link,mi_pool[MDM_IO_SIZE],mo_pool[MDM_IO_SIZE],mdm_fskget_enabled;
static volatile ushort mi_read,mi_write,mo_read,mo_write;
//--lan pool related variables
static uchar lan_used,lan_open[10],li_pool[10][LAN_IO_SIZE],lo_pool[10][LAN_IO_SIZE];
static volatile ushort li_read[10],li_write[10],lo_read[10],lo_write[10];
//--base date sync variables

volatile uchar g_ExternalTimeFlag = 0;		// 记录命令的类型

static struct platform_timer_param s_HandSet_timer;
static struct platform_timer *pHandSet_timer=NULL;

volatile uchar g_linktype = 0xff;
uchar g_btlinkstatus = 0;
uchar g_btlinkedflag = 0;
//uchar g_disablebackmutex = 0;
volatile uchar g_OnBaseStatus = 0;		// 用来处理座机是否出现了状态的改变
volatile uchar g_getbasebtinfo = 0;
extern volatile char g_GetBaseInfoFlag;
//added by shics
#if 1
typedef struct {
	unsigned char status;
	unsigned char name[64];
	unsigned char mac[6];
} __attribute__((__packed__)) ST_BT_STATUS;
#endif

static ST_BT_STATUS gBtStatus;

void s_HandSetInit(uchar linktype);
int OnbaseCheck(void);
static int HandSetSoftIrq(void);
static void HandSetTimerInterrupt(void);
static uchar step_recv_packet(uchar *packet,ushort *dlen);
static uchar step_send_packet(uchar *packet,ushort dlen);
static void pack_up(uchar seq,uchar *type,uchar *din,uchar *dout,ushort *dlen);
uchar base_modem_occupied(void);
void force_close_com(uchar channel_no);
uint port_max_close_time(uchar channel);
uchar link_send(uchar module,uchar func,uchar channel,uint data_len,uchar *data);
uchar is_hasbase(void);
void SetBaseType(uchar basetype);
int GetBaseType(void);
void SetLinkType(uchar linktype);
uchar GetLinkType(void);
int IsSupportBaseStateExt(void);

static void ClearBaseBtInfo(void)
{
	memset(&base_info.bt_exist,0,23);
	memset(base_info.bt_name,0,20);
}

static void HandSetTimerInterrupt(void)
{
	// ??3y?D??
	platform_timer_IntClear(pHandSet_timer);
	HandSetSoftIrq();
}

static void HandSetTimerInterruptNull(void)
{
	// ??3y?D??
	platform_timer_IntClear(pHandSet_timer);
}

void s_HandSetInit(uchar linktype)
{	
	static unsigned char flag;
	
	if(GetHWBranch() != D210HW_V2x) return;
//	s_SetImplicitEcho();				// 回显不在动态保存，修复彩屏机器在应用状态下如果放上座机会修改显示字符集的bug
	if(!flag)
	{
		SysUseProxy();
		s_CommInit_Proxy();	
		//--close LINK port first
		LinkComm_UartStop();
		//
		if(pHandSet_timer!=NULL)platform_stop_timer(pHandSet_timer);
		s_HandSet_timer.timeout=3000;   /* unit is 1us */
		s_HandSet_timer.mode=TIMER_PERIODIC;		/*timer periodic mode. TIMER_PERIODIC or TIMER_ONCE*/
		s_HandSet_timer.handler=HandSetTimerInterrupt;    /* interrupt handler function pointer */
		s_HandSet_timer.interrupt_prior=INTC_PRIO_7;	/*timer's interrupt priority*/
		s_HandSet_timer.id=TIMER_IRDA;            		/* timer id number */
		pHandSet_timer=platform_get_timer(&s_HandSet_timer);
		platform_start_timer(pHandSet_timer);

		//reset global variables
		task_p0=0;
		memset(&sys_state,0x00,sizeof(sys_state));
		memset(&cur_task,0x00,sizeof(cur_task));

		//--reset COM variables
		memset((uchar*)ci_read, 0x00, sizeof(ci_read));
		memset((uchar*)ci_write, 0x00, sizeof(ci_write));
		memset((uchar*)co_read, 0x00, sizeof(co_read));
		memset((uchar*)co_write, 0x00, sizeof(co_write));
		memset(com_pool_xtab,0xff,sizeof(com_pool_xtab));
		memset(com_channel_xtab,0xff,sizeof(com_channel_xtab));
		memset(com_attrib,0x00,sizeof(com_attrib));
		memset((uchar*)&base_state,0x00,sizeof(base_state));
		memset((uchar*)&heart_beat,0x00,sizeof(heart_beat));
		if(0 == linktype)
			memset((uchar*)&base_info,0x00,sizeof(base_info));
		base_state.com_channel[0]=base_state.com_channel[1]=0xff;
		base_state.com_channel[2]=0xff;
		base_state.com_channel[3]=0xff;
		//--reset MODEM variables
		mdm_channel=0xff;
		mi_read=0;mi_write=0;mo_read=0;mo_write=0;
		base_state.mdm_status=0x0b;

		//--reset LAN variables
		lan_used=0;
		memset(lan_open,0x00,sizeof(lan_open));
		memset((uchar*)li_read, 0x00, sizeof(li_read));
		memset((uchar*)li_write, 0x00, sizeof(li_write));
		memset((uchar*)lo_read, 0x00, sizeof(lo_read));
		memset((uchar*)lo_write, 0x00, sizeof(lo_write));
		LinkComm_UartOpen(921600);
		io_start=GetTimerCount();//for API io timeout compensation during off base
	}
	flag = 1;
}

void LinkReset(void)
{
	//reset global variables
	task_p0=0;
	memset(&sys_state,0x00,sizeof(sys_state));
	memset(&cur_task,0x00,sizeof(cur_task));
}

static uchar step_recv_packet(uchar *packet,ushort *dlen)
{
	uchar tmpc;
	ushort tmpu,crc;
	ushort timeout;
	static uint i,t0,t1,want_dn;
	static uchar resp;

	if(1 == g_linktype)
	{
		timeout = TIMEOUT_RX_BT;
	}
	else if(0 == g_linktype)
	{
		timeout = TIMEOUT_RX;
		if(g_ExternalTimeFlag)
		{
			timeout = TIMEOUT_RX * 6;		// ò?D?U?ì?áo?ó?±è??3¤μ?ê±??
		}
	}
	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	}

STEP0:
	resp=0;
	i=0;
	*dlen=0;
	want_dn=100;
	t0=GetMs();

	task_p1=1;
STEP1:
	while(1)
	{			
		//--fetch one byte from the link port
		if(1 == g_linktype)
		{
			//tmpc = BtBaseRecv(packet + i,0);//deleted by shics
		//	tmpc = BtPortRecv(packet + i,0);
		}
		else if(0 == g_linktype)
		{
			tmpc = LinkCommRx(packet+i);
		}
		
		if(tmpc)
		{

			if(GetMs()-t0 > timeout){resp=1;break;}
			task_p1=1;
			return 0;
		}

		//--reset the timer start point
		t0=GetMs();
		
		if(packet[0]==0x02)i++;

		//--fetch the packet length info
		if(i==3)
		{
			want_dn=(packet[1]<<8)+packet[2];//pure data length
			if(want_dn>LINK_PAYLOAD)
			{
				i=0;
				continue;
			}
			want_dn+=8;//full packet size
		}

		if(i>=want_dn)
		{
			//--compare the CRC field
			crc=(packet[want_dn-2]<<8)+packet[want_dn-1];
			tmpu=Cal_CRC16_PRE(packet,want_dn-2,0);
			if(crc!=tmpu)resp=2;
			break;
		}
	}//while(1)

	*dlen=i;
	task_p1=0;
	return resp;
}

static uchar step_send_packet(uchar *packet,ushort dlen)
{
	uchar a;	

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	}

STEP0:		
	
	if(1 == g_linktype)
	{
	//	BtUserPortSends(packet,dlen);
		//BtBasePortSends(packet,dlen);//deleted by shics
	}
	else if(0 == g_linktype)
	{
		LinkCommReset();
		LinkCommTxs(packet,dlen); 
	}
	task_p1=1;
STEP1:	
	if(1 == g_linktype)
	{
		//a = BtBasePortTxPoolCheck(); //deleted by shics
	}
	else if(0 == g_linktype)
	{
		//a=LinkCommTxPoolCheck();//deleted by shics
	}
	if(a)return 0;
	task_p1=0;
	return 0;
}

static void pack_up(uchar seq,uchar *type,uchar *din,uchar *dout,ushort *dlen)
{
	ushort dn,tmpu;

		dn=*dlen;

		dout[0]=0x02;
		dout[1]=dn>>8;
		dout[2]=dn&0xff;
		dout[3]=seq;
		dout[4]=type[0];
		dout[5]=type[1];
		if(din)
			memcpy(dout+6,din,dn);

		tmpu=Cal_CRC16_PRE(dout,dn+6,0);
		dout[6+dn]=tmpu>>8;
		dout[6+dn+1]=tmpu&0xff;

		*dlen=dn+8;

	if(type[0] == M_OHCI)
		g_ExternalTimeFlag = 1;
	else
		g_ExternalTimeFlag = 0;
	return;
}

static int HandSetSoftIrq(void)
{
	uchar a,tmpc,tmps[LINK_PAYLOAD];
	ushort tmpu,i,j;
	int tmpd;
	int k;
//	static int loops=0;
	static uchar resp,type[2],tmp_pool[MODEM_TMP_POOL];
	static ushort n_module,n_pool,pools,n_segment,segments;
	static ushort tx_len,rx_len,want_len;
	static ST_IO_PACK out,in;
	
	switch(task_p0)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	case 20:goto STEP20;
	case 21:goto STEP21;
	case 22:goto STEP22;
	case 30:goto STEP30;
	case 31:goto STEP31;
	case 32:goto STEP32;
	case 40:goto STEP40;
	case 41:goto STEP41;
	case 42:goto STEP42;
	case 50:goto STEP50;
	case 51:goto STEP51;
	case 52:goto STEP52;
	}

STEP0:
	resp=0;
	task_p1=0;
//	loops++;
//ScrPrint(0,2,0,"LOOPS:%d ",loops);
//	if(loops%20)return 0;//500,100,20

	//1--process SYNC request loops
	while(cur_task.pending && !sys_state.need_reset)
	{
		//1.1--pack up for sending
		memset(&out,0x00,sizeof(out));
		type[0]=cur_task.module_no;
		type[1]=cur_task.func_no;
		tx_len=cur_task.dlen;
		pack_up(sys_state.cur_seq,type,cur_task.data,(uchar*)&out,&tx_len);

		//1.2--send the request packet to the base set
SYNC_SEND:
		task_p0=1;
STEP1:
		tmpc=step_send_packet((uchar*)&out,tx_len);
		if(!tmpc&&task_p1)return 0;
//ScrPrint(0,4,0,"%d,TN:%d,RESULT:%d ",loops,tx_len,tmpc);

		//1.3--receive the response packet from the base set
		memset(&in,0x00,sizeof(in));
		task_p0=2;
STEP2:
		tmpc=step_recv_packet((uchar*)&in,&rx_len);
		if(!tmpc&&task_p1)return 0;
//ScrPrint(0,5,0,"%d,RX RESULT:%d,%d ",loops,tmpc,rx_len);
		if(tmpc)//--failed to receive the response from the base set
		{
			task_p0=3;
STEP3:
			//--wait forever if it's off base
			//if((0 == g_linktype && OnBase()) || (1 == g_linktype && (1 != g_btlinkstatus || 1 != GetBtLinkEnable())))//deleted by shics
			{
				return 0;
			}
			goto SYNC_SEND;
		}

		//1.4--sequence no increments
		sys_state.cur_seq++;

		//1.5--verify the response packet
		if(in.seq!=out.seq){resp=2;goto LINK_RESET;}
		if(memcmp(in.type,out.type,2)){resp=3;goto LINK_RESET;}

		//1.6--dispatch the response packet to the related module
		rx_len=(in.len[0]<<8)+in.len[1];
		switch(in.type[0])
		{
		case M_COM:
			if(rx_len<2){resp=4;goto LINK_RESET;}
			ComProcess(in.type[1],rx_len,in.data);

			//--if port is opened ok,clear the com_io_buf
			if(in.type[1]==F_OPEN && !in.data[0])
			{
				//--return from base:attrib_string[20]+port_info[10]
				if(rx_len<2+sizeof(com_attrib[0])+10){resp=5;goto LINK_RESET;}

				//--check if it's opened already
				if(com_pool_xtab[cur_task.channel_no]<4)//colin modify
				{
					a=com_pool_xtab[cur_task.channel_no];
				}
				else
				{
					//--give a pool number to the current channel
					if(com_channel_xtab[0]==0xff)a=0;
					else if(com_channel_xtab[1]==0xff)a=1;
					else if(com_channel_xtab[2]==0xff)a=2;
					else if(com_channel_xtab[3]==0xff)a=3;
					else a=3;//if it's a system fault,use the last pool
				}

				com_pool_xtab[cur_task.channel_no]=a;
				com_channel_xtab[a]=cur_task.channel_no;
				memcpy(com_attrib[a],in.data+2,sizeof(com_attrib[0]));

				//--update status of all com ports
				memcpy(base_state.com_channel,in.data+2+sizeof(com_attrib[0]),4);
				memcpy(base_state.com_rx_has,in.data+2+sizeof(com_attrib[0])+4,8);
				memcpy(base_state.com_tx_free,in.data+2+sizeof(com_attrib[0])+12,8);

				ci_read[a]=0;ci_write[a]=0;co_read[a]=0;co_write[a]=0;
			}
			else if(in.type[1]==F_CLOSE)
			{
				a=com_pool_xtab[cur_task.channel_no];
				if(a<4)com_channel_xtab[a]=0xff;
				com_pool_xtab[cur_task.channel_no]=0xff;
			}
			else if(in.type[1]==10 && !in.data[0])//set heartbeat
			{
				memcpy(&heart_beat,out.data,sizeof(heart_beat));
			}
			break;
		case M_MODEM:
			ModemProcess(in.type[1],rx_len,in.data);

			//--if modem is opened ok,clear the modem_io_buf
			if(in.type[1]==F_OPEN)
			{
				if(!in.data[0])base_state.mdm_status=0x0a;
				else base_state.mdm_status=in.data[0];
				base_state.mdm_rx_has=0;
				base_state.mdm_tx_free=0;
				mdm_channel=cur_task.channel_no;//0-async,1-sync,2-FSK,bit d4-callee
				mi_read=0;mi_write=0;mo_read=0;mo_write=0;
				mdm_icon_dial=0;mdm_icon_link=0;
				mdm_fskget_enabled=0;//Added on 2015.12.2
			}
			else if(in.type[1]==F_CLOSE)
			{
				mdm_channel=0xff;//closed
				ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
				base_state.mdm_status=0x0b;
				mi_read=0;mi_write=0;//clear the handset rx_pool,2014.6.17
			}
			break;
		case M_LAN:
			TcpipProcess(in.type[1],rx_len,in.data);
			//--if LAN is opened ok,clear the lan_io_buf
			if((in.type[1]==F_OPEN || in.type[1]==F_NET_LISTEN) && !memcmp(in.data,"\x0\x0\x0\x0",4))
			{
				i=le_load(out.data);//socket id
				lan_open[i]=1;
				li_read[i]=0;li_write[i]=0;lo_read[i]=0;lo_write[i]=0;
				base_state.lan_rx_has[i]=0;
				base_state.lan_tx_free[i]=0;
				base_state.lan_event[i]=0;
				lan_used=1;
			}
			else if(in.type[1] == F_NET_ACCEPT)
			{
				tmpd = le_load(in.data);
				if(tmpd >= 0)
				{
					lan_open[tmpd]=1;
					li_read[tmpd]=0;li_write[tmpd]=0;lo_read[tmpd]=0;lo_write[tmpd]=0;
					base_state.lan_rx_has[tmpd]=0;
					base_state.lan_tx_free[tmpd]=0;
					base_state.lan_event[tmpd]=0;
					lan_used=1;
				}
			}
			else if(in.type[1]==F_CLOSE)
			{
				i=le_load(out.data);//socket id
				if(i>=10)break;
				lan_open[i]=0;

				for(i=0;i<10;i++)if(lan_open[i])break;
				if(i>=10)lan_used=0;
			}
			break;
		case M_PRINT:
			//if(rx_len<1){resp=5;goto LINK_RESET;}//added on2013.5.17
			//PrnProcess(in.type[1], rx_len, in.data);
			break;
		case M_SYS:
			SysProcess(in.type[1],rx_len,in.data);
			break;
		case M_OHCI:
			OhciProcess(in.type[1],rx_len,in.data);
			break;
		}//switch(module)

		//1.7--clear the task pending flag to enable the following task input
		cur_task.pending=0;
		break;
	}//while(1)

	//--process ASYNC request loops
	//2--perform base status check
	if(!sys_state.need_reset &&(mdm_channel<0x13 || com_channel_xtab[0]!=0xff || com_channel_xtab[1]!=0xff || 
		 com_channel_xtab[2]!=0xff || com_channel_xtab[3]!=0xff ||	lan_used) )
	{
		//2.1--pack up for sending
		memset(&out,0x00,sizeof(out));
		tx_len=0;
		type[0]=M_SYS;
		if(IsSupportBaseStateExt())
		{
			type[1] = 2;
		}
		else
		{
			type[1]=1;//get base pool state
		}
		pack_up(sys_state.cur_seq,type,NULL,(uchar*)&out,&tx_len);

		//2.2--send the request packet to the base set
QUERY_SEND:
		task_p0=20;
STEP20:
		tmpc=step_send_packet((uchar*)&out,tx_len);
		if(!tmpc&&task_p1)return 0;
//ScrPrint(0,4,0,"%d,TN:%d,RESULT:%d ",loops,tx_len,tmpc);

		//2.3--receive the response packet from the base set
		memset(&in,0x00,sizeof(in));
		task_p0=21;
STEP21:
		tmpc=step_recv_packet((uchar*)&in,&rx_len);
		if(!tmpc&&task_p1)return 0;
//ScrPrint(0,5,0,"%d,RX RESULT:%d,%d ",loops,tmpc,rx_len);
		if(tmpc)//--failed to receive the response from the base set
		{
			task_p0=22;
STEP22:
			//--wait forever if it's off base
			//if((0 == g_linktype && OnBase()) || (1 == g_linktype && (1 != g_btlinkstatus || 1 != GetBtLinkEnable())))//deleted by shics
			{
				return 0;
			}
			goto QUERY_SEND;
		}

		//2.4--sequence no increments
		sys_state.cur_seq++;

		//2.5--verify the response packet
		if(in.seq!=out.seq){resp=22;goto LINK_RESET;}
		if(memcmp(in.type,out.type,2)){resp=23;goto LINK_RESET;}

		if(IsSupportBaseStateExt())
		{
			if(rx_len<sizeof(base_state)){resp=24;goto LINK_RESET;}
		}
		else
		{	
			if(rx_len<(sizeof(base_state) - BASE_STATE_RESERVE_LEN -40 - 40)){resp=24;goto LINK_RESET;}
		}
//		if(rx_len<sizeof(base_state)){resp=24;goto LINK_RESET;}

//		memcpy(&base_state,in.data,sizeof(base_state));
		memcpy(&base_state,in.data,(in.len[0]<<8) + in.len[1]);
		if(base_state.com_channel[0]>=P_PORT_MAX && base_state.com_channel[0]!=0xff
		  || base_state.com_channel[1]>=P_PORT_MAX && base_state.com_channel[1]!=0xff
		  || base_state.com_channel[2]>= P_PORT_MAX && base_state.com_channel[2]!=0xff	
		  || base_state.com_channel[3]>= P_PORT_MAX && base_state.com_channel[3]!=0xff 
		  )
		  {resp=25;goto LINK_RESET;}//invalid parameter

		//--refresh the TELEPHONE icon
		if(mdm_channel!=0xff)
			switch(base_state.mdm_status)
		{
		case 0x0a:
			if(mdm_icon_dial)break;
			ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);
			mdm_icon_dial=1;
			break;
		case 0x00:
			if(mdm_icon_link)break;
			ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
			mdm_icon_link=1;
			break;
		case 0x01:
		case 0x08:
		case 0x09:
			break;
		default:
			ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);

			//--if it's a caller connection,stop modem status check
			//--mdm_channel:0-async,1-sync,2-FSK,bit d4-callee
			if(mdm_channel==1)//Added on 2014.5.15
			{
				if(!base_state.mdm_rx_has)mdm_channel=0xff;
			}
			else if(mdm_channel<0x10)mdm_channel=0xff;
			break;
		}//switch(modem_status)

	}//check base status

	//3--perform pool sending
	for(n_module=0;n_module<3 && !sys_state.need_reset;n_module++)
	{
		//--determine tx's pools and segments for a certain module
		switch(n_module)
		{
		case 0://COM
			pools=4;
			segments=1;
			break;
		case 1://MODEM
			//--if modem is not opened,skip over
			if(mdm_channel>=0x13)continue;

			//--skip over if none data to be sent
			if(mo_read==mo_write)continue;

			//--skip over if base is tx_busy
			if(!base_state.mdm_tx_free)continue;

			pools=1;
			if(!mdm_channel || mdm_channel==0x10)segments=1;
			else//0-async,1-sync,2-FSK
			{
				tmpu=(mo_write+MDM_IO_SIZE-mo_read)%MDM_IO_SIZE;
				segments=(tmpu+LINK_PAYLOAD)/LINK_PAYLOAD;
			}
			break;
		case 2://LAN			
			pools=10;
			segments=1;
			break;
		}

		for(n_pool=0;n_pool<pools;n_pool++)
		{
			for(n_segment=0;n_segment<segments;n_segment++)
			{
			//3.0--fetch data from the tx pool
			memset(&out,0x00,sizeof(out));
			tx_len = 0;
			switch(n_module)
			{
			case 0://fetch data from COM pool
				//--if a channel is not opened,skip over
				a=com_channel_xtab[n_pool];
				if(a==0xff)continue;

				//--if com tx pool is empty,skip over
				if(co_read[n_pool]==co_write[n_pool])continue;

				//--determine the current maximum tx length
				if(base_state.com_channel[0]==a)tmpu=base_state.com_tx_free[0];
				else if(base_state.com_channel[1]==a)tmpu=base_state.com_tx_free[1];
				else if(base_state.com_channel[2]==a)tmpu=base_state.com_tx_free[2];
				else if(base_state.com_channel[3]==a)tmpu=base_state.com_tx_free[3];
				else tmpu=0;//it's a system false
				if(tmpu>LINK_PAYLOAD-1)tmpu=LINK_PAYLOAD-1;//consider the channel_no space
				for(i=0;i<tmpu;i++)
				{
					if(co_read[n_pool]==co_write[n_pool])break;

					out.data[1+i]=co_pool[n_pool][co_read[n_pool]];
					co_read[n_pool]=(co_read[n_pool]+1)%COM_IO_SIZE;
				}//for(i)
				tx_len=i;

				type[0]=M_COM;
				out.data[0]=com_channel_xtab[n_pool];//1st byte is the channel_no
				tx_len+=1;//add the channel_no byte length
				type[1]=2;//func:send
				break;
			case 1://fetch data from MODEM pool
				//--determine the current maximum tx length
				tmpu=base_state.mdm_tx_free;
				if(tmpu>LINK_PAYLOAD)tmpu=LINK_PAYLOAD;
				for(i=0,j=mo_read;i<tmpu;i++)//modified on 2014.4.1
				{
					if(j==mo_write)break;

					out.data[i]=mo_pool[j];
					j=(j+1)%MDM_IO_SIZE;
				}//for(i)
				//--refresh mo_read only for ASYNC on 2014.6.18
				if(!(mdm_channel&0x0f))mo_read=j;

				tx_len=i;
				type[0]=M_MODEM;
				break;
			case 2://fetch data from LAN pool
			default:
				//--if a channel is not opened,skip over
				a=lan_open[n_pool];
				if(!a)continue;

				//--if lan tx pool is empty,skip over
				if(lo_read[n_pool]==lo_write[n_pool])continue;

				//--determine the current maximum tx length
				tmpu=base_state.lan_tx_free[n_pool];
				if(!tmpu)  continue;
				if(tmpu>LINK_PAYLOAD-1)tmpu=LINK_PAYLOAD-1;//consider the channel_no space
				for(i=0;i<tmpu;i++)
				{
					if(lo_read[n_pool]==lo_write[n_pool])break;

					out.data[1+i]=lo_pool[n_pool][lo_read[n_pool]];
					lo_read[n_pool]=(lo_read[n_pool]+1)%LAN_IO_SIZE;
				}//for(i)
				tx_len=i;

				type[0]=M_LAN;
				out.data[0]=n_pool;//1st byte is the channel_no
				tx_len+=1;//add the channel_no byte length
				type[1]=2;//func:send
				break;
			}//switch(n_module)

			//if(n_module!=1 && !tx_len)continue;

			//3.1--pack up for sending
			type[1]=2;//func:send
			pack_up(sys_state.cur_seq,type,out.data,(uchar*)&out,&tx_len);

			//3.2--send the request packet to the base set
ASYNC_TX_SEND:
			task_p0=30;
STEP30:
			tmpc=step_send_packet((uchar*)&out,tx_len);
			if(!tmpc&&task_p1)return 0;
//ScrPrint(0,4,0,"%d,TN:%d,%d,m:%d,tn:%d,p:%d ",loops,tx_len,tmpc,n_module,tx_len,n_pool);

			//3.3--receive the response packet from the base set
			memset(&in,0x00,sizeof(in));
			task_p0=31;
STEP31:
			tmpc=step_recv_packet((uchar*)&in,&rx_len);
			if(!tmpc&&task_p1)return 0;
//ScrPrint(0,5,0,"%d,RX:%d,%d ",loops,tmpc,rx_len);
			if(tmpc)//--failed to receive the response from the base set
			{
				task_p0=32;
STEP32:
				//--wait forever if it's off base
				//if((0 == g_linktype && OnBase()) || (1 == g_linktype && (1 != g_btlinkstatus || 1 != GetBtLinkEnable())))//deleted by shics
				{
					return 0;
				}
				goto ASYNC_TX_SEND;
			}

			//3.4--sequence no increments
			sys_state.cur_seq++;

			//3.5--verify the response packet
			if(in.seq!=out.seq){resp=30;goto LINK_RESET;}
			if(memcmp(in.type,out.type,2)){resp=31;goto LINK_RESET;}
			
			switch(n_module)
			{
			case 0:
				if(rx_len<12){resp=32;goto LINK_RESET;}//too less response
				if(in.data[0])
				{
					if(in.data[1]==P_BASE_HOST) 
					{
						if(in.data[0]!=4) //USB_ERR_BUF
						{
							base_xtab[P_BASE_HOST]=0xff;						
							com_pool_xtab[P_BASE_HOST]=0xff;							
							break;	
						}	
					}
					else if(in.data[1]==P_BASE_DEV) ;					
					else {resp=33;goto LINK_RESET;}//rejected by base,a system false					
				}
				//--update status of all com ports
				memcpy(base_state.com_channel,in.data+2,4);
				memcpy(base_state.com_rx_has,in.data+6,8);
				memcpy(base_state.com_tx_free,in.data+14,8);
				break;
			case 2:
				if(rx_len<4){resp=34;goto LINK_RESET;}//too less response
				tmpd=le_load(in.data);
				if(tmpd==-1) break;
				if(tmpd!=tx_len-9){resp=35;goto LINK_RESET;}//rejected by base,a system false
				break;
			case 1://modem
				//--refresh SYNC/FSK's mo_read,added on 2014.6.18
				if(mdm_channel&0x0f)mo_read=(mo_read+tx_len-8)%MDM_IO_SIZE;//packet envelope is 8 bytes
				
				//--refresh SYNC modem_status,added on 2014.6.18
				if((mdm_channel&0x0f)==1 && (!base_state.mdm_status || base_state.mdm_status==0x08))
					base_state.mdm_status|=0x01;

				break;
			}//switch(n_module)

			}//for(segment_no)
		}//for(channel_no)
	}//for(module_no)

	//4--perform pool receiving
	for(n_module=0;n_module<3 && !sys_state.need_reset;n_module++)
	{
		//--determine rx's pools and segments for a certain module
		switch(n_module)
		{
		case 0://COM
			pools=4;
			segments=1;
			break;
		case 1://MODEM
			//--if modem is not opened,skip over
			if(mdm_channel>=0x13)continue;
			if((mdm_channel==2 || mdm_channel==0x12) && !mdm_fskget_enabled)continue;//skip FSK over

			//--check if the base channel has data
			if(!base_state.mdm_rx_has)continue;//not any data to be received

			//--check if the handset ASYNC rx pool has free space size
			if((mdm_channel&0x0f)==1)tmpu=MDM_IO_SIZE-1;//2053 for sync,2014.6.18
			else tmpu=(mi_read+MDM_IO_SIZE-1-mi_write)%MDM_IO_SIZE;
			if(!tmpu)continue;

			pools=1;
			if(!mdm_channel || mdm_channel==0x10)segments=1;
			else//0-async,1-sync,2-FSK
			{
				//if(mi_read!=mi_write)continue;//deleted on 2014.5.16
				segments=3;
			}
			break;
		case 2://LAN
			pools=10;
			segments=1;
			break;
		}

		for(n_pool=0;n_pool<pools;n_pool++)
		{
			for(n_segment=0;n_segment<segments;n_segment++)
			{
			//4.0--fetch data from the base rx pool
			memset(&out,0x00,sizeof(out));
			switch(n_module)
			{
			case 0://fetch data from the base COM
				//--if a channel is not opened,skip over
				a=com_channel_xtab[n_pool];
				if(a==0xff)continue;

				//--check if the base channel has data
				if(base_state.com_channel[0]==a)tmpu=base_state.com_rx_has[0];
				else if(base_state.com_channel[1]==a)tmpu=base_state.com_rx_has[1];
				else if(base_state.com_channel[2]==a)tmpu=base_state.com_rx_has[2];
				else if(base_state.com_channel[3]==a)tmpu=base_state.com_rx_has[3];
				else tmpu=0;//it's a sytem false
				if(!tmpu)continue;//not any data to be received

				//--check the handset rx free space size
				tmpu=(ci_read[n_pool]+COM_IO_SIZE-1-ci_write[n_pool])%COM_IO_SIZE;
				if(tmpu>=LINK_PAYLOAD)tmpu=LINK_PAYLOAD;
				want_len=tmpu;
				type[0]=M_COM;
				out.data[0]=com_channel_xtab[n_pool];//channel_no
				out.data[1]=want_len>>8;//required data size
				out.data[2]=want_len&0xff;
				tx_len=3;
				break;
			case 1://fetch data from the base MODEM
				//--check the handset rx free space size
				if((mdm_channel&0x0f)==1)tmpu=MDM_IO_SIZE-1;//2053 for sync,2014.6.18
				else tmpu=(mi_read+MDM_IO_SIZE-1-mi_write)%MDM_IO_SIZE;
				if(tmpu>=LINK_PAYLOAD)tmpu=LINK_PAYLOAD;

				want_len=tmpu;
				type[0]=M_MODEM;
				out.data[0]=mdm_channel;//channel_no
				out.data[1]=want_len>>8;//required data size
				out.data[2]=want_len&0xff;
				tx_len=3;
				break;
			case 2://fetch data from the base LAN
			default:
				//--if a channel is not opened,skip over
				a=lan_open[n_pool];
				if(!a)continue;

				//--check if the base channel has data
				tmpu=base_state.lan_rx_has[n_pool];
				if(!tmpu)continue;//not any data to be received

				//--check the handset rx free space size
				tmpu=(li_read[n_pool]+LAN_IO_SIZE-1-li_write[n_pool])%LAN_IO_SIZE;
				if(tmpu>=LINK_PAYLOAD)tmpu=LINK_PAYLOAD;
				want_len=tmpu;
				type[0]=M_LAN;
				out.data[0]=n_pool;//channel_no
				out.data[1]=want_len>>8;//required data size
				out.data[2]=want_len&0xff;
				tx_len=3;
				break;
			}
			if(!want_len)continue;

			//4.1--pack up for receiving
			type[1]=3;//func:receive
		//	out.data[1]=want_len>>8;//required data size
		//	out.data[2]=want_len&0xff;
		//	tx_len=3;
			pack_up(sys_state.cur_seq,type,out.data,(uchar*)&out,&tx_len);

			//4.2--send the request packet to the base set
ASYNC_RX_SEND:
			task_p0=40;
STEP40:
			tmpc=step_send_packet((uchar*)&out,tx_len);
			if(!tmpc&&task_p1)return 0;
//ScrPrint(0,4,0,"%d,tn:%d,R:%d ",loops,tx_len,tmpc);
//printk("%d,tn:%d,R:%d.\n",loops,tx_len,tmpc);
			//4.3--receive the response packet from the base set
			memset(&in,0x00,sizeof(in));
			task_p0=41;
STEP41:
			tmpc=step_recv_packet((uchar*)&in,&rx_len);
			if(!tmpc&&task_p1)return 0;
//if(tmpc==1)ScrPrint(0,5,0,"%d,rx:%d,%d ",loops,tmpc,rx_len);
//else ScrPrint(0,6,0,"%d,rx:%d,%d ",loops,tmpc,rx_len);
//printk("  %d,rx:%d,%d.\n",loops,tmpc,rx_len);
//for(i=0;i<rx_len;i++)printk("%02X ",*((uchar*)&in+i));
//printk("\n");
			if(tmpc)
			{
				task_p0=42;
STEP42:
				//--wait forever if it's off base
				//if((0 == g_linktype && OnBase()) || (1 == g_linktype && (1 != g_btlinkstatus || 1 != GetBtLinkEnable())))//deleted by shics
				{
					return 0;
				}
				goto ASYNC_RX_SEND;
			}

			//4.4--sequence no increments
			sys_state.cur_seq++;

			//4.5--verify the response packet
			if(in.seq!=out.seq){resp=40;goto LINK_RESET;}
			if(memcmp(in.type,out.type,2)){resp=41;goto LINK_RESET;}
			rx_len=(in.len[0]<<8)+in.len[1];//pure data size
			if(n_module!=2)
			{
				if(rx_len<2){resp=42;goto LINK_RESET;}//too less response
				if(in.data[0])
				{
					if(n_module==1)
					{
						n_segment=segments;//modem line may be broken,added on 2013.7.18
						if((mdm_channel&0x0f)==2 && in.data[0]==0xff){resp=47;goto LINK_RESET;}//FSK,not supported by elder baseset
					}
					else
					{
						if(in.data[1]==P_BASE_HOST || in.data[1]==P_BASE_DEV) ;						
						else
						{
							resp=43;//rejected by base,a system false
							goto LINK_RESET;
						}	
					}
				}
				if(rx_len-2>want_len){resp=44;goto LINK_RESET;}//too more response
				rx_len-=2;//excluding the leading two bytes
			}
			else
			{
				if(rx_len<5){resp=45;goto LINK_RESET;}//too less response
				tmpd=le_load(in.data);
				if(tmpd<0){resp=46;goto LINK_RESET;}//rejected by base,a system false
				if(rx_len-5>want_len){resp=47;goto LINK_RESET;}//too more response
				rx_len-=5;//excluding the leading five bytes
			}

			//4.6--store the received data into rx pool
			switch(n_module)
			{
			case 0://store data into com_rx_pool
				if(in.data[1]!=com_channel_xtab[n_pool]){resp=48;goto LINK_RESET;}//invalid channel_no

				for(i=0;i<rx_len;i++)
				{
					ci_pool[n_pool][ci_write[n_pool]]=in.data[2+i];
					ci_write[n_pool]=(ci_write[n_pool]+1)%COM_IO_SIZE;
				}//for(i)
				break;
			case 1://store data into modem_rx_pool
				if(!mdm_channel || mdm_channel==0x10)//ASYNC
				{
					n_segment=segments;
					for(i=0;i<rx_len;i++)
						mi_pool[(mi_write+i)%MDM_IO_SIZE]=in.data[2+i];
					mi_write=(mi_write+rx_len)%MDM_IO_SIZE;
				}
				else//SYNC or FSK
				{
					memcpy(tmp_pool+n_segment*(LINK_PAYLOAD-2),in.data+2,rx_len);
					if(rx_len!=LINK_PAYLOAD-2)
					{
						rx_len+=n_segment*(LINK_PAYLOAD-2);

						//--clear sync rx_pool,shifted here on 2014.6.12
						if(mdm_channel==1 || mdm_channel==0x11)mi_write=mi_read;
						
						for(i=0;i<rx_len;i++)
							mi_pool[(mi_write+i)%MDM_IO_SIZE]=tmp_pool[i];
						mi_write=(mi_write+rx_len)%MDM_IO_SIZE;
						n_segment=segments;
					}
				}
				break;
			case 2://store data into lan_rx_pool
			default:
				if(in.data[4]!=n_pool){resp=49;goto LINK_RESET;}//invalid channel_no

				for(i=0;i<rx_len;i++)
				{
					li_pool[n_pool][li_write[n_pool]]=in.data[5+i];
					li_write[n_pool]=(li_write[n_pool]+1)%LAN_IO_SIZE;
				}//for(i)
				break;
			}//switch(module_no)
			}//for(segment_no)
		}//for(channel_no)
	}//for(module_no)
	

	if(!resp && !sys_state.need_reset)goto LINK_TAIL;
LINK_RESET:
		//ScrPrint(0,7,0,"RESET,RESP:%d,%02X ",resp,sys_state.cur_seq);	
		//5.1--pack up for sending with failure code
		memset(&out,0x00,sizeof(out));
		type[0]=M_SYS;
		type[1]=0;//reset base
		k=0;
		tmps[k++]=resp;//failure code
		tmps[k++]=com_channel_xtab[0];//com channel_no
		tmps[k++]=com_channel_xtab[1];//com channel_no
		tmps[k++]=com_channel_xtab[2];//com channel_no
		tmps[k++]=com_channel_xtab[3];//com channel_no
		memcpy(&tmps[k],com_attrib[0],20);
		k+=20;
		memcpy(&tmps[k],com_attrib[1],20);
		k+=20;
		memcpy(&tmps[k],com_attrib[2],20);
		k+=20;
		memcpy(&tmps[k],com_attrib[3],20);
		k+=20;
		memcpy(&tmps[k],&heart_beat,heart_beat.msg_len+6);
		k+=(heart_beat.msg_len+6);
		tx_len=k;	
		tmpu=LanGetConfig(tmps+tx_len);
		tx_len+=tmpu;

		pack_up(sys_state.cur_seq,type,tmps,(uchar*)&out,&tx_len);

		//5.2--send the reset packet to the base set
RESET_SEND:
		task_p0=50;
		LinkCommReset();		
STEP50:
		tmpc=step_send_packet((uchar*)&out,tx_len);
		if(!tmpc&&task_p1)return 0;
//ScrPrint(0,4,0,"%d,TN:%d,RESULT:%d ",loops,tx_len,tmpc);

		//5.3--receive the response packet from the base set
		memset(&in,0x00,sizeof(in));
		task_p0=51;
STEP51:
		tmpc=step_recv_packet((uchar*)&in,&rx_len);
		if(!tmpc&&task_p1)return 0;
//ScrPrint(0,5,0,"%d,RX RESULT:%d,%d ",loops,tmpc,rx_len);
		if(tmpc)//--failed to receive the response from the base set
		{
			task_p0=52;
STEP52:
			//--wait forever if it's off base
			//if((0 == g_linktype && OnBase()) || (1 == g_linktype && (1 != g_btlinkstatus || 1 != GetBtLinkEnable())))//deleted by shics
			{
				return 0;
			}
			goto RESET_SEND;
		}
		//5.4--sequence no increments
		sys_state.cur_seq++;
//ScrPrint(0,0,0,"%d,RESET ",sys_state.cur_seq);

		//5.5--verify the response packet
		if(in.seq!=out.seq){resp=50;goto STEP51; } //LINK_RESET;}
		if(memcmp(in.type,out.type,2)){resp=51;goto LINK_RESET;}

		//--finally clear the need_reset flag
		sys_state.need_reset=0;

LINK_TAIL:
	//if(resp)	//{	//ScrPrint(0,6,0,"%d,RESP:%d ",loops,resp);	}
	task_p0=0;
	return resp;
}

uchar link_send(uchar module,uchar func,uchar channel,uint data_len,uchar *data)
{
	uchar n,a;
	int tmpd;
	ushort tmpu;
	uint i,free_tx_size,want_len;
	
	//--process async API requests
	//--fetch pool_no
	n=com_pool_xtab[channel];
	switch(func)
	{
	case F_SEND:
		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return 100;

		switch(module)
		{
		case M_COM:
			//--check if the com tx pool is enough for writing
			free_tx_size=(co_read[n]+COM_IO_SIZE-1-co_write[n])%COM_IO_SIZE;
			if(data_len>free_tx_size)return 1;

			//--copy the data to the com io_pool
			for(i=0;i<data_len;i++)
				co_pool[n][(co_write[n]+i)%COM_IO_SIZE]=data[i];
			co_write[n]=(co_write[n]+data_len)%COM_IO_SIZE;
			break;
		case M_MODEM:
			//--check if the mdm tx pool is enough for writing
			free_tx_size=(mo_read+MDM_IO_SIZE-1-mo_write)%MDM_IO_SIZE;
			if(data_len>free_tx_size)return 1;

			//--copy the data to the mdm io_pool
			for(i=0;i<data_len;i++)
				mo_pool[(mo_write+i)%MDM_IO_SIZE]=data[i];
			mo_write=(mo_write+data_len)%MDM_IO_SIZE;
			break;
		case M_LAN:
			//--check if the lan tx pool is enough for writing
			n=channel;
			free_tx_size=(lo_read[n]+LAN_IO_SIZE-1-lo_write[n])%LAN_IO_SIZE;
			if(data_len>free_tx_size)return 1;

			//--copy the data to the lan io_pool
			for(i=0;i<data_len;i++)
				lo_pool[n][(lo_write[n]+i)%LAN_IO_SIZE]=data[i];
			lo_write[n]=(lo_write[n]+data_len)%LAN_IO_SIZE;
			break;
	  case M_PRINT:			
			break;
		}//switch(module)
		return 0;
	case F_RECV:
		switch(module)
		{
		case M_COM:
			//--check if the rx pool has data
			if(ci_read[n]==ci_write[n])
			{
				//--if off base,waiting or aborted according to ScrSetEcho()
				tmpd=OnbaseCheck();
				if(tmpd<0)return 101;
				return 2;
			}
			//--output one byte
			data[0]=ci_pool[n][ci_read[n]];
			ci_read[n]=(ci_read[n]+1)%COM_IO_SIZE;

			break;
		case M_MODEM:
			data[0]=0;
			data[1]=0;
			if(mi_read==mi_write)
			{
				//--if off base,waiting or aborted according to ScrSetEcho()
				tmpd=OnbaseCheck();
				if(tmpd<0)return 102;
			}
			if((mdm_channel&0x0f)==1)want_len=MDM_IO_SIZE-1;//2053 for sync
			else want_len=MDM_IO_SIZE-6;//2048 for async
			if((mdm_channel&0x0f)==1)s_HandSet_timer.handler=HandSetTimerInterruptNull;//disable IRDA's IRQ
			for(i=0;i<want_len;i++)
			{
				if(mi_read==mi_write)break;

				data[2+i]=mi_pool[mi_read];
				mi_read=(mi_read+1)%MDM_IO_SIZE;
			}//for(i)
			//--clear has_data flag of mdm_status,on 2014.6.25
			if(base_state.mdm_status==0x08 || base_state.mdm_status==0x09)base_state.mdm_status&=0xf7;
			if((mdm_channel&0x0f)==1)s_HandSet_timer.handler=HandSetTimerInterrupt;//enable IRDA's IRQ
			data[0]=i>>8;
			data[1]=i&0xff;
			break;
		case M_LAN:
			//--check if the rx pool has data
			n=channel;
			if(li_read[n]==li_write[n])
			{
				//--if off base,waiting or aborted according to ScrSetEcho()
				tmpd=OnbaseCheck();
				if(tmpd<0)return 103;
				return 3;
			}
			//--output one byte
			data[0]=li_pool[n][li_read[n]];
			li_read[n]=(li_read[n]+1)%LAN_IO_SIZE;
			break;
		}//switch(module)
		return 0;
	case F_GET://for ModemAsyncGet
		if(module!=M_MODEM)return 10;

		//--if not ASYNC/FSK,reject it
		if((mdm_channel&0x0f)!=0 && (mdm_channel&0x0f)!=2)return 1;
		//--if FSK,enable ASYNC_RX task of IrdaLinkIo.Added on 2015.12.2
		if((mdm_channel&0x0f)==2 && !mdm_fskget_enabled)mdm_fskget_enabled=1;

		//--if non data,reject it
		if(mi_read==mi_write)
		{
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)return 103;
			return 1;
		}
		data[0]=mi_pool[mi_read];
		mi_read=(mi_read+1)%MDM_IO_SIZE;
		return 0;
	case F_RESET:
		switch(module)
		{
		case M_COM:
			ci_read[n]=ci_write[n];
			break;
		case M_MODEM:
			mi_read=mi_write;
			break;
	  	case M_PRINT:
			break;
		}//switch(module)
		return 0;
	case F_CHECK://com tx_pool check,modem state check,lan state and event check
		if(module==M_MODEM)
		{
			//B0:modem status reported by baseset
			data[0]=base_state.mdm_status;
			
			//B1:rx data existence reported by baseset
			//--added on 2014.4.15 for rx after line-broken
			data[1]=0;
			if(base_state.mdm_rx_has)data[1]=1;

			//B2:rx data existence reported by handset
			//--added on 2014.5.23 for ModemCheck() call
			data[2]=0;
			if(mi_read!=mi_write)data[2]=1;

			//B3:SDLC's tx data occupied reported by handset
			//--added on 2014.5.23 for repeated calls of ModemTxd()
			data[3]=0;
			if((mdm_channel&0x0f)==1 && mo_read!=mo_write)data[3]=1;
		}

		//--if off base,waiting or aborted according to ScrSetEcho()
		tmpd=OnbaseCheck();
		if(tmpd<0)return 104;

		switch(module)
		{
		case M_COM:
			//--determine the index no in base_state.com_tx_free[2]
			if(base_state.com_channel[0]==channel)a=0;
			else if(base_state.com_channel[1]==channel)a=1;
			else if(base_state.com_channel[2]==channel)a=2;
			else if(base_state.com_channel[3]==channel)a=3;
			else {data[0]=0;break;}//not opened

			if(co_write[n]!=co_read[n] || base_state.com_tx_free[a]<COM_IO_SIZE-1)data[0]=1;//tx not finished yet
			else data[0]=0;
			break;
		case M_LAN:
			memcpy(data,base_state.lan_event,10);

			//--output current socket's count of tx_free and rx_has in handset
			n=channel;
			free_tx_size=(lo_read[n]+LAN_IO_SIZE-1-lo_write[n])%LAN_IO_SIZE;
			memcpy(data+10,&free_tx_size,4);
			tmpd=(li_write[n]+LAN_IO_SIZE-li_read[n])%LAN_IO_SIZE;
			memcpy(data+14,&tmpd,4);//rx_has

			if(IsSupportBaseStateExt())
			{
				tmpd = base_state.lan_accept_count[n];
				memcpy(data+18,&tmpd,4);
				tmpd = base_state.lan_err_value[n];
				memcpy(data+22,&tmpd,4);
			}
			break;
	  	case M_PRINT:			
			break;
		}//switch(module)
		return 0;
	case F_PEEP:
		if(module!=M_COM)return 10;

		data[0]=0;data[1]=0;
		if(ci_read[n]==ci_write[n])
		{
			//--if off base,waiting or aborted according to ScrSetEcho()
			tmpd=OnbaseCheck();
			if(tmpd<0)return 105;
		}
		want_len=data_len;
		for(i=0;i<want_len;i++)
		{
		//	if(ci_read[n]==ci_write[n])break;
			if(((ci_read[n] + i)%COM_IO_SIZE) == ci_write[n]) break;
			data[2+i]=ci_pool[n][(ci_read[n]+i)%COM_IO_SIZE];
		}//for(i)
		data[0]=i>>8;
		data[1]=i&0xff;
		return 0;
	case F_MDM_POLL_TX_POOL://check if MDM FSK TxPool of handset is busy
		if(module==M_MODEM)
		{
			if(mo_read!=mo_write)return 0x0a;//busy
			return 0;//TxPool is free
		}
		break;
	case F_MDM_START_RX:
		//--if ModemRxd() is called under FSK,disable its ASYNC_RX task.Added on 2015.12.2
		if(module==M_MODEM)mdm_fskget_enabled=0;
		break;
	}//switch(func)

	//--process sync API requests
	//--if off base,waiting or aborted according to ScrSetEcho()
	tmpd=OnbaseCheck();
	if(tmpd<0)return 106;	

	//--check if the task service is idle
	if(cur_task.pending)return 2;

	memset(&cur_task,0x00,sizeof(cur_task));
	cur_task.module_no=module;
	cur_task.func_no=func;
	cur_task.channel_no=channel;
	memcpy(cur_task.data,data,data_len);
	cur_task.dlen=data_len;

	//--set pending flag finally
	cur_task.pending=1;

	return 0;
}
#if 1 //added by shics
int OnbaseCheck(void)
{
return -12;
}
#else
int OnbaseCheck(void)
{
	uint status=0;
	int iRet;
	ST_BT_STATUS pStatus;

	if(1 == g_linktype)				// 蓝牙一对一链路
	{
		if(1 != GetBtLinkEnable())
		{
			return -12;
		}
		else if(0 == BtBaseGetStatus(&pStatus) && 1 == pStatus.status)
		{
			return 0;
		}
		else
		{
			return -11;
		}
	}
	//--if handset is off base,pop up the RETURN TO BASE message
	else if(0 == g_linktype && OnBase())
	{
		//--show RETURN message if prompt is enabled
		status=s_ScrEcho(1);
		if(0==status)//message is shown
		{
			while(OnBase());
			s_ScrEcho(0);//restore the screen before RETURN prompt			
			io_start=GetMs();//GetTimerCount();//reset base API io timeout
			return 1;
		}
		else if(0x01==status)//echo copy is busy
		{
			while(1)
			{
				status=s_ScrEcho(1);
				if(0==status)
				{
					while(OnBase());
					s_ScrEcho(0);

					io_start=GetMs();//GetTimerCount();//reset base API io timeout
					return 2;
				}
			}//while(1)
		}
		else if(OnBase()) return -10;//IRDA_ERR_OFFBASE;
	}
	else if(0xff == g_linktype)			// no base
	{
		return -12;
	}
	return 0;
}
#endif
void DisplayProtocolBusy(void)
{
	uint status=0;
	//--show RETURN message if prompt is enabled
	status=s_ScrEcho(1);
	if(0 == status)//message is shown
	{
	//	while(kbhit());
		getkey();
		s_ScrEcho(0);//restore the screen before RETURN prompt			
		io_start=GetMs();//GetTimerCount();//reset base API io timeout
	//	return 1;
	}
	else if(0x01==status)//echo copy is busy
	{
		while(1)
		{
			status=s_ScrEcho(1);
			if(0==status)
			{
			//	while(kbhit());
				getkey();
				s_ScrEcho(0);

				io_start=GetMs();//GetTimerCount();//reset base API io timeout
			//	return 2;
			}
		}
	}
}

uchar base_modem_occupied(void)
{
	//if(mdm_channel!=0xff)return 1;//modem is running
	switch(base_state.mdm_status)
	{
		case 0x0a:
		case 0x00:
		case 0x01:
		case 0x08:
		case 0x09:
			return 1;//modem is running
		default:
			return 0;//modem is free
	}
}

uchar close_modem(void)
{
	uchar pool_no;	
	
	//--close the channel in the com config table
	pool_no=com_pool_xtab[P_MODEM];
	if(pool_no<4)com_channel_xtab[pool_no]=0xff;
	com_pool_xtab[P_MODEM]=0xff;

	return 0;
}
//force the base link to perform a reset in order to close base ports
void force_close_com(uchar channel_no)
{
	uchar pool_no;

	//--close the channel in the com config table
	pool_no=com_pool_xtab[channel_no];
	if(pool_no<4)com_channel_xtab[pool_no]=0xff;
	com_pool_xtab[channel_no]=0xff;

	//--set the reset flag for irda_link
	sys_state.need_reset=1;
}

uint port_max_close_time(uchar channel)
{
	uchar n,tmps[10];
	uint tt,baudrate;

	//--get pool number
	n=com_pool_xtab[channel];
	if(n>=4)return 0;

	memcpy(tmps,com_attrib[n],6);
	tmps[6]=0;
	baudrate=atol(tmps)/10;	
	if(!baudrate)
	{
		if(channel==P_BASE_DEV  || channel==P_BASE_HOST) return 3000;
		else
			return 0;
	}	
	tt=COM_IO_SIZE*1000/baudrate+3000;//1000

	return tt;//uint:1ms
}

uchar is_hasbase(void)
{	
	if(GetHWBranch() != D210HW_V2x) return 0;
	if(1 == g_linktype)
	{
		return g_hasbase==HANDSET_BTLINK;
	}
	else if(0 == g_linktype)
	{
		return g_hasbase==HANDSET_UARTLINK;
	}
	else 
	{
		return 0;
	}
}

uchar ShowbaseCfgInfo(int type)
{
	char keyword[33],context[33];
	int i,j,total_num,str_len;
	int page;
	int i_per_page[20];
	uchar key;
	S_HARD_INFO *pitem=base_info.cfginfo;
	total_num =MAX_BASE_CFGITEM;
	
#define CFG_START_LINE 2
	
	i=0;
	page=0;
	i_per_page[0]=0;
	key = KEYDOWN;

RE_DISP_LAST_PAGE1:
	j=CFG_START_LINE;
	for(i=i_per_page[page];i<total_num;i++)
	{
		if(CFG_START_LINE==j) i_per_page[page]=i;
		if(type != pitem[i].type) continue;
		str_len=strlen(pitem[i].keyword)+strlen(pitem[i].context);
		if(str_len<=20)
		{
			ScrPrint(0,j,0,"%s:%s",pitem[i].keyword,pitem[i].context);
			j++;
		}
		else
		{
			ScrPrint(0,j,0,"%s:",pitem[i].keyword);
			ScrPrint(0,j+1,0,"%s",pitem[i].context);  //?úèYDD′óoóíù?°??ê?
			j+=2;
		}
		
		if(j>6)
		{
			key=getkey();
			ScrClrLine(2,7);
			j=CFG_START_LINE;        //?úò3oó￡?′ó?eê?DD?aê???ê?
			if(KEYUP ==key )
			{
				page--;
				if(page<0)return key;
				goto RE_DISP_LAST_PAGE1;
			}
			else if(KEYMENU ==key ||KEYCLEAR ==key || KEYCANCEL == key)	return key;
			else		++page;
		}

	}
	if(CFG_START_LINE==j)
	{	
		if(i_per_page[0]==total_num-1)
			return getkey();

		return key;
		//return KEYDOWN; //×?oóò?ò3ò??-μè′yá?°′?üê±￡?2??ùμè′y°′?ü	

	}	
	key=getkey();
	if(KEYUP ==key)
	{
		page--;
		if(page<0) return key;
		ScrClrLine(2,7);
		goto RE_DISP_LAST_PAGE1;
	}
	return key;
}

// 获取座机bt相关的信息 F_GET_BASE_BT
int GetBaseBT(void)
{
	unsigned char tmpc,type[2],ptr;
	unsigned short tx_len;
	static unsigned char bufack[128],step;
	static ST_IO_PACK	out;
	static uint t0;
	int index;

	switch(step)
	{
		case 0: goto basebt0;break;
		case 1: goto basebt1;break;
		case 2: goto basebt2;break;
		case 3: goto basebt3;break;
	}
	
	basebt0:
		memset(bufack,0,sizeof(bufack));
		memset(&out,0,sizeof(out));
		ptr = 0;
		LinkCommReset();
		step = 1;
		return -1;

	basebt1:
		type[0] = 0x05;
		type[1] = F_GET_BASE_BT;
		tx_len = 0;
		pack_up(0,type,NULL,(uchar *)&out,&tx_len);
		LinkCommTxs((uchar *)&out,tx_len);
		step = 2;
		return -2;

	basebt2:
		if(LinkCommTxPoolCheck())	return -3;
		t0 = GetMs();
		step = 3;
		return -3;
		
	basebt3:
		while(1)
		{
			if(bufack[0] != 0x02) ptr = 0;
			if(LinkCommRx(bufack+ptr++)) break;
			if(ptr>=43+8) break;
		}
		if((ptr >= 43+8) || ((GetMs() - t0) > 1000))
		{
			step = 0;
			if(bufack[4] == 0x05 && bufack[5] == F_GET_BASE_BT)
			{
				index = 6;
				base_info.bt_exist = bufack[index];
				index++;
				memcpy(base_info.bt_mac,&bufack[index],6);
				index += 6;
				memcpy(base_info.bt_pincode,&bufack[index],16);
				index += 16;
				memcpy(base_info.bt_name,&bufack[index],20);
				return 0;
			}
		}
		return -4;
}

void dobasecheck(void)
{
	static int init_flag=0;
	static int step=0;
	static int t0=0,t1=0;
	static uchar bufack[10];
	static uchar ptr=0;
	ushort tx_len;
	uchar type[2];
	int i;
	uchar *pbuf;
	static ST_IO_PACK out;	
	// 如果确定有了座机
	if(0xff != g_linktype || (HANDSET_UARTLINK == g_hasbase || HANDSET_BTLINK == g_hasbase)) return;
	if(init_flag) return;
	if(GetHWBranch() != D210HW_V2x) return;	
	if(OnBase()) 
	{		
		t0=GetTimerCount();		
		return;
	}
	if(GetTimerCount()-t0<200) return;	
	
	switch(step)
	{
		case 0: goto basestep0;break;
		case 1: goto basestep1;break;
		case 2: goto basestep2;break;
		case 3: goto basestep3;break;
		//default: step=0; break;
	}
	step=0;
	return;
	
basestep0:
	memset(bufack,0,sizeof(bufack));
	memset(&out,0x00,sizeof(out));
	ptr=0;
	LinkComm_UartStop();
	LinkComm_UartOpen(921600);
	LinkCommReset();
	step=1;
	return;

basestep1:	
	type[0]=M_BASECHECK;
	type[1]=TEST_DATA;
	tx_len=0;
	pack_up(0,type,NULL,(uchar*)&out,&tx_len);
	LinkCommTxs((uchar*)&out,tx_len);
	step=2;
	return;
	
basestep2:
	if(LinkCommTxPoolCheck())	return;
	t1=GetTimerCount();	
	step=3;
	return;
	
basestep3:	
	while(1)
	{		
		if(bufack[0]!=0x02) ptr=0;
		if(LinkCommRx(bufack+ptr++))  break;		
		if(ptr>=8)  break;
	}		
	if((ptr>=8) || ((GetTimerCount()-t1)>1000) )
	{	
		step = 0;		
		if(bufack[4]==M_BASECHECK)
		{
			if(bufack[5]==TEST_DATA)  
			{
				init_flag = 1;  //??3?μ?μ××ù
				g_hasbase = HANDSET_CHARGEONLY;
			}	
			if(bufack[5]==TEST_DATA_ACK)    //óD×ù?ú
			{
				init_flag = 2;
				s_HandSetInit(0);
				g_hasbase = HANDSET_UARTLINK;
				g_linktype = 0;
			}		
		}		
	}	
	return;	
}

void CheckBtLink(void)
{
#if 0 //deleted by shics
	ST_BT_STATUS pStatus;
	memset(&pStatus,0,sizeof(ST_BT_STATUS));
	if(0 == BtBaseGetStatus(&pStatus))
	{
		if(1 == pStatus.status)		//连上了座机的蓝牙
		{
			if(!g_btlinkstatus || !g_btlinkedflag)
			{
			//	BtPortOpen();
				BtBasePortOpen();
				g_btlinkstatus = 1;						// 将蓝牙标志置位
				g_btlinkedflag = 1;						// 将蓝牙连接过标志置位
				memcpy((uchar *)&gBtStatus,(uchar *)&pStatus,sizeof(ST_BT_STATUS));
			}
		}
		else if(g_btlinkstatus)			// 链路断开了
		{
			g_btlinkstatus = 0;
		}
	}
	#endif
}

#if 1 //shics deleted
void base_task(void)
{
}
#else//shics deleted
void base_task(void)
{
	static uchar su8SetPin,su8FirstConn,su8BtStatus;
	ST_BT_CONFIG pCfg;
	ST_BT_SLAVE	 pSlave;
	ST_BT_STATUS pStatus;
	int iRet;
	static uchar linkbak,switchlinkflag;
	static int hasbaseback;
	
	while(!g_taskbase)	OsSleep(2);

	while(1)
	{
		OsSleep(10);		
		dobasecheck();
		if(is_hasbase())
			GetBaseVerBack();

		#if 0
		// 只有获取了座机信息之后才会获取座机蓝牙信息,并且是在没连接蓝牙的情况下
		if(g_GetBaseInfoFlag && !g_btlinkstatus && 1 == g_linktype)			
		{
			if(OnBase() && g_OnBaseStatus)			//座机被拿开了
			{
				g_OnBaseStatus = 0;
				ClearBaseBtInfo();						// 清除保存的座机信息
			}
			else if(!OnBase() && !g_OnBaseStatus)		// 座机刚被放上
			{
				if(!GetBaseBT())
				{
					g_OnBaseStatus = 1;
				}
			}
		}
		#endif

		if(GetBtLinkEnable())			// 只有在在使能了链路之后			
		{
			CheckBtLink();
		}
		else							// 使能开关关闭
		{
			// 主动断开蓝牙之后会将使能标志复位，需要重置连接状态
			if(!(0 == BtBaseGetStatus(&pStatus) && pStatus.status == 1))
			{
				if(g_btlinkstatus)
				{
					g_btlinkstatus = 0;
				}
			}
			if(g_btlinkedflag)
			{
				g_btlinkedflag = 0;				// 如果使能开关被关闭过则不满足自动重连的条件了
			}
		}
		// 如果是蓝牙链路 蓝牙被连接过 但是蓝牙断开了  需要自动重连
		if(g_linktype && g_btlinkedflag && !g_btlinkstatus)
		{
			if(0 == BtBaseGetStatus(&pStatus) && 0 == pStatus.status)
			{
				memcpy(&pSlave.mac,gBtStatus.mac,6);
		//		BtConnect(&pSlave);
				BtBaseReconnect(&pSlave);
			}
		}
	}
}
#endif
void s_basecheckinit(void)
{
	int t0;
	if(GetHWBranch() == D210HW_V2x )	
	{	
		if(OnBase())
		{
			g_OnBaseStatus = 0;				// 座机不在位
		}
		else
		{
			g_OnBaseStatus = 1;
		}
		s_ModemInit_Proxy(0);
		g_taskbase = OsCreate((OS_TASK_ENTRY)base_task,TASK_PRIOR_BASE,0x1000);
		if(!OnBase())
		{
			t0=GetTimerCount();
			while((GetTimerCount()-t0)<1500)
			{
				if(g_hasbase != HANDSET_NOBASE) 	break;				
			}
		}
		if(g_hasbase == HANDSET_UARTLINK || g_hasbase == HANDSET_BTLINK)
		{
			s_RouteSetDefault(0);
			s_RefreshIcons();
		}
	}
}

void s_BaseBtLinkEnable(int value)
{
	int iRet;
	uint t0;

	if(GetHWBranch() != D210HW_V2x) return ;
 
	if(0 != value && 1 != value) return ;
	//if(value && !GetBtLinkEnable())//deleted by shics
	return;//added by shics
	{
		//CheckBtLink();//deleted by shics
	}
	if(value)				 	// 使能手座机蓝牙链路
	{
		if(g_btlinkstatus && g_linktype != 0)		// 如果蓝牙连接上了的话且当前不是串口链路切换到蓝牙链路
		{
			g_linktype = 1;
			g_hasbase = HANDSET_BTLINK;
			//BtBasePortOpen();//deleted by shics
			// 切换api
			s_HandSetInit(1);		// switch bt link	
			//SetBtLinkEnable(value);//deleted by shics
		}
	}
	else
	{
		// 如果是关闭使能的话需要将
		if(1 == g_linktype)
		{
			//BtPortClose();//deleted by shics
		}
		//SetBtLinkEnable(value);//deleted by shics
	}

	// 等待手座机交互完成
	if(value && g_btlinkstatus)
	{
		t0 = GetTimerCount();
		while(!g_GetBaseInfoFlag)		// 没有获取到蓝牙座机的信息
		{
			if(GetTimerCount() - t0 > 500)
			{
				break;
			}
		}
	}
}

uchar GetBtLinkStatus(void)
{
	return g_btlinkstatus;
}

uchar SetLinkedFlag(uchar linkedflag)
{
	g_btlinkedflag = linkedflag;
}

uchar GetLinkedFlag(void)
{
	return g_btlinkedflag;
}

void SetBaseType(uchar basetype)
{
	g_hasbase = basetype;
}

int GetBaseType(void)
{
	int basetype;
	basetype = g_hasbase;
	return basetype;
}

void SetLinkType(uchar linktype)
{
	g_linktype = linktype;
}

uchar GetLinkType(void)
{
	uchar linktype;
	linktype = g_linktype;
	return linktype;
}
#if 1//shics
int CreateHandsetBaseLink(int index)
{
	return 0;
}
#else //deleted by shics
int CreateHandsetBaseLink(int index)
{
	uchar szDispBuf[50],ucRet;
	int iRet,iLoop;
	ST_BT_SLAVE		pSlave;
	ST_BT_CONFIG	pCfg;
	ST_BT_STATUS	pStatus;
	volatile uint 	t0;
	Menu	tCreateLinkMenu[3];
	// 创建蓝牙链路
	memset(tCreateLinkMenu,0,sizeof(tCreateLinkMenu));
	if(0 == index)
	{
		AddToMenu(&tCreateLinkMenu[0],0,0,0,0x00c1,"MODEM测试","MODEM TEST",NULL);
	}
	else
	{
		AddToMenu(&tCreateLinkMenu[0],0,0,0,0x00c1,"以太网测试","ETHERNET TEST",NULL);
	}
	AddToMenu(&tCreateLinkMenu[1],0,0,0,0x1001,"使能蓝牙链路...","ENABLE BT LINK...",NULL);
	ScrCls();
	iShowMenu(tCreateLinkMenu,2,0,NULL);
	memset(&pSlave,0,sizeof(ST_BT_SLAVE));
	iRet = BtOpen();
	if(iRet)
	{
		memset(szDispBuf,0,sizeof(szDispBuf));
		sprintf(szDispBuf,"BtOpen Failed : %d",iRet);
		AddToMenu(&tCreateLinkMenu[2],0,0,0,0x1000,szDispBuf,szDispBuf,NULL);
		ScrCls();
		iShowMenu(tCreateLinkMenu,3,0,NULL);
		getkey();
		BtClose();
		return 1;
	}
	memset(&pCfg,0,sizeof(pCfg));
	iRet = BtGetConfig(&pCfg);
	if(iRet)
	{
		memset(szDispBuf,0,sizeof(szDispBuf));
		sprintf(szDispBuf,"BtGetConfig Failed : %d",iRet);
		AddToMenu(&tCreateLinkMenu[2],0,0,0,0x1000,szDispBuf,szDispBuf,NULL);
		ScrCls();
		iShowMenu(tCreateLinkMenu,3,0,NULL);
		getkey();
		BtClose();
		return 2;
	}
	if(memcmp(pCfg.pin,base_info.bt_pincode,16))
	{
		memcpy(pCfg.pin,base_info.bt_pincode,16);
		iRet = BtSetConfig(&pCfg);
		if(iRet)
		{
			memset(szDispBuf,0,sizeof(szDispBuf));
			sprintf(szDispBuf,"BtSetConfig Failed : %d",iRet);
			AddToMenu(&tCreateLinkMenu[2],0,0,0,0x1000,szDispBuf,szDispBuf,NULL);
			ScrCls();
			iShowMenu(tCreateLinkMenu,3,0,NULL);
			getkey();
			BtClose();
			return 3;
		}
	}
	memcpy(pSlave.mac,base_info.bt_mac,6);
	iRet = BtConnect(&pSlave);
	if(iRet)
	{
		memset(szDispBuf,0,sizeof(szDispBuf));
		sprintf(szDispBuf,"BtConnect Failed : %d",iRet);
		AddToMenu(&tCreateLinkMenu[2],0,0,0,0x1000,szDispBuf,szDispBuf,NULL);
		ScrCls();
		iShowMenu(tCreateLinkMenu,3,0,NULL);
		getkey();
		BtClose();
		return 4;
	}
	AddToMenu(&tCreateLinkMenu[2],0,0,0,0x1000,"连接中...","Connectting...",NULL);
	ScrCls();
	iShowMenu(tCreateLinkMenu,3,0,NULL);
	t0 = GetMs();
	while(1)
	{
		if(kbhit() == 0 && getkey() == KEYCANCEL)		// 按下了取消键
		{
			BtClose();
			return 5;
		}
		if(GetMs() - t0 > 18000)			// timeout
		{
			AddToMenu(&tCreateLinkMenu[2],0,0,0,0x1000,"连接超时","Connect Timeout",NULL);
			ScrCls();
			iShowMenu(tCreateLinkMenu,3,0,NULL);
			BtClose();
			getkey();
			return 5;
		}
		memset(&pStatus,0,sizeof(pStatus));
		if(BtGetStatus(&pStatus) == 0 && pStatus.status == 1)
		{
			AddToMenu(&tCreateLinkMenu[2],0,0,0,0x1000,"蓝牙链路成功","Bt Link Success",NULL);
			ScrCls();
			iShowMenu(tCreateLinkMenu,3,0,NULL);
			SetBtLinkEnable(1);
			BtPortOpen();
			getkey();
			break;
		}
	}
	return 0;
}
#endif
#if 1
void RestoreHandsetBaseLink(void)
{
}
#else //deleted by shics
void RestoreHandsetBaseLink(void)
{
	ST_BT_STATUS	pStatus;
	int iRet;
	memset(&pStatus,0,sizeof(pStatus));
	iRet = BtGetStatus(&pStatus);
	if(0 == iRet && pStatus.status == 1)
	{
		BtDisconnect();
		BtClose();
	}
}
#endif
// 根据座机的版本号来判断是否支持获取座机状态的扩展命令
int IsSupportBaseStateExt(void)
{
	if(memcmp(base_info.ver_soft,"3.03",4) < 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

