#include "string.h"
#include "queue.h"
#include "base.h"

void QueueInit(QUEUE_T *q, uchar buf[], uint size)
{
	q->head = 0;
	q->tail = 0;
	q->size = size;
	q->buf = buf;
}

int QueueIsEmpty(QUEUE_T *q)
{
	return (q->head == q->tail);
}

int QueueIsFull(QUEUE_T *q)
{
	return (q->head == q->tail - 1) || ((q->tail == 0) && (q->head == q->size - 1));
}

int QueuePutc(QUEUE_T *q, uchar ch)
{
	int flag;
	uint ret = 0;

	irq_save(flag);
	if(!QueueIsFull(q))
	{
		q->buf[q->head] = ch;
		q->head++;
		if(q->head == q->size)
			q->head = 0;

		ret = 1;
	}
	irq_restore(flag);
	return ret;
}

int QueueGetc(QUEUE_T *q, uchar *ch)
{
	int flag;
	uint ret = 0;

	irq_save(flag);
	if(!QueueIsEmpty(q))
	{
		*ch = q->buf[q->tail];
		q->tail++;
		if(q->tail == q->size)
			q->tail = 0;

		ret = 1;
	}

	irq_restore(flag);
	return ret;
}

/* end of queue.c */
