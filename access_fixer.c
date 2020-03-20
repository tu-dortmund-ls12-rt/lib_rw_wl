#include <stdio.h>
#include <uk/config.h>

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY

extern unsigned long uk_so_wl_text_spare_vm_begin;
extern unsigned long uk_so_wl_text_spare_vm_size;

extern unsigned long uk_app_text_size;
extern unsigned long uk_app_got_size;

extern unsigned long uk_app_base;
extern unsigned long uk_spiining_begin;

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

struct armv8_ldr_instr {
    unsigned int rt : 5;
    unsigned int rn : 5;
    unsigned int imm : 12;
    unsigned int opcode2 : 10;
} __attribute((packed));

void uk_upper_level_data_abort_handler(unsigned long *register_stack) {
    unsigned long far;
    asm volatile("mrs %0,far_el1" : "=r"(far));

    unsigned long elr;
    asm volatile("mrs %0,elr_el1" : "=r"(elr));

    unsigned long p_far =
        (far - CONFIG_SPARE_VM_BASE) % (uk_so_wl_text_spare_vm_size * 0x1000);

    if (p_far >= 2 * uk_app_text_size &&
        p_far < 2 * uk_app_text_size + uk_app_got_size) {
        // printf("Valid abort to GOT/PLT found\n");
        if (elr >= uk_so_wl_text_spare_vm_begin &&
            elr < uk_so_wl_text_spare_vm_begin + 2 * uk_app_text_size) {
            // printf("Abort happened while app exec\n");
            unsigned long plain_instr =
                ((elr - CONFIG_SPARE_VM_BASE) %
                 (uk_so_wl_text_spare_vm_size * 0x1000)) +
                uk_app_base + 0x1000;
            volatile struct armv8_ldr_instr *instr =
                (volatile struct armv8_ldr_instr *)(plain_instr);
            // LDR with immediare and unsigned offset
            if (instr->opcode2 == 0b1111100101) {
                // printf("LDR found\n");
                unsigned long reg = instr->rn;
                unsigned long fvalue = register_stack[reg];
                if (fvalue >= CONFIG_SPARE_VM_BASE) {
                    unsigned long offset =
                        (fvalue - CONFIG_SPARE_VM_BASE) %
                        (uk_so_wl_text_spare_vm_size * 0x1000);
                    fvalue = uk_so_wl_text_spare_vm_begin + offset;
                    register_stack[reg] = fvalue;
                } else {
                    printf(
                        "ERROR: Faulting register does not conatin spare "
                        "value (0x%lx at X%d)\n",
                        fvalue, reg);
                    printf(
                        "PFAR is %d, while text ends at %d and got ends at "
                        "%d\n",
                        p_far, 2 * uk_app_text_size,
                        2 * uk_app_text_size + uk_app_got_size);
                    while (1)
                        ;
                }
            } else {
                unsigned int *instr_arr = ((unsigned int *)plain_instr);
                printf(
                    "ERROR: Invalid cannot parse current instruction type at "
                    "0x%lx (0x%x), plain instr is 0x%lx, far is 0x%lx, instr "
                    "at elr is 0x0\n",
                    elr, instr_arr[0], plain_instr, far);
                for (unsigned long i = 0; i < 128; i++) {
                    printf("0x%x\n", instr_arr[i]);
                }
                while (1)
                    ;
            }
        } else {
            printf("Abort did not happen in the app at (0x%lx)\n", elr);
            unsigned long plain_instr = elr;
            struct armv8_ldr_instr *instr =
                (struct armv8_ldr_instr *)(plain_instr);
            // LDR with immediare and unsigned offset
            if (instr->opcode2 == 0b1111100101) {
                // printf("LDR found\n");
                unsigned long reg = instr->rt;
                unsigned long fvalue = register_stack[reg];
                if (fvalue >= CONFIG_SPARE_VM_BASE) {
                    unsigned long offset =
                        (fvalue - CONFIG_SPARE_VM_BASE) %
                        (uk_so_wl_text_spare_vm_size * 0x1000);
                    fvalue = uk_so_wl_text_spare_vm_begin + offset;
                    register_stack[reg] = fvalue;
                } else {
                    printf(
                        "ERROR: Faulting register does not conatin spare "
                        "value\n");
                    while (1)
                        ;
                }
            } else {
                printf(
                    "ERROR: Invalid cannot parse current instruction type\n");
                while (1)
                    ;
            }
        }
    } else {
        printf("ERROR: Invalid abort on non PLT/GOT address\n");
        while (1)
            ;
    }
}
#endif