/**
 * @file path_filter.cpp
 * Filter paths based on globbed exclude/include list
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

#include "path_filter.h"
#include "string_manip.h"
#include "file_manip.h"

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


bool path_filter::match(std::string const & str) const
{
	string const & base = basename(str);

	// first, if any component of the dir is listed in exclude -> no
	string comp = dirname(str);
	while (!comp.empty() && comp != "/") {
		if (do_match(exclude_pattern, basename(comp)))
			return false;
		// FIXME: test uneccessary, wait a decent testsuite before
		// removing
		if (comp == dirname(comp))
			break;
		comp = dirname(comp);
	}

	// now if the file name is specifically excluded -> no
	if (do_match(exclude_pattern, base))
		return false;

	// now if the file name is specifically included -> yes
	if (do_match(include_pattern, base))
		return true;

	// now if any component of the path is included -> yes
	// note that the include pattern defaults to '*'
	string compi = dirname(str);
	while (!compi.empty() && compi != "/") {
		if (do_match(include_pattern, basename(compi)))
			return true;
		// FIXME see above.
		if (compi == dirname(compi))
			break;
		compi = dirname(compi);
	}

	return false;
}
