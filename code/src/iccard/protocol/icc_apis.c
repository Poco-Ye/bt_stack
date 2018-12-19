/**
 * =============================================================================
 * Following the principles of EMV, this document is aimed at completing 
 * the funtions for application development.
 * 
 * creator:  Liubo
 * data :  2009-10-22
 * applicability :  universal
 * Email: liubo@paxsz.com
 * =============================================================================
 * Copyright (C) PAX Computer Technology(SHEN ZHEN) co.,ltd
 * =============================================================================
 */
#include "icc_core.h"
#include "icc_errno.h"
#include "..\hardware\icc_hard_async.h"
#include "icc_apis.h"
#include "string.h"

/* the information for global logic devices */
struct emv_core gl_emv_devs[ EMV_TERMINAL_NUM ];
int             gl_dev_conf[ EMV_TERMINAL_NUM ];

/*
 * those global variables are used in other modules
 * rather than this for proccessing the historical problem.
 */
volatile unsigned int  k_Ic_Timer_Enable = 0;
volatile unsigned char k_CardStatus[ 4 ] = {0,};
extern   unsigned char k_ICC_CardInSert;

/**
 * ===================================================================
 * Get the Version Information of this SCI device driver module
 * -------------------------------------------------------------------
 * parameter： 
 * -------------------------------------------------------------------
 *       version : ouput the Version Infomation of module. 
 *                 the Formal of Version Infomation :
 *                      ESS-YYMMDD-R/D 
 *                 E      : Version Number of EMV Driver
 *                 SS     : Version Number of ISO Driver
 *                 YYMMDD : Compilation Data: year\month\day
 *                 R/D    : the Release Attribute
 *                           R - Release
 *                           D - Debug
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *     none
 * -------------------------------------------------------------------
 * notice：
 *	For the specific content on the requirement and the format 
 *	of version Information, please refer to the relating principle.
 * ===================================================================
 */
void SciGetVer(char *version )
{
	strcpy( version, SM_VERSION_SN );
	strcat( version, "-" );
	strcat( version, SM_VERSION_DATE );
	strcat( version, "-" );
	strcat( version, SM_VERSION_FLAG );

	return;
}

/**
 * ===================================================================
 * The HardWare Initial For SCI
 * -------------------------------------------------------------------
 * paramter：
 * -------------------------------------------------------------------
 *      none
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      none
 * ===================================================================
 */
void s_IccInit()
{
	int  i  = 0;

	sci_core_register( gl_emv_devs, EMV_TERMINAL_NUM );
	for( i = 0; i < EMV_TERMINAL_NUM; i++ )
	{
		gl_dev_conf[ i ]                = 0;
		memset( &( gl_emv_devs[ i ] ), '\0', sizeof( struct emv_core ) );
		gl_emv_devs[ i ].terminal_ch    = i;
		gl_emv_devs[ i ].terminal_auto  = 1;
		gl_emv_devs[ i ].terminal_fi    = 372;
		gl_emv_devs[ i ].terminal_di    = 1;
		gl_emv_devs[ i ].terminal_state = EMV_IDLE;
		gl_emv_devs[ i ].terminal_implict_fi   = 372;
		gl_emv_devs[ i ].terminal_implict_di   = 1;

		gl_emv_devs[ i ].terminal_cgt  = 12;
		gl_emv_devs[ i ].terminal_cwt  = 10080;
		gl_emv_devs[ i ].terminal_bwt  = 971;
		gl_emv_devs[ i ].terminal_wwt  = 10080;
		gl_emv_devs[ i ].terminal_ifs  = 254;
		gl_emv_devs[ i ].emv_card_ifs  = 32;
	}
	
	sci_hard_init();

	return;
}

/**
 * ===================================================================
 * To check usage status of all card channel in the kernel
 * parameter:
 *      none
 *
 * return:
 *      > 0 - the channel is occupied
 *      0   - the channel is idle
 */
