#define SYS_WRITE   64
#define SYS_GETPID  172

#include "proc.h"
#include "stddef.h"
#include "stdint.h"

extern struct task_struct* current;

uint64_t sys_write(unsigned int fd, const char* buf, size_t count);
uint64_t sys_getpid();