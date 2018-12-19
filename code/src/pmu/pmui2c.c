/**
 * =============================================================================
 * Implement i2c master machine with GPIO. 
 * =============================================================================
 * Author  : zhaorh
 * Date    : 2013-3-25
 * Email   : zhaorh@paxsz.com
 * Version : V1.0
 * =============================================================================
 * Copyright (c) PAX Computer Technology (ShenZhen) co.,ltd
 * =============================================================================
 */
#include "base.h"
#include "pmui2c.h"
#define PMU_I2C_SDA_GPIO GPIOD,5
#define PMU_I2C_SCLK_GPIO GPIOD,4
#define PMU_I2C_SDA_INPUT 	gpio_set_pin_type(PMU_I2C_SDA_GPIO,GPIO_INPUT)
#define PMU_I2C_SDA_OUTPUT 	gpio_set_pin_type(PMU_I2C_SDA_GPIO,GPIO_OUTPUT)
#define PMU_I2C_SCLK_INPUT 	gpio_set_pin_type(PMU_I2C_SCLK_GPIO,GPIO_INPUT)
#define PMU_I2C_SCLK_OUTPUT gpio_set_pin_type(PMU_I2C_SCLK_GPIO,GPIO_OUTPUT)
#define PMU_I2C_SDA_SET 	gpio_set_pin_val(PMU_I2C_SDA_GPIO,1)
#define PMU_I2C_SDA_CLEAR 	gpio_set_pin_val(PMU_I2C_SDA_GPIO,0)
#define PMU_I2C_SCLK_SET 	gpio_set_pin_val(PMU_I2C_SCLK_GPIO,1)
#define PMU_I2C_SCLK_CLEAR 	gpio_set_pin_val(PMU_I2C_SCLK_GPIO,0)
#define PMU_I2C_SDA_READ	gpio_get_pin_val(PMU_I2C_SDA_GPIO)
#define PMU_I2C_SCLK_READ	gpio_get_pin_val(PMU_I2C_SCLK_GPIO)

/*Delay 2.5us*/
void  pmu_iic_gap()
{
	DelayUs(3);
}

/*Delay 5us*/
void pmu_iic_etu()
{
	DelayUs(5);
}


/*
 * Simulate i2c-bus protocol.
 * Protocol specify when SCL is HIGH, the SDA generate falling edge.
 */
void pmu_iic_start()
{
	/* 
	 * SDA Generate falling edge when SCL is HIGH.
	 */
	PMU_I2C_SDA_OUTPUT;
	PMU_I2C_SCLK_OUTPUT;

	PMU_I2C_SDA_SET;
	pmu_iic_gap();
	PMU_I2C_SCLK_SET;
	pmu_iic_etu();
	PMU_I2C_SDA_CLEAR;
	pmu_iic_etu();
	PMU_I2C_SCLK_CLEAR;
	pmu_iic_gap();
}
/*
 * Simulate i2c-bus protocol.
 * Protocol specify when SCL is HIGH, the SDA generate rising edge.
 */
void pmu_iic_stop()
{
	/*OutPut mode*/
	PMU_I2C_SDA_OUTPUT;

	PMU_I2C_SDA_CLEAR;
    pmu_iic_gap();
	PMU_I2C_SCLK_SET;
	pmu_iic_etu();
	PMU_I2C_SDA_SET;
}

/*
 * Simulate i2c-bus protocol.
 * Protocol specify after received the 8th. bit, if the master received right,
 * the master turn SDA to LOW and provide one SCL signal to acknowledge.
 */
void pmu_iic_ack()
{
	/*OutPut mode*/
	PMU_I2C_SDA_OUTPUT;
	
	PMU_I2C_SDA_CLEAR;
	pmu_iic_gap();
	PMU_I2C_SCLK_SET;
	pmu_iic_etu();
	PMU_I2C_SCLK_CLEAR;
	pmu_iic_gap();
}

/*
 * Simulate i2c-bus protocol.
 * Protocol specify after received the 8th. bit, if the master task is over,
 * the master turn SDA to HIGH and provide one SCL signal to ready to generate 
 * stop condition.
 */
