
#include <typedefs.h>
#include "generic_osl.h"
#include <osl.h>
#include <bcmsdh.h>

#include <bcmdefs.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmdevs.h>

#include <siutils.h>
#include <hndpmu.h>
#include <hndsoc.h>
#include <bcmsdpcm.h>
#include <sbchipc.h>
#include <sbhnddma.h>

#include <sdio.h>
#include <sbsdio.h>
#include <sbsdpcmdev.h>
#include <bcmsdpcm.h>

#include <proto/ethernet.h>
#include <proto/802.1d.h>
#include <proto/802.11.h>

#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_bus.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <dhdioctl.h>
#include <sdiovar.h>

#define QLEN		256	/* bulk rx and tx queue lengths */
#define FCHI		(QLEN - 10)
#define FCLOW		(FCHI / 2)
#define PRIOMASK	7
#define TXRETRIES	2	/* # of retries for tx frames */

#define DHD_RXBOUND	1	/* Default for max rx frames in one scheduling */
#define DHD_TXBOUND	8	/* Default for max tx frames in one scheduling */
#define MEMBLOCK	2048		/* Block size used for downloading of dongle image */
#define MAX_NVRAMBUF_SIZE	4096	/* max nvram buf size */
#define MAX_DATA_BUF	(32 * 1024)	/* Must be large enough to hold biggest possible glom */

/* Total length of frame header for dongle protocol */
#define SDPCM_HDRLEN	(SDPCM_FRAMETAG_LEN + SDPCM_SWHEADER_LEN)
#define SDPCM_RESERVE	(SDPCM_HDRLEN + DHD_SDALIGN)

/* Space for header read, limit for data packets */
#ifndef MAX_HDR_READ
#define MAX_HDR_READ	32
#endif
#if !ISPOWEROF2(MAX_HDR_READ)
#error MAX_HDR_READ is not a power of 2!
#endif

#define MAX_RX_DATASZ	2048

/* Maximum milliseconds to wait for F2 to come up */
#define DHD_WAIT_F2RDY	3000

/* Bump up limit on waiting for HT to account for first startup;
 * if the image is doing a CRC calculation before programming the PMU
 * for HT availability, it could take a couple hundred ms more, so
 * max out at a 1 second (1000000us).
 */
#if (PMU_MAX_TRANSITION_DLY <= 1000000)
#undef PMU_MAX_TRANSITION_DLY
#define PMU_MAX_TRANSITION_DLY 1000000
#endif

/* Value for ChipClockCSR during initial setup */
#define DHD_INIT_CLKCTL1	(SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ)
#define DHD_INIT_CLKCTL2	(SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP)

/* Flags for SDH calls */
#define F2SYNC	(SDIO_REQ_4BYTE | SDIO_REQ_FIXED)

//DHD_SPINWAIT_SLEEP_INIT(sdioh_spinwait_sleep);
extern void dhd_wlfc_txcomplete(dhd_pub_t *dhd, void *txp, bool success);

/* Private data for SDIO bus interaction */
typedef struct dhd_bus {
	dhd_pub_t	*dhd;

	bcmsdh_info_t	*sdh;			/* Handle for BCMSDH calls */
	si_t		*sih;			/* Handle for SI calls */
	char		*vars;			/* Variables (from CIS and/or other) */
	uint		varsz;			/* Size of variables buffer */
	uint32		sbaddr;			/* Current SB window pointer (-1, invalid) */

	sdpcmd_regs_t	*regs;			/* Registers for SDIO core */
	uint		sdpcmrev;		/* SDIO core revision */
	uint		armrev;			/* CPU core revision */
	uint		ramrev;			/* SOCRAM core revision */
	uint32		ramsize;		/* Size of RAM in SOCRAM (bytes) */
	uint32		orig_ramsize;		/* Size of RAM in SOCRAM (bytes) */

	uint32		bus;			/* gSPI or SDIO bus */
	uint32		hostintmask;		/* Copy of Host Interrupt Mask */
	uint32		intstatus;		/* Intstatus bits (events) pending */
	bool		dpc_sched;		/* Indicates DPC schedule (intrpt rcvd) */
	bool		fcstate;		/* State of dongle flow-control */

	uint16		cl_devid;		/* cached devid for dhdsdio_probe_attach() */
	const char      *nvram_params;		/* user specified nvram params. */

	uint		blocksize;		/* Block size of SDIO transfers */
	uint		roundup;		/* Max roundup limit */

	struct pktq	txq;			/* Queue length used for flow-control */
    struct pktq rxq;            /* Queue length used for rx flow-control */
	uint8		flowcontrol;		/* per prio flow control bitmask */
	uint8		tx_seq;			/* Transmit sequence number (next) */
	uint8		tx_max;			/* Maximum transmit sequence allowed */

	uint8		hdrbuf[MAX_HDR_READ + DHD_SDALIGN];
	uint8		*rxhdr;			/* Header of current rx frame (in hdrbuf) */
	uint16		nextlen;		/* Next Read Len from last header */
	uint8		rx_seq;			/* Receive sequence number (expected) */
	bool		rxskip;			/* Skip receive (awaiting NAK ACK) */

	void		*glomd;			/* Packet containing glomming descriptor */
	void		*glom;			/* Packet chain for glommed superframe */
	uint		glomerr;		/* Glom packet read errors */

	uint8		*rxbuf;			/* Buffer for receiving control packets */
	uint		rxblen;			/* Allocated length of rxbuf */
	uint8		*rxctl;			/* Aligned pointer into rxbuf */
	uint8		*databuf;		/* Buffer for receiving big glom packet */
	uint8		*dataptr;		/* Aligned pointer into databuf */
	uint		rxlen;			/* Length of valid data in buffer */

	uint8		sdpcm_ver;		/* Bus protocol reported by dongle */

	bool		intr;			/* Use interrupts */
	bool		poll;			/* Use polling */
	bool		ipend;			/* Device interrupt is pending */
	bool		intdis;			/* Interrupts disabled by isr */
	uint 		intrcount;		/* Count of device interrupt callbacks */
	uint		lastintrs;		/* Count as of last watchdog timer */
	uint		spurious;		/* Count of spurious interrupts */
	uint		pollrate;		/* Ticks between device polls */
	uint		polltick;		/* Tick counter */
	uint		pollcnt;		/* Count of active polls */
	uint		regfails;		/* Count of R_REG/W_REG failures */
	uint		clkstate;		/* State of sd and backplane clock(s) */
	bool		activity;		/* Activity flag for clock down */
	int32		idletime;		/* Control for activity timeout */
	int32		idlecount;		/* Activity timeout counter */
	int32		idleclock;		/* How to set bus driver when idle */
	int32		sd_divisor;		/* Speed control to bus driver */
	int32		sd_mode;		/* Mode control to bus driver */
	int32		sd_rxchain;		/* If bcmsdh api accepts PKT chains */
	bool		use_rxchain;		/* If dhd should use PKT chains */
	bool		sleeping;		/* Is SDIO bus sleeping? */
	bool		rxflow_mode;	/* Rx flow control mode */
	bool		rxflow;			/* Is rx flow control on */
	uint		prev_rxlim_hit;		/* Is prev rx limit exceeded (per dpc schedule) */
	bool		alp_only;		/* Don't use HT clock (ALP only) */
	/* Field to decide if rx of control frames happen in rxbuf or lb-pool */
	bool		usebufpool;

	/* Some additional counters */
	uint		tx_sderrs;		/* Count of tx attempts with sd errors */
	uint		fcqueued;		/* Tx packets that got queued */
	uint		rxrtx;			/* Count of rtx requests (NAK to dongle) */
	uint		rx_toolong;		/* Receive frames too long to receive */
	uint		rxc_errors;		/* SDIO errors when reading control frames */
	uint		rx_hdrfail;		/* SDIO errors on header reads */
	uint		rx_badhdr;		/* Bad received headers (roosync?) */
	uint		rx_badseq;		/* Mismatched rx sequence number */
	uint		fc_rcvd;		/* Number of flow-control events received */
	uint		fc_xoff;		/* Number which turned on flow-control */
	uint		fc_xon;			/* Number which turned off flow-control */
	uint		rxglomfail;		/* Failed deglom attempts */
	uint		rxglomframes;		/* Number of glom frames (superframes) */
	uint		rxglompkts;		/* Number of packets from glom frames */
	uint		f2rxhdrs;		/* Number of header reads */
	uint		f2rxdata;		/* Number of frame data reads */
	uint		f2txdata;		/* Number of f2 frame writes */
	uint		f1regdata;		/* Number of f1 register accesses */
	uint8		*ctrl_frame_buf;
	uint32		ctrl_frame_len;
	bool		ctrl_frame_stat;
	uint32		rxint_mode;	/* rx interrupt mode */
	bool		turnOnINT;
} dhd_bus_t;

/* clkstate */
#define CLK_NONE	0
#define CLK_SDONLY	1
#define CLK_PENDING	2	/* Not used yet */
#define CLK_AVAIL	3

/* override the RAM size if possible */
#define DONGLE_MIN_MEMSIZE (128 *1024)
int dhd_dongle_memsize;

static bool retrydata;

#define RETRYCHAN(chan) (((chan) == SDPCM_EVENT_CHANNEL) || retrydata)

static const uint watermark = 8;
static const uint firstread = 32; /* must be a power of 2 */

#define HDATLEN (firstread - (SDPCM_HDRLEN))

/* Retry count for register access failures */
static const uint retry_limit = 2;

#define DHD_ALIGNMENT  4

#define PKTALIGN(osh, p, len, align)					\
	do {								\
		uint datalign;						\
		datalign = (uintptr)PKTDATA((osh), (p));		\
		datalign = ROUNDUP(datalign, (align)) - datalign;	\
		ASSERT(datalign < (align));				\
		ASSERT(PKTLEN((osh), (p)) >= ((len) + datalign));	\
		if (datalign)						\
			PKTPULL((osh), (p), datalign);			\
		PKTSETLEN((osh), (p), (len));				\
	} while (0)

/* Limit on rounding up frames */
static const uint max_roundup = 512;

/* To check if there's window offered */
#define DATAOK(bus) \
	(((uint8)(bus->tx_max - bus->tx_seq) != 0) && \
	(((uint8)(bus->tx_max - bus->tx_seq) & 0x80) == 0))

/* Macros to get register read/write status */
/* NOTE: these assume a local dhdsdio_bus_t *bus! */
#define R_SDREG(regvar, regaddr, retryvar) \
do { \
	retryvar = 0; \
	do { \
		regvar = R_REG(bus->dhd->osh, regaddr); \
	} while (bcmsdh_regfail(bus->sdh) && (++retryvar <= retry_limit)); \
	if (retryvar) { \
		bus->regfails += (retryvar-1); \
		if (retryvar > retry_limit) { \
			DHD_ERROR(("%s: FAILED" #regvar "READ, LINE %d\n", \
			           __FUNCTION__, __LINE__)); \
			regvar = 0; \
		} \
	} \
} while (0)

#define W_SDREG(regval, regaddr, retryvar) \
do { \
	retryvar = 0; \
	do { \
		W_REG(bus->dhd->osh, regaddr, regval); \
	} while (bcmsdh_regfail(bus->sdh) && (++retryvar <= retry_limit)); \
	if (retryvar) { \
		bus->regfails += (retryvar-1); \
		if (retryvar > retry_limit) \
			DHD_ERROR(("%s: FAILED REGISTER WRITE, LINE %d\n", \
			           __FUNCTION__, __LINE__)); \
	} \
} while (0)


/*
 * pktavail interrupts from dongle to host can be managed in 3 different ways
 * whenever there is a packet available in dongle to transmit to host.
 *
 * Mode 0:	Dongle writes the software host mailbox and host is interrupted.
 * Mode 1:	(sdiod core rev >= 4)
 *		Device sets a new bit in the intstatus whenever there is a packet
 *		available in fifo.  Host can't clear this specific status bit until all the 
 *		packets are read from the FIFO.  No need to ack dongle intstatus.
 * Mode 2:	(sdiod core rev >= 4)
 *		Device sets a bit in the intstatus, and host acks this by writing
 *		one to this bit.  Dongle won't generate anymore packet interrupts
 *		until host reads all the packets from the dongle and reads a zero to
 *		figure that there are no more packets.  No need to disable host ints.
 *		Need to ack the intstatus.
 */

#define SDIO_DEVICE_HMB_RXINT		    0	/* default old way */
#define SDIO_DEVICE_RXDATAINT_MODE_0	1	/* from sdiod rev 4 */
#define SDIO_DEVICE_RXDATAINT_MODE_1	2	/* from sdiod rev 4 */

#define FRAME_AVAIL_MASK(bus) 	\
	((bus->rxint_mode == SDIO_DEVICE_HMB_RXINT) ? I_HMB_FRAME_IND : I_XMTDATA_AVAIL)

#define PKT_AVAILABLE(bus, intstatus)	((intstatus) & (FRAME_AVAIL_MASK(bus)))

#define HOSTINTMASK		(I_HMB_SW_MASK | I_CHIPACTIVE)

static int dhdsdio_download_state(dhd_bus_t *bus, bool enter);

