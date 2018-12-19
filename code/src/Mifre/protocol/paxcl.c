/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : paxcl.c
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

#include "paxcl.h"

extern PICC_PARA gt_c_para;
/**
 * ptPcdInfo polling A cards in RF field.
 * 
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        > 0 - successfully.
 *        < 0, error.
 */
int PaxAPolling( struct ST_PCDINFO *ptPcdInfo )
{
    unsigned char aucAtqa[2];
    unsigned char ucSak;
    unsigned char aucAtqaBak[2];
    unsigned char aucRxBuf[256];
    
    int           iRev;
    
    iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );
    if( iRev )return ConvertHalToApi( iRev );
    
    memcpy( aucAtqaBak, aucAtqa, 2 );
    iRev = ISO14443_3A_AntiSel( ptPcdInfo, &ucSak );  
    if( iRev )return ConvertHalToApi( iRev ); 
    
    ISO14443_3A_Halt( ptPcdInfo );
    
    iRev = ISO14443_3A_Req( ptPcdInfo, aucAtqa );
    if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )return RET_RF_DET_ERR_COLL;
    
    iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );
    if( iRev )return ConvertHalToApi( iRev );
    if( 0 != memcmp( aucAtqaBak, aucAtqa, 2 ) )return RET_RF_ERR_PROTOCOL;
    ptPcdInfo->uiPcdState = PCD_WAKEUP_PICC;
    
    iRev = ISO14443_3A_AntiSel( ptPcdInfo, &ucSak );  
    if( iRev )return ConvertHalToApi( iRev ); 
    
    /* some card published by some corporation* is not compliant with specification*/
    if( !( ucSak & 0x20 )
		&& ( gt_c_para.card_type_check_w == 0 
		|| ( gt_c_para.card_type_check_w == 1 && gt_c_para.card_type_check_val == 0 ) ) )
	{
			return ConvertHalToApi( RET_RF_DET_ERR_PROTOCOL );				
	} 
                                                       
    ptPcdInfo->uiPollTypeFlag |= TYPEA_INDICATOR;
    
    return ConvertHalToApi( ISO14443_3A_Rats( ptPcdInfo, aucRxBuf ) );
}

/**
 * ptPcdInfo polling B cards in RF field.
 * 
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        > 0 - successfully.
 *        < 0, error.
 */
int PaxBPolling( struct ST_PCDINFO *ptPcdInfo )
{
    unsigned char aucAtqb[16];
    unsigned char aucRxBuf[256];
    int           iNum;
    int           i;
    int           iRev;
    
    iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
    if( iRev )return ConvertHalToApi( iRev );
    for( i = 0; i < 3; i++ )
    {
        iRev = ISO14443_3B_Halt( ptPcdInfo );
        if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )break;
    }
    
    if( iRev )return ConvertHalToApi( iRev );
    
    iRev = ISO14443_3B_Req( ptPcdInfo, aucAtqb );  
    if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )return RET_RF_DET_ERR_COLL;
    
    iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
    if( iRev )return ConvertHalToApi( iRev );   
    ptPcdInfo->uiPcdState = PCD_WAKEUP_PICC;
    ptPcdInfo->uiPollTypeFlag |= TYPEB_INDICATOR;
    
    iRev = ISO14443_3B_Attri( ptPcdInfo, aucRxBuf, &iNum );
    
    return ConvertHalToApi( iRev ); 
}

/**
 * ptPcdInfo polling mifare cards in RF field.
 * 
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        > 0 - successfully.
 *        < 0, error.
 */
int PaxMPolling( struct ST_PCDINFO *ptPcdInfo )
{
    unsigned char aucAtqa[2];
    unsigned char ucSak;
    
    int           iRev;
    
    iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );
    if( iRev )return ConvertHalToApi( iRev );
    
    ptPcdInfo->uiPcdState = PCD_WAKEUP_PICC;
    
    iRev = ISO14443_3A_AntiSel( ptPcdInfo, &ucSak );  
    if( iRev )return ConvertHalToApi( iRev ); 
    
    
    /*only one type A card is in field*/
	/* change by wanls 2013.12.10*/
    if( ((ucSak & 0x3F) == 0x20)
       &&(      gt_c_para.card_type_check_w == 0 
           || ( gt_c_para.card_type_check_w == 1 && gt_c_para.card_type_check_val == 0 ) ) 
      )
    {
        return ConvertHalToApi( RET_RF_DET_ERR_PROTOCOL );
    }
	/* change end */

	
	ptPcdInfo->uiPollTypeFlag |= TYPEM_INDICATOR;
    MifareTypeCheck( ptPcdInfo, ucSak );
    
    return ConvertHalToApi( 0 );
}

