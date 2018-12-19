// filename: ISSP_Extern.h
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
#ifndef INC_ISSP_EXTERN
#define INC_ISSP_EXTERN

#include "ISSP_directives.h"

extern unsigned char   fIsError;

extern void InitTargetTestData(unsigned char bBlockNum);
extern void LoadArrayWithSecurityData(unsigned char bStart, unsigned char bLength, unsigned char bType);
extern void Delay(unsigned char n);
extern void LoadProgramData(unsigned char bBlockNum);
extern signed char fLoadSecurityData(void);
extern unsigned char fSDATACheck(void);
extern void SCLKHigh(void);
extern void SCLKLow(void);
extern void SetSCLKHiZ(void);
extern void SetSCLKStrong(void);
extern void SetSDATAHigh(void);
extern void SetSDATALow(void);
extern void SetSDATAHiZ(void);
extern void SetSDATAStrong(void);
extern void SetXRESStrong(void);
extern void AssertXRES(void);
extern void DeassertXRES(void);
extern void SetTargetVDDStrong(void);
extern void ApplyTargetVDD(void);
extern void RemoveTargetVDD(void);

#ifdef USE_TP
extern void InitTP(void);
extern void SetTPHigh(void);
extern void SetTPLow(void);
extern void ToggleTP(void);
#endif

extern void RunClock(unsigned int iNumCycles);
extern unsigned char bReceiveBit(void);
extern unsigned char bReceiveByte(void);
extern void SendByte(unsigned char bCurrByte, unsigned char bSize);
extern void SendVector(const unsigned char* bVect, unsigned int iNumBits);
extern signed char fDetectHiLoTransition(void);
extern signed char fXRESInitializeTargetForISSP(void);
extern signed char fPowerCycleInitializeTargetForISSP(void);
extern signed char fVerifySiliconID(void);
extern signed char fReadStatus(void);
extern signed char fReadWriteSetup(void);
extern signed char fSyncEnable(void);
extern signed char fSyncDisable(void);
extern signed char fEraseTarget(void);
extern unsigned int iLoadTarget(void);
extern signed char fProgramTargetBlock(unsigned char bBlockNumber);
extern signed char fAccTargetBankChecksum(unsigned int* pAcc);
extern void ReStartTarget(void);
extern signed char fVerifySetup(unsigned char bBlockNumber);
extern signed char fReadByteLoop(void);
extern signed char fSecureTargetFlash(void);
extern signed char fReadSecurity(void);

#endif  //(INC_ISSP_EXTERN)
#endif  //(PROJECT_REV_)
//end of file ISSP_Extern.h
