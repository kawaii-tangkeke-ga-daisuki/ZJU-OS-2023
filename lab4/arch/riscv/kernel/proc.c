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

extern char uapp_start[];
extern char uapp_end[];
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
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)uapp_start;

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;
  
    Elf64_Phdr* phdr;
    int load_phdr_cnt = 0;
    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            // alloc space and copy content
            uint64_t offset = (uint64_t)(phdr->p_vaddr) - PGROUNDDOWN(phdr->p_vaddr);   //align to the page
            uint64_t size = PGROUNDUP(phdr->p_memsz + offset) / PGSIZE;
            uint64_t target_addr = alloc_pages(size);
            uint64_t src_start = (uint64_t)uapp_start + phdr->p_offset;
            for (int j = 0; j < phdr->p_memsz; ++j)
                ((char*)(target_addr + offset))[j] = ((char *)src_start)[j];
            memset((void *)(target_addr + offset + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
            
            // do mapping
            uint64_t perm = 0x11;
                perm |= (1 & phdr->p_flags) << 3;         //X
                perm |= (2 & phdr->p_flags) << 1;         //W
                perm |= (4 & phdr->p_flags) >> 1;         //R
            create_mapping(task->pgd, PGROUNDDOWN(phdr->p_vaddr), target_addr - PA2VA_OFFSET,
                phdr->p_memsz + offset, perm);
        }
    }
  
    // allocate user stack and do mapping
    uint64 U_stack_top = alloc_page();
    create_mapping(task->pgd, USER_END - PGSIZE, U_stack_top - PA2VA_OFFSET, PGSIZE, 23);
  
    // following code has been written for you
    // set user stack
    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // sstatus bits set
    // SPP=0, SPIE=1, SUM=1
    task->thread.sstatus = csr_read(sstatus);
    task->thread.sstatus &= ~(1 << 8);
    task->thread.sstatus |= 1 << 18;
    task->thread.sstatus |= 1 << 5;
    // user stack for user program
    task->thread.sscratch = USER_END;
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

        load_program(task[i]);
        
        // task[i]->thread.sepc = USER_START;
        // // SPP=0, SPIE=1, SUM=1
        // task[i]->thread.sstatus = csr_read(sstatus);
        // task[i]->thread.sstatus &= ~(1 << 8);
        // task[i]->thread.sstatus |= (1 << 5);
        // task[i]->thread.sstatus |= (1 << 18);
        // task[i]->thread.sscratch = USER_END;


        // //为用户栈分配空间
        // uint64_t U_stack_top = kalloc();

        // //拷贝二进制文件置进程专用的物理内存
        // uint64_t size = PGROUNDUP((uint64_t)uapp_end - (uint64_t)uapp_start) / PGSIZE;
        // uint64_t copy_addr = alloc_pages(size);
        // for (int j = 0; j < uapp_end - uapp_start; ++j) 
        //     ((char *)copy_addr)[j] = uapp_start[j];

        // create_mapping(task[i]->pgd, USER_START, (uint64)copy_addr - PA2VA_OFFSET,
        //     size * PGSIZE, 31); // 映射用户段   U|X|W|R|V
        // create_mapping(task[i]->pgd, USER_END - PGSIZE,
        //     U_stack_top - PA2VA_OFFSET, PGSIZE, 23); // 映射用户栈 U|-|W|R|V
    }
    // to get the offset of the members in the struct
    #define OFFSET(TYPE , MEMBER) ((unsigned long)(&(((TYPE *)0)->MEMBER)))

    const uint64 OffsetOfThreadInTask = (uint64)OFFSET(struct task_struct, thread);
    const uint64 OffsetOfRaInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, ra);
    const uint64 OffsetOfSpInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, sp);
    const uint64 OffsetOfSInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, s);
    const uint64 OffsetOfSepcInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, sepc);
    const uint64 OffsetOfSstatusInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, sstatus);
    const uint64 OffsetOfSscratchInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, sscratch);
    const uint64 OffsetOfPgdInTask = (uint64)OFFSET(struct task_struct, pgd);
    printk("%d\n",OffsetOfRaInTask);
    printk("%d\n",OffsetOfSpInTask);
    printk("%d\n",OffsetOfSInTask);
    printk("%d\n",OffsetOfSepcInTask);
    printk("%d\n",OffsetOfSstatusInTask);
    printk("%d\n",OffsetOfSscratchInTask);
    printk("%d\n",OffsetOfPgdInTask);

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