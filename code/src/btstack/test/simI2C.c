
#include "base.h"
#include "simI2C.h"

#define MAX_PORT_INDEX	4
#define MIN_PORT_INDEX	0
#define	MAX_PIN_INDEX	31
#define	MIN_PIN_INDEX	0
#define	I2C_INTVAL		15 // ~=30KHz

#define	DIR_MODE_IN 	0
#define	DIR_MODE_OUT	1

#define	CLK_DELAY_RETRY		10000

// define +MFI_DEBUG_
#ifdef _MFI_DEBUG_
#define log(fmt, ...) do{iDPrintf("<%s><%d>: ", __FUNCTION__, __LINE__);\
	iDPrintf(fmt, ##__VA_ARGS__);}while(0)
#else
#define log
#endif


/*
set the io dir
*/
static int i2c_SetIODir(int iPin, int iMode)
{
	int port, subno;

	port = (iPin >> 8) & 0xff;
	subno = (iPin & 0xff);
	if(port < MIN_PORT_INDEX || port > MAX_PORT_INDEX)
		return I2C_ERR_INVALID_PARAM;
	if(subno < MIN_PIN_INDEX|| subno > MAX_PIN_INDEX)
		return I2C_ERR_INVALID_PARAM;
	
	gpio_set_pin_type(port, subno, iMode);
	return 	I2C_OK;	
}

/*
set the io level
*/
static int i2c_SetIO(int iPin, int iLevel)
{
	int port, subno;

	port = (iPin >> 8) & 0xff;
	subno = (iPin & 0xff);
	if(port < MIN_PORT_INDEX || port > MAX_PORT_INDEX)
		return I2C_ERR_INVALID_PARAM;
	if(subno < MIN_PIN_INDEX|| subno > MAX_PIN_INDEX)
		return I2C_ERR_INVALID_PARAM;

	gpio_set_pin_val(port, subno, iLevel);
	return I2C_OK;
}

/*
get the io level
*/
static int i2c_GetIO(int iPin)
{
	int port, subno;

	port = (iPin >> 8) & 0xff;
	subno = (iPin & 0xff);
	if(port < MIN_PORT_INDEX || port > MAX_PORT_INDEX)
		return I2C_ERR_INVALID_PARAM;
	if(subno < MIN_PIN_INDEX|| subno > MAX_PIN_INDEX)
		return I2C_ERR_INVALID_PARAM;

	return gpio_get_pin_val(port, subno);
}

static void i2c_Intval(void)
{
	DelayUs(I2C_INTVAL);
}

static void i2c_start (T_SimI2CHdl *pHdl)
{
	int m;
	int clk;
	
	pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	pHdl->pfSetDIR(pHdl->iSDA, DIR_MODE_OUT);
	pHdl->pfSetIO(pHdl->iSDA, 1);
	do {
		pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_IN);	
		pHdl->pfSetIO(pHdl->iSCL, 1);
		if(pHdl->pfGetIO(pHdl->iSCL) == 0)
			continue ;
		else
			break;
	}while(1);

	pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	pHdl->pfSetIO(pHdl->iSCL, 1);	
	// TODO:
	pHdl->pfSetIO(pHdl->iSDA, 1);
	i2c_Intval();
	pHdl->pfSetIO(pHdl->iSDA, 0);	
	i2c_Intval();
	pHdl->pfSetIO(pHdl->iSCL, 0);	
	i2c_Intval();
}

static void i2c_stop (T_SimI2CHdl *pHdl)
{
	int m;
	int clk;
	
	pHdl->pfSetDIR(pHdl->iSDA, DIR_MODE_OUT);	
	//pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	pHdl->pfSetIO(pHdl->iSDA, 0);
	do {
		pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_IN);	
		pHdl->pfSetIO(pHdl->iSCL, 1);
		if(pHdl->pfGetIO(pHdl->iSCL) == 0)
			continue ;
		else
			break;
	}while(1);

	pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	pHdl->pfSetIO(pHdl->iSCL, 1);
	i2c_Intval();
	pHdl->pfSetIO(pHdl->iSDA, 1);	
	i2c_Intval();
	//log("out");
}


