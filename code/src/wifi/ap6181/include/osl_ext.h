/*
 * OS Abstraction Layer Extension - the APIs defined by the "extension" API
 * are only supported by a subset of all operating systems.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 * $Id: osl_ext.h,v 1.7 2009-11-27 00:38:05 Exp $
 */

#ifndef _osl_ext_h_
#define _osl_ext_h_


/* ---- Include Files ---------------------------------------------------- */

#include <ucos_osl_ext.h>
/* Include base operating system abstraction. */
#include <osl.h>


/* ---- Constants and Types ---------------------------------------------- */

/* -----------------------------------------------------------------------
 * Generic OS types.
 */
typedef enum osl_ext_status_t
{
	OSL_EXT_SUCCESS,
	OSL_EXT_ERROR,
	OSL_EXT_TIMEOUT

} osl_ext_status_t;

#define OSL_EXT_TIME_FOREVER ((osl_ext_time_ms_t)(-1))
typedef unsigned int osl_ext_time_ms_t;


/* -----------------------------------------------------------------------
 * Timers.
 */
typedef enum
{
	/* One-shot timer. */
	OSL_EXT_TIMER_MODE_ONCE,

	/* Periodic timer. */
	OSL_EXT_TIMER_MODE_REPEAT

} osl_ext_timer_mode_t;

/* User registered callback and parameter to invoke when timer expires. */
typedef void* osl_ext_timer_arg_t;
typedef void (*osl_ext_timer_callback)(osl_ext_timer_arg_t arg);

/* -----------------------------------------------------------------------
 * Tasks.
 */

/* Task entry argument. */
typedef void* osl_ext_task_arg_t;

/* Task entry function. */
typedef void (*osl_ext_task_entry)(osl_ext_task_arg_t arg);

/* Abstract task priority levels. */
typedef enum
{
	OSL_EXT_TASK_IDLE_PRIORITY,
	OSL_EXT_TASK_LOW_PRIORITY,
	OSL_EXT_TASK_LOW_NORMAL_PRIORITY,
	OSL_EXT_TASK_NORMAL_PRIORITY,
	OSL_EXT_TASK_HIGH_NORMAL_PRIORITY,
	OSL_EXT_TASK_HIGHEST_PRIORITY,
	OSL_EXT_TASK_TIME_CRITICAL_PRIORITY,

	/* This must be last. */
	OSL_EXT_TASK_NUM_PRIORITES
} osl_ext_task_priority_t;


/* ---- Variable Externs ------------------------------------------------- */
/* ---- Function Prototypes ---------------------------------------------- */


/* --------------------------------------------------------------------------
** Semaphore
*/

/****************************************************************************
* Function:   osl_ext_sem_create
*
* Purpose:    Creates a counting semaphore object, which can subsequently be
*             used to guard multiple instances of a given resource.
*
* Parameters: name     (in)  Name to assign to the semaphore (must be unique).
*             init_cnt (in)  Initial count that the semaphore should have.
*             sem      (out) Newly created semaphore.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_sem_create(char *name, int init_cnt, osl_ext_sem_t *sem);

/****************************************************************************
* Function:   osl_ext_sem_delete
*
* Purpose:    Destroys a previously created semaphore object.
*
* Parameters: sem (mod) Semaphore object to destroy.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_sem_delete(osl_ext_sem_t *sem);

/****************************************************************************
* Function:   osl_ext_sem_give
*
* Purpose:    Increments the count associated with the semaphore. This will
*             cause one thread blocked on a take to wake up.
*
* Parameters: sem (mod) Semaphore object to give.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_sem_give(osl_ext_sem_t *sem);

/****************************************************************************
* Function:   osl_ext_sem_take
*
* Purpose:    Decrements the count associated with the semaphore. If the count
*             is less than zero, then the calling task will become blocked until
*             another thread does a give on the semaphore. This function will only
*             block the calling thread for timeout_msec milliseconds, before
*             returning with OSL_EXT_TIMEOUT.
*
* Parameters: sem          (mod) Semaphore object to take.
*             timeout_msec (in)  Number of milliseconds to wait for the
*                                semaphore to enter a state where it can be
*                                taken.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_sem_take(osl_ext_sem_t *sem, osl_ext_time_ms_t timeout_msec);


/* --------------------------------------------------------------------------
** Mutex
*/

/****************************************************************************
* Function:   osl_ext_mutex_create
*
* Purpose:    Creates a mutex object, which can subsequently be used to control
*             mutually exclusive access to a resource.
*
* Parameters: name  (in)  Name to assign to the mutex (must be unique).
*             mutex (out) Mutex object to initialize.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_mutex_create(char *name, osl_ext_mutex_t *mutex);

/****************************************************************************
* Function:   osl_ext_mutex_delete
*
* Purpose:    Destroys a previously created mutex object.
*
* Parameters: mutex (mod) Mutex object to destroy.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_mutex_delete(osl_ext_mutex_t *mutex);

/****************************************************************************
* Function:   osl_ext_mutex_acquire
*
* Purpose:    Acquires the indicated mutual exclusion object. If the object is
*             currently acquired by another task, then this function will wait
*             for timeout_msec milli-seconds before returning with OSL_EXT_TIMEOUT.
*
* Parameters: mutex        (mod) Mutex object to acquire.
*             timeout_msec (in)  Number of milliseconds to wait for the mutex.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_mutex_acquire(osl_ext_mutex_t *mutex, osl_ext_time_ms_t timeout_msec);

/****************************************************************************
* Function:   osl_ext_mutex_release
*
* Purpose:    Releases the indicated mutual exclusion object. This makes it
*             available for another task to acquire.
*
* Parameters: mutex (mod) Mutex object to release.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_mutex_release(osl_ext_mutex_t *mutex);

/* --------------------------------------------------------------------------
** Queue
*/

/****************************************************************************
* Function:   osl_ext_queue_create
*
* Purpose:    Create a queue.
*
* Parameters: name     (in)  Name to assign to the queue (must be unique).
*             size     (in)  Size of the queue.
*             queue    (out) Newly created queue.
*
* Returns:    OSL_EXT_SUCCESS if the queue was created successfully, or an
*             error code if the queue could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_queue_create(char *name, unsigned int queue_size, osl_ext_queue_t *queue);
bool osl_ext_sdio_write_reg(uchar byFuncNum, uint uiRegAddr, uchar bySrc);
bool osl_ext_sdio_read_reg(uchar byFuncNum, uint uiRegAddr, uchar *pbyDst);
bool osl_ext_sdio_write_multi_reg(uchar byFuncNum, uchar byBlkMode, uchar byOpCode, uint uiRegAddr, uint uiCount, uint  uiBlkSize, uchar *pbySrc);
bool osl_ext_sdio_read_multi_reg(uchar byFuncNum, uchar byBlkMode, uchar byOpCode, uint uiRegAddr, uint uiCount, uint uiBlkSize, uchar *pbyDst);


#endif	/* _osl_ext_h_ */
