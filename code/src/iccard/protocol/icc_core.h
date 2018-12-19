#ifndef _ICC_CORE_H
#define _ICC_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "..\hardware\icc_config.h"

#define MAX_QBUF_SIZE   ( 512 )
#define MAX_APDU_SIZE   ( 256 )
#define T1_REPEAT_LMT 	( 4   )
#define INCONV_CH	( 0x3F )
#define CONV_CH		( 0x3B )

#define SET_BIT( x, y ) (x) |=  ( 1 << (y) ) 
#define CLR_BIT( x, y ) (x) &= ~( 1 << (y) )
#define	TST_BIT( x, y ) ( (x) & ( 1 << (y) ) )

#define IMR_BIT		( 5 )
#define ISN_BIT		( 6 )
#define RSN_BIT		( 4 )
#define SRS_BIT		( 5 )

#define CMP_IRSN_EQU( i, r ) 	( ( ( (i) & ( 1 << ISN_BIT ) ) >> 2 ) == ( ( r ) & ( 1 << RSN_BIT ) ) )
#define ISN_INC( x )         	( (x) ^ ( 1 << ISN_BIT ) )
#define ISN_DEC( x )          	ISN_INC( x )

#define ISN_RSN( x )    ( ( (x) & ( 1 << ISN_BIT ) ) >> 2 )         
#define RSN_ISN( x )    ( ( (x) & ( 1 << RSN_BIT ) ) << 2 )

/*communication buffer*/
#define INVALID_INDEX ( -1 )

/*the core data structure for EMV and ISO7816-3*/
struct emv_core
{
	int           terminal_ch; /* hardware channel */
	int           terminal_exist;/* card existence */
	int           terminal_open; /* device operating */
	int           terminal_mode; /* Work Mode: 1 - Synchronize Mode; 0 - Asynchronize Mode */
	int           terminal_state; /* current card will be or is status */
#define EMV_IDLE        ( 1 )
#define EMV_COLD_RESET  ( 2 )
#define EMV_WARM_RESET  ( 3 )
#define EMV_READY       ( 4 )
	int           terminal_vcc; /* voltage, by mV */
	int           terminal_auto; /* auto-send GET RESPONSE Command for T = 0 */
	int           terminal_pps;
	int           terminal_spec;
	int           terminal_conv; /* conversion logical : 0 - direction; 1 - reverse direction */
	int           terminal_ptype; /* the type of protocol: 1 - T = 1; 0 - T = 0 */
	int           terminal_fi;
	int           terminal_di;
	int           terminal_implict_fi; /* defined by ISO7816-3 in TA2. */
	int           terminal_implict_di;
	unsigned int  terminal_cgt;
	unsigned int  terminal_igt;
	unsigned int  terminal_bwt;
	unsigned int  terminal_wwt;
	unsigned int  terminal_cwt;
	int           terminal_ifs;
	int           emv_card_ifs;
	int           allow_stop_clock;     /* 0 - not support; 1 - stop low; 2 - stop high; 3 - no preference */
	int           allow_ops_condition;  /* 0 - A; 1 - B; 2 - C; 3 - A & B; 4 - B & C; 5 - A, B & C */
	int           card_use_spu;         /* 0 - not use; 1 - proprietary; other value - voltage */
	unsigned char emv_card_nad;
	unsigned char emv_repeat;
	unsigned char terminal_ipcb;
	unsigned char terminal_pcb;
	struct emv_queue *queue; /* the pointer of data buffer */

	unsigned char emv_card_ipcb;
	unsigned char emv_card_pcb;
	unsigned char card_trans_state; 
    
};

/* the ATR data struct */
struct emv_atr
{
	unsigned char 		ts;
	unsigned char 		t0;
	unsigned char		ta_flag;
	unsigned char		tb_flag;
	unsigned char		tc_flag;
	unsigned char		td_flag;
	unsigned char		ta[ 8 ];
	unsigned char		tb[ 8 ];
	unsigned char		tc[ 8 ];
	unsigned char		td[ 8 ];
	unsigned char		hbytes[ 15 ];
};

/* the Command data struct under TAL */
struct emv_apdu_req
{
	unsigned char cmd[ 4 ];
	int           lc;
	int           le;
	unsigned char data[ MAX_APDU_SIZE ];
};

/* the Response data struct under TAL */
struct emv_apdu_rsp
{
	int           len;
	unsigned char swa;
	unsigned char swb;
	unsigned char data[ MAX_APDU_SIZE + 2 ]; /* more than 2 bytes for receiving block 
							in t=1 to process swa/swb. */
};

/* the temperory data struct when process in T=1 */
struct emv_t1_block
{
	unsigned char nad;
	unsigned char pcb;
	unsigned char len;
	unsigned char data[ MAX_APDU_SIZE ];
	unsigned char lrc;
};

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
int fi_const_table( int index );

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
int di_const_table( int index );

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
int fi_const_index( int fi );

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
int di_const_index( int di );

#define CRAD_RECEI_DATA (0)
#define CRAD_TRANS_DATA (1)

/* ===================================================================
 * To get the data of ATR following the ISO7816-3 specification
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * [output]su_atr   : Data of ATR in the format of structure
 * -------------------------------------------------------------------
 * [ouput]atr       : Data of ATR in the format of string
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 */
int sci_emv_atr_analyser( struct emv_core *terminal, struct emv_atr *su_atr, unsigned char *atr );

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
int sci_emv_atr_parse( struct emv_core *terminal, struct emv_atr *su_atr );

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
int sci_emv_t0_exchange(struct emv_core     *terminal, struct emv_apdu_req *apdu_req, struct emv_apdu_rsp *apdu_resp);

/* ===================================================================
 * Data Exchange When T = 1
 * -------------------------------------------------------------------
 * Parameter£º
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * [input]apdu_req  : command structure pointer of the transmitting
 * 		      Data
 * -------------------------------------------------------------------
 * [output]apdu_resp : response structure pointera of the received Data
 * ------------------------------------------------------------------- 
 * return£º
 * -------------------------------------------------------------------
 *    please refer to the document "emv_errno.h"
 * ===================================================================
 */
int sci_emv_t1_exchange(struct emv_core     *terminal, struct emv_apdu_req *apdu_req, struct emv_apdu_rsp *apdu_resp);

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
int sci_emv_t1_ifsd_request( struct emv_core *terminal );

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
unsigned char  adjust_return_value_to_monitor( int errno );

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
int sci_iso_atr_parse( struct emv_core *pdev, struct emv_atr *su_atr );

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
int sci_iso_pps_procedure( struct emv_core *pdev, struct emv_atr *su_atr );    
                         

#ifdef __cplusplus
}
#endif
#endif

