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
#include "keyboard.h"
#include "proto.h"

void task_sys()
{
    MESSAGE msg;
    while(1)
    {
        send_recv(RECEIVE,ANY,&msg);
        int src = msg.source;

        switch (msg.type)
        {
        case GET_TICKS:
            /* code */
            msg.RETVAL = ticks;
            send_recv(SEND,src,&msg);
            break;
        case GET_PID:
            msg.type = SYSCALL_RET;
            msg.PID  = src;
            send_recv(SEND,src,&msg);
            break;
        default:
            panic("unknown msg type");
            break;
        }
    }
}