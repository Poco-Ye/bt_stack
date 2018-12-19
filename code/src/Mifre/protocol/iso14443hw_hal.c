/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : iso14443hw_hal.c
 *
 * Author : Liubo     
 * 
 * Date   : 2011-5-14
 *
 * Description:
 *     Implement the abstract layer for RF communication
 *
 * History:
 *
 * =============================================================================
 */
#include <string.h>
#include "iso14443hw_hal.h"
#include "pcd_apis.h"

extern unsigned char gucRfChipType;

/**
 * the maxium frame size index table.
 */
const unsigned short gausMaxFrameSizeTable[] = 
{
    16,  24,  32,  40,  48,  64,  96,  128, 256, 
    256, 256, 256, 256, 256, 256, 256, 256, 256,
};

/**
 * get maxium frame size
 */
unsigned short GetMaxFrameSize( int index )
{
    return gausMaxFrameSizeTable[ index ];    
}

/**
 * initialising PCD device
 * 
 * parameter:
 *            ptPcdInfo :  device information structure pointer
 *            ptChipOps :  the operations structure pointer with chip
 *            ptBsp     :  the operation structure pointer with hardware board
 * retval:
 *            0 - successfully
 *            others, error
 */
int PcdDefaultInit( struct       ST_PCDINFO *ptPcdInfo, 
             const struct ST_PCDOPS  *ptChipOps,
             const struct ST_PCDBSP  *ptBspOps )
{
    int iRev = 0;
    
    /** 
     * initialize these operations, if there are many configurations in your
     * system, then you may call this function to appdend or tell them 
     * in application interface layer.
     */
    memset( ptPcdInfo, '\0', sizeof( struct ST_PCDINFO ) );
    
    ptPcdInfo->ptPcdOps = (struct ST_PCDOPS *)ptChipOps;
    ptPcdInfo->ptBspOps = (struct ST_PCDBSP *)ptBspOps;
    
    ptPcdInfo->ucPiccType    = ISO14443_4_STANDARD_COMPLIANT_A;
    ptPcdInfo->ucProtocol    = ISO14443_TYPEA_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_SHORT_FRAME;
    ptPcdInfo->uiFwt         = 9;
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucCidEn       = 0;
    ptPcdInfo->ucNadEn       = 0;
    ptPcdInfo->uiPcdTxRNum   = 0;                   
    ptPcdInfo->uiFsc         = 32;/*default is 32 bytes*/
    ptPcdInfo->ucM1CryptoEn  = 0;
    ptPcdInfo->uiSfgt        = 0;
    
    /*call pcd initialized function*/
    ptPcdInfo->ptBspOps->RfBspInit();
    
    iRev = ptPcdInfo->ptPcdOps->PcdChipInit( ptPcdInfo );
    ptPcdInfo->ptBspOps->RfBspIntInit( ptPcdInfo->ptPcdOps->PcdChipISR ); 
	/* change end */
    
    return iRev; 
}

/**
 * open module power
 *
 * parameter:
 *            ptPcdInfo  :  device information structure pointer
 * retval:
 *            0 - successfully
 *            others, error
 */
int PcdPowerUp( struct ST_PCDINFO* ptPcdInfo )
{
    return  ptPcdInfo->ptBspOps->RfBspPowerOn( );
}

/**
 * close module power
 *
 * parameter:
 *            ptPcdInfo  :  device information structure pointer
 * retval:
 *            0 - successfully
 *            others, error
 */
int PcdPowerDown( struct ST_PCDINFO* ptPcdInfo )
{
    return ptPcdInfo->ptBspOps->RfBspPowerOff( );
}

/**
 * open carrier for radio, 13.56MHz
 *
 * parameter:
 *            ptPcdInfo  :  device information structure pointer
 * retval:
 *            0 - successfully
 *            others, error
 */
int PcdCarrierOn( struct ST_PCDINFO* ptPcdInfo )
{
    return ptPcdInfo->ptPcdOps->PcdChipOpenCarrier( ptPcdInfo );
}

