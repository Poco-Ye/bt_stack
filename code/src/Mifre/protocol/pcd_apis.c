/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : pcd_apis.c
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
#include "iso14443_3b.h"
#include "iso14443_4.h"

#include "mifare.h"
#include "pcd_apis.h"

#include "paypass.h"
#include "emvcl.h"
#include "paxcl.h"

/*PCD parameters*/
PICC_PARA gt_c_para;

/*the global variable in contactless*/
static struct ST_PCDINFO      gtPcdModuleInfo;
static unsigned int guiFelicaTimeOut = 0;

unsigned char gucRfChipType = 0;

struct ST_PCDINFO* GetPcdHandle()
{
    return ( &gtPcdModuleInfo );    
}


struct ST_PCDOPS * GetChipOpsHandle(unsigned char ucRfChipType)
{
	struct ST_PCDOPS *gtPcdOps = NULL;
	switch(ucRfChipType)
	{
	case CHIP_TYPE_RC531:
		gtPcdOps = NULL;
		break;
	case CHIP_TYPE_PN512:
		gtPcdOps = (struct ST_PCDOPS *)GetPn512OpsHandle();
		break;
	case CHIP_TYPE_RC663:
		gtPcdOps = (struct ST_PCDOPS *)GetRc663OpsHandle();
		break;
	case CHIP_TYPE_AS3911:
		gtPcdOps = (struct ST_PCDOPS *)GetAs3911OpsHandle();
		break;
	default:
		gtPcdOps = NULL;
		break;
	}
	return gtPcdOps;
}


/**
 * convert the error code from HAL to APIs
 *
 * param:
 *     iRev : HAL error code
 * 
 * retval:
 *     the API's return code
 *
 */
unsigned char ConvertHalToApi( int iRev )
{
    unsigned char ucRev = 0;
    
    if( iRev < 0 )
    {
        switch( iRev )
        {
        case ISO14443_HW_ERR_COMM_TIMEOUT:/*wait timeout*/
            ucRev = RET_RF_ERR_TIMEOUT;
        break;
        case ISO14443_4_ERR_PROTOCOL:/*ISO14443-4 protocol error*/
            ucRev = RET_RF_ERR_PROTOCOL;
        break;
        case ISO14443_HW_ERR_COMM_COLL:/*multi-card collission error*/
            ucRev = RET_RF_DET_ERR_COLL;
        break;
        case PHILIPS_MIFARE_ERR_AUTHEN:
            ucRev = RET_RF_ERR_AUTH;
        break;
        case ISO14443_HW_ERR_COMM_FIFO:
            ucRev = RET_RF_ERR_TRANSMIT;
        break;
        case ISO14443_HW_ERR_COMM_CODE:
            ucRev = RET_RF_ERR_CRC;
        break;
        case ISO14443_HW_ERR_COMM_FRAME:
            ucRev = RET_RF_ERR_FRAMING;
        break;
        case ISO14443_HW_ERR_COMM_PARITY:
            ucRev = RET_RF_ERR_PARITY;
        break;
        case ISO14443_HW_ERR_COMM_PROT:
            ucRev = RET_RF_ERR_TRANSMIT;
        break;
        case ISO14443_HW_ERR_RC663_SPI:
            ucRev = RET_RF_ERR_CHIP_ABNORMAL;/*there is no module*/
        break;
		case ISO14443_HW_ERR_COMM_CRC:
			ucRev = RET_RF_ERR_CRC;
		break;
		case ISO14443_PCD_ERR_USER_CANCEL:/*add by nt for ZhouJie paypass 3.0 2013.4.18  */
			ucRev = RET_RF_ERR_USER_CANCEL;
		break;
        default:
            ucRev = RET_RF_ERR_TRANSMIT;
        break;
        }
    }
    else
    {
        ucRev = iRev;    
    }
    
    return ucRev;
}






/**
 * Read the version of PCD driver
 * 
 * params:
 *          ver : the buffer of version information
 *                it must have 30 bytes space.
 * retval:
 *          no return value.
 */
void PcdGetVer( char* ver )
{
	strcpy(ver, PCD_VER);
}

/**
 * Initialising the PCD device
 * 
 * params:
 *          pucRfType:the type of Rf chip
 *          pucRfPara:antenna parameter
 * retval:
 *          return value.
 */
 /*pucRfPara返回的数据中第一个元素存放参数的个数，后面的值表示RF天线参数*/
unsigned char PcdInit(unsigned char *pucRfType, unsigned char *pucRfPara)
{
    unsigned char aucBuff[64];
	int iRet = 0;

	if (pucRfType == NULL || pucRfPara == NULL)
	{
		return RET_RF_ERR_PARAM;
	}

	memset( &gtPcdModuleInfo, '\0', sizeof( struct ST_PCDINFO ) );
    /**
     * if your board have more than one chip, then you may distinguish 
     * them here.
     * 
     * this example is only for Rc663, so i havn't distinguished
     *
     * here, we get the type of interface chip by configuration file.
     * initialization according with chip
     */
    if(GetRfChipType(&gucRfChipType))
    {
        return RET_RF_ERR_CHIP_ABNORMAL;
    }
	
	switch(gucRfChipType)
	{
	case CHIP_TYPE_RC531:
		break;
	case CHIP_TYPE_PN512:
		break;
	case CHIP_TYPE_RC663:
		break;
	case CHIP_TYPE_AS3911:
		break;
	default:
		return RET_RF_ERR_CHIP_ABNORMAL;
	}

	*pucRfType = gucRfChipType;

    /* add by wanls 2012.04.11 */
    iRet = PcdDefaultInit(&gtPcdModuleInfo, GetChipOpsHandle(gucRfChipType)
            , (const struct ST_PCDBSP *)GetBoardHandle());
    /* add end */
    if( 0 != iRet)
    {
        gtPcdModuleInfo.uiPcdState = PCD_CHIP_ABNORMAL;
        return RET_RF_ERR_CHIP_ABNORMAL;/*there is not rf module*/   
    }
    /*enable chip enter into power down mode to save power consumption*/
    PcdPowerDown( &gtPcdModuleInfo );
    gtPcdModuleInfo.uiPcdState = PCD_BE_CLOSED;
        
    /*initialising all paremeters from configuration file*/
    memset( &gt_c_para, 0, sizeof( PICC_PARA ) );
    if( 0 != gtPcdModuleInfo.ptPcdOps->GetParamTagValue( &gt_c_para, pucRfPara))return RET_RF_ERR_CHIP_ABNORMAL;
    strcpy( gt_c_para.drv_ver, PCD_VERSION );
    strcpy( gt_c_para.drv_date, PCD_DATETIME );
        
    
    return 0;
}