static void dhdsdio_release(dhd_bus_t *bus, osl_t *osh);
static void dhdsdio_release_malloc(dhd_bus_t *bus, osl_t *osh);
void dhdsdio_disconnect(void *ptr);
static bool dhdsdio_chipmatch(uint16 chipid);
static bool dhdsdio_probe_attach(dhd_bus_t *bus, osl_t *osh, void *sdh, void * regsva);
static bool dhdsdio_probe_malloc(dhd_bus_t *bus, osl_t *osh);
static bool dhdsdio_probe_init(dhd_bus_t *bus, osl_t *osh, void *sdh);
static void dhdsdio_release_dongle(dhd_bus_t *bus, osl_t *osh, bool dongle_isolation);

static void dhd_dongle_setmemsize(struct dhd_bus *bus, int mem_size);
static int dhd_bcmsdh_recv_buf(dhd_bus_t *bus, uint32 addr, uint fn, uint flags, uint8 *buf, uint nbytes, void *pkt);
static int dhd_bcmsdh_send_buf(dhd_bus_t *bus, uint32 addr, uint fn, uint flags, uint8 *buf, uint nbytes, void *pkt);

static bool dhdsdio_download_firmware(dhd_bus_t *bus, osl_t *osh, void *sdh);
static int _dhdsdio_download_firmware(dhd_bus_t *bus, firmware_info_t *fw_info);
static int dhdsdio_download_nvram(dhd_bus_t *bus, block_info_t *block_info);

static void dhd_dongle_setmemsize(struct dhd_bus *bus, int mem_size)
{
	int32 min_size =  DONGLE_MIN_MEMSIZE;

	/* Restrict the memsize to user specified limit */
	DHD_ERROR(("user: Restrict the dongle ram size to %d, min accepted %d\n", dhd_dongle_memsize, min_size));
	if ((dhd_dongle_memsize > min_size) &&
		(dhd_dongle_memsize < (int32)bus->orig_ramsize))
		bus->ramsize = dhd_dongle_memsize;
}

static int dhdsdio_set_siaddr_window(dhd_bus_t *bus, uint32 address)
{
	int err = 0;

	bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_SBADDRLOW, (address >> 8) & SBSDIO_SBADDRLOW_MASK, &err);

	if (!err)
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_SBADDRMID, (address >> 16) & SBSDIO_SBADDRMID_MASK, &err);
	if (!err)
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_SBADDRHIGH, (address >> 24) & SBSDIO_SBADDRHIGH_MASK, &err);

	return err;
}

/* Turn backplane clock on or off */
static int dhdsdio_htclk(dhd_bus_t *bus, bool on, bool pendok)
{
	int err;
	uint8 clkctl, clkreq, devctl;
	bcmsdh_info_t *sdh;

	clkctl = 0;
	sdh = bus->sdh;

	if (on) /* Request HT Avail */
    {
		clkreq = bus->alp_only ? SBSDIO_ALP_AVAIL_REQ : SBSDIO_HT_AVAIL_REQ;
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, clkreq, &err);
		if (err) {
            ASSERT(0);
			return BCME_ERROR;
		}

		if (pendok &&
		    ((bus->sih->buscoretype == PCMCIA_CORE_ID) && (bus->sih->buscorerev == 9))) {
			uint32 dummy, retries;
			R_SDREG(dummy, &bus->regs->clockctlstatus, retries);
		}

		/* Check current status */
		clkctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, &err);
		if (err) {
            ASSERT(0);
			return BCME_ERROR;
		}

		/* Go to pending and await interrupt if appropriate */
		if (!SBSDIO_CLKAV(clkctl, bus->alp_only) && pendok) 
        {
			/* Allow only clock-available interrupt */
			devctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, &err);
			if (err) {
                ASSERT(0);
				return BCME_ERROR;
			}

			devctl |= SBSDIO_DEVCTL_CA_INT_ONLY;
			bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, devctl, &err);
			bus->clkstate = CLK_PENDING;
			return BCME_OK;
		} 
        else if (bus->clkstate == CLK_PENDING) 
        {
			/* Cancel CA-only interrupt filter */
			devctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, &err);
			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, devctl, &err);
		}

		/* Otherwise, wait here (polling) for HT Avail */
		if (!SBSDIO_CLKAV(clkctl, bus->alp_only)) {
			SPINWAIT_SLEEP(sdioh_spinwait_sleep,
				((clkctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, &err)),
			          !SBSDIO_CLKAV(clkctl, bus->alp_only)), PMU_MAX_TRANSITION_DLY);
		}

		if (err) 
        {
            ASSERT(0);
			return BCME_ERROR;
		}

		if (!SBSDIO_CLKAV(clkctl, bus->alp_only)) {
            ASSERT(0);
			return BCME_ERROR;
		}

		/* Mark clock available */
		bus->clkstate = CLK_AVAIL;
		bus->activity = TRUE;
	} 
    else 
    {
		clkreq = 0;
		if (bus->clkstate == CLK_PENDING)  /* Cancel CA-only interrupt filter */
        {
			devctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, &err);
			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, devctl, &err);
		}

		bus->clkstate = CLK_SDONLY;
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, clkreq, &err);
		if (err) return BCME_ERROR;
	}

	return BCME_OK;
}

/* Change idle/active SD state */
static int dhdsdio_sdclk(dhd_bus_t *bus, bool on)
{
	int err;
	int32 iovalue;

	if (on) 
    {
		if (bus->idleclock == DHD_IDLE_STOP) 
        {
			iovalue = 1; /* Turn on clock and restore mode */
			err = bcmsdh_iovar_op(bus->sdh, "sd_clock", NULL, 0, &iovalue, sizeof(iovalue), TRUE);
			if (err) {
                ASSERT(0);
				return BCME_ERROR;
			}

			iovalue = bus->sd_mode;
			err = bcmsdh_iovar_op(bus->sdh, "sd_mode", NULL, 0, &iovalue, sizeof(iovalue), TRUE);
			if (err) {
                ASSERT(0);
				return BCME_ERROR;
			}
		} 
        else if (bus->idleclock != DHD_IDLE_ACTIVE) 
        {
			iovalue = bus->sd_divisor; /* Restore clock speed */
			err = bcmsdh_iovar_op(bus->sdh, "sd_divisor", NULL, 0, &iovalue, sizeof(iovalue), TRUE);
			if (err) {
                ASSERT(0);
				return BCME_ERROR;
			}
		}
		bus->clkstate = CLK_SDONLY;
	} 
    else /* Stop or slow the SD clock itself */
    {
		if ((bus->sd_divisor == -1) || (bus->sd_mode == -1)) {
            ASSERT(0);
			return BCME_ERROR;
		}

		if (bus->idleclock == DHD_IDLE_STOP) 
        {
            iovalue = 1;
            err = bcmsdh_iovar_op(bus->sdh, "sd_mode", NULL, 0, &iovalue, sizeof(iovalue), TRUE);
            if (err) {
                ASSERT(0);
                return BCME_ERROR;
            }

			iovalue = 0;
			err = bcmsdh_iovar_op(bus->sdh, "sd_clock", NULL, 0, &iovalue, sizeof(iovalue), TRUE);
			if (err) {
                ASSERT(0);
				return BCME_ERROR;
			}
		} 
        else if (bus->idleclock != DHD_IDLE_ACTIVE) 
        {
			/* Set divisor to idle value */
			iovalue = bus->idleclock;
			err = bcmsdh_iovar_op(bus->sdh, "sd_divisor", NULL, 0, &iovalue, sizeof(iovalue), TRUE);
			if (err) {
                ASSERT(0);
				return BCME_ERROR;
			}
		}
		bus->clkstate = CLK_NONE;
	}

	return BCME_OK;
}

/* Transition SD and backplane clock readiness */
static int dhdsdio_clkctl(dhd_bus_t *bus, uint target, bool pendok)
{
	int ret = BCME_OK;

	/* Early exit if we're already there */
	if (bus->clkstate == target) 
    {
		if (target == CLK_AVAIL) bus->activity = TRUE;
		return BCME_OK;
	}

	switch (target) {
	case CLK_AVAIL: /* Make sure SD clock is available */
		if (bus->clkstate == CLK_NONE)
			dhdsdio_sdclk(bus, TRUE);

		ret = dhdsdio_htclk(bus, TRUE, pendok); /* Now request HT Avail on the backplane */
		if (ret == BCME_OK) bus->activity = TRUE;

		break;

	case CLK_SDONLY: /* Remove HT request, or bring up SD clock */
		if (bus->clkstate == CLK_NONE)
			ret = dhdsdio_sdclk(bus, TRUE);
		else if (bus->clkstate == CLK_AVAIL)
			ret = dhdsdio_htclk(bus, FALSE, FALSE);
		break;

	case CLK_NONE:
		if (bus->clkstate == CLK_AVAIL) /* Make sure to remove HT request */
			dhdsdio_htclk(bus, FALSE, FALSE);

		ret = dhdsdio_sdclk(bus, FALSE); /* Now remove the SD clock */
		break;
	}
	return ret;
}

static int dhdsdio_bussleep(dhd_bus_t *bus, bool sleep)
{
	bcmsdh_info_t *sdh = bus->sdh;
	sdpcmd_regs_t *regs = bus->regs;
	uint retries = 0;

	/* Done if we're already in the requested state */
	if (sleep == bus->sleeping)
		return BCME_OK;

	/* Going to sleep: set the alarm and turn off the lights... */
	if (sleep) {
		/* Don't sleep if something is pending */
		if (bus->dpc_sched || bus->rxskip || pktq_len(&bus->txq))
			return BCME_BUSY;

		/* Disable SDIO interrupts (no longer interested) */
		bcmsdh_intr_disable(bus->sdh);

		/* Make sure the controller has the bus up */
		dhdsdio_clkctl(bus, CLK_AVAIL, FALSE);

		/* Tell device to start using OOB wakeup */
		W_SDREG(SMB_USE_OOB, &regs->tosbmailbox, retries);
		if (retries > retry_limit)
			DHD_ERROR(("CANNOT SIGNAL CHIP, WILL NOT WAKE UP!!\n"));

		/* Turn off our contribution to the HT clock request */
		dhdsdio_clkctl(bus, CLK_SDONLY, FALSE);
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, SBSDIO_FORCE_HW_CLKREQ_OFF, NULL);

		/* Isolate the bus */
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, SBSDIO_DEVCTL_PADS_ISO, NULL);

		/* Change state */
		bus->sleeping = TRUE;

	} else {
		/* Waking up: bus power up is ok, set local state */
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, 0, NULL);

		/* Force pad isolation off if possible (in case power never toggled) */
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, 0, NULL);

		/* Make sure the controller has the bus up */
		dhdsdio_clkctl(bus, CLK_AVAIL, FALSE);

		/* Send misc interrupt to indicate OOB not needed */
		W_SDREG(0, &regs->tosbmailboxdata, retries);
		if (retries <= retry_limit)
			W_SDREG(SMB_DEV_INT, &regs->tosbmailbox, retries);

		if (retries > retry_limit)
			DHD_ERROR(("CANNOT SIGNAL CHIP TO CLEAR OOB!!\n"));

		/* Make sure we have SD bus access */
		dhdsdio_clkctl(bus, CLK_SDONLY, FALSE);

		/* Change state */
		bus->sleeping = FALSE;

		/* Enable interrupts again */
		if (bus->intr && (bus->dhd->busstate == DHD_BUS_DATA)) {
			bus->intdis = FALSE;
			bcmsdh_intr_enable(bus->sdh);
		}
	}

	return BCME_OK;
}

#define BUS_WAKE(bus) \
	do { \
		if ((bus)->sleeping) \
			dhdsdio_bussleep((bus), FALSE); \
	} while (0);


/* Writes a HW/SW header into the packet and sends it. */
/* Assumes: (a) header space already there, (b) caller holds lock */
static int dhdsdio_txpkt(dhd_bus_t *bus, void *pkt, uint chan)
{
	int ret;
	osl_t *osh;
	uint8 *frame;
	uint16 len, pad1 = 0;
	uint32 swheader;
	uint retries = 0;
	bcmsdh_info_t *sdh;
	void *new;
	int i;

	sdh = bus->sdh;
	osh = bus->dhd->osh;

	if (bus->dhd->dongle_reset) 
    {
		ret = BCME_NOTREADY;
		goto done;
	}

	frame = (uint8*)PKTDATA(osh, pkt);

	/* Hardware tag: 2 byte len followed by 2 byte ~len check (all LE) */
	len = (uint16)PKTLEN(osh, pkt);
	*(uint16*)frame = htol16(len);
	*(((uint16*)frame) + 1) = htol16(~len);

	/* Software tag: channel, sequence number, data offset */
	swheader = ((chan << SDPCM_CHANNEL_SHIFT) & SDPCM_CHANNEL_MASK) | bus->tx_seq |
	        (((pad1 + SDPCM_HDRLEN) << SDPCM_DOFFSET_SHIFT) & SDPCM_DOFFSET_MASK);
	htol32_ua_store(swheader, frame + SDPCM_FRAMETAG_LEN);
	htol32_ua_store(0, frame + SDPCM_FRAMETAG_LEN + sizeof(swheader));

	/* Raise len to next SDIO block to eliminate tail command */
	if (bus->roundup && bus->blocksize && (len > bus->blocksize)) 
    {
		uint16 pad2 = bus->blocksize - (len % bus->blocksize);
		if ((pad2 <= bus->roundup) && (pad2 < bus->blocksize))
            len += pad2;
	} 
    else if (len % DHD_SDALIGN) 
    {
		len += DHD_SDALIGN - (len % DHD_SDALIGN);
	}

	/* Some controllers have trouble with odd bytes -- round to even */
	if ((len & (DHD_ALIGNMENT - 1)))
        len = ROUNDUP(len, DHD_ALIGNMENT);

	do {
		ret = dhd_bcmsdh_send_buf(bus, bcmsdh_cur_sbwad(sdh), SDIO_FUNC_2, F2SYNC, frame, len, pkt);

		if (ret < 0) /* On failure, abort the command and terminate the frame */
        {
            prints1("send sdio0 error!\r\n");
			bcmsdh_abort(sdh, SDIO_FUNC_2);
			bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_FRAMECTRL, SFC_WF_TERM, NULL);

			for (i = 0; i < 3; i++) 
            {
				uint8 hi, lo;
				hi = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_WFRAMEBCHI, NULL);
				lo = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_WFRAMEBCLO, NULL);
				if ((hi == 0) && (lo == 0)) break;
			}
		}
        else if (ret == 0)
			bus->tx_seq = (bus->tx_seq + 1) % SDPCM_SEQUENCE_WRAP;

	} while ((ret < 0) && retrydata && retries++ < TXRETRIES); /* no valid */

