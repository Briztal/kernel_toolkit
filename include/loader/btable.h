/*btable.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#ifndef KERNELTK_BTABLE_H
#define KERNELTK_BTABLE_H

#include <types.h>

struct btable {
	
	/*Address of the lowest byte of the table;*/
	void *t_start;
	
	/*Address of the table's last byte's successor;*/
	void *t_end;
	
	/*Size of one block in the table;*/
	usize t_bsize;
	
};



#define ptr_sum_byte_offset(ptr, offset)\
     ((void *) (((u8 *) (ptr)) + (offset)))



static inline u8 btable_get_bref(
	struct btable *dsc,
	usize index,
	void **dst
)
{
	/*Cache the block ref;*/
	void *bref = ptr_sum_byte_offset(dsc->t_start, index * dsc->t_bsize);
	
	/*If bad index, fail;*/
	if (bref >= dsc->t_end) {
		return 1;
	}
	
	/*If valid index, complete;*/
	*dst = bref;
	return 0;
	
}


#define BTABLE_ITERATE(dsc, var) \
    for ((var) = (dsc).t_start;\
        (usize)(var) < (usize)(dsc).t_end;\
        *((u8**)(&(var))) += (usize) (dsc).t_bsize\
    )


#endif /*KERNELTK_BTABLE_H*/
