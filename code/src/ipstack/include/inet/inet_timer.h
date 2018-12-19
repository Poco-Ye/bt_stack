#ifndef _INET_TIMER_H_
#define _INET_TIMER_H_
#include <inet/inet_list.h>

typedef struct inet_timer_s
{
	struct list_head  list;
	int               count;/* time = count*TIMER_VAL */
	void              *data;
	void              (*fun)(void *);
	int               expired;/* used by inet_timer.c */
	int               flag;/* 0x00: one slot, 0x01: continue */
}inet_timer;

void inet_timer_init(void);
void inet_timer_add(struct inet_timer_s  *timer);
void inet_timer_remove(struct inet_timer_s *timer);


/* NOTE: you can not call this in softirq or irq */

#endif/* _INET_TIMER_H_ */
