#include "icc_queue.h"

int adjust_timeout_type( struct emv_core *terminal );


/**
 * ===================================================================
 * To initialize the data buffer of emv_core struct indicating 
 * corresponding smart card information with specified Circular 
 * Queue Buffer  
 * 
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      0 - success
 *      Others，failure
 * ===================================================================
 */
int sci_core_init( struct emv_core *terminal, struct emv_queue *queue )
{
	terminal->queue = queue;

	return 0;
}

static struct emv_core *emv_devs;
static int	emv_devs_nums = 0;

/**
 * ===================================================================
 * 注册逻辑设备节点头和逻辑设备数量
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]terminal  : a pointer to emv_core struct indicating information
 * 		      on logic device 
 * -------------------------------------------------------------------
 * [input]nums      : the number of logic devices
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      0 - success
 *      others，failure
 * ===================================================================
 */
int sci_core_register( struct emv_core *devs, int nums )
{
	emv_devs      = devs;
	emv_devs_nums = nums;

	return 0;
}

/**
 * ===================================================================
 * 注册逻辑设备节点头和逻辑设备数量
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]terminal  : a pointer to emv_core struct indicating information
 * 		      on logic device 
 * -------------------------------------------------------------------
 * [input]nums      : the number of logic devices
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      0 - success
 *      others，failure
 * ===================================================================
 */
int sci_core_devs( struct emv_core **devs )
{
	*devs = emv_devs;

	return emv_devs_nums;
}

/**
 * ===================================================================
 * flush the Circular Queue Buffer. if done, the buffer will be empty,
 * and the relating status flag will be reset
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      0 - success
 *      others，failure
 * ===================================================================
 */
int sci_queue_flush( struct emv_core *terminal )
{
	terminal->queue->txin_index = 0;
	terminal->queue->txout_index = 0;
	terminal->queue->rxin_index = 0;
	terminal->queue->rxout_index = 0;

 	return 0;
}

/**
 * ===================================================================
 * To check if the Circular Queue Buffer is empty  
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      1 - empty
 *      0 - not empty
 * ===================================================================
 */
int sci_queue_empty( struct emv_core *terminal )
{
	return ( terminal->queue->rxin_index == terminal->queue->rxout_index ) ? 1 : 0;
}

/**
 * ===================================================================
 * Get the number of bytes of data in the Circular Queue Buffer
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]terminal  : information on logic device
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      the number of bytes of data in the Circular Queue Buffer
 * ===================================================================
 */
int sci_queue_length( struct emv_core *terminal )
{
	return ( terminal->queue->rxin_index < terminal->queue->rxout_index  ) ? 
		( MAX_QBUF_SIZE - terminal->queue->rxout_index + terminal->queue->rxin_index ) : 
		( terminal->queue->rxin_index - terminal->queue->rxout_index );
}

/**
 * ===================================================================
 * fill specified bytes of data to Circular Queue Buffer
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input/output]terminal  : Information on logic device
 * -------------------------------------------------------------------
 * [input]pbuf         : data buffer
 * -------------------------------------------------------------------
 * [input]length       ：number of bytes that will be filled 
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *       please refer to "emv_errno.h"
 * ===================================================================
 */
int sci_queue_fill( struct emv_core *terminal, unsigned char *pbuf, int length )
{
	int i = 0;

	for( i = 0; i < length; i++ )
	{
		terminal->queue->txr_buff[ terminal->queue->txin_index ] = pbuf[ i ];
		terminal->queue->txin_index++;
		if( MAX_QBUF_SIZE == terminal->queue->txin_index )
		{
			terminal->queue->txin_index = 0;
		}
	}

	return 0;
}

/**
 * ===================================================================
 * spill specified bytes of data from Circular Queue Buffer
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input]terminal  :  Information on logic device
 * -------------------------------------------------------------------
 * [output]pbuf      : data buffer
 * -------------------------------------------------------------------
 * [input]length    ：number of bytes expected
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *       please refer to "emv_errno.h"
 * ===================================================================
 */
extern void sci_emv_hard_fifo_spill(int channel, int length);
extern void ComSends(const char *str, ...);

int sci_queue_spill( struct emv_core *terminal, unsigned char *pbuf, int length )
{
	int i = 0;
	int result = 0;
	int channel  = terminal->terminal_ch;
		
	sci_emv_hard_fifo_spill(channel, length);
	
	while( i < length )
	{
		if( terminal->queue->timeout_index == terminal->queue->rxout_index)
		{
			result = adjust_timeout_type( terminal );
			break;
		}
			
		if( terminal->queue->parity_index == terminal->queue->rxout_index )
		{
			result = ICC_ERR_PARS;
			if( 0 == terminal->terminal_ptype )  break;
		}
	
		pbuf[ i++ ] = terminal->queue->txr_buff[ terminal->queue->rxout_index ];
		terminal->queue->rxout_index++;
		if( MAX_QBUF_SIZE == terminal->queue->rxout_index )
		{
			terminal->queue->rxout_index = 0;
		}
	}
	
	return result;
}


/**
 * ===================================================================
 * To tell the specific meaning according to the terminal state and
 * timeout code
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [input/output]terminal  : information on logic device
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *       Error Code List
 * ===================================================================
 */
int adjust_timeout_type( struct emv_core *terminal )
{
	int timeout_type;
	
	if( terminal->terminal_state == EMV_COLD_RESET ||
	terminal->terminal_state == EMV_WARM_RESET )
	{
		if(terminal->queue->timeout_index == 0) 
		{
			timeout_type = ICC_ERR_ATR_SWT;
		}
		else
		{
			timeout_type = ICC_ERR_ATR_CWT;
		}
	} 
	else if( 0 == terminal->terminal_ptype )
	{
		timeout_type = ICC_ERR_T0_WWT;
	}
	else
	{
		if(terminal->queue->timeout_index == 0) 
		{
			timeout_type = ICC_ERR_T1_BWT;
		}
		else
		{
			timeout_type = ICC_ERR_T1_CWT;
		}        
	}
    
	return timeout_type;
}