/**
 * check if there are card in field
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        = 0 - successfully.
 *        < 0, error.
 */
int PaxHaltCard( struct ST_PCDINFO *ptPcdInfo )
{
    int iRev  = 0;
    
    /*the card in protocol status, first de-select*/
    if( ( PCD_ACTIVE_PICC == ptPcdInfo->uiPcdState ) 
       && ptPcdInfo->uiPollTypeFlag 
      )
    {
        iRev = ISO14443_4_SRequest( ptPcdInfo, 0xC2, 0x00 );
    }
    else if(   ( PCD_WAKEUP_PICC == ptPcdInfo->uiPcdState ) 
             &&  ptPcdInfo->uiPollTypeFlag 
           )
    {/*if a card hasn't entered protocol status*/
        if( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag )
        {
            iRev = ISO14443_3B_Halt( ptPcdInfo );
        }
        else
        { /*mifare or typeA card*/
            iRev = ISO14443_3A_Halt( ptPcdInfo );
        }
    }
    
    ptPcdInfo->uiPcdState     = PCD_REMOVE_PICC;
    ptPcdInfo->uiPollTypeFlag = 0;
    
    PcdCarrierOff( ptPcdInfo );
    
    return ConvertHalToApi( iRev );
}

/**
 * check if there are card in field
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        = 0 - successfully.
 *        < 0, error.
 */
int PaxRemoveCard( struct ST_PCDINFO *ptPcdInfo )
{
    unsigned char aucAtqa[2];
    unsigned char aucAtqb[13];
    int           iRev = 0;
    
    int           iReSend = 0;
    
    /*the card in protocol status, first de-select*/
    if( ( PCD_ACTIVE_PICC == ptPcdInfo->uiPcdState ) 
       && ptPcdInfo->uiPollTypeFlag 
      )
    {
        iRev = ISO14443_4_SRequest( ptPcdInfo, 0xC2, 0x00 );
    }
    else if(   ( PCD_WAKEUP_PICC == ptPcdInfo->uiPcdState ) 
             &&  ptPcdInfo->uiPollTypeFlag 
           )
    {/*if a card hasn't entered protocol status*/
        if( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag )
        {
            iRev = ISO14443_3B_Halt( ptPcdInfo );
        }
        else
        { /*mifare or typeA card*/
            iRev = ISO14443_3A_Halt( ptPcdInfo );
        }
    }
    
    /*if card has been removed*/
    if( 0 == ptPcdInfo->uiPollTypeFlag )return  ConvertHalToApi( 0 );  
    
    ptPcdInfo->uiPcdState = PCD_REMOVE_PICC;
    /*removal procedure*/
    if( ( TYPEA_INDICATOR | TYPEM_INDICATOR ) & ptPcdInfo->uiPollTypeFlag )
    {
        for( iReSend = 0; iReSend < 3; iReSend++ )
        {
            iRev = ISO14443_3A_Wup( ptPcdInfo, aucAtqa );
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )break;
        }
        if( 3 > iReSend )
        {
            ISO14443_3A_Halt( ptPcdInfo );
            return ConvertHalToApi( RET_RF_DET_ERR_PROTOCOL );
        }
    }
    else if( TYPEB_INDICATOR & ptPcdInfo->uiPollTypeFlag )
    {
        for( iReSend = 0; iReSend < 3; iReSend++ )
        {
            iRev = ISO14443_3B_Wup( ptPcdInfo, aucAtqb );
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )break;
        }
        if( 3 > iReSend )
        {
            iRev = ISO14443_3B_Halt( ptPcdInfo );    
            if( ISO14443_HW_ERR_COMM_TIMEOUT != iRev )
            {
                return ConvertHalToApi( RET_RF_DET_ERR_PROTOCOL );
            }
        }
    }
    
    /*cancel these indicators for card type*/
    ptPcdInfo->uiPollTypeFlag = 0;
    
    return ConvertHalToApi( 0 );
}

/**
 * copy parameters
 */
