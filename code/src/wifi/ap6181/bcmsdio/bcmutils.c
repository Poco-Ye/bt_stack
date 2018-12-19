/*
 * Driver O/S-independent utility routines
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 * $Id: bcmutils.c,v 1.277.2.18 2011-01-26 02:32:08 Exp $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <stdarg.h>
#include <osl.h>
#include <generic_osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <bcmdevs.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/bcmip.h>
#include <proto/802.1d.h>
#include <proto/802.11.h>

/* copy a buffer into a pkt buffer chain */
uint pktfrombuf(osl_t *osh, void *p, uint offset, int len, uchar *buf)
{
	uint n, ret = 0;

	/* skip 'offset' bytes */
	for (; p && offset; p = PKTNEXT(osh, p)) {
		if (offset < (uint)PKTLEN(osh, p))
			break;
		offset -= PKTLEN(osh, p);
	}

	if (!p)
		return 0;

	/* copy the data */
	for (; p && len; p = PKTNEXT(osh, p)) {
		n = MIN((uint)PKTLEN(osh, p) - offset, (uint)len);
		bcopy(buf, PKTDATA(osh, p) + offset, n);
		buf += n;
		len -= n;
		ret += n;
		offset = 0;
	}

	return ret;
}


/* return total length of buffer chain */
uint pkttotlen(osl_t *osh, void *p)
{
	uint total;

	total = 0;
	for (; p; p = PKTNEXT(osh, p))
		total += PKTLEN(osh, p);
	return (total);
}

/*
 * osl multiple-precedence packet queue
 * hi_prec is always >= the number of the highest non-empty precedence
 */
void * pktq_penq(struct pktq *pq, int prec, void *p)
{
	struct pktq_prec *q;

	ASSERT(prec >= 0 && prec < pq->num_prec);
	ASSERT(PKTLINK(p) == NULL);         /* queueing chains not allowed */
	ASSERT(!pktq_full(pq));
	ASSERT(!pktq_pfull(pq, prec));

	q = &pq->q[prec];

	if (q->head)
		PKTSETLINK(q->tail, p);
	else
		q->head = p;

	q->tail = p;
	q->len++;

	pq->len++;

	if (pq->hi_prec < prec)
		pq->hi_prec = (uint8)prec;

	return p;
}

void * pktq_pdeq(struct pktq *pq, int prec)
{
	struct pktq_prec *q;
	void *p;

	ASSERT(prec >= 0 && prec < pq->num_prec);

	q = &pq->q[prec];

	if ((p = q->head) == NULL)
		return NULL;

	if ((q->head = PKTLINK(p)) == NULL)
		q->tail = NULL;

	q->len--;
	pq->len--;

	PKTSETLINK(p, NULL);

	return p;
}

void * pktq_pdeq_tail(struct pktq *pq, int prec)
{
	struct pktq_prec *q;
	void *p, *prev;

	ASSERT(prec >= 0 && prec < pq->num_prec);

	q = &pq->q[prec];

	if ((p = q->head) == NULL)
		return NULL;

	for (prev = NULL; p != q->tail; p = PKTLINK(p))
		prev = p;

	if (prev)
		PKTSETLINK(prev, NULL);
	else
		q->head = NULL;

	q->tail = prev;
	q->len--;

	pq->len--;

	return p;
}

void pktq_pflush(osl_t *osh, struct pktq *pq, int prec, bool dir, ifpkt_cb_t fn, int arg)
{
	struct pktq_prec *q;
	void *p, *prev = NULL;

	q = &pq->q[prec];
	p = q->head;

	while (p) 
    {
		if (fn == NULL || (*fn)(p, arg)) 
        {
			bool head = (p == q->head);
			if (head)
				q->head = PKTLINK(p);
			else
				PKTSETLINK(prev, PKTLINK(p));
			PKTSETLINK(p, NULL);
			pktfree(osh, p);
			q->len--;
			pq->len--;
			p = (head ? q->head : PKTLINK(prev));
		} 
        else 
        {
			prev = p;
			p = PKTLINK(p);
		}
	}

	if (q->head == NULL) {
		ASSERT(q->len == 0);
		q->tail = NULL;
	}
}

