    .section .text.entry
    .align 2
    .globl _traps 
.set PA2VA_OFFSET, 0xffffffdf80000000
_traps:
    csrr t0, sscratch
    beqz t0, _S_mode
    csrw sscratch, sp
    mv sp, t0   # switch stack from U-mode to S-mode

_S_mode:
    addi sp,sp,-37*8
    sd x0,0*8(sp)
    sd x1,1*8(sp)
    sd x2,2*8(sp)
    sd x3,3*8(sp)
    sd x4,4*8(sp)
    sd x5,5*8(sp)
    sd x6,6*8(sp)
    sd x7,7*8(sp)
    sd x8,8*8(sp)
    sd x9,9*8(sp)
    sd x10,10*8(sp)
    sd x11,11*8(sp)
    sd x12,12*8(sp)
    sd x13,13*8(sp)
    sd x14,14*8(sp)
    sd x15,15*8(sp)
    sd x16,16*8(sp)
    sd x17,17*8(sp)
    sd x18,18*8(sp)
    sd x19,19*8(sp)
    sd x20,20*8(sp)
    sd x21,21*8(sp)
    sd x22,22*8(sp)
    sd x23,23*8(sp)
    sd x24,24*8(sp)
    sd x25,25*8(sp)
    sd x26,26*8(sp)
    sd x27,27*8(sp)
    sd x28,28*8(sp)
    sd x29,29*8(sp)
    sd x30,30*8(sp)
    sd x31,31*8(sp)
    csrr t0, sepc
    sd t0, 32*8(sp)
    csrr t0, sstatus
    sd t0, 33*8(sp)
    csrr t0, stval
    sd t0, 34*8(sp)
    csrr t0, sscratch
    sd t0, 35*8(sp)
    csrr t0, scause
    sd t0, 36*8(sp)
    # save 32 registers and sepc to stack

    csrr a0, scause
    csrr a1, sepc
    mv a2, sp
    jal trap_handler
    # call trap_handler

    .global __ret_from_fork
__ret_from_fork:
    ld x0,0*8(sp)
    ld x1,1*8(sp)
    ld x3,3*8(sp)
    ld x4,4*8(sp)
    ld x5,5*8(sp)
    ld x6,6*8(sp)
    ld x7,7*8(sp)
    ld x8,8*8(sp)
    ld x9,9*8(sp)
    ld x10,10*8(sp)
    ld x11,11*8(sp)
    ld x12,12*8(sp)
    ld x13,13*8(sp)
    ld x14,14*8(sp)
    ld x15,15*8(sp)
    ld x16,16*8(sp)
    ld x17,17*8(sp)
    ld x18,18*8(sp)
    ld x19,19*8(sp)
    ld x20,20*8(sp)
    ld x21,21*8(sp)
    ld x22,22*8(sp)
    ld x23,23*8(sp)
    ld x24,24*8(sp)
    ld x25,25*8(sp)
    ld x26,26*8(sp)
    ld x27,27*8(sp)
    ld x28,28*8(sp)
    ld x29,29*8(sp)
    ld x30,30*8(sp)
    ld x31,31*8(sp)
    ld t0, 32*8(sp) 
    csrw sepc, t0
    ld t0, 33*8(sp)
    csrw sstatus, t0
    ld t0, 34*8(sp)
    csrw stval, t0
    ld t0, 35*8(sp)
    csrw sscratch, t0
    ld t0, 36*8(sp)
    csrw scause, t0
    ld x2,2*8(sp)
    addi x2,x2,37*8   
    # restore sepc and 32 registers (x2(sp) should be restore last) from stack

    csrr t0, sscratch
    beqz t0, __return
    csrw sscratch, sp
    mv sp, t0   # switch stack from S-mode to U-mode
    
__return:
    sret
    # return from trap

    .extern dummy
    .globl __dummy
__dummy:
    #la t0, 0
    #csrw sepc, t0
    csrr t0, sscratch
    csrw sscratch, sp
    mv sp, t0
    sret

    .globl __switch_to
__switch_to:
    # save state to prev process
    addi t0, a0, 48
    sd ra, 0*8(t0)
    sd sp, 1*8(t0)
    sd s0, 2*8(t0)
    sd s1, 3*8(t0)
    sd s2, 4*8(t0)
    sd s3, 5*8(t0)
    sd s4, 6*8(t0)
    sd s5, 7*8(t0)
    sd s6, 8*8(t0)
    sd s7, 9*8(t0)
    sd s8, 10*8(t0)
    sd s9, 11*8(t0)
    sd s10, 12*8(t0)
    sd s11, 13*8(t0)
    csrr t1, sepc
    sd t1, 14*8(t0)
    csrr t1, sstatus
    sd t1, 15*8(t0)
    csrr t1, sscratch
    sd t1, 16*8(t0)

    # restore state from next process
    addi t0, a1, 48
    ld ra, 0(t0)
    ld sp, 8(t0)
    ld s0, 2*8(t0)
    ld s1, 3*8(t0)
    ld s2, 4*8(t0)
    ld s3, 5*8(t0)
    ld s4, 6*8(t0)
    ld s5, 7*8(t0)
    ld s6, 8*8(t0)
    ld s7, 9*8(t0)
    ld s8, 10*8(t0)
    ld s9, 11*8(t0)
    ld s10, 12*8(t0)
    ld s11, 13*8(t0)
    ld t1, 14*8(t0)
    csrw sepc, t1
    ld t1, 15*8(t0)
    csrw sstatus, t1
    ld t1, 16*8(t0)
    csrw sscratch, t1

    # switch page table
    addi t0, zero, 1
    slli t0, t0, 63
    ld t1, 184(a1)
    li t2, PA2VA_OFFSET
    sub t1, t1, t2  #VA->PA
    srli t1, t1, 12 #get PPN
    or t0, t0, t1
    csrw satp, t0

    # flush tlb
    sfence.vma zero, zero
		
    # flush icache
    fence.i

    ret