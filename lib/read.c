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

int read(int fd,void *buf,int count)
{
    MESSAGE msg;
    msg.type    = READ;
    msg.FD      = fd;
    msg.BUF     = buf;
    msg.CNT     = count;

    send_recv(BOTH,TASK_FS,&msg);/* 发送给文件服务进程,调用do_rdwt() */

    return msg.CNT;
}