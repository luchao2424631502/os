%include "sconst.inc"

INT_VECTOR_SYS_CALL equ 0x90        ;系统调用中断号
_NR_printx          equ 0
_NR_sendrec         equ 1

;导出符号
global printx
global sendrec

bits 32
[SECTION .text]

;sendrec(int function,int src_dest,MESSAGE* msg)
sendrec:
    push ebx 
    push ecx 
    push edx 

    mov eax,_NR_sendrec
    mov ebx,[esp + 12 + 4]
    mov ecx,[esp + 12 + 8]
    mov edx,[esp + 12 + 12]
    int INT_VECTOR_SYS_CALL

    pop edx 
    pop ecx 
    pop ebx 

    ret 

;
printx:
    push edx 
    mov eax,_NR_printx
    mov edx,[esp + 4 + 4]
    int INT_VECTOR_SYS_CALL
    pop edx 
    ret 
