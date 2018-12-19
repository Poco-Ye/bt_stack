/*
 * Misc utility routines for accessing chip-specific features
 * of the SiliconBackplane-based Broadcom chips.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: siutils.c,v 1.813.2.36 2011-02-10 23:43:55 Exp $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <pcicfg.h>
#include <sbpcmcia.h>
#include <sbsocram.h>
#include <bcmsdh.h>
#include <sdio.h>
#include <sbsdio.h>
#include <sbhnddma.h>
#include <sbsdpcmdev.h>
#include <bcmsdpcm.h>
#include <hndpmu.h>
#include "generic_osl.h"
#include "siutils_priv.h"

/* local prototypes */
static si_info_t *si_doattach(si_info_t *sii, osl_t *osh, void *regs, uint bustype, void *sdh, char **vars, uint *varsz);
static bool si_buscore_setup(si_info_t *sii, chipcregs_t *cc, uint bustype, uint32 savewin, uint *origidx, void *regs);

static uint32 get_erom_ent(si_t *sih, uint32 **eromptr, uint32 mask, uint32 match)
{
	uint32 ent;
	uint inv = 0, nom = 0;

	while (TRUE) {
		ent = R_REG(si_osh(sih), *eromptr);
		(*eromptr)++;

		if (mask == 0)
			break;

		if ((ent & ER_VALID) == 0) {
			inv++;
			continue;
		}

		if (ent == (ER_END | ER_VALID))
			break;

		if ((ent & mask) == match)
			break;

		nom++;
	}

	return ent;
}

static uint32 get_asd(si_t *sih, uint32 **eromptr, uint sp, uint ad, uint st, uint32 *addrl, uint32 *addrh, uint32 *sizel, uint32 *sizeh)
{
	uint32 asd, sz, szd;

	asd = get_erom_ent(sih, eromptr, ER_VALID, ER_VALID);
	if (((asd & ER_TAG1) != ER_ADD) ||
	    (((asd & AD_SP_MASK) >> AD_SP_SHIFT) != sp) ||
	    ((asd & AD_ST_MASK) != st)) {
		/* This is not what we want, "push" it back */
		(*eromptr)--;
		return 0;
	}

	*addrl = asd & AD_ADDR_MASK;
	if (asd & AD_AG32)
		*addrh = get_erom_ent(sih, eromptr, 0, 0);
	else
		*addrh = 0;

	*sizeh = 0;
	sz = asd & AD_SZ_MASK;
	if (sz == AD_SZ_SZD) 
    {
		szd = get_erom_ent(sih, eromptr, 0, 0);
		*sizel = szd & SD_SZ_MASK;
		if (szd & SD_SG32)
			*sizeh = get_erom_ent(sih, eromptr, 0, 0);
	} 
    else
		*sizel = AD_SZ_BASE << (sz >> AD_SZ_SHIFT);

	return asd;
}

