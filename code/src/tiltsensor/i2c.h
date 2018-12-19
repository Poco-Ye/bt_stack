#ifndef _I2C_H
#define _I2C_H

#define TILT_I2CSDA           (31)
#define TILT_I2CSCL           (30)

/*
 * Define element functions simulating I2C BUS.
 * These functions is local.
 */
static void i2c_start(void);
static void i2c_stop(void);
static void i2c_ack(void);
static void i2c_nack(void);
static int  i2c_cack(void);
static int  i2c_wbyte(unsigned char byte);
static int  i2c_rbyte(unsigned char* byte);
static void i2c_gap(void);
static void i2c_etu(void);

/*
 * External interface on i2c bus protocol.
 */
int    i2c_config(void);
int    i2c_write(unsigned char slave, unsigned char* data, int len);  
int    i2c_read(unsigned char slave, unsigned char* data, int len);


#endif
