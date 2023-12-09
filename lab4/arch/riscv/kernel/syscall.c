#include "syscall.h"

uint64_t sys_write(unsigned int fd, const char* buf, size_t count)
{
    uint64_t length = 0;
    if (fd == 1) {
        for (size_t i = 0; i < count; ++i) {
            length += (uint64_t)printk("%c", buf[i]);
        }
    }

    return length;
}

uint64_t sys_getpid()
{
    return current->pid;
}