done:
	/* restore pkt buffer pointer before calling tx complete routine */
	PKTPULL(osh, pkt, SDPCM_HDRLEN + pad1);
    dhd_txcomplete(bus->dhd, pkt);
    pktfree(osh, pkt);
	return ret;
}

int check_tx_state(struct dhd_bus *bus)
{
    return (!bus->fcstate && pktq_mlen(&bus->txq, ~bus->flowcontrol) && DATAOK(bus));
}

void * get_dhd_info(void);
int wifi_check_busy(void) /* disable intr when out of interrupt service */
{
    if(get_dhd_info() == ((void *)NULL)) return 0;
    return (!dhd_os_check_sdlock());
}

int dhd_bus_txdata(struct dhd_bus *bus, void *pkt)
{
    int flag;
	int ret = BCME_ERROR;
	osl_t *osh;
	uint prec;
    int prec_out;

	osh = bus->dhd->osh;

	/* Add space for the header */
	PKTPUSH(osh, pkt, SDPCM_HDRLEN);
	ASSERT(ISALIGNED((uintptr)PKTDATA(osh, pkt), 2));

	prec = PRIO2PREC((PKTPRIO(pkt) & PRIOMASK));

    dhd_os_sdlock(bus->dhd);
#if 0
    if(!DATAOK(bus)) //!check_tx_state(bus))
    {
        prints1("data discard!\r\n");
        dhd_os_sdunlock(bus->dhd);
        pktfree(osh, pkt);
        return 0;
    }
#endif

    /* Otherwise, send it now */
    BUS_WAKE(bus);
    /* Make sure back plane ht clk is on, no pending allowed */
    dhdsdio_clkctl(bus, CLK_AVAIL, TRUE);
    ret = dhdsdio_txpkt(bus, pkt, SDPCM_DATA_CHANNEL);
    
    if ((bus->idletime == DHD_IDLE_IMMEDIATE) && !bus->dpc_sched) 
    {
        bus->activity = FALSE;
        dhdsdio_clkctl(bus, CLK_NONE, TRUE);
    }

    dhd_os_sdunlock(bus->dhd);
    return 0;
}

int dhd_bus_txctl(struct dhd_bus *bus, uchar *msg, uint msglen)
{
	uint8 *frame;
	uint16 len;
	uint32 swheader;
	uint retries = 0;
	bcmsdh_info_t *sdh = bus->sdh;
	uint8 doff = 0;
	int ret = -1;
	int i;

	if (bus->dhd->dongle_reset) return -EIO;

	/* Back the pointer to make a room for bus header */
	frame = msg - SDPCM_HDRLEN;
	len = (msglen += SDPCM_HDRLEN);

	/* Add alignment padding (optional for ctl frames) */
    if ((doff = ((uintptr)frame % DHD_SDALIGN))) {
        frame -= doff;
        len += doff;
        msglen += doff;
        bzero(frame, doff + SDPCM_HDRLEN);
    }

    ASSERT(doff < DHD_SDALIGN);
	doff += SDPCM_HDRLEN;

	/* Round send length to next SDIO block */
	if (bus->roundup && bus->blocksize && (len > bus->blocksize)) {
		uint16 pad = bus->blocksize - (len % bus->blocksize);

		if ((pad <= bus->roundup) && (pad < bus->blocksize))
			len += pad;
	} else if (len % DHD_SDALIGN) {
		len += DHD_SDALIGN - (len % DHD_SDALIGN);
	}

	/* Satisfy length-alignment requirements */
	if ((len & (DHD_ALIGNMENT - 1)))
		len = ROUNDUP(len, DHD_ALIGNMENT);

	ASSERT(ISALIGNED((uintptr)frame, 2));

	/* Need to lock here to protect txseq and SDIO tx calls */
	dhd_os_sdlock(bus->dhd);

	BUS_WAKE(bus);

	/* Make sure backplane clock is on */
	dhdsdio_clkctl(bus, CLK_AVAIL, FALSE);

	/* Hardware tag: 2 byte len followed by 2 byte ~len check (all LE) */
	*(uint16*)frame = htol16((uint16)msglen);
	*(((uint16*)frame) + 1) = htol16(~msglen);

	/* Software tag: channel, sequence number, data offset */
	swheader = ((SDPCM_CONTROL_CHANNEL << SDPCM_CHANNEL_SHIFT) & SDPCM_CHANNEL_MASK)
	        | bus->tx_seq | ((doff << SDPCM_DOFFSET_SHIFT) & SDPCM_DOFFSET_MASK);
	htol32_ua_store(swheader, frame + SDPCM_FRAMETAG_LEN);
	htol32_ua_store(0, frame + SDPCM_FRAMETAG_LEN + sizeof(swheader));

	if (!DATAOK(bus)) /* Send from dpc : FIXME: why (tx_max==0) & (tx_seq==255) */
    {
		bus->ctrl_frame_stat = TRUE;
		bus->ctrl_frame_buf = frame;
		bus->ctrl_frame_len = len;
		dhd_wait_for_event(bus->dhd);

		if (bus->ctrl_frame_stat == TRUE)
        {
			ret = -1;
			bus->ctrl_frame_stat = FALSE;
			goto done;
		}
        else
            ret = 0;
	}
    else
	{
		do {
			ret = dhd_bcmsdh_send_buf(bus, bcmsdh_cur_sbwad(sdh), SDIO_FUNC_2, F2SYNC, frame, len, NULL);

			if (ret < 0) /* On failure, abort the command and terminate the frame */
            {
                prints1("send sdio1 error!\r\n");
				bcmsdh_abort(sdh, SDIO_FUNC_2);
				bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_FRAMECTRL, SFC_WF_TERM, NULL);

				for (i = 0; i < 3; i++) 
                {
					uint8 hi, lo;
					hi = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_WFRAMEBCHI, NULL);
					lo = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_WFRAMEBCLO, NULL);
					if ((hi == 0) && (lo == 0))
						break;
				}
			}
			else if (ret == 0) 
				bus->tx_seq = (bus->tx_seq + 1) % SDPCM_SEQUENCE_WRAP;
			
		} while ((ret < 0) && retries++ < TXRETRIES);
	}

done:
	if ((bus->idletime == DHD_IDLE_IMMEDIATE) && !bus->dpc_sched) {
		bus->activity = FALSE;  /* default not come in */
		dhdsdio_clkctl(bus, CLK_NONE, TRUE);
	}

	dhd_os_sdunlock(bus->dhd);
	return ret ? -EIO : 0;
}

int dhd_bus_rxctl(struct dhd_bus *bus, uchar *msg, uint msglen)
{
	int timeleft;
	uint rxlen = 0;
	bool pending;

	if (bus->dhd->dongle_reset)
		return -EIO;

    dhd_os_get_ioctl_resp_timeout();
	dhd_os_ioctl_resp_wait(bus->dhd); /* Wait until control frame is available */
	dhd_os_sdlock(bus->dhd);

	rxlen = bus->rxlen;
	bcopy(bus->rxctl, msg, MIN(msglen, rxlen));
	bus->rxlen = 0;
	dhd_os_sdunlock(bus->dhd);

	return rxlen ? (int)rxlen : -ETIMEDOUT;
}

/* IOVar table */
enum {
	IOV_INTR = 1,
	IOV_POLLRATE,
	IOV_SDREG,
	IOV_SBREG,
	IOV_SDCIS,
	IOV_MEMBYTES,
	IOV_MEMSIZE,
	IOV_DOWNLOAD,
	IOV_SOCRAM_STATE,
	IOV_FORCEEVEN,
	IOV_SDIOD_DRIVE,
	IOV_READAHEAD,
	IOV_SDRXCHAIN,
	IOV_ALIGNCTL,
	IOV_SDALIGN,
	IOV_DEVRESET,
	IOV_CPU,
	IOV_SPROM,
	IOV_TXBOUND,
	IOV_RXBOUND,
	IOV_TXMINMAX,
	IOV_IDLETIME,
	IOV_IDLECLOCK,
	IOV_SD1IDLE,
	IOV_SLEEP,
	IOV_DONGLEISOLATION,
	IOV_VARS
};

const bcm_iovar_t dhdsdio_iovars[] = {
	{"intr",	IOV_INTR,	0,	IOVT_BOOL,	0 },
	{"sleep",	IOV_SLEEP,	0,	IOVT_BOOL,	0 },
	{"pollrate",	IOV_POLLRATE,	0,	IOVT_UINT32,	0 },
	{"idletime",	IOV_IDLETIME,	0,	IOVT_INT32,	0 },
	{"idleclock",	IOV_IDLECLOCK,	0,	IOVT_INT32,	0 },
	{"sd1idle",	IOV_SD1IDLE,	0,	IOVT_BOOL,	0 },
	{"membytes",	IOV_MEMBYTES,	0,	IOVT_BUFFER,	2 * sizeof(int) },
	{"memsize",	IOV_MEMSIZE,	0,	IOVT_UINT32,	0 },
	{"download",	IOV_DOWNLOAD,	0,	IOVT_BOOL,	0 },
	{"socram_state",	IOV_SOCRAM_STATE,	0,	IOVT_BOOL,	0 },
	{"vars",	IOV_VARS,	0,	IOVT_BUFFER,	0 },
	{"sdiod_drive",	IOV_SDIOD_DRIVE, 0,	IOVT_UINT32,	0 },
	{"readahead",	IOV_READAHEAD,	0,	IOVT_BOOL,	0 },
	{"sdrxchain",	IOV_SDRXCHAIN,	0,	IOVT_BOOL,	0 },
	{"alignctl",	IOV_ALIGNCTL,	0,	IOVT_BOOL,	0 },
	{"sdalign",	IOV_SDALIGN,	0,	IOVT_BOOL,	0 },
	{"devreset",	IOV_DEVRESET,	0,	IOVT_BOOL,	0 },
	{"dngl_isolation", IOV_DONGLEISOLATION,	0,	IOVT_UINT32,	0 },
	{NULL, 0, 0, 0, 0 }
};

static int dhdsdio_membytes(dhd_bus_t *bus, bool write, uint32 address, uint8 *data, uint size)
{
	int bcmerror = 0;
	uint32 sdaddr;
	uint dsize;

	/* Determine initial transfer parameters */
	sdaddr = address & SBSDIO_SB_OFT_ADDR_MASK;
	if ((sdaddr + size) & SBSDIO_SBWINDOW_MASK)
		dsize = (SBSDIO_SB_OFT_ADDR_LIMIT - sdaddr);
	else
		dsize = size;

	/* Set the backplane window to include the start address */
	if ((bcmerror = dhdsdio_set_siaddr_window(bus, address))) {
		DHD_ERROR(("%s: window change failed\n", __FUNCTION__));
		goto xfer_done;
	}

	/* Do the transfer(s) */
	while (size) {
		DHD_INFO(("%s: %s %d bytes at offset 0x%08x in window 0x%08x\n",
		          __FUNCTION__, (write ? "write" : "read"), dsize, sdaddr,
		          (address & SBSDIO_SBWINDOW_MASK)));
		if ((bcmerror = bcmsdh_rwdata(bus->sdh, write, sdaddr, data, dsize))) {
			DHD_ERROR(("%s: membytes transfer failed\n", __FUNCTION__));
			break;
		}

		/* Adjust for next transfer (if any) */
		if ((size -= dsize)) {
			data += dsize;
			address += dsize;
			if ((bcmerror = dhdsdio_set_siaddr_window(bus, address))) {
				DHD_ERROR(("%s: window change failed\n", __FUNCTION__));
				break;
			}
			sdaddr = 0;
			dsize = MIN(SBSDIO_SB_OFT_ADDR_LIMIT, size);
		}

	}

xfer_done:
	/* Return the window to backplane enumeration space for core access */
	if (dhdsdio_set_siaddr_window(bus, bcmsdh_cur_sbwad(bus->sdh))) {
		DHD_ERROR(("%s: FAILED to set window back to 0x%x\n", __FUNCTION__,
			bcmsdh_cur_sbwad(bus->sdh)));
	}

	return bcmerror;
}

