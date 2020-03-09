#include <uk_lib_so_wl/vmwalker.h>
#include <uk/config.h>
#include <arm/arm64/mmu.h>

#define CONFIG_MAP_SPARE_VM_SPACE

#ifdef CONFIG_MAP_SPARE_VM_SPACE

#define L1_SIZE ((0x1000000000000UL - CONFIG_SPARE_VM_BASE) >> 27)

unsigned long plat_mmu_sparevm_l1_table[L1_SIZE];

// Two pages for l2 tables, each entry points to 2MB
unsigned long plat_mmu_sparevm_l2_table[0x2000];

// Two pages for l3 tables, each entry points to 4KB
unsigned long plat_mmu_sparevm_l3_table[0x2000];

unsigned long set_spare_mapping(unsigned long valid_base,
                                unsigned long number_valid_pages) {
    // Determine the L1 slot
    if (valid_base < CONFIG_SPARE_VM_BASE) {
        printf("ERROR: base must be larger than Spare VM Begin");
        while (1)
            ;
    }
    unsigned long offset = valid_base - CONFIG_SPARE_VM_BASE;
    unsigned long l1_offset = offset >> 30;

    unsigned long l2_offset = (offset % 0x40000000) >> 21;

    unsigned long l3_offset = (offset % 0x200000) >> 12;

    for (unsigned long i = 0; i < L1_SIZE; i++) {
        // Make it a block descriptor with no access permissions from EL0
        plat_mmu_sparevm_l1_table[i] = 0b01;
    }
    plat_mmu_sparevm_l1_table[l1_offset] =
        (0b11) | ((unsigned long)(&plat_mmu_sparevm_l2_table));
    plat_mmu_sparevm_l1_table[l1_offset + 1] =
        (0b11) | (((unsigned long)(&plat_mmu_sparevm_l2_table)) + 0x1000);

    for (unsigned long i = 0; i < 1024; i++) {
        // Make it a block descriptor with no access permissions from EL0
        plat_mmu_sparevm_l2_table[i] = 0b01;
    }
    plat_mmu_sparevm_l2_table[l2_offset] =
        (0b11) | (((unsigned long)(&plat_mmu_sparevm_l3_table)) + 0x0);
    plat_mmu_sparevm_l2_table[l2_offset + 1] =
        (0b11) | (((unsigned long)(&plat_mmu_sparevm_l3_table)) + 0x1000);

    for (unsigned long i = 0; i < 1024; i++) {
        // Make it a block descriptor with no access permissions from EL0
        plat_mmu_sparevm_l3_table[i] = 0b01;
    }

    // Map the real pages
    for (unsigned long i = 0; i < number_valid_pages; i++) {
        plat_mmu_sparevm_l3_table[l3_offset + i] =
            0b11101100111 | (valid_base + i * 4096);
    }
    plat_mmu_flush_tlb();
}

#endif