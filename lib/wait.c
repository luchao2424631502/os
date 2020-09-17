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

int wait(int *status)
{
    MESSAGE msg;
    msg.type = WAIT;
    
    send_recv(BOTH,TASK_MM,&msg);
    
    *status = msg.STATUS;/* 将从内核拿到的status赋值给status */

    return (msg.PID == NO_TASK ? -1 : msg.PID);
}