/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"

//#define DEBUG_0

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */


typedef struct __node_t
{
    task_t tid;
    struct __node_t *next;
} node_t;

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks
U32 registered_commands[223];
node_t *head_tid;
static TCB* head_task;
extern void kcd_task(void);



/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      U_STACK_SIZE         |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      U_STACK_SIZE         |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      K_STACK_SIZE         |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      K_STACK_SIZE         |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

TCB *scheduler(void) 
{
    return head_task;
}

//SCHEDULER QUEUE IMPLEMENTAION (read over)

void add_task (TCB *task)
{
    TCB *temp = head_task;

    if(temp->prio > task->prio){
        task->next = temp;
        head_task = task;
    }
    else{
        while (temp->next != NULL)
        {
            if (temp->next->prio > task->prio)
            {
                TCB temp2 = *temp;
                temp->next = task;
                task->next = temp2.next;
                break;
            }
            temp = temp->next;
        }
    }
}

void remove_task(task_t tid)
{
    TCB *temp = head_task;

    if (temp->tid == tid)
    {
    	head_task = head_task->next;
    }
    else
    {
    	while (temp->next != NULL)
    	{
    		if(temp->next->tid == tid){
            temp->next = temp->next->next;
            return;
        }
      temp = temp->next;
    }
   }
}

void insert_node(int tid){
    node_t* newNode = k_mem_alloc(sizeof(node_t));
    newNode->next = head_tid->next;
    head_tid->next = newNode;
    newNode->tid = tid;
}

int remove_node(){
    node_t* tempNode = head_tid->next;
    int tid = tempNode->tid;
    head_tid->next = tempNode->next;
    k_mem_dealloc(tempNode);
    return tid;
}


/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{
    extern U32 SVC_RESTORE;

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;

    if (num_tasks > MAX_TASKS) {
    	return RTX_ERR;
    }

    head_tid = (node_t *)k_mem_alloc(sizeof(node_t));
    head_tid->next = NULL;
    head_tid->tid = 0;


    // create the first task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;
    p_tcb->next = NULL;
    p_tcb->mailbox.max_size = RAM_END;
    p_tcb->mailbox.trigger = 0;
    g_num_active_tasks++;
    gp_current_task = p_tcb;
    head_task = gp_current_task;

    int tid_kcd = (MAX_TASKS <= TID_KCD) ? (MAX_TASKS - 1) : TID_KCD;
    int k = (MAX_TASKS <= TID_KCD) ? MAX_TASKS - 2 : MAX_TASKS - 1;

	for (;k > 0; k--){
        if(k == TID_KCD || k == (int)TID_UART_IRQ){
            continue;
        }
        insert_node(k);
	}

    for(int i = 0; i< 223; i++){
        registered_commands[i] = 0;
    }

    // create the rest of the tasks
    int trigger = 0;
    p_taskinfo = task_info;
    for ( int i = 0; i < num_tasks; i++ ) {
        if(p_taskinfo->ptask == &kcd_task){ 
            trigger = 1;
            TCB *p_tcb = &g_tcbs[tid_kcd];
            if (k_tsk_create_new(p_taskinfo, p_tcb, TID_KCD) == RTX_OK) {
                g_num_active_tasks++;
            }
        }
        else{
            TCB *p_tcb = &g_tcbs[i+1];
            if (k_tsk_create_new(p_taskinfo, p_tcb, i+1) == RTX_OK) {
                remove_node();
                g_num_active_tasks++;
            }
        }
        p_taskinfo++;
    }
    if(trigger){
        insert_node(tid_kcd);
    }
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    if (g_num_active_tasks >= MAX_TASKS || p_taskinfo->ptask == NULL || p_taskinfo->prio == PRIO_NULL || p_taskinfo->prio == PRIO_RT || p_taskinfo->priv > 1)
    {
    	return RTX_ERR; 
    }

    p_tcb ->tid = tid;
    p_tcb->state = READY;
    if(tid == TID_KCD && MAX_TASKS <= TID_KCD){
        tid = MAX_TASKS - 1;
    }

    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (K_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//
        *(--sp) =  g_tcbs[tid].user_stack_ptr = (U32) k_alloc_p_stack(tid, p_taskinfo);
        
        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // kernel stack CPSR
    *(--sp) = (U32) INIT_CPSR_SVC;
    p_tcb->ksp = sp;
    p_tcb->prio = p_taskinfo->prio;
    p_tcb->priv = p_taskinfo->priv;
    p_tcb->ptask = p_taskinfo->ptask;
    p_tcb->mailbox.trigger = 0;
    add_task(p_tcb);
    
    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        MRS 	R1, CPSR
        PUSH 	{R1}
        STR     SP, [R0, #TCB_KSP_OFFSET]   ; save SP to p_old_tcb->ksp
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_KSP_OFFSET]   ; restore ksp of the gp_current_task
        POP		{R0}
        MSR		CPSR_cxsf, R0
        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();

    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        if(p_tcb_old->state != BLK_MSG){
            p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
        }
        k_tsk_switch(p_tcb_old);            // switch stacks
    }

    return RTX_OK;
}