int IccPowerStatus()
{
	int i      = 0;   
	int status = 0;

	for( i = 0; i < EMV_TERMINAL_NUM; i++ )
	{
		if( EMV_IDLE != gl_emv_devs[ i ].terminal_state )
		{
			status |= 1 << i; 
		}
	}

	return status;
}

/**
 * ===================================================================
 * card initialize, get ATR, parse ATR, application parameter
 * -------------------------------------------------------------------
 * parameter:
 * -------------------------------------------------------------------
 *     [input]slot:
 * 		        bit[2:0] : slot number(0~7). 
 *              bit[4:3] : For Voltage Operation 
 *                            00 - 5V; 
 *                            01 - 1.8V; 
 *                            10 - 3V; 
 *                            11 - 5V
 *              bit[5]  :  For PPS condition
 *                            0  - Not Supported
 *                            1  - Supported
 *              bit[6]  :  For Data Rate   
 *                            0 - 9600bps 
 *                            1 - 38400bps
 *              bit[7]   : For Specification 
 *                            0 - EMV; 
 *                            1 - ISO7816-3 
 * -------------------------------------------------------------------
 *      [output]ATR:  
 *              ATR[ 0 ]     : the valid number of bytes of ATR in buffer
 *              ATR[ 1 : n ] : Data buffer of ATR.
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 * 		please refer to the "Error Code List" below
 * ===================================================================
 */

