/*
 *  BCMSDH interface glue
 *  implement bcmsdh API for SDIOH driver
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcmsdh.c,v 1.57.6.4 2010-12-23 01:13:15 Exp $
 */
/* ****************** BCMSDH Interface Functions *************************** */

#include <typedefs.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <osl.h>
#include <bcmsdh.h>	/* BRCM API for SDIO clients (such as wl, dhd) */
#include <bcmsdbus.h>	/* common SDIO/controller interface */
#include <sbsdio.h>	/* BRCM sdio device core */
#include <sdio.h>	/* sdio spec */
#include "generic_osl.h"
#include <sdiovar.h>	
#include <bcmsdh_ahcsd_ucos.h>
#include <dhd_dbg.h>

#define DMA_ALIGN_MASK	0x03

uint sd_sdmode = SDIOH_MODE_SD4;	/* Use SD4 mode by default */
uint sd_divisor = 2;			    /* Default 48MHz/2 = 24MHz */
uint sd_power = 1;		/* Default to SD Slot powered ON */
uint sd_clock = 1;		/* Default to SD Clock turned ON */
uint sd_hiok = FALSE;	/* Don't use hi-speed mode by default */
uint sd_msglevel = 0x11;
uint sd_use_dma = TRUE;

void * dhdsdio_probe(void *regsva, osl_t * osh, void *sdh);

/* read 24 bits and return valid 17 bit addr */
int sdioh_ahcsd_get_cisaddr(uint32 regaddr)
{
	int i;
	uint32 scratch, regdata;
	uint8 *ptr = (uint8 *)&scratch;

	for (i = 0; i < 3; i++) {
		if(!osl_ext_sdio_read_reg(SDIO_FUNC_0, regaddr, (uint8*)&regdata))
            ASSERT(0);

		*ptr++ = (uint8) regdata;
		regaddr++;
	}

	/* Only the lower 17-bits are valid */
	scratch = ltoh32(scratch);
	scratch &= 0x0001FFFF;
	return (scratch);
}

#define SDIO_RDY_MAX_RETRIES   0x1000

int sdioh_ahcsd_enable_fn(uint8 devfn)
{
    uint8	devfn_bit = 1 << devfn;
    int     t  = 0;
    uint8   rdata = 0;

    /* read out the enable register */
    osl_ext_sdio_read_reg(SDIO_FUNC_0, SDIOD_CCCR_IOEN, &rdata);

    rdata = rdata | devfn_bit; /* enable IO */
    osl_ext_sdio_write_reg(SDIO_FUNC_0, SDIOD_CCCR_IOEN, rdata);

    /* check for IO ready */
    do {
		OSL_DELAY(1000);
        osl_ext_sdio_read_reg(SDIO_FUNC_0, SDIOD_CCCR_IORDY, &rdata);
        
        if ((rdata & devfn_bit) == devfn_bit) break;

    } while( t++ < SDIO_RDY_MAX_RETRIES );
    
    if( t == (SDIO_RDY_MAX_RETRIES + 1))
		return SDIOH_API_RC_FAIL;

	return SDIOH_API_RC_SUCCESS;
}

int sdioh_ahcsd_disable_fn(uint8 devfn)
{
    uint8  devfn_bit = 1 << devfn;
    uint8  data = 0;
    
    /* read out the enable register */
    osl_ext_sdio_read_reg(SDIO_FUNC_0, SDIOD_CCCR_IOEN, &data);
    
    data = data & (~devfn_bit); /* disable IO */
    osl_ext_sdio_write_reg(SDIO_FUNC_0, SDIOD_CCCR_IOEN, data);
	
	return SDIOH_API_RC_SUCCESS;
}

int sdioh_ahcsd_card_enablefuncs(sdioh_info_t *sd)
{
	uint32 fbraddr;
	uint8 func;

	/* Get the Card's common CIS address */
	sd->com_cis_ptr = sdioh_ahcsd_get_cisaddr(SDIOD_CCCR_CISPTR_0);

	/* Get the Card's function CIS (for each function) */
	for (fbraddr = SDIOD_FBR_STARTADDR, func = 1; func <= sd->num_funcs; func++, fbraddr += SDIOD_FBR_SIZE) {
		sd->func_cis_ptr[func] = sdioh_ahcsd_get_cisaddr(SDIOD_FBR_CISPTR_0 + fbraddr);
	}

	sd->func_cis_ptr[0] = sd->com_cis_ptr;

	if(sdioh_ahcsd_enable_fn(SDIO_FUNC_1) != SDIOH_API_RC_SUCCESS) /* Enable Function 1 */
        ASSERT(0);

	return SDIOH_API_RC_SUCCESS;
}

int sdioh_ahcsd_set_blksz(uint8 devfn, uint16 blksz)
{
    uint8	data  = 0 ;
    uint16  bsize = 0;
      
    data = (uint8)blksz;
    osl_ext_sdio_write_reg(SDIO_FUNC_0, SDIOD_FBR_BASE(devfn)+SDIOD_FBR_BLKSIZE_0, data);

    data = (uint8)(blksz >> 8);
    osl_ext_sdio_write_reg(SDIO_FUNC_0, SDIOD_FBR_BASE(devfn)+SDIOD_FBR_BLKSIZE_1, data);
    osl_ext_sdio_read_reg(SDIO_FUNC_0, SDIOD_FBR_BASE(devfn)+SDIOD_FBR_BLKSIZE_1, &data);
    
    bsize = data;
    osl_ext_sdio_read_reg(SDIO_FUNC_0, SDIOD_FBR_BASE(devfn)+SDIOD_FBR_BLKSIZE_0, &data);

    bsize = (bsize << 8) | data;
    return SDIOH_API_RC_SUCCESS;
}

