#ifndef _USB_SYS_H
#define _USB_SYS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned long

#define s8 char
#define s16 short
#define s32 long

#define size_t int

#ifdef WIN32
#define inline _inline 
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

//extern u32 inet_jiffier;

#ifndef WIN32
//#include "ZA9L.h"
//#include "ZA9L_MacroDef.h"
#include "base.h"

#endif

#ifdef USB_DEBUG
#define assert(expr) \
do{	\
	if ((!(expr))) \
	{	\
		char *str = __FILE__; \
		int i; \
		for(i=strlen(str);i>=0;i--) \
			if(str[i]=='\\'||str[i]=='/') \
				break; \
		stack_print(); \
		s_uart0_printf("ASSERTION FAILED (%s) at: %s:%d\n", #expr,	\
			str+i+1, __LINE__);				\
		ScrPrint(0, 2, 0, "ASSERTION FAILED (%s) at: %s:%d\n", #expr,	\
					str+i+1, __LINE__); 			\
		asm("mov r2, #0\n bx r2");\
	}									\
} while (0)
#else/* USB_DEBUG */
#define assert(expr) do{ \
}while(0)
#endif/* USB_DEBUG */

#define __LITTLE_ENDIAN_BITFIELD

#define s80_printf  


#if defined(__LITTLE_ENDIAN_BITFIELD)
#define ftohl(l) (l)/* fat order to host order  for 4bytes*/
#define ftohs(s) (s)/* fat order to host order  for 2bytes*/
#define htofl(l) (l)/* host order to fat order for 4bytes */
#define htofs(s) (s)
#else
#endif

static inline u32 htonl(u32 l)
{
	return ((((l)&0xff)<<24)|(((l)&0xff00)<<8)|
					(((l)&0xff0000)>>8)|(((l)&0xff000000)>>24));
}
static inline u16 htons(u16 s)
{
	return (u16)((((s)&0xff)<<8)|(((s)&0xff00)>>8));
}
#define ntohl(l) htonl(l)
#define ntohs(s) htons(s)


#endif/* _USB_SYS_H */