void pktq_init(struct pktq *pq, int num_prec, int max_len)
{
	int prec;

	ASSERT(num_prec > 0 && num_prec <= PKTQ_MAX_PREC);

	/* pq is variable size; only zero out what's requested */
	bzero(pq, OFFSETOF(struct pktq, q) + (sizeof(struct pktq_prec) * num_prec));

	pq->num_prec = (uint16)num_prec;
	pq->max = (uint16)max_len;

	for (prec = 0; prec < num_prec; prec++)
		pq->q[prec].max = pq->max;
}

void * pktq_deq(struct pktq *pq, int *prec_out)
{
	struct pktq_prec *q;
	void *p;
	int prec;

	if (pq->len == 0)
		return NULL;

	while ((prec = pq->hi_prec) > 0 && pq->q[prec].head == NULL)
		pq->hi_prec--;

	q = &pq->q[prec];

	if ((p = q->head) == NULL)
		return NULL;

	if ((q->head = PKTLINK(p)) == NULL)
		q->tail = NULL;

	q->len--;

	pq->len--;

	if (prec_out)
		*prec_out = prec;

	PKTSETLINK(p, NULL);

	return p;
}

void * pktq_deq_tail(struct pktq *pq, int *prec_out)
{
	struct pktq_prec *q;
	void *p, *prev;
	int prec;

	if (pq->len == 0)
		return NULL;

	for (prec = 0; prec < pq->hi_prec; prec++)
		if (pq->q[prec].head)
			break;

	q = &pq->q[prec];

	if ((p = q->head) == NULL)
		return NULL;

	for (prev = NULL; p != q->tail; p = PKTLINK(p))
		prev = p;

	if (prev)
		PKTSETLINK(prev, NULL);
	else
		q->head = NULL;

	q->tail = prev;
	q->len--;

	pq->len--;

	if (prec_out)
		*prec_out = prec;

	PKTSETLINK(p, NULL);

	return p;
}

void * pktq_peek(struct pktq *pq, int *prec_out)
{
	int prec;

	if (pq->len == 0)
		return NULL;

	while ((prec = pq->hi_prec) > 0 && pq->q[prec].head == NULL)
		pq->hi_prec--;

	if (prec_out)
		*prec_out = prec;

	return (pq->q[prec].head);
}

void * pktq_peek_tail(struct pktq *pq, int *prec_out)
{
	int prec;

	if (pq->len == 0)
		return NULL;

	for (prec = 0; prec < pq->hi_prec; prec++)
		if (pq->q[prec].head)
			break;

	if (prec_out)
		*prec_out = prec;

	return (pq->q[prec].tail);
}

void pktq_flush(osl_t *osh, struct pktq *pq, bool dir, ifpkt_cb_t fn, int arg)
{
	int prec;

	for (prec = 0; prec < pq->num_prec; prec++)
		pktq_pflush(osh, pq, prec, dir, fn, arg);

	if (fn == NULL)
		ASSERT(pq->len == 0);
}

/* Return sum of lengths of a specific set of precedences */
int pktq_mlen(struct pktq *pq, uint prec_bmp)
{
	int prec, len;

	len = 0;

	for (prec = 0; prec <= pq->hi_prec; prec++)
		if (prec_bmp & (1 << prec))
			len += pq->q[prec].len;

	return len;
}

/* Priority dequeue from a specific set of precedences */
void * pktq_mdeq(struct pktq *pq, uint prec_bmp, int *prec_out)
{
	struct pktq_prec *q;
	void *p;
	int prec;

	if (pq->len == 0)
		return NULL;

	while ((prec = pq->hi_prec) > 0 && pq->q[prec].head == NULL)
		pq->hi_prec--;

	while ((prec_bmp & (1 << prec)) == 0 || pq->q[prec].head == NULL)
		if (prec-- == 0)
			return NULL;

	q = &pq->q[prec];

	if ((p = q->head) == NULL)
		return NULL;

	if ((q->head = PKTLINK(p)) == NULL)
		q->tail = NULL;

	q->len--;

	if (prec_out)
		*prec_out = prec;

	pq->len--;

	PKTSETLINK(p, NULL);

	return p;
}

#define xToLower(C) \
	((C >= 'A' && C <= 'Z') ? (char)((int)C - (int)'A' + (int)'a') : C)


