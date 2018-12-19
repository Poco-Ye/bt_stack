/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : paypass.c
 *
 * Author : Liubo     
 * 
 * Date   : 2011-8-16
 *
 * Description:
 *      encapsulate MasterCard's PayPass function
 *
 * History:
 *
 * =============================================================================
 */
#include <string.h>  
#include "iso14443hw_hal.h"
#include "iso14443_3a.h"
#include "iso14443_3b.h"
#include "iso14443_4.h"

#include "pcd_apis.h"

#include "paypass.h"

/**
 * polling card in field, check if there is a typeA or typeB card.
 *  
 * params:
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        0 - ok
 *        others, failure or absent
 */
int PayPassPolling( struct ST_PCDINFO *ptPcdInfo )
{
    unsigned char aucAtqa[2];
    unsigned char aucAtqb[13];
    unsigned char ucSak = 0;
    
    int           i     = 0;
    
    int           iRev  = 0;
    
    memset( aucAtqa, '\0', 2 );
    memset( aucAtqb, '\0', 13 );
    
    /*first, polling typeA cards*/
    iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );
    if( 0 == iRev )/*typeA is present*/
    {
        /*select card and get uid*/
        iRev = ISO14443_3A_AntiSel( ptPcdInfo, &ucSak );
        if( 0 == iRev )
        {
            ISO14443_3A_Halt( ptPcdInfo );
            
            /*check if other type A card enter into field*/
            iRev = ISO14443_3A_Req( ptPcdInfo, aucAtqa );
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )return RET_RF_DET_ERR_COLL;
            
            /*check if any type B card enter into field*/
            DelayMs( 5 );
            iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )return RET_RF_DET_ERR_COLL;
            
            /*only one type A card is in field*/
            MifareTypeCheck( ptPcdInfo, ucSak );
            ptPcdInfo->uiPollTypeFlag |= TYPEA_INDICATOR;
            
            return ConvertHalToApi( 0 );
        }
    }
    else if( ISO14443_HW_ERR_COMM_TIMEOUT == iRev )
    {
        /*check if any type B card enter into field*/
        DelayMs( 5 );
        iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
        if( 0 == iRev )
        {
            for( i = 0; i < 3; i++ ) 
		    {
			    iRev = ISO14443_3B_Halt( ptPcdInfo );
				if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )break;
		    }
		    
		    if( 0 == iRev )
		    {
		        iRev = ISO14443_3B_Req( ptPcdInfo, aucAtqb );  
		        if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )return RET_RF_DET_ERR_COLL;
                
                /*check if any type A card enter into field*/
                iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );
                if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )return RET_RF_DET_ERR_COLL;
                
                /*only one type B card is in field*/
                ptPcdInfo->ucPiccType = ISO14443_4_STANDARD_COMPLIANT_B;
                ptPcdInfo->uiPollTypeFlag |= TYPEB_INDICATOR;  
                
                return ConvertHalToApi( 0 );
		    }
        }
        else
        {
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )return RET_RF_ERR_TRANSMIT;  
        }
    }
    if( ISO14443_HW_ERR_COMM_TIMEOUT == iRev )return RET_RF_CMD_ERR_NO_CARD;
    
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
int PayPassAntiSelect( struct ST_PCDINFO* ptPcdInfo )
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
            if( ISO14443_HW_ERR_COMM_TIMEOUT == iRev && i < 2 )
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
        if( iRev )return ConvertHalToApi( iRev ); 
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
                if( iRev )return ConvertHalToApi( iRev );
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
int PayPassActivate( struct ST_PCDINFO* ptPcdInfo )
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
            if( (    ( ISO14443_HW_ERR_COMM_TIMEOUT == iRev ) 
                  || ( ISO14443_HW_ERR_COMM_FRAME == iRev )
                ) && ( i < 2 ) 
              )
            {
                i++;   
            }
            else
            {
                if( iRev )return ConvertHalToApi( iRev );
            }
        }while( iRev );
    }
    
    if( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag )
    {
        i = 0;
        do
        {
            iRev = ISO14443_3B_Attri( ptPcdInfo, aucRxBuf, &iNum );
            if( (    ( ISO14443_HW_ERR_COMM_TIMEOUT == iRev ) 
                  || ( ISO14443_HW_ERR_COMM_FRAME == iRev )
                ) && ( i < 2 ) 
              )
            {
                i++;   
            }
            else
            {
                if( iRev )return ConvertHalToApi( iRev );
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
int PayPassRemoval( struct ST_PCDINFO* ptPcdInfo )
{
    unsigned char aucAtqa[2];
    unsigned char aucAtqb[16];
    
    int           iRev = 0;
    
    /*removal procedure*/
    if( TYPEA_INDICATOR & ptPcdInfo->uiPollTypeFlag )
    {
        iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );
        if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )
        {
            ISO14443_3A_Halt( ptPcdInfo );/*halt typeA*/
            return RET_RF_ERR_CARD_EXIST;
        }
        else
        {
            iRev = 0;
        }
    }
    
    if( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag )
    {
        iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
        if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )iRev = RET_RF_ERR_CARD_EXIST;
        else iRev = 0;
    }
    DelayMs( 5 );
    
    return ConvertHalToApi( iRev );
}


