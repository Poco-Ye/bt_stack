/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : mifare.c
 *
 * Author : Liubo     
 * 
 * Date   : 2011-5-20
 *
 * Description:
 *
 *
 * History:
 *
 * =============================================================================
 */
#include <string.h>  
#include "iso14443hw_hal.h"
#include "iso14443_3a.h"

#include "mifare.h"

/**
 * ananlysing the protocol supported by PICC according to 
 * Fig 3. in "AN 10834 Mifare ISO/IEC 14443 PICC Selection"
 *
 * params:
 *        ucSak : the picc response of selection
 * retval:
 *        0 - OK;
 *        others, need to distinguish between iso14443-4 and 
 *        "mifare pro"
 */
int MifareTypeCheck( struct ST_PCDINFO *ptPcdInfo, unsigned char ucSak )
{
   /** 
    * ananlysing the protocol supported by PICC according to Fig 3. 
    * in "AN 10834 Mifare ISO/IEC 14443 PICC Selection v3.0 20090626"
    *
    */
   if( ucSak & 0x08 )/*b4=1 ?*/
   {
        if( ucSak & 0x10 )/*b5=1 ?*/
        {
            ptPcdInfo->ucPiccType = PHILIPS_MIFARE_STANDARD_4K;
        }
        else
        {
            if( ucSak & 1 )/*b1=1 ?*/
            {
                ptPcdInfo->ucPiccType = PHILIPS_MIFARE_STANDARD_MINI;
            }
            else 
            {
                ptPcdInfo->ucPiccType = PHILIPS_MIFARE_STANDARD_1K;
            }
        }
    }
    else
    {
        if( ucSak & 0x10 )/*b5=1 ?*/
        {
            if( ucSak & 1 )/*b1=1 ?*/
            {
                ptPcdInfo->ucPiccType = PHILIPS_MIFARE_PLUS_4K;
            }
            else
            {
                ptPcdInfo->ucPiccType = PHILIPS_MIFARE_PLUS_2K; 
            }
        }
        else
        {
            if( ucSak & 0x20 )/*b6=1 ?*/
            {
                ptPcdInfo->ucPiccType = ISO14443_4_STANDARD_COMPLIANT_A;
            }
            else
            {
                ptPcdInfo->ucPiccType = PHILIPS_MIFARE_ULTRALIGHT;   
            }
        }
    }    
    
    return 0;
}

/**
 * mifare three authenticate between ptPcdInfo and PICC.
 *  
 * params:
 *         ptPcdInfo: device resource handle
 *         ucType   : the password type, 'A' or 'B'
 *         ucBlkNo  : the block number of authentication.
 *         pucPwd   : the password( 6 bytes )
 * retval:
 *         0 - successfully
 *         others, failure
 */
int MifareAuthority( struct ST_PCDINFO *ptPcdInfo,
                     unsigned char     ucType, 
                     unsigned char     ucBlkNo, 
                     unsigned char*    pucPwd )
{
    int irev  = 0;
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucProtocol    = MIFARE_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_STANDARD_FRAME;
    ptPcdInfo->ucTxCrcEnable = 1;
    ptPcdInfo->ucRxCrcEnable = 0;
    ptPcdInfo->uiFwt         = PHILIPS_MIFARE_AUTHEN_TOV;
    ptPcdInfo->ucEmdEn       = 0;
	ptPcdInfo->ucM1AuthorityBlkNo = 0;
    
    irev = PcdMifareAuthenticate( ptPcdInfo, ucType, ucBlkNo, pucPwd );
    if( 0 == irev )
	{
	    ptPcdInfo->ucM1CryptoEn  = 1;
		/* add by wanls 2012.06.04*/
		ptPcdInfo->ucM1AuthorityBlkNo = ucBlkNo;
    }
    
    return irev;
}

/**
 * mifare standard operate( increment/decrement/backup ).
 *  
 * params:
 *         ptPcdInfo    : device resource handle
 *         ucOpCode     : operation type
 *                       'w' or 'W'  - write the pval to src_blk
 *                       'r' or 'R'  - read datas from src_blk and save in pval
 *                       '+'         - added the pval to the src_blk 
 *                       '-'         - the pval substracted from the src_blk 
 *                       '>'         - the src_blk back up to the des_blk
 *                       'h' or 'H'  - halt mifare card, it don't need any parameters 
 *         ucSrcBlkNo  : operation block number
 *         pucVal      : operation value( if need )
 *         ucDesBlkNo  : operation destinct block number( if need )
 * retval:
 *         0 - successfully
 *         others, failure
 */
