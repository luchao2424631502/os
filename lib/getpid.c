#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"

int getpid()
{
    MESSAGE msg;
    msg.type = GET_PID; //systask.c中已经添加了GET_PID消息处理
    
    send_recv(BOTH,TASK_SYS,&msg);
    assert(msg.type == SYSCALL_RET);

    return msg.PID;
}