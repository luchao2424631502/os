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

#define TTY_FIRST   (tty_table)
#define TTY_END     (tty_table + NR_CONSOLES)

static void init_tty(TTY *);
static void tty_dev_read(TTY *tty);
static void tty_dev_write(TTY *tty);

static void tty_do_read(TTY *,MESSAGE *);
static void tty_do_write(TTY *,MESSAGE *);
static void put_key(TTY *p_tty,u32 key);

/* 3个tty轮流处理 */
void task_tty()
{
    TTY *tty;
    MESSAGE msg;
    
    init_keyboard();/* 初始化键盘? */

    for (tty=TTY_FIRST; tty<TTY_END; tty++)
    {
        init_tty(tty);
    }
    // nr_current_console = 0; -> 由slect_console来初始化
    select_console(0);
    
    while(1)
    {
        for (tty = TTY_FIRST; tty < TTY_END; tty++)
        {
            do {
                tty_dev_read(tty);
                tty_dev_write(tty);
            } while(tty->ibuf_cnt);/* 缓冲区是否满了 */
        }

        send_recv(RECEIVE,ANY,&msg);
        
        int src = msg.source;
        assert(src != TASK_TTY);

        TTY *ptty = &tty_table[msg.DEVICE];/* 根据消息操作哪一个TTY */

        switch (msg.type)
        {
        case DEV_OPEN:
            reset_msg(&msg);
            msg.type = SYSCALL_RET;
            send_recv(SEND,src,&msg);
            break;
        case DEV_READ:
            tty_do_read(ptty,&msg);
            break;
        case DEV_WRITE:
            tty_do_write(ptty,&msg);
            break;
        case HARD_INT:
            key_pressed = 0;
            continue;
        default:
            dump_msg("TTY::unknown msg", &msg);
            break;
        }
    }
}

/* 初始化tty */
static void init_tty(TTY *p_tty)
{
    p_tty->ibuf_cnt = 0;
    p_tty->ibuf_head = p_tty->ibuf_tail = p_tty->ibuf;
    
    p_tty->tty_caller = NO_TASK;
    p_tty->tty_procnr = NO_TASK;
    p_tty->tty_req_buf = 0;
    p_tty->tty_left_cnt = 0;
    p_tty->tty_trans_cnt = 0;

    /* tty指向对应的console,以及console初始化 */
    init_screen(p_tty);
}

/*处理字符(由扫描码转化得到) */
void in_process(TTY *p_tty,u32 key)
{
    // char output[2] = {'\0','\0'};

    /* 表示key是可打印字符 */
    if (!(key & FLAG_EXT))
    {
        put_key(p_tty,key);
    }
    /* 不可打印字符 */
    else 
    {
        int raw_code = key & MASK_RAW;
        switch (raw_code)
        {
        case ENTER:
            put_key(p_tty,'\n');
            break;
        case BACKSPACE:
            put_key(p_tty,'\b');
            break;
        case UP:
            /* 检测是否包含shift */
            if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R))
            {
                scroll_screen(p_tty->console,SCR_UP);
            }
            break;
        case DOWN:
            if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R))
            {
                scroll_screen(p_tty->console,SCR_DN);
            }
            break;
        case F1:
        case F2:
        case F3:
        case F4:
        case F5:
        case F6:
        case F7:
        case F8:
        case F9:
        case F10:
        case F11:
        case F12:
            /* Alt + F1 ~ F12 -> 切换控制台 */
            if ((key & FLAG_ALT_L) || (key & FLAG_ALT_R))
            {
                select_console(raw_code - F1);
            }
            // else 
            // {
            //     if (raw_code == F12)
            //     {
            //         disable_int();
            //         dump_proc(proc_table + 4);
            //         for(;;);
            //     }
            // }
            break;
        default:
            break;
        }
    }
}

/* 可打印字符写入键盘缓冲区 */
static void put_key(TTY *p_tty,u32 key)
{
    /* 把可打印字符添加到tty的缓冲区 */
    if (p_tty->ibuf_cnt < TTY_IN_BYTES)
    {
        *(p_tty->ibuf_head) = key;
        p_tty->ibuf_head++;
        if (p_tty->ibuf_head == p_tty->ibuf + TTY_IN_BYTES)
        {
            p_tty->ibuf_head = p_tty->ibuf;
        }
        p_tty->ibuf_cnt++;
    }
}

/* 从键盘获取字符 */
static void tty_dev_read(TTY *tty)
{
    if (is_current_console(tty->console))
        keyboard_read(tty);
}

