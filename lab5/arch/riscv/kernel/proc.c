//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"
#include "string.h"
#include "elf.h"

//arch/riscv/kernel/proc.c

extern void __dummy();
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);

extern char ramdisk_start[];
extern char ramdisk_end[];
extern unsigned long  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
*/
//extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
//extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组

static uint64_t load_program(struct task_struct* task) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)ramdisk_start;

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr* phdr;
    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            uint64_t perm = 0;
            perm |= (phdr->p_flags & PF_X) ? VM_X_MASK : 0;
            perm |= (phdr->p_flags & PF_W) ? VM_W_MASK : 0;
            perm |= (phdr->p_flags & PF_R) ? VM_R_MASK : 0;

            do_mmap(task, phdr->p_vaddr, phdr->p_memsz, perm, phdr->p_offset, phdr->p_filesz);
        }
    }


    // Set up the rest of the task structure.
    task->thread.sepc = ehdr->e_entry;
    task->thread.sstatus = csr_read(sstatus);
    task->thread.sstatus &= ~(1 << 8);  // Clear SIE
    task->thread.sstatus |= (1 << 18);  // Set SUM
    task->thread.sstatus |= (1 << 5);   // Set SPIE
    task->thread.sscratch = USER_END;

    return ehdr->e_entry;
}

void task_init() {
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle
    idle = (struct task_struct*)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current = idle;
    task[0] = idle;

    for (int i = 1; i < NR_TASKS; ++i) { // 初始化其他进程
        task[i] = (struct task_struct*)kalloc();
        task[i]->pid = i;
        task[i]->state = TASK_RUNNING;
        task[i]->counter = 0;
        task[i]->priority = rand();
        task[i]->thread.ra = (uint64)&__dummy;
        task[i]->thread.sp = (uint64)task[i] + PGSIZE;        
        //创建进程自己的页表并拷贝
        task[i]->pgd = (pagetable_t)alloc_page();
        for (int j = 0; j < 512; ++j) 
            task[i]->pgd[j] = swapper_pg_dir[j];
        task[i]->vma_cnt = 0;
        do_mmap(task[i], USER_END - PGSIZE, PGSIZE, VM_R_MASK | VM_W_MASK | VM_ANONYM, 0, 0);

        task[i]->thread.sepc = load_program(task[i]);
    }
    printk("...proc_init done!\n");
}


// arch/riscv/kernel/proc.c
void dummy() {
    //schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if(current->counter == 1){
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. thread space begin at = %lx\n", current->pid, current);
        }
    }
}

// arch/riscv/kernel/proc.c

extern void __switch_to(struct task_struct* prev, struct task_struct* next);

void switch_to(struct task_struct* next) {
    if (current == next)
        return;
    else{
        struct task_struct *prev = current;
        current = next;
        __switch_to(prev,next);
    }
}

// arch/riscv/kernel/proc.c

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度

    if (current == idle) 
        schedule();
    else {
        if ((long)(--(current->counter)) > 0)
            return;
        else {
            current->counter = 0;
            schedule();
        }
    }
}

void schedule(){
    #ifdef PRIORITY    
    int c,i,next;
    static int isInitialized = 0;
    while (1) {
        c = -1;
		next = 0;
		i = NR_TASKS;
		while (--i) {
			if (!task[i])
			    continue;
			if (task[i]->state == TASK_RUNNING && (long)(task[i]->counter) > c)
				c = task[i]->counter, next = i;
		    }
		if (c) {
            printk("\nswitch to [PID = %d PRIORITY = %d COUNTER = %d]\n", next, task[next]->priority, task[next]->counter);
            break;
        }
		for(i = 1; i < NR_TASKS ; ++i)
			if (task[i]) {
				task[i]->counter = (task[i]->counter >> 1) + task[i]->priority / 10;
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", i, task[i]->priority, task[i]->counter);
            }
	}
    switch_to(task[next]);
    #endif    

    #ifdef order //select tasks in order,only for test!
    int selected_task_id = -1;
    int is_all_zero = 1;
    // Check if all task counters are zero
    for(int i = 1; i < NR_TASKS; ++i) {
        if (!task[i])
            continue;
        if(task[i]->counter > 0) {
            is_all_zero = 0;
            break;
        }
    }

    // If all counters are zero, reset them to random values
    if(is_all_zero) {
        for(int i = 1; i < NR_TASKS; ++i) {
            if (!task[i])
                continue;
            task[i]->counter = rand();
            printk("SET [PID = %d COUNTER = %d]\n", i, task[i]->counter);
        }
    }

    // Find the task with the smallest id that is still running
    for(int i = 1; i < NR_TASKS; ++i) {
        if (!task[i])
            continue;
        if(task[i]->state == TASK_RUNNING && task[i]->counter > 0) {
            selected_task_id = i;
            break;
        }
    }

    // If a task is found, switch to it
    if(selected_task_id != -1) {
        printk("\nswitch to [PID = %d COUNTER = %d]\n", task[selected_task_id]->pid, task[selected_task_id]->counter);
        switch_to(task[selected_task_id]);
    } else {
        // No task to schedule
        printk("No runnable tasks, system is idle or re-schedule\n");
    }
    #endif

    #ifdef SJF
    int selected_task_id = -1;
    int min_remaining_time = (int)1e10;
    int is_all_zero = 1;

    // Check if all running task counters are zero
    for(int i = 1; i < NR_TASKS; ++i) {
        if (!task[i])
            continue;
        if(task[i]->state == TASK_RUNNING && task[i]->counter > 0) {
            is_all_zero = 0;
            break;
        }
    }

    // If all running task counters are zero, reset them to random values
    if(is_all_zero) {
        for(int i = 1; i < NR_TASKS; ++i) {
            if (!task[i])
                continue;
            task[i]->counter = rand();
            printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
        }
    }

    // Find the running task with the smallest remaining time
    for(int i = 1; i < NR_TASKS; ++i) {
        if (!task[i])
            continue;
        if(task[i]->state == TASK_RUNNING && task[i]->counter > 0 && task[i]->counter < min_remaining_time) {
            min_remaining_time = task[i]->counter;
            selected_task_id = i;
        }
    }

    // If a task is found, switch to it
    if(selected_task_id != -1) {
        printk("\nswitch to [PID = %d COUNTER = %d]\n", task[selected_task_id]->pid, task[selected_task_id]->counter);
        switch_to(task[selected_task_id]);
    } else {
        // No task to schedule
        printk("No runnable tasks with remaining time, system is idle or re-schedule\n");
    }
    #endif
}

void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
    uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file){
        
    struct vm_area_struct temp;
    temp.vm_start = addr;
    temp.vm_end = addr + length;
    temp.vm_flags = flags;
    temp.vm_content_offset_in_file = vm_content_offset_in_file;
    temp.vm_content_size_in_file = vm_content_size_in_file; //在file中的大小

    task->vmas[task->vma_cnt++] = temp; 
}

struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr){
    struct vm_area_struct *tmp;
    for(int i = 0;i < task->vma_cnt;i++){
        tmp = & task->vmas[i]; 
        if( addr >= tmp->vm_start && addr <= tmp->vm_end) //满足地址范围条件
            return tmp; 
    }
}