unsigned char  IccInit( unsigned char slot, unsigned char *ATR )
{
	int                  channel = slot & 0x07;
	struct emv_core     *pdev;
	struct emv_atr       su_atr;
	int                  state     = 0;
	unsigned char        status    = 0;

	if( 0 == sci_isvalid_channel( channel ) )
	{
		return SLOTERR;
	}
	if( NULL == ATR )
	{
		return FUN_PARAERR;   
	}

	channel = sci_select_slot( slot );
	pdev = &( gl_emv_devs[ channel ] );
	pdev->terminal_ch = channel;

	if( pdev->terminal_open )
	{
		sci_emv_hard_power_dump( pdev );
	}

	status = IccDetect( 0 ); 
	if( 0 == channel && 0 != status )
	{
		return CARD_OUT;
	}

	sci_disturb_interruption( pdev, 0 );/* disable the interruption of communication jamming */

	if( 0 == gl_dev_conf[ channel ] ) /* Analyse the parameter of each corresponding channel  */
	{ 
		switch( ( 3 << 3 ) & slot )/*Voltage*/
		{
			case ( 1 << 3 ):/*Bug, 2011-4-20 Liubo*/
				pdev->terminal_vcc = 1800;
			break;
			
			case ( 2 << 3 ):/*Bug, 2011-4-20 Liubo*/
				pdev->terminal_vcc = 3000;
			break;
			
			default:
				pdev->terminal_vcc = 5000;
			break;
		}

		if( ( 1 << 5 ) & slot )/*PPS support*/
		{
			pdev->terminal_pps = 1;
		}
		else
		{
			pdev->terminal_pps = 0;
		}

		pdev->terminal_fi         = 372;
		pdev->terminal_implict_fi = 372;
		if( ( 1 << 6 ) & slot )/*Speed*/
		{
			pdev->terminal_di         = 4;
			pdev->terminal_implict_di = 4;
		}
		else
		{
			pdev->terminal_di         = 1;
			pdev->terminal_implict_di = 1;
		}
	    
		if( ( 0 ==  ( slot & 0x07 ) ) && ( 0 == ( slot & ( 1 << 7 ) ) ) )
		{
			pdev->terminal_spec = 0;/*EMV Mode*/
		}
		else
		{
			pdev->terminal_spec = 1;/*ISO Mode*/
		}
	}

	gl_dev_conf[ channel ] = 0;

	pdev->terminal_ifs  = 254;
	pdev->terminal_pcb  = 0x00;
	pdev->terminal_ipcb = 0x00;
	pdev->emv_card_pcb  = 0x00;
	pdev->emv_card_ipcb = 0x00;
	pdev->terminal_igt  = 64;
	pdev->terminal_mode = 0; /* asynchronize card */

	sci_emv_hard_power_pump( pdev );/*power on and activation.*/	
	sci_emv_hard_cold_reset( pdev );

	/*Get ATR.*/
	if( 0 == ( state = sci_emv_atr_analyser( pdev, &su_atr, ATR )  ) )
	{
		if( 0 == pdev->terminal_spec )
		{
			state = sci_emv_atr_parse( pdev, &su_atr );
		}
		else
		{
			state = sci_iso_atr_parse( pdev, &su_atr );
		}

		DelayMs(5); /*at particular card with TCK in T=0,wait for extra tck */

		if( 0 == state )
		{
			if( pdev->terminal_pps )/*if cold reset is ok, then PPS*/
			{
				state = sci_iso_pps_procedure( pdev, &su_atr );
				if( state )
				{
					sci_emv_hard_power_dump( pdev );
					sci_disturb_interruption( pdev, 1 );

					return adjust_return_value_to_monitor( state ); 
				}
			}   
		}
	    
		if( 0 != state )
		{
			sci_emv_hard_warm_reset( pdev );/*warm reset.*/
			
			if( 0 == ( state = sci_emv_atr_analyser( pdev, &su_atr, ATR ) ) )
			{
				if( 0 == pdev->terminal_spec )
				{
					state = sci_emv_atr_parse( pdev, &su_atr );
				}
				else
				{
					state = sci_iso_atr_parse( pdev, &su_atr );
				}
				
				if( 0 != state )
				{
					sci_emv_hard_power_dump( pdev );
					sci_disturb_interruption( pdev, 1 );

					return adjust_return_value_to_monitor( state );
				}
				else
				{
					DelayMs(5); /*at particular card with TCK in T=0,wait for extra tck */
					if( pdev->terminal_pps )/*if warm reset is ok, then PPS*/
					{
						state = sci_iso_pps_procedure( pdev, &su_atr );
						if( state )
						{
							sci_emv_hard_power_dump( pdev );
							sci_disturb_interruption( pdev, 1 );

							return adjust_return_value_to_monitor( state ); 
						}
					}   	
				}
			}
			else
			{
				sci_emv_hard_power_dump( pdev );
				sci_disturb_interruption( pdev, 1 );

				return adjust_return_value_to_monitor( state );
			}
		}
	}
	else
	{
		sci_emv_hard_power_dump( pdev );
		sci_disturb_interruption( pdev, 1 );

		return adjust_return_value_to_monitor( state );
	}
	
	/* 2014-06-26,xuwt: 根据ISO7816，一个数据序列在最后一个数据的起始位的12etu后结束。
	 *（is completed 12 etu after the leading edge of the last character of the sequence.）
	 *而我们的数据接收一般到10etu就认为已经完全收到了。这对反向延时保护计算有影响。增加
	 *以下延时，等待数据序列结束。
	 */
	DelayMs(3);

	/*IFSD*/
	if( 1 == pdev->terminal_ptype )
	{
		if( 0 != ( state = sci_emv_t1_ifsd_request( pdev ) ) )
		{
		    sci_emv_hard_power_dump( pdev );
		    sci_disturb_interruption( pdev, 1 );

		    return adjust_return_value_to_monitor( state );
		}
		
	}
	sci_disturb_interruption( pdev, 1 );

	return 0;
}

/**
 * ===================================================================
 * Data Exchange between IC card and Terminal
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input] slot      :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * [input] ApduSend  : a pointer to the command struct
 * 			that the card will send
 * -------------------------------------------------------------------
 * [output] ApduRecv  : a pointer to the response struct 
 * 			 that the card have recevied  
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 * 	please refer to "error code list"
 * ===================================================================
 */