/* parse the enumeration rom to identify all cores */
void ai_scan(si_t *sih, void *regs)
{
	si_info_t *sii = SI_INFO(sih);
	chipcregs_t *cc = (chipcregs_t *)regs;
	uint32 erombase, *eromptr, *eromlim;

	erombase = R_REG(sii->osh, &cc->eromptr);
    eromptr = (uint32 *)(uintptr)erombase;
	eromlim = eromptr + (ER_REMAPCONTROL / sizeof(uint32));

	while (eromptr < eromlim) {
		uint32 cia, cib, cid, mfg, crev, nmw, nsw, nmp, nsp;
		uint32 mpd, asd, addrl, addrh, sizel, sizeh;
		uint32 *base;
		uint i, j, idx;
		bool br;

		br = FALSE;

		/* Grok a component */
		cia = get_erom_ent(sih, &eromptr, ER_TAG, ER_CI);
		if (cia == (ER_END | ER_VALID)) {
			SI_VMSG(("Found END of erom after %d cores\n", sii->numcores));
			return;
		}

		base = eromptr - 1;
		cib = get_erom_ent(sih, &eromptr, 0, 0);

		if ((cib & ER_TAG) != ER_CI) {
			SI_ERROR(("CIA not followed by CIB\n"));
			goto error;
		}

		cid = (cia & CIA_CID_MASK) >> CIA_CID_SHIFT;
		mfg = (cia & CIA_MFG_MASK) >> CIA_MFG_SHIFT;
		crev = (cib & CIB_REV_MASK) >> CIB_REV_SHIFT;
		nmw = (cib & CIB_NMW_MASK) >> CIB_NMW_SHIFT;
		nsw = (cib & CIB_NSW_MASK) >> CIB_NSW_SHIFT;
		nmp = (cib & CIB_NMP_MASK) >> CIB_NMP_SHIFT;
		nsp = (cib & CIB_NSP_MASK) >> CIB_NSP_SHIFT;

		if (((mfg == MFGID_ARM) && (cid == DEF_AI_COMP)) || (nsp == 0))
			continue;
		if ((nmw + nsw == 0)) {
			/* A component which is not a core */
			if (cid == OOB_ROUTER_CORE_ID) {
				asd = get_asd(sih, &eromptr, 0, 0, AD_ST_SLAVE,
					&addrl, &addrh, &sizel, &sizeh);
				if (asd != 0) {
					sii->oob_router = addrl;
				}
			}
			continue;
		}

		idx = sii->numcores;
/*		sii->eromptr[idx] = base; */
		sii->cia[idx] = cia;
		sii->cib[idx] = cib;
		sii->coreid[idx] = cid;

		for (i = 0; i < nmp; i++) {
			mpd = get_erom_ent(sih, &eromptr, ER_VALID, ER_VALID);
			if ((mpd & ER_TAG) != ER_MP) {
				SI_ERROR(("Not enough MP entries for component 0x%x\n", cid));
				goto error;
			}
		}
			
		/* First Slave Address Descriptor should be port 0:
		 * the main register space for the core
		 */
		asd = get_asd(sih, &eromptr, 0, 0, AD_ST_SLAVE, &addrl, &addrh, &sizel, &sizeh);
		if (asd == 0) {
			/* Try again to see if it is a bridge */
			asd = get_asd(sih, &eromptr, 0, 0, AD_ST_BRIDGE, &addrl, &addrh,
			              &sizel, &sizeh);
			if (asd != 0)
				br = TRUE;
			else
				if ((addrh != 0) || (sizeh != 0) || (sizel != SI_CORE_SIZE)) {
					SI_ERROR(("First Slave ASD for core 0x%04x malformed " "(0x%08x)\n", cid, asd));
					goto error;
				}
		}
		sii->coresba[idx] = addrl;
		sii->coresba_size[idx] = sizel;
		/* Get any more ASDs in port 0 */
		j = 1;
		do {
			asd = get_asd(sih, &eromptr, 0, j, AD_ST_SLAVE, &addrl, &addrh,
			              &sizel, &sizeh);
			if ((asd != 0) && (j == 1) && (sizel == SI_CORE_SIZE)) {
				sii->coresba2[idx] = addrl;
				sii->coresba2_size[idx] = sizel;
			}
			j++;
		} while (asd != 0);

		/* Go through the ASDs for other slave ports */
		for (i = 1; i < nsp; i++) {
			j = 0;
			do {
				asd = get_asd(sih, &eromptr, i, j++, AD_ST_SLAVE, &addrl, &addrh,
				              &sizel, &sizeh);
			} while (asd != 0);
			if (j == 0) {
				SI_ERROR((" SP %d has no address descriptors\n", i));
				goto error;
			}
		}

		/* Now get master wrappers */
		for (i = 0; i < nmw; i++) {
			asd = get_asd(sih, &eromptr, i, 0, AD_ST_MWRAP, &addrl, &addrh,
			              &sizel, &sizeh);
			if (asd == 0) {
				SI_ERROR(("Missing descriptor for MW %d\n", i));
				goto error;
			}
			if ((sizeh != 0) || (sizel != SI_CORE_SIZE)) {
				SI_ERROR(("Master wrapper %d is not 4KB\n", i));
				goto error;
			}
			if (i == 0)
				sii->wrapba[idx] = addrl;
		}

		/* And finally slave wrappers */
		for (i = 0; i < nsw; i++) {
			uint fwp = (nsp == 1) ? 0 : 1;
			asd = get_asd(sih, &eromptr, fwp + i, 0, AD_ST_SWRAP, &addrl, &addrh,
			              &sizel, &sizeh);
			if (asd == 0) {
				SI_ERROR(("Missing descriptor for SW %d\n", i));
				goto error;
			}
			if ((sizeh != 0) || (sizel != SI_CORE_SIZE)) {
				SI_ERROR(("Slave wrapper %d is not 4KB\n", i));
				goto error;
			}
			if ((nmw == 0) && (i == 0))
				sii->wrapba[idx] = addrl;
		}

		/* Don't record bridges */
		if (br)
			continue;

		/* Done with core */
		sii->numcores++;
	}

	SI_ERROR(("Reached end of erom without finding END"));

error:
	sii->numcores = 0;
	return;
}

