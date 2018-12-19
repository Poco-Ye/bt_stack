#ifndef __AUDIO_QUEUE_H__
#define __AUDIO_QUEUE_H__

typedef struct 
{
	unsigned int  head;
	unsigned int  tail;
	unsigned int  size;
	unsigned char *buf;
} QUEUE_T;

void QueueInit(QUEUE_T *q, unsigned char buf[], unsigned int  size);
int QueuePutc(QUEUE_T *q, unsigned char ch);
int QueueGetc(QUEUE_T *q, unsigned char *ch);
int QueueIsFull(QUEUE_T *q);
int QueueIsEmpty(QUEUE_T *q);

#endif /* end of __AUDIO_QUEUE_H__ */
