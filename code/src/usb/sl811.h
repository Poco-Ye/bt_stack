#ifndef _SL811_HCD_H_
#define _SL811_HCD_H_

#include "usb.h"

#define usb_debug //s80_printf

//Interrupt Status Mask
#define INSERT_REMOVE	0x20
#define DEVICE_DETECT   0x40
#define USB_RESET		0x40

#define sDATA0_WR   0x27   /* Data0 + SOF + OUT + Enable + Arm*/
#define sDATA0_RD   0x23   /* Data0 + SOF + IN +  Enable + Arm*/

#define PID_SETUP   0xD0
#define PID_IN      0x90
#define PID_OUT     0x10
#define MAX_EP  0x3       /* 0 is control, 1,2 is in/out */
#define SL811AB(reg, select) ((reg)+((select)==1?8:0))

/* URB ERROR CODE */
#define URB_TIMEOUT  1
#define URB_STALL    2
#define URB_OVERFLOW 3
#define URB_NAK      4

typedef struct sl811_ep
{
	u8   bAddr;// bit 7 = 1 = use PID_IN,
	u8   bAttr;// ISO | Bulk | Interrupt | Control
	u16  wPayLoad;// ISO range: 1-1023, Bulk: 1-64, etc
	u16  bInterval; // polling interval (for LS)
}SL811_EP;

struct sl811_dev;
struct urb_s;

/* SL811 USB DEV State */
enum{
	UDEV_NOPRESENT = 0x0,
	UDEV_HW_INIT,
	UDEV_HW_RESET,
	UDEV_GetDeiveDesc,
	UDEV_SetAddr,
	UDEV_GetConfigDesc,
	UDEV_GetAllDesc,
	UDEV_SetConfig,
	UDEV_SetIntf,/* set interface */
	UDEV_FINISHED,/* 9 ok ! */
};

typedef struct sl811_dev
{
	volatile u32  state;
	int   nak_delay;/* nak delay */
	u32   fail_state;/* for debug */
	u32   delay;/* in ms */
	u8   uAddr;
	u16  configs_len;/* total len of All configuration */
	u16  wVID, wPID;       // Vendor ID and Product ID
	u8   bClass, bSubClass;
	u8   bNumOfEPs;        // actual number endpoint from slave
	u8   iMfg;				// Manufacturer ID
	u8   iPdt;				// Product ID
	u8   epbulkin;//ep in addr
	u8   epbulkout;//ep out addr
	u8   bIntfSet;/* interface setting*/
	u8   bIntfIndex;/* interface index */
	u8   retry_counter[2];
	u8   enum_error;/* enum error counter */
	u32  toggle;
	u8   control_value;
	SL811_EP ep[MAX_EP];
	void (*finish_entry)(struct sl811_dev *dev, struct urb_s *urb);
}SL811_DEV;

static inline void usb_set_toggle(SL811_DEV *dev, int ep, int bit)
{
	dev->toggle = ((dev->toggle) & ~(1 << ep)) | (bit << ep);
}

static inline int usb_get_toggle(SL811_DEV *dev, int ep)
{
	return ((dev->toggle) >> ep) & 1;
}

static inline void usb_do_toggle(SL811_DEV *dev, int ep)
{
	dev->toggle ^= (1 << ep);
}

/* USB SETUP BLOCK*/
typedef struct
{
	u8   bmRequest;
	u8   bRequest;
	u16  wValue;
	u16  wIndex;
	u16  wLength;
} __attribute__((__packed__)) USBLOCK;

typedef enum
{
	URB_DONE=0,
	URB_DOING,
	URB_ERROR
}URB_STATE;

/* USB REQUEST BLOCK */
typedef struct urb_s
{
	volatile u8   state;
	u8   err_code;
	u8   cmd;
	u8   xferLen;
	
	u8   usbaddr;
	u8   endpoint;
	u8   prev_pid;
	u8   pid;
	u8   next_pid;
	u32  request_len;
	u32  buffer_len;
	u32  actual_len;
	u8   *buffer;
	USBLOCK setup;
}URB;

#endif/* _SL811_HCD_H_ */
