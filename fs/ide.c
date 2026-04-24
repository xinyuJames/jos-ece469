/*
 * Interrupt-driven PIO IDE driver.
 * Reads block on sys_ide_intr_wait() per sector; writes poll for DRQ
 * then interrupt-wait for completion.  IRQ 14 must be unmasked in the
 * 8259A (done in kern/trap.c:trap_init).
 */

#include "fs.h"
#include <inc/x86.h>

#define IDE_BSY		0x80
#define IDE_DRDY	0x40
#define IDE_DF		0x20
#define IDE_ERR		0x01

/* ATA Device Control Register (write-only): nIEN bit disables interrupts */
#define IDE_DCR		0x3F6
#define IDE_DCR_NIEN	0x02	/* set = interrupts disabled */

static int diskno = 1;

static int
ide_wait_ready(bool check_error)
{
	int r;

	while (((r = inb(0x1F7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
		/* do nothing */;

	if (check_error && (r & (IDE_DF|IDE_ERR)) != 0)
		return -1;
	return 0;
}

bool
ide_probe_disk1(void)
{
	int r, x;

	// wait for Device 0 to be ready
	ide_wait_ready(0);

	// switch to Device 1
	outb(0x1F6, 0xE0 | (1<<4));

	// check for Device 1 to be ready for a while
	for (x = 0;
	     x < 1000 && ((r = inb(0x1F7)) & (IDE_BSY|IDE_DF|IDE_ERR)) != 0;
	     x++)
		/* do nothing */;

	// switch back to Device 0
	outb(0x1F6, 0xE0 | (0<<4));

	cprintf("Device 1 presence: %d\n", (x < 1000));
	return (x < 1000);
}

void
ide_set_disk(int d)
{
	if (d != 0 && d != 1)
		panic("bad disk number");
	diskno = d;
}

int
ide_read(uint32_t secno, void *dst, size_t nsecs)
{
	int r;

	assert(nsecs <= 256);

	ide_wait_ready(0);

	outb(IDE_DCR, 0x00);		/* enable drive interrupts (nIEN=0) */
	outb(0x1F2, nsecs);
	outb(0x1F3, secno & 0xFF);
	outb(0x1F4, (secno >> 8) & 0xFF);
	outb(0x1F5, (secno >> 16) & 0xFF);
	outb(0x1F6, 0xE0 | ((diskno&1)<<4) | ((secno>>24)&0x0F));
	outb(0x1F7, 0x20);		/* CMD: read sectors */

	for (; nsecs > 0; nsecs--, dst += SECTSIZE) {
		/* Block until the drive raises IRQ 14 (sector ready). */
		sys_ide_intr_wait();
		if ((r = ide_wait_ready(1)) < 0)
			return r;
		insl(0x1F0, dst, SECTSIZE/4);
	}

	return 0;
}

int
ide_write(uint32_t secno, const void *src, size_t nsecs)
{
	int r;

	assert(nsecs <= 256);

	ide_wait_ready(0);

	outb(IDE_DCR, 0x00);		/* enable drive interrupts (nIEN=0) */
	outb(0x1F2, nsecs);
	outb(0x1F3, secno & 0xFF);
	outb(0x1F4, (secno >> 8) & 0xFF);
	outb(0x1F5, (secno >> 16) & 0xFF);
	outb(0x1F6, 0xE0 | ((diskno&1)<<4) | ((secno>>24)&0x0F));
	outb(0x1F7, 0x30);		/* CMD: write sectors */

	for (; nsecs > 0; nsecs--, src += SECTSIZE) {
		/*
		 * For writes the drive sets DRQ without an interrupt on the
		 * first sector (QEMU ATA behaviour); poll briefly for DRQ,
		 * write the data, then block on the completion interrupt.
		 */
		if ((r = ide_wait_ready(1)) < 0)
			return r;
		outsl(0x1F0, src, SECTSIZE/4);
		sys_ide_intr_wait();	/* wait for write-complete interrupt */
	}

	return 0;
}
