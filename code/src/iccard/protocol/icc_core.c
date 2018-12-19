/**
 * ===================================================================
 * Following the principle of EMV4.2 2008, this document is aimed at
 * implementing the funtion of data exchange which mainly include
 * the T=0/1 protocol and IFSD requestment protocol.
 * 
 * For the implementation of the ISO/WIE 7816-3 specification , please
 * refer to "emv_patch.c"
 * -------------------------------------------------------------------
 * creator: Liubo
 * date: 2009-8-7
 * email: liubo@paxsz.com
 * -------------------------------------------------------------------
 * CopyRight (C) PAX Computer Technology(ShenZhen) Co., Ltd. 2008-2009
 * ===================================================================
 */
#include "icc_core.h"
#include "..\hardware\icc_queue.h"
#include "icc_errno.h"
#include "icc_apis.h"
#include "string.h"


/* Clock Rate Conversion Integer(Fi) List providing in ISO7816-3 */
const int FI[ ] = { 372, 372, 558, 744,  1116, 1488, 1860, 0,
                    0,   512, 768, 1024, 1536, 2048, 0,    0   };

/* Baud Rate Adjustment Integer(Di) List  providing in ISO7816-3 */
const int DI[ ] = { 0, 	 1,   2,   4,    8,    16,   32,   64, 
                    12,  20,  0,   0,    0,    0,    0,    0   };


/* ===================================================================
 * To get the corresponding Fi according to the providing index
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]index : the index of Fi
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *      Clock Rate Conversion Interger
 * ===================================================================
 */
int fi_const_table( int index )
{
	return FI[ index ];
}

/* ===================================================================
 * To get the corresponding Di according to the providing index
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]index : the index of Di
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *      Baud Rate Adjustment integer
 * ===================================================================
 */
int di_const_table( int index )
{
	return DI[ index ];    
}

/* ===================================================================
 * To get the corresponding index according to the value of Fi 
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]fi : Clock Rate Conversion Interger
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *      the index of Fi
 * ===================================================================
 */
int fi_const_index( int fi )
{
	int i = 0;

	for( i = 1; i < 16; i++ )
	{
		if( FI[ i ] == fi )
		{
			    if( 0 == i ) i = 1;
			    return i;
		}   
	}

	return 0;
}

/* ===================================================================
 * To get the corresponding index according to the value of Di
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]di : Baud Rate Adjustment Interger
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *      the index of Di
 * ===================================================================
 */
int di_const_index( int di )
{
	int i = 0;

	for( i = 1; i < 16; i++ )
	{
		if( DI[ i ] == di )
		{
			return i;
		}   
	}

	return 0;
}
/**
 * ===================================================================
 * To confirm the card mode when proccessing the data exchange
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]cmd_case : command case when T=0
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *       none
 * ===================================================================
 */
static void emv_t0_cmd_status(int cmd_case, struct emv_core *terminal)
{
	if(cmd_case > 2)
	{
		terminal->card_trans_state = CRAD_RECEI_DATA;
	}
	else
	{
		terminal->card_trans_state = CRAD_TRANS_DATA;
	}

	return;
}

/**
 * ===================================================================
 * For compatible with the old version, this function is aimed at 
 * converting the error code from the definition under new architecture
 * to the definition in the Monitor program
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 *  [input]errno  : the error code under the new architecture
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 * the error code that definition in the Monitor program       
 * ===================================================================
 */
unsigned char  adjust_return_value_to_monitor( int errno )
{
	unsigned char state = 0;

	switch( errno )
	{
		case 0:
			state = 0;
		break;
	
		case ICC_ERR_DEVNO: 	/*common process*/
			state = SLOTERR;
		break;

		case ICC_ERR_CHIP:
			state = DEV_OTHERERR;
		break;

		case ICC_ERR_NOCARD:
			state = CARD_OUT;
		break;

		case ICC_ERR_PARAM:
			state = FUN_PARAERR;
		break;

		case ICC_ERR_POWER:
			state = DEV_OTHERERR;
		break;

		case ICC_ERR_OCCUPY:
			state = DEV_OTHERERR;
		break;

		case ICC_ERR_UNINIT:
			state = NO_INITERR;
		break;

		case ICC_ERR_PARS:
			state = PARERR;
		break;

		/*Answer to reset process*/
		case ICC_ERR_ATR_SWT:
			state = ATR_TIMEOUT;
		break;

		case ICC_ERR_ATR_TWT:
			state = ATR_TIMEOUT;
		break;

		case ICC_ERR_ATR_CWT:
			state = ATR_TIMEOUT;
		break;

		case ICC_ERR_ATR_TS:
			state = ATR_TSERR;
		break;

		case ICC_ERR_ATR_TA1:
			state = ATR_TA1ERR;
		break;

		case ICC_ERR_ATR_TB1:
			state = ATR_TB1ERR;
		break;

		case ICC_ERR_ATR_TC1:
			state = ATR_TC1ERR;
		break;

		case ICC_ERR_ATR_TD1:
			state = ATR_TD1ERR;
		break;

		case ICC_ERR_ATR_TA2:
			state = ATR_TA2ERR;
		break;

		case ICC_ERR_ATR_TB2:
			state = ATR_TB2ERR;
		break;

		case ICC_ERR_ATR_TC2:
			state = ATR_TC2ERR;
		break;

		case ICC_ERR_ATR_TD2:
			state = ATR_TD2ERR;
		break;

		case ICC_ERR_ATR_TA3:
			state = ATR_TA3ERR;
		break;

		case ICC_ERR_ATR_TB3:
			state = ATR_TB3ERR;
		break;

		case ICC_ERR_ATR_TC3:
			state = ATR_TC3ERR;
		break;

		case ICC_ERR_ATR_TCK:
			state = ATR_TCKERR;
		break;

		/*T0 exchange process*/
		case ICC_ERR_T0_WWT:
			state = T0_TIMEOUT;
		break;
		
		case ICC_ERR_T0_CREP:
			state = T0_MORERECEERR;
		break;

		case ICC_ERR_T0_PROB:
			state = T0_INVALIDSW;
		break;

		/*T1 exchange process*/
		case ICC_ERR_T1_BREP:
			state = T1_MOREERR;
		break;

		case ICC_ERR_T1_BWT:
			state = T1_BWTERR;
		break;

		case ICC_ERR_T1_CWT:
			state = T1_CWTERR;
		break;

		case ICC_ERR_T1_NAD:
			state = T1_NADERR;
		break;

		case ICC_ERR_T1_PCB:
			state = T1_PCBERR;
		break;

		case ICC_ERR_T1_LRC:
			state = T1_EDCERR;
		break;

		case ICC_ERR_T1_LEN:
			state = T1_LENGTHERR;
		break;

		case ICC_ERR_T1_SRL:
			state = T1_IFSCERR;
		break;

		case ICC_ERR_T1_SRC:
			state = T1_PCBERR;
		break;

		case ICC_ERR_T1_SRA:
			state = T1_ABORTERR;
		break;

		/*PPS process*/
		case ICC_ERR_PPSS:
			state = PPSS_ERR;
		break;

		case ICC_ERR_PPS1:
			state = PPS1_ERR;
		break;

		case ICC_ERR_PCK:
			state = PCK_ERR;
		break;

		default:
			state = DEV_OTHERERR;
		break;
	}

	return state;  
}
/* ===================================================================
 * To confirm the Case Number as illustrating in the EMV or ISO7816-3 
 * protocol which help process the Data exchange when T=0.
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]apdu_req : structure pointer of APDU command
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *        1 - CASE1
 *        2 - CASE2
 *        3 - CASE3 
 *        4 - CASE4
 * ===================================================================
 */

static int emv_tell_case( struct emv_apdu_req *apdu_req )
{
	int case_type = 0;
	
	if( apdu_req->lc )
	{
		if( apdu_req->le )
		{
			case_type = 4;
		}
		else
		{
			case_type = 3;
		}
	}
	else
	{
		if( apdu_req->le )
		{
			case_type = 2;
		}
		else
		{
			case_type = 1;
		}
	}
	
	return case_type;
}

/* ===================================================================
 * to compute the LRC appearing in the Epilogue field of Block Frame 
 * in the T=1 protocol.
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input/output]t1_block : input block  : Prologue field + information
 * 				           field  + Epilogue field
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *   always 0
 * ===================================================================
 */
static int emv_t1_compute_lrc( struct emv_t1_block *t1_block ) 
{
	int i = 0;
	
	t1_block->lrc = t1_block->nad ^ t1_block->pcb ^ t1_block->len;
	for( i = 0; i <  t1_block->len; i++ )
	{ 
		t1_block->lrc ^= t1_block->data[ i ]; 
	}
	
	return 0;
}

/* ===================================================================
 * According to the specification of APDU protocol, We will format
 * the data in a APDU structure to character string
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]apdu_req : command structure pointer abiding by the APDU specification
 * -------------------------------------------------------------------
 * [output]buf      : the character string formatted
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *        the number of bytes valid in the APDU data buffer
 * ===================================================================
 */
