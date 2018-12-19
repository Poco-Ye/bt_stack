
#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef struct{
	int head;
	int tail;
	int size;
	char *buf;
}T_Queue;

/*
Initialize the queue
*/
void QueInit(T_Queue *ptQue, char *buf, int size);

/*
reset the queue pointer, all the data will be lost
*/
void QueReset(T_Queue *ptQue);

/*
check the queue is empty or not
1-empty
0-not empty
*/
int QueIsEmpty(T_Queue *ptQue);

/*
check the queue is full or not
1-full
0-not full
*/
int QueIsFull(T_Queue *ptQue);

/*
get data from the queue
return the out data length
*/
int QueGets(T_Queue *ptQue, char *buf, int size);

/*
insert data to the queue
return the insert data length
*/
int QuePuts(T_Queue *ptQue, char *buf, int len);


/*
get one byte from the queue
*/
int QueGetc(T_Queue *ptQue, char *ch);


/*
insert one byte to the queue
*/
int QuePutc(T_Queue *ptQue, char ch);

/*
return how many empty bytes of the queue
*/
int QueCheckCapability(T_Queue *ptQue);

/*
just get the data, but doesn't change the pointer
*/
int QueGetsWithoutDelete(T_Queue *ptQue, char *buf, int size);


#endif

