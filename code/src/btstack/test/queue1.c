#include "queue1.h"
/*
Initialize the queue
*/
void QueInit(T_Queue *ptQue, char *buf, int size)
{
	ptQue->head = 0;
	ptQue->tail = 0;
	ptQue->size = size;
	ptQue->buf = buf;
}

/*
reset the queue pointer, all the data will be lost
*/
void QueReset(T_Queue *ptQue)
{
	ptQue->head = 0;
	ptQue->tail = 0;
}

/*
check the queue is empty or not
1-empty
0-not empty
*/
int QueIsEmpty(T_Queue *ptQue)
{
	if (ptQue->head == ptQue->tail)
		return 1;
	else
		return 0;
}

/*
check the queue is full or not
1-full
0-not full
*/
int QueIsFull(T_Queue *ptQue)
{
	if ((ptQue->head + 1) % ptQue->size == ptQue->tail)
		return 1;
	else
		return 0;
}

/*
get data from the queue
return the out data length
*/
int QueGets(T_Queue *ptQue, char *buf, int size)
{
	int iLen;	//the data length in the buffer
	int i;

	iLen = ((ptQue->head + ptQue->size) - ptQue->tail) % ptQue->size;
	if (iLen >= size)
		iLen = size;

	for(i=0; i<iLen; i++)
	{
		buf[i] = ptQue->buf[ptQue->tail];
		ptQue->tail = (ptQue->tail+1) % ptQue->size;
		/*
		ptQue->tail ++;
		if (ptQue->tail >= ptQue->size)
			ptQue->tail = 0;
			*/
	}
	return iLen;
}

/*
insert data to the queue
return the insert data length
*/
int QuePuts(T_Queue *ptQue, char *buf, int len)
{
	int iLen;	//the remained buffer size
	int i;

	if (ptQue->head == ptQue->tail)
		iLen = ptQue->size - 1;
	else
		iLen = ((ptQue->tail + ptQue->size) - ptQue->head) % ptQue->size - 1;
	if (iLen >= len)	   
		iLen = len;

	for(i=0; i<iLen; i++)
	{
		ptQue->buf[ptQue->head] = buf[i];
		ptQue->head = (ptQue->head+1) % ptQue->size;
		/*
		ptQue->head ++;
		if (ptQue->head >= ptQue->size)
			ptQue->head = 0;
			*/
	}
	return iLen;
}


/*
get one byte from the queue
*/
int QueGetc(T_Queue *ptQue, char *ch)
{
	if(QueIsEmpty(ptQue))	//queue is empty
		return 0;
	else
	{
		*ch = ptQue->buf[ptQue->tail];
		ptQue->tail = (ptQue->tail+1) % ptQue->size;
		/*
		ptQue->tail ++;
		if (ptQue->tail >= ptQue->size)
			ptQue->tail = 0;
			*/
		return 1;
	}
}

/*
insert one byte to the queue
*/
int QuePutc(T_Queue *ptQue, char ch)
{	
	if (QueIsFull(ptQue))	//queue is full
		return 0;
	else
	{
		ptQue->buf[ptQue->head] = ch;
		ptQue->head = (ptQue->head+1) % ptQue->size;
		/*
		ptQue->head ++;
		if (ptQue->head >= ptQue->size)
			ptQue->head = 0;
			*/
		return 1;
	}
}

/*
return how many empty bytes of the queue
*/
int QueCheckCapability(T_Queue *ptQue)
{
	int iLen;	//the remained buffer size

	if (ptQue->head == ptQue->tail)
		iLen = ptQue->size - 1;
	else
		iLen = ((ptQue->tail + ptQue->size) - ptQue->head) % ptQue->size - 1;

	return iLen;
}

/*
just get the data, but doesn't change the pointer
*/
int QueGetsWithoutDelete(T_Queue *ptQue, char *buf, int size)
{
	int iLen;	//the data length in the buffer
	int i, tail;

	tail = ptQue->tail;
	iLen = ((ptQue->head + ptQue->size) - tail) % ptQue->size;
	if (iLen >= size)
		iLen = size;

	for(i=0; i<iLen; i++)
	{
		buf[i] = ptQue->buf[tail];
		tail = (tail+1) % ptQue->size;
	}
	return iLen;
}

