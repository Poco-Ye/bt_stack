/*****< hcitrans.c >***********************************************************/
/*      Copyright 2012 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  HCITRANS - HCI Transport Layer for use with Bluetopia.                    */
/*                                                                            */
/*  Author:  Marcus Funk                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   11/08/12  M. Funk        Initial creation.                               */
/******************************************************************************/

#include "../btpskrnl/BTPSKRNL.h"       /* Bluetooth Kernel Prototypes/Constants.         */
#include "HCITRANS.h"       /* HCI Transport Prototypes/Constants.            */
//#include <posapi.h>
//#include <posapi_all.h>
//#include "HCITRCFG.h"       /* HCI Transport configuration.                   */

/*****< hcitrans.c >***********************************************************/
/*      Copyright 2012 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  HCITRANS - HCI Transport Layer for use with Bluetopia.                    */
/*                                                                            */
/*  Author:  Marcus Funk                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   11/08/12  M. Funk        Initial creation.                               */
/******************************************************************************/

#include "BTPSKRNL.h"       /* Bluetooth Kernel Prototypes/Constants.         */
#include "HCITRANS.h"       /* HCI Transport Prototypes/Constants.            */
//#include <posapi.h>
//#include <posapi_all.h>

   /* The following defines the UART port that we will attempt to use to*/
   /* communicate with the Bluetooth chip.                              */
//#define HCITR_UART_TTY_PORT       "ittyb:"
#define HCITR_UART_TTY_PORT       "ittyc:"

#define UART_PCR_MUX              3



#define RECEIVE_BUFFER_SIZE       1024*2
#define TRANSPORT_ID              1
#define RECEIVE_THREAD_STACK_SIZE 384

#define Display(_x)                 //do { BTPS_OutputMessage _x; } while(0)

   /* Internal Variables to this Module (Remember that all variables    */
   /* declared static are initialized to 0 automatically by the         */
   /* compiler as part of standard C/C++).                              */

static unsigned int            HCITransportOpen;

   /* COM Data Callback Function and Callback Parameter information.    */
static HCITR_COMDataCallback_t _COMDataCallback;
static unsigned long           _COMCallbackParameter;

static Byte_t                  ReceiveBuffer[RECEIVE_BUFFER_SIZE];

   /* Internal Function Prototypes.                                     */
   /* The following function is responsible for opening the HCI         */
   /* Transport layer that will be used by Bluetopia to send and receive*/
   /* COM (Serial) data.  This function must be successfully issued in  */
   /* order for Bluetopia to function.  This function accepts as its    */
   /* parameter the HCI COM Transport COM Information that is to be used*/
   /* to open the port.  The final two parameters specify the HCI       */
   /* Transport Data Callback and Callback Parameter (respectively) that*/
   /* is to be called when data is received from the UART.  A successful*/
   /* call to this function will return a non-zero, positive value which*/
   /* specifies the HCITransportID that is used with the remaining      */
   /* transport functions in this module.  This function returns a      */
   /* negative return value to signify an error.                        */
