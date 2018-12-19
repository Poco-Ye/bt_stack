// filename: ISSP_Vectors.h
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
#ifndef INC_ISSP_VECTORS
#define INC_ISSP_VECTORS

#include "ISSP_directives.h"
 
// ------------------------- PSoC CY8C20xx6 Devices ---------------------------

#ifdef CY8C20066_24LTXI
    unsigned char target_id_v[] = {0x00, 0x9A};     //ID for CY8C20066_24LTXI
#endif
#ifdef CY8C20236_24LKXI
    unsigned char target_id_v[] = {0x00, 0xB3};     //ID for CY8C20236_24LKXI
#endif
#ifdef CY8C20246_24LKXI
    unsigned char target_id_v[] = {0x00, 0xAA};     //ID for CY8C20246_24LKXI
#endif
#ifdef CY8C20266_24LKXI
    unsigned char target_id_v[] = {0x00, 0x96};     //ID for CY8C20266_24LKXI
#endif
#ifdef CY8C20296_24LKXI
    unsigned char target_id_v[] = {0x00, 0xBC};     //ID for CY8C20296_24LKXI
#endif
#ifdef CY8C20336_24LQXI
    unsigned char target_id_v[] = {0x00, 0xB4};     //ID for CY8C20336_24LQXI
#endif
#ifdef CY8C20346_24LQXI
    unsigned char target_id_v[] = {0x00, 0xAC};     //ID for CY8C20346_24LQXI
#endif
#ifdef CY8C20366_24LQXI
    unsigned char target_id_v[] = {0x00, 0x97};     //ID for CY8C20366_24LQXI
#endif
#ifdef CY8C20396_24LQXI
    unsigned char target_id_v[] = {0x00, 0xAF};     //ID for CY8C20396_24LQXI
#endif
#ifdef CY8C20436_24LQXI
    unsigned char target_id_v[] = {0x00, 0xB5};     //ID for CY8C20436_24LQXI
#endif
#ifdef CY8C20446_24LQXI
    unsigned char target_id_v[] = {0x00, 0xAD};     //ID for CY8C20446_24LQXI
#endif
#ifdef CY8C20466_24LQXI
    unsigned char target_id_v[] = {0x00, 0x98};     //ID for CY8C20466_24LQXI
#endif
#ifdef CY8C20496_24LQXI
    unsigned char target_id_v[] = {0x00, 0xBD};     //ID for CY8C20496_24LQXI
#endif
#ifdef CY8C20536_24PVXI
    unsigned char target_id_v[] = {0x00, 0xB9};     //ID for CY8C20536_24PVXI
#endif
#ifdef CY8C20546_24PVXI
    unsigned char target_id_v[] = {0x00, 0xAE};     //ID for CY8C20546_24PVXI
#endif
#ifdef CY8C20566_24PVXI
    unsigned char target_id_v[] = {0x00, 0x99};     //ID for CY8C20566_24PVXI
#endif
#ifdef CY8C20636_24LTXI
    unsigned char target_id_v[] = {0x00, 0xBA};     //ID for CY8C20636_24LTXI
#endif
#ifdef CY8C20646_24LTXI
    unsigned char target_id_v[] = {0x00, 0xB8};     //ID for CY8C20646_24LTXI
#endif
#ifdef CY8C20666_24LTXI
    unsigned char target_id_v[] = {0x00, 0x9C};     //ID for CY8C20666_24LTXI
#endif
#ifdef CY8C20746A_24FDXC
    unsigned char target_id_v[] = {0x00, 0xBE};     //ID for CY8C20746A_24FDXC 
#endif
#ifdef CY8C20766A_24FDXC
    unsigned char target_id_v[] = {0x00, 0xBF};     //ID for CY8C20766A_24FDXC 
#endif


//CY8C20xx6AS parts
#ifdef CY8C20246AS_24LKXI
    unsigned char target_id_v[] = {0x0B, 0xAA};     //ID for CY8C20246AS_24LKXI
#endif
#ifdef CY8C20346AS_24LQXI
    unsigned char target_id_v[] = {0x0B, 0xAC};     //ID for CY8C20346AS_24LQXI