/* This function changes the logical "focus" to the indicated core.
 * Return the current core's virtual address.
 */
void * ai_setcoreidx(si_t *sih, uint coreidx)
{
	si_info_t *sii = SI_INFO(sih);
	uint32 addr = sii->coresba[coreidx];
	uint32 wrap = sii->wrapba[coreidx];
	void *regs;

	if (coreidx >= sii->numcores)
		return (NULL);

	/*
	 * If the user has provided an interrupt mask enabled function,
	 * then assert interrupts are disabled before switching the core.
	 */
	ASSERT((sii->intrsenabled_fn == NULL) || !(*(sii)->intrsenabled_fn)((sii)->intr_arg));

    sii->curmap = regs = (void *)((uintptr)addr);
    sii->curwrap = (void *)((uintptr)wrap);
	sii->curmap = regs;
	sii->curidx = coreidx;

	return regs;
}

uint ai_flag(si_t *sih)
{
	si_info_t *sii;
	aidmp_t *ai;

	sii = SI_INFO(sih);
	ai = sii->curwrap;

	return (R_REG(sii->osh, &ai->oobselouta30) & 0x1f);
}

uint ai_corerev(si_t *sih)
{
	si_info_t *sii;
	uint32 cib;

	sii = SI_INFO(sih);
	cib = sii->cib[sii->curidx];
	return ((cib & CIB_REV_MASK) >> CIB_REV_SHIFT);
}

bool ai_iscoreup(si_t *sih)
{
	si_info_t *sii;
	aidmp_t *ai;

	sii = SI_INFO(sih);
	ai = sii->curwrap;

	return (((R_REG(sii->osh, &ai->ioctrl) & (SICF_FGC | SICF_CLOCK_EN)) == SICF_CLOCK_EN) &&
	        ((R_REG(sii->osh, &ai->resetctrl) & AIRC_RESET) == 0));
}

/*
 * Switch to 'coreidx', issue a single arbitrary 32bit register mask&set operation,
 * switch back to the original core, and return the new value.
 *
 * When using the silicon backplane, no fiddling with interrupts or core switches is needed.
 *
 * Also, when using pci/pcie, we can optimize away the core switching for pci registers
 * and (on newer pci cores) chipcommon registers.
 */
uint ai_corereg(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	uint origidx = 0;
	uint32 *r = NULL;
	uint w;
	uint intr_val = 0;
	bool fast = FALSE;
	si_info_t *sii;

	sii = SI_INFO(sih);

	ASSERT(GOODIDX(coreidx));
	ASSERT(regoff < SI_CORE_SIZE);
	ASSERT((val & ~mask) == 0);

	if (coreidx >= SI_MAXCORES)
		return 0;

	if (BUSTYPE(sih->bustype) == SI_BUS) {
		/* If internal bus, we can always get at everything */
		fast = TRUE;
		/* map if does not exist */
		if (!sii->regs[coreidx]) {
			sii->regs[coreidx] = REG_MAP(sii->coresba[coreidx],
			                            SI_CORE_SIZE);
			ASSERT(GOODREGS(sii->regs[coreidx]));
		}
		r = (uint32 *)((uchar *)sii->regs[coreidx] + regoff);
	} else if (BUSTYPE(sih->bustype) == PCI_BUS) {
		/* If pci/pcie, we can get at pci/pcie regs and on newer cores to chipc */

		if ((sii->coreid[coreidx] == CC_CORE_ID) && SI_FAST(sii)) {
			/* Chipc registers are mapped at 12KB */

			fast = TRUE;
			r = (uint32 *)((char *)sii->curmap + PCI_16KB0_CCREGS_OFFSET + regoff);
		} else if (sii->pub.buscoreidx == coreidx) {
			/* pci registers are at either in the last 2KB of an 8KB window
			 * or, in pcie and pci rev 13 at 8KB
			 */
			fast = TRUE;
			if (SI_FAST(sii))
				r = (uint32 *)((char *)sii->curmap +
				               PCI_16KB0_PCIREGS_OFFSET + regoff);
			else
				r = (uint32 *)((char *)sii->curmap +
				               ((regoff >= SBCONFIGOFF) ?
				                PCI_BAR0_PCISBR_OFFSET : PCI_BAR0_PCIREGS_OFFSET) +
				               regoff);
		}
	}

	if (!fast) {
		INTR_OFF(sii, intr_val);

		/* save current core index */
		origidx = si_coreidx(&sii->pub);

		/* switch core */
		r = (uint32*) ((uchar*) ai_setcoreidx(&sii->pub, coreidx) + regoff);
	}
	ASSERT(r != NULL);

	/* mask and set */
	if (mask || val) {
		w = (R_REG(sii->osh, r) & ~mask) | val;
		W_REG(sii->osh, r, w);
	}

	/* readback */
	w = R_REG(sii->osh, r);

	if (!fast) {
		/* restore core index */
		if (origidx != coreidx)
			ai_setcoreidx(&sii->pub, origidx);

		INTR_RESTORE(sii, intr_val);
	}

	return (w);
}

