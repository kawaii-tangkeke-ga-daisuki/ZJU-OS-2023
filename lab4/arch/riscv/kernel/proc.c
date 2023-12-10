//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"

//arch/riscv/kernel/proc.c

extern void __dummy();

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
*/
//extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
//extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组

void initialize_process(struct task_struct* process, int pid) {
    process->pid = pid;
    process->state = TASK_RUNNING;
    process->counter = (pid == 0) ? 0 : task_test_counter[pid];
    process->priority = (pid == 0) ? 0 : task_test_priority[pid];
    process->thread.ra = (uint64)&__dummy;
    process->thread.sepc = USER_START;
    process->thread.sstatus = 0x40020; // SPP=0, SPIE=1, SUM=1
    process->thread.sscratch = USER_END;
    process->thread.sp = (uint64)process + PGSIZE;
    process->pgd = (pagetable_t)(alloc_page() - PA2VA_OFFSET);
    memcpy((void*)((uint64)process->pgd + PA2VA_OFFSET), (void*)swapper_pg_dir, PGSIZE);
}

void task_init() {
    test_init(NR_TASKS);
    idle = (struct task_struct*)kalloc();
    initialize_process(idle, 0); // 初始化 idle 进程
    current = idle;
    task[0] = idle;

    for (int i = 1; i < NR_TASKS; ++i) {
        task[i] = (struct task_struct*)kalloc();
        initialize_process(task[i], i); // 初始化其他进程
#ifdef SJF
        printk("SET [PID = %d COUNTER = %d]\n", i, task_test_counter[i]);
#endif
#ifdef PRIORITY
        printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", i, task_test_priority[i], task_test_counter[i]);
#endif
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
    if(next->pid == current->pid) {
        return;
    } else {
        struct task_struct* old_current = current;
        current = next;
        uint64 next_satp = (((uint64)(next->pgd))>>12) | (1L << 63);
        __switch_to(old_current, next, next_satp);
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
        if(task[i]->counter > 0) {
            is_all_zero = 0;
            break;
        }
    }

    // If all counters are zero, reset them to random values
    if(is_all_zero) {
        for(int i = 1; i < NR_TASKS; ++i) {
            task[i]->counter = rand();
            printk("SET [PID = %d COUNTER = %d]\n", i, task[i]->counter);
        }
    }

    // Find the task with the smallest id that is still running
    for(int i = 1; i < NR_TASKS; ++i) {
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
    int min_remaining_time = 1e10;
    int is_all_zero = 1;

    // Check if all running task counters are zero
    for(int i = 1; i < NR_TASKS; ++i) {
        if(task[i]->state == TASK_RUNNING && task[i]->counter > 0) {
            is_all_zero = 0;
            break;
        }
    }

    // If all running task counters are zero, reset them to random values
    if(is_all_zero) {
        for(int i = 1; i < NR_TASKS; ++i) {
            task[i]->counter = rand();
            printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
        }
    }

    // Find the running task with the smallest remaining time
    for(int i = 1; i < NR_TASKS; ++i) {
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
