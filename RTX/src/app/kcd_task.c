/* The KCD Task Template File */

#include "k_task.h"
#include "k_msg.h"
#include "rtx.h"
#include "k_HAL_CA.h"
#include "printf.h"

static int messageSize = 0;
//static int mod_location = 0;

static char command_msg[64];

void init_msg_buffer(){
    messageSize = 0;
    for(int i = 0; i<64; i++){
        command_msg[i] = NULL;
    }
}

int isalnum(int c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

void send_invalid_command(){
    init_msg_buffer();
    char error_msg[] = "Invalid Command";
    SER_PutStr(1, error_msg);
    return;
}

void add_command(char command){
    if(messageSize >= 64){
        messageSize++;
        return;
    }
    command_msg[messageSize] = command;
    messageSize++;
}

void clear_queue(){
    char identifier = command_msg[1];
    if(!(isalnum((int)identifier)) || messageSize > 64){
        send_invalid_command();
        return;
    }
    if(registered_commands[(int)identifier - 32] == 0 || g_tcbs[registered_commands[(int)identifier - 32]].state == DORMANT){
        init_msg_buffer();
        char error_msg[] = "Command cannot be processed";
        SER_PutStr(1, error_msg);
    }
    RTX_MSG_HDR *msg = NULL;
    msg = mem_alloc(sizeof(RTX_MSG_HDR) + (messageSize - 1)*sizeof(char));
    msg->length = sizeof(RTX_MSG_HDR) + (messageSize - 1)*sizeof(char);
    msg->type = KCD_CMD;

    for (int i = 0; i < messageSize - 1; i++)
    {
    	*((char *)(msg + 1) + i) = command_msg[i + 1];
    }
    send_msg(registered_commands[(int)identifier - 32], msg);
    init_msg_buffer();
    mem_dealloc(msg);
}


void kcd_task(void)
{
    if(mbx_create(KCD_MBX_SIZE) == RTX_ERR){
        tsk_exit();
    }
    
    init_msg_buffer();
    void* msg_buf = mem_alloc(sizeof(char) + sizeof(RTX_MSG_HDR));
    task_t send_tid;
    
    while (1){
        if(recv_msg(&send_tid, msg_buf, sizeof(char) + sizeof(RTX_MSG_HDR)) == RTX_ERR)
        {
        	continue;
        }
        RTX_MSG_HDR msg_header;
        char current_char =  *((char *)((RTX_MSG_HDR*)msg_buf + 1));
        my_memcpy_from_mailbox(&msg_header, msg_buf, sizeof(RTX_MSG_HDR), 0);
        if(msg_header.type == DEFAULT || msg_header.type == KCD_CMD || (msg_header.type == KCD_REG && msg_header.length != sizeof(char) + sizeof(RTX_MSG_HDR))){
            continue;
        }
        if(msg_header.type == KCD_REG){
            if(isalnum((int)current_char)){
                registered_commands[(int)current_char - 32] = send_tid;
            }
            continue;
        }
        if(msg_header.type == KEY_IN && send_tid != TID_UART_IRQ){
            continue;
        }
        if(current_char == '\r' && msg_header.type == KEY_IN) 
        {
            if(command_msg[0] != '%'){
                send_invalid_command();
            }
            else{
                clear_queue();
            }
            continue;
        }
        add_command(current_char);
    }
}