void ai_core_disable(si_t *sih, uint32 bits)
{
	si_info_t *sii;
	volatile uint32 dummy;
	aidmp_t *ai;

	sii = SI_INFO(sih);

	ASSERT(GOODREGS(sii->curwrap));
	ai = sii->curwrap;

	/* if core is already in reset, just return */
	if (R_REG(sii->osh, &ai->resetctrl) & AIRC_RESET)
		return;

	W_REG(sii->osh, &ai->ioctrl, bits);
	dummy = R_REG(sii->osh, &ai->ioctrl);
	OSL_DELAY(10);

	W_REG(sii->osh, &ai->resetctrl, AIRC_RESET);
	OSL_DELAY(1);
}

/* reset and re-enable a core
 * inputs:
 * bits - core specific bits that are set during and after reset sequence
 * resetbits - core specific bits that are set only during reset sequence
 */
void ai_core_reset(si_t *sih, uint32 bits, uint32 resetbits)
{
	si_info_t *sii;
	aidmp_t *ai;
	volatile uint32 dummy;

	sii = SI_INFO(sih);
	ASSERT(GOODREGS(sii->curwrap));
	ai = sii->curwrap;

	/*
	 * Must do the disable sequence first to work for arbitrary current core state.
	 */
	ai_core_disable(sih, (bits | resetbits));

	/*
	 * Now do the initialization sequence.
	 */
	W_REG(sii->osh, &ai->ioctrl, (bits | SICF_FGC | SICF_CLOCK_EN));
	dummy = R_REG(sii->osh, &ai->ioctrl);
	W_REG(sii->osh, &ai->resetctrl, 0);
	OSL_DELAY(1);

	W_REG(sii->osh, &ai->ioctrl, (bits | SICF_CLOCK_EN));
	dummy = R_REG(sii->osh, &ai->ioctrl);
	OSL_DELAY(1);
}

si_info_t g_sii;
/* global flag to prevent shared resources from being initialized multiple times in si_attach() */
/*
 * Allocate a si handle.
 * devid - pci device id (used to determine chip#)
 * osh - opaque OS handle
 * regs - virtual address of initial core registers
 * bustype - pci/pcmcia/sb/sdio/etc
 * vars - pointer to a pointer area for "environment" variables
 * varsz - pointer to int to return the size of the vars
 */
si_t * si_attach(osl_t *osh, void *regs, uint bustype, void *sdh, char **vars, uint *varsz)
{
	si_info_t *sii=&g_sii;

	if (si_doattach(sii, osh, regs, bustype, sdh, vars, varsz) == NULL)
		return (NULL);

	sii->vars = vars ? *vars : NULL;
	sii->varsz = varsz ? *varsz : 0;

	return (si_t *)sii;
}

/* need to set memseg flag for CF card first before any sb registers access */
static bool si_buscore_prep(void)
{
    uint8 clkset;
    uint8 clkval;
    int   err;

    /* Try forcing SDIO core to do ALPAvail request only */
    clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ;
    bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, clkset, &err);

    if (0 == err) 
    {
        /* If register supported, wait for ALPAvail and then force ALP */
        clkval = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, NULL);
        if ((clkval & ~SBSDIO_AVBITS) == clkset) 
        {
            SPIN_WAIT(((clkval = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, NULL)), !SBSDIO_ALPAV(clkval)), PMU_MAX_TRANSITION_DLY);
            if (!SBSDIO_ALPAV(clkval)) 
            {
                ASSERT(0);
                return FALSE;
            }

            clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP;
            bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, clkset, &err);
            OSL_DELAY(65);
        }
    }

    /* Also, disable the extra SDIO pull-ups */
    bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_SDIOPULLUP, 0, NULL);
	return TRUE;
}

