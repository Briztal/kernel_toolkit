/*rmld.c - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#include <loader/elf64.h>
#include <loader/loader.h>
#include <string.h>


static inline char *strtbl_get(struct byte_table *strtbl, usize index)
{
	
	return ((char *) ptr_sum_byte_offset((strtbl->t_start), (index)));
	
}

/**
 * get_section_header : gets the reference of a section header from its index
 * 	in the section header table;
 * @param env : the loading environment;
 * @param section_id : the index of the section header to retrieve;
 * @param section_type : the expected type of the section; 0 to disable check;
 * @param shdr_p : the ref of the location where to store the ref of the
 * 	header;
 * @return an error code;
 */
static u8 get_section_header(
	struct loading_env *env,
	u16 section_id,
	u32 section_type,
	struct elf64_shdr **shdr_p
)
{
	struct elf64_shdr *shdr;
	u8 error;
	
	/*If the section index in undefined :*/
	if (section_id == SHN_UNDEF) {
		
		/*Error code 1;*/
		return 1;
		
	}
	
	/*If the section index is reserved :*/
	if (section_id >= SHN_LORESERVE) {
		
		/*Error code 2;*/
		return 2;
		
	}
	
	/*Cache the ref of the section header;*/
	error = byte_table_get_entry(&env->r_shtable, section_id, (void **) &shdr);
	
	/*If the index was invalid :*/
	if (error) {
		
		/*Error code 3;*/
		return 3;
		
	}
	
	/*If type check is enabled and types differ :*/
	if ((section_type) && (shdr->sh_type != section_type)) {
		
		/*Error code 4;*/
		return 4;
		
	}
	
	/*Update the section header;*/
	*shdr_p = shdr;
	
	/*Complete;*/
	return 0;
	
}


/**
 * shdr_to_byte_table : initializes @table with data describing the section
 * header @shdr, related to the loading environment @env;
 * @param env : the loading environment;
 * @param shdr : the section header whose byte table should be extracted;
 * @param table : the byte table where data should be saved;
 */
static inline void shdr_to_byte_table(
	const struct loading_env *env,
	const struct elf64_shdr *shdr,
	struct byte_table *table
)
{
	void *start;
	table->t_start = start = ptr_sum_byte_offset(env->r_hdr, shdr->sh_offset);
	table->t_end = ptr_sum_byte_offset(start, shdr->sh_size);
	table->t_bsize = (shdr)->sh_entsize;
}


/**
 * get_section_table : get the table referenced by a section header;
 * @param env : the loading environment;
 * @param section_id : the id of the section to get the table of;
 * @param section_type : the expected type of the section, 0 to disable check;
 * @param tbl : the table descriptor to update;
 * @return an error code;
 */
static u8 get_section_table(
	struct loading_env *env,
	u16 section_id,
	u32 section_type,
	struct byte_table *tbl
)
{
	
	struct elf64_shdr *shdr;
	u8 error;
	
	/*Get the string table section header;*/
	error = get_section_header(env, section_id, section_type, &shdr);
	
	/*If an error occurred, return the error code;*/
	if (error) {
		return error;
	}
	
	/*Update the table descriptor;*/
	shdr_to_byte_table(env, shdr, tbl);
	
	/*Complete;*/
	return 0;
	
}


/*---------------------------------------------------------------- loader init*/

/**
 * loader_init : initializes the loading environment for the provided elf file;
 * @param env : the environment to initialize;
 * @param ram_start : the address of the file's first byte in RAM;
 * @param file_size : the size in bytes of the file in RAM;
 */
void loader_init(
	struct loading_env *env,
	void *ram_start,
	usize file_size
)
{
	
	struct elf64_hdr *hdr;
	u8 *shtable;
	usize shentry_size;
	
	/*Initialize the elf header;*/
	env->r_hdr = hdr = ram_start;
	
	/*Initialize the file size*/
	env->r_file_end = ptr_sum_byte_offset(ram_start, file_size);
	
	/*Determine the address of the section table;*/
	env->r_shtable.t_start = shtable =
		ptr_sum_byte_offset(ram_start, hdr->e_shoff);
	
	/*Determine section table desc vars;*/
	env->r_shtable.t_bsize = shentry_size = hdr->e_shentsize;
	
	/*Determine the end of the section table;*/
	env->r_shtable.t_end =
		ptr_sum_byte_offset(shtable, shentry_size * hdr->e_shnum);
	
}


