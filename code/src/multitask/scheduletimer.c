#include "base.h"
static struct platform_timer *tasktimer;
static struct platform_timer_param  taskparam;

int BSP_TMR_Acquire(uint TimerNum, void* pIsr)
{
	taskparam.handler= pIsr;
	taskparam.id =TimerNum;
	taskparam.timeout=2000;
	taskparam.mode =TIMER_PERIODIC;
	taskparam.interrupt_prior =INTC_PRIO_15;
	tasktimer = platform_get_timer(&taskparam);

    return TimerNum;
}


int BSP_TMR_Start(uint TimerNum )
{
    platform_start_timer(tasktimer);
	return 0;
}
int  BSP_TMR_Stop( uint TimerNum )
{
    platform_stop_timer(tasktimer);
	return 0;
}

int BSP_TMR_Init(void)
{
    return 0;
}

void BspTimerClear()
{
    platform_timer_IntClear(tasktimer);
}
