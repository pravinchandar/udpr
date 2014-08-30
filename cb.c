#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cb.h"

void cb_init(cb* cbuff, int elem_size, int num_elements)
{
	cbuff->num_elems = num_elements;
	cbuff->elem_size = elem_size;
	cbuff->elems = malloc(cbuff->num_elems * cbuff->elem_size);
	assert(cbuff->elems != NULL);
	cbuff->w_ptr = cbuff->elems;
	cbuff->r_ptr = cbuff->elems;
	cbuff->l_ptr = cbuff->elems + ((cbuff->num_elems-1) * cbuff->elem_size);
	cbuff->written = 0;
	cbuff->read = 0;
}

void cb_destroy(cb* cbuff)
{
	free(cbuff->elems);
}

/*
 * XXX The write thread is not programmed (yet) to check if the next address is
 * being read or has read by all the reader threads.
 */
void cb_write_elem(cb* cbuff, void* elem)
{
	if (cbuff->written != 0 )
		cb_move_wptr(cbuff);

	memcpy(cbuff->w_ptr, elem, cbuff->elem_size);
	cbuff->written = 1;
}

void cb_read_elem(cb* cbuff, void* elem)
{
	memcpy(elem, cbuff->r_ptr, cbuff->elem_size);
}


/*
 * This function is called by the very last reader thread. Meaning all threads
 * have confirmed to have read the message. So the read pointer can be moved to
 * point to the next element.
 */
void cb_move_rptr(cb* cbuff)
{
	if (cbuff->r_ptr == cbuff->l_ptr)
		cbuff->r_ptr = cbuff->elems;
	else
		cbuff->r_ptr = cbuff->r_ptr + cbuff->elem_size;
}

static void cb_move_wptr(cb* cbuff)
{
	if (cbuff->w_ptr  == cbuff->l_ptr)
		cbuff->w_ptr = cbuff->elems;
	else
		cbuff->w_ptr = cbuff->w_ptr + cbuff->elem_size;
}

