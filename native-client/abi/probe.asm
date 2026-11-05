option casemap:none
EXTERN SelectedTarget:QWORD
EXTERN Inputs:BYTE
EXTERN ExpectedRsp:QWORD
EXTERN StolenR9:QWORD
EXTERN StolenR10:QWORD
EXTERN SeatAddonFullObserverActive:DWORD
.data
SavedMxcsr DWORD 0
InputMxcsr DWORD 03f80h
BadMxcsr DWORD 05f80h
Seed BYTE 16 DUP(05ah)
Manager QWORD 0123456789abcdefh
.code
PUBLIC ExerciseInlineSite
ExerciseInlineSite PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push r14
    .pushreg r14
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    stmxcsr SavedMxcsr
    mov ExpectedRsp, rsp
    ldmxcsr InputMxcsr
    movdqu xmm0, OWORD PTR Seed
    movdqu xmm1, OWORD PTR Seed
    movdqu xmm2, OWORD PTR Seed
    movdqu xmm3, OWORD PTR Seed
    movdqu xmm4, OWORD PTR Seed
    movdqu xmm5, OWORD PTR Seed
    lea rax, Manager
    mov rcx, 2222222222222222h
    mov rdx, 3333333333333333h
    mov r8, 4444444444444444h
    mov r9, 5555555555555555h
    mov r10, 6666666666666666h
    mov r11, 7777777777777777h
    mov rsi, 8899aabbccddeeffh
    mov rbx, 1122334455667788h
    mov r14, 99aabbccddeeff00h
    push 246h
    popfq
    ; This is an inline JMP: no synthetic return address is pushed.
    jmp QWORD PTR [SelectedTarget]
PUBLIC InlineContinuation
InlineContinuation::
    mov QWORD PTR Inputs[0], rax
    mov QWORD PTR Inputs[8], rcx
    mov QWORD PTR Inputs[16], rdx
    mov QWORD PTR Inputs[24], r8
    mov QWORD PTR Inputs[32], r9
    mov QWORD PTR Inputs[40], r10
    mov QWORD PTR Inputs[48], r11
    mov QWORD PTR Inputs[56], rsi
    mov QWORD PTR Inputs[64], rbx
    mov QWORD PTR Inputs[72], r14
    movdqu OWORD PTR Inputs[80], xmm0
    movdqu OWORD PTR Inputs[96], xmm1
    movdqu OWORD PTR Inputs[112], xmm2
    movdqu OWORD PTR Inputs[128], xmm3
    movdqu OWORD PTR Inputs[144], xmm4
    movdqu OWORD PTR Inputs[160], xmm5
    stmxcsr DWORD PTR Inputs[176]
    pushfq
    pop QWORD PTR Inputs[184]
    mov QWORD PTR Inputs[192], rsp
    ; Actual stolen six bytes: 45 33 C9 4C 8B 10.
    xor r9d, r9d
    mov r10, [rax]
    mov StolenR9, r9
    mov StolenR10, r10
    ldmxcsr SavedMxcsr
    add rsp, 20h
    pop r14
    pop rsi
    pop rbx
    ret
ExerciseInlineSite ENDP

PUBLIC ClobberMachineState
ClobberMachineState PROC
    xor eax, eax
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    pxor xmm0, xmm0
    pxor xmm1, xmm1
    pxor xmm2, xmm2
    pxor xmm3, xmm3
    pxor xmm4, xmm4
    pxor xmm5, xmm5
    ldmxcsr BadMxcsr
    ret
ClobberMachineState ENDP
END
