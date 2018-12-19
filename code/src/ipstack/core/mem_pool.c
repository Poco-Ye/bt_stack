#include <inet/inet.h>
#include <inet/inet_type.h>
#include <inet/mem_pool.h>

#define MEM_POOL_MAX_COUNT 60
static MEM_POOL *arr[MEM_POOL_MAX_COUNT];
static u32 magic_id[2]={0x12345678,0x87654321};
int arr_index = 0;
int mem_assert = /* fail to assert(0)?*/
#ifdef NET_DEBUG
1
#else
0
#endif
;

static void* my_malloc(int size)
{
	return NULL;
}
static void my_free(void *p)
{	
}


/*
 * List Operation
 */
static void list_init(LIST_NODE *node)
{
	node->prev = node->next = node;
}

static void list_head_init(LIST_HEAD *head)
{
	list_init(&head->node);
	head->count = 0;
}

static void list_insert(LIST_NODE *prev, LIST_NODE *node)
{
	node->next = prev->next;
	node->prev = prev;
	prev->next->prev = node;
	prev->next = node;
}
static void list_remove(LIST_HEAD *head, LIST_NODE *node)
{
	LIST_NODE *prev = node->prev, *next = node->next;
	prev->next = next;
	next->prev = prev;
	list_init(node);
	head->count--;
}
static void list_insertTail(LIST_HEAD *head, LIST_NODE *node)
{
	list_insert(head->node.prev, node);
	head->count++;
}
static void list_insertHead(LIST_HEAD *head, LIST_NODE *node)
{
	list_insert(&head->node, node);
	head->count++;
}

static int list_free_count(LIST_HEAD *head)
{
	return head->count;
}
static LIST_NODE *list_get(LIST_HEAD *head)
{
	LIST_NODE *node = head->node.next;
	if(head->count == 0)
		return NULL;
	list_remove(head, node);
	return node;
}
/*
 * 从heap分配空间
 */
static void *dyn_malloc(MEM_POOL *ppool)
{
	return my_malloc(ppool->size);
}

static void dyn_free(MEM_POOL *ppool, void *p)
{
	my_free(p);
}

/*
 * 从static buffer 分配空间
 */
static void *static_malloc(MEM_POOL *ppool)
{
	LIST_NODE *node;
	node = list_get(&ppool->list_head);
	if(!node)
	{
		return NULL;
	}
	return node+1;
}
static void static_free(MEM_POOL *ppool, void *p)
{
	LIST_NODE *node = (LIST_NODE*)p;
	node--;
	if(node->next != node &&
		node->prev != node)
	{
		s80_printf("%08x,%08x,%08x\n",
			node, node->next, node->prev);
		s80_printf("%s static free assert fail\n",ppool->name);
		assert(0);
		return;
	}
	assert(node->next==node&&node->prev==node);
	list_insertTail(&ppool->list_head, node);
}
int mem_pool_init(MEM_POOL *ppool, int size, int count, 
				char *p_static_buf, int static_buf_size, 
				const char*name)
{
	int i;
	LIST_NODE *node;
	assert(arr_index < MEM_POOL_MAX_COUNT);
	memset(ppool, 0, sizeof(MEM_POOL));
	ppool->name = name;
	ppool->size = size;	
	arr[arr_index++] = ppool;
	list_head_init(&ppool->list_head);
	if(count <= 0)
	{
		ppool->get = dyn_malloc;
		ppool->put = dyn_free;
		return 0;
	}else
	{
		ppool->get = static_malloc;
		ppool->put = static_free;
	}
	assert(p_static_buf != 0);
	ppool->p_static_buf = p_static_buf;
	p_static_buf = (char*)MEM_ALIGN(p_static_buf);
	for(i=0; i<count; i++)
	{
		node = (LIST_NODE*)p_static_buf;
		list_insertTail(&ppool->list_head, node);
		p_static_buf += MEM_NODE_SIZE(size);
		if(p_static_buf > ppool->p_static_buf+static_buf_size)
		{
			s80_printf("%s mem pool is bad, please fix it!\n", name);
			assert(0);
		}
	}
	return 0;
}

/*
** 不要用err_printf,该函数在s80下有问题
**/
void mem_debug_show(void)
{
	int i, total;
	MEM_POOL *ppool;
	s80_printf("\n-------------------------------\n");
	total = 0;
	for(i=0; i<arr_index; i++)
	{
		ppool = arr[i];
		s80_printf("%s : size %d; count %d; malloc %d; free %d; max %d\n",
					ppool->name,
					ppool->size,
					ppool->list_head.count+ppool->used,
					ppool->malloc_count,
					ppool->free_count,
					ppool->max_used
					);
		total += ppool->size*(ppool->list_head.count+ppool->used);
		if(ppool->malloc_count != ppool->free_count)
		{
			s80_printf("\tWARNING: MEM Leak!\n");
		}
	}
	s80_printf("Total Mem Size: %d\n", total);
}

void *mem_malloc(MEM_POOL *ppool)
{
	u32 *mg;
	void *p = ppool->get(ppool);
	if(!p)
	{
		if(!mem_assert)
			return NULL;
		s80_printf("Fail to malloc mem for %s\n", ppool->name);
		mem_debug_show();
		assert(0);
		return	NULL;
	}
	ppool->malloc_count++;
	ppool->used++;
	if(ppool->used > ppool->max_used)
		ppool->max_used = ppool->used;
	memset(p, 0, ppool->size);
	mg = (u32*)(((u8*)p)+ppool->size);
	INET_MEMCPY(mg, &magic_id, sizeof(magic_id));
	return p;
}

void mem_free(MEM_POOL *ppool, void *p)
{
	u32 *mg;
	ppool->put(ppool, p);
	ppool->free_count++;
	ppool->used--;
	mg = (u32*)(((u8*)p)+ppool->size);
	if(INET_MEMCMP(mg, &magic_id, sizeof(magic_id))!=0)
	{
		s80_printf("Mem op err in %s\n", ppool->name);
		assert(0);
	}
	return;
}

