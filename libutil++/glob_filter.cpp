/**
 * @file glob_filter.cpp
 * Filter strings based on globbed exclude/include list
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <fnmatch.h>

#include <string>
#include <vector>

#include "glob_filter.h"
#include "string_manip.h"

using namespace std;

// FIXME: use find_if

namespace {

/// return true if the string is globbed by any of the patterns
bool do_match(vector<string> const & patterns, string const & str)
{
	bool found = false;
	for (size_t i = 0 ; i < patterns.size() && !found; ++i) {
		if (fnmatch(patterns[i].c_str(), str.c_str(), 0) != FNM_NOMATCH)
			found = true;
	}

	return found;
}

};


bool glob_filter::match(std::string const & str) const
{
	if (do_match(exclude_pattern, str))
		return false;

	if (include_pattern.empty() || do_match(include_pattern, str))
		return true;

	return false;
}