/**
 * close carrier for radio, 13.56MHz
 *
 * parameter:
 *            ptPcdInfo  :  device information structure pointer
 * retval:
 *            0 - successfully
 *            others, error
 */
int PcdCarrierOff( struct ST_PCDINFO* ptPcdInfo )
{
    return ptPcdInfo->ptPcdOps->PcdChipCloseCarrier( ptPcdInfo );
}

/**
 * using chip timer delay n etu
 *
 * parameter:
 *            ptPcdInfo :  device information structure pointer
 *            iEtuCount :  the time with etu unit
 * retval:
 *            non-return
 */
void PcdWaiter( struct ST_PCDINFO* ptPcdInfo, unsigned int iEtuCount )
{
	ptPcdInfo->ptPcdOps->PcdChipWaiter( ptPcdInfo, iEtuCount );
	
    return;	
}

/**
 * read/write internal register in chip
 * 
 * parameter:
 *            ptPcdInfo :  device information structure pointer
 *            ucDir     :  0 - write / 1 - read
 *            uiRegAddr :  register address
 *            pucIOBuf  :  i/o buffer  
 *            iTRxN     :  buffer number
 * retval:
 *            non-return
 */
int PcdDeviceIO( struct ST_PCDINFO* ptPcdInfo, 
                 unsigned char     ucDir,
                 unsigned int      uiRegAddr,
                 unsigned char    *pucIOBuf, 
                 int               iTRxN )
{
    int iRev = 0;
    
    if( 0 == ucDir )
    {
        while( iTRxN-- )
        {
			if(gucRfChipType == CHIP_TYPE_RC663)
				iRev = Rc663SetRegisterConfigVal( ptPcdInfo->ucProtocol, uiRegAddr, *pucIOBuf++ );
			else if(gucRfChipType == CHIP_TYPE_PN512)
				iRev = Pn512SetRegisterConfigVal( ptPcdInfo->ucProtocol, uiRegAddr, *pucIOBuf++ );
			else if(gucRfChipType == CHIP_TYPE_AS3911)
				iRev = As3911SetRegisterConfigVal( ptPcdInfo->ucProtocol, uiRegAddr, *pucIOBuf++ );

            uiRegAddr++;
            if( iRev < 0 )break;
        }
        
        if( iRev < 0 )
        {
            ptPcdInfo->ptBspOps->RfBspWrite( uiRegAddr, pucIOBuf, iTRxN );   
        }
        
    }
    else
    {
        while( iTRxN-- )
        {
			if(gucRfChipType == CHIP_TYPE_RC663)
				iRev = Rc663GetRegisterConfigVal( ptPcdInfo->ucProtocol, uiRegAddr );
			else if(gucRfChipType == CHIP_TYPE_PN512)
				iRev = Pn512GetRegisterConfigVal( ptPcdInfo->ucProtocol, uiRegAddr );
			else if(gucRfChipType == CHIP_TYPE_AS3911)
				iRev = As3911GetRegisterConfigVal( ptPcdInfo->ucProtocol, uiRegAddr );
            uiRegAddr++;
            if( iRev < 0 )break;
            else *pucIOBuf++ = ( unsigned char )iRev;
        }
        
        if( iRev < 0 )
        {
            ptPcdInfo->ptBspOps->RfBspRead( uiRegAddr, pucIOBuf, iTRxN );
        }
        
    }
    
    return 0;
}

/**
 * start mifare authentication between PCD and PICC
 *
 * parameter:
 *            ptPcdInfo :   device information structure pointer
 *            iType     :   the type of key group
 *            iBlkNo    :   the number of block
 *            pucPwd    :   the sector password.(6 bybtes) 
 * retval:
 *            0 - successfully
 *            others, error
 */