static bool si_buscore_setup(si_info_t *sii, chipcregs_t *cc, uint bustype, uint32 savewin, uint *origidx, void *regs)
{
	bool pci, pcie;
	uint i;
	uint pciidx, pcieidx, pcirev, pcierev;

	cc = si_setcoreidx(&sii->pub, SI_CC_IDX);
	ASSERT((uintptr)cc);

	/* get chipcommon rev */
	sii->pub.ccrev = (int)si_corerev(&sii->pub);

	/* get chipcommon chipstatus */
	if (sii->pub.ccrev >= 11)
		sii->pub.chipst = R_REG(sii->osh, &cc->chipstatus);

	/* get chipcommon capabilites */
	sii->pub.cccaps = R_REG(sii->osh, &cc->capabilities);

	/* get chipcommon extended capabilities */
	if (sii->pub.ccrev >= 35)
		sii->pub.cccaps_ext = R_REG(sii->osh, &cc->capabilities_ext);

	/* get pmu rev and caps */
	if (sii->pub.cccaps & CC_CAP_PMU) {
		sii->pub.pmucaps = R_REG(sii->osh, &cc->pmucapabilities);
		sii->pub.pmurev = sii->pub.pmucaps & PCAP_REV_MASK;
	}

	/* figure out bus/orignal core idx */
	sii->pub.buscoretype = NODEV_CORE_ID;
	sii->pub.buscorerev = NOREV;
	sii->pub.buscoreidx = BADIDX;

	pci = pcie = FALSE;
	pcirev = pcierev = NOREV;
	pciidx = pcieidx = BADIDX;

	for (i = 0; i < sii->numcores; i++) {
		uint cid, crev;

		si_setcoreidx(&sii->pub, i);
		cid = si_coreid(&sii->pub);
		crev = si_corerev(&sii->pub);

		if ((BUSTYPE(bustype) == SDIO_BUS) && ((cid == PCMCIA_CORE_ID) || (cid == SDIOD_CORE_ID))) {
			sii->pub.buscorerev = crev;
			sii->pub.buscoretype = cid;
			sii->pub.buscoreidx = i;
		}

		/* find the core idx before entering this func. */
		if ((savewin && (savewin == sii->coresba[i])) ||
		    (regs == sii->regs[i]))
			*origidx = i;
	}

	if (pci) {
		sii->pub.buscoretype = PCI_CORE_ID;
		sii->pub.buscorerev = pcirev;
		sii->pub.buscoreidx = pciidx;
	} else if (pcie) {
		sii->pub.buscoretype = PCIE_CORE_ID;
		sii->pub.buscorerev = pcierev;
		sii->pub.buscoreidx = pcieidx;
	}

	/* Make sure any on-chip ARM is off (in case strapping is wrong), or downloaded code was
	 * already running.
	 */
    if (si_setcore(&sii->pub, ARM7S_CORE_ID, 0) ||
        si_setcore(&sii->pub, ARMCM3_CORE_ID, 0))
        si_core_disable(&sii->pub, 0);

	/* return to the original core */
	si_setcoreidx(&sii->pub, *origidx);
	return TRUE;
}

