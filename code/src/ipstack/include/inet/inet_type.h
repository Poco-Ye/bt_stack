/*
090824 sunJH
增加了时间比较宏定义TM_LE
*/
#ifndef _TYPE_H
#define _TYPE_H

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

/* time compare fun: a<=b */
#define TM_LE(a,b) (((s32)((a)-(b)))<=0)

#endif/* _TYPE_H */