static int emv_t1_extract( struct emv_apdu_req *apdu_req, unsigned char *buf )
{
	unsigned char  *pbuf;
	unsigned char   ch   = '\0';
	
	pbuf	= buf;
	memcpy( pbuf, apdu_req->cmd, 4 ); 
	pbuf 	+= 4;
	switch( emv_tell_case( apdu_req ) )
	{
		case 1: /*CMD HEADER*/
		break;
		
		case 2: /*CMD HEADER + Le*/
		{
			if( apdu_req->le > 255 )
			{
				ch = '\x0';
			}
			else
			{
				ch = ( unsigned char )( apdu_req->le % 256 );
			}
			*pbuf++ = ch; 
		}
		break;

		case 3: /*CMD HEADER + Lc + DATA(lc)*/
		{
			ch = ( unsigned char )( apdu_req->lc % 256 );
			*pbuf++ = ch; 
			memcpy( pbuf, apdu_req->data, ch );
			pbuf   += ch;
		}
		break;

		case 4:/*CMD HEADER + Lc + DATA(lc) + Le*/
		{
			ch = ( unsigned char )( apdu_req->lc % 256 );
			*pbuf++ = ch; 
			memcpy( pbuf, apdu_req->data, ch );
			pbuf   += ch;
			if( apdu_req->le > 255 )
			{
				ch = '\x0';
			}
			else
			{
				ch = ( unsigned char )( apdu_req->le % 256 );
			}
			*pbuf++ = ch; 
		}
		break;
	}

	return ( int )( pbuf - buf );
}

/* ===================================================================
 * I-Block Pack
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]terminal : the infomation on logic device
 * -------------------------------------------------------------------
 * [input]buf      :  providing raw data
 * -------------------------------------------------------------------
 * [input]len      :  the length of raw data
 * -------------------------------------------------------------------
 * [output]t1_block : I-Block Pack
 * ------------------------------------------------------------------- 
 * return£º
 * -------------------------------------------------------------------
 *    always 0
 * ===================================================================
 */
static int emv_t1_iblock_pack( struct emv_core *terminal, const unsigned char *buf, 
                        int len, struct emv_t1_block  *t1_block )
{
	if( len > terminal->emv_card_ifs )
	{
		t1_block->nad = terminal->emv_card_nad;
		t1_block->pcb = SET_BIT( terminal->terminal_ipcb, IMR_BIT );
		t1_block->len = terminal->emv_card_ifs;
		memcpy( t1_block->data, buf, t1_block->len );
	}
	else
	{
		t1_block->nad = terminal->emv_card_nad;
		t1_block->pcb = CLR_BIT( terminal->terminal_ipcb, IMR_BIT );
		t1_block->len = len;
		memcpy( t1_block->data, buf, t1_block->len );
	}
	terminal->terminal_ipcb = ISN_INC( t1_block->pcb );
	
	return 0;
}

/* ===================================================================
 * Transmit Block( I-Block R-Block S-Block )
 * -------------------------------------------------------------------
 * Parameter£º
 * -------------------------------------------------------------------
 * [input]terminal : information on logic device
 * -------------------------------------------------------------------
 * [input]t1_block : Block Data will be transmitted
 * ------------------------------------------------------------------- 
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 */
static int emv_t1_block_xmit( struct emv_core *terminal, struct emv_t1_block  *t1_block )
{
	sci_queue_flush( terminal );
	emv_t1_compute_lrc( t1_block );
	sci_queue_fill( terminal, &t1_block->nad, 1 );
	sci_queue_fill( terminal, &t1_block->pcb, 1 );
	sci_queue_fill( terminal, &t1_block->len, 1 );
	sci_queue_fill( terminal, t1_block->data, t1_block->len );
	sci_queue_fill( terminal, &t1_block->lrc, 1 );
	sci_emv_hard_xmit ( terminal );
	terminal->terminal_pcb = t1_block->pcb;/*record terminal prevoius type of block 
	                                         transmitted by terminal.*/
	return 0;
}

/* ===================================================================
 * Receive Block( I-Block R-Block S-Block)
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]terminal : information on logic device
 * -------------------------------------------------------------------
 * [ouput]t1_block : recevied block data
 * ------------------------------------------------------------------- 
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the documment "emv_errno.h"
 * ===================================================================
 */
static int emv_t1_block_recv( struct emv_core *terminal, struct emv_t1_block  *t1_block )
{
	unsigned char lrc        = 0;
	int           comm_state = 0;
	int           state      = 0;
	
	memset( t1_block, '\0', sizeof( struct emv_t1_block ) );
	comm_state = sci_queue_spill( terminal, &t1_block->nad, 1 );
	if( comm_state )
	{
		if( ICC_ERR_PARS == comm_state )
		{
			if( 0 == state )
				state = ICC_ERR_PARS;
		}
		else 
			return comm_state;
	}

	comm_state = sci_queue_spill( terminal, &t1_block->pcb, 1 );
	if( comm_state )
	{
		if( ICC_ERR_PARS == comm_state )
		{
			if( 0 == state )
				state = ICC_ERR_PARS;
		}
		else 
			return comm_state;
	}
	
	terminal->emv_card_pcb = t1_block->pcb;/*record card prevoius type of block transmitted by card.*/
	comm_state = sci_queue_spill( terminal, &t1_block->len, 1 );
	if( comm_state )
	{
		if( ICC_ERR_PARS == comm_state )
		{
			sci_queue_spill( terminal, t1_block->data, 256 );
			return comm_state;
		}
		else 
			return comm_state;
	}

	comm_state = sci_queue_spill( terminal, t1_block->data, t1_block->len );
	if( comm_state )
	{
		if( ICC_ERR_PARS == comm_state )
		{
			if( 0 == state )
				state = ICC_ERR_PARS;
		}
		else 
			return comm_state;
	}
	
	comm_state = sci_queue_spill( terminal, &lrc, 1 );
	if( comm_state )
	{
		if( ICC_ERR_PARS == comm_state )
		{
			if( 0 == state )
				state = ICC_ERR_PARS;
		}
		else 
			return comm_state;
	}

	emv_t1_compute_lrc( t1_block );
	if( lrc ^ t1_block->lrc )
	{
		return ICC_ERR_T1_LRC;
	}
	
	if( ICC_ERR_PARS == state ) 
	{
		return ICC_ERR_PARS;
	}
	
	if( t1_block->nad )
	{
		return ICC_ERR_T1_NAD;
	}

	if( 0 == TST_BIT( t1_block->pcb, 7 ) )  
	{
		if( t1_block->len > 254 )  /*I-block.*/
		{
			return ICC_ERR_T1_LEN;
		}

		if( TST_BIT( terminal->terminal_ipcb, IMR_BIT ) )
		{   /* i block has not completed, card should not response with i block */
			return ICC_ERR_T1_PCB;
		}
		
		if( TST_BIT( terminal->emv_card_ipcb, ISN_BIT ) != 
		    TST_BIT( t1_block->pcb, ISN_BIT ) )
		{   /* if the sn of i block transmitted by card is not expected */
			return ICC_ERR_T1_PCB;
		}
		
	}
	else
	{
		if( 0 == TST_BIT( t1_block->pcb, 6 ) )  /*R-block.*/
		{
			if( t1_block->len )
			{
				return ICC_ERR_T1_LEN;
			}
			if( TST_BIT( t1_block->pcb, 5 ) )
			{
				return ICC_ERR_T1_PCB;
			}
		}
		else  /*S-block.*/
		{
			/*terminal don't send S-request , so that received can't be S-response.*/
			if( ( 0xC0 != ( terminal->terminal_pcb & 0xC0 ) ) && 
			    ( TST_BIT( t1_block->pcb, 5 ) ) )
			{ 
				return ICC_ERR_T1_PCB;
			}
			
			if( ( t1_block->pcb & 0x1F ) > 4 )
			{
				return ICC_ERR_T1_SRC;
			}

			if( 0xC1 == t1_block->pcb )
			{
				if( t1_block->data[ 0 ] < 0x10 || t1_block->data[ 0 ] > 0xFE )
				{
					return ICC_ERR_T1_SRL;
				}
			}
			
			if( 0xC2 == t1_block->pcb )
			{
				return ICC_ERR_T1_SRA;
			}
			
			if( 1 != t1_block->len )
			{
				return ICC_ERR_T1_LEN;
			}
		}
	}
	
	return 0;
}

