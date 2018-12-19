#ifndef __SIM_I2C_H__
#define __SIM_I2C_H__


typedef int (*FunIODir)(int, int);
typedef int (*FunSetIO)(int, int);
typedef int (*FunGetIO)(int);
typedef struct{
	int iSDA;					//the SDA pin index; (GPIO_PORT_NUM << 8) | GPIO_SUN_NUM
	int iSCL;					//the SCL pin index; (GPIO_PORT_NUM << 8) | GPIO_SUN_NUM
	int iRetry;
	FunIODir pfSetDIR;		//this function will set the Direction of the IO
	FunSetIO pfSetIO;		//this function set the IO	level
	FunGetIO pfGetIO;		//this function get teh IO value
}T_SimI2CHdl;


#define	I2C_OK						0
#define	I2C_ERR_BASE				0
#define	I2C_ERR_INVALID_PARAM		I2C_ERR_BASE-1
#define	I2C_ERR_IO_OPERATE			I2C_ERR_BASE-2
#define	I2C_ERR_READ_FAILED			I2C_ERR_BASE-3
#define	I2C_ERR_WRITE_FAILED		I2C_ERR_BASE-4

/*********************************************************
Name :
	imI2CInit
Descritpion :
	initialize the I2C handle
input:

output:
	pHdl - the handle pointer;
return:
	= 0 - success
	< 0 - failed
**********************************************************/
int SimI2CInit(T_SimI2CHdl *pHdl, int iSDA, int iSCL); 


int SimI2CSetRetry(T_SimI2CHdl *pHdl, int iRetry);


/*
read data from the simulator I2C bus

return:
	< 0  read failed, return the error No.
	>=0 data length
*/
//int SimI2CReadDataFromAddr(T_SimI2CHdl *pHdl, char slave, char addr, char *buf, int iLen);
int SimI2CReadDataFromAddr(T_SimI2CHdl *pHdl, 
	unsigned char slave, unsigned char addr, unsigned char *buf, unsigned char iLen);

/*
Write data to the simulator I2C bus

return:
	< 0 write failed, return the error No.
	>=0 data length
*/
//int SimI2CWriteDataToAddr(T_SimI2CHdl *pHdl, char slave, char addr, char *buf, int iLen);
int SimI2CWriteDataToAddr(T_SimI2CHdl *pHdl, 
	unsigned char slave, unsigned char addr, unsigned char *buf, unsigned char iLen);

#endif

