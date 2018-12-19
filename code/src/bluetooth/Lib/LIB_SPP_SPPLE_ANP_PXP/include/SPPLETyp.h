/*****< sppletyp.h >***********************************************************/
/*      Copyright 2012 Stonestreet One.                                       */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  SPPLETYP - Embedded Bluetooth SPP Emulation using GATT (LE) Types File.   */
/*                                                                            */
/*  Author:  Tim Cook                                                         */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   04/16/12  Tim Cook       Initial creation.                               */
/******************************************************************************/
#ifndef __SPPLETYP_H__
#define __SPPLETYP_H__

   /* The following MACRO is a utility MACRO that assigns the SPPLE     */
   /* Service 16 bit UUID to the specified UUID_128_t variable.  This   */
   /* MACRO accepts one parameter which is a pointer to a UUID_128_t    */
   /* variable that is to receive the SPPLE UUID Constant value.        */
   /* * NOTE * The UUID will be assigned into the UUID_128_t variable in*/
   /*          Little-Endian format.                                    */
//#define SPPLE_ASSIGN_SPPLE_SERVICE_UUID_128(_x)          ASSIGN_BLUETOOTH_UUID_128(*((UUID_128_t *)(_x)), 0x14, 0x83, 0x9A, 0xC4, 0x7D, 0x7E, 0x41, 0x5c, 0x9A, 0x42, 0x16, 0x73, 0x40, 0xCF, 0x23, 0x39)
#define SPPLE_ASSIGN_SPPLE_SERVICE_UUID_128(_x)          ASSIGN_BLUETOOTH_UUID_128(*((UUID_128_t *)(_x)), 0x49, 0x53, 0x53, 0x43, 0xFE, 0x7D, 0x4A, 0xE5, 0x8F, 0xA9, 0x9F, 0xAF, 0xD2, 0x05, 0xE4, 0x55)

   /* The following MACRO is a utility MACRO that exist to compare a    */
   /* UUID 16 to the defined SPPLE Service UUID in UUID16 form.  This   */
   /* MACRO only returns whether the UUID_128_t variable is equal to the*/
   /* SPPLE Service UUID (MACRO returns boolean result) NOT less        */
   /* than/greater than.  The first parameter is the UUID_128_t variable*/
   /* to compare to the SPPLE Service UUID.                             */
//#define SPPLE_COMPARE_SPPLE_SERVICE_UUID_TO_UUID_128(_x) COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0x14, 0x83, 0x9A, 0xC4, 0x7D, 0x7E, 0x41, 0x5c, 0x9A, 0x42, 0x16, 0x73, 0x40, 0xCF, 0x23, 0x39)
#define SPPLE_COMPARE_SPPLE_SERVICE_UUID_TO_UUID_128(_x) COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0x49, 0x53, 0x53, 0x43, 0xFE, 0x7D, 0x4A, 0xE5, 0x8F, 0xA9, 0x9F, 0xAF, 0xD2, 0x05, 0xE4, 0x55)

   /* The following defines the SPPLE Service UUID that is used when    */
   /* building the SPPLE Service Table.                                 */
//#define SPPLE_SERVICE_BLUETOOTH_UUID_CONSTANT            { 0x39, 0x23, 0xCF, 0x40, 0x73, 0x16, 0x42, 0x9A, 0x5c, 0x41, 0x7E, 0x7D, 0xC4, 0x9A, 0x83, 0x14 }
#define SPPLE_SERVICE_BLUETOOTH_UUID_CONSTANT            { 0x55, 0xE4, 0x05, 0xD2, 0xAF, 0x9F, 0xA9, 0x8F, 0xE5, 0x4A, 0x7D, 0xFE, 0x43, 0x53, 0x53, 0x49 }

   /* The following MACRO is a utility MACRO that assigns the SPPLE TX  */
   /* Characteristic 16 bit UUID to the specified UUID_128_t variable.  */
   /* This MACRO accepts one parameter which is the UUID_128_t variable */
   /* that is to receive the SPPLE TX UUID Constant value.              */
   /* * NOTE * The UUID will be assigned into the UUID_128_t variable in*/
   /*          Little-Endian format.                                    */
