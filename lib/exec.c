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

/* 应用层给用户的3个exec
    path:可执行文件路径
    return: 0成功,-1失败
 */
int exec(const char *path)
{
    /* 参数表应该是继承的 */
    MESSAGE msg;
    msg.type = EXEC;
    msg.PATHNAME    = (void*)path;
    msg.NAME_LEN    = strlen(path);
    msg.BUF         = 0;
    msg.BUF_LEN     = 0;

    send_recv(BOTH,TASK_MM,&msg);
    assert(msg.type == SYSCALL_RET);

    return msg.RETVAL;
}

/* list */
int execl(const char *path,const char *arg,...)
{
    va_list parg = (va_list)(&arg);
    char **p = (char **)parg;
    return execv(path,p);
}
/* 
 */
int execv(const char *path,char *argv[])
{
    char **p = argv;
    char arg_stack[PROC_ORIGIN_STACK];
    int stack_len = 0;

    while(*p++)/* 遍历每一个char*指针(字符串) */
    {
        assert(stack_len + 2 * sizeof(char*) < PROC_ORIGIN_STACK);
        stack_len += sizeof(char*);
    }

    /* 空指针结尾 */
    *((int*)(&arg_stack[stack_len])) = 0;
    stack_len += sizeof(char*);

    char **q = (char**)arg_stack;
    /* 遍历一次argv,将char*指向的字符串赋值给arg_stack[] */
    for (p=argv; *p!=0; p++)
    {
        *q++ = &arg_stack[stack_len];/* char* = &char */
        
        assert(stack_len + strlen(*p) + 1 < PROC_ORIGIN_STACK);
        strcpy(&arg_stack[stack_len],*p);/* 字符串赋值 */
        stack_len += strlen(*p);        /* 加上字符串长度 */
        arg_stack[stack_len] = 0;       /* 添加结尾0 */
        stack_len++;
    }

    MESSAGE msg;
    msg.type = EXEC;
    msg.PATHNAME    = (void*)path;
    msg.NAME_LEN    = strlen(path);
    msg.BUF         = (void*)arg_stack;
    msg.BUF_LEN     = stack_len;

    send_recv(BOTH,TASK_MM,&msg);
    assert(msg.type == SYSCALL_RET);

    return msg.RETVAL;
}