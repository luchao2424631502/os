#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "tty.h"
#include "console.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "global.h"
#include "proto.h"

static void block(struct proc*);
static void unblock(struct proc*);
static int msg_send(struct proc*,int,MESSAGE*);
static int msg_receive(struct proc*,int,MESSAGE*);
static int deadlock(int ,int );

/* 进程调度函数,由clock_handler()调用 */
void schedule()
{
    struct proc* p;
    int greatest_ticks = 0;

    while(!greatest_ticks)
    {
        /* 遍历进程表,修改当前要被执行的进程是剩余执行时间最多(优先级最高的进程) */
        for (p = &FIRST_PROC; p <= &LAST_PROC; p++)
        {
            if (p->p_flags == 0)/* flags=0表示可以run */
            {
                if (p->ticks > greatest_ticks)
                {                
                    greatest_ticks = p->ticks;
                    p_proc_ready = p;
                }
            }
        }

        /* 所有进程剩余执行时间=0,恢复所有进程优先级 */
        if (!greatest_ticks)
        {
            for (p = &FIRST_PROC; p <= &LAST_PROC; p++)
            {
                if (p->p_flags == 0)
                    p->ticks = p->priority;
            }
        }
    }
}

/*---------------------系统调用(内核实现)-------------------------------*/

/* 把send的消息给msg_send处理,receive的消息给msg_receive处理 
function: send or receive
src_dest: to or from
m:  msg
p:  调用proc

return： 0 is  success
*/
int sys_sendrec(int function,int src_dest,MESSAGE *m,struct proc* p)
{
    assert(k_reenter == 0);/*  !=0 在中断的r0层次(保证当前不在r0层次) */
    assert((src_dest >= 0 && src_dest < NR_TASKS + NR_PROCS) || 
            src_dest == ANY ||
            src_dest == INTERRUPT);
        
    int ret = 0;
    int caller = proc2pid(p);
    MESSAGE *mla = (MESSAGE *)va2la(caller,m);
    mla->source = caller;

    assert(mla->source != src_dest);

    if (function == SEND)
    {
        ret = msg_send(p,src_dest,m);
        if (ret != 0)
            return ret;
    }
    else if (function == RECEIVE)
    {
        ret = msg_receive(p,src_dest,m);
        if (ret != 0)
            return ret;
    }
    else 
    {
        panic("{sys_sendrec} invalid function: "
		      "%d (SEND:%d, RECEIVE:%d).", function, SEND, RECEIVE);
    }

    return 0;
}


/*
它是对sendrec的封装，应避免直接调用sendrec
function: send receive or both
src_dest:   调用者进程的NR
msg :        
*/
int send_recv(int function,int src_dest,MESSAGE *msg)
{
    int ret = 0;
    if (function == RECEIVE)
        memset(msg,0,sizeof(MESSAGE));
    switch (function)
    {
    case BOTH:
        ret = sendrec(SEND,src_dest,msg);
        if (ret == 0)
            ret = sendrec(RECEIVE,src_dest,msg);
        break;
    case SEND: 
    case RECEIVE:
        ret = sendrec(function,src_dest,msg);
        break;
    default:
        assert((function == BOTH) || (function == SEND) || (function == RECEIVE));
        break;
    }

    return ret;
}

/* 
return 进程LDT index描述符 对应的 段基础地址
*/
int ldt_seg_linear(struct proc* p,int idx)
{
    struct descriptor *d = &p->ldts[idx];

    return d->base_high << 24 | d->base_mid << 16 | d->base_low;
}

/* va2la
虚拟地址->线性地址
pid     被计算的进程的pID
va      virtual address

return: 线性地址
 */
void *va2la(int pid,void *va)
{
    struct proc* p  = &proc_table[pid];

    u32 seg_base    = ldt_seg_linear(p,INDEX_LDT_RW);
    u32 la          = seg_base + (u32)va;

    if (pid < NR_TASKS + NR_NATIVE_PROCS)
    {
        assert(la == (u32)va);
    }

    return (void *)la;
}

/* 清除消息通过设置每一bit
*/
void reset_msg(MESSAGE *p)
{
    memset(p,0,sizeof(MESSAGE));
}

