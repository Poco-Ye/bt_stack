#ifndef __TS_SAMPLE_H__
#define __TS_SAMPLE_H__

#include "base.h"

/* Buffer: A circular queue */
#define QUE_WRAP             0x7ff
#define QUE_SIZE             2048  // Note: 必须是2的冥，方便wrap

typedef struct
{
	ushort x;
	ushort y;
	uint pressure;
}TS_SAMPLE_T;

enum TS_RECV_STATUS
{
	TS_RELEASE=0,
	TS_PRESSING,
	TS_TRIGGER,
};

typedef struct
{
	TS_SAMPLE_T s[QUE_SIZE];
	int head;
	int nbr;
}QUEUE_SAMPLE_T;

struct tslib_linear 
{
	/* Linear scaling and offset parameters for pressure */
	int	p_offset;
	int	p_mult;
	int	p_div;
	/* Linear scaling and offset parameters for x,y (can include rotation) */
	int	a[7];
} __attribute__ ((packed));

#endif /* end of __TS_SAMPLE_H__ */
