#include <typedefs.h>
#include <osl.h>
#include <generic_osl.h>
#include <epivers.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <dngl_stats.h>
#include <wlioctl.h>
#include <dhd.h>
#include <proto/bcmevent.h>
#include <dhd_bus.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <msgtrace.h>
#include <wlfc_proto.h>
#include <dhd_wlfc.h>
#include <bcmcdc.h>

dhd_cmn_t g_cmn;

struct dhd_cmn * dhd_common_init(osl_t *osh)
{
	dhd_cmn_t *cmn = &g_cmn;

	memset(cmn, 0, sizeof(dhd_cmn_t));
	cmn->osh = osh;
	return cmn;
}

void dhd_common_deinit(dhd_pub_t *dhd_pub, dhd_cmn_t *sa_cmn)
{
	osl_t *osh;
	dhd_cmn_t *cmn;

	if (dhd_pub != NULL)
		cmn = dhd_pub->cmn;
	else
		cmn = sa_cmn;

	if (!cmn)
		return;

	osh = cmn->osh;

	if (dhd_pub != NULL)
        dhd_pub->cmn = NULL;
}

int dhd_wl_ioctl_cmd(dhd_pub_t *dhd_pub, int cmd, void *arg, int len, uint8 set, int ifindex)
{
	wl_ioctl_t ioc;

	ioc.cmd = cmd;
	ioc.buf = arg;
	ioc.len = len;
	ioc.set = set;
	return dhd_wl_ioctl(dhd_pub, ifindex, &ioc, arg, len);
}

int dhd_wl_ioctl(dhd_pub_t *dhd_pub, int ifindex, wl_ioctl_t *ioc, void *buf, int len)
{
	int ret;

	dhd_os_proto_block(dhd_pub);
	ret = dhd_prot_ioctl(dhd_pub, ifindex, ioc, buf, len);
	dhd_os_proto_unblock(dhd_pub);
	return ret;
}

bool dhd_prec_enq(dhd_pub_t *dhdp, struct pktq *q, void *pkt, int prec)
{
	void *p;
	int eprec = -1;		/* precedence to evict from */
	bool discard_oldest;

	/* Fast case, precedence queue is not full and we are also not
	 * exceeding total queue length
	 */
	if (!pktq_pfull(q, prec) && !pktq_full(q)) {
		pktq_penq(q, prec, pkt);
		return TRUE;
	}

    ASSERT(0);
	/* Determine precedence from which to evict packet, if any */
	if (pktq_pfull(q, prec))
		eprec = prec;
	else if (pktq_full(q)) {
		p = pktq_peek_tail(q, &eprec);
		ASSERT(p);
		if (eprec > prec)
			return FALSE;
	}

	/* Evict if needed */
	if (eprec >= 0) 
    {
		/* Detect queueing to unconfigured precedence */
		ASSERT(!pktq_pempty(q, eprec));
		discard_oldest = AC_BITMAP_TST(dhdp->wme_dp, eprec);
		if (eprec == prec && !discard_oldest)
			return FALSE;		/* refuse newer (incoming) packet */

		/* Evict packet according to discard policy */
		p = discard_oldest ? pktq_pdeq(q, eprec) : pktq_pdeq_tail(q, eprec);
		ASSERT(p);
		pktfree(dhdp->osh, p);
	}

	/* Enqueue */
	p = pktq_penq(q, prec, pkt);
	ASSERT(p);
	return TRUE;
}

int wl_host_event(dhd_pub_t *dhd_pub, int *ifidx, void *pktdata, wl_event_msg_t *event, void **data_ptr)
{
	/* check whether packet is a BRCM event pkt */
	bcm_event_t *pvt_data = (bcm_event_t *)pktdata;
	char *event_data;
	uint32 type;

	if (bcmp(BRCM_OUI, (const void*)&pvt_data->bcm_hdr.oui[0], DOT11_OUI_LEN)) {
        ASSERT(0);
		return (BCME_ERROR);
	}

	/* BRCM event pkt may be unaligned - use xxx_ua to load user_subtype. */
	if (ntoh16_ua((void *)&pvt_data->bcm_hdr.usr_subtype) != BCMILCP_BCM_SUBTYPE_EVENT) {
        ASSERT(0);
		return (BCME_ERROR);
	}

	*data_ptr = (void*)&pvt_data[1];
	event_data = *data_ptr;

	/* memcpy since BRCM event pkt may be unaligned. */
	memcpy((void*)event, (const void*)&pvt_data->event, sizeof(wl_event_msg_t));

	type = ntoh32_ua((void *)&event->event_type);

	switch (type) {
	case WLC_E_FIFO_CREDIT_MAP: break;
	case WLC_E_IF: break;
	case WLC_E_NDIS_LINK:
		pvt_data->event.event_type = hton32(WLC_E_LINK);

	case WLC_E_LINK:
	case WLC_E_DEAUTH:
	case WLC_E_DEAUTH_IND:
	case WLC_E_DISASSOC:
	case WLC_E_DISASSOC_IND: /* fall through */
	default:
		if (type == WLC_E_NDIS_LINK) /* put it back to WLC_E_NDIS_LINK */
			pvt_data->event.event_type = ntoh32(WLC_E_NDIS_LINK);

		break;
	}

	return (BCME_OK);
}

