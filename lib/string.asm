[SECTION .text]
global memcpy       ;导出函数符号
global memset
global strcpy
global strlen

;void* memcpy(void* es:pDest,void* ds:pSrc,int iSize);
memcpy:
    push ebp 
    mov ebp,esp 

    push esi 
    push edi 
    push ecx 
    ;栈中应该还有ip
    mov edi,[ebp + 8]
    mov esi,[ebp + 12]
    mov ecx,[ebp + 16]
.1:
    cmp ecx,0
    jz .2 
    
    mov al,[ds:esi]
    inc esi 

    mov byte [es:edi],al 
    inc edi 

    dec ecx 
    jmp .1 

.2:
    mov eax,[ebp + 8]

    pop ecx 
    pop edi 
    pop esi 
    mov esp,ebp 
    pop ebp 

    ret 
;--------------------------

;----void memset(void *p_dst,char ch,int size)
memset:
    push ebp
    mov ebp,esp 

    push esi 
    push edi 
    push ecx 

    mov edi,[ebp + 8]       ;p_dst
    mov edx,[ebp + 12]      ;ch
    mov ecx,[ebp + 16]      ;size

.1:
    cmp ecx,0
    jz .2 

    mov byte [edi],dl 
    inc edi 
    
    dec ecx 
    jmp .1 
.2:
    
    pop ecx 
    pop edi 
    pop esi 
    mov esp,ebp 
    pop ebp 

    ret 

;----------char *strcpy(char *p_dst,char *p_src);
strcpy:
    push ebp 
    mov ebp,esp 
    
    mov esi,[ebp + 12]
    mov edi,[ebp + 8]
.1:
    mov al,[esi]
    inc esi 

    mov byte [edi],al 
    inc edi 

    cmp al,0
    jnz .1 

    mov eax,[ebp + 8]       ;eax存返回值,=p_dst

    pop ebp 
    ret

;int strlen(char *p_str);
strlen:
    push ebp
    mov ebp,esp

    mov eax,0
    mov esi,[ebp + 8]
    
.1:
    cmp byte [esi],0
    jz .2 
    inc esi 
    inc eax 
    jmp .1

.2:
    pop ebp 
    ret 