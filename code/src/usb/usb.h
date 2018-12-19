#ifndef _USB_H_
#define _USB_H_
#include "usb_sys.h"

// Standard Device Descriptor
typedef struct
{
	u8   bLength;
    u8   bDescriptorType;
    u16  bcdUSB;
    u8   bDeviceClass;
    u8   bDeviceSubClass;
    u8   bDeviceProtocol;
    u8   bMaxPacketSize0;
    u16  idVendor;
    u16  idProduct;
    u16  bcdDevice;
    u8   iManufacturer;
    u8   iProduct;
    u8   iSerialNumber;
    u8   bNumConfigurations;
} __attribute__((__packed__)) sDevDesc, *pDevDesc;

/* Standard Configuration Descriptor */
typedef struct
{	
	u8   bLength;                 // Size of descriptor in Byte
	u8   bType;					 // Configuration
	u16  wLength;                // Total length
	u8   bNumIntf;				 // Number of interface
	u8   bCV;             		 // bConfigurationValue
	u8   bIndex;          		 // iConfiguration
	u8   bAttr;                  // Configuration Characteristic
	u8   bMaxPower;				 // Power config
} __attribute__((__packed__)) sCfgDesc, *pCfgDesc;

// Standard Interface Descriptor
typedef struct
{	u8   bLength;
	u8   bType;
	u8   iNum;
	u8   iAltString;
	u8   bEndPoints;
	u8   iClass;
	u8   iSub; 
	u8   iProto;
	u8   iIndex; 
} __attribute__((__packed__)) sIntfDesc, *pIntfDesc;

// Standard EndPoint Descriptor
typedef struct
{	u8   bLength;
	u8   bType;
	u8   bEPAdd;
	u8   bAttr;
	u16  wPayLoad;               // low-speed this must be 0x08
	u8   bInterval;
} __attribute__((__packed__)) sEPDesc, *pEPDesc;

// Standard String Descriptor
typedef struct
{	u8   bLength;
	u8   bType;
	u16  wLang;
} __attribute__((__packed__)) sStrDesc, *pStrDesc;

/*-------------------------------------------------------------------------
 * Standard Chapter 9 definition
 *-------------------------------------------------------------------------
 */
#define GET_STATUS      0x00																  
#define CLEAR_FEATURE   0x01
#define SET_FEATURE     0x03
#define SET_ADDRESS     0x05
#define GET_DESCRIPTOR  0x06
#define SET_DESCRIPTOR  0x07
#define GET_CONFIG      0x08
#define SET_CONFIG      0x09
#define GET_INTERFACE   0x0a
#define SET_INTERFACE   0x0b
#define SYNCH_FRAME     0x0c


#define DEVICE          0x01
#define CONFIGURATION   0x02
#define STRING          0x03
#define INTERFACE       0x04
#define ENDPOINT        0x05

#define STDCLASS        0x00



#endif/* _USB_H_ */
