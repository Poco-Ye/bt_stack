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
 * $Id: pkt_lbuf.c,v 1.2 2008-12-01 22:57:20 Exp $
 */

/* ---- Include Files ---------------------------------------------------- */

#include "typedefs.h"
#include "bcmdefs.h"
#include "generic_osl.h"
#include "osl.h"
#include "lbuf.h"
#include "pkt_lbuf.h"
#include <stdlib.h>
#include <osl_ext.h>
#include <bcmutils.h>

#define irq_save(x) irq_save_asm(&x)
#define irq_restore(x) irq_restore_asm(&x)

lbuf_info_t g_lbuf_info;

static int g_cfg_lbuf[((LBUFCFGSZ+3+4)*NCFGBUF+3) / sizeof(int)];
static int g_cfg_lbuf_info[(NCFGBUF * sizeof(struct lbuf) + 3) / sizeof(int)];

static int g_ntx_lbuf[((LBUFSZ+4+4)*NTXBUF+3) / sizeof(int)];
static int g_ntx_lbuf_info[(NTXBUF * (sizeof(struct lbuf) + 3) + 3) / sizeof(int)];

static int g_nrx_lbuf[((LBUFSZ+4+4)*NRXBUF+3) / sizeof(int)];
static int g_nrx_lbuf_info[(NRXBUF * (sizeof(struct lbuf) + 3) + 3) / sizeof(int)];

pkt_handle_t* pkt_init(osl_t *osh)
{
	lbuf_info_t	*info=&g_lbuf_info;

	memset(info, 0, sizeof(lbuf_info_t));
	info->osh = osh;

	memset(&info->txfree, 0, sizeof(struct lbfree));
	memset(&info->rxfree, 0, sizeof(struct lbfree));
	memset(&info->cfgfree, 0, sizeof(struct lbfree));

    info->txfree.dbuf = (unsigned char *)g_ntx_lbuf;
    info->rxfree.dbuf = (unsigned char *)g_nrx_lbuf;
    info->cfgfree.dbuf = (unsigned char *)g_cfg_lbuf;

    info->txfree.sbuf = (unsigned char *)g_ntx_lbuf_info;
    info->rxfree.sbuf = (unsigned char *)g_nrx_lbuf_info;
    info->cfgfree.sbuf = (unsigned char *)g_cfg_lbuf_info;

    if (!lbuf_alloc_list(info, &(info->txfree), NTXBUF)) ASSERT(0);
    if (!lbuf_alloc_list(info, &(info->rxfree), NRXBUF)) ASSERT(0);
	if (!lbuf_alloc_list(info, &(info->cfgfree), NCFGBUF)) ASSERT(0);
	
	return ((pkt_handle_t*)info);
}


void pkt_deinit(osl_t *osh, pkt_handle_t *pkt_info)
{
	lbuf_info_t	*info = (lbuf_info_t*)pkt_info;

	lbuf_free_list(info, &(info->rxfree));
	lbuf_free_list(info, &(info->txfree));
	lbuf_free_list(info, &(info->cfgfree));
}

/* ----------------------------------------------------------------------- */
/* Converts a native (network interface) packet to driver packet.
 * Allocates a new lbuf and copies the contents
 */
void * pkt_frmnative(osl_t *osh, void *native_pkt, int len)
{
	struct lbuf* lb;
	lbuf_info_t *info = &g_lbuf_info;
	struct lbfree *list = &info->txfree;

    ASSERT(len < LBUFSZ - (12 + 32));
	ASSERT(osh);

	if ((lb = lbuf_get(list)) == NULL) {
        ASSERT(0);
		return (NULL);
	}
	
	/* Adjust for the head room requested */
	ASSERT(list->size > list->headroom); /* (12 + 32 ) */
	lb->data += list->headroom;
	lb->tail += list->headroom;

    memcpy(PKTDATA(osh, lb), native_pkt, len);
	pktsetlen(osh, lb, len);

	/* Save pointer to native packet. This will be freed later by pktfree(). */
	lb->native_pkt = NULL;
	return ((void *)lb);
}

static unsigned char g_native_pkt[LBUFCFGSZ+64+8];
/* ----------------------------------------------------------------------- */
/* Converts a driver packet to a native (network interface) packet.
 */
void* pkt_tonative(osl_t *osh, void *drv_pkt)
{
	void *native_pkt;
	struct lbuf* lb = (struct lbuf*) drv_pkt;

	ASSERT(osh);

	/* Allocate network interface (native) packet. */
	native_pkt = g_native_pkt;

	/* Copy data from driver to network interface packet */
	memcpy(native_pkt, PKTDATA(osh, lb), PKTLEN(osh, lb));
	return (native_pkt);
}