/*-------------------------------------------------------- sections assignment*/


/**
 * loader_assign_sections : update all section's values to their RAM addresses;
 * @param ldr : the loading environment;
 * @return 0 if all section were assigned correctly, 1 if a non-empty nobits
 * section was present; the latter should stop the loading;
 */
u8 loader_assign_sections(struct loading_env *ldr)
{
	
	void *hdr;
	struct byte_table shtable;
	struct elf64_shdr *shdr;
	
	/*Cache vars;*/
	hdr = ldr->r_hdr;
	shtable = ldr->r_shtable;
	
	/*Iterate over the section table :*/
	BTABLE_ITERATE(shtable, shdr) {
		
		void *address;
		u64 offset;
		
		/*If the section is not present in the file and has a non null
		 * size, fail;*/
		if ((shdr->sh_type == SHT_NOBITS) && (shdr->sh_size)) {
			return LOADER_ERR_NOBITS_SECTION;
		}
		
		
		/*Cache the offset of the section;*/
		offset = shdr->sh_offset;
		
		/*Use the section address;*/
		address = ptr_sum_byte_offset(hdr, offset);
		
		/*Update the section's address;*/
		shdr->sh_addr = (u64) address;
		
	}
	
	/*Complete;*/
	return 0;
	
}


/*--------------------------------------------------------- symbols assignment*/


/**
 * symbol_update_address : attempts to determine the address (value) of a
 * 	symbol, from its related section;
 * @param env : a the loading environment;
 * @param sym : the symbol to update the value of;
 */
static void update_symbol_address(
	struct loading_env *env,
	struct elf64_sym *sym
)
{
	
	u16 section_id;
	struct elf64_shdr *shdr;
	u8 error;
	usize value;
	
	/*Cache the symbol's section's index;*/
	section_id = sym->sy_shndx;
	
	/*Get the section header; the section should contain program data*/
	error = get_section_header(env, section_id, SHT_PROGBITS, &shdr);
	
	/*If an error occurred:*/
	if (error) {
		
		/*The symbol's value is set to null;*/
		value = 0;
		
	} else {
		
		/*If the section exists, determine the symbol's address;*/
		value = sym->sy_value + shdr->sh_addr;
		
	}
	
	/*Save the symbol's value;*/
	sym->sy_value = value;
	
}

/**
 * assing_symbol_table : for each symbol in the symbol table :
 * - if the symbol is defined updates the symbol's address internally and
 *   updated the list of symbol queries if required;
 * - if the symbol is not defined, search the list of external definitons
 *   for an eventual matching symbol;
 * It it possible that undefined symbols remain after the execution of this
 * function. Those will have their value assigned to 0;
 * @param env : the loading environment
 * @param symtbl_hdr : symbol table's section header;
 * @param definitions : a list of defined symbols, that are accessible to the
 * executable; if undefined symbols with matching names are found in the
 * executable, their value will be set to the value provided in the list;
 * @param queries : a set of symbols the executable may define; if defined
 * symbols with matching names are found in the executable, the list will be
 * updated with the value of the symbol in the executable;
 * @return 0 if all symbols had their value assigned, 1 if the symbol table's
 * string table index was invalid. This case should stop the loading;
 */
