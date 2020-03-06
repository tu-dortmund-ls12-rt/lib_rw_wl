#include <arm/arm64/mmu.h>
#include <arm/arm64/pmc_64.h>
#include <gic/gic-v2.h>
#include <stdint.h>
#include <stdio.h>
#include <uk/assert.h>
#include <uk/plat/irq.h>
#include <uk_lib_so_wl/pagebalancer.h>
#include <uk_lib_so_wl/stackbalancer.h>
#include <uk_lib_so_wl/writemnitor.h>

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
unsigned long uk_so_wl_write_count
    [CONFIG_SOFTONLYWEARLEVELINGLIB_MONITOR_CAPACITY] __WL_DATA;
#endif

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
unsigned long uk_so_wl_read_count
    [CONFIG_SOFTONLYWEARLEVELINGLIB_MONITOR_CAPACITY] __WL_DATA;
#endif

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_LEVELING
#define UK_SO_WL_READ_APPROX_SCALER                         \
    ((CONFIG_SOFTONLYWEARLEVELINGLIB_READ_SAMPLING_RATE /   \
      CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_SAMPLING_RATE) / \
     2)

#else
#define UK_SO_WL_READ_APPROX_SCALER 1
#endif

// This defines the offset where the monitoring starts to listen, it is usually
// the first page of the application
unsigned long uk_so_wl_monitor_offset __WL_DATA = 0;
unsigned long uk_so_wl_number_pages __WL_DATA = 0;
unsigned long uk_so_wl_number_text_pages __WL_DATA = 0;
unsigned long uk_so_wl_stack_offset_pages __WL_DATA = 0;

unsigned int uk_so_wl_waiting_for_write __WL_DATA = 0;
unsigned int uk_so_wl_waiting_for_read __WL_DATA = 0;

unsigned long uk_so_wl_overflow_count __WL_DATA = 0;
unsigned long uk_so_wl_text_overflow_count __WL_DATA = 0;

void uk_so_wl_trap_next_instr();
void uk_so_wl_restore_brk_instr();

unsigned long uk_so_wl_brk_instr;
unsigned long uk_so_wl_brk_word;

volatile unsigned int uk_so_wl_pause_reloc = 1;

int tcounter = 0;

int __WL_CODE uk_so_wl_writemonitor_handle_overflow(void* arg) {
    // printf("Overflow\n");
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
    if (uk_so_wl_pause_reloc) {
        // printf("Wrong overflow\n");
        arm64_pmc_write_event_counter(
            0, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_SAMPLING_RATE);
        return 1;
    }
    // Figure out which counter overflowed
    unsigned int page_mode = 0;
    if (arm64_pmc_read_counter_overflow_bit(0)) {
        arm64_pmc_clear_counter_overflow_bit(0);
        // printf("Write overflow\n");

        page_mode = 1;
        uk_so_wl_waiting_for_write = 1;
        arm64_pmc_write_event_counter(
            0, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_SAMPLING_RATE);
    }
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
    if (arm64_pmc_read_counter_overflow_bit(1)) {
        arm64_pmc_clear_counter_overflow_bit(1);
        // printf("Read overflow\n");

        page_mode = 2;
        uk_so_wl_waiting_for_read = 1;
        arm64_pmc_write_event_counter(
            1, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_READ_SAMPLING_RATE);

        /**
         * Intructions fetches are not captured by the permission trap system
         * (anyway makes no sense since then only instruction fetches would
         * cause faults) but still count on the PMCs, luckily we can sample them
         * by the interrupted PC here
         */
        unsigned long pc;
        asm volatile("mrs %0, elr_el1" : "=r"(pc));
        pc &= ~(0xFFF);

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
        extern unsigned long uk_app_text_size;
        if (pc >= PLAT_MMU_VTEXT_BASE + uk_app_text_size) {
            pc -= uk_app_text_size;
        }
        unsigned long number = (pc - PLAT_MMU_VTEXT_BASE) >> 12;
#else
        unsigned long number = (pc - uk_so_wl_monitor_offset) >> 12;
#endif
        // printf("Increasing read count for text page %d\n",number);
        if (tcounter++ >= 4) {
            if (number < uk_so_wl_number_pages) {
                tcounter = 0;
                uk_so_wl_read_count[number]++;
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_LEVELING
                if (uk_so_wl_read_count[number] >=
                    CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_NOTIFY_THRESHOLD) {
                    // Notify Wear Leveling
                    // printf("Triggering estimated text reads to 0x%lx\n",pc);

                    uk_so_wl_pb_trigger_rebalance(pc);

                    uk_so_wl_read_count[number] = 0;
                }
#endif
            }
        }
    }
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
    uk_so_wl_writemonitor_set_page_mode(page_mode);
#endif

    // Signal everything was fine
    return 1;
}