sdioh_info_t g_sd;
sdioh_info_t * sdioh_attach(osl_t *osh)
{
	sdioh_info_t *sd=&g_sd;

	bzero((char *)sd, sizeof(sdioh_info_t));
	sd->osh = osh;
	sd->num_funcs = 2;
	sd->sd_blockmode = TRUE;
	sd->use_client_ints = TRUE;
	sd->client_block_size[0] = 64;
	sd->client_block_size[1] = 64; /* Set function 1 block size */
	sd->client_block_size[2] = 64; /* Set function 2 block size */

	if (sdioh_start() != SDIOH_API_RC_SUCCESS)
		return NULL;

    if(sdioh_ahcsd_set_blksz(SDIO_FUNC_1, sd->client_block_size[1]) != SDIOH_API_RC_SUCCESS) ASSERT(0);
	if(sdioh_ahcsd_set_blksz(SDIO_FUNC_2, sd->client_block_size[2]) != SDIOH_API_RC_SUCCESS) ASSERT(0);
	if(sdioh_ahcsd_card_enablefuncs(sd) != SDIOH_API_RC_SUCCESS) ASSERT(0);
	return sd;
}

int sdioh_detach(osl_t *osh, sdioh_info_t *si)
{
	if (si) 
    {
		sdioh_ahcsd_disable_fn(SDIO_FUNC_2); /* Disable Function 2 */
		sdioh_ahcsd_disable_fn(SDIO_FUNC_1); /* Disable Function 1 */
	}
	
	return SDIOH_API_RC_SUCCESS;
}

int sdioh_request_byte(uint rw, uint func, uint addr, uint8 *byte)
{
	int err_ret = SDIOH_API_RC_FAIL;
	
	if(rw) { /* CMD52 Write */
		if (func == 0) {
			/* Can only directly write to some F0 registers.  Handle F2 enable
			 * as a special case.
			 */
			if (addr == SDIOD_CCCR_IOEN) {
				if (*byte & SDIO_FUNC_ENABLE_2) {
					/* Enable Function 2 */
					err_ret = sdioh_ahcsd_enable_fn(SDIO_FUNC_2);
					if (err_ret) {
						sd_err(("%s: Enable F2 failed\n", __FUNCTION__));
					}
				} else {
					/* Disable Function 2 */
					err_ret = sdioh_ahcsd_disable_fn(SDIO_FUNC_2);
					if (err_ret) {
						sd_err(("%s: Disab F2 failed\n", __FUNCTION__));
					}
				}
			}
			else if (addr < 0xF0) {
				sd_err(("%s: F0 Wr:0x%02x: write disallowed\n", __FUNCTION__));
			} else {
				/* perform F0 write */
				if(osl_ext_sdio_write_reg(SDIO_FUNC_0, addr, *byte))
					err_ret = 0;
				else
					err_ret = 1;
			}
		} else {
			if(osl_ext_sdio_write_reg(func, addr, *byte))
				err_ret = 0;
			else
				err_ret = 1;			
		}
	} else { /* CMD52 Read */
		if(osl_ext_sdio_read_reg(func, addr, byte))
			err_ret = 0;
		else
			err_ret = 1;		
	}
	
	if (err_ret) {
		sd_err(("%s: Failed to %s byte F%d:@0x%05x=%02x, Err: %d\n",
					__FUNCTION__, rw ? "Write" : "Read", func, addr, *byte, err_ret));
	}
	
	return ((err_ret == 0) ? SDIOH_API_RC_SUCCESS : SDIOH_API_RC_FAIL);
}

int sdioh_cfg_read(uint func, uint32 addr, uint8 *data)
{
	return sdioh_request_byte(SDIOH_READ, func, addr, data);
}

int sdioh_cfg_write(uint func, uint32 addr, uint8 *data)
{
	return sdioh_request_byte(SDIOH_WRITE, func, addr, data);
}

int sdioh_request_word(sdioh_info_t *si, uint cmd_type, uint rw, uint func, uint addr, uint32 *word, uint nbytes)
{
	int err_ret = SDIOH_API_RC_FAIL;
	
	if (func == 0) {
		sd_err(("%s: Only CMD52 allowed to F0.\n", __FUNCTION__));
		return SDIOH_API_RC_FAIL;
	}

	err_ret = sdioh_request_buffer(si, SDIOH_DATA_INC, (rw ? SDIOH_WRITE : SDIOH_READ), func, addr, 4, nbytes, (uint8*)word, NULL);

	if (err_ret) {
		sd_err(("%s: Failed to %s word, Err: 0x%08x", __FUNCTION__, rw ? "Write" : "Read", err_ret));
	}

	return ((err_ret == 0) ? SDIOH_API_RC_SUCCESS : SDIOH_API_RC_FAIL);
}

