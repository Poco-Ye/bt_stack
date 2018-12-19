#include  <stdio.h>
#include  <string.h>
#include  "base.h"
#include  "..\comm\comm.h"
#include  "..\kb\kb.h"
#include  "modem.h"
#include  "../lcd/LcdDrv.h"
//#include  "pf1_5BD.h"
//#include  "pf206e_2.h"
#include  "pf2_061e.h"
//#include "SI_P9_36BD.H"
#include  "si_p10_ec25.h"
//#define MDM_TEST
//#define printk x_debug


//static const uchar DRV_VER[]="86A\0";
//static const uchar DRV_MAKE_DATE[]="2012.07.16\0";
//static const uchar DRV_VER[]="87A\0";
//static const uchar DRV_MAKE_DATE[]="2013.06.09\0";
//static const uchar DRV_VER[]="87B\0";
//static const uchar DRV_MAKE_DATE[]="2013.10.29\0";
//static const uchar DRV_VER[]="87C\0";
//static const uchar DRV_MAKE_DATE[]="2013.12.25\0";
//static const uchar DRV_VER[]="87D\0";
//static const uchar DRV_MAKE_DATE[]="2014.01.08\0";
static const uchar DRV_VER[]="88E\0";
static const uchar DRV_MAKE_DATE[]="2016.11.01\0";

//86A-120305,first edition from old S80 with ZA9L0
//   -120620,changed patch from pf206e to pf206e_1 for dial tone threshold decreasing
//           added initialization of auto_reply_way in ModemDial()
//           added $F2 for 2400bps async fast connect
//           Minimized pbx_pause to 1 second for dial string
//           deleted some useless old codes
//   -120703,moved second dial tone detect AT commands
//   -120714,added auto patch load before dial
//87A-Added support for Si2457D,
//   -Modified patch from pf206e_1 to pf206e_2 for W dial code,Deleted two second dial tone detect AT settings
//   -Modified LED and signal ICON flash control
//   -Enlarged silicon S7 minimum from 5 to 6
//   -Changed FSK callee &D2 to &D3 in order to hangup reliably
//87B-改动点:
//   -加入应答音门限手动设置
//   -加入发送电平的手动设置
//   -对同步9600应答音检测门限做了限制(10MS~230MS)
//87C-Updated patch SI_P9_36BD.H to si_p10_ec25.h for async V.42bis disconnection in long transfer
//   -Added +IPR=115200 in si_callee_setup() to fix baudrate for FSK
//   -Modified references to port_ppp_serve[]
//87D-Added set_icon_dial() for color LCD
//88E-Added PortStop call for repeated ModemDial()
//    Restored sn_retries limit from 5 to 2 for VoIP lines
//    主叫同步1200/2400/9600,当接收到SDLC I帧的P/F=0时，I帧有效 
//    Updated patch from pf206e_2.h to pf2_061e.h to solve the invalid FF frames' emitting
//    Added support for SDLC V.29 fallback
//    Decreased the rx timeout of SDLC V.29 from 4s to 1s
//    Revised for Si2457D 9600 SDLC's not hangoff upon DTR switch-off

//extern uchar u_pool[];
//extern ushort u_rn;

//driver
typedef struct{
	int pwr_port;
	int pwr_pin;
	int reset_port;
	int reset_pin;
	int dcd_port;
	int dcd_pin;
	int cts_port;
	int cts_pin;
	int rts_port;
	int rts_pin;
	int dtr_port;
	int dtr_pin;
}T_ModemDrvConfig;

static T_ModemDrvConfig *ptModemDrvConfig = NULL;

T_ModemDrvConfig SxxModemDrvConfig = {
	.pwr_port = GPIOD,
	.pwr_pin = 8,
	.reset_port = GPIOD,
	.reset_pin = 7,
	.dcd_port = GPIOD,
	.dcd_pin = 6,
	.cts_port = GPIOB,
	.cts_pin = 5,
	.rts_port = GPIOD,
	.rts_pin = 4,
	.dtr_port = GPIOD,
	.dtr_pin = 5,
};
T_ModemDrvConfig SxxxModemDrvConfig = {
	.pwr_port = GPIOB,
	.pwr_pin = 11,
	.reset_port = GPIOB,
	.reset_pin = 10,
	.dcd_port = GPIOB,
	.dcd_pin = 9,
	.cts_port = GPIOB,
	.cts_pin = 5,
	.rts_port = GPIOD,
	.rts_pin = 4,
	.dtr_port = GPIOD,
	.dtr_pin = 5,
};
void s_ModemDrvInit(void)
{
	int mach = get_machine_type();
	if (mach == S300 || mach == S800 || mach == S900)
	{
		ptModemDrvConfig = &SxxxModemDrvConfig;
	}
	else
	{
		ptModemDrvConfig = &SxxModemDrvConfig;
	}
}


extern volatile ushort port_ppp_serve[3];

extern void watch_usb_interrupt(void);//in usbdev.c,serves the usb enumeration and re-attach
extern void  ScrSetIcon(unsigned char icon_no,unsigned char mode);

ulong ticks;
ulong line_bps;

static MODEM_CONFIG modem_config;
static ushort modem_running,patch_loaded,lcd_type;
static uchar mdm_chip_type,modem_status,task_p0,idle_watch,is_half_duplex;
static uchar is_sync,dial_only,has_picked,check_flag,asyn_form,frame_address;
static uchar auto_answer,answer_interface,auto_reply_way;
static ushort task_p1,task_p2,task_load_step;
static volatile ushort req_write,resp_write,resp_head;
static ulong tm00_remains,tm01_remains,tm02_remains,tmm_remains;
static uchar str_phone[256],ptr_phone;
static uchar ci_pool[FRAME_SIZE*3],sdlc_tx_pool[FRAME_SIZE*3];
static uchar resp_pack[PACK_SIZE+10];
static uchar frame_pool[FRAME_SIZE];
static uchar request_pool[FRAME_SIZE];
static uchar response_pool[FRAME_SIZE+10];
static uchar modem_name[20]="**\0";
static volatile uchar PowerOff_flag = 0; //0表示从不下电，1表示挂机时下电
static uchar const si_tx_level_tab[]={0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5};
static ushort const si_answer_tone_tab[]={0,20,100,230,360};//0,10ms,20ms,30ms,40ms
static struct
{
	ushort cmd_count;
	ushort connect_wait;
    uchar manual_adjust_tx_level;//菜单中手动调整发送电平,取值1~15.为0表示关闭手动调整
    uchar manual_adjust_answer_tone;//菜单中应答音门限值，取值1~255.为0表示关闭手动调整	
	uchar at_cmd[100][102];
}ex_cmd;

static struct
{
	uchar ptr;
	uchar clen;
	ushort on_10ms[256];
	ushort off_10ms[256];
}dial_info;

static struct
{
  ushort interrupt_count;
  ushort want_flashes;
  ushort count_on;
  ushort count_off;
  uchar open;
  uchar first_on;
  uchar last_on;
  uchar is_led_online;
  uchar is_led_txd;
  uchar counted;
  ushort callee_skips;
  uchar callee_lighted;
}led_control;

uchar s_ModemInit(uchar mode);//0--first step,1--last step,2--both steps
void s_ModemInfo(uchar *drv_ver,uchar *mdm_name,uchar *last_make_date,uchar *others);
void ModemPowerOff(void);

uchar ModemDial(COMM_PARA *MPara, uchar *TelNo, uchar mode);
uchar ModemCheck(void);
uchar ModemTxd(uchar *SendData,ushort len);
uchar ModemRxd(uchar *RecvData,ushort *len);
uchar ModemReset(void);
uchar OnHook(void);
uchar ModemAsyncGet(uchar *ch);
ushort ModemExCommand(uchar *CmdStr,uchar *RespData,ushort *Dlen,ushort Timeout10ms);
uchar modem_occupied(void);

static uchar cx_step_load_patch(void);
static uchar (*step_load_patch)(void);
static void hang_off(void);
static void interrupt_timer7(void);
static uchar (*caller_connect)(void);
static uchar cx_caller_connect(void);
static uchar cx_step_turn_to_rx(void);
static uchar cx_step_turn_to_tx(void);
static uchar (*step_turn_to_rx)(void);
static uchar (*step_turn_to_tx)(void);
static uchar caller_sync_comm(void);
static uchar step_send_to_serial1(uchar *str,ushort dlen);
static uchar step_rcv_v80_cmd(uchar long_wait,ushort *dlen);
static uchar step_send_to_smodem(uchar *str,ushort *dlen);
static uchar send_to_smodem(uchar *str,ushort *dlen);
static void pack_sdlc_v80(uchar ctrl,uchar *in_data,ushort *dlen);
static uchar async_comm(void);
static void set_icon_dial(uchar on_off);
void led_auto_flash(uchar led_no,uchar count,uchar first_on,uchar last_on);
static uchar cx_callee_setup(void);
static uchar (*callee_setup)(void);
static void store_response(uchar *str,ushort dlen);
static uchar cx_callee_connect(void);
static uchar (*callee_connect)(void);
static void nac_pack_up(uchar *in_str,ushort *dlen);
static uchar callee_sync_comm(void);
static uchar cx_v23hdx_txd(uchar *tx_data,ushort len);
static uchar cx_v23hdx_rxd(uchar *rx_data,ushort *len);
static uchar (*v23hdx_txd)(uchar *tx_data,ushort len);
static uchar (*v23hdx_rxd)(uchar *rx_data,ushort *len);

//--subroutines for SILICON modem
uchar two_one(uchar *in,uchar pairs,uchar *out);
static uchar si_caller_connect(void);
static uchar si_step_turn_to_rx(void);
static uchar si_step_turn_to_tx(void);
static uchar si_v23hdx_txd(uchar *tx_data,ushort len);
static uchar si_v23hdx_rxd(uchar *rx_data,ushort *len);
static uchar si_callee_setup(void);
static uchar si_callee_connect(void);
static uchar si_step_load_patch(uchar work_mode);

uchar mdm_adjust_tx_level(uchar en_manual_set,uchar level_value)
{
    uchar tmpc,a;
    uchar tmps[50];
    ushort rlen;
    
    a=0;
    if(en_manual_set)ex_cmd.manual_adjust_tx_level = level_value;//level_value不能为0
    else             
    {
        ex_cmd.manual_adjust_tx_level = 0;
    
        if(modem_name[0]!='S'||(rGPD_DIN&0x100))return 0;  
    
        PortStop(P_MODEM);
        tmpc=PortOpen(P_MODEM,"115200,8,n,1");
        if(tmpc)
        {
            a=1;
            goto tail_level;
        }
    
	    memset(tmps,0,sizeof(tmps));
	    strcpy(tmps,"AT:U52,0000\r");
	    tmpc=send_to_smodem(tmps,&rlen);
	    if(tmpc)a=2;
    
tail_level:    
        PortStop(P_MODEM);
    }
    return a;
}

uchar mdm_adjust_answer_tone(uchar en_manual_set,uchar value)
{
    uchar tmpc,a;
    uchar tmps[50];
    ushort rlen;  

    a=0;
    if(en_manual_set)ex_cmd.manual_adjust_answer_tone = value;
    else
    {
        ex_cmd.manual_adjust_answer_tone = 0;
        
        if(modem_name[0]!='S'||(rGPD_DIN&0x100))return 0;      
        PortStop(P_MODEM);
        tmpc=PortOpen(P_MODEM,"115200,8,n,1");
        if(tmpc)
        {
            a=1;
            goto tail_answer;
        }
    
	    memset(tmps,0,sizeof(tmps));
	    strcpy(tmps,"AT*Y254:W44F3,0032;:U1DA,78\r");
	    tmpc=send_to_smodem(tmps,&rlen);
	    if(tmpc)a=2;
    
tail_answer: 
        PortStop(P_MODEM);        
    }
    return a;
}

void s_ModemConfig(int value)
{
	if(value != 0 && value != 1) return;
	PowerOff_flag = value;
}

uchar modem_occupied(void)
{
	return modem_running;
}

void s_ModemInfo_std(uchar *drv_ver,uchar *mdm_name,uchar *last_make_date,uchar *others)
{
	if(drv_ver)strcpy(drv_ver,DRV_VER);
	if(last_make_date)strcpy(last_make_date,DRV_MAKE_DATE);
	if(mdm_name)strcpy(mdm_name,modem_name);
	if(others)sprintf(others,"V%s %s %s",DRV_VER,modem_name,DRV_MAKE_DATE);
}

uchar s_ModemInit_std(uchar mode)//0--first step,1--last step,2--both steps
{
	static uint t0;
	struct platform_timer_param tm_set;
	struct platform_timer *ptr_tm;
	long tmpl;
	uchar tmpc,a,resp,tmps[200],xstr[300];
	ushort i,rlen,tn;

	s_ModemDrvInit();
	
	if(mode==1)goto STEP1;

	ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);

	//--power on timer7,close its interrupt,enable OVIC
	rDMU_PWD_BLK2&=0xffffbfff;//power on tim3,d14:dmu_pwd_tim3 including timer 6 and 7
	rVIC1_INT_ENCLEAR=0x8000;//disable timer7 interrupt,d15 of SVIC1
	rPMB_CONTROL&=0xfffffffd;//d1:block open interrupts

	 //1--set relevant GPIOs
	 
	 //1.0--disable GPIO_IENs
	//1.1--set GPIO_ENs
	//1.2--set GPIO_OEs
	SET_MDM_PWR_OUTPUT_EN;// set PWR as output
	SET_MDM_RESET_OUTPUT_EN;//set RESET as output
    SET_MDM_RTS_OUTPUT_EN;//set RTS as output
    SET_MDM_DTR_OUTPUT_EN;//set DTR as output
    SET_PWR(LOW_LEVEL);// set PWR on
    CONFIG_RELAY_PORT;
	//--set DCD and CTS as OUTPUT temporarily for SILICON reset sampling
	SET_MDM_CTS_OUTPUT_EN;//set CTS as output
    SET_MDM_DCD_OUTPUT_EN;//set DCD as output
   
    SET_CTS(HIGH_LEVEL);// set CTS output high

	//1.3--set GPIO_OUTs
    SET_DCD(HIGH_LEVEL);
    SET_RESET(HIGH_LEVEL);
    SET_RTS(HIGH_LEVEL);
    SET_DTR(HIGH_LEVEL);
    
	SET_LED_IO;
	LED_TXD_OFF;
	LED_RXD_OFF;
	LED_ONLINE_OFF;

	//2--reset modem

    SET_RESET(LOW_LEVEL);//set RESET as low level
    DelayMs(30);
    SET_RESET(HIGH_LEVEL);//set RESET as high level
	//--return to input for CTS and DCD
	SET_MDM_CTS_INPUT_EN;
    SET_MDM_DCD_INPUT_EN;
    
	t0=GetTimerCount();
	if(!mode)return 0;

STEP1:
	//--wait for 2.5seconds elapsing
	ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
	while(GetTimerCount()-t0<=800);//800ms,at least 25ms for CONEXANT,300ms for SILICON

	ticks=0;
	tm00_remains=tm01_remains=tm02_remains=tmm_remains=0;
	task_p0=TASK_IDLE;
	modem_status=0x0b;
	mdm_chip_type=0;
	has_picked=0;
    patch_loaded=0;

	resp=0;
	memset(&ex_cmd,0x00,sizeof(ex_cmd));

    if(!SysParaRead(MODEM_LEVEL_INFO, tmps))
    {
        //为了和APP保持兼容，电平0dBm略去，只支持1~15范围调整
        if(tmps[0])ex_cmd.manual_adjust_tx_level = tmps[1]-'0';//如果菜单手动设置电平使能了则tmp[0]>0;如果禁止则tmp[1]==0        
    }

	 if(!SysParaRead(MODEM_ANSWER_TONE_INFO, tmps))
	 {
		  //应答音门限制的取值范围:1~255
		  if(tmps[0])ex_cmd.manual_adjust_answer_tone= atol(tmps+1);//如果菜单手动设置应答音门限值使能了则tmp[0]>0;如果禁止则tmp[1]==0
	 }

	patch_loaded=0;
	caller_connect=cx_caller_connect;
	step_turn_to_rx=cx_step_turn_to_rx;
	step_turn_to_tx=cx_step_turn_to_tx;
	v23hdx_txd=cx_v23hdx_txd;
	v23hdx_rxd=cx_v23hdx_rxd;
	callee_setup=cx_callee_setup;
	callee_connect=cx_callee_connect;
	step_load_patch=cx_step_load_patch;
	//3--set timer7 for modem task
	tm_set.timeout=10000;            	/* uint is 1us */
	tm_set.mode=TIMER_PERIODIC;			/*timer periodic mode. TIMER_PERIODIC or TIMER_ONCE*/
	tm_set.handler=interrupt_timer7;    /* interrupt handler function pointer */
	tm_set.interrupt_prior=INTC_PRIO_7;	/*timer's interrupt priority*/
	tm_set.id=TIMER_MDM;            		/* timer id number */
	ptr_tm=platform_get_timer(&tm_set);
	platform_start_timer(ptr_tm);

	//4--open modem port
	modem_running=0;
	tmpc=PortOpen(P_MODEM,"115200,8,n,1");
	if(tmpc)
	{
		modem_status=0xf0;
		ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
		return (10+tmpc);//ports occupied
	}

	//5--check modem
	memset(tmps,0,sizeof(tmps));
	strcpy(tmps,"ATE0I3\r");
	tmpc=send_to_smodem(tmps,&rlen);
//printk("RESULTc:%d,RN:%d,%s.\r\n",tmpc,rlen,tmps);
	if(tmpc)
	{
		resp=20+tmpc;
		goto I_TAIL;
	}

	//--search for SILICON chip
	//SILICON is:18F
	if(!tmpc)
	{
		for(i=0;i<rlen;i++)
			if(tmps[i]=='1')break;
		a=i;

		if(!memcmp(tmps+a,"18F",3))
		{
			strcpy(tmps,"ATI6I0\r");
			tmpc=send_to_smodem(tmps,&rlen);//\r\n2457 or \r\n2434
//printk("result:%d,RN:%d,%s.\r\n",tmpc,rlen,tmps);
			if(tmpc)
			{
				resp=100+tmpc;
				goto I_TAIL;
			}

			if(!memcmp(tmps+2,"2457",4))mdm_chip_type=2;
			else if(!memcmp(tmps+2,"2434",4))mdm_chip_type=3;

			modem_name[0]='S';
			modem_name[1]='i';
			memcpy(modem_name+2,tmps+2,4);
			modem_name[6]=tmps[10];
			modem_name[7]=0;
//printk("name:%s,type:%d.\r\n",modem_name,mdm_chip_type);
			if(mdm_chip_type)led_auto_flash(N_LED_RXD,mdm_chip_type,1,0);

			//--replace the differing functions
			caller_connect=si_caller_connect;
			step_turn_to_rx=si_step_turn_to_rx;
			step_turn_to_tx=si_step_turn_to_tx;
			v23hdx_txd=si_v23hdx_txd;
			v23hdx_rxd=si_v23hdx_rxd;
			callee_setup=si_callee_setup;
			callee_connect=si_callee_connect;
			step_load_patch=si_step_load_patch;
			goto I_TAIL;
		}
	}

	//--search for CONEXANT chip
	//CONEXANT HARLEY2 is:CX93001-EIS_V0.2013-V92CX93001-EIS_V0.2013-V92
	if(!tmpc)
	{
		for(i=0;i<rlen;i++)
			if(tmps[i]=='C')break;
		a=i;

		if(!memcmp(tmps+a,"CX9300",6))mdm_chip_type=2;

		memcpy(modem_name,tmps+a,7);
		modem_name[7]=0;

		for(i=a+8;i<rlen;i++)
			if(tmps[i]=='-')
			{
				memcpy(modem_name+7,tmps+i,4);
				modem_name[11]=0;
				break;
			}
	}
	if(mdm_chip_type)led_auto_flash(N_LED_RXD,mdm_chip_type,1,0);

I_TAIL:
	PortStop(P_MODEM);

//	Ninit();
//	Portinit("*");

	ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	ModemPowerOff();
	PowerOff_flag=0;
	return resp;
}

void ModemPowerOff(void)
{
    SET_PWR(1);//set MODEM_POWER high
	//--set all MODEM IOs to low
	SET_MDM_CTS_OUTPUT_EN;
    SET_MDM_DCD_OUTPUT_EN;
	SET_CTS(0);
	SET_RESET(0);
	SET_DCD(0);//set CTS RESET DCD low
    SET_DTR(0);
	SET_RTS(0);//set DRT,RTS low
    
	//--configure UART0,d20:UART0 TX,d19:UART0 RX
	gpio_set_mpin_type(GPIOA,0x180000,GPIO_OUTPUT);
	gpio_set_mpin_val(GPIOA,0x180000,0);
}

static uchar cx_step_load_patch(void)
{
	static uint t0;
	static uchar a,tmps[200],xstr[300];
	static ushort i,rlen,tn;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	case 4:goto STEP4;
	case 5:goto STEP5;
	case 6:goto STEP6;
	case 7:goto STEP7;
	case 8:goto STEP8;
	default:return 100;
	}

STEP0:
	task_p2=0;

	//--clear the patch_loaded flag
	patch_loaded=0;
	ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);

	//--power on timer7,enable OVIC
	rDMU_PWD_BLK2&=0xffffbfff;//power on tim3,d14:dmu_pwd_tim3 including timer 6 and 7
	rPMB_CONTROL&=0xfffffffd;//d1:block open interrupts

	 //1.0--disable GPIO_IENs
	//1.1--set GPIO_ENs
	//1.2--set GPIO_OEs

	SET_MDM_PWR_OUTPUT_EN;// set PWR as output
	SET_MDM_RESET_OUTPUT_EN;//set RESET as output
    SET_MDM_RTS_OUTPUT_EN;//set RTS as output
    SET_MDM_DTR_OUTPUT_EN;//set DTR as output
    SET_PWR(LOW_LEVEL);// set PWR on
	//--set DCD and CTS as OUTPUT temporarily for SILICON reset sampling
	SET_MDM_CTS_OUTPUT_EN;//set CTS as output
    SET_MDM_DCD_OUTPUT_EN;//set DCD as output
   
    SET_CTS(HIGH_LEVEL);// set CTS output high

	//1.3--set GPIO_OUTs
    SET_DCD(HIGH_LEVEL);
    SET_RESET(HIGH_LEVEL);
    SET_RTS(HIGH_LEVEL);
    SET_DTR(HIGH_LEVEL);
    
	SET_LED_IO;
	LED_TXD_OFF;
	LED_RXD_OFF;
	LED_ONLINE_OFF;

	//2--reset modem
	SET_RESET(LOW_LEVEL);//set RESET as low level
	//DelayMs(30);//at least 15ms for CONEXANT,5ms for SILICON
	t0=GetTimerCount();
	task_p1=1;
STEP1:
	while(GetTimerCount()-t0<=30)
	{
		return 0;
	}
	SET_RESET(HIGH_LEVEL);//set RESET as high level

	//--return to input for CTS and DCD
	SET_MDM_CTS_INPUT_EN;//set CTS as input
    SET_MDM_DCD_INPUT_EN;//set DCD as input

	//--wait for 2.5seconds elapsing
	ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
	t0=GetTimerCount();
	task_p1=2;
STEP2:
	while(GetTimerCount()-t0<=800)//800ms,at least 25ms for CONEXANT,300ms for SILICON
	{
		return 0;
	}

	strcpy(tmps,"ATE0\r");
	task_p1=3;
STEP3:
	a=step_send_to_smodem(tmps,&rlen);
	if(!a&&task_p2)return 0;
//printk("t4:%d,rn:%d,%s\r",a,rlen,tmps);
	if(a)
	{
		modem_status=0xe0;
		return 1;
	}

	//--request to load patch
	strcpy(tmps,"AT**\r");
	task_p1=4;
STEP4:
	a=step_send_to_smodem(tmps,&rlen);
	if(!a&&task_p2)return 0;
//printk("t5:%d,rn:%d\r",a,rlen);
	if(a)
	{
		modem_status=0xe1;
		return 2;
	}

	//--send patch codes
	tn=sizeof(patch_code_h2)/sizeof(patch_code_h2[0]);
	for(i=0;i<tn;i++)
	{
		rlen=strlen(patch_code_h2[i]);
		if(rlen>78)rlen=78;

		memcpy(tmps,patch_code_h2[i],rlen);
		if(rlen==78)
		{
			tmps[rlen]='\r';
			rlen++;
		}
		tmps[rlen]=0;
		task_p1=5;
STEP5:
		a=step_send_to_smodem(tmps,&rlen);
		if(!a&&task_p2)return 0;
//printk("t6:%d,rn:%d\r",a,rlen);
		if(a)
		{
			modem_status=0xe2;
			return 3;
		}

		//DelayMs(1);//1,2
		//--delay for 10ms once 10 loops
		if(i && !(i%10))
		{
			t0=GetTimerCount();
			task_p1=6;
STEP6:
			while(GetTimerCount()-t0<=10)
			{
				return 0;
			}
		}
	}//for(i)

	tmps[0]=0;
	tmps[2]='X';
	task_p1=7;
STEP7:
	a=step_send_to_smodem(tmps,&rlen);
	if(!a&&task_p2)return 0;
//printk("t7:%d,rn:%d,%s\r",a,rlen,tmps);
	if(a)
	{
		modem_status=0xe3;
		return 4;
	}

	//--set country code in order to validate DTMF level change
	strcpy(tmps,"ATE0+GCI=B5\r");//+GCI is not influenced by &F and Z
	task_p1=8;
STEP8:
	a=step_send_to_smodem(tmps,&rlen);
	if(!a&&task_p2)return 0;
//printk("t8:%d,rn:%d,%s\r",a,rlen,tmps);
	if(a)
	{
		modem_status=0xe4;
		return 5;
	}

	ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	patch_loaded=1;//set the patch_loaded flag
	task_p1=0;

	return 0;
}

static uchar si_step_load_patch(uchar work_mode)
//work_mode:0--idle,1-work
{
	static uint t0;
	static uchar a,tmps[200],xstr[300];
	static ushort i,rlen,tn;

	switch(task_load_step)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	case 4:goto STEP4;
	default:return 100;
	}

STEP0:
	task_p2=0;

	//--open modem port if it's unopen,added on2016.10.21
	if(PortTxPoolCheck(P_MODEM)==0x03)
	{
		a=PortOpen(P_MODEM,"115200,8,N,1");
		if(a)return 1;
	}
	//--clear the patch_loaded flag
	patch_loaded=0;
	ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);

	//--power on timer7,enable OVIC
	rDMU_PWD_BLK2&=0xffffbfff;//power on tim3,d14:dmu_pwd_tim3 including timer 6 and 7
	rPMB_CONTROL&=0xfffffffd;//d1:block open interrupts

	 //1.0--disable GPIO_IENs
	//1.1--set GPIO_ENs
	//1.2--set GPIO_OEs
	SET_MDM_PWR_OUTPUT_EN;//set POWER as output
	SET_MDM_RESET_OUTPUT_EN;//set RESET as output
	SET_MDM_RTS_OUTPUT_EN;//set RTS as output
	SET_MDM_DTR_OUTPUT_EN;//set DTR as output
	SET_PWR(LOW_LEVEL);//set POWER low for power-on

	//--set DCD and CTS as OUTPUT temporarily for SILICON reset sampling
	SET_MDM_CTS_OUTPUT_EN;//set CTS as output
	SET_MDM_DCD_OUTPUT_EN;//set DCD as output
	SET_CTS(HIGH_LEVEL);//set CTS high

	//--restore UART0 function,added on 2016.11.1
	writel_or(0x180000, GIO0_R_GRPF0_AUX_SEL_MEMADDR);

	//1.3--set GPIO_OUTs
	//set RESET,DCD,RTS,DTR high
	SET_RESET(HIGH_LEVEL);
	SET_DCD(HIGH_LEVEL);
	SET_RTS(HIGH_LEVEL);
	SET_DTR(HIGH_LEVEL);

	SET_LED_IO;
	LED_TXD_OFF;
	LED_RXD_OFF;
	LED_ONLINE_OFF;

	//2--reset modem
	SET_RESET(LOW_LEVEL);//set RESET low
	//DelayMs(30);//at least 15ms for CONEXANT,5ms for SILICON
	t0=GetTimerCount();
	task_load_step=1;
STEP1:
	while(GetTimerCount()-t0<=30)
	{
		return 0;
	}
	SET_RESET(HIGH_LEVEL);//set RESET high

	//--return to input for CTS and DCD
	SET_MDM_CTS_INPUT_EN;//set CTS as input
	SET_MDM_DCD_INPUT_EN;//set DCD as input

	//--wait for modem powering on
	ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
	t0=GetTimerCount();
	task_load_step=2;
STEP2:
	while(GetTimerCount()-t0<=300)//at least 25ms for CONEXANT,300ms for SILICON
	{
		return 0;
	}

	//--clear rx buffer,added on2016.10.31
	PortReset(P_MODEM);
	strcpy(tmps,"ATE0\r");
	task_load_step=3;
STEP3:
	a=step_send_to_smodem(tmps,&rlen);
	if(!a&&task_p2)return 0;
//printk("si:%d,rn:%d,%s",a,rlen,tmps);
	if(a)
	{
		if(work_mode)modem_status=0xe0;
		return 2;
	}

	//--send patch codes
	tn=sizeof(patch_code_si)/sizeof(patch_code_si[0]);
	for(i=0;i<tn;i++)
	{
		rlen=strlen(patch_code_si[i]);
		if(rlen>45)rlen=45;

		memcpy(tmps,"AT",2);
		memcpy(tmps+2,patch_code_si[i],rlen);
		tmps[2+rlen]=0;
		strcat(tmps,"\r");
		task_load_step=4;
STEP4:
		a=step_send_to_smodem(tmps,&rlen);
		if(!a&&task_p2)return 0;
//printk("%d,si:%d,rn:%d\n",i,a,rlen);
		if(a)
		{
			if(work_mode)modem_status=0xe1;
			return 3;
		}
	}//for(i)
//printk("PATCH ELAPSED %d.\n",GetTimerCount()-t0);

L_TAIL:
	ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	patch_loaded=1;//set the patch_loaded flag
	task_load_step=0;

	return 0;
}
uchar ModemDial_std(COMM_PARA *MPara, uchar *TelNo, uchar mode)
{
	uchar n,format[30];
	ushort i,rlen,xlen;
	uint t0;
	MODEM_CONFIG *mdm_cfg;
	static uchar need_reset=0;

	mdm_cfg=(MODEM_CONFIG*)MPara;

	DISABLE_MDM_INT;//disable modem timer interrupt

	if(task_p0==TASK_CALLER_DIAL || task_p0==TASK_CALLER_DATA)
	{
		hang_off();
		modem_status=0x0b;
		task_p0=TASK_IDLE;
		modem_running=0;//added on 2016.10.31
		PortStop(P_MODEM);
		goto M_TAIL;
	}

	if(task_p0==TASK_CALLEE_WAIT || task_p0==TASK_CALLEE_DATA)
		hang_off();

	if(mdm_cfg)
		memcpy(&modem_config,mdm_cfg,sizeof(modem_config));
	else
	{
		modem_config.mode=0;
		modem_config.ignore_dialtone=0;
		modem_config.dial_wait=0;
		modem_config.pbx_pause=10;
		modem_config.code_hold=70;
		modem_config.code_space=8;
		modem_config.pattern=0x02;
		modem_config.dial_times=1;
		modem_config.idle_timeout=6;//*10s
		modem_config.async_format=0;
	}

	rlen=strlen(TelNo);
	if(rlen>255)rlen=255;
	memset(str_phone,'.',sizeof(str_phone));
	memcpy(str_phone,TelNo,rlen);
	ptr_phone=0;
	dial_only=0;
	if(rlen>=2)
	{
		for(i=0;i<rlen-1;i++)
			if(str_phone[i]=='.')break;
		if(i<rlen-1 && str_phone[i+1]=='.')
			dial_only=1;
	}

	idle_watch=0;
	if(modem_config.pattern&0x80)is_sync=0;
	else is_sync=1;

	check_flag=0;
	has_picked=0;
	auto_answer=0;

	task_p1=0;
	task_p2=0;
	task_load_step=0;//2016.10.31
	modem_status=0x0a;
	frame_address=0x30;

	//4--open modem port
	if(is_sync)
	{
		modem_config.async_format=0;//clear it to avoid FSK process
		strcpy(format,"115200,8,n,1");
	}
	else
	{
		//Format:0-N81,1-E81,2-O81,3-N71,4-E71,5-O71,8-N81B202_FSK,9-N81V23C_FSK
		n=modem_config.async_format&0x0f;
		if(n==3)
		{
			hang_off();
			modem_status=0xfe;//unsupported parameter
			task_p0=TASK_IDLE;
			goto M_TAIL;
		}
		strcpy(format,"115200,");
		if(n>7)n=0;

		if(n>=3&&n<=5)strcat(format,"7,");
		else strcat(format,"8,");

		if(n==1 || n==4)strcat(format,"e,");
		else if(n==2 || n==5)strcat(format,"o,");
		else strcat(format,"n,");

		strcat(format,"1");

		//--assign a idle timeout for V23HDX for safety
		n=modem_config.async_format&0x0f;
		if((n==0x08 || n==0x09) && !modem_config.idle_timeout)
			modem_config.idle_timeout=90;//maximum 15 minutes
	}

	//----if callee answers
	if(modem_config.ignore_dialtone & 0x02)
	{
		hang_off();

		//--reset the modem
		if(need_reset)
		{
			n=0;
			//n=s_ModemInit(2);//V82D added,not required for CX93001
			if(n)
			{
				if(modem_status!=0xf0)modem_status=0xfa;//modem reset failed
				return modem_status;
			}
			modem_status=0x0a;
		}

		auto_answer=1;
		if(!is_sync && modem_config.ignore_dialtone&0x20)
		{
			answer_interface=1;//remote download interface
			tm00_remains=200;
		}
		else answer_interface=0; //normal modem interface

		//--added on 20120503
		if(is_sync && modem_config.ignore_dialtone&0x08)auto_reply_way=2;
		else auto_reply_way=0;

		need_reset=1;//V82C added
		task_p0=TASK_CALLEE_WAIT;//start callee_detect_ring task
	}
	else
	{
		need_reset=1;//V82C added
		task_p0=TASK_CALLER_DIAL;//start caller_dial task
	}

		modem_running=0;
		n=PortOpen(P_MODEM,format);
		if(n)
		{
			modem_status=0xf0;//ports occupied
			task_p0=TASK_IDLE;
			goto M_TAIL;
		}

		//--start timer7 interrupt
		ENABLE_MDM_INT;//enable modem timer interrupt
		modem_running=0xffff;

		if(!mode)return 0;

		t0=GetTimerCount();//300s
		while(11)
		{
			 if(modem_status!=0x0a)break;

			 if(!kbhit() && getkey()==KEYCANCEL)
			 //if(!s_kbhit() && s_getkey()=='x')//NEED
			 {
				  DISABLE_MDM_INT;//disable modem timer interrupt

				  hang_off();
				  modem_status=0xfd;
				  task_p0=TASK_IDLE;
				  modem_running=0;
				  PortStop(P_MODEM);

				  ENABLE_MDM_INT;//enable modem timer interrupt
				  break;
			 }

			 if(GetTimerCount()-t0>300000)
			 {
				 DISABLE_MDM_INT;//disable modem timer interrupt

				 hang_off();
				 modem_status=0xf9;//modem task inactive
				 task_p0=TASK_IDLE;
				 modem_running=0;
				 PortStop(P_MODEM);

				 ENABLE_MDM_INT;//enable modem timer interrupt
				 break;
			 }
		}

M_TAIL:
		n=modem_status;

		//--start timer1 interrupt
		ENABLE_MDM_INT;//enable modem timer interrupt

		if(n==0x08)return 0;
		return n;
}

