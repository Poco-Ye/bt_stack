/**
 * =============================================================================
 * Implement i2c master machine with GPIO. 
 * =============================================================================
 * Author  : zhaorh
 * Date    : 2013-3-25
 * Email   : lzhaorh@paxsz.com
 * Version : V1.0
 * =============================================================================
 * Copyright (c) PAX Computer Technology (ShenZhen) co.,ltd
 * =============================================================================
 */
#include "base.h"
#include "kbi2c.h"
#define KB_I2C_SDA_GPIO GPIOB,15
#define KB_I2C_SCLK_GPIO GPIOB,16
#define KB_I2C_SDA_INPUT 	gpio_set_pin_type(KB_I2C_SDA_GPIO,GPIO_INPUT)
#define KB_I2C_SDA_OUTPUT 	gpio_set_pin_type(KB_I2C_SDA_GPIO,GPIO_OUTPUT)
#define KB_I2C_SCLK_INPUT 	gpio_set_pin_type(KB_I2C_SCLK_GPIO,GPIO_INPUT)
#define KB_I2C_SCLK_OUTPUT 	gpio_set_pin_type(KB_I2C_SCLK_GPIO,GPIO_OUTPUT)
#define KB_I2C_SDA_SET 		gpio_set_pin_val(KB_I2C_SDA_GPIO,1)
#define KB_I2C_SDA_CLEAR 	gpio_set_pin_val(KB_I2C_SDA_GPIO,0)
#define KB_I2C_SCLK_SET 	gpio_set_pin_val(KB_I2C_SCLK_GPIO,1)
#define KB_I2C_SCLK_CLEAR 	gpio_set_pin_val(KB_I2C_SCLK_GPIO,0)
#define KB_I2C_SDA_READ		gpio_get_pin_val(KB_I2C_SDA_GPIO)
#define KB_I2C_SCLK_READ	gpio_get_pin_val(KB_I2C_SCLK_GPIO)


/*Delay 2.5us*/
void  kb_iic_gap()
{
	DelayUs(3);
}

/*Delay 5us*/
void kb_iic_etu()
{
	DelayUs(5);
}
void kb_iic_delay()
{
	DelayUs(170);
}


/*
 * Simulate i2c-bus protocol.
 * Protocol specify when SCL is HIGH, the SDA generate falling edge.
 */
void kb_iic_start()
{
	/* 
	 * SDA Generate falling edge when SCL is HIGH.
	 */
	KB_I2C_SDA_OUTPUT;
	KB_I2C_SCLK_OUTPUT;
	KB_I2C_SDA_SET;
	kb_iic_gap();
	KB_I2C_SCLK_SET;
	kb_iic_etu();
	KB_I2C_SDA_CLEAR;
	kb_iic_etu();
	KB_I2C_SCLK_CLEAR;
	kb_iic_gap();
}
/*
 * Simulate i2c-bus protocol.
 * Protocol specify when SCL is HIGH, the SDA generate rising edge.
 */
void kb_iic_stop()
{
	/*OutPut mode*/
	KB_I2C_SDA_OUTPUT;
	KB_I2C_SDA_CLEAR;
	kb_iic_gap();
	KB_I2C_SCLK_SET;
	kb_iic_etu();
	KB_I2C_SDA_SET;
}

/*
 * Simulate i2c-bus protocol.
 * Protocol specify after received the 8th. bit, if the master received right,
 * the master turn SDA to LOW and provide one SCL signal to acknowledge.
 */
void kb_iic_ack()
{
	/*OutPut mode*/
	KB_I2C_SDA_OUTPUT;
	KB_I2C_SDA_CLEAR;
	kb_iic_gap();
	KB_I2C_SCLK_SET;
	kb_iic_etu();
	KB_I2C_SCLK_CLEAR;
	kb_iic_gap();
}

/*
 * Simulate i2c-bus protocol.
 * Protocol specify after received the 8th. bit, if the master task is over,
 * the master turn SDA to HIGH and provide one SCL signal to ready to generate 
 * stop condition.
 */
void kb_iic_nack()
{
	/*OutPut mode*/
	KB_I2C_SDA_OUTPUT;
	KB_I2C_SDA_SET;
	kb_iic_gap();
	KB_I2C_SCLK_SET;
	kb_iic_etu();
	KB_I2C_SCLK_CLEAR;
	kb_iic_gap();
}

/*
 * Simulate i2c-bus protocol.
 * Protocol specify after transferred the 8th. bit, the master turn SDA to receiving
 * status and provide one SCL signal to detect slave device acknowledge.
 */
int  kb_iic_cack()
{
	int rval = 1;
	
	/*InPut-Mode*/
	gpio_set_pull(KB_I2C_SDA_GPIO,1);
	gpio_enable_pull(KB_I2C_SDA_GPIO);
	KB_I2C_SDA_INPUT;
	KB_I2C_SCLK_SET;
	kb_iic_gap();
	if(KB_I2C_SDA_READ==0){
		rval = 0;
	}
	KB_I2C_SCLK_CLEAR;
	kb_iic_gap();

	return rval;	
}

