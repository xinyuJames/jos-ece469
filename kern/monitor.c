// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#include <inc/mmu.h>
#include <kern/pmap.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

static int mon_showmappings(int argc, char **argv, struct Trapframe *tf);
static int mon_setperm(int argc, char **argv, struct Trapframe *tf);
static int mon_dumpva(int argc, char **argv, struct Trapframe *tf);
static int mon_dumppa(int argc, char **argv, struct Trapframe *tf);


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static int mon_continue(int argc, char **argv, struct Trapframe *tf);
static int mon_si(int argc, char **argv, struct Trapframe *tf);

// LAB 1: add your command to here...
static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },

	{ "showmappings", "Show VA->PA mappings and perm bits: showmappings va_start va_end", mon_showmappings },
	{ "setperm", "Change PTE perms: setperm va [+u|-u] [+w|-w] [+p|-p]", mon_setperm },
	{ "dumpva", "Dump memory by virtual address: dumpva va_start va_end", mon_dumpva },
	{ "dumppa", "Dump memory by physical address: dumppa pa_start pa_end", mon_dumppa },

	{ "continue", "Continue execution after a breakpoint", mon_continue },
	{ "si", "Single-step one instruction", mon_si },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// LAB 1: Your code here.
    // HINT 1: use read_ebp().
    // HINT 2: print the current ebp on the first line (not current_ebp[0])
	uint32_t ebp, eip, arg1, arg2, arg3, arg4, arg5;

	cprintf("Stack backtrace:\n");
	for (ebp=read_ebp(); ebp>0; ebp= *((uint32_t *)ebp))
	{
		struct Eipdebuginfo info = {0};
		// location of eip will be one location higher than ebp in mem
		eip = *((uint32_t *)(ebp + 4));
		// arguments will be higher than eip
		arg1 = *((uint32_t *)(ebp + 2*4));
		arg2 = *((uint32_t *)(ebp + 3*4));
		arg3 = *((uint32_t *)(ebp + 4*4));
		arg4 = *((uint32_t *)(ebp + 5*4));
		arg5 = *((uint32_t *)(ebp + 6*4));
		cprintf("  ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", ebp, eip, arg1, arg2, arg3, arg4, arg5);
		debuginfo_eip(eip, &info);
		cprintf("         %s:%d: %.*s+%d\n",
				info.eip_file,
				info.eip_line,
				info.eip_fn_namelen,
				info.eip_fn_name,
				eip - info.eip_fn_addr);
	}
	return 0;
}

static uintptr_t
parse_addr(const char *s)
{
	// strtol handles 0x... or decimal
	return (uintptr_t) strtol(s, 0, 0);
}

static void
print_perm(uint32_t pte)
{
	cprintf("%c", (pte & PTE_P) ? 'P' : '-');
	cprintf("%c", (pte & PTE_W) ? 'W' : '-');
	cprintf("%c", (pte & PTE_U) ? 'U' : '-');
	cprintf("%c", (pte & PTE_PWT) ? 'T' : '-');
	cprintf("%c", (pte & PTE_PCD) ? 'C' : '-');
	cprintf("%c", (pte & PTE_A) ? 'A' : '-');
	cprintf("%c", (pte & PTE_D) ? 'D' : '-');
}

static int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) {
		cprintf("Usage: showmappings va_start va_end\n");
		return 0;
	}

	uintptr_t start = parse_addr(argv[1]);
	uintptr_t end = parse_addr(argv[2]);

	if (end < start) {
		uintptr_t tmp = start;
		start = end;
		end = tmp;
	}

	uintptr_t va;
	start = ROUNDDOWN(start, PGSIZE);
	end = ROUNDDOWN(end, PGSIZE);

	for (va = start; ; va += PGSIZE) {
		pde_t pde = kern_pgdir[PDX(va)];
		if (!(pde & PTE_P)) {
			cprintf("va %08x: unmapped (no PDE)\n", va);
		} else {
			pte_t *pte = pgdir_walk(kern_pgdir, (void *) va, 0);
			if (!pte || !(*pte & PTE_P)) {
				cprintf("va %08x: unmapped (no PTE)\n", va);
			} else {
				physaddr_t pa = PTE_ADDR(*pte);
				cprintf("va %08x -> pa %08x  perm ", va, pa);
				print_perm(*pte);
				cprintf("\n");
			}
		}

		if (va == end)
			break;
	}

	return 0;
}

