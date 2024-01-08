#include "printk.h"
#include "sbi.h"
#include "defs.h"
#include "proc.h"

extern void test();

int start_kernel() {
    printk("[S-MODE] 2022");
    printk(" Hello RISC-V\n");
    printk("[S] Value of sstatus is %lx\n", csr_read(sstatus));

    schedule();
    test(); // DO NOT DELETE !!!

	return 0;
}
