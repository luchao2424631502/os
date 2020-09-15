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

/*通过宏定义在global.c中了,现在通过global.h引入,新增idt定义
void* memcpy(void *pDst,void *pSec,int iSize);

void disp_str(char *pszInfo);

u8 gdt_ptr[6];
DESCRIPTOR gdt[GDT_SIZE];
*/

void cstart()
{
    disp_str("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
		 "-----\"cstart\" begins-----\n");
    //将GDT复制到新的地方
    memcpy(&gdt,//此处==gdt
        (void *)(*((u32*)(&gdt_ptr[2]))),
        *((u16*)(&gdt_ptr[0])) + 1
    );

    //修改GDT表的大小128个
    u16 *p_gdt_limit = (u16*)(&gdt_ptr[0]);
    u32 *p_gdt_base  = (u32*)(&gdt_ptr[2]);
    *p_gdt_limit     = GDT_SIZE * sizeof(struct descriptor) - 1;
    *p_gdt_base      = (u32)&gdt;
    
    //现在IDTR还没有指向IDT,我们先准备好idt_ptr和idt内容
    u16 *p_idt_limit = (u16*)(&idt_ptr[0]);
    u32 *p_idt_base  = (u32*)(&idt_ptr[2]);
    *p_idt_limit     = IDT_SIZE * sizeof(struct gate) - 1;
    *p_idt_base      = (u32)&idt;       //指针指向的内容=idt的基础地址(强制类型转化为数)

    //8259A初始化和CPU对应中断在IDT中描述符初始化,以及第一个进程需要的TSS和LDT
    init_prot();

    disp_str("-----\"cstart\" finished-----\n");
}