/* ===================================================================
 * To get the data of ATR following the ISO7816-3 specification
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * [output]su_atr   : Data of ATR in the format of struct
 * -------------------------------------------------------------------
 * [ouput]atr       : Data of ATR in the format of string
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 */
int sci_emv_atr_analyser( struct emv_core *terminal, struct emv_atr *su_atr, unsigned char *atr )
{
	int	i    = 0;
	int	flag = 0;
	unsigned char     *patr	= atr;
	unsigned char 	  ch   = '\0';
	unsigned char     tck  = 0;
	int    	tck_cond = 0;
	int	comm_state  = 0;
	
	if( NULL == terminal )	return ICC_ERR_PARAM;
	if( NULL == atr )	return ICC_ERR_PARAM;
	if( NULL == su_atr )	return ICC_ERR_PARAM;
	
	memset( su_atr, '\0', sizeof( struct emv_atr ) );
	*patr++		= '\0';
	comm_state = sci_queue_spill( terminal, &ch, 1 );
	if( comm_state )
	{
		return comm_state;
	}

	if( CONV_CH == ch )
	{
		terminal->terminal_conv = 0; /* direct convention */
	}
	else if( INCONV_CH == ch )
	{
		terminal->terminal_conv = 1; /* inverse convention */
	}
	else
	{
		return ICC_ERR_ATR_TS;  /*neither '3B' nor '3F'*/
	}
	
	*patr++ 	= ch;
	su_atr->ts	= ch;
	comm_state  = sci_queue_spill( terminal, &ch, 1 );
	if( comm_state )
	{
		return comm_state;
	}

	*patr++ 	= ch;
	su_atr->t0	= ch;
	flag		= ch & 0xF0;
	i 		= 0;

	while( flag )
	{
		comm_state = sci_queue_spill( terminal, &ch, 1 );
		if( comm_state )
		{
			return comm_state;
		}		
		
		*patr++ = ch;
		if( TST_BIT( flag, 4 ) )
		{ /*TA*/
			SET_BIT( su_atr->ta_flag, i );
			su_atr->ta[ i ]	= ch;
			CLR_BIT( flag, 4 );
			continue;
		}
		
		if( TST_BIT( flag, 5 ) )
		{ /*TB*/
			SET_BIT( su_atr->tb_flag, i );
			su_atr->tb[ i ]	= ch;
			CLR_BIT( flag, 5 );
			continue;
		}
		
		if( TST_BIT( flag, 6 ) )
		{ /*TC*/
			SET_BIT( su_atr->tc_flag, i );
			su_atr->tc[ i ]	= ch;
			CLR_BIT( flag, 6 );
			continue;
		}
		
		if( TST_BIT( flag, 7 ) )
		{ /*TD*/
			SET_BIT( su_atr->td_flag, i );
			su_atr->td[ i++ ]	= ch;
			/*
			 * If only T=0 is indicated, possibly by default, then TCK shall be absent. 
			 * If T=0 and T=15 are present and in all the other cases, TCK shall be 
			 * present. When TCK is present, exclusive-oring all the bytes T0 to TCK 
			 * inclusive shall give '00'. Any other value is invalid.
           		*/
			tck_cond	|= ( ch & 0x0F );/*Tell TCK whether is exist or not?*/
			flag	         =   ch & 0xF0;  /* ( Tdi & 0xf0 ) | ( T0 & 0x0f ) */
		}
		else
		{
			break;
		}
	}

	/*history bytes.*/
	comm_state = sci_queue_spill( terminal, patr, ( su_atr->t0 & 0x0F ) );
	if( comm_state )
	{
		return comm_state;
	}	

	memcpy( su_atr->hbytes, patr, ( su_atr->t0 & 0x0F ) );
	patr += su_atr->t0 & 0x0F;
	if( tck_cond )
	{ /*TCK*/
		comm_state = sci_queue_spill( terminal, &ch, 1 );
		if( comm_state )
		{
			return comm_state;
		}

		i 	= 2;    /*T0 to TCK.*/
		tck	= 0;
		while( i < ( patr - atr ) )
		{
			tck ^= atr[ i++ ]; /*Compute TCK.*/
		}
		
		if( ch != tck )
		{
			return ICC_ERR_ATR_TCK;
		}
	}

	*atr	= patr - atr - 1;/*the number of ATR bytes.*/
	
	return 0;
}

