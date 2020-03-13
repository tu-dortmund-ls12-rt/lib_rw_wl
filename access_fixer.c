#include <uk/config.h>
#include <stdio.h>

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY

extern unsigned long uk_so_wl_text_spare_vm_begin;
extern unsigned long uk_so_wl_text_spare_vm_size;

void uk_upper_level_instruction_abort_handler(unsigned long *register_stack) {
    unsigned long far;
    asm volatile("mrs %0,far_el1" : "=r"(far));

    unsigned long elr;
    asm volatile("mrs %0,elr_el1" : "=r"(elr));

    // 64 byte aligned
    if (far != (elr & ~(0x3F))) {
        printf(
            "ERROR: The fault was not caused by an instruction fetch, cannot "
            "handle (ELR is 0x%lx while FAR is 0x%lx)\n",
            elr, far);
        while (1)
            ;
    }

    // Check if instruction was valid at a time
    if (elr < CONFIG_SPARE_VM_BASE) {
        printf("ERROR: Aborted instruction could have been never valid\n");
        while (1)
            ;
    }

    printf("Detected instruction abort at address 0x%lx\n", elr);
    unsigned long offset =
        (elr - CONFIG_SPARE_VM_BASE) % (uk_so_wl_text_spare_vm_size * 0x1000);
    elr = uk_so_wl_text_spare_vm_begin + offset;
    printf("Fixed instruction to 0x%lx\n", elr);

    asm volatile("msr elr_el1,%0" ::"r"(elr));
}
#endif