#include "type.h"       //u16等 int_handler函数类型定义
#include "stdio.h"
#include "const.h"      //idt个数
#include "protect.h"    //描述定义常量等宏
#include "fs.h"
#include "tty.h"
#include "console.h"
#include "proc.h"
#include "string.h"
#include "global.h"     //引入idt表
#include "proto.h"      //全局函数声明




//局部函数,目的:生成IDT表中的门()
static void init_idt_desc(unsigned char vector,u8 desc_type,int_handler handler,unsigned char privilege);
static void init_descriptor(struct descriptor *p_desc,u32 base,u32 limit,u16 attribute);


//kernel.asm中导出了中断函数名符号,在使用全局函数前声明一下 (等价于汇编中extern引入外部函数符号)
/*cpu中断异常在IDT中注册的偏移地址*/
void divide_error();
void single_step_exception();
void nmi();
void breakpoint_exception();
void overflow();
void bounds_check();
void inval_opcode();
void copr_not_available();
void double_fault();
void copr_seg_overrun();
void inval_tss();
void segment_not_present();
void stack_exception();
void general_protection();
void page_fault();
void copr_error();

/*硬件中断在IDT中注册的偏移地址*/
void hwint00();
void hwint01();
void hwint02();
void hwint03();
void hwint04();
void hwint05();
void hwint06();
void hwint07();
void hwint08();
void hwint09();
void hwint10();
void hwint11();
void hwint12();
void hwint13();
void hwint14();
void hwint15();

//cstart()中调用的初始化IDT函数
void init_prot()//在IDT表中提供cpu对应的中断描述符
{
    //初始化 中断模式
    init_8259A();
    
    //在IDT初始化处理器可以处理的中断和异常 (对应中断向量号就是在IDT中的偏移)
    init_idt_desc(INT_VECTOR_DIVIDE,DA_386IGate,divide_error,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_DEBUG,DA_386IGate,single_step_exception,PRIVILEGE_KRNL);
    
    init_idt_desc(INT_VECTOR_NMI,DA_386IGate,nmi,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_BREAKPOINT,DA_386IGate,breakpoint_exception,PRIVILEGE_USER);//ring3可以用

    init_idt_desc(INT_VECTOR_OVERFLOW,DA_386IGate,overflow,PRIVILEGE_USER);

    init_idt_desc(INT_VECTOR_BOUNDS,DA_386IGate,bounds_check,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_INVAL_OP,DA_386IGate,inval_opcode,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_COPROC_NOT,DA_386IGate,copr_not_available,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_DOUBLE_FAULT,DA_386IGate,double_fault,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_COPROC_SEG,DA_386IGate,copr_seg_overrun,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_INVAL_TSS,DA_386IGate,inval_tss,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_SEG_NOT,DA_386IGate,segment_not_present,PRIVILEGE_KRNL);  

    init_idt_desc(INT_VECTOR_STACK_FAULT,DA_386IGate,stack_exception,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_PROTECTION,DA_386IGate,general_protection,PRIVILEGE_KRNL);

    //页错误
    init_idt_desc(INT_VECTOR_PAGE_FAULT,DA_386IGate,page_fault,PRIVILEGE_KRNL);
    
    init_idt_desc(INT_VECTOR_COPROC_ERR,DA_386IGate,copr_error,PRIVILEGE_KRNL);

    /*----硬件中断:从0x20开始的15个----*/
    init_idt_desc(INT_VECTOR_IRQ0 + 0,DA_386IGate,hwint00,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 1,DA_386IGate,hwint01,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 2,DA_386IGate,hwint02,PRIVILEGE_KRNL);
    
    init_idt_desc(INT_VECTOR_IRQ0 + 3,DA_386IGate,hwint03,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 4,DA_386IGate,hwint04,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 5,DA_386IGate,hwint05,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 6,DA_386IGate,hwint06,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 7,DA_386IGate,hwint07,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 0,DA_386IGate,hwint08,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 1,DA_386IGate,hwint09,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 2,DA_386IGate,hwint10,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 3,DA_386IGate,hwint11,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 4,DA_386IGate,hwint12,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 5,DA_386IGate,hwint13,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 6,DA_386IGate,hwint14,PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 7,DA_386IGate,hwint15,PRIVILEGE_KRNL);

    /*----初始化系统调用中断号 int 0x90(仿制----*/
    init_idt_desc(INT_VECTOR_SYS_CALL,DA_386IGate,sys_call,PRIVILEGE_USER);

    /*----OS的第一个进程: 填充GDT中TSS描述符 (多进程公用一个TSS实现由中断r1->r0)----*/
    memset(&tss,0,sizeof(tss));
    tss.ss0 = SELECTOR_KERNEL_DS;
    //tss的esp0=0,那么从r1->r0 ?
    init_desc(&gdt[INDEX_TSS],
                    makelinear(SELECTOR_KERNEL_DS,&tss),
                    sizeof(tss) - 1,
                    DA_386TSS);
    tss.iobase = sizeof(tss);//没有io位图

    /*填写GDT中LDT描述符 多进程的LDT*/
    int i;
    
    /* 每个进程涉及的GDT->proc_table[]中LDT描述符以及ldt_sel指向GDT等关系 */
    for (i=0; i<NR_TASKS + NR_PROCS; i++)
    {
        memset(&proc_table[i],0,sizeof(struct proc));

        proc_table[i].ldt_sel   = SELECTOR_LDT_FIRST + (i << 3);/* 选择子后3bit没有用到,所以偏移3bit表示下一个选择子 */
        assert(INDEX_LDT_FIRST + i < GDT_SIZE);
        /* 初始化GDT中描述符(每个进程对应一个LDT表)描述进程表的LDTS数组 */
        init_desc(&gdt[INDEX_LDT_FIRST + i],
                        makelinear(SELECTOR_KERNEL_DS,proc_table[i].ldts),
                        LDT_SIZE * sizeof(struct descriptor) - 1,//每个LDT标准大小
                        DA_LDT);
    }
}