uint sdioh_query_iofnum(sdioh_info_t *si)
{
	return si->num_funcs;
}

/* Read client card reg */
int sdioh_ahcsd_card_regread(sdioh_info_t *si, int func, uint32 regaddr, int regsize, uint32 *data)
{
	if ((func == 0) || (regsize == 1)) {
		uint8 temp = 0;

		sdioh_request_byte(SDIOH_READ, func, regaddr, &temp);
		*data = temp;
		*data &= 0xff;
	} 
    else {
		sdioh_request_word(si, 0, SDIOH_READ, func, regaddr, data, regsize);
		if (regsize == 2)
			*data &= 0xffff;
	}

	return SDIOH_API_RC_SUCCESS;
}

int sdioh_cis_read(sdioh_info_t *si, uint func, uint8 *cisd, uint32 length)
{
	uint32 count;
	int offset;
	uint32 foo;
	uint8 *cis = cisd;

	if (!si->func_cis_ptr[func]) {
		bzero(cis, length);
        ASSERT(0);
		return SDIOH_API_RC_FAIL;
	}

	for (count = 0; count < length; count++) {
		offset =  si->func_cis_ptr[func] + count;
		if (sdioh_ahcsd_card_regread(si, 0, offset, 1, &foo) != SDIOH_API_RC_SUCCESS) {
            ASSERT(0);
			return SDIOH_API_RC_FAIL;
		}

		*cis = (uint8)(foo & 0xff);
		cis++;
	}

	return SDIOH_API_RC_SUCCESS;
}

int sdioh_request_packet(sdioh_info_t *si, uint fix_inc, uint write, uint func, uint32 addr, void *pkt)
{
	bool fifo = (fix_inc == SDIOH_DATA_FIX);
	void *pnext;
	int err_ret;

	ASSERT(pkt);

	for (pnext = pkt; pnext; pnext = PKTNEXT(si->osh, pnext)) 
    {
		uint pkt_len = PKTLEN(si->osh, pnext);
		pkt_len += 3;
		pkt_len &= 0xFFFFFFFC;

		/* Make sure the packet is aligned properly. If it isn't, then this
		 * is the fault of sdioh_request_buffer() which is supposed to give
		 * us something we can work with.
		 */

		ASSERT(((uint32)(PKTDATA(si->osh, pkt)) & DMA_ALIGN_MASK) == 0);

		if (write) 
        {
			if(pkt_len > si->client_block_size[func]) 
            {
				err_ret = osl_ext_sdio_write_multi_reg(func, SD_IO_BLOCK_MODE, fix_inc, addr, 
										pkt_len/si->client_block_size[func],
										si->client_block_size[func],
										((uint8*)PKTDATA(si->osh, pnext)));

				/* Deal with the remaining data which is less than a block size */
				if(pkt_len % si->client_block_size[func]) 
                {
					uint dlength = (pkt_len/si->client_block_size[func])*si->client_block_size[func];
					uint8* ptr = PKTDATA(si->osh, pnext);

					pkt_len -= dlength;
					addr += dlength;
					ptr += dlength;
					err_ret = osl_ext_sdio_write_multi_reg(func, SD_IO_BYTE_MODE, fix_inc, addr, pkt_len, si->client_block_size[func], ptr);					

				}
			} 
            else 
            {
				err_ret = osl_ext_sdio_write_multi_reg(func, SD_IO_BYTE_MODE, fix_inc, addr, 
										pkt_len, si->client_block_size[func], ((uint8*)PKTDATA(si->osh, pnext)));					
			}	
		} 
        else 
        {
			if(pkt_len > si->client_block_size[func]) 
            {
				err_ret = osl_ext_sdio_read_multi_reg(func, SD_IO_BLOCK_MODE, fix_inc, addr, 
										pkt_len/si->client_block_size[func], si->client_block_size[func], ((uint8*)PKTDATA(si->osh, pnext)));
		
				/* Deal with the remaining data which is less than a block size */
				if(pkt_len % si->client_block_size[func]) 
                {
					uint dlength = (pkt_len/si->client_block_size[func])*si->client_block_size[func];
					uint8* ptr = PKTDATA(si->osh, pnext);
							
					pkt_len -= dlength;
					addr += dlength;
					ptr += dlength;
							
					err_ret = osl_ext_sdio_read_multi_reg(func, SD_IO_BYTE_MODE, fix_inc, addr, pkt_len, si->client_block_size[func], ptr);
				}				
			} 
            else 
            {
				err_ret = osl_ext_sdio_read_multi_reg(func, SD_IO_BYTE_MODE, fix_inc, addr, pkt_len, 0,	((uint8*)PKTDATA(si->osh, pnext)));
			}
		}

		if (!err_ret)
            prints1("%s sdio error!\r\n", __FUNCTION__);

		if (!fifo)
			addr += pkt_len;
	}

	return (err_ret ? SDIOH_API_RC_SUCCESS : SDIOH_API_RC_FAIL);
}

/*
 * This function takes a buffer or packet, and fixes everything up so that in the
 * end, a DMA-able packet is created.
 *
 * A buffer does not have an associated packet pointer, and may or may not be aligned.
 * A packet may consist of a single packet, or a packet chain.  If it is a packet chain,
 * then all the packets in the chain must be properly aligned.  If the packet data is not
 * aligned, then there may only be one packet, and in this case, it is copied to a new
 * aligned packet.
 *
 */
