/**
 * @file oocg_hash.h
 * This file contains various definitions and interface for management
 * of in-memory, through mmaped file, growable hash table for the
 * call-graph files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef OCG_HASH_H
#define OCG_HASH_H

#include <stddef.h>
#include <stdint.h>

/** the type of key (actually composed of two pc value offsets) */
typedef uint64_t ocg_key_t;
/** the type of an information in the database */
typedef unsigned int ocg_value_t;
/** the type of index (node number), list are implemented through index */
typedef unsigned int ocg_index_t;
/** the type store node number */
typedef ocg_index_t ocg_node_nr_t;
/** store the hash mask, hash table size are always power of two */
typedef ocg_index_t ocg_hash_mask_t;

/* there is (bucket factor * nr node) entry in hash table, this can seem
 * excessive but hash coding eip don't give a good distributions and our
 * goal is to get a O(1) amortized insert time. bucket factor must be a
 * power of two. FIXME: see big comment in ocg_hash_add_node, you must
 * re-enable zeroing hash table if BUCKET_FACTOR > 2 (roughly exact, you
 * want to read the comment in ocg_add_hash_node() if you tune this define)
 */
#define BUCKET_FACTOR 1

/** a db hash node */
typedef struct {
	ocg_key_t key;			/**< eip */
	ocg_value_t value;		/**< samples count */
	ocg_index_t next;		/**< next entry for this bucket */
} ocg_node_t;

/** the minimal information which must be stored in the file to reload
 * properly the data base, following this header is the node array then
 * the hash table (when growing we avoid to copy node array)
 */
typedef struct {
	ocg_node_nr_t size;		/**< in node nr (power of two) */
	ocg_node_nr_t current_size;	/**< nr used node + 1, node 0 unused */
	int padding[6];			/**< for padding and future use */
} ocg_descr_t;

/** a "database". this is an in memory only description.
 *
 * We allow to manage a database inside a mapped file with an "header" of
 * unknown size so ocg_open get a parameter to specify the size of this header.
 * A typical use is:
 *
 * struct header { int etc; ... };
 * ocg_open(&hash, filename, OCG_RW, sizeof(header));
 * so on this library have no dependency on the header type.
 *
 * the internal memory layout from base_memory is:
 *  the unknown header (sizeof_header)
 *  ocg_descr_t
 *  the node array: (descr->size * sizeof(ocg_node_t) entries
 *  the hash table: array of ocg_index_t indexing the node array 
 *    (descr->size * BUCKET_FACTOR) entries
 *
 * the err_msg field is cleared by ocg_clear_error(). This field record
 * the first error encountered. It's a fatal error if an error occur and this
 * field is non NULL
 */
typedef struct {
	ocg_node_t * node_base;		/**< base memory area of the page */
	ocg_index_t * hash_base;	/**< base memory of hash table */
	ocg_descr_t * descr;		/**< the current state of database */
	ocg_hash_mask_t hash_mask;	/**< == descr->size - 1 */
	unsigned int sizeof_header;	/**< from base_memory to odb header */
	unsigned int offset_node;	/**< from base_memory to node array */
	void * base_memory;		/**< base memory of the maped memory */
	int fd;				/**< mmaped memory file descriptor */
	char const * err_msg;		/**< *first* error message */
} samples_ocg_t;

