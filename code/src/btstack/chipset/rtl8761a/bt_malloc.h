#ifndef __BT_MALLOC_H__
#define __BT_MALLOC_H__
void* bt_malloc(unsigned int size);
void bt_free(void* address);
int bt_get_peak_memory(void);
void bt_print_memory_leak(void);
#endif

