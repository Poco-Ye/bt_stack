#include "hal_tick.h"
#include "btstack_config.h"
static void dummy_handler(void)
{

	static int i=0;
	if(++i >100000)
	{
		i=0;
		//log_debug("tick_handler::dummy_handler\r\n");
	}
}

static void (*tick_handler)(void) = &dummy_handler;

void hal_tick_init(void){
//SHICS TODO::
}
#ifndef NULL
#define NULL ((void*)0) 
#endif
void hal_tick_set_handler(void (*handler)(void)){
	//SHICS TODO::

	if (handler == NULL){
		tick_handler = &dummy_handler;
		return;
	}
	tick_handler = handler;

}

int  hal_tick_get_tick_period_in_ms(void){
    return 1;//SHICS TODO::
}

#if 0 //shics
// Timer A1 interrupt service routine
#ifdef __GNUC__
__attribute__((interrupt(TIMER1_A0_VECTOR)))
#endif
#ifdef __IAR_SYSTEMS_ICC__
#pragma vector=TIMER1_A0_VECTOR
__interrupt
#endif
#endif

// (*tick_handler)(); should handle in timer

void bt_timer_handler(void){
    //TA1CCR0 += TIMER_COUNTDOWN;
    (*tick_handler)();
    //SHICS TODO::
    
    // force exit low power mode
    //__bic_SR_register_on_exit(LPM0_bits);   // Exit active CPU
}