uchar ModemCheck_std(void)
{
	uchar cc;

	cc=modem_status;
	if(check_flag==1)
	{
		if(cc==0x08)cc=0x00;
		check_flag=2;
	}
		//if(dial_only&&has_picked)cc=0x06;//is sending dial number

	return cc;
}

static void hang_off(void)
{
	SET_DTR(1);
	SET_RTS(1);//set RTS,DTR high
    RELAY_ON;
	//--switch off MODEM's PPP UART i/o channel
	//if(port_ppp_serve[1]==1)port_ppp_serve[1]=0;
	//if(port_ppp_serve[2]==1)port_ppp_serve[2]=0;

	led_control.open=0;
	LED_TXD_OFF;
	LED_RXD_OFF;
	LED_ONLINE_OFF;
	ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	set_icon_dial(S_ICON_NULL);
	 //--if Silicon V.29FC,power off to hang off because it cannot hang off
	if((modem_config.pattern&0xe0)==0x40 && modem_name[0]=='S')
	 {
	 	 if(has_picked)ModemPowerOff();
		 task_load_step=0;
	 }

	 if(has_picked)
	 {
		  tmm_remains=300;
		  has_picked=0;
	 }
}

uchar OnHook_std(void)
{
	DISABLE_MDM_INT;//disable modem timer interrupt

	hang_off();
	task_p0=TASK_IDLE;
	modem_status=0x0b;
	if(auto_answer)auto_answer=0;//forced to ORIGINATE
	modem_running=0;
	ex_cmd.cmd_count=0;
	ex_cmd.connect_wait=0;
	PortStop(P_MODEM);

	ENABLE_MDM_INT;//enable modem timer interrupt
	if (PowerOff_flag) ModemPowerOff();
	return 0;
}

ushort ModemExCommand_std(uchar *CmdStr,uchar *RespData,ushort *Dlen,ushort Timeout10ms)
{
	 uchar tmps[110],*ptr_c,a;
	 ushort dn,i;

	 if(ex_cmd.cmd_count >= sizeof(ex_cmd.at_cmd)/sizeof(ex_cmd.at_cmd[0]))
		return 1;//buffer full
	 dn=strlen(CmdStr);
	 if(dn>sizeof(ex_cmd.at_cmd[0])-2)
		return 2;//command too long
	 if(memcmp(CmdStr,"AT",2)&&memcmp(CmdStr,"S3",2)&&memcmp(CmdStr,"S7",2)&&
		 memcmp(CmdStr,"WT=",3))
		return 3;//unsupported command

	 if(task_p0!=TASK_IDLE)OnHook();

	 if(!memcmp(CmdStr,"WT=",3))
    {
		sprintf(tmps,"%.100s",CmdStr+3);
		ex_cmd.connect_wait=atoi(tmps);
    }
    else
    {
		 memcpy(ex_cmd.at_cmd[ex_cmd.cmd_count],CmdStr,dn);

		//--transfer conexant S91 command to blank for silicon
		if(modem_name[0]=='S' && !memcmp(CmdStr,"AT",2) &&
			(strstr(CmdStr,"S91=") || strstr(CmdStr,"s91=")))
		{
			sprintf(tmps,"%.101s",CmdStr);
			ptr_c=strstr(tmps,"S91=");
			if(ptr_c==NULL)ptr_c=strstr(tmps,"s91=");
			memset(ptr_c,0x20,4);
			for(a=ptr_c[4],i=4;a>0;i++)
			{
				a=ptr_c[i];
				if(a!=0x20 && (a<'0'||a>'9'))break;
				ptr_c[i]=0x20;
			}

			//--update the command line
//printk("CMD:%s.",tmps);
			memcpy(ex_cmd.at_cmd[ex_cmd.cmd_count],tmps,dn);
		}

		 if(CmdStr[dn-1]!='\r')
	    {
			ex_cmd.at_cmd[ex_cmd.cmd_count][dn]='\r';
			dn++;
	    }
		 ex_cmd.at_cmd[ex_cmd.cmd_count][dn]=0;
	    ex_cmd.cmd_count++;
    }

	 if(RespData)RespData[0]=0;
	 if(Dlen)*Dlen=0;

    return 0;
}

static uchar cx_v23hdx_txd(uchar *tx_data,ushort len)
{
	uchar tmpc,resp;
	ushort i;
	uint t0,time_out;

	resp=0;

	//1--wait for DCD off
	t0=GetTimerCount();//1s
	while(1)
	{
		if(GET_DCD)break;

		if(GetTimerCount()-t0>1000)return 0xf5;//carrier is continuously on
	}

	//2--set RTS on
	SET_RTS(LOW_LEVEL);//set RTS low

	//3--wait for CTS on
	t0=GetTimerCount();//1s
	while(2)
	{
		if(!GET_CTS)break;//CTS ready

		if(GetTimerCount()-t0>1000)return 0xf6;//CTS is not on yet
	}

	if(!len)return 0;

	//4--wait for an interval ahead of tx
	LED_TXD_ON;
	t0=GetTimerCount();//10ms
	while(GetTimerCount()-t0<=10);

	//5--reset idle timeout if required
	if(idle_watch&&len)
		tmm_remains=modem_config.idle_timeout*1000;

	//6--send head part of tx data
	tmpc=tx_data[0];
	for(i=0;i<len;i++)
	  if(tx_data[i]!=tmpc)break;
	PortTxs(P_MODEM,tx_data,i);

	//7--wait a time between two parts of tx data,at least 50ms
	time_out=(i*10/12+5)*10;
	t0=GetTimerCount();
	while(len)
	{
		if(GetTimerCount()-t0>time_out)break;
	}

	//8--send tail part of tx data
	PortTxs(P_MODEM,tx_data+i,len-i);

	//9--wait for tx empty
	time_out=((len-i)*10/12+2)*10;
	t0=GetTimerCount();
	while(3)
	{
		tmpc=PortTxPoolCheck(P_MODEM);
		//if(!tmpc)break;

		if(GetTimerCount()-t0>time_out)
		{
			if(tmpc){resp=0xf7;goto T_TAIL;}//TX POOL not empty
			break;
		}
	}

T_TAIL:
	//10--set RTS off
	SET_RTS(HIGH_LEVEL);//set RTS high
	LED_TXD_OFF;

	 return resp;
}

static uchar si_v23hdx_txd(uchar *tx_data,ushort len)
{
	uchar tmpc,resp,tmps[100];
	ushort i,data_start,TxSizeCS,TxSizeMark;
	uint t0,time_out;

	if(!len)return 0;

	resp=0;

	//0--wait for an interval ahead of tx
	LED_TXD_ON;
	t0=GetTimerCount();//10ms
	while(GetTimerCount()-t0<=10);

	//1--set RTS on
	SET_RTS(0);//set RTS low

	//3--reset idle timeout if required
	if(idle_watch&&len)
		tmm_remains=modem_config.idle_timeout*1000;

	//4.1--set TxSizeCS and TxSizeMark in bits
	for(i=0;i<len;i++)
	{
		if(tx_data[i]!=0x55)break;
	}
	data_start=i;
	TxSizeCS=data_start*10;
	if(TxSizeCS>500)TxSizeCS=500;

	TxSizeMark=40+data_start/10*10;
	if(TxSizeMark>300)TxSizeMark=300;

	//--UCB:TxSizeCS(leading flag) in bits
	//--UCC:TxSizeMark in bits
	sprintf(tmps,"AT:UCB,%02X,%02X",TxSizeCS,TxSizeMark);
	if(modem_config.ignore_dialtone&0x02)strcat(tmps,";:UD1,10,64");//2,64
	strcat(tmps,"\r");
	PortTxs(P_MODEM,tmps,strlen(tmps));

	//4.2--fetch response of configuration in 1s
	t0=GetTimerCount();
	i=0;
	while(1)
	{
		if(GetTimerCount()-t0>1000){resp=0xfb;goto T_TAIL;}//no reply at configure

		tmpc=PortRx(P_MODEM,tmps+i);
		if(tmpc)continue;//RX empty
//printk("%02X ",tmps[i]);

		i++;
		if(i>=6 && !memcmp(tmps+i-6,"\r\nOK\r\n",6))break;
		if(i>=sizeof(tmps)){resp=0xf8;goto T_TAIL;}//too more data received
	}//while(1)

	//5.1--send tx request
	strcpy(tmps,"AT+FTM=202\r");
	PortTxs(P_MODEM,tmps,strlen(tmps));//tx with Channel Seizure and Mark

	//5.2--fetch response within 1s
	t0=GetTimerCount();
	i=0;
	while(1)
	{
		if(GetTimerCount()-t0>1000){resp=0xfb;goto T_TAIL;}//no reply after +FTM,1s

		tmpc=PortRx(P_MODEM,tmps+i);
		if(tmpc)continue;//RX empty

		i++;
		if(i>=12 && !memcmp(tmps,"\r\nCONNECT \r\n",12))break;
		if(i>=sizeof(tmps)){resp=0xf8;goto T_TAIL;}//too more data received
	}//while(1)

	//6.1--send tx data
	PortTxs(P_MODEM,tx_data+data_start,len-data_start);
	PortTxs(P_MODEM,"\x10\x03",2);//means the end of the transmit

	//6.2--wait for tx empty
	t0=GetTimerCount();
	i=0;
	while(2)
	{
		if(GetTimerCount()-t0>20000){resp=0xf7;goto T_TAIL;}//no reply after TX,20s

		tmpc=PortRx(P_MODEM,tmps+i);
		if(tmpc)continue;//RX empty

		i++;
		if(i>=6 && !memcmp(tmps,"\r\nOK\r\n",6))break;
		if(i>=sizeof(tmps)){resp=0xf8;goto T_TAIL;}//too more data received
	}//while(2)

	//7--wait for 20ms in callee mode
	if(modem_config.ignore_dialtone&0x02)
	{
		t0=GetTimerCount();
		while(GetTimerCount()-t0<=20);
	}

T_TAIL:
	//10--set RTS off
	SET_RTS(1);//set RTS high
	LED_TXD_OFF;

	return resp;
}

uchar ModemTxd_std(uchar *SendData,ushort len)
{
	uchar cc,tmps[10];
	ushort xlen;

	cc=modem_status;
	if(task_p0!=TASK_CALLER_DATA && task_p0!=TASK_CALLEE_DATA)
	{
		if(!cc || cc==0x08 || cc==0x01 || cc==0x09)cc=0x01;
		return cc;
	}

	if( (!is_sync || task_p0==TASK_CALLER_DATA) && len>DATA_SIZE ||
		 is_sync && task_p0==TASK_CALLEE_DATA && len>DATA_SIZE+5)return 0xfe;

	//--if it's V23C half duplex
	cc=modem_config.async_format&0x0f;
	if(cc==0x08 || cc==0x09)
	{
		cc=v23hdx_txd(SendData,len);
		return cc;
	}

	if(!len)return 0xfe;

	if(is_sync)
	{
		if(task_p0==TASK_CALLEE_DATA)
		{
			if(len<6)return 0xf0;

			sprintf(tmps,"%02X%02X",SendData[1],SendData[2]);
			xlen=atoi(tmps);
			if((xlen+5)!=len)return 0xf1;

			if(SendData[0]!=0x02)return 0xf2;
			if(SendData[len-2]!=0x03)return 0xf3;
			for(cc=0,xlen=1;xlen<len;xlen++)cc^=SendData[xlen];
			if(cc)return 0xf4;
		}

		if(req_write)//occupied
			return 0x01;

		DISABLE_MDM_INT;//disable modem timer interrupt
		//memcpy(request_pool,SendData,len);
		if(task_p0!=TASK_CALLEE_DATA)
		{
			req_write=len;
			pack_sdlc_v80(0,SendData,(ushort*)&req_write);
		}
		else
		{
			req_write=len-5;
			pack_sdlc_v80(0,SendData+3,(ushort*)&req_write);
		}
		modem_status|=0x01;//Tx buffer occupied
		ENABLE_MDM_INT;//enable modem timer interrupt
	}
	else
	{
		xlen=len+req_write;
		if(xlen>PACK_SIZE)//overflowed
			return 0x01;

		LED_TXD_ON;
		DISABLE_MDM_INT;//disable modem timer interrupt
		memcpy(request_pool+req_write,SendData,len);
		req_write+=len;
		ENABLE_MDM_INT;//enable modem timer interrupt
	}

	if(idle_watch&&len)
		tmm_remains=modem_config.idle_timeout*1000;

  return 0;
}

static uchar cx_v23hdx_rxd(uchar *rx_data,ushort *len)
{
	uchar tmpc,cc;
	ushort i;
	uint t0,timeout;

	if(!resp_write)
	{
	//1--set RTS off
	SET_RTS(HIGH_LEVEL);//set RTS high

	//2--wait for DCD on
	LED_RXD_ON;
	t0=GetTimerCount();//3s
	while(1)
	{
		if(!GET_DCD)break;

		if(GetTimerCount()-t0>3000)return 0x0c;//no rx carrier
	}
	}

	//3--receive data
	i=0;
	t0=GetTimerCount();//50ms
	timeout=50;
	while(2)
	{
		if(!GET_DCD)//DCD is on(low)
		{
			t0=GetTimerCount();//100ms
			timeout=100;
		}
		else if(!resp_write && GetTimerCount()-t0>timeout)//DCD is off for a time:50ms
			break;

		if(!resp_write)//empty
			continue;

		rx_data[i]=response_pool[resp_head];
		i++;

		DISABLE_MDM_INT;//disable modem timer interrupt
		resp_head=(resp_head+1)%FRAME_SIZE;
		resp_write--;
		if(!resp_write && (modem_status==0x08 || modem_status==0x09))
			modem_status&=0xf7;
		ENABLE_MDM_INT;//enable modem timer interrupt

		if(idle_watch)
			tmm_remains=modem_config.idle_timeout*1000;
	}
	*len=i;
	LED_RXD_OFF;

	if(!i)return 0x0c;
	return 0;
}

static uchar si_v23hdx_rxd(uchar *rx_data,ushort *len)
{
	uchar resp,tmpc,tmps[DATA_SIZE+30];
	ushort rlen,i;
	uint t0,t1;

	//--if the buffer aleady has data
	i=0;
	while(resp_write)
	{
		rx_data[i]=response_pool[resp_head];
		i++;

		DISABLE_MDM_INT;//disable modem timer interrupt
		resp_head=(resp_head+1)%FRAME_SIZE;
		resp_write--;
		ENABLE_MDM_INT;//enable modem timer interrupt

		if(!resp_write || i>=DATA_SIZE)
		{
			*len=i;
			return 0;
		}
	}//while(resp_write)

	resp=0;

	//1--set RTS off
	SET_RTS(1);//set RTS high

	//2--send RX request to modem
	strcpy(tmps,"AT");
	if(modem_config.ignore_dialtone&0x02)strcat(tmps,":UD2,64;");
	strcat(tmps,"+FRM=200\r");
	PortTxs(P_MODEM,tmps,strlen(tmps));

	//3--fetch response within 1s
	LED_RXD_ON;
	t0=GetTimerCount();
	i=0;
	while(1)
	{
		if(GetTimerCount()-t0>1000){resp=0x0c;break;}//no reply after +FRM,1s

		//--after 20ms silence,determine the end of the receive
		if(i && GetTimerCount()-t1>=20)
		{
			if(i>=26 && !memcmp(tmps+i-8,"\x10\x03\r\nOK\r\n",8))break;
		}

		tmpc=PortRx(P_MODEM,tmps+i);
		if(tmpc)continue;//RX empty
//printk("%02X ",tmps[i]);
		i++;
		t1=GetTimerCount();
		t0=GetTimerCount();

		if(i>=16 && !memcmp(tmps,"\x10\x03\r\nNO CARRIER\r\n",16)){resp=0x0c;break;}
		if(i>DATA_SIZE+26){resp=0xf8;break;}//too more data received
	}//while(1)

	*len=i;
	LED_RXD_OFF;
	if(resp)
	{
		memcpy(rx_data,tmps,i);
		return resp;
	}

	//--delete the leading 18 "CONNECT 1\r\n\r\n1\r\n\r\n" bytes
	//                     or "CONNECT 2\r\n\r\n2\r\n\r\n" bytes
	*len=i-26;
	memcpy(rx_data,tmps+18,i-26);

	if(idle_watch)tmm_remains=modem_config.idle_timeout*1000;

	return 0;
}
uchar ModemRxd_std(uchar *RecvData,ushort *len)
{
	uchar cc,n;
	ushort tmpu;

	*len=0;

	cc=modem_status;
	if(!cc || cc==0x08 || cc==0x01 || cc==0x09)cc=0x0c;

	if(task_p0==TASK_IDLE || task_p0==TASK_CALLER_DIAL ||
		task_p0==TASK_CALLEE_WAIT&&is_sync)
		return cc;

	//--if it's V23C half duplex
	n=modem_config.async_format&0x0f;
	if(cc==0x0c && (n==0x08 || n==0x09) )
	{
		cc=v23hdx_rxd(RecvData,len);
		return cc;
	}

	if(!resp_write)//empty
		return 0x0c;

	DISABLE_MDM_INT;//disable modem timer interrupt
	*len=resp_write;

	if(is_sync)memcpy(RecvData,response_pool,resp_write);
	else
	{
		if((resp_head+resp_write)<=FRAME_SIZE)
			memcpy(RecvData,response_pool+resp_head,resp_write);
		else
		{
			tmpu=FRAME_SIZE-resp_head;
			memcpy(RecvData,response_pool+resp_head,tmpu);
			memcpy(RecvData+tmpu,response_pool,resp_write-tmpu);
		}

		resp_head=(resp_head+resp_write)%FRAME_SIZE;
	}

	//memset(response_pool,0x00,sizeof(response_pool));
	if(modem_status==0x08 || modem_status==0x09)modem_status&=0xf7;
	resp_write=0;//clear RX count
	ENABLE_MDM_INT;//enable modem timer interrupt

	if(idle_watch)
		tmm_remains=modem_config.idle_timeout*1000;

	 return 0;
}

uchar ModemAsyncGet_std(uchar *ch)
{
	uchar cc;

	cc=modem_status;
	if(task_p0!=TASK_CALLER_DATA && task_p0!=TASK_CALLEE_WAIT && task_p0!=TASK_CALLEE_DATA)
	{
		if(!cc || cc==0x08 || cc==0x01 || cc==0x09)cc=0x0c;
		return cc;
	}

	if(!resp_write || is_sync)//empty
		return 0x0c;

	ch[0]=response_pool[resp_head];

	DISABLE_MDM_INT;//disable modem timer interrupt
	resp_head=(resp_head+1)%FRAME_SIZE;
	resp_write--;
	if(!resp_write && (modem_status==0x08 || modem_status==0x09))
		modem_status&=0xf7;
	ENABLE_MDM_INT;//enable modem timer interrupt

	if(idle_watch)
		tmm_remains=modem_config.idle_timeout*1000;

	 return 0;
}

uchar ModemReset_std(void)
{
	if(!is_sync)
	{
		DISABLE_MDM_INT;//disable modem timer interrupt
		resp_write=0;
		memset(response_pool,0x00,sizeof(response_pool));
		if(modem_status==0x08 || modem_status==0x09)modem_status&=0xf7;
		ENABLE_MDM_INT;//enable modem timer interrupt
	}

    return 0;
}

static void interrupt_timer7(void)//generated per 10ms
{
	uchar tmpc,status;
	static ushort cycle;

//GPIO1_OUT_CLR_REG=0x100000;//set G1_20 low
    CLR_MDM_TIMER_INT_FLAG;
	ticks++;
//ScrPrint(0,0,0,"[%lu]",ticks);

	if(led_control.open)
	{
		  led_control.interrupt_count++;

		  if(led_control.is_led_txd)
		  {
			  if(dial_info.ptr>=dial_info.clen)
			  {
					led_control.open=0;
					LED_TXD_OFF;
					set_icon_dial(S_ICON_NULL);
					goto LED_TAIL;
			  }
			  if(!led_control.counted)
			  {
					cycle=dial_info.ptr;
					led_control.count_on=dial_info.on_10ms[cycle];
					led_control.count_off=dial_info.off_10ms[cycle];
					cycle=led_control.count_on+led_control.count_off;
					led_control.counted=1;
			  }

			  if(led_control.interrupt_count==1 && led_control.count_on)
			  {
					LED_TXD_ON;
					set_icon_dial(S_ICON_UP);
			  }
			  else if(led_control.interrupt_count-1==led_control.count_on)
			  {
					LED_TXD_OFF;
					set_icon_dial(S_ICON_NULL);
			  }

			  if(led_control.interrupt_count==cycle || !cycle)
			  {
					led_control.interrupt_count=0;
					led_control.counted=0;
					dial_info.ptr++;
					LED_TXD_OFF;
					set_icon_dial(S_ICON_NULL);
			  }
			  goto LED_TAIL;
		  }

		  if(!led_control.counted)
		  {
			  cycle=led_control.count_on+led_control.count_off;
			  led_control.counted=1;
		  }
		  if(led_control.first_on)
		  {
			  if(!(led_control.interrupt_count%cycle))
			  {
				  if(led_control.is_led_online)LED_ONLINE_ON;
				  else if(!led_control.is_led_txd)LED_RXD_ON;
				  set_icon_dial(S_ICON_UP);
			  }
			  else if(!(led_control.interrupt_count%cycle%led_control.count_on))
			  {
					if(led_control.interrupt_count >=
						 led_control.want_flashes*cycle-led_control.count_off)
					{
						led_control.open=0;
						if(!led_control.last_on)
						{
							if(led_control.is_led_online)LED_ONLINE_OFF;
							else if(!led_control.is_led_txd)LED_RXD_OFF;
						}
						set_icon_dial(S_ICON_NULL);
					}
					else
					{
						if(led_control.is_led_online)LED_ONLINE_OFF;
						else if(!led_control.is_led_txd)LED_RXD_OFF;
						set_icon_dial(S_ICON_NULL);
					}
			  }
		  }
		  else
		  {
			  if(!(led_control.interrupt_count%cycle))
			  {
					if(!led_control.last_on ||
					  led_control.interrupt_count<led_control.want_flashes*cycle)
					{
					  if(led_control.is_led_online)LED_ONLINE_OFF;
					  else if(!led_control.is_led_txd)LED_RXD_OFF;
					}
				  set_icon_dial(S_ICON_NULL);
			  }
			  else if(!(led_control.interrupt_count%cycle%led_control.count_off))
			  {
					if(led_control.interrupt_count>led_control.want_flashes*cycle)
						led_control.open=0;
					else
					{
						if(led_control.is_led_online)LED_ONLINE_ON;
						else if(!led_control.is_led_txd)LED_RXD_ON;
						set_icon_dial(S_ICON_UP);
					}
			  }
			}

	 }//if(b_led_interrupt)

LED_TAIL:

	if(tm00_remains)tm00_remains--;//for delay
	if(tm01_remains)tm01_remains--;
	if(tm02_remains)tm02_remains--;
	if(tmm_remains)tmm_remains--;//for idle timing

	if(task_p0==TASK_IDLE)
	{
//GPIO1_OUT_SET_REG=0x100000;//set G1_20 high
		if(modem_name[0]=='S' && !PowerOff_flag && (GET_PWR || !patch_loaded))
		{
			tmpc=si_step_load_patch(0);
			if(!tmpc&&task_load_step)return;
			 task_load_step=0;
		}
		return;
	}

	switch(task_p0)
	{
	case TASK_CALLER_DIAL:
		//--if power is off or patch not loaded,then perform patch loading
		if( (GET_PWR || !patch_loaded) && modem_name[0]=='C' )
		{
			tmpc=step_load_patch();
			if(!tmpc&&task_p1)return;
			if(tmpc)
			{
				ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
				ModemPowerOff();
				goto DIAL_FAIL_END;
			}
		}

		tmpc=caller_connect();
//	s_UartPrint(" dial:%d\r\n",tmpc);
		if(!tmpc)
		{
			if(task_p1)
			{
//GPIO1_OUT_SET_REG=0x100000;//set G1_20 high
				return;
			}

			ex_cmd.cmd_count=0;
			ex_cmd.connect_wait=0;
			task_p0=TASK_CALLER_DATA;
			if(modem_config.idle_timeout)idle_watch=1;
			else idle_watch=0;
//GPIO1_OUT_SET_REG=0x100000;//set G1_20 high
			return;
		}

DIAL_FAIL_END:
		ex_cmd.cmd_count=0;
		ex_cmd.connect_wait=0;
		hang_off();
		task_p0=TASK_IDLE;
		modem_running=0;
		PortStop(P_MODEM);
		break;
	case TASK_CALLER_DATA:
		if(idle_watch && !tmm_remains)
		{
			modem_status=0x0b;
			tmpc=1;
		}
		else
		{
			if(is_sync)tmpc=caller_sync_comm();
			else tmpc=async_comm();
			if(!tmpc && task_p1)
			{
//GPIO1_OUT_SET_REG=0x100000;//set G1_20 high
				return;
			}
		}

		if(tmpc)hang_off();
		task_p0=TASK_IDLE;
		modem_running=0;
		PortStop(P_MODEM);
		break;
	case TASK_CALLEE_WAIT:
		//--if power is off or patch not loaded,then perform patch loading
		if(GET_PWR || !patch_loaded)
		{
			switch(modem_name[0])
			{
			case 'C':
				tmpc=step_load_patch();
				if(!tmpc&&task_p1)return;
				break;
			case 'S':
				tmpc=si_step_load_patch(1);
				if(!tmpc&&task_load_step)return;
				break;
			}
			if(tmpc)
			{
				ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
				ModemPowerOff();
				task_p1=task_p2=0;
				return;
			}
			modem_status=0x0a;
		}

		if(modem_status==0x0a)
		{
			tmpc=callee_setup();
			if(!tmpc&&task_p1)return;//added on 20071217
			if(tmpc)break;
		}

		if(answer_interface && !tm00_remains && !is_sync)
		{
			store_response("\x02\x05\x00\x00\x05",5);
			tm00_remains=200;
		}//if(end of timer 4)

		tmpc=callee_connect();
		if(!tmpc && task_p1)
		{
//GPIO1_OUT_SET_REG=0x100000;//set G1_20 high
			return;
		}

		task_p1=task_p2=0;

		if(!tmpc)
		{
			task_p0=TASK_CALLEE_DATA;
			if(modem_config.idle_timeout)idle_watch=1;
			else idle_watch=0;
//GPIO1_OUT_SET_REG=0x100000;//set G1_20 high
			return;
		}
		break;
	case TASK_CALLEE_DATA:
		if(idle_watch && !tmm_remains)
		{
			modem_status=0x0b;
			tmpc=1;
		}
		else
		{
			if(is_sync)tmpc=callee_sync_comm();
			else tmpc=async_comm();
			if(!tmpc && task_p1)
			{
//GPIO1_OUT_SET_REG=0x100000;//set G1_20 high
				return;
			}
		}

		if(tmpc)hang_off();

		task_p0=TASK_CALLEE_WAIT;
		task_p1=0;task_p2=0;
		req_write=0;resp_write=0;resp_head=0;
		led_control.callee_skips=0;
		led_control.callee_lighted=0;
		tm00_remains=200;
		break;
	default:
		break;
	}

//GPIO1_OUT_SET_REG=0x100000;//set G1_20 high
}

static uchar cx_caller_connect(void)
{
	static uchar a,phone_tail,tmpc_data,tmps[210];
	static uchar i,speed_type,dial_times,dial_remains,b_modem_status;
	static uchar resp,x_retries,flashes;
	static ushort dn,tmpu,carrier_loss,tmpu_data;
	static struct
	{
		ushort dial_wait_ms;
		uchar pbx_pause_s;
		uchar code_hold_ms;
		uchar code_space_ms;
		uchar pulse_on_ms;
		uchar pulse_break_ms;
		ushort pulse_space_ms;
	}tm;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 200:goto STEP200;
	case 3:goto STEP3;
	case 4:goto STEP4;
	case 5:goto STEP5;
	case 6:goto STEP6;
	case 7:goto STEP7;
	case 71:goto STEP71;
  	case 72:goto STEP72;
	case 8:goto STEP8;
	case 9:goto STEP9;
	case 10:goto STEP10;
	case 101:goto STEP101;
	case 102:goto STEP102;
	case 11:goto STEP11;
	case 12:goto STEP12;
	case 13:goto STEP13;
	case 14:goto STEP14;
	case 15:goto STEP15;
	case 16:goto STEP16;
	case 17:goto STEP17;
	default:return 100;
	}

STEP0:
	if(!modem_config.dial_times)dial_remains=1;
	else dial_remains=modem_config.dial_times;

	dial_times=0;
	memset(&tm,0x00,sizeof(tm));

	 //--fetch connection speed
	 a=(modem_config.pattern>>5)&0x03;
	 if(is_sync)
	 {
		  if(a<2)a+=1;
		  else if(a==2)a=5;
		  else a=1; //forced to 1200bps for other sync speeds
	}
	 else
	 {
		  tmpc_data=modem_config.async_format&0x0f;
		  if(tmpc_data==0x08 || tmpc_data==0x09)a=0;
		  else if(modem_config.async_format>>4)a=modem_config.async_format>>4;
		  else if(a<2)a+=1;//1-1200,2-2400
		  else if(a==2)a=5;//9600
		  else a=7;//14400

		  if(mdm_chip_type==5 && a>7)a=7;
		  if(mdm_chip_type==3 && a>13)a=13;
	 }

	 speed_type=a;
	 asyn_form=modem_config.async_format&0x0f;

DIAL_BEGIN:
	modem_status=0x0a;
	req_write=resp_write=resp_head=0;
	dial_times++;
	task_p2=0;

	PortTx(P_MODEM,'X');//wake modem up

//s_TimerSet(5,1000);
	if(tmm_remains<1)tmm_remains=2;//leave 20ms for modem wakeup
	while(tmm_remains)//wait between two dials
	{
		LED_ONLINE_ON;
		ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
		task_p1=1;
STEP1:
		if(tmm_remains)//wait between two dials
			return 0;
		LED_ONLINE_OFF;
		ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	}

	for(i=0;i<25;i++)
	{
		a=PortRx(P_MODEM,tmps);
//if(!a)s_UartPrint(" data found:%02X\r\n",tmps[0]);
	}

	dn=modem_config.pbx_pause;
	dn=(dn+9)/10;
	if(!dn)dn=1;//minimized to 1 second for comma on 20120620
	tm.pbx_pause_s=dn;

    SET_DTR(0);
	SET_RTS(0);//set DRT,RTS low
	if(ex_cmd.connect_wait)
	{
		tmpu=ex_cmd.connect_wait;
		if(tmpu<5)tmpu=5;
		else if(tmpu>255)tmpu=255;
	}
	else
	{
		tmpu=(modem_config.pattern&0x07)*3+5;
		if(modem_config.ignore_dialtone&0x40)tmpu*=2;//double timeout
	}
	if(!speed_type)carrier_loss=255;//240 for 4 minutes,255 no hangup
	else if(speed_type<3)carrier_loss=14;//14,NEED
	else carrier_loss=14;//14
	//S7:carrier wait time,S10:carrier loss time to disconnect
	//S8:pause for ','
	//sprintf(tmps,"ATS7=%uS10=%u\r",tmpu,carrier_loss);
	sprintf(tmps,"AT&FE0S8=%uS7=%uS10=%u",dn,tmpu,carrier_loss);
	strcat(tmps,";-TRV\r");//read Tip-Ring Voltage
	task_p1=2;
STEP2:
	a=step_send_to_smodem(tmps,&dn);
	if(!a&&task_p2)return 0;