int sdioh_request_buffer(sdioh_info_t *si, uint fix_inc, uint write, uint func, uint32 addr, uint reg_width, uint32 buflen_u, uint8 * buffer, void *pkt)
{
	int Status;
	void *mypkt = NULL;

	if(buflen_u == 0)
		return SDIOH_API_RC_SUCCESS;

	if (pkt == NULL) /* Case 1: we don't have a packet. */
    {
		if (!(mypkt = pktcfgget(si->osh, buflen_u))) 
        {
            ASSERT(0);
			return SDIOH_API_RC_FAIL;
		}

		if (write)  /* For a write, copy the buffer data into the packet. */
			bcopy(buffer, PKTDATA(si->osh, mypkt), buflen_u);

		Status = sdioh_request_packet(si, fix_inc, write, func, addr, mypkt);

		if (!write) /* For a read, copy the packet data back to the buffer. */
			bcopy(PKTDATA(si->osh, mypkt), buffer, buflen_u);
		
		pktfree(si->osh, mypkt);
	}
	else if (((uint32)(PKTDATA(si->osh, pkt)) & DMA_ALIGN_MASK) != 0) 
    {
        ASSERT(0);
	}
	else /* case 3: We have a packet and it is aligned. */
    { 
		Status = sdioh_request_packet(si, fix_inc, write, func, addr, pkt);
	}

	return (Status);
}

/* IOVar table */
enum {
	IOV_MSGLEVEL = 1,
	IOV_BLOCKMODE,
	IOV_BLOCKSIZE,
	IOV_DMA,
	IOV_USEINTS,
	IOV_NUMINTS,
	IOV_NUMLOCALINTS,
	IOV_HOSTREG,
	IOV_DEVREG,
	IOV_DIVISOR,
	IOV_SDMODE,
	IOV_HISPEED,
	IOV_HCIREGS,
	IOV_POWER,
	IOV_CLOCK,
	IOV_RXCHAIN
};

const bcm_iovar_t sdioh_iovars[] = {
	{"sd_msglevel",     IOV_MSGLEVEL,	0,	IOVT_UINT32,	0 },
	{"sd_blockmode",    IOV_BLOCKMODE,  0,	IOVT_BOOL,	    0 },
	{"sd_blocksize",    IOV_BLOCKSIZE,  0,	IOVT_UINT32,	0 }, /* ((fn << 16) | size) */
	{"sd_dma",	        IOV_DMA,	    0,	IOVT_BOOL,	    0 },
	{"sd_ints", 	    IOV_USEINTS,	0,	IOVT_BOOL,	    0 },
	{"sd_numints",	    IOV_NUMINTS,	0,	IOVT_UINT32,	0 },
	{"sd_numlocalints", IOV_NUMLOCALINTS, 0, IOVT_UINT32,	0 },
	{"sd_hostreg",	    IOV_HOSTREG,	0,	IOVT_BUFFER,	sizeof(sdreg_t) },
	{"sd_devreg",	    IOV_DEVREG, 	0,	IOVT_BUFFER,	sizeof(sdreg_t) },
	{"sd_divisor",	    IOV_DIVISOR,	0,	IOVT_UINT32,	0 },
	{"sd_power",	    IOV_POWER,	    0,	IOVT_UINT32,	0 },
	{"sd_clock",	    IOV_CLOCK,	    0,	IOVT_UINT32,	0 },
	{"sd_mode", 	    IOV_SDMODE, 	0,	IOVT_UINT32,	100},
	{"sd_highspeed",    IOV_HISPEED,	0,	IOVT_UINT32,	0 },
	{"sd_rxchain",      IOV_RXCHAIN,    0, 	IOVT_BOOL,	    0 },
	{NULL, 0, 0, 0, 0 }
};


