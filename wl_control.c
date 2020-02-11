#include <arm/arm64/mmu.h>
#include <stdio.h>
#include <uk_lib_so_wl/control_flow.h>
#include <uk_lib_so_wl/wl_control.h>
#include <uk_lib_so_wl/writemnitor.h>

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

unsigned long uk_app_base __WL_DATA;
unsigned long uk_text_begin __WL_DATA;
unsigned long uk_text_end __WL_DATA;
unsigned long uk_data_begin __WL_DATA;
unsigned long uk_data_end __WL_DATA;
unsigned long uk_bss_begin __WL_DATA;
unsigned long uk_bss_end __WL_DATA;
unsigned long uk_spiining_begin __WL_DATA;
unsigned long uk_spinning_end __WL_DATA;

void __WL_CODE uk_so_wl_init_wl_system(
    unsigned long app_base, unsigned long text_begin, unsigned long text_end,
    unsigned long data_begin, unsigned long data_end, unsigned long bss_begin,
    unsigned long bss_end, unsigned long spiining_begin,
    unsigned long spinning_end) {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Initializing Software only wear leveling\n");
#endif
    uk_app_base = app_base;
    uk_text_begin = text_begin;
    uk_text_end = text_end;
    uk_data_begin = data_begin;
    uk_data_end = data_end;
    uk_bss_begin = bss_begin;
    uk_bss_end = bss_end;
    uk_spiining_begin = spiining_begin;
    uk_spinning_end = spinning_end;

    // Let the spinning skip the note section
    uk_spiining_begin = uk_text_begin;

    uk_so_wl_prepare_wl_code_permissions();

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING

    extern unsigned long __NVMSYMBOL__APPLICATION_STACK_BEGIN;
    extern unsigned long __NVMSYMBOL__APPLICATION_STACK_END;

    unsigned long start_monitoring = app_base + text_begin;
    unsigned long end_text = app_base + text_end;
    unsigned long start_stack =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_STACK_BEGIN);
    unsigned long end_monitoring =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_STACK_END);
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf(
        "Write monitoring is enabled, going to sample accesses from 0x%x to "
        "0x%x, "
        "with a sampling rate of %u\n",
        start_monitoring, end_monitoring,
        CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_SAMPLING_RATE);
#endif

    uk_so_wl_writemonitor_set_monitor_offset(start_monitoring);
    uk_so_wl_writemonitor_set_number_pages(
        (end_monitoring - start_monitoring) >> 12);

    uk_so_wl_writemonitor_set_text_size((end_text - start_monitoring) >> 12);

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_STACK_SPINNING
    uk_so_wl_writemonitor_set_stack_offset((start_stack - start_monitoring) >>
                                           12);
#endif

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_LEVELING
    uk_so_wl_pb_initialize();
#endif

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
    plat_mmu_setup_stack_pages();
#endif

#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
    plat_mmu_setup_text_pages();
#endif

    uk_so_wl_writemonitor_init();

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Initialized the write monitor\n");
#endif

#endif
}

void __WL_CODE uk_so_wl_exit_wl_system() {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Stopping write monitoring\n");
#endif

    uk_so_wl_writemonitor_terminate();

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_PLOT_APPROXIMATION_RESULTS
#ifndef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_LEVELING
    uk_so_wl_writemonitor_plot_results();
#endif
#endif
#endif
}

void (*uk_so_wl_global_entry_store)();

void __WL_CODE uk_so_wl_start_benchmark_el0() {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Switched down to EL0\n");
    unsigned long sp;
    asm volatile("mov %0, sp" : "=r"(sp));
    printf("EL0 sp is at 0x%lx\n", sp);
#endif

    uk_so_wl_global_entry_store();

    uk_so_wl_kick_to_el1();
}

void __WL_CODE uk_so_wl_start_benchmark_irq_stack() {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    unsigned long sp;
    asm volatile("mov %0, sp" : "=r"(sp));
    printf("Ececuting C on the IRQ stack (0x%x)\n", sp);
#endif

    // Now we are switching to EL0, the function actually will retun due to some
    // dirty hack

    uk_so_wl_switch_to_el0(uk_so_wl_start_benchmark_el0);
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    unsigned long current_el;
    asm volatile("mrs %0, currentel" : "=r"(current_el));
    printf("Returned from EL jump (now on on EL %d)\n", current_el >> 2);
#endif
}

void __WL_CODE uk_so_wl_prepare_wl_code_permissions() {
    unsigned long start = uk_app_base + uk_text_begin;
    unsigned long stop = uk_app_base + uk_text_end;

    for (unsigned long page = start; page < stop; page += 0x1000) {
        plat_mmu_set_access_permissions(page, PLAT_MMU_PERMISSION_RW_FROM_OS,
                                        1);
    }

    start = uk_app_base + uk_text_end;
    stop = uk_app_base + uk_bss_end;

    for (unsigned long page = start; page < stop; page += 0x1000) {
        plat_mmu_set_access_permissions(page,
                                        PLAT_MMU_PERMISSION_RW_FROM_OS_USER, 1);
    }
}

void __WL_CODE uk_so_wl_start_benchmark(void (*entry)()) {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    unsigned long sp;
    asm volatile("mov %0, sp" : "=r"(sp));
    printf("Switching execution to the interrupt stack, current sp is 0x%x\n",
           sp);
#endif

    uk_so_wl_global_entry_store = entry;

    uk_so_wl_switch_to_irq_stack(uk_so_wl_start_benchmark_irq_stack);

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    asm volatile("mov %0, sp" : "=r"(sp));
    printf("Returned from IRQ stack, back at 0x%x\n", sp);
#endif
}