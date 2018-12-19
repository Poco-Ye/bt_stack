#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack_debug.h>
#include "bt_malloc.h"


#define MEMORY_TABLE_MAX_SIZE 10

typedef struct {
	size_t size;
	void* address;
} memory_detect_t;


static memory_detect_t memory_table[MEMORY_TABLE_MAX_SIZE];
static int memory_table_num = 0;
static int total_memory_size = 0;
static int peak_memory_size = 0;

void sort_memory_table() {
	int i, j;

	for (i = 0; i < memory_table_num - 1; i++) {
		for (j = 0; j < memory_table_num - 1; j++) {
			if (memory_table[j].size < memory_table[j+1].size) {
				memory_detect_t temp;
				memcpy(&temp, &memory_table[j], sizeof(memory_detect_t));
				memcpy(&memory_table[j], &memory_table[j+1], sizeof(memory_detect_t));
				memcpy(&memory_table[j+1], &temp, sizeof(memory_detect_t));
			}
		}
	}
}

void dump_memory_table() {
	int i = 0;

	//////log_print("===== MEMORY DUMP =====\n");
	for (i = 0; i < memory_table_num; i++) {
		////log_print("%d: address: %p, size: %ld\n", i+1, memory_table[i].address, memory_table[i].size);
	}

	////log_print("=======================\n");
}

void* bt_malloc(unsigned int size) {
	void* ret = NULL;
	memory_detect_t* p_malloc_address;
	ret = malloc(size);
	////log_print("[%s] address: %p, size: %d\n", __func__, ret, size);
	p_malloc_address = &memory_table[memory_table_num];
	p_malloc_address->size = size;
	p_malloc_address->address = ret;

	if (memory_table_num < MEMORY_TABLE_MAX_SIZE) {
		memory_table_num++;
	} else {
		////log_print("out of memory table size\n");
		for(;;);
	}

	total_memory_size += size;

	if (total_memory_size > peak_memory_size) {
		peak_memory_size = total_memory_size;
	}

	// dump_memory_table();
	return ret;
}

void bt_free(void* address) {
	char detect_flag = 0;
	memory_detect_t* p_free_address = NULL;

	if (address == NULL) {
		////log_print("free address is NULL\n");
		return;
	}

	int i = 0;

	for (i = 0; i < 10; i++) {
		if (memory_table[i].address == address) {
			detect_flag = 1;
			p_free_address = &memory_table[i];
			break;
		}
	}

	if (detect_flag == 0) {
		////log_print("address is not found\n");
		return;
	} else {
		////log_print("[%s] free address: %p, size: %ld\n", __func__, p_free_address->address, p_free_address->size);
		total_memory_size -= p_free_address->size;
		p_free_address->address = NULL;
		p_free_address->size = 0;
		// sort table
		//dump_memory_table();
		sort_memory_table();
		// dump_memory_table();
		memory_table_num--;
	}

	free(address);
}

int bt_get_peak_memory(void) {
	return peak_memory_size;
}

void bt_print_memory_leak(void) {
	int i = 0;

	////log_print("====== MEMORY LEAK =====\n");
	for (i = 0; i < memory_table_num; i++) {
		if (memory_table[i].size > 0) {
			////log_print("address: %p, size: %ld\n",  memory_table[i].address, memory_table[i].size);
		}
	}

	////log_print("=========================\n");
}
