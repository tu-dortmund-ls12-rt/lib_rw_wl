#pragma once

#include <arm/arm64/mmu.h>
#include <uk/config.h>
#include <uk_lib_so_wl/rbtree.h>

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_WRITE_MONITORING
#define uk_so_wl_pb_MAX_MANAGED_PAGES \
    CONFIG_SOFTONLYWEARLEVELINGLIB_MONITOR_CAPACITY
#else
#define uk_so_wl_pb_MAX_MANAGED_PAGES 0
#endif

void uk_so_wl_pb_initialize();

void uk_so_wl_pb_trigger_rebalance(void *vm_page);

unsigned long uk_so_wl_pb_get_rebalance_count();