void* pktget(osl_t *osh, uint len, bool send)
{
	struct lbuf	*lb;
	lbuf_info_t *info = &g_lbuf_info;

	ASSERT(osh);

	if (len > LBUFSZ - 32)
    {
        ASSERT(0);
		return (NULL);
    }

    if((lb = lbuf_get(&info->rxfree)) == NULL) ASSERT(0);

	if (lb)
		lb->len = len;

	return ((void*) lb);
}

void* pktcfgget(osl_t *osh, uint len)
{
	struct lbuf	*lb;
	lbuf_info_t *info = &g_lbuf_info;

	ASSERT(osh);

    if (len > LBUFCFGSZ - 32)
    {
        ASSERT(0);
        return (NULL);
    }
	
	if((lb = lbuf_get(&info->cfgfree)) == NULL)
        ASSERT(0);

	if (lb)
		lb->len = len;

	return ((void*) lb);
}

void pktfree(osl_t *osh, struct lbuf *lb)
{
	struct lbuf *next;
	void *native_pkt;

    ASSERT(osh);
	ASSERT(lb);

	native_pkt = lb->native_pkt;
	
	while (lb) 
    {
		next = lb->next;
		lb->next = NULL;
		lbuf_put(lb->list, lb);
		lb = next;
	}

    return ;
}

void pkt_set_headroom(bool tx, unsigned int headroom)
{
	struct lbfree *list;

	if (tx)
		list = &g_lbuf_info.txfree;
	else
		list = &g_lbuf_info.rxfree;

	list->headroom = headroom;
}

static bool lbuf_addbuf(lbuf_info_t *info, struct lbfree *list, uchar *buf, struct lbuf *lb)
{
	memset(lb, 0, sizeof(struct lbuf));

	lb->head = (uchar*)((uint32)(buf+3) & ~(size_t)0x3);
	lb->end  = buf + list->size - 4;
	lb->list = list;

	lbuf_put(list, lb);
	return (TRUE);
}

bool lbuf_alloc_list(lbuf_info_t *lbuf_info, struct lbfree *list, uint total)
{
	bool status;
	int i;
	uchar *dbuf = NULL;
	uchar *sbuf = NULL;

	list->total = total;

	if(total > 1) 
		list->size = LBUFSZ+4;
	 else 
        list->size = LBUFCFGSZ+3;	

	memset(list->dbuf, 0, (list->total * list->size));
	dbuf = list->dbuf;

	memset(list->sbuf, 0, (list->total * sizeof(struct lbuf)));
	sbuf = list->sbuf;

	for (i = 0; i < total; i++) 
    {
		status = lbuf_addbuf(lbuf_info, list, dbuf, (struct lbuf *)(&(sbuf[i * sizeof(struct lbuf)])));
		if (!status)
			goto enomem;
		else
			dbuf += list->size;
	}

	return (TRUE);

enomem:
    ASSERT(0);
	lbuf_free_list(lbuf_info, list);
	return (FALSE);

}

void lbuf_free_list(lbuf_info_t *lbuf_info, struct lbfree *list)
{
	ASSERT(list->count <= list->total);
	memset(list, 0, sizeof(struct lbfree));
	return;
}

struct lbuf * lbuf_get(struct lbfree *list)
{
	struct lbuf		*lb;
    int flag;

	ASSERT(list->count <= list->total);

    irq_save(flag);

	if (lb = list->free) {
		list->free = lb->link;
		lb->link = lb->next = NULL;
		lb->data = lb->tail = lb->head;
		lb->priority = 0;
		lb->len = 0;
		lb->native_pkt = NULL;
		list->count--;
		memset(lb->pkttag, 0, OSL_PKTTAG_SZ);
	}
    else 
    {
        ASSERT(0);
    }

    irq_restore(flag); 
	return (lb);
}

void lbuf_put(struct lbfree *list, struct lbuf *lb)
{
    int flag;

	ASSERT(list->count <= list->total);
	ASSERT(lb->link == NULL);
	ASSERT(lb->next == NULL);
	ASSERT(lb->list == list);

    irq_save(flag);
	lb->data = lb->tail = (uchar*)(uintptr)0xdeadbeef;
	lb->len = 0;
	lb->link = list->free;
	list->free = lb;
	list->count++;
    irq_restore(flag); 
}

