#ifndef __ANPBLE_H__
#define __ANPBLE_H__

#include "SS1BTPS.h"             /* Main SS1 Bluetooth Stack Header.          */
#include "BTPSKRNL.h"            /* BTPS Kernel Prototypes/Constants.         */


   /* The following enumerated type represents all of the Category      */
   /* Identifications that may be received in an                        */
   /* etANS_Server_Control_Point_Command_Indication Server event OR that*/
   /* may be written to a remote ANS Server.                            */
typedef enum
{
  wciSimpleAlert       = 0x00,
  wciEmail             = 0x01,
  wciNews              = 0x02,
  wciCall              = 0x03,
  wciMissedCall        = 0x04,
  wciSMS_MMS           = 0x05,
  wciVoiceMail         = 0x06,
  wciSchedule          = 0x07,
  wciHighPriorityAlert = 0x08,
  wciInstantMessage    = 0x09,
  wciAllCategories     = 0xFF
} ANS_BLE_Category_Identification_t;


   /* The following represents to the structure of a New Alert.  This is*/
   /* used to notify a remote ANS Client of a New Alert in a specified  */
   /* category.                                                         */
   /* * NOTE * The LastAlertString member is optional and may be set to */
   /*          NULL.                                                    */
   /* * NOTE * CategoryID MAY NOT be set to ciAllCategories in this     */
   /*          structure.                                               */
typedef struct _tagANS_BLE_New_Alert_Data_t
{
   ANS_BLE_Category_Identification_t  CategoryID;
   Byte_t                         NumberOfNewAlerts;
   char                          *LastAlertString;
} ANS_BLE_New_Alert_Data_t;

   
	  /* The following represents to the structure of an Unread Alert.	   */
	  /* This is used to notify a remote ANS Client of a Unread Alert in a */
	  /* specified category.											   */
	  /* * NOTE * CategoryID MAY NOT be set to ciAllCategories in this	   */
	  /*		  structure.											   */
   typedef struct _tagANS_BLE_Un_Read_Alert_Data_t
   {
	  ANS_BLE_Category_Identification_t CategoryID;
	  Byte_t						NumberOfUnreadAlerts;
   } ANS_BLE_Un_Read_Alert_Data_t;

int ANPDiscover(void);
int ANPConfigure(char NewAlertEN,char UnreadAlertEN);
int ANPEnableNewAlertNotifications(ANS_BLE_Category_Identification_t Category);
int ANPEnableUnreadAlertNotifications(ANS_BLE_Category_Identification_t Category);
int ANPRegisterProfile(void);
int ANPUnregisterProfile(void);
int ANPNotifyNewAlerts(ANS_BLE_Category_Identification_t CategoryID);
int ANPNotifyUnreadAlerts(ANS_BLE_Category_Identification_t CategoryID);








#endif

