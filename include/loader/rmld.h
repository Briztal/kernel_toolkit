/*rmld.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#ifndef KERNELTK_KERNELTK_H
#define KERNELTK_KERNELTK_H

#include <loader/btable.h>
#include <loader/symbol.h>

#define rmld_error_t u16

struct rmld {
	
	/*Error flags; see below;*/
	rmld_error_t r_error;
	
	/*The ram file status; see below;*/
	u8 r_fstatus;
	
	
	/*The elf header;*/
	struct elf64_hdr *r_hdr;
	
	/*The end of the file;*/
	void *r_file_end;
	
	/*Section header table descriptor;*/
	struct btable r_shtable;
	
	
};


/*TODO FLAGS;*/
/*TODO FLAGS;*/
/*TODO FLAGS;*/
/*TODO FLAGS;*/
/*TODO FLAGS;*/

/*If set, an error occurred in the section allocation function;*/
#define RMLD_ERR_SECT_ASSIGN ((rmld_error_t) (1 << 0))

/*If set, an error occurred in the symbols assignment function;*/
#define RMLD_ERR_SYMB_ASSIGN ((rmld_error_t) (1 << 1))

/*If set, an error occurred in the relocations application function;*/
#define RMLD_ERR_RELOC_APPL ((rmld_error_t) (1 << 2))

/*If set, error flags set were set at a function entry;*/
#define RMLD_ERR_REDETECTION  ((rmld_error_t) (1 << 3))

/*If set, the file status was invalid at a function entry;;*/
#define RMLD_ERR_INV_FSTATUS ((rmld_error_t) (1 << 4))


/*If set, the provided object file comprised a non-empty nobits section;*/
#define RMLD_ERR_NOBITS_SECTION ((rmld_error_t) (1 << 5))


/*If set, a symbol table had an invalid string index;*/
#define RMLD_ERR_STBL_INV_STID ((rmld_error_t) (1 << 6))


/*If set, a relocation table had an invalid symbol table index;*/
#define RMLD_ERR_RTBL_INV_STID ((rmld_error_t) (1 << 7))

/*If set, a relocation table had an invalid relocation section index;*/
#define RMLD_ERR_RTBL_INV_RSID ((rmld_error_t) (1 << 8))

/*If set, a relocation symbol had a null index;*/
#define RMLD_ERR_RSYM_NULL_INDEX ((rmld_error_t) (1 << 9))

/*If set, a relocation symbol had an invalid index;*/
#define RMLD_ERR_RSYM_INV_INDEX ((rmld_error_t) (1 << 10))

/*If set, a relocation symbol had a null value;*/
#define RMLD_ERR_RSYM_NULL_ADDR ((rmld_error_t) (1 << 11))

/*If set, an unsupported relocation was required;*/
#define RMLD_ERR_RTYPE_UNS ((rmld_error_t) (1 << 12))

/*If set, a relocation value was too high for the relocation type;*/
#define RMLD_ERR_RVAL_OVF ((rmld_error_t) (1 << 13))



/*Ram data is identical to the disk image;*/
#define RMLD_FSTATUS_DISK_IMAGE 0

/*All sections are allocated and their value is up to date;*/
#define RMLD_FSTATUS_SECTIONS_ALLOCATED 1

/*Symbols values are up to date, undefined symbols have a null value;*/
#define RMLD_FSTATUS_SYMBOLS_ASSIGNED 2

/*Relocations have been executed;*/
#define RMLD_FSTATUS_RELOCATIONS_APPLIED 3



/*Initialize x rmld struct;*/
void rmld_ctor(
	struct rmld *ldr, void *ram_elf_file, usize elf_file_size
);

/*Allocate all sections of a rmld's executable;*/
u8 rmld_assign_sections(struct rmld *ldr);


u8 rmld_assign_symbols(
	struct rmld *rel,
	struct rmld_sym *defs,
	struct rmld_sym *undefs
);

u8 rmld_apply_relocations(struct rmld *rel);






#endif /*KERNELTK_KERNELTK_H*/
