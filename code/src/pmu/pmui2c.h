#ifndef _PMUI2C_H_
#ifdef __cplusplus
extern "C" {
#endif
#define _PMUI2C_H_

static void pmu_iic_gap( void );
static void pmu_iic_etu( void );
static void pmu_iic_start( void );
static void pmu_iic_stop( void );
static void iic_ack( void );
static void pmu_iic_nack( void );
static int  pmu_iic_cack( void );
static int  pmu_iic_wbyte( unsigned char byte );
static int  pmu_iic_rbyte( unsigned char* byte );
static int  pmu_iic_config( void );
static int  pmu_iic_write( unsigned char* data, int len );
static int  pmu_iic_read( unsigned char* data, int len );

int pmu_i2c_config( void );
int pmu_i2c_read_str( unsigned char slave, unsigned char *pbuf, int len );
int pmu_i2c_write_str( unsigned char slave, unsigned char *pbuf, int len );
int pmu_i2c_read_str_fromaddr( unsigned char slave, unsigned char addr, unsigned char *pbuf, int len );
int pmu_i2c_write_str_toaddr( unsigned char slave, unsigned char addr, unsigned char *pbuf, int len );

#ifdef __cplusplus
}
#endif
#endif
