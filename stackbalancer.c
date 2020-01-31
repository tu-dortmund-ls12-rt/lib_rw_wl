#include <arm/arm64/mmu.h>
#include <uk_lib_so_wl/stackbalancer.h>

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING

unsigned long __current_stack_base_ptr =
    PLAT_MMU_VSTACK_BASE + (2 * CONFIG_APPLICATION_STACK_SIZE);

void uk_so_wl_sb_relocate_from_irq(unsigned long *saved_stack_base) {
    // Print the register stack
    // printf("X0: 0x%lx\n", saved_stack_base[0]);
    // printf("X1: 0x%lx\n", saved_stack_base[1]);
    // printf("X2: 0x%lx\n", saved_stack_base[2]);
    // printf("X3: 0x%lx\n", saved_stack_base[3]);
    // printf("X4: 0x%lx\n", saved_stack_base[4]);
    // printf("X5: 0x%lx\n", saved_stack_base[5]);
    // printf("X6: 0x%lx\n", saved_stack_base[6]);
    // printf("X7: 0x%lx\n", saved_stack_base[7]);
    // printf("X8: 0x%lx\n", saved_stack_base[8]);
    // printf("X9: 0x%lx\n", saved_stack_base[9]);
    // printf("X10: 0x%lx\n", saved_stack_base[10]);
    // printf("X11: 0x%lx\n", saved_stack_base[11]);
    // printf("X12: 0x%lx\n", saved_stack_base[12]);
    // printf("X13: 0x%lx\n", saved_stack_base[13]);
    // printf("X14: 0x%lx\n", saved_stack_base[14]);
    // printf("X15: 0x%lx\n", saved_stack_base[15]);
    // printf("X16: 0x%lx\n", saved_stack_base[16]);
    // printf("X17: 0x%lx\n", saved_stack_base[17]);
    // printf("X18: 0x%lx\n", saved_stack_base[18]);
    // printf("X19: 0x%lx\n", saved_stack_base[19]);
    // printf("X20: 0x%lx\n", saved_stack_base[20]);
    // printf("X21: 0x%lx\n", saved_stack_base[21]);
    // printf("X22: 0x%lx\n", saved_stack_base[22]);
    // printf("X23: 0x%lx\n", saved_stack_base[23]);
    // printf("X24: 0x%lx\n", saved_stack_base[24]);
    // printf("X25: 0x%lx\n", saved_stack_base[25]);
    // printf("X26: 0x%lx\n", saved_stack_base[26]);
    // printf("X27: 0x%lx\n", saved_stack_base[27]);
    // printf("X28: 0x%lx\n", saved_stack_base[28]);
    // printf("X29: 0x%lx\n", saved_stack_base[29]);
    // printf("X30: 0x%lx\n", saved_stack_base[30]);
    // printf("ELR_EL1: 0x%lx\n", saved_stack_base[31]);
    // printf("SPSR_EL1: 0x%lx\n", saved_stack_base[32]);
    // printf("ESR_El1: 0x%lx\n", saved_stack_base[33]);

    // Applications stack pointer
    unsigned long app_sp;
    asm volatile("mrs %0, sp_el0" : "=r"(app_sp));
    // printf("App sp was at 0x%lx\n", app_sp);

    unsigned long __virtual_stack_begin =
        PLAT_MMU_VSTACK_BASE + CONFIG_APPLICATION_STACK_SIZE;

    unsigned long __shadow_stack_begin = PLAT_MMU_VSTACK_BASE;

    // printf("Stack base is at 0x%lx, begin at 0x%lx and shadow at 0x%lx\n",
    //        __current_stack_base_ptr, __virtual_stack_begin,
    //        PLAT_MMU_VSTACK_BASE);

    int will_wrap = __current_stack_base_ptr <
                    (__virtual_stack_begin +
                     CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP);

    // Relocate stack pointer
    app_sp -= CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP;

    // Copy the old stack
    for (unsigned long target =
             app_sp + CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP;
         target < __current_stack_base_ptr; target += 8) {
        unsigned long lword = *((unsigned long *)(target));
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_POINTER_RELOCATION
        // Check if the word has to be relocated
        if (lword < __current_stack_base_ptr && lword > __shadow_stack_begin) {
            lword -= CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP;
        }
        // Check if the word is in the shadow
        if (will_wrap && lword < __virtual_stack_begin &&
            lword >= __shadow_stack_begin) {
            lword += CONFIG_APPLICATION_STACK_SIZE;
        }
#endif
        *((unsigned long
               *)(target -
                  CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP)) = lword;
    }

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_POINTER_RELOCATION
    // Relocate all the current applications registers
    for (unsigned long i = 0; i < 31; i++) {
        unsigned long lword = saved_stack_base[i];
        // Check if the word has to be relocated
        if (lword < __current_stack_base_ptr && lword > __shadow_stack_begin) {
            // printf("Relocating X%d from 0x%lx to 0x%lx\n", i, lword,
            //        lword - CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP);
            // lword -= CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP;
        }
        // Check if the word is in the shadow
        if (will_wrap && lword < __virtual_stack_begin &&
            lword >= __shadow_stack_begin) {
                // printf("Wrap\n");
            lword += CONFIG_APPLICATION_STACK_SIZE;
        }
        saved_stack_base[i] = lword;
    }
#else
    /**
     * Relocate at least X29, as it is the frame pointer
     */
    unsigned long lword = saved_stack_base[29];
    // Check if the word has to be relocated
    if (lword < __current_stack_base_ptr && lword > __shadow_stack_begin) {
        lword -= CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP;
    }
    // Check if the word is in the shadow
    if (will_wrap && lword < __virtual_stack_begin &&
        lword >= __shadow_stack_begin) {
        lword += CONFIG_APPLICATION_STACK_SIZE;
    }
    saved_stack_base[29] = lword;
#endif

    // Relocate the new stack base
    __current_stack_base_ptr -=
        CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_MOVEMENT_STEP;
    if (will_wrap) {
        __current_stack_base_ptr += CONFIG_APPLICATION_STACK_SIZE;
    }

    if (will_wrap && app_sp < __virtual_stack_begin) {
        app_sp += CONFIG_APPLICATION_STACK_SIZE;
    }

    // printf("Setting SP to 0x%lx\n", app_sp);
    // unsigned long elr_el1;
    // asm volatile("mrs %0, elr_el1":"=r"(elr_el1));
    // printf("Exception at 0x%lx\n",elr_el1);

    // Write back the stack pointer
    asm volatile("msr sp_el0, %0" ::"r"(app_sp));
    // log_info("Done IRQ reloc");
}
#endif