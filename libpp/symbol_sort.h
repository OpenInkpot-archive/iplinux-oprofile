/**
 * @file symbol_sort.h
 * Sorting symbols
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef SYMBOL_SORT_H
#define SYMBOL_SORT_H

#include <vector>

class symbol_entry;

struct sort_options {
	sort_options()
		: vma(false), sample(false), symbol(false),
		  image(false), debug(false) {}
	bool vma;
	bool sample;
	bool symbol;
	bool image;
	bool debug;
};

/**
 * Sort the vector by the given criteria.
 */
void sort_by(std::vector<symbol_entry const *> & syms,
             sort_options const & options);

#endif // SYMBOL_SORT_H