/**
 * the contactless module power up.
 *  
 * params:
 *         no params
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccOpen( )
{
    int iRev;
    unsigned char pucRfPara[31];

	guiFelicaTimeOut = 0;
	
    /*a chip of RF module power up*/
	/* add by wanls 2013.4.16 */
	if(gucRfChipType == 0)
	{
	    gtPcdModuleInfo.uiPcdState = PCD_BE_CLOSED;
        gtPcdModuleInfo.uiPollTypeFlag = 0; 
        gtPcdModuleInfo.uiPcdState = PCD_CHIP_ABNORMAL;
        return RET_RF_ERR_CHIP_ABNORMAL;  
	}
	/* add end */

    /* add by wanls 2012.04.11*/
    iRev = PcdDefaultInit(&gtPcdModuleInfo, GetChipOpsHandle(gucRfChipType)
                , (const struct ST_PCDBSP *)GetBoardHandle());
    /* add end */

    /*there is not rf module*/
    if( 0 == iRev )
    {
        /*indicate the PCD status*/
        gtPcdModuleInfo.uiPcdState = PCD_BE_OPENED;
        gtPcdModuleInfo.uiPollTypeFlag = 0;
    }
    else
    {
        gtPcdModuleInfo.uiPcdState = PCD_BE_CLOSED;
        gtPcdModuleInfo.uiPollTypeFlag = 0; 
        gtPcdModuleInfo.uiPcdState = PCD_CHIP_ABNORMAL;
        
        return RET_RF_ERR_CHIP_ABNORMAL;    
    }

	/* add by wanls 2012.05.21 */
    memset( &gt_c_para, 0, sizeof( PICC_PARA ) );
    if( 0 != gtPcdModuleInfo.ptPcdOps->GetParamTagValue( &gt_c_para, pucRfPara))return RET_RF_ERR_CHIP_ABNORMAL;
    strcpy( gt_c_para.drv_ver, PCD_VERSION );
    strcpy( gt_c_para.drv_date, PCD_DATETIME );
	/* add end */
	
    return 0;
}

