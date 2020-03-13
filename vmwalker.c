#include <uk_lib_so_wl/vmwalker.h>
#include <uk/config.h>
#include <arm/arm64/mmu.h>

// #define CONFIG_MAP_SPARE_VM_SPACE

#ifdef CONFIG_MAP_SPARE_VM_SPACE

#define L1_SIZE ((0x1000000000000UL - CONFIG_SPARE_VM_BASE) >> 27)

unsigned long plat_mmu_sparevm_l1_table[L1_SIZE] __attribute((aligned(0x1000)));

// Two pages for l2 tables, each entry points to 2MB
unsigned long plat_mmu_sparevm_l2_table[0x2000] __attribute((aligned(0x1000)));

// Two pages for l3 tables, each entry points to 4KB
unsigned long plat_mmu_sparevm_l3_table[0x2000] __attribute((aligned(0x1000)));

/**
 * Some logic specific variables
 */
unsigned long uk_so_wl_text_spare_vm_begin = CONFIG_SPARE_VM_BASE;
unsigned long uk_so_wl_text_spare_vm_size = 0;

unsigned long uk_so_wl_set_spare_mapping(unsigned long valid_base,
                                         unsigned long invalud_base,
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

    unsigned long i_offset = invalud_base - CONFIG_SPARE_VM_BASE;
    unsigned long i_l1_offset = i_offset >> 30;

    unsigned long i_l2_offset = (i_offset % 0x40000000) >> 21;

    unsigned long i_l3_offset = (i_offset % 0x200000) >> 12;

    // Invalidate old L1 entires
    plat_mmu_sparevm_l1_table[i_l1_offset] = (0b00);
    plat_mmu_sparevm_l1_table[i_l1_offset + 1] = (0b00);

    // Valid new L1 entries
    plat_mmu_sparevm_l1_table[l1_offset] =
        (0b11) | ((unsigned long)(&plat_mmu_sparevm_l2_table));
    plat_mmu_sparevm_l1_table[l1_offset + 1] =
        (0b11) | (((unsigned long)(&plat_mmu_sparevm_l2_table)) + 0x1000);

    // Invalidate old L2 entires
    plat_mmu_sparevm_l2_table[i_l2_offset] = (0);
    plat_mmu_sparevm_l2_table[i_l2_offset + 1] = (0);

    // Valid new L2 entries
    plat_mmu_sparevm_l2_table[l2_offset] =
        (0b11) | (((unsigned long)(&plat_mmu_sparevm_l3_table)) + 0x0);
    plat_mmu_sparevm_l2_table[l2_offset + 1] =
        (0b11) | (((unsigned long)(&plat_mmu_sparevm_l3_table)) + 0x1000);

    // Valid new L3 entries (copy from old)
    // Map the real pages
    for (unsigned long i = 0; i < number_valid_pages; i++) {
        plat_mmu_sparevm_l3_table[l3_offset + i] =
            0b11101100111 |
            (plat_mmu_sparevm_l3_table[i_l3_offset + i] & (0xFFFFFFFFF000));
    }
    // Invalidate old L3 entires
    if (valid_base != invalud_base) {
        for (unsigned long i = 0; i < number_valid_pages; i++) {
            plat_mmu_sparevm_l3_table[i_l3_offset + i] = (0);
        }
    }
    plat_mmu_flush_tlb();
    return (unsigned long)(plat_mmu_sparevm_l3_table + l3_offset);
}

void uk_so_wl_invalidate_all() {
    for (unsigned long i = 0; i < 1024; i++) {
        // Make it a block descriptor with no access permissions from EL0
        plat_mmu_sparevm_l2_table[i] = 0b00;
    }
    for (unsigned long i = 0; i < 1024; i++) {
        // Make it a block descriptor with no access permissions from EL0
        plat_mmu_sparevm_l3_table[i] = 0b00;
    }
}

#endif