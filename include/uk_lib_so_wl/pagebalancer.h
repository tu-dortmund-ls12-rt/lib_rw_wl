#pragma once

// #include <uk_lib_so_wl/rbtree.h>
// #include <arm/arm64/mmu.h>

// extern void *__NVMSYMBOL__APPLICATION_INIT_FINI_BEGIN;
// extern void *__NVMSYMBOL__APPLICATION_STACK_END;
// extern void *__NVMSYMBOL__APPLICATION_STACK_BEGIN;
// extern void *__NVMSYMBOL_SPARE_VM_PAGE_BEGIN;

// #define MAX_MANAGED_PAGES MONITOR_CAPACITY

// struct phys_page_handle {
//     uintptr_t phys_address;
//     uintptr_t mapped_vm_page;
//     uint64_t access_count;
// };

// class PageBalancer {
//    public:
//     static PageBalancer instance;
//     PageBalancer();

//     void enable_balancing();

//     void trigger_rebalance(void *vm_page);

//     uint64_t get_rebalance_count();

//    private:
//     struct RBTree<phys_page_handle>::node managed_pages[MAX_MANAGED_PAGES];

//     RBTree<phys_page_handle> aes_tree;

//     uint64_t rebalance_count = 0;

// #ifdef RESPECT_PROCESS_VARIATION

//     /**
//      * Size of each process variated domain in virtual memory pages
//      */
//     unsigned long domain_size = 7;
//     /**
//      * This is a factor, which is used as increase for a often written page. It
//      * originates from the write_counts for a the domains: 10050565, 8638776
//      * The second domain has a higher favtor, cause it has less endurance
//      */
//     unsigned long age_factors[3] = {100, 116,100};

// #endif
// #ifdef PRE_AGED_MEMORY
//     // Pre aging of 20*fft run

//     unsigned long pre_aging[20] = {0,
//                                    0,
//                                    0,
//                                    0,
//                                    0,
//                                    0,
//                                    0,
//                                    0,
//                                    0,
//                                    0,
//                                    0,
//                                    ((85 * 5 * 116) / 15),
//                                    ((149 * 5 * 116) / 15),
//                                    ((17 * 5 * 116) / 15),
//                                    0,
//                                    0,
//                                    0,
//                                    0,
//                                    0};

// #endif
// };