unsigned char IccIsoCommand(unsigned char slot, APDU_SEND *ApduSend, APDU_RESP *ApduRecv)
{
	struct emv_core     *pdev;
	int                  channel   = slot & 0x07;
	struct emv_apdu_req  emv_req;
	struct emv_apdu_rsp  emv_rsp;
	int                  emv_state = 0;
	unsigned char        status    = 0;
	
	if( 0 == sci_isvalid_channel( channel ) )
	{
		return SLOTERR;
	}
	if( NULL == ApduSend || NULL == ApduRecv )
	{
		return FUN_PARAERR;
	}
	if( ApduSend->Lc > 512 )
	{
		return DATA_LENTHERR;
	}
    
	channel = sci_select_slot( slot );
	pdev = &( gl_emv_devs[ channel ] );
	pdev->terminal_ch = channel;
    
/*
* detect user card whether is in slot or not?
*/
	status = IccDetect( 0 );
	if( 0 == channel && 0 != status )
	{
		return CARD_OUT;
	}

	/* As the requestment from Test Department, We change the order between
	 * the action that check whether the card is in socket or not and the
	 * action that check whether the card'd been initialized or not
	 * 2011-03-23 
	 */  
	if( EMV_READY != pdev->terminal_state )
	{
		return NO_INITERR;
	}
	
	sci_disturb_interruption( pdev, 0 );

	memcpy( emv_req.cmd,  ApduSend->Command, 4 );
	emv_req.lc = ApduSend->Lc % 256;
	emv_req.le = ( ApduSend->Le > 256 ) ? 256 : ( ApduSend->Le );
	memcpy( emv_req.data, ApduSend->DataIn, emv_req.lc );
	memset( &emv_rsp, '\0', sizeof( struct emv_apdu_rsp ) );

	if( 0 == pdev->terminal_ptype )
	{ 
		emv_state = sci_emv_t0_exchange( pdev, &emv_req, &emv_rsp );
	}
	else
	{
		emv_state = sci_emv_t1_exchange( pdev, &emv_req, &emv_rsp );
	}

	memset( ApduRecv, '\0', sizeof( APDU_RESP ) );
	if( !emv_state )
	{
		ApduRecv->SWA    = emv_rsp.swa;
		ApduRecv->SWB    = emv_rsp.swb;
		ApduRecv->LenOut = emv_rsp.len;
		memcpy( ApduRecv->DataOut, emv_rsp.data, emv_rsp.len );
	}

	sci_disturb_interruption( pdev, 1 );
	
	if((0 == channel) && (0 != emv_state))
	{
		status = IccDetect( 0 );
		if(0 != status )
		{
			return CARD_OUT;
		}
	}
    
	return adjust_return_value_to_monitor( emv_state );
}

/**
 * ===================================================================
 * halt the IC card, implement the operation of Power Dump for Card 
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to  function "IccInit" 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      none.
 * ===================================================================
 */
void IccClose(unsigned char slot)
{
	struct emv_core *pdev;
	int              channel = slot & 0x07;

	if( 0 == sci_isvalid_channel( channel ) )
	{
		return;
	}
	channel = sci_select_slot( slot );
	pdev = &( gl_emv_devs[ channel ] );
	pdev->terminal_ch = channel;

	sci_emv_hard_power_dump( pdev );

	return;
}

/**
 * ===================================================================
 * To set the flag that indicate whether the terminal will automatically
 * get the remaining data from the card by sending "GET RESPONSE" command
 * or not 
 * -------------------------------------------------------------------
 * parameter： 
 * -------------------------------------------------------------------
 * [input]  slot     :  please refer to  function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  autoresp :  flag
 *                     0 - the terminal get the remaining data from the card
 *                     	   by sending "GET RESPONSE" command
 *                     1 - no auto
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      none
 * -------------------------------------------------------------------
 * notice：
 * -------------------------------------------------------------------
 *     It just be effective for the card with only T=0 protocol
 * ===================================================================
 */