#endif
#ifdef CY8C20446AS_24LQXI
    unsigned char target_id_v[] = {0x0B, 0xAD};     //ID for CY8C20446AS_24LQXI
#endif
#ifdef CY8C20466AS_24LQXIT
    unsigned char target_id_v[] = {0x0B, 0x98};     //ID for CY8C20466AS_24LQXIT
#endif
#ifdef CY8C20646AS_24LTXI
    unsigned char target_id_v[] = {0x0B, 0xB8};     //ID for CY8C20646AS_24LTXI
#endif
#ifdef CY8C20666AS_24LTXI
    unsigned char target_id_v[] = {0x0B, 0x9C};     //ID for CY8C20666AS_24LTXI 
#endif

//CY8C20xx6L parts
#ifdef CY8C20446L_24LQXI
    unsigned char target_id_v[] = {0x03, 0xAD};     //ID for CY8C20446L_24LQXI 
#endif
#ifdef CY8C20466L_24LQXI
    unsigned char target_id_v[] = {0x03, 0x98};     //ID for CY8C20466L_24LQXI
#endif
#ifdef CY8C20496L_24LQXI
    unsigned char target_id_v[] = {0x03, 0xBD};     //ID for CY8C20496L_24LQXI 
#endif
#ifdef CY8C20546L_24PVXI
    unsigned char target_id_v[] = {0x03, 0xAE};     //ID for CY8C20546L_24PVXI
#endif
#ifdef CY8C20566L_24PVXI
    unsigned char target_id_v[] = {0x03, 0x99};     //ID for CY8C20566L_24PVXI 
#endif
#ifdef CY8C20646L_24LTXI
    unsigned char target_id_v[] = {0x03, 0xB8};     //ID for CY8C20646L_24LTXI 
#endif
#ifdef CY8C20666L_24LTXI
    unsigned char target_id_v[] = {0x03, 0x9C};     //ID for  CY8C20666L_24LTXI
#endif


//CY8C20xx7 parts
#ifdef CY8C20237_24SXI
    unsigned char target_id_v[] = {0x01, 0x40};     //ID for CY8C20237_24SXI
#endif
#ifdef CY8C20247_24SXI
    unsigned char target_id_v[] = {0x01, 0x41};     //ID for CY8C20247_24SXI
#endif
#ifdef CY8C20237_24LKXI
    unsigned char target_id_v[] = {0x01, 0x42};     //ID for CY8C20237_24LKXI 
#endif
#ifdef CY8C20247_24LKXI
    unsigned char target_id_v[] = {0x01, 0x43};     //ID for CY8C20247_24LKXI
#endif
#ifdef CY8C20247S_24LKXI
    unsigned char target_id_v[] = {0x0B, 0x43};     //ID for CY8C20247S_24LKXI 
#endif
#ifdef CY8C20337AN_24LQXI
    unsigned char target_id_v[] = {0x01, 0x50};     //ID for CY8C20337AN_24LQXI 
#endif
#ifdef CY8C20337_24LQXI
    unsigned char target_id_v[] = {0x01, 0x44};     //ID for  CY8C20337_24LQXI
#endif
#ifdef CY8C20337H_24LQXI
    unsigned char target_id_v[] = {0x0C, 0x44};     //ID for CY8C20337H_24LQXI 
#endif
#ifdef CY8C20347_24LQXI
    unsigned char target_id_v[] = {0x01, 0x45};     //ID for CY8C20347_24LQXI
#endif
#ifdef CY8C20347S_24LQXI
    unsigned char target_id_v[] = {0x0B, 0x45};     //ID for CY8C20347S_24LQXI 
#endif
#ifdef CY8C20437AN_24LQXI
    unsigned char target_id_v[] = {0x01, 0x51};     //ID for CY8C20437AN_24LQXI
#endif
#ifdef CY8C20437_24LQXI
    unsigned char target_id_v[] = {0x01, 0x46};     //ID for CY8C20437_24LQXI 
#endif
#ifdef CY8C20447_24LQXI
    unsigned char target_id_v[] = {0x01, 0x47};     //ID for CY8C20447_24LQXI 