int BTPSAPI HCITR_COMOpen(HCI_COMMDriverInformation_t *COMMDriverInformation, HCITR_COMDataCallback_t COMDataCallback, unsigned long CallbackParameter)
{
   int          ret_val;
   unsigned int param;
	unsigned int TaskReturn;
	unsigned char ucRet;

   /* First, make sure that the port is not already open and make sure  */
   /* that valid COMM Driver Information was specified.                 */
   if((!HCITransportOpen) && (COMMDriverInformation) && (COMDataCallback))
   {
      /* Note the COM Callback information.                             */
      _COMDataCallback      = COMDataCallback;
      _COMCallbackParameter = CallbackParameter;



      /* Open the port for Reading/Writing.                             */
      //if((FilePointer = (FILE *)fopen(HCITR_UART_TTY_PORT, (pointer)(IO_SERIAL_HW_FLOW_CONTROL | IO_SERIAL_RAW_IO))) != NULL)
      {
         /* Initialize the UART Port.                                   */
         param = COMMDriverInformation->BaudRate;
		ucRet = PortOpen(7,"115200,8,n,1");
		Display(("PortOpen(7) = %d\r\n", ucRet));
		s_BtRstSetPinType(1);
		s_BtRstSetPinVal(0);
		BTPS_Delay(10);
		s_BtRstSetPinVal(1);
		BTPS_Delay(100);

#if 0
         /* Setup the reset and clock lines.                            */
         /* Set the clock output.                                       */
			 GpioSetRegOneBit(GPIO_B_OE,GPIOB7);
		 ////no pull  no up
			 //SetGpioNoPull(GPIO_BANK_B,GPIOB7);
			 GpioSetRegOneBit(GPIO_B_PU,GPIOB7); //2'b10
			 GpioClrRegOneBit(GPIO_B_PD,GPIOB7);
		//set low
		GpioClrRegOneBit(GPIO_B_OUT,GPIOB7);
         /* Clear the reset.                                            */
         BTPS_Delay(10);
		 //set high
         GpioSetRegOneBit(GPIO_B_OUT,GPIOB7);
         BTPS_Delay(100);
#endif
         /* Now that we have opened the UART, start a thread to read the*/
         /* data and dispatch Callbacks.                                */
				 //TaskReturn = OSTaskCreate(COMReadThread,"COMRead",RECEIVE_THREAD_STACK_SIZE,NULL,3,NULL);
		 TaskReturn = 0;
         if(!TaskReturn)
         {
            /* Flag that the HCI Transport is open.                     */
            HCITransportOpen = 1;

            /* Flag that Transport was initialized successfully.        */
            ret_val          = TRANSPORT_ID;
         }
         else
         {
            //DBG_MSG(DBG_ZONE_DEVELOPMENT,("Unable to start COM Read Thread\r\n"));

            /* Error, go ahead and close the opened device.             */
            //fclose(FilePointer);

            /* Flag that the device is not initialized.                 */
            
            ret_val               = HCITR_ERROR_UNABLE_TO_OPEN_TRANSPORT;
         }
      }

   }
   else
      ret_val = HCITR_ERROR_UNABLE_TO_OPEN_TRANSPORT;

   return(ret_val);
}

   /* The following function is responsible for closing the specific HCI*/
   /* Transport layer that was opened via a successful call to the      */
   /* HCITR_COMOpen() function (specified by the first parameter).      */
   /* Bluetopia makes a call to this function whenever an either        */
   /* Bluetopia is closed, or an error occurs during initialization and */
   /* the driver has been opened (and ONLY in this case).  Once this    */
   /* function completes, the transport layer that was closed will no   */
   /* longer process received data until the transport layer is         */
   /* Re-Opened by calling the HCITR_COMOpen() function.                */
   /* * NOTE * This function *MUST* close the specified COM Port.  This */
   /*          module will then call the registered COM Data Callback   */
   /*          function with zero as the data length and NULL as the    */
   /*          data pointer.  This will signify to the HCI Driver that  */
   /*          this module is completely finished with the port and     */
   /*          information and (more importantly) that NO further data  */
   /*          callbacks will be issued.  In other words the very last  */
   /*          data callback that is issued from this module *MUST* be a*/
   /*          data callback specifying zero and NULL for the data      */
   /*          length and data buffer (respectively).                   */
void BTPSAPI HCITR_COMClose(unsigned int HCITransportID)
{

   unsigned long            CallbackParameter;
   HCITR_COMDataCallback_t  COMDataCallback;

   /* Check to make sure that the specified Transport ID is valid.      */
   if((HCITransportID) && (HCITransportOpen))
   {
      /* Appears to be valid, go ahead and close the port.              */

      /* Assert the reset.                                              */
	  //set low
	  //GpioClrRegOneBit(GPIO_B_OUT,GPIOB7);
	s_BtRstSetPinVal(0);

      /* Note the Callback information.                                 */

      COMDataCallback   = _COMDataCallback;
      CallbackParameter = _COMCallbackParameter;

      /* Flag that the HCI Transport is no longer open.                 */
      HCITransportOpen = 0;


      /* Use a short delay to give the receive thread a chance to close.*/
      BTPS_Delay(10);



      /* Flag that there is no callback information present.            */
      _COMDataCallback      = NULL;
      _COMCallbackParameter = 0;

      /* All finished, perform the callback to let the upper layer know */
      /* that this module will no longer issue data callbacks and is    */
      /* completely cleaned up.                                         */
      if(COMDataCallback)
         (*COMDataCallback)(HCITransportID, 0, NULL, CallbackParameter);
   }
}

   /* The following function is responsible for instructing the         */
   /* specified HCI Transport layer (first parameter) that was opened   */
   /* via a successful call to the HCITR_COMOpen() function to          */
   /* reconfigure itself with the specified information.  This          */
   /* information is completely opaque to the upper layers and is passed*/
   /* through the HCI Driver layer to the transport untouched.  It is   */
   /* the responsibility of the HCI Transport driver writer to define   */
   /* the contents of this member (or completely ignore it).            */
   /* * NOTE * This function does not close the HCI Transport specified */
   /*          by HCI Transport ID, it merely reconfigures the          */
   /*          transport.  This means that the HCI Transport specified  */
   /*          by HCI Transport ID is still valid until it is closed    */
   /*          via the HCI_COMClose() function.                         */
void BTPSAPI HCITR_COMReconfigure(unsigned int HCITransportID, HCI_Driver_Reconfigure_Data_t *DriverReconfigureData)
{
   unsigned int BaudRate;
	
   Display(("kevin:HCITR_COMReconfigure \r\n"));
   /* Check to make sure that the specified Transport ID is valid.      */
   if((HCITransportID == TRANSPORT_ID) && (HCITransportOpen) && (DriverReconfigureData))
   {
      if((DriverReconfigureData->ReconfigureCommand == 65540/*HCI_COMM_DRIVER_RECONFIGURE_DATA_COMMAND_CHANGE_PARAMETERS*/) && (DriverReconfigureData->ReconfigureData))
      {
         /* Set tne new baud rate.                                      */
         BaudRate = ((HCI_COMMDriverInformation_t *)(DriverReconfigureData->ReconfigureData))->BaudRate;
    #if 0

		 BuartIOctl(UART_IOCTL_DISENRX_SET,0); //禁止收数据同时将uart双方都配置到高速（1M）模式
    //set high speed
    //ChangeBtBautRate(BaudRate);
    BuartIOctl(UART_IOCTL_BAUDRATE_SET,BaudRate);//different bt module may have the differnent bautrate!!
    BuartIOctl(UART_IOCTL_DISENRX_SET,1);
	#endif
      }
   }
}

