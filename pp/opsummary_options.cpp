/**
 * @file opsummary_options.cpp
 * Options for opsummary tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <sstream>

#include "opsummary_options.h"
#include "popt_options.h"
#include "format_output.h"
#include "cverb.h"

using namespace std;

namespace options {
	bool symbol_summary;
	bool debug_info;
	bool details;
	outsymbflag output_format_flags;
	bool include_dependant;
}


namespace {

string output_format;

popt::option options_array[] = {
	popt::option(options::symbol_summary, "symbol-summary", 'l',
		     "list samples by symbol"),
	popt::option(options::debug_info, "debug-info", 'b',
		     "add source file and line number to output"),
	popt::option(options::details, "details", 'a',
		     "output detailed samples for each symbol"),
	popt::option(output_format, "output", 'o',
		     "choose the output format", "output format strings"),
	// FIXME: implement --sort as in PP:5.14 or change the spec ?
	// FIXME: implement merge (PP:8.4 under specified ?)
	popt::option(options::include_dependant, "include-dependant", 'n',
		     "include shared libraries, modules and kernel samples"),
};


string format_help_string()
{
	ostringstream format_help;
	format_help << endl;
	format_output::show_help(format_help);
	// PP:8.3
	format_help << "default format is vspni, e format is added with -n "
		    << "option" << endl;

	return format_help.str();
}

}  // anonymous namespace


void get_options(int argc, char const * argv[])
{
	using namespace options;

	vector<string> tag_value_arguments;

	string format_help = format_help_string();

	popt::parse_options(argc, argv, tag_value_arguments, format_help);

	set_verbose(verbose);
}