#endif
#ifdef CY8C20447S_24LQXI
    unsigned char target_id_v[] = {0x0B, 0x47};     //ID for  CY8C20447S_24LQXI
#endif
#ifdef CY8C20447H_24LQXI
    unsigned char target_id_v[] = {0x0C, 0x47};     //ID for CY8C20447H_24LQXI 
#endif
#ifdef CY8C20447L_24LQXI
    unsigned char target_id_v[] = {0x03, 0x47};     //ID for CY8C20447L_24LQXI
#endif
#ifdef CY8C20467_24LQXI
    unsigned char target_id_v[] = {0x01, 0x48};     //ID for CY8C20467_24LQXI 
#endif
#ifdef CY8C20467S_24LQXI
    unsigned char target_id_v[] = {0x0B, 0x48};     //ID for CY8C20467S_24LQXI 
#endif
#ifdef CY8C20467L_24LQXI
    unsigned char target_id_v[] = {0x03, 0x48};     //ID for  CY8C20467L_24LQXI
#endif
#ifdef CY8C20637AN_LQXI
    unsigned char target_id_v[] = {0x01, 0x4F};     //ID for CY8C20637AN_LQXI 
#endif
#ifdef CY8C20637_24LQXI
    unsigned char target_id_v[] = {0x01, 0x49};     //ID for CY8C20637_24LQXI
#endif
#ifdef CY8C20647_24LQXI
    unsigned char target_id_v[] = {0x01, 0x4A};     //ID for CY8C20647_24LQXI 
#endif
#ifdef CY8C20647S_24LQXI
    unsigned char target_id_v[] = {0x0B, 0x4A};     //ID for CY8C20647S_24LQXI 
#endif
#ifdef CY8C20647L_24LQXI
    unsigned char target_id_v[] = {0x03, 0x4A};     //ID for  CY8C20647L_24LQXI
#endif
#ifdef CY8C20667_24LQXI
    unsigned char target_id_v[] = {0x01, 0x4B};     //ID for CY8C20667_24LQXI
#endif
#ifdef CY8C20667S_24LQXI
    unsigned char target_id_v[] = {0x0B, 0x4B};     //ID for  CY8C20667S_24LQXI
#endif
#ifdef CY8C20667L_24LQXI
    unsigned char target_id_v[] = {0x03, 0x4B};     //ID for  CY8C20667L_24LQXI
#endif
#ifdef CY8C20747_24FDXCI
    unsigned char target_id_v[] = {0x01, 0x4C};     //ID for  CY8C20747_24FDXCI
#endif
#ifdef CY8C20767_24FDXCI
    unsigned char target_id_v[] = {0x01, 0x4D};     //ID for  CY8C20767_24FDXCI
#endif




// Status return codes (error return codes)
// ----------------------------------------------------------------------------
unsigned char target_status00_v = 0x00;		// Status = 00 means Success, the SROM function did what it was supposed to 
unsigned char target_status01_v = 0x01;		// Status = 01 means that function is not allowed because of block level protection, for test with verify_setup (VERIFY-SETUP)
unsigned char target_status03_v = 0x03;		// Status = 03 is fatal error, SROM halted
unsigned char target_status04_v = 0x04;		// Status = 04 means that ___ for test with ___ (PROGRAM-AND-VERIFY) 
unsigned char target_status06_v = 0x06;		// Status = 06 means that Calibrate1 failed, for test with id_setup_1 (ID-SETUP-1)

// ---------------------------- CY8C20x66 Vectors ----------------------------
// Modifying these tables is NOT recommendended. Doing so will all but
// guarantee an ISSP error, unless updated vectors have been recommended or
// provided by Cypress Semiconductor.
// ----------------------------------------------------------------------------

