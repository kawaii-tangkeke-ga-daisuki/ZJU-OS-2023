#include "syscall.h"
#include "defs.h"
#include "elf.h"

typedef struct pt_regs {
  uint64 x[32];  //x0---x31
  uint64 sepc;
  uint64 sstatus;
  uint64 stval;//trap value
  uint64 sscratch;
  uint64 scause;
} pt_regs;

extern struct task_struct* current;
extern char uapp_start[];
extern char uapp_end[];

void page_fault_handler(pt_regs* regs) {
    uint64 stval = csr_read(stval);
    struct vm_area_struct* pgf_vm_area = find_vma(current, stval);
    if (pgf_vm_area == NULL) {
        printk("illegal address, run time error.\n");
        return;
    }

    uint64 va = PGROUNDDOWN(stval);
    uint64 sz = PGROUNDUP(stval + PGSIZE) - va;
    uint64 pa = alloc_pages(sz / PGSIZE);
    uint64 perm = !!(pgf_vm_area->vm_flags & VM_R_MASK) * PTE_R | 
                  !!(pgf_vm_area->vm_flags & VM_W_MASK) * PTE_W | 
                  !!(pgf_vm_area->vm_flags & VM_X_MASK) * PTE_X | 
                  PTE_U | PTE_V;
    memset((void*)pa, 0, sz);

    if (pgf_vm_area->vm_flags & VM_ANONYM) {
        // For anonymous mapping, the allocated space is already zeroed
    } else {
        // For file mapping
        uint64_t src_uapp = (uint64_t)uapp_start + pgf_vm_area->vm_content_offset_in_file;
        uint64_t offset = stval - pgf_vm_area->vm_start;
        uint64_t src_uapp1 = PGROUNDDOWN(src_uapp + offset);

        for (int j = 0; j < sz; ++j) {
            ((char*)(pa))[j] = ((char*)src_uapp1)[j];  // Copy contents from the file
        }
    }

    create_mapping(current->pgd, va, pa - PA2VA_OFFSET, sz, perm);
}


void trap_handler(uint64 scause, uint64 sepc, pt_regs* regs) {
    if (scause >> 63){ // 通过 `scause` 判断trap类型
        if (scause % 8 == 5) { // 如果是interrupt 判断是否是timer interrupt
            // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
            //printk("[S] Supervisor mode time interrupt!\n"); 
            clock_set_next_event();
            do_timer();
        }
    } else if (scause == 8) {
    uint64_t ret;
    uint64_t syscall_id = regs->x[17];

    if (syscall_id == SYS_GETPID) {
        ret = (uint64_t)sys_getpid();
    } else if (syscall_id == SYS_WRITE) {
        uint64_t fd = (uint64_t)regs->x[10];
        char* buffer = (char*)regs->x[11];
        size_t siz = (size_t)regs->x[12];

        ret = sys_write(fd, buffer, siz);
    } else {
        printk("[S] Unhandled syscall: %lx\n", syscall_id);
        while (1);
    }
    
    regs->x[10] = ret;
    regs->sepc += 4;
} else if (scause == 12) {
    // inst page fault
    printk("Instruction page fault.\n");
    page_fault_handler(regs);
} else if (scause == 13) {
    // ld page fault
    printk("LD page fault.\n");
    page_fault_handler(regs);
} else if (scause == 15) {
    // sd/amo page fault
    printk("SD/SMO page fault.\n");
    page_fault_handler(regs);
} else {
    printk("[S] Unhandled trap, scause: %lx, sstatus: %lx, sepc: %lx\n", scause, regs->sstatus, regs->sepc);
    while (1);
}

}
