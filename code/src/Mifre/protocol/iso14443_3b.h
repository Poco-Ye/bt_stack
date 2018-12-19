#ifndef ISO14443_B_H
#define ISO14443_B_H

#ifdef __cplusplus
extern "C" {
#endif

#define ISO14443_TYPEB_COMMAND_REQB   ( 0x05 )
#define ISO14443_TYPEB_COMMAND_WUPB   ( 0x05 )
#define ISO14443_TYPEB_FLAG_REQB      ( 0x00 )
#define ISO14443_TYPEB_FLAG_WUPB      ( 0x08 )
#define ISO14443_TYPEB_COMMAND_ATTR   ( 0x1D )
#define ISO14443_TYPEB_COMMAND_HLTB   ( 0x50 )

#define ISO14443_TYPEB_RESP_REQB      ( 0x50 )

#define ISO14443_TYPEB_FWT_REQB       ( 60 + 34 )
#define ISO14443_TYPEB_FWT_WUPB       ( 60 + 34 )

/**
 * ptPcdInfo send REQB command to PICC, and receive the ATQB from PICC.
 * 
 * params: 
 *        ptPcdInfo   : the deivce handler
 *        pucAtqb     : the buffer for ATQB, 11 bytes, consist of PUPI,
 *                      Application Data and Protocol Info
 * retval:
 *        0 - successfully, the ATQB is valid, consist of two bytes.
 *        others, error.
 */
int ISO14443_3B_Req( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAtqb );

/**
 * ptPcdInfo send WUPB command to PICC, and receive the ATQB from PICC.
 * 
 * params: 
 *        ptPcdInfo   : the deivce handler
 *        pucAtqb     : the buffer for ATQB, 11 bytes, consist of PUPI,
 *                      Application Data and Protocol Info
 * retval:
 *        0 - successfully, the ATQB is valid, consist of two bytes.
 *        others, error.
 */
int ISO14443_3B_Wup( struct ST_PCDINFO* ptPcdInfo, unsigned char* pucAtqb );
                   
/**
 * ptPcdInfo send the Slot-Mark command to PICC, and define the time slot.
 *
 * params:
 *         ptPcdInfo : the device handler
 *         ucApn     : the slot number, format is (nnnn0101)b
 *         pucResp   : the response from PICC
 *
 */
int ISO14443_3B_SlotMarker( struct ST_PCDINFO*  ptPcdInfo, 
                            unsigned char      ucApn, 
                            unsigned char*     pucResp );
                              
/**
 * PCD send ATTRIB command to PICC, and receive the SAK from PICC.
 * 
 * params: 
 *        ptPcdInfo    : the deivce handler
 *        pucDatas     : higher layer INF.(command and response )
 *        piNum        : the number of higher layer INF.
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int ISO14443_3B_Attri( struct ST_PCDINFO* ptPcdInfo, 
                       unsigned char*    pucDatas, 
                       int *piNum );
/**
 * PCD send HALTB command to PICC.
 * 
 * params: 
 *        ptPcdInfo  : the deivce handler
 * retval:
 *        0 - successfullys.
 *        others, error.
 */                      
int ISO14443_3B_Halt( struct ST_PCDINFO* ptPcdInfo );

/*error definitions*/
#define ISO14443_TYPEB_ERR_NUMBER       ( -81 )
#define ISO14443_TYPEB_ERR_ATQB0        ( -82 )
#define ISO14443_TYPEB_ERR_PROT         ( -83 )
#define ISO14443_TYPEB_ERR_CID          ( -84 )
#define ISO14443_TYPEB_ERR_HLTB         ( -85 )
 
#ifdef __cplusplus
}
#endif

#endif
