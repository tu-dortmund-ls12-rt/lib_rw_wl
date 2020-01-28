#include <uk_lib_so_wl/pagebalancer.h>

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

struct uk_so_wl_rbtree_node
    managed_pages[uk_so_wl_pb_MAX_MANAGED_PAGES] __WL_DATA;

struct uk_so_wl_rbtree aes_tree __WL_DATA = {0, 0};

unsigned long rebalance_count __WL_DATA = 0;

unsigned char uk_so_wl_spare_page[4096] __attribute((aligned(0x1000)))
__WL_DATA;

extern unsigned long __NVMSYMBOL__APPLICATION_TEXT_BEGIN;
extern unsigned long __NVMSYMBOL__APPLICATION_STACK_END;

void __WL_CODE uk_so_wl_pb_initialize() {
    // Create nodes for all managed pages and add them to a tree
    unsigned long managed_pages_begin =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_TEXT_BEGIN);
    unsigned long managed_pages_end =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_STACK_END);

    for (unsigned long i = 0; i < uk_so_wl_pb_MAX_MANAGED_PAGES; i++) {
        uk_so_wl_rbtree_insert(&aes_tree, managed_pages + i);
        managed_pages_begin += 0x1000;
        if (managed_pages_begin == managed_pages_end) {
            break;
        }
    }
    if (managed_pages_begin < managed_pages_end) {
        printf("Not enough balancing capacity for the application pages");
        while (1)
            ;
    }
}

void __WL_CODE uk_so_wl_pb_trigger_rebalance(void *vm_page) {
    unsigned long managed_pages_begin =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_TEXT_BEGIN);
    // Get the target node out of the RBTree
    struct uk_so_wl_rbtree_phys_page_handle target =
        uk_so_wl_rbtree_pop_minimum(&aes_tree);
    // log("[RMAP] " << vm_page << "{" << MMU::instance.get_mapping(vm_page)
    //               << "} -> " << hex << target.phys_address << "{" << hex
    //               << target.mapped_vm_page << "}");

    if (target.mapped_vm_page == (unsigned long)vm_page) {
        // The page cannot be at any better physical location
        // Reinsert the page into the tree
        unsigned int array_offset =
            (target.phys_address - managed_pages_begin) / 0x1000;

        struct uk_so_wl_rbtree_node *self = managed_pages + array_offset;

        self->value.access_count++;

        uk_so_wl_rbtree_insert(&aes_tree, self);
        // TODO increase aec anyway
        return;
    }

    rebalance_count++;

    // Determine the current physical address
    unsigned long physical_address;
    { physical_address = (unsigned long)plat_mmu_get_pm_mapping(vm_page); }
    unsigned int array_offset =
        (physical_address - managed_pages_begin) / 0x1000;

    // log_info("[RMAP]: " << vm_page << "[" << hex << physical_address << "] ->
    // "
    //                     << hex << target.phys_address << "{" << hex
    //                     << target.mapped_vm_page << "}");

    // Swap both pages
    unsigned long former_vm = target.mapped_vm_page;
    managed_pages[array_offset].value.mapped_vm_page = former_vm;
    target.mapped_vm_page = (unsigned long)vm_page;

    // Exchange pagetables
    plat_mmu_set_pm_mapping(vm_page, target.phys_address);

    plat_mmu_set_pm_mapping(former_vm, physical_address);

    // Last copy the actual data
    unsigned long *n_page_ptr = (unsigned long *)vm_page;
    unsigned long *o_page_ptr = (unsigned long *)former_vm;
    unsigned long *spare = (unsigned long *)(&uk_so_wl_spare_page);

    for (unsigned long i = 0; i < 512; i++) {
        spare[i] = n_page_ptr[i];
    }

    for (unsigned long i = 0; i < 512; i++) {
        n_page_ptr[i] = o_page_ptr[i];
    }

    for (unsigned long i = 0; i < 512; i++) {
        o_page_ptr[i] = spare[i];
    }

    unsigned int target_array_offset =
        (target.phys_address - managed_pages_begin) / 0x1000;

    target.access_count++;
    managed_pages[target_array_offset].value = target;
    uk_so_wl_rbtree_insert(&aes_tree, managed_pages + target_array_offset);
}

unsigned long __WL_CODE uk_so_wl_pb_get_rebalance_count() {
    return rebalance_count;
}
