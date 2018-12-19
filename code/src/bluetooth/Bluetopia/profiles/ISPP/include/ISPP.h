/*****< ispp.h >***************************************************************/
/*      Copyright 2011 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  ISPP - Bluetooth Stack iSPP Interface for Stonestreet One Bluetooth       */
/*         Protocol Stack.                                                    */
/*                                                                            */
/*  Author:  Tim Thomas                                                       */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   04/22/11  T. Thomas      Initial creation.                               */
/******************************************************************************/
#ifndef __ISPPH__
#define __ISPPH__

#include "BTTypes.h"            /* Bluetooth Type Definitions/Constants.      */

   /* The following function is responsible for making sure that the    */
   /* Bluetooth Stack iAP Serial Port Profile (iSPP) Module is          */
   /* Initialized correctly.  This function *MUST* be called before ANY */
   /* other Bluetooth Stack iSPP function can be called.  This function */
   /* returns a non-zero value if the Module was initialized correctly, */
   /* or zero if there was an error.                                    */
   /* * NOTE * Internally, this module will make sure that this         */
   /*          function has been called at least once so that the       */
   /*          module will function.  Calling this function from an     */
   /*          external location is not necessary.                      */
int InitializeISPPModule(void);

   /* The following function is responsible for instructing the         */
   /* Bluetooth Stack iAP Serial Port Profile (iSPP) Module to clean up */
   /* any resources that it has allocated.  Once this function has      */
   /* completed, NO other Bluetooth Stack iSPP Functions can be called  */
   /* until a successful call to the InitializeISPPModule() function is */
   /* made.  The parameter to this function specifies the context in    */
   /* which this function is being called.  If the specified parameter  */
   /* is TRUE, then the module will make sure that NO functions that    */
   /* would require waiting/blocking on Mutexes/Events are called.  This*/
   /* parameter would be set to TRUE if this function was called in a   */
   /* context where threads would not be allowed to run.  If this       */
   /* function is called in the context where threads are allowed to run*/
   /* then this parameter should be set to FALSE.                       */
void CleanupISPPModule(Boolean_t ForceCleanup);

#endif
