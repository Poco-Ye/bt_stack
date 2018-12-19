/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "btstack_run_loop_embedded.c"

/*
 *  btstack_run_loop_embedded.c
 *
 *  For this run loop, we assume that there's no global way to wait for a list
 *  of data sources to get ready. Instead, each data source has to queried
 *  individually. Calling ds->isReady() before calling ds->process() doesn't 
 *  make sense, so we just poll each data source round robin.
 *
 *  To support an idle state, where an MCU could go to sleep, the process function
 *  has to return if it has to called again as soon as possible
 *
 *  After calling process() on every data source and evaluating the pending timers,
 *  the idle hook gets called if no data source did indicate that it needs to be
 *  called right away.
 *
 */


#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_linked_list.h"
#include "hal_tick.h"
#include "hal_cpu.h"

#include "btstack_debug.h"

#include <stddef.h> // NULL

// #define DEBUG_FUNC 

#ifdef HAVE_EMBEDDED_TIME_MS
#include "hal_time_ms.h"
#endif

#if defined(HAVE_EMBEDDED_TICK) && defined(HAVE_EMBEDDED_TIME_MS)
#error "Please specify either HAVE_EMBEDDED_TICK or HAVE_EMBEDDED_TIME_MS"
#endif

#if defined(HAVE_EMBEDDED_TICK) || defined(HAVE_EMBEDDED_TIME_MS)
#define TIMER_SUPPORT
#endif

static const btstack_run_loop_t btstack_run_loop_embedded;

// the run loop
static btstack_linked_list_t data_sources;

#ifdef TIMER_SUPPORT
static btstack_linked_list_t timers;
#endif

#ifdef HAVE_EMBEDDED_TICK
static volatile uint32_t system_ticks;
#endif

static int trigger_event_received = 0;

/**
 * Add data_source to run_loop
 */
static void btstack_run_loop_embedded_add_data_source(btstack_data_source_t *ds){
    btstack_linked_list_add(&data_sources, (btstack_linked_item_t *) ds);
//	log_debug("btstack_run_loop_embedded_add_data_source  called\r\n");
#ifdef DEBUG_FUNC //shics
 func_call_info[17].call_num++;
 func_call_info[17].func_addr =  btstack_run_loop_embedded_add_data_source;
#endif
}

/**
 * Remove data_source from run loop
 */
static int btstack_run_loop_embedded_remove_data_source(btstack_data_source_t *ds){
//log_debug("btstack_run_loop_embedded_remove_data_source  called\r\n");
#ifdef DEBUG_FUNC //shics
 func_call_info[18].call_num++;
 func_call_info[18].func_addr = btstack_run_loop_embedded_remove_data_source;
#endif
    return btstack_linked_list_remove(&data_sources, (btstack_linked_item_t *) ds);
}

// set timer
static void btstack_run_loop_embedded_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
#ifdef DEBUG_FUNC //shics
 func_call_info[19].call_num++;
 func_call_info[19].func_addr =  btstack_run_loop_embedded_set_timer;
#endif
#ifdef HAVE_EMBEDDED_TICK
    uint32_t ticks = btstack_run_loop_embedded_ticks_for_ms(timeout_in_ms);
    if (ticks == 0) ticks++;
    // time until next tick is < hal_tick_get_tick_period_in_ms() and we don't know, so we add one
    ts->timeout = system_ticks + 1 + ticks; 
#endif
#ifdef HAVE_EMBEDDED_TIME_MS
    ts->timeout = hal_time_ms() + timeout_in_ms + 1;
#endif
}

