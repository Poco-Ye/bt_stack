#ifndef __TS_API_H__
#define __TS_API_H__

#include "base.h"

/* ts owner */
#define TS_OWNER_SYS	0xff
#define TS_OWNER_SHARE	0x7f

enum SAMPLERATE_MODEL
{
	BTN_SP_SPEED,
	SIGN_SP_SPEED,
	SPEED_LIMIT,
};

typedef struct 
{
	unsigned char sampleRateModel;
	unsigned char reserved[32];
}TS_ATTR_T;

typedef struct 
{
	int lastOwner;
	int currOwner;
	int lock;
	int isOwnerSwitch;
	int isSKeySupported;
	TS_ATTR_T tAttr;
}TS_DEV_T;

typedef struct 
{
	int x;
	int y;
	int pressure;
	int reserved[2];
}TS_POINT_T;

/* return code */
#define TS_RET_SUCCESS				0x00 /* excute successfully */
#define TS_RET_NO_MODULE			-1
#define TS_RET_SYS_ERR				-1
#define TS_RET_DEV_NOT_OPENED		-2
#define TS_RET_DEV_BUSY 			-3
#define TS_RET_INVALID_PARAM 		-4

#endif /* __TS_API_H__ */

