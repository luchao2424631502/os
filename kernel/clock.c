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

/* 进程调度 */
void clock_handler(int irq)
{
    if (++ticks >= MAX_TICKS)
        ticks = 0;        //统计时钟中断发生次数
    
    if (p_proc_ready->ticks)/* 当前进程剩余时间-1 */
        p_proc_ready->ticks--;

    if (key_pressed)
        inform_int(TASK_TTY);/* 唤醒TTY */

    /* 判断是否重入 */
    if (k_reenter != 0)
    {
        return ;
    }

    /* 并没有和让优先级高的一定要执行完毕,当剩余时间一样,轮流执行 */
    if (p_proc_ready->ticks > 0)
        return ;
    /* 执行进程调度程序 */
    schedule();
}

/* 延迟函数 */
void milli_delay(int milli_sec)
{
    int t = get_ticks();    //t=发生了10ms时钟中断多少次

    while(((get_ticks() - t) * 1000 / HZ) < milli_sec) {}  
}

void init_clock()
{
    /* 初始化8253定时器,设置为10ms一次时钟中断 */
    out_byte(TIMER_MODE,RATE_GENERATOR);    //模式寄存器
    out_byte(TIMER0,(u8)(TIMER_FREQ/HZ));   //100Hz对应计数器的值,这里宏除法靠编译机器的编译器来完成
    out_byte(TIMER0,(u8)((TIMER_FREQ/HZ) >> 8));

    /*开启时钟中断,设置中断处理程序*/
    put_irq_handler(CLOCK_IRQ,clock_handler);
    enable_irq(CLOCK_IRQ);
}