//printk(" trv:%d,dn:%d,%s\r\n",a,dn,tmps,tmps[0]);
	if(a)
	{
		modem_status=0xd0;
		return 2;
	}

	if(dn==6 && !memcmp(tmps,"\r\nOK\r\n",6))
	{
		tmps[0]=0;tmps[2]='X';//read Tip-Ring Voltage again
		task_p1=200;
STEP200:
		a=step_send_to_smodem(tmps,&dn);
		if(!a && task_p2)return 0;
//s_UartPrint(" trv again :%d,dn:%d,%s\r\n",a,dn,tmps);
	}

	//--search for the voltage string
	for(i=0;i<dn;i++)
		if(tmps[i]=='\n'&&isdigit(tmps[i+1]))break;
	if(i<dn)
	{
		for(tmpu_data=0;tmpu_data<3;tmpu_data++)
			if(tmps[i+tmpu_data]=='.')break;
		tmps[i+tmpu_data]=0;
		tmpu=atoi(tmps+i);
		tmps[i+tmpu_data]='.';//modified on 2011.2.23
	}
	else tmpu=19;

	if(tmpu<2)
	{
		modem_status=0x33;//0x03,no_line
		return 3;
	}

	if(dial_only && tmpu>=19)
	{
		modem_status=0x83;//not picked up for dial only
		return 4;
	}

	if(!dial_only && tmpu<19 && !(modem_config.ignore_dialtone&0x04))
	{
		modem_status=0x03;
		return 5;
	}

  //3.1--set dial tone detect
	dn=modem_config.dial_wait;
	dn=(dn+9)/10;
	if(dn<2)dn=2;//at least 2 seconds
	if(dn>5)dn=5;//limited to 2000ms~5000ms

	if(modem_config.ignore_dialtone&0x01)tm.dial_wait_ms=dn*1000+700;
	else tm.dial_wait_ms=1200;
		//X1:blind dial,X3:busy tone detect
		//S6:pause before blind dial(second)
		//S38:delay before forced hangup
		//S24:sleep inactivity timer
		//-SLP:set low power mode:0--3
		//!4897:set answer tone valid time,0x14--200ms
	//sprintf(tmps,"ATX4S38=1S6=%u",dn);
	//sprintf(tmps,"ATX4S38=1S6=%uS24=5-SLP=3;!4879=14;",dn);
	sprintf(tmps,"ATX4S38=1S6=%uS24=0-SLP=0;",dn);

    if(ex_cmd.manual_adjust_answer_tone)
    {
        i=ex_cmd.manual_adjust_answer_tone;
        if( (speed_type==5) && is_sync && (i>20) )i=20;//同步9600在应答音门限大于230毫秒时无法连接上，在这里做个限定
        sprintf(tmps+((uchar)strlen(tmps)),"!4879=%02X;",i);
    }
    else
    {
    	if(speed_type==1 || speed_type==2)strcat(tmps,"!4879=14;");//200ms for 1200/2400bps
    	else strcat(tmps,"!4879=0A;");//100ms for other speeds
    }
    
	if(modem_config.ignore_dialtone&0x01)tmps[3]='3';

		//if(dial_only || modem_config.ignore_dialtone&0x04)strcat(tmps,"-STE=4\r");//not detect parallel phone
		//else strcat(tmps,"-STE=5\r");//detect parallel phone
//	strcat(tmps,"-STE=4\r");//not detect parallel phone
	strcat(tmps,"-STE=0\r");//not detect remote hang off,not detect parallel phone
	task_p1=3;
STEP3:
	a=step_send_to_smodem(tmps,&dn);
	if(!a && task_p2)return 0;
//s_UartPrint(" STE:%d,dn:%d,%s\r\n",a,dn,tmps);
	if(a)
	{
		modem_status=0xd3;
		return 6;
	}

  //3.3--set DTMF/PULSE cadence,second dial tone detection
	a=modem_config.mode;
	if(!a || a>2)//DTMF dial code
	{
		dn=modem_config.code_hold;
		if(dn<50)dn=50;//limited to 50ms~255ms
		tm.code_hold_ms=dn;
		tm.code_space_ms=dn;
		sprintf(tmps,"ATS11=%u\r",dn);
		task_p1=4;
STEP4:
		a=step_send_to_smodem(tmps,&dn);
		if(!a && task_p2)return 0;
		if(a)
		{
			modem_status=0xd4;
			return 7;
		}
	}
	else
	{
		//pulse mode 1:61/39,mode 2:67/33
		if(a==2)dn=67;
		else dn=61;
		tm.pulse_break_ms=dn;

		if(a==2)tmpu=33;
		else tmpu=39;
		tm.pulse_on_ms=tmpu;

		tm.pulse_space_ms=860;
		tmpu=a-1;
		sprintf(tmps,"AT&P%d\r",tmpu);
		task_p1=5;
STEP5:
		a=step_send_to_smodem(tmps,&dn);
		if(!a && task_p2)return 0;
		if(a)
		{
			modem_status=0xd5;
			return 8;
		}
	}

	 //3.5--set connection speed
	 strcpy(tmps,"AT");

    if (ex_cmd.manual_adjust_tx_level>0)
    {
        sprintf(tmps+2, "S91=%d", ex_cmd.manual_adjust_tx_level);
    }
    else
    {
    	if(is_sync)
    	{
    	  if(speed_type==5)strcat(tmps,"S91=10");//10
    	  else if(speed_type==2)strcat(tmps,"S91=10");//8,
    	  else strcat(tmps,"S91=10");
    	}
    	else	strcat(tmps,"S91=10");
	}
	 if(speed_type==1 && modem_config.pattern&0x10)//BELL only for 1200bps
	 {
		if(modem_config.pattern&0x08)a=1;//CCITT
		else a=0;//BELL
	 }
	 else	a=1;//CCITT
	 if(a)strcat(tmps,"B0");
	 else	strcat(tmps,"B1");
	 switch(speed_type)
	 {
	  case 0://1200bps,half duplex
		if((modem_config.async_format&0x0f)==0x09)strcat(tmps,"+MS=V23C;B2;+SMS");
		else strcat(tmps,"+MS=B202;B2;+SMS");
		SET_RTS(HIGH_LEVEL);//set RTS high as off
		break;
	  case 1://1200bps
		if(is_sync)
		{
			if(!(modem_config.ignore_dialtone&0x80))//fast connect
				strcat(tmps,"%C0\\N0$F2S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0;+MS=V22");
			else
				strcat(tmps,"%C0\\N0S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0;+MS=V22");
		}
		else
		{
			if(!(modem_config.ignore_dialtone&0x80))//fast connect
				strcat(tmps,"%C0\\N0\\V1$F2S17=2+A8E=,,,0;+MS=V22");
			else
				strcat(tmps,"%C0\\N0\\V1S17=2+A8E=,,,0;+MS=V22");
		}
		break;
	  case 2://2400bps
		if(is_sync)
		{
			if(modem_config.ignore_dialtone&0x80)//fast connect
				strcat(tmps,"%C0\\N0$F2S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1;+MS=V22B");
			else
				strcat(tmps,"%C0\\N0S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0;+MS=V22B");
		}
		else
		{
			if(modem_config.ignore_dialtone&0x80)//fast connect
				strcat(tmps,"%C0\\N0\\V1$F2S17=2+A8E=,,,0;+MS=V22B");
			else
				strcat(tmps,"%C0\\N0\\V1+A8E=,,,0;+MS=V22B");
		}
		break;
	 case 3://4800bps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V32,,,4800,,4800");
		break;
	 case 4://7200bps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V32,,,7200,,7200");
		break;
	 case 5://9600bps
		if(is_sync)
		{
			if(!(modem_config.ignore_dialtone&0x80))//fast connect,fallback to V.22FC
				strcat(tmps,"%C0\\N0$F4S17=37+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0");
			else//fall back to V.22bis,2016.10.31
				strcat(tmps,"%C0\\N0$F4S17=5+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0");
			//strcat(tmps,"$F4S17=37&K0+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0");
			SET_RTS(HIGH_LEVEL);//set RTS high as off
		}
		else strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V32");
		break;
	 case 6://12.0kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V32B,,,12000,,12000");
		break;
	 case 7://14.4kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V32B");
		break;
    case 8://19.2kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V34,,,19200,,19200");
		break;
	 case 9://24kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V34,,,24000,,24000");
		break;
    case 10://26.4kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V34,,,26400,,26400");
		break;
	 case 11://28.8kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V34,,,28800,,28800");
		break;
	 case 12://31.2kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V34,,,31200,,31200");
		break;
	 case 13://33.6kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V34");
		break;
    case 14://48kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V92,,,48000,,48000");
		break;
    case 15://56kbps
		strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V92");
		break;
	 default:
		if(is_sync)strcat(tmps,"%C0\\N0$F2S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0;+MS=V22");
		else strcat(tmps,"\\V1&Q5+A8E=,,,0;+MS=V32B");
		break;
  }
  strcat(tmps,"\r");
//rGPCDAT|=0x40;
  task_p1=6;
STEP6:
  a=step_send_to_smodem(tmps,&dn);
  if(!a && task_p2)return 0;
//rGPCDAT&=0xffbf;
  if(a)
  {
//s_UartPrint(" pickup:%d,dn:%d,%s\r\n",a,dn,tmps);//NEED
		modem_status=0xd6;
		return 9;
  }

	for(tmpu=0;tmpu<ex_cmd.cmd_count;tmpu++)
	{
	  strcpy(tmps,ex_cmd.at_cmd[tmpu]);  
	  task_p1=7;
STEP7:
	  a=step_send_to_smodem(tmps,&dn);
	  if(!a && task_p2)return 0;
//printk(" ex_cmd%d,dn:%d,%s.\r\n",tmpu,strlen(ex_cmd.at_cmd[tmpu]),ex_cmd.at_cmd[tmpu]);//NEED
//printk(" ex_cmd result:%d,dn:%d,%s\r\n",a,dn,tmps);//NEED
	  if(a)
	  {
			modem_status=0xd9;
			return 10;
	  }
    }//for(tmpu)
    if (ex_cmd.cmd_count && ex_cmd.manual_adjust_tx_level)
    {
        sprintf(tmps, "ATS91=%d\r", ex_cmd.manual_adjust_tx_level);
        task_p1=71;
STEP71:
        a=step_send_to_smodem(tmps,&dn);
        if(!a && task_p2)return 0;

        if(a)
        {
            modem_status=0xda;//??
            return 71;
        }        
    }
    if(ex_cmd.cmd_count && ex_cmd.manual_adjust_answer_tone)
    {
         i=ex_cmd.manual_adjust_answer_tone;
        if( (speed_type==5) && is_sync && (i>20) )i=20;//同步9600在应答音门限大于230毫秒时无法连接上，在这里做个限定
        sprintf(tmps,"AT!4879=%02X\r",i);
        task_p1=72;
STEP72:
        a=step_send_to_smodem(tmps,&dn);
        if(!a && task_p2)return 0;

        if(a)
        {
            modem_status=0xdb;//??
            return 72;
        } 
    }
  //4--begin to pick up and dial
	strcpy(tmps,"ATDT");
	a=modem_config.mode;
	if(a==1 || a==2)tmps[3]='P';
	i=4;
	tmpu=tm.dial_wait_ms;
	dial_info.ptr=0;
	dial_info.clen=1;
	dial_info.on_10ms[0]=0;
	dial_info.off_10ms[0]=tm.dial_wait_ms/10;
	while(str_phone[ptr_phone]!= ';' && str_phone[ptr_phone]!='.')
	{
		a=toupper(str_phone[ptr_phone]);
		if(tmps[3]=='T')//DTMF dial
		{
			if(a>='0'&&a<='9'||a=='!'||a==','||a=='*'||a=='#'||a=='W'||
				a>='A'&&a<='D')
			{
				if(modem_config.ignore_dialtone&0x01 && (a=='W'))
				{
					a=',';
				}
				tmps[i]=a;
				i++;

				dn=tmpu;
				if(a==',')tmpu+=tm.pbx_pause_s*1000;
				else if(a=='!')tmpu=500;
				else if(a=='W')tmpu+=500;/*补丁的最长等待时间为5s*/
				else tmpu+=tm.code_hold_ms+tm.code_space_ms;
				if(tmpu<dn)tmpu=60000;

				//--write for DTMF led flashing
				tmpc_data=dial_info.clen;
				if(a==','||a=='!'|| a=='W' )tmpu_data=0;
				else tmpu_data=tm.code_hold_ms/10;
				dial_info.on_10ms[tmpc_data]=tmpu_data;

				if(a==',')tmpu_data=tm.pbx_pause_s*100;
				else if(a=='!')tmpu_data=50;
				else if(a=='W')tmpu_data=500;
				else tmpu_data=tm.code_space_ms/10;
				dial_info.off_10ms[tmpc_data]=tmpu_data;
				dial_info.clen++;
			}
		}
		else if(a>='0'&&a<='9'||a==','||a=='W')//PULSE dial
		{
			if(a=='W')
			{
				a=',';
			}
			
			tmps[i]=a;
			i++;

			dn=tmpu;
			if(a==',')tmpu+=tm.pbx_pause_s*1000;
			else
			{
				if(a=='0')a=10;
				else a-='0';
				tmpu+=(tm.pulse_break_ms+tm.pulse_on_ms)*a+tm.pulse_space_ms;
			}
			if(tmpu<dn)tmpu=60000;

			//--write for PULSE led flashing
			tmpc_data=dial_info.clen;
			if(a==',')tmpu_data=0;
			else tmpu_data=(tm.pulse_break_ms+tm.pulse_on_ms)/10*a;
			dial_info.on_10ms[tmpc_data]=tmpu_data;

			if(a==',')tmpu_data=tm.pbx_pause_s*100;
			else tmpu_data=tm.pulse_space_ms/10;
			dial_info.off_10ms[tmpc_data]=tmpu_data;
			dial_info.clen++;
		}
		ptr_phone++;
	}//while(get phone number)

	if(ex_cmd.connect_wait)
	{
		dn=ex_cmd.connect_wait;
		if(dn<5)dn=5;
		else if(dn>255)dn=255;
		dn+=3;
		dn*=100;
	}
	else
	{
		dn=(modem_config.pattern&0x07)*300+800;//500
		if(modem_config.ignore_dialtone&0x40)dn*=2;//double timeout
		tmpu=tmpu/10+dn;
		if(tmpu>6000)dn=6000;//maximum 60s
		else dn=tmpu;
	}

	if(dial_only)tmps[i++]=';';
	tmps[i]='\r';
	tmps[i+1]=0;
	if(str_phone[ptr_phone]=='.')
	{phone_tail=1;ptr_phone=0;dial_remains--;}
	else phone_tail=0;
	if(str_phone[ptr_phone]==';')ptr_phone++;

//rGPCDAT|=0x40;
	has_picked=1;
	//SET_RELAY(0);//set RELAY low for cut off
    RELAY_OFF;
    
	led_auto_flash(N_LED_TXD,1,0,0);
	ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);
//s_UartPrint(" dial:%s\r\n",tmps);
	task_p1=8;
STEP8:
	a=step_send_to_smodem(tmps,&dn);
	if(!a&&task_p2)return 0;
//rGPCDAT&=0xffbf;
	led_control.open=0;LED_TXD_OFF;ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);//light off LED_TXD
//printk(" a:%d,result:%s,remains:%d,tail:%d\r\n",a,tmps,dial_remains,phone_tail);
	if(a)
	{
		if(dial_remains || !phone_tail)
		{
			hang_off();
			goto DIAL_BEGIN;
		}
		//modem_status=0xe7;
		modem_status=0x05;//0x05,NEED
		return 11;
	}

	if(dial_only && dn>5 && !memcmp(tmps+2,"OK",2))
	{
		modem_status=0x06;//is sending tel_no
		tm00_remains=200;
		while(tm00_remains)
		{
			task_p1=9;
STEP9:
			if(tm00_remains)return 0;
		}

		modem_status=0x0a;//is sending
		return 12;
	}

	if(dn>9 && !memcmp(tmps+2,"CONNECT",7))
	{
		flashes=0;
		if(is_sync)
		{
			if(dn>=13 && !memcmp(tmps,"\r\nCONNECT V29",13))is_half_duplex=1;
			else {is_half_duplex=0;SET_RTS(LOW_LEVEL);}//set RTS low as ON,2016.10.31

			task_p1=10;
STEP10:
			a=step_rcv_v80_cmd(0,&dn);
			if(!a)
			{
				if(task_p2)return 0;

				a=frame_pool[0];
				if(a==0x20)flashes=1;
				else if(a==0x21)flashes=2;
				else if(a==0x24)flashes=5;
				else if(a==0x22)flashes=3;
				else if(a==0x23)flashes=4;
				else
				{
					b_modem_status=0x040;//0x04,modified V81N
					resp=12;
					goto L_END;
				}
			}
//s_UartPrint(" Dn:%u,%02X%02X%02X\r\n",dn,frame_pool[0],frame_pool[1],frame_pool[2]);
		}
		else
		{
			for(tmpu=dn-1;;tmpu--)
			{
				if(tmps[tmpu]=='/'||tmps[tmpu]==0x20)break;
				if(!tmpu)break;
			}

			if(!tmpu || memcmp(tmps+dn-2,"\r\n",2))
			{
				b_modem_status=0xd7;
				resp=13;
				goto L_END;
			}

			tmps[dn-2]=0;
			tmpu_data=atoi(tmps+tmpu+1);
			line_bps=tmpu_data;
			if(tmpu_data==1200)flashes=1;
			else if(tmpu_data==2400)flashes=2;
			else if(tmpu_data==9600)flashes=5;
			else if(tmpu_data==14400)flashes=7;
			else if(tmpu_data==4800)flashes=3;
			else if(tmpu_data==7200)flashes=4;
			else if(tmpu_data==12000)flashes=6;
			else if(tmpu_data>14400 && tmpu_data<=19200)flashes=8;
			else if(tmpu_data>19200 && tmpu_data<=24000)flashes=9;
			else if(tmpu_data==26400)flashes=10;
			else if(tmpu_data==28800)flashes=11;
			else if(tmpu_data==31200)flashes=12;
			else if(tmpu_data==33600)flashes=13;
			else if(tmpu_data>33600 && tmpu_data<=48000)flashes=14;
			else if(tmpu_data>48000)flashes=15;
		}
//s_UartPrint(" line_bps:%ld,flashes:%d\r\n",line_bps,flashes);

		//LED_ONLINE=1;
		if(modem_config.idle_timeout>90)modem_config.idle_timeout=90;//enlarged to 90 from 65,2009.4.29
		tmm_remains=modem_config.idle_timeout*1000;
		goto L_TAIL;
	}

	//5--check dial tone
	if(dn>13 && !memcmp(tmps+2,"NO DIALTONE",11))
	{
		//--wait for 300ms
		tm00_remains=30;
		while(tm00_remains)
		{
			task_p1=101;
STEP101:
			if(tm00_remains)return 0;
		}

		//--check the line voltage
		strcpy(tmps,"AT-TRV\r");//read Tip-Ring Voltage
		task_p1=102;
STEP102:
		a=step_send_to_smodem(tmps,&dn);
		if(!a && task_p2)return 0;
//s_UartPrint(" trv:%d,dn:%d,%s\r\n",a,dn,tmps);
		if(a)
		{
			modem_status=0xd8;
			return 14;
		}

		if(dn>6 && !memcmp(tmps,"\r\n1.40",6))
		{
			modem_status=0xee;//must not be changed
			return 15;
		}

		b_modem_status=0xe8;//must not be changed
		resp=16;
		goto L_END;
	}

  //6--check busy signal
  if(dn>6 && !memcmp(tmps+2,"BUSY",4))
  {
		b_modem_status=0x0d;//callee busy
		resp=17;
		goto L_END;
  }

  //7--check carrier
  if(dn>12 && !memcmp(tmps+2,"NO CARRIER",10))
  {
		b_modem_status=0x05;//has answer but not connected
		resp=18;
		goto L_END;
  }

  //--parallel phone detected
  if(dn>12 && !memcmp(tmps+2,"LINE IN USE",11))
  {
		modem_status=0x03;
		return 19;
  }

  //--line absent
  if(dn>9 && !memcmp(tmps+2,"NO LINE",7))
  {
		modem_status=0x33;//0x33,NEED
		return 20;
  }

  //--parallel phone off hook
  if(dn>20 && !memcmp(tmps+2,"OFF-HOOK INTRUSION",18))
  {
		modem_status=0x03;
		return 21;
  }

  //--line broken
  if(dn>24 && !memcmp(tmps+2,"LINE REVERSAL DETECTED",22))
  {
		modem_status=0x33;//0x03
		return 22;
  }

  if(dn>7 && !memcmp(tmps+2,"ERROR",5))
  {
		modem_status=0xe9;
		return 23;
  }

  if(dn>23 && !memcmp(tmps+2,"DIGITAL LINE DETECTED",21))
  {
		modem_status=0xec;//must not be changed
		return 24;
  }

  modem_status=0xef;//unknown error
  return 30;

L_TAIL:

	resp=0;
	if(!is_sync)
		goto L_END;

//TimerSet(2,2000);
	x_retries=0;
	while(1)
	{
		if(is_half_duplex)
		{
			task_p1=11;
STEP11:
			a=step_turn_to_rx();
			if(!a && task_p2)return 0;
		}
		task_p1=12;
STEP12:
		a=step_rcv_v80_cmd(0,&dn);
		if(!a && task_p2)return 0;
//s_UartPrint(" SNRM a:%d,dn:%u,%02X%02X\r\n",a,dn,frame_pool[0],frame_pool[1]);
//if(x_retries<8)ScrPrint(0,x_retries,0,"%d,T:%d,a:%d,%02X%02X",
//			x_retries,2000-TimerCheck(2),a,frame_pool[0],frame_pool[1]);
//goto B_TAIL;
		if(a==2 && dn)//overflowed
		{
			b_modem_status=0x10;
			resp=31;
			goto L_END;
		}
		if(a || dn<2)
		{
			if(a==6)
			{
			 if(dn==14 && !memcmp(frame_pool,"\r\nNO CARRIER\r\n",14))
			 {
				b_modem_status=0x04;
				resp=32;
				goto L_END;
			 }
			 if(x_retries==2)goto SEND_UA;
			}
			goto B_TAIL;
		}

		if(frame_pool[0]!=0x30 && frame_pool[0]!=0xff)
		{
//ScrPrint(0,0,0," SNRM a:%d,dn:%u,%02X%02X%02X%02X\r\n",a,dn,frame_pool[0],frame_pool[1],frame_pool[2],frame_pool[3]);
			b_modem_status=0x11;
			resp=33;
			goto L_END;
		}

		frame_address=frame_pool[0];
		if(x_retries && frame_pool[1]==0x11)break;
		if(frame_pool[1]!=0x93)
		{
			b_modem_status=0x12;
			resp=34;
			goto L_END;
		}

STEP13:
SEND_UA:

		if(is_half_duplex)
		{
			task_p1=14;
STEP14:
			a=step_turn_to_tx();
			if(!a && task_p2)return 0;
//s_UartPrint(" tx:%d\r\n",a);
		}

		memcpy(frame_pool,"\x30\x73\x19\xb1",4);
		task_p1=15;
STEP15:
		a=step_send_to_serial1(frame_pool,4);
		if(!a && task_p2)return 0;
//s_UartPrint(" SEND_UA a:%d\r\n",a);

		if(is_half_duplex)
		{
			task_p1=16;
STEP16:
			a=step_turn_to_rx();
			if(!a && task_p2)return 0;
		}
		task_p1=17;
STEP17:
		a=step_rcv_v80_cmd(0,&dn);
		if(!a && task_p2)return 0;

//s_UartPrint(" RR a:%d,dn:%u,%02X%02X\r\n",a,dn,frame_pool[0],frame_pool[1]);
		if(a==2 && dn)
		{
			b_modem_status=0x13;
			resp=39;
			goto L_END;
		}
		if(a || dn<2)goto B_TAIL;

		frame_address=frame_pool[0];
		if(frame_pool[1]==0x93)goto SEND_UA;
		if(frame_pool[1]==0x11)break;

B_TAIL:
		x_retries++;
		if(x_retries>=10)//5
		{
			b_modem_status=0x04;//NO CARRIER
			resp=41;
			goto L_END;
		}

		/*masked on V85G,080528
		if(a==6)continue;//if timeover occurs

		tm00_remains=100;
		while(tm00_remains)
		{
			task_p1=18;
STEP18:
			if(tm00_remains)return 0;
		}
		*/
	}//while(1)

L_END:
	if(resp)
	{
		if(dial_remains || !phone_tail)
		{
			hang_off();
			goto DIAL_BEGIN;
		}
		modem_status=b_modem_status;
		return resp;
	}

	led_auto_flash(N_LED_ONLINE,flashes,1,1);//modified 2013.5.25
	ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
//s_UartPrint(" flashes:%d\r\n",flashes);
	req_write=resp_write=resp_head=0;
	check_flag=1;
	modem_status=0x00;

	task_p1=0;
  return 0;
}

static uchar caller_sync_comm(void)
{
	static uchar Nr,Ns,a,r_retries,s_retries,resp_r_retries,polls,sn_retries;
	static uchar tmpc_data;
	static ushort dn,i;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	case 4:goto STEP4;
	case 5:goto STEP5;
	case 6:goto STEP6;
	case 7:goto STEP7;
	case 8:goto STEP8;
	case 9:goto STEP9;
	case 10:goto STEP10;
	case 11:goto STEP11;
	case 12:goto STEP12;
	case 13:goto STEP13;
	case 14:goto STEP14;
	case 15:goto STEP15;
	case 16:goto STEP16;
	case 17:goto STEP17;
	case 18:goto STEP18;
	case 190:goto STEP190;
	case 19:goto STEP19;
	case 20:goto STEP20;
	default:return 100;
	}

STEP0:
	//--exchange data
	Ns=Nr=0;
	polls=MAX_SYNC_POLLS;
	while(2)
	{
		if(!req_write && modem_status!=0x01 && modem_status!=0x09
		 || polls<MAX_SYNC_POLLS)
		{
			if(is_half_duplex)
			{
				task_p1=1;
STEP1:
				a=step_turn_to_tx();
				if(!a && task_p2)return 0;
//s_UartPrint(" 1tx:%d ",a);
			}
			tmpc_data=(Nr<<5)+0x11;
			dn=0;
			pack_sdlc_v80(tmpc_data,NULL,&dn);
			task_p1=2;
STEP2:
			a=step_send_to_serial1(ci_pool,dn);
			if(!a && task_p2)return 0;
			if(a){modem_status=0x041;return 1;}//0x04

			if(polls<MAX_SYNC_POLLS)polls++;
		}
		else//data received from handset
		{
			polls=0;
			s_retries=0;
			if(modem_status==0x09)modem_status=0x01;//throw away the response data

REQ_S_RETRY:
			s_retries++;
			if(s_retries>MAX_SYNC_T_RETRIES){modem_status=0x042;return 2;}

			if(is_half_duplex)
			{
				 task_p1=3;
STEP3:
				a=step_turn_to_tx();
				if(!a && task_p2)return 0;
//s_UartPrint(" 2tx:%d ",a);
			}
			//dn=req_write;
			//pack_sdlc_v80(tmpc_data,request_pool,&dn);
			sdlc_tx_pool[1]=(Nr<<5)+0x10+(Ns<<1);//data frame
			task_p1=4;
STEP4:
			a=step_send_to_serial1(sdlc_tx_pool,req_write);
			if(!a && task_p2)return 0;
//printk("send, DN:%d,%d",req_write,a);
			if(a){modem_status=0x043;return 3;}//0x04

			r_retries=0;
			sn_retries=0;
REQ_R_RETRY:
			r_retries++;
			if(r_retries>MAX_SYNC_T_RETRIES){modem_status=0x044;return 4;}//0x04

			if(is_half_duplex)
			{
				task_p1=5;
STEP5:
				a=step_turn_to_rx();
				if(!a && task_p2)return 0;
//s_UartPrint(" 2rx:%d ",a);
			}
			task_p1=6;
STEP6:
			a=step_rcv_v80_cmd(1,&dn);
			if(!a && task_p2)return 0;
//s_UartPrint(" send's ACK:%d,dn:%d,%02X%02X%02X ",a,dn,frame_pool[0],frame_pool[1],frame_pool[2]);
			if(a && a<2)
			{
				modem_status=0x14;
				return 5;
			}
			if(a==10)//Tx Underrun
			{
				modem_status=0x19;
				return 5;
			}
			if(a==6 && dn>=26 && !memcmp(frame_pool+dn-26,"\r\nLINE REVERSAL DETECTED\r\n",26) ||
				a==6 && dn>=14 && !memcmp(frame_pool+dn-14,"\r\nNO CARRIER\r\n",14) )
			{
				modem_status=0x15;
				return 6;
			}

			if(a>=4 || dn<2 || frame_pool[0]!=0x30&&frame_pool[0]!=0xff)
			{
				if(a==ERR_SDLC_TX_OVERRUN)
				{
					modem_status=0x18;
					return 7;
				}

				if(is_half_duplex)
				{
					task_p1=7;
STEP7:
					a=step_turn_to_tx();
					if(!a && task_p2)return 0;
				}
				//tmpc_data=(Nr<<5)+0x1d;//SREJ frame,masked since 85I,08.9.12
				tmpc_data=(Nr<<5)+0x11;//RR frame
				dn=0;
				pack_sdlc_v80(tmpc_data,NULL,&dn);
				task_p1=8;
STEP8:
				a=step_send_to_serial1(ci_pool,dn);
				if(!a && task_p2)return 0;
				if(a){modem_status=0x045;return 8;}//0x04
				goto REQ_R_RETRY;
			}
			frame_address=frame_pool[0];

			//--if need repeat
			if(Ns==(frame_pool[1]>>5))
			{
				sn_retries++;

				if(sn_retries<=5)//2
				{
					if(is_half_duplex)
					{
						task_p1=9;
STEP9:
						a=step_turn_to_tx();
						if(!a && task_p2)return 0;
//s_UartPrint(" 3tx:%d ",a);
					}
					tmpc_data=(Nr<<5)+0x11;
					dn=0;
					pack_sdlc_v80(tmpc_data,NULL,&dn);
					task_p1=10;
STEP10:
					a=step_send_to_serial1(ci_pool,dn);
					if(!a && task_p2)return 0;
					if(a){modem_status=0x046;return 9;}//0x04

					r_retries=0;
					goto REQ_R_RETRY;
				}
				else goto REQ_S_RETRY;
			}

			Ns=(Ns+1)%8;

			if(Ns!=(frame_pool[1]>>5))
			{
				modem_status=0x16;
				return 10;
			}
			req_write=0;
			modem_status&=0xfe;

			if((frame_pool[1]&0x01)==0x00)//2016.10.31,解决印度客户后台I帧P/F=0而导致的丢包问题 .response received quickly
			{
				resp_r_retries=0;
				goto PROCESS_RESP;
			}

			//--send RR in order to receive response
			if(is_half_duplex)
			{
				task_p1=11;
STEP11:
				a=step_turn_to_tx();
				if(!a && task_p2)return 0;
//s_UartPrint(" 4tx:%d ",a);
			}
			tmpc_data=(Nr<<5)+0x11;
			dn=0;
			pack_sdlc_v80(tmpc_data,NULL,&dn);
			task_p1=12;
STEP12:
			a=step_send_to_serial1(ci_pool,dn);
			if(!a && task_p2)return 0;
			if(a){modem_status=0x047;return 11;}//0x04
		}

		resp_r_retries=0;
RESP_R_RETRY://main receive procedure
		resp_r_retries++;
		if(resp_r_retries>MAX_SYNC_R_RETRIES){modem_status=0x048;return 12;}//0x04

		if(is_half_duplex)
		{
			task_p1=13;
STEP13:
			a=step_turn_to_rx();
			if(!a && task_p2)return 0;
//s_UartPrint(" 5rx:%d ",a);
		}
		task_p1=14;
STEP14:
		a=step_rcv_v80_cmd(1,&dn);
		if(!a && task_p2)return 0;
//s_UartPrint(" main rcv:%d,dn:%d,%02X%02X%02X ",a,dn,frame_pool[0],frame_pool[1],frame_pool[2]);
		if(a && a<2)
		{
			modem_status=0x17;
			return 13;
		}
		if(a==6 && dn>=26 && !memcmp(frame_pool+dn-26,"\r\nLINE REVERSAL DETECTED\r\n",26) ||
			a==6 && dn>=14 && !memcmp(frame_pool+dn-14,"\r\nNO CARRIER\r\n",14) )
		{
			modem_status=0x15;
			return 14;
		}

		if(a>=4 || dn<2 || frame_pool[0]!=0x30&&frame_pool[0]!=0xff)
		{
		  //--send REJ in order to retry
			if(is_half_duplex)
			{
				task_p1=15;
STEP15:
				a=step_turn_to_tx();
				if(!a && task_p2)return 0;
//s_UartPrint(" 51tx:%d ",a);
			}
			 //tmpc_data=(Nr<<5)+0x1d;//SREJ frame,masked since 85I,08.9.12
			 tmpc_data=(Nr<<5)+0x11;//RR frame
			 dn=0;
			 pack_sdlc_v80(tmpc_data,NULL,&dn);
			 task_p1=16;
STEP16:
			 a=step_send_to_serial1(ci_pool,dn);
			 if(!a && task_p2)return 0;
			 if(a){modem_status=0x049;return 15;}//0x04
			 goto RESP_R_RETRY;
		}
	  frame_address=frame_pool[0];

	  if(frame_pool[1]==0x93)
	  {
			Ns=Nr=0;

			if(is_half_duplex)
			{
				 task_p1=17;
STEP17:
				a=step_turn_to_tx();
				if(!a && task_p2)return 0;
//s_UartPrint(" 52tx:%d ",a);
			}
			 memcpy(frame_pool+1,"\x73\x19\xb1",3);
			 task_p1=18;
STEP18:
			a=step_send_to_serial1(frame_pool,4);
			if(!a && task_p2)return 0;
//if(a)s_UartPrint(" sd:%d ",a);

			resp_r_retries=0;
			goto RESP_R_RETRY;
	  }
    if((frame_pool[1]&0x01) != 0x00)continue;//2016.10.31,解决印度客户后台I帧P/F=0而导致的丢包问题

PROCESS_RESP:
	  //--if response pack arrived
	  if(Nr == (frame_pool[1]>>1)%8)
	  {
		Nr=((frame_pool[1]>>1)+1)%8;
		dn-=2;

		//--wait 300ms if the last packet hasn't been fetched
		tm01_remains=30;
		task_p1=190;
STEP190:
		while(resp_write)
		{
		  if(tm01_remains)return 0;
		  break;
		}

		memcpy(response_pool,frame_pool+2,dn);
		resp_write=dn;
		modem_status|=0x08;//has data in from remote
	  }

	  //frame_pool[1]=(Nr<<5)+0x1d;//SREJ
	  if(is_half_duplex)
	  {
		  task_p1=19;
STEP19:
		  a=step_turn_to_tx();
		  if(!a && task_p2)return 0;
//s_UartPrint(" 53tx:%d ",a);
	  }
	  tmpc_data=(Nr<<5)+0x11;//RR
	  dn=0;
	  pack_sdlc_v80(tmpc_data,NULL,&dn);
	  task_p1=20;
STEP20:
	  a=step_send_to_serial1(ci_pool,dn);
	  if(!a && task_p2)return 0;
	  if(a){modem_status=0x04a;return 16;}//0x04

	  polls=MAX_SYNC_POLLS;
	  resp_r_retries=0;
	  goto RESP_R_RETRY;
	}//while(2)
}