void wl_event_to_host_order(wl_event_msg_t * evt)
{
	/* Event struct members passed from dongle to host are stored in network
	 * byte order. Convert all members to host-order.
	 */
	evt->event_type = ntoh32(evt->event_type);
	evt->flags = ntoh16(evt->flags);
	evt->status = ntoh32(evt->status);
	evt->reason = ntoh32(evt->reason);
	evt->auth_type = ntoh32(evt->auth_type);
	evt->datalen = ntoh32(evt->datalen);
	evt->version = ntoh16(evt->version);
}

int dhd_preinit_ioctls(dhd_pub_t *dhd)
{
	int ret = 0;
	char eventmask[WL_EVENTING_MASK_LEN];
	char iovbuf[WL_EVENTING_MASK_LEN + 12];	/*  Room for "event_msgs" + '\0' + bitvec  */
	uint up = 0;

	/* Setup event_msgs */
	bcm_mkiovar("event_msgs", eventmask, WL_EVENTING_MASK_LEN, iovbuf, sizeof(iovbuf));
	dhd_wl_ioctl_cmd(dhd, WLC_GET_VAR, iovbuf, sizeof(iovbuf), FALSE, 0);
	if (ret < 0) return ret;

	bcopy(iovbuf, eventmask, WL_EVENTING_MASK_LEN); /* Setup event_msgs */
	setbit(eventmask, WLC_E_SET_SSID);
	setbit(eventmask, WLC_E_PRUNE);
	setbit(eventmask, WLC_E_AUTH);
	setbit(eventmask, WLC_E_REASSOC);
	setbit(eventmask, WLC_E_REASSOC_IND);
	setbit(eventmask, WLC_E_DEAUTH_IND);
	setbit(eventmask, WLC_E_DISASSOC_IND);
	setbit(eventmask, WLC_E_DISASSOC);
	setbit(eventmask, WLC_E_JOIN);
	setbit(eventmask, WLC_E_ASSOC_IND);
	setbit(eventmask, WLC_E_PSK_SUP);
	setbit(eventmask, WLC_E_LINK);
	setbit(eventmask, WLC_E_NDIS_LINK);
	setbit(eventmask, WLC_E_MIC_ERROR);
	setbit(eventmask, WLC_E_PMKID_CACHE);
	setbit(eventmask, WLC_E_TXFAIL);
	setbit(eventmask, WLC_E_JOIN_START);
	setbit(eventmask, WLC_E_SCAN_COMPLETE);
	setbit(eventmask, WLC_E_ROAM);
	bcm_mkiovar("event_msgs", eventmask, WL_EVENTING_MASK_LEN, iovbuf, sizeof(iovbuf));
	dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);

	/* Force STA UP */
	ret = dhd_wl_ioctl_cmd(dhd, WLC_UP, (char *)&up, sizeof(up), TRUE, 0);
	if (ret < 0) return ret;

	return 0;
}

/* Packet alignment for most efficient SDIO (can change based on platform) */
#define RETRIES 2		/* # of retries to retrieve matching ioctl response */
#define BUS_HEADER_LEN	(16+DHD_SDALIGN)	/* Must be at least SDPCM_RESERVE
				 * defined in dhd_sdio.c (amount of header tha might be added)
				 * plus any space that might be needed for alignment padding.
				 */
#define ROUND_UP_MARGIN	2048 	/* Biggest SDIO block size possible for
				 * round off at the end of buffer
				 */

typedef struct dhd_prot {
	uint16 reqid;
	uint8 pending;
	uint32 lastcmd;
	uint8 bus_header[BUS_HEADER_LEN];
	cdc_ioctl_t msg;
	unsigned char buf[WLC_IOCTL_MAXLEN + ROUND_UP_MARGIN];
} dhd_prot_t;

