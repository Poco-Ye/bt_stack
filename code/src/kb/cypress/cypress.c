// filename: cypress.c
#include "ISSP_Revision.h"
#ifdef PROJECT_REV_1
/* Copyright 2006-2015, Cypress Semiconductor Corporation.
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
// Cypress锟� product in a life-support systems application implies that the
// manufacturer assumes all risk of such use and in doing so indemnifies
// Cypress against all charges.
//
// Use may be limited by and subject to the applicable Cypress software
// license agreement.
//
//---------------------------------------------------------------------------*/

/* ############################################################################
   ###################  CRITICAL PROJECT CONSTRAINTS   ########################
   ############################################################################ 

   ISSP programming can only occur within a temperature range of 5C to 50C.
   - This project is written without temperature compensation and using
     programming pulse-widths that match those used by programmers such as the
     Mini-Prog and the ISSP Programmer.
     This means that the die temperature of the PSoC device cannot be outside
     of the above temperature range.
     If a wider temperature range is required, contact your Cypress Semi-
     conductor FAE or sales person for assistance.

   This program supports only the CY8C20xx6 and CY8C20xx7 family of devices. 
   - It does not suport devices that have not been released for sale at the 
     time this version was created. If you need to ISSP program a newly released
     device, please contact Cypress Semiconductor Applications, your FAE or 
     sales person for assistance.

   ############################################################################ 
   ##########################################################################*/

/*/////////////////////////////////////////////////////////////////////////////
//  PROJECT HISTORY
//  date     	 revision  author  description
//  -------- 	 --------  ------  -----------------------------------------------
//  2/6/10    		1.00     XCH     - First release 
//
/////////////////////////////////////////////////////////////////////////////*/
     
/* This program uses information found in Cypress Semiconductor application 
// notes "Programming - In-System Serial Programming Protocol", AN2026.  The 
// version of this application note that applies to the specific PSoC that is
// being In-System Serial Programmed should be read and and understood when 
// using this program. (http://www.cypress.com)

// This project is included with releases of PSoC Programmer software. It is 
// important to confirm that the latest revision of this software is used when
// it is used. The revision of this copy can be found in the Project History 
// table below.
*/

