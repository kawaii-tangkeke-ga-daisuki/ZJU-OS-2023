.extern start_kernel

    .section .text.init
    .globl _start
_start:
    la t0,_traps
    csrw stvec,t0
    # set stvec = _traps

    csrr t0,sie
    ori t0,t0,0x20
    csrw sie,t0
    # set sie[STIE] = 1

    andi a7,x0,0x00
    andi a6,x0,0
    andi a5,x0,0
    andi a4,x0,0
    andi a3,x0,0
    andi a2,x0,0
    andi a1,x0,0
    li t0,10000000
    rdtime a0
    add a0,a0,t0
    ecall
    # set first time interrupt

    csrr t0,sstatus
    ori t0,t0,0x2
    csrw sstatus,t0
    # set sstatus[SIE] = 1

    la sp,boot_stack_top # store the address of the stack top into the register sp
    jal start_kernel
    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size(4KB)

    .globl boot_stack_top
boot_stack_top: