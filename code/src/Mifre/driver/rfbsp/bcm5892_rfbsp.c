/**
 * =============================================================================
 * Copyright (c), PAX Computer Technology(Shenzhen) Co.,Ltd.
 *
 * File   : s90_rfbsp.c
 *
 * Author : Wanlishan     
 * 
 * Date   : 2012-03-13
 *
 * Description:
 *
 *
 * History:
 *
 * =============================================================================
 */
#include "bcm5892_rfbsp.h"
#include "base.h"

#define enable_spi()	gpio_set_pin_val(GPIOA,11,0) 	 
#define disable_spi()	gpio_set_pin_val(GPIOA,11,1) 

#define RF_SPI_CHANNEL (1)

extern unsigned char gucRfChipType;




struct ST_PCDBSP
{
    int (*RfBspInit)( void );
    
    int (*RfBspPowerOn)( void );
    int (*RfBspPowerOff)( void );
    
    int (*RfBspRead)( unsigned char, unsigned char*, int );
    int (*RfBspWrite)( unsigned char, unsigned char*, int );
    
    int (*RfBspIntInit)( void(*)(void) );
    int (*RfBspIntEnable)( void );
    int (*RfBspIntDisable)( void );
    
};



const struct ST_PCDBSP gtBoard = 
{
    RfBspInit,
    
    RfBspPowerOn,
    RfBspPowerOff,
    
    RfBspSpiReadRegister,
    RfBspSpiWriteRegister,
    
    RfBspIntInit, 
    RfBspIntEnable,
    RfBspIntDisable
};

/**
 * the congurations of the communication interface between MCU and RC663
 * 
 * params:
 *         no params
 * retval:
 *         no return value
 */
void RfBspSpiInit()
{
    /*初始化SPI，配置为9M速率*/
	gpio_set_pin_type(GPIOA,11,GPIO_OUTPUT); /*配置SPI_NCS口线*/
	disable_spi();
    gpio_set_pin_type(GPIOA,8,GPIO_FUNC0);   /*配置SPI_CLK口线*/
    gpio_set_pin_type(GPIOA,9,GPIO_FUNC0);   /*配置SPI_MISO口线*/	
    gpio_set_pin_type(GPIOA,10,GPIO_FUNC0);  /*配置SPI_MOSI口线*/
	
	if(gucRfChipType == CHIP_TYPE_AS3911)
	{
	    /*Phase =1,clkpol = 0,配置SPI速率为5M*/
	    spi_config(1, 5000000, 8, 1);
	}
	else if((gucRfChipType == CHIP_TYPE_RC663) || (gucRfChipType == CHIP_TYPE_PN512))
	{   
	    /*Phase =0,clkpol = 0,配置SPI速率为9M*/
        spi_config(1, 9000000, 8, 0);    
	}

	/*初始化RSTPD*/
	gpio_set_pin_type(GPIOA,1,GPIO_OUTPUT);
	
    return;    
}

/**
 * Rf module power on
 * 
 * params:
 *         no param
 * retval:
 *         no return value
 */
int RfBspPowerOn( )
{
    volatile int  i = 0;

    /*add by tangyf for S300*/
    if(get_machine_type()==S300)
    {
        if(GetVolt()>=5500)
        {    
        	gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);
        	gpio_set_pin_val(GPIOB,31, 0);//power off
            return 0XFF; 
        }

        gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);
        gpio_set_pin_val(GPIOB,31, 1);//power open
    }
    /*add end*/

    if(gucRfChipType == CHIP_TYPE_RC663)
    {
        gpio_set_pin_val(GPIOA,1,0);
    }
	else if(gucRfChipType == CHIP_TYPE_PN512)
	{
	    gpio_set_pin_val(GPIOA,1,1);
	}
	
	/*延时3ms,等待芯片正常启动*/
    DelayMs(3);
    
    return 0;
}

/**
 * rf module power down
 * 
 * params:
 *         no param
 * retval:
 *         no return value
 */
int RfBspPowerOff( )
{
    volatile int  i = 0;

	if(gucRfChipType == CHIP_TYPE_RC663)
	{
        gpio_set_pin_val(GPIOA,1,1);
	}
	else if(gucRfChipType == CHIP_TYPE_PN512)
	{
	    gpio_set_pin_val(GPIOA,1,0);
	}
	/*
    if(get_machine_type()==S300)
    {
        gpio_set_pin_type(GPIOB, 31, GPIO_OUTPUT);
    	gpio_set_pin_val(GPIOB,31, 0);//power off
    }*/
	/*延时3ms,等待芯片正常关闭*/
    DelayMs(3);
    
    return 0;
}

/**
 * SPI selection
 * 
 * params:
 *          Stat : the signal status
 * retval:
 *          no return value
 */
void RfBspSpiNss( int Stat )
{
    volatile int i = 0;
    
    if( Stat )
    {
        disable_spi(); 
    }
    else
    {
        enable_spi();
    }  
}

/**
 * write data the internal register into RC663
 *
 * params:
 *          ucRegNo   : the number of registers
 *          pucRegVal : the value buffer of registers
 *
 * retval:
 *         the status of operation
 */
