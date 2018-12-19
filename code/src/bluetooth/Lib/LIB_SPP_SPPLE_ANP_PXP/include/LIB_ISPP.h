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
#ifndef __ISPPLIB_H__
#define __SPPLIB_H__


  /* The following enumerates the various states that the              */
   /* authentication state machine may be in during the identification  */
   /* and authentication process.  These values are supplied in the     */
   /* status field of a Process Status Event structure.                 */
typedef enum
{
   psStartIdentificationRequest,
   psStartIdentificationProcess,
   psIdentificationProcess,
   psIdentificationProcessComplete,
   psStartAuthenticationProcess,
   psAuthenticationProcess,
   psAuthenticationProcessComplete
} Process_State_t;

  /* The following defines the structure of the Open Session event.    */
   /* This event is dispatched when a Open Session request is received  */
   /* from the Apple Device.                                            */
   /* * NOTE * The user must Accept/Reject this connection by calling   */
   /*          the ISPP_Open_Session_Request_Response function.         */
   /* * NOTE * The MaxTxSessionPayloadSize defines that maximum number  */
   /*          of bytes that can be specified when sending session data */
   /*          to the connected Apple device.                           */
typedef struct _tagISPP_Session_Open_Indication_Data_t
{
   unsigned int SerialPortID;
   Word_t       SessionID;
   Byte_t       ProtocolIndex;
   Word_t       MaxTxSessionPayloadSize;
} ISPP_Session_Open_Indication_Data_t;

   /* The following defines the structure of the Close Session event.   */
   /* This event is dispatched when a Close Session request is received */
   /* from the Apple Device.                                            */
typedef struct _tagISPP_Session_Close_Indication_Data_t
{
   unsigned int SerialPortID;
   Word_t       SessionID;
} ISPP_Session_Close_Indication_Data_t;

   /* The following defines the structure of the Session Data event.    */
   /* This event is dispatched when Session Data is received from the   */
   /* Apple Device.                                                     */
   /* * NOTE * During normal operation the data dispatched in this      */
   /*          callback will be consumed.  If there are no available    */
   /*          resources to consume the packet, the PacketConsumed flag */
   /*          should be set to FALSE.  If a packet is not consumed, the*/
   /*          lower layer to buffer the session data and postpone the  */
   /*          sending of an ACK for the session data.  The pointer to  */
   /*          the data and the data length must be maintained by the   */
   /*          upper layer.  The pointer and length will be valid until */
   /*          the packet is later ACKed.  If the packet is not ACKed is*/
   /*          a reasonable amount of time the remote iOS device might  */
   /*          consider the channel unusable and drop the connection.   */
typedef struct _tagISPP_Session_Data_Indication_Data_t
{
   unsigned int  SerialPortID;
   Word_t        SessionID;
   Word_t        DataLength;
   Byte_t       *DataPtr;
   Boolean_t     PacketConsumed;
} ISPP_Session_Data_Indication_Data_t;

   /* The following structure is dispatched to inform the upper layer   */
   /* about the status of the authentication process.                   */
typedef struct _tagISPP_Process_Status_Data_t
{
   unsigned int    SerialPortID;
   Process_State_t ProcessState;
   Byte_t          Status;
} ISPP_Process_Status_Data_t;

int ISPPProfileOpen(unsigned int ServerPort);

int ISPPProfileClose();

 int ISPPConnect(BD_ADDR_t BD_ADDR, unsigned int ServerPort);

 int ISPPDisconnect();

 int ISPPDataRead(char *Data,unsigned long DataSize);

int ISPPDataWrite(char *Data,unsigned long DataLength);
 
#endif