static si_info_t * si_doattach(si_info_t *sii, osl_t *osh, void *regs, uint bustype, void *sdh, char **vars, uint *varsz)
{
	struct si_pub *sih = &sii->pub;
	uint32 w, savewin;
	chipcregs_t *cc;
	char *pvars = NULL;
	uint origidx;

	ASSERT(GOODREGS(regs));

	bzero((uchar*)sii, sizeof(si_info_t));

	savewin = 0;
	sih->buscoreidx = BADIDX;
	sii->curmap = regs;
	sii->sdh = sdh;
	sii->osh = osh;

    cc = (chipcregs_t *)sii->curmap;
	sih->bustype = bustype;
	
	/* bus/core/clk setup for register access */
	if (!si_buscore_prep()) 
		return NULL;

	/* ChipID recognition.
	 *   We assume we can read chipid at offset 0 from the regs arg.
	 *   If we add other chiptypes (or if we need to support old sdio hosts w/o chipcommon),
	 *   some way of recognizing them needs to be added here.
	 */
	w = R_REG(osh, &cc->chipid);

	sih->chip  = w & CID_ID_MASK;    /* Might as wll fill in chip id rev & pkg */
	sih->socitype = (w & CID_TYPE_MASK) >> CID_TYPE_SHIFT;
	sih->chiprev  = (w & CID_REV_MASK) >> CID_REV_SHIFT;
	sih->chippkg  = (w & CID_PKG_MASK) >> CID_PKG_SHIFT;

	if (CHIPID(sih->chip) == BCM4322_CHIP_ID && (((sih->chipst & CST4322_SPROM_OTP_SEL_MASK)
		>> CST4322_SPROM_OTP_SEL_SHIFT) == (CST4322_OTP_PRESENT | CST4322_SPROM_PRESENT))) {
        ASSERT(0);
		return NULL;
	}

	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && (sih->chiprev == 0) &&
		(sih->chippkg != BCM4329_289PIN_PKG_ID)) {
		sih->chippkg = BCM4329_182PIN_PKG_ID;
	}

	sih->issim = IS_SIM(sih->chippkg);
    ASSERT(CHIPTYPE(sii->pub.socitype) == SOCI_AI);

    ai_scan(&sii->pub, (void *)(uintptr)cc);

	if (sii->numcores == 0) 
    { /* no cores found, bail out */
        ASSERT(0);
		return NULL;
	}

	/* bus/core/clk setup */
	origidx = SI_CC_IDX;
    
	if (!si_buscore_setup(sii, cc, bustype, savewin, &origidx, regs)) {
		SI_ERROR(("si_doattach: si_buscore_setup failed\n"));
		goto exit;
	}

	/* assume current core is CC */
	if ((sii->pub.ccrev == 0x25) && ((CHIPID(sih->chip) == BCM43236_CHIP_ID ||
	                                  CHIPID(sih->chip) == BCM43235_CHIP_ID ||
	                                  CHIPID(sih->chip) == BCM43238_CHIP_ID) &&
	                                 (CHIPREV(sii->pub.chiprev) == 0))) {

		if ((cc->chipstatus & CST43236_BP_CLK) != 0) {
			uint clkdiv;
			clkdiv = R_REG(osh, &cc->clkdiv);
			/* otp_clk_div is even number, 120/14 < 9mhz */
			clkdiv = (clkdiv & ~CLKD_OTP) | (14 << CLKD_OTP_SHIFT);
			W_REG(osh, &cc->clkdiv, clkdiv);
			SI_ERROR(("%s: set clkdiv to %x\n", __FUNCTION__, clkdiv));
		}
		OSL_DELAY(10);
	}

	pvars = NULL;

    if (sii->pub.ccrev >= 20) 
    {
        cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
        ASSERT(cc != NULL);
        W_REG(osh, &cc->gpiopullup, 0);
        W_REG(osh, &cc->gpiopulldown, 0);
        si_setcoreidx(sih, origidx);
    }

	return (sii);

exit:
	return NULL;
}

/* may be called with core in reset */
void si_detach(si_t *sih)
{
    si_info_t *sii;
    uint idx;

    sii = SI_INFO(sih);
    if (sii == NULL) return;

    if (BUSTYPE(sih->bustype) == SI_BUS)
    {
        for (idx = 0; idx < SI_MAXCORES; idx++)
            if (sii->regs[idx])
            {
                REG_UNMAP(sii->regs[idx]);
                sii->regs[idx] = NULL;
            }
    }
}

void * si_osh(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->osh;
}

uint si_flag(si_t *sih)
{
    return ai_flag(sih);
}

uint si_coreid(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->coreid[sii->curidx];
}

uint si_coreidx(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->curidx;
}

uint si_corerev(si_t *sih)
{
    return ai_corerev(sih);
}

/* return index of coreid or BADIDX if not found */
uint si_findcoreidx(si_t *sih, uint coreid, uint coreunit)
{
	si_info_t *sii;
	uint found;
	uint i;

	sii = SI_INFO(sih);

	found = 0;

	for (i = 0; i < sii->numcores; i++)
		if (sii->coreid[i] == coreid) {
			if (found == coreunit)
				return (i);
			found++;
		}

	return (BADIDX);
}

/*
 * This function changes logical "focus" to the indicated core;
 * must be called with interrupts off.
 * Moreover, callers should keep interrupts off during switching out of and back to d11 core
 */
void * si_setcore(si_t *sih, uint coreid, uint coreunit)
{
	uint idx;

	idx = si_findcoreidx(sih, coreid, coreunit);
	if (!GOODIDX(idx))
		return (NULL);

    return ai_setcoreidx(sih, idx);
}

void * si_setcoreidx(si_t *sih, uint coreidx)
{
    return ai_setcoreidx(sih, coreidx);
}