void pmu_iic_nack()
{
	/*OutPut mode*/
	PMU_I2C_SDA_OUTPUT;
	
	PMU_I2C_SDA_SET;
	pmu_iic_gap();
	PMU_I2C_SCLK_SET;
	pmu_iic_etu();
	PMU_I2C_SCLK_CLEAR;
	pmu_iic_gap();
}

/*
 * Simulate i2c-bus protocol.
 * Protocol specify after transferred the 8th. bit, the master turn SDA to receiving
 * status and provide one SCL signal to detect slave device acknowledge.
 */
int  pmu_iic_cack()
{
	int rval = 1;
	
	/*InPut-Mode*/
	gpio_set_pull(PMU_I2C_SDA_GPIO,1);
	gpio_enable_pull(PMU_I2C_SDA_GPIO);
	PMU_I2C_SDA_INPUT;
	pmu_iic_gap();
	PMU_I2C_SCLK_SET;
	pmu_iic_gap();
	if((PMU_I2C_SDA_READ)==0){
		rval = 0;
	}
	pmu_iic_gap();
	PMU_I2C_SCLK_CLEAR;
	pmu_iic_gap();

	return rval;	
}

/*
 * Simulate i2c-bus protocol.
 * Transfer one byte to slave device, the first bit is most significant bit.
 */
int  pmu_iic_wbyte(unsigned char byte)
{
	int  n = 7;

	/*OutPut mode*/
	PMU_I2C_SDA_OUTPUT;
	
	/*
	 * Transfer 8 bits of data. 
	 */
	pmu_iic_etu();
	pmu_iic_gap();
	for(n = 7; n >= 1; n--){
		if(byte & (1<<n)){
			PMU_I2C_SDA_SET;
		}
		else{
			PMU_I2C_SDA_CLEAR;
		}
		pmu_iic_gap();
		PMU_I2C_SCLK_SET;
		pmu_iic_etu();
		PMU_I2C_SCLK_CLEAR;
		pmu_iic_gap();
	}
	if(byte & (1<<0)){
		gpio_set_pull(PMU_I2C_SDA_GPIO,1);
		gpio_enable_pull(PMU_I2C_SDA_GPIO);
		PMU_I2C_SDA_INPUT;
	}
	else{
		PMU_I2C_SDA_CLEAR;
	}
	pmu_iic_gap();
	PMU_I2C_SCLK_SET;
	pmu_iic_etu();
	PMU_I2C_SCLK_CLEAR;
	gpio_set_pull(PMU_I2C_SDA_GPIO,1);
	gpio_enable_pull(PMU_I2C_SDA_GPIO);
	PMU_I2C_SDA_INPUT;
	pmu_iic_gap();
	return 1;
}
/*
 * Simulate i2c-bus protocol.
 * Receive one byte from slave device, the first bit is most significant bit.
 */
int  pmu_iic_rbyte(unsigned char* byte)
{
	int n = 7;
	
	*byte = 0;
	/*InPut mode*/
	gpio_set_pull(PMU_I2C_SDA_GPIO,1);
	gpio_enable_pull(PMU_I2C_SDA_GPIO);
	PMU_I2C_SDA_INPUT;
	pmu_iic_etu();
	pmu_iic_gap();
	for(n = 7; n >= 0; n--){
		pmu_iic_gap();
		PMU_I2C_SCLK_SET;
		pmu_iic_gap();
		if(PMU_I2C_SDA_READ){
			*byte |= (1<<n);
		}
		pmu_iic_gap();
		PMU_I2C_SCLK_CLEAR;
		pmu_iic_gap();
	}

	return 1;
}

/*
 * Definitions of Interface functions.
 */
int   pmu_iic_config()
{
	/*
	 * Configurate SDA and SCL ports to OutPut-Mode
	 * prohibit other function.
	 */
	PMU_I2C_SDA_OUTPUT;
	PMU_I2C_SCLK_OUTPUT;
}

/*
 * Transfer data to slave device with i2c bus.
 * parameters:
 *		slave : slave device address.
 *      data  : datas will be written.
 *      len   : the number of datas will be written.
 * return:
 *      if value is 0, means error happened.
 *      if value is equal to len, means operation is right.
 */