#ifdef __cplusplus
extern "C" {
#endif

/* db_manage.c */

/** how to open the DB hash file */
enum ocg_rw {
	OCG_RDONLY = 0,	/**< open for read only */
	OCG_RDWR = 1	/**< open for read and/or write */
};

/**
 * ocg_init - initialize a hash struct
 * @param hash the hash object to init
 */
void ocg_init(samples_ocg_t * hash);

/**
 * ocg_open - open a ODB hash file
 * @param hash the data base object to setup
 * @param filename the filename where go the maped memory
 * @param rw \enum OCG_RW if opening for writing, else \enum OCG_RDONLY
 * @param sizeof_header size of the file header if any
 *
 * The sizeof_header parameter allows the data file to have a header
 * at the start of the file which is skipped.
 * ocg_open() always preallocate a few number of pages.
 * returns EXIT_SUCCESS on success, EXIT_FAILURE on failure
 * on failure hash->err_msg contains a pointer to a malloced string
 * containing an error message.
 */
int ocg_open(samples_ocg_t * hash, char const * filename, enum ocg_rw rw, size_t sizeof_header);

/** Close the given ODB hash */
void ocg_close(samples_ocg_t * hash);

/** clear the last occured error */
void ocg_clear_error(samples_ocg_t * hash);

/** issue a msync on the used size of the mmaped file */
void ocg_sync(samples_ocg_t const * hash);

/** add a page returning its index. Take care all page pointer can be
 * invalidated by this call !
 * returns the index of the created node on success or
 * OCG_NODE_NR_INVALID on failure
 * on failure hash->err_msg contains a pointer to a malloced string
 * containing an error message.
 */
ocg_node_nr_t ocg_hash_add_node(samples_ocg_t * hash);

/** "immpossible" node number to indicate an error from ocg_hash_add_node() */
#define OCG_NODE_NR_INVALID ((ocg_node_nr_t)-1)

/* ocg_debug.c */
/** check than the hash is well build */
int ocg_check_hash(const samples_ocg_t * hash);
/** display the item in hash table */
void ocg_display_hash(samples_ocg_t const * hash);
/** same as above, do not travel through the hash table but display raw node */
void ocg_raw_display_hash(samples_ocg_t const * hash);

/* ocg_stat.c */
typedef struct ocg_hash_stat_t ocg_hash_stat_t;
ocg_hash_stat_t * ocg_hash_stat(samples_ocg_t const * hash);
void ocg_hash_display_stat(ocg_hash_stat_t const * stats);
void ocg_hash_free_stat(ocg_hash_stat_t * stats);

/* ocg_insert.c */
/** insert info at key, if key already exist the info is added to the
 * existing samples
 * returns EXIT_SUCCESS on success, EXIT_FAILURE on failure
 * on failure hash->err_msg contains a pointer to a malloced string
 * containing an error message.
 */
int ocg_insert(samples_ocg_t * hash, ocg_key_t key, ocg_value_t value);

/* ocg_travel.c */
/** the call back type to pass to travel() */
typedef void (*samples_ocg_travel_callback)(ocg_key_t key, ocg_value_t value, void * data);
/** iterate through key in range [first, last[ passing it to callback,
 * data is optional user data to pass to the callback */
/* caller woukd use the more efficient ocg_get_iterator() interface. This
 * interface is for debug purpose and is likely to be removed in future */
void samples_ocg_travel(samples_ocg_t const * hash, ocg_key_t first, ocg_key_t last,
	       samples_ocg_travel_callback callback, void * data);
/**
 * return a base pointer to the node array and number of node in this array
 * caller then will iterate through:
 *
 * ocg_node_nr_t node_nr, pos;
 * ocg_node_t * node = ocg_get_iterator(hash, &node_nr);
 *	for ( pos = 0 ; pos < node_nr ; ++pos) {
 *		if (node[pos].key) {
 *			// do something
 *		}
 *	}
 *
 *  note than caller *must* filter nil key.
 */
ocg_node_t * ocg_get_iterator(samples_ocg_t const * hash, ocg_node_nr_t * nr);

static __inline unsigned int ocg_do_hash(samples_ocg_t const * hash, ocg_key_t value)
{
	// FIXME: update for ocg !
	/* FIXME: better hash for eip value, needs to instrument code
	 * and do a lot of tests ... */
	return ((value << 0) ^ (value >> 35)) & hash->hash_mask;
}

/** not a part of the public interface: set error message to error. Fatal error
 * occur if  hash->error_msg != NULL
 */
void ocg_set_error(samples_ocg_t * hash, char const * err_msg);

#ifdef __cplusplus
}
#endif

#endif /* !OCG_HASH_H */
