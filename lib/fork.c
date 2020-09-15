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

/* 
fork.c (用户态的实现:是向内核发送消息,
由内核判断消息内容来调用内核版本dofork
*/
int fork()
{
    MESSAGE msg;
    msg.type = FORK; /* 消息类型:FORK */

    send_recv(BOTH,TASK_MM,&msg);
    assert(msg.type == SYSCALL_RET);
    assert(msg.RETVAL == 0);

    return msg.PID;
}