static uchar cx_step_turn_to_rx(void)
{
	switch(task_p2)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	}

STEP0:
	//1--set RTS off
	SET_RTS(HIGH_LEVEL);//set RTS high

	//2--wait for DCD on
	tm02_remains=100;//300,NEED
	while(1)
	{
STEP1:
		if(!GET_DCD)break;

		if(!tm02_remains){task_p2=0;return 1;}

		task_p2=1;
		return 0;
	}

	task_p2=0;
	return 0;
}

static uchar cx_step_turn_to_tx(void)
{
	switch(task_p2)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	}

STEP0:
	//1--wait for DCD off
	tm02_remains=100;//100
	while(1)
	{
STEP1:
		if(GET_DCD)break;

		if(!tm02_remains){task_p2=0;return 1;}

		task_p2=1;
		return 0;
	}

	//2--set RTS on
	SET_RTS(LOW_LEVEL);//set RTS low

	//4--wait for CTS on
	tm02_remains=100;//100
	while(1)
	{
STEP2:
		if(!GET_CTS)break;//CTS ready
		//if(!MDM_CTS)break;

		if(!tm02_remains){task_p2=0;return 2;}

		task_p2=2;
		return 0;
	}

	tm02_remains=1;//1,NEED
STEP3:
	if(tm02_remains)
	{
		task_p2=3;
		return 0;
	}

	task_p2=0;
	return 0;
}

static uchar si_step_turn_to_rx(void)
{
	//1--set RTS off
	SET_RTS(1);//set RTS high

	return 0;
}

static uchar si_step_turn_to_tx(void)
{
	switch(task_p2)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	}

STEP0:
	//2--set RTS on
	SET_RTS(0);//set RTS low

	tm02_remains=12;//at least 120ms for silicon
	while(1)
	{
STEP1:
		if(!tm02_remains)break;

		task_p2=1;
		return 0;
	}//while(1)

	task_p2=0;
	return 0;
}

static uchar step_send_to_serial1(uchar *str, ushort dlen)
{
	static uchar resp,tmpc;
	static ushort i;

//rGPCDAT|=0x08;
	switch(task_p2)
	{
	case 0:goto STEP0;
//	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	}

STEP0:
	resp=0;
	if(!dlen){resp=1;goto T_TAIL;}

	LED_TXD_ON;
	PortTxs(P_MODEM,str,dlen);

	//--wait for tx_buffer empty
	if(dlen<500)tm02_remains=500;
	else if(dlen<1000)tm02_remains=1000;
	else	tm02_remains=1500;
	while(2)
	{
STEP2:
		tmpc=PortTxPoolCheck(P_MODEM);
		if(!tmpc)break;

		if(!tm02_remains){resp=2;goto T_TAIL;}

		task_p2=2;
		return 0;
	}

T_TAIL:
	if(is_half_duplex)
	{
		tm02_remains=10;//10
		//--SILICON modem needs more wait time following the UA frame
		if(str[1]==0x73 && modem_name[0]=='S')tm02_remains=35;//35
STEP3:
		if(tm02_remains)
		{
			task_p2=3;
			return 0;
		}

		//set RTS off
		SET_RTS(HIGH_LEVEL);//set RTS high
	}
	 task_p2=0;
	return resp;
}

static uchar step_rcv_v80_cmd(uchar long_wait,ushort *dlen)
//--use frame_pool[] as out_data_pool[]
{
static uchar tmpc,em_found,hunt_flag,cc,resp,cmd_type,aborts;
static ushort timeout,i,want_dlen;
//static ushort j;

	switch(task_p2)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	}

STEP0:
	i=0;
//j=0;
	em_found=0;
	hunt_flag=0;
	want_dlen=10000;
	aborts=0;

	if(!long_wait)timeout=84;
	else if(long_wait==1)timeout=400;//300
	else	timeout=400;
	//--set 1s timeout for sync 9600bps,2016.10.31
	if(long_wait && is_half_duplex)timeout=100;

	resp=0;
	*dlen=0;
	cmd_type=0;
	tm02_remains=timeout;

	while(1)
	{
STEP1:
	 tmpc=PortRx(P_MODEM,&cc);
	 if(tmpc)
	 {
		if(!tm02_remains){resp=6;break;}

		task_p2=1;
		return 0;
	}

	 ci_pool[i]=cc;
	 if(!i)
	 {
		LED_TXD_OFF;
		if(cc==0x30 || cc==0xff)LED_RXD_ON;
	 }
	 i++;
//ci_pool[1000+j]=cc;
//j++;
//printk("%02X ",cc);

	if(em_found)
		switch(cc)
	{
		case 0xbe://connect rate
			cmd_type=cc;
			i=0;
			want_dlen=2;
			em_found=0;
			break;
		case 0xb0://abort
//s_UartPrint("ABORT:%d,I:%d,%02X%02X\r\n",aborts,i,ci_pool[0],ci_pool[1]);
			aborts++;
			em_found=0;
			i=0;
			if(aborts>=6)//3
			{
//for(k=0;k<j;k++)
//s_UartPrint(" %02X",ci_pool[1000+k]);
				resp=100;
				goto R_END;
			}
			break;
		case 0xb4://tx underrun
			resp=ERR_SDLC_TX_UNDERRUN;//10;
			goto R_END;
		case 0xb5://tx overrun
			resp=ERR_SDLC_TX_OVERRUN;//11;
			goto R_END;
		case 0xb6://rx overrun
			resp=ERR_SDLC_RX_OVERRUN;//12;
			goto R_END;
		case 0xb2://hunt
//s_UartPrint("HUNT,I:%d,%02X%02X\r\n",i,ci_pool[0],ci_pool[1]);
			cmd_type=cc;
			i=0;
			em_found=0;
			hunt_flag=1;
			LED_RXD_ON;
			break;
		case 0xb1://valid frame
//s_UartPrint("VALID,I:%d,%02X%02X%02X%02X\r\n",i,ci_pool[0],ci_pool[1],ci_pool[2],ci_pool[3]);
			cmd_type=cc;
			if(i>=2)i-=2;
			em_found=0;
			if(i==1&&ci_pool[0]==0xff || i>=2 && !memcmp(ci_pool,"\xff\xff",2))//throw away invalid frame
				i=0;
			else if(!i)want_dlen=10000;//added on 2012.4.10
			else want_dlen=i;
			break;
		case 0xa0://0x11
			if(i>2)i-=2;
			ci_pool[i]=0x11;
			i++;
			em_found=0;
			break;
		case 0xa1://0x13
			if(i>2)i-=2;
			ci_pool[i]=0x13;
			i++;
			em_found=0;
			break;
		case 0x5c://0x19
			if(i>2)i-=2;
			ci_pool[i]=0x19;
			i++;
			em_found=0;
			break;
		case 0x76://0x99
			if(i>2)i-=2;
			ci_pool[i]=0x99;
			i++;
			em_found=0;
			break;
		case 0x5d://0x19 pair
			if(i>2)i-=2;
			memset(ci_pool+i,0x19,2);
			i+=2;
			em_found=0;
			break;
		case 0x77://0x99 pair
			if(i>2)i-=2;
			memset(ci_pool+i,0x99,2);
			i+=2;
			em_found=0;
			break;
		case 0xa2://0x11 pair
			if(i>2)i-=2;
			memset(ci_pool+i,0x11,2);
			i+=2;
			em_found=0;
			break;
		case 0xa3://0x13 pair
			if(i>2)i-=2;
			memset(ci_pool+i,0x13,2);
			i+=2;
			em_found=0;
			break;
		case 0xa4://\x19\x99
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x19\x99",2);
			i+=2;
			em_found=0;
			break;
		case 0xa5://\x19\x11
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x19\x11",2);
			i+=2;
			em_found=0;
			break;
		case 0xa6://\x19\x13
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x19\x13",2);
			i+=2;
			em_found=0;
			break;
		case 0xa7://\x99\x19
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x99\x19",2);
			i+=2;
			em_found=0;
			break;
		case 0xa8://\x99\x11
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x99\x11",2);
			i+=2;
			em_found=0;
			break;
		case 0xa9://\x99\x13
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x99\x13",2);
			i+=2;
			em_found=0;
			break;
		case 0xaa://\x11\x19
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x11\x19",2);
			i+=2;
			em_found=0;
			break;
		case 0xab://\x11\x99
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x11\x99",2);
			i+=2;
			em_found=0;
			break;
		case 0xac://\x11\x13
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x11\x13",2);
			i+=2;
			em_found=0;
			break;
		case 0xad://\x13\x19
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x13\x19",2);
			i+=2;
			em_found=0;
			break;
		case 0xae://\x13\x99
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x13\x99",2);
			i+=2;
			em_found=0;
			break;
		case 0xaf://\x13\x11
			if(i>2)i-=2;
			memcpy(ci_pool+i,"\x13\x11",2);
			i+=2;
			em_found=0;
			break;
		case 0xba://carrier lost
			resp=1;
			i=0;
			em_found=0;
			//break;
			goto R_END;
		case 0xbd:
			i=0;
			em_found=0;
			break;
		default:
//s_UartPrint("UNKOWN:%02X,I:%d,%02X%02X\r\n",cc,i,ci_pool[0],ci_pool[1]);
			//em_found=0;
			//i=0;
			//break;
			resp=20;
			goto R_END;
	}
	if(cc==0x19)em_found=1;

	if(i>=want_dlen)
	{
		if(i<FRAME_SIZE)break;
		else {resp=3;break;}
	}
	if(i>DATA_SIZE+3){LED_RXD_OFF;resp=5;break;}//2008.8.9

	//if(long_wait&&(hunt_flag||!em_found))tm02_remains=timeout;//100
	//if(hunt_flag||em_found)tm02_remains=timeout;//100
	tm02_remains=timeout;

  }//while(1)

R_END:

  *dlen=i;
  memcpy(frame_pool,ci_pool,i);
  frame_pool[i]=cmd_type;

  LED_RXD_OFF;
  task_p2=0;
  return resp;
}

static uchar step_send_to_smodem(uchar *str,ushort *dlen)
{
	static uchar returns,resp,mode,tmpc;
	static ushort i,wait_time;

	switch(task_p2)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	}

STEP0:
	if(str[2]=='D'||str[2]=='A')
	{
		mode=1;//dial command
		wait_time=*dlen;
	}
	else if(str[2]=='*'||str[2]=='H'||!str[0]&&!str[2])mode=2;
	else if(!str[0] && str[2]=='C')mode=3;//read caller ID
	else if(str[0]=='S')mode=4;//for patch load
	else mode=0;
	*dlen=strlen(str);

	resp=0;
	for(i=0;i<*dlen;i++)
	{
		tm02_remains=200;
		while(1)
		{
STEP1:
			if(!GET_CTS)break;//CTS ready

			if(!tm02_remains){resp=1;goto S_TAIL;}

			task_p2=1;
			return 0;
		}//while(1)

		PortTx(P_MODEM,str[i]);
//s_UartPrint(" %02X",str[i]);
	}//for(i)

//s_UartPrint(" sn:%d",*dlen);
	i=0;
	returns=0;
	*dlen=0;
	memset(str,0x00,10);

	if(!mode)tm02_remains=200;//200
	else if(mode==1)tm02_remains=wait_time;
	else if(mode==3)tm02_remains=300;
	else if(mode==4)goto S_TAIL;
	else tm02_remains=10;//10
	while(2)
	{
STEP2:
		tmpc=PortRx(P_MODEM,str+i);
		if(tmpc)
		{
			if(!tm02_remains)
			{
				resp=2;
				break;
			}

			task_p2=2;
			return 0;
		}
//s_UartPrint(" %02X",str[i]);
		if(str[i]=='\n')returns++;
		i++;

		if(mode==1)
		{
			switch(returns)
			{
			case 2:
				goto S_TAIL;
			}
		}
		else if(!mode)
		{
			if(i>=6&&!memcmp(str+i-4,"OK\r\n",4))break;
			if(i>=9&&!memcmp(str+i-9,"\r\nERROR\r\n",9)){resp=6;break;}//3,20040719
		}
		else
		{
			if(tm02_remains<2)tm02_remains=2;//enlarge timeout
			if(returns==2)
			{
				if(!memcmp(str,"\r\nOK",4))break;//20040216
				else if(!memcmp(str,"\r\nCONNECT",9))break;//20040216
				else if(!memcmp(str,"\r\nNO CARRIER",12))break;//20040216
				else if(!memcmp(str,"\r\nRING",6))break;//20090331
				else if(!memcmp(str,"\r\n04",4))break;//20090331,simple CID
				else if(!memcmp(str,"\r\n80",4))break;//20100105,complex CID
				else if(!memcmp(str,"\r\nDownload initiated ..",23))break;
			}
			else if(returns==3 && !memcmp(str,"\r\nCIDM",6))break;//20130420,silicon CID
		}

		if(i>=150){resp=5;break;}//>=48
	}//while(2)

S_TAIL:

	str[i]=0;
	*dlen=i;
	task_p2=0;
	return resp;
}

static uchar send_to_smodem(uchar *str,ushort *dlen)
{
	uchar tmpc,returns,resp,mode;
	ushort wait_time,j;
	ushort i;
	uint t0;

	if(str[2]=='D'||str[2]=='A')
	{
		mode=1;//dial command
		wait_time=*dlen;
	}
	else if(str[2]=='*'||str[2]=='H')mode=2;
	else if(str[0]=='S')mode=3;//for patch load
	else mode=0;
	*dlen=strlen(str);

	resp=0;
	for(i=0;i<*dlen;i++)
	{
		t0=GetTimerCount();//2s
		while(1)
		{
			if(!GET_CTS)break;//CTS ready

			if(GetTimerCount()-t0>2000){resp=2;goto S_TAIL;}
		}//while(1)

		PortTx(P_MODEM,str[i]);
	}//for(i)
//PortTxs(P_MODEM,str,*dlen);

	i=0;
	returns=0;
	*dlen=0;
	memset(str,0x00,10);

	if(!mode)wait_time=2000;//2s
	else if(mode==1);
	else if(mode==3)goto S_TAIL;
	else wait_time=100;//100ms

	t0=GetTimerCount();//wait_time
	while(2)
	{
		if(GetTimerCount()-t0>wait_time){resp=3;break;}

		tmpc=PortRx(P_MODEM,str+i);
		if(tmpc)continue;//RX empty

//s_UartPrint(" %02X",str[i]);
		if(str[i]=='\n')returns++;
		i++;

		if(mode==1)
		{
			switch(returns)
			{
			case 2:
				goto S_TAIL;
			}
		}
		else if(!mode)
		{
			if(i>=6&&!memcmp(str+i-4,"OK\r\n",4))break;
			if(i>=9&&!memcmp(str+i-9,"\r\nERROR\r\n",9)){resp=6;break;}
		}
		else if(returns==2)
		{
			if(!memcmp(str,"\r\nOK",4))break;
			else if(!memcmp(str,"\r\nDownload initiated ..",23))break;
			else if(!memcmp(str,"\r\nNO CARRIER",12))break;
			else if(!memcmp(str,"\r\nCONNECT",9))break;
		}

		if(i>=150){resp=5;break;}//>=48
	}//while(2)

S_TAIL:
	str[i]=0;
	*dlen=i;
	return resp;
}

static void pack_sdlc_v80(uchar ctrl,uchar *in_data,ushort *dlen)
//--use sdlc_tx_pool[] for out data frame buffer
//--use ci_pool[] as out administrate frame buffer
{
	uchar k,tmpc_data,xpool[FRAME_SIZE];
	ushort i,j,dn;
	uchar const single_esc[4]="\x5c\x76\xa0\xa1";
	uchar const pair_esc[16]="\x5d\x77\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf";
	char *out_pool;

	dn=*dlen;
	if(dn>FRAME_SIZE-2)dn=FRAME_SIZE-2;

	i=0;
	xpool[i++]=frame_address;//0x30,0xff
	xpool[i++]=ctrl;
	if(ctrl&0x01)out_pool=ci_pool;
	else out_pool=sdlc_tx_pool;
	memcpy(xpool+i,in_data,dn);
	i+=dn;
	dn=i;

	for(i=j=0;j<dn;j++)
	{
		tmpc_data=xpool[j];
		k=xpool[j+1];
		//--process pair_mask
		if(j+2<=dn)
		{
			switch(tmpc_data)
			{
			case 0x19:
				switch(k)
				{
				case 0x19:k=0;break;
				case 0x99:k=4;break;
				case 0x11:k=5;break;
				case 0x13:k=6;break;
				default:k=16;break;
				}
				break;
			case 0x99:
				switch(k)
				{
				case 0x19:k=7;break;
				case 0x99:k=1;break;
				case 0x11:k=8;break;
				case 0x13:k=9;break;
				default:k=16;break;
				}
				break;
			case 0x11:
				switch(k)
				{
				case 0x11:k=2;break;
				case 0x19:k=10;break;
				case 0x99:k=11;break;
				case 0x13:k=12;break;
				default:k=16;break;
				}
				break;
			case 0x13:
				switch(k)
				{
				case 0x13:k=3;break;
				case 0x19:k=13;break;
				case 0x99:k=14;break;
				case 0x11:k=15;break;
				default:k=16;break;
				}
				break;
			default:k=16;break;
			}
			if(k<16)
			{
				out_pool[i++]=0x19;
				out_pool[i++]=pair_esc[k];
				j++;
				continue;
			}
		}

		//--process single_mask
		switch(tmpc_data)
		{
		case 0x19:k=0;break;
		case 0x99:k=1;break;
		case 0x11:k=2;break;
		case 0x13:k=3;break;
		default:k=4;break;
		}
		if(k<4)
		{
			out_pool[i++]=0x19;
			out_pool[i++]=single_esc[k];
			continue;
		}

		out_pool[i++]=tmpc_data;
	}

	out_pool[i++]=0x19;
	out_pool[i++]=0xb1;

	*dlen=i;
}

static uchar async_comm(void)
{
	static uchar a,cc,carrier_lost,line_lost,speed_type;
	static ushort ptr_carrier,ptr_line,ptr_ok;
	uchar const NO_CARRIER[]="\x0d\x0aNO CARRIER\x0d\x0a";
	uchar const NO_LINE[]="\x0d\x0aLINE REVERSAL DETECTED\x0d\x0a";
	uchar const DUAL_OK[]="\x0d\x0aOK\x0d\x0a\x0d\x0aOK\x0d\x0a";

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 10:goto STEP10;
	default:return 100;
	}

STEP0:
	//--if it's SILICON FSK,do null loop
	while(modem_name[0]=='S' && (asyn_form==8||asyn_form==9))
	{
		task_p1=10;
STEP10:
		return 0;
	}

	tm00_remains=1;//for LED_RXD delay off control
	carrier_lost=0;
	line_lost=0;
	ptr_carrier=0;
	ptr_line=0;
	ptr_ok=0;

	speed_type=((modem_config.pattern>>5)&0x03)+1;
	if(modem_config.async_format>>4)speed_type=modem_config.async_format>>4;
	if(speed_type<3)speed_type=3;
	else speed_type=1;

	while(1)
	{
		//--send data to modem
		if(req_write && !PortTxPoolCheck(P_MODEM) &&
		  port_ppp_serve[0]!=1 )
		{
			PortTxs(P_MODEM,request_pool,req_write);
			LED_TXD_OFF;
			req_write=0;
		}

		//--receive data from modem
		a=PortRx(P_MODEM,&cc);
		if(a)
		{
			task_p1=1;
			return 0;
		}
		else
		{
			LED_RXD_ON;
//s_UartPrint(" :%02X",cc);
			if(asyn_form>=3&&asyn_form<=5)cc&=0x7f;

			//3.1--detect NO CARRIER
			if(cc==NO_CARRIER[ptr_carrier])ptr_carrier++;
			else if(cc==NO_CARRIER[0])ptr_carrier=1;
			else ptr_carrier=0;
			if(ptr_carrier<14)carrier_lost=0;
			else
			{
				carrier_lost=1;
				ptr_carrier=2;
				tm01_remains=50;//for carrier detect
			}
			//3.2--detect active hangup by +++ and ath
			if(auto_answer && !carrier_lost)
			{
				if(cc==DUAL_OK[ptr_ok])ptr_ok++;
				else if(cc==DUAL_OK[0])ptr_ok=1;
				else ptr_ok=0;
				if(ptr_ok>=12)
				{
					carrier_lost=1;
					ptr_ok=6;
					tm01_remains=50;//for carrier detect
				}
			}
			//3.3--detect LINE REVERSAL
			if(cc==NO_LINE[ptr_line])ptr_line++;
			else if(cc==NO_LINE[0])ptr_line=1;
			else ptr_line=0;
			if(ptr_line<26)line_lost=0;
			else
			{
				line_lost=1;
				ptr_line=2;
				tm02_remains=50;//for line detect
			}

			modem_status|=0x08;//has data in from remote

			if(resp_write<DATA_SIZE)
			{
				response_pool[(resp_head+resp_write)%FRAME_SIZE]=cc;
				resp_write++;
			}
			tm00_remains=speed_type;
		}

STEP1:
		//----detect carrier
		if(carrier_lost&&!tm01_remains || line_lost&&!tm02_remains)
		{
//s_UartPrint(" status:%08X,c:%d,l:%d ",rGPBDAT,carrier_lost,line_lost);
			modem_status=0x04;//NO CARRIER
			return 1;
		}

		if(!tm00_remains)LED_RXD_OFF;
	}//while(1)

	return 0;
}

static void set_icon_dial(uchar on_off)
{
	ScrSetIcon(ICON_TEL_NO,3+on_off);
}

void led_auto_flash(uchar led_no,uchar count,uchar first_on,uchar last_on)
{
	if(!count)return;

	led_control.open=0;
	led_control.is_led_online=0;
	led_control.is_led_txd=0;

	//--OFF first
	if(led_no==N_LED_ONLINE)
	{
		LED_ONLINE_OFF;
		set_icon_dial(S_ICON_NULL);
		led_control.is_led_online=1;

		led_control.count_on=50;
		led_control.count_off=60;
	}
	else if(led_no==N_LED_TXD)
	{
		LED_TXD_OFF;
		ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
		led_control.is_led_txd=1;
	}
	else
	{
		LED_RXD_OFF;
		set_icon_dial(S_ICON_NULL);
		led_control.is_led_online=0;

		led_control.count_on=50;
		led_control.count_off=60;
	}

	led_control.interrupt_count=0;
	if(count>25)count=25;
	led_control.want_flashes=count;
	led_control.last_on=last_on;

	if(first_on)
	{
		led_control.first_on=1;
		if(led_control.is_led_online){LED_ONLINE_ON;}
		else if(!led_control.is_led_txd){LED_RXD_ON;}

		if(!led_control.is_led_txd)set_icon_dial(S_ICON_UP);
	}
	else led_control.first_on=0;

	led_control.counted=0;

	led_control.open=1;
	return;
}

uchar cx_callee_setup(void)
//--use frame_pool as temporary buffer
{
	static uchar a,speed_type,n;
	static ushort dn,carrier_loss,i;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 10:goto STEP10;
	case 2:goto STEP2;
	case 3:goto STEP3;
	default:return 100;
	}

STEP0:
  modem_status=0x0a;
  task_p2=0;

  a=(modem_config.pattern>>5)&0x03;
  if(is_sync)
  {
	if(a<2)a+=1;
	else if(a==2)a=5;
	else a=1;//forced to 1200bps for other sync speeds
  }
  else
  {
	n=modem_config.async_format&0x0f;
	if(n==0x08 || n==0x09)a=0;
	else if(modem_config.async_format>>4)a=modem_config.async_format>>4;
	else if(a<2)a+=1;//1-1200,2-2400
	else if(a==2)a=5;//9600
	else a=7;//14400

	if(mdm_chip_type==5 && a>7)a=7;
	if(mdm_chip_type==3 && a>13)a=13;
  }
  speed_type=a;
  asyn_form=modem_config.async_format&0x0f;

	PortTx(P_MODEM,'X');//wake modem up
	//--delay 20ms in order that modem wakes up
	tm01_remains=2;//2
	while(tm01_remains)
	{
		task_p1=10;
STEP10:
		if(tm01_remains)return 0;
	}

	//--clear serial port 1 buffer
	for(dn=0;dn<25;dn++)
	{
		a=PortRx(P_MODEM,frame_pool);
	}

  //2----set
	if(ex_cmd.connect_wait)
	{
		dn=ex_cmd.connect_wait;
		if(dn<5)dn=5;
		else if(dn>255)dn=255;
	}
	else
	{
		dn=(modem_config.pattern&0x07)*3+5;
		if(modem_config.ignore_dialtone&0x40)dn*=2;
	}
	if(!speed_type)carrier_loss=255;//240 for 4 minutes,255 no hangup
	else if(speed_type<3)carrier_loss=14;//14
	else carrier_loss=14;//2
	//sprintf(frame_pool,"AT&FE0X4S38=1S7=%uS10=%u\r",dn,carrier_loss);
	sprintf(frame_pool,"AT&FE0X4S38=1S7=%uS10=%u-SLP=0\r",dn,carrier_loss);

	task_p1=1;
STEP1:
	a=step_send_to_smodem(frame_pool,&dn);
	if(!a&&task_p2)return 0;
	if(a)
	{
		modem_status=0xc0;
		return 1;
	}

  //4----set connection speed
  strcpy(frame_pool,"AT");
  if(is_sync)
  {
	if(speed_type==5)strcat(frame_pool,"S91=10");//10
	else strcat(frame_pool,"S91=10");//8,10(2012.4.12)
  }
  else
  {
	if(speed_type==1)strcat(frame_pool,"S91=10");
	else strcat(frame_pool,"S91=10");
  }
  if(speed_type==1 && modem_config.pattern&0x10)//BELL only for 1200bps
  {
		if(modem_config.pattern&0x08)a=1;//CCITT
		else a=0;//BELL
  }
  else a=1;//CCITT
  if(a)strcat(frame_pool,"B0");
  else strcat(frame_pool,"B1");
  a=speed_type;
  switch(a)
  {
  case 0://1200bps,half duplex
		 if((modem_config.async_format&0x0f)==0x09)strcat(frame_pool,"+MS=V23C;B2;+VCID=2;+SMS");
		 else strcat(frame_pool,"+MS=B202;B2;+VCID=2;+SMS");
		 //GPIO1_OUT_SET_REG=0x20;//set RTS high as off
		 break;
  case 1://1200bps
		if(is_sync)
		{
			if(!(modem_config.ignore_dialtone&0x80))//fast connect
				strcat(frame_pool,"%C0\\N0$F2S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0");
			else
				strcat(frame_pool,"%C0\\N0S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0");
		}
		else
		{
			if(!(modem_config.ignore_dialtone&0x80))//fast connect
				strcat(frame_pool,"%C0\\N0\\V1$F2S17=6+A8E=,,,0;+MS=V22");
			else strcat(frame_pool,"%C0\\N0\\V1+A8E=,,,0;+MS=V22");
		}
		break;
  case 2://2400bps
		if(is_sync)
		{
			if(modem_config.ignore_dialtone&0x80)//fast connect
				strcat(frame_pool,"%C0\\N0$F2S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1;+MS=V22B");
			else
				strcat(frame_pool,"%C0\\N0S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0;+MS=V22B");
		}
		else
		{
			if(modem_config.ignore_dialtone&0x80)//fast connect
				strcat(frame_pool,"%C0\\N0\\V1$F2S17=6+A8E=,,,0;+MS=V22B");
			else
				strcat(frame_pool,"%C0\\N0\\V1+A8E=,,,0;+MS=V22B");
		}
		break;
  case 3://4800bps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V32,,,4800,,4800");
		break;
  case 4://7200bps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V32,,,7200,,7200");
		break;
  case 5://9600bps
		if(is_sync)
		{
			strcat(frame_pool,"%C0\\N0$F4S17=37+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0");
			//strcat(frame_pool,"%C0\\N0;+A8E=,,,0;$F4;+ES=6,,8;+ESA=0,0,,,1,0;S17=13");
			//strcat(tmps,"$F4S17=37&K0+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0");
			//GPIO1_OUT_CLR_REG=0x20;//set RTS low as on
		}
		else strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V32");
		break;
  case 6://12.0kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V32B,,,12000,,12000");
		break;
  case 7://14.4kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V32B");
		break;
  case 8://19.2kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V34,,,19200,,19200");
		break;
  case 9://24kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V34,,,24000,,24000");
		break;
  case 10://26.4kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V34,,,26400,,26400");
		break;
  case 11://28.8kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V34,,,28800,,28800");
		break;
  case 12://31.2kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V34,,,31200,,31200");
		break;
  case 13://33.6kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V34");
		break;
  case 14://48kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V92,,,48000,,48000");
		break;
  case 15://56kbps
		strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V92");
		break;
  default:
		if(is_sync)strcat(frame_pool,"%C0\\N0$F2S17=15+A8E=,,,0;+ES=6,,8;+ESA=0,0,,,1,0");
		else strcat(frame_pool,"\\V1&Q5+A8E=,,,0;+MS=V32B");
		break;
  }
  strcat(frame_pool,"\r");
  task_p1=2;
STEP2:
  a=step_send_to_smodem(frame_pool,&dn);
  if(!a&&task_p2)return 0;
  if(a)
  {
		modem_status=0xc1;
		return 3;
  }

  for(i=0;i<ex_cmd.cmd_count;i++)
  {
	  strcpy(frame_pool,ex_cmd.at_cmd[i]);
	  task_p1=3;
STEP3:
	  a=step_send_to_smodem(frame_pool,&dn);
	  if(!a&&task_p2)return 0;
	  if(a)
	  {
			modem_status=0xc2;
			return 4;
	  }
  }//for(i)

	req_write=resp_write=resp_head=0;
	led_control.callee_skips=0;
	led_control.callee_lighted=0;

	task_p1=0;
	modem_status=0x04;
	//set RTS,DTR low for on
	SET_RTS(LOW_LEVEL);
	SET_DTR(LOW_LEVEL);
	
  return 0;
}

uchar si_callee_setup(void)
//--use frame_pool as temporary buffer
{
	static uchar a,speed_type,n;
	static ushort dn,carrier_loss,i;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 21:goto STEP21;
	case 3:goto STEP3;
	case 4:goto STEP4;
	case 41:goto STEP41;
	case 5:goto STEP5;
	case 6:goto STEP6;
	case 7:goto STEP7;
	case 8:goto STEP8;
	case 9:goto STEP9;
	case 10:goto STEP10;
	default:return 100;
	}

STEP0:
  modem_status=0x0a;
  task_p2=0;

  a=(modem_config.pattern>>5)&0x03;
  if(is_sync)
  {
	if(a<2)a+=1;
	else if(a==2)a=5;
	else a=1;//forced to 1200bps for other sync speeds
  }
  else
  {
	n=modem_config.async_format&0x0f;
	if(n==0x08 || n==0x09)a=0;
	else if(modem_config.async_format>>4)a=modem_config.async_format>>4;
	else if(a<2)a+=1;//1-1200,2-2400
	else if(a==2)a=5;//9600
	else a=7;//14400

	if(mdm_chip_type==5 && a>7)a=7;
	if(mdm_chip_type==3 && a>13)a=13;
  }
  speed_type=a;
  asyn_form=modem_config.async_format&0x0f;

	//PortTx(P_MODEM,'X');//wake modem up
	//--wait for modem hangs up
	while(tmm_remains)
	{
		task_p1=1;
STEP1:
		if(tmm_remains)return 0;
	}

	//--clear serial port 1 buffer
	for(dn=0;dn<25;dn++)
	{
		a=PortRx(P_MODEM,frame_pool);
	}

	//1----do reset
	strcpy(frame_pool,"ATZ\r");
	task_p1=2;
STEP2:
	a=step_send_to_smodem(frame_pool,&dn);
	if(!a&&task_p2)return 0;
	if(a)
	{
		modem_status=0xc0;
		return 1;
	}

	strcpy(frame_pool,"AT+IPR=115200\r");
	task_p1=21;