// --------- ID-SETUP-1
const unsigned int num_bits_id_setup_1 = 594;		
const unsigned char id_setup_1[] =
{			
	// Vector in Hex format
	0xCA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0D, 0xEE, 0x21, 0xF7, 0xF0, 0x27, 0xDC, 0x40, 
	0x9F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xE7, 0xC1, 
	0xD7, 0x9F, 0x20, 0x7E, 0x7D, 0x88, 0x7D, 0xEE, 
	0x21, 0xF7, 0xF0, 0x07, 0xDC, 0x40, 0x1F, 0x70, 
	0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0, 0x1F, 0xDE, 
	0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0, 0x13, 0xF7, 
	0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D, 0x18, 0x7D, 
	0xFE, 0x25, 0x80 

	// Vector in Binary format
//	0b11001010, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 
//	0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 
//	0b00001101, 0b11101110, 0b00100001, 0b11110111, 0b11110000, 0b00100111, 0b11011100, 0b01000000, 
//	0b10011111, 0b01110000, 0b00000001, 0b11111101, 0b11101110, 0b00100001, 0b11100111, 0b11000001, 
//	0b11010111, 0b10011111, 0b00100000, 0b01111110, 0b01111101, 0b10001000, 0b01111101, 0b11101110, 
//	0b00100001, 0b11110111, 0b11110000, 0b00000111, 0b11011100, 0b01000000, 0b00011111, 0b01110000, 
//	0b00000001, 0b11111101, 0b11101110, 0b00000001, 0b11110111, 0b10100000, 0b00011111, 0b11011110, 
//	0b10100000, 0b00011111, 0b01111011, 0b00000000, 0b01111101, 0b11100000, 0b00010011, 0b11110111, 
//	0b11000000, 0b00000111, 0b11011111, 0b00101000, 0b00011111, 0b01111101, 0b00011000, 0b01111101, 
//	0b11111110, 0b00100101, 0b10000000
};

// --------- ID-SETUP-2
const unsigned int num_bits_id_setup_2 = 418; 		
const unsigned char id_setup_2[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09, 
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81, 
	0xF9, 0xF4, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40, 
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0, 
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0, 
	0x0D, 0xF7, 0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D, 
	0x18, 0x7D, 0xFE, 0x25, 0x80

	// Vector in Binary format
//	0b11011110, 0b11100010, 0b00011111, 0b01111111, 0b00000010, 0b01111101, 0b11000100, 0b00001001, 
//	0b11110111, 0b00000000, 0b00011111, 0b10011111, 0b00000111, 0b01011110, 0b01111100, 0b10000001, 
//	0b11111001, 0b11110100, 0b00000001, 0b11110111, 0b11110000, 0b00000111, 0b11011100, 0b01000000, 
//	0b00011111, 0b01110000, 0b00000001, 0b11111101, 0b11101110, 0b00000001, 0b11110111, 0b10100000, 
//	0b00011111, 0b11011110, 0b10100000, 0b00011111, 0b01111011, 0b00000000, 0b01111101, 0b11100000, 
//	0b00001101, 0b11110111, 0b11000000, 0b00000111, 0b11011111, 0b00101000, 0b00011111, 0b01111101, 
//	0b00011000, 0b01111101, 0b11111110, 0b00100101, 0b10000000			
};

// --------- SET-BLOCK-NUM
// *NOTE: This vector is broken up to accomodate the variable for the block number
const unsigned int num_bits_set_block_num = 33;		
const unsigned char set_block_num[] =
{
	// Vector in Hex format
	0xDE, 0xE0, 0x1E, 0x7D, 0x00, 0x70

	// Vector in Binary format
//	0b11011110, 0b11100000, 0b00011110, 0b01111101, 0b00000000, 0b01110000
};
const unsigned int num_bits_set_block_num_end = 3;		
const unsigned char set_block_num_end = 0xE0;

// --------- CHECKSUM-SETUP
const unsigned int num_bits_checksum_setup = 418;		//Checksum with TSYNC Enable and Disable
const unsigned char checksum_setup[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09, 
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81, 
	0xF9, 0xF4, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40, 
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0, 
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0, 
	0x0F, 0xF7, 0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D, 
	0x18, 0x7D, 0xFE, 0x25, 0x80 

	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01111111, 0b00000010, 0b01111101, 0b11000100, 0b00001001,
//	0b11110111, 0b00000000, 0b00011111, 0b10011111, 0b00000111, 0b01011110, 0b01111100, 0b10000001, 
//	0b11111001, 0b11110100, 0b00000001, 0b11110111, 0b11110000, 0b00000111, 0b11011100, 0b01000000, 
//	0b00011111, 0b01110000, 0b00000001, 0b11111101, 0b11101110, 0b00000001, 0b11110111, 0b10100000,
//	0b00011111, 0b11011110, 0b10100000, 0b00011111, 0b01111011, 0b00000000, 0b01111101, 0b11100000, 
//	0b00001111, 0b11110111, 0b11000000, 0b00000111, 0b11011111, 0b00101000, 0b00011111, 0b01111101, 
//	0b00011000, 0b01111101, 0b11111110, 0b00100101, 0b10000000		
};

