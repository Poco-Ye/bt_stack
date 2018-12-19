// filename: ISSP_Directives.h
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
//-----------------------------------------------------------------------------

// --------------------- Compiler Directives ----------------------------------
#ifndef INC_ISSP_DIRECTIVES
#define INC_ISSP_DIRECTIVES

// This directive will enable a Genral Purpose test-point on P1.7
// It can be toggled as needed to measure timing, execution, etc...
// A "Test Point" sets a GPIO pin of the host processor high or low. This GPIO
// pin can be observed with an oscilloscope to verify the timing of key
// programming steps. TPs have been added in main() that set Port 0, pin 1
// high during bulk erase, during each block write and during security write.
// The timing of these programming steps should be verified as correct as part
// of the validation process of the final program.
//#define USE_TP


// ****************************************************************************
// ************* USER ATTENTION REQUIRED: TARGET SUPPLY VOLTAGE ***************
// ****************************************************************************
// This directive causes the proper Initialization vector #3 to be sent
// to the Target, based on what the Target Vdd programming voltage will
// be. Either 5V (if #define enabled) or 3.3V (if #define disabled).
//#define TARGET_VOLTAGE_IS_5V


// ****************************************************************************
// **************** USER ATTENTION REQUIRED: PROGRAMMING MODE *****************
// ****************************************************************************
// This directive selects whether code that uses reset programming mode or code
// that uses power cycle programming is use. Reset programming mode uses the
// external reset pin (XRES) to enter programming mode. Power cycle programming
// mode uses the power-on reset to enter programming mode.
// Applying signals to various pins on the target device must be done in a 
// deliberate order when using power cycle mode. Otherwise, high signals to GPIO
// pins on the target will power the PSoC through the protection diodes.
//#define RESET_MODE

// ****************************************************************************
// ****************** USER ATTENTION REQUIRED: TARGET PSOC ********************
// ****************************************************************************
// The directives below enable support for various PSoC devices. The root part
// number to be programmed should be un-commented so that its value becomes
// defined.  All other devices should be commented out.
// Select one device to be supported below:

// **** CY8C20xx6 devices ****



//#define CY8C20066_24LTXI 
//#define CY8C20236_24LKXI
//#define CY8C20246_24LKXI
//#define CY8C20266_24LKXI
//#define CY8C20296_24LKXI
//#define CY8C20336_24LQXI
//#define CY8C20346_24LQXI
//#define CY8C20366_24LQXI
//#define CY8C20396_24LQXI
#define CY8C20436_24LQXI
//#define CY8C20446_24LQXI
//#define CY8C20446_24LQXI
//#define CY8C20466_24LQXI
//#define CY8C20496_24LQXI
//#define CY8C20536_24PVXI
//#define CY8C20546_24PVXI
//#define CY8C20566_24PVXI
//#define CY8C20636_24LTXI
//#define CY8C20646_24LTXI
//#define CY8C20666_24LTXI
//#define CY8C20746A_24FDXC
//#define CY8C20766A_24FDXC 

//#define CY8C20246AS_24LKXI
//#define CY8C20346AS_24LQXI
//#define CY8C20446AS_24LQXI
//#define CY8C20466AS_24LQXIT
//#define CY8C20646AS_24LTXI
//#define CY8C20666AS_24LTXI

//#define CY8C20446L_24LQXI
//#define CY8C20466L_24LQXI
//#define CY8C20496L_24LQXI
//#define CY8C20546L_24PVXI
//#define CY8C20566L_24PVXI
//#define CY8C20646L_24LTXI
//#define CY8C20666L_24LTXI



// **** CY8C20xx7 devices ****



//#define CY8C20237_24SXI 
//#define CY8C20247_24SXI 
//#define CY8C20237_24LKXI 
//#define CY8C20247_24LKXI 
//#define CY8C20247S_24LKXI 
//#define CY8C20337AN_24LQXI
//#define CY8C20337_24LQXI
//#define CY8C20337H_24LQXI 
//#define CY8C20347_24LQXI 
//#define CY8C20347S_24LQXI 
//#define CY8C20437AN_24LQXI
//#define CY8C20437_24LQXI 
//#define CY8C20447_24LQXI 
//#define CY8C20447S_24LQXI 
//#define CY8C20447H_24LQXI 
//#define CY8C20447L_24LQXI 
//#define CY8C20467_24LQXI  
//#define CY8C20467S_24LQXI 
//#define CY8C20467L_24LQXI   
//#define CY8C20637AN_24LQXI
//#define CY8C20637_24LQXI 
//#define CY8C20647_24LQXI 
//#define CY8C20647S_24LQXI 
//#define CY8C20647L_24LQXI 
//#define CY8C20667_24LQXI 
//#define CY8C20667S_24LQXI 
//#define CY8C20667L_24LQXI 
//#define CY8C20747_24FDXCI
//#define CY8C20767_24FDXCI 


//-----------------------------------------------------------------------------
// This section sets the Family that has been selected. These are used to 
// simplify other conditional compilation blocks.
//-----------------------------------------------------------------------------

