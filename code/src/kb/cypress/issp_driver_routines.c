// filename: ISSP_Driver_Routines.c
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
// Cypress锟� product in a life-support systems application implies that the
// manufacturer assumes all risk of such use and in doing so indemnifies
// Cypress against all charges.
//
// Use may be limited by and subject to the applicable Cypress software
// license agreement.
//
//--------------------------------------------------------------------------

#include "ISSP_defs.h"
#include "ISSP_errors.h"
#include "ISSP_directives.h"
#include "../../pmu/pmu.h"
#define KB_I2C_SDA_GPIO GPIOB,15
#define KB_I2C_SCLK_GPIO GPIOB,16
#define KB_I2C_SDA_INPUT 	gpio_set_pin_type(KB_I2C_SDA_GPIO,GPIO_INPUT)
#define KB_I2C_SDA_OUTPUT 	gpio_set_pin_type(KB_I2C_SDA_GPIO,GPIO_OUTPUT)
#define KB_I2C_SCLK_INPUT 	gpio_set_pin_type(KB_I2C_SCLK_GPIO,GPIO_INPUT)
#define KB_I2C_SCLK_OUTPUT 	gpio_set_pin_type(KB_I2C_SCLK_GPIO,GPIO_OUTPUT)
#define KB_I2C_SDA_SET 		gpio_set_pin_val(KB_I2C_SDA_GPIO,1)
#define KB_I2C_SDA_CLEAR 	gpio_set_pin_val(KB_I2C_SDA_GPIO,0)
#define KB_I2C_SCLK_SET 	gpio_set_pin_val(KB_I2C_SCLK_GPIO,1)
#define KB_I2C_SCLK_CLEAR 	gpio_set_pin_val(KB_I2C_SCLK_GPIO,0)
#define KB_I2C_SDA_READ		gpio_get_pin_val(KB_I2C_SDA_GPIO)


extern    unsigned char    bTargetDataPtr;
extern    unsigned char    abTargetDataOUT[TARGET_DATABUFF_LEN];
unsigned char gucFirmwareData[TARGET_DATABUFF_LEN*BLOCKS_PER_BANK];
unsigned char gucFirmwareSecData[SECURITY_BYTES_PER_BANK];
#define CHECK_SUM_INDEX 21
int CheckFirmware(uchar *firmware, int filelen)
{
	uchar temp[256];
	uchar targetDataTemp[TARGET_DATABUFF_LEN/2];
	int i,j,k;
	int index;
	uchar *pstr=NULL;
	uchar *pTarget=NULL;
	pstr = firmware;
	pTarget = gucFirmwareData;
	uint iCheckSum=0;
	uint iReadCheckSum;

	for (i = 0;i < BLOCKS_PER_BANK*2;i++)
	{
		memset(temp, 0, sizeof(temp));
		memset(targetDataTemp, 0, sizeof(targetDataTemp));
		index = 0;
		for (j = 9;j < TARGET_DATABUFF_LEN+9;j++)
		{
			temp[index] = pstr[j];
			index++;
		}
			
		if (s_string2hex(temp, targetDataTemp)>0)
		{
			memcpy(pTarget, targetDataTemp, TARGET_DATABUFF_LEN/2);
			pstr += TARGET_DATABUFF_LEN+13;
			pTarget += TARGET_DATABUFF_LEN/2;
			for (k=0; k<TARGET_DATABUFF_LEN/2;k++)
			{
				iCheckSum += targetDataTemp[k];
			}
		}
		else
		{
			return -1;
		}
			
	}
	//check the checksum
	memset(temp, 0, sizeof(temp));
	memcpy(temp, firmware+filelen-CHECK_SUM_INDEX, 4);
	if (s_string2hex(temp, targetDataTemp)>0)
	{
		iReadCheckSum = targetDataTemp[0]*256+targetDataTemp[1];
		iCheckSum = iCheckSum&0xffff;
		if (iCheckSum != iReadCheckSum)
		{
			return -2;
		}
	}
	else
	{
		return -1;
	}

	memset(gucFirmwareSecData, 0xff, SECURITY_BYTES_PER_BANK);
	return 0;
}
// ****************************** PORT BIT MASKS ******************************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
//#define SDATA_PIN   0x01
//#define SCLK_PIN    0x02
//#define XRES_PIN    0x01
//#define TARGET_VDD  0x02
//#define TP_PIN      0x80
// ((((((((((((((((((((((( DEMO ISSP SUBROUTINE SECTION )))))))))))))))))))))))
// ((((( Demo Routines can be deleted in final ISSP project if not used   )))))
// ((((((((((((((((((((((((((((((((((((()))))))))))))))))))))))))))))))))))))))

