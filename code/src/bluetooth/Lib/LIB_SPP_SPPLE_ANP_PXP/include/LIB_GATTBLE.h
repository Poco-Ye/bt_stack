#ifndef __GATTBLE_H__
#define __GATTBLE_H__

#include "SS1BTPS.h"             /* Main SS1 Bluetooth Stack Header.          */
#include "BTPSKRNL.h"            /* BTPS Kernel Prototypes/Constants.         */


	  /* The following enumerated type represents the different connection */
	  /* types that are supported by GATT.								   */

   /* The following enumerated type definition defines the different    */
   /* types of service discovery that can be performed.                 */
typedef enum
{
   sdGAPS,
   sdSPPLE,
   sdANS,
   sdLLS,
   sdIAS,
   sdTPS
} Service_Discovery_Type_t;

typedef enum
{
	wgctLE,
	wgctBR_EDR
} GATT_BLE_Connection_Type_t;

   /* The following event is dispatched when a remote device is         */
   /* connected to the local GATT instance.                             */
   /* * NOTE * This event contains the Connection ID that is used to    */
   /*          uniquely identify the connection.                        */
typedef struct _tagGATT_BLE_Device_Connection_Data_t
{
   unsigned int           ConnectionID;
   GATT_BLE_Connection_Type_t ConnectionType;
   BD_ADDR_t              RemoteDevice;
   Word_t                 MTU;
} GATT_BLE_Device_Connection_Data_t;


   /* The following event is dispatched when a remote device disconnects*/
   /* from the local GATT instance.                                     */
typedef struct _tagGATT_BLE_Device_Disconnection_Data_t
{
   unsigned int           ConnectionID;
   GATT_BLE_Connection_Type_t ConnectionType;
   BD_ADDR_t              RemoteDevice;
} GATT_BLE_Device_Disconnection_Data_t;




//start BLE_Service_Discovery

			/* The following structure contains the Handles that will need to be */
			/* cached by a LLS client in order to only do service discovery once.*/
typedef struct _tagGAPS_Support_Service_Information_t
{
	Word_t Supported_GAPS;
		 
} GAPS_Support_Service_Information_t;
			

	  /* The following structure contains the Handles that will need to be */
	  /* cached by a LLS client in order to only do service discovery once.*/
typedef struct _tagSPPLE_Support_Service_Information_t
{
	  Word_t Supported_SPPLE;
   
} SPPLE_Support_Service_Information_t;
	  

   /* The following structure contains the Handles that will need to be */
   /* cached by a ANS client in order to only do service discovery once.*/
typedef struct _tagANS_Support_Service_Information_t
{
   Word_t Supported_New_Alert_Category;
   Word_t New_Alert;
   Word_t New_Alert_Client_Configuration;
   Word_t Supported_Unread_Alert_Category;
   Word_t Unread_Alert_Status;
   Word_t Unread_Alert_Status_Client_Configuration;
   Word_t Control_Point;
} ANS_Support_Service_Information_t;
   
	  /* The following structure contains the Handles that will need to be */
	  /* cached by a LLS client in order to only do service discovery once.*/
   typedef struct _tagLLS_Support_Service_Information_t
   {
	  Word_t Alert_Level;
   
   } LLS_Support_Service_Information_t;
	  
		 /* The following structure contains the Handles that will need to be */
		 /* cached by a IAS client in order to only do service discovery once.*/
	  typedef struct _tagIAS_Support_Service_Information_t
	  {
		 Word_t Control_Point;
	  } IAS_Support_Service_Information_t;
		 

   /* The following structure contains the Handles that will need to be */
   /* cached by a TPS client in order to only do service discovery once.*/
typedef struct _tagTPS_Support_Service_Information_t
{
   Word_t Tx_Power_Level;
} TPS_Support_Service_Information_t;

   

typedef struct _tagGATT_BLE_Service_Discovery_Complete_Data_t
{
   Byte_t       				Status;
   Service_Discovery_Type_t		DiscoveryType;
   void							*SupportServer;
} GATT_BLE_Service_Discovery_Complete_Data_t;


//end BLE_Service_Discovery  

#endif