//#define SPPLE_ASSIGN_TX_UUID_128(_x)                    ASSIGN_BLUETOOTH_UUID_128((_x), 0x07, 0x34, 0x59, 0x4A, 0xA8, 0xE7, 0x4b, 0x1a, 0xA6, 0xB1, 0xCD, 0x52, 0x43, 0x05, 0x9A, 0x57)
#define SPPLE_ASSIGN_TX_UUID_128(_x)                    ASSIGN_BLUETOOTH_UUID_128((_x), 0x49, 0x53, 0x53, 0x43, 0x1E, 0x4D, 0x4B, 0xD9, 0xBA, 0x61, 0x23, 0xC6, 0x47, 0x24, 0x96, 0x16)
																	//49535343-1E4D-4BD9-BA61-23C647249616
   /* The following MACRO is a utility MACRO that exist to compare a    */
   /* UUID 16 to the defined SPPLE TX UUID in UUID16 form.  This MACRO  */
   /* only returns whether the UUID_128_t variable is equal to the TX   */
   /* UUID (MACRO returns boolean result) NOT less than/greater than.   */
   /* The first parameter is the UUID_128_t variable to compare to the  */
   /* SPPLE TX UUID.                                                    */
//#define SPPLE_COMPARE_SPPLE_TX_UUID_TO_UUID_128(_x)     COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0x07, 0x34, 0x59, 0x4A, 0xA8, 0xE7, 0x4b, 0x1a, 0xA6, 0xB1, 0xCD, 0x52, 0x43, 0x05, 0x9A, 0x57)
#define SPPLE_COMPARE_SPPLE_TX_UUID_TO_UUID_128(_x)     COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0x49, 0x53, 0x53, 0x43, 0x1E, 0x4D, 0x4B, 0xD9, 0xBA, 0x61, 0x23, 0xC6, 0x47, 0x24, 0x96, 0x16)

   /* The following defines the SPPLE TX Characteristic UUID that is    */
   /* used when building the SPPLE Service Table.                       */
//#define SPPLE_TX_CHARACTERISTIC_BLUETOOTH_UUID_CONSTANT { 0x57, 0x9A, 0x05, 0x43, 0x52, 0xCD, 0xB1, 0xA6, 0x1a, 0x4b, 0xE7, 0xA8, 0x4A, 0x59, 0x34, 0x07 }
#define SPPLE_TX_CHARACTERISTIC_BLUETOOTH_UUID_CONSTANT { 0x16, 0x96, 0x24, 0x47, 0xC6, 0x23, 0x61, 0xBA, 0xD9, 0x4B, 0x4D, 0x1E, 0x43, 0x53, 0x53, 0x49 }

   /* The following MACRO is a utility MACRO that assigns the SPPLE     */
   /* TX_CREDITS Characteristic 16 bit UUID to the specified UUID_128_t */
   /* variable.  This MACRO accepts one parameter which is the          */
   /* UUID_128_t variable that is to receive the SPPLE TX_CREDITS UUID  */
   /* Constant value.                                                   */
   /* * NOTE * The UUID will be assigned into the UUID_128_t variable in*/
   /*          Little-Endian format.                                    */
//#define SPPLE_ASSIGN_TX_CREDITS_UUID_128(_x)                    ASSIGN_BLUETOOTH_UUID_128((_x), 0xBA, 0x04, 0xC4, 0xB2, 0x89, 0x2B, 0x43, 0xbe, 0xB6, 0x9C, 0x5D, 0x13, 0xF2, 0x19, 0x53, 0x92)
#define SPPLE_ASSIGN_TX_CREDITS_UUID_128(_x)                    ASSIGN_BLUETOOTH_UUID_128((_x), 0x49, 0x53, 0x53, 0x43, 0x6D, 0xAA, 0x4D, 0x02, 0xAB, 0xF6, 0x19, 0x56, 0x9A, 0xCA, 0x69, 0xFE)


   /* The following MACRO is a utility MACRO that exist to compare a    */
   /* UUID 16 to the defined SPPLE TX_CREDITS UUID in UUID16 form.  This*/
   /* MACRO only returns whether the UUID_128_t variable is equal to the*/
   /* TX_CREDITS UUID (MACRO returns boolean result) NOT less           */
   /* than/greater than.  The first parameter is the UUID_128_t variable*/
   /* to compare to the SPPLE TX_CREDITS UUID.                          */
//#define SPPLE_COMPARE_SPPLE_TX_CREDITS_UUID_TO_UUID_128(_x)     COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0xBA, 0x04, 0xC4, 0xB2, 0x89, 0x2B, 0x43, 0xbe, 0xB6, 0x9C, 0x5D, 0x13, 0xF2, 0x19, 0x53, 0x92)
#define SPPLE_COMPARE_SPPLE_TX_CREDITS_UUID_TO_UUID_128(_x)     COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0x49, 0x53, 0x53, 0x43, 0x6D, 0xAA, 0x4D, 0x02, 0xAB, 0xF6, 0x19, 0x56, 0x9A, 0xCA, 0x69, 0xFE)

   /* The following defines the SPPLE TX_CREDITS Characteristic UUID    */
   /* that is used when building the SPPLE Service Table.               */