int sdioh_iovar_op(sdioh_info_t *si, const char *name, void *params, int plen, void *arg, int len, bool set)
{
	const bcm_iovar_t *vi = NULL;
	int bcmerror = 0;
	int val_size;
	int32 int_val = 0;
	bool bool_val;
	uint32 actionid;

	ASSERT(name);
	ASSERT(len >= 0);
	ASSERT(set || (arg && len)); /* Get must have return space; Set does not take qualifiers */
	ASSERT(!set || (!params && !plen));

	if ((vi = bcm_iovar_lookup(sdioh_iovars, name)) == NULL) {
		bcmerror = BCME_UNSUPPORTED;
		goto exit;
	}

	if ((bcmerror = bcm_iovar_lencheck(vi, arg, len, set)) != 0)
		goto exit;

	/* Set up params so get and set can share the convenience variables */
	if (params == NULL) {
		params = arg;
		plen = len;
	}

	if (vi->type == IOVT_VOID)
		val_size = 0;
	else if (vi->type == IOVT_BUFFER)
		val_size = len;
	else
		val_size = sizeof(int);

	if (plen >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	actionid = set ? IOV_SVAL(vi->varid) : IOV_GVAL(vi->varid);
	switch (actionid) {
	case IOV_GVAL(IOV_MSGLEVEL):
		int_val = (int32)sd_msglevel;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_MSGLEVEL):
		sd_msglevel = int_val;
		break;

	case IOV_GVAL(IOV_BLOCKMODE):
		int_val = (int32)si->sd_blockmode;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_BLOCKMODE):
		si->sd_blockmode = (bool)int_val;
		/* Haven't figured out how to make non-block mode with DMA */
		break;

	case IOV_GVAL(IOV_BLOCKSIZE):
		if ((uint32)int_val > si->num_funcs) {
			bcmerror = BCME_BADARG;
			break;
		}
		int_val = (int32)si->client_block_size[int_val];
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_BLOCKSIZE):
	{
		uint func = ((uint32)int_val >> 16);
		uint blksize = (uint16)int_val;
		uint maxsize;

		if (func > si->num_funcs) {
			bcmerror = BCME_BADARG;
			break;
		}

		switch (func) {
		case 0: maxsize = 32; break;
		case 1: maxsize = BLOCK_SIZE_4318; break;
		case 2: maxsize = BLOCK_SIZE_4328; break;
		default: maxsize = 0;
		}
		if (blksize > maxsize) {
			bcmerror = BCME_BADARG;
			break;
		}
		if (!blksize) {
			blksize = maxsize;
		}

		/* Now set it */
		si->client_block_size[func] = blksize;

		break;
	}

	case IOV_GVAL(IOV_RXCHAIN):
		int_val = FALSE;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_GVAL(IOV_DMA):
		int_val = (int32)si->sd_use_dma;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_DMA):
		si->sd_use_dma = (bool)int_val;
		break;

	case IOV_GVAL(IOV_USEINTS):
		int_val = (int32)si->use_client_ints;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_USEINTS):
		si->use_client_ints = (bool)int_val;
		if (si->use_client_ints)
			si->intmask |= CLIENT_INTR;
		else
			si->intmask &= ~CLIENT_INTR;

		break;

	case IOV_GVAL(IOV_DIVISOR):
		int_val = (uint32)sd_divisor;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_DIVISOR):
		sd_divisor = int_val;
		break;

	case IOV_GVAL(IOV_POWER):
		int_val = (uint32)sd_power;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_POWER):
		sd_power = int_val;
		break;

	case IOV_GVAL(IOV_CLOCK):
		int_val = (uint32)sd_clock;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_CLOCK):
		sd_clock = int_val;
		break;

	case IOV_GVAL(IOV_SDMODE):
		int_val = (uint32)sd_sdmode;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_SDMODE):
		sd_sdmode = int_val;
		break;

	case IOV_GVAL(IOV_HISPEED):
		int_val = (uint32)sd_hiok;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_SVAL(IOV_HISPEED):
		sd_hiok = int_val;
		break;

	case IOV_GVAL(IOV_NUMINTS):
		int_val = (int32)si->intrcount;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_GVAL(IOV_NUMLOCALINTS):
		int_val = (int32)0;
		bcopy(&int_val, arg, val_size);
		break;

	case IOV_GVAL(IOV_HOSTREG):
	{
		sdreg_t *sd_ptr = (sdreg_t *)params;

		if (sd_ptr->offset < SD_SysAddr || sd_ptr->offset > SD_MaxCurCap) {
			sd_err(("%s: bad offset 0x%x\n", __FUNCTION__, sd_ptr->offset));
			bcmerror = BCME_BADARG;
			break;
		}

		if (sd_ptr->offset & 1)
			int_val = 8; /* sdioh_sdmmc_rreg8(si, sd_ptr->offset); */
		else if (sd_ptr->offset & 2)
			int_val = 16; /* sdioh_sdmmc_rreg16(si, sd_ptr->offset); */
		else
			int_val = 32; /* sdioh_sdmmc_rreg(si, sd_ptr->offset); */

		bcopy(&int_val, arg, sizeof(int_val));
		break;
	}

	case IOV_SVAL(IOV_HOSTREG):
	{
		sdreg_t *sd_ptr = (sdreg_t *)params;

		if (sd_ptr->offset < SD_SysAddr || sd_ptr->offset > SD_MaxCurCap) 
        {
			sd_err(("%s: bad offset 0x%x\n", __FUNCTION__, sd_ptr->offset));
			bcmerror = BCME_BADARG;
			break;
		}
		break;
	}

	case IOV_GVAL(IOV_DEVREG):
	{
		sdreg_t *sd_ptr = (sdreg_t *)params;
		uint8 data = 0;

		if (sdioh_cfg_read(sd_ptr->func, sd_ptr->offset, &data)) {
			bcmerror = BCME_SDIO_ERROR;
			break;
		}

		int_val = (int)data;
		bcopy(&int_val, arg, sizeof(int_val));
		break;
	}

	case IOV_SVAL(IOV_DEVREG):
	{
		sdreg_t *sd_ptr = (sdreg_t *)params;
		uint8 data = (uint8)sd_ptr->value;

		if (sdioh_cfg_write(sd_ptr->func, sd_ptr->offset, &data)) {
			bcmerror = BCME_SDIO_ERROR;
			break;
		}
		break;
	}

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}
exit:

	return bcmerror;
}