// --------- READ-CHECKSUM
const unsigned char read_checksum_v[] = 
{  
	// Vector in Hex format
    0xBF, 0x20, 0xDF, 0x80, 0x80
                
	// Vector in Binary format	
//  0b10111111, 0b00100000,0b11011111,0b10000000,0b10000000 
};

// --------- PROGRAM-AND_VERIFY
const unsigned int num_bits_program_and_verify = 440;		
const unsigned char program_and_verify[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09, 
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81, 
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40, 
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA0, 
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC, 
	0x01, 0xF7, 0x80, 0x57, 0xDF, 0x00, 0x1F, 0x7C, 
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96 

	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01111111, 0b00000010, 0b01111101, 0b11000100, 0b00001001, 
//	0b11110111, 0b00000000, 0b00011111, 0b10011111, 0b00000111, 0b01011110, 0b01111100, 0b10000001, 
//	0b11111001, 0b11110111, 0b00000001, 0b11110111, 0b11110000, 0b00000111, 0b11011100, 0b01000000, 
//	0b00011111, 0b01110000, 0b00000001, 0b11111101, 0b11101110, 0b00000001, 0b11110110, 0b10100000, 
//	0b00001111, 0b11011110, 0b10000000, 0b01111111, 0b01111010, 0b10000000, 0b01111101, 0b11101100, 
//	0b00000001, 0b11110111, 0b10000000, 0b01010111, 0b11011111, 0b00000000, 0b00011111, 0b01111100, 
//	0b10100000, 0b01111101, 0b11110100, 0b01100001, 0b11110111, 0b11111000, 0b10010110
};

// --------- ERASE
const unsigned int num_bits_erase = 396;		
const unsigned char erase[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09, 
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x85, 
	0xFD, 0xFC, 0x01, 0xF7, 0x10, 0x07, 0xDC, 0x00, 
	0x7F, 0x7B, 0x80, 0x7D, 0xE0, 0x0B, 0xF7, 0xA0, 
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x04, 0x7D, 0xF0, 
	0x01, 0xF7, 0xC9, 0x87, 0xDF, 0x48, 0x1F, 0x7F, 
	0x89, 0x60 

	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01111111, 0b00000010, 0b01111101, 0b11000100, 0b00001001,
//	0b11110111, 0b00000000, 0b00011111, 0b10011111, 0b00000111, 0b01011110, 0b01111100, 0b10000101, 
//	0b11111101, 0b11111100, 0b00000001, 0b11110111, 0b00010000, 0b00000111, 0b11011100, 0b00000000, 
//	0b01111111, 0b01111011, 0b10000000, 0b01111101, 0b11100000, 0b00001011, 0b11110111, 0b10100000, 
//	0b00011111, 0b11011110, 0b10100000, 0b00011111, 0b01111011, 0b00000100, 0b01111101, 0b11110000, 
//	0b00000001, 0b11110111, 0b11001001, 0b10000111, 0b11011111, 0b01001000, 0b00011111, 0b01111111, 
//	0b10001001, 0b01100000
};

