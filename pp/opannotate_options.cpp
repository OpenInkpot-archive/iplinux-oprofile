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

#include "op_exception.h"
#include "opannotate_options.h"
#include "popt_options.h"
#include "cverb.h"

using namespace std;

namespace options {
	bool demangle = true;
	bool smart_demangle;
	string source_dir;
	string output_dir;
	string base_dir;
	string include_file;
	string exclude_file;
	string include_symbol;
	string exclude_symbol;
	bool source;
	bool assembly;
}


namespace {

popt::option options_array[] = {
	popt::option(options::demangle, "demangle", 'd',
		     "demangle GNU C++ symbol names (default on)"),
	popt::option(options::demangle, "no-demangle", '\0',
		     "don't demangle GNU C++ symbol names"),
	popt::option(options::smart_demangle, "smart-demangle", 'D',
		     "demangle GNU C++ symbol names and shrink them"),
	popt::option(options::source_dir, "source-dir", 'd',
		     "base directory of source", "directory name"),
	popt::option(options::source_dir, "output-dir", 'o',
		     "output directory", "directory name"),
	popt::option(options::base_dir, "base-dir", 'b',
		     "FIXME", "directory name"),
	popt::option(options::include_file, "include-file", '\0',
		     "include these comma separated filename", "filenames"),
	popt::option(options::exclude_file, "exclude-file", '\0',
		     "exclude these comma separated filename", "filenames"),
	popt::option(options::include_symbol, "include-symbols", 'i',
		     "include these comma separated symbols", "symbols"),
	popt::option(options::exclude_symbol, "exclude-symbol", 'e',
		     "exclude these comma separated symbols", "symbols"),
	popt::option(options::source, "source", 's', "output source"),
	popt::option(options::assembly, "assembly", 'a', "output assembly"),
};

}  // anonymous namespace


void handle_options(vector<string> const & /*non_options*/)
{
	using namespace options;

	if (!assembly && !source) {
		throw invalid_argument("you must specify at least --source or --assembly\n");
	}
}