int DisconnectExcept = 0;
   /* The following function is responsible for actually sending data   */
   /* through the opened HCI Transport layer (specified by the first    */
   /* parameter).  Bluetopia uses this function to send formatted HCI   */
   /* packets to the attached Bluetooth Device.  The second parameter to*/
   /* this function specifies the number of bytes pointed to by the     */
   /* third parameter that are to be sent to the Bluetooth Device.  This*/
   /* function returns a zero if the all data was transferred           */
   /* successfully or a negative value if an error occurred.  This      */
   /* function MUST NOT return until all of the data is sent (or an     */
   /* error condition occurs).  Bluetopia WILL NOT attempt to call this */
   /* function repeatedly if data fails to be delivered.  This function */
   /* will block until it has either buffered the specified data or sent*/
   /* all of the specified data to the Bluetooth Device.                */
   /* * NOTE * The type of data (Command, ACL, SCO, etc.) is NOT passed */
   /*          to this function because it is assumed that this         */
   /*          information is contained in the Data Stream being passed */
   /*          to this function.                                        */
int BTPSAPI HCITR_COMWrite(unsigned int HCITransportID, unsigned int Length, unsigned char *Buffer)
{
   int ret_val;


   unsigned int	LengthCopy;
   unsigned char *BufferCopy;
   char hcistr[3];
  //ignoreLogging = TRUE;
   //Nina added for showing HCI buffer

   /* Check to make sure that the specified Transport ID is valid and   */
   /* the output buffer appears to be valid as well.                    */
   if((HCITransportID) && (HCITransportOpen) && (Length) && (Buffer))
   {
	/* kevin add for show HCI TX Log Info */
    ShowHciTxLogInfo(Length, Buffer);
        /* TX:02 01 20 08 00 04 00 42 00 31 1f 01 08 
           RX:04 13 05 01 01 00 01 00 YiJia Phone Disconnect Except */
       if((memcmp(Buffer,"\x02\x01\x20\x08\x00\x04",6) == 0) && (memcmp(&Buffer[9],"\x31\x1f\x01\x08",4) == 0))
       {
           DisconnectExcept  = 1;
       }
      while(Length)
      {
         ret_val = PortSends(7,Buffer, Length);
				//FuartSend(Buffer, Length);
				 //PortSends(0, Buffer, Length);
         if((ret_val > 0) && (ret_val <= Length))
         {
            Length -= ret_val;
            Buffer += ret_val;
         }
         else
            Length = 0;
      }

      ret_val = Length;
   }
   else
      ret_val = HCITR_ERROR_WRITING_TO_PORT;

   return(ret_val);
}

   /* The following function is provided to allow a mechanism for       */
   /* modules to force the processing of incoming COM Data.             */
   /* * NOTE * This function is only applicable in device stacks that   */
   /*          are non-threaded.  This function has no effect for device*/
   /*          stacks that are operating in threaded environments.      */
void BTPSAPI HCITR_COMProcess(unsigned int HCITransportID)
{
	int     Result;
	if((Result = PortRecvs(7,ReceiveBuffer, sizeof(ReceiveBuffer), 0)) > 0)
    {
        /* kevin add for show HCI Rx Log Info */
        ShowHciRxLogInfo((unsigned int)Result, ReceiveBuffer);
		
		if((_COMDataCallback) && (HCITransportOpen))
            (*_COMDataCallback)(1, (unsigned int)Result, ReceiveBuffer, _COMCallbackParameter);
	}
}

   /* The following function is responsible for suspending the HCI COM  */
   /* transport.  It will block until the transmit buffers are empty and*/
   /* all data has been sent then put the transport in a suspended      */
   /* state.  This function will return a value of 0 if the suspend was */
   /* successful or a negative value if there is an error.              */
   /* * NOTE * An error will occur if the suspend operation was         */
   /*          interrupted by another event, such as data being received*/
   /*          before the transmit buffer was empty.                    */
   /* * NOTE * The calling function must lock the Bluetooth stack with a*/
   /*          call to BSC_LockBluetoothStack() before this function is */
   /*          called.                                                  */
   /* * NOTE * This function should only be called when the baseband    */
   /*          low-power protocol in use has indicated that it is safe  */
   /*          to sleep.  Also, this function must be called            */
   /*          successfully before any clocks necessary for the         */
   /*          transport to operate are disabled.                       */
int BTPSAPI HCITR_COMSuspend(unsigned int HCITransportID)
{
	//printf_uart("kevin:HCITR_COMSuspend\r\n");
	return 0;
}