void IccAutoResp( unsigned char slot, unsigned char autoresp )
{
	struct emv_core *pdev;
	int              channel = slot & 0x07;
    
	if( 0 == sci_isvalid_channel( channel ) )
	{
		return;
	}
	
	channel = sci_select_slot( slot );
	pdev = &( gl_emv_devs[ channel ] );
	pdev->terminal_ch = channel;
	
	if( 1 == autoresp )
	{
		pdev->terminal_auto = 1;
	}
	
	if( 0 == autoresp )
	{
		pdev->terminal_auto = 0;
	}
    
	return;
} 

/**
 * ===================================================================
 * To check whether the card is in socket or not 
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 * 		pease refer to the "Error Code List"
 * ===================================================================
 */
unsigned char IccDetect( unsigned char slot )
{
	struct emv_core *pdev;
	int              channel     = slot & 0x07;
	int              emv_state   = 0;
	unsigned char    state       = 0;

	if( 0 == sci_isvalid_channel( channel ) )
	{
		return SLOTERR;   
	}

	channel = sci_select_slot( slot );
	pdev = &( gl_emv_devs[ channel ] );
	pdev->terminal_ch = channel;

	sci_disturb_interruption( pdev, 0 );
	if( 0 == gl_dev_conf[ channel ] && ( 0 != channel ) )
	{ 
		switch( ( 3 << 3 ) & slot )/*Voltage*/
		{
			case (1 << 3):
				pdev->terminal_vcc = 1800;
			break;

			case (2 << 3):
				pdev->terminal_vcc = 3000;
			break;

			default:
				pdev->terminal_vcc = 5000;
			break;
		}

		if( ( 1 << 5 ) & slot )/*PPS support*/
		{
			pdev->terminal_pps = 1;
		}
		else
		{
			pdev->terminal_pps = 0;
		}

		pdev->terminal_fi    = 372;
		if( ( 1 << 6 ) & slot )/*Speed*/
		{
			pdev->terminal_di = 4;
		}
		else
		{
			pdev->terminal_di = 1;
		}

		/*the specification of parse ATR*/
		if( 0 == ( slot & ( 1 << 7 ) ) )
		{
			pdev->terminal_spec = 0;/*EMV Mode*/
		}
		else
		{
			pdev->terminal_spec = 1;/*ISO Mode*/
		}
	}

	if( 1 == sci_hard_detect( pdev ) )
	{
		/*Enable user alarm interruption.*/
		sci_disturb_interruption( pdev, 1 );
		return 0;
	}

	sci_disturb_interruption( pdev, 1 );

	return 0xff;
}

/**
 * ===================================================================
 * parameters relating to card initialization setting
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  please refer to the relating description of
 * 			the function named "IccInit"    
 * -------------------------------------------------------------------
 * [input]ParaConfig  : analyse as bitmap, usage of bitmap showing as below
 *              #VCC_5000MV      5000mV
 *              #VCC_3000MV      3000mV
 *              #VCC_1800MV      1800mV
 *              #BAUD_NORMAL     Fi = 372, Di = 1
 *              #BAUD_M2         Fi = 372, Di = 2
 *              #BAUD_M4         Fi = 372, Di = 4
 *              #SUP_PPS         PPS
 *              #SPEC_ISO        ISO mode                         
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *       0 - success 
 *       others, failure
 * -------------------------------------------------------------------
 * warning：
 *       1) The config should be made before the funtion "IccInit"
 *          to be called, or won't take effect
 *       2) The config may be made as the example below
 *         eg.
 *         IccChannelConfig( 0,  VCC_3000MV | BAUD_M4 | SUP_PPS | SPEC_ISO );
 *       3) The config of Speed Mode will be ignored if the mode of specification
 *       assigns EMV
 *	
 * ===================================================================
 */
