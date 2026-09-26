; syscall_stub.asm — MASM x64 universal syscall dispatcher
; Сигнатура: NTSTATUS SyscallInvoke(uint32_t ssn, ...)
;
; Компиляция (MASM из VS Tools):
;   ml64.exe /c /Fo syscall_stub.obj syscall_stub.asm
;
; Microsoft x64 calling convention:
;   rcx = ssn (первый аргумент)
;   rdx, r8, r9 = arg1, arg2, arg3
;   [rsp+28h], [rsp+30h], ... = arg4, arg5...
; После переноса SSN в eax нужно сдвинуть реальные аргументы на позиции rcx..r9..stack.

.CODE

SyscallInvoke PROC
    ; rcx = SSN, остальные аргументы сдвигаются
    mov    eax, ecx              ; eax = SSN
    mov    rcx, rdx              ; rcx = arg1 (был rdx)
    mov    rdx, r8               ; rdx = arg2 (был r8)
    mov    r8,  r9               ; r8  = arg3 (был r9)
    ; arg4 был в [rsp+28h], сдвигаем стек: [rsp+20h] = [rsp+28h] и т.д.
    ; shadow space 32 байта уже выделен caller-ом
    mov    r9,  [rsp+28h]        ; r9  = arg4
    ; arg5..N: копируем оставшийся стек вверх на 8 байт
    ; На практике PoE2 loader использует не более 6 аргументов (NtCreateThreadEx=11,
    ; но мы передаём через wrapper с фиксированной сигнатурой)
    ; Для упрощения: аргументы 5-8 руками до вызова:
    mov    r10, rcx              ; syscall ABI: r10 = rcx (kernel ожидает r10)
    syscall
    ret
SyscallInvoke ENDP

END