/* Disable device interrupt */
void sdioh_ahcsd_devintr_off(sdioh_info_t *sd)
{
	uint8 reg_value = 0;

	sd->intmask &= ~CLIENT_INTR;

	/* disable interrupt on device */
	osl_ext_sdio_read_reg(SDIO_FUNC_0, SDIOD_CCCR_INTEN, &reg_value);					

	/* Function 1&2 interrupt disable */
	reg_value &= ~INTR_CTL_FUNC1_EN;
	reg_value &= ~INTR_CTL_FUNC2_EN;

	/* Master interrupt disable */
	reg_value &= ~INTR_CTL_MASTER_EN;
	
	osl_ext_sdio_write_reg(SDIO_FUNC_0, SDIOD_CCCR_INTEN, reg_value);		
}

/* Enable device interrupt */
void sdioh_ahcsd_devintr_on(sdioh_info_t *sd)
{
	uint8 reg_value = 0;

	sd->intmask |= CLIENT_INTR;

	/* enable interrupt on device */
	osl_ext_sdio_read_reg(SDIO_FUNC_0, SDIOD_CCCR_INTEN, &reg_value);					

	/* Function 1&2 interrupt enable */
	reg_value |= INTR_CTL_FUNC1_EN;
	reg_value |= INTR_CTL_FUNC2_EN;

	/* Master interrupt enable */
	reg_value |= INTR_CTL_MASTER_EN;
	osl_ext_sdio_write_reg(SDIO_FUNC_0, SDIOD_CCCR_INTEN, reg_value);	
}

/* Interrupt enable/disable */
int sdioh_interrupt_set(sdioh_info_t *sd, bool enable)
{
	// 需要处理sd指针为空的情况
	if ((NULL == sd) || (enable && !(sd->intr_handler && sd->intr_handler_arg)))
		return SDIOH_API_RC_FAIL;

	sd->client_intr_enabled = enable;
	if (enable) 
		sdioh_ahcsd_devintr_on(sd);
	else
		sdioh_ahcsd_devintr_off(sd);

	return SDIOH_API_RC_SUCCESS;
}

int sdioh_abort(uint func)
{
    osl_ext_sdio_write_reg(SDIO_FUNC_0, SDIOD_CCCR_IOABORT, func);
	OSL_DELAY(250000);
	return SDIOH_API_RC_SUCCESS;
}

int sdioh_start(void)
{
	if (!sdio_reset_dev()) 
		return SDIOH_API_RC_FAIL;
    
	return SDIOH_API_RC_SUCCESS;
}

#define SDIOH_API_ACCESS_RETRY_LIMIT	2

struct bcmsdh_info
{
	bool	init_success;	/* underlying driver successfully attached */
	void	*sdioh;		/* handler for sdioh */
	uint32  vendevid;	/* Target Vendor and Device ID on SD bus */
	osl_t   *osh;
	bool	regfail;	/* Save status of last reg_read/reg_write call */
	uint32	sbwad;		/* Save backplane window address */
};

bcmsdh_info_t g_bcmsdh;

bcmsdh_info_t * bcmsdh_get_info(void)
{
    return &g_bcmsdh;
}

bcmsdh_info_t * bcmsdh_init(osl_t *osh, void **regsva)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();

	bzero((char *)bcmsdh, sizeof(bcmsdh_info_t));

	if (!(bcmsdh->sdioh = sdioh_attach(osh))) 
    {
		bcmsdh_detach(osh);
		return NULL;
	}

	bcmsdh->osh = osh;
	bcmsdh->init_success = TRUE;
	bcmsdh->sbwad = SI_ENUM_BASE; /* Report the BAR, to fix if needed */

	*regsva = (uint32 *)SI_ENUM_BASE;
	return bcmsdh;
}

int bcmsdh_detach(osl_t *osh)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();

	if (bcmsdh != NULL) 
    {
		if (bcmsdh->sdioh) 
        {
			sdioh_detach(osh, bcmsdh->sdioh);
			bcmsdh->sdioh = NULL;
		}
	}

	return 0;
}

int bcmsdh_iovar_op(void *sdh, const char *name, void *params, int plen, void *arg, int len, bool set)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	return sdioh_iovar_op(bcmsdh->sdioh, name, params, plen, arg, len, set);
}

int bcmsdh_intr_enable(void *sdh)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	int status;

	ASSERT(bcmsdh);

	status = sdioh_interrupt_set(bcmsdh->sdioh, TRUE);
	return (SDIOH_API_SUCCESS(status) ? 0 : BCME_ERROR);
}

int bcmsdh_intr_disable(void *sdh)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	int status;

	ASSERT(bcmsdh);

	status = sdioh_interrupt_set(bcmsdh->sdioh, FALSE);
	return (SDIOH_API_SUCCESS(status) ? 0 : BCME_ERROR);
}

uint8 bcmsdh_cfg_read(uint fnc_num, uint32 addr, int *err)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
    int status;
    int32 retry = 0;
    uint8 data = 0;

    ASSERT(bcmsdh);
    ASSERT(bcmsdh->init_success);

    do {
        if (retry)              /* wait for 1 ms till bus get settled down */
            OSL_DELAY(1000);
        status = sdioh_cfg_read(fnc_num, addr, (uint8 *) & data);
    } while (!SDIOH_API_SUCCESS(status) && (retry++ < SDIOH_API_ACCESS_RETRY_LIMIT));

    if (err)
        *err = (SDIOH_API_SUCCESS(status) ? 0 : BCME_SDIO_ERROR);

    return data;
}

