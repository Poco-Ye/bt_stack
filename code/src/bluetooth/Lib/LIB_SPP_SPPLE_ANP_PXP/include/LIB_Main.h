/*****< main.c >***************************************************************/
/*      Copyright 2012 Stonestreet One.                                       */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  MAIN - Stonestreet One main sample application header.                    */
/*                                                                            */
/*  Author:  Tim Cook                                                         */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   01/28/12  T. Cook        Initial creation.                               */
/******************************************************************************/
#ifndef __MAIN_H__
#define __MAIN_H__

#include "SS1BTPS.h"             /* Main SS1 Bluetooth Stack Header.          */
#include "BTPSKRNL.h"            /* BTPS Kernel Prototypes/Constants.         */


#if 0

		 /* The following represent the defined I/O Capabilities that can be  */
		 /* specified/utilized by this module.								  */
		 /* Display Only		 - The device only has a display with no	  */
		 /* 					   input capability.						  */
		 /* Display with Yes/No  - The device has both a display and the	  */
		 /* 					   ability for a user to enter yes/no, either */
		 /* 					   through a single keypress or via a Keypad  */
		 /* Keyboard Only		 - The device has no display capability, but  */
		 /* 					   does have a single key or keypad.		  */
		 /* No Input nor Output  - The device has no input and no output. A   */
		 /* 					   device such as this may use Out of Band or */
		 /* 					   Just Works associations. 				  */
	  typedef enum
	  {
		 DisplayOnly,
		 DisplayYesNo,
		 KeyboardOnly,
		 NoInputNoOutput
	  } CL_SSP_IO_Capability_t;
#endif
	  /* The following enumerated type is used with the Callback Event	   */
	  /* Event Data Structure and defines the reason that the Callback was */
	  /* issued, this defines what data in the structure is pertinent.	   */
   typedef enum
   {
   	  CALLBACK_NONE,
	  CL_INQUIRY_RESULT,
	  CL_INQUIRY_NONAME_RESULT,
	  CL_INQUIRY_NAME_RESULT,
	  CL_REMOTE_NAME_COMPLETE,
	  CL_PIN_CODE_RREQUEST,
	  CL_AUTHENTICATION_STATUS,
	  USER_CONFIRMATION_REQUEST,
	  PASSKEY_REQUEST,

	  SECURE_SIMPLE_PAIRING_COMPLETE,

	  
	  GAP_BLE_CONNECTION_COMPLETE,
	  GAP_BLE_DISCONNECTION_COMPLETE,
	  GAP_BLE_ADVERTISING_REPORT,
	  
	  GATT_BLE_CONNECTION_DEVICE_CONNECTION,
	  GATT_BLE_CONNECTION_DEVICE_DISCONNECTION,
	  GATT_BLE_SERVICE_DISCOVERY_COMPLETE,

	  SPP_REMOTE_PORT,
	  REMOTE_DEVICE_TYPE,
	  
	  SPP_CONNECT_INDICATION,
	  SPP_PORT_STATUS_INDICATION,
	  SPP_DATA_INDICATION,
	  SPP_DISCONNECT_INDICATION,
	  
      SPP_PORT_OPEN_INDICATION_WITHIN_ISPP,
      SPP_PORT_STATUS_INDICATION_WITHIN_ISPP,
      SPP_DATA_INDICATION_WITHIN_ISPP,

	  ISPP_OPEN_SESSION_INDICATION,
	  ISPP_DATA_INDICATION,
	  ISPP_CLOSE_SESSION_INDICATION,
	  ISPP_PORT_PROCESS_STATUS,
	  ISPP_CLOSE_PORT_INDICATION,
		
	  SPPLE_DATA_INDICATION,

	  ANP_NEW_ALERT_NOTIFICATION,
	  ANP_UN_READ_ALERT_NOTIFICATION,

	  PXP_GET_ALERT_LEVEL_COMPLETE,
	  PXP_GET_TX_POWER_LEVEL_COMPLETE,
	  PXP_ALERT_LEVEL_UPDATE_INDICATION,
	  
	  MAP_OPEN_REMOTE_SERVER_INDICATION,
	  MAP_NOTIFICATION_REGISTRATION_INDICATION,
	  MAP_SEND_EVENT_INDICATION,
	  MAP_GET_MESSAGE_INDICATION,
	  MAP_OPEN_CONNECT_INDICATION,
	  MAP_CLOSE_PORT_INDICATION,
	  HFRE_CHANGE_STATUS_INDICATION,
	  HFRE_CHANGE_STATUS_CONFIRMATION,
	  HFRE_CALL_WAITING_NOTIFICATION_INDICATION,
	  HFRE_CLOSE_PORT_INDICATION,
	  HFRE_AUDIO_CONNECTION_INDICATION,
	  HFRE_AUDIO_DISCONNECTION_INDICATION,
	  HID_OPEN_INDICATION,
	  HID_OPEN_CONFIRMATION,
	  HID_CLOSE_INDICATION,
	  PBAP_OPEN_CONFIRMATION,
	  PBAP_CLOSE_PORT_INDICATION,
	  PBAP_PULL_PHONE_BOOK_INDICATION,
	  AVRCP_CONNECT_INDICATION,
	  AVRCP_CONNECT_CONFIRMATION,
	  AVRCP_MESSAGE_INDICATION,
	  AVRCP_DISCONNECT_INDICATION,
	  AVRCP_ELEMENT_ATTRIBUTE_INDICATION,
	  AUD_OPEN_REQUEST_INDICATION,
	  AUD_STREAM_OPEN_INDICATION,
	  AUD_STREAM_OPEN_CONFIRMATION,
	  AUD_CLOSE_INDICATION,
	  AUD_REMOTE_CONTROL_OPEN_INDICATION,
	  AUD_REMOTE_CONTROL_OPEN_CONFIRMATION,
	  AUD_REMOTE_CONTROL_CLOSE_INDICATION,
	  AUD_STATE_CONTROL_INDICATION,
	  

   } CallbackEvent_t;
   
	  /* The following structure represents the container structure that   */
	  /* holds all Callback Event Data. 								   */
   typedef struct _tagCallback_Event_Data_t
   {
   	  int CallbackTimer;
	  CallbackEvent_t CallbackEvent;
	  void *CallbackParameter;
   } Callback_Event_Data_t;

