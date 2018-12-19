// filename: ISSP_Defs.h
#include "ISSP_Revision.h"
#ifdef PROJECT_REV_1
// Copyright 2006-2015, Cypress Semiconductor Corporation.
//
// This software is owned by Cypress Semiconductor Corporation (Cypress)
// and is protected by and subject to worldwide patent protection (United
// States and foreign), United States copyright laws and international 
// treaty provisions. Cypress hereby grants to licensee a personal, 
// non-exclusive, non-transferable license to copy, use, modify, create 
// derivative works of, and compile the Cypress Source Code and derivative 
// works for the sole purpose of creating custom software in support of 
// licensee product to be used only in conjunction with a Cypress integrated 
// circuit as specified in the applicable agreement. Any reproduction, 
// modification, translation, compilation, or representation of this 
// software except as specified above is prohibited without the express 
// written permission of Cypress.
//
// Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND,EXPRESS OR IMPLIED, 
// WITH REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Cypress reserves the right to make changes without further notice to the
// materials described herein. Cypress does not assume any liability arising
// out of the application or use of any product or circuit described herein.
// Cypress does not authorize its products for use as critical components in
// life-support systems where a malfunction or failure may reasonably be
// expected to result in significant injury to the user. The inclusion of
// Cypress� product in a life-support systems application implies that the
// manufacturer assumes all risk of such use and in doing so indemnifies
// Cypress against all charges.
//
// Use may be limited by and subject to the applicable Cypress software
// license agreement.
//
//--------------------------------------------------------------------------
#ifndef INC_ISSP_DEFS
#define INC_ISSP_DEFS

#include "ISSP_directives.h"

#define TARGET_DATABUFF_LEN    128 

// The number of Flash blocks in each part is defined here. This is used in
// main programming loop when programming and verifying the blocks.

#ifdef CY8C20x36							// **** CY8C20x36 (8k) Device ****
    #define NUM_BANKS                     1
    #define BLOCKS_PER_BANK              64
    #define SECURITY_BYTES_PER_BANK      16
#endif

#ifdef CY8C20x46							// **** CY8C20x46 (16k) Device ****
    #define NUM_BANKS                     1
    #define BLOCKS_PER_BANK             128
    #define SECURITY_BYTES_PER_BANK      32
#endif

#ifdef CY8C20x66							// **** CY8C20x66 (32k) Device ****
    #define NUM_BANKS                     1
    #define BLOCKS_PER_BANK             256
    #define SECURITY_BYTES_PER_BANK      64
#endif

#ifdef CY8C20x37							// **** CY8C20x37 (8k) Device ****
    #define NUM_BANKS                     1
    #define BLOCKS_PER_BANK              64
    #define SECURITY_BYTES_PER_BANK      16
#endif

#ifdef CY8C20x47							// **** CY8C20x47 (16k) Device ****
    #define NUM_BANKS                     1
    #define BLOCKS_PER_BANK             128
    #define SECURITY_BYTES_PER_BANK      32
#endif

#ifdef CY8C20x67							// **** CY8C20x67 (32k) Device ****
    #define NUM_BANKS                     1
    #define BLOCKS_PER_BANK             256
    #define SECURITY_BYTES_PER_BANK      64
#endif

#endif //(INC_ISSP_DEFS)
#endif //(PROJECT_REV_)
//end of file ISSP_Defs.h