static u8 assing_symbol_table(
	struct loading_env *env,
	struct elf64_shdr *symtbl_hdr,
	struct rmld_sym *definitions,
	struct rmld_sym *queries
)
{
	
	u16 strtbl_id;
	u8 strtbl_id_error;
	struct byte_table strtbl;
	struct byte_table symtable;
	struct elf64_sym *sym;
	
	/*Cache the string table id;*/
	strtbl_id = (u16) symtbl_hdr->sh_link;
	
	/*Cache the string table;*/
	strtbl_id_error = get_section_table(env, strtbl_id, SHT_STRTAB, &strtbl);
	
	/*If the string table index was invalid, fail;*/
	if (strtbl_id_error) {
		return LOADER_ERR_SYMTBL_BAD_STRTBL_ID;
	}
	
	/*Cache the symbol table descriptor;*/
	shdr_to_byte_table(env, symtbl_hdr, &symtable);
	
	/*Iterate over the symbol table;*/
	BTABLE_ITERATE(symtable, sym) {
		
		const char *s_name;
		struct rmld_sym *ext_sym;
		
		/*Cache the name start;*/
		s_name = strtbl_get(&strtbl, sym->sy_name);
		
		
		/*
		 * Internal symbol definition;
		 */
		
		/*If the symbol is undefined :*/
		if (sym->sy_shndx == SHN_UNDEF) {
			
			/*If a definition exists, update the value;
			 * if not, set the symbol's value to 0;*/
			sym->sy_value = (u64) sym_def_find(definitions, s_name);
			
		} else {
			
			/*If the symbol is defined, update its value;*/
			update_symbol_address(env, sym);
			
		}
		
		/*If the symbol's value is null, stop here;*/
		if (!sym->sy_value) {
			continue;
		}
		
		
		/*
		 * External symbols definition;
		 */
		
		/*Initialise the current symbol;*/
		ext_sym = queries;
		
		/*For each external symbol:*/
		while (ext_sym) {
			
			/*If the symbol is already defined, skip;*/
			/*If symbols names do not match, skip;*/
			if ((ext_sym->s_defined) ||
				(strcmp(s_name, ext_sym->s_name) != 0)) {
				ext_sym = ext_sym->s_next;
				continue;
				
			}
			
			/*Define the external symbol;*/
			ext_sym->s_defined = 1;
			ext_sym->s_addr = (void *) sym->sy_value;
			
			/*Stop here;*/
			break;
			
		}
		
	}
	
	/*Complete;*/
	return 0;
	
}

#define FAIL_WITH(mask) {env->r_error |= (mask); goto error;}


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
)
{
	
	struct byte_table shtable;
	struct elf64_shdr *sheader;
	u16 section_index;
	
	/*Cache section header table descriptor;*/
	shtable = env->r_shtable;
	
	/*Reset the section index;*/
	section_index = 0;
	
	/*Iterate over the section table :*/
	BTABLE_ITERATE(shtable, sheader) {
		
		/*If the section holds a symbol table :*/
		if (sheader->sh_type == SHT_SYMTAB) {
			
			u8 error;
			
			/*Assign symbols in the symbol table;*/
			error = assing_symbol_table(env, sheader, defs, undefs);
			
			/*If an error occurred, fail;*/
			if (error) {
				return section_index;
			}
			
		}
		
		/*Update the section index;*/
		section_index++;
		
	}
	
	/*Complete;*/
	return 0;
	
}


/*--------------------------------------------------------------- relocations */


u8 rel16(void *dst, u64 val, u8 relative)
{
	
	u64 abs;
	u16 final_value;
	
	/*If the value is relative, val is a signed number;*/
	if (relative) {
		
		/*Cast the provided value;*/
		s64 sval = (s64) val;
		
		/*Determine the absolute value;*/
		abs = (u64) ((sval < 0) ? -sval : sval);
		
		/*Determine the final value;*/
		final_value = (u16) (s16) sval;
		
	} else {
		
		/*If the value is absolute, no need for a signed check;*/
		abs = val;
		final_value = (u16) val;
		
	}
	
	/*Check the value for an overflow;*/
	if (abs > (u64) ((u16) -1)) {
		return 1;
	}
	
	/*Apply the relocation;*/
	*((u16 *) dst) = final_value;
	
	return 0;
	
}

u8 rel32(void *dst, u64 val, u8 relative)
{
	
	u64 abs;
	u32 final_value;
	
	/*If the value is relative, val is a signed number;*/
	if (relative) {
		
		/*Cast the provided value;*/
		s64 sval = (s64) val;
		
		/*Determine the absolute value;*/
		abs = (u64) ((sval < 0) ? -sval : sval);
		
		/*Determine the final value;*/
		final_value = (u32) (s32) sval;
		
	} else {
		
		/*If the value is absolute, no need for a signed check;*/
		abs = val;
		final_value = (u32) val;
		
	}
	
	/*Check the value for an overflow;*/
	if (abs > (u64) ((u32) -1)) {
		return 1;
	}
	
	/*Apply the relocation;*/
	*((u32 *) dst) = final_value;
	
	return 0;
	
}