void bcmsdh_cfg_write(uint fnc_num, uint32 addr, uint8 data, int *err)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	int status;
	int32 retry = 0;

    ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	do {
		if (retry)	OSL_DELAY(1000);
        status = sdioh_cfg_write(fnc_num, addr, (uint8 *)&data);

	} while (status && (retry++ < SDIOH_API_ACCESS_RETRY_LIMIT));

	if (err)
		*err = status ? BCME_SDIO_ERROR : 0;
}

uint32 bcmsdh_cfg_read_word(void *sdh, uint fnc_num, uint32 addr, int *err)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info(); 
	int status;
	uint32 data = 0;

    ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	status = sdioh_request_word(bcmsdh->sdioh, SDIOH_CMD_TYPE_NORMAL, SDIOH_READ, fnc_num, addr, &data, 4);
	if (err)
		*err = (SDIOH_API_SUCCESS(status) ? 0 : BCME_SDIO_ERROR);
	return data;
}

void bcmsdh_cfg_write_word(void *sdh, uint fnc_num, uint32 addr, uint32 data, int *err)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	int status;

    ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	status = sdioh_request_word(bcmsdh->sdioh, SDIOH_CMD_TYPE_NORMAL, SDIOH_WRITE, fnc_num, addr, &data, 4);
	if (err) *err = (SDIOH_API_SUCCESS(status) ? 0 : BCME_SDIO_ERROR);
}

int bcmsdh_cis_read(uint func, uint8 *cis, uint length)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	int status;
	uint8 tmp_buf[SBSDIO_CIS_SIZE_LIMIT], *tmp_ptr;
	uint8 *ptr;
	bool ascii = func & ~0xf;

	func &= 0x7;

    ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);
	ASSERT(cis);
	ASSERT(length <= SBSDIO_CIS_SIZE_LIMIT);

	status = sdioh_cis_read(bcmsdh->sdioh, func, cis, length);
	if (ascii) 
    {
		bcopy(cis, tmp_buf, length);
		for (tmp_ptr = tmp_buf, ptr = cis; ptr < (cis + length - 4); tmp_ptr++) {
			ptr += sprintf((char*)ptr, "%.2x ", *tmp_ptr & 0xff);
			if ((((tmp_ptr - tmp_buf) + 1) & 0xf) == 0)
				ptr += sprintf((char *)ptr, "\n");
		}
	}

	return (SDIOH_API_SUCCESS(status) ? 0 : BCME_ERROR);
}


static int bcmsdhsdio_set_sbaddr_window(void *sdh, uint32 address)
{
	int err = 0;
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_SBADDRLOW, (address >> 8) & SBSDIO_SBADDRLOW_MASK, &err);
	if (!err)
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_SBADDRMID, (address >> 16) & SBSDIO_SBADDRMID_MASK, &err);
	if (!err)
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_SBADDRHIGH, (address >> 24) & SBSDIO_SBADDRHIGH_MASK, &err);

	return err;
}

uint32 bcmsdh_reg_read(void *sdh, uint32 addr, uint size)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	int status;
	uint32 word = 0;
	uint bar0 = addr & ~SBSDIO_SB_OFT_ADDR_MASK;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	if (bar0 != bcmsdh->sbwad) 
    {
		if (bcmsdhsdio_set_sbaddr_window(bcmsdh, bar0))
			return 0xFFFFFFFF;

		bcmsdh->sbwad = bar0;
	}

	addr &= SBSDIO_SB_OFT_ADDR_MASK;
	if (size == 4)
		addr |= SBSDIO_SB_ACCESS_2_4B_FLAG;

	status = sdioh_request_word(bcmsdh->sdioh, SDIOH_CMD_TYPE_NORMAL, SDIOH_READ, SDIO_FUNC_1, addr, &word, size);
	bcmsdh->regfail = !(SDIOH_API_SUCCESS(status));

	/* if ok, return appropriately masked word */
	if (SDIOH_API_SUCCESS(status)) 
    {
		switch (size) {
			case sizeof(uint8):
				return (word & 0xff);
			case sizeof(uint16):
				return (word & 0xffff);
			case sizeof(uint32):
				return word;
			default:
				bcmsdh->regfail = TRUE;
		}
	}

	/* otherwise, bad sdio access or invalid size */
	BCMSDH_ERROR(("%s: error reading addr 0x%04x size %d\n", __FUNCTION__, addr, size));
	return 0xFFFFFFFF;
}

uint32 bcmsdh_reg_write(void *sdh, uint32 addr, uint size, uint32 data)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info(); //(bcmsdh_info_t *)sdh;
	int status;
	uint bar0 = addr & ~SBSDIO_SB_OFT_ADDR_MASK;
	int err = 0;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	if (bar0 != bcmsdh->sbwad) {
		if ((err = bcmsdhsdio_set_sbaddr_window(bcmsdh, bar0)))
			return err;

		bcmsdh->sbwad = bar0;
	}

	addr &= SBSDIO_SB_OFT_ADDR_MASK;
	if (size == 4)
		addr |= SBSDIO_SB_ACCESS_2_4B_FLAG;
	status = sdioh_request_word(bcmsdh->sdioh, SDIOH_CMD_TYPE_NORMAL, SDIOH_WRITE, SDIO_FUNC_1, addr, &data, size);
	bcmsdh->regfail = !(SDIOH_API_SUCCESS(status));

	if (SDIOH_API_SUCCESS(status))
		return 0;

    ASSERT(0);
	return 0xFFFFFFFF;
}

