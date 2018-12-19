/*
 * Definitions for API from sdio common code (bcmsdh) to individual
 * host controller drivers.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcmsdbus.h,v 13.17.86.2 2010-12-23 01:13:20 Exp $
 */

#ifndef	_sdio_api_h_
#define	_sdio_api_h_


#define SDIOH_API_RC_SUCCESS                        (0x00)
#define SDIOH_API_RC_FAIL	                        (0x01)
#define SDIOH_API_SUCCESS(status) (status == 0)

#define SDIOH_READ              0	/* Read request */
#define SDIOH_WRITE             1	/* Write request */

#define SDIOH_DATA_FIX          0	/* Fixed addressing */
#define SDIOH_DATA_INC          1	/* Incremental addressing */

#define SDIOH_CMD_TYPE_NORMAL   0       /* Normal command */
#define SDIOH_CMD_TYPE_APPEND   1       /* Append command */
#define SDIOH_CMD_TYPE_CUTTHRU  2       /* Cut-through command */

#define SDIOH_DATA_PIO          0       /* PIO mode */
#define SDIOH_DATA_DMA          1       /* DMA mode */



/* SDio Host structure */
typedef struct sdioh_info sdioh_info_t;

/* callback function, taking one arg */
typedef void (*sdioh_cb_fn_t)(void *);

/* attach, return handler on success, NULL if failed.
 *  The handler shall be provided by all subsequent calls. No local cache
 *  cfghdl points to the starting address of pci device mapped memory
 */
extern int sdioh_interrupt_register(sdioh_info_t *si, sdioh_cb_fn_t fn, void *argh);
extern int sdioh_interrupt_deregister(sdioh_info_t *si);

/* query whether SD interrupt is enabled or not */
extern int sdioh_interrupt_query(sdioh_info_t *si, bool *onoff);

/* enable or disable SD interrupt */
extern int sdioh_interrupt_set(sdioh_info_t *si, bool enable_disable);

/* read or write one byte using cmd52 */
extern int sdioh_request_byte(uint rw, uint fnc, uint addr, uint8 *byte);

/* read or write 2/4 bytes using cmd53 */
extern int sdioh_request_word(sdioh_info_t *si, uint cmd_type, uint rw, uint fnc,
	uint addr, uint32 *word, uint nbyte);

/* read or write any buffer using cmd53 */
extern int sdioh_request_buffer(sdioh_info_t *si, uint fix_inc, uint rw, uint fnc_num, uint32 addr, uint regwidth, uint32 buflen, uint8 *buffer, void *pkt);

/* get cis data */
extern int sdioh_cis_read(sdioh_info_t *si, uint fuc, uint8 *cis, uint32 length);

extern int sdioh_cfg_read(uint fuc, uint32 addr, uint8 *data);
extern int sdioh_cfg_write(uint fuc, uint32 addr, uint8 *data);
extern int sdioh_iovar_op(sdioh_info_t *si, const char *name, void *params, int plen, void *arg, int len, bool set);

/* Issue abort to the specified function and clear controller as needed */
extern int sdioh_abort(uint fnc);

/* Start and Stop SDIO without re-enumerating the SD card. */
extern int sdioh_start(void);

/* Wait system lock free */
extern int sdioh_waitlockfree(sdioh_info_t *si);

#endif /* _sdio_api_h_ */
