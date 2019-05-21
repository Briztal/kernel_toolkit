/*symbol.c - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/


#include <string.h>
#include <symbol.h>

/*Search a symbol table for a symbol definition;*/
void *sym_def_find(struct rmld_sym *defs, const char *name)
{
	
	while (defs) {
		
		/*If undefined or names do not match, skip;*/
		if ((!defs->s_defined) || (strcmp(name, defs->s_name) != 0)) {
			
			defs = defs->s_next;
			continue;
			
		}
		
		/*If names match, return the defined value;*/
		return defs->s_addr;
		
	}
	
	/*If no def was found, return 0;*/
	return 0;
	
}

