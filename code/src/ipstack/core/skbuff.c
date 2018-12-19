/*
	协议栈报文所需的内存管理
修改历史:
080623 sunJH
取消1024块内存,全部集中到1518中
512块主要用于ack等小报文;
081027 sunJH
Fix the bug: skb_new和skb_free对skb_mem_pools数组操作有问题
**/
#include <inet/inet.h>
#include <inet/dev.h>
#include <inet/skbuff.h>
#include <inet/mem_pool.h>

#define SKB_BUF_SIZE(size, count) \
	MEM_NODE_SIZE(sizeof(struct sk_buff)+(size))*count

static MEM_POOL skb_512;
static MEM_POOL skb_1518;
static char skb_512_static_buf[SKB_BUF_SIZE(512, MAX_512_COUNT)+4];
static char skb_1518_static_buf[SKB_BUF_SIZE(1518, MAX_1518_COUNT)+4];

int skb_check(struct sk_buff* skb)
{
	u32 p;
	u32 start, end;

	if(skb == NULL)
		return NET_OK;
	p = (u32)(skb);
	start = (u32)skb_512_static_buf;
	end = (u32)(skb_512_static_buf+sizeof(skb_512_static_buf));
	if(p>=start&&p<end)
		goto check_data;
	start = (u32)skb_1518_static_buf;
	end = (u32)(skb_1518_static_buf+sizeof(skb_1518_static_buf));
	if(p>=start&&p<end)
		goto check_data;
	return -1;

check_data:
	p = (u32)(skb->data);
	start = (u32)(skb+1);
	end = (u32)(skb+1)+skb->truesize;
	if(p>=start&&p<end)
		return NET_OK;
	return -1;
	
}

static MEM_POOL *skb_mem_pools[]=
{
	&skb_512,
	&skb_1518,
	NULL,
};

#define SKB_DATA_SPACE(pPool) (pPool)->size-sizeof(struct sk_buff)

struct sk_buff* skb_new(int size)
{
	struct sk_buff *skb;
	MEM_POOL *pPool;
	int data_space, i;
	skb = NULL;
	for(i=0; skb_mem_pools[i]!=NULL; i++)
	{
		pPool = skb_mem_pools[i];
		data_space = SKB_DATA_SPACE(pPool);
		if(data_space<size)
			continue;
		skb = mem_malloc(pPool);
		if(skb!=NULL)
			break;		
	}
	if(skb==NULL)
		return NULL;
	size = data_space;
	memset(skb, 0, size);
	skb->head = (u8*)(skb+1);
	skb->end = skb->head+size;
	skb->data = skb->tail = skb->head;
	skb->truesize = size;
	return skb;
}
void skb_free(struct sk_buff *skb)
{
	MEM_POOL *pPool;
	int i;
	if(skb==NULL)
		return;
	for(i=0; skb_mem_pools[i]!=NULL; i++)
	{
		pPool = skb_mem_pools[i];
		if(SKB_DATA_SPACE(pPool)==skb->truesize)
			break;
	}
	if(pPool)
	{
		mem_free(pPool, skb);
	}else
	{
		assert(0);
	}
}


int skb_init(void)
{
	mem_pool_init(&skb_512, sizeof(struct sk_buff)+512,
		MAX_512_COUNT,
		skb_512_static_buf,
		sizeof(skb_512_static_buf),
		"skb_512"
		);
	mem_pool_init(&skb_1518, sizeof(struct sk_buff)+1518,
		MAX_1518_COUNT,
		skb_1518_static_buf,
		sizeof(skb_1518_static_buf),
		"skb_1518"
		);
	return 0;
}

