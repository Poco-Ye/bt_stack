#ifndef ISO14443_4_H
#define ISO14443_4_H

#ifdef __cplusplus
extern "C" {
#endif 

#define ISO14443_4_DEBUG                          ( 0 )

enum EXC_STEP 
    { 
        ORG_IBLOCK = 1,
        ORG_ACKBLOCK,  
        ORG_NACKBLOCK,
        ORG_SBLOCK,
        ORG_TRARCV, /*transmitted and received*/
        RCV_IBLOCK,
        RCV_RBLOCK,
        RCV_SBLOCK,
        RCV_INVBLOCK,/*protocol error*/ 
        NON_EVENT /*end*/
    };

/*the re-transmission maxium count*/
#define ISO14443_PROTOCOL_RETRANSMISSION_LIMITED  ( 2 )

/**
 * protocol PCB structure 
 */
#define ISO14443_CL_PROTOCOL_CHAINED              ( 0x10 )
#define ISO14443_CL_PROTOCOL_CID                  ( 0x8 )
#define ISO14443_CL_PROTOCOL_NAD                  ( 0x4 )
#define ISO14443_CL_PROTOCOL_ISN                  ( 0x1 )


/**
 * implement the half duplex communication protocol with ISO14443-4
 * 
 * parameters:
 *             ptPcdInfo  : PCD information structure pointer
 *             pucSrc     : the datas information will be transmitted by ptPcdInfo
 *             iTxNum     : the number of transmitted datas by ptPcdInfo
 *             pucDes     : the datas information will be transmitted by PICC
 *             piRxN      : the number of transmitted datas by PICC.
 * retval:
 *            0 - successfully
 *            others, error.
 */
int ISO14443_4_HalfDuplexExchange( struct ST_PCDINFO *ptPcdInfo, 
                                   unsigned char *pucSrc, 
                                   int iTxN, 
                                   unsigned char *pucDes, 
                                   int *piRxN );

/**
 * send 'DESELECT' command to PICC
 *
 * param:
 *       ptPcdInfo  :  PCD information structure pointer
 *       ucSPcb     :  S-block PCB
 *                       0xC2 - DESELECT( no parameter )
 *                       0xF2 - WTX( one byte wtx parameter )
 *       ucParam    :  S-block parameters
 * reval:
 *       0 - successfully
 *       others, failure
 */
int ISO14443_4_SRequest( struct ST_PCDINFO *ptPcdInfo, 
                         unsigned char      ucSPcb,
                         unsigned char      ucParam );

#define ISO14443_4_ERR_PROTOCOL  ( -100 )

#ifdef __cplusplus
}
#endif

#endif