//在IDT表中生成386的中断门描述符
void init_idt_desc(unsigned char vector,u8 desc_type,
                int_handler handler,unsigned char privilege)
{
    struct gate *p_gate    = &idt[vector];//指向第vector个门描述符(其实都是0
    u32 base        = (u32)handler;
    p_gate->offset_low  = base & 0xFFFF;
    p_gate->selector    = SELECTOR_KERNEL_CS;//宏替换selector_flat_c
    p_gate->dcount      = 0;//中断gate没有参数 默认0
    p_gate->attr        = desc_type | (privilege << 5);//DPL
    p_gate->offset_high = (base >> 16) & 0xFFFF;
} 

u32 seg2phys(u16 seg)
{
    struct descriptor *p_dest = &gdt[seg >> 3];
    return (p_dest->base_high<<24 ) | (p_dest->base_mid<<16) | (p_dest->base_low);
}

/* 和上面函数功能一样,目前不能还不能替换所有(暂时先添加一个新的) */
u32 seg2linear(u16 seg)
{
    struct descriptor *p_dest = &gdt[seg >> 3];
    return (p_dest->base_high<<24) | (p_dest->base_mid<<16) | (p_dest->base_low);
}

/*描述符*/
static void init_descriptor(struct descriptor *p_desc,u32 base,u32 limit,u16 attribute) 
{
    p_desc->limit_low   = limit & 0x0FFFF;
    p_desc->base_low    = base  & 0x0FFFF;
    p_desc->base_mid    = (base >> 16) & 0x0FF;
    p_desc->attr1       = attribute & 0xFF;
    p_desc->limit_high_attr2 = ((limit >> 16) & 0x0F) | ((attribute >> 8) & 0xF0);
    p_desc->base_high   = (base >> 24) & 0x0FF;
}

/* 添加全局的init_desc(上面的给原来用_一个一个修改原来调用太麻烦) */
void init_desc(struct descriptor *p_desc,u32 base,u32 limit,u16 attribute) 
{
    p_desc->limit_low   = limit & 0x0FFFF;
    p_desc->base_low    = base  & 0x0FFFF;
    p_desc->base_mid    = (base >> 16) & 0x0FF;
    p_desc->attr1       = attribute & 0xFF;
    p_desc->limit_high_attr2 = ((limit >> 16) & 0x0F) | ((attribute >> 8) & 0xF0);
    p_desc->base_high   = (base >> 24) & 0x0FF;
}

//中断处理例程,根据vec_no调用具体处理函数,(此时中断是没有特权级转换的中断)
void exception_handler(int vec_no,int err_code,int eip,int cs,int eflags)
{
    int i;
    int text_color = 0x74;  //灰底红字

	char err_description[][64] = {	
                    "#DE Divide Error",
					"#DB RESERVED",
					"—  NMI Interrupt",
					"#BP Breakpoint",
					"#OF Overflow",
					"#BR BOUND Range Exceeded",
					"#UD Invalid Opcode (Undefined Opcode)",
					"#NM Device Not Available (No Math Coprocessor)",
					"#DF Double Fault",
					"    Coprocessor Segment Overrun (reserved)",
					"#TS Invalid TSS",
					"#NP Segment Not Present",
					"#SS Stack-Segment Fault",
					"#GP General Protection",
					"#PF Page Fault",
					"—  (Intel reserved. Do not use.)",
					"#MF x87 FPU Floating-Point Error (Math Fault)",
					"#AC Alignment Check",
					"#MC Machine Check",
					"#XF SIMD Floating-Point Exception"
				};

    disp_pos = 0;
    for (i=0; i<80*5; i++)//打印空格清屏
    {
        disp_str(" ");
    }
    disp_pos = 0;

	disp_color_str("Exception! --> ", text_color);
	disp_color_str(err_description[vec_no], text_color);
	disp_color_str("\n\n", text_color);
	disp_color_str("EFLAGS:", text_color);
	disp_int(eflags);
	disp_color_str("CS:", text_color);
	disp_int(cs);
	disp_color_str("EIP:", text_color);
	disp_int(eip);

    if (err_code != 0xFFFFFFFF)
    {
        disp_color_str("Error code:",text_color);
        disp_int(err_code);
    }
}








