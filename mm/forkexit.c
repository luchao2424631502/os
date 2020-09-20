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

static void cleanup(struct proc* proc);

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
    int caller_T_base = reassembly(ppd->base_high,24,
                        ppd->base_mid,16,
                        ppd->base_low);
    int caller_T_limit = reassembly(0,0,
                        (ppd->limit_high_attr2 & 0xF),16,
                        ppd->limit_low);
    int caller_T_size = ((caller_T_limit + 1) * 
                        ((ppd->limit_high_attr2 & (DA_LIMIT_4K >> 8)) ? 
                        4096 : 1));

    /* 父进程数据段(栈)段信息 */
    ppd = &proc_table[pid].ldts[INDEX_LDT_RW];
    int caller_D_S_base = reassembly(ppd->base_high,24,
                        ppd->base_mid,16,
                        ppd->base_low);
    int caller_D_S_limit = reassembly((ppd->limit_high_attr2 & 0xF),16,
                        0,0,
                        ppd->limit_low);
    int caller_D_S_size = ((caller_T_limit + 1) * 
                        ((ppd->limit_high_attr2 & (DA_LIMIT_4K >> 8)) ? 
                        4096 : 1));

    assert((caller_T_base == caller_D_S_base) &&
            (caller_T_limit == caller_D_S_limit) &&
            (caller_T_size == caller_D_S_size));
    /* text,data段(LDT描述符指向)共享一块内存空间,申请一次内存空间 */
    int child_base = alloc_mem(child_pid,caller_T_size);/* 大小是每个进程的text段大小 */
    /* child_limit = caller_T_limit */
    // printl("{MM} 0x%x <- 0x%x (0x%x bytes)\n",
    //         child_base,caller_T_base,caller_T_size);
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

/*
MM对于进程A做的事情:
1.通知fs，以便清理与fd相关的事物
2.告诉task_sys
3.释放A的记忆
4.设置A.exit_status，它用于父级
5.取决于父母的身份，如果父母(p)是：
（1）等待： 
        -清除p的WAITING bit,
        -向p发送消息取消阻塞
        -释放A的proc_table[]槽
 (2) not 等待:
        - 设置A's HANGING bit

6. 重复proc_table []，如果proc b被发现是A的孩子，则：
（1）将INIT设为B的新父代，并且
（2）如果INIT正在等待而b正在挂起，则：
     -clean INIT的等待位，
     -发送INIT消息以解除阻止
     -释放b的proc_table []插槽{B的exit（）调用已完成}
其他
     1.如果INIT在等待但b没有挂起，则
        -b将调用exit（），并且将在do_exit（）处完成操作
     2.如果B挂起但init没有等待，则
        -INIT将调用wait（），事情将在do_wait（）处完成
---------
区别waiting(进程未执行完毕,但是处于阻塞)状态和hanging(一直执行完毕,等待回收)状态 
1.hanging:除了proc_table[]条目以外的资源都已经被清除了
2.waiting:进程至少有1个child,然后此进程正在等待child调用exit() 
zombie进程解释:~
 */
