#ifndef RC663DRIVER_H
#define RC663DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_RF_TIMER                     ( 0x02 )

#define RC663_HW_DEVICE_IDLE             ( 0x08 )
#define RC663_HW_TRANSFERRING            ( 0x07 )
#define RC663_HW_TRANSMISSION_COMPLETED  ( 0x01 )
#define RC663_HW_RECEPTION_COMPLETED     ( 0x02 )
#define RC663_HW_ERROR_OCCURED           ( 0x03 )
#define RC663_HW_LPCD_DETECTED           ( 0x04 )
#define RC663_HW_RX_WAIT_TIMEOUT         ( 0x05 )
#define RC663_HW_RC663_RXSOF             ( 0x06 )

int  Rc663Init( struct ST_PCDINFO* );
int  Rc663CarrierOn( struct ST_PCDINFO* );
int  Rc663CarrierOff( struct ST_PCDINFO* );
int  Rc663Config( struct ST_PCDINFO* );
int  Rc663Waiter( struct ST_PCDINFO*, unsigned int );
int  Rc663MifareAuthenticate( struct ST_PCDINFO* );
int  Rc663Trans( struct ST_PCDINFO* );
int  Rc663TransCeive( struct ST_PCDINFO* );
void Rc663CommIsr( void );

int  Rc663SetParamValue(PICC_PARA *ptPiccPara );
int  Rc663GetParamTagValue(PICC_PARA *ptParam, unsigned char *pucRfPara);


struct ST_PCDOPS * GetRc663OpsHandle();

#ifdef __cplusplus
}
#endif
#endif

