/**
 * @file cg_insert.c
 * Inserting a key-value pair into a callgraph db hash
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ocg_hash.h"

int ocg_insert(samples_ocg_t * hash, ocg_key_t key, ocg_value_t value)
{
	size_t index;
	ocg_index_t new_node;
	ocg_node_t * node;

	index = hash->hash_base[ocg_do_hash(hash, key)];
	while (index) {
		if (index <= 0 || index >= hash->descr->current_size)
			return EXIT_FAILURE;
		node = &hash->node_base[index];
		if (node->key == key) {
			if (node->value + value >= node->value) {
				node->value += value;
			} else {
				/* post profile tools must handle overflow */
				node->value = ~(ocg_value_t)0;
			}
			return EXIT_SUCCESS;
		}

		index = node->next;
	}

	/* no locking is necessary: iteration interface retrieve data through
	 * the node_base array, ocg_hash_add_node() increase current_size but
	 * ocg_travel just ignore node with a zero key so on setting the key
	 * atomically update the node */
	new_node = ocg_hash_add_node(hash);
	if (new_node == OCG_NODE_NR_INVALID) {
		return EXIT_FAILURE;
	}

	node = &hash->node_base[new_node];
	node->value = value;
	node->key = key;

	/* we need to recalculate hash code, hash table has perhaps grown */
	index = ocg_do_hash(hash, key);
	node->next = hash->hash_base[index];
	hash->hash_base[index] = new_node;
	
	return EXIT_SUCCESS;
}