STEP21:
	a=step_send_to_smodem(frame_pool,&dn);
	if(!a&&task_p2)return 0;
	if(a)
	{
		modem_status=0xc1;
		return 2;
	}

	//2----set flow control method,carrier wait time/connection time
	if(ex_cmd.connect_wait)
	{
		dn=ex_cmd.connect_wait;
		if(dn<5)dn=5;
		else if(dn>255)dn=255;
	}
	else
	{
		dn=(modem_config.pattern&0x07)*3+5;
		if(modem_config.ignore_dialtone&0x40)dn*=2;
	}
	if(!speed_type)carrier_loss=255;//240 for 4 minutes,255 no hangup
	else if(speed_type<3)carrier_loss=14;//14
	else carrier_loss=14;//2
	//N0:wire mode
	//\Q2:CTS,\Q3:RTS/CTS,\Q4:XON/XOFF
	//X4:full report
	//&D2--use ESC_PIN to hang-off
	//&D3--use ESC_PIN to reset
	//U69,set MODE bit=1 for on-hook line voltage
	//U70--d15:enable ESC_PIN,d13:enable +++ escape
	//S7:carrier wait time,S10:carrier loss time to disconnect
	//S10:carrier loss timer

	//NOTE--if the header 20 bytes modified,below two lines might be modified
	sprintf(frame_pool,"ATE0S0=0\\Q2\\N0X4&D2S7=%uS10=%u:U69,4,A000\r",dn,carrier_loss);
	if(!is_sync&&speed_type>2)frame_pool[13]='3';
	if(asyn_form==8 || asyn_form==9)frame_pool[18]='3';
	task_p1=3;
STEP3:
	a=step_send_to_smodem(frame_pool,&dn);
	if(!a&&task_p2)return 0;
	if(a)
	{
		modem_status=0xc2;
		return 2;
	}

	//3----set V29FC and FSK mode
	if(is_sync||!speed_type)
	{
		strcpy(frame_pool,"AT+FCLASS=");
		if(speed_type==5)
			strcat(frame_pool,"1\r");
		else if(!speed_type)strcat(frame_pool,"256\r");
		else	strcat(frame_pool,"0\r");
		task_p1=4;
STEP4:
		a=step_send_to_smodem(frame_pool,&dn);
		if(!a&&task_p2)return 0;
//printk(" c5:%d,dn:%d,%s\r\n",a,dn,tmps);
		if(a)
		{
			modem_status=0xc3;
			return 3;
		}

		if(speed_type==5)
			strcpy(frame_pool,"AT+IFC=0,2\r");
		else if(!speed_type)strcpy(frame_pool,"AT+IFC=0,2;:U52,0\r");
		else	strcpy(frame_pool,"AT+ITF=0383,0128\r");
		task_p1=41;
STEP41:
		a=step_send_to_smodem(frame_pool,&dn);
		if(!a&&task_p2)return 0;
//printk(" c51:%d,dn:%d,%s\r\n",a,dn,tmps);
		if(a)
		{
			modem_status=0xcb;
			return 31;
		}
	}

	//4----set connection speed and communication format
	strcpy(frame_pool,"AT");
	switch(speed_type)//callee
	{
	case 0://1200bps,half duplex
		//:UCA,d0--0:BELL202,1:V.23
		//BELL202--mark:1200Hz,space:2200Hz; V.23--mark:1300Hz,space:2100Hz
		//:UAA,d2--enable rude disconnect
		if((modem_config.async_format&0x0f)==0x09)//V.23
			strcat(frame_pool,":UCA,1;:UCD|2;:UAA,4;:UD4,7FFF;+ITF=383,128;");//V.23
		else strcat(frame_pool,":UCA,0;:UCD|2;:UAA,4;:UD4,7FFF;+ITF=383,128;");//BELL202
		SET_RTS(1);//set RTS high as off
		break;
	case 1://1200bps
		if(is_sync)
			strcat(frame_pool,"+ES=6,,8;+ESA=0,0,,,1;+IFC=0,2;+MS=V22");
		else
			strcat(frame_pool,"\\N0&H7");//V.22,1200bps
		break;
	case 2://2400bps
		if(is_sync)
			strcat(frame_pool,"+ES=6,,8;+ESA=0,0,,,1;+IFC=0,2;+MS=V22B");
		else
			strcat(frame_pool,"&H6");//V.22bis,2400/1200bps
		break;
	case 3://4800bps
		strcat(frame_pool,"&H4&G5");//4800bps max
		break;
	case 4://7200bps
		strcat(frame_pool,"&H4&G6");//7200bps max
		break;
	case 5://9600bps
		if(is_sync)
		{
			strcat(frame_pool,"\\V2+ES=6,,8;+ESA=0,0,,,1;:UAA,4;+ITF=383,128");
			SET_RTS(1);//set RTS high as off
		}
		else
			strcat(frame_pool,"&H4&G7");//9600bps max
		break;
	case 6://12.0kbps
		strcat(frame_pool,"&H4&G8");//12kbps max
		break;
	case 7://14.4kbps
		strcat(frame_pool,"&H4&G9");//14.4kbps max
		break;
	case 8://19.2kbps
		strcat(frame_pool,"&H2&G11");//19.2kbps max
		break;
	case 9://24kbps
		strcat(frame_pool,"&H2&G13");//24kbps max
		break;
	case 10://26.4kbps
		strcat(frame_pool,"&H2&G14");//26.4kbps max
		break;
	case 11://28.8kbps
		strcat(frame_pool,"&H2&G15");//28.8kbps max
		break;
	case 12://31.2kbps
		strcat(frame_pool,"&H2&G16");//31.2kbps max
		break;
	case 13://33.6kbps
		strcat(frame_pool,"&H2&G17");//33.6kbps max
		break;
	case 14://48kbps
		strcat(frame_pool,"&H0");
		break;
	case 15://56kbps
		strcat(frame_pool,"&H0");
		break;
	default://1200bps for sync,14.4kbps for async
		if(is_sync)
			strcat(frame_pool,"+ES=6,,8;+ESA=0,0,,,1,0;+IFC=0,2;+MS=V22");
		else
			strcat(frame_pool,"&H4&G9");//14.4kbps max
		break;
	}
	//--communication format
	if(!is_sync)
	{
		a=modem_config.async_format&0x0f;//async format
		switch(a)
		{
		case 1:
			strcat(frame_pool,"\\P0\\B5");//8E1
			break;
		case 2:
			strcat(frame_pool,"\\P2\\B5");//8O1
			break;
		case 3:
			strcat(frame_pool,"\\B1");//7N1
			break;
		case 4:
			strcat(frame_pool,"\\P0\\B2");//7E1
			break;
		case 5:
			strcat(frame_pool,"\\P2\\B2");//7O1
			break;
		default:
			strcat(frame_pool,"\\B3");//8N1
			break;
		}
	}
  strcat(frame_pool,"\r");
  task_p1=5;
STEP5:
  a=step_send_to_smodem(frame_pool,&dn);
  if(!a&&task_p2)return 0;
  if(a)
  {
		modem_status=0xc4;
		return 4;
  }

	//5--set SDLC/ASYNC mode
	strcpy(frame_pool,"AT");
	if(!is_sync)
	{
		switch(speed_type)
		{
		case 0://FSK
			//U52--transmit level
			//UD7--mark detect bits threshold
			//UD2--RX timeout in 10ms
			//U7A--enable chinese EPOS SMS,i.e FSK
			//+VNH--auto hangup control,1:not hangup
			//+VCID--0:no CID,1:formatted CID output,2:raw data CID output
			//strcat(frame_pool,":U7A|4000;:UD7,14;:UD2,64;:U52,0;+VNH=1;+VCID=2");//ASYNC
			//strcat(frame_pool,":U7A|4000;:UD7,14;:UD1,3C,FA;+VNH=1;+VCID=2");//ASYNC
			strcat(frame_pool,":U7A|4000;:UD7,14;:UD2,64;+VNH=1;+VCID=2");//ASYNC
			break;
		case 1:
			if(!(modem_config.ignore_dialtone&0x80))//fast connect
				strcat(frame_pool,"\\V4:U7A,0001;:U1D4|4000");
			else
				strcat(frame_pool,"\\V4:U7A,0000;:U1D4|4000");
			break;
		case 2:
			if(!(modem_config.ignore_dialtone&0x80))//standard connect
				strcat(frame_pool,"\\V4:U80,012C;:U7A,0000;:U1D4|4000");
			else
				strcat(frame_pool,"\\V4:U80,012C;:U7A,0001;:U1D4|4000");
			break;
		default:
			//strcat(frame_pool,"\\V4:U7A,0000;+VCID=2");
			strcat(frame_pool,"\\V4:U7A,0000");
			break;
		}//switch(speed_type)
	}
	else
	{
		//--U87 Synchronous Access Mode
		strcat(frame_pool,":U87,050A;");
		switch(speed_type)
		{
		case 1:
			//U1D4 D14--enable V.29FC and some V.22 and V.22FC improvements,other bits reserved
			if(!(modem_config.ignore_dialtone&0x80))//fast connect
				strcat(frame_pool,":U7A,0003;:U1D4|4000");
			else
				strcat(frame_pool,":U7A,0002;:U1D4|4000");
			break;
		case 2:
			//U80 D15-enable non-answer-tone connect,d14~d0--delay time in 1/600s
			//    d4-new FC;d1-1:hdlc,0-async;d0-1:fast connect,0-normal
			if(!(modem_config.ignore_dialtone&0x80))//standard connect
				strcat(frame_pool,":U80,012C;:U7A,0002;:U1D4|4000");
			else
				strcat(frame_pool,":U80,012C;:U7A,0003;:U1D4|4000");
			break;
		case 5:
			//UD3:V29C answer tone detect threshold(50~180ms)
			//strcat(frame_pool,":U7A,2009;:U1D4|4000;:UD3,B4");//d13:RTS toggle for direction
			strcat(frame_pool,":U7A,2009;:U1D4|4000;:UD3,32");//d13:RTS toggle for direction
			break;
		}//switch(speed_type)
	}
  strcat(frame_pool,"\r");
  task_p1=6;
STEP6:
  a=step_send_to_smodem(frame_pool,&dn);
  if(!a&&task_p2)return 0;
  if(a)
  {
		modem_status=0xc5;
		return 5;
  }

  //6--set ring minimum on time to 250ms(V*2400)
  //U6E--d12~d8:CLKOUT divider,0-disable
  strcpy(frame_pool,"AT:U4B,258;:U6E&E0FF");
  //--if FSK,set CID detection mode:detection always on
  if(!speed_type)strcat(frame_pool,";+VCDT=1");
  strcat(frame_pool,"\r");
  task_p1=7;
STEP7:
  a=step_send_to_smodem(frame_pool,&dn);
  if(!a&&task_p2)return 0;
  if(a)
  {
		modem_status=0xc6;
		return 6;
  }

	//7--if FSK,set voice mode and DTMF detection
  if(!speed_type)
  {
	  //7.1--switch to voice mode
	  strcpy(frame_pool,"AT+FCLASS=8\r");
	  task_p1=8;
STEP8:
	  a=step_send_to_smodem(frame_pool,&dn);
	  if(!a&&task_p2)return 0;
	  if(a)
	  {
			modem_status=0xc7;
			return 7;
	  }

	  //7.2--set DTMF detection
	  strcpy(frame_pool,"AT+VLS=14\r");
	  task_p1=9;
STEP9:
	  a=step_send_to_smodem(frame_pool,&dn);
	  if(!a&&task_p2)return 0;
	  if(a)
	  {
			modem_status=0xc8;
			return 8;
	  }
  }//if(FSK)

  //10--send ExCommands
  for(i=0;i<ex_cmd.cmd_count;i++)
  {
	  strcpy(frame_pool,ex_cmd.at_cmd[i]);
	  task_p1=10;
STEP10:
	  a=step_send_to_smodem(frame_pool,&dn);
	  if(!a&&task_p2)return 0;
	  if(a)
	  {
			modem_status=0xca;
			return 10;
	  }
  }//for(i)

	req_write=resp_write=resp_head=0;
	led_control.callee_skips=0;
	led_control.callee_lighted=0;

	task_p1=0;
	modem_status=0x04;
	//set RTS,DTR low for on
	SET_RTS(LOW_LEVEL);
	SET_DTR(LOW_LEVEL);
	
  return 0;
}

static void store_response(uchar *str,ushort dlen)
{
	ushort i;

	  for(i=0;i<dlen;i++)
			if(resp_write<DATA_SIZE+5)//Enlarged 5 on 2011.1.5
			{
				response_pool[(resp_head+resp_write)%FRAME_SIZE]=str[i];
				resp_write++;
			}
}

static uchar htoc(uchar *in)
{
	uchar tmpc,a;

	tmpc=toupper(in[0]);
	if(tmpc>='A'&&tmpc<='F')tmpc=tmpc-('A'-0x0A);
	else      tmpc&=0x0f;
	a=tmpc<<4;

	tmpc=toupper(in[1]);
	if(tmpc>='A'&&tmpc<='F')tmpc=tmpc-('A'-0x0A);
	else         tmpc&=0x0f;
	a+=tmpc;

	return a;
}

static void del_msb(uchar *io,ushort dn)
{
	ushort i;

	for(i=0;i<dn;i++)
		if(io[i]=='B')io[i]='3';
}

static uchar cx_callee_connect(void)
//--use frame_pool as temporary buffer
{
	static uchar a,resp,flashes,tmps[40];
	static ushort dn,tmpu,tmpu_data;
	static uchar light_run;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	case 4:goto STEP4;
	case 5:goto STEP5;
	case 6:goto STEP6;
	case 7:goto STEP7;
	default:return 100;
	}

STEP0:
	resp=0;
	has_picked=0;
	light_run=0;
	task_p2=0;

  //0--detect ring indicator
	memset(frame_pool,0x00,3);
	task_p1=1;
STEP1:
	a=step_send_to_smodem(frame_pool,&dn);
	if(!a&&task_p2)return 0;
//if(dn)s_UartPrint(" a:%d,dn:%d,%s",a,dn,frame_pool);
	if(a && a!=2)
	{
		modem_status=0xb0;
		resp=1;
		goto C_TAIL;
	}

	if(!dn || memcmp(frame_pool+2,"RING",4))
	{
	if(led_control.callee_skips)
	{
		led_control.callee_skips++;
		light_run=1;
		if(led_control.callee_skips>=30)//30
			led_control.callee_skips=0;
		resp=4;
		goto C_TAIL;
	}

	led_control.callee_skips++;

  //1--detect on-hook line voltage
	strcpy(frame_pool,"AT-TRV\r");
	task_p1=2;
STEP2:
	a=step_send_to_smodem(frame_pool,&dn);
	if(!a&&task_p2)return 0;
//if(dn)s_UartPrint(" trv:%d,dn:%d,%s",a,dn,frame_pool);
	if(a)
	{
		modem_status=0xb1;
		resp=5;
		goto C_TAIL;
	}

	if(dn<5 || memcmp(frame_pool,"\x0d\x0a",2))
	{
		modem_status=0xb2;
		resp=7;
		goto C_TAIL;
	}
	for(tmpu=0;tmpu<3;tmpu++)
		if(frame_pool[2+tmpu]=='.')break;
	frame_pool[2+tmpu]=0;
	tmpu=atoi(frame_pool+2);
	if(tmpu<2)
	{
		modem_status=0x33;//0x03,no_line
		resp=8;
	}
	else if(tmpu<19)
	{
		LED_ONLINE_OFF;
		ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
		led_control.callee_lighted=0;
		resp=9;
		modem_status=0xb3;
	}
	else
	{
		if(!led_control.callee_lighted)
			{LED_ONLINE_ON;led_control.callee_lighted=1;ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);}
		else {LED_ONLINE_OFF; led_control.callee_lighted=0;ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);}
		resp=10;
		light_run=1;
		modem_status=0x0b;
	}

	goto C_TAIL;
  }//line free

  //2--reports the ring info
	if(!is_sync)
	{
		if(!answer_interface)store_response("\r\nRING\r\n",8);
		else store_response("\x02\x03\x00\x00\x03",5);
	}

  //3--reads the call number and reports to application
	a=modem_config.async_format&0x0f;
	if(a==0x08 || a==0x09)
	{
		frame_pool[0]=0;frame_pool[2]='C';
		task_p1=3;
STEP3:
		a=step_send_to_smodem(frame_pool,&dn);
		if(!a&&task_p2)return 0;
		if(dn>4 && !answer_interface)
		{
			if(!memcmp(frame_pool,"\r\n04",4))//simple data format
			{
				store_response("\r\nCID:",6);
				if(dn>8)del_msb(frame_pool+6,dn-8);
				store_response(frame_pool+4,dn-4);
			}
			else if(!memcmp(frame_pool,"\r\n80",4))//complex data format
			{
				store_response("\r\nCID:",6);

				//31--sum and output the total length
				for(a=8,tmpu=6;tmpu<dn;)
				{
					if(memcmp(frame_pool+tmpu,"02",2))//if not tel_no field
					{
						tmpu+=htoc(frame_pool+tmpu+2)*2+4;
						continue;
					}
					//if tel_no field
					a+=htoc(frame_pool+tmpu+2);//length
					break;
				}
				sprintf(tmps,"%02X",a);
				store_response(tmps,2);//output the total length

				//32--fetch and output the 8-byte time field
				for(tmpu=6;tmpu<dn;)
				{
					if(memcmp(frame_pool+tmpu,"01",2))//if not tel_no field
						tmpu+=htoc(frame_pool+tmpu+2)*2+4;
					else
					{
						//output time field
						del_msb(frame_pool+tmpu+4,16);
						store_response(frame_pool+tmpu+4,16);
						break;
					}
				}
				//output time field
				if(tmpu>=dn)
				{
					memset(tmps,'0',16);
					store_response(tmps,16);
				}

				//33--fetch and output the tel_no field
				for(tmpu=6;tmpu<dn;)
				{
					if(memcmp(frame_pool+tmpu,"02",2))//if not tel_no field
					{
						tmpu+=htoc(frame_pool+tmpu+2)*2+4;
						continue;
					}
					a=htoc(frame_pool+tmpu+2)*2;//length
					del_msb(frame_pool+tmpu+4,a);
					store_response(frame_pool+tmpu+4,a);
					break;
				}
				store_response("FF\r\n",4);
			}
			else store_response(frame_pool,dn);
		}

		//--wait 30ms for application to read
		tm01_remains=3;
		while(tm01_remains)
		{
			task_p1=4;
STEP4:
			if(tm01_remains)return 0;
		}
	}

  //4--answers the call
	has_picked=1;
	SET_DTR(LOW_LEVEL);//set RELAY,DTR low
	RELAY_OFF;
	a=modem_config.async_format&0x0f;
	if(a==0x08 || a==0x09)
		SET_RTS(HIGH_LEVEL);//set RTS high as off
	else SET_RTS(LOW_LEVEL);//set RTS low as on

	led_control.open=0;
	LED_ONLINE_OFF;
	strcpy(frame_pool,"ATA\r");
	if(ex_cmd.connect_wait)
	{
		dn=ex_cmd.connect_wait;
		if(dn<5)dn=5;
		else if(dn>255)dn=255;
		dn+=3;
		dn*=100;
	}
	else
	{
		dn=(modem_config.pattern&0x07)*300+800;//set wait time
		if(modem_config.ignore_dialtone&0x40)dn*=2;
	}
	LED_TXD_ON;
	ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);
	task_p1=5;
STEP5:
	a=step_send_to_smodem(frame_pool,&dn);
	if(!a&&task_p2)return 0;
//if(dn)s_UartPrint(" ata:%d,dn:%d,%s",a,dn,frame_pool);
	LED_TXD_OFF;ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	if(a)
	{
		if(!is_sync)
		{
			if(!answer_interface)store_response("\r\nTIMEOUT\r\n",11);
			else store_response("\x02\x06\x00\x00\x06",5);
		}

		modem_status=0xb5;
		resp=11;
		goto C_TAIL;
	}

	if(dn<=10 || memcmp(frame_pool+2,"CONNECT",7))
	{
		resp=13;
		modem_status=0xb8;
		goto C_TAIL;
	}

	flashes=0;
	if(is_sync)
	{
		if(dn>=13 && !memcmp(frame_pool,"\r\nCONNECT V29",13))
			is_half_duplex=1;
		else is_half_duplex=0;

		task_p1=6;
STEP6:
		a=step_rcv_v80_cmd(0,&dn);
		if(!a&&task_p2)return 0;
		if(!a)
		{
			a=frame_pool[0];
			if(a==0x20)flashes=1;
			else if(a==0x21)flashes=2;
			else if(a==0x24)flashes=5;
			else if(a==0x22)flashes=3;
			else if(a==0x23)flashes=4;
		}
	}//if(is_sync)
	else
	{
		for(tmpu=dn-1;;tmpu--)
		{
			if(frame_pool[tmpu]=='/'||frame_pool[tmpu]==0x20)break;
			if(!tmpu)break;
		}

		if(!tmpu || memcmp(frame_pool+dn-2,"\r\n",2))
		{
			modem_status=0xb6;
			return 20;
		}

		a=frame_pool[dn-2];//store the byte
		frame_pool[dn-2]=0;
		tmpu_data=atoi(frame_pool+tmpu+1);
		frame_pool[dn-2]=a;//recover the byte
		if(!tmpu_data && dn>=14 && !memcmp(frame_pool,"\r\nCONNECT V.23",14))
			flashes=1;
		else if(tmpu_data==1200)flashes=1;
		else if(tmpu_data==2400)flashes=2;
		else if(tmpu_data==9600)flashes=5;
		else if(tmpu_data==14400)flashes=7;
		else if(tmpu_data==4800)flashes=3;
		else if(tmpu_data==7200)flashes=4;
		else if(tmpu_data==12000)flashes=6;
		else if(tmpu_data>14400 && tmpu_data<=19200)flashes=8;
		else if(tmpu_data>19200 && tmpu_data<=24000)flashes=9;
		else if(tmpu_data==26400)flashes=10;
		else if(tmpu_data==28800)flashes=11;
		else if(tmpu_data==31200)flashes=12;
		else if(tmpu_data==33600)flashes=13;
		else if(tmpu_data>33600 && tmpu_data<=48000)flashes=14;
		else if(tmpu_data>48000)flashes=15;
		else
		{
			modem_status=0xb7;
			if(!answer_interface)store_response("\r\nNO CARRIER\r\n",14);
			else	store_response("\x02\x06\x00\x00\x06",5);
			resp=12;
			goto C_TAIL;
		}
	}

	//--wait 100ms to avoid callee hang-off in 1200/2400bps
	if(!is_sync && flashes<3)
	{
		tm01_remains=10;
		while(tm01_remains)
		{
			task_p1=7;
STEP7:
			if(tm01_remains)return 0;
		}
	}

	modem_status=0x00;
	if(modem_config.idle_timeout>90)modem_config.idle_timeout=90;//enlarged to 90 from 65,2009.4.29
	tmm_remains=modem_config.idle_timeout*1000;
	led_control.callee_lighted=0;
	led_auto_flash(N_LED_ONLINE,flashes,1,0);

	if(!is_sync)
	{
		if(!answer_interface)store_response(frame_pool,dn);
		else	store_response("\x02\x04\x00\x00\x04",5);
		LED_ONLINE_ON;
		ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
	}

	task_p1=0;
	return 0;

C_TAIL:
  if(!light_run)
  {
	LED_ONLINE_OFF;
	ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	led_control.callee_lighted=0;
  }
  task_p1=0;
  return resp;
}

//--fetch FSK or DTMF CID from silicon modem
static uchar si_step_rcv_cid(uchar *str,ushort *dlen)
{
	static uchar resp,tmpc,a,dle_found,ring_found,cid_found,dtmf_found;
	static uchar dtmf_code[100];
	static ushort i;

	switch(task_p2)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	}

STEP0:
	resp=0;
	i=0;
	*dlen=0;
	dle_found=0;
	ring_found=0;
	cid_found=0;
	dtmf_found=0;

	//--formated TYPE II FSK:
	//105210584349444D0D0A444154453D303531330D0A54494D453D313731320D0A4E4D42523D38363135353436390D0A102E
	//1052
	//1071
	//--raw TYPE I FSK:
	//10,52,
	//10,58,10,2E,0D,0A,43,49,44,4D,04,10,30,35,31,33,31,37,34,32,38,36,
	//31,35,35,34,36,39,A9,0D,0A,4E,4F,20,43,41,52,52,49,45,52,0D,0A,
	//10,52,
	//10,71,
	//--raw DTMF:
	//10,2F,10,44,10,7E,10,2F,10,35,10,7E,10,2F,10,34,10,7E,10,2F,10,33,10,7E,
	//10,2F,10,32,10,7E,10,2F,10,31,10,7E,10,2F,10,30,10,7E,10,2F,10,39,10,7E,
	//10,2F,10,38,10,7E,10,2F,10,37,10,7E,10,2F,10,36,10,7E,10,2F,10,30,10,7E,
	//10,2F,10,32,10,7E,10,2F,10,43,10,7E,10,52,10,71,
	tm02_remains=200;
	while(1)
	{
STEP1:
		tmpc=PortRx(P_MODEM,&a);
		if(tmpc)
		{
			if(!tm02_remains)
			{
				resp=1;
				break;
			}

			task_p2=1;
			return 0;
		}
//printk("%02X ",a);

		if(dtmf_found!=2)tm02_remains=200;
		else tm02_remains=500;//require 5s for following ring

		if(cid_found)
		{
			str[i]=a;
//printk("%d:%02X ",i,a);
			i++;
			//--get FSK CID
			if(i>=14 && !memcmp(str+i-14,"\r\nNO CARRIER\r\n",14))break;
			continue;
		}

		if(!dle_found)
		{
			if(a==0x10)dle_found=1;
			continue;
		}

		if(dtmf_found==1 && i<sizeof(dtmf_code))
		{
			dtmf_code[i]=a;
			i++;
			dtmf_found=2;
			continue;
		}

		switch(a)
		{
		case 'R'://Ring incoming
			ring_found++;
			break;
		case 'X'://Complex event report header
			break;
		case '.'://Complex event report terminator
			cid_found=1;
			break;
		case '/'://DTMF tone detected
			dtmf_found=1;
			break;
		}//switch()

		dle_found=0;

		if(dtmf_found==2 && ring_found)
		{
			memcpy(str,"DTMF:",5);

			//--trim the leading alpha code if more than 2 codes
			a=0;
			tmpc=dtmf_code[0];
			if(i>2 && tmpc>='A' && tmpc>='D'){i--;a=1;}
			memcpy(str+5,dtmf_code+a,i);

			//--trim the tail alpha code if more than 2 codes
			if(i>2 && str[i+4]>='A' && str[i+4]<='D')i--;

			//--append a tail to be same as FSK CID report
			memcpy(str+5+i,"NO CARRIER\r\n",12);
			i+=17;//5+12
			break;
		}
	}//while(1)

//printk("+++");
	str[i]=0;//append tail zero
	*dlen=i;
	task_p2=0;
	return resp;
}

static uchar si_callee_connect(void)
//--use frame_pool as temporary buffer
{
	static uchar a,resp,flashes,tmps[40];
	static ushort dn,tmpu,tmpu_data,i;
	static uchar light_run,is_fsk,skip_limit;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	case 4:goto STEP4;
	case 40:goto STEP40;
	case 41:goto STEP41;
	case 42:goto STEP42;
	case 5:goto STEP5;
	case 6:goto STEP6;
	case 7:goto STEP7;
	default:return 100;
	}

STEP0:
	resp=0;
	has_picked=0;
	light_run=0;
	task_p2=0;
	is_fsk=0;
	a=modem_config.async_format&0x0f;
	if(a==8 || a==9)is_fsk=1;

  //0--detect ring indicator
	strcpy(frame_pool,"AT:I\r");//interrupt source
	task_p1=1;
STEP1:
	if(!is_fsk)a=step_send_to_smodem(frame_pool,&dn);
	else a=si_step_rcv_cid(frame_pool,&dn);
	if(!a&&task_p2)return 0;
//if(dn)printk(" a:%d,dn:%d.\n",a,dn);
	if(a)
	{
		skip_limit=2;
		if(is_fsk)goto CALLEE_IDLE;

		modem_status=0xb0;
		resp=1;
		goto C_TAIL;
	}
	if(!is_fsk&&dn!=12&&dn!=20)
	{
		resp=2;
		goto C_TAIL;
	}

	if(!is_fsk && (dn==12&&!(frame_pool[3]&0x02) ||
						dn==20&&memcmp(frame_pool+2,"RING",4)))
	{
		skip_limit=10;
CALLEE_IDLE:
		if(led_control.callee_skips)
		{
			led_control.callee_skips++;
			light_run=1;
			if(led_control.callee_skips>=skip_limit)
				led_control.callee_skips=0;

			//--wait for 300ms
			tm01_remains=30;
			while(tm01_remains)
			{
				task_p1=2;
STEP2:
				if(tm01_remains)return 0;
			}

			resp=3;
			goto C_TAIL;
		}

		led_control.callee_skips++;

	  //1--detect on-hook line voltage
		strcpy(frame_pool,"AT:R79\r");
		task_p1=3;
STEP3:
		a=step_send_to_smodem(frame_pool,&dn);
		if(!a&&task_p2)return 0;
//if(dn)printk(" read line:%d,dn:%d,%s",a,dn,frame_pool);
		if(a)
		{
			modem_status=0xb1;
			resp=4;
			goto C_TAIL;
		}

		if(dn!=16&&dn!=14)
		{
			modem_status=0xb2;
			resp=5;
			goto C_TAIL;
		}
		if(dn==16)two_one(frame_pool+6,2,&a);
		else two_one(frame_pool+4,2,&a);
		tmpu=a*275;
		if(!tmpu)
		{
			modem_status=0x03;
			resp=6;
		}
		else if(tmpu<19)
		{
			LED_ONLINE_OFF;
			ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
			led_control.callee_lighted=0;
			resp=7;
			modem_status=0xb3;
		}
		else
		{
			if(!led_control.callee_lighted)
				{LED_ONLINE_ON;led_control.callee_lighted=1;ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);}
			else {LED_ONLINE_OFF; led_control.callee_lighted=0;ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);}
			resp=8;
			light_run=1;
			modem_status=0x0b;
		}

		goto C_TAIL;
  }//line free

  //2--reports the ring info
	if(!is_sync)
	{
		if(!answer_interface)store_response("\r\nRING\r\n",8);
		else store_response("\x02\x03\x00\x00\x03",5);
	}

  //3--reads the call number and reports to application
	if(is_fsk)
	{
		if(dn>7 && !answer_interface)
		{
			//--trim tail NO_CARRIER bytes
			if(dn>=12&&!memcmp(frame_pool+dn-12,"NO CARRIER\r\n",12))dn-=12;

			if(dn>=11&&!memcmp(frame_pool,"\r\nCIDM\x04",7))//simple data format
			{
				//simple CID reported by silicon modem:
				//\r\nCIDM\x04\x100420165586155469\xA9\r\nNO CARRIER\r\n
				store_response("\r\nCID:",6);
				for(i=7;i<dn-2;i++)
				{
					sprintf(tmps,"%02X",frame_pool[i]);
					if(i!=dn-3)del_msb(tmps,2);
					store_response(tmps,2);
				}
				store_response(frame_pool+dn-2,2);
			}
			else if(dn>=11&&!memcmp(frame_pool,"\r\nCIDM\x80",7))//complex data format
			{
				//complex CID reported by silicon modem:
				//\r\nCIDM\x80\x21
				//\x01\x08,B0,38,32,31,32,B0,B3,31,
				//02,0C,32,B9,B0,31,32,B3,34,B5,B6,37,B0,31,
				//07,07,C2,E5,E9,EA,E9,6E,67,89,
				//\r\nNO CARRIER\r\n
				store_response("\r\nCID:",6);

				//321--sum and output the total length
				for(a=8,i=8;i<dn;)
				{
					if(frame_pool[i]!=0x02)//if not tel_no field
					{
						i+=frame_pool[i+1]+2;
						continue;
					}
					//if tel_no field
					a+=frame_pool[i+1];//add tel_no length to time length
					break;
				}
				sprintf(tmps,"%02X",a);
				store_response(tmps,2);//output the total length

				//322--fetch and output the 8-byte time field
				for(i=8;i<dn;)
				{
					if(frame_pool[i]!=0x01)//if not tel_no field
					{
						i+=frame_pool[i+1]+2;
						continue;
					}

					//output time field
					for(tmpu=0;tmpu<8;tmpu++)
					{
						sprintf(tmps,"%02X",frame_pool[i+2+tmpu]);
						del_msb(tmps,2);
						store_response(tmps,2);
					}
					break;
				}
				//--if blank time,output time field with '0'
				if(i>=dn)
				{
					memset(tmps,'0',16);
					store_response(tmps,16);
				}

				//323--fetch and output the tel_no field
				for(i=8;i<dn;)
				{
					if(frame_pool[i]!=0x02)//if not tel_no field
					{
						i+=frame_pool[i+1]+2;
						continue;
					}

					//output tel_no field
					a=frame_pool[i+1];//tel_no length
					for(tmpu=0;tmpu<a;tmpu++)
					{
						sprintf(tmps,"%02X",frame_pool[i+2+tmpu]);
						del_msb(tmps,2);
						store_response(tmps,2);
					}
					break;
				}
				store_response("FF\r\n",4);
			}
			else if(dn>=5&&!memcmp(frame_pool,"DTMF:",5))//DTMF data format
			{
				store_response("\r\nCID:",6);

				//331--sum and output the total length
				sprintf(tmps,"%02X",dn-5+16);
				store_response(tmps,2);//output the total length

				//332--output time field with space
				memset(tmps,0x20,16);
				store_response(tmps,16);

				//333--output tel_no field
				a=dn-5;//tel_no length
				for(tmpu=0;tmpu<a;tmpu++)
				{
					sprintf(tmps,"%02X",frame_pool[i+5+tmpu]);
					store_response(tmps,2);
				}
				store_response("FF\r\n",4);
			}
			else
			{
				for(i=0;i<dn;i++)
				{
					sprintf(tmps,"%02X",frame_pool[i]);
					store_response(tmps,2);
				}//for(i)
			}
		}

		//--wait 30ms for application to read
		tm01_remains=3;
		while(tm01_remains)
		{
			task_p1=4;
STEP4:
			if(tm01_remains)return 0;
		}
	}//if(FSK callee)

  //4--If FSK,pick up the phone in voice mode
	has_picked=1;
	SET_DTR(0);//set RELAY,DTR low
	RELAY_OFF;
	if(!is_fsk) SET_RTS(0);//set RTS low as on
	else
	{
		SET_RTS(1);//set RTS high as off
		strcpy(frame_pool,"AT+FCLASS=8\r");
		task_p1=40;
STEP40:
		a=step_send_to_smodem(frame_pool,&dn);
		if(!a&&task_p2)return 0;
//printk(" pick up1:%d,%d,%s.",a,dn,frame_pool);
		if(a)
		{
			modem_status=0xb4;
			resp=5;
			goto C_TAIL;
		}

		//--pick up,stop tone detection
		strcpy(frame_pool,"AT+VLS=1\r");
		task_p1=41;
STEP41:
		a=step_send_to_smodem(frame_pool,&dn);
		if(!a&&task_p2)return 0;
//printk(" pick up2:%d,%d,%s.",a,dn,frame_pool);
		if(a)
		{
			modem_status=0xb4;
			resp=5;
			goto C_TAIL;
		}

		//--send CAS tone for 80ms
		//strcpy(frame_pool,"AT+VTS=[2130,2750,8]\r");
		strcpy(frame_pool,"AT+FCLASS=256\r");//return to FSK mode
		task_p1=42;
STEP42:
		a=step_send_to_smodem(frame_pool,&dn);
		if(!a&&task_p2)return 0;
//printk(" pick up3:%d,%d,%s.",a,dn,frame_pool);
		if(a)
		{
			modem_status=0xb9;
			resp=9;
			goto C_TAIL;
		}
	}

  //5--answers the call
	led_control.open=0;
	LED_ONLINE_OFF;
	//if(is_fsk)strcpy(frame_pool,"ATX1S6=1DT1;\r");
	if(is_fsk)
	  strcpy(frame_pool,"AT+VNH=1\r");//set not auto hangup
	else strcpy(frame_pool,"ATA\r");
	if(ex_cmd.connect_wait)
	{
		dn=ex_cmd.connect_wait;
		if(dn<5)dn=5;
		else if(dn>255)dn=255;
		dn+=3;
		dn*=100;
	}
	else
	{
		dn=(modem_config.pattern&0x07)*300+800;//set wait time
		if(modem_config.ignore_dialtone&0x40)dn*=2;
	}
	LED_TXD_ON;
	ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);
	task_p1=5;
