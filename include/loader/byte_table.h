/*btable.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#ifndef KERNELTK_BTABLE_H
#define KERNELTK_BTABLE_H

#include <types.h>

/**
 * The byte table struct contains data to describe an abstract byte table,
 * that contains a given number of entries, of a constant size;
 */
struct byte_table {
	
	/*Address of the lowest byte of the table;*/
	void *t_start;
	
	/*Address of the table's last byte's successor;*/
	void *t_end;
	
	/*Size of one block in the table;*/
	usize t_bsize;
	
};

/**
 * byte_table_get_entry : update *entry to the ref of the first byte of the
 * entry at @index in @table, if such an index is valid, and to 0 otherwise;
 * @param table : the byte table descriptor;
 * @param index : the index of the entry to fetch;
 * @param entry : the location where to save the ref of the entry's first byte;
 * @return 1 if the index was valid, 0 otherwise;
 */
static inline u8 byte_table_get_entry(
	struct byte_table *dsc,
	usize index,
	void **entry
)
{
	/*Cache the block ref;*/
	void *bref = ptr_sum_byte_offset(dsc->t_start, index * dsc->t_bsize);
	
	/*If bad index, fail;*/
	if (bref >= dsc->t_end) {
		return 1;
	}
	
	/*If the index was valid, complete;*/
	*entry = bref;
	return 0;
	
}

/**
 * BTABLE_ITERATE : iterates on each entry of @table, saving the ref of the
 * current entry in @entry_p;
 */
#define BTABLE_ITERATE(table, entry_p) \
    for ((entry_p) = (table).t_start;\
        (usize)(entry_p) < (usize)(table).t_end;\
		(entry_p) = ptr_sum_byte_offset(entry_p,(table).t_bsize)\
    )


#endif /*KERNELTK_BTABLE_H*/