/* ===================================================================
 * To parse the validity of received data following the EMV specification
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * [input]su_atr    : the data of ATR
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 */
int sci_emv_atr_parse( struct emv_core *terminal, struct emv_atr *su_atr )
{
	int	cwi = 0;
	int	bwi = 0;
	int ext = 0;
	
	if( NULL == terminal || NULL == su_atr )
	{
		return ICC_ERR_PARAM;
	}
	/* The terminal shall accept an ATR containing TD1 with the m.s. nibble 
	* having any value (provided that the value returned correctly indicates 
	* and is consistent with the interface characters TA2 to TD2 actually 
	* returned ), and the l.s. nibble having a value of '0' or '1'. The 
	* terminal shall reject an ATR containing other values of TD1.
	*/
	if( TST_BIT( su_atr->td_flag, 0 ) )
	{ 
		if( ( su_atr->td[ 0 ] & 0x0F ) > 1 )
		{
			return ICC_ERR_ATR_TD1; 
		}
		terminal->terminal_ptype = su_atr->td[ 0 ] & 0x0F;
	}
	else
	{
		terminal->terminal_ptype = 0;
	}
	
	/* If TA1 is present in the ATR (indicated by b5 of T0 set to 1) and TA2 
	* is returned with b5 = 0 (specific mode, parameters defined by the interface
	* bytes), the terminal shall:
	*   1. Accept the ATR if the value of TA1 is in the range '11' to '13',5 and
	*   immediately implement the values of F and D indicated (F=372 and D = 1, 2, or 4).
	*   2. Reject the ATR if the value of TA1 is not in the range '11' to '13', 
	*   unless it is able to support and immediately implement the conditions indicated.
	*   
	* If TA1 is present in the ATR (indicated by b5 of T0 set to 1) and TA2 is not
	* returned (negotiable mode), the terminal shall accept the ATR and shall
	* continue using the default values of D = 1 and F = 372 during all subsequent
	* exchanges, unless it supports a proprietary technique for negotiating the
	* parameters to be used.
	*  
	* If TA1 is absent from the ATR, the default values of D = 1 and F = 372 shall 
	* be used during all subsequent exchanges.
	*/
	if( TST_BIT( su_atr->ta_flag, 0 ) )/*TA1 is present.*/
	{ 
		if( ( TST_BIT( su_atr->ta_flag, 1 ) ) && /*TA2 is present.*/
		    ( 0 == TST_BIT( su_atr->ta[ 1 ], 4 ) ) ) /*bug, emv bug, 2011-3-29 Liubo*/
		{
			if( 0x11 > su_atr->ta[ 0 ] || 0x13 < su_atr->ta[ 0 ] )
			{
				return ICC_ERR_ATR_TA1;
			}
		}
	}
	/*
	 * The terminal shall accept an ATR containing TA2 provided that all the following 
	 * conditions are met:
	 *     1. The protocol indicated in the l.s. nibble is also the first indicated protocol 
     	 * in the ATR.
	 *     2. b5 = 0
	 *     3. The terminal is able to support the exact conditions indicated in the
	 * applicable interface characters and immediately uses those conditions.
	 * Otherwise, the terminal shall reject an ATR containing TA2.
	 */
	if( TST_BIT( su_atr->ta_flag, 1 ) )
	{ 
		/*TA2 is present,means ic card operation in specific mode.*/
		if( TST_BIT( su_atr->ta[ 1 ], 4 ) )
		{
			return ICC_ERR_ATR_TA2;
		}
		if( ( su_atr->ta[ 1 ] & 0x0f ) > 1 )
		{
			return ICC_ERR_ATR_TA2;
		}
		if( TST_BIT( su_atr->ta_flag, 0 ) )
		{/*Apply specific parameter values.*/
			if( 0 != fi_const_table( su_atr->ta[ 0 ] / 16 ) && 0 != di_const_table( su_atr->ta[ 0 ] % 16 ) )
			{
				terminal->terminal_fi 	  = fi_const_table( su_atr->ta[ 0 ] / 16 );
				terminal->terminal_di 	  = di_const_table( su_atr->ta[ 0 ] % 16 );
				terminal->terminal_ptype  = su_atr->ta[ 1 ] & 0x0f;
			}
		}
	}
	
	/*
	 * In response to a cold reset, the terminal shall accept only an ATR containing 
	 * TB1 = '00'. 
	 * In response to a warm reset the terminal shall accept an ATR  containing TB1 
	 * of any value (provided that b6 of T0 is set to 1) or not containing TB1 ( provided
	 * that b6 of T0 is set to 0) and shall continue the card session as though TB1 = '00'
	 * had been returned. VPP shall never be generated.
	 */
	if( EMV_COLD_RESET == terminal->terminal_state )
	{
		if( 0 == TST_BIT( su_atr->tb_flag, 0 ) )
		{
			return ICC_ERR_ATR_TB1;
		}

		if( su_atr->tb[ 0 ] )
		{
			return ICC_ERR_ATR_TB1;
		}
	}
	/*
	* If the value of TC1 is in the range '00' to 'FE', between 0 and 254 etus of 
	* extra guardtime shall be added to the minimum character to character duration,
	* which for subsequent transmissions shall be between 12 and 266 etus.
	*
	* If the value of TC1 = 'FF', then the minimum character to character duration for
	* subsequent transmissions shall be 12 etus if T=0 is to be used, or 11 etus if T=1
	* is to be used.
	*
	* The terminal shall accept an ATR not containing TC1 ( provided that b7 of T0 is set 
	* to 0), and shall continue the card session as though TC1 = '00' had been returned.
	*/
	if( TST_BIT( su_atr->tc_flag, 0 ) )
	{
		if( su_atr->tc[ 0 ] < 255 )
		{
			terminal->terminal_cgt = 12 + su_atr->tc[ 0 ];
			ext	= su_atr->tc[ 0 ];
		}
		else
		{
			if( 0 == terminal->terminal_ptype )
			{
				terminal->terminal_cgt =  12;
			}
			if( 1 == terminal->terminal_ptype )
			{
				terminal->terminal_cgt =  11;
			}
			ext	= -1;
		}
	}
	else
	{/*TC1 is absent, as though TC1='00' has been return.*/
		terminal->terminal_cgt =  12;
	}
	
	/*
	 * The terminal shall reject an ATR containing TB2.
	 */
	if( TST_BIT( su_atr->tb_flag, 1 ) )
	{
		return ICC_ERR_ATR_TB2;
	}
	
	/*
	* The terminal shall:
	*    1. reject an ATR containing TC2 = '00'
	*    2. accept an ATR containing TC2 = '0A'
	*    3. reject an ATR containing TC2 having any other value unless it is 
	*       able to support it.
	*/
	terminal->terminal_wwt = ( 960 * 10 + 480 ) * terminal->terminal_di + 1;
	if( TST_BIT( su_atr->tc_flag, 1 ) )
	{
		if( 0x0A != su_atr->tc[ 1 ] )
		{
			return ICC_ERR_ATR_TC2;
		}
		terminal->terminal_wwt = ( 960 * su_atr->tc[ 1 ] + 480 ) * terminal->terminal_di + 1;
	}
	
	/*
	 *  The terminal shall accept an ATR containing TD2 with the m.s. nibble having 
	 *  any value (provided that the value returned correctly indicates and is 
	 *  consistent with the interface characters TA3 to TD3 actually returned), 
	 *  and the l.s. nibble having a value of '1' ( or 'E' if the l.s. nibble of TD1 
	 *  is '0'). The terminal shall reject an ATR containing other values of TD2.
	 */
	if( TST_BIT( su_atr->td_flag, 1 ) )
	{
		/*TD2 is present, the l.s.nibble value = '1', 
		  or value = 'E' and the l.s.nibble value of TD1 is '1'.*/
		if( 1 != ( su_atr->td[ 1 ] & 0x0F ) )
		{
			if( !( 0 == ( su_atr->td[ 0 ] & 0x0f ) && 0x0E == ( su_atr->td[ 1 ] & 0x0F ) ) )
			{
				return ICC_ERR_ATR_TD2;
			}
		}
	}
	
	/*
	 * The terminal shall accept an ATR not containing TA3 ( provided that b5 of TD2 
	 * is set to 0), and shall continue the card session using a value of '20' for 
	 * TA3. The terminal shall reject an ATR containing TA3 having a value in the 
	 * range '00' to '0F' or a value of 'FF'.
	 */
	terminal->emv_card_ifs = 32;
	if( TST_BIT( su_atr->ta_flag, 2 ) )
	{
		if( su_atr->ta[ 2 ] < 0x10 || su_atr->ta[ 2 ] == 0xff )
		{
			return ICC_ERR_ATR_TA3;
		}
		terminal->emv_card_ifs = su_atr->ta[ 2 ];
	}
    /*
     * TB3 (if T=1 is indicated in TD2) indicates the values of the CWI and the BWI
     * used to compute the CWT and BWT respectively.
     * The terminal shall reject an ATR not containing TB3, or containing a TB3 
     * indicating BWI greater than 4 and/or CWI greater than 5, or having a value 
     * such that 2CWI ¡Ü (N + 1). It shall accept an ATR containing a TB3 having 
     * any other value.
     *
     * Note: 
     *   N is the extra guardtime indicated in TC1. When using T=1, if TC1='FF', 
     *   the value of N shall be taken as ¨C1. Since the maximum value for CWI 
     *   allowed by these specifications is 5, note that when T=1 is used, TC1 
     *   shall have a value in the range '00' to '1E' or a value of 'FF' in order 
     *   to avoid a conflict between TC1 and TB3.
     *
     * The maximum interval between the leading edges of the start bits of two
     * consecutive characters sent in the same block (the character waiting time, CWT)
     * shall not exceed (1<<CWI + 11) etus. The character waiting time integer, CWI shall
     * have a value of 0 to 5 as described in section 8.3.3.10, and thus CWT lies in the
     * range 12 to 43 etus. The receiver shall be able to correctly interpret a character
     * having a maximum interval between the leading edge of the start bit of the character 
     * and the leading edge of the start bit of the previous character of (CWT + 4) etus
     *
     * The maximum interval between the leading edge of the start bit of the last
     * character that gave the right to send to the ICC and the leading edge of the start
     * bit of the first character sent by the ICC (the block waiting time, BWT) shall not
     * exceed {(1<<BWI x 960) + 11} etus. The block waiting time integer, BWI shall have a
     * value in the range 0 to 4 as described in section 8.3.3.10, and thus BWT lies in
     * the range 971 to 15,371 etus for a D of 1.       
     */
	if(terminal->terminal_ptype == 1)	
	{
		if( 0 == TST_BIT( su_atr->tb_flag, 2 ) )
		{
			return ICC_ERR_ATR_TB3;
		}
	}
	terminal->terminal_cwt = 47;
	terminal->terminal_bwt = ( 960 + 960 ) * terminal->terminal_di + 11 + 1;

	if( 1 == ( su_atr->td[ 1 ] & 0x0F ) )
	{
		if( 0 == TST_BIT( su_atr->tb_flag, 2 ) )
		{
			return ICC_ERR_ATR_TB3;
		}

		bwi = su_atr->tb[ 2 ] / 16;
		cwi = su_atr->tb[ 2 ] % 16;
		if( bwi > 4 || cwi > 5 )
		{
			return ICC_ERR_ATR_TB3;
		}

		if( ( 1 << cwi ) <= ( ext + 1 ) )
		{
			return ICC_ERR_ATR_TB3;
		}                                             
		terminal->terminal_bwt = ( ( 1 << bwi ) * 960 + 960 ) * terminal->terminal_di + 11 + 1;
		terminal->terminal_cwt = ( ( 1 << cwi ) + 11 + 4 );
	}
	/*
	 * The terminal shall accept an ATR containing TC3 = '00'. It shall reject 
	 * an ATR containing TC3 having any other value.
	 */
	if( TST_BIT( su_atr->tc_flag, 2 ) )
	{
		if( su_atr->tc[ 2 ] )
		{
			return ICC_ERR_ATR_TC3;
		}
	}

	/*
	 * The initial size immediately following the answer to reset shall be 254 bytes, 
	 * and this size shall be used throughout the rest of the card session.
	 */
	terminal->terminal_ifs  = 254;
	terminal->terminal_ipcb = 0;
	terminal->terminal_pcb  = 0;
	terminal->emv_card_ipcb = 0;
	terminal->emv_card_pcb  = 0;
	if( 0 == terminal->terminal_ptype )
	{
		terminal->terminal_igt  = 16; /* minimum value is 16. */
	}
	else
	{
		terminal->terminal_igt  = 22; /* minimum value is 22. */
	}

	/* Notice:
	 *    the state machine change.
	 */
	terminal->terminal_state = EMV_READY;
	terminal->terminal_open  = 1;

	return 0;
}

/* ===================================================================
 * Data Exchange When T = 0
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * [input]apdu_req  : the pointer to  command struct transmiting data
 * -------------------------------------------------------------------
 * [output]apdu_resp : the pointer to  response struct received data
 * ------------------------------------------------------------------- 
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 */
int sci_emv_t0_exchange(    struct emv_core     *terminal, 
                        struct emv_apdu_req *apdu_req, 
                        struct emv_apdu_rsp *apdu_resp 	)
{
	int             cmd_case 	  = 0;
	int             tx_residual	= 0;
	int             rx_residual = 0;
	int             is_first_sw	= 1; /*control '62','63' etc. is not first procedure byte.*/
	int             exchange_sw = 0;
	unsigned char  *prx;
	unsigned char  *ptx;
	unsigned char   ch 	        = 0;
	unsigned char   ch2         = 0;
	unsigned char   sw1         = 0;
	unsigned char   sw2         = 0;
	int             comm_state  = 0;
	enum T0_EVENT { T0_TERMINAL_EVENT = 0, T0_SEND_CMD_EVENT,    T0_GET_PROCEDURE_EVENT,
		            T0_INS_PROC_EVENT,     T0_INVINS_PROC_EVENT, T0_SPECIAL_PROC_EVENT,
	}t0_proc_event;
	
	if( NULL == terminal  || NULL == apdu_req || 
	    NULL == apdu_resp || apdu_req->lc > 255 )
	{
		return ICC_ERR_PARAM;
	}
	