STEP5:
	a=step_send_to_smodem(frame_pool,&dn);
	if(!a&&task_p2)return 0;
//printk(" ata:%d,dn:%d,%s",a,dn,frame_pool);
	LED_TXD_OFF;ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	if(a)
	{
		if(!is_sync)
		{
			if(!answer_interface)store_response("\r\nTIMEOUT\r\n",11);
			else store_response("\x02\x06\x00\x00\x06",5);
		}

		modem_status=0xb5;
		resp=11;
		goto C_TAIL;
	}

	//--translate the FSK response for uniform process
	if(is_fsk && dn>=6 && !memcmp(frame_pool,"\r\nOK\r\n",6))
	{
		strcpy(frame_pool,"\r\nCONNECT V.23\r\n");
		dn=16;
	}

	if(dn<=10 || memcmp(frame_pool+2,"CONNECT",7))
	{
		resp=13;
		modem_status=0xb8;
		goto C_TAIL;
	}

	flashes=0;
	if(is_sync)
	{
		if(dn>=13 && !memcmp(frame_pool,"\r\nCONNECT V29",13))
			is_half_duplex=1;
		else is_half_duplex=0;

		task_p1=6;
STEP6:
		a=step_rcv_v80_cmd(0,&dn);
		if(!a&&task_p2)return 0;
		if(!a)
		{
			a=frame_pool[0];
			if(a==0x20)flashes=1;
			else if(a==0x21)flashes=2;
			else if(a==0x24)flashes=5;
			else if(a==0x22)flashes=3;
			else if(a==0x23)flashes=4;
		}
	}//if(is_sync)
	else
	{
		for(tmpu=dn-1;;tmpu--)
		{
			if(frame_pool[tmpu]=='/'||frame_pool[tmpu]==0x20)break;
			if(!tmpu)break;
		}

		if(!tmpu || memcmp(frame_pool+dn-2,"\r\n",2))
		{
			modem_status=0xb6;
			return 20;
		}

		a=frame_pool[dn-2];//store the byte
		frame_pool[dn-2]=0;
		tmpu_data=atoi(frame_pool+tmpu+1);
		frame_pool[dn-2]=a;//recover the byte
		if(!tmpu_data && dn>=14 && !memcmp(frame_pool,"\r\nCONNECT V.23",14))
			flashes=1;
		else if(tmpu_data==1200)flashes=1;
		else if(tmpu_data==2400)flashes=2;
		else if(tmpu_data==9600)flashes=5;
		else if(tmpu_data==14400)flashes=7;
		else if(tmpu_data==4800)flashes=3;
		else if(tmpu_data==7200)flashes=4;
		else if(tmpu_data==12000)flashes=6;
		else if(tmpu_data>14400 && tmpu_data<=19200)flashes=8;
		else if(tmpu_data>19200 && tmpu_data<=24000)flashes=9;
		else if(tmpu_data==26400)flashes=10;
		else if(tmpu_data==28800)flashes=11;
		else if(tmpu_data==31200)flashes=12;
		else if(tmpu_data==33600)flashes=13;
		else if(tmpu_data>33600 && tmpu_data<=48000)flashes=14;
		else if(tmpu_data>48000)flashes=15;
		else
		{
			modem_status=0xb7;
			if(!answer_interface)store_response("\r\nNO CARRIER\r\n",14);
			else	store_response("\x02\x06\x00\x00\x06",5);
			resp=12;
			goto C_TAIL;
		}
	}

CONNECT_TAIL:
	//--if FSK,wait 100ms for modem to pick up
	if(is_fsk)
	{
		tm01_remains=50;//100
		while(tm01_remains)
		{
			task_p1=7;
STEP7:
			if(tm01_remains)return 0;
		}
	}

	modem_status=0x00;
	if(modem_config.idle_timeout>90)modem_config.idle_timeout=90;//enlarged to 90 from 65,2009.4.29
	tmm_remains=modem_config.idle_timeout*1000;
	led_control.callee_lighted=0;
	led_auto_flash(N_LED_ONLINE,flashes,1,0);

	if(!is_sync)
	{
		if(!answer_interface)store_response(frame_pool,dn);
		else	store_response("\x02\x04\x00\x00\x04",5);
		LED_ONLINE_ON;
		ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
	}

	task_p1=0;
	return 0;

C_TAIL:
  if(!light_run)
  {
	LED_ONLINE_OFF;
	ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	led_control.callee_lighted=0;
  }
  task_p1=0;
  return resp;
}

static void nac_pack_up(uchar *in_str,ushort *dlen)
//--use resp_pack[] as out_buffer[]
{
	uchar lrc;
	ushort i,dn;

	resp_pack[0]=0x02;
	dn=*dlen;
	resp_pack[1]=((dn/1000)<<4)+(((dn%1000)/100)&0x0f);
	resp_pack[2]=(((dn%100)/10)<<4)+(dn%10);
	memcpy(resp_pack+3,in_str,*dlen);
	resp_pack[dn+3]=0x03;
	for(i=1,lrc=0;i<dn+4;i++)lrc^=resp_pack[i];
	resp_pack[dn+4]=lrc;
	*dlen=dn+5;
	return;
}

uchar callee_sync_comm(void)
//--use frame_pool as temporary pool
{
	static uchar has_request,is_auto_reply,a,Nnac,Npos;
	static uchar s_retries,r_retries,lost_retries,tmpc_data,sn_retries;
	static ushort i,rlen,response_dlen;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 101:goto STEP101;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 3:goto STEP3;
	case 4:goto STEP4;
	case 5:goto STEP5;
	case 6:goto STEP6;
	case 7:goto STEP7;
	case 8:goto STEP8;
	case 9:goto STEP9;
	case 10:goto STEP10;
	case 11:goto STEP11;
	case 12:goto STEP12;
	case 13:goto STEP13;
	case 14:goto STEP14;
	case 15:goto STEP15;
	case 16:goto STEP16;
	case 17:goto STEP17;
	case 18:goto STEP18;
	case 19:goto STEP19;
	case 20:goto STEP20;
	default:return 100;
	}

STEP0:
	//--build the SDLC link
	s_retries=5;
	frame_address=0x30;

	//--wait for a moment
	tm01_remains=6;
	while(tm01_remains)
	{
		task_p1=101;
STEP101:
		if(tm01_remains)return 0;
	}

	for(i=0;i<s_retries;i++)
	{
		if(is_half_duplex)
		{
			task_p1=1;
STEP1:
			a=step_turn_to_tx();
			if(!a&&task_p2)return 0;
//s_UartPrint("TX SNRM WAIT,RESULT:%d\r\n",a);
		}
		//--send SNRM(set normal response mode)
		memcpy(frame_pool,"\x30\x93\x19\xb1",4);
		task_p1=2;
STEP2:
		a=step_send_to_serial1(frame_pool,4);
		if(!a&&task_p2)return 0;
		if(a)
		{
			modem_status=0xa0;
			return 1;
		}
//send_to_serial("\x99\x99",2);

		//--receive ACK from remote
		if(is_half_duplex)
		{
			task_p1=3;
STEP3:
			a=step_turn_to_rx();
			if(!a&&task_p2)return 0;
		}
		task_p1=4;
STEP4:
		a=step_rcv_v80_cmd(0,&rlen);
		if(!a&&task_p2)return 0;
//s_UartPrint("RCV UA,RESULT:%d,DN:%d,%02X%02X\r\n",a,rlen,frame_pool[0],frame_pool[1]);
		if(a==2)
		{
			modem_status=0xa1;
			return 2;
		}

		if(rlen==2&&frame_pool[1]==0x73)break;
	}

	if(i>=s_retries)
	{
		modem_status=0xa2;
		return 3;
	}

	if(is_half_duplex)
	{
		task_p1=5;
STEP5:
		a=step_turn_to_tx();
		if(!a&&task_p2)return 0;
//s_UartPrint("TX RR WAIT,RESULT:%d\r\n",a);
	}
	 Nnac=0;
	 Npos=0;
	 //send first RR frame
	 tmpc_data=(Npos<<5)+0x11;
	 rlen=0;
	 pack_sdlc_v80(tmpc_data,NULL,&rlen);
	 task_p1=6;
STEP6:
	 a=step_send_to_serial1(ci_pool,rlen);
	 if(!a&&task_p2)return 0;
	 if(a)
	 {
		modem_status=0xa3;
		return 4;
	 }

	 LED_ONLINE_ON;
	 ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
	 has_request=0;
	 is_auto_reply=0;
	 s_retries=0;
	 r_retries=0;
	 lost_retries=0;

	 while(2)
	 {
		//10--receive request from remote
		if(is_half_duplex)
		{
			task_p1=7;
STEP7:
			a=step_turn_to_rx();
			if(!a&&task_p2)return 0;
//s_UartPrint("R0 WAIT,RESULT:%d\r\n",a);
		}
		task_p1=8;
STEP8:
		a=step_rcv_v80_cmd(1,&rlen);
		if(!a&&task_p2)return 0;
//s_UartPrint("R0:%d,RN:%d:%02X%02X%02X\r\n",a,rlen,frame_pool[0],frame_pool[1],frame_pool[2]);
		if(a&&a<4)
		{
			modem_status=0xa4;
			return 5;
		}
		if(a==6 && rlen>=26 && !memcmp(frame_pool+rlen-26,"\r\nLINE REVERSAL DETECTED\r\n",26) ||
			a==6 && rlen>=14 && !memcmp(frame_pool+rlen-14,"\r\nNO CARRIER\r\n",14) )
		{
			modem_status=0xa5;
			return 6;
		}

		//--to avoid wasting long time if remote losts
		if((a==100||a==6) && !rlen)
		{
			lost_retries++;
			if(lost_retries>=1)//2
			{
				modem_status=0xa6;
				return 7;
			}
		}
		else lost_retries=0;

		if(a==4||rlen<2)
		{
			s_retries++;
			if(s_retries>MAX_SYNC_T_RETRIES)
			{
				modem_status=0xa7;
				return 8;
			}

			if(is_half_duplex)
			{
				task_p1=9;
STEP9:
				a=step_turn_to_tx();
				if(!a&&task_p2)return 0;
//s_UartPrint("T0 WAIT:%d\r\n",a);
			}
			tmpc_data=(Npos<<5)+0x11;
			rlen=0;
			pack_sdlc_v80(tmpc_data,NULL,&rlen);
			task_p1=10;
STEP10:
			a=step_send_to_serial1(ci_pool,rlen);
			if(!a&&task_p2)return 0;
//s_UartPrint("T0:%d,Npos:%d,SN:%d:%02X%02X\r\n",a,Npos,rlen,ci_pool[0],ci_pool[1]);
			if(a)
			{
				modem_status=0xa8;
				return 9;
			}
			continue;
		}
		s_retries=0;

		if((frame_pool[1]&0x11)==0x10)//a DATA pack
		{
			if(Npos==(frame_pool[1]>>1)%8 &&	frame_pool[0]=='0')
			{
				Npos=(Npos+1)%8;
				rlen-=2;//4 for tdk,si2414
//memcpy(response_pool,frame_pool+2,rlen);
//response_dlen=rlen;
				nac_pack_up(frame_pool+2,&rlen);
				store_response(resp_pack,rlen);
				resp_write=rlen;
				has_request=1;
				tm00_remains=AUTO_RESPONSE_TIMEOUT;
			}
		}

		if((frame_pool[1]&0x11)==0x11)//for a RR pack
		{
			if(has_request&&auto_reply_way<2&&!tm00_remains)
			{
//send_to_serial("\x66\x66",2);
				is_auto_reply=1;
				if(!auto_reply_way)auto_reply_way=1;//auto reply
				goto RESPONSE_BEGIN;
			}

		//11--send RR(ready to receive) command
			//if(!fetch_nac_pack(response_pool,&response_dlen))
			response_dlen=req_write;
			if(response_dlen)
			{
RESPONSE_BEGIN:
				if(is_half_duplex)
				{
					task_p1=11;
STEP11:
					a=step_turn_to_tx();
					if(!a&&task_p2)return 0;
//s_UartPrint("T1 WAIT:%d\r\n",a);
				}
				if(!is_auto_reply)
				{
					rlen=response_dlen;
					if(!auto_reply_way)auto_reply_way=2;//not auto reply
				}
				else
				{
					rlen=15;
					memset(response_pool,0x00,rlen);
					response_pool[0]=0x60;
					pack_sdlc_v80(0,response_pool,&rlen);
				}
				sdlc_tx_pool[1]=(Npos<<5)+0x10+(Nnac<<1);
				task_p1=12;
STEP12:
				a=step_send_to_serial1(sdlc_tx_pool,rlen);
				if(!a&&task_p2)return 0;
//s_UartPrint("T1:%d,Npos:%d,SN:%d:%02X%02X\r\n",a,Npos,rlen,sdlc_tx_pool[0],sdlc_tx_pool[1]);
				if(a)
				{
					modem_status=0xa9;
					return 10;
				}

			r_retries=0;
			sn_retries=0;
REQ_R_RETRY:
			r_retries++;
			if(r_retries>MAX_SYNC_R_RETRIES){modem_status=0xad;return 11;}

				if(is_half_duplex)
				{
					task_p1=13;
STEP13:
					a=step_turn_to_rx();
					if(!a&&task_p2)return 0;
				}
				task_p1=14;
STEP14:
				a=step_rcv_v80_cmd(1,&rlen);
				if(!a&&task_p2)return 0;
//s_UartPrint("R1:%d,SN:%d:%02X%02X\r\n",a,rlen,frame_pool[0],frame_pool[1]);
				if(a&&a<2)
				{
					modem_status=0xaa;
					return 12;
				}
				if(a==10)//Tx Underrun
				{
					modem_status=0x19;
					return 13;
				}
				if(a==6 && rlen>=26 && !memcmp(frame_pool+rlen-26,"\r\nLINE REVERSAL DETECTED\r\n",26) ||
					a==6 && rlen>=14 && !memcmp(frame_pool+rlen-14,"\r\nNO CARRIER\r\n",14) )
				{
					modem_status=0xa5;
					return 14;
				}
				if(a>=4 || rlen<2 || frame_pool[0]!=0x30&&frame_pool[0]!=0xff)
				{
					if(a==ERR_SDLC_TX_OVERRUN)
					{
						modem_status=0x18;
						return 15;
					}

					if(is_half_duplex)
					{
						task_p1=15;
STEP15:
						a=step_turn_to_tx();
						if(!a && task_p2)return 0;
					}

					tmpc_data=(Npos<<5)+0x11;//RR frame
					rlen=0;
					pack_sdlc_v80(tmpc_data,NULL,&rlen);
					task_p1=16;
STEP16:
					a=step_send_to_serial1(ci_pool,rlen);
					if(!a && task_p2)return 0;
					if(a){modem_status=0xae;return 16;}
					goto REQ_R_RETRY;
				}
				r_retries=0;
				has_request=0;

			//--if need repeat
			if(Nnac==(frame_pool[1]>>5))
			{
				sn_retries++;

				if(sn_retries<=2)//5,2(2016.10.31)
				{
					if(is_half_duplex)
					{
						task_p1=17;
STEP17:
						a=step_turn_to_tx();
						if(!a && task_p2)return 0;
					}
					tmpc_data=(Npos<<5)+0x11;
					rlen=0;
					pack_sdlc_v80(tmpc_data,NULL,&rlen);
					task_p1=18;
STEP18:
					a=step_send_to_serial1(ci_pool,rlen);
					if(!a && task_p2)return 0;
					if(a){modem_status=0xaf;return 17;}//0x04

					goto REQ_R_RETRY;
				}
				else goto RESPONSE_BEGIN;
			}

				if((frame_pool[1]&0x11)==0x10)//for a DATA pack
				{
					if(Npos==(frame_pool[1]>>1)%8)
					{
						Npos=(Npos+1)%8;
						rlen-=2;//4 for tdk,si2414
						memcpy(response_pool,frame_pool+2,rlen);
						response_dlen=rlen;

						nac_pack_up(frame_pool+2,&rlen);
						store_response(resp_pack,rlen);
						has_request=1;
						tm00_remains=AUTO_RESPONSE_TIMEOUT;
					}
				}

				if((frame_pool[1]&0x10)==0x10)//RR or DATA pack
				{
					if((frame_pool[1]>>5)!=(Nnac+1)%8)goto RESPONSE_BEGIN;
					Nnac=(Nnac+1)%8;
					req_write=0;
					modem_status&=0xfe;
				}
				is_auto_reply=0;
			}
		}//if(a RR pack)

		if(is_half_duplex)
		{
			task_p1=19;
STEP19:
			a=step_turn_to_tx();
			if(!a&&task_p2)return 0;
//s_UartPrint("T2 WAIT:%d \r\n",a);
		}
		tmpc_data=(Npos<<5)+0x11;
		rlen=0;
		pack_sdlc_v80(tmpc_data,NULL,&rlen);
		task_p1=20;
STEP20:
		a=step_send_to_serial1(ci_pool,rlen);
		if(!a&&task_p2)return 0;
//s_UartPrint("T2:%d,Npos:%d,SN:%d:%02X%02X\r\n",a,Npos,rlen,ci_pool[0],ci_pool[1]);
		if(a)
		{
			modem_status=0xac;
			return 18;
		}
	 }//while(2)
}

uchar two_one(uchar *in,uchar pairs,uchar *out)
{
	uchar i,j,cc;

	j=pairs/2;
	for(i=0;i<j;i++)
	{
	  cc=in[i*2];
	  if(cc>='0'&&cc<='9')cc-='0';
	  else if(cc>='A'&&cc<='F')cc=cc-'A'+0x0a;
	  else if(cc>='a'&&cc<='f')cc=cc-'a'+0x0a;
	  else return 1;

	  out[i]=cc<<4;

	  cc=in[i*2+1];
	  if(cc>='0'&&cc<='9')cc-='0';
	  else if(cc>='A'&&cc<='F')cc=cc-'A'+0x0a;
	  else if(cc>='a'&&cc<='f')cc=cc-'a'+0x0a;
	  else return 2;

	  out[i]+=cc;
	}
	return 0;
}

static uchar si_caller_connect(void)
{
	static uchar a,phone_tail,tmpc_data,tmps[210];
	static uchar i,speed_type,dial_times,dial_remains,b_modem_status;
	static uchar resp,x_retries,flashes;
	static ushort dn,tmpu,carrier_loss,tmpu_data;
	static struct
	{
		ushort dial_wait_ms;
		uchar pbx_pause_s;
		uchar code_hold_ms;
		uchar code_space_ms;
		uchar pulse_on_ms;
		uchar pulse_break_ms;
		ushort pulse_space_ms;
	}tm;

	switch(task_p1)
	{
	case 0:goto STEP0;
	case 100:goto STEP100;
	case 1:goto STEP1;
	case 2:goto STEP2;
	case 200:goto STEP200;
	case 3:goto STEP3;
	case 4:goto STEP4;
	case 5:goto STEP5;
	case 6:goto STEP6;
	case 7:goto STEP7;
	case 71:goto STEP71;
	case 8:goto STEP8;
	case 9:goto STEP9;
	case 10:goto STEP10;
	case 11:goto STEP11;
	case 12:goto STEP12;
	case 13:goto STEP13;
	case 14:goto STEP14;
	case 15:goto STEP15;
	case 16:goto STEP16;
	case 17:goto STEP17;
	case 18:goto STEP18;
	case 20:goto STEP20;
	case 21:goto STEP21;
	case 22:goto STEP22;
	case 23:goto STEP23;
	case 24:goto STEP24;
	case 25:goto STEP25;
	default:return 100;
	}

STEP0:
	if(!modem_config.dial_times)dial_remains=1;
	else dial_remains=modem_config.dial_times;

	dial_times=0;
	memset(&tm,0x00,sizeof(tm));

    //--fetch connection speed
	 a=(modem_config.pattern>>5)&0x03;
	 if(is_sync)
    {
		  if(a<2)a+=1;
		  else if(a==2)a=5;
		  else a=1; //forced to 1200bps for other sync speeds
	}
	 else
    {
		  tmpc_data=modem_config.async_format&0x0f;
		  if(tmpc_data==0x08 || tmpc_data==0x09)a=0;
		  else if(modem_config.async_format>>4)a=modem_config.async_format>>4;
		  else if(a<2)a+=1;//1-1200,2-2400
		  else if(a==2)a=5;//9600
		  else a=7;//14400

		  if(mdm_chip_type==5 && a>7)a=7;
		  if(mdm_chip_type==3 && a>13)a=13;
    }

	 speed_type=a;
	 asyn_form=modem_config.async_format&0x0f;

DIAL_BEGIN:
	modem_status=0x0a;
	req_write=resp_write=resp_head=0;
	dial_times++;
	task_p2=0;

	//PortTx(P_MODEM,'X');//wake modem up,deleted on 2016.10.31

	//--load patch to silicon modem if it's off or unloaded,shifted on 2016.10.21
	if(GET_PWR || !patch_loaded)
	{
		task_p1=100;
		task_load_step=0;

STEP100:
		a=si_step_load_patch(1);
		if(!a&&task_load_step)return 0;

		if(a)
		{
			modem_status=0xd0;
			return 1;
		}
	}

	if(tmm_remains<1)tmm_remains=2;//leave 20ms for modem wakeup
	while(tmm_remains)//wait between two dials
	{
		LED_ONLINE_ON;
		ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
		task_p1=1;
STEP1:
		if(tmm_remains)//wait between two dials
			return 0;
		LED_ONLINE_OFF;
		ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);
	}

	for(i=0;i<25;i++)
	{
		a=PortRx(P_MODEM,tmps);
//if(!a)s_UartPrint(" data found:%02X\r\n",tmps[0]);
	}

	SET_RTS(LOW_LEVEL);
	SET_DTR(LOW_LEVEL);//set RTS,DTR low for on
	dn=modem_config.pbx_pause;
	dn=(dn+9)/10;
	if(!dn)dn=1;//minimized to 1 second for comma
	tm.pbx_pause_s=dn;

	if(ex_cmd.connect_wait)
	{
		tmpu=ex_cmd.connect_wait;
		if(tmpu<6)tmpu=6;//5,modified on 20130621
		else if(tmpu>255)tmpu=255;
	}
	else
	{
		tmpu=(modem_config.pattern&0x07)*3+6;//5,modified on 20130621
		if(modem_config.ignore_dialtone&0x40)tmpu*=2;//double timeout
	}
	if(!speed_type)carrier_loss=255;//240 for 4 minutes,255 no hangup
	else if(speed_type<3)carrier_loss=14;//14,NEED
	else carrier_loss=50;//14

	//Q2:CTS,Q3:RTS/CTS,Q4:XON/XOFF--Q3
	//S7:carrier wait time,S10:carrier loss time to disconnect
	//S8:pause for ','
	//S10:carrier loss timer
	//U69,set MODE bit=1 for on-hook line voltage
	//sprintf(tmps,"ATE0\\Q2\\N0S14=2S8=%uS7=%uS10=%u:U69,0004\r",dn,tmpu,carrier_loss);
	sprintf(tmps,"ATE0\\Q2\\N0S14=2S8=%uS7=%uS9=20S10=%u:U69,0004\r",dn,tmpu,carrier_loss);
	if(!is_sync&&speed_type>2)tmps[9]='3';//\\N3 for async
	task_p1=2;
STEP2:
	a=step_send_to_smodem(tmps,&dn);
	if(!a&&task_p2)return 0;
	if(a)
	{
//printk(" c1:%d,dn:%d,%s\r\n",a,dn,tmps);
		//--forced to do hard reset and redial once the chip fails
		patch_loaded=0;
		task_p1=0;

		//modem_status=0xd0;
		return 0;// 2
	}

	//--set ESC_PIN hangoff mode,read Tip-Ring Voltage
	//&D2--use ESC_PIN to hang-off
	//&D3--use ESC_PIN to reset
	//U6E--d12~d8:CLKOUT divider,0-disable
	//U70--d15:enable ESC_PIN
	//S38--hang off delay time in seconds
	strcpy(tmps,"ATS0=0S38=0&D3:U70,8000;:U6E&E0FF;:R6C\r");//d15-8:line voltage,S38 added on 2016.10.21
	task_p1=200;
STEP200:
	a=step_send_to_smodem(tmps,&dn);
	if(!a&&task_p2)return 0;
//printk(" c2:%d,dn:%d,%s\r\n",a,dn,tmps);
	if(a)
	{
		modem_status=0xd1;
		return 3;
	}

	two_one(tmps+2,2,&a);
	tmpu=a;
	if(a&0x80)tmpu=256-tmpu;
//printk(" c20:%d\r\n",tmpu);
	if(tmpu<2)
	{
		modem_status=0x33;//0x03,no_line
		return 4;
	}

	if(dial_only&&tmpu>=19)
	{
		modem_status=0x83;//not picked up for dial only
		return 5;
	}
	if(!dial_only && tmpu<19 && !(modem_config.ignore_dialtone&0x04))
	{
		modem_status=0x03;
		return 6;
	}

  //3.1--set dial tone detect
	if(modem_config.ignore_dialtone&0x01)
	{
		dn=modem_config.dial_wait;
		dn=(dn+9)/10;
		if(dn<2)dn=2;//at least 2 seconds
		tm.dial_wait_ms=dn*1000+200;
		//X1:blind dial,X3:busy tone detect
		//S6:pause before blind dial(second)
		sprintf(tmps,"ATX3S6=%u\r",dn);
		task_p1=3;
STEP3:
		a=step_send_to_smodem(tmps,&dn);
		if(!a&&task_p2)return 0;
		if(a)
		{
			modem_status=0xd2;
			return 7;
		}
	}
	else
	{
		//--set dial tone ON/OFF threshold
		dn=modem_config.dial_wait;
		dn*=100;
		tm.dial_wait_ms=250;
		if(dn<2000)dn=2000;//300,2000
		if(dn>5000)dn=5000;//limited to 2000ms~5000ms
		//X4:full monitor
		//U15:dial tone ON threshold
		//U16:dial tone OFF threshold
		//U34:dial tone detect window(ms)
		//U35,dial tone detect valid time(0B40:400ms,0168:50ms)
		sprintf(tmps,"ATX4:U15,0060,0040;:U34,%04X,0168\r",dn);
		task_p1=4;
STEP4:
		a=step_send_to_smodem(tmps,&dn);
		if(!a&&task_p2)return 0;
//printk(" c3:%d,dn:%d,%s\r\n",a,dn,tmps);
		if(a)
		{
			modem_status=0xd3;
			return 8;
		}
	}

  //3.3--set DTMF/PULSE cadence
	if(!modem_config.mode)//DTMF dial code
	{
		dn=modem_config.code_hold;
		if(dn<50)dn=50;//limited to 50ms~255ms
		tm.code_hold_ms=dn;

		tmpu=modem_config.code_space;
		tmpu*=10;
		if(tmpu<50)tmpu=50;
		if(tmpu>250)tmpu=250;//limited to 50ms~250ms
		//U46:DTMF power level in format 0HL0
		//U47:dial DTMF on time(ms)
		//U48:dial DTMF off time(ms)
		tm.code_space_ms=tmpu;
		sprintf(tmps,"AT:U46,08A0,%04X,%04X\r",dn,tmpu);
		task_p1=5;
STEP5:
		a=step_send_to_smodem(tmps,&dn);
		if(!a&&task_p2)return 0;
//printk(" c4:%d,dn:%d,%s\r\n",a,dn,tmps);
		if(a)
		{
			modem_status=0xd4;
			return 9;
		}
	}
	else
	{
		//pulse mode 1:61/39,mode 2:67/33
		if(modem_config.mode==2)dn=67;
		else dn=61;
		tm.pulse_break_ms=dn;

		if(modem_config.mode==2)tmpu=33;
		else tmpu=39;
		tm.pulse_on_ms=tmpu;

		if(modem_config.mode==2)tmpu_data=610;
		else tmpu_data=510;
		tm.pulse_space_ms=dn;
		//U42:pulse dial break time
		//U43:pulse dial make time
		//U45:pulse dial interdigit time
		sprintf(tmps,"AT:U42,%04X,%04X;:U45,%04X\r",dn,tmpu,tmpu_data);
		task_p1=6;
STEP6:
		a=step_send_to_smodem(tmps,&dn);
		if(!a&&task_p2)return 0;
		if(a)
		{
			modem_status=0xd5;
			return 10;
		}
	}

	if(is_sync||!speed_type)
	{
		strcpy(tmps,"AT+FCLASS=");
		if(speed_type==5)
			strcat(tmps,"1\r");
		else if(!speed_type)strcat(tmps,"256\r");
		else	strcat(tmps,"0\r");
		task_p1=7;
STEP7:
		a=step_send_to_smodem(tmps,&dn);
		if(!a&&task_p2)return 0;
//printk(" c5:%d,dn:%d,%s\r\n",a,dn,tmps);
		if(a)
		{
			modem_status=0xd6;
			return 11;
		}

		if(!speed_type || speed_type==5)
			strcpy(tmps,"AT+IFC=0,2\r");
		else	strcpy(tmps,"AT+ITF=0383,0128\r");
		task_p1=71;
STEP71:
		a=step_send_to_smodem(tmps,&dn);
		if(!a&&task_p2)return 0;
//printk(" c71:%d,dn:%d,%s\r\n",a,dn,tmps);
		if(a)
		{
			modem_status=0xde;
			return 71;
		}
	}

  //3.5--set connection speed
	strcpy(tmps,"AT");
	switch(speed_type)
	{
	case 0://1200bps,half duplex
		//:UCA,d0--0:BELL202,1:V.23
		//BELL202--mark:1200Hz,space:2200Hz; V.23--mark:1300Hz,space:2100Hz
		//:UAA,d2--enable rude disconnect
		if((modem_config.async_format&0x0f)==0x09)//V.23
			strcat(tmps,":UCA,1;:UCD|2;:UAA,4;:UD4,7FFF;+ITF=383,128;");//V.23
		else strcat(tmps,":UCA,0;:UCD|2;:UAA,4;:UD4,7FFF;+ITF=383,128;");//BELL202
		SET_RTS(1);//set RTS high as off
		break;
	case 1://1200bps
		if(is_sync)
			strcat(tmps,"+ES=6,,8;+ESA=0,0,,,1;+IFC=0,2;+MS=V22");
		else
			strcat(tmps,"\\N0&H7");//V.22,1200bps
		break;
	case 2://2400bps
		if(is_sync)
			strcat(tmps,"+ES=6,,8;+ESA=0,0,,,1;+IFC=0,2;+MS=V22B");
		else
			strcat(tmps,"&H6");//V.22bis,2400/1200bps
		break;
	case 3://4800bps
		strcat(tmps,"&H4&G5");//4800bps max
		break;
	case 4://7200bps
		strcat(tmps,"&H4&G6");//7200bps max
		break;
	case 5://9600bps
		if(is_sync)
		{
			//UAA D2-enable rude disconnect,D1-enable V.29
			strcat(tmps,"\\V2+ES=6,,8;+ESA=0,0,,,1;:UAA,4;+ITF=383,128");
			SET_RTS(1);//set RTS high as off
		}
		else
			strcat(tmps,"&H4&G7");//9600bps max
		break;
	case 6://12.0kbps
		strcat(tmps,"&H4&G8");//12kbps max
		break;
	case 7://14.4kbps
		strcat(tmps,"&H4&G9");//14.4kbps max
		break;
	case 8://19.2kbps
		strcat(tmps,"&H2&G11");//19.2kbps max
		break;
	case 9://24kbps
		strcat(tmps,"&H2&G13");//24kbps max
		break;
	case 10://26.4kbps
		strcat(tmps,"&H2&G14");//26.4kbps max
		break;
	case 11://28.8kbps
		strcat(tmps,"&H2&G15");//28.8kbps max
		break;
	case 12://31.2kbps
		strcat(tmps,"&H2&G16");//31.2kbps max
		break;
	case 13://33.6kbps
		strcat(tmps,"&H2&G17");//33.6kbps max
		break;
	case 14://48kbps
		strcat(tmps,"&H0");
		break;
	case 15://56kbps
		strcat(tmps,"&H0");
		break;
	default://1200bps for sync,14.4 for async
		if(is_sync)
			strcat(tmps,"+ES=6,,8;+ESA=0,0,,,1,0;+IFC=0,2;+MS=V22");
		else
			strcat(tmps,"&H4&G9");//14.4kbps max
		break;
  }
	//3.8--set communication format
	if(!is_sync)
	{
		a=modem_config.async_format&0x0f;//async format
		switch(a)
		{
		case 1:
			strcat(tmps,"\\P0\\B5");//8E1
			break;
		case 2:
			strcat(tmps,"\\P2\\B5");//8O1
			break;
		case 3:
			strcat(tmps,"\\B1");//7N1
			break;
		case 4:
			strcat(tmps,"\\P0\\B2");//7E1
			break;
		case 5:
			strcat(tmps,"\\P2\\B2");//7O1
			break;
		default:
			strcat(tmps,"\\B3");//8N1
			break;
		}
	}
	strcat(tmps,"\r");
	task_p1=8;
