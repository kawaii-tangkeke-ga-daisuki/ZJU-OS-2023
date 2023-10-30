#include "printk.h"

extern void clock_set_next_event();

void trap_handler(unsigned long scause, unsigned long sepc) {
    if (scause >> 63){ // 通过 `scause` 判断trap类型
        if (scause % 8 == 5) { // 如果是interrupt 判断是否是timer interrupt
            // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
            printk("[S] Time interrupt!\n"); 
            clock_set_next_event();
        }
    }
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略
}