static int dhdcdc_msg(dhd_pub_t *dhd)
{
	int err = 0;
	dhd_prot_t *prot = dhd->prot;
	int len = prot->msg.len + sizeof(cdc_ioctl_t);

	/* NOTE : cdc->msg.len holds the desired length of the buffer to be
	 *        returned. Only up to CDC_MAX_MSG_SIZE of this buffer area
	 *	  is actually sent to the dongle
	 */
	if (len > CDC_MAX_MSG_SIZE) /* 1518 */
		len = CDC_MAX_MSG_SIZE;

	/* Send request */

	err = dhd_bus_txctl(dhd->bus, (uchar*)&prot->msg, len);
	return err;
}

static int dhdcdc_cmplt(dhd_pub_t *dhd, uint32 id, uint32 len)
{
	int ret;
	int cdc_len = len+sizeof(cdc_ioctl_t);
	dhd_prot_t *prot = dhd->prot;

	do {
		ret = dhd_bus_rxctl(dhd->bus, (uchar*)&prot->msg, cdc_len);
		if (ret < 0) break;

	} while (CDC_IOC_ID(prot->msg.flags) != id);

	return ret;
}

int dhdcdc_query_ioctl(dhd_pub_t *dhd, int ifidx, uint cmd, void *buf, uint len, uint8 action)
{
	dhd_prot_t *prot = dhd->prot;
	cdc_ioctl_t *msg = &prot->msg;
	void *info;
	int ret = 0, retries = 0;
	uint32 id, flags = 0;

	/* Respond "bcmerror" and "bcmerrorstr" with local cache */
	if (cmd == WLC_GET_VAR && buf)
	{
		if (!strcmp((char *)buf, "bcmerrorstr"))
		{
			strncpy((char *)buf, bcmerrorstr(dhd->dongle_error), BCME_STRLEN);
            return 0;
		}
		else if (!strcmp((char *)buf, "bcmerror"))
		{
			*(int *)buf = dhd->dongle_error;
            return 0;
		}
	}

	memset(msg, 0, sizeof(cdc_ioctl_t));

	msg->cmd = cmd;
	msg->len = len;
	msg->flags = (++prot->reqid << CDCF_IOC_ID_SHIFT);
	msg->flags = ((msg->flags & ~CDCF_IOC_IF_MASK) | (ifidx << CDCF_IOC_IF_SHIFT));

	action &= WL_IOCTL_ACTION_MASK; /* add additional action bits */
	msg->flags |= (action << CDCF_IOC_ACTION_SHIFT);
	msg->flags = msg->flags;

	if (buf)
		memcpy(prot->buf, buf, len);

	if ((ret = dhdcdc_msg(dhd)) < 0) {
        ASSERT(0);
        return ret;
	}

    do { /* wait for interrupt and get first fragment */

        if ((ret = dhdcdc_cmplt(dhd, prot->reqid, len)) < 0)
            return ret;

        flags = msg->flags;
        id = (flags & CDCF_IOC_ID_MASK) >> CDCF_IOC_ID_SHIFT;

    } while((id < prot->reqid) && (++retries < RETRIES));

	if (id != prot->reqid) 
    {
        ASSERT(0);
        return -EINVAL;
	}

	/* Check info buffer */
	info = (void*)&msg[1];

	/* Copy info buffer */
	if (buf)
	{
		if (ret < (int)len)
			len = ret;
		memcpy(buf, info, len);
	}

	/* Check the ERROR flag */
	if (flags & CDCF_IOC_ERROR)
	{
		ret = msg->status;
		/* Cache error from dongle */
		dhd->dongle_error = ret;
        return ret;
	}

	return 0;
}

int dhdcdc_set_ioctl(dhd_pub_t *dhd, int ifidx, uint cmd, void *buf, uint len, uint8 action)
{
	dhd_prot_t *prot = dhd->prot;
	cdc_ioctl_t *msg = &prot->msg;
	int ret = 0;
	uint32 flags, id;

	memset(msg, 0, sizeof(cdc_ioctl_t));

	msg->cmd = cmd;
	msg->len = len;
	msg->flags = (++prot->reqid << CDCF_IOC_ID_SHIFT);
	msg->flags = ((msg->flags & ~CDCF_IOC_IF_MASK) | (ifidx << CDCF_IOC_IF_SHIFT));

	action &= WL_IOCTL_ACTION_MASK; /* add additional action bits */
	msg->flags |= (action << CDCF_IOC_ACTION_SHIFT) | CDCF_IOC_SET;
	msg->flags = msg->flags;

	if (buf)
		memcpy(prot->buf, buf, len);

	if ((ret = dhdcdc_msg(dhd)) < 0)
        return ret;

	if ((ret = dhdcdc_cmplt(dhd, prot->reqid, len)) < 0)
        return ret;

	flags = msg->flags;
	id = (flags & CDCF_IOC_ID_MASK) >> CDCF_IOC_ID_SHIFT;

	if (id != prot->reqid)
		return -EINVAL;

	if (flags & CDCF_IOC_ERROR) /* Check the ERROR flag */
	{
		ret = dhd->dongle_error = msg->status;
        return ret;
	}

	return 0;
}

