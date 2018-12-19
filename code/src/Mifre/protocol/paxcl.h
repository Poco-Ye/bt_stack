#ifndef PAXCL_H
#define PAXCL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ptPcdInfo polling A cards in RF field.
 * 
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        > 0 - successfully.
 *        < 0, error.
 */
int PaxAPolling( struct ST_PCDINFO *ptPcdInfo );

/**
 * ptPcdInfo polling B cards in RF field.
 * 
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        > 0 - successfully.
 *        < 0, error.
 */
int PaxBPolling( struct ST_PCDINFO *ptPcdInfo );

/**
 * ptPcdInfo polling mifare cards in RF field.
 * 
 * params: 
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        > 0 - successfully.
 *        < 0, error.
 */
int PaxMPolling( struct ST_PCDINFO *ptPcdInfo );

void PcdMemset( PICC_PARA *pSTPiccPara );
void PcdMemcpy( PICC_PARA *pSTPiccPara, PICC_PARA *pSTSourcePiccPara );
int  GetParamTagValue( PICC_PARA *ptParam, unsigned char *pucRfPara);
int  SetParamValue( PICC_PARA *ptPiccPara );
int GetRfChipType(unsigned char * ucRfChipType);

#ifdef __cplusplus
}
#endif

#endif