/* Turn off interrupt as required by sb_setcore, before switch core */
void * si_switch_core(si_t *sih, uint coreid, uint *origidx, uint *intr_val)
{
	void *cc;
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (SI_FAST(sii)) {
		/* Overloading the origidx variable to remember the coreid,
		 * this works because the core ids cannot be confused with
		 * core indices.
		 */
		*origidx = coreid;
		if (coreid == CC_CORE_ID)
			return (void *)CCREGS_FAST(sii);
		else if (coreid == sih->buscoretype)
			return (void *)PCIEREGS(sii);
	}
	INTR_OFF(sii, *intr_val);
	*origidx = sii->curidx;
	cc = si_setcore(sih, coreid, 0);
	ASSERT(cc != NULL);

	return cc;
}

/* restore coreidx and restore interrupt */
void si_restore_core(si_t *sih, uint coreid, uint intr_val)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	if (SI_FAST(sii) && ((coreid == CC_CORE_ID) || (coreid == sih->buscoretype)))
		return;

	si_setcoreidx(sih, coreid);
	INTR_RESTORE(sii, intr_val);
}

bool si_iscoreup(si_t *sih)
{
    return ai_iscoreup(sih);
}

uint si_corereg(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
    return ai_corereg(sih, coreidx, regoff, mask, val);
}

void si_core_disable(si_t *sih, uint32 bits)
{
    ai_core_disable(sih, bits);
}

void si_core_reset(si_t *sih, uint32 bits, uint32 resetbits)
{
    ai_core_reset(sih, bits, resetbits);
}

static uint32 BCMINITFN(factor6)(uint32 x)
{
	switch (x) {
	case CC_F6_2:	return 2;
	case CC_F6_3:	return 3;
	case CC_F6_4:	return 4;
	case CC_F6_5:	return 5;
	case CC_F6_6:	return 6;
	case CC_F6_7:	return 7;
	default:	return 0;
	}
}

/* set chip watchdog reset timer to fire in 'ticks' */
void si_watchdog(si_t *sih, uint ticks)
{
	uint nb, maxt;

	if (PMUCTL_ENAB(sih)) {

		if ((CHIPID(sih->chip) == BCM4319_CHIP_ID) &&
		    (CHIPREV(sih->chiprev) == 0) && (ticks != 0)) {
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, clk_ctl_st), ~0, 0x2);
			si_setcore(sih, USB20D_CORE_ID, 0);
			si_core_disable(sih, 1);
			si_setcore(sih, CC_CORE_ID, 0);
		}

		nb = (sih->ccrev < 26) ? 16 : ((sih->ccrev >= 37) ? 32 : 24);
		/* The mips compiler uses the sllv instruction,
		 * so we specially handle the 32-bit case.
		 */
		if (nb == 32)
			maxt = 0xffffffff;
		else
			maxt = ((1 << nb) - 1);

		if (ticks == 1)
			ticks = 2;
		else if (ticks > maxt)
			ticks = maxt;

		si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, pmuwatchdog), ~0, ticks);
	} else {
		maxt = (1 << 28) - 1;
		if (ticks > maxt)
			ticks = maxt;

		si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), ~0, ticks);
	}
}

/* Return the size of the specified SOCRAM bank */
static uint socram_banksize(si_info_t *sii, sbsocramregs_t *regs, uint8 index, uint8 mem_type)
{
	uint banksize, bankinfo;
	uint bankidx = index | (mem_type << SOCRAM_BANKIDX_MEMTYPE_SHIFT);

	ASSERT(mem_type <= SOCRAM_MEMTYPE_DEVRAM);

	W_REG(sii->osh, &regs->bankidx, bankidx);
	bankinfo = R_REG(sii->osh, &regs->bankinfo);
	banksize = SOCRAM_BANKINFO_SZBASE * ((bankinfo & SOCRAM_BANKINFO_SZMASK) + 1);
	return banksize;
}

/* Return the RAM size of the SOCRAM core */
uint32 si_socram_size(si_t *sih)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;

	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;
	uint32 coreinfo;
	uint memsize = 0;

	sii = SI_INFO(sih);

	/* Block ints and save current core */
	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);

	corerev = si_corerev(sih);
	coreinfo = R_REG(sii->osh, &regs->coreinfo);

	/* Calculate size from coreinfo based on rev */
	if (corerev == 0)
		memsize = 1 << (16 + (coreinfo & SRCI_MS0_MASK));
	else if (corerev < 3) {
		memsize = 1 << (SR_BSZ_BASE + (coreinfo & SRCI_SRBSZ_MASK));
		memsize *= (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
	} else if ((corerev <= 7) || (corerev == 12)) {
		uint nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		uint bsz = (coreinfo & SRCI_SRBSZ_MASK);
		uint lss = (coreinfo & SRCI_LSS_MASK) >> SRCI_LSS_SHIFT;
		if (lss != 0)
			nb --;
		memsize = nb * (1 << (bsz + SR_BSZ_BASE));
		if (lss != 0)
			memsize += (1 << ((lss - 1) + SR_BSZ_BASE));
	} else {
		uint8 i;
		uint nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		for (i = 0; i < nb; i++)
			memsize += socram_banksize(sii, regs, i, SOCRAM_MEMTYPE_RAM);
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, intr_val);

	return memsize;
}