	t0_proc_event = T0_SEND_CMD_EVENT;
	exchange_sw   = 0;
	is_first_sw   = 1;
	prx	      = apdu_resp->data;
	ptx	      = apdu_req->data;
	do{
		switch( t0_proc_event )
		{
		case T0_SEND_CMD_EVENT:
			cmd_case    = emv_tell_case( apdu_req );
			tx_residual =   apdu_req->lc;
			rx_residual = ( apdu_req->le >= 256 ) || (apdu_req->le == 0) ? 256 : apdu_req->le;
			sci_queue_flush( terminal );
			sci_queue_fill( terminal, apdu_req->cmd, 4 );
			if( cmd_case > 2 )
			{
				ch = ( unsigned char )( apdu_req->lc % 256 );
			}
			else if( 2 == cmd_case && apdu_req->le < 256 )
			{
				ch = ( unsigned char )( apdu_req->le % 256 );
			}
			else
			{
				ch = '\0';
			}
			
			emv_t0_cmd_status(cmd_case, terminal);
			sci_queue_fill( terminal, &ch, 1 );
			sci_emv_hard_xmit( terminal );
			t0_proc_event = T0_GET_PROCEDURE_EVENT;
		break;

		case T0_GET_PROCEDURE_EVENT:
			comm_state = sci_queue_spill( terminal, &ch, 1 );/*read a procedure byte.*/
			if( comm_state )
			{
				sci_emv_hard_power_dump( terminal );
				return comm_state;
			}
			if( ch == apdu_req->cmd[ 1 ] )
			{/*analyse procedure byte.*/
				if( 0x90 != ( ch & 0xF0 ) && 0x60 != ( ch & 0xF0 ) )
				{
					t0_proc_event = T0_INS_PROC_EVENT;
					is_first_sw = 0;
				}
				else
				{
					t0_proc_event = T0_SPECIAL_PROC_EVENT;
				}
			}
			else if( ch == ( 0xFF ^ apdu_req->cmd[ 1 ] ) )
			{
				if( 0x90 != ( ch & 0xF0 ) && 0x60 != ( ch & 0xF0 ) )
				{
					t0_proc_event = T0_INVINS_PROC_EVENT;
					is_first_sw = 0;
				}
				else
				{
					t0_proc_event = T0_SPECIAL_PROC_EVENT;
				}
			}
			else if( 0x60 == ch )
			{
				t0_proc_event = T0_GET_PROCEDURE_EVENT;
				is_first_sw = 0;
			}
			else
			{
				t0_proc_event = T0_SPECIAL_PROC_EVENT;
			}
		break;

		case T0_INS_PROC_EVENT:/*INS*/
		{
			if( cmd_case < 3 )
			{
				if( ( ( prx - apdu_resp->data ) + rx_residual ) > 256 )
				{
					sci_emv_hard_power_dump( terminal );
					return ICC_ERR_PARAM;
				}
				comm_state = sci_queue_spill( terminal, prx, rx_residual );
				if( comm_state )
				{
					sci_emv_hard_power_dump( terminal );
					return comm_state;
				}
				prx		+= rx_residual;
				rx_residual	 = 0;
			}
			else
			{
				sci_queue_flush( terminal );
				sci_queue_fill( terminal, ptx, tx_residual );
				sci_emv_hard_xmit( terminal );
				ptx		+= tx_residual;
				tx_residual	 = 0;
			}
			t0_proc_event = T0_GET_PROCEDURE_EVENT;
		}
		break;

		case T0_INVINS_PROC_EVENT:/*0xFF ^ INS*/
		{
			if( cmd_case < 3 )
			{
				if( ( prx - apdu_resp->data ) > 256 || 
				    ( ( prx - apdu_resp->data ) + rx_residual ) > 256 )
				{
					sci_emv_hard_power_dump( terminal );
					return ICC_ERR_PARAM;
				}
				comm_state = sci_queue_spill( terminal, prx, 1 );
				if( comm_state )
				{
					sci_emv_hard_power_dump( terminal );
					return comm_state;
				}
				prx++;
				rx_residual--;
			}
			else
			{
				if( ( ptx - apdu_req->data ) > 256 )
				{/* Xuwt 2010-9-22 */
					sci_emv_hard_power_dump( terminal );
					return ICC_ERR_PARAM;
				}
				sci_queue_flush( terminal );
				sci_queue_fill( terminal, ptx, 1 );
				sci_emv_hard_xmit( terminal );
				ptx++;
				tx_residual--;
			}
			t0_proc_event = T0_GET_PROCEDURE_EVENT;
		}
		break;

		case T0_SPECIAL_PROC_EVENT:/*special procedure.*/
		{
			if( ( 0x90 != ( ch & 0xF0 ) ) && ( 0x60 != ( ch & 0xF0 ) ) )
			{
				sci_emv_hard_power_dump( terminal );
				return ICC_ERR_T0_PROB;
			}
			comm_state = sci_queue_spill( terminal, &ch2, 1 );
			if( comm_state )
			{
				sci_emv_hard_power_dump( terminal );
				return comm_state;
			}
			apdu_resp->swa = ch;
			apdu_resp->swb = ch2;
			t0_proc_event  = T0_TERMINAL_EVENT;/*ready to terminate.*/
		
			if( terminal->terminal_auto )
			{
				switch( ch )
				{
					case 0x61:
						memcpy( apdu_req->cmd, "\x00\xC0\x00\x00", 4 );
						apdu_req->lc = 0;
						apdu_req->le = ch2;
						is_first_sw = 0;
						t0_proc_event = T0_SEND_CMD_EVENT;
					break;

					case 0x6C:
						apdu_req->lc = 0;
						apdu_req->le = ch2;
						is_first_sw = 0;
						t0_proc_event = T0_SEND_CMD_EVENT;
					break;
					
					default:
						if( 4 == cmd_case && 0 == is_first_sw )
						{
							if( ( 0x62 == ch ) || ( 0x63 == ch ) ||
							    ( ( 0x90 == ( ch & 0xF0 ) ) && ( 0x90 != ch || 0x00 != ch2 ) ) )
							{
								memcpy( apdu_req->cmd,  "\x00\xC0\x00\x00", 4 );
								apdu_req->lc = 0;
								apdu_req->le = 256;
								exchange_sw  = 1;
								sw1          = ch;
								sw2          = ch2;
								t0_proc_event = T0_SEND_CMD_EVENT;
							}
						}
					break;
				}
			}
		}
		break;
	
		default:
		break;
		}
	}while( T0_TERMINAL_EVENT != t0_proc_event );

	if( 1 == exchange_sw )
	{
		apdu_resp->swa = sw1;
		apdu_resp->swb = sw2;
	}
	apdu_resp->len = ( int )( prx - apdu_resp->data );
	
	return 0;
}

/* ===================================================================
 * Data Exchange When T = 1
 * -------------------------------------------------------------------
 * Parameter£º
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
* [input]apdu_req  : the pointer to command struct transmiting data
 * -------------------------------------------------------------------
 * [output]apdu_resp : the pointer to response struct received data
 * ------------------------------------------------------------------- 
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 */
int sci_emv_t1_exchange(    struct emv_core     *terminal, 
                        struct emv_apdu_req *apdu_req, 
                        struct emv_apdu_rsp *apdu_resp 	)
{
	unsigned char       buf[ MAX_APDU_SIZE + 6 ]; /*command header + lc + data + le*/
	unsigned char       *pbuf;
	unsigned char       *pb_end;
	unsigned char       *prbuf;
	struct emv_t1_block t1_sblock;
	struct emv_t1_block t1_rblock;
	int                 state      = 0;
	int                 ib_len     = 0;
	int                 bwt        = 971;
	int                 ifsc       = 0;

	enum BLOCK_TYPE {INVALID_BLOCK = 0,I_BLOCK,R_BLOCK,S_RESPONSE_BLOCK};
	enum BLOCK_TYPE send_block_type;

	enum 
	{ 
		I_BLOCK_EVENT = 0, R_BLOCK_EVENT,     S_BLOCK_EVENT, 
		TRAN_RCEV_EVENT,   TERMINATOR_EVENT,  I_ORG_EVENT,
		INVALID_ORG_EVENT, PCB_ANALYSE_EVENT, ACK_EVENT,
		ACK_SREQ_EVENT
	}t1_proc_event;
	
	if( NULL == terminal  || NULL == apdu_req || 
	    NULL == apdu_resp || apdu_req->lc > 255 )
	{
		return ICC_ERR_PARAM;
	}

T1_EXCHANGE_START:
	ifsc    = terminal->terminal_ifs;
	bwt     = terminal->terminal_bwt;
	prbuf   = apdu_resp->data;
	pb_end 	= buf + emv_t1_extract( apdu_req, buf );
	pbuf	= buf;
	
