#pragma once

void uk_so_wl_init_wl_system(unsigned long app_base, unsigned long text_begin,
                             unsigned long text_end, unsigned long data_begin,
                             unsigned long data_end, unsigned long bss_begin,
                             unsigned long bss_end,
                             unsigned long spiining_begin,
                             unsigned long spinning_end);
void uk_so_wl_exit_wl_system();

void uk_so_wl_start_benchmark(void (*entry)());