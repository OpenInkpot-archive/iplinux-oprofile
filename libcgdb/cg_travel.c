/**
 * @file cg_travel.c
 * Inspection of a callgraph db
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include "ocg_hash.h"

static void do_travel(samples_ocg_t const * hash, ocg_key_t first, ocg_key_t last,
		samples_ocg_travel_callback callback, void * data)
{
	size_t pos;
	for (pos = 1; pos < hash->descr->current_size ; ++pos) {
		ocg_node_t const * node = &hash->node_base[pos];
		/* look ocg_insert about locking rationale and the need
		 * to never pass to callback() a 0 key */
		if (node->key >= first && node->key < last && node->key) {
			callback(node->key, node->value, data);
		}
	}
}
 

void samples_ocg_travel(samples_ocg_t const * hash, ocg_key_t first, ocg_key_t last,
		samples_ocg_travel_callback callback, void * data)
{
	do_travel(hash, first, last, callback, data);
}


ocg_node_t * ocg_get_iterator(samples_ocg_t const * hash, ocg_node_nr_t * nr)
{
	/* node zero is unused */
	*nr = hash->descr->current_size - 1;
	return hash->node_base + 1;
}