// under the assumption that a tick value is +/- 2^30 away from now, calculate the upper bits of the tick value
static int btstack_run_loop_embedded_reconstruct_higher_bits(uint32_t now, uint32_t ticks){
 #ifdef DEBUG_FUNC //shics
 func_call_info[20].call_num++;
 func_call_info[20].func_addr =  btstack_run_loop_embedded_reconstruct_higher_bits;
#endif
int32_t delta = ticks - now;
    if (delta >= 0){
        if (ticks >= now) {
            return 0;
        } else {
            return 1;
        }
    } else {
        if (ticks < now) {
            return 0;
        } else {
            return -1;
        }
    }
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void btstack_run_loop_embedded_add_timer(btstack_timer_source_t *ts){
 #ifdef DEBUG_FUNC //shics
 func_call_info[21].call_num++;
 func_call_info[21].func_addr =  btstack_run_loop_embedded_add_timer;
#endif
#ifdef TIMER_SUPPORT

#ifdef HAVE_EMBEDDED_TICK
    uint32_t now = system_ticks;
#endif
#ifdef HAVE_EMBEDDED_TIME_MS
    uint32_t now = hal_time_ms();
#endif
//log_debug("btstack_run_loop_embedded_add_timer  called\r\n");
    uint32_t new_low = ts->timeout;
    int     new_high = btstack_run_loop_embedded_reconstruct_higher_bits(now, new_low);

    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &timers; it->next ; it = it->next){
        // don't add timer that's already in there
        if ((btstack_timer_source_t *) it->next == ts){
            log_error( "btstack_run_loop_timer_add error: timer to add already in list!\r\n");
            return;
        }
        uint32_t next_low  = ((btstack_timer_source_t *) it->next)->timeout;
        int      next_high = btstack_run_loop_embedded_reconstruct_higher_bits(now, next_low);
        if (new_high < next_high || ((new_high == next_high) && (new_low < next_low))){
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
#endif
}

/**
 * Remove timer from run loop
 */
static int btstack_run_loop_embedded_remove_timer(btstack_timer_source_t *ts){
 #ifdef DEBUG_FUNC //shics
 func_call_info[22].call_num++;
 func_call_info[22].func_addr =  btstack_run_loop_embedded_remove_timer;
#endif
#ifdef TIMER_SUPPORT
//log_debug("btstack_run_loop_embedded_remove_timer  called\r\n");
    return btstack_linked_list_remove(&timers, (btstack_linked_item_t *) ts);
#else
    return 0;
#endif
}

static void btstack_run_loop_embedded_dump_timer(void){
 #ifdef DEBUG_FUNC //shics
 func_call_info[23].call_num++;
 func_call_info[23].func_addr =  btstack_run_loop_embedded_dump_timer;
#endif
#ifdef TIMER_SUPPORT
#ifdef ENABLE_LOG_INFO 
    btstack_linked_item_t *it;
    int i = 0;
    for (it = (btstack_linked_item_t *) timers; it ; it = it->next){
        btstack_timer_source_t *ts = (btstack_timer_source_t*) it;
        log_debug("timer %u, timeout %u\n", i, (unsigned int) ts->timeout);
    }
#endif
#endif
}

static void btstack_run_loop_embedded_enable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags |= callback_types;
//log_debug(" enable ds->flags:  %02x\r\n", ds->flags);
 #ifdef DEBUG_FUNC //shics
 func_call_info[24].call_num++;
 func_call_info[24].func_addr =  btstack_run_loop_embedded_enable_data_source_callbacks;
#endif
}

static void btstack_run_loop_embedded_disable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags &= ~callback_types;
//log_debug(" disable ds->flags:  %02x\r\n", ds->flags);
 #ifdef DEBUG_FUNC //shics
 func_call_info[25].call_num++;
 func_call_info[25].func_addr =  btstack_run_loop_embedded_disable_data_source_callbacks;
#endif
}

/**
 * Execute run_loop once
 */
//uint32_t hci_transport_h5_send_start;
//uint32_t packet_send_time;
void btstack_run_loop_embedded_execute_once_data(void) {
	btstack_data_source_t* ds;
	// process data sources
	btstack_data_source_t* next;
	
	//hci_transport_h5_send_start = btstack_run_loop_get_time_ms();

	for (ds = (btstack_data_source_t*) data_sources; ds != NULL ; ds = next) {
		next = (btstack_data_source_t*) ds->item.next;  // cache pointer to next data_source to allow data source to remove itself

		if (ds->flags & DATA_SOURCE_CALLBACK_POLL) {
			ds->process(ds, DATA_SOURCE_CALLBACK_POLL);
		}
	}

}

void btstack_run_loop_embedded_execute_once_timer(void) {
#ifdef TIMER_SUPPORT
#ifdef HAVE_EMBEDDED_TICK
		uint32_t now = system_ticks;
#endif
#ifdef HAVE_EMBEDDED_TIME_MS
		uint32_t now = hal_time_ms();
#endif
	
		// process timers
		//btstack_run_loop_embedded_dump_timer();
		while (timers) {
			btstack_timer_source_t* ts = (btstack_timer_source_t*) timers;
			uint32_t timeout_low = ts->timeout;
			int 	 timeout_high = btstack_run_loop_embedded_reconstruct_higher_bits(now, timeout_low);
	
			if (timeout_high > 0 || ((timeout_high == 0) && (timeout_low > now))) {
				break;
			}
	
			btstack_run_loop_embedded_remove_timer(ts);
			ts->process(ts);
		}
	
#endif


}