static unsigned char g_varbuf[MAX_NVRAMBUF_SIZE + 8];
int dhdsdio_downloadvars(dhd_bus_t *bus, void *arg, int len)
{
	int bcmerror = BCME_OK;

	/* Basic sanity checks */
	if (bus->dhd->up) {
		bcmerror = BCME_NOTDOWN;
		goto err;
	}
	if (!len) {
		bcmerror = BCME_BUFTOOSHORT;
		goto err;
	}

	/* Free the old ones and replace with passed variables */
    ASSERT(len < MAX_NVRAMBUF_SIZE + 4);
	bus->vars = g_varbuf;
	bus->varsz = bus->vars ? len : 0;
	if (bus->vars == NULL) {
		bcmerror = BCME_NOMEM;
		goto err;
	}

	/* Copy the passed variables, which should include the terminating double-null */
	bcopy(arg, bus->vars, bus->varsz);
err:
	return bcmerror;
}

static int dhdsdio_write_vars(dhd_bus_t *bus)
{
	int bcmerror = 0;
	uint32 varsize;
	uint32 varaddr;
	uint8 vbuffer[MAX_NVRAMBUF_SIZE + 8];
	uint32 varsizew;

	/* Even if there are no vars are to be written, we still need to set the ramsize. */
	varsize = bus->varsz ? ROUNDUP(bus->varsz, 4) : 0;
	varaddr = (bus->ramsize - 4) - varsize;

	if (bus->vars) 
    {
		if ((bus->sih->buscoretype == SDIOD_CORE_ID) && (bus->sdpcmrev == 7)) 
        {
			if (((varaddr & 0x3C) == 0x3C) && (varsize > 4)) 
            {
				DHD_ERROR(("PR85623WAR in place\n"));
				varsize += 4;
				varaddr -= 4;
			}
		}

		bzero(vbuffer, varsize);
		bcopy(bus->vars, vbuffer, bus->varsz);

		/* Write the vars list */
		bcmerror = dhdsdio_membytes(bus, TRUE, varaddr, vbuffer, varsize);
	}

	/* adjust to the user specified RAM */
	DHD_INFO(("Physical memory size: %d, usable memory size: %d\n", bus->orig_ramsize, bus->ramsize));
	DHD_INFO(("Vars are at %d, orig varsize is %d\n", varaddr, varsize));
	varsize = ((bus->orig_ramsize - 4) - varaddr);

	/*
	 * Determine the length token:
	 * Varsize, converted to words, in lower 16-bits, checksum in upper 16-bits.
	 */
	if (bcmerror) {
		varsizew = 0;
	} else {
		varsizew = varsize / 4;
		varsizew = (~varsizew << 16) | (varsizew & 0x0000FFFF);
		varsizew = htol32(varsizew);
	}

	/* Write the length token to the last word */
	bcmerror = dhdsdio_membytes(bus, TRUE, (bus->orig_ramsize - 4), (uint8*)&varsizew, 4);

	return bcmerror;
}

