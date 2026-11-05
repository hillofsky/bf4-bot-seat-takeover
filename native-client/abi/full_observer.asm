option casemap:none
EXTERN SeatAddonAllowFullVehicle:PROC
EXTERN SeatAddonOriginalFull:QWORD
EXTERN SeatAddonFullObserverActive:DWORD
.code
PUBLIC SeatAddonFullObserver
; Inline-site entry: RSP mod 16 = 0 (not a function-call entry).
; R14 = UI, RBX = vehicle, SIL = native FULL flag, RAX = UI manager.
SeatAddonFullObserver PROC FRAME
    pushfq
    .allocstack 8
    lock inc DWORD PTR [SeatAddonFullObserverActive]
    sub rsp, 0d8h
    .allocstack 0d8h
    .endprolog
    mov [rsp+20h], rax
    mov [rsp+28h], rcx
    mov [rsp+30h], rdx
    mov [rsp+38h], r8
    mov [rsp+40h], r9
    mov [rsp+48h], r10
    mov [rsp+50h], r11
    mov [rsp+58h], rsi
    stmxcsr [rsp+60h]
    movdqu [rsp+70h], xmm0
    movdqu [rsp+80h], xmm1
    movdqu [rsp+90h], xmm2
    movdqu [rsp+0a0h], xmm3
    movdqu [rsp+0b0h], xmm4
    movdqu [rsp+0c0h], xmm5
    mov rcx, r14
    mov rdx, rbx
    call SeatAddonAllowFullVehicle
    mov [rsp+68h], al
    movdqu xmm0, [rsp+70h]
    movdqu xmm1, [rsp+80h]
    movdqu xmm2, [rsp+90h]
    movdqu xmm3, [rsp+0a0h]
    movdqu xmm4, [rsp+0b0h]
    movdqu xmm5, [rsp+0c0h]
    ldmxcsr [rsp+60h]
    mov rsi, [rsp+58h]
    cmp BYTE PTR [rsp+68h], 0
    je PreserveFull
    and rsi, -256
PreserveFull:
    mov rax, [rsp+20h]
    mov rcx, [rsp+28h]
    mov rdx, [rsp+30h]
    mov r8, [rsp+38h]
    mov r9, [rsp+40h]
    mov r10, [rsp+48h]
    mov r11, [rsp+50h]
    add rsp, 0d8h
    lock dec DWORD PTR [SeatAddonFullObserverActive]
    popfq
    jmp QWORD PTR [SeatAddonOriginalFull]
SeatAddonFullObserver ENDP
END
