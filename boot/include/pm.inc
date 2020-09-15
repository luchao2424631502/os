;usage: Descriptor Base(基础地址),Limit(段长度),Attr(段属性)
DA_32       equ 4000H
DA_LIMIT_4K equ 8000H   ;段界限粒度4Kbyte

DA_DPL0 equ 00h 
DA_DPL1 equ 20h 
DA_DPL2 equ 40h 
DA_DPL3 equ 60h 

;存储段描述符
DA_DR   equ 90h ;只读数据段
DA_DRW  equ 92h ;读写数据段
DA_DRWA equ 93h ;a=1的读写数据段
DA_C    equ 98h ;只执行代码段
DA_CR   equ 9Ah ;可执行可读代码段
DA_CCO  equ 9Ch ;只执行,一致代码段
DA_CCOR equ 9Eh ;可执行可读一致代码段

;系统段描述符
DA_LDT      equ 82h
DA_TaskGate equ 85h
DA_386TSS   equ 89h ;可用386 TSS
DA_386CGate equ 8Ch ;调用门
DA_386IGate equ 8Eh ;中断门
DA_386TGate equ 8Fh ;陷阱门

;选择子类型值 ti 2 RPL 1 0  
SA_RPL0 equ 0
SA_RPL1 equ 1 
SA_RPL2 equ 2
SA_RPL3 equ 3 

SA_TIG  equ 0
SA_TIL  equ 4

;分页机制常量
PG_P    equ 1 
PG_RWR  equ 0
PG_RWW  equ 2
PG_USS  equ 0
PG_USU  equ 4


;拼凑存储段描述符: %1基地址 %2界限 %3属性
%macro Descriptor 3
    dw %2 & 0ffffh  ;段界限0-15
    dw %1 & 0ffffh  ;段基地址0-15 (底32bit)
    db (%1 >> 16) & 0ffh    ;段基础地址1byte
    dw ((%2 >> 8) & 0f00h) | (%3 & 0f0ffh)                  ;type+长度
    db (%1 >> 24) & 0ffh    ;段基地址1byte
%endmacro

;拼凑系统段描述符: %1 selector %2 offset %3 Dcount %4 attr
%macro Gate 4 
    dw (%2 & 0FFFFh)
    dw %1
    dw (%3 & 1Fh) | ((%4 << 8) & 0FF00h)
    dw ((%2 >> 16) & 0FFFFh)
%endmacro 