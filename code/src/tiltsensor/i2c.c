#include "base.h"
#include "i2c.h"
/*
 *======================================================================
 * 本文档代码功能是:
 * 使用GPIO模拟I2C协议的主机端控制
 * =====================================================================
 * 版本：0.01a
 * 作者：刘波
 * 日期：2007-12-12
 * 项目：S60/S90
 * 版权：深圳市百富计算机技术有限公司
 * 电邮：liubo@paxsz.com / liubo1234@126.com
 * 说明：
 *       表明这里的代码是硬件相关的，移植时需要额外的注意等效代码.
 *======================================================================
 */
/*======================================================================
 * 更新记录表： 
 * ---------------------------------------------------------------------
 *     日期            执行人                  说明
 * ---------------------------------------------------------------------
 *    2009-12-28		杨瑞彬					移植用于tiltsensor
 * ---------------------------------------------------------------------
 *    
 * ---------------------------------------------------------------------
 * [注意事项]
 *    
 * ---------------------------------------------------------------------
 */

void  i2c_gap()
{
	volatile int j = 0;
	
	for(j = 0; j < 14; j++);
}

void i2c_etu()
{
	volatile int j = 0;
	
	for(j = 0; j < 21; j++);
}

/*
 * Simulate i2c-bus protocol.
 * Protocol specify when SCL is HIGH, the SDA generate falling edge.
 */
void i2c_start()
{
	/* 
	 * SDA Generate falling edge when SCL is HIGH.
	 */ 
	gpio_set_pin_type(GPIOB, TILT_I2CSDA, GPIO_OUTPUT);/*SDA*/
	gpio_set_pin_type(GPIOB, TILT_I2CSCL, GPIO_OUTPUT);/*SCL*/
	gpio_set_pin_val(GPIOB, TILT_I2CSDA, 1);
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 1);
	i2c_etu();
	gpio_set_pin_val(GPIOB, TILT_I2CSDA, 0);
	i2c_gap();
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 0);
	i2c_gap();
}
/*
 * Simulate i2c-bus protocol.
 * Protocol specify when SCL is HIGH, the SDA generate rising edge.
 */
void i2c_stop()
{
	/*OutPut mode*/
	gpio_set_pin_type(GPIOB, TILT_I2CSDA, GPIO_OUTPUT);/*SDA*/	
	gpio_set_pin_val(GPIOB, TILT_I2CSDA, 0);
    i2c_gap();
    gpio_set_pin_val(GPIOB, TILT_I2CSCL, 1);
	i2c_etu();
	gpio_set_pin_val(GPIOB, TILT_I2CSDA, 1);
}
/*
 * Simulate i2c-bus protocol.
 * Protocol specify after received the 8th. bit, if the master received right,
 * the master turn SDA to LOW and provide one SCL signal to acknowledge.
 */
void i2c_ack()
{
	/*OutPut mode*/
	gpio_set_pin_type(GPIOB, TILT_I2CSDA, GPIO_OUTPUT);/*SDA*/	
	gpio_set_pin_val(GPIOB, TILT_I2CSDA, 0);	
	i2c_gap();
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 1);
	i2c_etu();
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 0);
	i2c_gap();
}
/*
 * Simulate i2c-bus protocol.
 * Protocol specify after received the 8th. bit, if the master task is over,
 * the master turn SDA to HIGH and provide one SCL signal to ready to generate 
 * stop condition.
 */
void i2c_nack()
{
	/*OutPut mode*/
	gpio_set_pin_type(GPIOB, TILT_I2CSDA, GPIO_OUTPUT);/*SDA*/	
	gpio_set_pin_val(GPIOB, TILT_I2CSDA, 1);	
	i2c_gap();
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 1);
	i2c_etu();
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 0);
	i2c_gap();
}
/*
 * Simulate i2c-bus protocol.
 * Protocol specify after transferred the 8th. bit, the master turn SDA to receiving
 * status and provide one SCL signal to detect slave device acknowledge.
 */
