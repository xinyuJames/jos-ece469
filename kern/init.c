/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>


void
i386_init(void)
{
	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	// (gdb) x/12x ap
	// 0xf010ffd4:     0x01    0x00    0x00    0x00    0x03    0x00    0x00    0x00
	// 0xf010ffdc:     0x04    0x00    0x00    0x00
	// (gdb) x/s fmt
	// 0xf0101ab7:     "x %d, y %x, z %d\n"
	// int x = 1, y = 3, z = 4;
	// cprintf("x %d, y %x, z %d\n", x, y, z);

	// Lab 2 memory management initialization functions
	mem_init();

	// Lab 3 user environment initialization functions
	env_init();
	trap_init();

	// Challenge 2: set up sysenter/sysexit MSRs for fast system calls.
	// IA32_SYSENTER_CS (0x174): kernel code segment selector.
	//   sysexit uses CS+16 (GD_UT) and CS+24 (GD_UD) for user CS/SS.
	// IA32_SYSENTER_ESP (0x175): kernel stack top.
	// IA32_SYSENTER_EIP (0x176): address of sysenter_handler.
	extern void sysenter_handler(void);
	wrmsr(0x174, GD_KT);
	wrmsr(0x175, KSTACKTOP);
	wrmsr(0x176, (uint32_t) sysenter_handler);

#if defined(TEST)
	// Don't touch -- used by grading script!
	ENV_CREATE(TEST, ENV_TYPE_USER);
#else
	// Touch all you want.
	ENV_CREATE(user_divzero, ENV_TYPE_USER);
	
#endif // TEST*

	// We only have one user environment for now, so just run it.
	env_run(&envs[0]);
}


/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	asm volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