static void i2c_ack (T_SimI2CHdl *pHdl)
{
	int m;
	int clk;
	
	pHdl->pfSetDIR(pHdl->iSDA, DIR_MODE_OUT);	
	//pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	i2c_Intval();	
//	i2c_Intval();	
	pHdl->pfSetIO(pHdl->iSDA, 0);
//	i2c_Intval();
	do {
		pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_IN);	
		pHdl->pfSetIO(pHdl->iSCL, 1);
		if(pHdl->pfGetIO(pHdl->iSCL) == 0)
			continue ;
		else
			break;
	}while(1);

	pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	pHdl->pfSetIO(pHdl->iSCL, 1);	
	i2c_Intval();
	pHdl->pfSetIO(pHdl->iSCL, 0);	
	i2c_Intval();	
	//log("out");
}

static void i2c_nack (T_SimI2CHdl *pHdl)
{
	int m;
	int clk;
	
	pHdl->pfSetDIR(pHdl->iSDA, DIR_MODE_OUT);	
	//pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	i2c_Intval();	
//	i2c_Intval();	
	pHdl->pfSetIO(pHdl->iSDA, 1);
	do {
		pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_IN);	
		pHdl->pfSetIO(pHdl->iSCL, 1);
		if(pHdl->pfGetIO(pHdl->iSCL) == 0)
			continue ;
		else
			break;
	}while(1);

	pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	pHdl->pfSetIO(pHdl->iSCL, 1);
	i2c_Intval();
	pHdl->pfSetIO(pHdl->iSCL, 0);	
	i2c_Intval();	
	//log("out");
	
}

/*
the receiver acknowlage the ack?
1 yes, 0 no
*/
static int i2c_isack (T_SimI2CHdl *pHdl)
{
	int val = 1;
	int m;
	int clk;
	
//	pHdl->pfSetIO(pHdl->iSDA, 0);	//Joshua _a
	pHdl->pfSetDIR(pHdl->iSDA, DIR_MODE_IN);	
//	i2c_Intval();	
	//pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	do {
		pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_IN);	
		pHdl->pfSetIO(pHdl->iSCL, 1);
		if(pHdl->pfGetIO(pHdl->iSCL) == 0)
			continue ;
		else
			break;
	}while(1);

	pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	pHdl->pfSetIO(pHdl->iSCL, 1);	
	i2c_Intval();
	val = pHdl->pfGetIO(pHdl->iSDA);
	pHdl->pfSetIO(pHdl->iSCL, 0);	
	i2c_Intval();	

	return val==0? 1: 0;	
}

static int i2c_ReadByte(T_SimI2CHdl *pHdl, char *ch)
{
	int i;
	int temp = 0;
	int m;
	int clk;


	//pHdl->pfSetIO(pHdl->iSDA, 1);		//Joshua _a
	pHdl->pfSetDIR(pHdl->iSDA, DIR_MODE_IN);	
	//pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
	//pHdl->pfSetIO(pHdl->iSDA, 1);
	i2c_Intval();

	for (i=0; i<8; i++)
	{
		temp <<= 1;
		do {
			pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_IN);	
			pHdl->pfSetIO(pHdl->iSCL, 1);
			if(pHdl->pfGetIO(pHdl->iSCL) == 0)
				continue ;
			else
				break;
		}while(1);

		pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
		pHdl->pfSetIO(pHdl->iSCL, 1);
		i2c_Intval();
		temp |= pHdl->pfGetIO(pHdl->iSDA);
		pHdl->pfSetIO(pHdl->iSCL, 0);
		i2c_Intval();
	}
	*ch = (char)(temp&0xff);	
	//log("ReadByte = %02X", temp);
	return 1;
}

