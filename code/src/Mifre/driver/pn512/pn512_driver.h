#ifndef PN512DRIVER_H
#define PN512DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_RF_TIMER                     ( 0x02 )

#define PN512_HW_DEVICE_IDLE             ( 0x08 )
#define PN512_HW_TRANSFERRING            ( 0x07 )
#define PN512_HW_TRANSMISSION_COMPLETED  ( 0x01 )
#define PN512_HW_RECEPTION_COMPLETED     ( 0x02 )
#define PN512_HW_ERROR_OCCURED           ( 0x03 )
#define PN512_HW_LPCD_DETECTED           ( 0x04 )
#define PN512_HW_RX_WAIT_TIMEOUT         ( 0x05 )
#define PN512_HW_PN512_RXSOF             ( 0x06 )

int  Pn512Init( struct ST_PCDINFO* );
int  Pn512CarrierOn( struct ST_PCDINFO* );
int  Pn512CarrierOff( struct ST_PCDINFO* );
int  Pn512Config( struct ST_PCDINFO* );
int  Pn512Waiter( struct ST_PCDINFO*, unsigned int );
int  Pn512MifareAuthenticate( struct ST_PCDINFO* );
int  Pn512Trans( struct ST_PCDINFO* );
int  Pn512TransCeive( struct ST_PCDINFO* );
void Pn512CommIsr( void );

int  Pn512SetParamValue(PICC_PARA *ptPiccPara );
int  Pn512GetParamTagValue(PICC_PARA *ptParam, unsigned char *pucRfPara);


struct ST_PCDOPS * GetPn512OpsHandle();

#ifdef __cplusplus
}
#endif
#endif

