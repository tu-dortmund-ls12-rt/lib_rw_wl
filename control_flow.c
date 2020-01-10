#include <uk_lib_so_wl/control_flow.h>

#define __WL_CODE __attribute((section(".wl_text")))
#define __WL_DATA __attribute((section(".wl_data")))

unsigned long uk_so_wl_stored_sp __WL_DATA;

void __WL_CODE uk_so_wl_switch_to_irq_stack(void (*call_param)()) {
    // First figure out the irq stack
    extern unsigned long __irqstack_top;
    unsigned long irq_stack_top =
        (unsigned long)(&__irqstack_top);  // Resides on the current stack
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Jumping to stack 0x%x and call 0x%x\n", irq_stack_top, call_param);
#endif

    /**
     * According to arm, only X19-X29 are callee saved, which means X0-X18 and
     * X30 need to be backed up before the subprocedure call
     */
    asm volatile(
        "stp x0,x1,[sp,#-16]!;"
        "stp x2,x3,[sp,#-16]!;"
        "stp x4,x5,[sp,#-16]!;"
        "stp x6,x7,[sp,#-16]!;"
        "stp x8,x9,[sp,#-16]!;"
        "stp x10,x11,[sp,#-16]!;"
        "stp x12,x13,[sp,#-16]!;"
        "stp x14,x15,[sp,#-16]!;"
        "stp x16,x17,[sp,#-16]!;"
        "stp x18,x30,[sp,#-16]!;"
        ""
        "mov x0, sp;"
        "ldr x1, =uk_so_wl_stored_sp;"
        "str x0, [x1];"             // Store current sp
        "ldr x0, =__irqstack_top;"  // Switch sp
        "mov sp, x0;"
        "blr %0;"  // Call the target
        "ldr x1, =uk_so_wl_stored_sp;"
        "ldr x0, [x1];"  // Restore old sp
        "mov sp, x0;"
        ""
        "ldp x18,x30,[sp],#16;"
        "ldp x16,x17,[sp],#16;"
        "ldp x14,x15,[sp],#16;"
        "ldp x12,x13,[sp],#16;"
        "ldp x10,x11,[sp],#16;"
        "ldp x8,x9,[sp],#16;"
        "ldp x6,x7,[sp],#16;"
        "ldp x4,x5,[sp],#16;"
        "ldp x2,x3,[sp],#16;"
        "ldp x0,x1,[sp],#16;"

        :
        : "r"(call_param)
        : "x0", "x1");
}