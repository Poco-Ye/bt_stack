#ifndef _NOS_H_
#define _NOS_H_

#define MAX_NOS_TASKS       32
#define MAX_NOS_PRIORITY    (MAX_NOS_TASKS - 1)

/* NOS task states*/
#define NOS_STATE_CURRENT					0
#define NOS_STATE_READY						1
#define NOS_STATE_SUSPENDED 				2
#define NOS_STATE_SLEEPING					3


/* ARM processor modes */
#define ARM_MODE_USR							0x10
#define ARM_MODE_FIQ							0x11
#define ARM_MODE_IRQ							0x12
#define ARM_MODE_SVC							0x13
#define ARM_MODE_ABT							0x17
#define ARM_MODE_UND							0x1B
#define ARM_MODE_SYS							0x1F

typedef void(*OS_TASK_ENTRY)(void);

/* NOS Task Control Block (TCB) */
typedef struct NOS_TASK_S
{
    uint        State;
    uint        Priority;
    uint        * pStack;
    uint        StackLen;
    OS_TASK_ENTRY  pEntry;
    uint        SleepTicks;     // non-zero while sleeping
    uint        CurTimeSlice;   // Not implemented
    uint        MaxTimeSlice;   // Not implemented

    /* Task processor state */
    uint        CPSR;       // Offset 32
    uint        PC;         // Offset 36
    uint        LR;         // Offset 40
    uint        SP;         // Offset 44
    uint        R0_R12[13]; // Offset 48
} OS_TASK;

/*Globals */
extern volatile OS_TASK	* volatile pCurTask;
extern volatile OS_TASK	* volatile pNextTask;


/* Function Prototypes*/
extern OS_TASK * OsCreate(OS_TASK_ENTRY pEntry, uint Priority, uint StackLen);
extern void OsSuspend(OS_TASK *pTask);
extern void OsResume(OS_TASK *pTask);
extern void OsSleep(uint SleepTime);
extern void OsStop();
extern void OsStart();

#endif