/**
 * loading the configuration into driver chip.
 *  
 * params:
 *         ucMode    :  
 *         ptPiccPara : 
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccSetup( unsigned char ucMode, PICC_PARA *ptPiccPara )
{
    unsigned char aucParam[8];
    
    if( NULL == ptPiccPara )return RET_RF_ERR_PARAM;
    
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;

    /* add by wanls 2012.12.25 根据香港的需求设置felica的
	超时时间*/
	if (ucMode == 'T' || ucMode == 't')
	{
		guiFelicaTimeOut = *((unsigned int *)ptPiccPara);
		
		return RET_RF_OK;
	}
	/* add end */
    
    if( 's' == ucMode || 'S' == ucMode )
    {
        /*2011-10-27 取消备份机制*/
        gt_c_para.a_conduct_w   = 1;
		gt_c_para.a_conduct_val = ptPiccPara->a_conduct_val;
		
		gt_c_para.b_modulate_w   = 1;
		gt_c_para.b_modulate_val = ptPiccPara->b_modulate_val;
		
		gt_c_para.card_RxThreshold_w   = 1;
		gt_c_para.card_RxThreshold_val = ptPiccPara->card_RxThreshold_val;
        
        gt_c_para.f_modulate_w   = 1;
        gt_c_para.f_modulate_val = ptPiccPara->f_modulate_val;
        
        gt_c_para.a_card_RxThreshold_w   = 1;
		gt_c_para.a_card_RxThreshold_val = ptPiccPara->a_card_RxThreshold_val;
		
		gt_c_para.a_card_antenna_gain_w   = 1;
		gt_c_para.a_card_antenna_gain_val = ptPiccPara->a_card_antenna_gain_val;
		
		gt_c_para.b_card_antenna_gain_w   = 1;
		gt_c_para.b_card_antenna_gain_val = ptPiccPara->b_card_antenna_gain_val;
		
		gt_c_para.f_card_antenna_gain_w   = 1;
		gt_c_para.f_card_antenna_gain_val = ptPiccPara->f_card_antenna_gain_val;

        /* add by wanls 2012.08.14 */
		gt_c_para.f_conduct_w = 1;
		gt_c_para.f_conduct_val = ptPiccPara->f_conduct_val;
		/* add end */
		
		/*configuration all parameters*/
 		gtPcdModuleInfo.ptPcdOps->SetParamValue( &gt_c_para );
 		
		return 0;
    }
    
    if( ( 'r' != ucMode ) && 
        ( 'R' != ucMode ) && 
        ( 'w' != ucMode ) && 
        ( 'W' != ucMode ) 
      )
    {
        return RET_RF_ERR_PARAM;
    }
    
    if( 'R' == ucMode || 'r' == ucMode )
    {
        memset( ptPiccPara, '\0', sizeof( PICC_PARA ) );
        
        strcpy( ptPiccPara->drv_ver, gt_c_para.drv_ver ); 
        strcpy( ptPiccPara->drv_date, gt_c_para.drv_date );      

		/* add by wanls 2014.11.06 */
		gt_c_para.protocol_layer_fwt_set_val = gtPcdModuleInfo.uiFwt;
		/* add end */

		/* add by wanls 20150921 */
		gt_c_para.picc_cmd_exchange_set_val = 
		(gtPcdModuleInfo.ucTxCrcEnable | (gtPcdModuleInfo.ucRxCrcEnable << 1));
		/* add end */
    }
    
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;
    
    if( 'r' == ucMode || 'R' == ucMode )
    {
        /*copy the real parameters to function parameter*/
        PcdMemcpy( ptPiccPara, &gt_c_para );
    }
    else /*Write mode*/
    {
		if( 1 == ptPiccPara->card_buffer_w ) 
		{
	     	gt_c_para.card_buffer_w   = 1;  
	     	if( ptPiccPara->card_buffer_val > 256 )ptPiccPara->card_buffer_val = 256;
			gt_c_para.card_buffer_val = ptPiccPara->card_buffer_val;
 		}	
	
		if( 1 == ptPiccPara->card_type_check_w ) 
		{
	    	gt_c_para.card_type_check_w   = 1;  
			gt_c_para.card_type_check_val = ptPiccPara->card_type_check_val;
 		}
 		
		/*RF carrier amplifier*/
        if( 1 == ptPiccPara->a_conduct_w )
	    {
	        gt_c_para.a_conduct_w   = 1;
	    	gt_c_para.a_conduct_val = ptPiccPara->a_conduct_val;
	    }
	    
	    /*TypeB modulation deepth*/
	    if( 1 == ptPiccPara->b_modulate_w )
	    {
            gt_c_para.b_modulate_w   = 1;
            gt_c_para.b_modulate_val = ptPiccPara->b_modulate_val;
	    }
	    
	    /*Type B Receive Threshold value*/
        if( 1 == ptPiccPara->card_RxThreshold_w )
        { 
            gt_c_para.card_RxThreshold_w   = 1;
            gt_c_para.card_RxThreshold_val = ptPiccPara->card_RxThreshold_val;
        }
        
        /*Type C (Felica) modulate value*/
        if( 1 == ptPiccPara->f_modulate_w )
        {      
            gt_c_para.f_modulate_w   = 1;
            gt_c_para.f_modulate_val = ptPiccPara->f_modulate_val;
            
        }
        
        /*Type A Receive Threshold value*/
        if( 1 == ptPiccPara->a_card_RxThreshold_w )
        {    
            gt_c_para.a_card_RxThreshold_w   = 1;
            gt_c_para.a_card_RxThreshold_val = ptPiccPara->a_card_RxThreshold_val;
        }
        
        /*Type C(Felica) Receive Threshold value*/
        if( 1 == ptPiccPara->f_card_RxThreshold_w )
        {   
            gt_c_para.f_card_RxThreshold_w   = 1;
            gt_c_para.f_card_RxThreshold_val = ptPiccPara->f_card_RxThreshold_val;
        }
        
        /*Type A antenna gain value*/
        if( 1 == ptPiccPara->a_card_antenna_gain_w )
        {
            gt_c_para.a_card_antenna_gain_w   = 1;
            gt_c_para.a_card_antenna_gain_val = ptPiccPara->a_card_antenna_gain_val;
        }
        
        /*Type B antenna gain value*/   
        if( 1 == ptPiccPara->b_card_antenna_gain_w )
        {   
            gt_c_para.b_card_antenna_gain_w   = 1;
            gt_c_para.b_card_antenna_gain_val = ptPiccPara->b_card_antenna_gain_val;
        }
        
        /*Felica antenna gain value*/
        if( 1 == ptPiccPara->f_card_antenna_gain_w )
        {    
            gt_c_para.f_card_antenna_gain_w   = 1;
            gt_c_para.f_card_antenna_gain_val = ptPiccPara->f_card_antenna_gain_val;
        }

		/* add by wanls 2012.08.14 */
		/* felica RF carrier amplifier*/
		if( 1 == ptPiccPara->f_conduct_w)
		{
		    gt_c_para.f_conduct_w = 1;
			gt_c_para.f_conduct_val = ptPiccPara->f_conduct_val;
		}
		/* add end */
        
        /*configuration all parameters*/
 		gtPcdModuleInfo.ptPcdOps->SetParamValue( &gt_c_para );
 		
 		/* add by nt for paypass 3.0 test 2013/03/11 */
		if( 1== ptPiccPara->user_control_w)
		{
		   gt_c_para.user_control_w = 1;
		   gt_c_para.user_control_key_val = ptPiccPara->user_control_key_val;
		}
		if( 1== ptPiccPara->wait_retry_limit_w)
		{
		   gt_c_para.wait_retry_limit_w = 1;
		   gt_c_para.wait_retry_limit_val = ptPiccPara->wait_retry_limit_val;
		}
		/* add end */
		/* add by wanls 2014.11.06 */
        if ( 1 == ptPiccPara->protocol_layer_fwt_set_w)
        { 
		    gt_c_para.protocol_layer_fwt_set_w = 1;
		    gt_c_para.protocol_layer_fwt_set_val = ptPiccPara->protocol_layer_fwt_set_val;
			gtPcdModuleInfo.uiFwt = gt_c_para.protocol_layer_fwt_set_val; 
        }
		/* add end */
		/*add by wanls 2015.9.16*/
		if( 1 == ptPiccPara->picc_cmd_exchange_set_w)
		{
		    if(MIFARE_STANDARD == gtPcdModuleInfo.ucProtocol)
		    {
		        gt_c_para.picc_cmd_exchange_set_w = 1;
			    gt_c_para.picc_cmd_exchange_set_val = ptPiccPara->picc_cmd_exchange_set_val;
				
				gtPcdModuleInfo.ucTxCrcEnable = !!(gt_c_para.picc_cmd_exchange_set_val & TX_CRC_MASK);
				gtPcdModuleInfo.ucRxCrcEnable = !!(gt_c_para.picc_cmd_exchange_set_val & RX_CRC_MASK);
		    }
			else
			{
			    return RET_RF_ERR_STATUS;
			}
		}
		/*add end*/

    }
    
    return RET_RF_OK;
}