static int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 3) {
		cprintf("Usage: setperm va [+u|-u] [+w|-w] [+p|-p]\n");
		return 0;
	}

	uintptr_t va = ROUNDDOWN(parse_addr(argv[1]), PGSIZE);

	pte_t *pte = pgdir_walk(kern_pgdir, (void *) va, 0);
	if (!pte || !(*pte & PTE_P)) {
		cprintf("va %08x: not mapped\n", va);
		return 0;
	}

	uint32_t flags = *pte & 0xFFF;
	uint32_t pa = PTE_ADDR(*pte);

	int i;
	for (i = 2; i < argc; i++) {
		char *op = argv[i];
		if ((op[0] != '+' && op[0] != '-') || op[2] != 0) {
			cprintf("bad flag '%s' (expected +u, -w, etc)\n", op);
			return 0;
		}

		uint32_t bit = 0;
		if (op[1] == 'u') bit = PTE_U;
		else if (op[1] == 'w') bit = PTE_W;
		else if (op[1] == 'p') bit = PTE_P;
		else {
			cprintf("unknown flag '%s'\n", op);
			return 0;
		}

		if (op[0] == '+') flags |= bit;
		else flags &= ~bit;
	}

	*pte = pa | flags;
	tlb_invalidate(kern_pgdir, (void *) va);

	cprintf("va %08x updated: pa %08x  perm ", va, pa);
	print_perm(*pte);
	cprintf("\n");

	return 0;
}

static void
dump_line(uintptr_t addr, uint8_t *p, int n)
{
	int i;
	cprintf("%08x: ", addr);
	for (i = 0; i < n; i++) {
		cprintf("%02x ", p[i]);
	}
	cprintf("\n");
}

static int
mon_dumpva(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) {
		cprintf("Usage: dumpva va_start va_end\n");
		return 0;
	}

	uintptr_t start = parse_addr(argv[1]);
	uintptr_t end = parse_addr(argv[2]);

	if (end < start) {
		uintptr_t tmp = start;
		start = end;
		end = tmp;
	}

	uintptr_t addr = start;
	while (addr <= end) {
		// Verify mapping for this page
		pte_t *pte = pgdir_walk(kern_pgdir, (void *) addr, 0);
		if (!pte || !(*pte & PTE_P)) {
			cprintf("va %08x: unmapped\n", ROUNDDOWN(addr, PGSIZE));
			return 0;
		}

		// Dump up to 16 bytes, but do not cross end
		int n = 16;
		if (end - addr + 1 < (uintptr_t) n)
			n = (int) (end - addr + 1);

		dump_line(addr, (uint8_t *) addr, n);
		addr += n;
	}

	return 0;
}

static int
mon_dumppa(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) {
		cprintf("Usage: dumppa pa_start pa_end\n");
		return 0;
	}

	physaddr_t start = (physaddr_t) parse_addr(argv[1]);
	physaddr_t end = (physaddr_t) parse_addr(argv[2]);

	if (end < start) {
		physaddr_t tmp = start;
		start = end;
		end = tmp;
	}

	physaddr_t pa = start;
	while (pa <= end) {
		int n = 16;
		if (end - pa + 1 < (physaddr_t) n)
			n = (int) (end - pa + 1);

		uint8_t *kva = (uint8_t *) KADDR(pa);
		dump_line((uintptr_t) pa, kva, n);
		pa += n;
	}

	return 0;
}



static int
mon_continue(int argc, char **argv, struct Trapframe *tf)
{
	if (tf == NULL) {
		cprintf("No trapframe: not in an exception context\n");
		return 0;
	}
	// Clear the Trap Flag so execution runs freely
	tf->tf_eflags &= ~FL_TF;
	return -1; // exit the monitor loop
}

static int
mon_si(int argc, char **argv, struct Trapframe *tf)
{
	if (tf == NULL) {
		cprintf("No trapframe: not in an exception context\n");
		return 0;
	}
	// Set the Trap Flag: CPU will fire T_DEBUG after the next instruction
	tf->tf_eflags |= FL_TF;
	return -1; // exit the monitor loop, execute one instruction, then re-trap
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