/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
    if (gp_current_task->prio < gp_current_task->next->prio)
    {
        return RTX_OK;
    }

    if(gp_current_task->prio == PRIO_NULL){
    	return k_tsk_run_new();
    }
    TCB *temp = gp_current_task;
    remove_task(gp_current_task->tid);
    add_task(temp);
    int code  = k_tsk_run_new();

    return code;
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = %d, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */
    int remainder = stack_size % 8;
    stack_size = (remainder == 0) ? stack_size : (stack_size + 8 - remainder);

    //printf("New prio %d\n", prio);
    if (g_num_active_tasks >= MAX_TASKS || stack_size < U_STACK_SIZE || task_entry == NULL || prio == PRIO_NULL || task == NULL || prio == PRIO_RT) //|| stack_size < U_STACK_SIZE
    {
    	return RTX_ERR; //check if were at max tasks or we don't have enough space
    }

    *task = (task_t)remove_node();

    RTX_TASK_INFO rtx_task_info;
    rtx_task_info.tid = *task;
    rtx_task_info.prio = prio;
    rtx_task_info.state = READY;
    rtx_task_info.priv = 0;
    rtx_task_info.ptask = task_entry;    
    rtx_task_info.u_stack_size = stack_size;
    rtx_task_info.k_stack_size = K_STACK_SIZE;
    g_tcbs[*(task)].tid = *task;
    g_tcbs[*(task)].prio = prio;
    g_tcbs[*(task)].state = READY;
    g_tcbs[*(task)].priv = 0;
    g_tcbs[*(task)].ptask = task_entry;
    g_tcbs[*(task)].u_stack_size = stack_size;

    g_num_active_tasks++; 

    int code = k_tsk_create_new(&rtx_task_info,  &g_tcbs[*(task)], *task);

    k_tsk_run_new();
    return code;

}


void k_tsk_exit(void) 
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */

    if(gp_current_task == NULL){ 
        return;
    }
    if(!(gp_current_task->priv)){
        task_t tmpTID = gp_current_task->tid;
        gp_current_task->tid = 0;
        k_mem_dealloc((void *)gp_current_task->user_stack_ptr);
        gp_current_task->tid = tmpTID;
    }

    gp_current_task->state = DORMANT;
    g_num_active_tasks--;

    //mailbox free
    k_mem_dealloc(gp_current_task->mailbox.buffer);
    insert_node(gp_current_task->tid);
    remove_task(gp_current_task->tid);
    k_tsk_run_new();

    return;
}


int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */
    if(&g_tcbs[task_id] == NULL || task_id == NULL || prio == NULL || task_id > MAX_TASKS || task_id < 1 || prio < 1 || prio > 254){
        //check for invali/null input parameters
        return RTX_ERR;
    }

    if(gp_current_task->priv == 0 && g_tcbs[task_id].priv == 1){
        //check if a user tasks attempts to change kernel tasks
        return RTX_ERR;
    }

    if(g_tcbs[task_id].state == DORMANT){
        return RTX_ERR;
    }

    g_tcbs[task_id].prio = prio;
    remove_task(task_id);
    add_task(&g_tcbs[task_id]);
    
    k_tsk_run_new();
    return RTX_OK;    
}

int k_tsk_get_info(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get_info: entering...\n\r");
    printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL || g_tcbs[task_id].state == DORMANT) {
        return RTX_ERR;
    }
    /* The code fills the buffer with some fake task information. 
       You should fill the buffer with correct information    */
    //how do we get the current task?

    buffer->tid = g_tcbs[task_id].tid; //check if this is right
    buffer->prio = g_tcbs[task_id].prio;
    buffer->state = g_tcbs[task_id].state;
    buffer->priv = g_tcbs[task_id].priv;
    buffer->ptask = g_tcbs[task_id].ptask;
    buffer->k_stack_size = K_STACK_SIZE;
    buffer->u_stack_size = g_tcbs[task_id].u_stack_size;

    return RTX_OK;     
}

task_t k_tsk_get_tid(void)
{
#ifdef DEBUG_0
    printf("k_tsk_get_tid: entering...\n\r");
#endif /* DEBUG_0 */ 
    if(gp_current_task == NULL || gp_current_task->state != RUNNING){
        return 0;
    }
    return gp_current_task->tid;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