static int dhdsdio_download_state(dhd_bus_t *bus, bool enter)
{
	uint retries;
	int bcmerror = 0;

	/* To enter download state, disable ARM and reset SOCRAM.
	 * To exit download state, simply reset ARM (default is RAM boot).
	 */
	if (enter) {
		bus->alp_only = TRUE;

		if (!(si_setcore(bus->sih, ARM7S_CORE_ID, 0)) &&
		    !(si_setcore(bus->sih, ARMCM3_CORE_ID, 0))) {
			DHD_ERROR(("%s: Failed to find ARM core!\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto fail;
		}

		si_core_disable(bus->sih, 0);
		if (bcmsdh_regfail(bus->sdh)) {
			bcmerror = BCME_SDIO_ERROR;
			goto fail;
		}

		if (!(si_setcore(bus->sih, SOCRAM_CORE_ID, 0))) {
			DHD_ERROR(("%s: Failed to find SOCRAM core!\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto fail;
		}

		si_core_reset(bus->sih, 0, 0);
		if (bcmsdh_regfail(bus->sdh)) {
			DHD_ERROR(("%s: Failure trying reset SOCRAM core?\n", __FUNCTION__));
			bcmerror = BCME_SDIO_ERROR;
			goto fail;
		}

		/* Clear the top bit of memory */
		if (bus->ramsize) {
			uint32 zeros = 0;
			if (dhdsdio_membytes(bus, TRUE, bus->ramsize - 4, (uint8*)&zeros, 4) < 0) {
				bcmerror = BCME_SDIO_ERROR;
				goto fail;
			}
		}
	} else {
		if (!(si_setcore(bus->sih, SOCRAM_CORE_ID, 0))) {
			DHD_ERROR(("%s: Failed to find SOCRAM core!\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto fail;
		}

		if (!si_iscoreup(bus->sih)) {
			DHD_ERROR(("%s: SOCRAM core is down after reset?\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto fail;
		}

		if ((bcmerror = dhdsdio_write_vars(bus))) {
			DHD_ERROR(("%s: could not write vars to RAM\n", __FUNCTION__));
			goto fail;
		}

		if (!si_setcore(bus->sih, PCMCIA_CORE_ID, 0) &&
		    !si_setcore(bus->sih, SDIOD_CORE_ID, 0)) {
			DHD_ERROR(("%s: Can't change back to SDIO core?\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto fail;
		}
		W_SDREG(0xFFFFFFFF, &bus->regs->intstatus, retries);

		if (!(si_setcore(bus->sih, ARM7S_CORE_ID, 0)) &&
		    !(si_setcore(bus->sih, ARMCM3_CORE_ID, 0))) {
			DHD_ERROR(("%s: Failed to find ARM core!\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto fail;
		}

		si_core_reset(bus->sih, 0, 0);
		if (bcmsdh_regfail(bus->sdh)) {
			DHD_ERROR(("%s: Failure trying to reset ARM core?\n", __FUNCTION__));
			bcmerror = BCME_SDIO_ERROR;
			goto fail;
		}

		/* Allow HT Clock now that the ARM is running. */
		bus->alp_only = FALSE;
		bus->dhd->busstate = DHD_BUS_LOAD;
	}

fail:
	/* Always return to SDIOD core */
	if (!si_setcore(bus->sih, PCMCIA_CORE_ID, 0))
		si_setcore(bus->sih, SDIOD_CORE_ID, 0);

	return bcmerror;
}

void dhd_bus_stop(struct dhd_bus *bus, bool enforce_mutex)
{
	osl_t *osh;
	uint32 local_hostintmask;
	uint8 saveclk;
	uint retries;
	int err;

	if (!bus->dhd) return;

	osh = bus->dhd->osh;

	if (enforce_mutex)
		dhd_os_sdlock(bus->dhd);

	BUS_WAKE(bus);

	/* Change our idea of bus state */
	bus->dhd->busstate = DHD_BUS_DOWN;

	/* Enable clock for device interrupts */
	dhdsdio_clkctl(bus, CLK_AVAIL, FALSE);
	/* Disable and clear interrupts at the chip level also */
	W_SDREG(0, &bus->regs->hostintmask, retries);
	local_hostintmask = bus->hostintmask;
	bus->hostintmask = 0;

	/* Force clocks on backplane to be sure F2 interrupt propagates */
	saveclk = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, &err);
	if (!err) {
		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, (saveclk | SBSDIO_FORCE_HT), &err);
	}
	if (err) {
		DHD_ERROR(("%s: Failed to force clock for F2: err %d\n", __FUNCTION__, err));
	}

	/* Turn off the bus (F2), free any pending packets */
	bcmsdh_intr_disable(bus->sdh);
	bcmsdh_cfg_write(SDIO_FUNC_0, SDIOD_CCCR_IOEN, SDIO_FUNC_ENABLE_1, NULL);

	/* Clear any pending interrupts now that F2 is disabled */
	W_SDREG(local_hostintmask, &bus->regs->intstatus, retries);

	/* Turn off the backplane clock (only) */
	dhdsdio_clkctl(bus, CLK_SDONLY, FALSE);

	/* Clear any held glomming stuff */
	if (bus->glomd)
		pktfree(osh, bus->glomd);

	if (bus->glom)
		pktfree(osh, bus->glom);

	bus->glom = bus->glomd = NULL;

	/* Clear rx control and wake any waiters */
	bus->rxlen = 0;
	dhd_os_ioctl_resp_wake(bus->dhd);

	/* Reset some F2 state stuff */
	bus->rxskip = FALSE;
	bus->tx_seq = bus->rx_seq = 0;

	if (enforce_mutex)
		dhd_os_sdunlock(bus->dhd);
}

int dhd_bus_init(dhd_pub_t *dhdp, bool enforce_mutex)
{
	dhd_bus_t *bus = dhdp->bus;
	dhd_timeout_t tmo;
	uint retries = 0;
	uint8 ready, enable;
	int err, ret = 0;
	uint8 saveclk;

	if (enforce_mutex)
        dhd_os_sdlock(bus->dhd);

	/* Make sure backplane clock is on, needed to generate F2 interrupt */
	dhdsdio_clkctl(bus, CLK_AVAIL, FALSE);
	if (bus->clkstate != CLK_AVAIL) {
        ASSERT(0);
		goto exit;
	}

	/* Force clocks on backplane to be sure F2 interrupt propagates */
	saveclk = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, &err);
	if (!err) bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, (saveclk | SBSDIO_FORCE_HT), &err);

	if (err) {
        ASSERT(0);
		goto exit;
	}

	/* Enable function 2 (frame transfers) */
	W_SDREG((SDPCM_PROT_VERSION << SMB_DATA_VERSION_SHIFT), &bus->regs->tosbmailboxdata, retries);

	enable = (SDIO_FUNC_ENABLE_1 | SDIO_FUNC_ENABLE_2);
	bcmsdh_cfg_write(SDIO_FUNC_0, SDIOD_CCCR_IOEN, enable, NULL);

	/* Give the dongle some time to do its thing and set IOR2 */
	dhd_timeout_start(&tmo, DHD_WAIT_F2RDY * 1000);

	ready = 0;
	while (ready != enable && !dhd_timeout_expired(&tmo))
	        ready = bcmsdh_cfg_read(SDIO_FUNC_0, SDIOD_CCCR_IORDY, NULL);

	/* If F2 successfully enabled, set core and enable interrupts */
	if (ready == enable) 
    {
		/* Make sure we're talking to the core. */
		if (!(bus->regs = si_setcore(bus->sih, PCMCIA_CORE_ID, 0)))
			bus->regs = si_setcore(bus->sih, SDIOD_CORE_ID, 0);
		ASSERT(bus->regs != NULL);

		/* Set up the interrupt mask and enable interrupts */
		bus->hostintmask = HOSTINTMASK;
		/* corerev 4 could use the newer interrupt logic to detect the frames */
		if ((bus->sih->buscoretype == SDIOD_CORE_ID) && (bus->sdpcmrev == 4) &&
			(bus->rxint_mode != SDIO_DEVICE_HMB_RXINT)) {
			bus->hostintmask &= ~I_HMB_FRAME_IND;
			bus->hostintmask |= I_XMTDATA_AVAIL;
		}
		W_SDREG(bus->hostintmask, &bus->regs->hostintmask, retries);

		bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_WATERMARK, (uint8)watermark, &err);

		/* Set bus state according to enable result */
		dhdp->busstate = DHD_BUS_DATA;

		/* bcmsdh_intr_unmask(bus->sdh); */

		bus->intdis = FALSE;
		if (bus->intr) 
			bcmsdh_intr_enable(bus->sdh);
		else
			bcmsdh_intr_disable(bus->sdh);
	}
    else { /* Disable F2 again */
		bcmsdh_cfg_write(SDIO_FUNC_0, SDIOD_CCCR_IOEN, SDIO_FUNC_ENABLE_1, NULL);
	}

	/* Restore previous clock setting */
	bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, saveclk, &err);

	/* If we didn't come up, turn off backplane clock */
	if (dhdp->busstate != DHD_BUS_DATA)
		dhdsdio_clkctl(bus, CLK_NONE, FALSE);

exit:
	if (enforce_mutex)
		dhd_os_sdunlock(bus->dhd);

	return ret;
}

static void dhdsdio_rxfail(dhd_bus_t *bus, bool abort, bool rtx)
{
	bcmsdh_info_t *sdh = bus->sdh;
	sdpcmd_regs_t *regs = bus->regs;
	uint retries = 0;
	uint16 lastrbc;
	uint8 hi, lo;
	int err;

	if (abort)
		bcmsdh_abort(sdh, SDIO_FUNC_2);

	bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_FRAMECTRL, SFC_RF_TERM, &err);

	/* Wait until the packet has been flushed (device/FIFO stable) */
	for (lastrbc = retries = 0xffff; retries > 0; retries--) 
    {
		hi = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_RFRAMEBCHI, NULL);
		lo = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_RFRAMEBCLO, NULL);
		if ((hi == 0) && (lo == 0))
			break;

		if ((hi > (lastrbc >> 8)) && (lo > (lastrbc & 0x00ff)))
            ASSERT(0);
		lastrbc = (hi << 8) + lo;
	}

	if (!retries) 
        ASSERT(0);

	if (rtx) 
    {
		W_SDREG(SMB_NAK, &regs->tosbmailbox, retries);
		if (retries <= retry_limit)
			bus->rxskip = TRUE;
	}

	/* Clear partial in any case */
	bus->nextlen = 0;

	/* If we can't reach the device, signal failure */
	if (err || bcmsdh_regfail(sdh))
		bus->dhd->busstate = DHD_BUS_DOWN;
}

static void dhdsdio_read_control(dhd_bus_t *bus, uint8 *hdr, uint len, uint doff)
{
	bcmsdh_info_t *sdh = bus->sdh;
	uint rdlen, pad;
	int sdret;

	ASSERT(bus->rxbuf);

	/* Set rxctl for frame (w/optional alignment) */
	bus->rxctl = bus->rxbuf;
    bus->rxctl += firstread;
    if ((pad = ((uintptr)bus->rxctl % DHD_SDALIGN)))
        bus->rxctl += (DHD_SDALIGN - pad);
    bus->rxctl -= firstread;
	ASSERT(bus->rxctl >= bus->rxbuf);

	/* Copy the already-read portion over */
	bcopy(hdr, bus->rxctl, firstread);
	if (len <= firstread)
		goto gotpkt;

	/* Raise rdlen to next SDIO block to avoid tail command */
	rdlen = len - firstread;
	if (bus->roundup && bus->blocksize && (rdlen > bus->blocksize)) {
		pad = bus->blocksize - (rdlen % bus->blocksize);
		if ((pad <= bus->roundup) && (pad < bus->blocksize) && ((len + pad) < bus->dhd->maxctl))
			rdlen += pad;

	} else if (rdlen % DHD_SDALIGN) {
		rdlen += DHD_SDALIGN - (rdlen % DHD_SDALIGN);
	}

	/* Satisfy length-alignment requirements */
	if ((rdlen & (DHD_ALIGNMENT - 1)))
		rdlen = ROUNDUP(rdlen, DHD_ALIGNMENT);

	/* Drop if the read is too big or it exceeds our maximum */
	if ((rdlen + firstread) > bus->dhd->maxctl) {
		dhdsdio_rxfail(bus, FALSE, FALSE);
		goto done;
	}

	if ((len - doff) > bus->dhd->maxctl) {
		dhdsdio_rxfail(bus, FALSE, FALSE);
		goto done;
	}

	/* Read remainder of frame body into the rxctl buffer */
	sdret = dhd_bcmsdh_recv_buf(bus, bcmsdh_cur_sbwad(sdh), SDIO_FUNC_2, F2SYNC, (bus->rxctl + firstread), rdlen, NULL);

	/* Control frame failures need retransmission */
	if (sdret < 0) 
    {
        prints1("%s:%d-error!\r\n", __FUNCTION__, __LINE__);
        prints1("receive sdio0 failed!\r\n");
		dhdsdio_rxfail(bus, TRUE, TRUE);
		goto done;
	}

gotpkt:
	/* Point to valid data and indicate its length */
	bus->rxctl += doff;
	bus->rxlen = len - doff;

done:
	/* Awake any waiters */
	dhd_os_ioctl_resp_wake(bus->dhd);
    return ;
}

static uint8 dhdsdio_rxglom(dhd_bus_t *bus, uint8 rxseq)
{
	uint16 dlen, totlen;
	uint8 *dptr, num = 0;

	uint16 sublen, check;
	void *pfirst, *plast, *pnext, *save_pfirst;
	osl_t *osh = bus->dhd->osh;

	int errcode;
	uint8 chan, seq, doff, sfdoff;
	uint8 txmax;

	int ifidx = 0;
	bool usechain = bus->use_rxchain;

	/* If packets, issue read(s) and send up packet chain */
	/* Return sequence numbers consumed? */
	/* If there's a descriptor, generate the packet chain */

	if (bus->glomd) 
    {
		pfirst = plast = pnext = NULL;
		dlen = (uint16)PKTLEN(osh, bus->glomd);
		dptr = PKTDATA(osh, bus->glomd);
		if (!dlen || (dlen & 1))
			dlen = 0;

		for (totlen = num = 0; dlen; num++) /* Get (and move past) next length */
        {
			sublen = ltoh16_ua(dptr); /* frame header */
			dlen -= sizeof(uint16);
			dptr += sizeof(uint16);
			if ((sublen < SDPCM_HDRLEN) || ((num == 0) && (sublen < (2 * SDPCM_HDRLEN)))) {
                ASSERT(0);
				pnext = NULL;
				break;
			}

			if (sublen % DHD_SDALIGN)
				usechain = FALSE;
			
			totlen += sublen;

			if (!dlen) /* For last frame, adjust read len so total is a block multiple */
            {
				sublen += (ROUNDUP(totlen, bus->blocksize) - totlen);
				totlen = ROUNDUP(totlen, bus->blocksize);
			}

			/* Allocate/chain packet for next subframe */
			if ((pnext = pktget(osh, sublen + DHD_SDALIGN, FALSE)) == NULL)
            {
                ASSERT(0);
				break;
            }

			ASSERT(!PKTLINK(pnext));
			if (!pfirst) 
            {
				ASSERT(!plast);
				pfirst = plast = pnext;
			} 
            else 
            {
				ASSERT(plast);
				PKTSETNEXT(osh, plast, pnext);
				plast = pnext;
			}

			/* Adhere to start alignment requirements */
			PKTALIGN(osh, pnext, sublen, DHD_SDALIGN);
		}

		/* If all allocations succeeded, save packet chain in bus structure */
		if (pnext) 
        {
			bus->glom = pfirst;
			pfirst = pnext = NULL;
		} 
        else 
        {
            ASSERT(0);
			if (pfirst)
				pktfree(osh, pfirst);
			bus->glom = NULL;
			num = 0;
		}

		/* Done with descriptor packet */
		pktfree(osh, bus->glomd);
		bus->glomd = NULL;
		bus->nextlen = 0;
	}

	if (bus->glom) /* Ok -- either we just generated a packet chain, or had one from before */
    {
		pfirst = bus->glom;
		dlen = (uint16)pkttotlen(osh, pfirst);

		/* Do an SDIO read for the superframe.  Configurable iovar to
		 * read directly into the chained packet, or allocate a large
		 * packet and and copy into the chain.
		 */
		if (usechain) 
        {
			errcode = dhd_bcmsdh_recv_buf(bus, bcmsdh_cur_sbwad(bus->sdh), SDIO_FUNC_2, F2SYNC, (uint8*)PKTDATA(osh, pfirst), dlen, pfirst);
		} 
        else if (bus->dataptr) 
        {
			errcode = dhd_bcmsdh_recv_buf(bus, bcmsdh_cur_sbwad(bus->sdh), SDIO_FUNC_2, F2SYNC, bus->dataptr, dlen, NULL);
			sublen = (uint16)pktfrombuf(osh, pfirst, 0, dlen, bus->dataptr);
			if (sublen != dlen)
            {
                ASSERT(0);
				errcode = -1;
            }
			
			pnext = NULL;
		} 
        else 
        {
            prints1("%s:%d-error!\r\n", __FUNCTION__, __LINE__);
			errcode = -1;
        }
	
		/* On failure, kill the superframe, allow a couple retries */
		if (errcode < 0) 
        {
            prints1("%s:%d-error!\r\n", __FUNCTION__, __LINE__);
            prints1("receive sdio1 failed!\r\n");
			if (bus->glomerr++ < 3) {
				dhdsdio_rxfail(bus, TRUE, TRUE);
			} else {
				bus->glomerr = 0;
				dhdsdio_rxfail(bus, TRUE, FALSE);
				pktfree(osh, bus->glom);
				bus->rxglomfail++;
				bus->glom = NULL;
			}
			return 0;
		}

		/* Validate the superframe header */
		dptr = (uint8 *)PKTDATA(osh, pfirst);
		sublen = ltoh16_ua(dptr);
		check = ltoh16_ua(dptr + sizeof(uint16));

		chan = SDPCM_PACKET_CHANNEL(&dptr[SDPCM_FRAMETAG_LEN]);
		seq = SDPCM_PACKET_SEQUENCE(&dptr[SDPCM_FRAMETAG_LEN]);
		bus->nextlen = dptr[SDPCM_FRAMETAG_LEN + SDPCM_NEXTLEN_OFFSET];
		if ((bus->nextlen << 4) > MAX_RX_DATASZ)
			bus->nextlen = 0;
		
		doff = SDPCM_DOFFSET_VALUE(&dptr[SDPCM_FRAMETAG_LEN]);
		txmax = SDPCM_WINDOW_VALUE(&dptr[SDPCM_FRAMETAG_LEN]);

		errcode = 0;
		if ((uint16)~(sublen^check))
			errcode = -1;
		else if (ROUNDUP(sublen, bus->blocksize) != dlen)
			errcode = -1;
		else if (SDPCM_PACKET_CHANNEL(&dptr[SDPCM_FRAMETAG_LEN]) != SDPCM_GLOM_CHANNEL)
			errcode = -1;
		else if (SDPCM_GLOMDESC(&dptr[SDPCM_FRAMETAG_LEN]))
			errcode = -1;
		else if ((doff < SDPCM_HDRLEN) || (doff > (PKTLEN(osh, pfirst) - SDPCM_HDRLEN)))
			errcode = -1;

		/* Check sequence number of superframe SW header */
		if (rxseq != seq)
			rxseq = seq;

		/* Check window for sanity */
		if ((uint8)(txmax - bus->tx_seq) > 0x40)
			txmax = bus->tx_seq + 2;
		
		bus->tx_max = txmax;

		/* Remove superframe header, remember offset */
		PKTPULL(osh, pfirst, doff);
		sfdoff = doff;

		/* Validate all the subframe headers */
		for (num = 0, pnext = pfirst; pnext && !errcode; num++, pnext = PKTNEXT(osh, pnext)) 
        {
			dptr = (uint8 *)PKTDATA(osh, pnext);
			dlen = (uint16)PKTLEN(osh, pnext);
			sublen = ltoh16_ua(dptr);
			check = ltoh16_ua(dptr + sizeof(uint16));
			chan = SDPCM_PACKET_CHANNEL(&dptr[SDPCM_FRAMETAG_LEN]);
			doff = SDPCM_DOFFSET_VALUE(&dptr[SDPCM_FRAMETAG_LEN]);

			if ((uint16)~(sublen^check))
				errcode = -1;
			else if ((sublen > dlen) || (sublen < SDPCM_HDRLEN))
				errcode = -1;
			else if ((chan != SDPCM_DATA_CHANNEL) && (chan != SDPCM_EVENT_CHANNEL))
				errcode = -1;
			else if ((doff < SDPCM_HDRLEN) || (doff > sublen))
				errcode = -1;
		}

		if (errcode) {
            ASSERT(0);
			/* Terminate frame on error, request a couple retries */
			if (bus->glomerr++ < 3) {
				/* Restore superframe header space */
				PKTPUSH(osh, pfirst, sfdoff);
				dhdsdio_rxfail(bus, TRUE, TRUE);
			} else {
				bus->glomerr = 0;
				dhdsdio_rxfail(bus, TRUE, FALSE);
				pktfree(osh, bus->glom);
				bus->rxglomfail++;
				bus->glom = NULL;
			}
			bus->nextlen = 0;
			return 0;
		}

		/* Basic SD framing looks ok - process each packet (header) */
		save_pfirst = pfirst;
		bus->glom = NULL;
		plast = NULL;

		for (num = 0; pfirst; rxseq++, pfirst = pnext) 
        {
			pnext = PKTNEXT(osh, pfirst);
			PKTSETNEXT(osh, pfirst, NULL);

			dptr = (uint8 *)PKTDATA(osh, pfirst);
			sublen = ltoh16_ua(dptr);
			chan = SDPCM_PACKET_CHANNEL(&dptr[SDPCM_FRAMETAG_LEN]);
			seq = SDPCM_PACKET_SEQUENCE(&dptr[SDPCM_FRAMETAG_LEN]);
			doff = SDPCM_DOFFSET_VALUE(&dptr[SDPCM_FRAMETAG_LEN]);

			ASSERT((chan == SDPCM_DATA_CHANNEL) || (chan == SDPCM_EVENT_CHANNEL));

			if (rxseq != seq)
				rxseq = seq;

			PKTSETLEN(osh, pfirst, sublen);
			PKTPULL(osh, pfirst, doff);

			if (PKTLEN(osh, pfirst) == 0) {
				pktfree(bus->dhd->osh, pfirst);
				if (plast) {
					PKTSETNEXT(osh, plast, pnext);
				} else {
					ASSERT(save_pfirst == pfirst);
					save_pfirst = pnext;
				}
				continue;
			} else if (dhd_prot_hdrpull(bus->dhd, &ifidx, pfirst) != 0) {
				pktfree(osh, pfirst);
				if (plast) {
					PKTSETNEXT(osh, plast, pnext);
				} else {
					ASSERT(save_pfirst == pfirst);
					save_pfirst = pnext;
				}
				continue;
			}

			/* this packet will go up, link back into chain and count it */
			PKTSETNEXT(osh, pfirst, pnext);
			plast = pfirst;
			num++;

		}

		if (num) {
			dhd_os_sdunlock(bus->dhd);
			dhd_rx_frame(bus->dhd, ifidx, save_pfirst, num, 0);
			dhd_os_sdlock(bus->dhd);
		}

		bus->rxglomframes++;
		bus->rxglompkts += num;
	}
	return num;
}

/* Return TRUE if there may be more frames to read */
static uint dhdsdio_readframes(dhd_bus_t *bus, uint maxframes, bool *finished)
{
	osl_t *osh = bus->dhd->osh;
	bcmsdh_info_t *sdh = bus->sdh;

	uint16 len, check;	/* Extracted hardware header fields */
	uint8 chan, seq, doff;	/* Extracted software header fields */
	uint8 fcbits;		/* Extracted fcbits from software header */
	uint8 delta;

	void *pkt;	/* Packet for event or data frames */
	uint16 pad;	/* Number of pad bytes to read */
	uint16 rdlen;	/* Total number of bytes to read */
	uint8 rxseq;	/* Next sequence number to expect */
	uint rxleft = 0;	/* Remaining number of frames allowed */
	int sdret;	/* Return code from bcmsdh calls */
	uint8 txmax;	/* Maximum tx sequence offered */
	bool len_consistent; /* Result of comparing readahead len and len from hw-hdr */
	uint8 *rxbuf;
	int ifidx = 0;
	uint rxcount = 0; /* Total frames read */

	ASSERT(maxframes);

	/* Not finished unless we encounter no more frames indication */
	*finished = FALSE;

	for (rxseq = bus->rx_seq, rxleft = maxframes; !bus->rxskip && rxleft && bus->dhd->busstate != DHD_BUS_DOWN; rxseq++, rxleft--) 
    {
		/* Handle glomming separately */
		if (bus->glom || bus->glomd) 
        {
			uint8 cnt;
			cnt = dhdsdio_rxglom(bus, rxseq);
			rxseq += cnt - 1;
			rxleft = (rxleft > cnt) ? (rxleft - cnt) : 1;
			continue;
		}

		/* Try doing single read if we can */
		if (bus->nextlen) {
			uint16 nextlen = bus->nextlen;
			bus->nextlen = 0;

            rdlen = len = nextlen << 4;

            /* Pad read to blocksize for efficiency */
            if (bus->roundup && bus->blocksize && (rdlen > bus->blocksize)) {
                pad = bus->blocksize - (rdlen % bus->blocksize);
                if ((pad <= bus->roundup) && (pad < bus->blocksize) && ((rdlen + pad + firstread) < MAX_RX_DATASZ))
                    rdlen += pad;
            } else if (rdlen % DHD_SDALIGN) {
                rdlen += DHD_SDALIGN - (rdlen % DHD_SDALIGN);
            }

			/* We use bus->rxctl buffer in WinXP for initial control pkt receives.
			 * Later we use buffer-poll for data as well as control packets.
			 * This is required because dhd receives full frame in gSPI unlike SDIO.
			 * After the frame is received we have to distinguish whether it is data
			 * or non-data frame.
			 */
			/* Allocate a packet buffer */

			if (!(pkt = pktget(osh, rdlen + DHD_SDALIGN, FALSE))) {
                /* Give up on data, request rtx of events */
                /* Just go try again w/normal header read */
                continue;
			} else {
                ASSERT(!PKTLINK(pkt));
				PKTALIGN(osh, pkt, rdlen, DHD_SDALIGN);
				rxbuf = (uint8 *)PKTDATA(osh, pkt);

				/* Read the entire frame */
				sdret = dhd_bcmsdh_recv_buf(bus, bcmsdh_cur_sbwad(sdh), SDIO_FUNC_2, F2SYNC, rxbuf, rdlen, pkt);
				if (sdret < 0) 
                {
                    prints1("%s:%d-error!\r\n", __FUNCTION__, __LINE__);
                    prints1("receive sdio2 failed!\r\n");
					pktfree(bus->dhd->osh, pkt);
					/* Force retry w/normal header read.  Don't attempt NAK for
					 * gSPI
					 */
					dhdsdio_rxfail(bus, TRUE, TRUE);
					continue;
				}
			}

			/* Now check the header */
			bcopy(rxbuf, bus->rxhdr, SDPCM_HDRLEN);

			/* Extract hardware header fields */
			len = ltoh16_ua(bus->rxhdr);
			check = ltoh16_ua(bus->rxhdr + sizeof(uint16));

			/* All zeros means readahead info was bad */
			if (!(len|check)) {
                pktfree(bus->dhd->osh, pkt);
				continue;
			}

			/* Validate check bytes */
			if ((uint16)~(len^check)) {
                pktfree(bus->dhd->osh, pkt);
				bus->rx_badhdr++;
				dhdsdio_rxfail(bus, FALSE, FALSE);
				continue;
			}

			/* Validate frame length */
			if (len < SDPCM_HDRLEN) {
                pktfree(bus->dhd->osh, pkt);
				continue;
			}

			/* Check for consistency with readahead info */
			len_consistent = (nextlen != (ROUNDUP(len, 16) >> 4));
			if (len_consistent) {
				/* Mismatch, force retry w/normal header (may be >4K) */
                pktfree(bus->dhd->osh, pkt);
				dhdsdio_rxfail(bus, TRUE, TRUE);
				continue;
			}

			/* Extract software header fields */
			chan = SDPCM_PACKET_CHANNEL(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
			seq = SDPCM_PACKET_SEQUENCE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
			doff = SDPCM_DOFFSET_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
			txmax = SDPCM_WINDOW_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);

			bus->nextlen = bus->rxhdr[SDPCM_FRAMETAG_LEN + SDPCM_NEXTLEN_OFFSET];
            if ((bus->nextlen << 4) > MAX_RX_DATASZ)
                bus->nextlen = 0;

		    /* Handle Flow Control */
			fcbits = SDPCM_FCMASK_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);

			delta = 0;
			if (~bus->flowcontrol & fcbits)
				delta = 1;

			if (bus->flowcontrol & ~fcbits)
				delta = 1;

			if (delta)
				bus->flowcontrol = fcbits;

			/* Check and update sequence number */
			if (rxseq != seq)
				rxseq = seq;

			/* Check window for sanity */
			if ((uint8)(txmax - bus->tx_seq) > 0x40)
                txmax = bus->tx_seq + 2;
    		
			bus->tx_max = txmax;

			if (chan == SDPCM_CONTROL_CHANNEL) {
                /* Force retry w/normal header read */
                bus->nextlen = 0;
                dhdsdio_rxfail(bus, FALSE, TRUE);
                pktfree(bus->dhd->osh, pkt);
                continue;
			}

            /* Validate data offset */
			if ((doff < SDPCM_HDRLEN) || (doff > len)) {
                pktfree(bus->dhd->osh, pkt);
				ASSERT(0);
				dhdsdio_rxfail(bus, FALSE, FALSE);
				continue;
			}

			/* All done with this one -- now deliver the packet */
			goto deliver;
		}

		/* Read frame header (hardware and software) */
		sdret = dhd_bcmsdh_recv_buf(bus, bcmsdh_cur_sbwad(sdh), SDIO_FUNC_2, F2SYNC, bus->rxhdr, firstread, NULL);

		if (sdret < 0) 
        {
            prints1("%s:%d-error!\r\n", __FUNCTION__, __LINE__);
            prints1("receive sdio4 failed!\r\n");
			dhdsdio_rxfail(bus, TRUE, TRUE);
			continue;
		}

		/* Extract hardware header fields */
		len = ltoh16_ua(bus->rxhdr);
		check = ltoh16_ua(bus->rxhdr + sizeof(uint16));

		/* All zeros means no more frames */
		if (!(len|check)) {
			*finished = TRUE;
			break;
		}

		/* Validate check bytes */
		if ((uint16)~(len^check)) {
            ASSERT(0);
			dhdsdio_rxfail(bus, FALSE, FALSE);
			continue;
		}

		if (len < SDPCM_HDRLEN) { /* Validate frame length */
            ASSERT(0);
			continue;
		}

		/* Extract software header fields */
		chan = SDPCM_PACKET_CHANNEL(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
		seq = SDPCM_PACKET_SEQUENCE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
		doff = SDPCM_DOFFSET_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
		txmax = SDPCM_WINDOW_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);

		/* Validate data offset */
		if ((doff < SDPCM_HDRLEN) || (doff > len)) {
			ASSERT(0);
			dhdsdio_rxfail(bus, FALSE, FALSE);
			continue;
		}

		/* Save the readahead length if there is one */
		bus->nextlen = bus->rxhdr[SDPCM_FRAMETAG_LEN + SDPCM_NEXTLEN_OFFSET];
		if ((bus->nextlen << 4) > MAX_RX_DATASZ)
			bus->nextlen = 0;

		/* Handle Flow Control */
		fcbits = SDPCM_FCMASK_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);

		delta = 0;
		if (~bus->flowcontrol & fcbits) {
			bus->fc_xoff++;
			delta = 1;
		}
		if (bus->flowcontrol & ~fcbits) {
			bus->fc_xon++;
			delta = 1;
		}

		if (delta)
			bus->flowcontrol = fcbits;

		/* Check and update sequence number */
		if (rxseq != seq)
			rxseq = seq;

		/* Check window for sanity */
		if ((uint8)(txmax - bus->tx_seq) > 0x40)
			txmax = bus->tx_seq + 2;
		
		bus->tx_max = txmax;

		/* Call a separate function for control frames */
		if (chan == SDPCM_CONTROL_CHANNEL) {
			dhdsdio_read_control(bus, bus->rxhdr, len, doff);
			continue;
		}

		ASSERT((chan == SDPCM_DATA_CHANNEL) || (chan == SDPCM_EVENT_CHANNEL) ||
		       (chan == SDPCM_TEST_CHANNEL) || (chan == SDPCM_GLOM_CHANNEL));

		/* Length to read */
		rdlen = (len > firstread) ? (len - firstread) : 0;

		/* May pad read to blocksize for efficiency */
		if (bus->roundup && bus->blocksize && (rdlen > bus->blocksize)) {
			pad = bus->blocksize - (rdlen % bus->blocksize);
			if ((pad <= bus->roundup) && (pad < bus->blocksize) &&
			    ((rdlen + pad + firstread) < MAX_RX_DATASZ))
				rdlen += pad;
		} else if (rdlen % DHD_SDALIGN) {
			rdlen += DHD_SDALIGN - (rdlen % DHD_SDALIGN);
		}

		/* Satisfy length-alignment requirements */
		if ((rdlen & (DHD_ALIGNMENT - 1)))
			rdlen = ROUNDUP(rdlen, DHD_ALIGNMENT);

		if ((rdlen + firstread) > MAX_RX_DATASZ) { /* Too long -- skip this frame */
			dhdsdio_rxfail(bus, FALSE, FALSE);
			continue;
		}

		if (!(pkt = pktget(osh, (rdlen + firstread + DHD_SDALIGN), FALSE))) {
            ASSERT(0);
			dhdsdio_rxfail(bus, FALSE, RETRYCHAN(chan));
			continue;
		}

		ASSERT(!PKTLINK(pkt));

		/* Leave room for what we already read, and align remainder */
		ASSERT(firstread < (PKTLEN(osh, pkt)));
		PKTPULL(osh, pkt, firstread);
		PKTALIGN(osh, pkt, rdlen, DHD_SDALIGN);

		/* Read the remaining frame data */
		sdret = dhd_bcmsdh_recv_buf(bus, bcmsdh_cur_sbwad(sdh), SDIO_FUNC_2, F2SYNC, ((uint8 *)PKTDATA(osh, pkt)), rdlen, pkt);

		if (sdret < 0) 
        {
            prints1("%s:%d-error!\r\n", __FUNCTION__, __LINE__);
            prints1("receive sdio5 failed!\r\n");
			pktfree(bus->dhd->osh, pkt);
			dhdsdio_rxfail(bus, TRUE, RETRYCHAN(chan));
			continue;
		}

		/* Copy the already-read portion */
		PKTPUSH(osh, pkt, firstread);
		bcopy(bus->rxhdr, PKTDATA(osh, pkt), firstread);

deliver:
		/* Save superframe descriptor and allocate packet frame */
		if (chan == SDPCM_GLOM_CHANNEL) 
        {
			if (SDPCM_GLOMDESC(&bus->rxhdr[SDPCM_FRAMETAG_LEN])) 
            {
				PKTSETLEN(osh, pkt, len);
				ASSERT(doff == SDPCM_HDRLEN);
				PKTPULL(osh, pkt, SDPCM_HDRLEN);
				bus->glomd = pkt;
			} 
            else 
				dhdsdio_rxfail(bus, FALSE, FALSE);

			continue;
		}

		/* Fill in packet len and prio, deliver upward */
		PKTSETLEN(osh, pkt, len);
		PKTPULL(osh, pkt, doff);

		if (PKTLEN(osh, pkt) == 0) {
			pktfree(bus->dhd->osh, pkt);
			continue;
		} else if (dhd_prot_hdrpull(bus->dhd, &ifidx, pkt) != 0) {
			pktfree(bus->dhd->osh, pkt);
			continue;
		}

		/* Unlock during rx call */
		dhd_os_sdunlock(bus->dhd);
		dhd_rx_frame(bus->dhd, ifidx, pkt, 1, chan);
		dhd_os_sdlock(bus->dhd);
	}

	rxcount = maxframes - rxleft;

	/* Back off rxseq if awaiting rtx, update rx_seq */
	if (bus->rxskip) rxseq--;
	bus->rx_seq = rxseq;

	return rxcount;
}

static uint32 dhdsdio_hostmail(dhd_bus_t *bus)
{
	sdpcmd_regs_t *regs = bus->regs;
	uint32 intstatus = 0;
	uint32 hmb_data;
	uint8 fcbits;
	uint retries = 0;

	/* Read mailbox data and ack that we did so */
	R_SDREG(hmb_data, &regs->tohostmailboxdata, retries);
	if (retries <= retry_limit)
		W_SDREG(SMB_INT_ACK, &regs->tosbmailbox, retries);

	/* Dongle recomposed rx frames, accept them again */
	if (hmb_data & HMB_DATA_NAKHANDLED) 
    {
		if (!bus->rxskip)
            ASSERT(0);
		
		bus->rxskip = FALSE;
		intstatus |= FRAME_AVAIL_MASK(bus);
	}

	if (hmb_data & (HMB_DATA_DEVREADY | HMB_DATA_FWREADY)) 
    {
		bus->sdpcm_ver = (hmb_data & HMB_DATA_VERSION_MASK) >> HMB_DATA_VERSION_SHIFT;
		if (bus->sdpcm_ver != SDPCM_PROT_VERSION)
            ASSERT(0);
	}

	/*
	 * Flow Control has been moved into the RX headers and this out of band
	 * method isn't used any more.  Leave this here for possibly remaining backward
	 * compatible with older dongles
	 */
	if (hmb_data & HMB_DATA_FC) {
		fcbits = (hmb_data & HMB_DATA_FCDATA_MASK) >> HMB_DATA_FCDATA_SHIFT;

		if (fcbits & ~bus->flowcontrol)
			bus->fc_xoff++;
		if (bus->flowcontrol & ~fcbits)
			bus->fc_xon++;

		bus->flowcontrol = fcbits;
	}

	/* Shouldn't be any others */
	if (hmb_data & ~(HMB_DATA_DEVREADY | HMB_DATA_FWHALT | HMB_DATA_NAKHANDLED | HMB_DATA_FC |
	                 HMB_DATA_FWREADY | HMB_DATA_FCDATA_MASK | HMB_DATA_VERSION_MASK)) {
        ASSERT(0);
	}

	return intstatus;
}

int get_bus_tx_max(struct dhd_bus *bus)
{
    return bus->tx_max;
}

int get_bus_tx_seq(struct dhd_bus *bus)
{
    return bus->tx_seq;
}

static bool dhdsdio_dpc(dhd_bus_t *bus)
{
	bcmsdh_info_t *sdh = bus->sdh;
	sdpcmd_regs_t *regs = bus->regs;
	uint32 intr_status, newstatus = 0;
	uint retries = 0;
	uint rxlimit = DHD_RXBOUND; /* Rx frames to read before resched */
	uint txlimit = DHD_TXBOUND; /* Tx frames to send before resched */
	uint framecnt = 0;		  /* Temporary counter of tx/rx frames */
	bool rxdone = TRUE;		  /* Flag for no more read data */
	bool resched = FALSE;	  /* Flag indicating resched wanted */
    int ret;

	/* Start with leftover status bits */
	intr_status = bus->intstatus;

	/* If waiting for HTAVAIL, check status */
	if (bus->clkstate == CLK_PENDING) 
    {
		int err;
		uint8 clkctl, devctl = 0;
		/* Read CSR, if clock on switch to AVAIL, else ignore */
		clkctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, &err);
		if (err) 
        {
			bus->dhd->busstate = DHD_BUS_DOWN;
            ASSERT(0);
		}

		if (SBSDIO_HTAV(clkctl)) 
        {
			devctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, &err);
			if (err) 
            {
				bus->dhd->busstate = DHD_BUS_DOWN;
                ASSERT(0);
			}

			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_DEVICE_CTL, devctl, &err);
			if (err) 
            {
				bus->dhd->busstate = DHD_BUS_DOWN;
                ASSERT(0);
			}
			bus->clkstate = CLK_AVAIL;
		} 
        else 
			goto clkwait;
	}

	BUS_WAKE(bus);

	/* Make sure backplane clock is on */
	dhdsdio_clkctl(bus, CLK_AVAIL, TRUE);
	if (bus->clkstate != CLK_AVAIL)
		goto clkwait;

	/* Pending interrupt indicates new device status */
	if (bus->ipend) 
    {
		bus->ipend = FALSE;
		R_SDREG(newstatus, &regs->intstatus, retries);
		if (bcmsdh_regfail(bus->sdh))
			newstatus = 0;

		newstatus &= bus->hostintmask;
		bus->fcstate = !!(newstatus & I_HMB_FC_STATE);
		if (newstatus) 
        {
			if ((bus->rxint_mode != SDIO_DEVICE_RXDATAINT_MODE_0) || (newstatus != I_XMTDATA_AVAIL)) 
                W_SDREG(newstatus, &regs->intstatus, retries);
		}
	}

	/* Merge new bits with previous */
	intr_status |= newstatus;
	bus->intstatus = 0;

	/* Handle flow-control change: read new state in case our ack
	 * crossed another change interrupt.  If change still set, assume
	 * FC ON for safety, let next loop through do the debounce.
	 */
	if (intr_status & I_HMB_FC_CHANGE) {
		intr_status &= ~I_HMB_FC_CHANGE;
		W_SDREG(I_HMB_FC_CHANGE, &regs->intstatus, retries);
		R_SDREG(newstatus, &regs->intstatus, retries);
		bus->fcstate = !!(newstatus & (I_HMB_FC_STATE | I_HMB_FC_CHANGE));
		intr_status |= (newstatus & bus->hostintmask);
	}

	if (intr_status & I_CHIPACTIVE)
		intr_status &= ~I_CHIPACTIVE;

	if (intr_status & I_HMB_HOST_INT) { /* Handle host mailbox indication */
		intr_status &= ~I_HMB_HOST_INT;
		intr_status |= dhdsdio_hostmail(bus);
	}

	if (intr_status & I_WR_OOSYNC)
    {
		intr_status &= ~I_WR_OOSYNC;
        ASSERT(0);
    }

	if (intr_status & I_RD_OOSYNC)
    {
		intr_status &= ~I_RD_OOSYNC;
        ASSERT(0);
    }

	if (intr_status & I_SBINT) 
    {
		intr_status &= ~I_SBINT;
        ASSERT(0);
    }

	if (intr_status & I_CHIPACTIVE)
		intr_status &= ~I_CHIPACTIVE;

	if (bus->rxskip) /* Ignore frame indications if rxskip is set */
		intr_status &= ~FRAME_AVAIL_MASK(bus);

	if (PKT_AVAILABLE(bus, intr_status)) 
    { /* On frame indication, read available frames */
		framecnt = dhdsdio_readframes(bus, rxlimit, &rxdone);
		if (rxdone || bus->rxskip)
			intr_status  &= ~FRAME_AVAIL_MASK(bus);

        rxlimit -= MIN(framecnt, rxlimit);
	}

	/* Keep still-pending events for next scheduling */
	bus->intstatus = intr_status;

