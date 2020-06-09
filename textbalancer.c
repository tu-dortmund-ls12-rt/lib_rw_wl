#include <arm/arm64/mmu.h>
#include <uk_lib_so_wl/textbalancer.h>
#include <uk_lib_so_wl/vmwalker.h>

// #define CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

extern unsigned long uk_app_base;
extern unsigned long uk_app_text_size;
extern unsigned long uk_spiining_begin;
extern unsigned long uk_spinning_end;

extern void uk_reloc_adjust_adrp(volatile unsigned int *instr,
                                 int number_pages);

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
#define SWITCHING_OLD_BASE old_spare_vm_base
#else
#define SWITCHING_OLD_BASE text_begin_base
#endif

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
#define SWITCHING_REST (uk_so_wl_text_spare_vm_size * 0x1000)
#else
#define SWITCHING_REST 0
#endif

void __WL_CODE uk_so_wl_tb_text_from_irq(unsigned long *saved_stack_base) {
    unsigned long text_begin_base = PLAT_MMU_VTEXT_BASE;
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
    extern unsigned long uk_so_wl_text_spare_vm_size;

    extern unsigned long uk_so_wl_text_spare_vm_begin;

    uk_so_wl_set_spare_mapping(
        uk_so_wl_text_spare_vm_begin + uk_so_wl_text_spare_vm_size * 0x1000,
        uk_so_wl_text_spare_vm_begin, uk_so_wl_text_spare_vm_size);
    unsigned long old_spare_vm_base = uk_so_wl_text_spare_vm_begin;
    uk_so_wl_text_spare_vm_begin += uk_so_wl_text_spare_vm_size * 0x1000;
    text_begin_base = uk_so_wl_text_spare_vm_begin;
#endif

    // printf("Triggered Text rebalance from 0x%lx to 0x%lx\n",
    //        text_begin_base - 0x1000 + uk_spiining_begin,
    //        text_begin_base - 0x1000 + uk_spinning_end);

    int will_wrap = 0;
    if (text_begin_base + uk_spiining_begin +
            CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP - 0x1000 >=
        text_begin_base + uk_app_text_size) {
        will_wrap = 1;
        // printf("Wrapping around");
    }

    // Move the text first
    unsigned int *curr =
        (unsigned int *)(text_begin_base + uk_spinning_end - 0x1000);
    // We dont need the last word doubled
    curr--;
    while ((unsigned long)curr >=
           text_begin_base + uk_spiining_begin - 0x1000) {
        unsigned int *target =
            (unsigned int *)(((unsigned long)curr) +
                             CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP);
        *target = *curr;
        // printf("Copying 0x%lx from 0x%lx to 0x%lx (0x%lx)\n", *curr, curr,
        //        target, *target);
        unsigned long target_p = ((unsigned long)target) >> 12;
        unsigned long src_p = ((unsigned long)curr) >> 12;
        if ((*target & 0x9f000000) == 0x90000000) {
            if (target_p != src_p) {
                // printf(
                //     "moving adrp instruction 0x%lx over page bound (%d to "
                //     "%d)\n",
                //     target, src_p, target_p);
                uk_reloc_adjust_adrp(target, -1 * (target_p - src_p));
            }
            if (will_wrap) {
                // printf("Wrapping  back\n");
                uk_reloc_adjust_adrp(target, uk_app_text_size >> 12);
            }
        }
        curr--;
    }

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
    // Patch at least X30, since this is the link register and definetly points
    // to text
    unsigned long lword = saved_stack_base[30];
    if (lword >= SWITCHING_OLD_BASE + uk_spiining_begin - 0x1000 &&
        lword < SWITCHING_OLD_BASE + uk_spinning_end - 0x1000) {
        // printf("Reloc text register X%d (0x%lx)", i, lword);
        lword +=
            CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP + SWITCHING_REST;
        if (will_wrap) {
            lword -= uk_app_text_size;
        }
        // printf(" to 0x%lx\n", lword);
        saved_stack_base[30] = lword;
    }
#else
    // Patch the register file, if any intermediate address calculation is in
    // progress right now
    for (unsigned long i = 0; i < 31; i++) {
        unsigned long lword = saved_stack_base[i];
        if (lword >= SWITCHING_OLD_BASE + uk_spiining_begin - 0x1000 &&
            lword < SWITCHING_OLD_BASE + uk_spinning_end - 0x1000) {
            // printf("Reloc text register X%d (0x%lx)", i, lword);
            lword += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP +
                     SWITCHING_REST;
            if (will_wrap) {
                lword -= uk_app_text_size;
            }
            // printf(" to 0x%lx\n", lword);
            saved_stack_base[i] = lword;
        }
    }
#endif

    uk_spiining_begin += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;
    uk_spinning_end += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP;

    // Relocate text references in binary
    // Can happen irregardless of consitency method
    extern unsigned long app_internal_text_offsets_size;
    extern unsigned long app_internal_text_offsets[];
    for (unsigned long i = 1; i <= app_internal_text_offsets_size; i++) {
        // Offset - 0x1000 because the binary has an initial VM page, which is
        // not mapped to the upper virtuals, +uk_app_text_size since the shadow
        // lays before the GOT/PLT
        unsigned long *entry =
            (unsigned long *)(app_internal_text_offsets[i] + text_begin_base +
                              uk_app_text_size - 0x1000);
        *entry +=
            CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP + SWITCHING_REST;
        if (will_wrap) {
            *entry -= uk_app_text_size;
        }
        // printf("Entry %d at 0x%lx is now 0x%lx\n", i, entry, *entry);
    }

    // Same
    extern unsigned long uk_so_wl_brk_instr;
    // Only fix the breakpoint if it is part of the text
    if (uk_so_wl_brk_instr >= SWITCHING_OLD_BASE + uk_spiining_begin - 0x1000 &&
        uk_so_wl_brk_instr < SWITCHING_OLD_BASE + uk_spinning_end - 0x1000) {
        uk_so_wl_brk_instr +=
            CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP + SWITCHING_REST;
        if (will_wrap) {
            uk_so_wl_brk_instr -= uk_app_text_size;
        }
    }

    // Maybe most important, fix the PC
    // Can be done anyway
    unsigned long pc;
    asm volatile("mrs %0, elr_el1" : "=r"(pc));
    if (pc >= SWITCHING_OLD_BASE &&
        pc < (SWITCHING_OLD_BASE + 2 * uk_app_text_size)) {
        // printf("PC was 0x%lx\n", pc);
        pc +=
            CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP + SWITCHING_REST;
        if (will_wrap) {
            pc -= uk_app_text_size;
        }
        asm volatile("msr elr_el1, %0" ::"r"(pc));
        // printf("PC is 0x%lx\n", pc);
    }

    // Walk down the stack if something needs to be fixed
    unsigned long sp;
    asm volatile("mrs %0, sp_el0" : "=r"(sp));

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
    // Walk down the stack frames to fix at least the saved link registers
    unsigned long *base_ptr = (unsigned long *)(saved_stack_base[29]);
    while (base_ptr != 0) {
        unsigned long lr = base_ptr[1];
        // printf("Reloc at 0x%lx from 0x%lx", base_ptr, lr);
        if (lr >= SWITCHING_OLD_BASE &&
            lr < (SWITCHING_OLD_BASE + 2 * uk_app_text_size)) {
            lr += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP +
                  SWITCHING_REST;
            if (will_wrap) {
                lr -= uk_app_text_size;
            }
        }
        // printf(" to 0x%lx\n", lr);
        base_ptr[1] = lr;
        base_ptr = (unsigned long *)(base_ptr[0]);
    }
#else

    extern unsigned long __NVMSYMBOL__APPLICATION_STACK_END;
    unsigned long max_stack =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_STACK_END);
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
    // TODO adjust to real upper stack (if not, contents may be shifted twice)
    extern unsigned long __current_stack_base_ptr;
    // max_stack = PLAT_MMU_VSTACK_BASE + 2 * CONFIG_APPLICATION_STACK_SIZE;
    max_stack = __current_stack_base_ptr;
#endif
    while (sp < max_stack) {
        unsigned long *word = (unsigned long *)(sp);
        if (*word >= SWITCHING_OLD_BASE &&
            *word < (SWITCHING_OLD_BASE + 2 * uk_app_text_size)) {
            // printf("Adjusting SW 0x%lx at 0x%lx", *word, word);
            *word += CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_MOVEMENT_STEP +
                     SWITCHING_REST;
            if (will_wrap) {
                *word -= uk_app_text_size;
            }
            // printf(" to 0x%lx\n", *word);
        }
        sp += 8;
    }
#endif

    // Implement the wraparound
    if (will_wrap) {
        // printf("TEXT Wraparound\n");
        uk_spiining_begin -= uk_app_text_size;
        uk_spinning_end -= uk_app_text_size;
    }

    // printf("New text base at 0x%lx\n", uk_spiining_begin);

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
    // printf("Text VM begins at 0x%lx\n", uk_so_wl_text_spare_vm_begin);
#endif

    extern void cache_maintenance();
    cache_maintenance();
}
#endif