/**
 * loading the configuration into driver chip and open carrier.
 *  
 * params:
 *         ucMode        :  the card polling mode, 0, 1, 'A', 'B' or 'M'. 
 *         pucCardType   :  return the card type, 'M', 'A', 'B'etc.
 *         pucSerialInfo :  the serial number of PICC
 *         pucCID        :  zero
 *         pucOther      :  Len[0] + ErrCod[2] + 
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccDetect( unsigned char  ucMode, 
                          unsigned char* pucCardType,
                          unsigned char* pucSerialInfo,
                          unsigned char* pucCID,
                          unsigned char* pucOther )
{
    unsigned char  ucCardType;
    unsigned char *pucInfo;
    int            iRev = 0;
	unsigned char ucRet = 0; //add by nt 2011/12/28
  
	unsigned int uiFWTtemp = 0x00; //add by nt 2011/12/28
    APDU_SEND apdu_s; //add by nt 2011/12/28
    APDU_RESP apdu_r; //add by nt 2011/12/28
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if the RF module is already opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;
	
    /*将小写字母换成大写字母*/
	ucMode = ((((ucMode) >= 'a') && ((ucMode) <= 'z')) ? ((ucMode)- ('a' - 'A')):(ucMode));
	/*初始化卡片类型*/
	
    /*add by wanls  20140303*/
    /*PiccDetect的d模式及p模式为广电运通的需求*/
	//add by nt 2011/12/28 
	if ( ( ucMode == 'D' || ucMode == 'd' ) ||
		( ucMode == 'P' || ucMode == 'p' ) )
	{
	    uiFWTtemp = gtPcdModuleInfo.uiFwt;
		//modify FWI and FWT
		gtPcdModuleInfo.uiFwt = 4132;  
         
		memset(&apdu_r, 0, sizeof(APDU_RESP));
		memset(&apdu_s, 0, sizeof(APDU_SEND));
        memset(apdu_s.DataIn, 0, sizeof(apdu_s.DataIn));

		if( ucMode == 'D' || ucMode == 'd' )
		{
		    /*ED*/
    	    memcpy(apdu_s.Command, "\x80\x5C\x00\x01", 4);/*Liubo*/
		}
		else
		{
		    /*EP*/
    	    memcpy(apdu_s.Command, "\x80\x5C\x00\x02", 4);/*Liubo*/
		}
		apdu_s.Lc = 0;
		apdu_s.Le = 4;

		ucRet = PiccIsoCommand( 0, &apdu_s, &apdu_r);

		gtPcdModuleInfo.uiFwt = uiFWTtemp; //还原之前的FWT

		if (ucRet == 0)
		{
		    return RET_RF_OK; //返回OK
		}
		else
		{
		    return 3; //返回超时
		}

	}
	/*add end*/

	
	gtPcdModuleInfo.uiPollTypeFlag = 0;

    /*output base carrier*/
    PcdCarrierOn( &gtPcdModuleInfo );
    
    /*polling card in field base on assigned mode*/
    switch( ucMode )
    {
    case 0: /*paypass mode polling*/
        iRev = PayPassPolling( &gtPcdModuleInfo );
        if( iRev )
        {
            /*has more than one card in field*/
			/*change by wanls 2012.12.13 解决在临界点出现读卡错误，
			导致一直读卡错误的问题*/
            if( RET_RF_CMD_ERR_NO_CARD != iRev )
            {
                /*all card unregistered*/
                gtPcdModuleInfo.uiPollTypeFlag = 0;
                PcdCarrierOff( &gtPcdModuleInfo );
            }
            break;
        }
        
        iRev = PayPassAntiSelect( &gtPcdModuleInfo );
        if( iRev )
        {
            gtPcdModuleInfo.uiPollTypeFlag = 0;
            PcdCarrierOff( &gtPcdModuleInfo );
            break;
        }
        
        iRev = PayPassActivate( &gtPcdModuleInfo );
        if( iRev )
        {
            gtPcdModuleInfo.uiPollTypeFlag = 0;
            PcdCarrierOff( &gtPcdModuleInfo );
        }
    break;
    case 1: /*EMV mode polling*/
        iRev = EmvPolling( &gtPcdModuleInfo );
        if( iRev )
        {
            /*has more than one card in field*/
            if( RET_RF_CMD_ERR_NO_CARD != iRev )
            {
                /*all card unregistered*/
                gtPcdModuleInfo.uiPollTypeFlag = 0;
                PcdCarrierOff( &gtPcdModuleInfo );
            }
            break;
        }

        iRev = EmvAntiSelect( &gtPcdModuleInfo );
        if( iRev )
        {
            gtPcdModuleInfo.uiPollTypeFlag = 0;
            PcdCarrierOff( &gtPcdModuleInfo );
            break;
        }
        
        iRev = EmvActivate( &gtPcdModuleInfo );
        if( iRev )
        {
            gtPcdModuleInfo.uiPollTypeFlag = 0;
            PcdCarrierOff( &gtPcdModuleInfo );
        }
    break;
    case 'a':
    case 'A':/*polling TypeA card*/
        iRev = PaxAPolling( &gtPcdModuleInfo );
        if( iRev )
        {
            /*all card unregistered*/
            gtPcdModuleInfo.uiPollTypeFlag = 0; 

			/*add by wanls 2013.12.10*/
			/*如果返回的错误码不是超时错误，才进行复位载波，避免无卡时
			频繁复位载波使LCD出现闪屏*/
			if(iRev !=  RET_RF_ERR_TIMEOUT)
			/*add end */
                PcdCarrierOff( &gtPcdModuleInfo );
        }
    break;
    case 'b':
    case 'B':/*polling TypeB card*/
        iRev = PaxBPolling( &gtPcdModuleInfo );
        if( iRev )
        {
            /*all card unregistered*/
            gtPcdModuleInfo.uiPollTypeFlag = 0;

			/*add by wanls 2013.12.10*/
			/*如果返回的错误码不是超时错误，才进行复位载波，避免无卡时
			频繁复位载波使LCD出现闪屏*/
			if(iRev !=  RET_RF_ERR_TIMEOUT)
			/*add end */
                PcdCarrierOff( &gtPcdModuleInfo );
        }
    break;
    case 'm':
    case 'M':/*polling Mifare card*/
        iRev = PaxMPolling( &gtPcdModuleInfo );
        if( iRev )
        {
            /*all card unregistered*/
            gtPcdModuleInfo.uiPollTypeFlag = 0; 

			/*add by wanls 2013.12.10*/
			/*如果返回的错误码不是超时错误，才进行复位载波，避免无卡时
			频繁复位载波使LCD出现闪屏*/
			if(iRev !=  RET_RF_ERR_TIMEOUT)
			/*add end */
                PcdCarrierOff( &gtPcdModuleInfo );
        }
		/* add by wanls 20150921*/
		else
		{
		    /*Set protocol type*/
			gtPcdModuleInfo.ucProtocol = MIFARE_STANDARD;
			/*Set FWT*/
			gtPcdModuleInfo.uiFwt = PHILIPS_MIFARE_OPS_TOV;
			/*disable retransmit*/
			gtPcdModuleInfo.ucEmdEn = 0;
		}
		/* add end */
    break;
    default:
        iRev = RET_RF_ERR_PARAM;
    break;
    }

    /*card type indicator*/
    if( 0 == iRev )
    {
        ucCardType = 0;
        if( TYPEB_INDICATOR & gtPcdModuleInfo.uiPollTypeFlag )
        {
            ucCardType = 'B';
        }
        else
        {   /*if is 'A', it can is 'A' or 'M'*/
            if( 'M' == ucMode )
            {
                ucCardType = 'M';
            }
            else
            {
                ucCardType = 'A';  
            }
        }
        
        /*card is actived, enter into protocol mode, Mifare no enter this status*/
        if( ( 'A' == ucCardType ) || ( 'B' == ucCardType ) )
        {
            gtPcdModuleInfo.uiPcdState = PCD_ACTIVE_PICC;
        } 
        
        /*out card type*/
        if( NULL != pucCardType )
        {
            *pucCardType = ucCardType;    
        }
        
        /*output card's CID*/
        if( NULL != pucCID )
        {
            *pucCID = 0;   
        }
        
        /*output the unique identification of card*/
        if( NULL != pucSerialInfo )
        {
            pucSerialInfo[0] = gtPcdModuleInfo.ucUidLen; 
            memcpy( pucSerialInfo + 1, gtPcdModuleInfo.aucUid, gtPcdModuleInfo.ucUidLen );
        }
    }
    
    /*output all procedure information*/
    if( NULL != pucOther )
    {
        pucInfo = pucOther;
        /*Length*/
        *pucInfo++ = 0;
        
        /*Error Code*/
        *pucInfo++ = iRev % 256;
        *pucInfo++ = iRev / 256;
        
        if( 0 == iRev )
        {
            /*ATQx, x = A or B*/
            memcpy( pucInfo, gtPcdModuleInfo.aucATQx + 1, gtPcdModuleInfo.aucATQx[0] );
            pucInfo += gtPcdModuleInfo.aucATQx[0];

            /*SAK or AttriB*/
            memcpy( pucInfo, gtPcdModuleInfo.aucSAK + 1, gtPcdModuleInfo.aucSAK[0] ); 
            pucInfo += gtPcdModuleInfo.aucSAK[0];
 
            /*TypeA ATS*/
            if( 'A' == ucCardType )
            {
                memcpy( pucInfo, gtPcdModuleInfo.aucATS, gtPcdModuleInfo.aucATS[0] );
                pucInfo += gtPcdModuleInfo.aucATS[0];
            }
        }
        /*calculating the valid datas length*/
        pucOther[0] = pucInfo - pucOther - 1;
    }
    
    /*for compliant with P80*/
    if( RET_RF_ERR_TIMEOUT == iRev )
    {
        iRev = RET_RF_DET_ERR_NO_CARD;   
    }
    
    /*force to adjust IFSC*/
    if( 1 == gt_c_para.card_buffer_w )gtPcdModuleInfo.uiFsc = gt_c_para.card_buffer_val;
    
    return iRev;
}

