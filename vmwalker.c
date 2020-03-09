#include <uk_lib_so_wl/vmwalker.h>
#include <uk/config.h>

#ifdef CONFIG_MAP_SPARE_VM_SPACE

unsigned long
    plat_mmu_sparevm_l1_table[(0x1000000000000UL - CONFIG_SPARE_VM_BASE) >> 39];

#endif