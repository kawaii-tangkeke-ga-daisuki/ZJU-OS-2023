#include "vm.h"
#include "virtio.h"

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long  early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void) {
    /* 
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表 
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。 
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */
    memset(early_pgtbl, 0x0, PGSIZE);

    int index = PHY_START >> 30 & 0x1ff;

    early_pgtbl[index] = (PHY_START >> 30 & 0x3fff) << 28 | 15;
    index = VM_START >> 30 & 0x1ff;
    early_pgtbl[index] = (PHY_START >> 30 & 0x3fff) << 28 | 15;

    return;
}

unsigned long  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern uint64 _skernel,_stext, _srodata, _sdata, _sbss;
void setup_vm_final(void) {
    memset(swapper_pg_dir, 0x0, PGSIZE);
    // No OpenSBI mapping required
    // mapping kernel text X|-|R|V
    create_mapping(swapper_pg_dir, (uint64)&_stext, (uint64)&_stext - PA2VA_OFFSET, (uint64)&_srodata - (uint64)&_stext, PTE_X | PTE_R | PTE_V);
    // mapping kernel rodata -|-|R|V
    create_mapping(swapper_pg_dir, (uint64)&_srodata, (uint64)&_srodata - PA2VA_OFFSET, (uint64)&_sdata - (uint64)&_srodata, PTE_R | PTE_V);
    // mapping other memory -|W|R|V
    create_mapping(swapper_pg_dir, (uint64)&_sdata, (uint64)&_sdata - PA2VA_OFFSET,PHY_SIZE - ((uint64)&_sdata - (uint64)&_stext), PTE_W | PTE_R | PTE_V);
    create_mapping(swapper_pg_dir, io_to_virt(VIRTIO_START), VIRTIO_START, VIRTIO_SIZE * VIRTIO_COUNT, PTE_W | PTE_R | PTE_V);
    // set satp with swapper_pg_dir
asm volatile (
        "mv t0, %[swapper_pg_dir]\n"

        ".set _pa2va_, 0xffffffdf80000000\n"
        "li t1, _pa2va_\n"
        "sub t0, t0, t1\n" //VA->PA
        "srli t0, t0, 12\n"
        "addi t2, zero, 1\n"
        "slli t2, t2, 31\n"
        "slli t2, t2, 31\n"
        "slli t2, t2, 1\n"
        "or t0, t0, t2\n"

        "csrw satp, t0\n"

        : : [swapper_pg_dir] "r" (swapper_pg_dir)
        : "memory"        
    );
//     YOUR CODE HERE

    // flush TLB
    asm volatile("sfence.vma zero, zero");
  
    // flush icache
    asm volatile("fence.i");
    return;
}


/**** 创建多级页表映射关系 *****/
/* 不要修改该接口的参数和返回值 */
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm) {
    uint64 VPN[3];
    uint64 *page_table[3];
    uint64 new_page;

    for (uint64 addr = va; addr < va + sz; addr += PGSIZE, pa += PGSIZE) {
        page_table[2] = pgtbl;

        // 为每个级别的页表计算VPN
        VPN[2] = (addr >> 30) & 0x1ff;
        VPN[1] = (addr >> 21) & 0x1ff;
        VPN[0] = (addr >> 12) & 0x1ff;

        // 检查并创建每个级别的页表项
        for (int level = 2; level > 0; level--) {
            if ((page_table[level][VPN[level]] & 1) == 0) {
                new_page = kalloc();
                page_table[level][VPN[level]] = (((new_page - PA2VA_OFFSET) >> 12) << 10) | 1;
            }
            page_table[level - 1] = (uint64 *)(((page_table[level][VPN[level]] >> 10) << 12) + PA2VA_OFFSET);
        }

        // 设置最后一级页表项
        page_table[0][VPN[0]] = (perm & 0x3ff) | ((pa >> 12) << 10);
    }
}

uint64_t virt_to_phys(uint64_t virt_addr)
{
    return virt_addr - PA2VA_OFFSET;
}