clkwait:
	bcmsdh_intr_enable(sdh);

	if (DATAOK(bus) && bus->ctrl_frame_stat && (bus->clkstate == CLK_AVAIL))  
    {
		int ret, i;
		ret = dhd_bcmsdh_send_buf(bus, bcmsdh_cur_sbwad(sdh), SDIO_FUNC_2, F2SYNC, (uint8 *)bus->ctrl_frame_buf, (uint32)bus->ctrl_frame_len, NULL);

		if (ret < 0) /* On failure, abort the command and terminate the frame */
        {
            prints1("send sdio2 error!\r\n");
			bcmsdh_abort(sdh, SDIO_FUNC_2);
			bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_FRAMECTRL, SFC_WF_TERM, NULL);

			for (i = 0; i < 3; i++) 
            {
				uint8 hi, lo;
				hi = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_WFRAMEBCHI, NULL);
				lo = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_WFRAMEBCLO, NULL);
				if ((hi == 0) && (lo == 0)) break;
			}
		}

		if (ret == 0)
			bus->tx_seq = (bus->tx_seq + 1) % SDPCM_SEQUENCE_WRAP;

		bus->ctrl_frame_stat = FALSE;
		dhd_wait_event_wakeup(bus->dhd);
	}
	
	/* Resched if events or tx frames are pending, else await next interrupt */
	/* On failed register access, all bets are off: no resched or interrupts */
	if ((bus->dhd->busstate == DHD_BUS_DOWN) || bcmsdh_regfail(sdh)) 
    {
		bus->dhd->busstate = DHD_BUS_DOWN;
		bus->intstatus = 0;
	} 
    else if (bus->clkstate == CLK_PENDING) 
    {
		 /* 
         * Awaiting I_CHIPACTIVE; don't resched 
         */
	} 
    else if (bus->intstatus || bus->ipend || (!bus->fcstate && pktq_mlen(&bus->txq, ~bus->flowcontrol) && DATAOK(bus)) || PKT_AVAILABLE(bus, bus->intstatus)) 
    {
		resched = TRUE; /* Read multiple frames */
	}

	bus->dpc_sched = resched;
	/* If we're done for now, turn off clock request. */
	if ((bus->idletime == DHD_IDLE_IMMEDIATE) && (bus->clkstate != CLK_PENDING)) {
		bus->activity = FALSE; /* default not come in */
		dhdsdio_clkctl(bus, CLK_NONE, FALSE);
	}

	return resched;
}