int  i2c_cack()
{
	int rval = 1;
	
	/*InPut-Mode*/
	gpio_set_pin_type(GPIOB, TILT_I2CSDA, GPIO_INPUT);/*SDA*/
	i2c_gap();
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 1);
	i2c_gap();
	if((gpio_get_pin_val(GPIOB, TILT_I2CSDA))==0){
		rval = 0;
	}
	i2c_gap();
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 0);
	i2c_gap();

	return rval;	
}
/*
 * Simulate i2c-bus protocol.
 * Transfer one byte to slave device, the first bit is most significant bit.
 */
int  i2c_wbyte(unsigned char byte)
{
	int  n = 7;

	/*OutPut mode*/
	gpio_set_pin_type(GPIOB, TILT_I2CSDA, GPIO_OUTPUT);/*SDA*/
	/*
	 * Transfer 8 bits of data.
	 */  
	for(n = 7; n >= 0; n--){
		if(byte & (1<<n)){
			gpio_set_pin_val(GPIOB, TILT_I2CSDA, 1);	
		}
		else{
			gpio_set_pin_val(GPIOB, TILT_I2CSDA, 0);
		}
		i2c_gap();
		gpio_set_pin_val(GPIOB, TILT_I2CSCL, 1);
		i2c_etu();
		gpio_set_pin_val(GPIOB, TILT_I2CSCL, 0);
		i2c_gap();
	}

	return 1;
}
/*
 * Simulate i2c-bus protocol.
 * Receive one byte from slave device, the first bit is most significant bit.
 */
int  i2c_rbyte(unsigned char* byte)
{
	int n = 7;
	
	*byte = 0;
	/*InPut mode*/
	gpio_set_pin_type(GPIOB, TILT_I2CSDA, GPIO_INPUT);/*SDA*/
	for(n = 7; n >= 0; n--){
		gpio_set_pin_val(GPIOB, TILT_I2CSDA, 1);
		i2c_gap();
		do{
			gpio_set_pin_val(GPIOB, TILT_I2CSCL, 1);
			i2c_gap();
			/*If slave need time to process other task, SCL will be hold LOW by slave.*/
			if(gpio_get_pin_val(GPIOB, TILT_I2CSCL)==0){
				continue;
			}
			else{
				break;	
			}
		}while(1);
		if(gpio_get_pin_val(GPIOB, TILT_I2CSDA)){
			*byte |= (1<<n);
		}
		i2c_gap();
		gpio_set_pin_val(GPIOB, TILT_I2CSCL, 0);
		i2c_gap();
	}

	return 1;
}

/*
 * Definitions of Interface functions.
 */
int   i2c_config()
{
	/*
	 * Configurate SDA and SCL ports to OutPut-Mode
	 * prohibit other function.
	 */
    gpio_set_pull(GPIOB, TILT_I2CSDA, 1);
    gpio_enable_pull(GPIOB, TILT_I2CSDA);

    gpio_disable_pull(GPIOB, TILT_I2CSCL);
	gpio_set_pin_type(GPIOB, TILT_I2CSDA, GPIO_OUTPUT);/*SDA*/
	gpio_set_pin_type(GPIOB, TILT_I2CSCL, GPIO_OUTPUT);/*SCL*/

	gpio_set_pin_val(GPIOB, TILT_I2CSDA, 1);
	gpio_set_pin_val(GPIOB, TILT_I2CSCL, 1);
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
int    i2c_write(unsigned char slave, unsigned char* data, int len)
{
	int  i = 0;

	i2c_start();
	i2c_wbyte(slave);
	if(i2c_cack() > 0){
		i2c_stop();
		return 0;
	}
	for(i = 0; i<len ; i++){
		i2c_wbyte(data[i]);
		if(i2c_cack() > 0){
			i2c_stop();
			return 0;
		}
	}
	i2c_stop();

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
int    i2c_read(unsigned char slave, unsigned char* data, int len)
{
	int  i = 0;

	i2c_start();
	i2c_wbyte(slave);
	if(i2c_cack() > 0){
		i2c_stop();
		return 0;
	}
	for(i = 0; i<len ; i++){
		i2c_rbyte((data + i));
		if(i == (len-1)){
			i2c_nack();
		}
		else{
			i2c_ack();
		}	
	}
	i2c_stop();

	return len;
}

