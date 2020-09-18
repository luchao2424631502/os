extern main 
extern exit ;这个符号是提供的库

bits 32
[section .text]

global _start

_start:
    push eax ;argc,argv参数
    push ecx 
    call main 

    push eax 
    call exit;

    hlt
