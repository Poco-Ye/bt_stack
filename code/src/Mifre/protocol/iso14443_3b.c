/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : iso14443_3b.c
 *
 * Author : Liubo     
 * 
 * Date   : 2011-5-28
 *
 * Description:
 *     implement these type B command communication protocol with ISO14443-3
 *
 * History:
 *
 * =============================================================================
 */
#include <stdlib.h> 
#include <string.h>

#include "iso14443hw_hal.h"
#include "iso14443_3b.h"

/**
 * PCD send REQB command to PICC, and receive the ATQB from PICC.
 * 
 * params: 
 *        ptPcdInfo   : the deivce handler
 *        pucAtqb     : the buffer for ATQB, 11 bytes, consist of PUPI,
 *                      Application Data and Protocol Info
 * retval:
 *        0 - successfully, the ATQB is valid, consist of two bytes.
 *        others, error.
 */
int ISO14443_3B_Req( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAtqb )
{
    int           iRev          = 0;
    unsigned char aucReqBCommand[3];
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucProtocol    = ISO14443_TYPEB_STANDARD;
    ptPcdInfo->uiFwt         = ISO14443_TYPEB_FWT_REQB;
    ptPcdInfo->uiSfgt        = 53;
    ptPcdInfo->uiFsc         = 32;
    ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 1;
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucEmdEn       = 0;/*switch off electric magnetic disturb detect*/
    ptPcdInfo->aucATQx[0]    = 0;
    ptPcdInfo->ucModInvert   = 1;/*invert modulation*/
    
    aucReqBCommand[0] = ISO14443_TYPEB_COMMAND_REQB;/*the anticollision Prefix byte is ( 00000101 )b*/
    aucReqBCommand[1] = 0x00;/*if is zero, all card shall process the REQB command*/
    aucReqBCommand[2] = ISO14443_TYPEB_FLAG_REQB;/*b7-b4 are RFU, the b3 indicated the REQB/WUPB, b2-b0 code the slots*/
    iRev = PcdTransCeive( ptPcdInfo, aucReqBCommand, 3, pucAtqb, 13 );
    if( iRev )return iRev;
    
    if( ( 12 != ptPcdInfo->uiPcdTxRNum ) && ( 13 != ptPcdInfo->uiPcdTxRNum ) )return ISO14443_TYPEB_ERR_NUMBER;
    if( ISO14443_TYPEB_RESP_REQB != pucAtqb[0] )return ISO14443_TYPEB_ERR_ATQB0;
    
    /*record ATQB*/
    ptPcdInfo->aucATQx[0] = ptPcdInfo->uiPcdTxRNum;
    memcpy( ptPcdInfo->aucATQx + 1, pucAtqb, ptPcdInfo->uiPcdTxRNum );
    
    /*PUPI*/
    ptPcdInfo->ucUidLen = 4;
    memcpy( ptPcdInfo->aucUid, pucAtqb + 1, 4 );    
    
    /*CID, NAD or ADC is whether supported?*/
    if( pucAtqb[11] & 0x02 )ptPcdInfo->ucNadEn = 1;
    if( pucAtqb[11] & 0x01 )ptPcdInfo->ucCidEn = 1;
    if( pucAtqb[11] & 0x04 )ptPcdInfo->ucAdc   = 1;
    
    /*frame wait time, FWT + delta(FWT)*/
    ptPcdInfo->uiFwt = 32 * ( 1 << ( ( pucAtqb[11] & 0xF0 ) >> 4 ) ) + 
                       3 * ( 1 << ( ( pucAtqb[11] & 0xF0 ) >> 4 ) ) +
                       34/*Tr0 + Tr1*/;
                
    /*IFSC*/
    ptPcdInfo->uiFsc = GetMaxFrameSize( ( pucAtqb[10] & 0xF0 ) >> 4 );

    if( 0x01 == ( pucAtqb[10] & 0x0F ) )
    {/*iso14443-4 protocol supported by PICC*/
        ptPcdInfo->ucPiccType = ISO14443_4_STANDARD_COMPLIANT_B;
    }
    else
    {
        ptPcdInfo->ucPiccType = PICC_SUPPORT_STANDARD_UNKOWN;   
    }
    
    /*the max speed supported by ptPcdInfo or PICC*/
    if( 0x00 == pucAtqb[9] )ptPcdInfo->uiPcdSupportBaudrate = ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_106000;   
    if( pucAtqb[9] & 0x10 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_212000;
    if( pucAtqb[9] & 0x20 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_424000;
    if( pucAtqb[9] & 0x40 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_848000; 
    
    if( pucAtqb[9] & 0x01 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_212000;
    if( pucAtqb[9] & 0x02 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_424000;
    if( pucAtqb[9] & 0x04 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_848000; 
    
    /*the same speed in both direction*/
    if( 0x80 == ( pucAtqb[9] & 0x80 ) )ptPcdInfo->uiPcdSupportBaudrate = ptPcdInfo->uiPiccSupportBaudrate;                       
    
    /*SFGT*/
    if( 13 == ptPcdInfo->uiPcdTxRNum )
    {
        ptPcdInfo->uiSfgt = 32 * ( 1 << ( ( pucAtqb[12] & 0xF0 ) >> 4 ) ) + 
                             3 * ( 1 << ( ( pucAtqb[12] & 0xF0 ) >> 4 ) ); 
    }
    if( ptPcdInfo->uiSfgt < 60 )ptPcdInfo->uiSfgt = 60; 
    ptPcdInfo->ucEmdEn = 1;/*switch on electric magnetic disturb detect*/
    
    return 0;
}

/**
 * PCD send WUPB command to PICC, and receive the ATQB from PICC.
 * 
 * params: 
 *        ptPcdInfo   : the deivce handler
 *        pucAtqb     : the buffer for ATQB, 11 bytes, consist of PUPI,
 *                      Application Data and Protocol Info
 * retval:
 *        0 - successfully, the ATQB is valid, consist of two bytes.
 *        others, error.
 */
int ISO14443_3B_Wup( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAtqb )
{
    int           iRev           = 0;
    unsigned char aucWupBCommand[3];
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucProtocol    = ISO14443_TYPEB_STANDARD;
    ptPcdInfo->uiFwt         = ISO14443_TYPEB_FWT_WUPB;
    ptPcdInfo->uiSfgt        = 53;
    ptPcdInfo->uiFsc         = 32;
    ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 1;
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucEmdEn       = 0;/*switch off electric magnetic disturb detect*/
    ptPcdInfo->aucATQx[0]    = 0;
    ptPcdInfo->ucModInvert   = 1;/*invert modulation*/
	ptPcdInfo->uiPcdTxRLastBits = 0;
    
    aucWupBCommand[0] = ISO14443_TYPEB_COMMAND_WUPB;/*the anticollision Prefix byte is ( 00000101 )b*/
    aucWupBCommand[1] = 0x00;/*if is zero, all card shall process the REQB command*/
    aucWupBCommand[2] = ISO14443_TYPEB_FLAG_WUPB;/*b7-b4 are RFU, the b3 indicated the REQB/WUPB, b2-b0 code the slots*/
    iRev = PcdTransCeive( ptPcdInfo, aucWupBCommand, 3, pucAtqb, 13 );
    if( iRev )return iRev;
    
    if( 12 != ptPcdInfo->uiPcdTxRNum && 13 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEB_ERR_NUMBER;
    if( ISO14443_TYPEB_RESP_REQB != pucAtqb[0] )return ISO14443_TYPEB_ERR_ATQB0;

    /*record ATQB*/
    ptPcdInfo->aucATQx[0] = ptPcdInfo->uiPcdTxRNum;
    memcpy( ptPcdInfo->aucATQx + 1, pucAtqb, ptPcdInfo->uiPcdTxRNum );
    
    /*PUPI*/
    ptPcdInfo->ucUidLen = 4;
    memcpy( ptPcdInfo->aucUid, pucAtqb + 1, 4 );   
    
    /*PICC if support the NAD or CID*/
    if( pucAtqb[11] & 0x02 )ptPcdInfo->ucNadEn = 1;
    if( pucAtqb[11] & 0x01 )ptPcdInfo->ucCidEn = 1;
    if( pucAtqb[11] & 0x04 )ptPcdInfo->ucAdc   = 1;
    
    /*frame wait time, FWT + delta(FWT)*/
    ptPcdInfo->uiFwt = 32 * ( 1 << ( ( pucAtqb[11] & 0xF0 ) >> 4 ) ) +
                       3 * ( 1 << ( ( pucAtqb[11] & 0xF0 ) >> 4 ) ) +
                       34/*TR0 + TR1.Max*/;
    
    /*IFSC*/
    ptPcdInfo->uiFsc = GetMaxFrameSize( ( pucAtqb[10] & 0xF0 ) >> 4 );
    
    if( 0x01 == ( pucAtqb[10] & 0x0F ) )
    {/*iso14443-4 protocol supported by PICC*/
        ptPcdInfo->ucPiccType = ISO14443_4_STANDARD_COMPLIANT_B;
    }
    else
    {
        ptPcdInfo->ucPiccType = PICC_SUPPORT_STANDARD_UNKOWN;   
    }
	/*by tanbx  20140303 for case TB304_14 
	protocol type字节的bit4为RFU位，必须为0，否则当错误处理*/
    if( 0x08 == ( pucAtqb[10] & 0x08 ) )	
    {
		return ISO14443_TYPEB_ERR_ATQB0;
    }
	/*add end*/
    
    /*the speed supported by ptPcdInfo or PICC*/
    if( 0x00 == pucAtqb[9])ptPcdInfo->uiPcdSupportBaudrate = ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_106000;   
    
    if( pucAtqb[9] & 0x01 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_212000;
    if( pucAtqb[9] & 0x02 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_424000;
    if( pucAtqb[9] & 0x04 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_848000; 
    
    if( pucAtqb[9] & 0x10 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_212000;
    if( pucAtqb[9] & 0x20 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_424000;
    if( pucAtqb[9] & 0x40 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_848000; 
    /*same speed in both direction*/
    if( 0x80 == ( pucAtqb[9] & 0x80 ) )ptPcdInfo->uiPcdSupportBaudrate = ptPcdInfo->uiPiccSupportBaudrate;                       
    
    /*SFGT*/
    if( 13 == ptPcdInfo->uiPcdTxRNum )
    {
        ptPcdInfo->uiSfgt = 32 * ( 1 << ( ( pucAtqb[12] & 0xF0 ) >> 4 ) ) + 
                             3 * ( 1 << ( ( pucAtqb[12] & 0xF0 ) >> 4 ) ); 
    }
    if( ptPcdInfo->uiSfgt < 60 )ptPcdInfo->uiSfgt = 60; 
    ptPcdInfo->ucEmdEn = 1;/*switch on electric magnetic disturb detect*/
    
    return 0;
}

/**
 * PCD send the Slot-Mark command to PICC, and define the time slot.
 *
 * params:
 *         ptPcdInfo : the device handler
 *         ucApn     : the slot number, format is (nnnn0101)b
 *         pucResp   : the response from PICC
 *
 */
int ISO14443_3B_SlotMarker( struct ST_PCDINFO*  ptPcdInfo, 
                            unsigned char      ucApn, 
                            unsigned char*     pucResp )
{
    return 0;   
}

/**
 * PCD send ATTRIB command to PICC, and receive the SAK from PICC.
 * 
 * params: 
 *        ptPcdInfo    : the deivce handler
 *        pucDatas     : higher layer INF.(command and response )
 *        piNum        : the number of higher layer INF.
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int ISO14443_3B_Attri( struct ST_PCDINFO* ptPcdInfo, 
                       unsigned char*    pucDatas, 
                       int *piNum )
{
    unsigned char aucAttriBCommand[9];
    unsigned char ucRch = 0;
    int           iRev  = 0;
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucProtocol    = ISO14443_TYPEB_STANDARD;
    ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 1;
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
	ptPcdInfo->aucSAK[0]     = 0;
	ptPcdInfo->ucModInvert   = 1;/*invert modulation*/
	
    /*initialiszed the PCB*/
    ptPcdInfo->ucPcdPcb  = 0x02;
    ptPcdInfo->ucPiccPcb = 0x03;
    
    aucAttriBCommand[0] = ISO14443_TYPEB_COMMAND_ATTR;
    memcpy( aucAttriBCommand + 1, ptPcdInfo->aucUid, 4 ); 
    aucAttriBCommand[5] = 0x00;
    aucAttriBCommand[6] = 0x08; /*256 IFSD*/
    aucAttriBCommand[7] = 0x01;/*protocol type, use iso14443-4*/
    if( ptPcdInfo->ucCidEn )aucAttriBCommand[8] = ptPcdInfo->ucCid & 0x0F;
    else aucAttriBCommand[8] = 0x00;
    iRev = PcdTransCeive( ptPcdInfo, aucAttriBCommand, 9, &ucRch, 1 );
    if( iRev )return iRev;
    
    /*record AttriB*/
    ptPcdInfo->aucSAK[0] = 1;
    memcpy( ptPcdInfo->aucSAK + 1, &ucRch, 1 );
    
    if( 1 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEB_ERR_NUMBER;
    if( 0x00 == ( ucRch & 0x0F ) )
    {
        ptPcdInfo->ucCidEn = 0;
    }
    else
    {
        if( ( ptPcdInfo->ucCid & 0x0F ) != ( ucRch & 0x0F ) )return ISO14443_TYPEB_ERR_CID;
    }
    
    if( 0x00 == ( ucRch & 0xF0 ) )
    {
        ptPcdInfo->uiMaxBuf = ptPcdInfo->uiFsc;
    }
    else
    {
        ptPcdInfo->uiMaxBuf = ptPcdInfo->uiFsc * ( 1 << ( ( ucRch & 0xF0 ) >> 4 ) );
    }
    PcdWaiter( ptPcdInfo, ptPcdInfo->uiSfgt );
    ptPcdInfo->uiSfgt = 60; 
    
    return 0;
}

/**
 * PCD send HALTB command to PICC.
 * 
 * params: 
 *        ptPcdInfo  : the deivce handler
 * retval:
 *        0 - successfullys.
 *        others, error.
 */
int ISO14443_3B_Halt( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char aucHaltBCommand[5];
    unsigned char ucRch  = 0xff;
    int           iRev = 0;
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;/*pcd communication speed*/
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;/*picc communication speed*/
    ptPcdInfo->ucProtocol    = ISO14443_TYPEB_STANDARD;
    ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 1;/*enable RX CRC*/
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucModInvert   = 1;/*invert modulation*/
    
    aucHaltBCommand[0] = ISO14443_TYPEB_COMMAND_HLTB;
    memcpy( aucHaltBCommand + 1, ptPcdInfo->aucUid, 4 ); 
    iRev = PcdTransCeive( ptPcdInfo, aucHaltBCommand, 5, &ucRch, 1 );
    if( iRev )return iRev;
    if( 1 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEB_ERR_NUMBER;
    if( ucRch )return ISO14443_TYPEB_ERR_HLTB;
    
    return 0;
}
