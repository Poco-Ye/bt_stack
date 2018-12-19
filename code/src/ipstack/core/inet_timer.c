/*
 * TCP/IP stack Timer
 * This is a continue Timer for packet sending , connection TIMEOUT and other.
 * The timer interval is 10ms
 * Author:jhsun 2007-07-11
090723 sunJH
增加了eth_irq_handler用于以太网定时处理中断,
sp30直接在中断里面处理有问题;
该方法会降低以太网性能，但是应该可以接受！
 */

#include <inet/inet.h>
#include <inet/inet_irq.h>
#include <inet/inet_softirq.h>
#include <inet/inet_type.h>
#include <inet/inet_timer.h>

static u16 timer_count = 0;
static u8  timer_prescale = 0;
static struct list_head timer_head;
volatile u32  inet_jiffier=0;

#define TIMER_DEBUG //s80_printf
#define TIMER_VAL 1/* ms , 定时器间隔*/

int   timer_irq_enable = 0;

void disable_softirq_timer(void)
{
	inet_softirq_disable();
}

void enable_softirq_timer(void)
{
	inet_softirq_enable();
}

void timer_irq_Handler(void)
{
	static u8 counter = 0;
	inet_timer *ptimer;
	struct list_head *p;
	inet_jiffier=s_Get10MsTimerCount()*INET_TIMER_SCALE;
	if(!timer_irq_enable) return;
	
	inet_softirq(INET_SOFTIRQ_TIMER);
}


void inet_timer_init(void)
{
	timer_irq_enable = 1;
}

void inet_delay(u32 ms)
{
	u32 t = inet_jiffier+ms;
	while(inet_jiffier < t)
	{
	}
}

atomic_t  inet_softirq_count={0};

volatile unsigned long inet_softirq_flag = 0;
static struct list_head sft_irq_head={&sft_irq_head, &sft_irq_head};

u32  softirq_counter[3] = {0,0,0};

void s_InetSoftirqInit(void)
{
	inet_softirq_flag = 0;
	INIT_LIST_HEAD(&sft_irq_head);
}


struct inet_softhirq_h
{
	unsigned long mask;
	void (*h)(unsigned long softirq);
};

void _do_softirq(void)
{
	unsigned long flag;
	struct list_head *p, *next;
	struct inet_softirq_s  *sft;
	int count = 0;

	do
	{
		flag = inet_softirq_flag;		
		inet_softirq_flag = 0;
		if(flag == 0)	break;
		if(flag&INET_SOFTIRQ_TX) softirq_counter[0]++;
		if(flag&INET_SOFTIRQ_RX) softirq_counter[1]++;
		if(flag&INET_SOFTIRQ_TIMER)	softirq_counter[2]++;
		for(p=sft_irq_head.next; p!=&sft_irq_head;)
		{
			sft = list_entry(p, struct inet_softirq_s, list);
			next = p->next;
			if(flag&sft->mask)	sft->h(flag&sft->mask);
			p = next;
		}
		count++;
		if(count > 1000) break;
	}while(1);
}

#include "inet\dev.h"
void inet_softirq(unsigned long softirq)
{	
	inet_softirq_flag |= softirq;
	if(atomic_test_and_inc(&inet_softirq_count)==0)
	{
		return;
	}

    if(RouteGetDefault_std() == WIFI_IFINDEX_BASE)
    {
        if(WifiIsBusy())
        {
            atomic_set(&inet_softirq_count, 0);
            return ;
        }
    }

	_do_softirq();
	atomic_set(&inet_softirq_count, 0);
}

int inet_usable(void)
{
    return atomic_test_and_inc(&inet_softirq_count);
}

int inet_release(void)
{
	return atomic_set(&inet_softirq_count, 0);
}

void inet_softirq_register(struct inet_softirq_s *sftirq)
{
	inet_softirq_disable();
	list_add_tail(&sftirq->list, &sft_irq_head);
	inet_softirq_enable();
}

void inet_softirq_unregister(struct inet_softirq_s *sftirq)
{
	inet_softirq_disable();
	list_del(&sftirq->list);
	inet_softirq_enable();
}

