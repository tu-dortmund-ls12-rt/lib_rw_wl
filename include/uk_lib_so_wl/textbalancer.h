#pragma once

#include <uk/config.h>

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_SPINNING
void uk_so_wl_sb_text_from_irq(unsigned long *saved_stack_base);
#endif