/**
 * using current configuration to send and receive.
 *  
 * params:
 *         ucpSrc  : the buffer will be sent
 *         iTxN    : the length of datas sent
 *         ucpDes  : the received buffer
 *         ipRxN   : the length of receiver buffer
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccCmdExchange( unsigned int   iTxN,
                               unsigned char* ucpSrc, 
                               unsigned int*  ipRxN, 
                               unsigned char* ucpDes )
{
    int           iRev = 0;
    unsigned char aucRxBuf[272];
    
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;

    /* change by wanls 20150921*/
	/*为增加ultralight卡的支持，此处判断到用M模式寻卡成功后，并且通过
	PiccSetup设置了picc_cmd_exchange_set_w为1的情况下，将不再判断卡片是否
	处在协议态*/
    if((1 != gt_c_para.picc_cmd_exchange_set_w) 
		|| (MIFARE_STANDARD != gtPcdModuleInfo.ucProtocol))
    {
        /*check if picc has be actived*/
		if( PCD_ACTIVE_PICC != gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NOT_ACT;
    }
	/* add end */
    
    /*check if there are datas in TX-Buffer*/
    if( ( NULL == ucpSrc ) || ( NULL == ucpDes ) || ( 0 == iTxN ) || ( 0 == ipRxN ) )
    {
        return RET_RF_CMD_ERR_INVALID_PARAM;   
    }
    
    /*don't support CID and NAD*/
    gtPcdModuleInfo.ucCidEn = 0;
    gtPcdModuleInfo.ucNadEn = 0;
    
    /*direct transfer, no encapsulation*/
    /* change by wanls 20150921*/
    if(( JISX6319_4_STANDARD == gtPcdModuleInfo.ucProtocol )
		|| ((MIFARE_STANDARD == gtPcdModuleInfo.ucProtocol) 
		    && (1 == gt_c_para.picc_cmd_exchange_set_w)))
	/*change end*/
    {
        iRev = PcdTransCeive( &gtPcdModuleInfo, 
                               ucpSrc, 
                               iTxN, 
                               aucRxBuf, 
                               272 );
        if( 0 == iRev )
        {
            *ipRxN = gtPcdModuleInfo.uiPcdTxRNum;
            memcpy( ucpDes, aucRxBuf, *ipRxN );       
        }
    }
    else
    {/*encapsulation by iso14443-4 half duplex transmission protocol*/
        iRev = ISO14443_4_HalfDuplexExchange( &gtPcdModuleInfo,
                                               ucpSrc,
                                               iTxN,
                                               ucpDes,
                                               ipRxN );
    }
    
    return ConvertHalToApi( iRev );
}

