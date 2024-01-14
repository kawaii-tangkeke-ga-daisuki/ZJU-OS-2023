#include "syscall.h"
#include "printk.h"
#include "defs.h"
#include "mm.h"
#include "string.h"
#include "vm.h"
#include "fs.h"

extern struct task_struct* current;
extern void __ret_from_fork();
extern struct task_struct* task[NR_TASKS];
extern unsigned long swapper_pg_dir[512];

// arch/riscv/kernel/syscall.c

int64_t sys_close(int fd) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        target_file->opened = 0;
        ret = 0;
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}

int64_t sys_lseek(int fd, int64_t offset, int whence) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        /* todo: indirect call */
        if (target_file->perms & FILE_READABLE) 
            ret = target_file->lseek(target_file, offset, whence);
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}

int64_t sys_write(unsigned int fd, const char* buf, size_t count)
{
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        /* todo: indirect call */
        if (target_file->perms & FILE_WRITABLE) 
            ret = target_file->write(target_file, buf, count);
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}


// arch/riscv/kernel/syscall.c

int64_t sys_read(unsigned int fd, char* buf, uint64_t count) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        /* todo: indirect call */
        if (target_file->perms & FILE_READABLE)
            ret = target_file->read(target_file, buf, count);
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}

// arch/riscv/syscall.c

int64_t sys_openat(int dfd, const char* filename, int flags) {
    int fd = -1;

    // Find an available file descriptor first
    for (int i = 0; i < PGSIZE / sizeof(struct file); i++) {
        if (!current->files[i].opened) {
            fd = i;
            break;
        }
    }

    // Do actual open
    file_open(&(current->files[fd]), filename, flags);

    return fd;
}

uint64_t sys_getpid()
{
    return current->pid;
}

uint64_t sys_clone(struct pt_regs *regs) {
    // 先申请一个pid, 看看是否成功
    int pid = 0;
    while (pid < NR_TASKS)
        if (task[pid] == NULL) 
            break;
        else
            ++pid;
    if (pid == NR_TASKS) 
        return -1;

    // 1. 创建一个新的task
    struct task_struct* child_task;

    child_task = (struct task_struct*)kalloc();
    task[pid] = child_task;
    for (int i = 0; i < 1 << 12; ++i)
        ((char *)child_task)[i] = ((char *)current)[i];
    child_task->pid = pid;
    child_task->thread.ra = (uint64)__ret_from_fork;

    // 2. 计算出 child task 的对应的 pt_regs 的地址, 设置其中的值。
    uint64_t offset = (uint64_t)regs - (uint64_t)current;
    struct pt_regs *child_regs = (struct pt_regs *)((uint64_t)child_task + offset);
    child_regs->x[10] = 0;
    child_regs->x[2] = (uint64_t)child_regs;
    child_regs->sepc = regs->sepc + 4;

    child_task->thread.sp = (uint64_t)child_regs;

    // 3. 为 child task 分配一个根页表，并将swapper_pg_dir中的一级内核页表项复制进去
    child_task->pgd = (pagetable_t)alloc_page();
    memset(child_task->pgd, 0, 1 << 12);
    for (int j = 0; j < 512; ++j)
        child_task->pgd[j] = swapper_pg_dir[j];

    // 4. 根据 parent task 的页表和 vma 来分配并拷贝 child task 在用户态会用到的内存
    for (int i = 0; i < current->vma_cnt; ++i) {
        struct vm_area_struct vma = current->vmas[i];
        for (uint64_t vaddr = PGROUNDDOWN(vma.vm_start); vaddr < vma.vm_end; vaddr += PGSIZE) {
            if ((current->pgd[(vaddr >> 30) & 0x1ff] & (PTE_V)) == PTE_V) {
                uint64_t child_page = alloc_page();
                for (int j = 0; j < PGSIZE; ++j) 
                    ((char *)child_page)[j] = ((char *)vaddr)[j];
                create_mapping(child_task->pgd, vaddr, child_page - PA2VA_OFFSET, PGSIZE, vma.vm_flags | PTE_U | PTE_V);
            }
        }
    }
    printk("[S] New task: %d\n", pid);

    // 5. 返回子 task 的 pid
    return child_task->pid;
}