bool dhd_bus_dpc(struct dhd_bus *bus)
{
	bool resched;

	dhd_os_sdlock(bus->dhd);

	if(!bus->turnOnINT) //Fred to turn off INT and it will be turned on again in dhdsdio_dpc().
    { 
		bcmsdh_intr_disable(bus->sdh);
		bus->intdis = TRUE;
		bus->turnOnINT = FALSE;
	}

    resched = dhdsdio_dpc(bus);
	dhd_os_sdunlock(bus->dhd);
	return resched;
}

int wl_drv_rx_pkt_if (void * pkt, unsigned int buffer_len)
{
	if ( buffer_len < 8) 
    {
        ASSERT(0);
		return -1;
	}
	
    _wifi_rx(pkt, buffer_len);
	return 0;
}

int read_int_state(dhd_bus_t *bus)
{
    int intstatus;
    int ret;

	dhd_os_sdlock(bus->dhd);
    intstatus = bcmsdh_cfg_read(SDIO_FUNC_0, SDIOD_CCCR_INTPEND, NULL);
    ret = (intstatus & (INTR_STATUS_FUNC1 | INTR_STATUS_FUNC2));
    bus->ipend = TRUE;
	dhd_os_sdunlock(bus->dhd);
    return ret;
}

static bool dhdsdio_chipmatch(uint16 chipid)
{
	if (chipid == BCM4325_CHIP_ID)
		return TRUE;
	if (chipid == BCM4329_CHIP_ID)
		return TRUE;
	if (chipid == BCM4315_CHIP_ID)
		return TRUE;
	if (chipid == BCM4319_CHIP_ID)
		return TRUE;
	if (chipid == BCM4336_CHIP_ID)
		return TRUE;
	if (chipid == BCM4330_CHIP_ID)
		return TRUE;
	if (chipid == BCM43237_CHIP_ID)
		return TRUE;
	if (chipid == BCM43362_CHIP_ID)
		return TRUE;
	if (chipid == BCM43239_CHIP_ID)
		return TRUE;
	return FALSE;
}

dhd_bus_t g_bus;
void * dhdsdio_probe(void *regsva, osl_t * osh, void *sdh)
{
	int ret;
	dhd_bus_t *bus = &g_bus;
	dhd_cmn_t *cmn;

	/* Init global variables at run-time, not as part of the declaration.
	 * This is required to support init/de-init of the driver. Initialization
	 * of globals as part of the declaration results in non-deterministic
	 * behavior since the value of the globals may be different on the
	 * first time that the driver is initialized vs subsequent initializations.
	 */
	retrydata = FALSE;
	dhd_dongle_memsize = 0;
	
	bzero(bus, sizeof(dhd_bus_t));
	bus->sdh = sdh;
	bus->bus = SDIO_BUS;
	bus->tx_seq = SDPCM_SEQUENCE_WRAP - 1;

	/* attach the common module */
	if (!(cmn = dhd_common_init(osh))) {
        ASSERT(0);
		goto fail;
	}

	/* alp & soc attach */
	if (!(dhdsdio_probe_attach(bus, osh, sdh, regsva))) {
		dhd_common_deinit(NULL, cmn);
        ASSERT(0);
		goto fail;
	}

	/* Attach to the dhd/OS/network interface */
	if (!(bus->dhd = dhd_attach(osh, bus, SDPCM_RESERVE))) {
        ASSERT(0);
		goto fail;
	}

	bus->dhd->cmn = cmn;
	cmn->dhd = bus->dhd;

	if (!(dhdsdio_probe_malloc(bus, osh))) {
        ASSERT(0);
		goto fail;
	}

	if (!(dhdsdio_probe_init(bus, osh, sdh))) {
        ASSERT(0);
		goto fail;
	}

	return bus;

fail:
	dhdsdio_release(bus, osh);
	return NULL;
}

