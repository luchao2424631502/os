%include "sconst.inc"

extern disp_pos

[SECTION .text]
;导出函数
global  disp_str 
global  disp_color_str
global  out_byte
global  in_byte 

global  enable_irq
global  disable_irq

global  enable_int 
global  disable_int

global  port_read
global  port_write
global  glitter

; void disp_str(char pszInfo);
disp_str:
    push ebp 
    mov ebp,esp 
    
    mov esi,[ebp + 8]
    mov edi,[disp_pos]
    mov ah,0Fh
.1:
    lodsb 
    test al,al 
    jz .2 
    cmp al,0Ah 
    jnz .3
    push eax 
    push ebx 
    mov eax,edi 
    mov bl,160
    div bl 
    and eax,0FFh 
    inc eax 
    mov bl,160
    mul bl 
    mov edi,eax 
    pop ebx 
    pop eax 
    jmp .1 
.3:
    mov [gs:edi],ax 
    add edi,2 
    jmp .1 
.2:
    mov [disp_pos],edi 

    pop ebp 
    ret 

; void disp_color_str(char *info,int color);
disp_color_str:
    push ebp 
    mov ebp,esp 
    
    mov esi,[ebp + 8]       ;info
    mov edi,[disp_pos]
    mov ah,[ebp + 12]       ;颜色

.1:
    lodsb 
    test al,al 
    jz .2 
    cmp al,0Ah 
    jnz .3
    push eax 
    push ebx 
    mov eax,edi
    mov bl,160
    div bl
    and eax,0FFh 
    inc eax 
    mov bl,160
    mul bl
    mov edi,eax 
    pop ebx 
    pop eax 
    jmp .1  
    
.3:
    mov [gs:edi],ax 
    add edi,2 
    jmp .1 

.2:
    mov [disp_pos],edi 
    
    pop ebp 
    ret 

;void out_byte(u16 port,u8 value);
out_byte:
    mov edx,[esp + 4]
    mov al,[esp + 8]
    out dx,al 
    nop 
    nop 
    ret 

;void in_byte(u16 port); 返回给ax
in_byte:
    mov edx,[esp + 4]
    xor eax,eax 
    in al,dx 
    nop 
    nop 
    ret 

;----------------------
port_read:
    mov edx,[esp + 4]
    mov edi,[esp + 4 + 4]
    mov ecx,[esp + 4 + 4 + 4]
    shr ecx,1 
    cld 
    rep insw
    ret 

port_write:
    mov edx,[esp + 4]
    mov esi,[esp + 4 + 4]
    mov ecx,[esp + 4 + 4 + 4]
    shr ecx,1
    cld 
    rep outsw
    ret 
    
;---------------------关闭irq中断,void disable_irq(int irq);
disable_irq:
    mov ecx,[esp + 4]
    pushf 
    cli             ;禁止中断 
    mov ah,1
    rol ah,cl       ; 1 << (irq % 8)
    cmp cl,8
    jae disable_8   ;>=8就转移
disable_0:
    in al,INT_M_CTLMASK
    test al,ah 
    jnz dis_already
    or al,ah 
    out INT_M_CTLMASK,al 
    popf 
    mov eax,1 
    ret 
disable_8:
    in al,INT_S_CTLMASK
    test al,ah 
    jnz dis_already
    or al,ah 
    out INT_S_CTLMASK,al 
    popf 
    mov eax,1 
    ret 
dis_already:
    popf 
    xor eax,eax 
    ret 

;--------------开启irq中断 void enable_irq(int irq);
enable_irq:
    mov ecx,[esp + 4]
    pushf 
    cli 
    mov ah,~1
    rol ah,cl       ;ah = (1 << (irq % 8))
    cmp cl,8
    jae enable_8
enable_0:
    in al,INT_M_CTLMASK
    and al,ah 
    out INT_M_CTLMASK,al 
    popf 
    ret 
enable_8:
    in al,INT_S_CTLMASK
    and al,ah 
    out INT_S_CTLMASK,al 
    popf 
    ret 

;void disable_int();
disable_int:
    cli 
    ret 

;void enable_int();
enable_int:
    sti 
    ret 

;void glitter(int row,int col)
glitter:
    push	eax
	push	ebx
	push	edx

	mov	eax, [.current_char]
	inc	eax
	cmp	eax, .strlen
	je	.1
	jmp	.2
.1:
	xor	eax, eax
.2:
	mov	[.current_char], eax
	mov	dl, byte [eax + .glitter_str]

	xor	eax, eax
	mov	al, [esp + 16]		; row
	mov	bl, .line_width
	mul	bl			; ax <- row * 80
	mov	bx, [esp + 20]		; col
	add	ax, bx
	shl	ax, 1
	movzx	eax, ax

	mov	[gs:eax], dl

	inc	eax
	mov	byte [gs:eax], 4

	jmp	.end

.current_char:	dd	0
.glitter_str:	db	'-\|/'
		db	'1234567890'
		db	'abcdefghijklmnopqrstuvwxyz'
		db	'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
.strlen		equ	$ - .glitter_str
.line_width	equ	80

.end:
	pop	edx
	pop	ebx
	pop	eax
	ret