/*
 * Simulate i2c-bus protocol.
 * Transfer one byte to slave device, the first bit is most significant bit.
 */
int  kb_iic_wbyte(unsigned char byte)
{
	int  n = 7;

	/*OutPut mode*/
	KB_I2C_SDA_OUTPUT;
	/*
	 * Transfer 8 bits of data. 
	 */
	kb_iic_etu();
	kb_iic_gap();
	for(n = 7; n >= 0; n--){
		if(byte & (1<<n)){	
			KB_I2C_SDA_SET;
		}
		else{
			KB_I2C_SDA_CLEAR;
		}
		kb_iic_gap();
		KB_I2C_SCLK_SET;
		kb_iic_etu();
		KB_I2C_SCLK_CLEAR;
		kb_iic_gap();
	}
	gpio_set_pull(KB_I2C_SDA_GPIO,1);
	gpio_enable_pull(KB_I2C_SDA_GPIO);
	KB_I2C_SDA_INPUT;
	return 1;
}
/*
 * Simulate i2c-bus protocol.
 * Receive one byte from slave device, the first bit is most significant bit.
 */
int  kb_iic_rbyte(unsigned char* byte)
{
	int n = 7;
	
	*byte = 0;
	/*InPut mode*/
	gpio_set_pull(KB_I2C_SDA_GPIO,1);
	gpio_enable_pull(KB_I2C_SDA_GPIO);
	KB_I2C_SDA_INPUT;
	kb_iic_etu();
	kb_iic_gap();
	for(n = 7; n >= 0; n--){
		kb_iic_gap();
		KB_I2C_SCLK_SET;
		kb_iic_gap();
		if(KB_I2C_SDA_READ){
			*byte |= (1<<n);
		}
		kb_iic_gap();
		KB_I2C_SCLK_CLEAR;
		kb_iic_gap();
	}

	return 1;
}

/*
 * Definitions of Interface functions.
 */
int   kb_iic_config()
{
	/*
	 * Configurate SDA and SCL ports to OutPut-Mode
	 * prohibit other function.
	 */
	KB_I2C_SDA_OUTPUT;
	KB_I2C_SCLK_OUTPUT;
	
	KB_I2C_SDA_SET;
	KB_I2C_SCLK_SET;
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
int    kb_iic_write(unsigned char* data, int len)
{
	int  i = 0;

	for(i = 0; i<len ; i++){
		kb_iic_wbyte(data[i]);
		/*if( iic_cack() > 0 ){
			iic_stop();
			return 0;
		}*/
		kb_iic_stop();
	}
	
	return len;
}
void kb_iic_waitclk(void)
{
	int i=0;
	gpio_set_pull(KB_I2C_SCLK_GPIO,1);
	gpio_enable_pull(KB_I2C_SCLK_GPIO);
	KB_I2C_SCLK_INPUT;
	for (i =0;i < 200;i++)
	{
		kb_iic_etu();
		
		if(KB_I2C_SCLK_READ != 0 ){
		break;
		}
	}
	KB_I2C_SCLK_SET;
	KB_I2C_SCLK_OUTPUT;
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
int    kb_iic_read(unsigned char* data, int len)
{
	int  i = 0;

	for(i = 0; i<len ; i++){
		kb_iic_rbyte((data + i));
		if(i == (len-1)){
			kb_iic_nack();
		}
		else{
			kb_iic_ack();
		}
	}
	
	return len;
}

/**
 * i2c conifiguartion
 */
int kb_i2c_config()
{
    kb_iic_config();    
}

/**
 * i2c read operation
 */
int giCypressUpdateBusy = 0;
int kb_i2c_read_str( unsigned char slave, unsigned char *pbuf, int len )
{
    int  i = 0;
	if (giCypressUpdateBusy) return;
	kb_iic_start( );
	kb_iic_wbyte( slave );
	kb_iic_waitclk();
	kb_iic_cack();
	for( i = 0; i < len; i++ ){
		kb_iic_rbyte( ( pbuf + i ) );
		if( i == ( len - 1 ) ){
			kb_iic_nack( );
		}
		else{
			kb_iic_ack( );
		}	
		kb_iic_waitclk();
	}
	kb_iic_stop( );
	kb_iic_delay();
	return len;
}
/**
 * i2c write operation
 */
int kb_i2c_write_str( unsigned char slave, unsigned char *pbuf, int len )
{
    int  i = 0;
	if (giCypressUpdateBusy) return;
	kb_iic_start( );
	kb_iic_wbyte( slave );
	kb_iic_waitclk();
	kb_iic_cack();
	for( i = 0; i < len; i++ ){
		kb_iic_wbyte( pbuf[i] );
		kb_iic_waitclk();
		kb_iic_cack();
	}
	kb_iic_stop( );
	kb_iic_delay();
	return len;
}
