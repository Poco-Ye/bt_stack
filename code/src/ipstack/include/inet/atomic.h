#ifndef _ATOMIC_H_
#define _ATOMIC_H_
#include <inet/inet_irq.h>

typedef struct { volatile int counter; } atomic_t;
#define atomic_read(v)	((v)->counter)
#define atomic_set(v, i) ((v)->counter)=i

/* return value:
 * 1: you catch the sem v
 * 0: you can not catch the sem v
 * Used by IRQ
 */
static inline int atomic_test_and_inc(atomic_t *v)
{
	int err = 0;
	if(v->counter==0)
	{
		v->counter++;
		err = 1;
	}
	return err;
}

#endif/* _ATOMIC_H_ */
