option casemap:none
EXTERN SeatAddonArmorReturn:QWORD
EXTERN SeatAddonOriginalRead:QWORD
EXTERN SeatAddonArmorEnabled:DWORD
EXTERN SeatAddonArmorCorrections:QWORD
.code
PUBLIC SeatAddonArmorRead
SeatAddonArmorRead PROC
    cmp DWORD PTR [SeatAddonArmorEnabled], 0
    je original
    mov rax, [rsp]
    cmp rax, QWORD PTR [SeatAddonArmorReturn]
    jne original
    cmp r8d, 8
    jne original
    mov r10, [rbp-18h]
    mov r11, [rbp-10h]
    test r10, r10
    jz original
    cmp rcx, r10
    jbe original
    cmp rcx, r11
    jae original
    sub r11, r10
    cmp r11, 200h
    ja original
    mov rax, rcx
    sub rax, r10
    test al, 7
    jne original
    mov rax, [rbp-8]
    test rax, rax
    jz original
    cmp [rcx], rax
    je original
    cmp [rcx-8], rax
    jne original
    sub rcx, 8
    lock inc QWORD PTR [SeatAddonArmorCorrections]
original:
    jmp QWORD PTR [SeatAddonOriginalRead]
SeatAddonArmorRead ENDP
END