void do_exit(int status)
{
    int i;
    int pid = mm_msg.source;/* PID of caller */
    int parent_pid = proc_table[pid].p_parent;
    struct proc* p = &proc_table[pid];

    /* 告诉FS task,调用fs_exit() */
    MESSAGE msg2fs;
    msg2fs.type = EXIT;
    msg2fs.PID  = pid;
    send_recv(BOTH,TASK_FS,&msg2fs);

    /* 
    还应该向TASK_SYS发送消息以进行一些清理工作。
    如果该proc被另一个proc杀死，则TASK_SYS应该检查该proc是否恰好正在发送消息，
    如果是，则将该proc从发送队列中删除。
     */
    free_mem(pid);/* 释放pid的内存 */
    p->exit_status = status;/* 参数代表的是自己的退出状态 */
    /* 判断父亲进程现在处于什么状态 */
    if (proc_table[parent_pid].p_flags & WAITING)
    {
        /* printl("{MM} ((--do_exit():: %s (%d) is WAITING, %s (%d) will be cleaned up.--))\n",
                    proc_table[parent_pid].name,parent_pid,p->name,pid);

        printl("{MM} ((-do_exit():1: proc_table[parent_pid].p_flags: 0x%x--))\n",
                    proc_table[parent_pid].p_flags); */
        
        proc_table[parent_pid].p_flags &= ~WAITING;/* 取消父进程的阻塞状态 */
        cleanup(&proc_table[pid]);/* 清除child进程的所有信息 */        
    }
    else/* 子进程执行exit(),但是父进程没有在waiting状态 */
    {
        /* printl("{MM} ((--do_exit():: %s (%d) is not WAITING,%s (%d) will be HANGING--))\n",
                    proc_table[parent_pid].name,parent_pid,p->name,pid); */
        /* 子进程将被hanging */
        proc_table[pid].p_flags |= HANGING;
    }

    /* 查看自己有没有别的child 进程,让自己的child进程的pid=INIT */
    for (i=0; i<NR_TASKS + NR_PROCS; i++)
    {
        if (proc_table[i].p_parent == pid)
        {
            proc_table[i].p_parent = INIT;
            /* printl("{MM} %s (%d) exit(),so %s (%d) is INIT's child now\n",
                    p->name,pid,proc_table[i].name,i);
            printl("{MM} ((--do_exit():2: proc_table[INIT].p_flags: 0x%x--))\n",
                    proc_table[INIT].p_flags); */

            /* 如果INIT正在WAITING并且i进程处于HANGING状态 */
            if ((proc_table[INIT].p_flags & WAITING) && 
                (proc_table[i].p_flags & HANGING))
            {
                proc_table[INIT].p_flags &= ~WAITING;
                cleanup(&proc_table[i]);
                // assert(0);
            }
            // else{}
        }
    }
}

/* 
完成最后的工作以彻底清除proc；
    -向proc的父级发送消息以解除阻塞，
    -并释放proc的proc_table []条目
*/
static void cleanup(struct proc* proc)
{
    /* 给内核发送消息(通知父亲进程child资源已经释放) */
    MESSAGE msg2parent;
    msg2parent.type = SYSCALL_RET;
    msg2parent.PID  = proc2pid(proc);
    msg2parent.STATUS = proc->exit_status;
    send_recv(SEND,proc->p_parent,&msg2parent);

    proc->p_flags = FREE_SLOT;/* 释放槽位 */
    // printl("{MM} ((--cleanup():: %s (%d) has been cleaned up.--))\n",proc->name,proc2pid(proc));
}

/* 
进程P调用wait(),则MM模块需要做到如下:
1.遍历proc_table[]:
    如果进程A被发现是P的child,并且处于HANGING状态
    - 给进程P回复消息(cleanup->杰出p的阻塞)
        {进程P的wait()执行完毕}
    - 释放A的proc_table[]条目资源
        {进程A的exit()执行完毕}
    - return (MM继续while循环,处理下一条消息)
2.如果P进程没有child位于HANGING状态
    - 设置P进程的WAITING bit(则P一直阻塞)
    {do_exit()完成}
3.如果进程P没有child
    - 回复进程P一个error
    {进程P的wait()调用完成}
4.return
*/
void do_wait()
{
    // printl("{MM} ((--do_wait()--))");
    int pid = mm_msg.source;/* 调用这的Pid */

    int i;
    int children = 0;
    struct proc* p_proc = proc_table;
    for (i=0; i<NR_TASKS + NR_PROCS; i++,p_proc++)
    {
        /* 是P的child进程 */
        if (p_proc->p_parent == pid)
        {
            children++;
            /* child is HANGING 所以可以被回收资源 */
            if (p_proc->p_flags & HANGING)
            {
                /* 释放第一个遇到的HANGING状态的child */
                /* printl("{MM} ((--do_wait():: %s (%d) is HANGING, so let's clean it up.--))",
                        p_proc->name,i); */
                cleanup(p_proc);
                return ;
            }
        }
    }

    /* 有children个child进程,但是都不在HANGING状态,则P继续阻塞 */
    if (children)
    {
        proc_table[pid].p_flags |= WAITING;//father 继续 WAITING
        // printl("{MM} ((--do_wait():: %s (%d) is WAITING for child to exit().--))\n",proc_table[pid].name,pid);
    }
    /* 没有child进程,father's wait返回error */
    else
    {
       /*  printl("{MM} ((--do_wait()::%s (%d) has no child at all.--))\n",
                proc_table[pid].name,pid); */
        /* 回复father一个error */
        MESSAGE msg;
        msg.type = SYSCALL_RET;
        msg.PID  = NO_TASK;
        send_recv(SEND,pid,&msg);
    }
    
}