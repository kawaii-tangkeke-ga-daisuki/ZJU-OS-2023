#ifndef _SYSCALL_H_K
#define _SYSCALL_H_K

#define SYS_OPENAT  56
#define SYS_CLOSE   57
#define SYS_LSEEK   62
#define SYS_READ    63
#define SYS_WRITE   64
#define SYS_GETPID  172
#define SYS_CLONE   220

#include "proc.h"
#include "stddef.h"
#include "stdint.h"

void file_open(struct file* file, const char* path, int flags);

int64_t sys_close(int fd);
int64_t sys_lseek(int fd, int64_t offset, int whence);
int64_t sys_write(unsigned int fd, const char* buf, uint64_t count);
int64_t sys_read(unsigned int fd, char* buf, uint64_t count);
int64_t sys_openat(int dfd, const char* filename, int flags);
uint64_t sys_getpid();
uint64_t sys_clone(struct pt_regs *regs);

#endif