// --------- SECURE
const unsigned int num_bits_secure = 440;		
const unsigned char secure[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09, 
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81, 
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40, 
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA0, 
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC, 
	0x01, 0xF7, 0x80, 0x27, 0xDF, 0x00, 0x1F, 0x7C, 
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96
	
	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01111111, 0b00000010, 0b01111101, 0b11000100, 0b00001001, 
//	0b11110111, 0b00000000, 0b00011111, 0b10011111, 0b00000111, 0b01011110, 0b01111100, 0b10000001, 
//	0b11111001, 0b11110111, 0b00000001, 0b11110111, 0b11110000, 0b00000111, 0b11011100, 0b01000000, 
//	0b00011111, 0b01110000, 0b00000001, 0b11111101, 0b11101110, 0b00000001, 0b11110110, 0b10100000, 
//	0b00001111, 0b11011110, 0b10000000, 0b01111111, 0b01111010, 0b10000000, 0b01111101, 0b11101100, 
//	0b00000001, 0b11110111, 0b10000000, 0b00100111, 0b11011111, 0b00000000, 0b00011111, 0b01111100, 
//	0b10100000, 0b01111101, 0b11110100, 0b01100001, 0b11110111, 0b11111000, 0b10010110
};

// --------- READ-SECURITY-SETUP
const unsigned int num_bits_read_security_setup = 88;		
const unsigned char read_security_setup[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x60, 0x88, 0x7D, 0x84, 0x21, 
	0xF7, 0xB8, 0x07

	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01100000, 0b10001000, 0b01111101, 0b10000100, 0b00100001,
//	0b11110111, 0b10111000, 0b00000111
};

// --------- READ-SECURITY-1
// *NOTE: This vector is broken up to accomodate the variable for the address
const unsigned int num_bits_read_security_pt1 = 78;		
const unsigned char read_security_pt1[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x72, 0x87, 0x7D, 0xCA, 0x01, 
	0xF7, 0x28 

	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01110010, 0b10000111, 0b01111101, 0b11001010, 0b00000001,
//	0b11110111, 0b00101000
};

const unsigned int num_bits_read_security_pt1_end = 25;		
const unsigned char read_security_pt1_end[] =
{
	// Vector in Hex format
	0xFB, 0x94, 0x03, 0x80 

	// Vector in Binary format	
//	0b11111011, 0b10010100, 0b00000011, 0b10000000
};

// --------- READ-SECURITY-2
const unsigned int num_bits_read_security_pt2 = 198;		
const unsigned char read_security_pt2[] =
{
	// Vector in Hex format
	0xDE, 0xE0, 0x1F, 0x7A, 0x01, 0xFD, 0xEA, 0x01, 
	0xF7, 0xB0, 0x07, 0xDF, 0x0B, 0xBF, 0x7C, 0xF2, 
	0xFD, 0xF4, 0x61, 0xF7, 0xB8, 0x87, 0xDF, 0xE2, 
	0x58
	
	// Vector in Binary format	
//	0b11011110, 0b11100000, 0b00011111, 0b01111010, 0b00000001, 0b11111101, 0b11101010, 0b00000001, 
//	0b11110111, 0b10110000, 0b00000111, 0b11011111, 0b00001011, 0b10111111, 0b01111100, 0b11110010, 
//	0b11111101, 0b11110100, 0b01100001, 0b11110111, 0b10111000, 0b10000111, 0b11011111, 0b11100010, 
//	0b01011000
};

// --------- READ-SECURITY-3
// *NOTE: This vector is broken up to accomodate the variable for the address
const unsigned int num_bits_read_security_pt3 = 122;		
const unsigned char read_security_pt3[] =
{
	// Vector in Hex format
	0xDE, 0xE0, 0x1F, 0x7A, 0x01, 0xFD, 0xEA, 0x01, 
	0xF7, 0xB0, 0x07, 0xDF, 0x0A, 0x7F, 0x7C, 0xC0

	// Vector in Binary format	
//	0b11011110, 0b11100000, 0b00011111, 0b01111010, 0b00000001, 0b11111101, 0b11101010, 0b00000001, 
//	0b11110111, 0b10110000, 0b00000111, 0b11011111, 0b00001010, 0b01111111, 0b01111100, 0b11000000
};

const unsigned int num_bits_read_security_pt3_end = 47;		
const unsigned char read_security_pt3_end[] =
{
	// Vector in Hex format
	0xFB, 0xE8, 0xC3, 0xEF, 0xF1, 0x2C 

	// Vector in Binary format	
//	0b11111011, 0b11101000, 0b11000011, 0b11101111, 0b11110001, 0b00101100
};