int    pmu_iic_write(unsigned char* data, int len)
{
	int  i = 0;

	for(i = 0; i<len ; i++){
		pmu_iic_wbyte(data[i]);
		if( pmu_iic_cack() > 0 ){
			pmu_iic_stop();
			return 0;
		}
	}
	
	return len;
}

/*
 * Read from slave device data in i2c bus.
 * parameters:
 *		slave : slave device address.
 *      data  : datas will be readed.
 *      len   : the number of datas will be readed.
 * return:
 *      if value is 0, means error happened.
 *      if value is equal to len, means operation is right.
 */
int    pmu_iic_read(unsigned char* data, int len)
{
	int  i = 0;

	for(i = 0; i<len ; i++){
		pmu_iic_rbyte((data + i));
		if(i == (len-1)){
			pmu_iic_nack();
		}
		else{
			pmu_iic_ack();
		}
	}
	
	return len;
}

/**
 * i2c conifiguartion
 */
int pmu_i2c_config()
{
    pmu_iic_config();    
}

/**
 * i2c read operation
 */
int pmu_i2c_read_str( unsigned char slave, unsigned char *pbuf, int len )
{
    int  i = 0;

	pmu_iic_start( );
	pmu_iic_wbyte( slave );
	if( pmu_iic_cack() > 0 ){
		pmu_iic_stop( );
		return 0;
	}
	for( i = 0; i < len; i++ ){
		pmu_iic_rbyte( ( pbuf + i ) );
		if( i == ( len - 1 ) ){
			pmu_iic_nack( );
		}
		else{
			pmu_iic_ack( );
		}	
	}
	pmu_iic_stop( );

	return len;
}
/**
 * i2c read operation
 */
int pmu_i2c_read_str_fromaddr( unsigned char slave, unsigned char addr, unsigned char *pbuf, int len )
{
    int  i = 0;

	pmu_iic_start( );
	pmu_iic_wbyte( slave );
	if( pmu_iic_cack() > 0 ){
		pmu_iic_stop( );
		return 0;
	}
	pmu_iic_wbyte( addr );
	if( pmu_iic_cack() > 0 ){
		pmu_iic_stop( );
		return 0;
	}
	pmu_iic_start( );
	pmu_iic_wbyte( slave | 0x01);
	if( pmu_iic_cack() > 0 ){
		pmu_iic_stop( );
		return 0;
	}
	for( i = 0; i < len; i++ ){
		pmu_iic_rbyte( ( pbuf + i ) );
		if( i == ( len - 1 ) ){
			pmu_iic_nack( );
		}
		else{
			pmu_iic_ack( );
		}	
	}
	pmu_iic_stop( );

	return len;
}

/**
 * i2c write operation
 */
int pmu_i2c_write_str( unsigned char slave, unsigned char *pbuf, int len )
{
    int  i = 0;

	pmu_iic_start( );
	pmu_iic_wbyte( slave );
	if( pmu_iic_cack( ) > 0 ){
		pmu_iic_stop( );
		return 0;
	}
	for( i = 0; i < len; i++ ){
		pmu_iic_wbyte( pbuf[i] );
		if( pmu_iic_cack( ) > 0 ){
			pmu_iic_stop( );
			return 0;
		}
	}
	pmu_iic_stop( );

	return len;
}
/**
 * i2c write operation
 */
int pmu_i2c_write_str_toaddr( unsigned char slave, unsigned char addr, unsigned char *pbuf, int len )
{
    int  i = 0;

	pmu_iic_start( );
	pmu_iic_wbyte( slave );
	if( pmu_iic_cack( ) > 0 ){
		pmu_iic_stop( );
		return 0;
	}
	pmu_iic_wbyte( addr );
	if( pmu_iic_cack( ) > 0 ){
		pmu_iic_stop( );
		return 0;
	}
	for( i = 0; i < len; i++ ){
		pmu_iic_wbyte( pbuf[i] );
		if( pmu_iic_cack( ) > 0 ){
			pmu_iic_stop( );
			return 0;
		}
	}
	pmu_iic_stop( );

	return len;
}

