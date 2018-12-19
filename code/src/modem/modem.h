#ifndef _MODEM_H_
#define _MODEM_H_

//2007.3.15:added GsmCallModem() and struct GSM_CALL

//common constants
#define PACK_SIZE   2060
#define FRAME_SIZE  2060
#define DATA_SIZE   2048
#define STX 0x02
#define TM_MDM				2/*soft timer no for SIO*/
#define MAX_SYNC_POLLS  5
#define AUTO_RESPONSE_TIMEOUT 800
#define MAX_SYNC_T_RETRIES 15
#define MAX_SYNC_R_RETRIES 8

#define ERR_SDLC_TX_UNDERRUN 10
#define ERR_SDLC_TX_OVERRUN  11
#define ERR_SDLC_RX_OVERRUN  12

//register definition
#define rPMB_CONTROL  (*(volatile unsigned int*)0x00080100)
#define rDMU_PWD_BLK2  (*(volatile unsigned int*)0x010240C4)
#define rVIC1_INT_ENABLE  (*(volatile unsigned int*)0x0102B010)
#define rVIC1_INT_ENCLEAR (*(volatile unsigned int*)0x0102B014)
#define rTIMER7_INT_CLR   (*(volatile unsigned int*)0x000C302C)
#define rGPB_DIN          (*(volatile unsigned int*)0x000CD800)
#define rGPD_DIN          (*(volatile unsigned int*)0x000D7800)
#define rGPD_DOUT         (*(volatile unsigned int*)0x000D7804)

#define rGPB_EN           (*(volatile unsigned int*)0x000CD808)
#define rGPB_INT_TYPE     (*(volatile unsigned int*)0x000CD80C)
#define rGPB_INT_DE       (*(volatile unsigned int*)0x000CD810)
#define rGPB_INT_EDGE     (*(volatile unsigned int*)0x000CD814)
#define rGPB_INT_MSK      (*(volatile unsigned int*)0x000CD818)
#define rGPB_INT_SCR      (*(volatile unsigned int*)0x000CD82C)

#define HIGH_LEVEL  1
#define LOW_LEVEL   0

//led controls
#define SET_LED_IO 
#define LED_TXD_ON 
#define LED_RXD_ON 
#define LED_ONLINE_ON 
#define LED_TXD_OFF 
#define LED_RXD_OFF 
#define LED_ONLINE_OFF 
  
#define ICON_TEL_NO 1
//mdm pin type define
#define SET_MDM_PWR_OUTPUT_EN   gpio_set_pin_type(ptModemDrvConfig->pwr_port,ptModemDrvConfig->pwr_pin,GPIO_OUTPUT) /*set PWR as output*/
#define SET_MDM_RESET_OUTPUT_EN gpio_set_pin_type(ptModemDrvConfig->reset_port,ptModemDrvConfig->reset_pin,GPIO_OUTPUT) /*set RESET as output*/
#define SET_MDM_RTS_OUTPUT_EN   gpio_set_pin_type(ptModemDrvConfig->rts_port,ptModemDrvConfig->rts_pin,GPIO_OUTPUT)  /*set RTS as output*/
#define	SET_MDM_DTR_OUTPUT_EN   gpio_set_pin_type(ptModemDrvConfig->dtr_port,ptModemDrvConfig->dtr_pin,GPIO_OUTPUT)  /*set DTR as output*/
#define SET_MDM_CTS_OUTPUT_EN   gpio_set_pin_type(ptModemDrvConfig->cts_port,ptModemDrvConfig->cts_pin,GPIO_OUTPUT)  /*set CTS as output*/
#define SET_MDM_DCD_OUTPUT_EN	gpio_set_pin_type(ptModemDrvConfig->dcd_port,ptModemDrvConfig->dcd_pin,GPIO_OUTPUT)  /*set DCD as output*/
#define SET_MDM_CTS_INPUT_EN    gpio_set_pin_type(ptModemDrvConfig->cts_port,ptModemDrvConfig->cts_pin,GPIO_INPUT)    
#define SET_MDM_DCD_INPUT_EN    gpio_set_pin_type(ptModemDrvConfig->dcd_port,ptModemDrvConfig->dcd_pin,GPIO_INPUT)

