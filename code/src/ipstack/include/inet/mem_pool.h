#ifndef _MEM_POOL_H_
#define _MEM_POOL_H_

//#include <mem_conf.h>

#define MEM_ALIGN(m) ((((unsigned long)(m))+0x3)&~0x3)

typedef struct list_node_s
{
	struct list_node_s *prev, *next;
}LIST_NODE;

typedef struct list_head_s
{
	LIST_NODE node;
	int       count;
}LIST_HEAD;

#define MEM_NODE struct list_node_s



typedef struct mem_pool_s
{
	LIST_HEAD   list_head;
	const char* name;
	int        malloc_count;
	int        free_count;
	int        used;
	int        max_used;
	int        size, max_size, min_size;
	char*      p_static_buf;
	void*      (*get)(struct mem_pool_s *);
	void       (*put)(struct mem_pool_s*, void*);
}MEM_POOL;

#define MEM_NODE_SIZE(item_size) (MEM_ALIGN(item_size)+sizeof(MEM_NODE)+8)

int mem_pool_init(MEM_POOL *ppool, int size, int count, 
				char *p_static_buf, int static_buf_size, 
				const char*name);
				
void *mem_malloc(MEM_POOL *ppool);

void mem_free(MEM_POOL *ppool, void *p);

void mem_debug_show(void);


#endif/* _MEM_POOL_H_ */
