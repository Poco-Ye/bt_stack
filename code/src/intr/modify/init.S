
#include "bcm5892_reg.h"
#include "irqs.h"

    .text

    .EQU    ABT_Stack,  0x1EFE0000  /* 32K bytes    */
    .EQU    UND_Stack,  0x1EFD8000  /* 32K bytes    */
    .EQU    FIQ_Stack,  0x1EFD0000  /* 64K bytes    */
    .EQU    IRQ_Stack,  0x1EFC0000  /* 64K bytes    */
    .EQU    SVC_Stack,  0x1EFB0000  /* 1856K -16Kbytes   */
    .EQU    USR_Stack,  0x30000000  /* 4096K bytes  */
    .EQU    TTB_BASE,   0x401FC000   

    .global     entry_point
    .global     ExceptionTable

    .extern     AbortHandler
    .extern     SWIHandler
    .extern     main_code

entry_point:
		b reset_start
		.WORD MonitorInfo
reset_start:              
    /**** initialization*/
    BL  ClearIDCache 
    @BL ClearTTB
    MRS R0, CPSR    
    BIC R0, R0, #0x1F  
    ORR R0, R0, #0xD1      /* FIQ mode with FIQ and IRQ msked */
    MSR CPSR_c, R0   
    LDR R13, =FIQ_Stack

    MRS R0, CPSR     
    BIC R0, R0, #0x1F  
    ORR R0, R0, #0xD2      /* IRQ mode with FIQ and IRQ msked */
    MSR CPSR_c, R0
    LDR R13, =IRQ_Stack

    MRS R0, CPSR
    BIC R0, R0, #0x1F
    ORR R0, R0, #0xD3      /* SVC mode with FIQ and IRQ msked */ 
    MSR CPSR_c, R0
    LDR R13, =SVC_Stack

    MRS R0, CPSR
    BIC R0, R0, #0x1F
    ORR R0, R0, #0xD7      /* ABORT mode with FIQ and IRQ msked */
    MSR CPSR_c, R0
    LDR R13, =ABT_Stack

    MRS R0, CPSR
    BIC R0, R0, #0x1F
    ORR R0, R0, #0xDB     /* Undefine mode with FIQ and IRQ msked */
    MSR CPSR_c, R0
    LDR R13, =UND_Stack

    MRS R0, CPSR
    BIC R0, R0, #0x1F
    ORR R0, R0, #0xDF      /* USER Sys mode with FIQ and IRQ msked */
    MSR CPSR_c, R0
    LDR R13, =USR_Stack

    BL  InitMMUentry
    
    /****       Copy Exception table to 0x00000000 */
    MOV R8, #0x00
    LDR R9, =ExceptionTable
    LDMIA   R9!,{R0-R7}
    STMIA   R8!,{R0-R7}
    LDMIA   R9!,{R0-R7}
    STMIA   R8!,{R0-R7}

/*copy low power consumption code to sram*/
	LDR   R3, =sram_sector_start
	LDR   R1, =sram_sector_end      /* top of zero init segment *//* 取ZI段的结束地址 */
	LDR   R2, =Image__data__End
LoopCopy:									/* 将ZI段清零*/
	CMP     	 R3, R1  				              /* zero init */
	BCS	     end_Copy
	LDRCC    R0, [R2],#4  
	STRCC    R0, [R3], #4  					/*如果r3<r1，将r2内容写入到r3地址单元中，然后r3=r3+1*/
	B     	 LoopCopy      					/*如果r3<r1(即C=0)，则跳转到LoopZI EQ*/   
end_Copy:

    .extern Image__bss__ZI__Base                    
    .extern Image__bss__ZI__Limit  

    LDR R3, =Image__bss__ZI__Base 
    LDR R1, =Image__bss__ZI__Limit
    MOV R2, #0                          
LoopZI: /* clear bbs segment*/
    CMP R3, R1              
    BCS end_bss_zero
    STRCC   R2, [R3], #4   
    B   LoopZI             
    
end_bss_zero:
    
    /**** I-Cache and D-Cache */
    MRC P15, 0, R1, C1, C0, 0
    ORR R1, R1, #0x00000004
    ORR R1, R1, #0x00001000
    MCR P15, 0, R1, C1, C0, 0
    MOV R0, #0xDF      /* USER Sys mode with FIQ and IRQ masked */
    MSR CPSR_c, R0

    LDR R0,=OsInit
    MOV PC,R0