static int i2c_WriteByte(T_SimI2CHdl *pHdl, char ch)
{
	int i;
	char temp = ch;
	int m;
	int clk;
	
	pHdl->pfSetDIR(pHdl->iSDA, DIR_MODE_OUT);	
	//pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);

	for (i=0; i<8; i++)
	{
		if ((temp << i) & 0x80)
		{
			pHdl->pfSetIO(pHdl->iSDA, 1);
		}
		else
		{
			pHdl->pfSetIO(pHdl->iSDA, 0);
		}
		do {
			pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_IN);	
			pHdl->pfSetIO(pHdl->iSCL, 1);
			if(pHdl->pfGetIO(pHdl->iSCL) == 0)
				continue ;
			else
				break;
		}while(1);

		pHdl->pfSetDIR(pHdl->iSCL, DIR_MODE_OUT);
		pHdl->pfSetIO(pHdl->iSCL, 1);
		i2c_Intval();
		pHdl->pfSetIO(pHdl->iSCL, 0);
		if (i >= 7)
			pHdl->pfSetDIR(pHdl->iSDA, DIR_MODE_IN);
		i2c_Intval();
	}
}

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
Joshua Guo. @ 2012-06-27
**********************************************************/
int SimI2CInit(T_SimI2CHdl *pHdl, int iSDA, int iSCL) 
{
	int sda_port, sda_subno;
	int scl_port, scl_subno;
	
	sda_port = (iSDA >> 8) & 0xff;
	sda_subno = (iSDA & 0xff);
	scl_port = (iSCL >> 8) & 0xff;
	scl_subno = (iSCL & 0xff);

	if(pHdl == NULL
		|| sda_port < MIN_PORT_INDEX || sda_port > MAX_PORT_INDEX
		|| sda_subno < MIN_PIN_INDEX || sda_subno > MAX_PIN_INDEX
		|| scl_port < MIN_PORT_INDEX || scl_port > MAX_PORT_INDEX
		|| scl_subno < MIN_PIN_INDEX || scl_subno > MAX_PIN_INDEX)
		return I2C_ERR_INVALID_PARAM;
	
	pHdl->iSDA = iSDA;
	pHdl->iSCL = iSCL;
	pHdl->iRetry = 0;
	pHdl->pfGetIO = i2c_GetIO;
	pHdl->pfSetIO = i2c_SetIO;
	pHdl->pfSetDIR = i2c_SetIODir;

	return 0;
}

int SimI2CSetRetry(T_SimI2CHdl *pHdl, int iRetry)
{
	if (pHdl == NULL || iRetry < 0)
	{
		return I2C_ERR_INVALID_PARAM;
	}
    pHdl->iRetry = iRetry;
    return 0;
}


/*
read data from the simulator I2C bus

return:
	< 0  read failed, return the error No.
	>=0 data length
	
Joshua Guo. @ 2012-06-27
*/
//int SimI2CReadDataFromAddr(T_SimI2CHdl *pHdl, char slave, char addr, char *buf, int iLen)
int SimI2CReadDataFromAddr(T_SimI2CHdl *pHdl, 
	unsigned char slave, unsigned char addr, unsigned char *buf, unsigned char iLen)
{
	int i;
	int iRetry1 = 0;
	int iRetry2 = 0;
//	if (pHdl == NULL || buf == NULL || iLen < 0)
	if (pHdl == NULL || buf == NULL)
	{
		log("I2C_ERR_INVALID_PARAM");
		return I2C_ERR_INVALID_PARAM;
	}
	if (iLen == 0)
	{
		log("iLen = 0");
		return 0;
	}
	/*
	 * Read Data From the Device
	 * +---+-------+---+------------+---+---+---+-------+---+
	 * | S | SLA+W | A | MemAddress | A | P | S | SLA+R | A | ...
	 * +---+-------+---+------------+---+---+---+-------+---+
	 *   +-------+----+-------+----+-----+-------+-----+---+
	 *   | Data1 | mA | Data2 | mA | ... | DATAn | /mA | P |
	 *   +-------+----+-------+----+-----+-------+-----+---+
	 * S - Start Condition
	 * P - Stop Condition
	 * SLA+W - Slave Address plus Wirte Bit
	 * SLA+R - Slave Address plus Read Bit
	 * MemAddress - Targe memory address within device
	 * mA - Host Acknowledge Bit
	 * A - Slave Acknowledge Bit
	 */
/*
	i2c_nack(pHdl);
	i2c_ack(pHdl);
	i2c_stop(pHdl);
*/
retry1:	
    //start
	i2c_start(pHdl);
	//slave with the R/W as 0
	i2c_WriteByte(pHdl, slave);
	if (i2c_isack(pHdl) == 0)		//not acknowlage the ACK
	{
		i2c_stop(pHdl);
		if(iRetry1++ >= pHdl->iRetry)
		{
        	log("I2C_ERR_READ_FAILED 1");
        	return I2C_ERR_READ_FAILED;
		}
		goto retry1;
	}

	//addr byte
	i2c_WriteByte(pHdl, addr);
	if (i2c_isack(pHdl) == 0)		//not acknowlage the ACK
	{
		i2c_stop(pHdl);
		log("I2C_ERR_READ_FAILED 2");
		return I2C_ERR_READ_FAILED;
	}
retry2:
	//start
	i2c_start(pHdl);
	//slave byte with R/W as 1
	i2c_WriteByte(pHdl, (slave|0x01));
	if (i2c_isack(pHdl) == 0)		//not acknowlage the ACK
	{
		i2c_stop(pHdl);
		if(iRetry2++ >= pHdl->iRetry)
		{
        	log("I2C_ERR_READ_FAILED 3");
        	return I2C_ERR_READ_FAILED;
		}
		goto retry2;
	}

	//real read
	for(i=0; i<iLen; i++)
	{
		i2c_ReadByte(pHdl, buf+i);
		if (i == (iLen-1))	//the last byte will acknowlage the NACK
		{
			//log("if");
			i2c_nack(pHdl);
		}
		else
		{
			//log("else");
			i2c_ack(pHdl);
		}
	}

	//log("stop");
	//stop
	i2c_stop(pHdl);
	//logHex(buf, iLen, "Read<%d> = ", iLen);
	return iLen;
}


