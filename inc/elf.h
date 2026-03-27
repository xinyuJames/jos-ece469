#ifndef JOS_INC_ELF_H
#define JOS_INC_ELF_H

#define ELF_MAGIC 0x464C457FU	/* "\x7FELF" in little endian */

struct Elf {
	uint32_t e_magic;	// must equal ELF_MAGIC
	uint8_t e_elf[12];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry; // entry point of the kernel
	uint32_t e_phoff; // offset to program header table
	uint32_t e_shoff; // offset to section header table
	uint32_t e_flags;
	uint16_t e_ehsize; // size of ELF header
	uint16_t e_phentsize;
	uint16_t e_phnum; // number of program header
	uint16_t e_shentsize; //size of each section header entry
	uint16_t e_shnum; // number of section header
	uint16_t e_shstrndx;
};

struct Proghdr {
	uint32_t p_type;
	uint32_t p_offset; // offset of segment data in the file
	uint32_t p_va; // virtual address where ph should be loaded
	uint32_t p_pa;
	uint32_t p_filesz; // size of segment in the file
	uint32_t p_memsz; // size of segment in the memory
	uint32_t p_flags; // permission flags
	uint32_t p_align; // allignment requirement
};

struct Secthdr {
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	uint32_t sh_addr;
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entsize;
};

// Values for Proghdr::p_type
#define ELF_PROG_LOAD		1

// Flag bits for Proghdr::p_flags
#define ELF_PROG_FLAG_EXEC	1
#define ELF_PROG_FLAG_WRITE	2
#define ELF_PROG_FLAG_READ	4

// Values for Secthdr::sh_type
#define ELF_SHT_NULL		0
#define ELF_SHT_PROGBITS	1
#define ELF_SHT_SYMTAB		2
#define ELF_SHT_STRTAB		3

// Values for Secthdr::sh_name
#define ELF_SHN_UNDEF		0

#endif /* !JOS_INC_ELF_H */