/**
 * send format information according to iso7816-4, and receiving the 
 * response from PICC.
 *  
 * params:
 *         ucCID      : the cid of picc
 *         ptApduSend : the sent data structure
 *         ptApduRecv : the received data structure
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccIsoCommand( unsigned char ucCID, 
                              APDU_SEND*    ptApduSend, 
                              APDU_RESP*    ptApduRecv )
{
    unsigned char aucSendBuf[272];
    int           iTxN = 0;
    unsigned char aucRecvBuf[272];
    int           iRxN = 0;
    
    int           iRev = 0;
    
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;
    
    /*check if picc has be actived*/
    if( PCD_ACTIVE_PICC != gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NOT_ACT;
    
    /*check if parameters are valid*/
    if( NULL == ptApduSend )return RET_RF_ERR_PARAM;
    if( NULL == ptApduRecv )return RET_RF_ERR_PARAM;
    if( ptApduSend->Lc > 255 )return RET_RF_ERR_PARAM;
    if( ucCID > 14 )return RET_RF_ERR_PARAM;
    
    /*parse the parameters*/
    iTxN = 0;
    memcpy( aucSendBuf + iTxN, ptApduSend->Command, 4 );
    iTxN = 4;
    if( ptApduSend->Lc > 0 )/*Lc is present*/
    {
        aucSendBuf[iTxN++] = ( unsigned char )ptApduSend->Lc;
        memcpy( aucSendBuf + iTxN, ptApduSend->DataIn, ptApduSend->Lc );
        iTxN += ptApduSend->Lc;
        if( ptApduSend->Le > 0 )/*Le is present*/
        {
            if( ptApduSend->Le < 256 )aucSendBuf[ iTxN++ ] = ptApduSend->Le;
            else aucSendBuf[ iTxN++ ] = 0x00;
        }
    }
    else
    {
        if( 0 < ptApduSend->Le )/*Lc and Le is absent*/
        {
            if( ptApduSend->Le > 256 )aucSendBuf[ iTxN++ ] = 0x00;
            else aucSendBuf[ iTxN++ ] = ptApduSend->Le;
        }
        else /*Le is present*/
        {
            aucSendBuf[ iTxN++ ] = 0x00;
        }
    }
    
    /*don't support CID and NAD*/
    gtPcdModuleInfo.ucCidEn = 0;
    gtPcdModuleInfo.ucNadEn = 0;
    
    iRev = ISO14443_4_HalfDuplexExchange( &gtPcdModuleInfo,
                                           aucSendBuf,
                                           iTxN,
                                           aucRecvBuf,
                                          &iRxN );
    if( !iRev )
    {
        if( iRxN > 1 )/*tidy the response*/
        {
            ptApduRecv->SWA    = aucRecvBuf[ iRxN - 2 ]; 
            ptApduRecv->SWB    = aucRecvBuf[ iRxN - 1 ];
            ptApduRecv->LenOut = iRxN - 2;
            ptApduRecv->LenOut = ( ptApduRecv->LenOut > ptApduSend->Le ) 
                                  ? ( ptApduSend->Le ) : ( ptApduRecv->LenOut );
            memcpy( ptApduRecv->DataOut, aucRecvBuf, ptApduRecv->LenOut );        
        }
    }
    
    return ConvertHalToApi( iRev );
}

/**
 * remove card operation.
 *  
 * params:
 *         ucMode : removal mode
 *                  'H' or 'h' - only send the halt command
 *                  'R' or 'r' - after sent the halt command, detect if the card is in field. 
 *         ucCID  : card's logical channel, default is zero
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccRemove( unsigned char ucMode, unsigned char ucCID )
{
    unsigned char ucRev  = 0;
    int           iRev   = 0;
    
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;
    
    /*check if picc has be actived*/
    if(   ( PCD_ACTIVE_PICC != gtPcdModuleInfo.uiPcdState )
        &&( PCD_WAKEUP_PICC != gtPcdModuleInfo.uiPcdState )
        &&(( PCD_REMOVE_PICC != gtPcdModuleInfo.uiPcdState ) || (ucMode == 'H') || (ucMode == 'h'))
      )
    {
        return RET_RF_ERR_NOT_ACT;
    }
    
    /*check if parameters are valid*/
    if( ucCID > 14 )return RET_RF_CMD_ERR_INVALID_PARAM;
    
    if(    ( 'H' != ucMode ) && ( 'h' != ucMode )
        && ( 'R' != ucMode ) && ( 'r' != ucMode )
        && ( 'E' != ucMode ) && ( 'e' != ucMode ) 
      )
    {
        return RET_RF_CMD_ERR_INVALID_PARAM;  
    }
    
    /*removal procedure*/
    switch( ucMode )
    {
    case 'H':
    case 'h':
        iRev = PaxHaltCard( &gtPcdModuleInfo );
        if( 0 == iRev )
        {
            gtPcdModuleInfo.uiPollTypeFlag = 0;  
            PcdCarrierOff( &gtPcdModuleInfo ); 
            PcdCarrierOn( &gtPcdModuleInfo );
        }
    break;
    case 'R':
    case 'r':
        iRev = PaxRemoveCard( &gtPcdModuleInfo );
        if( 0 == iRev )
        {
            gtPcdModuleInfo.uiPollTypeFlag = 0; 
            PcdCarrierOff( &gtPcdModuleInfo );  
            PcdCarrierOn( &gtPcdModuleInfo );
        }
        
		/*当Mode='r'时,测试A\B\M卡函数第一次返回0x06,
		第二次返回0x06,移开卡片后再调用返回0x00，移入
		卡片后再调用函数返回0x00,与RC531返回码一致(以前返回时0x03)*/
		/* add by wanls 2012.05.03 */
		gtPcdModuleInfo.uiPcdState = PCD_REMOVE_PICC;
		/* add end */
    break;
    case 'E':/*removal card operation by EMV*/
    case 'e':
        if( PCD_ACTIVE_PICC == gtPcdModuleInfo.uiPcdState )
        {/*subcarrier reset*/
            PcdCarrierOff( &gtPcdModuleInfo );
            PcdCarrierOn( &gtPcdModuleInfo );
            gtPcdModuleInfo.uiPcdState = PCD_REMOVE_PICC;
        }
        
        iRev = EmvRemoval( &gtPcdModuleInfo );
        if( 0 == iRev )/*there isn't card in field*/
        {
            /*all card unregistered*/
			/*gtPcdModuleInfo.uiPollTypeFlag = 0注释掉,防止成功
			移卡后再调用PiccRemove进入死循环 */
            //gtPcdModuleInfo.uiPollTypeFlag = 0;
            //PcdCarrierOff( &gtPcdModuleInfo );
        }
    break;
    default:
    break;
    }
    
    return ConvertHalToApi( iRev );
}