int MifareOperate(  struct ST_PCDINFO  *ptPcdInfo,
                    unsigned char      ucOpCode, 
                    unsigned char      ucSrcBlkNo, 
                    unsigned char*     pucVal, 
                    unsigned char      ucDesBlkNo )
{
    unsigned char aucMifareCommand[20];
    unsigned char aucMifareRxBuf[20];
    
    int           irev;
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucProtocol    = MIFARE_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_STANDARD_FRAME;
    ptPcdInfo->ucTxCrcEnable = 1;
    ptPcdInfo->ucRxCrcEnable = 0;
    ptPcdInfo->ucM1CryptoEn  = 1;
    ptPcdInfo->uiFwt         = PHILIPS_MIFARE_OPS_TOV;
    ptPcdInfo->ucEmdEn       = 0;
    
    /*send operation command*/
    switch( ucOpCode )
    {
    case 'r': case 'R':/*read operation*/
        ptPcdInfo->ucRxCrcEnable = 1;
        aucMifareCommand[0] = PHILIPS_CMD_READ_BLOCK;
    break;
    case 'w': case 'W':/*write operation*/
        aucMifareCommand[0] = PHILIPS_CMD_WRITE_BLOCK;
    break;
    case '+': /*increase operation*/
        aucMifareCommand[0] = PHILIPS_CMD_INCREASE;
    break;
    case '-': /*decrease operation*/ 
        aucMifareCommand[0] = PHILIPS_CMD_DECREASE;
    break;
    case '>': /*back up operation*/
        aucMifareCommand[0] = PHILIPS_CMD_BACKUP;
    break;
    default:
    break;
    }
    aucMifareCommand[1] = ucSrcBlkNo;
    irev = PcdTransCeive( ptPcdInfo, aucMifareCommand, 2, aucMifareRxBuf, 20 );
    if( irev )return irev;
    
    /*next step process*/
    switch( ucOpCode )
    {
    case 'r': /*read operation*/
    case 'R':
        if( 16 == ptPcdInfo->uiPcdTxRNum )
        {
            memcpy( pucVal, aucMifareRxBuf, 16 );
            return 0;
        }
        else
        {
            return PHILIPS_MIFARE_ERR_COMM;   
        }
    break;
    case 'w': /*write operation*/
    case 'W':
        if( 1 == ptPcdInfo->uiPcdTxRNum )
        {
            if( PHILIPS_RESPONSE_ACK == aucMifareRxBuf[0] )
            {
                memcpy( aucMifareCommand, pucVal, 16 );
                irev = PcdTransCeive( ptPcdInfo, aucMifareCommand, 16, aucMifareRxBuf, 20 );
                if( irev )return irev;
                if( 1 != ptPcdInfo->uiPcdTxRNum )return ISO14443_HW_ERR_COMM_FIFO;
                if( PHILIPS_RESPONSE_ACK != ( aucMifareRxBuf[0] & 0x0F ) )return PHILIPS_MIFARE_ERR_NACK;
            }
            else
            {
                return PHILIPS_MIFARE_ERR_NACK;  
            }
        }
        else
        {
            return ISO14443_HW_ERR_COMM_FIFO;   
        }
    break;
    case '+': /*increase operation*/
    case '-': /*decrease operation*/ 
    case '>': /*back up operation*/
        if( 1 == ptPcdInfo->uiPcdTxRNum )
        {
            if( PHILIPS_RESPONSE_ACK == aucMifareRxBuf[0] )
            {
                memcpy( aucMifareCommand, pucVal, 4 );
                irev = PcdTrans( ptPcdInfo, aucMifareCommand, 4 );              
                if( irev )return irev;

                /*send transfer command to mifare card*/
                aucMifareCommand[0] = PHILIPS_CMD_TRANSFER;
                aucMifareCommand[1] = ucDesBlkNo;
                irev = PcdTransCeive( ptPcdInfo, aucMifareCommand, 2, aucMifareRxBuf, 20 );
                if( irev )return irev;
                if( 1 != ptPcdInfo->uiPcdTxRNum )return ISO14443_HW_ERR_COMM_FIFO;
                if( PHILIPS_RESPONSE_ACK != ( aucMifareRxBuf[0] & 0x0F ) )return PHILIPS_MIFARE_ERR_NACK;
            }
            else
            {
                return PHILIPS_MIFARE_ERR_NACK;  
            }
        }
        else
        {
            return ISO14443_HW_ERR_COMM_FIFO;   
        }
    break;
    default:
    break;
    }
    
    return 0;
}
