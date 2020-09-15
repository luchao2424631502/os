//设置8259A
#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "proc.h"
#include "fs.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"

//由汇编代码转到C初始化8259A
void init_8259A()
{
    //主片0x20端口,ICW1=设置模式,级联,需要ICW4
    out_byte(INT_M_CTL,0x11);

    //从片0xA0端口,ICW1
    out_byte(INT_S_CTL,0x11);

    //IQR0对应中断向量号0x20
    out_byte(INT_M_CTLMASK,INT_VECTOR_IRQ0);
    
    //IRQ8(从片)对应中断向量号 0x28
    out_byte(INT_S_CTLMASK,INT_VECTOR_IRQ8);

    //主片IR2对应从片
    out_byte(INT_M_CTLMASK,0x4);

    //设置从片连接主片的IR2 (查看对应的ICW3格式)
    out_byte(INT_S_CTLMASK,0x2);

    //ICW4=中断结束方法,正常EOI
    out_byte(INT_M_CTLMASK,0x1);

    //ICW4从片
    out_byte(INT_S_CTLMASK,0x1);

    /*--------OCW:----------*/
    out_byte(INT_M_CTLMASK,0xFF);
    out_byte(INT_S_CTLMASK,0xFF);   

    /*初始化所有中断的处理函数*/
    int i;
    for (i=0; i<NR_IRQ; i++)
        irq_table[i] = spurious_irq;
}

/*硬件中断的处理函数*/
void spurious_irq(int irq)
{
    disp_str("spurious_irq: ");
    disp_int(irq);
    disp_str("\n");
}

/*初始化中断号的处理函数*/
void put_irq_handler(int irq,irq_handler handler)
{
    disable_irq(irq);
    irq_table[irq] = handler;
}
