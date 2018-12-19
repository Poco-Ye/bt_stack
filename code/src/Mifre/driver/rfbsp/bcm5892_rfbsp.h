#ifndef RF_BSP_H
#define RF_BSP_H

#ifdef __cplusplus
extern "C" {
#endif
#define GIO1_R_GRPF1_INT_CLR_MEMADDR    0xcd824
#define GIO1_R_GRPF1_INT_MSTAT_MEMADDR  0xcd820
#define RF_INT_MASK (0x1 << 1)


#define  CHIP_TYPE_RC531     ( 1 )
#define  CHIP_TYPE_PN512     ( 2 )
#define  CHIP_TYPE_RC663     ( 3 )
#define  CHIP_TYPE_AS3911    ( 4 )



void RfBspSpiInit( void );
int  RfBspPowerOn( void );
int  RfBspPowerOff( void );
void RfBspSpiNss( int Stat );
int  RfBspSpiWriteRegister( unsigned char ucRegNo, unsigned char *pucRegVal, int Nums );
int  RfBspSpiReadRegister( unsigned char ucRegNo, unsigned char *pucRegVal, int Nums );

int  RfBspInit( void );
int  RfBspIntInit( void(*)( void ) );
int  RfBspIntEnable( void );
int  RfBspIntDisable( void );
void RfBspIsr( void );
struct ST_PCDBSP *GetBoardHandle();


#ifdef __cplusplus
}
#endif

#endif
