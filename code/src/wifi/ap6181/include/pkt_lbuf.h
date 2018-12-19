/*
 * Network interface packet buffer routines. These are used internally by the
 * driver to allocate/de-allocate packet buffers, and manipulate the buffer
 * contents and attributes.
 *
 * This implementation is specific to LBUF (linked buffer) packet buffers.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: pkt_lbuf.h,v 1.4 2009-10-06 23:59:27 Exp $
 */


#ifndef _pkt_lbuf_h_
#define _pkt_lbuf_h_

/* Handle for packet buffer module. */
typedef int pkt_handle_t;

/* Macros to allocate/de-allocate packet buffers, and manipulate the buffer
 * contents and attributes.
 */
extern void *pktget(osl_t *osh, uint len, bool send);
extern void pktfree(osl_t *osh, struct lbuf * lb);
extern void *pktcfgget(osl_t *osh, uint len);

#define PKTSETLEN(osh, lb, len)		pktsetlen((osh), (lb), (len))
#define PKTPUSH(osh, lb, bytes)		pktpush((osh), (lb), (bytes))
#define PKTPULL(osh, lb, bytes)		pktpull((osh), (lb), (bytes))
extern void pkt_set_headroom(bool tx, unsigned int headroom);

#define PKTDATA(osh, lb)		((struct lbuf *)(lb))->data
#define PKTLEN(osh, lb)			((struct lbuf *)(lb))->len
#define PKTHEADROOM(osh, lb)		(PKTDATA(osh, lb)-(((struct lbuf *)(lb))->head))
#define PKTTAILROOM(osh, lb)		((((struct lbuf *)(lb))->end)-(((struct lbuf *)(lb))->tail))
#define PKTNEXT(osh, lb)		((struct lbuf *)(lb))->next
#define PKTSETNEXT(osh, lb, x)		((struct lbuf *)(lb))->next = (struct lbuf*)(x)
#define PKTTAG(lb)			(((void *) ((struct lbuf *)(lb))->pkttag))
#define PKTLINK(lb)			((struct lbuf *)(lb))->link
#define PKTSETLINK(lb, x)		((struct lbuf *)(lb))->link = (struct lbuf*)(x)
#define PKTPRIO(lb)			((struct lbuf *)(lb))->priority
#define PKTSETPRIO(lb, x)		((struct lbuf *)(lb))->priority = (x)
#define PKTSETPOOL(osh, lb, x, y)	do {} while (0)
#define PKTPOOL(osh, lb)		FALSE

/* ---- Variable Externs ------------------------------------------------- */
/* ---- Function Prototypes ---------------------------------------------- */

/****************************************************************************
* Function:   pkt_init
*
* Purpose:    Initialize packet buffer module.
*
* Parameters: osh (mod) Operating system handle.
*
* Returns:    Packet buffer module handle.
*****************************************************************************
*/
pkt_handle_t* pkt_init(osl_t *osh);

/****************************************************************************
* Function:   pkt_deinit
*
* Purpose:    De-initialize packet buffer module.
*
* Parameters: osh      (mod) Operating system handle.
*             pkt_info (mod) Packet buffer module handle.
*
* Returns:    Nothing.
*****************************************************************************
*/
void pkt_deinit(osl_t *osh, pkt_handle_t *pkt_info);

/****************************************************************************
* Function:   pkt_free_native
*
* Purpose:    Free network interface packet.
*
* Parameters: native_pkt (in) Packet to free.
*
* Returns:    0 on success, else -1.
*****************************************************************************
*/
static INLINE void pktsetlen(osl_t *osh, struct lbuf *lb, uint len)
{
	ASSERT(len + PKTHEADROOM(osh, lb) <= LBUFSZ - 32);
	ASSERT(len >= 0);

	lb->len = len;
	lb->tail = lb->data + len;
}

static INLINE uchar* pktpush(osl_t *osh, struct lbuf *lb, uint bytes)
{
	if (bytes) 
    {
		ASSERT(PKTHEADROOM(osh, lb) >= bytes);
		lb->data -= bytes;
		lb->len += bytes;
	}

	return (lb->data);
}

static INLINE uchar* pktpull(osl_t *osh, struct lbuf *lb, uint bytes)
{
	if (bytes) 
    {
		ASSERT(bytes <= lb->len);

		lb->data += bytes;
		lb->len -= bytes;
	}

	return (lb->data);
}

#endif  /* _pkt_lbuf_h_  */