static uint32_t hci_transport_h5_send_start;
static uint32_t packet_send_time;
extern int hasConnect;

extern int zfNum;
void btstack_run_loop_embedded_execute_once(void) {


	btstack_data_source_t* ds;
	// process data sources
	btstack_data_source_t* next;
	
	//hci_transport_h5_send_start = btstack_run_loop_get_time_ms();

	for (ds = (btstack_data_source_t*) data_sources; ds != NULL ; ds = next) {
		next = (btstack_data_source_t*) ds->item.next;  // cache pointer to next data_source to allow data source to remove itself

		if (ds->flags & DATA_SOURCE_CALLBACK_POLL) {
			ds->process(ds, DATA_SOURCE_CALLBACK_POLL);

		}
	}

#ifdef TIMER_SUPPORT
#ifdef HAVE_EMBEDDED_TICK
	uint32_t now = system_ticks;
#endif
#ifdef HAVE_EMBEDDED_TIME_MS
	uint32_t now = hal_time_ms();
#endif

	// process timers
	//btstack_run_loop_embedded_dump_timer();
	while (timers) {
		btstack_timer_source_t* ts = (btstack_timer_source_t*) timers;
		uint32_t timeout_low = ts->timeout;
		int      timeout_high = btstack_run_loop_embedded_reconstruct_higher_bits(now, timeout_low);

		if (timeout_high > 0 || ((timeout_high == 0) && (timeout_low > now))) {
			break;
		}

		btstack_run_loop_embedded_remove_timer(ts);
		ts->process(ts);
	}

#endif
	// disable IRQs and check if run loop iteration has been requested. if not, go to sleep
	//hal_cpu_disable_irqs();

	//if (trigger_event_received) {
		//trigger_event_received = 0;
		//hal_cpu_enable_irqs();
	//} else {
		//hal_cpu_enable_irqs_and_sleep();
	//}

	//if(hasConnect) {
		//packet_send_time = btstack_run_loop_get_time_ms() - hci_transport_h5_send_start;
		//log_debug("run loop time:%d", packet_send_time);

	//}


}



/* 
void btstack_run_loop_embedded_execute_once(void) {
    btstack_data_source_t *ds;

static int count=0;
static int count_1=0;
extern volatile int count_2;
count_2=1;
extern volatile int download_fw_complete;

if(count++ % 1000 == 0 && download_fw_complete)
{
    //log_debug("****execute_once loop****\r\n");
}
    // process data sources
    btstack_data_source_t *next;
    for (ds = (btstack_data_source_t *) data_sources; ds != NULL ; ds = next){
        next = (btstack_data_source_t *) ds->item.next; // cache pointer to next data_source to allow data source to remove itself
count_2=7;
	if (ds->flags & DATA_SOURCE_CALLBACK_POLL){
            ds->process(ds, DATA_SOURCE_CALLBACK_POLL);//ds==transport,  btstack_uart_embedded_process
            count_2=2;
		if(count % 1000 == 0 || count_1++ <2 )//shics debug
		{
		   // log_debug("*execute_once:: ds:%x, ds->process:%x\r\n",ds, ds->process);
		}
		else if(download_fw_complete > 0)
		{
			//log_debug("execute_once:: ds:%x, ds->process:%x\r\n",ds, ds->process);
		}
        }
	else{
		count_2=8;
		//log_debug("execute_once:: ds:%x, ds->process:%x, ds->flags:%x\r\n",ds, ds->process, ds->flags);
		}
    }
    count_2=3;
#ifdef TIMER_SUPPORT

#ifdef HAVE_EMBEDDED_TICK
    uint32_t now = system_ticks;
#endif
#ifdef HAVE_EMBEDDED_TIME_MS
    uint32_t now = hal_time_ms();
#endif
count_2=4;
    // process timers
    while (timers) {
		count_2=9;
        btstack_timer_source_t *ts = (btstack_timer_source_t *) timers;
        uint32_t timeout_low = ts->timeout;
        int      timeout_high = btstack_run_loop_embedded_reconstruct_higher_bits(now, timeout_low);
        if (timeout_high > 0 || ((timeout_high == 0) && (timeout_low > now)))
	{
			count_2=11;
			break;
	}
        btstack_run_loop_embedded_remove_timer(ts);
        ts->process(ts);
		count_2=10;
    }
#endif
    count_2=5;
    // disable IRQs and check if run loop iteration has been requested. if not, go to sleep
    hal_cpu_disable_irqs();
    if (trigger_event_received){
        trigger_event_received = 0;
        hal_cpu_enable_irqs();
    } else {
        hal_cpu_enable_irqs_and_sleep();
    }
    count_2=6;
}*/

