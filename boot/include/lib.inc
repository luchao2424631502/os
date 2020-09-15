DispAL:
	push	ecx
	push	edx
	push	edi

	mov	edi, [dwDispPos]

	mov	ah, 0Fh			; 0000b: 黑底    1111b: 白字
	mov	dl, al
	shr	al, 4
	mov	ecx, 2
.begin:
	and	al, 01111b
	cmp	al, 9
	ja	.1
	add	al, '0'
	jmp	.2
.1:
	sub	al, 0Ah
	add	al, 'A'
.2:
	mov	[gs:edi], ax
	add	edi, 2

	mov	al, dl
	loop	.begin
	;add	edi, 2

	mov	[dwDispPos], edi

	pop	edi
	pop	edx
	pop	ecx

	ret

DispInt:
    mov	eax, [esp + 4]
	shr	eax, 24
	call	DispAL

	mov	eax, [esp + 4]
	shr	eax, 16
	call	DispAL

	mov	eax, [esp + 4]
	shr	eax, 8
	call	DispAL

	mov	eax, [esp + 4]
	call	DispAL

    mov ah,07h
    mov al,'h'
    push edi 
    mov edi,[dwDispPos]
    mov [gs:edi],ax 
    add edi,4
    mov [dwDispPos],edi 
    pop edi 

    ret 

DispStr:
    push ebp 
    mov ebp,esp 
    push ebx 
    push esi 
    push edi 

    mov esi,[ebp + 8]
    mov edi,[dwDispPos]
    mov ah,0Fh
.1:
    lodsb 
    test al,al 
    jz .2 
    cmp al,0Ah
    jnz .3  ;不是回车就转跳
    push	eax
	mov	eax, edi
	mov	bl, 160
	div	bl
	and	eax, 0FFh
	inc	eax
	mov	bl, 160
	mul	bl
	mov	edi, eax
	pop	eax
    jmp .1 
.3:
    mov [gs:edi],ax 
    add edi,2
    jmp .1 

.2:
    mov [dwDispPos],edi 

    pop edi 
    pop esi 
    pop ebx 
    pop ebp 
    ret 


DispReturn:
    push szReturn
    call DispStr
    add esp,4

    ret 

;内存拷贝
Memcpy:
	push ebp 
	mov ebp,esp 
	
	push esi 
	push edi 
	push ecx 

	mov edi,[ebp + 8]		;Destination
	mov esi,[ebp + 12]		;Source
	mov ecx,[ebp + 16]		;Counter

.1:
	cmp ecx,0
	jz .2

	mov al,[ds:esi]
	inc esi

	mov byte [es:edi],al	;逐字节移动
	inc edi 

	dec ecx 
	jmp .1
.2:
	mov eax,[ebp + 8] 		;返回值是目的地址
	
	pop ecx 
	pop edi 
	pop esi 
	mov esp,ebp 
	pop ebp 

	ret 
