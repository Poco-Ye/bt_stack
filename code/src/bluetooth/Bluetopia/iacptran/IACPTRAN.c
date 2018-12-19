/*****< iacptran.c >***********************************************************/
/*      Copyright 2001 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  IACPTRAN - Stonestreet One Apple Authentication Coprocessor Transport     */
/*             Layer.                                                         */
/*                                                                            */
/*  Author:  Tim Thomas                                                       */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   01/13/11  T. Thomas      Initial creation.                               */
/******************************************************************************/
#include "BTPSKRNL.h"            /* Bluetooth Kernel Prototypes/Constants.    */
#include "IACPTRAN.h"            /* ACP Transport Prototypes/Constants.       */

#define Display(_x)           // do { printf_uart _x; } while(0)

   /* The following function is responsible for Initializing the ACP    */
   /* hardware.  The function is passed a pointer to an opaque object.  */
   /* Since the hardware platform is not know, it is intended that the  */
   /* parameter be used to pass hardware specific information to the    */
   /* module that would be needed for proper operation.  Each           */
   /* implementation will define what gets passed to this function.     */
   /* This function returns zero if successful or a negative value if   */
   /* the initialization fails.                                         */
int BTPSAPI IACPTR_Initialize(void *Parameters)
{
  Display(("IACPTR_Initialize \r\n"));
//xxx
    MfiCpInit();
   return(0);
}

   /* The following function is responsible for performing any cleanup  */
   /* that may be required by the hardware or module.  This function    */
   /* returns no status.                                                */
void BTPSAPI IACPTR_Cleanup(void)
{
  Display(("IACPTR_Cleanup \r\n"));

//xxx
    MfiCpClose();
}

   /* The following function is responsible for Reading Data from a     */
   /* device via the ACP interface.  The first parameter to this        */
   /* function is the Register/Address that is to be read.  The second  */
   /* parameter indicates the number of bytes that are to be read from  */
   /* the device.  The third parameter is a pointer to a buffer where   */
   /* the data read is placed.  The size of the Read Buffer must be     */
   /* large enough to hold the number of bytes being read.  The last    */
   /* parameter is a pointer to a variable that will receive status     */
   /* information about the result of the read operation.  This function*/
   /* should return a non-negative return value if successful (and      */
   /* return IACP_STATUS_SUCCESS in the status parameter).  This        */
   /* function should return a negative return error code if there is an*/
   /* error with the parameters and/or module initialization.  Finally, */
   /* this function can also return success (non-negative return value) */
   /* as well as specifying a non-successful Status value (to denote    */
   /* that there was an actual error during the write process, but the  */
   /* module is initialized and configured correctly).                  */
   /* * NOTE * If this function returns a non-negative return value then*/
   /*          the caller will ALSO examine the value that was placed in*/
   /*          the Status parameter to determine the actual status of   */
   /*          the operation (success or failure).  This means that a   */
   /*          successful return value will be a non-negative return    */
   /*          value and the status member will contain the value:      */
   /*          IACP_STATUS_SUCCESS                                      */