int dhd_prot_ioctl(dhd_pub_t *dhd, int ifidx, wl_ioctl_t * ioc, void * buf, int len)
{
	dhd_prot_t *prot = dhd->prot;
	int ret = -1;
	uint8 action;
	wl_scan_results_t *list = NULL;
	wl_bss_info_t *bi = NULL;			
	int i;

	if (dhd->busstate == DHD_BUS_DOWN)
        return -1;

	ASSERT(len <= WLC_IOCTL_MAXLEN);

	if (prot->pending == TRUE)
        return ret;

	prot->pending = TRUE;
	prot->lastcmd = ioc->cmd;
	action = ioc->set;
	if (action & WL_IOCTL_ACTION_SET)
		ret = dhdcdc_set_ioctl(dhd, ifidx, ioc->cmd, buf, len, action);
	else 
    {
		ret = dhdcdc_query_ioctl(dhd, ifidx, ioc->cmd, buf, len, action);
		if (ret > 0) ioc->used = ret - sizeof(cdc_ioctl_t);
	}

	/* Too many programs assume ioctl() returns 0 on success */
	if (ret >= 0)
		ret = 0;
	else {
		cdc_ioctl_t *msg = &prot->msg;
		ioc->needed = msg->len; /* len == needed when set/query fails from dongle */
	}

	/* Intercept the wme_dp ioctl here */
	if ((!ret) && (ioc->cmd == WLC_SET_VAR) && (!strcmp(buf, "wme_dp"))) {
		int slen, val = 0;

		slen = strlen("wme_dp") + 1;
		if (len >= (int)(slen + sizeof(int)))
			bcopy(((char *)buf + slen), &val, sizeof(int));
		dhd->wme_dp = (uint8)val;
	}

	if(WLC_SCAN_RESULTS ==  ioc->cmd){
		list = (wl_scan_results_t *) buf;
		bi = NULL;			
		bi = list->bss_info;
		
		for (i = 0; i < list->count; i++){
			if(((bi->chanspec)&WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_40){
				if(((bi->chanspec)&WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_UPPER)
					(bi->chanspec) = (bi->chanspec) +2;	
				else if(((bi->chanspec)&WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_LOWER)
					(bi->chanspec) = (bi->chanspec) -2;	
			}
			bi = (wl_bss_info_t *) ((int8 *) bi + bi->length);
		}
   	}

	prot->pending = FALSE;
	return 0;
}

void dhd_prot_hdrpush(dhd_pub_t *dhd, int ifidx, void *pktbuf)
{
	struct bdc_header *h;

	/* Push BDC header used to convey priority for buses that don't */
	PKTPUSH(dhd->osh, pktbuf, BDC_HEADER_LEN);

	h = (struct bdc_header *)PKTDATA(dhd->osh, pktbuf);

	h->flags = (BDC_PROTO_VER << BDC_FLAG_VER_SHIFT);
	h->priority = (PKTPRIO(pktbuf) & BDC_PRIORITY_MASK);
	h->flags2 = 0;
	h->dataOffset = 0;
	h->flags2 = ((h->flags2 & ~BDC_FLAG2_IF_MASK) | (ifidx << BDC_FLAG2_IF_SHIFT));

    return ;
}

int dhd_prot_hdrpull(dhd_pub_t *dhd, int *ifidx, void *pktbuf)
{
	struct bdc_header *h;

	/* Pop BDC header used to convey priority for buses that don't */
	if (PKTLEN(dhd->osh, pktbuf) < BDC_HEADER_LEN) 
    {
        ASSERT(0);
		return BCME_ERROR;
	}

	h = (struct bdc_header *)PKTDATA(dhd->osh, pktbuf);

	if ((*ifidx = BDC_GET_IF_IDX(h)) >= DHD_MAX_IFS) 
    {
        ASSERT(0);
		return BCME_ERROR;
	}

	if (((h->flags & BDC_FLAG_VER_MASK) >> BDC_FLAG_VER_SHIFT) != BDC_PROTO_VER) 
    {
        ASSERT(0);
		if (((h->flags & BDC_FLAG_VER_MASK) >> BDC_FLAG_VER_SHIFT) == BDC_PROTO_VER_1)
			h->dataOffset = 0;
		else
			return BCME_ERROR;
	}

	PKTSETPRIO(pktbuf, (h->priority & BDC_PRIORITY_MASK));
	PKTPULL(dhd->osh, pktbuf, BDC_HEADER_LEN);

	if (PKTLEN(dhd->osh, pktbuf) < (uint32) (h->dataOffset << 2)) 
    {
        ASSERT(0);
		return BCME_ERROR;
	}

	PKTPULL(dhd->osh, pktbuf, (h->dataOffset << 2));
	return 0;
}

dhd_prot_t g_cdc;
int dhd_prot_attach(dhd_pub_t *dhd)
{
	dhd_prot_t *cdc = &g_cdc;

	memset(cdc, 0, sizeof(dhd_prot_t));

	/* ensure that the msg buf directly follows the cdc msg struct */
	if ((uintptr)(&cdc->msg + 1) != (uintptr)cdc->buf) 
    {
        ASSERT(0);
        return BCME_NOMEM;
	}

	dhd->prot = cdc;
	dhd->hdrlen += BDC_HEADER_LEN;
	dhd->maxctl = WLC_IOCTL_MAXLEN + sizeof(cdc_ioctl_t) + ROUND_UP_MARGIN;
	return 0;
}

/* ~NOTE~ What if another thread is waiting on the semaphore?  Holding it? */
void dhd_prot_detach(dhd_pub_t *dhd)
{
	dhd->prot = NULL;
}

int dhd_prot_init(dhd_pub_t *dhd)
{
	int ret = 0;
	wlc_rev_info_t revinfo;
	char buf[128];
	int retry=0;

	/* Get the device MAC address */
	do {
		strcpy(buf, "cur_etheraddr");
		ret = dhd_wl_ioctl_cmd(dhd, WLC_GET_VAR, buf, sizeof(buf), FALSE, 0);
		if (ret < 0 && retry==2) 
            return -1;

		memcpy((void*)dhd->mac.octet, buf, ETHER_ADDR_LEN);
	} while(ret && (++retry<3));

	/* Get the device rev info */
	memset(&revinfo, 0, sizeof(revinfo));
	ret = dhd_wl_ioctl_cmd(dhd, WLC_GET_REVINFO, &revinfo, sizeof(revinfo), FALSE, 0);
	if (ret < 0) return ret;
	
	ret = dhd_preinit_ioctls(dhd);
	ASSERT(!ret);
	
	return 0;
}

int dhd_set_suspend(int value, dhd_pub_t *dhd)
{
#define htod32(i) i
	int power_mode = PM_MAX;
	wl_pkt_filter_enable_t	enable_parm;
	char iovbuf[32];
	int bcn_li_dtim = 3;

	if (dhd && dhd->up) 
    {
		if (value) 
        {
			dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)&power_mode, sizeof(power_mode), TRUE, 0);

			/* Enable packet filter, only allow unicast packet to send up */
			enable_parm.id = htod32(100);
			enable_parm.enable = htod32(1);
			bcm_mkiovar("pkt_filter_enable", (char *)&enable_parm,
				sizeof(wl_pkt_filter_enable_t), iovbuf, sizeof(iovbuf));
			dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);

			/* set bcn_li_dtim */
			bcm_mkiovar("bcn_li_dtim", (char *)&bcn_li_dtim, 4, iovbuf, sizeof(iovbuf));
			dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
		} 
        else 
        {
			power_mode = PM_FAST;
			dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)&power_mode, sizeof(power_mode), TRUE, 0);

			/* disable pkt filter */
			enable_parm.id = htod32(100);
			enable_parm.enable = htod32(0);
			bcm_mkiovar("pkt_filter_enable", (char *)&enable_parm,
				sizeof(wl_pkt_filter_enable_t), iovbuf, sizeof(iovbuf));
			dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);

			/* set bcn_li_dtim */
			bcn_li_dtim = 0;
			bcm_mkiovar("bcn_li_dtim", (char *)&bcn_li_dtim, 4, iovbuf, sizeof(iovbuf));
			dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
		}
	}

	return 0;
}

