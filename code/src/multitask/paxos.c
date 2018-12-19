#include "base.h"
#include "paxos.h"
#include "stdarg.h"

volatile OS_TASK * volatile TaskTable[MAX_NOS_TASKS];
volatile OS_TASK * volatile pCurTask;
volatile OS_TASK * volatile pNextTask;
volatile OS_TASK * volatile pMainTask;
volatile uint InOsIsr=0;
static int OsTimer;

extern int main_code(void);
extern void TaskSwitch( void );
/* Run the highest priority task in ready state.*/
static void OsScheduler(void)  //直接切换到下一个任务去运行
{
	uint		i;
	OS_TASK volatile *ptask;
	if( InOsIsr != 0 )	while( 1 );

	for( i=0; i<MAX_NOS_TASKS; i++ )
	{
		ptask = TaskTable[i];

		if(NULL==ptask) continue;
		if( ptask->State == NOS_STATE_READY )
		{
			pNextTask=ptask;
			TaskSwitch();  
			break;
		}		
	}
}

void OsSuspend(OS_TASK * pTask)
{
	uint flag;
	irq_save(flag);
	pCurTask->State=NOS_STATE_READY;    
	pTask->State = NOS_STATE_SUSPENDED;
	OsScheduler();
    irq_restore(flag);
}

void OsResume(OS_TASK *pTask)//可以在中断中使用
{
	uint flag;
	irq_save(flag);
	if( pTask->State == NOS_STATE_SUSPENDED )
	{
		pTask->State = NOS_STATE_READY;
		pTask->SleepTicks = 0;

		if( pTask->Priority < pCurTask->Priority )
		{
			pCurTask->State = NOS_STATE_READY;
			if(InOsIsr == 0) OsScheduler();
			else pNextTask = pTask;
		}
	}

	irq_restore(flag);
}

void OsSleep(uint	SleepTime)
{
	uint flag;
	irq_save(flag);
	if( SleepTime )
	{
		pCurTask->State = NOS_STATE_SLEEPING;
		pCurTask->SleepTicks = SleepTime;
	}
	else
	{
		pCurTask->State = NOS_STATE_READY;
	}
    OsScheduler();
	irq_restore(flag);
}

static void OsTimerLisr(void)
{
	uint i;
	OS_TASK volatile *pTask;
    BspTimerClear();
	for( i=0; i<MAX_NOS_TASKS; i++ )
	{
		pTask = TaskTable[i];
	
		if( pTask->State != NOS_STATE_SLEEPING ) continue;

		pTask->SleepTicks--;	
		if( pTask->SleepTicks ) continue ;
		pTask->State = NOS_STATE_READY;
        //xxprintf("ptask:%d,next:%d!\r\n",pTask->Priority,pNextTask->Priority);
		/* * If this task has higher priority than the interrupted task (pNextTask),
		 * switch pNextTask to this one.*/
		if( pTask->Priority < pNextTask->Priority ) pNextTask = pTask;
	}
}


int BSP_Init(void)
{
	int	Status;
	Status = BSP_INT_Init();
	Status = BSP_MEM_Init();
	BSP_TMR_Init();
	return Status;
}

void OsInit(void)
{
	int i;
	/* Initialize the BSP*/
	i=BSP_Init();
	for(i=0;i<MAX_NOS_TASKS;i++) TaskTable[i]=NULL;
	pCurTask  = NULL;
	OsTimer = BSP_TMR_Acquire(TIMER_OS,OsTimerLisr);
	pMainTask=OsCreate((OS_TASK_ENTRY)main_code,TASK_PRIOR_APP,0);

	BSP_TMR_Start(OsTimer);
	OsScheduler();
	while(1);	
}

void OsStop()
{
	BSP_TMR_Stop( OsTimer );
}

void OsStart()
{
	BSP_TMR_Start( OsTimer );
}

int get_cur_task(void)
{
    return pCurTask->Priority;
}

int IsMainCodeTask(void)
{
    int ret, flag;

    irq_save(flag);
    ret = (pCurTask->Priority == TASK_PRIOR_APP) ? 1 : 0;
    irq_restore(flag);
    return ret;
}

OS_TASK *OsCreate(OS_TASK_ENTRY pEntry,uint Priority,uint StackLen)
{
	OS_TASK volatile *pTask = NULL;
	uint flag;

	if((Priority > TASK_PRIOR_USER0) && 
       (Priority != TASK_PRIOR_APP) && (Priority != TASK_PRIOR_IDLE)) return NULL;
    
	irq_save(flag);
	if( TaskTable[Priority] == NULL)
	{
		if(TASK_PRIOR_APP!=Priority && StackLen<0x100) StackLen=0x100;//Prevent other task's memory not be destroyed
		pTask = (OS_TASK *)BSP_Malloc( sizeof(OS_TASK) + StackLen );
		if( pTask )
		{
			pTask->State        = NOS_STATE_READY;
			pTask->Priority     = Priority;
			pTask->pStack       = (uint *)&pTask[1];
			pTask->StackLen     = StackLen;
			pTask->pEntry       = pEntry;
			pTask->SleepTicks  = 0;
			pTask->MaxTimeSlice = 0xf1f1;
            //pTask->CurTimeSlice = 10;


			/* Set the Tasks initial CPSR
			 * In this implementation, all tasks will run in System mode.
			 */
			pTask->CPSR = ARM_MODE_SYS;
			/* R15  Initial value of PC is the task entry point. */
			pTask->PC = (uint)pTask->pEntry;
			/* R14:* Initial value of LR is the task destroy function.
			 * Therefore when the task exits we can reclaim memory used for TCB and stack. */
			pTask->LR = (uint) 0;

			/* R13: Initial stack pointer */
			//pTask->SP = (uint)( pTask->pStack + (StackLen >> 2) );
			if(TASK_PRIOR_APP==Priority)
				pTask->SP =0x30000000;	
			else
				pTask->SP = (uint)( pTask->pStack + (StackLen >> 2) );
			/* Add the task to the Task list.*/
			TaskTable[ Priority ] = pTask;
			/* See if the newly created task should start executing immediately. */
			if( pCurTask )
			{
				if( pTask->Priority < pCurTask->Priority )
				{
					pCurTask->State = NOS_STATE_READY;
					OsScheduler();
				}
			}
		}
	}
	irq_restore(flag);
	return (OS_TASK*)pTask;
}

