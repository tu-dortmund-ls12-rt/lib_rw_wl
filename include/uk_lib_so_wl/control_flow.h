#pragma once

/**
 * This function changes the stack to the irq stack and calls a passed function
 * on this stack. After that one returns. the stack is set back to the original
 * stack and the function returns normally (at least i hope so)
 */
void uk_so_wl_switch_to_irq_stack(void (*call_param)());