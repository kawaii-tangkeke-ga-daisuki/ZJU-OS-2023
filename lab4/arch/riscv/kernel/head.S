.extern start_kernel
.extern setup_vm
.extern setup_vm_final
.extern mm_init
.extern task_init
.extern _traps

.set PA2VA_OFFSET, 0xffffffdf80000000
.set _shift_left_satp_mode8, 0x3f
    .section .text.init
    .globl _start
_start:
    la sp, boot_stack_top # store the address of the stack top into the register sp
    li t0, PA2VA_OFFSET
    sub sp, sp, t0

    call setup_vm
    call relocate
    call mm_init
    call setup_vm_final
    call task_init
    
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

    #csrr t0,sstatus
    #ori t0,t0,0x2
    #csrw sstatus,t0
    # set sstatus[SIE] = 1

    jal start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    li t0, PA2VA_OFFSET
    add ra, ra, t0
    add sp, sp, t0

    # set satp with early_pgtbl
    la t2, early_pgtbl
    sub t2, t2, t0 # VA->PA
    srli t2, t2, 12 # PA->PPN
    # t0 = MODE8
    addi t0, x0, 1
    li t1, 63
    sll t0, t0, t1
    or t2, t2, t0
    csrw satp, t2
    
    # flush tlb
    sfence.vma zero, zero
		
    # flush icache
    fence.i
		
    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size(4KB)

    .globl boot_stack_top
boot_stack_top:
