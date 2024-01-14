#include "types.h"
#include "sbi.h"


struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5) 
{
	struct sbiret retval;

    __asm__ volatile (
        "mv a7,%[ext]\n"
        "mv a6,%[fid]\n"
        "mv a5,%[arg5]\n"
        "mv a4,%[arg4]\n"
        "mv a3,%[arg3]\n"
		"mv a2,%[arg2]\n"
		"mv a1,%[arg1]\n"
		"mv a0,%[arg0]\n"
		"ecall \n"
		"mv %[rev],a0 \n"
		"mv %[val],a1 \n"
		: [rev] "=r" (retval.error), [val] "=r" (retval.value)
		: [ext] "r" (ext), [fid] "r" (fid), [arg5] "r" (arg5), [arg4] "r" (arg4),
		[arg3] "r" (arg3), [arg2] "r" (arg2), [arg1] "r" (arg1), [arg0] "r" (arg0)
		:"memory"
	);


	return retval;
}
