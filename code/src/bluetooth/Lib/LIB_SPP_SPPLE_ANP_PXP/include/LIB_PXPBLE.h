#ifndef __PXPBLE_H__
#define __PXPBLE_H__

#include "SS1BTPS.h"             /* Main SS1 Bluetooth Stack Header.          */
#include "BTPSKRNL.h"            /* BTPS Kernel Prototypes/Constants.         */


typedef struct _tagPXP_BLE_Get_Alert_Level_Data_t
{
   Byte_t Alert_Level;
} PXP_BLE_Get_Alert_Level_Data_t;


typedef struct _tagPXP_BLE_Get_Tx_Power_Level_Data_t
{
   SByte_t Tx_Power_Level;
}PXP_BLE_Get_Tx_Power_Level_Data_t;


typedef struct _tagPXP_BLE_Alert_Level_Update_Data_t
{
   Byte_t Alert_Level;
}PXP_BLE_Alert_Level_Update_Data_t;


int PXPRegisterProfileLLS(void);
int PXPUnregisterProfileLLS(void);
int PXPDiscoverLLS(void);
int PXPRegisterProfileIAS(void);
int PXPUnregisterProfileIAS(void);
int PXPDiscoverIAS(void);
int PXPRegisterProfileTPS(void);
int PXPUnregisterProfileTPS(void);
int PXPDiscoverTPS(void);


int PXPGetAlertLevel(void);
int PxPSetAlertLevel(Byte_t AlertLevel);
int PXPConfigureAlertLevel(int Input);
int PXPGetTxPowerLevel(void);
int PXPSetTxPowerLevel(int Tx_Power_Level);








#endif