ExceptionTable:
    LDR PC, ResetVector
    LDR PC, UndefVector
    LDR PC, SoftwareVector
    LDR PC, PrefetchVector
    LDR PC, DataVector
    LDR PC, ReservedVector
    LDR PC, IrqVector
    LDR PC, FiqVector

ResetVector:
    .word   0x02000000  /*Boot Rom start address*/
UndefVector:
    .word   AbortHandler
SoftwareVector:
    .word   SWI_Handler
PrefetchVector:
    .word   AbortHandler
DataVector:
    .word   AbortHandler
ReservedVector:
    .word   AbortHandler
IrqVector:
    .word   Irq_Handler
FiqVector:
    .word   Fiq_Handler

soft_interrupt_lock:
    .word   0

/*Multitask process start*/
.global TaskSwitch
TaskSwitch:
    /*Save R0  on the stack to create a working register*/
    STMDB   SP!, {r0}
    LDR     r0, =pCurTask
    LDR     r0, [r0, #0]
    
    /*No context to save if pCurTask is NULLPTR*/
    CMP     r0, #0
    BNE     TS_0
    ADD     SP, SP, #4
    MRS     r2, CPSR
    B       RestoreNextTask

TS_0:
    /*Save Task LR in TCB as LR and PC */
    STR     LR, [r0, #+40]
    STR     LR, [r0, #+36] @ Save as task PC

    /*Save processor registers r0-r12 in TCB*/
    ADD     LR, r0, #+52
    STMIA       LR, {r1-r12}
    LDMIA       SP!, {r1}
    STR     r1, [r0, #+48]
    
    /*Save SP of task in TCB*/
    STR     SP, [r0, #+44]

    /*Save CPSR of task in TCB*/
    MRS     r2, CPSR
    STR     r2, [r0, #+32]   @ Save as task CPSR

    /*Now switch to new task*/
    /*Disable interrupts if not already*/
    ORR     r1, r2, #0xC0
    MSR         CPSR_c, r1
    
RestoreNextTask:
    /*Load SP and LR from TCB in SYS/ USR mode*/
    LDR   r0, =pNextTask
    LDR   r0, [r0, #0]

    LDR   LR, [r0, #+40]
    LDR   SP, [r0, #+44]

    /*Switch to UNDF Mode to store new tasks CPSR in UNDF_SPSR*/
    BIC r2, r2, #0x1F
    ORR r2, r2, #0xDB   @ SVC modes with FIQ and IRQ msked
    MSR CPSR_fsxc, r2
    LDR r1, [r0, #+32]
    MSR SPSR_fsxc, r1
     
    /*Load LR_UNDF with Task PC (return address)*/
    LDR   LR, [r0, #+36]
    STMDB SP!, {LR}
    
    
    /*Update current task pointer*/
    LDR r1, =pCurTask
    STR r0, [r1]
    MOV r1, #0              @ NOS_STATE_CURRENT = 0
    STR r1, [r0, #0]        @ Update task state

    /*Load processor regs*/
    ADD     r14, r0, #+48
    LDMIA   r14, {r0-r12}
    LDMIA   SP!, {LR}
            
    SUBS    PC, LR, #0


Irq_Handler:
    SUB     r14, r14, #4
    STMDB   SP!, {r0-r1}
    ldr     R0,=g_interrupt_addr
    str     LR,[R0]
    LDR     r0, =pCurTask 
    LDR     r0, [r0, #0]
    SUBS    r1, r0, #0   @ be sure current state is NOS_STATE_CURRENT
    BNE     SaveTaskContext
    /*We should never get here....*/
IrqFault:      
    B       IrqFault

SaveTaskContext:
    /*Save processor state on current TCB of task */
    STR     r14, [r0, #+36]     @ Save IRQ LR as task PC
    MRS     r1, SPSR            @ Get task CPSR
    STR     r1, [r0, #+32]       @ Save IRQ SPSR as task CPSR
    ADD     r1, r0, #+56        @ Point R1 to TCB.r2
    STMIA   r1, {r2-r12}        @ Save task R2-r12
    LDMIA   SP!, {r2-r3}        @ Get r0 and r1 of task
    ADD     r1, r0, #+48        @ Point R1 to TCB.r0
    STMIA   r1, {r2-r3}         @ Save task r0 and r1
    ADD     r1, r0, #+40        @ Point R1 to TCB.LR
    STMIA   r1, {r14}^          @ Save LR of task
    ADD     r1, r0, #+44        @ Point R1 to TCB.SP
    STMIA   r1, {r13}^          @ Save SP of task

    /*Change this state of task to ready*/
    MOV     r1, #1        @ NOS_STATE_READY = 1
    STR     r1, [r0, #+0]

    /*Set pNextTask to reference this task*/
    LDR     r1, =pNextTask
DoIsr:
    /*Set a flag to indicate we are running an ISR....*/
    LDR     r1, =InOsIsr
    LDR     r2, [r1, #0]
    ADD     r2, r2, #1
    STR     r2, [r1, #0 ]

    /*Read VICIRQSTATUS then get victor number  */
    ldr R3, =VIC0_R_VICIRQSTATUS_MEMADDR
    ldr R3, [R3,#0]
    teq R3, #0
    beq 1001f
    ldr R1, =SVIC_IRQ_END       
    clz R2, R3
    sub R0,R1,R2
    b   1003f
1001:   
    ldr R3,=VIC1_R_VICIRQSTATUS_MEMADDR
    ldr R3, [R3,#0]
    teq R3, #0
    beq 1002f
    ldr R1, =OVIC0_IRQ_END      
    clz R2, R3
    sub R0,R1,R2
    b   1003f
1002:   
    ldr R3, =VIC2_R_VICIRQSTATUS_MEMADDR
    ldr R3, [R3,#0]
    teq R3, #0
    beq 1003f
    ldr R1, =OVIC1_IRQ_END      
    clz R2, R3
    sub R0,R1,R2
    b   1003f
1003:
    /*compare interrupt id, if id more then mast id,jump to end*/
    CMP R0, #NR_IRQS
    BCC VALID_IRQ
    B   IRQ_END

VALID_IRQ:
    /*The'vecotr'obtained is just an index into the IsrTable which contains */
    /*the actual ISR address */
    LDR R1, =IsrTable
    LDR R2, [R1, R0, LSL #2]
    MOV LR,PC
    BX  R2

IRQ_END:
    /*Disable interrupts and switch to Sys mode.*/
    MRS     r2, CPSR
    ORR     r2, r2, #0xDF
    MSR     CPSR_c, r2

    LDR     r1, =InOsIsr
    LDR     r2, [r1, #0]
    ADD     r2, r2, #-1
    STR     r2, [r1, #0]
    /*Resume state of pNextTask*/
    B RestoreNextTask
    /*Multitask process end*/


Fiq_Handler:
    SUB LR, LR,#4
    STMFD   SP!, {R0-R12,LR}

    /*Read VICFIQSTATUS then get victor number */
    ldr R3, =VIC0_R_VICFIQSTATUS_MEMADDR
    ldr R3, [R3,#0]
    teq R3, #0
    beq 1001f
    ldr R1, =SVIC_IRQ_END       
    clz R2, R3
    sub R0,R1,R2
    b   1003f
1001:   
    ldr R3,=VIC1_R_VICFIQSTATUS_MEMADDR
    ldr R3, [R3,#0]
    teq R3, #0
    beq 1002f
    ldr R1, =OVIC0_IRQ_END      
    clz R2, R3
    sub R0,R1,R2
    b   1003f
1002:   
    ldr R3, =VIC2_R_VICFIQSTATUS_MEMADDR
    ldr R3, [R3,#0]
    teq R3, #0
    beq 1003f
    ldr R1, =OVIC1_IRQ_END      
    clz R2, R3
    sub R0,R1,R2
    b   1003f
1003:

    /**** 比较获取的中断号，如果大于31，则不处理 */
    CMP R0, #0x20
    BCC VALID_FIQ
    B   FIQ_END

VALID_FIQ:
    /*The 'vecotr' obtained is just an index into the IsrTable which contains */
    /*the actual ISR address */
    LDR R1, =IsrTable
    LDR R2, [R1, R0, LSL #2]
    MOV LR,PC
    BX  R2
 
FIQ_END:
    LDMFD   SP!, {R0-R12, PC}^          /*restore R0 */

SWI_Handler:
    STMFD   SP!, {R0-R12,LR}

    MRS R1, CPSR
    BIC R1, R1, #0xC0
    MSR CPSR,R1
    ldr r0, =k_CallAddr
    str lr, [r0]

    LDR R0, [LR,#-4]
    BIC R0, R0, #0xFF000000
    MOV R1,R3    /*R3,应用层传递参数地址用，不能修改*/
    BL  SWIHandler

    LDMFD   SP!, {R0-R12,PC}^

.global RunStartAddr
.extern RunStartAddr
RunStartAddr:

    MRS       r1, CPSR
    /*关中断以保证spsr不被更改 转换到SVC模式使用其SPSR */
    mov r2,#0xd3
    MSR CPSR_c, R2

    BIC r1, r1, #0x1F
    ORR r1, r1, #0x10
    MSR spsr,r1
    movs    pc,r0

/*MMU config*/
    .EQU Section,       0b0010      /* L1 PT 1MB Section entry */
    .EQU Section_min,   0b0001      /* L1 PT 4KB Section entry */
    .EQU TTBit,         0b10000     /* Must be set in non-Fault PTE */
    .EQU DOMAIN15,      0b111100000 /* Domain bits */

/*AP (access permission) */
    .EQU AP_NA_NA,         0b000000000000     /* priv=NA, usr=NA */
    .EQU AP_RW_NA,         0b010000000000     /* priv=RW, usr=NA */
    .EQU AP_RW_RO,         0b100000000000     /* priv=RW, usr=RO */
    .EQU AP_RW_RW,         0b110000000000     /* priv=RW, usr=RW */

/****************************************  
****  P1: 虚拟地址高位
****  P2: 物理地址高位
****  P3: B, C属性控制位
****  P4: 域属性和读写权限控制
*****************************************/
.macro    MAPSEC P1, P2, P3, P4
    MOV         R2, #TTBit | Section    /* make this descriptor pattern a 1MB "section" type */
    ORR         R2, R2, #\P4

    LDR         R1, =\P2                /* the section number (0x000..0xFFF) value to write */
    ORR         R3, R2, R1,LSL#20
    ORR         R3, R3,#\P3             /* set cachable and bufferable attributes for this section */

    LDR         R1, =\P1                /* get offset for PTE */
    STR         R3, [R0, R1,LSL#2 ]      /* store PTE at TTB base (R0) + sec# /*(3:2) = VA */
.endm

.macro    MAPSEC_MIN P1, P2, P4
    MOV         R2, #TTBit | Section_min /* make this descriptor pattern a 1MB "section" type */
    ORR         R2, R2, #\P4
    LDR         R1, =\P2                 /* the section number (0x000..0xFFF) value to write */
    ORR         R3, R2, R1,LSL#8

    LDR         R1, =\P1                 /* get offset for PTE */
    STR         R3, [R0, R1,LSL#2 ]       /* store PTE at TTB base (R0) + sec# /*(3:2) = VA */
.endm


InitMMUentry:

    MRC P15, 0, R0, C1, C0, 0    /* read CP15 register 1 into r0 */
    BIC R0, R0, #0x1     /* clear bit 0 (the MPU enable bit) */
    MCR P15, 0, R0, C1, C0, 0    /* write value back */

    MOV R0, #0
    MCR P15, 0, R0, C7, C7, 0        /* invalidate caches */
    MCR P15, 0, R0, C8, C7, 0        /* invalidate TLBs */

init_ttb:
    LDR R0, =TTB_BASE           /* L1 Page Table Entries stored at 0x204FC000 (TTB), must be on 16KB boundary */
    MCR P15, 0, R0, C2, C0, 0           /* write to CP15 register 2 (translation table base) */

    /* Create translation table for flat mapping */
    /* Top 12 bits of VA is pointer into table of 2^12=4096 entries of 1MB blocks */
    /* Create 4096 entries from 000xxxxx to fffxxxxx */
    LDR R1, =0xfff   /* loop counter to go from 0xFFF to 0x000 */
    MOV R2, #TTBit | Section  /* make this descriptor pattern a 1MB "section" type */
    ORR R2, R2, #DOMAIN15 | AP_RW_NA

init_ttb_1:
    ORR R3, R2, R1, LSL#20
    STR R3, [R0, R1, LSL#2]
    SUBS    R1, R1, #1
    BPL init_ttb_1

    LDR R0, =TTB_BASE   /* Reset R0 to TTB base for remapping specific mappings */

    /*system heap district*/
    MAPSEC 0x1EE, 0x400, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x1EF, 0x401, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x2DF, 0x400, 0b1100, DOMAIN15 | AP_RW_RW   /*app param area,limited to 16KB*/  

    /*TTB & DMA district*/
    MAPSEC 0x401, 0x401, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x402, 0x402, 0b0000, DOMAIN15 | AP_RW_NA
    /*Monitor district,0x20000000-0x21000000,13M physical memory*/
    MAPSEC 0x200, 0x403, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x201, 0x404, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x202, 0x405, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x203, 0x406, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x204, 0x407, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x205, 0x408, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x206, 0x43C, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x207, 0x43B, 0b1100, DOMAIN15 | AP_RW_RW
/*added by shics +3M*/ 
    MAPSEC 0x208, 0x43A, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x209, 0x439, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x20A, 0x438, 0b1100, DOMAIN15 | AP_RW_RW
 
    /*Font Lib*/
    MAPSEC 0x20B, 0x409, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x20C, 0x40A, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x20D, 0x40B, 0b1100, DOMAIN15 | AP_RW_NA        
    MAPSEC 0x20E, 0x40C, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x20F, 0x40D, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x210, 0x40E, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x211, 0x40F, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x212, 0x410, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x213, 0x411, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x214, 0x412, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x215, 0x413, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x216, 0x414, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x217, 0x415, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x218, 0x416, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x219, 0x417, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x21A, 0x418, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x21B, 0x419, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x21C, 0x41A, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x21D, 0x41B, 0b1100, DOMAIN15 | AP_RW_NA
     
    /*Base So 6M*/
    MAPSEC 0x220, 0x41B, 0b1100, DOMAIN15 | AP_RW_NA     
    MAPSEC 0x221, 0x41A, 0b1100, DOMAIN15 | AP_RW_NA     
    MAPSEC 0x222, 0x419, 0b1100, DOMAIN15 | AP_RW_NA     
    MAPSEC 0x223, 0x418, 0b1100, DOMAIN15 | AP_RW_NA     
    MAPSEC 0x224, 0x417, 0b1100, DOMAIN15 | AP_RW_NA     
    MAPSEC 0x225, 0x416, 0b1100, DOMAIN15 | AP_RW_NA     
    MAPSEC 0x226, 0x415, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x227, 0x414, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x228, 0x413, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x229, 0x412, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x22A, 0x411, 0b1100, DOMAIN15 | AP_RW_NA
    MAPSEC 0x22B, 0x410, 0b1100, DOMAIN15 | AP_RW_NA
    
    //MPATCH 1M
    MAPSEC 0x270, 0x41C, 0b1100, DOMAIN15 | AP_RW_NA
	
    /*Application district 0x28000000-0x2A000000*/
    /*master application,2M*/
    MAPSEC 0x280, 0x41D, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x281, 0x41E, 0b1100, DOMAIN15 | AP_RW_RW
    /*slave application,30M*/	   
    MAPSEC 0x282, 0x41F, 0b1100, DOMAIN15 | AP_RW_RW	 
    MAPSEC 0x283, 0x420, 0b1100, DOMAIN15 | AP_RW_RW	 
    MAPSEC 0x284, 0x421, 0b1100, DOMAIN15 | AP_RW_RW	 
    MAPSEC 0x285, 0x422, 0b1100, DOMAIN15 | AP_RW_RW 
    MAPSEC 0x286, 0x423, 0b1100, DOMAIN15 | AP_RW_RW	 
    MAPSEC 0x287, 0x424, 0b1100, DOMAIN15 | AP_RW_RW	 
    MAPSEC 0x288, 0x425, 0b1100, DOMAIN15 | AP_RW_RW	 
    MAPSEC 0x289, 0x426, 0b1100, DOMAIN15 | AP_RW_RW	 
    MAPSEC 0x28A, 0x427, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x28B, 0x428, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x28C, 0x429, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x28D, 0x42A, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x28E, 0x42B, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x28F, 0x42C, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x290, 0x42D, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x291, 0x42E, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x292, 0x42F, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x293, 0x430, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x294, 0x431, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x295, 0x432, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x296, 0x433, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x297, 0x434, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x298, 0x435, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x299, 0x436, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x29A, 0x437, 0b1100, DOMAIN15 | AP_RW_RW	
/*     MAPSEC 0x29B, 0x438, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x29C, 0x439, 0b1100, DOMAIN15 | AP_RW_RW	
    MAPSEC 0x29D, 0x43A, 0b1100, DOMAIN15 | AP_RW_RW	*/ /*deleted by shics -3M*/
	
    /* stack district < 3MB,re-use Fontlib space*/
    MAPSEC 0x2F1, 0x40A, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2F2, 0x40B, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2F3, 0x40C, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2F4, 0x40D, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2F5, 0x40E, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2F6, 0x40F, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2F7, 0x410, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2F8, 0x411, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2F9, 0x412, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2FA, 0x413, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2FB, 0x414, 0b1100, DOMAIN15 | AP_RW_RW
    MAPSEC 0x2FC, 0x415, 0b1100, DOMAIN15 | AP_RW_RW
    
    MAPSEC 0x2FD, 0x43D, 0b1100, DOMAIN15 | AP_RW_RW 
    MAPSEC 0x2FE, 0x43E, 0b1100, DOMAIN15 | AP_RW_RW     
    MAPSEC 0x2FF, 0x43F, 0b1100, DOMAIN15 | AP_RW_RW    
    MAPSEC 0x300, 0x440, 0b1100, DOMAIN15 | AP_RW_RW /*this memory only has virtual space,no physical memory,for application compatibily*/  

    /*init_domains*/
    MOV     R0,#(0b01 << 30)        /* must define behavior for domain 15 (31:30), set client */
    MCR     P15, 0, R0, C3, C0, 0   /* write to CP15 reg */

    /*enable the MMU */
    MRC     P15, 0, R0, C1, C0, 0   /* read CP15 register 1 into r0 */
    BIC     R0, R0, #(0x1  <<12)    /* ensure I Cache disabled */
    BIC     R0, R0, #(0x1  <<2)     /* ensure D Cache disabled */
    ORR     R0, R0, #0x1            /* enable MMU before scatter loading */
    MCR     P15, 0, R0, C1, C0, 0    /* write CP15 register 1 */

    BX LR
    
.global ClearIDCache
.extern ClearIDCache
ClearIDCache:
    MRS r3,cpsr     /*store cpsr then disable IRQ&FIQ */
    ORR r0,r3,#0xC0
    MSR cpsr_c,r0
    MRC P15, 0, R2, C1, C0, 0       
    BIC R0, R2, #0x00001000     /* disabled I Cache disabled */
    BIC R0, R2, #0x00000004      /* disabled D Cache disabled */
    MCR P15, 0, R0, C1, C0, 0     
    mov r0,#0
    MCR p15, 0, r0, c7, c14, 0     /* Clean and Invalidate Entire Data Cache  */
    MCR p15, 0, r0, c7, c5, 0      /* Invalidate Entire Instruction Cache  */
    MCR p15, 0, r0, c7, c10, 4     /* drain write buffer  */
    MCR P15, 0, R2, C1, C0, 0      /* Re-enable I/D cache */
    MSR cpsr_c,r3                  /*restore cpsr enable IRQ&FIQ */ 
    mov pc, lr       

.global irq_save_asm
irq_save_asm:
    mrs r1,cpsr
    orr r2,r1,#0xC0
    msr cpsr_c,r2
    str r1,[r0]
    bx lr
	
.global irq_restore_asm
irq_restore_asm:
	ldr r1,[r0]
	msr cpsr_c,r1
	bx lr

.global swi_addr
swi_addr:
    ldr r1, =k_CallAddr
    ldr r0, [r1]
    bx  lr
    
    .data
;.global k_CallAddr
k_CallAddr: 
    .word entry_point
 