/*
Write data to the simulator I2C bus

return:
	< 0 write failed, return the error No.
	>=0 data length
	
Joshua Guo. @ 2012-06-27
*/
//int SimI2CWriteDataToAddr(T_SimI2CHdl *pHdl, char slave, char addr, char *buf, int iLen)
int SimI2CWriteDataToAddr(T_SimI2CHdl *pHdl, 
	unsigned char slave, unsigned char addr, unsigned char *buf, unsigned char iLen)
{
	int i;
	int iRetry = 0;
	//if (pHdl == NULL || buf == NULL || iLen < 0)
	if (pHdl == NULL || buf == NULL)
	{
		log("I2C_ERR_INVALID_PARAM");
		return I2C_ERR_INVALID_PARAM;
	}
	if (iLen == 0)
	{
		log("iLen = 0");
		return 0;
	} 
	
	/*
	 *  Write Data to the Device
     *  +---+-------+---+------------+---+------+---+---+
     *  | S | SLA+W | A | MemAddress | A | Data | A | P |
     *  +---+-------+---+------------+---+------+---+---+
     *  S - Start Condition
     *  SLA+W - Slave Address plus write bit
     *  MemAddress - Targe memory address within device
     *  Data - Data to be written
     *  A - Slave Acknowledge Bit
     *  P - Stop Condition
	 */
retry:
	//start
	i2c_start(pHdl);
	//slave
	i2c_WriteByte(pHdl, slave);
	if (i2c_isack(pHdl) == 0)
	{
	    i2c_stop(pHdl);
	    if(iRetry++ >= pHdl->iRetry)
	    {
    		log("I2C_ERR_WRITE_FAILED 1");
    		return I2C_ERR_WRITE_FAILED;
		}
		goto retry;
	}

	//addr
	i2c_WriteByte(pHdl, addr);
	if (i2c_isack(pHdl) == 0)		//not acknowlage the ACK
	{
		i2c_stop(pHdl);
		log("I2C_ERR_WRITE_FAILED 2");
		return I2C_ERR_WRITE_FAILED;
	}

	for (i=0; i<iLen; i++)
	{
		i2c_WriteByte(pHdl, buf[i]);
		if (i2c_isack(pHdl) == 0)		//not acknowlage the ACK
		{
			i2c_stop(pHdl);
			log("I2C_ERR_WRITE_FAILED 3");
			return I2C_ERR_WRITE_FAILED;
		}
	}
	i2c_stop(pHdl);
	//log("return iLen = %d", iLen);
	return iLen;
}

// TODO: test

