#ifndef PHILIPS_MIFARE_H
#define PHILIPS_MIFARE_H

#ifdef __cplusplus
extern "C" {
#endif

#define PHILIPS_MIFARE_AUTHEN_TOV    ( 560 )
#define PHILIPS_MIFARE_OPS_TOV       ( 1024 )

#define PHILIPS_CMD_AUTHEN_KEYA      ( 0x60 )
#define PHILIPS_CMD_AUTHEN_KEYB      ( 0x61 )
#define PHILIPS_CMD_READ_BLOCK       ( 0x30 )
#define PHILIPS_CMD_WRITE_BLOCK      ( 0xA0 )
#define PHILIPS_CMD_INCREASE         ( 0xC1 )
#define PHILIPS_CMD_DECREASE         ( 0xC0 )
#define PHILIPS_CMD_BACKUP           ( 0xC2 )
#define PHILIPS_CMD_TRANSFER         ( 0xB0 )

#define PHILIPS_RESPONSE_ACK         ( 0x0A )
#define PHILIPS_RESPONSE_NOALLOW     ( 0x04 )
#define PHILIPS_RESPONSE_NACK        ( 0x05 )

/*mifare card type*/
#define PHILIPS_MIFARE_ULTRALIGHT     ( 0x01 ) /*non-crypto1*/
#define PHILIPS_MIFARE_STANDARD_MINI  ( 0x02 ) /*crypto1*/
#define PHILIPS_MIFARE_STANDARD_1K    ( 0x03 ) /*crypto1*/
#define PHILIPS_MIFARE_STANDARD_4K    ( 0x04 ) /*crypto1*/
#define PHILIPS_MIFARE_PLUS_2K        ( 0x05 ) 
#define PHILIPS_MIFARE_PLUS_4K        ( 0x06 )

/**
 * ananlysing the protocol supported by PICC according to 
 * Fig 3. in "AN 10834 Mifare ISO/IEC 14443 PICC Selection"
 *
 * params:
 *        ucSak : the picc response of selection
 * retval:
 *        0 - OK;
 *        others, need to distinguish between iso14443-4 and 
 *        "mifare pro"
 */
int MifareTypeCheck( struct ST_PCDINFO *ptPcdInfo, unsigned char ucSak );

/**
 * mifare three authenticate between ptPcdInfo and PICC.
 *  
 * params:
 *         ptPcdInfo: device resource handle
 *         ucType   : the password type, 'A' or 'B'
 *         ucBlkNo  : the block number of authentication.
 *         pucPwd   : the password( 6 bytes )
 * retval:
 *         0 - successfully
 *         others, failure
 */
int MifareAuthority(  struct ST_PCDINFO  *ptPcdInfo,
                      unsigned char      ucType, 
                      unsigned char      ucBlkNo, 
                      unsigned char*     pucPwd );

/**
 * mifare standard operate( increment/decrement/backup ).
 *  
 * params:
 *         ptPcdInfo    : device resource handle
 *         ucOpCode     : operation type
 *                       'w' or 'W'  - write the pval to src_blk
 *                       'r' or 'R'  - read datas from src_blk and save in pval
 *                       '+'         - added the pval to the src_blk 
 *                       '-'         - the pval substracted from the src_blk 
 *                       '>'         - the src_blk back up to the des_blk
 *                       'h' or 'H'  - halt mifare card, it don't need any parameters 
 *         ucSrcBlkNo  : operation block number
 *         pucVal      : operation value( if need )
 *         ucDesBlkNo  : operation destinct block number( if need )
 * retval:
 *         0 - successfully
 *         others, failure
 */
int MifareOperate( struct ST_PCDINFO  *ptPcdInfo,
                   unsigned char      ucOpCode, 
                   unsigned char      ucSrcBlkNo, 
                   unsigned char*     pucVal, 
                   unsigned char      ucDesBlkNo );


#define PHILIPS_MIFARE_ERR_NACK    ( -200 )
#define PHILIPS_MIFARE_ERR_COMM    ( -201 ) 
#define PHILIPS_MIFARE_ERR_AUTHEN  ( -202 ) 


#ifdef __cplusplus
}
#endif

#endif
