/**
 * @file opannotate_options.cpp
 * Options for opannotate tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <vector>

#include "opannotate_options.h"
#include "popt_options.h"
#include "cverb.h"

using namespace std;

namespace options {
	bool demangle = true;
	bool smart_demangle;
}


namespace {

popt::option options_array[] = {
	popt::option(options::demangle, "demangle", 'd',
		     "demangle GNU C++ symbol names (default on)"),
	popt::option(options::demangle, "no-demangle", '\0',
		     "don't demangle GNU C++ symbol names"),
	popt::option(options::smart_demangle, "smart-demangle", 'D',
		     "demangle GNU C++ symbol names and shrink them"),
};

}  // anonymous namespace


void get_options(int argc, char const * argv[])
{
	using namespace options;

	vector<string> non_option_args;

	popt::parse_options(argc, argv, non_option_args);

	set_verbose(verbose);
}