void __WL_CODE
uk_upper_level_page_fault_handler_w(unsigned long* register_stack) {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
    // First disable the overflow interrupt to not mess up things here
    arm64_pmc_enable_overflow_interrupt(0, 0);
    uk_so_wl_writemonitor_set_page_mode(0);

    // Check if we are really waiting for a write
    if (uk_so_wl_waiting_for_write) {
        uk_so_wl_waiting_for_write = 0;
        // Get the causing address for the access fault
        unsigned long far_el1;
        asm volatile("mrs %0, far_el1" : "=r"(far_el1));
        far_el1 &= ~0xFFF;

        // Determine faulting page
        unsigned long number = (far_el1 - uk_so_wl_monitor_offset) >> 12;

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING
        if (far_el1 >= PLAT_MMU_VSTACK_BASE) {
            if ((far_el1 - PLAT_MMU_VSTACK_BASE) <
                CONFIG_APPLICATION_STACK_SIZE) {
                number = (far_el1 - PLAT_MMU_VSTACK_BASE) >> 12;
            } else {
                number = (far_el1 - PLAT_MMU_VSTACK_BASE -
                          CONFIG_APPLICATION_STACK_SIZE) >>
                         12;
            }
            number += uk_so_wl_stack_offset_pages;
        }
        uk_so_wl_overflow_count++;
        if (uk_so_wl_overflow_count >=
            CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_NOTIFY_THRESHOLD) {
            uk_so_wl_overflow_count = 0;
            uk_so_wl_sb_relocate_from_irq(register_stack);
        }
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
        extern unsigned long uk_app_text_size;
        if (far_el1 >= PLAT_MMU_VTEXT_BASE &&
            far_el1 < PLAT_MMU_VTEXT_BASE + 2 * uk_app_text_size) {
            if ((far_el1 - PLAT_MMU_VTEXT_BASE) < uk_app_text_size) {
                number = (far_el1 - PLAT_MMU_VTEXT_BASE) >> 12;
            } else {
                number =
                    (far_el1 - PLAT_MMU_VTEXT_BASE - uk_app_text_size) >> 12;
            }
            number += 0;
        }
        uk_so_wl_text_overflow_count++;
        if (uk_so_wl_text_overflow_count >=
            CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_NOTIFY_THRESHOLD) {
            uk_so_wl_text_overflow_count = 0;
            uk_so_wl_tb_text_from_irq(register_stack);
        }
#endif

        if (number < uk_so_wl_number_pages) {
            uk_so_wl_write_count[number] += 1 * UK_SO_WL_READ_APPROX_SCALER;
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_LEVELING
            if (uk_so_wl_write_count[number] >=
                CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_NOTIFY_THRESHOLD) {
                // Notify Wear Leveling

                uk_so_wl_pb_trigger_rebalance(far_el1);

                uk_so_wl_write_count[number] = 0;
            }
#endif
        }
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
        arm64_pmc_write_event_counter(
            0, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_SAMPLING_RATE);
#endif
    }

    // if we are still waiting for a read, only allow a single instruction
    if (uk_so_wl_waiting_for_read) {
        uk_so_wl_trap_next_instr();
    }

    arm64_pmc_enable_overflow_interrupt(0, 1);
#endif
}