/* 回显刚刚按下的字符并将其转移到等待过程 */
static void tty_dev_write(TTY *tty)
{
    while(tty->ibuf_cnt)
    {
        char ch = *(tty->ibuf_tail);
        tty->ibuf_tail++;
        if (tty->ibuf_tail == tty->ibuf + TTY_IN_BYTES)/* tail是否到达尾部 */
        {
            tty->ibuf_tail = tty->ibuf;
        }
        tty->ibuf_cnt--;

        if (tty->tty_left_cnt)
        {
            if (ch >= ' ' && ch <= '~')/* 可打印字符 */
            {
                out_char(tty->console,ch);

                assert(tty->tty_req_buf);
                void *p = tty->tty_req_buf + tty->tty_trans_cnt;
                phys_copy(p,(void *)va2la(TASK_TTY,&ch),1);
                tty->tty_trans_cnt++;
                tty->tty_left_cnt--;
            } else if (ch == '\b' && tty->tty_trans_cnt)
            {
                out_char(tty->console,ch);
                tty->tty_trans_cnt--;
				tty->tty_left_cnt++;
            }

            if (ch == '\n' || tty->tty_left_cnt == 0)
            {
                out_char(tty->console,'\n');

                assert(tty->tty_procnr != NO_TASK);
                MESSAGE msg;
                msg.type    = RESUME_PROC;
                msg.PROC_NR = tty->tty_procnr;
                msg.CNT     = tty->tty_trans_cnt;
                send_recv(SEND,tty->tty_caller,&msg);
                tty->tty_left_cnt = 0;
            }
        }
    }
}

/* void tty_do_read() 
设置完tty struct.telling fs的某些成员以挂起要读取的proc后，
例程将立即返回，此处的传输（tty缓冲区-> proc缓冲区）未完成
*/
static void tty_do_read(TTY* tty,MESSAGE *msg)
{
    tty->tty_caller     = msg->source;
    tty->tty_procnr     = msg->PROC_NR;
    tty->tty_req_buf    = va2la(tty->tty_procnr,msg->BUF);
    tty->tty_left_cnt   = msg->CNT;
    tty->tty_trans_cnt  = 0;

    msg->type           = SUSPEND_PROC;
    // msg->CNT            = tty->tty_left_cnt;
    send_recv(SEND,tty->tty_caller,msg);
}

static void tty_do_write(TTY* tty, MESSAGE* msg)
{
	char buf[TTY_OUT_BUF_LEN];
	char * p = (char*)va2la(msg->PROC_NR, msg->BUF);
	int i = msg->CNT;
	int j;

	while (i) {
		int bytes = min(TTY_OUT_BUF_LEN, i);
		phys_copy(va2la(TASK_TTY, buf), (void*)p, bytes);
		for (j = 0; j < bytes; j++)
			out_char(tty->console, buf[j]);
		i -= bytes;
		p += bytes;
	}

	msg->type = SYSCALL_RET;
	send_recv(SEND, msg->source, msg);
}

/* 系统调用内核版本 int sys_write(char *buf,int len,PROCESS *p_proc) */
// int sys_write(char *buf,int len,PROCESS *p_proc)
// {
//     tty_write(&tty_table[p_proc->nr_tty],buf,len);
//     return 0;
// }

/*  */
int sys_printx(int _unused1, int _unused2, char* s, struct proc* p_proc)
{
	const char * p;
	char ch;

	char reenter_err[] = "? k_reenter is incorrect for unknown reason";
	reenter_err[0] = MAG_CH_PANIC;

	/**
	 * @note Code in both Ring 0 and Ring 1~3 may invoke printx().
	 * If this happens in Ring 0, no linear-physical address mapping
	 * is needed.
	 *
	 * @attention The value of `k_reenter' is tricky here. When
	 *   -# printx() is called in Ring 0
	 *      - k_reenter > 0. When code in Ring 0 calls printx(),
	 *        an `interrupt re-enter' will occur (printx() generates
	 *        a software interrupt). Thus `k_reenter' will be increased
	 *        by `kernel.asm::save' and be greater than 0.
	 *   -# printx() is called in Ring 1~3
	 *      - k_reenter == 0.
	 */
	if (k_reenter == 0)  /* printx() called in Ring<1~3> */
		p = va2la(proc2pid(p_proc), s);
	else if (k_reenter > 0) /* printx() called in Ring<0> */
		p = s;
	else	/* this should NOT happen */
		p = reenter_err;

	/**
	 * @note if assertion fails in any TASK, the system will be halted;
	 * if it fails in a USER PROC, it'll return like any normal syscall
	 * does.
	 */
	if ((*p == MAG_CH_PANIC) ||
	    (*p == MAG_CH_ASSERT && p_proc_ready < &proc_table[NR_TASKS])) {
		disable_int();
		char * v = (char*)V_MEM_BASE;
		const char * q = p + 1; /* +1: skip the magic char */

		while (v < (char*)(V_MEM_BASE + V_MEM_SIZE)) {
			*v++ = *q++;
			*v++ = RED_CHAR;
			if (!*q) {
				while (((int)v - V_MEM_BASE) % (SCR_WIDTH * 16)) {
					/* *v++ = ' '; */
					v++;
					*v++ = GRAY_CHAR;
				}
				q = p + 1;
			}
		}

		__asm__ __volatile__("hlt");
	}

	while ((ch = *p++) != 0) {
		if (ch == MAG_CH_PANIC || ch == MAG_CH_ASSERT)
			continue; /* skip the magic char */

		/* TTY * ptty; */
		/* for (ptty = TTY_FIRST; ptty < TTY_END; ptty++) */
		/* 	out_char(ptty->console, ch); /\* output chars to all TTYs *\/ */
		out_char(TTY_FIRST->console, ch);
	}

	//__asm__ __volatile__("nop;jmp 1f;ud2;1: nop");
	//__asm__ __volatile__("nop;cli;1: jmp 1b;ud2;nop");

	return 0;
}