int PcdMifareAuthenticate( struct ST_PCDINFO* ptPcdInfo, int iType, int iBlkNo, unsigned char *pucPwd )
{
    unsigned char aucAuthenCommand[12];
    
    ptPcdInfo->ptPcdOps->PcdChipWaiter( ptPcdInfo, ptPcdInfo->uiSfgt );
    
    /*configurating chip for m1 authentication*/
    ptPcdInfo->ptPcdOps->PcdChipConf( ptPcdInfo );  
    
    /*using A or B group key*/ 
    if( 'A' == iType || 'a' == iType )ptPcdInfo->aucPcdTxRBuffer[0] = 0x60;
    if( 'B' == iType || 'b' == iType )ptPcdInfo->aucPcdTxRBuffer[0] = 0x61;
    
    /*block number*/
    ptPcdInfo->aucPcdTxRBuffer[1] = iBlkNo;
    
    /*m1 uid and password*/
	
	/* change by wanls 2013.04.17 */
	if(ptPcdInfo->ucUidLen == 4)
	{
		memcpy( ptPcdInfo->aucPcdTxRBuffer + 2, ptPcdInfo->aucUid, 4 );
	}
	else if(ptPcdInfo->ucUidLen == 7)
	{
		memcpy( ptPcdInfo->aucPcdTxRBuffer + 2, &ptPcdInfo->aucUid[3], 4 );
	}
	/* add end */

    memcpy( ptPcdInfo->aucPcdTxRBuffer + 6, pucPwd, 6 );
    ptPcdInfo->uiPcdTxRNum = 12;
    
    /*start authentication*/
    return ptPcdInfo->ptPcdOps->PcdChipMifareAuthen( ptPcdInfo );
}

/**
 * start transfer in PCD device 
 *
 * parameter:
 *            ptPcdInfo    :   device information structure pointer
 *            pucWriteBuf  :   the datas buffer will be transmitted
 *            iTxN         :   the number will be transmitted
 * retval:
 *            0 - successfully
 *            others, error
 */
int PcdTrans( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucWriteBuf, int iTxN )
{
    int iRev = 0;
    
    if( !iTxN )return 0;
    
    ptPcdInfo->ptPcdOps->PcdChipWaiter( ptPcdInfo, ptPcdInfo->uiSfgt );
    
    /*configurating chip*/
    ptPcdInfo->ptPcdOps->PcdChipConf( ptPcdInfo );
    
    /*start transmission*/
    memcpy( ptPcdInfo->aucPcdTxRBuffer, pucWriteBuf, iTxN );
    ptPcdInfo->uiPcdTxRNum = iTxN;
    
    iRev = ptPcdInfo->ptPcdOps->PcdChipTrans( ptPcdInfo );/*read the datas from buffer*/
    
    return iRev;
}

/**
 * start transfer in PCD device and receiving the response from PICC
 *
 * parameter:
 *            ptPcd        :   device information structure pointer
 *            pucWriteBuf  :   the datas buffer will be transmitted
 *            iTxN         :   the number will be transmitted
 *            pucReadBuf   :   the datas buffer will be received
 *            iExN         :   the expected received number 
 * retval:
 *            0 - successfully
 *            others, error
 */
int PcdTransCeive( struct ST_PCDINFO* ptPcdInfo, 
                   unsigned char* pucWriteBuf, 
                   int iTxN, 
                   unsigned char* pucReadBuf, 
                   int iExN )
{
    int iRev = 0;

    ptPcdInfo->ptPcdOps->PcdChipWaiter( ptPcdInfo, ptPcdInfo->uiSfgt );
    
    /*configurating chip*/
    ptPcdInfo->ptPcdOps->PcdChipConf( ptPcdInfo );
    
    /*starting transmission and read the datas from buffer*/
    memcpy( ptPcdInfo->aucPcdTxRBuffer, pucWriteBuf, iTxN );
    ptPcdInfo->uiPcdTxRNum = iTxN;
    
    if( 0 == ( iRev = ptPcdInfo->ptPcdOps->PcdChipTransCeive( ptPcdInfo ) ) )
    {
        iExN = iExN > ptPcdInfo->uiPcdTxRNum ? ptPcdInfo->uiPcdTxRNum : iExN;
        memcpy( pucReadBuf, ptPcdInfo->aucPcdTxRBuffer, iExN );
    }
    
    return iRev;
}
