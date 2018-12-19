#ifndef __GAPLIB_H__
#define __GAPLIB_H__
   
#include "SS1BTPS.h"             /* Main SS1 Bluetooth Stack Header.          */
#include "BTPSKRNL.h"            /* BTPS Kernel Prototypes/Constants.         */


		 /* The following defines the values that are used for the Mode 	  */
		 /* Bitmask.														  */
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
		 /* from any API calls for this module. 							  */
#define BTH_ERROR_INVALID_PARAMETER                            (-1)
#define BTH_ERROR_REQUEST_FAILURE                              (-2)
#define BTH_ERROR_OPERATION_NOT_ALLOWED                        (-3)
#define BTH_ERROR_BUFFER_QUEUE_FULL                            (-4)
#define BTH_ERROR_RESOURCE_FAILURE                             (-5)
	  
#define NUM_SUPPORTED_LINK_KEYS                                       16



	  /* The following represent the defined I/O Capabilities that can be  */
	  /* specified/utilized by this module. 							   */
	  /* Display Only		  - The device only has a display with no	   */
	  /*						input capability.						   */
	  /* Display with Yes/No  - The device has both a display and the	   */
	  /*						ability for a user to enter yes/no, either */
	  /*						through a single keypress or via a Keypad  */
	  /* Keyboard Only		  - The device has no display capability, but  */
	  /*						does have a single key or keypad.		   */
	  /* No Input nor Output  - The device has no input and no output. A   */
	  /*						device such as this may use Out of Band or */
	  /*						Just Works associations.				   */
   typedef enum
   {
	  DisplayOnly,
	  DisplayYesNo,
	  KeyboardOnly,
	  NoInputNoOutput
   } CL_SSP_IO_Capability_t;



   /* The following type declaration defines an individual result of an */
   /* Inquiry Process that was started via the GAP_Perform_Inquiry()	*/
   /* function.  This event data is generated for each Inquiry Result as*/
   /* it is received.													*/
typedef struct _tagCL_Inquiry_Entry_Event_Data_t
{
	   BD_ADDR_t		 BD_ADDR;
	   Byte_t			 Page_Scan_Repetition_Mode;
	   Byte_t			 Page_Scan_Period_Mode;
	   Byte_t			 Page_Scan_Mode;
	   Class_of_Device_t Class_of_Device;
	   Word_t			 Clock_Offset;

   
} CL_Inquiry_Entry_Event_Data_t; 
   /* The following type declaration defines the result of an Inquiry	*/
   /* Process that was started via the GAP_Perform_Inquiry() function.	*/
   /* The Number of Devices Entry defines the number of Inquiry Data	*/
   /* Entries that the GAP_Inquiry_Data member points to (if non-zero). */
typedef struct _tagCL_Inquiry_Event_Data_t
{
   Word_t			   Number_Devices;
   CL_Inquiry_Entry_Event_Data_t *CL_Inquiry_Data;
} CL_Inquiry_Event_Data_t;




   /* The following structure represents the GAP Remote Name Response	*/
   /* Event Data that is returned from the								*/
   /* GAP_Query_Remote_Device_Name() function.	The Remote_Name 		*/
   /* member will point to a NULL terminated string that represents 	*/
   /* the User Friendly Bluetooth Name of the Remote Device associated	*/
   /* with the specified BD_ADDR.										*/
typedef struct _tagCL_Remote_Name_Event_Data_t
{
   Byte_t	  Remote_Name_Status;
   BD_ADDR_t  Remote_Device;
   char 	 *Remote_Name;
} CL_Remote_Name_Event_Data_t;

typedef struct _tagCL_Pin_Code_Request_Event_Data_t
{
   BD_ADDR_t  Remote_Device;
} CL_Pin_Code_Request_Event_Data_t;

typedef struct _tagCL_Remote_Device_Event_Data_t
{
   BD_ADDR_t  Remote_Device;
} CL_Remote_Device_Event_Data_t;

typedef struct _tagCL_Authentication_Event_Data_t
{
   GAP_Authentication_Event_Type_t GAP_Authentication_Event_Type;
   BD_ADDR_t                       Remote_Device;
   union
   {
      Byte_t                                   Authentication_Status;
      Byte_t                                   Secure_Simple_Pairing_Status;
      Boolean_t                                Remote_IO_Capabilities_Known;
      GAP_Authentication_Event_Link_Key_Info_t Link_Key_Info;
      DWord_t                                  Numeric_Value;
      GAP_Keypress_t                           Keypress_Type;
      GAP_IO_Capabilities_t                    IO_Capabilities;
   } Authentication_Event_Data;
} CL_Authentication_Event_Data_t;

typedef struct _tagCL_Inquiry_Name_Event_Data_t
{
   BD_ADDR_t  Remote_Device;
   char 	 *Remote_Name;
} CL_Inquiry_Name_Event_Data_t;


int GetClassOfDevice(Class_of_Device_t *Class_of_Device);
int SetClassOfDevice(Class_of_Device_t Class_of_Device);
int SetLocalDeviceName(char *DeviceName);
int ConnectionInquiry(unsigned int InquiryPeriodLength, unsigned int MaximumResponses);
int GetLocalDeviceName(char *Name);
int  GetRemoteDeviceType(BD_ADDR_t RemoteDevice);





int GetRemoteDeviceName(BD_ADDR_t BD_ADDR);
int ConnectionPINCodeResponse(BD_ADDR_t Remote_BD_ADDR,char *PIN_Code);
int ConnectionPair(BD_ADDR_t BD_ADDR);
int ConnectionPairingCancel(BD_ADDR_t BD_ADDR);
int SetDiscoverabilityMode(int Mode);
int SetConnectabilityMode(int Mode);
int SetPairabilityMode(int Pair_Mode);
int GetLocalAddress(BD_ADDR_t *BD_ADDR);
int CancelGetRemoteDeviceName(BD_ADDR_t BD_ADDR);

int ConnectionSetSSPParameters(CL_SSP_IO_Capability_t CL_SSP_IO_Capability, Boolean_t MITM_Protection);
int UserConfirmationResponse(BD_ADDR_t Remote_BD_ADDR,unsigned short Confirmation);
int PassKeyResponse(BD_ADDR_t Remote_BD_ADDR, DWord_t Passkey);

void Register_HAL_NV_DataRead_Callback(void * Callback);
void Register_HAL_NV_DataWrite_Callback(void * Callback);

int SetLinkSupervisionTimeout(int timeout);

#endif
