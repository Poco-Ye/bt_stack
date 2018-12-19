#ifndef _ICC_QUEUE_H
#define _ICC_QUEUE_H

#ifdef __cplusplus 
extern "C" {
#endif

#include "..\protocol\icc_core.h"
#include "..\protocol\icc_errno.h"

struct emv_queue
{
	unsigned char txr_buff[512];
	int txin_index;
	int txout_index;
	int rxin_index;
	int rxout_index;
	int parity_index;
	int timeout_index;
	int wait_counter;
	int dev_sta;
};

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
int sci_core_init( struct emv_core *terminal, struct emv_queue *queue );

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
int sci_core_register( struct emv_core *devs, int nums );

/**
 * ===================================================================
 * 获取驱动中的设备信息节点头
 * -------------------------------------------------------------------
 * parameter：
 * -------------------------------------------------------------------
 * [output]devs  :  a pointer to emv_core struct indicating information
 * 		      on logic device `
 * -------------------------------------------------------------------
 * return：
 * -------------------------------------------------------------------
 *      0 - success
 *      others，failure
 * ===================================================================
 */
int sci_core_devs( struct emv_core **devs );

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
int sci_queue_flush( struct emv_core *terminal );

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
int sci_queue_empty( struct emv_core *terminal );

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
int sci_queue_length( struct emv_core *terminal );

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
int sci_queue_fill( struct emv_core *terminal, unsigned char *pbuf, int length );

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
int sci_queue_spill( struct emv_core *terminal, unsigned char *pbuf, int length );

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
int adjust_timeout_type( struct emv_core *terminal );

#ifdef __cplusplus
}
#endif
#endif
