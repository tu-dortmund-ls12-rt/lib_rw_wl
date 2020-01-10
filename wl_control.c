#include <stdio.h>
#include <uk_lib_so_wl/wl_control.h>
#include <uk_lib_so_wl/writemnitor.h>

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

void __WL_CODE uk_so_wl_init_wl_system() {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Initializing Software only wear leveling\n");
#endif

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
    extern unsigned long __NVMSYMBOL__APPLICATION_TEXT_BEGIN;
    extern unsigned long __NVMSYMBOL__APPLICATION_BSS_END;

    unsigned long start_monitoring =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_TEXT_BEGIN);
    unsigned long end_monitoring =
        (unsigned long)(&__NVMSYMBOL__APPLICATION_BSS_END);
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
    uk_so_wl_writemonitor_init();

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Initialized the write monitor\n");
#endif

#endif
}

void uk_so_wl_exit_wl_system() {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Stopping write monitoring\n");
#endif

    uk_so_wl_writemonitor_terminate();

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_PLOT_APPROXIMATION_RESULTS
    uk_so_wl_writemonitor_plot_results();
#endif
#endif
}