int IccChannelConfig( unsigned char slot, unsigned int ParaConfig )
{
	struct emv_core  *pdev    = 0;
	int               channel = slot & 0x07;
    
	if( 0 == sci_isvalid_channel( channel ) )
	{
		return SLOTERR;   
	}
    
	channel = sci_select_slot( slot );
	pdev    = &( gl_emv_devs[ channel ] );
	if( pdev->terminal_open )
	{
		return DEV_OTHERERR;   
	}
    
	pdev->terminal_fi         = 372;
	pdev->terminal_implict_fi = 372;
	
	if( pdev->terminal_ch )
	{ 	/*SAM order by ISO7816-3 mode*/
		ParaConfig |= SPEC_ISO;
	}

	if( ParaConfig & SPEC_ISO )
	{	/*ISO7816-3 framework*/
		pdev->terminal_spec = 1; /*ISO mode*/
        
		if( ParaConfig & SUP_PPS )
		{
			pdev->terminal_pps = 1;
		}
		else
		{
			pdev->terminal_pps = 0;
		}
        
		switch( ParaConfig & BAUD_MASK )
		{
			case BAUD_M2:
				pdev->terminal_di = 2;
				pdev->terminal_implict_di = 2;
			break;

			case BAUD_M4:
				pdev->terminal_di = 4;
				pdev->terminal_implict_di = 4;
			break;

			default:
				pdev->terminal_di = 1;
				pdev->terminal_implict_di = 1;
			break;
		}
        
		switch( ParaConfig & VCC_MASK )
		{
			case VCC_1800MV:
				pdev->terminal_vcc = 1800;
			break;
			
			case VCC_3000MV:
				pdev->terminal_vcc = 3000;
			break;
			
			default:
				pdev->terminal_vcc = 5000;
			break;
		}
	}
	else
	{ 	/*EMV framework*/
		pdev->terminal_spec = 0; /*EMV mode*/
		switch( ParaConfig & VCC_MASK )
		{
			case VCC_1800MV:
				pdev->terminal_vcc = 1800;
			break;
			
			case VCC_3000MV:
				pdev->terminal_vcc = 3000;
			break;
			
			default:
				pdev->terminal_vcc = 5000;
			break;
		}
		pdev->terminal_di     	  = 1;
		pdev->terminal_implict_di = 1;
	}
	
	gl_dev_conf[ channel ] = 1;

	return 0;
}

/**
 * ===================================================================
 * to get the flag indicating whether the card slot is occupied or not
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     : refer to  function "IccInit"
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *     1 - occupied / 0 - idle
 * ===================================================================
 */
unsigned char Read_CardSlotInfo( unsigned char slot )
{
	struct emv_core *pdev;
    
	if( 0 == sci_isvalid_channel( slot ) )
	{
		return SLOTERR;
	}

	slot = sci_select_slot( slot );
	pdev = &( gl_emv_devs[ slot ] );
	pdev->terminal_ch = slot;
	
	return pdev->terminal_open;
}

/**
 * ===================================================================
 * to set the flag indicating whether the card slot is occupied or not
 *
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to the description of the function 
 * 			named "IccInit"  
 * -------------------------------------------------------------------
 * [input]  slotopen :  1 - occupied / 0 - idle
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *     none
 * ===================================================================
 */
void Write_CardSlotInfo( unsigned char slot, unsigned char slotopen )
{
	struct emv_core *pdev;
    
	if( 0 == sci_isvalid_channel( slot ) )
	{
		return;
	}

	slot = sci_select_slot( slot );
	pdev = &( gl_emv_devs[ slot ] );
	pdev->terminal_ch   = slot;
	pdev->terminal_mode = 1;/* Synchronize card mode */
	
	if( slotopen )
	{
		pdev->terminal_open = 1;
	}
	else
	{
		pdev->terminal_open = 0;   
	}
	
	return;
}

/**
 * ===================================================================
 * To get the number of times the card plug in or out after the terminal
 * power up
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 *      none
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      number of times the card plug in or out
 * ===================================================================
 */
int IccGetInsertCnt()
{
	return sci_alarm_count();   
}