STEP8:
	a=step_send_to_smodem(tmps,&dn);
	if(!a&&task_p2)return 0;
//printk(" c6:%d,dn:%d,%s\r\n",a,dn,tmps);
	if(a)
	{
		modem_status=0xd7;
		return 12;
	}

	//3.6--set SDLC/ASYNC mode
	strcpy(tmps,"AT");
	if(!ex_cmd.cmd_count)strcat(tmps,"H1");//pick up the modem from the line
	if(!is_sync)
	{
		switch(speed_type)
		{
		case 0://FSK
			//U52--transmit level
			//UD7--mark detect bits threshold
			//UD2--RX timeout in 10ms
			//U7A--enable chinese EPOS SMS,i.e FSK
			//+VNH--auto hangup control,1:not hangup
			//strcat(tmps,":U7A|4000;:UD7,28;:UD2,7D0;:U52,0;+VNH=1");//ASYNC
			strcat(tmps,":U7A|4000;:UD7,14;:UD2,64;:U52,0;+VNH=1");//ASYNC
			//strcat(tmps,":U7A|4000;:UD7,28;:UD2,64;:U52,0;+VNH=1");//ASYNC
			break;
		case 1:
			if(!(modem_config.ignore_dialtone&0x80))//fast connect
				strcat(tmps,"\\V4:U7A,0001;:U1D4|4000");
			else
				strcat(tmps,"\\V4:U7A,0000;:U1D4|4000");
			break;
		case 2:
			if(!(modem_config.ignore_dialtone&0x80))//standard connect
				strcat(tmps,"\\V4:U80,012C;:U7A,0000;:U1D4|4000");
			else
				strcat(tmps,"\\V4:U80,012C;:U7A,0001;:U1D4|4000");
			break;
		default:
			strcat(tmps,"\\V4:U7A,0000");
			break;
		}//switch(speed_type)
	}
	else
	{
		//--U87 Synchronous Access Mode
		strcat(tmps,":U87,050A;");
		switch(speed_type)
		{
		case 1:
			//U1D4 D14--enable V.29FC and some V.22 and V.22FC improvements,other bits reserved
			//    d4-new FC;d1-1:hdlc,0-async;d0-1:fast connect,0-normal
			if(!(modem_config.ignore_dialtone&0x80))//fast connect
				strcat(tmps,":U7A,0012;:U1D4|4000");
			else
				strcat(tmps,":U7A,0002;:U1D4|4000");
			break;
		case 2:
			//U80 d15-enable non-answer-tone connect,d14~d0--delay time in 1/600s
			//    d4-new FC;d1-1:hdlc,0-async;d0-1:fast connect,0-normal
			if(!(modem_config.ignore_dialtone&0x80))//standard connect
				strcat(tmps,":U80,012C;:U7A,0002;:U1D4|4000");
			else
				strcat(tmps,":U80,012C;:U7A,0012;:U1D4|4000");
			break;
		case 5:
			//UD3:V29C answer tone detect threshold(50~180ms)
			//U7A d3-enable V.29FC
			//strcat(tmps,":U7A,2009;:U1D4|4000;:UD3,B4");//d13:RTS toggle for direction
			strcat(tmps,":U7A,2009;:U1D4|4000;:UD3,64");//d13:RTS toggle for direction,answer tone threshold changed to 100ms
			break;
		}//switch(speed_type)
	}
	strcat(tmps,"\r");

	has_picked=1;
	//SET_RELAY(0);//set RELAY low for cut off
	task_p1=9;
STEP9:
	a=step_send_to_smodem(tmps,&dn);
	if(!a&&task_p2)return 0;
//printk(" c7:%d,dn:%d,%s\r\n",a,dn,tmps);
	if(a)
	{
		modem_status=0xd8;
		return 13;
	}
    
	//--execute the preset AT commands
	for(tmpu=0;tmpu<ex_cmd.cmd_count;tmpu++)
	{
	  strcpy(tmps,ex_cmd.at_cmd[tmpu]);
	  task_p1=10;
STEP10:
	  a=step_send_to_smodem(tmps,&dn);
	  if(!a && task_p2)return 0;
	  if(a)
	  {
		  modem_status=0xd9;
		  return 13;
	  }
	}//for(tmpu)

	if(ex_cmd.cmd_count)
	{
	  strcpy(tmps,"ATE0H1\r");
	  task_p1=11;
STEP11:
	  a=step_send_to_smodem(tmps,&dn);
	  if(!a && task_p2)return 0;
	  if(a)
	  {
		  modem_status=0xda;
		  return 14;
	  }
	}

	//--read loop current to avoid being burnt
	//strcpy(tmps,"AT:R63\r");//d15-d8:loop current

	strcpy(tmps,"AT:R79");
    if (ex_cmd.manual_adjust_tx_level)
    {
        sprintf(tmps+strlen(tmps), ";:U52,%04X", si_tx_level_tab[ex_cmd.manual_adjust_tx_level]);
    }

    if(ex_cmd.manual_adjust_answer_tone)
    {
        if(ex_cmd.manual_adjust_answer_tone<5) 
        {
            tmpu_data = si_answer_tone_tab[ex_cmd.manual_adjust_answer_tone];
        }
        else
        {
            i=ex_cmd.manual_adjust_answer_tone;
            if( (speed_type==5) && is_sync && (i>20) )i=20;//同步9600在应答音门限大于200毫秒时无法连接上，在这里做个限定
            tmpu_data = 468+(i-5)*10*12;
        }
        sprintf(tmps+strlen(tmps),";*Y254:W44F3,%04X;:U1DA,0",tmpu_data);
    }
	strcat(tmps,"\r");  
	task_p1=12;
STEP12:
	a=step_send_to_smodem(tmps,&dn);
	if(!a && task_p2)return 0;
//printk(" c8:%d,dn:%d,%s\r\n",a,dn,tmps);
	if(a)
	{   
		modem_status=0xdb;
		return 15;
	}
	for(i=0;i<dn;i++)
		if(tmps[i]!=0x0d && tmps[i]!=0x0a)break;
	if(dn>i+6 && !memcmp(tmps+i,"001F",4))//overload of loop current
	{
		modem_status=0xec;//must not be changed
		return 16;
	}

	//--add a delay just before dialling
	if(!(modem_config.ignore_dialtone&0x01) && str_phone[ptr_phone]!='-')
	{
		tmm_remains=60;//600ms
		while(tmm_remains)//wait between two dials
		{
			task_p1=13;
STEP13:
			if(tmm_remains)return 0;
		}
	}

  //4--begin to pick up and dial
	strcpy(tmps,"ATDT");
	a=modem_config.mode;
	if(a==1 || a==2)tmps[3]='P';
	i=4;
	tmpu=tm.dial_wait_ms;
	dial_info.ptr=0;
	dial_info.clen=1;
	dial_info.on_10ms[0]=0;
	dial_info.off_10ms[0]=tm.dial_wait_ms/10;
	while(str_phone[ptr_phone]!= ';' && str_phone[ptr_phone]!='.')
	{
		a=toupper(str_phone[ptr_phone]);
		if(tmps[3]=='T')//DTMF dial
		{
			if(a>='0'&&a<='9'||a=='!'||a==','||a=='*'||a=='#'||a=='W'||
				a>='A'&&a<='D')
			{
				if(modem_config.ignore_dialtone&0x01 && a=='W')a=',';
				tmps[i]=a;
				i++;

				dn=tmpu;
				if(a==',')tmpu+=tm.pbx_pause_s*1000;
				else if(a=='!')tmpu=500;
				else if(a=='W')tmpu+=500;/*补丁的最长等待时间为5s*/
				else tmpu+=tm.code_hold_ms+tm.code_space_ms;
				if(tmpu<dn)tmpu=60000;

				//--write for DTMF led flashing
				tmpc_data=dial_info.clen;
				if(a==','||a=='!'|| a=='W' )tmpu_data=0;
				else tmpu_data=tm.code_hold_ms/10;
				dial_info.on_10ms[tmpc_data]=tmpu_data;

				if(a==',')tmpu_data=tm.pbx_pause_s*100;
				else if(a=='!')tmpu_data=50;
				else if(a=='W')tmpu_data=500;
				else tmpu_data=tm.code_space_ms/10;
				dial_info.off_10ms[tmpc_data]=tmpu_data;
				dial_info.clen++;
			}
		}
		else if(a>='0'&&a<='9'||a==','||a=='W')//PULSE dial
		{
			if(a=='W')a=',';
			tmps[i]=a;
			i++;

			dn=tmpu;
			if(a==',')tmpu+=tm.pbx_pause_s*1000;
			else
			{
				if(a=='0')a=10;
				else a-='0';
				tmpu+=(tm.pulse_break_ms+tm.pulse_on_ms)*a+tm.pulse_space_ms;
			}
			if(tmpu<dn)tmpu=60000;

			//--write for PULSE led flashing
			tmpc_data=dial_info.clen;
			if(a==',')tmpu_data=0;
			else tmpu_data=(tm.pulse_break_ms+tm.pulse_on_ms)/10*a;
			dial_info.on_10ms[tmpc_data]=tmpu_data;

			if(a==',')tmpu_data=tm.pbx_pause_s*100;
			else tmpu_data=tm.pulse_space_ms/10;
			dial_info.off_10ms[tmpc_data]=tmpu_data;
			dial_info.clen++;
		}
		ptr_phone++;
	}//while(get phone number)

	if(ex_cmd.connect_wait)
	{
		dn=ex_cmd.connect_wait;
		if(dn<5)dn=5;
		else if(dn>255)dn=255;
		dn+=3;
		dn*=100;
	}
	else
	{
		dn=(modem_config.pattern&0x07)*300+800;//500
		if(modem_config.ignore_dialtone&0x40)dn*=2;//double timeout
		tmpu=tmpu/10+dn;
		if(tmpu>6000)dn=6000;//maximum 60s
		else dn=tmpu;
	}

	//--if it's for manual dial or a FSK dial,add a comma to dial_no
	if(dial_only||!speed_type)tmps[i++]=';';
	tmps[i]='\r';
	tmps[i+1]=0;
	if(str_phone[ptr_phone]=='.'){phone_tail=1;ptr_phone=0;dial_remains--;}
	else phone_tail=0;
	if(str_phone[ptr_phone]==';')ptr_phone++;

	led_auto_flash(N_LED_TXD,1,0,0);
	ScrSetIcon(ICON_TEL_NO,S_ICON_DOWN);
	task_p1=14;
STEP14:
	a=step_send_to_smodem(tmps,&dn);
	if(!a&&task_p2)return 0;
	led_control.open=0;LED_TXD_OFF;ScrSetIcon(ICON_TEL_NO,S_ICON_NULL);//light off LED_TXD
//printk(" c9:%d,dn:%d,%s\r\n",a,dn,tmps);
	if(a)
	{
		if(dial_remains || !phone_tail)
		{
			hang_off();
			goto DIAL_BEGIN;
		}
		modem_status=0x05;
		return 30;
	}

	//--FSK response
	if(!speed_type && dn>5 && !memcmp(tmps+2,"OK",2))
	{
		line_bps=1200;
		flashes=1;
		if(modem_config.idle_timeout>90)modem_config.idle_timeout=90;
		tmm_remains=modem_config.idle_timeout*1000;
		goto L_TAIL;
	}

	if(dial_only && dn>5 && !memcmp(tmps+2,"OK",2))
	{
		modem_status=0x06;//is sending tel_no
		tm00_remains=200;
		while(tm00_remains)
		{
			task_p1=15;
STEP15:
			if(tm00_remains)return 0;
		}

		modem_status=0x0a;//is sending
		return 31;
	}

	if(dn>9 && !memcmp(tmps+2,"CONNECT",7))
	{
		flashes=0;
		if(is_sync)
		{
				task_p1=16;
STEP16:
				a=step_rcv_v80_cmd(0,&dn);
				if(!a&&task_p2)return 0;
//printk(" c10:%d,dn:%d,%02X%02X\r\n",a,dn,frame_pool[0],frame_pool[1]);
				if(!a)
				{
					a=frame_pool[0];
					if(a==0x20)flashes=1;
					else if(a==0x21)flashes=2;
					else if(a==0x24)flashes=5;
					else if(a==0x22)flashes=3;
					else if(a==0x23)flashes=4;
					else
					{
						b_modem_status=0x040;
						resp=32;
						goto L_END;
					}
				}
			if(flashes==5)
				is_half_duplex=1;
			else is_half_duplex=0;
		}
		else
		{
//printk(" c11:%d,dn:%d,%s.",a,dn,tmps);
			//SILICON connect response string example:
			//\r\nCONNECT 26400 26400\r\n
			for(tmpu=dn-1;;tmpu--)
			{
				if(tmps[tmpu]=='/'||tmps[tmpu]==0x20)break;
				if(!tmpu)break;
			}

			if(!tmpu || memcmp(tmps+dn-2,"\r\n",2))
			{
				b_modem_status=0xdc;
				resp=33;
				goto L_END;
			}

			tmps[dn-2]=0;
			tmpu_data=atoi(tmps+tmpu+1);
			line_bps=tmpu_data;
			if(tmpu_data==1200)flashes=1;
			else if(tmpu_data==2400)flashes=2;
			else if(tmpu_data==9600)flashes=5;
			else if(tmpu_data==14400)flashes=7;
			else if(tmpu_data==4800)flashes=3;
			else if(tmpu_data==7200)flashes=4;
			else if(tmpu_data==12000)flashes=6;
			else if(tmpu_data>14400 && tmpu_data<=19200)flashes=8;
			else if(tmpu_data>19200 && tmpu_data<=24000)flashes=9;
			else if(tmpu_data==26400)flashes=10;
			else if(tmpu_data==28800)flashes=11;
			else if(tmpu_data==31200)flashes=12;
			else if(tmpu_data==33600)flashes=13;
			else if(tmpu_data>33600 && tmpu_data<=48000)flashes=14;
			else if(tmpu_data>48000)flashes=15;
		}
//s_UartPrint(" line_bps:%ld,flashes:%d\r\n",line_bps,flashes);

		if(modem_config.idle_timeout>90)modem_config.idle_timeout=90;//enlarged to 90 from 65,2009.4.29
		tmm_remains=modem_config.idle_timeout*1000;
		goto L_TAIL;
	}

	//5--check dial tone
	if(dn>13 && !memcmp(tmps+2,"NO DIALTONE",11))
	{
		//--wait for 300ms
		tm00_remains=30;
		while(tm00_remains)
		{
			task_p1=17;
STEP17:
			if(tm00_remains)return 0;
		}

		//--check the line voltage
		strcpy(tmps,"ATS0=0:R6C\r");//d15-8:line voltage
		task_p1=18;
STEP18:
		a=step_send_to_smodem(tmps,&dn);
		if(!a && task_p2)return 0;
//s_UartPrint(" trv:%d,dn:%d,%s\r\n",a,dn,tmps);
		if(a)
		{
			modem_status=0xdd;
			return 35;
		}

		if(dn>6 && !memcmp(tmps,"\r\n00",4))
		{
			modem_status=0xee;//must not be changed
			return 36;
		}

		b_modem_status=0xe8;//must not be changed
		resp=37;
		goto L_END;
	}

  //6--check busy signal
  if(dn>6 && !memcmp(tmps+2,"BUSY",4))
  {
		b_modem_status=0x0d;//callee busy
		resp=38;
		goto L_END;
  }

  //7--check carrier
  if(dn>12 && !memcmp(tmps+2,"NO CARRIER",10))
  {
		b_modem_status=0x05;//has answer but not connected
		resp=39;
		goto L_END;
  }

  //--parallel phone detected
  if(dn>12 && !memcmp(tmps+2,"LINE IN USE",11))
  {
		modem_status=0x03;
		return 40;
  }

  //--line absent
  if(dn>9 && !memcmp(tmps+2,"NO LINE",7))
  {
		modem_status=0x33;//0x33
		return 41;
  }

  //--over current
  if(dn>1 && !memcmp(tmps,"X",1))
  {
		modem_status=0xec;
		return 42;
  }

  //--line broken
  if(dn>19 && !memcmp(tmps+2,"POLARITY REVERSAL",17))
  {
		modem_status=0x33;//0x03
		return 43;
  }

  //--no answer
  if(dn>11 && !memcmp(tmps+2,"NO ANSWER",9))
  {
		modem_status=0xea;
		return 44;
  }

  if(dn>7 && !memcmp(tmps+2,"ERROR",5))
  {
		modem_status=0xe9;
		return 45;
  }

  modem_status=0xef;//unknown error
  return 50;

L_TAIL:

	resp=0;
	if(!is_sync)
		goto L_END;

	x_retries=0;
	while(1)
	{
		if(is_half_duplex)
		{
			task_p1=20;
STEP20:
			a=step_turn_to_rx();
			if(!a && task_p2)return 0;
//printk(" step_rx:%d\r\n",a);
		}
		task_p1=21;
STEP21:
		a=step_rcv_v80_cmd(0,&dn);
		if(!a && task_p2)return 0;
//printk("SNRM a:%d,dn:%u,%02X%02X\r\n",a,dn,frame_pool[0],frame_pool[1]);
		if(a==2 && dn)//overflowed
		{
			b_modem_status=0x10;
			resp=51;
			goto L_END;
		}
		if(a || dn<2)
		{
			if(a==6)
			{
			 if(dn==14 && !memcmp(frame_pool,"\r\nNO CARRIER\r\n",14))
			 {
				b_modem_status=0x04;
				resp=52;
				goto L_END;
			 }
			}
			goto B_TAIL;
		}

		if(frame_pool[0]!=0x30 && frame_pool[0]!=0xff)
		{
//ScrPrint(0,0,0," SNRM a:%d,dn:%u,%02X%02X%02X%02X\r\n",a,dn,frame_pool[0],frame_pool[1],frame_pool[2],frame_pool[3]);
			b_modem_status=0x11;
			resp=53;
			goto L_END;
		}

		frame_address=frame_pool[0];
		if(x_retries && frame_pool[1]==0x11)break;
		if(frame_pool[1]!=0x93)
		{
			b_modem_status=0x12;
			resp=54;
			goto L_END;
		}

SEND_UA:
		if(is_half_duplex)
		{
			task_p1=22;
STEP22:
			a=step_turn_to_tx();
			if(!a && task_p2)return 0;
//s_UartPrint(" tx:%d\r\n",a);
		}

		memcpy(frame_pool,"\x30\x73\x19\xb1",4);
		task_p1=23;
STEP23:
		//--NOTE:SILICON modem needs 350ms wait at 9600bps
		a=step_send_to_serial1(frame_pool,4);
		if(!a && task_p2)return 0;
//s_UartPrint(" SEND_UA a:%d\r\n",a);

		if(is_half_duplex)
		{
			task_p1=24;
STEP24:
			a=step_turn_to_rx();
			if(!a && task_p2)return 0;
		}
		task_p1=25;
STEP25:
		a=step_rcv_v80_cmd(0,&dn);
		if(!a && task_p2)return 0;

//printk(" RR a:%d,dn:%u,%02X%02X\r\n",a,dn,frame_pool[0],frame_pool[1]);
		if(a==2 && dn)
		{
			b_modem_status=0x13;
			resp=55;
			goto L_END;
		}
		if(a || dn<2)goto B_TAIL;

		frame_address=frame_pool[0];
		if(frame_pool[1]==0x93)goto SEND_UA;
		if(frame_pool[1]==0x11)break;

B_TAIL:
		x_retries++;
		if(x_retries>=10)//5
		{
			b_modem_status=0x04;//NO CARRIER
			resp=56;
			goto L_END;
		}
	}//while(1)

L_END:
	if(resp)
	{
		if(dial_remains || !phone_tail)
		{
			hang_off();
			goto DIAL_BEGIN;
		}
		modem_status=b_modem_status;
		return resp;
	}

	led_auto_flash(N_LED_ONLINE,flashes,1,1);//modified on 2013.5.25
	ScrSetIcon(ICON_TEL_NO,S_ICON_UP);
	req_write=resp_write=resp_head=0;
	check_flag=1;
	modem_status=0x00;

	task_p1=0;
	return 0;
}

#ifdef MDM_TEST
uchar fetch_nac_pack(uchar port_no,uchar *out_pack,ushort *out_dlen)
{
	static ushort pn,i=0,si_flag=0;
	static uchar d_pool[2500];
	ushort wn,tmpu_data;
	uchar a,b;

	if(out_pack==NULL){si_flag=0;i=0;return 0;}
	if(PortRecv(port_no,d_pool+i,0))return 1;//no data found
	i++;
	switch(si_flag)
	{
	case 0:
		a=d_pool[0];
		//1--detect if the header is right, else search the first header
		if(a!=0x02){i=0;return 2;}
		si_flag=1;
	case 1:
		//3--detect if it's an entire pack
		if(i<5)return 4;//not an entire pack 1
		a=d_pool[1];
		b=d_pool[2];
		pn=(a>>4)*1000+(a&0x0f)*100+(b>>4)*10+(b&0x0f)+5;
		if(pn>2053)//invalid pack length,data may be lost
		{
			i=0;//throw away all the receiving data
			si_flag=0;
			return 5;
		}
		si_flag=2;
	case 2:
		if(i<pn)return 6;//not an entire pack 2
		//3--fetch a pack with header and tail from receive buffer
		memcpy(out_pack,d_pool,pn);

		for(a=0,wn=1;wn<pn;wn++)a^=out_pack[wn];

		//4--reply for the request pack
		si_flag=0;
		if(a)
		{
			i=0;
			return 7;//LRC error
		}

		*out_dlen=pn;
		i=0;
		return 0;
	 default:
		si_flag=0;
		i=0;
		return 8;
	 }//switch(a)
}

uchar rcv_sdlc_pack(uchar *str,ushort *dlen)
{
  uchar tmpc,resp,tmps[10];
  ushort i,j,pn;
  uint t0,timeout;

  *dlen=0;
  j=0;
  t0=GetTimerCount();//2s
  timeout=2000;
  resp=0;
  pn=100;
  while(1)
  {
	  if(GetTimerCount()-t0>timeout){resp=1;goto R_END;}

	  if(!kbhit()&&getkey()==KEYCANCEL)
	  {
			ScrPrint(0,7,0,"MODEM_RXD QUIT       ");
			resp=2;
			goto R_END;
	  }

	  tmpc=ModemAsyncGet(str+j);
	  if(tmpc==0x0c)continue;
	  if(tmpc)
	  {
			ScrPrint(0,7,0,"MODEM_RXD RESULT:%d",tmpc);
			resp=3;
			goto R_END;
	  }

	  t0=GetTimerCount();//1s
	  timeout=1000;

	  if(str[0]==0x02)
		j++;

	  if(j==3)
	  {
		sprintf(tmps,"%02X%02X",str[1],str[2]);
		pn=atoi((char*)tmps);
	  }

	  if(j==pn+5)break;

	  if(j>=2048)//1030
	  {
			ScrPrint(0,7,0,"MODEM_RXD OVERFLOW   ");
			resp=4;
			goto R_END;
	  }

  }//while(1)

  if(str[0]!=0x02)
  {
	ScrPrint(0,7,0,"ERR MODEM_RXD STX:%02X ",str[0]);
	resp=5;
	goto R_END;
  }

  if(str[j-2]!=0x03)
  {
	ScrPrint(0,7,0,"ERR MODEM_RXD ETX:%02X ",str[j-2]);
	resp=6;
	goto R_END;
  }

  for(tmpc=0,i=1;i<j;i++)tmpc^=str[i];
  if(tmpc)
  {
	ScrPrint(0,7,0,"ERR MODEM_RXD LRC    ");
	resp=7;
	goto R_END;
  }

R_END:
  *dlen=j;

  return resp;
}

//--header_bytes:numbers of 0x55 for leading flag byte ahead of the frame
void fsk_pack_up(uchar header_bytes,uchar *in_str,ushort *dlen,uchar *out_str)
{
	uchar sum;
	ushort i,dn;
	
	memset(out_str,0x55,header_bytes);
	dn=*dlen;
	if(!dn)
	{
		*dlen=header_bytes;
		return;
	}
	
	out_str[header_bytes]=in_str[0];
	out_str[header_bytes+1]=(dn-1)>>8;
	out_str[header_bytes+2]=(dn-1)&0xff;
	memcpy(out_str+header_bytes+3,in_str+1,dn-1);
	
	for(i=header_bytes,sum=0;i<header_bytes+3+dn-1;i++)sum+=out_str[i];
	out_str[header_bytes+3+dn-1]=256-sum;
	*dlen=header_bytes+3+dn;
	return;
}

uchar rcv_fsk_pack(uchar *str,ushort *dlen)
{
	uchar sum,tmpc,resp,tmps[2100];
	ushort i,j,pn,data_start,dn;
	
	TimerSet(1,30);
	resp=0;
	pn=0;
	while(1)
	{
		if(!TimerCheck(1)){resp=1;goto R_END;}
		
		if(!kbhit()&&getkey()==KEYCANCEL)
		{
			ScrPrint(0,7,0,"MODEM_RXD QUIT       ");
			resp=2;
			goto R_END;
		}
		
		tmpc=ModemRxd(tmps,&pn);
		if(!tmpc)break;
		if(tmpc==0x0c)continue;
		if(tmpc)
		{
			ScrPrint(0,7,0,"MODEM_RXD RESULT:%d",tmpc);
			resp=3;
			goto R_END;
		}
	}//while(1)

	//--filter the first byte
	for(i=0;i<pn;i++)
		if(tmps[i]!=0x55 && tmps[i]!=0xfd && tmps[i]!=0xf5)break;
	data_start=i;

	dn=(tmps[data_start+1]<<8)+tmps[data_start+2]+4;
	if(pn-data_start<dn)
	{
		ScrPrint(0,7,0,"RCV LEN ERR,DN:%d,PN:%d ",pn-data_start,dn);
		resp=4;
		goto R_END;
	}

	for(sum=0,i=data_start;i<data_start+dn-1;i++)sum+=tmps[i];
	sum=256-sum;
	if(sum!=tmps[data_start+dn-1])
	{
		ScrPrint(0,7,0,"RECV CHECKSUM ERR   ");
		resp=5;
		goto R_END;
	}

	str[0]=tmps[data_start];
	memcpy(str+1,tmps+data_start+3,dn-4);
	*dlen=dn-3;
R_END:
	if(resp)
	{
		*dlen=pn;
		memcpy(str,tmps,pn);
	}

	return resp;
}

void modem_test(void)
{
	  uchar a,tmpc,fn,mode,port_no,tel_no[100],tmps[8200],xstr[8200];
	  ushort i,j,sn,rn,dn,auto_answer,data_bits;
	  MODEM_CONFIG mdm;
	  int tmpd,loops;
	  uint t0;
	  uchar backup_telno[100]="\0";

loops=0;
	 while(1)
	 {
T_BEGIN:
		 ScrCls();
		 ScrPrint(0,0,0, "1-PICKUP 3-AT  V0609B");
		 ScrPrint(0,1,0, "4 - COM TEST");
		 ScrPrint(0,2,0, "5 - MODEM INIT");
		 ScrPrint(0,3,0, "6 - SYNC DIAL");
		 ScrPrint(0,4,0, "7 - ONHOOK");
		 ScrPrint(0,5,0, "8 - ASYNC DIAL");
		 ScrPrint(0,6,0, "9 - FSK DIAL");
		 ScrPrint(0,7,0, "0-CALLEE  2-PULSE");
//if(!loops){loops++;tmpc='6';}//'6'
//else
		 tmpc = getkey();
		 if(tmpc==KEYCANCEL)return;

		  switch(tmpc)
		  {
			 case '1':
				ScrCls();
				memset(&mdm,0x00,sizeof(mdm));
				mdm.ignore_dialtone=0x41;//double timeout
				mdm.pattern=0x07;
				while(1)
				{
					ScrPrint(0,5,0, "PICKING UP...  ");
					tmpc=ModemDial((COMM_PARA*)&mdm,tel_no,1);
					if(tmpc==0xfd)break;

					ScrPrint(0,5,0, "DIAL RESULT:%02X",tmpc);
					if(!kbhit() && getkey()==KEYCANCEL)break;
					Beep();

					DelayMs(1000);
				}
				break;
			 case '2':
				 ScrCls();

				 memset(&mdm,0x00,sizeof(mdm));
				 strcpy(tel_no,"1,2,3,4,5,6,7,8,9,*,0,#");

				 ScrPrint(0,2,0, "SELECT PULSE MODE:");
				 ScrPrint(0,3,0, "1.--61/39");
				 ScrPrint(0,4,0, "2.--67/33");

				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 if(tmpc=='2')mdm.mode=2;
				 else mdm.mode=1;
				 strcpy(tel_no,"1,2,3,4,5,6,7,8,9,*,0,#");
				 ScrCls();

				 ScrPrint(0,5,0, "DIALING %s...",tel_no);
				 //mdm.pattern=0x40;//9600bps
				 mdm.pattern=0x02;//1200bps
				 tmpc=ModemDial((COMM_PARA*)&mdm,tel_no,1);
				 Beep();
				 ScrPrint(0,5,0, "DIAL RESULT:%02X",tmpc);
				 getkey();
				break;
			 case '3':
				 PortOpen(P_MODEM,"115200,8,n,1");
				 PortOpen(P_RS232A,"115200,8,n,1");
				 ScrCls();
				 ScrPrint(0,0,0, "DIRECT ACCESS");
				 ScrPrint(0,7,0, "EXTERN:115200,8,N,1");
				 i=0;
				 while(1)
				 {
				  if(!kbhit())
				  {
					 a=getkey();
					 if(a==KEYCANCEL)break;
				  }

				  if(!PortRecv(P_RS232A,&a,0))
						PortSend(P_MODEM,a);

				  tmpc=PortRecv(P_MODEM,&a,0);
				  if(!tmpc)
				  {
						PortSend(P_RS232A,a);
						//ScrPrint(0,2,0,"%02X,I:%d ",a,i);
						i++;
				  }
				  //if(tmpc!=0xff)ScrPrint(0,3,0,"FAIL:%02X ",tmpc);
				}
				break;
			 case '4':
				 ScrCls();

				 ScrPrint(0,1,0, "SELECT PORT");
				 ScrPrint(0,2,0, "1--COM1");
				 ScrPrint(0,3,0, "2--PINPAD");
				 tmpc=getkey();
				 if(tmpc=='2')port_no=P_PINPAD;
				 else port_no=P_RS232A;

				 ScrCls();
				 ScrPrint(0,1,0, "SELECT SPEED");
				 ScrPrint(0,2,0, "1--600  2--1200");
				 ScrPrint(0,3,0, "3--9600 4--14400");
				 ScrPrint(0,4,0, "5--19200 6-28800");
				 ScrPrint(0,5,0, "7--38400 8-57600");
				 ScrPrint(0,6,0, "9--115200");
				 ScrPrint(0,7,0, "0--230400");
				 tmpc=getkey();
				 if(tmpc=='1')strcpy(tmps,"600,");
				 else if(tmpc=='2')strcpy(tmps,"1200,");
				 else if(tmpc=='3')strcpy(tmps,"9600,");
				 else if(tmpc=='4')strcpy(tmps,"14400,");
				 else if(tmpc=='5')strcpy(tmps,"19200,");
				 else if(tmpc=='6')strcpy(tmps,"28800,");
				 else if(tmpc=='7')strcpy(tmps,"38400,");
				 else if(tmpc=='8')strcpy(tmps,"57600,");
				 else if(tmpc=='0')strcpy(tmps,"230400,");
				 else strcpy(tmps,"115200,");
/*
				 ScrCls();
				 ScrPrint(0,1,0, "SELECT DATA BITS");
				 ScrPrint(0,2,0, "1--8");
				 ScrPrint(0,3,0, "2--7");
				 ScrPrint(0,4,0, "3--6");
				 ScrPrint(0,5,0, "4--5");
				 tmpc=getkey();
				 if(tmpc=='2')strcat(tmps,"7,");
				 else if(tmpc=='3')strcat(tmps,"6,");
				 else if(tmpc=='4')strcat(tmps,"5,");
				 else strcat(tmps,"8,");
				 if(tmpc<'1' || tmpc>'4')tmpc='1';
				 data_bits=9-(tmpc-'0');

				 ScrCls();
				 ScrPrint(0,1,0, "SELECT PARITY");
				 ScrPrint(0,2,0, "1--NONE");
				 ScrPrint(0,3,0, "2--EVEN");
				 ScrPrint(0,4,0, "3--ODD");
				 tmpc=getkey();
				 if(tmpc=='2')strcat(tmps,"e,");
				 else if(tmpc=='3')strcat(tmps,"o,");
				 else strcat(tmps,"n,");

				 ScrCls();
				 ScrPrint(0,1,0, "SELECT STOP BITS");
				 ScrPrint(0,2,0, "1--1");
				 ScrPrint(0,3,0, "2--2");
				 tmpc=getkey();
				 if(tmpc=='2')strcat(tmps,"2");
				 else strcat(tmps,"1");
*/
data_bits=8;
//port_no=P_PINPAD;
//strcpy(tmps,"115200,8,n,1");
strcat(tmps,"8,n,1");
				 PortOpen(P_RS232A,tmps);

				 tmpc=PortOpen(port_no,tmps);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,5,0, "PORT_OPEN RESULT:%02X",tmpc);
					getkey();
					break;
				 }

				 ScrCls();
				 ScrPrint(0,0,0, "%s",tmps);

				 ScrPrint(0,1,0, "1--CLIENT");
				 ScrPrint(0,2,0, "2--SERVER");
				 tmpc=getkey();
				 ScrClrLine(2,7);
				 if(tmpc=='2')
				 {
					 ScrPrint(0,1,0, "SERVER    ");
					 while(1)
					 {
						tmpc=PortRecv(port_no,xstr,0);
						if(!tmpc)PortSend(port_no,xstr[0]);

						if(!kbhit() && getkey()==KEYCANCEL)goto T_BEGIN;
					 }
				 }

				 ScrPrint(0,6,0, "INPUT TX LEN:");
				 for(i=0;i<5;i++)
				 {
					tmpc=getkey();
					if(tmpc==KEYCANCEL)goto T_BEGIN;
					if(tmpc==KEYENTER)break;
					if(tmpc<'0'||tmpc>'9')tmpc='0';
					tmps[i]=tmpc;
					tmps[i+1]=0;
					ScrPrint((20-i)*6,7,0,"%s",tmps);
				 }
				 sn=atoi(tmps);
				 if(sn<3)sn=3;
				 ScrClrLine(2,7);

				 while(sn<2060)
				 {

					ScrPrint(0,3,0, "%d,SENDING ...",sn);
					//tmps[6]=sn&0xff;
				 for(i=0;i<sizeof(tmps)-5;i++)tmps[5+i]=i & 0xff>>(8-data_bits);//&0xff
				 memcpy(tmps,"\x10\x00\x10\x00\x10",5);
				 tmps[sn-2]=tmps[sn-3]^0xff;
				 tmps[sn-1]=tmps[sn-2];
				 tmpc=PortSends(port_no,tmps,sn);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,4,0, "PORTSENDS RESULT:%02X",tmpc);
					getkey();
					break;
				 }
				 ScrPrint(0,5,0, "%d,RECEIVING ...",sn);
				 rn=0;
				 while(11)
				 {
					if(!kbhit())
					{
					  tmpc=getkey();
					  if(tmpc==KEYCANCEL){tmpc=0xfd;break;}
					  if(tmpc==KEYENTER)
						ScrPrint(0,5,0, "%d,RECEIVING %d...",sn,rn);
					  else if(tmpc=='1')
					  {
						  PortSends(P_RS232A,"\xaa\xaa\xaa\xaa\xaa",5);
						  PortSends(P_RS232A,xstr,rn);
					  }
					  else if(tmpc=='2')
					  {
						  PortSends(P_RS232A,"\xbb\xbb\xbb\xbb\xbb",5);
						  PortSends(P_RS232A,tmps,3);
					  }
					}
					//tmpc=PortRecv(port_no,xstr+rn,0);
					//if(tmpc)continue;
					tmpd=PortRecvs(port_no,xstr+rn,1000);
					if(tmpd<=0)continue;
					//rn++;
					rn+=tmpd;

					if(rn>=sn)break;
				  }//while(11)

				  if(tmpc)
				  {
					Beep();
					ScrPrint(0,5,0, "PORTRCV RESULT:%02X",tmpc);
					getkey();
					break;
				  }

				  if(rn!=sn)
				  {
					Beep();
					ScrPrint(0,5,0, "SN/RN mismatch:%d-%d",sn,rn);
					getkey();
					break;
				  }

				  if(memcmp(tmps,xstr,sn))
				  {
					Beep();
					ScrPrint(0,5,0, "data mismatch,SN:%d",sn);
					while(1)
					{
						ScrPrint(0,7,0, "ANY KEY ...");
						if(getkey()==KEYCANCEL)break;
						tmpc=PortSends(P_RS232A,xstr,rn);
						ScrClrLine(6,7);
						for(i=0;i<sn;i++)
							if(tmps[i]!=xstr[i])
							{
								ScrPrint(0,6,0, "OFFSET:%d,%02X-%02X",i,tmps[i],xstr[i]);
								if(getkey()==KEYCANCEL)break;
							}
					}
				  }
					ScrPrint(0,6,0, "%d,RECEIVED %d ",sn,rn);
					DelayMs(1000);
					//PortSend(port_no,0x06);
					sn=(sn+1)%2060;
					if(!sn)sn=3;
				 }//while

				 PortClose(P_RS232A);
				 PortClose(port_no);
				break;
			 case '5':
				 ScrCls();
				 tmpc=s_ModemInit(2);
				 ScrPrint(0,5,0, "INIT RESULT:%d",tmpc);
				 DelayMs(1500);
				 break;
			 case '6':
				 ScrCls();

				 memset(&mdm,0x00,sizeof(mdm));

				 ScrPrint(0,0,0, "SELECT DIAL NO:");
				 ScrPrint(0,1,0, "1--85  2--8337");
				 ScrPrint(0,2,0, "3--1,2,3,...,0");
				 ScrPrint(0,3,0, "4--983163166");
				 ScrPrint(0,4,0, "5--1   6-8312");
				 ScrPrint(0,5,0, "7--902166674500");
				 //ScrPrint(0,6,0, "8--90018665214314");
				 ScrPrint(0,6,0, "8--982989000");
				 ScrPrint(0,7,0, "0--...  X--8335");
