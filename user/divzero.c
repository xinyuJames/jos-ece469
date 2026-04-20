// buggy program - causes a divide by zero exception

#include <inc/lib.h>

int zero;

void
umain(int argc, char **argv)
{
	zero = 0;
	// int i = 1/zero;
	cprintf("1/0 is %08x!\n", 1/zero);
}