/* (((((((((((((((((((((((((((((((((((((())))))))))))))))))))))))))))))))))))))
 PSoC In-System Serial Programming (ISSP) Template
 This PSoC Project is designed to be used as a template for designs that
 require PSoC ISSP Functions.
 
 This project is based on the AN2026 series of Application Notes. That app
 note should be referenced before any modifications to this project are made.
 
 The subroutines and files were created in such a way as to allow easy cut & 
 paste as needed. There are no customer-specific functions in this project. 
 This demo of the code utilizes a PSoC as the Host.
 
 Some of the subroutines could be merged, or otherwise reduced, but they have
 been written as independently as possible so that the specific steps involved
 within each function can easily be seen. By merging things, some code-space 
 savings could be realized. 
 
 As is, and with all features enabled, the project consumes approximately 3500
 bytes of code space, and 19-Bytes of RAM (not including stack usage). The
 Block-Verify requires a 128-Byte buffer for read-back verification. This same
 buffer could be used to hold the (actual) incoming program data.
 
 Please refer to the compiler-directives file "directives.h" to see the various
 features.
 
 The pin used in this project are assigned as shown below. The HOST pins are 
 arbitrary and any 3 pins could be used (the masks used to control the pins 
 must be changed). The TARGET pins cannot be changed, these are fixed function
 pins on the PSoC. 
 The PWR pin is used to provide power to the target device if power cycle
 programming mode is used. The compiler directive RESET_MODE in ISSP_directives.h
 is used to select the programming mode. This pin could control the enable on
 a voltage regulator, or could control the gate of a FET that is used to turn
 the power to the PSoC on.
 The TP pin is a Test Point pin that can be used signal from the host processor
 that the program has completed certain tasks. Predefined test points are
 included that can be used to observe the timing for bulk erasing, block 
 programming and security programming.
 
      SIGNAL  HOST  TARGET
      ---------------------
      SDATA   P1.2   P1.0
      SCLK    P1.3   P1.1
      XRES    P2.0   XRES
      PWR     P2.1   Vdd 
      TP      P0.7   n/a
 
 For test & demonstration, this project generates the program data internally. 
 It does not take-in the data from an external source such as I2C, UART, SPI,
 etc. However, the program was written in such a way to be portable into such
 designs. The spirit of this project was to keep it stripped to the minimum 
 functions required to do the ISSP functions only, thereby making a portable 
 framework for integration with other projects.

 The high-level functions have been written in C in order to be portable to
 other processors. The low-level functions that are processor dependent, such 
 as toggling pins and implementing specific delays, are all found in the file
 ISSP_Drive_Routines.c. These functions must be converted to equivalent
 functions for the HOST processor.  Care must be taken to meet the timing 
 requirements when converting to a new processor. ISSP timing information can
 be found in Application Note AN57631.  All of the sections of this program
 that need to be modified for the host processor have "PROCESSOR_SPECIFIC" in
 the comments. By performing a "Find in files" using "PROCESSOR_SPECIFIC" these
 sections can easily be identified.

 The variables in this project use Hungarian notation. Hungarian prepends a
 lower case letter to each variable that identifies the variable type. The
 prefixes used in this program are defined below:
  b = byte length variable, signed char and unsigned char
  i = 2-byte length variable, signed int and unsigned int
  f = byte length variable used as a flag (TRUE = 0, FALSE != 0)
  ab = an array of byte length variables


 After this program has been ported to the desired host processor the timing
 of the signals must be confirmed.  The maximum SCLK frequency must be checked
 as well as the timing of the bulk erase, block write and security write
 pulses.
 
 The maximum SCLK frequency for the target device can be found in the device 
 datasheet under AC Programming Specifications with a Symbol of "Fsclk".
 An oscilloscope should be used to make sure that no half-cycles (the high 
 time or the low time) are shorter than the half-period of the maximum
 freqency. In other words, if the maximum SCLK frequency is 8MHz, there can be
 no high or low pulses shorter than 1/(2*8MHz), or 62.5 nsec.

 The test point (TP) functions, enabled by the define USE_TP, provide an output
 from the host processor that brackets the timing of the internal bulk erase,
 block write and security write programming pulses. An oscilloscope, along with
 break points in the PSoC ICE Debugger should be used to verify the timing of 
 the programming.  The Application Note, "Host-Sourced Serial Programming"
 explains how to do these measurements and should be consulted for the expected
 timing of the erase and program pulses.

(((((((((((((((((((((((((((((((((((((()))))))))))))))))))))))))))))))))))))) */

/*----------------------------------------------------------------------------
//                               C main line
//----------------------------------------------------------------------------
*/

#include <base.h>  
#include "posapi.h"   
#include "../kb_touch.h"


// ------ Declarations Associated with ISSP Files & Routines -------
//     Add these to your project as needed.
#include "ISSP_extern.h"
#include "ISSP_directives.h"
#include "ISSP_defs.h"
#include "ISSP_errors.h"
/* ------------------------------------------------------------------------- */

unsigned int  iBlockCounter;
unsigned int  iChecksumData;
unsigned int  iChecksumTarget;

/* ========================================================================= */
// ErrorTrap()
// Return is not valid from main for PSOC, so this ErrorTrap routine is used.
// For some systems returning an error code will work best. For those, the 
// calls to ErrorTrap() should be replaced with a return(bErrorNumber). For
// other systems another method of reporting an error could be added to this
// function -- such as reporting over a communcations port.
/* ========================================================================= */
int ErrorTrap(unsigned char bErrorNumber)
{
#ifndef RESET_MODE
    // Set all pins to highZ to avoid back powering the PSoC through the GPIO
    // protection diodes.
    SetSCLKHiZ();   
    SetSDATAHiZ();
    // If Power Cycle programming, turn off the target
    RemoveTargetVDD();
#endif
    return(bErrorNumber);
}

/* ========================================================================= */
/* MAIN LOOP                                                                 */
/* Based on the diagram in the AN57631                                       */
/* ========================================================================= */

