option casemap:none
EXTERN SeatAddonObserveBoat:PROC
EXTERN SeatAddonOriginalBoat:QWORD
EXTERN SeatAddonOriginalGround:QWORD
EXTERN SeatAddonObserverActive:DWORD
.code
PUBLIC SeatAddonBoatObserver
SeatAddonBoatObserver PROC FRAME
    pushfq
    .allocstack 8
    lock inc DWORD PTR [SeatAddonObserverActive]
    sub rsp, 0c0h
    .allocstack 0c0h
    .endprolog
    mov [rsp+20h], rax
    mov [rsp+28h], rcx
    mov [rsp+30h], rdx
    mov [rsp+38h], r8
    mov [rsp+40h], r9
    mov [rsp+48h], r10
    mov [rsp+50h], r11
    stmxcsr [rsp+58h]
    movdqu [rsp+60h], xmm0
    movdqu [rsp+70h], xmm1
    movdqu [rsp+80h], xmm2
    movdqu [rsp+90h], xmm3
    movdqu [rsp+0a0h], xmm4
    movdqu [rsp+0b0h], xmm5
    call SeatAddonObserveBoat
    movdqu xmm0, [rsp+60h]
    movdqu xmm1, [rsp+70h]
    movdqu xmm2, [rsp+80h]
    movdqu xmm3, [rsp+90h]
    movdqu xmm4, [rsp+0a0h]
    movdqu xmm5, [rsp+0b0h]
    ldmxcsr [rsp+58h]
    mov rax, [rsp+20h]
    mov rcx, [rsp+28h]
    mov rdx, [rsp+30h]
    mov r8, [rsp+38h]
    mov r9, [rsp+40h]
    mov r10, [rsp+48h]
    mov r11, [rsp+50h]
    add rsp, 0c0h
    lock dec DWORD PTR [SeatAddonObserverActive]
    popfq
    jmp QWORD PTR [SeatAddonOriginalBoat]
SeatAddonBoatObserver ENDP
PUBLIC SeatAddonGroundObserver
SeatAddonGroundObserver PROC FRAME
    pushfq
    .allocstack 8
    lock inc DWORD PTR [SeatAddonObserverActive]
    sub rsp, 0c0h
    .allocstack 0c0h
    .endprolog
    mov [rsp+20h], rax
    mov [rsp+28h], rcx
    mov [rsp+30h], rdx
    mov [rsp+38h], r8
    mov [rsp+40h], r9
    mov [rsp+48h], r10
    mov [rsp+50h], r11
    stmxcsr [rsp+58h]
    movdqu [rsp+60h], xmm0
    movdqu [rsp+70h], xmm1
    movdqu [rsp+80h], xmm2
    movdqu [rsp+90h], xmm3
    movdqu [rsp+0a0h], xmm4
    movdqu [rsp+0b0h], xmm5
    call SeatAddonObserveBoat
    movdqu xmm0, [rsp+60h]
    movdqu xmm1, [rsp+70h]
    movdqu xmm2, [rsp+80h]
    movdqu xmm3, [rsp+90h]
    movdqu xmm4, [rsp+0a0h]
    movdqu xmm5, [rsp+0b0h]
    ldmxcsr [rsp+58h]
    mov rax, [rsp+20h]
    mov rcx, [rsp+28h]
    mov rdx, [rsp+30h]
    mov r8, [rsp+38h]
    mov r9, [rsp+40h]
    mov r10, [rsp+48h]
    mov r11, [rsp+50h]
    add rsp, 0c0h
    lock dec DWORD PTR [SeatAddonObserverActive]
    popfq
    jmp QWORD PTR [SeatAddonOriginalGround]
SeatAddonGroundObserver ENDP
END
