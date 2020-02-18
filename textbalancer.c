#include <arm/arm64/mmu.h>
#include <uk_lib_so_wl/textbalancer.h>

#define CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

extern unsigned long uk_app_base;
extern unsigned long uk_app_text_size;
extern unsigned long uk_spiining_begin;
extern unsigned long uk_spinning_end;

void __WL_CODE uk_so_wl_tb_text_from_irq(unsigned long *saved_stack_base) {
    printf("Triggered Text rebalance\n");
    // Move the text first
    unsigned long *curr =
        (unsigned long *)(PLAT_MMU_VTEXT_BASE + uk_spinning_end - 0x1000);
    while ((unsigned long)curr >=
           PLAT_MMU_VTEXT_BASE + uk_spiining_begin - 0x1000) {
        unsigned long *target =
            (unsigned long
                 *)(((unsigned long)curr) +
                    CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP);
        // printf("Copying from 0x%lx to 0x%lx\n", curr, target);
        *target = *curr;
        curr--;
    }

    int will_wrap = 0;
    if (PLAT_MMU_VTEXT_BASE + uk_spiining_begin +
            CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP - 0x1000 >=
        PLAT_MMU_VTEXT_BASE + uk_app_text_size - 0x1000) {
        will_wrap = 1;
    }

    // Patch the register file, if any intermediate address calculation is in
    // progress right now
    for (unsigned long i = 0; i < 31; i++) {
        unsigned long lword = saved_stack_base[i];
        if (lword >= PLAT_MMU_VTEXT_BASE + uk_spiining_begin - 0x1000 &&
            lword < PLAT_MMU_VTEXT_BASE + uk_spinning_end - 0x1000) {
            printf("Reloc text register X%d (0x%lx)\n", i, lword);
            lword += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;
            if (will_wrap) {
                lword -= uk_app_text_size;
            }
            saved_stack_base[i] = lword;
        }
    }

    uk_spiining_begin += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;
    uk_spinning_end += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;

    // Relocate text references in binary
    extern unsigned long app_internal_text_offsets_size;
    extern unsigned long app_internal_text_offsets[];
    for (unsigned long i = 1; i <= app_internal_text_offsets_size; i++) {
        unsigned long *entry =
            (unsigned long *)(app_internal_text_offsets[i] + uk_app_base);
        *entry += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;
        if (will_wrap) {
            *entry -= uk_app_text_size;
        }
        printf("Entry %d at 0x%lx is now 0x%lx\n", i, entry, *entry);
    }

    extern unsigned long uk_so_wl_brk_instr;
    uk_so_wl_brk_instr += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;
    if (will_wrap) {
        uk_so_wl_brk_instr -= uk_app_text_size;
    }

    // Maybe most important, fix the PC
    unsigned long pc;
    asm volatile("mrs %0, elr_el1" : "=r"(pc));
    printf("PC was 0x%lx\n", pc);
    pc += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;
    if (will_wrap) {
        pc -= uk_app_text_size;
    }
    asm volatile("msr elr_el1, %0" ::"r"(pc));
    printf("PC is 0x%lx\n", pc);

    // Walk down the stack if something needs to be fixed
    unsigned long sp;
    asm volatile("mrs %0, sp_el0" : "=r"(sp));
    printf("Current sp is 0x%lx, while x29 points to 0x%lx\n", sp,
           saved_stack_base[29]);
    extern unsigned long __NVMSYMBOL__APPLICATION_STACK_END;
    unsigned long max_stack =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_STACK_END);
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
    // TODO adjust to real upper stack
    max_stack = PLAT_MMU_VSTACK_BASE + 2 * APPLICATION_STACK_SIZE;
#endif
    while (sp < max_stack) {
        unsigned long *word = (unsigned long *)(sp);
        if (*word >= PLAT_MMU_VTEXT_BASE &&
            *word < (PLAT_MMU_VTEXT_BASE + 2 * uk_app_text_size)) {
            printf("Adjusting SW 0x%lx at 0x%lx\n", *word, word);
            *word += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;
            if (will_wrap) {
                *word -= uk_app_text_size;
            }
        }
        sp += 8;
    }

    printf("New text base at 0x%lx\n", uk_spiining_begin);

    // Implement the wraparound
    if (will_wrap) {
        printf("TEXT Wraparound\n");
        uk_spiining_begin -= uk_app_text_size;
        uk_spinning_end -= uk_app_text_size;
    }
}
#endif