/**
 * loading the configuration of Felica into driver chip.
 *  
 * params:
 *         ucSpeed       : Felica speed parameter( 0 - 212bps, 1 - 424Kbps )
 *         ucModInvert   : inverted modulation
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccInitFelica( unsigned char ucSpeed, unsigned char ucModInvert )
{   
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;

    /* add by wanls 2012.08.29 */
	/*打开载波*/
	PcdCarrierOn( &gtPcdModuleInfo );
	/* add end */
	
    /*Felica mode*/
    gtPcdModuleInfo.ucProtocol = JISX6319_4_STANDARD;
    
    /*disable mifare one crypto1 function*/
    gtPcdModuleInfo.ucM1CryptoEn = 0;
    gtPcdModuleInfo.uiFsc  = 256;
    gtPcdModuleInfo.uiFwt  = 5000;/*47ms*/
    gtPcdModuleInfo.uiSfgt = 50;
    gtPcdModuleInfo.ucTxCrcEnable = 1;/*disable TX CRC*/
    gtPcdModuleInfo.ucRxCrcEnable = 1;/*disable RX CRC*/
    gtPcdModuleInfo.ucEmdEn = 0;

    /* add by wanls 2012.12.25 根据香港的需求设置felica的
	超时时间*/
	if(guiFelicaTimeOut != 0)
	{
	    gtPcdModuleInfo.uiFwt = guiFelicaTimeOut;
	}
	else
	{
	    gtPcdModuleInfo.uiFwt = 5000;/*47ms*/
	}
	/* add end */
    
    /*speed indicator*/
    if( 1 == ucSpeed )
    {
        gtPcdModuleInfo.uiPcdBaudrate = BAUDRATE_424000;   
    }
    else
    {
        gtPcdModuleInfo.uiPcdBaudrate = BAUDRATE_212000; 
    }
    
    /*check if inverted modulation*/
    if( 1 == ucModInvert )
    {
         gtPcdModuleInfo.ucModInvert = 1;   
    }
    else
    {
         gtPcdModuleInfo.ucModInvert = 0;   
    }
    
    gtPcdModuleInfo.uiPcdState = PCD_ACTIVE_PICC;
    
    return RET_RF_OK;
}

/**
 * close carrier and power down.
 *  
 * params:
 *         no params
 * retval:
 *         0 - successfully
 *         others, failure
 */
void PiccClose( )
{
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )
    {
        gtPcdModuleInfo.uiPollTypeFlag = 0;
        return;
    }

	/* add by wanls 2012.05.21 */
	/* 防止连续PiccClose出现死机*/
	if(gtPcdModuleInfo.uiPcdState == PCD_BE_CLOSED)
	{
	    return;
	}
	// add end */

	gtPcdModuleInfo.uiPollTypeFlag = 0;
    gtPcdModuleInfo.uiPcdState = PCD_BE_CLOSED;

    /*close carrier*/
    PcdCarrierOff( &gtPcdModuleInfo );
    
    /*enable chip enter into power down mode to save power consumption*/
    PcdPowerDown( &gtPcdModuleInfo );
    
    return;
}

/**
 * loading the configuration into driver chip and open carrier.
 *  
 * params:
 *         ucMode      :  0 - Read / 1 - Write
 *         ucRegAddr   :  register address
 *         pucOutData  :  i/o data buffer
 * retval:
 *         0 - successfully
 *         others, failure
 */
unsigned char PiccManageReg( unsigned char  ucMode, 
                             unsigned char  ucRegAddr, 
                             unsigned char* pucOutData )
{   
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;
    
    /*read or write the chip register internal*/
    PcdDeviceIO( &gtPcdModuleInfo, !ucMode, ucRegAddr, pucOutData, 1 );
    
    return RET_RF_OK;
}

/**
 * Mifare read crypto1 authenticate operations.
 * 
 * parameters:
 *          ucType         : the Key group, 'A' or 'B'
 *          ucBlkNo        : the block number
 *          pucPwd         : the password( 6 bytes )
 *          pucSerialNo    : the UID( 4 bytes )
 *
 * retval:
 *          0 - successfully
 *          others, error
 *
 */