int BTPSAPI IACPTR_Read(unsigned char Register, unsigned char BytesToRead, unsigned char *ReadBuffer, unsigned char *Status)
{
   int ret_val;
   int i =0;
  //Display(("IACPTR_Read \r\n"));
  //Display(("IACPTR_Read BytesToRead = %d\r\n",BytesToRead));
   /* Verify that the parameters passed in appear valid.                */
   if((BytesToRead) && (ReadBuffer) && (Status))
   {
//xxx
       ret_val = MfiCpGetRegData(Register, ReadBuffer, BytesToRead);
        if(ret_val == 0)
      {
         /* If the number of bytes written is equal to the number of    */
         /* bytes specified to write then flag the status byte as       */
         /* success.                                                    */
         *Status = IACP_STATUS_SUCCESS;
		 Display(("IACPTR_Read BytesToRead =%d\r\n",BytesToRead));
		 Display(("IACPTR_Read ReadBuffer="));
		 for(i=0;i<(int)BytesToRead; i++)
			 Display(("%d",ReadBuffer[i]));
	   	Display(("\r\n"));
      }
      else
      {
         /* If the number of bytes written is not equal to the number of*/
         /* bytes specified to write then flag the error in the status  */
         /* byte.                                                       */
         *Status = IACP_STATUS_READ_FAILURE;
		 Display(("************kevin:IACPTR_Read error ****************\r\n"));
      }
   }
   else
      ret_val = IACP_INVALID_PARAMETER;
   
  // Display(("IACPTR_Read ret_val = %d\r\n",ret_val));
   return(ret_val);
}

   /* The following function is responsible for Writing Data to a device*/
   /* via the ACP interface.  The first parameter to this function is   */
   /* the Register/Address where the write operation is to start.  The  */
   /* second parameter indicates the number of bytes that are to be     */
   /* written to the device.  The third parameter is a pointer to a     */
   /* buffer where the data to be written is placed.  The last parameter*/
   /* is a pointer to a variable that will receive status information   */
   /* about the result of the write operation.  This function should    */
   /* return a non-negative return value if successful (and return      */
   /* IACP_STATUS_SUCCESS in the status parameter).  This function      */
   /* should return a negative return error code if there is an error   */
   /* with the parameters and/or module initialization.  Finally, this  */
   /* function can also return success (non-negative return value) as   */
   /* well as specifying a non-successful Status value (to denote that  */
   /* there was an actual error during the write process, but the module*/
   /* is initialized and configured correctly).                         */
   /* * NOTE * If this function returns a non-negative return value then*/
   /*          the caller will ALSO examine the value that was placed in*/
   /*          the Status parameter to determine the actual status of   */
   /*          the operation (success or failure).  This means that a   */
   /*          successful return value will be a non-negative return    */
   /*          value and the status member will contain the value:      */
   /*          IACP_STATUS_SUCCESS                                      */
int BTPSAPI IACPTR_Write(unsigned char Register, unsigned char BytesToWrite, unsigned char *WriteBuffer, unsigned char *Status)
{
   int ret_val;
   int i=0;
 // Display(("IACPTR_Write \r\n"));
 // Display(("IACPTR_Write BytesToWrite = %d\r\n",BytesToWrite));
   /* Verify that the parameters passed in appear valid.                */
   if((BytesToWrite) && (WriteBuffer) && (Status))
   {
//xxx
        ret_val = MfiCpSetRegData(Register, WriteBuffer, BytesToWrite);
      if(ret_val == 0)
      {
         /* If the number of bytes written is equal to the number of    */
         /* bytes specified to write then flag the status byte as       */
         /* success.                                                    */
         *Status = IACP_STATUS_SUCCESS;
		 Display(("IACPTR_Write BytesToWrite=%d\r\n",BytesToWrite));
		 Display(("IACPTR_Write WriteBuffer="));
		 for(i=0;i<(int)BytesToWrite; i++)
			 Display(("%d",WriteBuffer[i]));
		Display(("\r\n"));
//		  Display(("IACPTR_Write BytesToWrite = %d\r\n",BytesToWrite));
      }
      else
      {
         /* If the number of bytes written is not equal to the number of*/
         /* bytes specified to write then flag the error in the status  */
         /* byte.                                                       */
         *Status = IACP_STATUS_WRITE_FAILURE;
		 Display(("************kevin:IACPTR_Write error ****************\r\n"));
      }
   }
   else
      ret_val = IACP_INVALID_PARAMETER;

   return(ret_val);
}

   /* The following function is responsible for resetting the ACP       */
   /* hardware.  The implementation should not return until the reset is*/
   /* complete.                                                         */
   /* * NOTE * This function only applies to chips version 2.0B and     */
   /*          earlier.  For chip version 2.0C and newer, this function */
   /*          should be implemented as a NO-OP.                        */
void BTPSAPI IACPTR_Reset(void)
{
  Display(("IACPTR_Reset \r\n"));

//xxx
    MfiCpReset();
}

