/*****< btpskrnl.h >***********************************************************/
/*      Copyright 2012 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  BTPSKRNL - Stonestreet One Bluetooth Stack Kernel Implementation Type     */
/*             Definitions, Constants, and Prototypes.                        */
/*                                                                            */
/*  Author:  Damon Lange                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   11/08/12  M. Funk        Initial creation.                               */
/******************************************************************************/
#ifndef __BTPSKRNLH__
#define __BTPSKRNLH__

#include "BKRNLAPI.h"           /* Bluetooth Kernel Prototypes/Constants.     */

   /* Defines the maximum number of bytes that will be allocated by the */
   /* kernel abstraction module to support allocations.                 */
   /* * NOTE * This module declares a memory array of this size (in     */
   /*          bytes) that will be used by this module for memory       */
   /*          allocation.                                              */
#define BTPS_MEMORY_BUFFER_SIZE (40 * 1024)

   /* The following declared type represents the Prototype Function for */
   /* a function that can be registered with the BTPSKRNL module to     */
   /* receive output characters.  This function will be called whenever */
   /* BTPS_OutputMessage() or BTPS_DumpData() is called (or if debug is */
   /* is enabled - DEBUG preprocessor symbol, whenever the DBG_MSG() or */
   /* DBG_DUMP() MACRO is used and there is debug data to output.       */
   /* * NOTE * This function can be registered by passing the address   */
   /*          of the implementation function in the                    */
   /*          MessageOutputCallback member of the                      */
   /*          BTPS_Initialization_t structure which is passed to the   */
   /*          BTPS_Init() function.  If no function is registered then */
   /*          there will be no output (i.e. it will simply be ignored).*/
typedef int (BTPSAPI *BTPS_MessageOutputCallback_t)(int Length, char *Message);

   /* The following structure represents the structure that is passed   */
   /* to the BTPS_Init() function to notify the Bluetooth abstraction   */
   /* layer of the function(s) that are required for proper device      */
   /* functionality.                                                    */
   /* * NOTE * This structure *MUST* be passed to the BTPS_Init()       */
   /*          function AND THE GetTickCountCallback MEMBER MUST BE     */
   /*          SPECIFIED.  Failure to provide this function will cause  */
   /*          the Bluetooth sub-system to not function because the     */
   /*          scheduler will not function (as the Tick Count will      */
   /*          never change).                                           */
typedef struct _tagBTPS_Initialization_t
{
   BTPS_MessageOutputCallback_t MessageOutputCallback;
} BTPS_Initialization_t;

#define BTPS_INITIALIZATION_SIZE                         (sizeof(BTPS_Initialization_t))

   /* The following constant represents the Minimum Amount of Time      */
   /* that can be scheduled with the BTPS Scheduler.  Attempting to     */
   /* Schedule a Scheduled Function less than this time will result in  */
   /* the function being scheduled for the specified Amount.  This      */
   /* value is in Milliseconds.                                         */
#define BTPS_MINIMUM_SCHEDULER_RESOLUTION                (0)

   /* The following function is responsible for the Memory Usage        */
   /* Information.  This function accepts as input the Memory Pool Usage*/
   /* Length and a pointer to an Buffer of Memory Pool Usage structures.*/
   /* The third parameter to this function is a pointer to a unsigned   */
   /* int which will passed the Total Buffer Size.  The fourth parameter*/
   /* to this function is a pointer to a unsigned int which will be     */
   /* passed the Memory Buffer Usage.                                   */
int BTPSAPI BTPS_QueryMemoryUsage(unsigned int *Used, unsigned int *Free, unsigned int *MaxFree);

#include "BKRNLAPI.h"           /* Bluetooth Kernel Prototypes/Constants.     */

#endif