//#define SPPLE_TX_CREDITS_CHARACTERISTIC_BLUETOOTH_UUID_CONSTANT { 0x92, 0x53, 0x19, 0xF2, 0x13, 0x5D, 0x9C, 0xB6, 0xbe, 0x43, 0x2B, 0x89, 0xB2, 0xC4, 0x04, 0xBA }
#define SPPLE_TX_CREDITS_CHARACTERISTIC_BLUETOOTH_UUID_CONSTANT { 0xFE, 0x69, 0xCA, 0x9A, 0x56, 0x19, 0xF6, 0xAB, 0x02, 0x4D, 0xAA, 0x6D, 0x43, 0x53, 0x53, 0x49 }

   /* The following MACRO is a utility MACRO that assigns the SPPLE RX  */
   /* Characteristic 16 bit UUID to the specified UUID_128_t variable.  */
   /* This MACRO accepts one parameter which is the UUID_128_t variable */
   /* that is to receive the SPPLE RX UUID Constant value.              */
   /* * NOTE * The UUID will be assigned into the UUID_128_t variable in*/
   /*          Little-Endian format.                                    */
//#define SPPLE_ASSIGN_RX_UUID_128(_x)                     ASSIGN_BLUETOOTH_UUID_128((_x), 0x8B, 0x00, 0xAC, 0xE7, 0xEB, 0x0B, 0x49, 0xb0, 0xBB, 0xE9, 0x9A, 0xEE, 0x0A, 0x26, 0xE1, 0xA3)
#define SPPLE_ASSIGN_RX_UUID_128(_x)                     ASSIGN_BLUETOOTH_UUID_128((_x), 0x49, 0x53, 0x53, 0x43, 0x88, 0x41, 0x43, 0xF4, 0xA8, 0xD4, 0xEC, 0xBE, 0x34, 0x72, 0x9B, 0xB3)
																//49535343-8841-43F4-A8D4-ECBE34729BB3
   /* The following MACRO is a utility MACRO that exist to compare a    */
   /* UUID 16 to the defined SPPLE RX UUID in UUID16 form.  This MACRO  */
   /* only returns whether the UUID_128_t variable is equal to the RX   */
   /* UUID (MACRO returns boolean result) NOT less than/greater than.   */
   /* The first parameter is the UUID_128_t variable to compare to the  */
   /* SPPLE RX UUID.                                                    */
//#define SPPLE_COMPARE_SPPLE_RX_UUID_TO_UUID_128(_x)      COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0x8B, 0x00, 0xAC, 0xE7, 0xEB, 0x0B, 0x49, 0xb0, 0xBB, 0xE9, 0x9A, 0xEE, 0x0A, 0x26, 0xE1, 0xA3)
#define SPPLE_COMPARE_SPPLE_RX_UUID_TO_UUID_128(_x)      COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x),  0x49, 0x53, 0x53, 0x43, 0x88, 0x41, 0x43, 0xF4, 0xA8, 0xD4, 0xEC, 0xBE, 0x34, 0x72, 0x9B, 0xB3)

   /* The following defines the SPPLE RX Characteristic UUID that is    */
   /* used when building the SPPLE Service Table.                       */
//#define SPPLE_RX_CHARACTERISTIC_BLUETOOTH_UUID_CONSTANT  { 0xA3, 0xE1, 0x26, 0x0A, 0xEE, 0x9A, 0xE9, 0xBB, 0xb0, 0x49, 0x0B, 0xEB, 0xE7, 0xAC, 0x00, 0x8B }
#define SPPLE_RX_CHARACTERISTIC_BLUETOOTH_UUID_CONSTANT  { 0xB3, 0x9B, 0x72, 0x34, 0xBE, 0xEC, 0xD4, 0xA8, 0xF4, 0x43, 0x41, 0x88, 0x43, 0x53, 0x53, 0x49 }

   /* The following MACRO is a utility MACRO that assigns the SPPLE     */
   /* RX_CREDITS Characteristic 16 bit UUID to the specified UUID_128_t */
   /* variable.  This MACRO accepts one parameter which is the          */
   /* UUID_128_t variable that is to receive the SPPLE RX_CREDITS UUID  */
   /* Constant value.                                                   */
   /* * NOTE * The UUID will be assigned into the UUID_128_t variable in*/
   /*          Little-Endian format.                                    */
