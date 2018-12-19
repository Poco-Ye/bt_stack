/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : iso14443_3a.c
 *
 * Author : Liubo     
 * 
 * Date   : 2011-5-20
 *
 * Description:
 *    Implement these type A command communication protocol with ISO/IEC 14443-3
 * and type A activation protocol with ISO/IEC 14443-4.
 *
 * History:
 * 
 * =============================================================================
 */
#include <stdlib.h>
#include <string.h>

#include "iso14443hw_hal.h"
#include "iso14443_3a.h"

/* add by wanls  20140303*/
extern PICC_PARA gt_c_para; 


/**
 * Calculting the ucBcc byte with the string, be used in AntiCollision/Select command.
 * 
 * params: 
 *        pucBuf  : the buffer
 *        iNum    : the datas length
 *        pucBcc  : save the calculation result, 1 byts.
 * retval:
 *        0 - successfully.
 *        others, error.
 * Reference:
 * < EMV Contactless Book D - Contactless Comm Protocol 2.1, section 5.4.2>
 */
int ISO14443_3A_CalBcc( const unsigned char* pucBuf, int iNum, unsigned char* pucBcc )
{
    
    *pucBcc = 0;
    while( iNum-- )*pucBcc ^= *pucBuf++;
    
    return 0;
}

/**
 * ptPcdInfo send REQA command to PICC, and receive the ATQA from PICC.
 * 
 * params: 
 *        ptPcdInfo  : the deivce handler
 *        pucAtqa    : the buffer for ATQA, 2 bytes 
 * retval:
 *        0 - successfully, the ATQA is valid, consist of two bytes.
 *        others, error.
 * Reference:
 * < EMV Contactless Book D - Contactless Comm Protocol 2.1,section 5.3.2>
 */
