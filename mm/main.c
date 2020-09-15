#include "type.h"
#include "config.h"
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

void do_fork_test();

static void init_mm();

/* 内核mm服务进程 */
void task_mm()
{
    init_mm();
    
    while(1)
    {
        send_recv(RECEIVE,ANY,&mm_msg);
        int src     = mm_msg.source;
        int reply   = 1;

        int msgtype = mm_msg.type;
        
        switch(msgtype)
        {
        case FORK:
            mm_msg.RETVAL = do_fork();
            break;
        case EXIT:
            // do_exit(mm_msg.STATUS);
            reply = 0;
            break;
        /* case exec: */
        case WAIT:
            // do_wait();
            reply = 0;
            break;
        default:
            dump_msg("MM::unknown msg",&mm_msg);
            assert(0);
            break;
        }

        if (reply)
        {
            mm_msg.type = SYSCALL_RET;
            send_recv(SEND,src,&mm_msg);
        }
    }
}

/* MM task initialization work */
static void init_mm()
{
    struct boot_params bp;
    get_boot_params(&bp);

    memory_size = bp.mem_size;
    
    /* 打印memory size */
    printl("{MM} memsize:%dMB\n",memory_size / (1024 * 1024));
}

/* 
    替进程分配内存块
    pid : 替哪一个进程
    memsize : 需要多大内存

    return : 分配内存的基础地址
 */
int alloc_mem(int pid,int memsize)
{
    assert(pid >= (NR_TASKS + NR_NATIVE_PROCS));
    if (memsize > PROC_IMAGE_SIZE_DEFAULT)/* 不能超过1MB大小 */
    {
        panic("unsupported memory request: %d. "
                "(should be less than %d)",
                memsize,
                PROC_IMAGE_SIZE_DEFAULT);
    }

    /* 在某指定内存基础上按照进程id分配内存 */
    int base = PROCS_BASE + (pid - (NR_TASKS + NR_NATIVE_PROCS)) * PROC_IMAGE_SIZE_DEFAULT;

    /* 不能超过实际内存大小 */
    if (base + memsize >= memory_size)
        panic("memory allocation failed. pid:%d",pid);

    return base;
}

int free_mem(int pid)
{
    return 0;
}