	terminal->emv_repeat = 0;
	t1_proc_event        = I_ORG_EVENT;
	do
	{
		switch( t1_proc_event )
		{
		case I_ORG_EVENT:
		{
			emv_t1_iblock_pack( terminal, pbuf, pb_end - pbuf, &t1_sblock );
			ib_len        = t1_sblock.len;/*record the I block length with prevoius I block transmitted.*/
			pbuf         += ib_len;
			send_block_type = I_BLOCK;
			t1_proc_event = TRAN_RCEV_EVENT;
		}
		break;

		case TRAN_RCEV_EVENT:
		{
			terminal->emv_repeat++;
			if( T1_REPEAT_LMT == terminal->emv_repeat )
			{
				sci_emv_hard_power_dump( terminal );
				state	      = ICC_ERR_T1_BREP; 
				t1_proc_event = TERMINATOR_EVENT;
			}
			else
			{
				emv_t1_block_xmit( terminal, &t1_sblock );
				state = emv_t1_block_recv( terminal, &t1_rblock );
				terminal->terminal_bwt = bwt;/*restore bwt.*/
				if( ICC_ERR_T1_BWT == state || ICC_ERR_T1_CWT == state )
				{
					sci_emv_hard_power_dump( terminal );/*Please start deactivaton procedure in emv hardware processing.*/
					t1_proc_event = TERMINATOR_EVENT;
				}
				else
				{
					if( state )
					{
						t1_proc_event = INVALID_ORG_EVENT;
					}
					else
					{
						t1_proc_event = PCB_ANALYSE_EVENT;
					}
				}
			}
		}
		break;

		case PCB_ANALYSE_EVENT:
		{
			if( 0 == TST_BIT( t1_rblock.pcb, 7 ) )
			{
				t1_proc_event = I_BLOCK_EVENT;
			}
			else
			{
				if( 0 == TST_BIT( t1_rblock.pcb, 6 ) )
				{
					t1_proc_event = R_BLOCK_EVENT;
				}
				else
				{
					t1_proc_event = S_BLOCK_EVENT;
				}
			}
		}
		break;

		case I_BLOCK_EVENT:
		{
			terminal->emv_repeat = 0;
			
			if((prbuf - apdu_resp->data) + t1_rblock.len > MAX_APDU_SIZE + 2)
			{
				/* overflow */
				t1_proc_event = TERMINATOR_EVENT;
				state = ICC_ERR_PARAM;
				break;
			}

			if( TST_BIT( t1_rblock.pcb, IMR_BIT ) )
			{
				memcpy( prbuf, t1_rblock.data, t1_rblock.len );
				prbuf += t1_rblock.len;
				t1_proc_event = ACK_EVENT;
			}
			else
			{
				memcpy( prbuf, t1_rblock.data, t1_rblock.len );
				prbuf += t1_rblock.len;
				if(prbuf - apdu_resp->data < 2)
				{
					/* invalid response APDU */
					state = ICC_ERR_T1_LEN;
				}
				else
				{
					apdu_resp->len = ( prbuf - apdu_resp->data ) - 2;
					apdu_resp->swb = *( --prbuf );
					apdu_resp->swa = *( --prbuf );
				}
				t1_proc_event = TERMINATOR_EVENT;
			}
			terminal->emv_card_ipcb = ISN_INC( t1_rblock.pcb );/*next expected sn.*/
		}
		break;

		case R_BLOCK_EVENT:
		{
			/*the sn of r block received is equal to the sn of next i block transmitted by terminal.*/
			if( CMP_IRSN_EQU( terminal->terminal_ipcb, t1_rblock.pcb ) )
			{
				if( TST_BIT( terminal->terminal_ipcb, IMR_BIT ) )
				{
					t1_proc_event = I_ORG_EVENT;/*next i block.*/
				}
				else
				{ /* R( PCB = 0x82 )  or R block in card sending */
					t1_proc_event = INVALID_ORG_EVENT;
				}
				terminal->emv_repeat = 0;
			}
			else
			{/*the sn of r block received is not equal to the sn of expected.*/
				if( TST_BIT( terminal->emv_card_ipcb, IMR_BIT ) )
				{ 
					terminal->emv_repeat = 0;
					/* R block with sn error */
					t1_proc_event = INVALID_ORG_EVENT;
				}
				else
				{
					terminal->terminal_ipcb = ISN_DEC( terminal->terminal_ipcb );
					pbuf         -= ib_len; /*restore previous i block.*/
					t1_proc_event = I_ORG_EVENT;
				}
			}
		}
		break;

		case S_BLOCK_EVENT:
		{
			if( TST_BIT( t1_rblock.pcb, SRS_BIT ) )
			{
			    /* we don't send S requst block, 
			       then we can't recept S response block */
				t1_proc_event = INVALID_ORG_EVENT;
			}
			else
			{
				terminal->emv_repeat = 0;
				switch( t1_rblock.pcb & 0x0F )
				{
					case 0x00:/*Re-synchronize, it is not used.*/
					break;
					
					case 0x01:/*ifsc*/
					{
						if( t1_rblock.len )
						{
							terminal->emv_card_ifs  = t1_rblock.data[ 0 ];
							t1_proc_event = ACK_SREQ_EVENT;
						}
					}
					break;
					
					case 0x02:/*abort, already process in "INVALID_ORG_EVENT"*/
					break;
					
					case 0x03:/*wtx*/
					{
						if( t1_rblock.len )
						{
							terminal->terminal_bwt = bwt * t1_rblock.data[ 0 ];
							t1_proc_event = ACK_SREQ_EVENT;
						}
					}
					break;
					
					case 0x04:/*vpp error*/
					{
						sci_emv_hard_power_dump( terminal );
						t1_proc_event = TERMINATOR_EVENT;
					}
					break;
				}/*switch -!>*/
			}
		}
		break;

		case ACK_SREQ_EVENT:/*terminal acknowledge card's S Block request.*/
		{
			t1_sblock.nad = t1_rblock.nad;
			t1_sblock.pcb = SET_BIT( t1_rblock.pcb, SRS_BIT );
			t1_sblock.len = 1;
			t1_sblock.data[ 0 ] = t1_rblock.data[ 0 ];
			send_block_type = S_RESPONSE_BLOCK;
			t1_proc_event = TRAN_RCEV_EVENT;
		}
		break;

		case ACK_EVENT:/*card request that terminal send next i block.*/
		{
			t1_sblock.nad = terminal->emv_card_nad;
			t1_sblock.pcb = 0x80 | ISN_RSN( terminal->emv_card_ipcb );
			t1_sblock.len = 0;
			send_block_type = R_BLOCK;
			t1_proc_event = TRAN_RCEV_EVENT;
		}
		break;

		case INVALID_ORG_EVENT:/*answer to invalid block by R-Block..*/
		{
			t1_sblock.nad = terminal->emv_card_nad;
			t1_sblock.len = 0;

			if(state == ICC_ERR_T1_SRA)
			{
				/* abort request */
				sci_emv_hard_power_dump( terminal );
				t1_proc_event = TERMINATOR_EVENT;
				break;
			}

			if(send_block_type == R_BLOCK)
			{
				/*If an invalid block is received in response to a R-block, 
				the sender shall retransmit the R-block.*/
				send_block_type = R_BLOCK;
				t1_proc_event = TRAN_RCEV_EVENT;
				break;
			}
			else //send_block_type == I_BLOCK or S_RESPONSE_BLOCK
			{
				/*If an invalid block is received in response to an I-block,
				the sender shall transmit a R-block with its sequence number code.
				If an invalid block is received in response to a S(... response) block, 
				the sender shall transmit a R-block with its sequence number code. */
				send_block_type = R_BLOCK;
				switch( state )
				{
					case ICC_ERR_PARS:
					case ICC_ERR_T1_LRC:
						t1_sblock.pcb = 0x81 | ISN_RSN( terminal->emv_card_ipcb );
						t1_proc_event = TRAN_RCEV_EVENT;
					break;			
					default:
						t1_sblock.pcb = 0x82 | ISN_RSN( terminal->emv_card_ipcb );
						t1_proc_event = TRAN_RCEV_EVENT;
					break;
				}
			}	
		}
		break;

		default:
			t1_proc_event = TERMINATOR_EVENT;
		break;
		}/*switch -!>*/
	}while( TERMINATOR_EVENT != t1_proc_event );
	
	return state;
}

/* ===================================================================
 * Transmit IFSD block( the value of IFSD is specified by "terminal_ifsd")
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]terminal : information on logic device
 * ------------------------------------------------------------------- 
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 * 
 */
int sci_emv_t1_ifsd_request( struct emv_core *terminal )
{
	struct emv_t1_block  t1_block;
	int	state    = 0;
	
	terminal->emv_repeat = 1;/*SP30 PCI2.1 EMV L1 certification updated 2011-3-15 XuWT*/
	do
	{
		t1_block.nad = terminal->emv_card_nad;
		t1_block.pcb = 0xC1;
		t1_block.len = 1;
		t1_block.data[ 0 ] = 0xFE;

		terminal->emv_repeat++;
		emv_t1_block_xmit( terminal, &t1_block );
		state = emv_t1_block_recv( terminal, &t1_block );
		
		if( ICC_ERR_T1_BWT == state || ICC_ERR_T1_CWT == state )
		{
			if( 1 == terminal->terminal_spec )
			{
				break;
			}
			else /* EMV Mode */
			{
				sci_emv_hard_power_dump( terminal );
				return state;
			}
		}
		else
		{ /* IFSD ok */
			if( 0    == state && 
			    1    == t1_block.len &&
			    0xE1 == t1_block.pcb &&
			    0xFE == t1_block.data[ 0 ] &&
			    t1_block.nad == terminal->emv_card_nad)
			{
				terminal->terminal_ifs = 0xFE;
				terminal->emv_repeat = 0;
				break;
			}
			
			/** 
			 * if failure 
			 * Because i found some card with ISO7816 don't support IFSD.
	 		 * It cause timeout.
	 		 * Also, some card , the ifsd value in response is not 0xfe.
	 		 * Above, I process the ifsd proccedure.
	 		 *  
			 */
			if( T1_REPEAT_LMT == terminal->emv_repeat )
			{
				if( 1 == terminal->terminal_spec )/* PC/SC card don't support IFSD */
				{
					break;
				}
				else /* EMV mode */
				{
					sci_emv_hard_power_dump( terminal );
					return ICC_ERR_T1_BREP;	
				}
			}
		}

	}while( 1 );

	return 0;
}

