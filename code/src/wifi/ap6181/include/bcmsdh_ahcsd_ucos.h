/*
 * SDIO host client driver interface of Broadcom HNBU
 *     export functions to client drivers
 *     abstract OS and BUS specific details of SDIO
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: bcmsdh_generic.h,v 1.2 2008-12-01 22:56:58 Exp $
 */

#ifndef	_bcmsdh_rex_h_
#define	_bcmsdh_rex_h_

#include <osl.h>
#include <bcmsdbus.h>	/* bcmsdh to/from specific controller APIs */
#include <sdio.h>	/* SDIO Device and Protocol Specs */
#include <sdioh.h>	/* SDIO Host Controller Specification */
#include <dhd_dbg.h>

/* Global message bits */
extern uint sd_msglevel;

#define sd_err(x)	do { if (sd_msglevel & SDH_ERROR_VAL) osl_ext_printf x; } while (0)
#define sd_trace(x)	do { if (sd_msglevel & SDH_TRACE_VAL) osl_ext_printf x; } while (0)
#define sd_info(x)	do { if (sd_msglevel & SDH_INFO_VAL) osl_ext_printf x; } while (0)
#define sd_debug(x) do { if (sd_msglevel & SDH_DEBUG_VAL) osl_ext_printf x; } while (0)
#define sd_data(x)	do { if (sd_msglevel & SDH_DATA_VAL) osl_ext_printf x; } while (0)
#define sd_ctrl(x)	do { if (sd_msglevel & SDH_CTRL_VAL) osl_ext_printf x; } while (0)

#define BLOCK_SIZE_4318 64
#define BLOCK_SIZE_4328 512

/* private bus modes */
#define SDIOH_MODE_SD4		2
#define CLIENT_INTR 		0x100	/* Get rid of this! */

struct sdioh_info {
	osl_t 		*osh;			/* osh handler */
	bool		client_intr_enabled;	/* interrupt connnected flag */
	bool		intr_handler_valid;	/* client driver interrupt handler valid */
	sdioh_cb_fn_t	intr_handler;		/* registered interrupt handler */
	void		*intr_handler_arg;	/* argument to call interrupt handler */
	uint16		intmask;		/* Current active interrupts */
	void		*sdos_info;		/* Pointer to per-OS private data */

	uint 		irq;			/* Client irq */
	int 		intrcount;		/* Client interrupts */

	bool		sd_use_dma;		/* DMA on CMD53 */
	bool 		sd_blockmode;		/* sd_blockmode == FALSE => 64 Byte Cmd 53s. */
						/*  Must be on for sd_multiblock to be effective */
	bool 		use_client_ints;	/* If this is false, make sure to restore */
	int 		sd_mode;		/* SD1/SD4/SPI */
	int 		client_block_size[SDIOD_MAX_IOFUNCS];		/* Blocksize */
	uint8 		num_funcs;		/* Supported funcs on client */
	uint32 		com_cis_ptr;
	uint32 		func_cis_ptr[SDIOD_MAX_IOFUNCS];
	uint		max_dma_len;
	uint		max_dma_descriptors;	/* DMA Descriptors supported by this controller. */
	uint32		devID;			/* Device ID */
};

#endif	/* _bcmsdh_rex_h_ */