/*
在设置了“ p_flags”之后调用此例程，
它调用“ schedule（）”以选择另一个proc作为“ proc_ready”  
*/
static void block(struct proc* p)
{
    assert(p->p_flags);
    schedule();
}

/*
这是一个虚拟例程，当
它已被清除的名为“ p_flags”时，实际上不执行任何操作 
 */
static void unblock(struct proc* p)
{
    assert(p->p_flags == 0);
}

/* 
判断是否成环发生死锁

*/
static int deadlock(int src,int dest)
{
    struct proc* p = proc_table + dest;
    while(1)
    {
        if (p->p_flags & SENDING)
        {
            /* 末尾想要发给开头(环) */
            if (p->p_sendto == src)
            {
                p = proc_table + dest;
                printl("=_=%s", p->name);
                do{
                    assert(p->p_msg);
                    p = proc_table + p->p_sendto;
                    printl("->%s",p->name);
                }while(p != proc_table + src);
                printl("=_=");
                
                return 1;
            }
            p = proc_table + p->p_sendto;
        }
        else 
        {
            break;
        }
    }
    return 0;
}

static int msg_send(struct proc* current,int dest,MESSAGE *m)
{
    struct proc* sender = current;
    struct proc* p_dest = proc_table + dest;

    assert(proc2pid(sender) != dest);

    if (deadlock(proc2pid(sender),dest))
    {
        panic(">>DEADLOCK<< %s->%s", sender->name, p_dest->name);
    }

    /* 接受进程正好等待这从我发过来的消息 */
    if ((p_dest->p_flags & RECEIVING) && 
        (p_dest->p_recvfrom == proc2pid(sender) ||
        p_dest->p_recvfrom == ANY))
    {
        assert(p_dest->p_msg);
        assert(m);

        /* 传递消息给目标进程 */
        phys_copy(va2la(dest,p_dest->p_msg),
                    va2la(proc2pid(sender),m),
                    sizeof(MESSAGE));
        p_dest->p_msg       = 0;
        p_dest->p_flags     &= ~RECEIVING;
        p_dest->p_recvfrom  = NO_TASK;
        unblock(p_dest); 

        assert(p_dest->p_flags == 0);
		assert(p_dest->p_msg == 0);
		assert(p_dest->p_recvfrom == NO_TASK);
		assert(p_dest->p_sendto == NO_TASK);
		assert(sender->p_flags == 0);
		assert(sender->p_msg == 0);
		assert(sender->p_recvfrom == NO_TASK);
		assert(sender->p_sendto == NO_TASK);
    }
    /* 目标进程并不是正在等待消息 */
    else 
    {
        /* 修改当前进程信息 */
        sender->p_flags |= SENDING;
        assert(sender->p_flags == SENDING);
        sender->p_sendto = dest;
        sender->p_msg = m;

        /* 添加到消息发送队列 */
        struct proc* p;
        if (p_dest->q_sending)
        {
            p = p_dest->q_sending;
            while(p->next_sending)
                p = p->next_sending;
            p->next_sending = sender;
        }
        else 
        {
            p_dest->q_sending = sender;
        }
        sender->next_sending = 0;

        block(sender);/* 阻塞发送进程 */

        assert(sender->p_flags == SENDING);
		assert(sender->p_msg != 0);
		assert(sender->p_recvfrom == NO_TASK);
		assert(sender->p_sendto == dest);
    }
    return 0;
}