u8 rel64(void *dst, u64 val, u8 relative)
{
	
	u64 abs;
	u64 final_value;
	
	/*If the value is relative, val is a signed number;*/
	if (relative) {
		
		/*Cast the provided value;*/
		s64 sval = (s64) val;
		
		/*Determine the absolute value;*/
		abs = (u64) ((sval < 0) ? -sval : sval);
		
		/*Determine the final value;*/
		final_value = (u64) (s64) sval;
		
	} else {
		
		/*If the value is absolute, no need for a signed check;*/
		abs = val;
		final_value = (u64) val;
		
	}
	
	/*Check the value for an overflow;*/
	if (abs > (u64) ((u64) -1)) {
		return 1;
	}
	
	/*Apply the relocation;*/
	*((u64 *) dst) = final_value;
	
	return 0;
	
}

u8 relocate(
	struct loading_env *ldr,
	u64 rel_addr,
	u64 sym_addr,
	s64 addend,
	u32 rel_type
)
{
	
	u8 error;
	u8 relative;
	u64 rel_value;
	
	/*The function that will apply the relocation;*/
	u8 (*rel_f)(void *, u64, u8);
	
	/*
	 * Each case will :
	 *  - update the relocation function;
	 *  - update operands to fit the generic value calculation;
	 */
	switch (rel_type) {
		
		
		case 2: /*R_AMD64_PC32 :*/
			rel_f = &rel32;
			relative = 1;
			break;
		
		
		case 4:    /*R_AMD64_PLT32 :*/
			rel_f = &rel32;
			relative = 1;
			break;
		
		default: /*Unsupported relocation :*/
			
			/*Set the related flag and quit;*/
			ldr->r_error |= RMLD_ERR_RTYPE_UNS;
			return 1;
		
	}
	
	/*Compute the relocation value;*/
	rel_value = sym_addr + addend - rel_addr;
	
	/*Apply the relocation;*/
	error = (*rel_f)((void *) rel_addr, rel_value, relative);
	
	/*If the value was too high for the relocation size :*/
	if (error) {
		
		/*Set the flag and fail;*/
		ldr->r_error |= RMLD_ERR_RVAL_OVF;
		return 1;
	}
	
	/*Complete;*/
	return 0;
	
}


/**
 * apply_relocations : attempts to apply all relocations of the provided
 *  relocation table;
 *
 * @param hdr : the elf file header;
 * @param shtbl : the elf section header;
 * @param reltbl_hdr : the relocation table header;
 * @return an error flag;
 */