// ============================================================================
// InitTargetTestData()     
// PROCESSOR_SPECIFIC
// Loads a 64-Byte array to use as test data to program target. Ultimately,
// this data should be fed to the Host by some other means, ie: I2C, RS232,
// etc. Data should be derived from hex file.
//  Global variables affected:
//    bTargetDataPtr
//    abTargetDataOUT
// ============================================================================
void InitTargetTestData(unsigned char bBlockNum)
{
    // create unique data for each block
    int index = bBlockNum*TARGET_DATABUFF_LEN;
    for (bTargetDataPtr = 0; bTargetDataPtr < TARGET_DATABUFF_LEN; bTargetDataPtr++) {
        abTargetDataOUT[bTargetDataPtr] = gucFirmwareData[index+bTargetDataPtr];
    }
}


// ============================================================================
// LoadArrayWithSecurityData()
// PROCESSOR_SPECIFIC
// Most likely this data will be fed to the Host by some other means, ie: I2C,
// RS232, etc., or will be fixed in the host. The security data should come
// from the hex file.
//   bStart  - the starting byte in the array for loading data
//   bLength - the number of byte to write into the array 
//   bType   - the security data to write over the range defined by bStart and
//             bLength
// ============================================================================
void LoadArrayWithSecurityData(unsigned char bStart, unsigned char bLength)
{
	// Now, write the desired security-bytes for the range specified
    for (bTargetDataPtr = bStart; bTargetDataPtr < bLength; bTargetDataPtr++) {
        abTargetDataOUT[bTargetDataPtr] = gucFirmwareSecData[bTargetDataPtr];
    }
}


// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// Delay()
// This delay uses a simple "nop" loop. With the CPU running at 24MHz, each
// pass of the loop is about 1 usec plus an overhead of about 3 usec.
//      total delay = (n + 3) * 1 usec
// To adjust delays and to adapt delays when porting this application, see the
// ISSP_Delays.h file.
// ****************************************************************************
void Delay(unsigned char n)
{
	DelayUs(n);
}


// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// LoadProgramData()
// The final application should load program data from HEX file generated by 
// PSoC Designer into a 64 byte host ram buffer.
//    1. Read data from next line in hex file into ram buffer. One record 
//      (line) is 64 bytes of data.
//    2. Check host ram buffer + record data (Address, # of bytes) against hex
//       record checksum at end of record line
//    3. If error reread data from file or abort
//    4. Exit this Function and Program block or verify the block.
// This demo program will, instead, load predetermined data into each block.
// The demo does it this way because there is no comm link to get data.
// ****************************************************************************
void LoadProgramData(unsigned char bBlockNum)
{
    // >>> The following call is for demo use only. <<<
    // Function InitTargetTestData fills buffer for demo
    InitTargetTestData(bBlockNum);

    // Note:
    // Error checking should be added for the final version as noted above.
    // For demo use this function just returns VOID.
}


// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// fLoadSecurityData()
// Load security data from hex file into 64 byte host ram buffer. In a fully 
// functional program (not a demo) this routine should do the following:
//    1. Read data from security record in hex file into ram buffer. 
//    2. Check host ram buffer + record data (Address, # of bytes) against hex
//       record checksum at end of record line
//    3. If error reread security data from file or abort
//    4. Exit this Function and Program block
// In this demo routine, all of the security data is set to unprotected (0x00)
// and it returns.
// This function always returns PASS. The flag return is reserving
// functionality for non-demo versions.
// ****************************************************************************
signed char fLoadSecurityData(unsigned char bBankNum)
{
    // >>> The following call is for demo use only. <<<
    // Function LoadArrayWithSecurityData fills buffer for demo
    LoadArrayWithSecurityData(0,SECURITY_BYTES_PER_BANK);
    
    // Note:
    // Error checking should be added for the final version as noted above.
    // For demo use this function just returns PASS.
    return(PASS);
}


// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// fSDATACheck()
// Check SDATA pin for high or low logic level and return value to calling
// routine.
// Returns:
//     0 if the pin was low.
//     1 if the pin was high.
// ****************************************************************************
unsigned char fSDATACheck(void)
{
    if(KB_I2C_SDA_READ)    
        return(1);
    else
        return(0);
}
        

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************        
// SCLKHigh()
// Set the SCLK pin High
// ****************************************************************************        
void SCLKHigh(void)
{
	KB_I2C_SCLK_SET;
}


// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SCLKLow()
// Make Clock pin Low
// ****************************************************************************
void SCLKLow(void)
{
    KB_I2C_SCLK_CLEAR;
}
void iic_gap()
{
	DelayUs(3);
}
void iic_etu()
{
	DelayUs(5);
}
#ifndef RESET_MODE  // Only needed for power cycle mode
// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSCLKHiZ()
// Set SCLK pin to HighZ drive mode.
// ****************************************************************************
void SetSCLKHiZ(void)
{
    //PRT1DM0 &= ~SCLK_PIN;    
    //PRT1DM1 |=  SCLK_PIN;
    //PRT1DM2 &= ~SCLK_PIN;
    KB_I2C_SCLK_INPUT;
	gpio_disable_pull(KB_I2C_SCLK_GPIO);
}
#endif

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSCLKStrong()
// Set SCLK to an output (Strong drive mode)
// ****************************************************************************
void SetSCLKStrong(void)
{
    //PRT1DM0 |=  SCLK_PIN;
    //PRT1DM1 &= ~SCLK_PIN;
   // PRT1DM2 &= ~SCLK_PIN;
	KB_I2C_SCLK_OUTPUT;
}


// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSDATAHigh()
// Make SDATA pin High
// ****************************************************************************
void SetSDATAHigh(void)
{
    //PRT1DR |= SDATA_PIN;
    KB_I2C_SDA_SET;
}

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSDATALow()
// Make SDATA pin Low
// ****************************************************************************
void SetSDATALow(void)
{
    KB_I2C_SDA_CLEAR;
}

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSDATAHiZ()
// Set SDATA pin to an input (HighZ drive mode).
// ****************************************************************************
void SetSDATAHiZ(void)
{
   // PRT1DM0 &= ~SDATA_PIN;    
   // PRT1DM1 |=  SDATA_PIN;
   // PRT1DM2 &= ~SDATA_PIN;
	KB_I2C_SDA_INPUT;
	gpio_set_pull(KB_I2C_SDA_GPIO, 0);
	gpio_enable_pull(KB_I2C_SDA_GPIO);
   
}

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSDATAStrong()
// Set SDATA for transmission (Strong drive mode) -- as opposed to being set to
// High Z for receiving data.
// ****************************************************************************
void SetSDATAStrong(void)
{
    //PRT1DM0 |=  SDATA_PIN;
   // PRT1DM1 &= ~SDATA_PIN;
    //PRT1DM2 &= ~SDATA_PIN;
    KB_I2C_SDA_OUTPUT;
}

#ifdef RESET_MODE
// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetXRESStrong()
// Set external reset (XRES) to an output (Strong drive mode).
// ****************************************************************************
void SetXRESStrong(void)
{
    //PRT2DM0 |=  XRES_PIN;
    //PRT2DM1 &= ~XRES_PIN;
    //PRT2DM2 &= ~XRES_PIN;
}

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// AssertXRES()
// Set XRES pin High
// ****************************************************************************
void AssertXRES(void)
{
    //PRT2DR |= XRES_PIN;    
}

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// DeassertXRES()
// Set XRES pin low.
// ****************************************************************************
void DeassertXRES(void)
{
    //PRT2DR &= ~XRES_PIN;
}
#else

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetTargetVDDStrong()
// Set VDD pin (PWR) to an output (Strong drive mode).
// ****************************************************************************
void RemoveTargetVDD(void);
void SetTargetVDDStrong(void)
{
    //PRT2DM0 |=  TARGET_VDD;
    //PRT2DM1 &= ~TARGET_VDD;
   // PRT2DM2 &= ~TARGET_VDD;
   RemoveTargetVDD();
   DelayMs(1000);
}

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// ApplyTargetVDD()
// Provide power to the target PSoC's Vdd pin through a GPIO.
// ****************************************************************************
void ApplyTargetVDD(void)
{
    s_TouchKeyPowerSwitch(1);//power on touchkey
}

// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// RemoveTargetVDD()
// Remove power from the target PSoC's Vdd pin.
// ****************************************************************************
void RemoveTargetVDD(void)
{
   // PRT2DR &= ~TARGET_VDD;
   s_TouchKeyPowerSwitch(0);//power on touchkey
}
#endif

#ifdef USE_TP
// ********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// A "Test Point" sets a GPIO pin of the host processor high or low.
// This GPIO pin can be observed with an oscilloscope to verify the timing of
// key programming steps. TPs have been added in main() that set Port 0, pin 1
// high during bulk erase, during each block write and during security write.
// The timing of these programming steps should be verified as correct as part
// of the validation process of the final program.
// ****************************************************************************
void InitTP(void)
{
    //PRT0DM0 |= TP_PIN;
    //PRT0DM1 &= ~TP_PIN;
    //PRT0DM2 &= ~TP_PIN;
}
void SetTPHigh(void)
{
    //PRT0DR |= TP_PIN;
}
void SetTPLow(void)
{
    //PRT0DR &= ~TP_PIN;
}
void ToggleTP(void)
{
    //PRT0DR ^= TP_PIN;
}
#endif
#endif  //(PROJECT_REV_)
//end of file ISSP_Drive_Routines.c