/*  
*/
static int msg_receive(struct proc* current,int src,MESSAGE *m)
{
    struct proc* p_who_wanna_recv   = current;
    struct proc* p_from             = 0;/* 等下得到 */
    struct proc* prev               = 0;
    int copyok                      = 0;

    assert(proc2pid(p_who_wanna_recv) != src);/* 不能自己收自己 */

    /* 当前有中断请求消息 */
    if ((p_who_wanna_recv->has_int_msg) &&
        ((src == ANY) || (src == INTERRUPT)))
    {
        MESSAGE msg;
        reset_msg(&msg);
        msg.source  = INTERRUPT;
        msg.type    = HARD_INT;

        assert(m);

        /* 复制消息 */
        phys_copy(va2la(proc2pid(p_who_wanna_recv),m),
                    &msg,
                    sizeof(MESSAGE));
        p_who_wanna_recv->has_int_msg = 0;/* 清除中断消息标志位 */

        assert(p_who_wanna_recv->p_flags == 0);
		assert(p_who_wanna_recv->p_msg == 0);
		assert(p_who_wanna_recv->p_sendto == NO_TASK);
		assert(p_who_wanna_recv->has_int_msg == 0);

		return 0;
    }

    /* 检查了中断消息 来到这里 */
    if (src == ANY)
    {
        /* p_who_wanna_recv已准备好接收来自任何proc的消息，
        我们将检查发送队列并选择其中的第一个proc */
        if (p_who_wanna_recv->q_sending)
        {
            p_from = p_who_wanna_recv->q_sending;
            copyok = 1;

            assert(p_who_wanna_recv->p_flags == 0);
			assert(p_who_wanna_recv->p_msg == 0);
			assert(p_who_wanna_recv->p_recvfrom == NO_TASK);
			assert(p_who_wanna_recv->p_sendto == NO_TASK);
			assert(p_who_wanna_recv->q_sending != 0);
			assert(p_from->p_flags == SENDING);
			assert(p_from->p_msg != 0);
			assert(p_from->p_recvfrom == NO_TASK);
			assert(p_from->p_sendto == proc2pid(p_who_wanna_recv));
        }
    }//接收消息从一个确定的进程src
    else
    {
        p_from = &proc_table[src];

        /* 消息来源进程正好发送给我 */
        if ((p_from->p_flags & SENDING) &&
            (p_from->p_sendto == proc2pid(p_who_wanna_recv)))
        {
            copyok = 1;
            struct proc *p = p_who_wanna_recv->q_sending;
            
            assert(p);

            while(p)
            {
                assert(p_from->p_flags & SENDING);

                if (proc2pid(p) == src)//p是一个?
                {
                    p_from = p;
                    break;
                }
                prev = p;
                p = p->next_sending;
            }
            assert(p_who_wanna_recv->p_flags == 0);
			assert(p_who_wanna_recv->p_msg == 0);
			assert(p_who_wanna_recv->p_recvfrom == NO_TASK);
			assert(p_who_wanna_recv->p_sendto == NO_TASK);
			assert(p_who_wanna_recv->q_sending != 0);
			assert(p_from->p_flags == SENDING);
			assert(p_from->p_msg != 0);
			assert(p_from->p_recvfrom == NO_TASK);
			assert(p_from->p_sendto == proc2pid(p_who_wanna_recv));
        }
    }
    
    /*确定要从哪个proc复制消息，
    请注意该proc必须一直在队列中等待这一刻，
    因此我们应该将其从队列中删除 
     */
    if (copyok)
    {
        if (p_from == p_who_wanna_recv->q_sending)
        {
            assert(prev == 0);/* 第一个 */
            p_who_wanna_recv->q_sending = p_from->next_sending;
            p_from->next_sending = 0;
        }
        else 
        {
            /* 从队列中拿掉p_from */
            assert(prev);
            prev->next_sending = p_from->next_sending;
            p_from->next_sending = 0;
        }

        assert(m);
        assert(p_from->p_msg);

        /* 复制消息 */
        phys_copy(va2la(proc2pid(p_who_wanna_recv),m),
                    va2la(proc2pid(p_from),p_from->p_msg),
                    sizeof(MESSAGE));
        
        p_from->p_msg       = 0;
        p_from->p_sendto    = NO_TASK;
        p_from->p_flags     &= ~SENDING;

        unblock(p_from);
    }
    else 
    {
        /* 没有人发送任何消息 */
        p_who_wanna_recv->p_flags   |= RECEIVING;
        
        p_who_wanna_recv->p_msg     = m;

        // p_who_wanna_recv->p_recvfrom = src;
        if (src == ANY)
        {
            p_who_wanna_recv->p_recvfrom = ANY;
        }else
        {
            p_who_wanna_recv->p_recvfrom = proc2pid(p_from);
        }

        block(p_who_wanna_recv);

        assert(p_who_wanna_recv->p_flags == RECEIVING);
		assert(p_who_wanna_recv->p_msg != 0);
		assert(p_who_wanna_recv->p_recvfrom != NO_TASK);
		assert(p_who_wanna_recv->p_sendto == NO_TASK);
		assert(p_who_wanna_recv->has_int_msg == 0);
    }
    
    return 0;
}