void __WL_CODE
uk_upper_level_page_fault_handler_r(unsigned long* register_stack) {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
    // First disable the overflow interrupt to not mess up things here
    arm64_pmc_enable_overflow_interrupt(1, 0);
    uk_so_wl_writemonitor_set_page_mode(0);

    // Check if we are really waiting for a read
    if (uk_so_wl_waiting_for_read) {
        uk_so_wl_waiting_for_read = 0;
        // Get the causing address for the access fault
        unsigned long far_el1;
        asm volatile("mrs %0, far_el1" : "=r"(far_el1));
        far_el1 &= ~0xFFF;

        // Determine faulting page
        unsigned long number = (far_el1 - uk_so_wl_monitor_offset) >> 12;

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING
        if (far_el1 >= PLAT_MMU_VSTACK_BASE) {
            if ((far_el1 - PLAT_MMU_VSTACK_BASE) <
                CONFIG_APPLICATION_STACK_SIZE) {
                number = (far_el1 - PLAT_MMU_VSTACK_BASE) >> 12;
            } else {
                number = (far_el1 - PLAT_MMU_VSTACK_BASE -
                          CONFIG_APPLICATION_STACK_SIZE) >>
                         12;
            }
            number += uk_so_wl_stack_offset_pages;
        }
        uk_so_wl_overflow_count++;
        if (uk_so_wl_overflow_count >=
            CONFIG_SOFTONLYWEARLEVELINGLIB_STACK_NOTIFY_THRESHOLD) {
            uk_so_wl_overflow_count = 0;
            uk_so_wl_sb_relocate_from_irq(register_stack);
        }
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
        extern unsigned long uk_app_text_size;
        if (far_el1 >= PLAT_MMU_VTEXT_BASE &&
            far_el1 < PLAT_MMU_VTEXT_BASE + 2 * uk_app_text_size) {
            // printf("RAccessing vtext page 0x%lx\n", far_el1);
            if ((far_el1 - PLAT_MMU_VTEXT_BASE) < uk_app_text_size) {
                number = (far_el1 - PLAT_MMU_VTEXT_BASE) >> 12;
            } else {
                number =
                    (far_el1 - PLAT_MMU_VTEXT_BASE - uk_app_text_size) >> 12;
            }
            number += 0;
        }
        uk_so_wl_text_overflow_count++;
        if (uk_so_wl_text_overflow_count >=
            CONFIG_SOFTONLYWEARLEVELINGLIB_TEXT_NOTIFY_THRESHOLD) {
            uk_so_wl_text_overflow_count = 0;
            uk_so_wl_tb_text_from_irq(register_stack);
        }
#endif

        if (number < uk_so_wl_number_pages) {
            uk_so_wl_read_count[number]++;
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_LEVELING
            if (uk_so_wl_read_count[number] >=
                CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_NOTIFY_THRESHOLD) {
                // Notify Wear Leveling

                uk_so_wl_pb_trigger_rebalance(far_el1);

                uk_so_wl_read_count[number] = 0;
            }
#endif
        }
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
        arm64_pmc_write_event_counter(
            1, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_READ_SAMPLING_RATE);
#endif
    }

    arm64_pmc_enable_overflow_interrupt(1, 1);
#endif
}

void uk_upper_level_breakpoint_handler(unsigned long* register_stack) {
    // fix the breakpoint
    uk_so_wl_restore_brk_instr();
    // if we are waiting for a read, set permissions
    if (uk_so_wl_waiting_for_read) uk_so_wl_writemonitor_set_page_mode(2);
}

void uk_so_wl_trap_next_instr() {
    unsigned long pc;
    asm volatile("mrs %0, elr_el1" : "=r"(pc));
    /**
     * This is perfectly allowed, since this function is only called for memory
     * faults, ie the elr_el1 pointer is a mem access instruction. Since
     * conditionals are not longer allowed in ARMv8, it cannot branche away,
     * thus we can simply modify the next instruction
     */
    pc += 8;
    unsigned long aligned_pc = pc & ~(0b111);
    int lower = (pc == aligned_pc);
    // printf("BRK will be at 0x%lx\n", pc);
    unsigned long* instr = (unsigned long*)aligned_pc;
    uk_so_wl_brk_instr = pc;
    // printf("FAR brk at 0x%lx\n", instr);
    uk_so_wl_brk_word = *instr;
    *instr &= lower ? ~(0xFFFFFFFF) : ~(0xFFFFFFFF00000000);
    *instr |=
        lower ? (0xD4200000) : (0xD420000000000000);  // Store a breakpoint here
}

void uk_so_wl_restore_brk_instr() {
    unsigned long pc;
    asm volatile("mrs %0, elr_el1" : "=r"(pc));
    if (pc != uk_so_wl_brk_instr) {
        // uk_pr_err("ERROR, breakpoint at unexpected location\n");
        while (1)
            ;
    }
    unsigned long aligned_pc = pc & ~(0b111);
    unsigned long* instr = (unsigned long*)aligned_pc;
    *instr = uk_so_wl_brk_word;
}

void __WL_CODE uk_so_wl_writemonitor_init() {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
    for (unsigned long i = 0;
         i < CONFIG_SOFTONLYWEARLEVELINGLIB_MONITOR_CAPACITY; i++) {
        uk_so_wl_write_count[i] = 0;
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
        uk_so_wl_read_count[i] = 0;
#endif
    }
#endif
    /**
     * This is a generic initialization of the PMC functionality
     */
    arm64_pmc_set_counters_enabled(1);
    arm64_pmc_set_event_counter_enabled(0, 1);
    arm64_pmc_set_count_event(0, ARM64_PMC_BUS_ACCESS_STORE);
    arm64_pmc_set_non_secure_el0_counting(0, 1);
    arm64_pmc_set_non_secure_el1_counting(0, 1);
    arm64_pmc_set_el1_counting(0, 0);
    arm64_pmc_set_el0_counting(0, 1);
    arm64_pmc_enable_overflow_interrupt(0, 1);

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
    arm64_pmc_set_event_counter_enabled(1, 1);
    arm64_pmc_set_count_event(1, ARM64_PMC_BUS_ACCESS_LOAD);
    arm64_pmc_set_non_secure_el0_counting(1, 1);
    arm64_pmc_set_non_secure_el1_counting(1, 1);
    arm64_pmc_set_el1_counting(1, 0);
    arm64_pmc_set_el0_counting(1, 1);
    arm64_pmc_enable_overflow_interrupt(1, 1);
#endif

    int rc = ukplat_irq_register(320, uk_so_wl_writemonitor_handle_overflow, 0);
    if (rc < 0) UK_CRASH("Failed to register PMC overflow interrupt handler\n");
    gic_enable_irq(320);
    gic_set_irq_prio(320, 0);

    asm volatile("msr daifclr, #0b1111");
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
    arm64_pmc_write_event_counter(
        0, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_SAMPLING_RATE);
#endif

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
    arm64_pmc_write_event_counter(
        1, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_READ_SAMPLING_RATE);
#endif
}

