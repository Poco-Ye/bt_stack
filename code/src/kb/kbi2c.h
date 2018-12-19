#ifndef _KB_I2C_H_
#define _KB_I2C_H_
#ifdef __cplusplus
extern "C" {
#endif

static void kb_iic_gap( void );
static void kb_iic_etu( void );
static void kb_iic_start( void );
static void kb_iic_stop( void );
static void kb_iic_ack( void );
static void kb_iic_nack( void );
static int  kb_iic_cack( void );
static int  kb_iic_wbyte( unsigned char byte );
static int  kb_iic_rbyte( unsigned char* byte );
static int  kb_iic_config( void );
static int  kb_iic_write( unsigned char* data, int len );
static int  kb_iic_read( unsigned char* data, int len );

int kb_i2c_config( void );
int kb_i2c_read_str( unsigned char slave, unsigned char *pbuf, int len );
int kb_i2c_write_str( unsigned char slave, unsigned char *pbuf, int len );
#ifdef __cplusplus
}
#endif
#endif