unsigned char M1Authority( unsigned char  ucType, 
                           unsigned char  ucBlkNo,
                           unsigned char *pucPwd, 
                           unsigned char *pucSerialNo )
{
    int iRet = 0;
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;

    /* add by wanls 2012.03.15 卡片进入协议态，调用认证命令，返回卡片状态错误*/
	if( PCD_ACTIVE_PICC == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_STATUS;
	/* add end */

    /*check if picc has be actived*/
    if( PCD_WAKEUP_PICC != gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NOT_ACT;
    
    /*check if password is null*/
    if( NULL == pucPwd )return RET_RF_ERR_PARAM;

	/*add by wanls 2012.05.03 */
	if(('A' != ucType) && ('a' != ucType) && ('B' != ucType) && ('b' != ucType))
	{
	    return RET_RF_ERR_PARAM;
	}

	if(gtPcdModuleInfo.ucPiccType == PHILIPS_MIFARE_STANDARD_1K)
	{
	    if(ucBlkNo > 63)
	    {
	        return RET_RF_ERR_PARAM;
	    }
	}
	/* add end */
    
    /*check if these SN is same*/
    if( NULL != pucSerialNo )
    {
        if( memcmp( pucSerialNo, gtPcdModuleInfo.aucUid, 4 ) )return RET_RF_ERR_PARAM;/*UID error*/
    }
	iRet = MifareAuthority( &gtPcdModuleInfo, ucType, ucBlkNo, pucPwd );
	return ConvertHalToApi(iRet);
}

/**
 * Mifare read clock operations.
 * 
 * parameters:
 *          ucBlkNo        : the block number
 *          pucBlkValue    : the value of operation(16 bytes)
 *
 * retval:
 *          0 - successfully
 *          others, error
 *
 */
unsigned char M1ReadBlock( unsigned char ucBlkNo, unsigned char *pucBlkValue )
{
    int iRev = 0;
    
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;
    
    /*check if picc has be actived*/
    if( PCD_WAKEUP_PICC != gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NOT_ACT;
    
    /*check if read buffer is valid*/
    if( NULL == pucBlkValue )return RET_RF_ERR_PARAM;

    /*add by wanls 2012.05.03*/
	if(gtPcdModuleInfo.ucPiccType == PHILIPS_MIFARE_STANDARD_1K)
	{
	    if(ucBlkNo > 63)
	    {
	        return RET_RF_ERR_PARAM;
	    }
	}
	/*add end */
    
    /*check if mifare card has been authentication*/
    /*add by wanls 2013.12.10*/
	/*如果是ultralight卡将不进行是否认证判断及扇区有效性判断*/
	if(gtPcdModuleInfo.ucPiccType != PHILIPS_MIFARE_ULTRALIGHT)
	/*add end */
	{

	    /* change by wanls 2012.06.04 */
        if(( !gtPcdModuleInfo.ucM1CryptoEn ) 
		    || ((ucBlkNo / 4) != (gtPcdModuleInfo.ucM1AuthorityBlkNo / 4)))
	    {
	        return RET_RF_ERR_NO_AUTH;
        }
	}
	/* change end */
    
    iRev = MifareOperate( &gtPcdModuleInfo, 
                           'r', 
                           ucBlkNo, 
                           pucBlkValue, 
                           ucBlkNo );
    
    return ConvertHalToApi( iRev );
}

/**
 * Mifare write block operations.
 * 
 * parameters:
 *          ucBlkNo        : the block number
 *          pucBlkValue    : the value of operation(16 bytes)
 *
 * retval:
 *          0 - successfully
 *          others, error
 *
 */
unsigned char M1WriteBlock( unsigned char ucBlkNo, unsigned char *pucBlkValue )
{
    int iRev = 0;
    
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;
    
    /*check if picc has be actived*/
    if( PCD_WAKEUP_PICC != gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NOT_ACT;
    
    /*check if write buffer is valid*/
    if( NULL == pucBlkValue )return RET_RF_ERR_PARAM;
	/* add by wanls 2012.05.06*/
	if(gtPcdModuleInfo.ucPiccType == PHILIPS_MIFARE_STANDARD_1K)
	{
	    if(ucBlkNo > 63)
	    {
	        return RET_RF_ERR_PARAM;
	    }
	}
	/* add end */
    
    /*check if mifare card has been authentication*/

	/*add by wanls 2013.12.10*/
	/*如果是ultralight卡将不进行是否认证判断及扇区有效性判断*/
	if(gtPcdModuleInfo.ucPiccType != PHILIPS_MIFARE_ULTRALIGHT)
	/*add end */
	{

	
        /* change by wanls 2012.06.04 */
        if(( !gtPcdModuleInfo.ucM1CryptoEn ) 
		    || ((ucBlkNo / 4) != (gtPcdModuleInfo.ucM1AuthorityBlkNo / 4)))
	    {
	        return RET_RF_ERR_NO_AUTH;
        }
	}
	/* change end */
    
    iRev = MifareOperate( &gtPcdModuleInfo, 
                           'w', 
                           ucBlkNo, 
                           pucBlkValue, 
                           ucBlkNo );
    
    return ConvertHalToApi( iRev );
}

/**
 * Mifare operations, inc, dec or backup.
 * 
 * parameters:
 *          ucType         : operation type,
 *                            '+' - increase operation
 *                            '-' - decrease operation 
 *                            '>' - backup operation
 *          ucBlkNo        : the block number
 *          pucValue       : the value of operation
 *          ucUpdateBlkNo  : backup operation destinct block number
 *
 * retval:
 *          0 - successfully
 *          others, error
 *
 */
unsigned char M1Operate( unsigned char  ucType, 
                         unsigned char  ucBlkNo, 
                         unsigned char *pucValue, 
                         unsigned char  ucUpdateBlkNo )
{
    int iRev = 0;
    
    /*check if the RF module is already ok*/
    if( PCD_CHIP_ABNORMAL == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_CHIP_ABNORMAL;
    
    /*check if module is opened*/
    if( PCD_BE_CLOSED == gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NO_OPEN;
    
    /*check if picc has be actived*/
    if( PCD_WAKEUP_PICC != gtPcdModuleInfo.uiPcdState )return RET_RF_ERR_NOT_ACT;
    
    /*check if operation symbol is valid*/
    if(    '+' != ucType 
        && '-' != ucType
        && '>' != ucType 
      )
    {
        return RET_RF_ERR_PARAM;/*unkown operation symbol*/   
    }
    
    /*check if block value buffer is valid*/
    if( NULL == pucValue )return RET_RF_ERR_PARAM;

	/* add by wanls 2012.05.03 */
	if(gtPcdModuleInfo.ucPiccType == PHILIPS_MIFARE_STANDARD_1K)
	{
	    if((ucBlkNo > 63) || (ucUpdateBlkNo > 63))
	    {
	        return RET_RF_ERR_PARAM;
	    }
	}
	/* add end */
    
    /* check if the source block and the destinct block is in same sector and 
     * if the source block or the destinct block is the last block in sector. 
     */
    if(    ( ( ucBlkNo / 4 ) != ( ucUpdateBlkNo / 4 ) )
        || ( 0 == ( ucBlkNo + 1 ) % 4 ) 
        || ( 0 == ( ucUpdateBlkNo + 1 ) % 4 )
      )
    {
        return RET_RF_ERR_PARAM;
    }
    
    /*check if mifare card has been authentication*/

	/* change by wanls 2012.06.04 */
    if(( !gtPcdModuleInfo.ucM1CryptoEn ) 
		|| ((ucBlkNo / 4) != (gtPcdModuleInfo.ucM1AuthorityBlkNo / 4)))
	{
	    return RET_RF_ERR_NO_AUTH;
    }
	/* change end */
    
    iRev = MifareOperate( &gtPcdModuleInfo, 
                           ucType, 
                           ucBlkNo, 
                           pucValue, 
                           ucUpdateBlkNo );
    
    return ConvertHalToApi( iRev );
}

