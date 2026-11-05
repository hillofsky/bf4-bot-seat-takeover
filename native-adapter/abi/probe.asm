option casemap:none
EXTERN SelectedTarget:QWORD
EXTERN NativeInputs:BYTE
EXTERN NativeReturns:BYTE
EXTERN ExpectedRsp:QWORD
EXTERN ExpectedReturn:QWORD
.data
SavedMxcsr DWORD 0
InputMxcsr DWORD 03f80h
ClobberMxcsr DWORD 05f80h
Seed BYTE 16 DUP(05ah)
ReturnSeed BYTE 16 DUP(0a5h)
.code
PUBLIC ExerciseTarget
ExerciseTarget PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    stmxcsr SavedMxcsr
    lea rax, [rsp-8]
    mov ExpectedRsp, rax
    lea rax, Returned
    mov ExpectedReturn, rax
    ldmxcsr InputMxcsr
    movdqu xmm0, OWORD PTR Seed
    movdqu xmm1, OWORD PTR Seed
    movdqu xmm2, OWORD PTR Seed
    movdqu xmm3, OWORD PTR Seed
    movdqu xmm4, OWORD PTR Seed
    movdqu xmm5, OWORD PTR Seed
    mov rax, 1111111111111111h
    mov rcx, 2222222222222222h
    mov rdx, 3333333333333333h
    mov r8, 4444444444444444h
    mov r9, 5555555555555555h
    mov r10, 6666666666666666h
    mov r11, 7777777777777777h
    push 246h
    popfq
    call QWORD PTR [SelectedTarget]
Returned:
    mov QWORD PTR NativeReturns[0], rax
    movdqu OWORD PTR NativeReturns[8], xmm0
    pushfq
    pop QWORD PTR NativeReturns[24]
    mov QWORD PTR NativeReturns[32], rsp
    stmxcsr DWORD PTR NativeReturns[40]
    ldmxcsr SavedMxcsr
    add rsp, 28h
    ret
ExerciseTarget ENDP

PUBLIC FakeNative
FakeNative PROC
    mov QWORD PTR NativeInputs[0], rax
    mov QWORD PTR NativeInputs[8], rcx
    mov QWORD PTR NativeInputs[16], rdx
    mov QWORD PTR NativeInputs[24], r8
    mov QWORD PTR NativeInputs[32], r9
    mov QWORD PTR NativeInputs[40], r10
    mov QWORD PTR NativeInputs[48], r11
    movdqu OWORD PTR NativeInputs[56], xmm0
    movdqu OWORD PTR NativeInputs[72], xmm1
    movdqu OWORD PTR NativeInputs[88], xmm2
    movdqu OWORD PTR NativeInputs[104], xmm3
    movdqu OWORD PTR NativeInputs[120], xmm4
    movdqu OWORD PTR NativeInputs[136], xmm5
    stmxcsr DWORD PTR NativeInputs[152]
    pushfq
    pop QWORD PTR NativeInputs[160]
    mov QWORD PTR NativeInputs[168], rsp
    mov rax, [rsp]
    mov QWORD PTR NativeInputs[176], rax
    mov rax, 0fedcba9876543210h
    movdqu xmm0, OWORD PTR ReturnSeed
    push 202h
    popfq
    ret
FakeNative ENDP

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
    ldmxcsr ClobberMxcsr
    ret
ClobberMachineState ENDP
END