bool bcmsdh_regfail(void *sdh) /* write or read reg failed! */
{
	return ((bcmsdh_info_t *)sdh)->regfail;
}

int bcmsdh_recv_buf(void *sdh, uint32 addr, uint fn, uint flags, uint8 *buf, uint nbytes, void *pkt)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	int status;
	uint incr_fix;
	uint width;
	uint bar0 = addr & ~SBSDIO_SB_OFT_ADDR_MASK;
	int err = 0;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);
	ASSERT(!(flags & SDIO_REQ_ASYNC)); /* Async not implemented yet */

	if (flags & SDIO_REQ_ASYNC)
		return BCME_UNSUPPORTED;

	if (bar0 != bcmsdh->sbwad) 
    {
		if ((err = bcmsdhsdio_set_sbaddr_window(bcmsdh, bar0)))
			return err;

		bcmsdh->sbwad = bar0;
	}

	addr &= SBSDIO_SB_OFT_ADDR_MASK;

	incr_fix = (flags & SDIO_REQ_FIXED) ? SDIOH_DATA_FIX : SDIOH_DATA_INC;
	width = (flags & SDIO_REQ_4BYTE) ? 4 : 2;
	if (width == 4)
		addr |= SBSDIO_SB_ACCESS_2_4B_FLAG;

	status = sdioh_request_buffer(bcmsdh->sdioh, incr_fix, SDIOH_READ, fn, addr, width, nbytes, buf, pkt);
	return (SDIOH_API_SUCCESS(status) ? 0 : BCME_SDIO_ERROR);
}

int bcmsdh_send_buf(void *sdh, uint32 addr, uint fn, uint flags, uint8 *buf, uint nbytes, void *pkt)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();
	int status;
	uint incr_fix;
	uint width;
	uint bar0 = addr & ~SBSDIO_SB_OFT_ADDR_MASK;
	int err = 0;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);
	ASSERT(!(flags & SDIO_REQ_ASYNC)); /* Async not implemented yet */

	if (flags & SDIO_REQ_ASYNC)
		return BCME_UNSUPPORTED;

	if (bar0 != bcmsdh->sbwad) {
		if ((err = bcmsdhsdio_set_sbaddr_window(bcmsdh, bar0)))
			return err;

		bcmsdh->sbwad = bar0;
	}

	addr &= SBSDIO_SB_OFT_ADDR_MASK;

	incr_fix = (flags & SDIO_REQ_FIXED) ? SDIOH_DATA_FIX : SDIOH_DATA_INC;
	width = (flags & SDIO_REQ_4BYTE) ? 4 : 2;
	if (width == 4)
		addr |= SBSDIO_SB_ACCESS_2_4B_FLAG;

	status = sdioh_request_buffer(bcmsdh->sdioh, incr_fix, SDIOH_WRITE, fn, addr, width, nbytes, buf, pkt);

	return (SDIOH_API_SUCCESS(status) ? 0 : BCME_ERROR);
}

int bcmsdh_rwdata(void *sdh, uint rw, uint32 addr, uint8 *buf, uint nbytes)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info(); //(bcmsdh_info_t *)sdh;
	int status;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);
	ASSERT((addr & SBSDIO_SBWINDOW_MASK) == 0);

	addr &= SBSDIO_SB_OFT_ADDR_MASK;
	addr |= SBSDIO_SB_ACCESS_2_4B_FLAG;

	status = sdioh_request_buffer(bcmsdh->sdioh, SDIOH_DATA_INC, (rw ? SDIOH_WRITE : SDIOH_READ), SDIO_FUNC_1, addr, 4, nbytes, buf, NULL);

	return (SDIOH_API_SUCCESS(status) ? 0 : BCME_ERROR);
}

int bcmsdh_abort(void *sdh, uint fn)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;

	return sdioh_abort(fn);
}

uint bcmsdh_query_iofnum(void)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();

	ASSERT(bcmsdh);
	return sdioh_query_iofnum(bcmsdh->sdioh);
}

uint32 bcmsdh_cur_sbwad(void *sdh)
{
	bcmsdh_info_t *bcmsdh = bcmsdh_get_info();

	ASSERT(bcmsdh);
	return (bcmsdh->sbwad);
}

bcmsdh_info_t *g_sdh=NULL;

void bcmsdh_remove(osl_t *osh, void *instance)
{
	if (instance)
        dhdsdio_disconnect(instance);

	if (g_sdh)
		bcmsdh_detach(osh);
}

void* bcmsdh_probe(osl_t *osh)
{
	void	*bus = NULL;
	void	*regsva;

	g_sdh = bcmsdh_init(osh, &regsva); /* para init & get cis */
	if (g_sdh == NULL) goto err;

    bus = dhdsdio_probe(regsva, osh, g_sdh);
	if (bus) return (bus);

err:
	bcmsdh_remove(osh, bus);
	return (NULL);
}