#ifdef CY8C20066_24LTXI
    #define CY8C20x66	// 32k devices
#endif
#ifdef CY8C20236_24LKXI
    #define CY8C20x36	// 8k devices
#endif
#ifdef CY8C20246_24LKXI
    #define CY8C20x46   // 16k devices
#endif
#ifdef CY8C20266_24LKXI
    #define CY8C20x66	// 32k devices
#endif
#ifdef CY8C20296_24LKXI
    #define CY8C20x46   // 16k devices
#endif
#ifdef CY8C20336_24LQXI
    #define CY8C20x36	// 8k devices
#endif
#ifdef CY8C20346_24LQXI
    #define CY8C20x46   // 16k devices
#endif
#ifdef CY8C20366_24LQXI
    #define CY8C20x66	// 32k devices
#endif
#ifdef CY8C20396_24LQXI
    #define CY8C20x46   // 16k devices
#endif
#ifdef CY8C20436_24LQXI
    #define CY8C20x36	// 8k devices
#endif
#ifdef CY8C20446_24LQXI
    #define CY8C20x46   // 16k devices
#endif
#ifdef CY8C20466_24LQXI
    #define CY8C20x66	// 32k devices
#endif
#ifdef CY8C20496_24LQXI
    #define CY8C20x46   // 16k devices
#endif
#ifdef CY8C20536_24PVXI
    #define CY8C20x36	// 8k devices
#endif
#ifdef CY8C20546_24PVXI
    #define CY8C20x46   // 16k devices
#endif
#ifdef CY8C20566_24PVXI
    #define CY8C20x66	// 32k devices
#endif
#ifdef CY8C20636_24LTXI
    #define CY8C20x36	// 8k devices
#endif
#ifdef CY8C20646_24LTXI
    #define CY8C20x46   // 16k devices
#endif
#ifdef CY8C20666_24LTXI
    #define CY8C20x66	// 32k devices
#endif
#ifdef CY8C20746A_24FDXC
    #define CY8C20x46	// 16k devices 
#endif
#ifdef CY8C20766A_24FDXC
    #define CY8C20x66	// 32k devices 
#endif



//CY8C20xx6AS parts
#ifdef CY8C20246AS_24LKXI
    #define CY8C20x46	// 16k devices 
#endif
#ifdef CY8C20346AS_24LQXI
    #define CY8C20x46	// 16k devices 
#endif
#ifdef CY8C20446AS_24LQXI
    #define CY8C20x46	// 16k devices 
#endif
#ifdef CY8C20466AS_24LQXIT
    #define CY8C20x66	// 32k devices 
#endif
#ifdef CY8C20646AS_24LTXI
    #define CY8C20x46	// 16k devices 
#endif
#ifdef CY8C20666AS_24LTXI
    #define CY8C20x66	// 32k devices 
#endif


//CY8C20xx6L parts
#ifdef CY8C20446L_24LQXI
    #define CY8C20x46	// 16k devices
#endif
#ifdef CY8C20466L_24LQXI
    #define CY8C20x66	// 32k devices
#endif
#ifdef CY8C20496L_24LQXI
    #define CY8C20x46	// 16k devices
#endif
#ifdef CY8C20546L_24PVXI
    #define CY8C20x46	// 16k devices
#endif
#ifdef CY8C20566L_24PVXI
    #define CY8C20x66	// 32k devices
#endif
#ifdef CY8C20646L_24LTXI
    #define CY8C20x46	// 16k devices
#endif
#ifdef CY8C20666L_24LTXI
    #define CY8C20x66	// 32k devices
#endif


//CY8C20xx7 parts
#ifdef CY8C20237_24SXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20247_24SXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20237_24LKXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20247_24LKXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20247S_24LKXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20337AN_24LQXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20337_24LQXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20337H_24LQXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20347_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20347S_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20437AN_24LQXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20437_24LQXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20447_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20447S_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20447H_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20447L_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20467_24LQXI
    #define CY8C20x67	// 32k devices
#endif
#ifdef CY8C20467S_24LQXI
    #define CY8C20x67	// 32k devices
#endif
#ifdef CY8C20467L_24LQXI
    #define CY8C20x67	// 32k devices
#endif
#ifdef CY8C20637AN_LQXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20637_24LQXI
    #define CY8C20x37	// 8k devices
#endif
#ifdef CY8C20647_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20647S_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20647L_24LQXI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20667_24LQXI
    #define CY8C20x67	// 32k devices
#endif
#ifdef CY8C20667S_24LQXI
    #define CY8C20x67	// 32k devices
#endif
#ifdef CY8C20667L_24LQXI
    #define CY8C20x67	// 32k devices
#endif
#ifdef CY8C20747_24FDXCI
    #define CY8C20x47	// 16k devices
#endif
#ifdef CY8C20767_24FDXCI
    #define CY8C20x67	// 32k devices
#endif



// ----------------------------------------------------------------------------
#endif  //(INC_ISSP_DIRECTIVES)
#endif  //(PROJECT_REV_)
//end of file ISSP_Directives.h
