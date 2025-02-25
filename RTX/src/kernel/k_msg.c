/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"
#include "k_task.h"

// #define DEBUG_0

void create_mailbox(size_t size, mailbox_queue* mailbox_addr) {
    task_t tmpTID = gp_current_task->tid;
    gp_current_task->tid = 0;
    mailbox_addr->head = 0;
    mailbox_addr->tail = 0;
    mailbox_addr->bytes_remaining = size;
    mailbox_addr->max_size = size;
    mailbox_addr->trigger = 1;

    if (size <= 0) { // if the given size is invalid, return NULL
        return;
    }

    // allocate memory for the buffer
    mailbox_addr->buffer = k_mem_alloc(size);
    gp_current_task->tid = tmpTID;
}

void my_memcpy_from_mailbox(void* dest, const void* src, size_t n, size_t receiver_tid) {
    if(dest == NULL){
        return;
    }
    char* pdest = (char*) dest;
    const char* psrc = (const char*) src;
    mailbox_queue mbx = g_tcbs[receiver_tid].mailbox;
    char* start_addr = (char *)mbx.buffer;
    for(int i = 0; i < n; i++){
        if(psrc == start_addr + mbx.max_size){
        	psrc = start_addr;
        }
        *(pdest++) = *(psrc++);
    }
    return;
}

void my_memcpy_to_mailbox(void* dest, const void* src, size_t n, size_t receiver_tid) {
    char* pdest = (char*) dest;
    const char* psrc = (const char*) src;
    mailbox_queue mbx = g_tcbs[receiver_tid].mailbox;
    char* start_addr = (char *)mbx.buffer;
    for(int i = 0; i < n; i++){
        if(pdest == start_addr + mbx.max_size){
            pdest = start_addr;
        }
        *(pdest++) = *(psrc++);
    }
    return;
}

#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */

int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %d\r\n", size);
#endif /* DEBUG_0 */

    if(size < MIN_MBX_SIZE){
        return RTX_ERR;
    }
    if(gp_current_task->mailbox.trigger == 1){
        return RTX_ERR;
    }

    create_mailbox(size, &gp_current_task->mailbox);
    
    if(gp_current_task->mailbox.buffer == NULL){
        task_t tmpTID = gp_current_task->tid;
        gp_current_task->tid = 0;
        k_mem_dealloc(gp_current_task->mailbox.buffer);
        k_mem_dealloc(&gp_current_task->mailbox);
        gp_current_task->tid = tmpTID;
        return RTX_ERR;
    }

    return RTX_OK;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */

    if(receiver_tid == TID_KCD && MAX_TASKS <= TID_KCD){
        receiver_tid = MAX_TASKS - 1;
    }
    int unblocking_msg = 0;
    //check for mailbox
     if (g_tcbs[receiver_tid].state == DORMANT || g_tcbs[receiver_tid].mailbox.buffer == NULL || buf == NULL || g_tcbs[receiver_tid].mailbox.trigger == 0)
     {
         return RTX_ERR;
     }

     if (g_tcbs[receiver_tid].state == BLK_MSG)
     {
         g_tcbs[receiver_tid].state = READY;
         add_task(&g_tcbs[receiver_tid]);
         unblocking_msg = 1;
     }

     char *mailbox_ptr = g_tcbs[receiver_tid].mailbox.buffer;

     RTX_MSG_HDR message_header;
     my_memcpy_from_mailbox(&message_header, buf, sizeof(RTX_MSG_HDR), 0);
     int tail = g_tcbs[receiver_tid].mailbox.tail;

     if(g_tcbs[receiver_tid].mailbox.bytes_remaining < message_header.length || message_header.length < (MIN_MSG_SIZE +sizeof(RTX_MSG_HDR))){
        return RTX_ERR;
     }
    
    //writing tid to mailbox
    my_memcpy_to_mailbox((void *)(mailbox_ptr + tail), &(gp_current_task->tid), sizeof(task_t), receiver_tid);
    tail = (tail + sizeof(task_t)) % g_tcbs[receiver_tid].mailbox.max_size;

    my_memcpy_to_mailbox((void *)(mailbox_ptr + tail), buf, message_header.length, receiver_tid);
    g_tcbs[receiver_tid].mailbox.tail = (tail + message_header.length) % g_tcbs[receiver_tid].mailbox.max_size;

    g_tcbs[receiver_tid].mailbox.bytes_remaining -= (message_header.length + sizeof(task_t));

    if(unblocking_msg){
        return k_tsk_run_new(); 
    }

    return RTX_OK;
}

int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {

#ifdef DEBUG_0
    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    if(gp_current_task->mailbox.trigger == 0){
        return RTX_ERR;
    }

    if (gp_current_task->mailbox.bytes_remaining == gp_current_task->mailbox.max_size)
    {
        gp_current_task->state = BLK_MSG;
        remove_task(gp_current_task->tid);
        k_tsk_run_new(); //check logic here
    }


    int head = gp_current_task->mailbox.head;
    char *mailbox_ptr = gp_current_task->mailbox.buffer;

    //fixed
    RTX_MSG_HDR temp_header;
    task_t tid;
    my_memcpy_from_mailbox(&tid, (void *)(mailbox_ptr + head), sizeof(task_t), gp_current_task->tid);
    head = (gp_current_task->mailbox.head  + sizeof(task_t)) % gp_current_task->mailbox.max_size;

    my_memcpy_from_mailbox(&temp_header, (void *)(mailbox_ptr + head), sizeof(RTX_MSG_HDR),  gp_current_task->tid);

    //not enough memory in buf:
    if (len < temp_header.length || buf == NULL)
    {
        gp_current_task->mailbox.head = (head + temp_header.length) % gp_current_task->mailbox.max_size;
        gp_current_task->mailbox.bytes_remaining +=  (temp_header.length + sizeof(task_t));
        return RTX_ERR;
    }
    my_memcpy_from_mailbox(sender_tid, &tid, sizeof(task_t), 0);

    my_memcpy_from_mailbox(buf, (void *)(mailbox_ptr + head), temp_header.length,  gp_current_task->tid);
    gp_current_task->mailbox.head = (head + temp_header.length) % gp_current_task->mailbox.max_size;
    gp_current_task->mailbox.bytes_remaining +=  (temp_header.length + sizeof(task_t));
    return RTX_OK;
}

int k_recv_msg_nb(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg_nb: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    return 0;
}

int k_mbx_ls(task_t *buf, int count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}
