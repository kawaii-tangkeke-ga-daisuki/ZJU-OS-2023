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
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组

void task_init() {
    test_init(NR_TASKS);
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle
    idle = (struct task_struct*)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->pid = 0;
    current = task[0] = idle;

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址
    for(int i = 1;i < NR_TASKS;i++){
        task[i] = (struct task_struct*)kalloc(); 
        task[i]->state = TASK_RUNNING;
        task[i]->counter = task_test_counter[i]; 
        task[i]->priority = task_test_priority[i]; 
        task[i]->pid = i;
        task[i]->thread.ra = (uint64)&__dummy;
        task[i]->thread.sp = (uint64)task[i] + PGSIZE;
    }
    #define OFFSET(TYPE , MEMBER) ((unsigned long)(&(((TYPE *)0)->MEMBER)))

    const uint64 OffsetOfThreadInTask = (uint64)OFFSET(struct task_struct, thread);
    const uint64 OffsetOfRaInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, ra);
    const uint64 OffsetOfSpInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, sp);
    const uint64 OffsetOfSInTask = OffsetOfThreadInTask+(uint64)OFFSET(struct thread_struct, s);
    printk("%d\n",OffsetOfRaInTask);
    printk("%d\n",OffsetOfSpInTask);
    printk("%d\n",OffsetOfSInTask);

    printk("...proc_init done!\n");
}

// arch/riscv/kernel/proc.c
void dummy() {
    schedule_test();
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
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
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
            printk("switch to [PID = %d PRIORITY = %d COUNTER = %d]\n", next, task[next]->priority, task[next]->counter);
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
        printk("switch to [PID = %d COUNTER = %d]\n", task[selected_task_id]->pid, task[selected_task_id]->counter);
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
        printk("switch to [PID = %d COUNTER = %d]\n", task[selected_task_id]->pid, task[selected_task_id]->counter);
        switch_to(task[selected_task_id]);
    } else {
        // No task to schedule
        printk("No runnable tasks with remaining time, system is idle or re-schedule\n");
    }
    #endif
}
