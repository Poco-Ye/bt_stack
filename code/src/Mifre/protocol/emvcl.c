/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : emvcl.c
 *
 * Author : Liubo     
 * 
 * Date   : 2011-8-16
 *
 * Description:
 *    encapsulate EMV Contactless function
 *
 * History:
 *
 * =============================================================================
 */
#include "iso14443hw_hal.h"
#include "iso14443_3a.h"
#include "iso14443_3b.h"

#include "pcd_apis.h"

#include "emvcl.h"
/**
 * ptPcdInfo polling all cards in RF field.
 * 
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        = 0 - successfully.
 *        !0, error.
 * Reference:
 * < EMV Contactless Book D - Contactless Comm Protocol 2.1, section 9.2>
 */
int EmvPolling( struct ST_PCDINFO *ptPcdInfo )
{
    unsigned char aucAtqa[2];
    unsigned char aucAtqb[16];
    
    int           i = 0;
    
    int           iRev  = 0;
    
    /*polling typeA*/
    iRev  = 0;
    if( 0 == ( TYPEA_INDICATOR & ptPcdInfo->uiPollTypeFlag ) )
    {
        iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );   
        if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )
        {
            ptPcdInfo->uiPollTypeFlag |= TYPEA_INDICATOR;
            /*halt typeA*/
            ISO14443_3A_Halt( ptPcdInfo );
        }
    }
    
    DelayMs( 5 );
    /*polling typeB*/
    iRev  = 0;
    if( 0 == ( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag ) )
    {
        iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
        if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )
        {
            ptPcdInfo->uiPollTypeFlag |= TYPEB_INDICATOR;
            
            /*when there is a B card in field, we must check if there is A card in field*/
            DelayMs( 5 );
            iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );   
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )
            {
                ptPcdInfo->uiPollTypeFlag |= TYPEA_INDICATOR;
                /*halt typeA*/
                ISO14443_3A_Halt( ptPcdInfo );
            }
            
        }
        DelayMs( 5 );
    }
    
    /*tell if there is only one card in field*/
    if(    ( TYPEA_INDICATOR & ptPcdInfo->uiPollTypeFlag )
        && ( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag ) 
      )
    {
        return RET_RF_DET_ERR_COLL;
    }
    
    if( 0 == (  ( TYPEA_INDICATOR | TYPEB_INDICATOR ) & ptPcdInfo->uiPollTypeFlag ) )
    {
        iRev = RET_RF_CMD_ERR_NO_CARD;         
    }
    else
    {
        iRev = 0;
    }
    
    return ConvertHalToApi( iRev );
}

/**
 * ptPcdInfo collision detection.
 * 
 * params: 
 *        ptPcdInfo : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int EmvAntiSelect( struct ST_PCDINFO* ptPcdInfo )
{        
    unsigned char aucAtqa[2];
    unsigned char aucAtqb[16];
    
    unsigned char ucSak;

    int           i    = 0;
    int           iRev = 0;
    
    /*anti-collision procedure and activation procedure*/
    if( TYPEA_INDICATOR & ptPcdInfo->uiPollTypeFlag )
    {
        i = 0;
        do
        {
            iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );  
            if( ( ISO14443_HW_ERR_COMM_TIMEOUT == iRev ) && ( i < 2 ) )
            {
                i++;
            }
            else
            {
                if( iRev )return ConvertHalToApi( iRev );
            }
        }while( iRev );
        ptPcdInfo->uiPcdState = PCD_WAKEUP_PICC;
        
        iRev = ISO14443_3A_AntiSel( ptPcdInfo, &ucSak );  
        //if( iRev )return ConvertHalToApi( iRev ); 
        if( iRev )return RET_RF_DET_ERR_COLL;
    }
    
    if( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag )
    {
        i = 0;
        do
        {
            iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
            if( ( ISO14443_HW_ERR_COMM_TIMEOUT == iRev ) && i < 2 )
            {
                i++;
            }
            else
            {
                //if( iRev )return ConvertHalToApi( iRev );
                if( iRev )return RET_RF_DET_ERR_COLL;
            }
        }while( iRev );
        ptPcdInfo->uiPcdState = PCD_WAKEUP_PICC;
    }
    
    return ConvertHalToApi( 0 );
}

/**
 * ptPcdInfo activating card.
 * 
 * params: 
 *        ptPcdInfo : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int EmvActivate( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char aucRxBuf[256];
    int           iNum = 0;
    int           i    = 0;
    
    int           iRev = 0;
    
    if( TYPEA_INDICATOR & ptPcdInfo->uiPollTypeFlag )
    {
        i = 0;
        do
        {
            iRev = ISO14443_3A_Rats( ptPcdInfo, aucRxBuf );
            if( ( ISO14443_HW_ERR_COMM_TIMEOUT == iRev ||
                  ISO14443_HW_ERR_COMM_FRAME == iRev ) && 
                  i < 2 )
            {
                i++;   
            }
            else
            {
                if( iRev )break;
            }
        }while( iRev );
    }
    
    if( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag )
    {
        i = 0;
        do
        {
            iRev = ISO14443_3B_Attri( ptPcdInfo, aucRxBuf, &iNum );
            if( ( ISO14443_HW_ERR_COMM_TIMEOUT == iRev ||
                  ISO14443_HW_ERR_COMM_FRAME == iRev ) && 
                  i < 2 )
            {
                i++;
            }
            else
            {
                if( iRev )break;
            }
        }while( iRev );
    }
    
    return ConvertHalToApi( iRev );
}

/**
 * removal card.
 * 
 * params: 
 *        ptPcdInfo : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int EmvRemoval( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char aucAtqa[2];
    unsigned char aucAtqb[16];
    
    int           iRev  = 0;
    
    int           iPass = 0;
    int           iFail = 0;
    
    /*removal procedure*/
    do
    {
		/*change by wanls 2012.03.15 解决用M模式寻卡成功后，
		调用E模式移卡出现死循环的情况*/
        if(( TYPEA_INDICATOR & ptPcdInfo->uiPollTypeFlag )
			|| (TYPEM_INDICATOR & ptPcdInfo->uiPollTypeFlag))
        {
            iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )
            {
                ISO14443_3A_Halt( ptPcdInfo );/*halt typeA*/
                iRev = RET_RF_ERR_CARD_EXIST;
                iPass = 0;
                iFail++;
            }
            else
            {
                iRev = 0;
                iPass++;
            }
        }
    
        else if( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag )
        {
            iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )
            {
                iFail++;
                iPass = 0;
                iRev = RET_RF_ERR_CARD_EXIST;
            }
            else 
            {
                iPass++;
                iRev = 0; 
            }
        }
		else
		{
		    break;
		}
		
        DelayMs( 5 );
    }while( ( iPass < 3 ) && ( iFail < 3 ) );/*change iFaile < 3. change by wanls 2012.09.24*/
    
	/*a dd by wanls 2015.05.03*/
    ptPcdInfo->uiPcdState = PCD_REMOVE_PICC;
	/* add end */

    return ConvertHalToApi( iRev );
}
