#include <uk/arch/syscalls.h>
#include <uk/config.h>
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

unsigned long uk_so_wl_intermediate_el1_sp __WL_DATA;

UK_PLAT_SYSCALL_HANDLER(0x42) {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Execution is back at EL1, kicking to saved state\n");
#endif
    asm volatile("b uk_so_wl_exit_call_mark");
}

void __WL_CODE uk_so_wl_switch_to_el0(void (*call_param)()) {
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_LOGGING
    printf("Jumping to el0 and call 0x%x\n", call_param);
#endif

    UK_PLAT_REGISTER_SYSCALL(0x42);

    /**
     * According to arm, only X19-X29 are callee saved, which means X0-X18 and
     * X30 need to be backed up before the subprocedure call
     */
    asm volatile(
        "msr daifset, #0b1111;"  // Disable interrupts
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
        "ldr x1, =uk_so_wl_intermediate_el1_sp;"  // Store the old stack
                                                  // pointer. After finishing
                                                  // the execution on el0, the
                                                  // execution will be forced to
                                                  // continue here :D
        "mov x0, sp;"
        "str x0, [x1];"
        "ldr x0, =__appstack_top;"  // Store the application stack begin to the
                                    // el0 stack pointer
        "msr sp_el0, x0;"
        "msr elr_el1, %0;"
        "mrs x0, spsr_el1;"  // Configure a fake exception return, which goes
                             // back to el0 and uses the sp of el0
        "bic x0, x0, #0b111;"
        "msr spsr_el1, x0;"
        "eret;"  // This will never returm, however we will have a trap handler,
                 // which kicks to exactly this location, indeed we have a fake
                 // return here
        "uk_so_wl_exit_call_mark:"  // This is a faking, the sp is set back to a
                                    // previous state, so the ongoing ISR is
                                    // completely discarded. Execution continues
                                    // then as if it was never interrupted
        ""
        "ldr x1, =uk_so_wl_intermediate_el1_sp;"
        "ldr x0, [x1];"
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

void uk_so_wl_kick_to_el1() { asm volatile("svc #0x42"); }