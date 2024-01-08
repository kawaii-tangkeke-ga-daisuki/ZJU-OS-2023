#include "syscall.h"
#include "defs.h"
#include "elf.h"
#include "string.h"
#include "mm.h"
#include "printk.h"

extern struct task_struct* current;
extern char ramdisk_start[];
extern char ramdisk_end[];
extern void clock_set_next_event(void);
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);

void do_page_fault(struct pt_regs* regs) {
    /*
     1. 通过 stval 获得访问出错的虚拟内存地址（Bad Address）
     2. 通过 find_vma() 查找 Bad Address 是否在某个 vma 中
     3. 分配一个页，将这个页映射到对应的用户地址空间
     4. 通过 (vma->vm_flags | VM_ANONYM) 获得当前的 VMA 是否是匿名空间
     5. 根据 VMA 匿名与否决定将新的页清零或是拷贝 uapp 中的内容
    */
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
        uint64_t src_uapp = (uint64_t)ramdisk_start + pgf_vm_area->vm_content_offset_in_file;
        uint64_t offset = stval - pgf_vm_area->vm_start;
        uint64_t src_uapp1 = PGROUNDDOWN(src_uapp + offset);

        for (int j = 0; j < sz; ++j) {
            ((char*)(pa))[j] = ((char*)src_uapp1)[j];  // Copy contents from the file
        }
    }

    create_mapping(current->pgd, va, pa - PA2VA_OFFSET, sz, perm);
}


void trap_handler(uint64 scause, uint64 sepc, struct pt_regs* regs) {
    if (scause >> 63){ // 通过 `scause` 判断trap类型
        if (scause % 8 == 5) { // 如果是interrupt 判断是否是timer interrupt
            // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
            printk("[S] Supervisor mode time interrupt!\n"); 
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
        } else if (syscall_id == SYS_CLONE)
        {
            ret = sys_clone(regs);
        } else {
            printk("[S] Unhandled syscall: %lx\n", syscall_id);
            while (1);
        }
        regs->x[10] = ret;
        regs->sepc += 4;
    } else if (scause == 12) {
        // inst page fault
        printk("[S] Instruction page fault.\n");
        printk("sepc: %lx, scause: %lx, stval: %lx.\n", csr_read(sepc), csr_read(scause), csr_read(stval));
        do_page_fault(regs);
    } else if (scause == 13) {
        // ld page fault
        printk("[S] LD page fault.\n");
        printk("sepc: %lx, scause: %lx, stval: %lx.\n", csr_read(sepc), csr_read(scause), csr_read(stval));
        do_page_fault(regs);
    } else if (scause == 15) {
        // sd/amo page fault
        printk("[S] SD/SMO page fault.\n");
        printk("sepc: %lx, scause: %lx, stval: %lx.\n", csr_read(sepc), csr_read(scause), csr_read(stval));
        do_page_fault(regs);
    } else {
        printk("[S] Unhandled trap, scause: %lx, sstatus: %lx, sepc: %lx\n", scause, regs->sstatus, regs->sepc);
        while (1);
    }
}
