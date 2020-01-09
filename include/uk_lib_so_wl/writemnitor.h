/*
 * Filename: libs/uk_lib_so_wl/include/uk_lib_so_wl/writemnitor.h
 * Path: libs/uk_lib_so_wl/include/uk_lib_so_wl
 * Created Date: Monday, December 16th 2019, 11:38:18 am
 * Author: Christian Hakert
 *
 * This implementation provides a write monitor. This means, the write accesses
 * from the CPU are sampled by regular performance counter interruopts (counting
 * the write accesses). Subsequently, the access permissions of a page are set
 * to read only, to trigger an access permission violation trap on the next
 * write access. This is then collected as a statistical distribution of write
 * accesses
 */

#pragma once

void uk_so_wl_writemonitor_init();
void uk_so_wl_writemonitor_set_monitor_offset(unsigned long offset);
void uk_so_wl_writemonitor_set_number_pages(unsigned long number_pages);

void uk_so_wl_writemonitor_terminate();
void uk_so_wl_writemonitor_plot_results();
void uk_so_wl_writemonitor_set_page_mode(int generate_interrupts);