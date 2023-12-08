#include "syscall.h"

uint64_t sys_write(unsigned int fd, const char* buf, size_t count)
{
    
}

uint64_t sys_getpid()
{
    return current->pid;
}