#if 0
  	  /* The following type declaration defines an individual result of an */
	  /* Inquiry Process that was started via the GAP_Perform_Inquiry()    */
	  /* function.	This event data is generated for each Inquiry Result as*/
	  /* it is received.												   */
   typedef struct _tagCL_Inquiry_Entry_Event_Data_t
   {
		  BD_ADDR_t 		BD_ADDR;
		  Byte_t			Page_Scan_Repetition_Mode;
		  Byte_t			Page_Scan_Period_Mode;
		  Byte_t			Page_Scan_Mode;
		  Class_of_Device_t Class_of_Device;
		  Word_t			Clock_Offset;

	  
   } CL_Inquiry_Entry_Event_Data_t; 
	  /* The following type declaration defines the result of an Inquiry   */
	  /* Process that was started via the GAP_Perform_Inquiry() function.  */
	  /* The Number of Devices Entry defines the number of Inquiry Data    */
	  /* Entries that the GAP_Inquiry_Data member points to (if non-zero). */
   typedef struct _tagCL_Inquiry_Event_Data_t
   {
	  Word_t			  Number_Devices;
	  CL_Inquiry_Entry_Event_Data_t *CL_Inquiry_Data;
   } CL_Inquiry_Event_Data_t;
   
   

   
	  /* The following structure represents the GAP Remote Name Response   */
	  /* Event Data that is returned from the							   */
	  /* GAP_Query_Remote_Device_Name() function.  The Remote_Name		   */
	  /* member will point to a NULL terminated string that represents	   */
	  /* the User Friendly Bluetooth Name of the Remote Device associated  */
	  /* with the specified BD_ADDR.									   */
   typedef struct _tagCL_Remote_Name_Event_Data_t
   {
	  Byte_t	 Remote_Name_Status;
	  BD_ADDR_t  Remote_Device;
	  char		*Remote_Name;
   } CL_Remote_Name_Event_Data_t;

   typedef struct _tagCL_Pin_Code_Request_Event_Data_t
   {
	  BD_ADDR_t  Remote_Device;
   } CL_Pin_Code_Request_Event_Data_t;
#endif
   typedef struct _tagSPP_Connect_Indication_Event_Data_t
   {
   	  unsigned int SerialPortID;
	  BD_ADDR_t  Remote_Device;
   } SPP_Connect_Indication_Event_Data_t;

typedef struct _tagSPP_Data_Indication_Event_Data_t
{
   unsigned int  SerialPortID;
   Word_t        DataLength;
} SPP_Data_Indication_Event_Data_t;

   typedef struct _tagSPP_Disconnect_Indication_Event_Data_t
   {
	  unsigned int SerialPortID;
   } SPP_Disconnect_Indication_Event_Data_t;



   //joe add

        typedef struct _tagParameter_t
{
   char     *strParam;
   SDWord_t  intParam;
} Parameter_t;
   typedef struct _tagParameterList_t
{
   int         NumberofParameters;
   Parameter_t Params[5];
} ParameterList_t;

/* The following structure Bluetooth Version	 */
typedef struct _tagBluetooth_Stack_Version_t
{
   Byte_t StackVersion[3];
   Byte_t CustomerVersion[3];	
} Bluetooth_Stack_Version_t;


   /* Error Return Codes.                                               */

   /* Error Codes that are smaller than these (less than -1000) are     */
   /* related to the Bluetooth Protocol Stack itself (see BTERRORS.H).  */
#define APPLICATION_ERROR_INVALID_PARAMETERS             (-1000)
#define APPLICATION_ERROR_UNABLE_TO_OPEN_STACK           (-1001)

   /* The following function is used to initialize the application      */
   /* instance.  This function should open the stack and prepare to     */
   /* execute commands based on user input.  The first parameter passed */
   /* to this function is the HCI Driver Information that will be used  */
   /* when opening the stack and the second parameter is used to pass   */
   /* parameters to BTPS_Init.  This function returns the               */
   /* BluetoothStackID returned from BSC_Initialize on success or a     */
   /* negative error code (of the form APPLICATION_ERROR_XXX).          */
int InitializeApplication(HCI_DriverInformation_t *HCI_DriverInformation, BTPS_Initialization_t *BTPS_Initialization);

   /* The following function is used to process a command line string.  */
   /* This function takes as it's only parameter the command line string*/
   /* to be parsed and returns TRUE if a command was parsed and executed*/
   /* or FALSE otherwise.                                               */
void MainTask(void *UserParameter);

void DeInitializeApplication(void);

#endif