int cypressdownload(uchar *firmware, int filelen)
{
    // -- This example section of commands show the high-level calls to -------
    // -- perform Target Initialization, SilcionID Test, Bulk-Erase, Target ---
    // -- RAM Load, FLASH-Block Program, and Target Checksum Verification. ----

	if (CheckFirmware(firmware, filelen)!=0)
		return -1;
    #ifdef USE_TP
    InitTP();
    #endif
    
    // >>>> ISSP Programming Starts Here <<<<
    
    // Acquire the device through reset or power cycle
    #ifdef RESET_MODE
    // Initialize the Host & Target for ISSP operations
    if (fIsError = fXRESInitializeTargetForISSP()) {
        return ErrorTrap(fIsError);
    }
    #else
    // Initialize the Host & Target for ISSP operations
    if (fIsError = fPowerCycleInitializeTargetForISSP()) {
        return ErrorTrap(fIsError);
    }
    #endif
        
    // Run the SiliconID Verification, and proceed according to result.
    if (fIsError = fVerifySiliconID()) { 
        return ErrorTrap(fIsError);
    }

    #ifdef USE_TP
    SetTPHigh();    // Only used of Test Points are enabled  
    #endif
    
    // Bulk-Erase the Device.
	if (fIsError = fEraseTarget()) {
        return ErrorTrap(fIsError);
    }
    #ifdef USE_TP
    SetTPLow();    // Only used of Test Points are enabled
    #endif
    //==============================================================//
    // Program Flash blocks with predetermined data. In the final application
    // this data should come from the HEX output of PSoC Designer.
    iChecksumData = 0;     // Calculte the device checksum as you go
    for (iBlockCounter=0; iBlockCounter<BLOCKS_PER_BANK; iBlockCounter++) {

        LoadProgramData((unsigned char)iBlockCounter);			//this loads CY8C24894-24LTXI (the programmer) with test data
        iChecksumData += iLoadTarget();											
        																		
            #ifdef USE_TP
            SetTPHigh();    // Only used of Test Points are enabled 
            #endif

		if (fIsError = fProgramTargetBlock((unsigned char)iBlockCounter)) {
            return ErrorTrap(fIsError);
            }
        
		#ifdef USE_TP
        SetTPLow();    // Only used of Test Points are enabled  
        #endif
		
	    if (fIsError = fReadStatus()) { 
    		return ErrorTrap(fIsError);
	    }
    }
    //=======================================================//
    // Doing Verify
	// Verify included for completeness in case host desires to do a stand-alone verify at a later date.
    for (iBlockCounter=0; iBlockCounter<BLOCKS_PER_BANK; iBlockCounter++) {
      	LoadProgramData((unsigned char) iBlockCounter);
        if (fIsError = fVerifySetup((unsigned char)iBlockCounter)) {
            return ErrorTrap(fIsError);
        }
		if (fIsError = fReadStatus()) { 
        	return ErrorTrap(fIsError);
    	}
		if (fIsError = fReadByteLoop()) {
			return ErrorTrap(fIsError);
		}			
    }
	//=======================================================//
    // Program security data into target PSoC. In the final application this 
    // data should come from the HEX output of PSoC Designer.
		        
    // Load one bank of security data from hex file into buffer
    if (fIsError = fLoadSecurityData()) {
        return ErrorTrap(fIsError);
    }
	// Secure one bank of the target flash
    if (fIsError = fSecureTargetFlash()) {
        return ErrorTrap(fIsError);
    }
    //==============================================================//
    //Do READ-SECURITY after SECURE    
    //Load one bank of security data from hex file into buffer
    //loads abTargetDataOUT[] with security data that was used in secure bit stream
	if (fIsError = fLoadSecurityData()) {
	    return ErrorTrap(fIsError);
	}
	if (fIsError = fReadSecurity()) { 
    	return ErrorTrap(fIsError);
	}
    //=======================================================//     
    //Doing Checksum after READ-SECURITY
    iChecksumTarget = 0;

	if (fIsError = fAccTargetBankChecksum(&iChecksumTarget)) {
        return ErrorTrap(fIsError);
    }
    
	if (iChecksumTarget != (iChecksumData&0xffff)){
        return ErrorTrap(VERIFY_ERROR);
    }
     
    // *** SUCCESS *** 
    // At this point, the Target has been successfully Initialize, ID-Checked,
    // Bulk-Erased, Block-Loaded, Block-Programmed, Block-Verified, and Device-
    // Checksum Verified.

    // You may want to restart Your Target PSoC Here.
    ReStartTarget();
    return(PASS);
} 
extern int giCypressUpdateBusy;
void TouchKeyUpdate(void)
{
	uchar TouchKeyFirmware[20*1024];
	int iRet;
	int cnt = 0;
	int buflen;
	int i;
	uchar ver;
	memset(TouchKeyFirmware, 0, sizeof(TouchKeyFirmware));
	ScrCls();
	if (get_machine_type() != D200)
	{
		ScrPrint(0, 0, 0xE0, "Touchkey Update");
		ScrPrint(0, 4, 0x60, "No Touchkey!");
		GetKeyWait(6000);
		return;
	}
	PortOpen(0, "115200,8,n,1");
	ScrPrint(0, 0, 0xE0, "Touchkey Update");

	//1. download the firmware file
	ScrPrint(0, 2, 0x40, "file downloading...");
	ScrPrint(0, 4, 0x40, "Now Version:%02x",s_TouchKeyVersion());
	PortReset(0);
	buflen = sizeof(TouchKeyFirmware);
	
	while(!PortPeep(0, TouchKeyFirmware, buflen))
	{
		if (!kbhit() && getkey()==KEYCANCEL)
		{
			PortClose(0);
			return;
		}
			
	}
	while(1)
	{
		iRet = PortRecvs(0, TouchKeyFirmware+cnt, buflen-cnt, 500);
		if (iRet > 0)
		{
			cnt += iRet;
		}
		else
		{
			break;
		}
		ScrPrint(0, 6, 0x40, "%d bytes",cnt);
	}
	PortClose(0);
	//clean dirty data
	for (i = 0;i < cnt;i++)
	{
		if (TouchKeyFirmware[i] == ':')
			break;
	}
	if (i == cnt)
	{
		goto UPDATE_ERROR;
	}
	if (i > 0)
	{
		memmove(TouchKeyFirmware, TouchKeyFirmware+i, cnt-i);
		memset(TouchKeyFirmware+cnt-i, 0, i);
	}
	//check file ending
	buflen = strlen(TouchKeyFirmware);
	if (buflen < 128*128)
	{
		goto UPDATE_ERROR;
	}
	if(memcmp(TouchKeyFirmware+buflen-13, ":00000001ff\r\n", 13) != 0)
	{
		goto UPDATE_ERROR;
	}
	
	//2. update the firmware file
	ScrClrLine(2,9);
	ScrPrint(0, 4, 0x40, "Updating...");
	KbLock(2);
	s_TouchKeyStop();
	gpio_set_pin_interrupt(KB_INT_GPIO,0);
	gpio_set_pin_interrupt(WLAN_EN_KEY,0);
	giCypressUpdateBusy = 1;
	cnt = 3;
	while(cnt--)
	{
		iRet = cypressdownload(TouchKeyFirmware, buflen);
		if (iRet == 0)
			break;
	}
	giCypressUpdateBusy = 0;
				
	if (iRet == 0)
	{
		DelayMs(100);
		ver = s_TouchKeyVersion();
		if (ver >= 0x04)//兼容V03版触摸按键驱动
		{
			s_TouchKeySetBaseLine();
		}
		s_TouchKeyStart();
		gpio_set_pin_interrupt(KB_INT_GPIO,1);
		gpio_set_pin_interrupt(WLAN_EN_KEY,1);
		ScrClrLine(2,9);
		ScrPrint(0, 4, 0x40, "TouchKey Update Success!");
		ScrPrint(0, 6, 0x40, "Now Version:%02x",ver);
		ScrPrint(0, 8, 0x40, "Please Update Config File!");
		while(1);
	}
	else
	{
		ScrClrLine(2,9);
		ScrPrint(0, 4, 0x40, "TouchKey Update Fail!");
		ScrPrint(0, 6, 0x40, "Code<%d>",iRet);
		while(1);
	}
UPDATE_ERROR:
	ScrClrLine(2,9);
	ScrPrint(0, 4, 0x40, "TouchKey Update Error!");
	ScrPrint(0, 6, 0x40, "File Check Error!");
	PortClose(0);
	GetKeyWait(6000);
}
#endif  //(PROJECT_REV_)
// end of file main.c
