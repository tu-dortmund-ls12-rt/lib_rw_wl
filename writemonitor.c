#include <arm/arm64/pmc_64.h>
#include <uk/assert.h>
#include <uk/plat/irq.h>
#include <uk_lib_so_wl/writemnitor.h>

int uk_so_wl_writemonitor_handle_overflow(void* arg) {}

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

    arm64_pmc_write_event_counter(0, 0xFFFFFFFF - 64);
}