#ifndef __GAPBLE_H__
#define __GAPBLE_H__

#include "SS1BTPS.h"             /* Main SS1 Bluetooth Stack Header.          */
#include "BTPSKRNL.h"            /* BTPS Kernel Prototypes/Constants.         */


   /* The following enumerated type represents the different LE Address */
   /* Types.                                                            */
typedef enum
{
   wlatPublic,
   wlatRandom
} GAP_BLE_Address_Type_t;

   /* The following structure is a container structure for all the      */
   /* connection parameters that are present for all LE connections.    */
   /* * NOTE * The Connection_Interval and Supervision_Timeout members  */
   /*          are specified in milliseconds.                           */
   /* * NOTE * The Slave_Latency member is specified in number of       */
   /*          connection intervals.                                    */
typedef struct _tagGAP_BLE_Current_Connection_Parameters_t
{
   Word_t Connection_Interval;
   Word_t Slave_Latency;
   Word_t Supervision_Timeout;
} GAP_BLE_Current_Connection_Parameters_t;


typedef struct _tagGAP_BLE_Connection_Complete_Event_Data_t
{
   Byte_t                                 Status;
   Boolean_t                              Master;
   GAP_BLE_Address_Type_t                  Peer_Address_Type;
   BD_ADDR_t                              Peer_Address;
   GAP_BLE_Current_Connection_Parameters_t Current_Connection_Parameters;
} GAP_BLE_Connection_Complete_Event_Data_t;


   /* The following type declaration defines the structure of the data  */
   /* returned in a GAP etLE_Disconnection_Complete event.              */
typedef struct _tagGAP_BLE_Disconnection_Complete_Event_Data_t
{
   Byte_t                Status;
   Byte_t                Reason;
   GAP_LE_Address_Type_t Peer_Address_Type;
   BD_ADDR_t             Peer_Address;
} GAP_BLE_Disconnection_Complete_Event_Data_t;

   



   /* The following enumerated type represents the possible Advertising */
   /* Report types that can be returned via the GAP                     */
   /* etLE_Advertising_Report for each device in the mentioned events.  */
typedef enum
{
   wrtConnectableUndirected,
   wrtConnectableDirected,
   wrtScannableUndirected,
   wrtNonConnectableUndirected,
   wrtScanResponse
} GAP_BLE_Advertising_Report_Type_t;

    /* The following structure defines an individual Advertising Report  */
   /* Structure Entry that is present in an LE Advertising Report.      */
   /* This structure is used with the GAP_LE_Advertising_Report_Data_t  */
   /* container structure so that individual entries can be accessed in */
   /* a convenient, array-like, form.  The first member specifies the   */
   /* Advertising Data type (These types are of the form                */
   /* HCI_LE_ADVERTISING_REPORT_DATA_TYPE_xxx where 'xxx' is the        */
   /* individual Data type).  The second member specifies the length of */
   /* data that is pointed to by the third member.  The third member    */
   /* points to the actual data for the individual entry (length is     */
   /* specified by the second member).                                  */
   /* * NOTE * The AD_Type member is defined in the specification to    */
   /*          be variable length.  The current specification does not  */
   /*          utilize this member in this way (they are all defined to */
   /*          be a single octet, currently).                           */
typedef struct _tagGAP_BLE_Advertising_Data_Entry_t
{
   DWord_t  AD_Type;
   Byte_t   AD_Data_Length;
   Byte_t  *AD_Data_Buffer;
} GAP_BLE_Advertising_Data_Entry_t;


   /* The following structure is a container structure that is used to  */
   /* represent all the entries in an Advertising Data Structure.       */
   /* This structure is used so that all fields are easy to             */
   /* parse and access (i.e. there are no MACRO's required to access    */
   /* variable length records).  The first member of this structure     */
   /* specifies how many individual entries are contained in the        */
   /* Extended Inquiry Response Data structure.  The second member is a */
   /* pointer to an array that contains each individual entry of the    */
   /* Extended Inquiry Response Data structure (note that the number of */
   /* individual entries pointed to by this array will be specified by  */
   /* the Number_Entries member (first member).                         */
typedef struct _tagGAP_BLE_Advertising_Data_t
{
   unsigned int                     Number_Data_Entries;
   GAP_BLE_Advertising_Data_Entry_t *Data_Entries;
} GAP_BLE_Advertising_Data_t;
   
   /* The following structure is a container structure that is used to  */
   /* represent all the entries in an LE Advertising Report Data Field  */
   /* This structure is used so that all fields are easy to parse and   */
   /* access (i.e. there are no MACRO's required to access variable     */
   /* length records).  The Advertising_Data member contains the the    */
   /* actual parsed data for the report (either a Scan Response or      */
   /* Advertising report - specified by the                             */
   /* GAP_LE_Advertising_Report_Type member).                           */
   /* * NOTE * The Raw_Report_Length and Raw_Report_Data members contain*/
   /*          the length (in bytes) of the actual report received, as  */
   /*          well as a pointer to the actual bytes present in the     */
   /*          report, respectively.                                    */
typedef struct _tagGAP_BLE_Advertising_Report_Data_t
{
   GAP_BLE_Advertising_Report_Type_t  Advertising_Report_Type;
   GAP_BLE_Address_Type_t             Address_Type;
   BD_ADDR_t                         BD_ADDR;
   SByte_t                           RSSI;
   GAP_BLE_Advertising_Data_t         Advertising_Data;
   Byte_t                            Raw_Report_Length;
   Byte_t                           *Raw_Report_Data;
} GAP_BLE_Advertising_Report_Data_t;

   /* The following type declaration defines the structure of the data  */
   /* returned in a GAP etLE_Advertising_Report event. The structure    */
   /* is used to return Advertising Reports returned from multiple      */
   /* devices.                                                          */
typedef struct _tagGAP_BLE_Advertising_Report_Event_Data_t
{
   unsigned int                       Number_Device_Entries;
   GAP_BLE_Advertising_Report_Data_t  *Advertising_Data;
} GAP_BLE_Advertising_Report_Event_Data_t;
   

int BLEConnect(char *ConnectedADDR,GAP_BLE_Address_Type_t Address_Type);
int BLEDisconnect(char *ConnectedADDR);
int BLEStartScanning(void);
 int BLEStopScanning(void);
int BLEDiscoverGAPS(char *ConnectedADDR);
int SPPLEDiscover(char *ConnectedADDR);
 int BLEAdvertiseStart(void);
 int BLEAdvertiseStop(void);

 int SetBLELocalDeviceName(char *name);
 
#endif