/**
 * Execute run_loop
 */
 #if 1 //shics
 extern void DelayUs(uint us);
  extern void OsSleep(uint	SleepTime);
static void btstack_run_loop_embedded_execute(void) {
#ifdef DEBUG_FUNC //shics
 func_call_info[33].func_addr =  btstack_run_loop_embedded_execute;
#endif

    while (1) 
    {	
#ifdef DEBUG_FUNC //shics
 func_call_info[33].call_num++;
#endif
        btstack_run_loop_embedded_execute_once();
       //log_debug("btstack_run_loop_embedded_execute_once();\r\n");
	OsSleep(1); // 
#ifdef DEBUG_FUNC //shics
 func_call_info[33].call_num++;
#endif
    }
}
 #else
static void btstack_run_loop_embedded_execute(void) {
    while (1) {	
        btstack_run_loop_embedded_execute_once();
    }
}
#endif

#ifdef HAVE_EMBEDDED_TICK
static void btstack_run_loop_embedded_tick_handler(void){
    system_ticks++;
    trigger_event_received = 1;
 #ifdef DEBUG_FUNC //shics
 func_call_info[26].call_num++;
 func_call_info[26].func_addr =  btstack_run_loop_embedded_tick_handler;
#endif
}

uint32_t btstack_run_loop_embedded_get_ticks(void){
 #ifdef DEBUG_FUNC //shics
 func_call_info[27].call_num++;
 func_call_info[27].func_addr =  btstack_run_loop_embedded_get_ticks;
#endif
    return system_ticks;
}

uint32_t btstack_run_loop_embedded_ticks_for_ms(uint32_t time_in_ms){
 #ifdef DEBUG_FUNC //shics
 func_call_info[28].call_num++;
 func_call_info[28].func_addr =  btstack_run_loop_embedded_ticks_for_ms;
#endif
    return time_in_ms / hal_tick_get_tick_period_in_ms();
}
#endif

static uint32_t btstack_run_loop_embedded_get_time_ms(void){
 #ifdef DEBUG_FUNC //shics
 func_call_info[29].call_num++;
 func_call_info[29].func_addr =  btstack_run_loop_embedded_get_time_ms;
#endif
#if   defined(HAVE_EMBEDDED_TIME_MS)
    return hal_time_ms();
#elif defined(HAVE_EMBEDDED_TICK)
    return system_ticks * hal_tick_get_tick_period_in_ms();
#else
    return 0;
#endif
}


/**
 * trigger run loop iteration
 */
void btstack_run_loop_embedded_trigger(void){
 #ifdef DEBUG_FUNC //shics
 func_call_info[30].call_num++;
 func_call_info[30].func_addr =  btstack_run_loop_embedded_trigger;
#endif
    trigger_event_received = 1;
}

static void btstack_run_loop_embedded_init(void){
    data_sources = NULL;
 #ifdef DEBUG_FUNC //shics
 func_call_info[31].call_num++;
 func_call_info[31].func_addr =  btstack_run_loop_embedded_init;
#endif

#ifdef TIMER_SUPPORT
    timers = NULL;
#endif

#ifdef HAVE_EMBEDDED_TICK
    system_ticks = 0;
    hal_tick_init();//timer init
    hal_tick_set_handler(&btstack_run_loop_embedded_tick_handler); // set timer handler
#endif
}

/**
 * Provide btstack_run_loop_embedded instance 
 */
const btstack_run_loop_t * btstack_run_loop_embedded_get_instance(void){
    return &btstack_run_loop_embedded;
}

static const btstack_run_loop_t btstack_run_loop_embedded = {
    &btstack_run_loop_embedded_init,
    &btstack_run_loop_embedded_add_data_source,
    &btstack_run_loop_embedded_remove_data_source,
    &btstack_run_loop_embedded_enable_data_source_callbacks,
    &btstack_run_loop_embedded_disable_data_source_callbacks,
    &btstack_run_loop_embedded_set_timer,
    &btstack_run_loop_embedded_add_timer,
    &btstack_run_loop_embedded_remove_timer,
    &btstack_run_loop_embedded_execute,
    &btstack_run_loop_embedded_dump_timer,
    &btstack_run_loop_embedded_get_time_ms,
};

