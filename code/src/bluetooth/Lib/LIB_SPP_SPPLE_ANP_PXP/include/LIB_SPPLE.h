#ifndef __SPPLE_H__
#define __SPPLE_H__

#include "SS1BTPS.h"             /* Main SS1 Bluetooth Stack Header.          */
#include "BTPSKRNL.h"            /* BTPS Kernel Prototypes/Constants.         */

typedef struct _tagSPPLE_Data_Indication_Event_Data_t
{
   BD_ADDR_t     ConnectionBD_ADDR;
   Word_t        DataLength;
} SPPLE_Data_Indication_Event_Data_t;


void SetBLESniffConnectionParameters(Word_t Connection_Interval_Min, Word_t Connection_Interval_Max, Word_t Slave_Latency, Word_t Supervision_Timeout);

int SPPLEProfileRegister(void);

int SPPLEProfileUnRegister(void);


int SPPLEConfigure(char *ConnectedADDR);

int SPPLEDataRead(unsigned int BufferLength, Byte_t *Buffer);

int SPPLEDataWrite(unsigned int DataLength, Byte_t *Data,char *ConnectedADDR);

#endif