/****************************************************************************
* Function:   bcmstricmp
*
* Purpose:    Compare to strings case insensitively.
*
* Parameters: s1 (in) First string to compare.
*             s2 (in) Second string to compare.
*
* Returns:    Return 0 if the two strings are equal, -1 if t1 < t2 and 1 if
*             t1 > t2, when ignoring case sensitivity.
*****************************************************************************
*/
int bcmstricmp(const char *s1, const char *s2)
{
	char dc, sc;

	while (*s2 && *s1) {
		dc = xToLower(*s1);
		sc = xToLower(*s2);
		if (dc < sc) return -1;
		if (dc > sc) return 1;
		s1++;
		s2++;
	}

	if (*s1 && !*s2) return 1;
	if (!*s1 && *s2) return -1;
	return 0;
}


/****************************************************************************
* Function:   bcmstrnicmp
*
* Purpose:    Compare to strings case insensitively, upto a max of 'cnt'
*             characters.
*
* Parameters: s1  (in) First string to compare.
*             s2  (in) Second string to compare.
*             cnt (in) Max characters to compare.
*
* Returns:    Return 0 if the two strings are equal, -1 if t1 < t2 and 1 if
*             t1 > t2, when ignoring case sensitivity.
*****************************************************************************
*/
int bcmstrnicmp(const char* s1, const char* s2, int cnt)
{
	char dc, sc;

	while (*s2 && *s1 && cnt) {
		dc = xToLower(*s1);
		sc = xToLower(*s2);
		if (dc < sc) return -1;
		if (dc > sc) return 1;
		s1++;
		s2++;
		cnt--;
	}

	if (!cnt) return 0;
	if (*s1 && !*s2) return 1;
	if (!*s1 && *s2) return -1;
	return 0;
}

/* Takes an Ethernet frame and sets out-of-bound PKTPRIO.
 * Also updates the inplace vlan tag if requested.
 * For debugging, it returns an indication of what it did.
 */
uint pktsetprio(void *pkt, bool update_vtag)
{
	struct ether_header *eh;
	struct ethervlan_header *evh;
	uint8 *pktdata;
	int priority = 0;
	int rc = 0;

	pktdata = (uint8 *) PKTDATA(NULL, pkt);
	ASSERT(ISALIGNED((uintptr)pktdata, sizeof(uint16)));

	eh = (struct ether_header *) pktdata;
    if (ntoh16(eh->ether_type) == ETHER_TYPE_8021Q) {
		uint16 vlan_tag;
		int vlan_prio, dscp_prio = 0;

		evh = (struct ethervlan_header *)eh;

		vlan_tag = ntoh16(evh->vlan_tag);
		vlan_prio = (int) (vlan_tag >> VLAN_PRI_SHIFT) & VLAN_PRI_MASK;

		if (ntoh16(evh->ether_type) == ETHER_TYPE_IP) {
			uint8 *ip_body = pktdata + sizeof(struct ethervlan_header);
			uint8 tos_tc = IP_TOS46(ip_body);
			dscp_prio = (int)(tos_tc >> IPV4_TOS_PREC_SHIFT);
		}

		/* DSCP priority gets precedence over 802.1P (vlan tag) */
		if (dscp_prio != 0) {
			priority = dscp_prio;
			rc |= PKTPRIO_VDSCP;
		} else {
			priority = vlan_prio;
			rc |= PKTPRIO_VLAN;
		}
		/*
		 * If the DSCP priority is not the same as the VLAN priority,
		 * then overwrite the priority field in the vlan tag, with the
		 * DSCP priority value. This is required for Linux APs because
		 * the VLAN driver on Linux, overwrites the skb->priority field
		 * with the priority value in the vlan tag
		 */
		if (update_vtag && (priority != vlan_prio)) {
			vlan_tag &= ~(VLAN_PRI_MASK << VLAN_PRI_SHIFT);
			vlan_tag |= (uint16)priority << VLAN_PRI_SHIFT;
			evh->vlan_tag = hton16(vlan_tag);
			rc |= PKTPRIO_UPD;
		}
	}
    else if (ntoh16(eh->ether_type) == ETHER_TYPE_IP) 
    {
		uint8 *ip_body = pktdata + sizeof(struct ether_header);
		uint8 tos_tc = IP_TOS46(ip_body);
		priority = (int)(tos_tc >> IPV4_TOS_PREC_SHIFT);
		rc |= PKTPRIO_DSCP;
	}

	ASSERT(priority >= 0 && priority <= MAXPRIO);
	PKTSETPRIO(pkt, priority);
	return (rc | priority);
}

static char bcm_undeferrstr[32];
static const char *bcmerrorstrtable[] = BCMERRSTRINGTABLE;

