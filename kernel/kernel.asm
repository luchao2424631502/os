%include "sconst.inc"

;导入函数
extern cstart
extern kernel_main
extern exception_handler;异常处理函数在protect.h中定义
extern spurious_irq
extern disp_str
extern delay
extern clock_handler
extern irq_table

;全局变量
extern gdt_ptr
extern idt_ptr
extern disp_pos 
extern p_proc_ready
extern tss
extern k_reenter
extern sys_call_table   ;系统调用表

bits 32

[SECTION .data]
clock_int_msg   db "^",0

[SECTION .bss]
StackSpace  resb 2 * 1024
StackTop:   

[SECTION .text]
;导出start以及 中断异常处理过程函数
global _start

global restart
global sys_call

;cpu中断以及异常
global divide_error
global single_step_exception
global nmi
global breakpoint_exception
global overflow
global bounds_check
global inval_opcode
global copr_not_available
global double_fault
global copr_seg_overrun
global inval_tss
global segment_not_present
global stack_exception
global general_protection
global page_fault
global copr_error
;栈符合参数(eip,中断向量号,错误码,eip,ecs,eflags)

global hwint00
global hwint01 
global hwint02 
global hwint03 
global hwint04 
global hwint05 
global hwint06
global hwint07
global hwint08
global hwint09
global hwint10 
global hwint11 
global hwint12 
global hwint13
global hwint14 
global hwint15

_start:
    mov esp,StackTop
    ;disp_pos,初始化
    mov dword [disp_pos],0

    sgdt [gdt_ptr]      ;将GDT从loader中移动到kernel中
    call cstart 
    lgdt [gdt_ptr]      ;cstart中准备好了gdt_ptr和gdt表内容
    
    lidt [idt_ptr]

    jmp SELECTOR_KERNEL_CS:csinit
csinit:
    ;第一个进程的TSS已经在GDT中准备好了,加载TR寄存器
    xor eax,eax 
    mov ax,SELECTOR_TSS
    ltr ax

    ;sti 在r0->r1的eflags中设置了IF,等价于sti
    jmp kernel_main
    
;硬件中断 宏定义
%macro hwint_master 1
    call save
    
    ;屏蔽当前中断
    in al,INT_M_CTLMASK
    or al,(1 << %1)
    out INT_M_CTLMASK,al 

    mov al,EOI              ;发送EOI,结束当前中断
    out INT_M_CTL,al 

    sti                     ;处理什么中断就屏蔽什么中断,但是可以响应其他中断
    ;--------------进程调度--------------------------
    push %1
    call [irq_table + 4 * %1]
    pop ecx 

    cli 

    ;开启当前中断
    in al,INT_M_CTLMASK
    and al,~(1 << %1)
    out INT_M_CTLMASK,al 

    ret ;转跳到return or return_reenter
%endmacro 

;-----------------硬件中断------------------------------
ALIGN 16
hwint00:
    hwint_master 0

ALIGN 16
hwint01:
    hwint_master 1

ALIGN 16
hwint02:
    hwint_master 2

ALIGN 16
hwint03:
    hwint_master 3

ALIGN 16
hwint04:
    hwint_master 4

ALIGN 16
hwint05:
    hwint_master 5

ALIGN 16
hwint06:
    hwint_master 6

ALIGN 16
hwint07:
    hwint_master 7

%macro hwint_slave 1;编写硬盘据驱动,重写从片中断
    call save
    in al,INT_S_CTLMASK
    or al,(1 << (%1 - 8))   ;屏蔽当前中断
    out INT_S_CTLMASK,al 

    mov al,EOI
    out INT_M_CTL,al 
    nop 
    out INT_S_CTL,al;主从片都要置EOI

    sti         ;CPU在响应中断时会自动关中断,接下来打开中断
    push %1
    call [irq_table + 4 * %1]
    pop ecx 
    cli 

    in al,INT_S_CTLMASK
    and al,~(1 << (%1 - 8))     ;打开当前中断
    out INT_S_CTLMASK,al 

    ret 
%endmacro

ALIGN 16
hwint08:
    hwint_slave 8

ALIGN 16
hwint09:
    hwint_slave 9

ALIGN 16
hwint10:
    hwint_slave 10

ALIGN 16
hwint11:
    hwint_slave 11

ALIGN 16
hwint12:
    hwint_slave 12 

ALIGN 16
hwint13:
    hwint_slave 13

ALIGN 16
hwint14:
    hwint_slave 14

ALIGN 16
hwint15:
    hwint_slave 15

;cpu(中断和异常) (错误码查看保护模式下对应的错误码)
divide_error:
    push 0xFFFFFFFF     ;no error code
    push 0              ;向量号
    jmp exception
single_step_exception:
    push 0xFFFFFFFF
    push 1
    jmp exception
nmi:
    push 0xFFFFFFFF
    push 2
    jmp exception
breakpoint_exception:
    push 0xFFFFFFFF
    push 3
    jmp exception
overflow:
    push 0xFFFFFFFF
    push 4
    jmp exception
bounds_check:
    push 0xFFFFFFFF
    push 5
    jmp exception
inval_opcode:
    push 0xFFFFFFFF
    push 6
    jmp exception
copr_not_available:
    push 0xFFFFFFFF
    push 7
    jmp exception
double_fault:;有错误码,自己入栈
    push 8
    jmp exception
copr_seg_overrun:
    push 0xFFFFFFFF
    push 9
    jmp exception
inval_tss:
    push 10
    jmp exception
segment_not_present:
    push 11
    jmp exception
stack_exception:
    push 12
    jmp exception
general_protection:
    push 13 
    jmp exception
page_fault:
    push 14
    jmp exception
copr_error:
    push 0xFFFFFFFF
    push 16 
    jmp exception
    
exception:
    call exception_handler
    add esp,4*2         ;指向EIP(此时还在中断处理函数中)
    hlt 
    
;--------save-----------------
save:
    pushad 
    push ds 
    push es 
    push fs 
    push gs     ;进程表寄存器存满了

    mov esi,edx 

    mov dx,ss 
    mov ds,dx 
    mov es,dx 
    mov fs,dx 

    mov edx,esi 

    mov esi,esp                 ;保存当前栈的值
    
    inc dword [k_reenter]   ;判断是否重入
    cmp dword [k_reenter],0
    jne .1
    ;-----------进入内核栈----------- 
    mov esp,StackTop
    push restart
    jmp [esi + RETADR - P_STACKBASE]      ;退出save
    ;-------------------------------
.1:
    push restart_reenter
    jmp [esi + RETADR - P_STACKBASE]     ;退出save
;----------------------------------------------------------------

;sys_call:
sys_call:
    call save 
    ;注意此时已经在内核栈中了,
    sti 
    push esi 

    push dword [p_proc_ready]
    push edx 
    push ecx 
    push ebx 
    call [sys_call_table + eax * 4]     ;调用对应API
    add esp,4 * 4

    pop esi 
    mov [esi + EAXREG - P_STACKBASE],eax ;写回去
    cli 
    ret 

;-----------------------------------------------------------------
restart:
    mov esp,[p_proc_ready]
    lldt [esp + P_LDT_SEL]              ;第一个进程的LDT加载到ldtr
    lea eax,[esp + P_STACKTOP]      
    mov dword [tss + TSS3_S_SP0],eax    ;r1-r0时栈指针指向进程表,ss0在kernel_main中设置了

    ;iretd 因为eflags恢复所以IF=1,开启了时钟中断
restart_reenter:    
    dec dword [k_reenter]
    pop gs
    pop fs
    pop es 
    pop ds 
    popad 

    add esp,4

    iretd 
; 