void __WL_CODE uk_so_wl_writemonitor_set_monitor_offset(unsigned long offset) {
    uk_so_wl_monitor_offset = offset;
}

void __WL_CODE
uk_so_wl_writemonitor_set_number_pages(unsigned long number_pages) {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
    if (number_pages > CONFIG_SOFTONLYWEARLEVELINGLIB_MONITOR_CAPACITY) {
        UK_CRASH(
            "Too many pages to monitor are requested. Requested %u, bot only "
            "%u are available. Please change the configuration accordingly.\n",
            number_pages, CONFIG_SOFTONLYWEARLEVELINGLIB_MONITOR_CAPACITY);
    }
    uk_so_wl_number_pages = number_pages;
#endif
}

void __WL_CODE uk_so_wl_writemonitor_set_text_size(unsigned long number_pages) {
    uk_so_wl_number_text_pages = number_pages;
}

void uk_so_wl_writemonitor_set_stack_offset(unsigned long number_pages) {
    uk_so_wl_stack_offset_pages = number_pages;
}

void __WL_CODE uk_so_wl_writemonitor_terminate() {
    gic_disable_irq(320);
    // Set observed pages logic
    arm64_pmc_enable_overflow_interrupt(0, 0);
    arm64_pmc_set_event_counter_enabled(0, 0);

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
    arm64_pmc_enable_overflow_interrupt(1, 0);
    arm64_pmc_set_event_counter_enabled(1, 0);
#endif
}

void __WL_CODE uk_so_wl_writemonitor_plot_results() {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING

    // Print out write counts for all approximated pages
    for (unsigned long i = 0; i < uk_so_wl_number_pages; i++) {
// Print out the address and the approximation
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_READ_MONITORING
        printf("%x R %u W %u\n", (uk_so_wl_monitor_offset + i * 0x1000),
               uk_so_wl_read_count[i], uk_so_wl_write_count[i]);
#else
        printf("%x W %u\n", (uk_so_wl_monitor_offset + i * 0x1000),
               uk_so_wl_write_count[i]);
#endif
    }
#endif
}

void __WL_CODE uk_so_wl_writemonitor_set_page_mode(int generate_interrupts) {
    // printf("No %d\n", uk_so_wl_number_text_pages);
    unsigned int permission;
    if (generate_interrupts == 0) {
        permission = PLAT_MMU_PERMISSION_RW_FROM_OS_USER;
    }
    if (generate_interrupts == 1) {
        permission = PLAT_MMU_PERMISSION_R_FROM_OS_USER;
    }
    if (generate_interrupts == 2) {
        permission = PLAT_MMU_PERMISSION_RW_FROM_OS;
    }

    for (unsigned long i = 0; i < uk_so_wl_number_pages; i++) {
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
        if (i < uk_so_wl_stack_offset_pages) {
#endif
#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
            if (i < uk_so_wl_number_text_pages) {
                // printf("Setting WP for 0x%lx\n",
                //        PLAT_MMU_VTEXT_BASE + (i)*0x1000);
                plat_mmu_set_access_permissions(
                    (PLAT_MMU_VTEXT_BASE) + (i)*0x1000, permission, 1);
            } else {
#endif
                // printf("Setting WP for 0x%lx\n",
                //        uk_so_wl_monitor_offset + i * 0x1000);
                plat_mmu_set_access_permissions(
                    uk_so_wl_monitor_offset + i * 0x1000, permission, 1);
#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
            }
#endif

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
        } else {
            plat_mmu_set_access_permissions(
                (PLAT_MMU_VSTACK_BASE) +
                    (i - uk_so_wl_stack_offset_pages) * 0x1000,
                permission, 1);
        }
#endif
    }
    // printf("\n");
}