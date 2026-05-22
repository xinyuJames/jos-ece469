// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf) // allocate a new page for current fault addr
{
	void *addr = (void *) utf->utf_fault_va;
	void * addr_alligned = (void *) ROUNDDOWN((uint32_t) addr, PGSIZE);
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(err & FEC_WR)) panic("pgfault: access not write\n");
	if (!((uint32_t) uvpt[PGNUM(addr)] & PTE_COW)) panic("pgfault: access not on COW page\n");
 
 	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	envid_t curenv_id = sys_getenvid();

	// insert a new physical page to va
	r = sys_page_alloc(curenv_id, PFTEMP, PTE_W|PTE_U|PTE_P);
	if (r < 0) panic("fork:sys_page_alloc error, %d\n", r);

	// copy old -> new
	memcpy(PFTEMP, addr_alligned, PGSIZE);

	// change mapping old -> new
	r = sys_page_map(curenv_id, PFTEMP, curenv_id, addr_alligned, PTE_P|PTE_W|PTE_U|PTE_COW);
	if (r < 0) panic("sys_page_map error, %d\n", r);

	if (sys_page_unmap(curenv_id, UTEMP) < 0) panic("pgfault:sys_page_umap error\n");

	// panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn) // map curenv addr to target_env addr's pp
{
	int r;

	// LAB 4: Your code here.
	// panic("duppage not implemented");

	uint32_t addr = pn * PGSIZE;
	uint8_t is_w;
	uint32_t pte = (uint32_t) uvpt[pn];

	// if current page not exist in src
	// if ((pte & PTE_P) == 0) return 0;


	if (pte & PTE_SHARE) // lab 5
	{
		r = sys_page_map(0, (void *) addr, envid, (void *) addr, PTE_SYSCALL & (pte & 0xfff));
	}
	else if (pte & PTE_W || pte & PTE_COW)
	{
		r = sys_page_map(0, (void *) addr, envid, (void *) addr, PTE_U|PTE_P|PTE_COW);
		if (r < 0) panic("fork:duppage COW first sys_page_map error, %d\n", r);
		r = sys_page_map(0, (void *) addr, 0, (void *) addr, PTE_U|PTE_P|PTE_COW);
		if (r < 0) panic("fork:duppage COW second sys_page_map error, %d\n", r);
	} else
	{
		r = sys_page_map(0, (void *) addr, envid, (void *) addr, PTE_U|PTE_P);
		if (r < 0) panic("fork:duppage no COW sys_page_map error, %d\n", r);
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	// panic("fork not implemented");
	envid_t envid;

	// set page_fault_handler to pgfault
	set_pgfault_handler(pgfault);

	// create a child
	envid = sys_exofork();
	if (envid < 0) panic("fork:sys_exofork: child create failed\n");
	
	if (envid == 0)
	{ // child space
		// cprintf("CHILDREN SPACE ENTERRED\n");
		// set up child page fault handler
		// set_pgfault_handler(pgfault); // this won't work because child's _pgfault_upcall non-zero, with duppage
		// alloc a page for child UXstack

		// set child upcall

		// fix child thisenv
		thisenv = &envs[ENVX(sys_getenvid())];
		// child receive 0
		return 0;
	} else
	{ // parent space
		int r;
		// map user page to child, child read only
		for (uint32_t i = 0; i < UTOP; i += PGSIZE)
		{
			// skip UXstack region
			if (i == UXSTACKTOP - PGSIZE) continue;

			if ((uvpd[PDX(i)] & PTE_P) && (uvpt[PGNUM(i)] & PTE_P) )
				duppage(envid, PGNUM(i));
		}

		r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_U|PTE_P|PTE_W);
		if (r < 0) panic("fork:sys_page_alloc error, %d\n", r);

		extern void _pgfault_upcall(void);
		r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
		if (r < 0) panic("fork:sys_env_set_pgfault_upcall error, %d\n", r);

		// mark child as runnable
		r = sys_env_set_status(envid, ENV_RUNNABLE);
		if (r < 0) panic("fork:sys_env_set_status error, %d\n", r);

		// parent receive child envid
		//cprintf("[%08x] FORKED FROM PARENT\n", envid);
		return envid;
	}
	panic("fork: control reach end\n");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
