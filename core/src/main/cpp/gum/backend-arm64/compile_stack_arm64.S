.text
.align 2
.global spindle_run_on_compile_stack
.hidden spindle_run_on_compile_stack
.type spindle_run_on_compile_stack, %function
spindle_run_on_compile_stack:
    stp x29, x30, [sp, #-48]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    mov x19, x0
    mov x20, x1
    bl spindle_compile_stack_enter
    cbz x0, .Lspindle_compile_direct
    // x0 is one past the last writable byte. Stack grows down.
    bic x0, x0, #15
    mov x21, sp
    mov sp, x0
    mov x0, x20
    blr x19
    mov sp, x21
    bl spindle_compile_stack_leave
    b .Lspindle_compile_out
.Lspindle_compile_direct:
    mov x0, x20
    blr x19
.Lspindle_compile_out:
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #48
    ret
.size spindle_run_on_compile_stack, . - spindle_run_on_compile_stack