typedef struct {
	uint8 strength;			/* Pad Drive Strength in mA */
	uint8 sel;			/* Chip-specific select value */
} sdiod_drive_str_t;

/* SDIO Drive Strength to sel value table for PMU Rev 1 */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab1)[] = {
	{4, 0x2},
	{2, 0x3},
	{1, 0x0},
	{0, 0x0} };

/* SDIO Drive Strength to sel value table for PMU Rev 2, 3 */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab2)[] = {
	{12, 0x7},
	{10, 0x6},
	{8, 0x5},
	{6, 0x4},
	{4, 0x2},
	{2, 0x1},
	{0, 0x0} };

/* SDIO Drive Strength to sel value table for PMU Rev 8 (1.8V) */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab3)[] = {
	{32, 0x7},
	{26, 0x6},
	{22, 0x5},
	{16, 0x4},
	{12, 0x3},
	{8, 0x2},
	{4, 0x1},
	{0, 0x0} };

/* SDIO Drive Strength to sel value table for PMU Rev 11 (1.8v) */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab4_1v8)[] = {
	{32, 0x6},
	{26, 0x7},
	{22, 0x4},
	{16, 0x5},
	{12, 0x2},
	{8, 0x3},
	{4, 0x0},
	{0, 0x1} };

/* SDIO Drive Strength to sel value table for PMU Rev 13 (1.8v) */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab5_1v8)[] = {
	{6, 0x7},
	{5, 0x6},
	{4, 0x5},
	{3, 0x4},
	{2, 0x2},
	{1, 0x1},
	{0, 0x0} };

#define SDIOD_DRVSTR_KEY(chip, pmu)	(((chip) << 16) | (pmu))

void si_sdiod_drive_strength_init(si_t *sih, osl_t *osh, uint32 drivestrength)
{
	chipcregs_t *cc;
	uint origidx, intr_val = 0;
	sdiod_drive_str_t *str_tab = NULL;
	uint32 str_mask = 0;
	uint32 str_shift = 0;

	if (!(sih->cccaps & CC_CAP_PMU)) {
		return;
	}

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *) si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);

	switch (SDIOD_DRVSTR_KEY(sih->chip, sih->pmurev)) {
	case SDIOD_DRVSTR_KEY(BCM4325_CHIP_ID, 1):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab1;
		str_mask = 0x30000000;
		str_shift = 28;
		break;
	case SDIOD_DRVSTR_KEY(BCM4325_CHIP_ID, 2):
	case SDIOD_DRVSTR_KEY(BCM4325_CHIP_ID, 3):
	case SDIOD_DRVSTR_KEY(BCM4315_CHIP_ID, 4):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab2;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BCM4336_CHIP_ID, 8):
	case SDIOD_DRVSTR_KEY(BCM4336_CHIP_ID, 11):
		if (sih->pmurev == 8) {
			str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab3;
		}
		else if (sih->pmurev == 11) {
			str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab4_1v8;
		}
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BCM4330_CHIP_ID, 12):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab4_1v8;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	/* FIXME: Fred signed integer overflow */
	case SDIOD_DRVSTR_KEY((uint32)BCM43362_CHIP_ID, 13):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab5_1v8;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	default:
		break;
	}

	if (str_tab != NULL) {
		uint32 cc_data_temp;
		int i;

		/* Pick the lowest available drive strength equal or greater than the
		 * requested strength.	Drive strength of 0 requests tri-state.
		 */
		for (i = 0; drivestrength < str_tab[i].strength; i++)
			;

		if (i > 0 && drivestrength > str_tab[i].strength)
			i--;

		W_REG(osh, &cc->chipcontrol_addr, 1);
		cc_data_temp = R_REG(osh, &cc->chipcontrol_data);
		cc_data_temp &= ~str_mask;
		cc_data_temp |= str_tab[i].sel << str_shift;
		W_REG(osh, &cc->chipcontrol_data, cc_data_temp);
	}

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
}