//tmpc='6';//'6'
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 if(tmpc=='1')strcpy(tel_no,"85");
				 //if(tmpc=='1')strcpy(tel_no,"908713628925");
				 else if(tmpc=='2')strcpy(tel_no,"8337");
				 else if(tmpc=='3')
				 {
					strcpy(tel_no,",1,2,3,4,5,6,7,8,9,*,0,#");
					mdm.code_hold=100;
					//ModemExCommand("AT+GCI=26\r",NULL,NULL,0);
					ModemExCommand("ATS91=2\r",NULL,NULL,0);
				 }
				 else if(tmpc=='4')strcpy(tel_no,"983163166");
				 else if(tmpc=='5')strcpy(tel_no,"1");
				 else if(tmpc=='6')strcpy(tel_no,"8312");
				 else if(tmpc=='7')
				 {
					strcpy(tel_no,"902166674500");
					mdm.code_hold=90;
				 }
				 else if(tmpc=='8')
				 {
					//strcpy(tel_no,"94008200358");
					//strcpy(tel_no,"90018665214314");
					strcpy(tel_no,"982989000");
					mdm.code_hold=90;
					mdm.ignore_dialtone=0x40;//double timeout
				 }
				 else if(tmpc==KEYENTER)strcpy(tel_no,"8335");
				 else//manual input the tel_no
				 {
				 ScrCls();
				 if(!backup_telno[0])strcpy(backup_telno,tel_no);
				 strcpy(tmps,backup_telno);
				 ScrPrint(0,2,0,"INPUT TEL NO:%.29s",backup_telno);
				 ScrGotoxy(0,7);
				 tmpc=GetString(tmps,0x21|0x10|0x80,0,49);//input with initial value
				 if(tmpc)break;
				 tmps[tmps[0]+1]=0;
				 if(tmps[0])strcpy((char*)backup_telno,(char*)tmps+1);
				 else strcpy(backup_telno,tel_no);//avoid null tel_no
				 strcpy(tel_no,backup_telno);
				 }
				 
				 ScrCls();
				 ScrPrint(0,2,0, "SELECT SPEED:");
				 ScrPrint(0,3,0, "1.--1200");
				 ScrPrint(0,4,0, "2.--2400");
				 ScrPrint(0,5,0, "3.--9600");
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;

				 mdm.pattern=0x07;//1200bps,0x02
				 if(tmpc=='2')mdm.pattern|=0x27;//0x20
				 else if(tmpc=='3')mdm.pattern|=0x40;

				 if(tmpc!='3')
				 {
				 ScrCls();
				 ScrPrint(0,2,0, "SPEED MODE:");
				 ScrPrint(0,3,0, "0--DEFAULT");
				 ScrPrint(0,4,0, "1--CHANGE");
//tmpc='1';
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 if(tmpc=='1')mdm.ignore_dialtone|=0x80;//2400 fast connect,1200 standard connect
				 }

				 mdm.async_format=0x08;

DIAL_AGAIN:
				 ScrCls();
				 ScrPrint(0,5,0, "DIALING %s...",tel_no);
				 tmpc=ModemDial((COMM_PARA*)&mdm,tel_no,1);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,5,0, "DIAL RESULT:%02X.",tmpc);
					getkey();
					break;
				 }
				 Beep();
				 ScrPrint(0,6,0, "DIAL OK");
				 ScrPrint(0,7,0, "5-REDIAL,OTHER-TX/RX");

				 tmpc=getkey();
				 if(tmpc==KEYCANCEL){OnHook();break;}
				 if(tmpc=='5'){OnHook();goto DIAL_AGAIN;}
				 ScrCls();
				 ScrPrint(0,6,0, "INPUT TX LEN:");
				 for(i=0;i<5;i++)
				 {
					tmpc=getkey();
					if(tmpc==KEYCANCEL){OnHook();break;}
					if(tmpc==KEYENTER)break;
					if(tmpc<'0'||tmpc>'9')tmpc='0';
					tmps[i]=tmpc;
					tmps[i+1]=0;
					ScrPrint((20-i)*6,7,0,"%s",tmps);
				 }
				 sn=atoi(tmps);
//sn=500;//2000
				 ScrCls();
				 for(i=0;i<sizeof(tmps)-5;i++)tmps[5+i]=i&0xff;
				 memcpy(tmps,"\x60\x00\x00\x00\x00",5);
				 while(sn<2060)
				 {

					ScrPrint(0,5,0, "%d,SENDING ...",sn);
					//tmps[6]=sn&0xff;
					tmpc=ModemTxd(tmps,sn);
printk("TXD:%02X,sn:%d \n",tmpc,sn);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,5,0, "MODE_TXD RESULT:%02X",tmpc);
					getkey();
					break;
				 }

				ScrPrint(0,5,0, "%d,RECEIVING ...",sn);
				 while(11)
				 {
					if(!kbhit() && getkey()==KEYCANCEL){tmpc=0xfd;break;}

					tmpc=ModemRxd(xstr,&rn);
					if(tmpc!=0x0c)break;
				  }//while(11)

printk("RXD:%02X,rn:%d \n",tmpc,rn);
				  if(tmpc)
				  {
					Beep();
					ScrPrint(0,5,0, "MODE_RXD RESULT:%02X",tmpc);
					getkey();
					break;
				  }

				  if(rn!=sn)
				  {
					Beep();
					ScrPrint(0,5,0, "SN/RN mismatch:%d-%d",sn,rn);
					getkey();
					break;
				  }

				  if(memcmp(tmps+5,xstr+5,sn-5))
				  {
					Beep();
					ScrPrint(0,5,0, "data mismatch,SN:%d",sn);
					getkey();
					break;
				  }
					ScrPrint(0,6,0, "%d,RECEIVED %d ",sn,rn);
printk("%d,RECEIVED %d \n",sn,rn);
					sn++;
				 }//while

				 OnHook();
				 break;
			 case '7':
				 ScrCls();
				 ScrPrint(0,5,0, "ONHOOK ...");
				 OnHook();
				 //PortClose(P_PINPAD);
				 break;
			 case '8':
				 memset(&mdm,0x00,sizeof(mdm));
				ScrCls();
				 ScrPrint(0,0,0, "ASYNC DIAL");
				 ScrPrint(0,1,0, "SELECT DIAL NO:");
				 ScrPrint(0,2,0, "1--996169 8-8312");
				 ScrPrint(0,3,0, "2--983163166");
				 ScrPrint(0,4,0, "3--1    4--90519");
				 ScrPrint(0,5,0, "5--8335");
//tmpc='8';
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 strcpy(tel_no,"996169");
				 if(tmpc=='2')strcpy(tel_no,"983163166");
				 else if(tmpc=='3')strcpy(tel_no,"1");
				 else if(tmpc=='5')strcpy(tel_no,"8335");
				 else if(tmpc=='8')strcpy(tel_no,"8312");

				 ScrCls();
				 ScrPrint(0,2,0, "SELECT SPEED:");
				 ScrPrint(0,3,0, "1-1200");
				 ScrPrint(0,4,0, "2-2400");
				 ScrPrint(0,5,0, "3-9600 4-14400");
				 ScrPrint(0,6,0, "5-33600  6-57600");
//tmpc='6';
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 mdm.pattern=0x87;
				 mdm.ignore_dialtone=0x40;
				 mdm.async_format=0xf0;
				 if(tmpc=='1')mdm.async_format=0x10;
				 else if(tmpc=='2')mdm.async_format=0x20;
				 else if(tmpc=='3')mdm.async_format=0x50;
				 else if(tmpc=='4')mdm.async_format=0x70;
				 else if(tmpc=='5')mdm.async_format=0xd0;
				 else if(tmpc=='6')mdm.async_format=0xf0;

				 if(tmpc=='1'||tmpc=='2')
				 {
					 ScrCls();
					 ScrPrint(0,2,0, "SPEED MODE:");
					 ScrPrint(0,3,0, "0--DEFAULT");
					 ScrPrint(0,4,0, "1--CHANGE");
					 //tmpc='1';
					 tmpc=getkey();
					 if(tmpc==KEYCANCEL)break;
					 if(tmpc=='1')mdm.ignore_dialtone|=0x80;//2400 fast connect,1200 standard connect
				 }

				 PortOpen(P_RS232A,"115200,8,n,1");

ASYNC_REDIAL:
				 ScrCls();
				 ScrPrint(0,0,0, "FORMAT:%02X,DIALING %s...",
								mdm.async_format,tel_no);
				 t0=GetTimerCount();
				 tmpc=ModemDial((COMM_PARA*)&mdm,tel_no,1);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,4,0, "DIAL RESULT:%02X",tmpc);
					ScrPrint(0,5,0, "ELAPSED:%d ",GetTimerCount()-t0);
					getkey();
					break;
				 }
				 Beep();
				 ScrPrint(0,2,0, "OK,ELAPSED:%d ",GetTimerCount()-t0);

				 dn=2000;
				 ScrPrint(0,3,0, "INPUT TX LEN:");
				 for(i=0;i<5;i++)
				 {
					tmpc=getkey();
					if(tmpc==KEYCANCEL){OnHook();break;}
					if(tmpc==KEYENTER)break;
					if(tmpc<'0'||tmpc>'9')tmpc='0';
					tmps[i]=tmpc;
					tmps[i+1]=0;
					ScrPrint((20-i)*6,4,0,"%s",tmps);
				 }
				 dn=atoi(tmps);

				//--wait connection info
				ScrClrLine(2,7);
				while(1)
				{
					ScrPrint(0,2,0,"RCV ASYNC CONNECT INFO...");
					tmpc=rcv_sdlc_pack(xstr,&rn);
					if(tmpc==2){OnHook();goto T_BEGIN;}//kbhit CANCEL
					if(!tmpc&&rn>=17&&!memcmp(xstr+8,"CONNECT",7))break;
					ScrPrint(0,4,0,"RESULT:%d,RN:%d",tmpc,rn);
				}//while(1),wait for connection info

				 while(dn<2060)
				 {

				 //tmps[6]=sn&0xff;
				 for(i=0;i<2043;i++)tmps[5+i]=i&0xff;
				 memcpy(tmps,"\x60\x00\x00\x00\x00",5);
				 sn=dn;
				 nac_pack_up(tmps,&sn);
				 memcpy(tmps,resp_pack,sn);
				 ScrPrint(0,5,0, "%d,SENDING %u...",dn,sn);
				 ModemReset();
				 tmpc=ModemTxd(tmps,sn);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,5,0, "TX RESULT:%02X ",tmpc);
					getkey();
					break;
				 }

				 ScrPrint(0,6,0, "%d,RECEIVING ...",dn);
				 tmpc=rcv_sdlc_pack(xstr,&rn);
				  if(tmpc)
				  {
					if(tmpc==2)break;//kbhit cancel

					//Beep();
					ScrPrint(0,7,0, "RX FAIL:%02X,RN:%d ",tmpc,rn);
					//DelayMs(5000);
					continue;
				  }

				  if(tmpc || rn<sn)
				  {
					Beep();
					ScrPrint(0,7,0,"RCV:%d,SN:%d,RN:%d",tmpc,sn,rn);
					getkey();
					PortSends(P_RS232A,xstr,rn);
					continue;
					//break;
				  }
				  if(memcmp(tmps,xstr,sn))
				  {
					Beep();
					ScrPrint(0,7,0, "data error,SN:%d,RN:%d,I:%d ",sn,rn,i);
					PortSends(P_RS232A,xstr,rn);
					getkey();
					break;
				  }
				  ScrPrint(0,7,0, "%d,RECEIVED %d ",dn,rn);

				  dn++;
				 }//while(dn<2060)

				 OnHook();
				 PortClose(P_RS232A);
				break;
			 case '9':
				 memset(&mdm,0x00,sizeof(mdm));
				 ScrCls();
				 ScrPrint(0,0,0, "FSK DIAL");
				 ScrPrint(0,1,0, "SELECT DIAL NO:");
				 ScrPrint(0,2,0, "1--8312   2-1");
				 ScrPrint(0,3,0, "3--9051986186261");
				 ScrPrint(0,4,0, "4--902161621615");
				 ScrPrint(0,5,0, "5--983509525");
				 ScrPrint(0,6,0, "6--961682442  7-85");
//tmpc='1';
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 strcpy(tel_no,"8312");
				 if(tmpc=='2')strcpy(tel_no,"1");
				 else if(tmpc=='3')strcpy(tel_no,"9051986186261");
				 else if(tmpc=='4')strcpy(tel_no,"902161621615");
				 else if(tmpc=='5')strcpy(tel_no,"983509525");
				 else if(tmpc=='6')strcpy(tel_no,"961682442");
				 else if(tmpc=='7')strcpy(tel_no,"85");

				 ScrCls();
				 ScrPrint(0,2,0, "SELECT FORMAT:");
				 ScrPrint(0,3,0, "8-B202_FSK");
				 ScrPrint(0,4,0, "9--V23C_FSK");
tmpc='8';
//				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;

				 mdm.pattern=0x87;
				 mdm.ignore_dialtone=0x40;
				 mdm.async_format=0x08;
				 if(tmpc=='9')mdm.async_format=0x09;
FSK_REDIAL:
				 ScrCls();
				 ScrPrint(0,0,0, "FORMAT:%02X,DIALING %s...",
								mdm.async_format,tel_no);
				 t0=GetTimerCount();
				 tmpc=ModemDial((COMM_PARA*)&mdm,tel_no,1);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,4,0, "DIAL RESULT:%02X",tmpc);
					ScrPrint(0,5,0, "ELAPSED:%d ",GetTimerCount()-t0);
					getkey();
					break;
				 }
				 Beep();
				 ScrPrint(0,2,0, "OK,ELAPSED:%d ",GetTimerCount()-t0);
				 PortOpen(P_RS232A,"115200,8,n,1");

				 dn=2000;
				 ScrPrint(0,3,0, "INPUT TX LEN:");
				 for(i=0;i<5;i++)
				 {
					tmpc=getkey();
					if(tmpc==KEYCANCEL){OnHook();break;}
					if(tmpc==KEYENTER)break;
					if(tmpc<'0'||tmpc>'9')tmpc='0';
					tmps[i]=tmpc;
					tmps[i+1]=0;
					ScrPrint((20-i)*6,4,0,"%s",tmps);
				 }
				 dn=atoi(tmps);

				//--wait connection info
				ScrClrLine(2,7);
				while(1)
				{
					ScrPrint(0,2,0,"RCV FSK CONNECT INFO...");
					tmpc=rcv_fsk_pack(xstr,&rn);
					if(tmpc==2){OnHook();goto T_BEGIN;}//kbhit CANCEL
					if(!tmpc&&rn>=9&&!memcmp(xstr,"CONNECTED",9))break;

					ScrPrint(0,4,0,"RESULT:%d,RN:%d",tmpc,rn);
				}//while(1),wait for connection info

				 while(dn<2060)
				 {
				 for(i=0;i<sizeof(tmps);i++)tmps[i]=i&0xff;
				 tmps[1]=dn&0xff;
/*
				 memcpy(tmps,"\x87"//\x00\x3C"
						 "\x00\x31\x2E\x32\x35\x31\x35\x31\x39\x30"
						 "\x30\x31\x34\x30\x30\x30\x31\x30\x30\x35"
						 "\x30\x30\x30\x30\x36\x36\x00\x00\x34\x46"
						 "\x42\x32\x42\x43\x41\x37\x34\x35\x36\x39"
						 "\x32\x33\x42\x39\x32\x45\x39\x44\x30\x32"
						 "\x45\x43\x37\x33\x35\x35\x35\x43\x34\x33"
						 //"\x63",
						 ,61);
				 dn=61;
*/
				 sn=dn;
				 fsk_pack_up(20,tmps,&sn,xstr);
				 ScrPrint(0,5,0, "%d,SENDING %u...",sn,dn);
				 tmpc=ModemTxd(xstr,sn);
				 if(tmpc)
				 {
					Beep();
					ScrPrint(0,5,0, "TX RESULT:%02X ",tmpc);
					getkey();
					break;
				 }

				 ScrPrint(0,6,0, "%d,RECEIVING ...",sn);
				 tmpc=rcv_fsk_pack(xstr,&rn);
				 if(tmpc==2)break;//kbhit CANCEL

				 //--filter the handshake response
				 if(!tmpc&&rn>=9&&!memcmp(xstr,"CONNECTED",9))continue;

				 if(tmpc || rn<dn)
				 {
					Beep();
					ScrPrint(0,7,0, "RCV:%d,DN:%d,RN:%d ",tmpc,dn,rn);
					getkey();
					PortSends(P_RS232A,xstr,rn);
					//continue;
					break;
				 }
				  if(memcmp(tmps,xstr,dn))
				  {
					Beep();
					ScrPrint(0,7,0, "data error,SN:%d,RN:%d,I:%d ",dn,rn,i);
					PortSends(P_RS232A,xstr,rn);
					getkey();
					break;
				  }
				  ScrPrint(0,7,0, "%d,RECEIVED %d ",sn,rn);

				  dn++;
				 }//while

				 OnHook();
//				 PortClose(P_RS232A);
				break;
			 case '0':
				 ScrCls();
				 memset(&mdm,0x00,sizeof(mdm));
				 //mdm.pattern=0x40;//9600bps
				 mdm.ignore_dialtone=0x02;

				 ScrPrint(0,1,0, "SELECT ASYNC/SYNC:");
				 ScrPrint(0,2,0, "1.--SYNC,AUTO ANSWER");
				 ScrPrint(0,3,0, "2.--SYNC,NOT AA");
				 ScrPrint(0,4,0, "3.--ASYNC,AUTO ANSWER");
				 ScrPrint(0,5,0, "4.--ASYNC,NOT AA");
				 ScrPrint(0,6,0, "5.--FSK,AUTO ANSWER");
				 ScrPrint(0,7,0, "6.--FSK,NOT AA");
				 tmpc=getkey();
				 if(tmpc==KEYCANCEL)break;
				 if(tmpc=='1'||tmpc=='2')
				 {
					 mdm.pattern=0x07;
					 mode=1;
				 }
				 else if(tmpc=='3'||tmpc=='4')
				 {
					 mdm.pattern=0x87;
					 mode=0;
				 }
				 else
				 {
					 mdm.pattern=0x87;
					 mode=2;//FSK
				 }
				 auto_answer=0;
				 if(tmpc=='1'||tmpc=='3'||tmpc=='5')auto_answer=1;

				 if(mode<2)
				 {
					 ScrCls();
					 ScrPrint(0,1,0, "SELECT SPEED:");
					 ScrPrint(0,2,0, "1.--1200");
					 ScrPrint(0,3,0, "2.--2400");
					 ScrPrint(0,4,0, "3.--9600");
					 if(!mode)
					 {
						 ScrPrint(0,5,0, "4.--14400");
						 ScrPrint(0,6,0, "5.--33600");
						 ScrPrint(0,7,0, "6.--57600");
					 }
					 
					 tmpc=getkey();
					 if(tmpc==KEYCANCEL)break;
					 
					 if(tmpc=='1')mdm.pattern|=0x00;
					 else if(tmpc=='2')mdm.pattern|=0x20;
					 else if(tmpc=='3')mdm.pattern|=0x40;
					 else if(tmpc=='4')mdm.pattern|=0x60;
					 else if(tmpc=='5')mdm.async_format|=0xd0;
					 else if(tmpc=='6')mdm.async_format|=0xf0;
					 else mdm.async_format|=0xf0;
				 }//if(mode<2)

				 if(!mode)
				 {
					 ScrCls();
					 ScrPrint(0,1,0, "SELECT FORMAT:");
					 ScrPrint(0,2,0, "0-8N1  1-8E1");
					 ScrPrint(0,3,0, "2-8O1  3-7N1");
					 ScrPrint(0,4,0, "4-7E1  5-7O1");
					 tmpc=getkey();
					 if(tmpc==KEYCANCEL)break;
					 if(tmpc>='0' && tmpc<='5')mdm.async_format|=tmpc-'0';
				 }
				 else if(mode==1 && mdm.pattern<0x40)
				 {
					 ScrCls();
					 ScrPrint(0,1,0, "SET FAST CONNECT:");
					 ScrPrint(0,2,0, "0-DEFAULT");
					 ScrPrint(0,3,0, "1-CHANGE");
					 tmpc=getkey();
					 if(tmpc==KEYCANCEL)break;
					 if(tmpc>='1')mdm.ignore_dialtone|=0x80;
				 }
				 else if(mode==2)
				 {
					 ScrCls();
					 ScrPrint(0,1,0, "SELECT FORMAT:");
					 ScrPrint(0,5,0, "8-B202_FSK");
					 ScrPrint(0,6,0, "9-V23C_FSK");
					 tmpc=getkey();
					 if(tmpc==KEYCANCEL)break;
					 if(tmpc=='8' || tmpc=='9')mdm.async_format|=tmpc-'0';
				 }

CALLEE_RETRY:
				ScrCls();
				if(mode==1)ScrPrint(0,0,0, "CALLEE,SYNC");
				else if(mode==2)ScrPrint(0,0,0, "CALLEE,FSK");
				else ScrPrint(0,0,0, "CALLEE,ASYNC");
				ScrPrint(0,1,0, "PATTERN:%02X,FORMAT:%02X",mdm.pattern,mdm.async_format);

				if(mode==1)strcpy(tmps,"9600,8,n,1");
				else strcpy(tmps,"115200,8,n,1");
				PortOpen(COM1,tmps);
				ScrPrint(0,7,0, "EXTERNAL:%s",tmps);

				tmpc=ModemDial((COMM_PARA*)&mdm,tel_no,1);
CALLEE_WAIT:
				ScrClrLine(2,6);
				ScrPrint(0,2,0, "CALLEE RESULT:%02X",ModemCheck());
				ScrPrint(0,3,0, "5-RETRY,6-ONHOOK");
				if(!auto_answer)fetch_nac_pack(COM1,NULL,NULL);

				//--FSK and ASYNC wait for connect message
				while(!mode || mode==2)
				{
					if(!kbhit() && getkey()==KEYCANCEL){OnHook();goto T_BEGIN;}
					
					tmpc=ModemCheck();
					ScrPrint(0,2,0, "STATUS:%02X  ",tmpc);
					
					tmpc=ModemRxd(tmps,&rn);
					if(rn)
					{
						ScrPrint(0,4,0,"ModemRxd,RN:%d ",rn);
						PortSends(COM1,tmps,rn);
					}
					if(tmpc)continue;
					
					if(rn>10 && !memcmp(tmps,"\r\nCONNECT",9))
					{
						ScrPrint(0,5,0,"CONNECTED  ");
						break;
					}
				}//while(async or FSK),wait for connect message
				

				//--FSK and ASYNC shake hands
				loops=0;
				while(auto_answer && (!mode||mode==2))
				{
					if(!kbhit() && getkey()==KEYCANCEL){OnHook();goto T_BEGIN;}

					if(!mode)
					{
						sn=12;
						memcpy(tmps,"\x60\x00\x00\x00\x00",5);
						memcpy(tmps+5,"CONNECT",7);
						nac_pack_up(tmps,&sn);
						memcpy(xstr,resp_pack,sn);
					}
					else
					{
						sn=9;
						memcpy(tmps,"CONNECTED",sn);
						fsk_pack_up(20,tmps,&sn,xstr);
					}
					
printk("%d,shakehand tx...\n",loops);
					//memset(xstr,0x55,30);
					//memcpy(xstr+30,"\x81\x00\x05\xd0\xa8,*\x00\x10",9);
					tmpc=ModemTxd(xstr,sn);
					if(tmpc)
					{
printk(" tx failed:%d.\n",tmpc);
//						goto CALLEE_RETRY;
					}
for(i=0;i<sn;i++)
	printk("%02X ",xstr[i]);
printk("\n");

printk("%d,shakehand rx...",loops);
					if(!mode)tmpc=rcv_sdlc_pack(tmps,&rn);
					else tmpc=rcv_fsk_pack(xstr,&rn);
					if(!tmpc)
					{
printk("OK.\n");
						ScrPrint(0,5,0,"SHAKEHAND OK ");
						break;
					}
printk(" rx failed:%d.\n",tmpc);
for(i=0;i<sn;i++)
printk("%02X ",xstr[i]);
printk("\n");
					loops++;
					if(loops>=5)//10
					{
printk("timeout.\n");
						OnHook();
						goto CALLEE_RETRY;
					}
					
				}//while(async or FSK),shake hands
				
				if(auto_answer && (!mode||mode==2))
				{
printk("begin to send data...\n");
					if(mode==2)fsk_pack_up(20,xstr,&rn,tmps);
					ModemTxd(tmps,rn);
				}

				i=0;
				loops=0;
				if(auto_answer && mode!=1)TimerSet(1,50);//5s rx timeout for auto answer mode
				while(102)
				{
					if(!kbhit())
					{
						tmpc=getkey();
						if(tmpc==KEYCANCEL)break;
						if(tmpc=='0')i=0;
						if(tmpc=='5')goto CALLEE_RETRY;
						if(tmpc=='6')break;
					}
					tmpc=ModemCheck();
					//if(tmpc==0x04)ScrClrLine(4,6);
					ScrPrint(0,4,0, "%d,STATUS:%02X ",loops,tmpc);

					if(mode==2 && tmpc&&tmpc!=0x08&&tmpc!=0x01&&tmpc!=0x09)
					{
						Beep();
						ScrPrint(0,5,0, "%d,KEY... ",loops);
						getkey();
						ScrPrint(0,5,0, "            ");
						goto CALLEE_RETRY;
					}

					if(!auto_answer)
					{
						sn=0;
						if(mode==1)//sync
							tmpc=fetch_nac_pack(COM1,xstr,&sn);
						else
						{
							tmpc=PortRecv(COM1,xstr,0);
							if(!tmpc)sn=1;
						}
						//ScrPrint(0,6,0,"PortRecv:%d,SN:%d  ",tmpc,sn);
						if(sn)
						{
							loops++;
							ScrPrint(0,6,0,"%d,Fetch:%d,SN:%d  ",loops,tmpc,sn);
						}
						if(!tmpc)tmpc=ModemTxd(xstr,sn);

						//ScrPrint(0,1,0,"ModemRxd..");
						if(mode==1)
						{
							tmpc=ModemRxd(tmps,&rn);
							if(!tmpc)
							{
								ScrPrint(0,5,0,"%d,RECV:%d,RN:%d  ",loops,tmpc,rn);
								PortSends(COM1,tmps,rn);
							}
						}
						else if(mode==2)
						{
							tmpc=rcv_fsk_pack(tmps,&rn);
							if(tmpc)
							{
								OnHook();
								goto CALLEE_RETRY;
							}

							ScrPrint(0,5,0,"%d,RECV:%d,RN:%d  ",loops,tmpc,rn);
							PortSends(COM1,tmps,rn);
						}
						else
						{
							tmpc=ModemAsyncGet(&tmpc);
							if(tmpc && tmpc!=0x0c)goto CALLEE_WAIT;

							PortSend(COM1,tmpc);
						}

						continue;
					}

					if(mode!=1 && !TimerCheck(1))
					{
						Beep();
						ScrPrint(0,6,0,"RX TIME OUT ");
						DelayMs(3000);
						OnHook();
						goto CALLEE_RETRY;
					}

					//--auto answer mode
					if(mode<2)tmpc=ModemRxd(tmps,&rn);
					else tmpc=rcv_fsk_pack(xstr,&rn);
					if(mode!=1 && tmpc && tmpc!=0x0c)
					{
						Beep();
						ScrPrint(0,6,0,"RX FAIL:%d ",tmpc);
						DelayMs(3000);
						OnHook();
						goto CALLEE_RETRY;
					}
					if(!tmpc)
					{
						ScrPrint(0,1,0,"%d,ECHO %d  ",i++,rn);
printk("rn:%d,echo...",rn);
						if(mode==2)fsk_pack_up(20,xstr,&rn,tmps);
for(i=0;i<rn;i++)
	printk("%02X ",tmps[i]);
printk("\n");
						tmpc=ModemTxd(tmps,rn);
						if(tmpc)
						{
							Beep();
							ScrPrint(0,6,0,"TX FAIL:%d ",tmpc);
printk("tn:%d,tx failed.\n",rn,tmpc);
							DelayMs(3000);
							goto CALLEE_WAIT;
						}
						//--reset rx timeout timer
						TimerSet(1,50);
					}

				}//while(102)
			PortClose(COM1);
			OnHook();
			break;
			}//switch
	 }//while(1)
}
#endif

