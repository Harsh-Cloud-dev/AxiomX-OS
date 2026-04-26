; =========================================================
; sched.asm — Context switch for Axiom kernel
;
; void context_switch(uint64_t *old_rsp, uint64_t new_rsp)
;   rdi = pointer to old process's rsp field in PCB
;   rsi = new process's saved rsp value
;
; What this does:
;   1. Push all callee-saved registers onto the CURRENT stack
;   2. Save current RSP into *old_rsp  (so we can resume here later)
;   3. Load new_rsp into RSP           (switch to new process's stack)
;   4. Pop all callee-saved registers  (restore new process's state)
;   5. ret                             (return into new process)
;
; Why callee-saved only?
;   context_switch() is called as a normal C function. The C ABI
;   guarantees caller-saved registers (rax, rcx, rdx, rsi, rdi,
;   r8-r11) are already saved by the caller if needed. We only
;   need to preserve callee-saved regs: rbx, rbp, r12-r15.
;   We also save rip implicitly via the call/ret mechanism.
; =========================================================

[bits 64]
[global context_switch]

context_switch:
    ; ── Save callee-saved registers of current process ────
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; ── Save current RSP into old_rsp (rdi) ───────────────
    mov [rdi], rsp

    ; ── Switch to new stack (rsi = new_rsp) ───────────────
    mov rsp, rsi
    ; ── Restore callee-saved registers of new process ─────
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; ── Return into the new process ───────────────────────
    ; The return address on the new stack is where that
    ; process last called context_switch() — so it resumes
    ; right after the call, as if it never stopped.
    ;
    ; For brand new processes, sched_spawn() sets up the
    ; stack so ret jumps to proc_entry() which calls fn().
    ret