static unsigned char g_cis_buf[SBSDIO_CIS_SIZE_LIMIT * (SDIOD_MAX_IOFUNCS+1)];
static bool dhdsdio_probe_attach(struct dhd_bus *bus, osl_t *osh, void *sdh, void *regsva)
{
	int err = 0;
	uint8 clkctl = 0;
    uint fn, numfn;
    uint8 *cis[SDIOD_MAX_IOFUNCS];

	bus->alp_only = TRUE;

	/* Return the window to backplane enumeration space for core access */
	if (dhdsdio_set_siaddr_window(bus, SI_ENUM_BASE))
        ASSERT(0);

	/* Force PLL off until si_attach() programs PLL control regs */
	bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, DHD_INIT_CLKCTL1, &err);
	if (!err) clkctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, &err);

	if (err || ((clkctl & ~SBSDIO_AVBITS) != DHD_INIT_CLKCTL1)) {
        ASSERT(0);
		goto fail;
	}

    numfn = bcmsdh_query_iofnum();
    ASSERT(numfn <= SDIOD_MAX_IOFUNCS);

    /* Make sure ALP is available before trying to read CIS */
    SPIN_WAIT(((clkctl = bcmsdh_cfg_read(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, NULL)), !SBSDIO_ALPAV(clkctl)), PMU_MAX_TRANSITION_DLY);

    /* Now request ALP be put on the bus */
    bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, DHD_INIT_CLKCTL2, &err);
    OSL_DELAY(65);

    for (fn = 0; fn <= numfn; fn++) 
    {
        cis[fn] = &g_cis_buf[SBSDIO_CIS_SIZE_LIMIT * fn];
        bzero(cis[fn], SBSDIO_CIS_SIZE_LIMIT);

        if (err = bcmsdh_cis_read(fn, cis[fn], SBSDIO_CIS_SIZE_LIMIT)) 
        {
            ASSERT(0);
            break;
        }
    }

    while (fn-- > 0)
        ASSERT(cis[fn]);

    if (err) goto fail;

	/* si_attach() will provide an SI handle and scan the backplane */
	if (!(bus->sih = si_attach(osh, regsva, SDIO_BUS, sdh, &bus->vars, &bus->varsz))) {
        ASSERT(0);
		goto fail;
	}

	if (!dhdsdio_chipmatch((uint16)bus->sih->chip)) {
        ASSERT(0);
		goto fail;
	}

	si_sdiod_drive_strength_init(bus->sih, osh, 6); /* default value */

	/* Get info on the ARM and SOCRAM cores... */
    if ((si_setcore(bus->sih, ARM7S_CORE_ID, 0)) || (si_setcore(bus->sih, ARMCM3_CORE_ID, 0))) {
        bus->armrev = si_corerev(bus->sih);
    } else {
        ASSERT(0);
        goto fail;
    }

    if (!(bus->orig_ramsize = si_socram_size(bus->sih))) {
        ASSERT(0);
        goto fail;
    }

    bus->ramsize = bus->orig_ramsize;
    if (dhd_dongle_memsize)
    {
        dhd_dongle_setmemsize(bus, dhd_dongle_memsize);
    }

	/* ...but normally deal with the SDPCMDEV core */
	if (!(bus->regs = si_setcore(bus->sih, PCMCIA_CORE_ID, 0)) &&
	    !(bus->regs = si_setcore(bus->sih, SDIOD_CORE_ID, 0))) {
        ASSERT(0);
		goto fail;
	}

	bus->sdpcmrev = si_corerev(bus->sih);

	/* Set core control so an SDIO reset does a backplane reset */
	OR_REG(osh, &bus->regs->corecontrol, CC_BPRESEN);

	bus->rxint_mode = SDIO_DEVICE_HMB_RXINT;

	if ((bus->sih->buscoretype == SDIOD_CORE_ID) && (bus->sdpcmrev >= 4) &&
		(bus->rxint_mode == SDIO_DEVICE_RXDATAINT_MODE_1))
	{
		uint32 val;

		val = R_REG(osh, &bus->regs->corecontrol);
		val &= ~CC_XMTDATAAVAIL_MODE;
		val |= CC_XMTDATAAVAIL_CTRL;
		W_REG(osh, &bus->regs->corecontrol, val);
	}

	pktq_init(&bus->txq, PRIOMASK+1, QLEN);
	pktq_init(&bus->rxq, PRIOMASK+1, QLEN);

	/* Locate an appropriately-aligned portion of hdrbuf */
	bus->rxhdr = (uint8 *)ROUNDUP((uintptr)&bus->hdrbuf[0], DHD_SDALIGN);

	/* Set the poll and/or interrupt flags */
	bus->intr = (bool)dhd_intr;
	if ((bus->poll = (bool)dhd_poll))
		bus->pollrate = 1;

	return TRUE;

fail:
	if (bus->sih != NULL)
		si_detach(bus->sih);
	return FALSE;
}


#define ROUND_UP_MARGIN	2048
static unsigned char g_databuf[MAX_DATA_BUF+4];
static unsigned char g_rxbuf[WLC_IOCTL_MAXLEN + sizeof(cdc_ioctl_t) + ROUND_UP_MARGIN + SDPCM_HDRLEN + 32 + 8];
static bool dhdsdio_probe_malloc(dhd_bus_t *bus, osl_t *osh)
{
	ASSERT(bus->dhd->maxctl);

    bus->rxblen = ROUNDUP((bus->dhd->maxctl + SDPCM_HDRLEN), DHD_ALIGNMENT) + DHD_SDALIGN;
    bus->rxbuf = g_rxbuf;
	bus->databuf = g_databuf;
    
	if ((uintptr)bus->databuf % DHD_SDALIGN)
		bus->dataptr = bus->databuf + (DHD_SDALIGN - ((uintptr)bus->databuf % DHD_SDALIGN));
	else
		bus->dataptr = bus->databuf;

	return TRUE;
}

static bool dhdsdio_probe_init(dhd_bus_t *bus, osl_t *osh, void *sdh)
{
	int32 fnum;

	/* Disable F2 to clear any intermediate frame state on the dongle */
	bcmsdh_cfg_write(SDIO_FUNC_0, SDIOD_CCCR_IOEN, SDIO_FUNC_ENABLE_1, NULL);
	bus->dhd->busstate = DHD_BUS_DOWN;
	bus->sleeping = FALSE;
	bus->rxflow = FALSE;
	bus->prev_rxlim_hit = 0;

	/* Done with backplane-dependent accesses, can drop clock... */
	bcmsdh_cfg_write(SDIO_FUNC_1, SBSDIO_FUNC1_CHIPCLKCSR, 0, NULL);

	bus->clkstate = CLK_SDONLY;
	bus->idletime = (int32)DHD_IDLETIME_TICKS;
	bus->idleclock = DHD_IDLE_ACTIVE;

	/* Query the SD clock speed */
	if (bcmsdh_iovar_op(sdh, "sd_divisor", NULL, 0, &bus->sd_divisor, sizeof(int32), FALSE) != BCME_OK) {
		bus->sd_divisor = -1;
        ASSERT(0);
	} 

	/* Query the SD bus mode */
	if (bcmsdh_iovar_op(sdh, "sd_mode", NULL, 0, &bus->sd_mode, sizeof(int32), FALSE) != BCME_OK) {
		bus->sd_mode = -1;
        ASSERT(0);
	} 

	/* Query the F2 block size, set roundup accordingly */
	fnum = 2;
	if (bcmsdh_iovar_op(sdh, "sd_blocksize", &fnum, sizeof(int32), &bus->blocksize, sizeof(int32), FALSE) != BCME_OK) {
		bus->blocksize = 0;
        ASSERT(0);
	} 

	bus->roundup = MIN(max_roundup, bus->blocksize);

	/* Query if bus module supports packet chaining, default to use if supported */
	if (bcmsdh_iovar_op(sdh, "sd_rxchain", NULL, 0, &bus->sd_rxchain, sizeof(int32), FALSE) != BCME_OK) {
		bus->sd_rxchain = FALSE;
	} 

	bus->use_rxchain = (bool)bus->sd_rxchain;
	return TRUE;
}

bool dhd_bus_download_firmware(struct dhd_bus *bus, osl_t *osh, firmware_info_t *fw_info)
{
	bool ret;

	dhdsdio_clkctl(bus, CLK_AVAIL, FALSE);
	ret = _dhdsdio_download_firmware(bus, fw_info) == 0;
	dhdsdio_clkctl(bus, CLK_SDONLY, FALSE);
	return ret;
}

/* Detach and free everything */
static void dhdsdio_release(dhd_bus_t *bus, osl_t *osh)
{
	bool dongle_isolation = FALSE;

	if (bus) 
    {
		ASSERT(osh);

		/* De-register interrupt handler */
		bcmsdh_intr_disable(bus->sdh);

		if (bus->dhd) 
        {
			dhd_common_deinit(bus->dhd, NULL);
			dongle_isolation = bus->dhd->dongle_isolation;
			dhd_detach(bus->dhd);
			dhdsdio_release_dongle(bus, osh, dongle_isolation);
			bus->dhd = NULL;
		}

		dhdsdio_release_malloc(bus, osh);
	}
}

static void dhdsdio_release_malloc(dhd_bus_t *bus, osl_t *osh)
{
	if (bus->dhd && bus->dhd->dongle_reset)
		return;

	if (bus->rxbuf) 
    {
		bus->rxctl = bus->rxbuf = NULL;
		bus->rxlen = 0;
	}

	if (bus->databuf)
		bus->databuf = NULL;

	if (bus->vars && bus->varsz)
		bus->vars = NULL;

    return ; 
}


static void dhdsdio_release_dongle(dhd_bus_t *bus, osl_t *osh, bool dongle_isolation)
{
	if (bus->dhd && bus->dhd->dongle_reset)
		return;

	if (bus->sih) 
    {
		if (bus->dhd)
			dhdsdio_clkctl(bus, CLK_AVAIL, FALSE);

		if (dongle_isolation == FALSE)
			si_watchdog(bus->sih, 4);

		if (bus->dhd)
			dhdsdio_clkctl(bus, CLK_NONE, FALSE);

		si_detach(bus->sih);
		bus->vars = NULL;
	}
}

void dhdsdio_disconnect(void *ptr)
{
	dhd_bus_t *bus = (dhd_bus_t *)ptr;

	if (bus) 
    {
		ASSERT(bus->dhd);
		dhdsdio_release(bus, bus->dhd->osh);
	}
}


/* Register/Unregister functions are called by the main DHD entry
 * point (e.g. module insertion) to link with the bus driver, in
 * order to look for or await the device.
 */

static int dhdsdio_download_code_array(struct dhd_bus *bus, block_info_t *dlarray_block_info)
{
	int bcmerror = -1;
	int offset = 0;
    int ARRAY_SIZE = dlarray_block_info->len;
    unsigned char *dlarray = dlarray_block_info->data_array;

	/* Download image */
	while ((offset + MEMBLOCK) < ARRAY_SIZE) {
		bcmerror = dhdsdio_membytes(bus, TRUE, offset, (uint8 *) (dlarray + offset), MEMBLOCK);
		if (bcmerror) {
            ASSERT(0);
			goto err;
		}

		offset += MEMBLOCK;
	}

	if (offset < ARRAY_SIZE) {
		bcmerror = dhdsdio_membytes(bus, TRUE, offset, (uint8 *) (dlarray + offset), ARRAY_SIZE - offset);
		if (bcmerror) {
            ASSERT(0);
			goto err;
		}
	}

err:
	return bcmerror;
}

static int dhdsdio_download_nvram(struct dhd_bus *bus, block_info_t *block_info)
{
	int bcmerror = -1;
	uint len;
	char memblock[MAX_NVRAMBUF_SIZE];
	char *bufp;
    int PARA_LENGTH = block_info->len;
    char *nvram_params = block_info->data_array;

    len = PARA_LENGTH;
    ASSERT(len <= MAX_NVRAMBUF_SIZE);
    memcpy(memblock, nvram_params, len);

	if (len > 0 && len < MAX_NVRAMBUF_SIZE) 
    {
		bufp = (char *)memblock;
		bufp[len] = 0;
		len = process_nvram_vars(bufp, len);
		if (len % 4) 
			len += 4 - (len % 4);
		
		bufp += len;
		*bufp++ = 0;

		if (len)
			bcmerror = dhdsdio_downloadvars(bus, memblock, len + 1);

		if (bcmerror)
			DHD_ERROR(("%s: error downloading vars: %d\n", __FUNCTION__, bcmerror));
	}
	else 
    {
		DHD_ERROR(("%s: error reading nvram file: %d\n", __FUNCTION__, len));
		bcmerror = BCME_SDIO_ERROR;
	}

err:
	return bcmerror;
}

static int _dhdsdio_download_firmware(struct dhd_bus *bus, firmware_info_t *firm_info)
{
	/* Keep arm in reset */
	if (dhdsdio_download_state(bus, TRUE)) {
        ASSERT(0);
        return -1;
	}

    if (dhdsdio_download_code_array(bus, firm_info->block_info[BLK_TYPE_DLARRAY])) {
        ASSERT(0);
        return -2;
    }

	if (dhdsdio_download_nvram(bus, firm_info->block_info[BLK_TYPE_NVRAM])) /* External nvram takes precedence if specified */
        ASSERT(0);

	if (dhdsdio_download_state(bus, FALSE)) { /* Take arm out of reset */
        ASSERT(0);
        return -4;
	}

	return 0;
}

static int dhd_bcmsdh_recv_buf(dhd_bus_t *bus, uint32 addr, uint fn, uint flags, uint8 *buf, uint nbytes, void *pkt)
{
	return bcmsdh_recv_buf(bus->sdh, addr, fn, flags, buf, nbytes, pkt);
}

static int dhd_bcmsdh_send_buf(dhd_bus_t *bus, uint32 addr, uint fn, uint flags, uint8 *buf, uint nbytes, void *pkt)
{
	return (bcmsdh_send_buf(bus->sdh, addr, fn, flags, buf, nbytes, pkt));
}

uint dhd_bus_chip(struct dhd_bus *bus)
{
	ASSERT(bus->sih != NULL);
	return bus->sih->chip;
}

void * dhd_bus_pub(struct dhd_bus *bus)
{
	return bus->dhd;
}

uint dhd_bus_hdrlen(struct dhd_bus *bus)
{
	return SDPCM_HDRLEN;
}

