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

#include "symbol_sort.h"
#include "opp_symbol.h"
#include "symbol_functors.h"
#include "file_manip.h"

#include <algorithm>

using namespace std;

namespace {

/// compare based on vma value (address)
struct less_by_vma {
	bool operator()(sample_entry const & lhs, sample_entry const & rhs) const {
		return lhs.vma < rhs.vma;
	}

	bool operator()(symbol_entry const * lhs, symbol_entry const * rhs) const {
		return (*this)(lhs->sample, rhs->sample);
	}
};


/// compare based on number of accumulated samples
struct less_by_samples {
	bool operator()(symbol_entry const * lhs, symbol_entry const * rhs) const {
		// sorting by vma when samples count are identical is better
		if (lhs->sample.count != rhs->sample.count)
			return lhs->sample.count > rhs->sample.count;

		return lhs->sample.vma > rhs->sample.vma;
	}
};


/// compare based on name
struct less_by_name {
	bool operator()(symbol_entry const * lhs, symbol_entry const * rhs) const {
		return lhs->name < rhs->name;
	}
};


/// compare based on owning image
struct less_by_image {
	bool operator()(symbol_entry const * lhs, symbol_entry const * rhs) const {
		return lhs->image_name < rhs->image_name;
	}
};

}


void sort_by(std::vector<symbol_entry const *> & syms,
             sort_options const & options)
{
	if (options.sample)
		sort(syms.begin(), syms.end(), less_by_samples());
	if (options.vma)
		stable_sort(syms.begin(), syms.end(), less_by_vma());
	// FIXME: the three below are not correct: the displayed values
	// can be different (short filenames, demangling) so the sort
	// will look wrong
	if (options.symbol)
		stable_sort(syms.begin(), syms.end(), less_by_name());
	if (options.image)
		stable_sort(syms.begin(), syms.end(), less_by_image());
	if (options.debug)
		stable_sort(syms.begin(), syms.end(), less_by_file_loc());
}