/* 通知进程发生了中断 */
void inform_int(int task_nr)
{
    struct proc *p = proc_table + task_nr;

    if ((p->p_flags & RECEIVING) && 
        ((p->p_recvfrom == INTERRUPT) || (p->p_recvfrom == ANY)))
    {
        p->p_msg->source = INTERRUPT;
        p->p_msg->type   = HARD_INT;
        p->p_msg = 0;
        p->has_int_msg = 0;
        p->p_flags &= ~RECEIVING;
        p->p_recvfrom = NO_TASK;
        assert(p->p_flags == 0);
        unblock(p);
        
        assert(p->p_flags == 0);
		assert(p->p_msg == 0);
		assert(p->p_recvfrom == NO_TASK);
		assert(p->p_sendto == NO_TASK);
    }
    else 
    {
        p->has_int_msg = 1;
    }
}



void dump_proc(struct proc* p)
{
    char info[STR_DEFAULT_LEN];
    int i;
    int text_color = MAKE_COLOR(GREEN,RED);

    int dump_len = sizeof(struct proc);

    out_byte(CRTC_ADDR_REG,START_ADDR_H);
    out_byte(CRTC_DATA_REG,0);
    out_byte(CRTC_ADDR_REG,START_ADDR_L);
    out_byte(CRTC_DATA_REG,0);

    sprintf(info, "byte dump of proc_table[%d]:\n", p - proc_table); disp_color_str(info, text_color);
	for (i = 0; i < dump_len; i++) {
		sprintf(info, "%x.", ((unsigned char *)p)[i]);
		disp_color_str(info, text_color);
	}

	/* printl("^^"); */

	disp_color_str("\n\n", text_color);
	sprintf(info, "ANY: 0x%x.\n", ANY); disp_color_str(info, text_color);
	sprintf(info, "NO_TASK: 0x%x.\n", NO_TASK); disp_color_str(info, text_color);
	disp_color_str("\n", text_color);

	sprintf(info, "ldt_sel: 0x%x.  ", p->ldt_sel); disp_color_str(info, text_color);
	sprintf(info, "ticks: 0x%x.  ", p->ticks); disp_color_str(info, text_color);
	sprintf(info, "priority: 0x%x.  ", p->priority); disp_color_str(info, text_color);
	// sprintf(info, "pid: 0x%x.  ", p->pid); disp_color_str(info, text_color);
	sprintf(info, "name: %s.  ", p->name); disp_color_str(info, text_color);
	disp_color_str("\n", text_color);
	sprintf(info, "p_flags: 0x%x.  ", p->p_flags); disp_color_str(info, text_color);
	sprintf(info, "p_recvfrom: 0x%x.  ", p->p_recvfrom); disp_color_str(info, text_color);
	sprintf(info, "p_sendto: 0x%x.  ", p->p_sendto); disp_color_str(info, text_color);
	// sprintf(info, "nr_tty: 0x%x.  ", p->nr_tty); disp_color_str(info, text_color);
	disp_color_str("\n", text_color);
	sprintf(info, "has_int_msg: 0x%x.  ", p->has_int_msg); disp_color_str(info, text_color);
}

/*  */
void dump_msg(const char *title,MESSAGE *m)
{
    int packed = 0;
	printl("{%s}<0x%x>{%ssrc:%s(%d),%stype:%d,%s(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)%s}%s",  //, (0x%x, 0x%x, 0x%x)}",
	       title,
	       (int)m,
	       packed ? "" : "\n        ",
	       proc_table[m->source].name,
	       m->source,
	       packed ? " " : "\n        ",
	       m->type,
	       packed ? " " : "\n        ",
	       m->u.m3.m3i1,
	       m->u.m3.m3i2,
	       m->u.m3.m3i3,
	       m->u.m3.m3i4,
	       (int)m->u.m3.m3p1,
	       (int)m->u.m3.m3p2,
	       packed ? "" : "\n",
	       packed ? "" : "\n"/* , */
		);
}