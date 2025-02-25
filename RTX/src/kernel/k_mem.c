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

/**************************************************************************/ /**
  * @file        k_mem.c
  * @brief       Kernel Memory Management API C Code
  *
  * @version     V1.2021.01.lab2
  * @authors     Yiqing Huang
  * @date        2021 JAN
  *
  * @note        skeleton code
  *
  *****************************************************************************/

/**
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "Serial.h"
#include "k_task.h"

//#define DEBUG_0

#ifdef DEBUG_0
#include "printf.h"

#endif /* DEBUG_0 */

/*
 *==========================================================================
 *                            TODO LIST
 *==========================================================================
    - For alloc function: When allocating a new block check if starting_adr + size > RAM_END
 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = K_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.cs
const U32 g_p_stack_size = U_STACK_SIZE;

int heap_size;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][K_STACK_SIZE >> 2] __attribute__((aligned(8)));

// process stack for tasks in SYS mode
U32 g_p_stacks[MAX_TASKS][U_STACK_SIZE >> 2] __attribute__((aligned(8)));

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

typedef struct
{
    int size;
    unsigned int task_id;
} header_T;

typedef struct __node_t
{
    int size;
    struct __node_t *next;
} node_t;

node_t *head;
node_t* tail;
unsigned int end_addr;

U32 *k_alloc_k_stack(task_t tid)
{
    return g_k_stacks[tid + 1];
}

U32 *k_alloc_p_stack(task_t tid, RTX_TASK_INFO *rtx_info)
{   
    task_t tmpTID = gp_current_task->tid;
    gp_current_task->tid = 0;

    void* returnVal =(void *) ((U32) k_mem_alloc(rtx_info->u_stack_size) + rtx_info->u_stack_size);

    gp_current_task->tid = tmpTID;
    
    return returnVal;
}

int k_mem_init(void)
{

	 end_addr = (unsigned int)&Image$$ZI_DATA$$ZI$$Limit;
#ifdef DEBUG_0
    printf("k_mem_init: image ends at 0x%x\r\n", end_addr);
    printf("k_mem_init: RAM ends at 0x%x\r\n", RAM_END);
#endif /* DEBUG_0 */

    head = (node_t *)end_addr;
    node_t *real_head = (node_t*)((U32)head + sizeof(node_t));
    tail = (node_t*)((RAM_END + 1) - sizeof(node_t));

    head->size = sizeof(node_t);
    tail->size = sizeof(node_t);
    heap_size = tail - real_head;
    real_head->size = heap_size;

    real_head->next = tail;
    tail->next = NULL;
    head->next = real_head;

    return RTX_OK;
}



void *k_mem_alloc(size_t size)
{

#ifdef DEBUG_0
//    printf("k_mem_alloc: requested memory size = %d\r\n", size);
#endif /* DEBUG_0 */
	size += sizeof(header_T);
    int remainder = size % 8;
    size = (remainder == 0) ? size : (size + 8 - remainder);


   if (size > heap_size) 
   {
   	//printf("Here with heap_size: %d\n", heap_size);
       return NULL;
   }

    node_t *currNode = head->next;
    node_t *prevNode = head;

    while (currNode != tail)
    {
        if (currNode->size >= size)
        {
        	node_t *tempNext = currNode->next;
        	U32 oldSize = currNode->size;

        	header_T *memBlockHeader = (header_T *)currNode;
            memBlockHeader->size = size;
            memBlockHeader->task_id = gp_current_task->tid; 

			// if(oldSize > size + sizeof(header_T))
            if(oldSize > size)
			{
				node_t *newFreeBlock = (node_t *)((U32)memBlockHeader + size);
				newFreeBlock->size = oldSize - size;
				prevNode->next = newFreeBlock;
				newFreeBlock->next = tempNext;
			}
			else
			{
				memBlockHeader->size = oldSize;
				prevNode->next = tempNext;
			}
            return (void *)((U32)memBlockHeader + sizeof(header_T));
        }
		prevNode = currNode;
		currNode = currNode->next;
    }
    return NULL;
}

int k_mem_dealloc(void *ptr)
{
#ifdef DEBUG_0
    printf("k_mem_dealloc: freeing 0x%x\r\n", (U32)ptr);
#endif /* DEBUG_0 */

    if ((U32)ptr >= (U32)tail || (U32)ptr <= (U32)head || ((U32)ptr &0x3))
    {
        return RTX_ERR;
    }
    node_t *currNode = head->next;
    node_t *prevNode = head;
    header_T *header = (header_T*)((U32)ptr - sizeof(header_T));

    if (gp_current_task->tid != header->task_id)
    {
        return RTX_ERR;
    }

    while((U32)header > (U32)currNode){
    		prevNode = currNode;
    		currNode = currNode->next;
    }

    if((U32)header == (U32)currNode){
    	return RTX_ERR;
    }

    U32 startingAddr = (U32)prevNode + prevNode->size;
    while(startingAddr < (U32)header){
    	startingAddr = startingAddr + ((header_T*)startingAddr)->size;
    }

    if(startingAddr > (U32)header){
    	return RTX_ERR;
    }

   U32 tempHeaderSize = header->size;
//   printf("the header size is %u\n", tempHeaderSize);
   node_t *newFreeBlock = (node_t *)header;
   prevNode->next = newFreeBlock;
   newFreeBlock->next = currNode;
   newFreeBlock->size = tempHeaderSize;

   if(currNode != tail) {
	   if((U32)newFreeBlock + newFreeBlock->size == (U32)currNode){
		   newFreeBlock->size = newFreeBlock->size + currNode->size;
		   prevNode->next = newFreeBlock;
		   newFreeBlock->next = currNode->next;
	   }
   }

   if(prevNode != head) {
	   if((U32)prevNode + prevNode->size == (U32)newFreeBlock){
		   prevNode->size = prevNode->size + newFreeBlock->size;
		   prevNode->next = newFreeBlock->next;
	   }
   }
   return RTX_OK;
}

int k_mem_count_extfrag(size_t size)
{
#ifdef DEBUG_0
    printf("k_mem_extfrag: size = %d\r\n", size);
#endif /* DEBUG_0 */

    int counter = 0;
    node_t *currNode = head->next;
    while (currNode != tail)
    {
        if (currNode->size < size)
        {
            counter++;
        }
        currNode = currNode->next;
    }
    return counter;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
