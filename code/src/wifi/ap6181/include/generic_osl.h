/*
 * Generic OS Support Layer
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: generic_osl.h,v 1.6 2009-05-14 22:33:52 Exp $
 */


#ifndef generic_osl_h
#define generic_osl_h

#ifdef __cplusplus
extern "C" {
#endif

#include <typedefs.h>
#include <lbuf.h>
#include <string.h>
#include <stdio.h>


/* --------------------------------------------------------------------------
 *  ASSERT
 */

extern void osl_assert(char *exp, char *file, int line);
//#define ASSERT(exp) \
//		do { if (!(exp)) while(1) osl_assert(#exp, __FILE__, __LINE__); } while (0)
#define ASSERT(exp) 

/* Helper macros for unsupported functionality. */
static INLINE int NU_OSL_STUB_INT(int ret)
{
	return (ret);
}

static INLINE void* NU_OSL_STUB_VOID_PTR(void* ret)
{
	return (ret);
}


/* --------------------------------------------------------------------------
 * Network interface packet buffer macros.
 *
 *  This needs to be included after the ASSERT macros because it includes
 * static include functions that use the ASSERT macro.
 */
#include <pkt_lbuf.h>


/* --------------------------------------------------------------------------
 * Printf
 */

/* Logging functions used by the Mobile Communications BSP. */
#ifdef BWL_MOBCOM_DBGPRINTF
#include <stdio.h>
int fprintf_bsp(FILE *stream, const char *fmt, ...);
int printf_bsp(const char *fmt, ...);
#define fprintf fprintf_bsp
#define printf printf_bsp
#define fputs(str, stream) 		printf("%s", str)
#define fflush(stream)
#endif   /* BWL_MOBCOM_DBGPRINTF */

/* --------------------------------------------------------------------------
** OS abstraction APIs.
*/

typedef struct osl_pubinfo {
	unsigned int pktalloced;	/* Number of allocated packet buffers */

} osl_pubinfo_t;


/* microsecond delay */
#define	OSL_DELAY(usec)		osl_delay(usec)
extern void osl_delay(uint usec);


/* map from internal BRCM error code to OS error code. */
#define OSL_ERROR(bcmerror) ((bcmerror) < 0 ? -1 : 0)

osl_t* os_resource_init(void);
void os_resource_free(osl_t *osh);


/* --------------------------------------------------------------------------
 * Hardware/architecture APIs.
 */

/* NOT SUPPORTED. */
/* Map/unmap physical to virtual - NOT SUPPORTED. */
#define REG_MAP(pa, size)	NU_OSL_STUB_VOID_PTR(NULL)
#define REG_UNMAP(va)		NU_OSL_STUB_VOID_PTR(NULL)


/* --------------------------------------------------------------------------
 * Register access macros.
 */
#include <bcmsdh.h>
#define R_REG(osh, r) (\
	sizeof(*(r)) == sizeof(uint8) ? \
		(uint8)(bcmsdh_reg_read(NULL, (uint32)r, sizeof(*(r))) & 0xff) : \
	sizeof(*(r)) == sizeof(uint16) ? \
		(uint16)(bcmsdh_reg_read(NULL, (uint32)r, sizeof(*(r))) & 0xffff) : \
	bcmsdh_reg_read(NULL, (uint32)r, sizeof(*(r))))
#define	W_REG(osh, r, v)	bcmsdh_reg_write(NULL, (uint32)r, sizeof(*(r)), (v))


/* bcopy, bcmp, and bzero */
#define bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define bzero(b, len)		memset((b), 0, (len))

#ifdef __cplusplus
	}
#endif

#endif  /* generic_osl_h  */