static u8 apply_reloaction_table(
	struct loading_env *ldr,
	struct elf64_shdr *reltbl_hdr
)
{
	
	u8 explicit_addend;
	u8 error;
	struct byte_table reltable;
	u16 symtbl_id;
	
	u16 rel_sect_id;
	struct elf64_shdr *rel_sect_hdr;
	u64 rel_sect_start;
	
	struct byte_table symtbl;
	
	struct elf64_rela *rel;
	
	
	/*Determine whether an explicit addend is provided;*/
	explicit_addend = (u8) (reltbl_hdr->sh_type == SHT_RELA);
	
	/*Cache the relocation table;*/
	shdr_to_byte_table(ldr, reltbl_hdr, &reltable);
	
	
	/*Cache the symbol table header identifier;*/
	symtbl_id = (u16) reltbl_hdr->sh_link;
	
	/*Cache the symbol table;*/
	error = get_section_table(ldr, symtbl_id, SHT_SYMTAB, &symtbl);
	
	/*If the symbol table index is invalid, set the flag and fail;*/
	if (error) {
		ldr->r_error |= RMLD_ERR_RTBL_INV_STID;
		return 1;
	}
	
	
	/*Cache the index of the section to relocate;*/
	rel_sect_id = (u16) reltbl_hdr->sh_info;
	
	/*Cache the header of the section to relocate;*/
	error = get_section_header(ldr, rel_sect_id, SHT_PROGBITS, &rel_sect_hdr);
	
	/*If the relocation section index is invalid, fail;*/
	if (error) {
		ldr->r_error |= RMLD_ERR_RTBL_INV_RSID;
		return 1;
	}
	
	/*Cache the start address section to relocate;*/
	rel_sect_start = rel_sect_hdr->sh_addr;
	
	
	/*Iterate over the relocation table;*/
	BTABLE_ITERATE(reltable, rel) {
		
		u64 rel_info;
		u32 sym_index;
		u32 rel_type;
		
		struct elf64_sym *sym;
		
		u64 rel_addr;
		u64 sym_addr;
		s64 addend;
		
		/*Determine the relocation's address;*/
		rel_addr = (u64) ptr_sum_byte_offset(rel_sect_start,
											 rel->r_offset);
		
		/*Cache relocation information, get index and type;*/
		rel_info = rel->r_info;
		sym_index = ELF64_R_SYM(rel_info);
		rel_type = ELF64_R_TYPE(rel_info);
		
		/*If the symbol's index is invalid :*/
		if (!sym_index) {
			ldr->r_error |= RMLD_ERR_RSYM_NULL_INDEX;
			return 1;
		}
		
		/*Cache the symbol reference;*/
		error = byte_table_get_entry(&symtbl, sym_index, (void **) &sym);
		
		if (error) {
			ldr->r_error |= RMLD_ERR_RSYM_INV_INDEX;
			return 1;
		}
		
		/*Cache the symbol's value;*/
		sym_addr = sym->sy_value;
		
		/*If the symbol's address is null :*/
		if (!sym_addr) {
			ldr->r_error |= RMLD_ERR_RSYM_NULL_ADDR;
			return 1;
		}
		
		/*Initialise the addend;*/
		addend = (explicit_addend) ? rel->r_addend : 0;
		
		/*Apply the relocation;*/
		error = relocate(ldr, rel_addr, sym_addr, addend, rel_type);
		
		/*If an error occurred, fail;*/
		if (error) {
			return 1;
		}
		
	}
	
	/*If all relocations were properly applied, complete;*/
	return 0;
	
}


/**
 * rmld_apply_relocations : attempts to apply all relocations of all
 * 	relocation tables of the provided rmld's file;
 *
 * 	This function requires a file with symbols assigned;
 *
 * @param ldr : the rmld;
 * @return an error code;
 */

u8 rmld_apply_relocations(struct loading_env *ldr)
{
	
	struct byte_table shtable;
	struct elf64_shdr *shdr;
	
	/*If error flags are set, fail;*/
	if (ldr->r_error) {
		FAIL_WITH(RMLD_ERR_REDETECTION);
	}
	
	/*If the file status is invalid, fail;*/
	if (ldr->r_fstatus != RMLD_FSTATUS_SYMBOLS_ASSIGNED) {
		FAIL_WITH(RMLD_ERR_INV_FSTATUS);
	}
	
	
	/*Cache the symbol table descriptor;*/
	shtable = ldr->r_shtable;
	
	/*Iterate over the section table :*/
	BTABLE_ITERATE(shtable, shdr) {
		
		u32 sh_type;
		
		/*Cache the section type;*/
		sh_type = shdr->sh_type;
		
		/*If the section contains a relocation table :*/
		if ((sh_type == SHT_REL) || (sh_type == SHT_RELA)) {
			
			u8 error;
			
			/*Attempt to apply relocations;*/
			error = apply_reloaction_table(ldr, shdr);
			
			/*If an error occurred, fail;*/
			if (error) {
				goto error;
			}
			
		}
		
	}
	
	/*Update the file status;*/
	ldr->r_fstatus = RMLD_FSTATUS_RELOCATIONS_APPLIED;
	
	/*Complete;*/
	return 0;
	
	
	/*
	 * Error section;
	 */
	
	error :
	
	/*Set the error flag;*/
	ldr->r_error |= RMLD_ERR_RELOC_APPL;
	
	/*Fail;*/
	return 1;
	
}

