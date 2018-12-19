#include "base.h"

#define SYS_MEMORY_SIZE (1024 * 100)
uchar	SysMemory[SYS_MEMORY_SIZE ];

/* Memory allocations are aligned on 32-bit boundaries.*/
#define	roundmb(x)	(uint)( ((uint)(x) + 3) & 0xFFFFFFFC )

typedef struct	FREE_BLOCK_S
{
	struct FREE_BLOCK_S		 *	mnext;		//pNext;
	uint							mlen;			//Len;
} FREE_BLOCK;

typedef struct	ALLOCATED_BLOCK_S
{
	uint							Size;
	FREE_BLOCK					 * pPool;
} ALLOCATED_BLOCK;

static void MemMgrInit(uint	Base,uint Len)
{
	FREE_BLOCK	 * pMem = (FREE_BLOCK*)Base;

	Len &= 0xFFFFFFFC;
	Base += sizeof( FREE_BLOCK );
	Len  -= sizeof( FREE_BLOCK );
	pMem->mnext = (FREE_BLOCK*)Base;
	pMem->mlen  = 0;

	pMem = (FREE_BLOCK *)Base;
	pMem->mnext = NULL;
	pMem->mlen  = Len;
}

void *BSP_Malloc(uint nbytes)
{
	void * pPool=SysMemory;
	FREE_BLOCK * q = (FREE_BLOCK *)pPool;
	FREE_BLOCK * p = q->mnext;
	FREE_BLOCK * leftover;
	ALLOCATED_BLOCK * pRet = NULL;

	if( nbytes == 0 )
	{
		return( NULL );
	}
	if( p == (FREE_BLOCK *) NULL )
	{
		return( NULL );
	}
	nbytes = (uint) roundmb(nbytes) + sizeof(ALLOCATED_BLOCK);
	for( ;p != (FREE_BLOCK *) NULL;q=p, p=p->mnext )
	{
		if( p->mlen == nbytes ) 
		{
			q->mnext = p->mnext;
			pRet = (ALLOCATED_BLOCK *)p;
			break;
		} 
		else 
		{
			if ( p->mlen > nbytes ) 
			{
				leftover = (FREE_BLOCK *)( (uchar *)p + nbytes );
				q->mnext = leftover;
				leftover->mnext = p->mnext;
				leftover->mlen = p->mlen - nbytes;
				pRet = (ALLOCATED_BLOCK *)p;
				break;
			}
		}
	}	

	if( pRet )
	{
		/*
		 * Record the size of the allocation and note the pool from it was allocated.
		 */
		pRet->Size = nbytes;
		pRet->pPool = pPool;
		pRet++;
	}
	return( pRet );
}

void BSP_Free(void * pData)
{
	FREE_BLOCK * p, *q;
	uchar * top;
	ALLOCATED_BLOCK * pMem = (ALLOCATED_BLOCK *)((uchar *)pData - sizeof(ALLOCATED_BLOCK) );
	FREE_BLOCK *	block = (FREE_BLOCK *)pMem;
 	uint Size = pMem->Size;
 	FREE_BLOCK * pPool = pMem->pPool;


	for( p=pPool->mnext, q=pPool;
	     p != (FREE_BLOCK *) NULL && p < block ;
	     q=p,p=p->mnext )
		;
	if (((top=q->mlen+(uchar *)q)>(uchar *)block && q!= pPool) ||
	    (p!=NULL && (Size+(uchar *)block) > (uchar *)p) ) 
	{
		return;
	}
	if ( q!= pPool && top == (uchar *)block )
			q->mlen += Size;
	else {
		block->mlen = Size;
		block->mnext = p;
		q->mnext = block;
		q = block;
	}
	if ( (uchar *)( q->mlen + (uchar *)q ) == (uchar *)p) {
		q->mlen += p->mlen;
		q->mnext = p->mnext;
	}
}

void BSP_MEM_Init(void)
{
	MemMgrInit( (uint)SysMemory, SYS_MEMORY_SIZE );
   	return ;
}

