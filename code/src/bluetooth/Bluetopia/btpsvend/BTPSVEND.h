/*****< btpsvend.h >***********************************************************/
/*      Copyright 2009 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  BTPSVEND - Vendor specific functions/definitions/constants used to define */
/*             a set of vendor specific functions supported by the Bluetopia  */
/*             Protocol Stack.  These functions may be unique to a given      */
/*             hardware platform.                                             */
/*                                                                            */
/*  Author:  Damon Lange                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   06/09/09  D. Lange       Initial creation.                               */
/******************************************************************************/
#ifndef __BTPSVENDH__
#define __BTPSVENDH__

#include "BVENDAPI.h"           /* BTPS Vendor Specific Prototypes/Constants. */
//#define __SUPPORT_LOW_ENERGY__ 
   /* The following function represents the vendor specific function to */
   /* change the baud rate.  It will format and send the vendor specific*/
   /* command to change the baseband's baud rate then issue a           */
   /* reconfigure command to the HCI driver to change the local baud    */
   /* rate.  This function will return a BOOLEAN TRUE upon success or   */
   /* FALSE if there is an error.                                       */
Boolean_t HCI_VS_SetBaudRate(unsigned int BluetoothStackID, unsigned int NewBaudRate);

#endif
