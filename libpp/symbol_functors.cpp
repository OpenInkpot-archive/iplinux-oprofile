/**
 * @file symbol_functors.cpp
 * Functors for symbol/sample comparison
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "symbol_functors.h"

bool less_symbol::operator()(symbol_entry const & lhs,
			     symbol_entry const & rhs) const
{
	if (lhs.sample.file_loc.image_name != rhs.sample.file_loc.image_name)
		return lhs.sample.file_loc.image_name < 
			rhs.sample.file_loc.image_name;

	if (lhs.sample.file_loc.app_name != rhs.sample.file_loc.app_name)
		return lhs.sample.file_loc.app_name <
			rhs.sample.file_loc.app_name;

	if (lhs.sample.vma != rhs.sample.vma)
		return lhs.sample.vma < rhs.sample.vma;

	if (lhs.name != rhs.name)
		return lhs.name < rhs.name;

	return lhs.size < rhs.size;
}