//mdm input pin define
#define GET_CTS (gpio_get_pin_val(ptModemDrvConfig->cts_port,ptModemDrvConfig->cts_pin))
#define GET_DCD (gpio_get_pin_val(ptModemDrvConfig->dcd_port,ptModemDrvConfig->dcd_pin))
#define GET_PWR (gpio_get_pin_val(ptModemDrvConfig->pwr_port,ptModemDrvConfig->pwr_pin))
//mdm output pin define
#define SET_RESET(x)    gpio_set_pin_val(ptModemDrvConfig->reset_port,ptModemDrvConfig->reset_pin,x)
#define SET_PWR(x)      gpio_set_pin_val(ptModemDrvConfig->pwr_port,ptModemDrvConfig->pwr_pin,x)
#define SET_CTS(x)      gpio_set_pin_val(ptModemDrvConfig->cts_port,ptModemDrvConfig->cts_pin,x)
#define SET_RTS(x)      gpio_set_pin_val(ptModemDrvConfig->rts_port,ptModemDrvConfig->rts_pin,x)
#define SET_DTR(x)      gpio_set_pin_val(ptModemDrvConfig->dtr_port,ptModemDrvConfig->dtr_pin,x)
#define SET_DCD(x)      gpio_set_pin_val(ptModemDrvConfig->dcd_port,ptModemDrvConfig->dcd_pin,x)

#define DISABLE_MDM_INT     rVIC1_INT_ENCLEAR=0x8000
#define ENABLE_MDM_INT      rVIC1_INT_ENABLE=0x8000
#define CLR_MDM_TIMER_INT_FLAG rTIMER7_INT_CLR=0x01
#define RELAY_ON  
#define RELAY_OFF 
#define CONFIG_RELAY_PORT 

enum TASK{TASK_IDLE=0,TASK_CALLER_DIAL,TASK_CALLER_DATA,TASK_CALLEE_WAIT,TASK_CALLEE_DATA};
enum LED_NO{N_LED_TXD=0,N_LED_RXD,N_LED_ONLINE};
enum ICON_TEL_STATUS{S_ICON_NULL=0,S_ICON_UP,S_ICON_DOWN};

typedef struct
{
  uchar mode;//D1D0--00:DTMF,01:pulse 1,0x10:pulse 2
  uchar ignore_dialtone;//D0--ignore tone detect,D1--callee,D2--ignore line
								//D4D3--sync NAC type,D3--auto fallback for async 2400
								//D5--callee interface,
								//D6--double timeout,D7--async fast connect
  uchar dial_wait;//unit:100ms
  uchar pbx_pause;//unit:100ms
  uchar code_hold;//unit:1ms
  uchar code_space;//unit:10ms
  uchar pattern;//D7-async/sdlc,D6D5-speed,D4D3-bell(10)/ccitt
  uchar dial_times;
  uchar idle_timeout;//unit:10s
  uchar async_format;//0-N81,1-E81,2-O81,3-N71,4-E71,5-O71
							//high 4 bits for async_speed:
							//1-1200,2-2400,3-4800,4-7200,5-9600,6-12000,7-14400
							//8-19.2k,9-24k,10-26.4k,11-28.8k,12-31.2k,13-33.6k
							//14-48k,15-56k
}MODEM_CONFIG;

uchar s_ModemInit_std(uchar mode);//0--first step,1--last step,2--both steps
void s_ModemInfo_std(uchar *drv_ver,uchar *mdm_name,uchar *last_make_date,uchar *others);

void led_auto_flash(uchar led_no,uchar count,uchar first_on,uchar last_on);

#endif

