#include <arm/arm64/pmc_64.h>
#include <gic/gic-v2.h>
#include <stdint.h>
#include <stdio.h>
#include <uk/assert.h>
#include <uk/plat/irq.h>
#include <uk_lib_so_wl/writemnitor.h>

unsigned long
    uk_so_wl_write_count[CONFIG_SOFTONLYWEARLEVELINGLIB_MONITOR_CAPACITY];

// This defines the offset where the monitoring starts to listen, it is usually
// the first page of the application
unsigned long uk_so_wl_monitor_offset = 0;

int uk_so_wl_writemonitor_handle_overflow(void* arg) {
    arm64_pmc_write_event_counter(
        0, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_SAMPLING_RATE);

    // Signal everything was fine
    return 1;
}

void uk_so_wl_writemonitor_init() {
    /**
     * This is a generic initialization of the PMC functionality
     */
    arm64_pmc_set_counters_enabled(1);
    arm64_pmc_set_event_counter_enabled(0, 1);
    arm64_pmc_set_count_event(0, ARM64_PMC_BUS_ACCESS_STORE);
    arm64_pmc_set_non_secure_el0_counting(0, 1);
    arm64_pmc_set_non_secure_el1_counting(0, 1);
    arm64_pmc_enable_overflow_interrupt(0, 1);

    int rc = ukplat_irq_register(320, uk_so_wl_writemonitor_handle_overflow, 0);
    if (rc < 0) UK_CRASH("Failed to register PMC overflow interrupt handler\n");
    gic_enable_irq(320);
    gic_set_irq_prio(320, 0);

    asm volatile("msr daifclr, #0b1111");

    arm64_pmc_write_event_counter(
        0, 0xFFFFFFFF - CONFIG_SOFTONLYWEARLEVELINGLIB_WRITE_SAMPLING_RATE);
}

void uk_so_wl_writemonitor_set_monitor_offset(unsigned long offset) {
    uk_so_wl_monitor_offset = offset;
}

void uk_so_wl_writemonitor_terminate() {
    gic_disable_irq(320);
    // Set observed pages logic
    arm64_pmc_enable_overflow_interrupt(0, 0);
    arm64_pmc_set_event_counter_enabled(0, 0);
}

void uk_so_wl_writemonitor_plot_results() {
    // Print out write counts for all approximated pages
    for (unsigned long i = 0;
         i < CONFIG_SOFTONLYWEARLEVELINGLIB_MONITOR_CAPACITY; i++) {
        // Print out the address and the approximation
        printf("%x %u\n", (uk_so_wl_monitor_offset + i * 0x1000),
               uk_so_wl_write_count[i]);
    }
}

void uk_so_wl_writemonitor_set_page_mode(int generate_interrupts){
    
}