/* ===================================================================
 * To parse the validity of ATR data which follows the ISO7816-3 protocol
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * [input]su_atr    : the data of ATR
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 */
int sci_iso_atr_parse( struct emv_core *pdev, struct emv_atr *su_atr )
{
	int	cwi = 0;
	int	bwi = 0;
	int ext = 0;
	int i   = 0; 
	int T   = 0;
	
	if( NULL == pdev || NULL == su_atr )
	{
		return ICC_ERR_PARAM;
	}
	/*
	 * If TD1, TD2 and so on are present, the encoded types T shall be in ascending 
	 * numerical order. If present, T=0 shall be first, T=15 shall be last. T=15 is 
	 * invalid in TD1.
	 *
	* The "first offered transmission protocol" is defined as follows.
	*  -   If TD1 is present, then it encodes the first offered protocol T.
	*  -   If TD1 is absent, then the only offer is T=0.
	*/
	if( TST_BIT( su_atr->td_flag, 0 ) )
	{ 
		if( ( su_atr->td[ 0 ] & 0x0F ) > 1 )
		{
			return ICC_ERR_ATR_TD1; 
		}
		pdev->terminal_ptype = su_atr->td[ 0 ] & 0x0F;
		
		/*Check ascending numerical order.*/
		i = 1;
		while( TST_BIT( su_atr->td_flag, i ) )
		{
			if( ( su_atr->td[ i - 1 ] & 0x0f ) > ( su_atr->td[ i ] & 0x0f ) )
			{
				return ICC_ERR_T_ORDER;
			}
			i++;
		}
	}
	else
	{
		pdev->terminal_ptype = 0;
	}
	
	/*
	 * TA2 is the specific mode byte as shown in Figure 15. For the use of TA2, see 6.3.1 and 7.1.
	*   1. Bit 8 indicates the ability for changing the negotiable/specific mode:
	*      -  capable to change if bit 8 is set to 0;
	*      -  unable to change if bit 8 is set to 1.
	*   2. Bits 7 and 6 are reserved for future use (set to 0 when not used).
	*   3. Bit 5 indicates the definition of the parameters F and D.
	*      -  If bit 5 is set to 0, then the integers Fi and Di defined above by TA1 shall apply.
	*      -  If bit 5 is set to 1, then implicit values (not defined by the interface bytes) shall apply.
	*/
	if( TST_BIT( su_atr->ta_flag, 1 ) )
	{ 
		/*TA2 is present,means ic card operation in specific mode.*/
		if( TST_BIT( su_atr->ta[ 1 ], 4 ) )
		{
			pdev->terminal_fi = pdev->terminal_implict_fi;/*implicit values*/
			pdev->terminal_di = pdev->terminal_implict_di;
		}
		else
		{
			if( ( su_atr->ta[ 1 ] & 0x0f ) > 1 )
			{
				return ICC_ERR_ATR_TA2;
			}

			if( TST_BIT( su_atr->ta_flag, 0 ) )
			{/*Apply specific parameter values.*/
				if( 0 != fi_const_table( su_atr->ta[ 0 ] / 16 ) && 0 != di_const_table( su_atr->ta[ 0 ] % 16 ) )
				{
					pdev->terminal_fi 	  = fi_const_table( su_atr->ta[ 0 ] / 16 );
					pdev->terminal_di 	  = di_const_table( su_atr->ta[ 0 ] % 16 );
					pdev->terminal_ptype  = su_atr->ta[ 1 ] & 0x0f;/*?*?*?*/
				}
				else
				{
					pdev->terminal_fi = 372;
					pdev->terminal_di = 1;
				}
			}
		}
	}
	
	/* TB1 and TB2 are deprecated. The card should not transmit them. The interface 
	 * device shall ignore them.
	* NOTE:
	*     The first two editions of ISO/IEC 7816-3 specified TB1 and TB2 to fix 
	* electrical parameters of the integrated circuit for the deprecated use of
	* contact C6 (see 5.1.1).
	*/
	
	  ;
	
	/*
	 * TC1 encodes the extra guard time integer (N) from 0 to 255 over the eight bits. 
	 * The default value is N = 0. 
	 *   -  If N = 0 to 254, then before being ready to receive the next character, 
	 *   the card requires the following delay from the leading edge of the previous 
	 *   character (transmitted by the card or the interface device).
     *              GT = 12 etu + R ¡Á N / f
     *   If T=15 is absent in the Answer-to-Reset, then R = F / D, 
     *   i.e., the integers used for computing the etu.
     *   If T=15 is present in the Answer-to-Reset, then R = Fi / Di, 
     *   i.e., the integers defined above by TA1.
     * No extra guard time is used to transmit characters from the card: GT = 12 etu.
     *   - The use of N = 255 is protocol dependent: GT = 12 etu in PPS (see 9) and in T=0 (see 10). 
     * For the use of N = 255 in T=1, see 11.2.
	 */
	if( TST_BIT( su_atr->tc_flag, 0 ) )
	{
		if( su_atr->tc[ 0 ] < 255 )
		{
			pdev->terminal_cgt = 12 + su_atr->tc[ 0 ];
			ext	= su_atr->tc[ 0 ];
		}
		else
		{
			if( 0 == pdev->terminal_ptype )
			{
				pdev->terminal_cgt =  12;
			}
			if( 1 == pdev->terminal_ptype )
			{
				pdev->terminal_cgt =  11;
			}
			ext	= -1;
		}
	}
	else
	{
		/*TC1 is absent, as though TC1='00' has been return.*/
		pdev->terminal_cgt =  12;
	}
	
	/*
	 * -  If present in the Answer-to-Reset, the interface byte TC2 encodes the 
	 * waiting time integer WI over the eight bits, except the value '00' reserved 
	 * for future use. 
	 * -  If TC2 is absent, then the default value is WI = 10.
     * The "waiting time" (see 7.2) shall be:
     *    WT = WI ¡Á 960 ¡Á Fi/f. 
     *    ETU = Fi/( Di ¡Á f )
     *
     *    WT = WI ¡Á 960 ¡Á ETU ¡Á Di.
	 */
	pdev->terminal_wwt = ( 960 * 10 + 480 + 16 ) * pdev->terminal_di;
	if( TST_BIT( su_atr->tc_flag, 1 ) )
	{
		if( 0x00 == su_atr->tc[ 1 ] )
		{
			return ICC_ERR_ATR_TC2;
		}
		pdev->terminal_wwt = ( 960 * su_atr->tc[ 1 ] + 480 + 16 ) * pdev->terminal_di;
	}
	
	/**
	 *  TA1, TB1, TC1, TA2 and TB2 are global. TC2 is specific to T=0, see 10.2.
     *  The interpretation of TAi TBi TCi for i > 2 depends on the type T 
     *  encoded in TD[i¨C1].
     *  1) After T from 0 to 14, TAi TBi and TCi are specific to the transmission
     *  protocol T.
     *  2) After T=15, TAi TBi and TCi are global.
     *  If more than three interface bytes TAi TBi TCi TAi+1 TBi+1 TCi+1 ¡­ are
     *  defined for the same type T, then each one is unambiguously identified 
     *  by its position after the first, the second ¡­ occurrence of T in TDi¨C1 
     *  for i > 2.
     *  Consequently, for each type T, the first TA TB TC, the second TA TB TC, 
     *  and so on, are available.
     * NOTE 
     *  The combination of the type T with the bitmap technique allows transmit-
     *  ting only useful interface bytes and when needed, to use default values 
     *  for parameters corresponding to absent interface bytes.
     *  For example, clause 11.4 specifies three interface bytes specific to T=1,
     *  namely the first TA, TB and TC for T=1. 
     *  If needed, such a byte shall be transmitted respectively as TA3 TB3 and 
     *  TC3 after TD2 indicating T=1.
     *  Depending on whether the card also offers T=0 or not, TD1 shall indicate 
     *  either T=0 or T=1. 
	 */
	cwi = 13;
	bwi = 4;
	pdev->emv_card_ifs = 32;
	
	i = 1;  /* indicate TD2... */
	while( TST_BIT( su_atr->td_flag, i ) )
	{
		T = su_atr->td[ i ] & 0x0f;
		i++; /* tell TA( i + 2 ), TB( i + 2 )... */
		
		switch( T )
		{
		case 1: /* T=1 specific interface bytes */
			if( TST_BIT( su_atr->ta_flag, i ) )
			{
				if( su_atr->ta[ i ] == 0x00 || su_atr->ta[ i ] == 0xff )
				{
					return ICC_ERR_ATR_TA3;
				}
				pdev->emv_card_ifs = su_atr->ta[ i ];
			}

			if( TST_BIT( su_atr->tb_flag, i ) )
			{
				bwi = su_atr->tb[ i ] / 16;
				cwi = su_atr->tb[ i ] % 16;
				if( bwi > 9 || cwi > 15 )
				{
					return ICC_ERR_ATR_TB3;
				}
				if( ( 1 << cwi ) <= ( ext + 1 ) )
				{
					return ICC_ERR_ATR_TB3;
				}
			}
		break;

		case 15: /* global interface bytes */
		/* The first TA for T=15 encodes the clock stop indicator (X) and 
		 * the class indicator (Y). The default values are X = "clock stop
		 * not supported" and Y = "only class A supported". 
		 * For the use of clock stop, see 6.3.2. For the use of the classes 
		 * of operating conditions
		 * the bit7~bit6 indicate that card support whether stop clock siganl 
		 * or no.
		 */
			if( TST_BIT( su_atr->ta_flag, i ) )
			{
				switch( su_atr->ta[ i ] & 0xC0 )
				{
					case 0x00:
						pdev->allow_stop_clock = 0;
					break;

					case 0x40:
						pdev->allow_stop_clock = 1;
					break;

					case 0x80:
						pdev->allow_stop_clock = 2;
					break;

					case 0xC0:
						pdev->allow_stop_clock = 3;
					break; 
				}
				
				/* the bit5~bit0 indicate the operator condition of card */
				switch( su_atr->ta[ i ] & 0x3F )
				{
					case 1:
					    pdev->allow_ops_condition = 0; /* A */
					break;
					
					case 2:
					    pdev->allow_ops_condition = 1; /* B */
					break;
					
					case 3:
					    pdev->allow_ops_condition = 3; /* A and B */
					break;
					
					case 4:
					    pdev->allow_ops_condition = 2; /* C */
					break;
					
					case 6:
					    pdev->allow_ops_condition = 4; /* B and C */
					break;
					
					case 7:
					    pdev->allow_ops_condition = 5; /* A,B and C */
					break;
					
					default:
					break;   
				}
			}
		/* The first TB for T=15 indicates the use of SPU by the card 
		 * (see 5.2.4). The default value is "SPU not used".
		* Coded over bits 7 to 1, the use is either standard (bit 8 
		* set to 0), or proprietary (bit 8 set to 1). The value '00'
		* indicates that the card does not use SPU. ISO/IEC JTC 1/SC 
		* 17 reserves for future use any other value where bit 8 is 
		* set to 0. 
		*/
			if( TST_BIT( su_atr->tb_flag, i ) )
			{
				if( su_atr->tb[ i ] & 0x80 )
				{ /* proprietary. */
					pdev->card_use_spu = su_atr->tb[ i ]; 
				}
				else
				{ /* standard */
					if( su_atr->tb[ i ] & 0x7F )
					{ /* reserved. */  
						pdev->card_use_spu = ( su_atr->tb[ i ] & 0x7F << 8 );
					}
					else
					{ /* not use SPU */
						pdev->card_use_spu = 0;   
					}
				}
			}
		break;

		default:
		break;
		}
	}
	pdev->terminal_bwt = ( ( 1 << bwi ) * 960 + 11 + 960 ) * pdev->terminal_di;
	pdev->terminal_cwt = pdev->terminal_di * ( ( 1 << cwi ) + 11 + 4 );
	
	/*
	 * The initial size immediately following the answer to reset shall be 254 bytes, 
	 * and this size shall be used throughout the rest of the card session.
	 */
	pdev->terminal_ifs  = 32; /* default value */
	pdev->terminal_ipcb = 0;
	pdev->terminal_pcb  = 0;
	pdev->emv_card_ipcb = 0;
	pdev->emv_card_pcb  = 0;
	if( 0 == pdev->terminal_ptype )
	{
		pdev->terminal_igt  = pdev->terminal_cgt > 16 ? pdev->terminal_cgt : 16; /* minimum value is 16. */
	}
	else
	{
		pdev->terminal_igt  = 22; /* BGT = 22 */
	}
	
	/* Notice:
	 *    the state machine change.
	 */
	pdev->terminal_state= EMV_READY;
	pdev->terminal_open = 1;
	
	return 0;
}

