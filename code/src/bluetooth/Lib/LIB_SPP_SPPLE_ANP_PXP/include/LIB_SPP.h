/*****< sppdemo.c >************************************************************/
/*      Copyright 2011 - 2012 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  SPPDEMO - Simple embedded application using SPP Profile.                  */
/*                                                                            */
/*  Author:  Tim Cook                                                         */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   01/24/11  T. Cook        Initial creation.                               */
/******************************************************************************/
#ifndef __SPPDEMO_H__
#define __SPPDEMO_H__

#if 0
   /* The following defines the values that are used for the Mode       */
   /* Bitmask.                                                          */
#define CONNECTABLE_MODE_MASK                                0x0001
#define NON_CONNECTABLE_MODE                                 0x0000
#define CONNECTABLE_MODE                                     0x0001

#define DISCOVERABLE_MODE_MASK                               0x0002
#define NON_DISCOVERABLE_MODE                                0x0000
#define DISCOVERABLE_MODE                                    0x0002

#define PAIRABLE_MODE_MASK                                   0x000C
#define NON_PAIRABLE_MODE                                    0x0000
#define PAIRABLE_NON_SSP_MODE                                0x0004
#define PAIRABLE_SSP_MODE                                    0x0008

   /* The following defines the list of error code that can be returned */
   /* from any API calls for this module.                               */
#define BTH_ERROR_INVALID_PARAMETER                            (-1)
#define BTH_ERROR_REQUEST_FAILURE                              (-2)
#define BTH_ERROR_OPERATION_NOT_ALLOWED                        (-3)
#define BTH_ERROR_BUFFER_QUEUE_FULL                            (-4)
#define BTH_ERROR_RESOURCE_FAILURE                             (-5)

#define NUM_SUPPORTED_LINK_KEYS                                       8

int GetClassOfDevice(Class_of_Device_t *Class_of_Device);
int SetClassOfDevice(Class_of_Device_t Class_of_Device);
int SetLocalDeviceName(char *DeviceName);
int ConnectionInquiry(unsigned int InquiryPeriodLength, unsigned int MaximumResponses);
int GetLocalDeviceName(char *Name);




int GetRemoteDeviceName(BD_ADDR_t BD_ADDR);
int ConnectionPINCodeResponse(BD_ADDR_t Remote_BD_ADDR,char *PIN_Code);
int ConnectionPair(BD_ADDR_t BD_ADDR);
int ConnectionPairingCancel(BD_ADDR_t BD_ADDR);
#endif

int SPPProfileOpen(unsigned int ServerPort);

//int SPPProfileClose(void);
int SPPProfileCloseByNumbers(int LocalSerialPortID);

//int SPPDataRead(char *Data,unsigned long DataLength);
int SPPDataRead(char *Data,unsigned long DataSize,int LocalSerialPortID);
//int SPPDataWrite(char *Data,unsigned long DataLength);
int SPPDataWrite(char *Data,unsigned long DataLength,int  LocalSerialPortID);
int SPPConnect(BD_ADDR_t BD_ADDR, unsigned int ServerPort);
int SPPDisconnect(void);
int SPPDisconnectByNumbers(int LocalSerialPortID);

/* The following function is responsible for issuing a Service Search*/ 
/* Attribute Request to a Remote SDP Server. This function returns */ 
/* zero if successful and a negative value if an error occurred. */ 
int ServiceDiscovery(BD_ADDR_t BD_ADDR,int ProfileIndex);


/* Just Open ISPP, No-IOS Connect SPP, data read function */
int SPPDataReadWithInISPP(char *Data,unsigned long DataSize,int LocalSerialPortID);

/* Just Open ISPP, No-IOS Connect SPP, data write function */
int SPPDataWriteWithInISPP(char *Data,unsigned long DataLength,int  LocalSerialPortID);



#endif

