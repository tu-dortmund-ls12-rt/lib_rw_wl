#include <arm/arm64/pmc_64.h>
#include <gic/gic-v2.h>
#include <stdint.h>
#include <stdio.h>
#include <uk/assert.h>
#include <uk/plat/irq.h>
#include <uk_lib_so_wl/writemnitor.h>

unsigned int irq_count = 0;

int uk_so_wl_writemonitor_handle_overflow(void* arg) {
    printf("Interrupt!\n");
    irq_count++;

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

    arm64_pmc_write_event_counter(0, 0xFFFFFFFF - 5);

    uint32_t v1 = arm64_pmc_read_event_counter(0);
    uint32_t p0 = gic_is_irq_pending(320);
    printf("TEST!\n");
    uint32_t v2 = arm64_pmc_read_event_counter(0);
    uint32_t p1 = gic_is_irq_pending(320);
    printf("TEST!\n");
    printf("TEST!\n");
    printf("V1 %u, V2 %u, IC %u\n", v1, v2, irq_count);
    printf("P0 %u, P1 %u\n", p0, p1);
}