int RfBspSpiWriteRegister( unsigned char ucRegNo, unsigned char *pucRegVal, int Nums )
{
    if(Nums < 0)
    {
        return -1;
    }
	
	if(gucRfChipType == CHIP_TYPE_AS3911)
	{
		/*if the numbers of bytes to write is 0 mean Send command, ucRegNo is command value,
		The command vlaue is:11XXXXXX.*/
		if((Nums == 0) && ((ucRegNo & 0xC0) != 0xC0))return;
		
		RfBspSpiNss(0);
	
		spi_txrx(RF_SPI_CHANNEL,(unsigned short)ucRegNo, 8);
		while( Nums-- )
		{
			spi_txrx(RF_SPI_CHANNEL,(unsigned short)(*pucRegVal++), 8);
		}

		RfBspSpiNss(1);
    
		return 0;    
	}
	else if((gucRfChipType == CHIP_TYPE_RC663) || (gucRfChipType == CHIP_TYPE_PN512))
	{
        RfBspSpiNss(0);

    spi_txrx(RF_SPI_CHANNEL,(unsigned short)(ucRegNo << 1), 8);

	
	while(Nums--)
	{
	    spi_txrx(RF_SPI_CHANNEL, (unsigned short)(*pucRegVal++), 8);
	}
	RfBspSpiNss(1);
    
        return 0; 
	}
}

/**
 * read the assigned register context
 * 
 * params:
 *          ucRegNo  : the number of registers
 *          pucRegVal: the value buffer of registers
 * retval:
 *          the status of operation
 */
int RfBspSpiReadRegister( unsigned char ucRegNo, unsigned char *pucRegVal, int Nums )
{
    if(Nums <= 0)
    {
        return -1;
    }

	if(gucRfChipType == CHIP_TYPE_AS3911)
	{
		/* If ucRegNo is not the address of FIFO Read, ucRegNo must or 0x40 */
		if(ucRegNo != 0xBF)
		{
			ucRegNo |= 0x40; 
		}
		RfBspSpiNss(0);

		spi_txrx(RF_SPI_CHANNEL,(unsigned short)ucRegNo, 8);
		Nums--;
		while( Nums-- )
		{
			*pucRegVal = 0x00;
			*pucRegVal++ = (char)spi_txrx(RF_SPI_CHANNEL, 0xAA, 8);
		}
		
		*pucRegVal = 0x00;
		*pucRegVal++ = (char)spi_txrx(RF_SPI_CHANNEL, 0xAA, 8);
    
		RfBspSpiNss(1);
		
		return 0;
	}
	else if(gucRfChipType == CHIP_TYPE_RC663)
	{
		RfBspSpiNss(0);
		spi_txrx(RF_SPI_CHANNEL, (unsigned short)((ucRegNo++ << 1) | 1), 8);

		Nums--;
		while(Nums--)
		{
			*pucRegVal = 0x00;
			*pucRegVal++ = (char)spi_txrx(RF_SPI_CHANNEL, (unsigned short)((ucRegNo++ << 1) | 1), 8);
		}
		*pucRegVal = 0x00;
		*pucRegVal++ = (char)spi_txrx(RF_SPI_CHANNEL, 0x00, 8);
		RfBspSpiNss(1);

		return 0; 
	}

	else if(gucRfChipType == CHIP_TYPE_PN512)
	{
		RfBspSpiNss(0);
		spi_txrx(RF_SPI_CHANNEL, (unsigned short)((ucRegNo++ << 1) | 0x80), 8);
		
		Nums--;
		while(Nums--)
		{
			*pucRegVal = 0x00;
			*pucRegVal++ = (char)spi_txrx(RF_SPI_CHANNEL, (unsigned short)((ucRegNo++ << 1) | 0x80), 8);
		}
		*pucRegVal = 0x00;
		*pucRegVal++ = (char)spi_txrx(RF_SPI_CHANNEL, 0x00, 8);
		RfBspSpiNss(1);

		return 0; 
	}
}

/**
 * Rf module interrupts initialized base on board
 * parameters: 
 *            pIsr  :  the chip interrupts service route.
 * retval :
 *            no
 */
static void (*RfBspCallBackService)( void );
int RfBspIntInit( void (*pIsr)( void ) )
{ 
    /*初始化中断*/
	RfBspCallBackService = pIsr;
	if(gucRfChipType == CHIP_TYPE_AS3911)
	{
	    gpio_enable_pull(1, 1);
        gpio_set_pull(1, 1, 0 );
        s_setShareIRQHandler(GPIOB,1,INTTYPE_HIGHLEVEL,RfBspIsr);
	}
	else if(gucRfChipType == CHIP_TYPE_RC663)
	{
	    s_setShareIRQHandler(GPIOB,1,INTTYPE_LOWLEVEL,RfBspIsr);
	}
	else if(gucRfChipType == CHIP_TYPE_PN512)
	{
		s_setShareIRQHandler(GPIOB,1,INTTYPE_LOWLEVEL,RfBspIsr);
	}

    return 0;   
}

/**
 * enable interrupts
 * parameters: 
 *            no
 * retval :
 *            no
 */
int RfBspIntEnable()
{
    /*开中断*/
	gpio_set_pin_interrupt(GPIOB,1,1);
    
    return 0;
}

/**
 * disable interrupts
 * parameters: 
 *            no
 * retval :
 *            no
 */
int RfBspIntDisable()
{
    /*关中断*/
	gpio_set_pin_interrupt(GPIOB,1,0);
	
    return 0;
}

/**
 * interrupt service function
 */
void RfBspIsr( void )
{
	/*清中断由系统完成，这里不再添加*/
    
    /*call process*/
    RfBspCallBackService();  
}

/**
 * initialisze the rf modeule base on hardware board
 * parameters :
 *             no
 * retval :
 *             0 - successfully
 *             others, failurely.
 */
int RfBspInit()
{
    RfBspSpiInit();
    RfBspPowerOn();
    RfBspIntDisable();
    
    return 0;
}


struct ST_PCDBSP *GetBoardHandle()
{
    return (struct ST_PCDBSP *)&gtBoard;
}

