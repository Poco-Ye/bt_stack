#ifndef _WNET_PPP_H_
#define _WNET_PPP_H_

#include "ppp.h"

/* Wirless Type Define */
#define WTYPE_GPRS 1
#define WTYPE_CDMA 2
#define WTYPE_TDSCDMA 3
#define WTYPE_WCDMA	  4

/* manufacturer ID */
#define MFR_FIBCOM_G6x0	 0 /*GPRS*/
#define MFR_HUAWEI_MG323 1 /*GPRS*/
#define MFR_HUAWEI_WCDMA 2 /*WCDMA*/
#define MFR_HUAWEI_MC509 3 /*CDMA*/
#define MFR_FIBCOM_H330  4 /*WCDMA*/
#define MFR_HUAWEI_MU709 5 /*WCDMA*/
#define MFR_MAX_ID        6

extern u8 PortOpen(u8 channel, u8 *Attr);

extern int ppp_get_comm(int wnet_port);
extern void ppp_com_irq_disable(int comm);
extern void ppp_com_irq_enable(int comm);
extern void ppp_com_irq_tx(int comm);
extern int ppp_com_check(int comm);

extern int wnet_ppp_used( void);
extern int wnet_get_signal(unsigned char *signal_level);
extern int wnet_type_set(int wtype, int mfr);

#define PRE_TOATMODE_WAIT 1500

#define TOATMODE_WAIT 1200

#define SIGNAL_UPDATE_TIME (300*1000)

typedef enum
{
	CMD_STATE_INIT = 0,
	CMD_STATE_WAIT ,
	CMD_STATE_RST,
}CMD_STATE;

#define CMD_TOATMODE   (1<<1)/* cmd for DataMode to AtCmdMode */
#define CMD_SIGNAL     (1<<2)/* cmd for GetSignal */
#define CMD_REG        (1<<3)/* cmd for Read GSM Register Status */
#define CMD_ECHO       (1<<4)/* echo Response */
#define CMD_ATO        (1<<5)/* This cmd is ATO */
#define CMD_UNDO       (1<<6)/* Undo this cmd */
#define CMD_CGATT      (1<<7)/* CGATT? */
#define CMD_RST        (1<<8)/* Reset Module when cmd fail! */
#define CMD_DO_CGATT   (1<<9)/* do CGATT?*/
#define CMD_CHK_CREG   (1<<10)
#define CMD_CHK_MODE  (1<<11)
#define CMD_CELL_INFO	 (1<<12)
typedef struct wnet_cmd_rsp_s
{
	char *cmd;
	char *rsp;
	int  timeout;
	int  retry;/* retry count*/
	int  flag;/* Cmd Flag */
}WNET_CMD_RSP_T;

typedef struct wnet_cmd_rsp_arr_s
{
	WNET_CMD_RSP_T *cmd_rsp;
	int             num;
	int             idx;
	volatile CMD_STATE  state;
	u32             timestamp;
	int             retry;
	int             count;
	int             flag;/* Response Flag */
}WNET_CMD_RSP_ARR_T;

typedef enum
{
	WNET_STATE_DOWN =0,
	WNET_STATE_PRE_DIALING,
	WNET_STATE_DIALING, 
	WNET_STATE_UP, 
	WNET_STATE_PPP, 
	WNET_STATE_PRE_ONHOOK, 
	WNET_STATE_ONHOOK,
	WNET_STATE_MAX,
}WNET_STATE;

typedef enum
{
	SIGNAL_STATE_IDLE = 0,
	SIGNAL_STATE_WAIT,
	SIGNAL_STATE_MAX,
}SIGNAL_STATE;

typedef struct wnet_signal
{
	volatile SIGNAL_STATE state;
	int err;
	u8  level;
	u8  ber;
	u32 nexttime;/* update time*/
	u32 starttime;/*start time*/
	WNET_CMD_RSP_ARR_T cmd_rsp_arr;
}WNET_SIGNAL_T;

typedef struct wnet_dev_t
{
	volatile int            type;/* WTYPE_GPRS or WTYPE_CDMA */
	volatile int            mfr;/* manufacturer ID */
	volatile WNET_STATE    state;/* WNet  state*/
	volatile int         err_code;
	volatile USER_EVENT  user_event;
	volatile long        auth;
	volatile int         port;/*sieral port*/
	PPP_DEV_T   ppp_dev;/*PPP Dev*/
	INET_DEV_T  inet_dev;/* IP Dev */
	WNET_CMD_RSP_ARR_T dialing_cmd_rsp;
	WNET_CMD_RSP_ARR_T  onhook_cmd_rsp;
	u8          rsp_buf[2048];
	int         rall;
	char        cgatt_do_start, cgatt_do_end;
	WNET_SIGNAL_T    signal;
	volatile u8		AcT;
	u8		mode_buf[2048];
	volatile u32 		timestamp;
	volatile u32 		logout_jiffier;
	volatile u32 		login_jiffier;

	volatile u32         cmd_jiffier;
	volatile int reset_cnt;
	volatile int         no_carrier;
	volatile u32         no_carrier_time;
}WNET_DEV_T;

#endif