/* Convert the error codes into related error strings  */
const char * bcmerrorstr(int bcmerror)
{
	/* check if someone added a bcmerror code but forgot to add errorstring */
	ASSERT(ABS(BCME_LAST) == (ARRAYSIZE(bcmerrorstrtable) - 1));

	if (bcmerror > 0 || bcmerror < BCME_LAST) {
		snprintf(bcm_undeferrstr, sizeof(bcm_undeferrstr), "Undefined error %d", bcmerror);
		return bcm_undeferrstr;
	}

	ASSERT(strlen(bcmerrorstrtable[-bcmerror]) < BCME_STRLEN);

	return bcmerrorstrtable[-bcmerror];
}

/* iovar table lookup */
const bcm_iovar_t* bcm_iovar_lookup(const bcm_iovar_t *table, const char *name)
{
	const bcm_iovar_t *vi;
	const char *lookup_name;

	/* skip any ':' delimited option prefixes */
	lookup_name = strrchr(name, ':');
	if (lookup_name != NULL)
		lookup_name++;
	else
		lookup_name = name;

	ASSERT(table != NULL);

	for (vi = table; vi->name; vi++) {
		if (!strcmp(vi->name, lookup_name))
			return vi;
	}
	/* ran to end of table */

	return NULL; /* var name not found */
}

int bcm_iovar_lencheck(const bcm_iovar_t *vi, void *arg, int len, bool set)
{
	int bcmerror = 0;

	/* length check on io buf */
	switch (vi->type) {
	case IOVT_BOOL:
	case IOVT_INT8:
	case IOVT_INT16:
	case IOVT_INT32:
	case IOVT_UINT8:
	case IOVT_UINT16:
	case IOVT_UINT32:
		/* all integers are int32 sized args at the ioctl interface */
		if (len < (int)sizeof(int)) {
			bcmerror = BCME_BUFTOOSHORT;
		}
		break;

	case IOVT_BUFFER:
		/* buffer must meet minimum length requirement */
		if (len < vi->minlen) {
			bcmerror = BCME_BUFTOOSHORT;
		}
		break;

	case IOVT_VOID:
		if (!set) {
			/* Cannot return nil... */
			bcmerror = BCME_UNSUPPORTED;
		} else if (len) {
			/* Set is an action w/o parameters */
			bcmerror = BCME_BUFTOOLONG;
		}
		break;

	default:
		/* unknown type for length check in iovar info */
		ASSERT(0);
		bcmerror = BCME_UNSUPPORTED;
	}

	return bcmerror;
}

uint bcm_mkiovar(char *name, char *data, uint datalen, char *buf, uint buflen)
{
	uint len;

	len = strlen(name) + 1;

	if ((len + datalen) > buflen)
		return 0;

	strncpy(buf, name, buflen);

	/* append data onto the end of the name string */
	memcpy(&buf[len], data, datalen);
	len += datalen;

	return len;
}

/*
 * ProcessVars:Takes a buffer of "<var>=<value>\n" lines read from a file and ending in a NUL.
 * also accepts nvram files which are already in the format of <var1>=<value>\0\<var2>=<value2>\0
 * Removes carriage returns, empty lines, comment lines, and converts newlines to NULs.
 * Shortens buffer as needed and pads with NULs.  End of buffer is marked by two NULs.
*/

unsigned int process_nvram_vars(char *varbuf, unsigned int len)
{
	char *dp;
	bool findNewline;
	int column;
	unsigned int buf_len, n;
	unsigned int pad = 0;

	dp = varbuf;

	findNewline = FALSE;
	column = 0;

	for (n = 0; n < len; n++) {
		if (varbuf[n] == '\r')
			continue;
		if (findNewline && varbuf[n] != '\n')
			continue;
		findNewline = FALSE;
		if (varbuf[n] == '#') {
			findNewline = TRUE;
			continue;
		}
		if (varbuf[n] == '\n') {
			if (column == 0)
				continue;
			*dp++ = 0;
			column = 0;
			continue;
		}
		*dp++ = varbuf[n];
		column++;
	}
	buf_len = (unsigned int)(dp - varbuf);
	if (buf_len % 4) {
		pad = 4 - buf_len % 4;
		if (pad && (buf_len + pad <= len)) {
			buf_len += pad;
		}
	}

	while (dp < varbuf + n)
		*dp++ = 0;

	return buf_len;
}