int ISO14443_3A_Req( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAtqa )
{
    int           iRev       = 0;
    unsigned char ucReqCommand = ISO14443_A_COMMAND_REQA;
    unsigned char ucCheck = 0;
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucProtocol    = ISO14443_TYPEA_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_SHORT_FRAME;
    ptPcdInfo->uiFwt         = ISO14443_TYPEB_FWT_REQA;
    ptPcdInfo->uiSfgt        = 53;
    ptPcdInfo->uiFsc         = 32;
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucTxCrcEnable = 0;/*disable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 0;/*disable RX CRC*/
    ptPcdInfo->ucEmdEn       = 0;/*switch off electric magnetic disturb detect*/
    ptPcdInfo->aucATQx[0]    = 0;
    ptPcdInfo->ucUidLen      = 0;
    ptPcdInfo->ucModInvert   = 0;/*direct modulation*/
    
    iRev = PcdTransCeive( ptPcdInfo, &ucReqCommand, 1, pucAtqa, 2 );
    if( iRev )return iRev;
    if( 2 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEA_ERR_NUMBER;
    
    /*the bit7 to bit4 of ATQA[1] must be (0000)b*/
    //if( pucAtqa[1] & 0xf0 )return ISO14443_TYPEA_ERR_ATQA1;

    /*change by wanls 20150921 修改PiccDetect 获取的ATQA[1]参数存在错误的问题*/
    /*only one bit must be '1' in the bit4 to bit0 of ATQA[0]*/
    ucCheck = ( pucAtqa[0] & 0x1F );
    if(   ( 0x01 != ucCheck ) 
       && ( 0x02 != ucCheck)
       && ( 0x04 != ucCheck ) 
       && ( 0x08 != ucCheck )
       && ( 0x10 != ucCheck ) 
      )
    {
        return ISO14443_TYPEA_ERR_ATQA0;
    }
    
    /*the bit5 must be '0'*/
    //if( pucAtqa[0] & 0x20 )return ISO14443_TYPEA_ERR_ATQA0;
    
    /*the bit7 and bit6 of ATQA[0] must be (00)b, (01)b or (10)b*/
    ucCheck = ( pucAtqa[0] & 0xC0 );
    if(    ( 0x00 != ucCheck) 
        && ( 0x40 != ucCheck ) 
        && ( 0x80 != ucCheck) 
      )
    {
        return ISO14443_TYPEA_ERR_ATQA0;   
    }
    else
    {
        ptPcdInfo->ucUidLen = 4 + ( ucCheck >> 6 ) * 3;      
    }
    /*change end*/
    
    /*record ATQA*/
    ptPcdInfo->aucATQx[0] = 2;
    memcpy( ptPcdInfo->aucATQx + 1, pucAtqa, 2 );
    
    return 0;
}

/**
 * PCD send WUPA command to PICC, and receive the ATQA from PICC.
 * 
 * params: 
 *        ptPcdInfo  : the deivce handler
 *        pucAtqa    : the buffer for ATQA, 2 bytes 
 * retval:
 *        0 - successfully, the ATQA is valid, consist of two bytes.
 *        others, error.
 * Reference:
 * < EMV Contactless Book D - Contactless Comm Protocol 2.1,section 5.3.2>
 */
int ISO14443_3A_Wup( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAtqa )
{
    int           iRev         = 0;
    unsigned char ucWupCommand = ISO14443_A_COMMAND_WUPA;
    unsigned char ucCheck = 0;
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucProtocol    = ISO14443_TYPEA_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_SHORT_FRAME;
    ptPcdInfo->uiFwt         = ISO14443_TYPEB_FWT_REQA;
    ptPcdInfo->uiSfgt        = 53;
    ptPcdInfo->uiFsc         = 32;
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucTxCrcEnable = 0;/*disable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 0;/*disable RX CRC*/
    ptPcdInfo->ucEmdEn       = 0;/*switch off electric magnetic disturb detect*/
    ptPcdInfo->aucATQx[0]    = 0;
    ptPcdInfo->ucUidLen      = 0;
    ptPcdInfo->ucModInvert   = 0;/*direct modulation*/
    
    iRev = PcdTransCeive( ptPcdInfo, &ucWupCommand, 1, pucAtqa, 2 );
    if( iRev )return iRev;
    if( 2 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEA_ERR_NUMBER;
    
    /*the bit7 to bit4 of ATQA[1] must be (0000)b*/
    //if( pucAtqa[1] & 0xf0 )return ISO14443_TYPEA_ERR_ATQA1;
    
    /*only one bit must be '1' in the bit4 to bit0 of ATQA[0]*/
    /*change by wanls 20150921 修改PiccDetect 获取的ATQA[1]参数存在错误的问题*/
    ucCheck = ( pucAtqa[0] & 0x1F );
    if(   ( 0x01 != ucCheck ) 
       && ( 0x02 != ucCheck ) 
       && ( 0x04 != ucCheck ) 
       && ( 0x08 != ucCheck ) 
       && ( 0x10 != ucCheck ) 
      )
    {
        return ISO14443_TYPEA_ERR_ATQA0;
    }    
    
    /*the bit5 must be '0'*/
    //if( pucAtqa[0] & 0x20 )return ISO14443_TYPEA_ERR_ATQA0;
    
    /*the bit7 and bit6 of ATQA[0] must be (00)b, (01)b or (10)b*/
    ucCheck = ( pucAtqa[0] & 0xC0 );
    if(   ( 0x00 != ucCheck ) 
       && ( 0x40 != ucCheck ) 
       && ( 0x80 != ucCheck ) 
      )
    {
        return ISO14443_TYPEA_ERR_ATQA0;   
    }
    else
    {
        ptPcdInfo->ucUidLen = 4 + ( ucCheck >> 6 ) * 3; 
    }
    /*change end*/
    
    /*record ATQA*/
    ptPcdInfo->aucATQx[0] = 2;
    memcpy( ptPcdInfo->aucATQx + 1, pucAtqa, 2 );
    
    return 0;
}

/**
 * PCD send ANTICOLLISION command to PICC, and receive the UID from PICC.
 * 
 * params: 
 *        ptPcdInfo   : the deivce handler
 *        pucSak      : the select command response       
 * retval:
 *        0 - successfullys.
 *        others, error.
 *
 * Reference:
 * < EMV Contactless Book D - Contactless Comm Protocol 2.1, section 5.4.2>
 * < ISO/IEC 14443-3:2001(E) Section 6.4.3.1 and 6.4.4 >           
 */
int ISO14443_3A_AntiSel( struct ST_PCDINFO* ptPcdInfo, unsigned char *pucSak )
{
    unsigned char   ucCascadeLevel     = 0;
    unsigned char   ucBcc              = 0;
    unsigned char   aucAntiCommand[7]  = {0,};
    unsigned char   aucRxBuf[5]        = {0,};
    unsigned char  *pucUid             = ptPcdInfo->aucUid;
    unsigned char  *pucRecSak          = ptPcdInfo->aucSAK;
    int             iRev               = 0;
    int             iUidNum            = ptPcdInfo->ucUidLen;
    int             iRetrans           = 0;
    
    ptPcdInfo->uiPcdBaudrate = BAUDRATE_106000;
    ptPcdInfo->uiPiccBaudrate= BAUDRATE_106000;
    ptPcdInfo->ucProtocol    = ISO14443_TYPEA_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_STANDARD_FRAME;
    ptPcdInfo->uiFwt         = ISO14443_TYPEB_FWT_MIN;/*6780 clock cycles*/
    ptPcdInfo->uiSfgt        = 53;
    ptPcdInfo->uiFsc         = 32;
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucEmdEn       = 0;/*switch off electric magnetic disturb detect*/
    ptPcdInfo->aucSAK[ 0 ]   = 0;
    ptPcdInfo->ucModInvert   = 0;/*direct modulation*/
    
    if(    ( 4  != iUidNum ) 
        && ( 7  != iUidNum ) 
        && ( 10 != iUidNum ) 
       )
    {
        return ISO14443_TYPEA_ERR_ATQA0;
    }
    iRetrans = 0;
    *pucSak  = 0;
    pucRecSak = ptPcdInfo->aucSAK;
    pucRecSak++;
    
    pucUid = ptPcdInfo->aucUid;
    iUidNum  = ptPcdInfo->ucUidLen;
    ucCascadeLevel  = ISO14443_A_COMMAND_SEL1;/*levell cascade*/
    while( iUidNum )
    {
        /*anticollision command*/
        while( iRetrans++ < 3 )
        {
            ptPcdInfo->ucTxCrcEnable = 0;/*disable TX CRC*/
            ptPcdInfo->ucRxCrcEnable = 0;/*disable RX CRC*/
        
            aucAntiCommand[0] = ucCascadeLevel;
            aucAntiCommand[1] = ISO14443_A_SELECT_NVB( 0 );
            iRev = PcdTransCeive( ptPcdInfo, aucAntiCommand, 2, aucRxBuf, 5 );
            if( ISO14443_HW_ERR_COMM_TIMEOUT == iRev )continue;
            if( iRev )return iRev;/*communication error*/
            if( 5 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEA_ERR_NUMBER;
            
            ISO14443_3A_CalBcc( aucRxBuf, 5, &ucBcc );
            if( ucBcc )return ISO14443_TYPEA_ERR_BCC;
            iRetrans = 0;
            break;
        }
        if( iRetrans )return ISO14443_HW_ERR_COMM_TIMEOUT;
                     
        /*analyse the anticollision response and send select command*/
        if( 4 == iUidNum )
        {
            /*the last 4 bytes uid, the first byte must be not zero*/
            if( ISO14443_A_CASCADE_TAG == aucRxBuf[0] )
			{
			    /*add by wanls 20140303*/
			    /*针对广州羊城通部分不规范卡，返回0x88值问题*/
				if((gt_c_para.card_type_check_w == 0 ) || (gt_c_para.card_type_check_w == 1 && gt_c_para.card_type_check_val == 0))
				{
				    return ISO14443_TYPEA_ERR_SELECT;
				}
            }
			     
            memcpy( pucUid, aucRxBuf, 4 );
            pucUid += 4;
            iUidNum -= 4;
            
            while( iRetrans++ < 3 )
            {    
                ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
                ptPcdInfo->ucRxCrcEnable = 1;/*enable RX CRC*/
            
                aucAntiCommand[0] = ucCascadeLevel;
                aucAntiCommand[1] = ISO14443_A_SELECT_NVB( 40 );
                memcpy( aucAntiCommand + 2, aucRxBuf, 4 );
                ISO14443_3A_CalBcc( aucAntiCommand + 2, 4, aucAntiCommand + 6 );
                
                iRev = PcdTransCeive( ptPcdInfo, aucAntiCommand, 7, pucSak, 1 );
                if( ISO14443_HW_ERR_COMM_TIMEOUT == iRev )continue;
                if( iRev )return iRev;/*communication error*/
                
                if( 1 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEA_ERR_NUMBER;
                
                if( *pucSak & 0x04 )return ISO14443_TYPEA_ERR_SELECT;
                iRetrans = 0;
                
                /*record the SAK[n]*/
                ptPcdInfo->aucSAK[ 0 ]++;
                *pucRecSak++ = *pucSak;
                
                break;
            } 
            if( iRetrans )return ISO14443_HW_ERR_COMM_TIMEOUT;
        }
        else/*not completed uid, need to next cascade*/
        {
            if( ISO14443_A_CASCADE_TAG != aucRxBuf[0] )return ISO14443_TYPEA_ERR_CASCADE_TAG;
            memcpy( pucUid, aucRxBuf + 1, 3 );/*extract uid to devcie information*/
            pucUid += 3;
            iUidNum -= 3;

            while( iRetrans++ < 3 )
            { 
                ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
                ptPcdInfo->ucRxCrcEnable = 1;/*enable RX CRC*/
                
                aucAntiCommand[0] = ucCascadeLevel;
                aucAntiCommand[1] = ISO14443_A_SELECT_NVB( 40 );
                aucAntiCommand[2] = ISO14443_A_CASCADE_TAG;
                memcpy( aucAntiCommand + 3,  aucRxBuf + 1, 3 );
                ISO14443_3A_CalBcc( aucAntiCommand + 2, 4, aucAntiCommand + 6 );
                iRev = PcdTransCeive( ptPcdInfo, aucAntiCommand, 7, pucSak, 1 );
                if( ISO14443_HW_ERR_COMM_TIMEOUT == iRev )continue;
                if( iRev )return iRev;/*communication error*/
                if( 1 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEA_ERR_NUMBER;
                if( !( *pucSak & 0x04 ) )return ISO14443_TYPEA_ERR_SELECT; 
                iRetrans = 0;
                
                /*record the SAK[n]*/
                ptPcdInfo->aucSAK[ 0 ]++;
                *pucRecSak++ = *pucSak;
                
                break;
            }
            if( iRetrans )return ISO14443_HW_ERR_COMM_TIMEOUT;
            
            /*next cascade level*/
            ucCascadeLevel += 2;
            
        }
    }
    
    /*tell the sak is valid*/
    if( *pucSak & 0x20 )
    {/*compliant with iso14443-4 protocol*/
        ptPcdInfo->ucPiccType = ISO14443_4_STANDARD_COMPLIANT_A;
    }
    ptPcdInfo->ucEmdEn = 1;/*enable emd(anti-electric-magnetic disturb) function*/
    
    return 0;
}

/**
 * PCD send HALT command to PICC.
 * 
 * params:
 *        ptPcdInfo   : the deivce handler
 * retval:
 *        0 - successfullys.
 *        others, error.
 */
int ISO14443_3A_Halt( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char aucHaltCommand[2] = { ISO14443_A_COMMAND_HALT, 0x00 };
    
    ptPcdInfo->ucProtocol    = ISO14443_TYPEA_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_STANDARD_FRAME;
    ptPcdInfo->uiFwt         = ISO14443_TYPEB_FWT_HLTA;/*~1ms*/
    ptPcdInfo->uiSfgt        = 53;
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 1;/*enable RX CRC*/
    ptPcdInfo->ucModInvert   = 0;/*direct modulation*/
    
    aucHaltCommand[ 0 ] = ISO14443_A_COMMAND_HALT;
    aucHaltCommand[ 1 ] = 0x00;
    PcdTrans( ptPcdInfo, aucHaltCommand, 2 );
    
    return 0;
}

/**
 * Request answer to selection.( defined by iso14443-4 )
 *  
 * params:
 *       ptPcdInfo   : the deivce handler
 *       pucAts      : the response from PICC( space must be more than 256 bytes )
 * retval:
 *       0 - successfullys.
 *       others, error.
 */
int ISO14443_3A_Rats( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAts )
{
    unsigned char  aucRatsCommand[2];
    unsigned char* ucpParam;
    int            iUidNum  = 0;
    int            iRev = 0;
    
    ptPcdInfo->ucProtocol    = ISO14443_TYPEA_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_STANDARD_FRAME;
    ptPcdInfo->uiFwt         = ISO14443_TYPEB_FWT_RATS;/*~4.833ms*/
    ptPcdInfo->uiSfgt        = 60;
    ptPcdInfo->uiFsc         = 32;/*the default value of PICC maxium block size*/
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 1;/*enable RX CRC*/
    ptPcdInfo->aucATS[0]     = 0;
    ptPcdInfo->ucModInvert   = 0;/*direct modulation*/
    
    /*initialiszed the PCB*/
    ptPcdInfo->ucPcdPcb  = 0x02;
    ptPcdInfo->ucPiccPcb = 0x03;
    
    aucRatsCommand[0] = ISO14443_A_COMMAND_RATS;
    aucRatsCommand[1] = 0x80;/*the IFSD is 256, and the CID='0'*/
    iRev = PcdTransCeive( ptPcdInfo, aucRatsCommand, 2, pucAts, 254 );
    if( iRev )return iRev;
    if( pucAts[0] != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEA_ERR_NUMBER;
    if( pucAts[0] > 254 )return ISO14443_TYPEA_ERR_TL;
    
    /*record ATS*/
    memcpy( ptPcdInfo->aucATS, pucAts, pucAts[0] );
    
    /*parsing the ATS and apply these parameters*/
    if( pucAts[0] > 1 )/*if T0 is present*/
    {
        /*parse the parameters in ATS*/
        if( pucAts[1] & 0x80 )return ISO14443_TYPEA_ERR_T0;
    
        /*pick out the IFSC and calculate the maxium buffer size with PICC*/
        ptPcdInfo->uiFsc = GetMaxFrameSize( pucAts[1] & 0x0F );
    
        /*pick out the parameters, TA1, TB1 or TC1*/
        ucpParam = pucAts + 2;
        if( pucAts[1] & 0x10 )/*TA1 present*/
        {
            //if( *ucpParam & 0x08 )return ISO14443_TYPEA_ERR_TA1;
        
            /*pick out the maxium value of speed supported by PICC or ptPcdInfo*/
            if( *ucpParam & 0x01 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_212000;
            if( *ucpParam & 0x02 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_424000;
            if( *ucpParam & 0x04 )ptPcdInfo->uiPcdSupportBaudrate = BAUDRATE_848000;
        
            if( *ucpParam & 0x10 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_212000;
            if( *ucpParam & 0x20 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_424000;
            if( *ucpParam & 0x40 )ptPcdInfo->uiPiccSupportBaudrate = BAUDRATE_848000;
        
            if( *ucpParam & 0x80 )ptPcdInfo->uiPcdSupportBaudrate = ptPcdInfo->uiPiccSupportBaudrate;  
            ucpParam++;
        }
    
        if( pucAts[1] & 0x20 )/*TB1 present*/
        {
            /*the maxium frame waiting timeFWT + delta(FWT)*/
            ptPcdInfo->uiFwt  = 32 * ( 1 << ( ( *ucpParam & 0xF0 ) >> 4 ) ) + 
                                 3 * ( 1 << ( ( *ucpParam & 0xF0 ) >> 4 ) ) + 
                                 34;
            
            /*the minium frame guard time, SFGT + delta(SFGT)*/
			/* by tanbx 20140303 for case TA105_10~16  允许sfgi为[0x09,0x0F]的值*/
		    if(( *ucpParam & 0x0F ) > 0)
			/*add end*/
            {
                ptPcdInfo->uiSfgt = 32 * ( 1 << ( *ucpParam & 0x0F ) ) +
                                     3 * ( 1 << ( *ucpParam & 0x0F ) );
            }
            ucpParam++;
        }

        if( pucAts[1] & 0x40 )/*TC1 present*/
        {
            //if( 0x00 != ( *ucpParam & 0xFC ) )return ISO14443_TYPEA_ERR_TC1;
            if( *ucpParam & 0x01 )ptPcdInfo->ucNadEn = 1;/*NAD is supported by PICC*/
            if( *ucpParam & 0x02 )ptPcdInfo->ucCidEn = 1;/*CID is supported by PICC*/
            ucpParam++;
        }
    }
    if( ptPcdInfo->uiSfgt < 60 )ptPcdInfo->uiSfgt = 60; 
    PcdWaiter( ptPcdInfo, ptPcdInfo->uiSfgt );
    ptPcdInfo->uiSfgt = 60;
    
    return 0;
}

/**
 * Protocol and parameters selection requestion
 *  
 * params:
 *       ptPcdInfo    : the deivce handler
 * retval:
 *       0 - successfullys.
 *       others, error.
 * description: 
 *      after RATS, if support PPS, you may change parameter
 * by sending PPS command to PICC.
 *
 * <ISO/IEC 14443-4 section 5>
 */
int ISO14443_3A_Pps( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char aucPpsCommand[3];
    unsigned char ucRxC  = 0;
    
    int           iRev   = 0;
    
    ptPcdInfo->ucProtocol    = ISO14443_TYPEA_STANDARD;
    ptPcdInfo->ucFrameMode   = ISO14443_TYPEA_STANDARD_FRAME;
    ptPcdInfo->ucTxCrcEnable = 1;/*enable TX CRC*/
    ptPcdInfo->ucRxCrcEnable = 1;/*enable RX CRC*/
    ptPcdInfo->ucM1CryptoEn  = 0;/*switch off mifare crypto1*/
    ptPcdInfo->ucEmdEn       = 0;/*switch off electric magnetic disturb detect*/
    ptPcdInfo->ucModInvert   = 0;/*direct modulation*/
    
    aucPpsCommand[0] = ISO14443_A_COMMAND_PPS;
    aucPpsCommand[1] = 0x11;/*PPS1 is present*/
    aucPpsCommand[2] = 0x00;/*baudrate adjust integer*/
    iRev = PcdTransCeive( ptPcdInfo, aucPpsCommand, 3, &ucRxC, 1 );
    if( iRev )return iRev;
    if( 1 != ptPcdInfo->uiPcdTxRNum )return ISO14443_TYPEA_ERR_NUMBER;
    if( ISO14443_A_COMMAND_PPS != ucRxC )return ISO14443_TYPEA_ERR_PPSS;
    
    return 0;
}