// --------- READ-WRITE-SETUP
const unsigned int num_bits_read_write_setup = 66;		
const unsigned char read_write_setup[] =
{
	// Vector in Hex format
	0xDE, 0xF0, 0x1F, 0x78, 0x00, 0x7D, 0xA0, 0x03, 
	0xC0 

	// Vector in Binary format	
//	0b11011110, 0b11110000, 0b00011111, 0b01111000, 0b00000000, 0b01111101, 0b10100000, 0b00000011,
//	0b11000000
};

// --------- WRITE-BYTE
// *NOTE: This vector is broken up to accomodate the variable for the address
const unsigned char    write_byte_start = 0x90;			
const unsigned char    write_byte_end = 0xE0;

// --------- READ-ID-WORD (READ-STATUS)
// *NOTE: This vector is generic and not explicit to a particular part #
// also, this vector is the basis for READ-STATUS  vector
const unsigned char read_id_v[] =
{
	// Vector in Hex format
	0xBF, 0x00, 0xDF, 0x90, 0x00, 0xFE, 0x60, 0xFF, 0x00

	// Vector in Binary format	
//	0b10111111,0b00000000,0b11011111,0b10010000,0b00000000,0b11111110,0b0110000,0b11111111,0b00000000
};

// --------- VERIFY-SETUP
const unsigned int num_bits_my_verify_setup = 440;
const unsigned char verify_setup[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09, 
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81, 
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40, 
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA8, 
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC, 
	0x01, 0xF7, 0x80, 0x0F, 0xDF, 0x00, 0x1F, 0x7C, 
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96 

	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01111111, 0b00000010, 0b01111101, 0b11000100, 0b00001001, 
//	0b11110111, 0b00000000, 0b00011111, 0b10011111, 0b00000111, 0b01011110, 0b01111100, 0b10000001, 
//	0b11111001, 0b11110111, 0b00000001, 0b11110111, 0b11110000, 0b00000111, 0b11011100, 0b01000000, 
//	0b00011111, 0b01110000, 0b00000001, 0b11111101, 0b11101110, 0b00000001, 0b11110110, 0b10101000, 
//	0b00001111, 0b11011110, 0b10000000, 0b01111111, 0b01111010, 0b10000000, 0b01111101, 0b11101100, 
//	0b00000001, 0b11110111, 0b10000000, 0b00001111, 0b11011111, 0b00000000, 0b00011111, 0b01111100, 
//	0b10100000, 0b01111101, 0b11110100, 0b01100001, 0b11110111, 0b11111000, 0b10010110 			
};

// --------- READ-BYTE
const unsigned char read_byte_v[] = 
{  
	// Vector in Hex format
    0xB0, 0x80 
               
	// Vector in Binary format	
//  0b10110000, 0b10000000 
};

// --------- SYNC-ENABLE
const unsigned int num_bits_tsync_enable = 110;
const unsigned char tsync_enable[] =
{
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09, 
	0xF7, 0x00, 0x1F, 0xDE, 0xE0, 0x1C 

	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01111111, 0b00000010, 0b01111101, 0b11000100, 0b00001001,
//	0b11110111, 0b00000000, 0b00011111, 0b11011110, 0b11100000, 0b00011100
};

// --------- SYNC-DISABLE
const unsigned int num_bits_tsync_disable = 110;
const unsigned char tsync_disable[] =
{			
	// Vector in Hex format
	0xDE, 0xE2, 0x1F, 0x71, 0x00, 0x7D, 0xFC, 0x01, 
	0xF7, 0x00, 0x1F, 0xDE, 0xE0, 0x1C

	// Vector in Binary format	
//	0b11011110, 0b11100010, 0b00011111, 0b01110001, 0b00000000, 0b01111101, 0b11111100, 0b00000001, 
//	0b11110111, 0b00000000, 0b00011111, 0b11011110, 0b11100000, 0b00011100
};

const unsigned char    num_bits_wait_and_poll_end = 40;
const unsigned char    wait_and_poll_end[] = 
{  
    0x00, 0x00, 0x00, 0x00, 0x00 
};    // forty '0's per the spec
#endif  //(INC_ISSP_VECTORS)
#endif  //(PROJECT_REV_)
//end of file ISSP_Vectors.h
