#ifndef _INET_SOFTIRQ_H_
#define _INET_SOFTIRQ_H_
#include <inet/inet_list.h>
#include <inet/atomic.h>

struct inet_softirq_s
{
	struct list_head list;
	unsigned long mask;
	void (*h)(unsigned long softirq);
};

extern atomic_t inet_softirq_count;
#define INET_SOFTIRQ_TX 0x1
#define INET_SOFTIRQ_RX 0x2
#define INET_SOFTIRQ_TIMER 0x4

#define inet_softirq_disable()  { \
                                    int flag; \
                                    if(RouteGetDefault_std() == 12) { \
                                        irq_save_asm(&flag); \
                                        if(get_cur_task() != 10) { \
                                            while(!atomic_test_and_inc(&inet_softirq_count)) { \
                                                if(get_cur_task() != 30) { \
                                                    irq_restore_asm(&flag); \
                                                    OsSleep(2); \
                                                } else {\
                                                    irq_restore_asm(&flag); \
                                                    DelayUs(500); \
                                                } \
                                                irq_save_asm(&flag); \
                                            }; \
                                        } \
                                        irq_restore_asm(&flag); \
                                    } else { \
                                        atomic_set(&inet_softirq_count, 1) ; \
                                    } \
                                }

#define inet_softirq_enable() do {  \
                                    int flag; \
                                    if(RouteGetDefault_std() == 12) { \
                                        irq_save_asm(&flag); \
                                        if(get_cur_task() != 10) \
                                            atomic_set(&inet_softirq_count, 0); \
                                        irq_restore_asm(&flag); \
                                    } else { \
                                        atomic_set(&inet_softirq_count, 0); \
                                    } \
                                } while(0)

extern volatile unsigned long inet_softirq_flag;
#define INET_SOFTIRQ(softirq) inet_softirq_flag |= softirq
extern void inet_softirq(unsigned long softirq);
extern void inet_softirq_register(struct inet_softirq_s *sftirq);
extern void inet_softirq_unregister(struct inet_softirq_s *sftirq);
extern void _do_softirq(void);
#endif/* _INET_SOFTIRQ_H_ */
