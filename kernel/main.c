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

int kernel_main()
{
    disp_str("-----\"kernel_main\" begins-----\n");
    
    int i,j,eflags,prio;
    u8 rpl,priv;

    struct task *t;
    struct proc *p = proc_table;

    //开始分配栈的地址
    char *stk = task_stack + STACK_SIZE_TOTAL;

    /* 由task_table表信息根据类型填写到proc_table中 */
    for (i=0; i<NR_TASKS + NR_PROCS; i++,p++,t++)
    {
        /* 过了OS服务常驻进程+4个开始的进程 就是 普通进程 */
        if (i >= NR_TASKS + NR_NATIVE_PROCS)
        {
            p->p_flags = FREE_SLOT; /* 创建proc_table中的空选项 */
            continue;
        }

        /* OS的5个服务进程 */
        if (i < NR_TASKS)
        {
            t = task_table + i;
            priv    = PRIVILEGE_TASK;
            rpl     = RPL_TASK;
            eflags  = 0x1202; /* IF=1(可响应外部可屏蔽中断),IOPL=1 */
            prio    = 15; //进程调度优先级
        }
        else /* 4个用户进程 */
        {
            t       = user_proc_table + (i - NR_TASKS);
            priv    = PRIVILEGE_USER;//用户级
            rpl     = RPL_USER;
            eflags  = 0x202;/* IF=1 */
            prio    = 5;
        }
        strcpy(p->name,t->name);
        p->p_parent = NO_TASK;
        
        /* 除了Init进程外其他进程的text段和data段都使用平坦模式的段*/
        if (strcmp(t->name,"INIT") != 0)
        {
            p->ldts[INDEX_LDT_C]    = gdt[SELECTOR_KERNEL_CS >> 3];
            p->ldts[INDEX_LDT_RW]   = gdt[SELECTOR_KERNEL_DS >> 3];

            /* DPL */
            p->ldts[INDEX_LDT_C].attr1  = DA_C | priv << 5;
            p->ldts[INDEX_LDT_RW].attr1 = DA_DRW | priv << 5;
        }
        else/* Init进程 */
        {
            unsigned int k_base;
            unsigned int k_limit;
            int ret = get_kernel_map(&k_base,&k_limit);
            assert(ret == 0);
            /* 更改init进程LDT表中的描述符 */
            init_desc(&p->ldts[INDEX_LDT_C],
                        0,
                        (k_base + k_limit) >> LIMIT_4K_SHIFT,
                        DA_32 | DA_LIMIT_4K | DA_C | priv << 5);

            init_desc(&p->ldts[INDEX_LDT_RW],
                        0,
                        (k_base + k_limit) >> LIMIT_4K_SHIFT,
                        DA_32 | DA_LIMIT_4K | DA_DRW | priv << 5);
        }

        /* proc_table[]中的其他信息 */
        p->regs.cs  = INDEX_LDT_C << 3 | SA_TIL | rpl;
        p->regs.ds  = 
            p->regs.es  =
            p->regs.fs  = 
            p->regs.ss  = INDEX_LDT_RW << 3 | SA_TIL | rpl;
        p->regs.gs  = (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;/* 普通进程gs寄存器指向内核的gs段 */
        p->regs.eip = (u32)t->initial_eip;/* 将task[]中的eip复制到proctable[]中 */
        p->regs.esp = (u32)stk;
        p->regs.eflags = eflags;

        p->ticks = p->priority = prio; /* 调度优先级 */

        /* 在内核中的状态  */
        p->p_flags  = 0;
        p->p_msg    = 0;
        p->p_recvfrom = NO_TASK;
        p->p_sendto = NO_TASK;
        p->has_int_msg  = 0;
        p->q_sending = 0;
        p->next_sending = 0;

        /* 初始化进程指向的fd数组 */
        for (j=0; j<NR_FILES; j++)
            p->filp[j] = 0;
        
        stk -= t->stacksize;
    }

    k_reenter = 0;     /*解决中断重入的全局变量*/
    ticks = 0;      /*kernel API 0 全局变量初始化*/

    p_proc_ready    = proc_table;

    /* 设置时钟中断 */
    init_clock();
    /* 设置键盘中断 */
    init_keyboard();

    restart(); /*进入第一个进程A(r0->r1)*/

    while(1) {}
}

int get_ticks()
{
    MESSAGE msg;
	reset_msg(&msg);
	msg.type = GET_TICKS;
	send_recv(BOTH, TASK_SYS, &msg);
	return msg.RETVAL;
}

/* 为了实现fork,先添加init进程 */
void Init()
{
    int fd_stdin = open("/dev_tty0",O_RDWR);
    assert(fd_stdin == 0);
    int fd_stdout = open("/dev_tty0",O_RDWR);
    assert(fd_stdout == 1);

    printf("Init() is running ... \n");

    int pid = fork();
    if (pid != 0)
    {
        printf("parent is running, child pid: %d\n",pid);
        spin("parent");
    }
    else
    {
        printf("child is running,pid: %d\n",getpid());
        spin("child");
    }
    
}

/*进程A*/
void TestA()
{  
    for (;;)
        ;
}

/*进程B*/
void TestB()
{   
    for (;;);
}

void TestC()
{
    for (;;);
}

void panic(const char *fmt,...)
{
    char buf[256];

    va_list arg = (va_list)((char *)&fmt + 4);

    vsprintf(buf,fmt,arg);

    printl("%c !!panic!! %s", MAG_CH_PANIC, buf);

    __asm__ __volatile__("ud2");
}