/*rmld.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#ifndef KERNELTK_KERNELTK_H
#define KERNELTK_KERNELTK_H

#include <loader/byte_table.h>
#include <loader/symbol.h>

#define rmld_error_t u16

/**
 * The loading environment contains data related to a relocatable elf file
 * that must be loaded into memory;
 */
struct loading_env {
	
	/*The elf header;*/
	struct elf64_hdr *r_hdr;
	
	/*The end of the file;*/
	void *r_file_end;
	
	/*Section header table descriptor;*/
	struct byte_table r_shtable;
	
};


/*TODO FLAGS;*/
/*TODO FLAGS;*/
/*TODO FLAGS;*/
/*TODO FLAGS;*/
/*TODO FLAGS;*/



/*If set, the provided object file comprised a non-empty nobits section;*/
#define LOADER_ERR_NOBITS_SECTION ((rmld_error_t) (1 << 5))


/*If set, a symbol table had an invalid string index;*/
#define LOADER_ERR_SYMTBL_BAD_STRTBL_ID ((rmld_error_t) (1 << 6))


/*If set, a relocation table had an invalid symbol table index;*/
#define LOADER_ERR_RELTBL_BAD_STRTBL_ID ((rmld_error_t) (1 << 7))

/*If set, a relocation table had an invalid relocation section index;*/
#define LOADER_ERR_RTBL_INV_RSID ((rmld_error_t) (1 << 8))

/*If set, a relocation symbol had a null index;*/
#define LOADER_ERR_RSYM_NULL_INDEX ((rmld_error_t) (1 << 9))

/*If set, a relocation symbol had an invalid index;*/
#define LOADER_ERR_RSYM_INV_INDEX ((rmld_error_t) (1 << 10))

/*If set, a relocation symbol had a null value;*/
#define LOADER_ERR_RSYM_NULL_ADDR ((rmld_error_t) (1 << 11))

/*If set, an unsupported relocation was required;*/
#define LOADER_ERR_RTYPE_UNS ((rmld_error_t) (1 << 12))

/*If set, a relocation value was too high for the relocation type;*/
#define LOADER_ERR_RVAL_OVF ((rmld_error_t) (1 << 13))


/**
 * loader_init : initializes the loading environment for the provided elf file;
 * @param env : the environment to initialize;
 * @param ram_start : the address of the file's first byte in RAM;
 * @param file_size : the size in bytes of the file in RAM;
 */
void loader_init(struct loading_env *env, void *ram_start, usize file_size);

/**
* loader_assign_sections : update all section's values to their RAM addresses;
* @param ldr : the loading environment;
* @return 0 if all section were assigned correctly, 1 if a non-empty nobits
* section was present; the latter should stop the loading;
*/
u8 loader_assign_sections(struct loading_env *ldr);

/**
 * loader_assign_symbols : for each symbol in the environment :
 * - if the symbol is defined updates the symbol's address internally and
 *   updated the list of symbol queries if required;
 * - if the symbol is not defined, search the list of external definitons
 *   for an eventual matching symbol;
 * It it possible that undefined symbols remain after the execution of this
 * function. Those will have their value assigned to 0;
 * @param env : the loading environment
 * @param definitions : a list of defined symbols, that are accessible to the
 * executable; if undefined symbols with matching names are found in the
 * executable, their value will be set to the value provided in the list;
 * @param queries : a set of symbols the executable may define; if defined
 * symbols with matching names are found in the executable, the list will be
 * updated with the value of the symbol in the executable;
 * @return 0 if all symbols had their value assigned, or, if a symbol table's
 * string table index was invalid (only source of error), the index of the
 * symbol table's section header; this error should stop the loading;
 */
u16 loader_assign_symbols(
	struct loading_env *env,
	struct rmld_sym *defs,
	struct rmld_sym *undefs
);

u8 rmld_apply_relocations(struct loading_env *rel);






#endif /*KERNELTK_KERNELTK_H*/