void PcdMemcpy( PICC_PARA *pSTDesPiccPara, PICC_PARA *pSTSourcePiccPara )
{
	if( pSTDesPiccPara == NULL ) return;
	
	strcpy( pSTDesPiccPara->drv_ver, pSTSourcePiccPara->drv_ver );
	strcpy( pSTDesPiccPara->drv_date, pSTSourcePiccPara->drv_date );
	
	pSTDesPiccPara->a_conduct_w  = 0;
	pSTDesPiccPara->a_conduct_val= pSTSourcePiccPara->a_conduct_val;
	
	pSTDesPiccPara->m_conduct_w  = 0;
	pSTDesPiccPara->m_conduct_val= pSTSourcePiccPara->m_conduct_val;
	
    pSTDesPiccPara->b_modulate_w  = 0;	
	pSTDesPiccPara->b_modulate_val= pSTSourcePiccPara->b_modulate_val;
	
	pSTDesPiccPara->card_buffer_w  = 0;
	pSTDesPiccPara->card_buffer_val= pSTSourcePiccPara->card_buffer_val;
    
    pSTDesPiccPara->wait_retry_limit_w  = 0;
	pSTDesPiccPara->wait_retry_limit_val= pSTSourcePiccPara->wait_retry_limit_val;
	
	pSTDesPiccPara->card_type_check_w  = 0;
	pSTDesPiccPara->card_type_check_val= pSTSourcePiccPara->card_type_check_val;
    
    pSTDesPiccPara->card_RxThreshold_w  = 0;
	pSTDesPiccPara->card_RxThreshold_val= pSTSourcePiccPara->card_RxThreshold_val;
	
	pSTDesPiccPara->f_modulate_w  = 0;
	pSTDesPiccPara->f_modulate_val= pSTSourcePiccPara->f_modulate_val;
	
	/*added by Liubo 2011-10-28*/
	pSTDesPiccPara->a_modulate_w  = 0;
	pSTDesPiccPara->a_modulate_val= pSTSourcePiccPara->a_modulate_val;
	
	pSTDesPiccPara->a_card_RxThreshold_w  = 0;
	pSTDesPiccPara->a_card_RxThreshold_val= pSTSourcePiccPara->a_card_RxThreshold_val;
	
	pSTDesPiccPara->a_card_antenna_gain_w  = 0;
	pSTDesPiccPara->a_card_antenna_gain_val= pSTSourcePiccPara->a_card_antenna_gain_val;
	
	pSTDesPiccPara->b_card_antenna_gain_w  = 0;
	pSTDesPiccPara->b_card_antenna_gain_val= pSTSourcePiccPara->b_card_antenna_gain_val;
	
	pSTDesPiccPara->f_card_antenna_gain_w  = 0;
	pSTDesPiccPara->f_card_antenna_gain_val= pSTSourcePiccPara->f_card_antenna_gain_val;
	
	pSTDesPiccPara->f_card_RxThreshold_w  = 0;
	pSTDesPiccPara->f_card_RxThreshold_val= pSTSourcePiccPara->f_card_RxThreshold_val;

	/* add by wanls 2012.08.14 */
	pSTDesPiccPara->f_conduct_w = 0;
	pSTDesPiccPara->f_conduct_val = pSTSourcePiccPara->f_conduct_val;
	/* add end */
	
	/* add by nt for paypass 3.0 test 2013/03/11 */
	pSTDesPiccPara->user_control_w = 0;
	pSTDesPiccPara->user_control_key_val = pSTSourcePiccPara->user_control_key_val;
	/* add end */
    /*add by wanls 20141106*/
	pSTDesPiccPara->protocol_layer_fwt_set_w = 0;
	pSTDesPiccPara->protocol_layer_fwt_set_val = pSTSourcePiccPara->protocol_layer_fwt_set_val;
	/*add end*/

	/* add by wanls 20150921*/
	pSTDesPiccPara->picc_cmd_exchange_set_w = 0;
	pSTDesPiccPara->picc_cmd_exchange_set_val = pSTSourcePiccPara->picc_cmd_exchange_set_val;
	/*add end*/
	
	return;
}

/* add by wanls 2011.04.11 */
/******************************************
函数原型：unsigned char ReadCFGRFType()          
参数：
buffer 读配置文件返回的数据
返回值：
<0      失败
其他    成功
*******************************************/
int GetRfChipType(unsigned char * ucRfChipType)
{
	int ret = 0;
	unsigned char buffer[5];
	
	memset(buffer, 0x00, sizeof(buffer));
	ret = ReadCfgInfo("RF_1356M", buffer);
	if(ret < 0)
		return ret;

	*ucRfChipType = (unsigned char)atoi(buffer);

	return 0;
}
/* add end */




