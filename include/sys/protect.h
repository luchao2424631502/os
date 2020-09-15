#ifndef _ORANGES_PROTECT_H_
#define _ORANGES_PROTECT_H_

//存储段描述符
struct descriptor{
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8 attr1;
    u8 limit_high_attr2;
    u8 base_high;
};

/* 从描述符中提取出本来的值 */
#define reassembly(high,high_shift,mid,mid_shift,low)   \
    (((high) << (high_shift)) + ((mid) << (mid_shift)) + (low))


//门描述符
struct gate{
    u16 offset_low;
    u16 selector;       //转跳过去的选择子
    u8  dcount;         //参数占据5bit,高3bit全是0(调用门栈切换需要辅助参数个数)
    u8  attr;           //p(1) DPL(2) DT(1) type(4)
    u16 offset_high;     
};

struct tss{
    u32 backlink;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 flags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16 trap;/*陷阱调试bit*/
    u16 iobase;/*iobase*/
};

/*GDT描述符索引(loader中已经确定拥有的)*/
#define INDEX_DUMMY     0
#define INDEX_FLAT_C    1
#define INDEX_FLAT_RW   2
#define INDEX_VIDEO     3
#define INDEX_TSS       4
#define INDEX_LDT_FIRST 5

/*选择子*/
#define SELECTOR_DUMMY      0
#define SELECTOR_FLAT_C     0x08
#define SELECTOR_FLAT_RW    0x10
#define SELECTOR_VIDEO      (0x18+3)//RPL=3
#define SELECTOR_TSS        0x20 
#define SELECTOR_LDT_FIRST  0x28

#define SELECTOR_KERNEL_CS  SELECTOR_FLAT_C
#define SELECTOR_KERNEL_DS  SELECTOR_FLAT_RW
#define SELECTOR_KERNEL_GS  SELECTOR_VIDEO

/*LDT大小*/
#define LDT_SIZE    2
/*  */
#define INDEX_LDT_C     0
#define INDEX_LDT_RW    1


//自己对照描述符格式(16bit)对应上
/*描述符类型,从汇编宏转到C宏*/
#define DA_32           0x4000      //32bit段
#define DA_LIMIT_4K     0x8000      //段界限粒度4K
#define LIMIT_4K_SHIFT  12
#define DA_DPL0         0x00        
#define DA_DPL1         0x20
#define DA_DPL2         0x40
#define DA_DPL3         0x60

/*存储段描述符类型值*/
#define DA_DR           0x90        //只读数据段
#define DA_DRW          0x92        //可读写数据段
#define DA_DRWA         0x93        //已被访问的可读写数据段
#define DA_C            0x98        //可执行代码段
#define DA_CR           0x9A        //可读可执行代码段
#define DA_CCO          0x9C        //可执行一致代码段
#define DA_CCOR         0x9E        //可执行可读一致代码段

/*系统段描述符类型值*/
#define DA_LDT          0x82        //
#define DA_TaskGate     0x85        //任务门
#define DA_386TSS       0x89        //可用TSS
#define DA_386CGate     0x8C        //386调用门
#define DA_386IGate     0x8E        //386中断门
#define DA_386TGate     0x8F        //陷阱门

/*选择子类型:*/
#define SA_RPL_MASK     0xFFFC
#define SA_RPL0         0
#define SA_RPL1         1
#define SA_RPL2         2
#define SA_RPL3         3

#define SA_TI_MASK      0xFFFB
#define SA_TIG          0
#define SA_TIL          4

/*中断向量(对照保护模式中断和异常)*/
#define INT_VECTOR_DIVIDE       0x0 //除法错
#define INT_VECTOR_DEBUG        0x1 //调试异常
#define INT_VECTOR_NMI          0x2 //非屏蔽外部中断
#define INT_VECTOR_BREAKPOINT   0x3 //调试断点
#define INT_VECTOR_OVERFLOW     0x4 //溢出
#define INT_VECTOR_BOUNDS       0x5 //越界
#define INT_VECTOR_INVAL_OP     0x6 //无效指令 ud2
#define INT_VECTOR_COPROC_NOT   0x7 //无数学协处理器
#define INT_VECTOR_DOUBLE_FAULT 0x8 //双重错误
#define INT_VECTOR_COPROC_SEG   0x9 //(保留)协处理器段越界
#define INT_VECTOR_INVAL_TSS    0xA //无效TSS
#define INT_VECTOR_SEG_NOT      0xB //段不存在
#define INT_VECTOR_STACK_FAULT  0xC //栈错误
#define INT_VECTOR_PROTECTION   0xD //常规保护错误(内存或者其他保护检验)
#define INT_VECTOR_PAGE_FAULT   0xE //页错误
#define INT_VECTOR_COPROC_ERR   0x10//FPU浮点错误

/*中断请求号*/
#define INT_VECTOR_IRQ0         0x20 
#define INT_VECTOR_IRQ8         0x28

#define INT_VECTOR_SYS_CALL     0x90

/*线性地址 -> 物理地址*/
#define vir2phys(seg_base,vir) (u32)(((u32)seg_base) + (u32)(vir))

/* seg:off -> linear addr (段地址+偏移 转化为 线性地址) */
#define makelinear(seg,off) (u32)(((u32)(seg2linear(seg))) + (u32)(off))

#endif 