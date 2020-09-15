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

// static void cleanup(struct proc* proc);

/* mm api的内核实现 */
int do_fork()
{
    /* 找到proc_table中的第一个空位 */
    struct proc *p = proc_table;
    int i;
    for (i=0; i<NR_TASKS + NR_PROCS; i++,p++)
    {
        if (p->p_flags == FREE_SLOT)
            break;
    }

    int child_pid = i;
    assert(p == &proc_table[child_pid]);
    assert(child_pid >= NR_TASKS + NR_NATIVE_PROCS);
    if (i == NR_TASKS + NR_PROCS)/* proc_table[]没有空槽给子进程用 */
        return -1;
    assert(i < NR_TASKS + NR_PROCS);

    /* 子进程的proc_table[]复制父进程表 */
    int pid = mm_msg.source;//父进程id
    u16 child_ldt_sel = p->ldt_sel;
    *p = proc_table[pid];   //子进程proc表内容全部=父进程proc表内容
    p->ldt_sel = child_ldt_sel;     //子进程的ldt_sel对应的新的在GDT表中的LDT选择子
    p->p_parent = pid;      //父进程是pid
    sprintf(p->name,"%s_%d",proc_table[pid].name,child_pid);

    /* 父进程代码段信息 */
    struct descriptor *ppd;
    ppd = &proc_table[pid].ldts[INDEX_LDT_C];/* 拿到父进程代码段描述符LDT */
    int caller_T_base = reassembly(ppd->base_high,24,ppd->base_mid,16,ppd->base_low);
    int caller_T_limit = reassembly(0,0,(ppd->limit_high_attr2 & 0xF),16,ppd->limit_low);
    int caller_T_size = ((caller_T_limit + 1) * ((ppd->limit_high_attr2 & (DA_LIMIT_4K >> 8)) ? 4096 : 1));

    /* 父进程数据段(栈)段信息 */
    ppd = &proc_table[pid].ldts[INDEX_LDT_RW];
    int caller_D_S_base = reassembly(ppd->base_high,24,ppd->base_mid,16,ppd->base_low);
    int caller_D_S_limit = reassembly((ppd->limit_high_attr2 & 0xF),16,0,0,ppd->limit_low);
    int caller_D_S_size = ((caller_D_S_limit + 1) * ((ppd->limit_high_attr2 & (DA_LIMIT_4K >> 8)) ? 4096 : 1));

    assert((caller_T_base == caller_D_S_base) &&
            (caller_T_limit == caller_D_S_limit) &&
            (caller_T_size == caller_D_S_size));
    /* text,data段(LDT描述符指向)共享一块内存空间,申请一次内存空间 */
    int child_base = alloc_mem(child_pid,caller_T_size);/* 大小是每个进程的text段大小 */
    /* child_limit = caller_T_limit */
    printl("{MM} 0x%x <- 0x%x (0x%x bytes)\n",
            child_base,caller_T_base,caller_T_size);
    /* 从父进程复制内存段到子进程 */
    phys_copy((void*)child_base,(void*)caller_T_base,caller_T_size);

    /* 子进程LDT */
    init_desc(&p->ldts[INDEX_LDT_C],
                child_base,
                (PROC_IMAGE_SIZE_DEFAULT - 1) >> LIMIT_4K_SHIFT,
                DA_LIMIT_4K | DA_32 | DA_C | PRIVILEGE_USER << 5);
    init_desc(&p->ldts[INDEX_LDT_RW],
                child_base,
                (PROC_IMAGE_SIZE_DEFAULT - 1) >> LIMIT_4K_SHIFT,
                DA_LIMIT_4K | DA_32 | DA_DRW | PRIVILEGE_USER << 5);
    /* fork后,fd等资源也需要复制 */
    MESSAGE msg2fs;
    msg2fs.type = FORK;
    msg2fs.PID = child_pid;
    send_recv(BOTH,TASK_FS,&msg2fs);

    /* 子进程ID返回给父进程ID */
    mm_msg.PID = child_pid;

    MESSAGE m;
    m.type = SYSCALL_RET;
    m.RETVAL = 0;
    m.PID = 0;
    send_recv(SEND,child_pid,&m);
    
    return 0;
}
