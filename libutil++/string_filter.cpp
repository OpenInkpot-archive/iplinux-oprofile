/**
 * @file string_filter.cpp
 * Filter strings based on exclude/include list
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "string_filter.h"
#include "string_manip.h"

using namespace std;

// FIXME: use find_if

namespace {

/// return true if the string matches any of the patterns
bool do_match(vector<string> const & patterns, string const & str)
{
	bool found = false;
	for (size_t i = 0 ; i < patterns.size() && !found; ++i) {
		if (patterns[i] == str)
			found =  true;
	}

	return found;
}

};


string_filter::string_filter(string const & include_patterns,
                             string const & exclude_patterns)
{
	separate_token(include_pattern, include_patterns, ',');
	separate_token(exclude_pattern, exclude_patterns, ',');
}


string_filter::string_filter(vector<string> const & include_patterns,
                             vector<string> const & exclude_patterns)
	: include_pattern(include_patterns),
	exclude_pattern(exclude_patterns)
{
}


// FIXME: PP reference
bool string_filter::match(std::string const & str) const
{
	if (do_match(exclude_pattern, str))
		return false;

	if (include_pattern.empty() || do_match(include_pattern, str))
		return true;

	return false;
}
