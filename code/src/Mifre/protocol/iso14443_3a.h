#ifndef ISO14443_A_H
#define ISO14443_A_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * the command definitions to IEC/ISO14443-3-A
 *
 */
#define ISO14443_A_COMMAND_REQA          ( 0x26 )
#define ISO14443_A_COMMAND_WUPA          ( 0x52 )
#define ISO14443_A_COMMAND_HALT          ( 0x50 )
#define ISO14443_A_COMMAND_SEL1          ( 0x93 )
#define ISO14443_A_COMMAND_SEL2          ( 0x95 )
#define ISO14443_A_COMMAND_SEL3          ( 0x97 )
#define ISO14443_A_SELECT_NVB( x )       ( ( ( 16 + x ) / 8 ) << 4 + ( x % 8 ) )
#define ISO14443_A_CASCADE_TAG           ( 0x88 )
#define ISO14443_A_COMMAND_RATS          ( 0xE0 )
#define ISO14443_A_COMMAND_PPS           ( 0xD0 )

/**
 * the bit of SAK definitions
 */
#define ISO14443_A_SAK_CASCADE           ( 1 << 2 )
#define ISO14443_A_SAK_COMPLIANT         ( 1 << 5 )

/*
 * the frame wait time macro definitions
 */
#define ISO14443_TYPEB_FWT_MIN           ( 9 )
#define ISO14443_TYPEB_FWT_REQA          ( 9 )
#define ISO14443_TYPEB_FWT_WUPA          ( 9 )
#define ISO14443_TYPEB_FWT_RATS          ( 560 )
#define ISO14443_TYPEB_FWT_HLTA          ( 107 )

/**
 * Calculting the BCC byte with the string, be used in AntiCollision/Select command.
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
int ISO14443_3A_CalBcc( const unsigned char* pucBuf, 
                        int iNum, 
                        unsigned char* pucBcc );

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
int ISO14443_3A_Req( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAtqa );

/**
 * ptPcdInfo send WUPA command to PICC, and receive the ATQA from PICC.
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
int ISO14443_3A_Wup( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAtqa );

/**
 * ptPcdInfo send ANTICOLLISION command to PICC, and receive the UID from PICC.
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
int ISO14443_3A_AntiSel( struct ST_PCDINFO* ptPcdInfo, unsigned char *pucSak );

/**
 * ptPcdInfo send HALT command to PICC.
 * 
 * params:
 *        ptPcdInfo   : the deivce handler
 * retval:
 *        0 - successfullys.
 *        others, error.
 */
int ISO14443_3A_Halt( struct ST_PCDINFO* ptPcdInfo );

/**
 * Request answer to selection.( defined by iso14443-4 )
 *  
 * params:
 *       ptPcdInfo  : the deivce handler
 *       pucAts     : the response from PICC( space must be more than 256 bytes ).
 * retval:
 *       0 - successfullys.
 *       others, error.
 */
int ISO14443_4A_Rats( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAts );

/**
 * Protocol and parameters selection requestion
 *  
 * params:
 *       ptPcdInfo    : the deivce handler
 * retval:
 *       0 - successfullys.
 *       others, error.
 */
int ISO14443_3A_Pps( struct ST_PCDINFO* ptPcdInfo );

#define ISO14443_TYPEA_ERR_NUMBER       ( -51 )
#define ISO14443_TYPEA_ERR_ATQA0        ( -52 )
#define ISO14443_TYPEA_ERR_ATQA1        ( -53 )
#define ISO14443_TYPEA_ERR_BCC          ( -54 )
#define ISO14443_TYPEA_ERR_CASCADE_TAG  ( -55 )
#define ISO14443_TYPEA_ERR_SELECT       ( -56 )
#define ISO14443_TYPEA_ERR_HLTA         ( -57 )
#define ISO14443_TYPEA_ERR_TL           ( -58 )
#define ISO14443_TYPEA_ERR_T0           ( -59 )
#define ISO14443_TYPEA_ERR_TA1          ( -60 )
#define ISO14443_TYPEA_ERR_TB1          ( -61 )
#define ISO14443_TYPEA_ERR_TC1          ( -62 )
#define ISO14443_TYPEA_ERR_PPSS         ( -63 )
#define ISO14443_TYPEA_ERR_STATE        ( -64 )

#ifdef __cplusplus
}
#endif

#endif
