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

#define __BTSTACK_FILE__ "btstack_link_key_db_memory.c"

#include <string.h>
#include <stdlib.h>

#include "classic/btstack_link_key_db_memory.h"

#include "btstack_debug.h"
#include "btstack_linked_list.h"
#include "btstack_memory.h"
#include "btstack_util.h"
#include "classic/core.h"
#include "bt_init.h"

// This list should be directly accessed only by tests
btstack_linked_list_t db_mem_link_keys = NULL;

// Device info
static void db_open(void) {
}

static void db_set_local_bd_addr(bd_addr_t bd_addr) {
	(void)bd_addr;
}

static void db_close(void) {
}

static btstack_link_key_db_memory_entry_t* get_item(btstack_linked_list_t list, bd_addr_t bd_addr) {
	btstack_linked_item_t* it;

	for (it = (btstack_linked_item_t*) list; it ; it = it->next) {
		btstack_link_key_db_memory_entry_t* item = (btstack_link_key_db_memory_entry_t*) it;

		if (bd_addr_cmp(item->bd_addr, bd_addr) == 0) {
			return item;
		}
	}

	return NULL;
}
#if 1
static int get_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t* link_key_type) {
	btstack_link_key_db_memory_entry_t* item = get_item(db_mem_link_keys, bd_addr);

	if (!item) {
		return 0;
	}

	memcpy(link_key, item->link_key, LINK_KEY_LEN);

	if (link_key_type) {
		*link_key_type = item->link_key_type;
	}

	btstack_linked_list_remove(&db_mem_link_keys, (btstack_linked_item_t*) item);
	btstack_linked_list_add(&db_mem_link_keys, (btstack_linked_item_t*) item);
	return 1;
}
#endif


static void delete_link_key(bd_addr_t bd_addr) {
	btstack_link_key_db_memory_entry_t* item = get_item(db_mem_link_keys, bd_addr);

	if (!item) {
		return;
	}

	btstack_linked_list_remove(&db_mem_link_keys, (btstack_linked_item_t*) item);
	btstack_memory_btstack_link_key_db_memory_entry_free((btstack_link_key_db_memory_entry_t*)item);
}

#if 1
static char link_key_to_str_buffer[LINK_KEY_STR_LEN+1];  // 11223344556677889900112233445566\0
static char *link_key_to_str(link_key_t link_key){
    char * p = link_key_to_str_buffer;
    int i;
    for (i = 0; i < LINK_KEY_LEN ; i++) {
        *p++ = char_for_nibble((link_key[i] >> 4) & 0x0F);
        *p++ = char_for_nibble((link_key[i] >> 0) & 0x0F);
    }
    *p = 0;
    return (char *) link_key_to_str_buffer;
}

static char link_key_type_to_str_buffer[2];
static char *link_key_type_to_str(link_key_type_t link_key){
    snprintf(link_key_type_to_str_buffer, sizeof(link_key_type_to_str_buffer), "%d", link_key);
    return (char *) link_key_type_to_str_buffer;
}

typedef int (*pairData_callback_tt)(int size, void* data);

static void put_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t link_key_type) {
	int LengthKey, LengthType;
	unsigned char *Buffer;
	
	// check for existing record and remove if found
	btstack_link_key_db_memory_entry_t* record = get_item(db_mem_link_keys, bd_addr);

	if (record) {
		btstack_linked_list_remove(&db_mem_link_keys, (btstack_linked_item_t*) record);
	}

	// record not found, get new one from memory pool
	if (!record) {
		record = btstack_memory_btstack_link_key_db_memory_entry_get();
	}

	// if none left, re-use last item and remove from list
	if (!record) {
		record = (btstack_link_key_db_memory_entry_t*) btstack_linked_list_get_last_item(&db_mem_link_keys);

		if (record) {
			btstack_linked_list_remove(&db_mem_link_keys, (btstack_linked_item_t*) record);
		}
	}

	if (!record) {
		return;
	}

	memcpy(record->bd_addr, bd_addr, sizeof(bd_addr_t));
	memcpy(record->link_key, link_key, LINK_KEY_LEN);
	record->link_key_type = link_key_type;
	btstack_linked_list_add(&db_mem_link_keys, (btstack_linked_item_t*) record);
	
	#if 0
	char *link_key_str = link_key_to_str(link_key);
	LengthKey = strlen(link_key_str);
	char *link_key_type_str = link_key_type_to_str(link_key_type); 
	LengthType = strlen(link_key_type_str);
	log_debug("put_link_key LengthKey:%d LengthType:%d", LengthKey, LengthType);
	log_debug("put_link_key link_key_str:%s link_key_type_str:%s", link_key_str, link_key_type_str);
	
	pairDataWrite_callback(LengthKey, link_key_str);
	#endif
}
#endif

#if 0
static char link_key_to_str_buffer[LINK_KEY_STR_LEN+1];  // 11223344556677889900112233445566\0
static char *link_key_to_str(link_key_t link_key){
    char * p = link_key_to_str_buffer;
    int i;
    for (i = 0; i < LINK_KEY_LEN ; i++) {
        *p++ = char_for_nibble((link_key[i] >> 4) & 0x0F);
        *p++ = char_for_nibble((link_key[i] >> 0) & 0x0F);
    }
    *p = 0;
    return (char *) link_key_to_str_buffer;
}

static char link_key_type_to_str_buffer[2];
static char *link_key_type_to_str(link_key_type_t link_key){
    snprintf(link_key_type_to_str_buffer, sizeof(link_key_type_to_str_buffer), "%d", link_key);
    return (char *) link_key_type_to_str_buffer;
}

typedef int (*pairData_callback_tt)(int size, void* data);

extern pairData_callback_tt pairDataWrite_callback;
extern pairData_callback_tt pairDataRead_callback;

static void put_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t link_key_type) {
	int Length, LengthType;
	unsigned char *Buffer;
	
	char *link_key_str = link_key_to_str(link_key);
	Length = strlen(link_key_str);
	char *link_key_type_str = link_key_type_to_str(link_key_type); 
	LengthType = strlen(link_key_type_str);
	log_debug("put_link_key Length:%d LengthType:%d", Length, LengthType);
	log_debug("put_link_key link_key_str:%s link_key_type_str:%s", link_key_str, link_key_type_str);
	
	pairDataWrite_callback(Length, link_key_str);
}
#endif


static int iterator_init(btstack_link_key_iterator_t* it) {
	it->context = (void*) db_mem_link_keys;
	return 1;
}

static int  iterator_get_next(btstack_link_key_iterator_t* it, bd_addr_t bd_addr, link_key_t link_key, link_key_type_t* link_key_type) {
	btstack_link_key_db_memory_entry_t* item = (btstack_link_key_db_memory_entry_t*) it->context;

	if (item == NULL) {
		return 0;
	}

	// fetch values
	memcpy(bd_addr,  item->bd_addr, 6);
	memcpy(link_key, item->link_key, 16);
	*link_key_type = item->link_key_type;
	// next
	it->context = (void*) item->item.next;
	return 1;
}

static void iterator_done(btstack_link_key_iterator_t* it) {
	UNUSED(it);
}

const btstack_link_key_db_t btstack_link_key_db_memory = {
	db_open,
	db_set_local_bd_addr,
	db_close,
	get_link_key,
	put_link_key,
	delete_link_key,
	iterator_init,
	iterator_get_next,
	iterator_done,
};

const btstack_link_key_db_t* btstack_link_key_db_memory_instance(void) {
	return &btstack_link_key_db_memory;
}