//#define SPPLE_ASSIGN_RX_CREDITS_UUID_128(_x)                    ASSIGN_BLUETOOTH_UUID_128((_x), 0xE0, 0x6D, 0x5E, 0xFB, 0x4F, 0x4A, 0x45, 0xc0, 0x9E, 0xB1, 0x37, 0x1A, 0xE5, 0xA1, 0x4A, 0xD4)
#define SPPLE_ASSIGN_RX_CREDITS_UUID_128(_x)                    ASSIGN_BLUETOOTH_UUID_128((_x), 0x49, 0x53, 0x53, 0x43, 0xAC, 0xA3, 0x48, 0x1C, 0x91, 0xEC, 0xD8, 0x5E, 0x28, 0xA6, 0x03, 0x18)

   /* The following MACRO is a utility MACRO that exist to compare a    */
   /* UUID 16 to the defined SPPLE RX_CREDITS UUID in UUID16 form.  This*/
   /* MACRO only returns whether the UUID_128_t variable is equal to the*/
   /* RX_CREDITS UUID (MACRO returns boolean result) NOT less           */
   /* than/greater than.  The first parameter is the UUID_128_t variable*/
   /* to compare to the SPPLE RX_CREDITS UUID.                          */
//#define SPPLE_COMPARE_SPPLE_RX_CREDITS_UUID_TO_UUID_128(_x)     COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0xE0, 0x6D, 0x5E, 0xFB, 0x4F, 0x4A, 0x45, 0xc0, 0x9E, 0xB1, 0x37, 0x1A, 0xE5, 0xA1, 0x4A, 0xD4)
#define SPPLE_COMPARE_SPPLE_RX_CREDITS_UUID_TO_UUID_128(_x)     COMPARE_BLUETOOTH_UUID_128_TO_CONSTANT((_x), 0x49, 0x53, 0x53, 0x43, 0xAC, 0xA3, 0x48, 0x1C, 0x91, 0xEC, 0xD8, 0x5E, 0x28, 0xA6, 0x03, 0x18)

   /* The following defines the SPPLE RX_CREDITS Characteristic UUID    */
   /* that is used when building the SPPLE Service Table.               */
//#define SPPLE_RX_CREDITS_CHARACTERISTIC_BLUETOOTH_UUID_CONSTANT { 0xD4, 0x4A, 0xA1, 0xE5, 0x1A, 0x37, 0xB1, 0x9E, 0xc0, 0x45, 0x4A, 0x4F, 0xFB, 0x5E, 0x6D, 0xE0 }
#define SPPLE_RX_CREDITS_CHARACTERISTIC_BLUETOOTH_UUID_CONSTANT { 0x18, 0x03, 0xA6, 0x28, 0x5E, 0xD8, 0xEC, 0x91, 0x1C, 0x48, 0xA3, 0xAC, 0x43, 0x53, 0x53, 0x49 }

   /* The following defines the structure that holds all of the SPPLE   */
   /* Characteristic Handles that need to be cached by a SPPLE Client.  */
typedef struct _tagSPPLE_Client_Info_t
{
   Word_t Tx_Characteristic;
   Word_t Tx_Client_Configuration_Descriptor;
   Word_t Rx_Characteristic;
   Word_t Tx_Credit_Characteristic;
   Word_t Rx_Credit_Characteristic;
   Word_t Rx_Credit_Client_Configuration_Descriptor;
} SPPLE_Client_Info_t;

#define SPPLE_CLIENT_INFO_DATA_SIZE                      (sizeof(SPPLE_Client_Info_t))

#define SPPLE_CLIENT_INFORMATION_VALID(_x)               (((_x).Tx_Characteristic) && ((_x).Tx_Client_Configuration_Descriptor) && ((_x).Rx_Characteristic) && ((_x).Tx_Credit_Characteristic) && ((_x).Rx_Credit_Characteristic) && ((_x).Rx_Credit_Client_Configuration_Descriptor))

   /* The following defines the structure that holds the information    */
   /* thatneed to be cached by a SPPLE Server for EACH paired SPPLE     */
   /* Client.                                                           */
typedef struct _tagSPPLE_Server_Info_t
{
   Word_t Tx_Client_Configuration_Descriptor;
   Word_t Rx_Credit_Client_Configuration_Descriptor;
} SPPLE_Server_Info_t;

#define SPPLE_SERVER_INFO_DATA_SIZE                      (sizeof(SPPLE_Server_Info_t))

   /* The following defines the length of the SPPLE CTS characteristic  */
   /* value.                                                            */
#define SPPLE_TX_CREDIT_VALUE_LENGTH                     (WORD_SIZE)

   /* The following defines the length of the SPPLE RTS characteristic  */
   /* value.                                                            */
#define SPPLE_RX_CREDIT_VALUE_LENGTH                     (WORD_SIZE)

   /* The following defines the length of the Client Characteristic     */
   /* Configuration Descriptor.                                         */
#define SPPLE_CLIENT_CHARACTERISTIC_CONFIGURATION_VALUE_LENGTH (WORD_SIZE)

   /* The following defines the HRS GATT Service Flags MASK that should */
   /* be passed into GATT_Register_Service when the HRS Service is      */
   /* registered.                                                       */
#define SPPLE_SERVICE_FLAGS                              (GATT_SERVICE_FLAGS_LE_SERVICE)

#endif