/**
 * ===================================================================
 * implement the process of PPS specified in ISO7816-3 specification
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input/ouput]pdev  : information on logic device
 * -------------------------------------------------------------------
 * [input]su_atr   : received ATR
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *       please refer to the document "emv_errno.h"
 * ===================================================================
 */
int sci_iso_pps_procedure( struct emv_core *pdev, struct emv_atr *su_atr )
{
    int           Fi        = 0;
    int           Di        = 0;
    int           state     = 0;
    unsigned char pps[ 4 ]  = { 0xff, 0x10, 0x13, 0x00 };
    unsigned char ch        = 0;
    unsigned char pps0      = 0;
    unsigned char pck       = 0;
    int           wwt       = pdev->terminal_wwt / pdev->terminal_di;
    int           bwt       = pdev->terminal_bwt / pdev->terminal_di;
    int           cwt       = pdev->terminal_cwt / pdev->terminal_di;
    
    /*
     * After completion of the answer to reset, the card shall wait for characters 
     * from the interface device: their transmission is governed by transmission 
     * parameters (see 7.1); their interpretation is governed by a protocol (see 9, 10 and 11). 
     * Figure 4 illustrates the principles of selection of transmission parameters and protocol.
     * 1. If TA2 (see 8.3) is present in the Answer-to-Reset (card in specific mode), 
     * then the interface device shall start the specific transmission protocol using the
     * specific values of the transmission parameters.
     * 2. Otherwise (card in negotiable mode), for the transmission parameters, the 
     * values used during the answer to reset (i.e., the default values of the transmission 
     * parameters, see 8.1) shall continue to apply as follows.
     *     a. If the value of the first character received by the card is 'FF', then the 
     * interface device shall have started a PPS exchange (see 9); the default values of 
     * the transmission parameters shall continue to apply until completion of a successful 
     * PPS exchange (see 9.3), after what the interface device shall start the negotiated 
     * transmission protocol using the negotiated values of the transmission parameters.
     *     b. Otherwise, the interface device shall have started the "first offered transmission 
     * protocol" (see TD1 in 8.2.3). The interface device shall do so when the card offers only 
     * one transmission protocol and only the default values of the transmission parameters. 
     * Such a card need not support PPS exchange.
     *
     * In the following three cases: overrun of WT, erroneous PPS response, unsuccessful 
     * PPS exchange, the interface device shall perform a deactivation.
     */
	if( TST_BIT( su_atr->ta_flag, 1 ) )
	{
	/* ===========================================================================================
	 * In <ISO/IEC 7816-3> description:
	 * ===========================================================================================
	 * TA2 is the specific mode byte as shown in Figure 15. For the use of TA2, see 6.3.1 and 7.1.
	 * Bit 8 indicates the ability for changing the negotiable/specific mode:
	 *    - capable to change if bit 8 is set to 0;
	 *    - unable to change if bit 8 is set to 1.
	 * Bits 7 and 6 are reserved for future use (set to 0 when not used).
	 * Bit 5 indicates the definition of the parameters F and D.
	 *    - If bit 5 is set to 0, then the integers Fi and Di defined above by TA1 shall apply.
	 *    - If bit 5 is set to 1, then implicit values (not defined by the interface bytes) shall 
	 *      apply. Bits 4 to 1 encode a type T.
	 * =========================================================================================== 
	 * In < EMV 4.1 > description:
	 * ===========================================================================================
	 * b8 indicates whether the ICC is capable of changing its mode of operation. It is capable 
	 * of changing if b8 is set to 1, and unable to change if b8 is set to 0.
	 * ===========================================================================================
	 */
		return 0;
	}

	if( TST_BIT( su_atr->ta_flag, 0 ) )/*TA1 exist*/
	{
		Fi = su_atr->ta[ 0 ] / 16;
		Di = su_atr->ta[ 0 ] % 16;
	}
	else
	{
		return 0; 
	}

	/* restore parameter when cold resetting */
	pdev->terminal_fi = pdev->terminal_implict_fi;
	pdev->terminal_di = pdev->terminal_implict_di;

	/* ready datas for PPS */
	pps[ 1 ]  = 0x10 | pdev->terminal_ptype;/*PPS0*/
	pps[ 2 ]  = Fi * 16 + Di;  /*PPS1*/
	pps[ 3 ]  = pps[ 0 ] ^ pps[ 1 ] ^ pps[ 2 ];/*PCK*/
	sci_queue_flush( pdev );
	sci_queue_fill( pdev, pps, 4 );
	sci_emv_hard_xmit( pdev );

	/*PPSS*/
	state = sci_queue_spill( pdev, &ch, 1 );
	if( state )
	{
		return state;   
	}
	if( 0xff != ch )
	{
		return ICC_ERR_PPSS;
	}
	pck = ch;

	/*PPS0*/
	state = sci_queue_spill( pdev, &ch, 1 );
	if( state )
	{
		return state;   
	}
	pps0 = ch;
	pck ^= ch;
	/*
	* PPS1 allows the interface device to propose values of F and D to the card. 
	* Encoded in the same way as in TA1, these values shall be from Fd to Fi and 
	* from Dd to Di respectively. If an interface device does not transmit PPS1, 
	* it proposes to continue with Fd and Dd. 
	* The card either acknowledges both values by echoing PPS1 (then these values 
	* become Fn and Dn) or does not transmit PPS1 to continue with Fd and Dd (then 
	* Fn = 372 and Dn = 1).
	*/
	if( TST_BIT( pps0, 4 ) )
	{
		state = sci_queue_spill( pdev, &ch, 1 );
		if( state )
		{
			return state;   
		}
		pck ^= ch;

		if( pps[ 2 ] == ch )
		{
			pdev->terminal_fi  = fi_const_table( ch / 16 );
			pdev->terminal_di  = di_const_table( ch % 16 );

			/* added on 2010-6-17 */
			pdev->terminal_wwt = wwt * pdev->terminal_di;
			pdev->terminal_bwt = bwt * pdev->terminal_di;
			pdev->terminal_cwt = cwt * pdev->terminal_di;
		}
		else
		{
			return ICC_ERR_PPS1;   
		}
	}
	else
	{ /* don't change parameter to continue session. */
	 ;
	}

	if( TST_BIT( pps0, 5 ) )
	{
		state = sci_queue_spill( pdev, &ch, 1 );
		if( state )
		{
			return state;   
		}
		pck ^= ch;
	}

	if( TST_BIT( pps0, 6 ) )
	{
		state = sci_queue_spill( pdev, &ch, 1 );
		if( state )
		{
			return state;   
		}
		pck ^= ch;
	}

	state = sci_queue_spill( pdev, &ch, 1 );
	if( state )
	{
		return state;   
	}

	pck ^= ch;
	if( pck )
	{
		return ICC_ERR_PCK;
	}

	return 0;
}


