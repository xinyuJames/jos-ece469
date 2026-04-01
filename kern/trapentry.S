/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

// No error code pushed by CPU:
TRAPHANDLER_NOEC(t_divide,  T_DIVIDE)   // 0  divide error
TRAPHANDLER_NOEC(t_debug,   T_DEBUG)    // 1  debug exception
TRAPHANDLER_NOEC(t_nmi,     T_NMI)      // 2  non-maskable interrupt
TRAPHANDLER_NOEC(t_brkpt,   T_BRKPT)    // 3  breakpoint
TRAPHANDLER_NOEC(t_oflow,   T_OFLOW)    // 4  overflow
TRAPHANDLER_NOEC(t_bound,   T_BOUND)    // 5  bounds check
TRAPHANDLER_NOEC(t_illop,   T_ILLOP)    // 6  illegal opcode
TRAPHANDLER_NOEC(t_device,  T_DEVICE)   // 7  device not available

// Error code pushed by CPU:
TRAPHANDLER(t_dblflt,  T_DBLFLT)        // 8  double fault
TRAPHANDLER(t_tss,     T_TSS)           // 10 invalid TSS
TRAPHANDLER(t_segnp,   T_SEGNP)         // 11 segment not present
TRAPHANDLER(t_stack,   T_STACK)         // 12 stack exception
TRAPHANDLER(t_gpflt,   T_GPFLT)         // 13 general protection fault
TRAPHANDLER(t_pgflt,   T_PGFLT)         // 14 page fault

// No error code:
TRAPHANDLER_NOEC(t_fperr,   T_FPERR)    // 16 floating point error
TRAPHANDLER(t_align,   T_ALIGN)         // 17 alignment check (has error code)
TRAPHANDLER_NOEC(t_mchk,    T_MCHK)     // 18 machine check
TRAPHANDLER_NOEC(t_simderr, T_SIMDERR)  // 19 SIMD floating point error

TRAPHANDLER_NOEC(t_syscall, T_SYSCALL)  // 48 system call

// IRQ handlers (IRQ_OFFSET = 32)
TRAPHANDLER_NOEC(t_irq0,  IRQ_OFFSET+0)
TRAPHANDLER_NOEC(t_irq1,  IRQ_OFFSET+1)
TRAPHANDLER_NOEC(t_irq2,  IRQ_OFFSET+2)
TRAPHANDLER_NOEC(t_irq3,  IRQ_OFFSET+3)
TRAPHANDLER_NOEC(t_irq4,  IRQ_OFFSET+4)
TRAPHANDLER_NOEC(t_irq5,  IRQ_OFFSET+5)
TRAPHANDLER_NOEC(t_irq6,  IRQ_OFFSET+6)
TRAPHANDLER_NOEC(t_irq7,  IRQ_OFFSET+7)
TRAPHANDLER_NOEC(t_irq8,  IRQ_OFFSET+8)
TRAPHANDLER_NOEC(t_irq9,  IRQ_OFFSET+9)
TRAPHANDLER_NOEC(t_irq10, IRQ_OFFSET+10)
TRAPHANDLER_NOEC(t_irq11, IRQ_OFFSET+11)
TRAPHANDLER_NOEC(t_irq12, IRQ_OFFSET+12)
TRAPHANDLER_NOEC(t_irq13, IRQ_OFFSET+13)
TRAPHANDLER_NOEC(t_irq14, IRQ_OFFSET+14)
TRAPHANDLER_NOEC(t_irq15, IRQ_OFFSET+15)

/*
 * Challenge 2: Fast system call handler for sysenter/sysexit.
 *
 * Register convention on entry (set by user lib/fast_syscall):
 *   eax = syscall number
 *   edx = a1, ecx = a2, ebx = a3, edi = a4
 *   esi = user return EIP  (where sysexit should jump back)
 *   ebp = user return ESP  (what sysexit should restore to esp)
 *   esp = KSTACKTOP        (loaded from IA32_SYSENTER_ESP MSR by CPU)
 */
.globl sysenter_handler
.type sysenter_handler, @function
.align 2
sysenter_handler:
	pushl %ebp			/* save user return ESP on kernel stack */
	pushl %esi			/* save user return EIP on kernel stack */
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	pushl $0			/* a5 = 0 */
	pushl %edi			/* a4 */
	pushl %ebx			/* a3 */
	pushl %ecx			/* a2 */
	pushl %edx			/* a1 */
	pushl %eax			/* syscall number */
	call syscall
	addl $24, %esp			/* pop 6 arguments */
	popl %edx			/* edx = return EIP (sysexit: new EIP) */
	popl %ecx			/* ecx = return ESP (sysexit: new ESP) */
	movw $(GD_UD|3), %ax		/* restore user data segment */
	movw %ax, %ds
	movw %ax, %es
	sysexit

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	pushl %ds
	pushl %es
	pusha
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp
	call trap


