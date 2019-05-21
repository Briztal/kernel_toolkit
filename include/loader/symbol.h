/*symbol.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#ifndef KERNELTK_SYMBOL_H
#define KERNELTK_SYMBOL_H

#include <types.h>

struct rmld_sym {
	
	/*symbols are referenced in a linked list;*/
	struct rmld_sym *s_next;
	
	/*The address of the symbol;*/
	void *s_addr;
	
	/*A flag, set if the symbol is defined;*/
	u8 s_defined;
	
	/*The name of the symbol;*/
	const char *s_name;
	
	
};

/*Search a symbol table for a symbol definition;*/
void *sym_def_find(struct rmld_sym *defs, const char *name);


#endif /*KERNELTK_SYMBOL_H*/
