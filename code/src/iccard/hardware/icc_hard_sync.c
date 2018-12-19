/**
 * =============================================================================
 * This document is mainly aimed at implementing the functions for synchronised card
 * 
 * creator  : xuwt
 * data  : 2011-7-11
 * e-mail  : xuwt@paxsz.com
 * 
 * All content in this document should be transplanted 
 * =============================================================================
 */

#include "icc_hard_bcm5892.h"
#include "icc_config.h"
#include "ncn8025.h"
#include "tda8026.h"


/* syncronous card interface by tda8026 */
int (*Mc_Vcc_by_controller)(unsigned char devch, unsigned int mode);
void (*Mc_Reset_by_controller)(unsigned char devch, unsigned char level);
void (*Mc_C4_Write_by_controller)(unsigned char devch, unsigned char level);
void (*Mc_C8_Write_by_controller)(unsigned char devch, unsigned char level);


/**
 * ===================================================================
 * switch the operating mode between synchronised card and 
 * asynchronised card 
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - disable / 1 - enable
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *         none
 * ===================================================================
 */
void Mc_Clk_Enable( unsigned char slot, unsigned char enable )
{
	if(slot != USER_SLOT) 
		return;

	sci_gpio_set_pin_type(GPIOA, SCI0_CLK, enable ? GPIO_OUTPUT : GPIO_FUNC0);	
	sci_gpio_set_pin_type(GPIOA, SCI0_IO, enable ? GPIO_OUTPUT : GPIO_FUNC0);
}


/**
 * ===================================================================
 * set the direction of IO pin, namely OUTPUT or INPUT
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - output / 1 - input
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *       none
 * ===================================================================
 */
void Mc_Io_Dir( unsigned char slot, unsigned char mode )
{ 
	if(slot != USER_SLOT) 
		return;

	sci_gpio_set_pin_type(GPIOA, SCI0_IO, mode ? GPIO_INPUT : GPIO_OUTPUT);	
}

 
/**
 * ===================================================================
 * get the logic level on IO pin
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *         logic level: 
 *             0   - LOW
 *             not 0 - HIGH
 * ===================================================================
 */
unsigned char Mc_Io_Read( unsigned char slot )
{
	if(slot != USER_SLOT) 
		return 0;

	return sci_gpio_get_pin_val(GPIOA, SCI0_IO);		
}


/**
 * ===================================================================
 * change the logic level on IO Pin
 * -------------------------------------------------------------------
 * parameter:
 * -------------------------------------------------------------------
 * [input]  slot     :  please refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_Io_Write( unsigned char slot, unsigned char level )
{
	if (slot != USER_SLOT)
		return ;

	sci_gpio_set_pin_val(GPIOA, SCI0_IO, level);
}


/**
 * supply clock signal to Card or not
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_Clk( unsigned char slot, unsigned char level )
{
	if (slot != USER_SLOT)
		return;

	sci_gpio_set_pin_val(GPIOA, SCI0_CLK, level);	
}



/**
 * ===================================================================
 * Power supply on or off to Card
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit"  
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - power off / 1 - power on 
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *     none
 * ===================================================================
 */
void Mc_Vcc( unsigned char slot, unsigned char mode )
{
	int vcc_mode;
	unsigned char voltage_flag;

	if ((slot& 0x07) != USER_SLOT)
		return;

	/* determine vcc voltage */
	if(mode != 0)
	{
		voltage_flag = ( slot & 0x18 ) >> 3;
		if(voltage_flag == 1)
		{
			vcc_mode = 1800;
		}
		else if(voltage_flag == 2)
		{
			vcc_mode = 3000;
		}
		else
		{
			vcc_mode = 5000;
		}
	}
	else
	{
		vcc_mode = 0;
	}

	Mc_Vcc_by_controller(USER_CH, vcc_mode);

	return;
}

/**
 * ===================================================================
 * change the logic level on Reset Pin
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_Reset( unsigned char slot, unsigned char level )
{
	unsigned char regval;

	if (slot != USER_SLOT)
		return ;

	Mc_Reset_by_controller(USER_CH, level);
}




/**
 * ===================================================================
 * change the logic level on C4 Pin
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_C4_Write( unsigned char slot, unsigned char level )
{
	unsigned char regval;

	if (slot != USER_SLOT)
		return;

	Mc_C4_Write_by_controller(USER_CH, level);
}

/**
 * ===================================================================
 * change the logic level on C8 Pin
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
void Mc_C8_Write( unsigned char slot, unsigned char level )
{ 
	unsigned char regval;
	
	if (slot != USER_SLOT)
		return;

	Mc_C8_Write_by_controller(USER_CH, level);
}

/**
 * ===================================================================
 * get the logic level from C4 Pin
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
unsigned char Mc_C4_Read( unsigned char slot )
{
	/* unused */
	return 0;
}


/**
 * ===================================================================
 * get the logic level from C8 Pin
 * -------------------------------------------------------------------
 * parameter£º
 * -------------------------------------------------------------------
 * [input]  slot     :  refer to function "IccInit" 
 * -------------------------------------------------------------------
 * [input]  mode     :  0 - LOW / 1 - HIGH 
 * -------------------------------------------------------------------
 * return£º
 * -------------------------------------------------------------------
 *    none
 * ===================================================================
 */
unsigned char Mc_C8_Read( unsigned char slot )
{   
 	/* unused */
	return 0;
}


int sync_interface_init(unsigned int icc_controller_type)
{
	if(icc_controller_type == TDA8026)
	{
		Mc_Vcc_by_controller = Mc_Vcc_TDA8026;
		Mc_Reset_by_controller = Mc_Reset_TDA8026;
		Mc_C4_Write_by_controller = Mc_C4_Write_TDA8026;
		Mc_C8_Write_by_controller = Mc_C8_Write_TDA8026;
	}
	else if(icc_controller_type == NCN8025)
	{
		Mc_Vcc_by_controller = Mc_Vcc_NCN8025;
		Mc_Reset_by_controller = Mc_Reset_NCN8025;
		Mc_C4_Write_by_controller = Mc_C4_Write_NCN8025;
		Mc_C8_Write_by_controller = Mc_C8_Write_NCN8025;
	}
	else
	{
		return -1;
	}

	return 0;
}

