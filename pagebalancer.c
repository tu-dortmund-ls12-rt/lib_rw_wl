#include <uk_lib_so_wl/pagebalancer.h>

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

struct uk_so_wl_rbtree_node
    managed_pages[uk_so_wl_pb_MAX_MANAGED_PAGES] __WL_DATA;

struct uk_so_wl_rbtree aes_tree __WL_DATA = {0, 0};

unsigned long rebalance_count __WL_DATA = 0;

unsigned char uk_so_wl_spare_page[4096] __attribute((aligned(0x1000)))
__WL_DATA;

extern unsigned long __NVMSYMBOL__APPLICATION_DATA_BEGIN;
extern unsigned long __NVMSYMBOL__APPLICATION_STACK_END;

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
extern unsigned long uk_app_base;
extern unsigned long uk_text_begin;
extern unsigned long uk_text_end;
extern unsigned long uk_data_begin;

extern unsigned long uk_app_text_size;
#endif

void __WL_CODE uk_so_wl_pb_initialize() {
    // Create nodes for all managed pages and add them to a tree
    unsigned long managed_pages_begin =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_DATA_BEGIN) + 0x1000;
    unsigned long managed_pages_end =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_STACK_END);

    for (unsigned long i = 0; i < uk_so_wl_pb_MAX_MANAGED_PAGES; i++) {
        managed_pages[i].value.mapped_vm_page = managed_pages_begin;
        managed_pages[i].value.phys_address = managed_pages_begin;
        managed_pages[i].value.access_count = 0;
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
        (unsigned long)(&__NVMSYMBOL__APPLICATION_DATA_BEGIN) + 0x1000;
    // Get the target node out of the RBTree
    struct uk_so_wl_rbtree_phys_page_handle target =
        uk_so_wl_rbtree_pop_minimum(&aes_tree);
    // printf("[RMAP] 0x%lx {0x%lx} -> 0x%lx {0x%lx}\n", vm_page,
    //        plat_mmu_get_pm_mapping(vm_page), target.phys_address,
    //        target.mapped_vm_page);

    unsigned long real_vm = (unsigned long)vm_page;

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
    unsigned long text_begin_base = PLAT_MMU_VTEXT_BASE;
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
    extern unsigned long uk_so_wl_text_spare_vm_begin;
    text_begin_base = uk_so_wl_text_spare_vm_begin;
#endif
    if (real_vm >= text_begin_base &&
        real_vm < text_begin_base + 2 * uk_app_text_size) {
        if ((real_vm - text_begin_base < uk_app_text_size)) {
            vm_page = (void *)(real_vm - text_begin_base + uk_app_base +
                               uk_text_begin);
        } else {
            vm_page = (void *)(real_vm - text_begin_base - uk_app_text_size +
                               uk_app_base + uk_text_begin);
        }
        // printf("Adjusting text page from 0x%lx to 0x%lx\n", real_vm, vm_page);
    }
    if (real_vm >= text_begin_base + 2 * uk_app_text_size) {
        vm_page =
            (void *)(real_vm - text_begin_base + uk_app_base + uk_text_begin);
        // printf("Adjusting text page from 0x%lx to 0x%lx\n", real_vm, vm_page);
    }
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING
    extern unsigned long __NVMSYMBOL__APPLICATION_STACK_BEGIN;
    unsigned long stack_begin =
        (unsigned long)&__NVMSYMBOL__APPLICATION_STACK_BEGIN;
    extern unsigned long __NVMSYMBOL__APPLICATION_STACK_END;
    unsigned long stack_end =
        (unsigned long)&__NVMSYMBOL__APPLICATION_STACK_END;
    if (real_vm >= PLAT_MMU_VSTACK_BASE) {
        if ((real_vm - PLAT_MMU_VSTACK_BASE < CONFIG_APPLICATION_STACK_SIZE)) {
            vm_page = (void *)(real_vm - PLAT_MMU_VSTACK_BASE + stack_begin);
        } else {
            vm_page = (void *)(real_vm - PLAT_MMU_VSTACK_BASE -
                               CONFIG_APPLICATION_STACK_SIZE + stack_begin);
        }
    }
#endif

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
#if defined CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING || \
    defined CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
    { physical_address = (unsigned long)plat_mmu_get_pm_mapping(real_vm); }
    // printf("PM of HOT 0x%lx\n", physical_address);
#else
    { physical_address = (unsigned long)plat_mmu_get_pm_mapping(vm_page); }
#endif
    unsigned int array_offset =
        (physical_address - managed_pages_begin) / 0x1000;

    // log_info("[RMAP]: " << vm_page << "[" << hex << physical_address << "]
    // ->"
    //                     << hex << target.phys_address << "{" << hex
    //                     << target.mapped_vm_page << "}");

    // Swap both pages
    unsigned long former_vm = target.mapped_vm_page;
    managed_pages[array_offset].value.mapped_vm_page = former_vm;
    target.mapped_vm_page = (unsigned long)vm_page;

    unsigned long real_former_vm = (unsigned long)former_vm;
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING
    if (real_former_vm >= stack_begin && real_former_vm < stack_end) {
        former_vm =
            (void *)(real_former_vm - stack_begin + PLAT_MMU_VSTACK_BASE);
    }
    // printf("Remapping 0x%lx (fake 0x%lx) to 0x%lx (fake 0x%lx)\n", real_vm,
    //        vm_page, real_former_vm, former_vm);
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING

    if (real_former_vm >= uk_app_base + uk_text_begin &&
        real_former_vm < uk_app_base + uk_text_end) {
        former_vm = (void *)(real_former_vm - (uk_app_base + uk_text_begin) +
                             text_begin_base);
        // printf("Adjusting target text from 0x%lx to 0x%lx\n", real_former_vm,
        //        former_vm);
    }
    if (real_former_vm >= uk_app_base + uk_text_end &&
        real_former_vm < uk_app_base + uk_data_begin) {
        former_vm = (void *)(real_former_vm - (uk_app_base + uk_text_begin) +
                             text_begin_base + uk_app_text_size);
        // printf("Adjusting target got/plt from 0x%lx to 0x%lx\n", real_former_vm,
        //        former_vm);
    }
    // printf(
    //     "Remapping 0x%lx (fake 0x%lx) [0x%lx] to 0x%lx (fake 0x%lx) [0x%lx]\n",
    //     real_vm, vm_page, plat_mmu_get_pm_mapping(vm_page), real_former_vm,
    //     former_vm, plat_mmu_get_pm_mapping(former_vm));
#endif

// Exchange pagetables
#if defined CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING || \
    defined CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
    plat_mmu_set_pm_mapping(real_vm, target.phys_address);
    plat_mmu_set_pm_mapping(former_vm, physical_address);
#else
    plat_mmu_set_pm_mapping(vm_page, target.phys_address);
    plat_mmu_set_pm_mapping(former_vm, physical_address);
#endif

    // Last copy the actual data
    unsigned long *n_page_ptr;
    unsigned long *o_page_ptr;
#if defined CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING || \
    defined CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
    n_page_ptr = (unsigned long *)real_vm;
    o_page_ptr = (unsigned long *)former_vm;
#else
    n_page_ptr = (unsigned long *)vm_page;
    o_page_ptr = (unsigned long *)former_vm;
#endif

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
    // printf("\n");
}

unsigned long __WL_CODE uk_so_wl_pb_get_rebalance_count() {
    return rebalance_count;
}
