.data
extern getMultiplier : proc
extern returnAddress : qword

public SoulMultiplier

.code
	SoulMultiplier proc
        mov ebx,edx ; 5A541F 2 bytes
        mov rdi,rcx ; ..21 3 bytes

        test    ebx, ebx
        jle      _ret

        push    rax
	    sub     rsp, 40h

        movdqu [rsp + 30h], xmm0
        movdqu [rsp + 20h], xmm1

        call getMultiplier

        cvtsi2ss xmm1, ebx
        mulss xmm1, xmm0
        cvtss2si ebx, xmm1

        movdqu xmm0, [rsp + 30h]
        movdqu xmm1, [rsp + 20h]

        add     rsp, 40h
        pop     rax
_ret:
        jmp returnAddress
	SoulMultiplier endp
end