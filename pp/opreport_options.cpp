/**
 * @file opreport_options.cpp
 * Options for opreport tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <vector>
#include <list>
#include <iostream>

#include "op_config.h"
#include "file_manip.h"
#include "parse_cmdline.h"
#include "opreport_options.h"
#include "popt_options.h"
#include "cverb.h"

using namespace std;

namespace options {
	vector<string> symbols;
	bool debug_info;
	bool details;
	double threshold;
	bool threshold_percent;
	bool include_dependent;
	vector<string> sort_by;
	vector<string> ignore_symbols;
	vector<string> exclude_symbols;
	vector<string> image_path;
	vector<string> merge;
	bool no_header;
	bool short_filename;
	bool accumulated;
	bool reverse_sort;
	bool global_percent;
}


namespace {

string threshold;

popt::option options_array[] = {
	// PP:5
	popt::option(options::symbols, "symbols", 'l',
		     "symbols to consider", "comma separated list"),
	popt::option(options::debug_info, "debug-info", 'b',
		     "add source file and line number to output"),
	popt::option(options::details, "details", 'a',
		     "output detailed samples for each symbol"),
	popt::option(threshold, "threshold", 't',
		     "threshold of minimum value before a symbol is printed",
		     "count or percent"),
	popt::option(options::include_dependent, "include-dependent", 'n',
		     "include libs, modules etc."),
	popt::option(options::sort_by, "sort", 's',
		     "sort by", "vma,sample,symbol,debug,image"),
	popt::option(options::ignore_symbols, "ignore-symbols", 'i',
		     "ignore these comma separated symbols", "symbols"),
	popt::option(options::exclude_symbols, "exclude-symbols", 'e',
		     "exclude these comma separated symbols", "symbols"),
	popt::option(options::image_path, "image-path", 'p',
		     "comma separated path to search missing binaries","path"),
	popt::option(options::merge, "merge", 'm',
		     "comma separated list", "cpu,pid,lib"),
	popt::option(options::no_header, "no-header", 'h',
		     "remove all header from output"),
	popt::option(options::short_filename, "short-filename", '\0',
		     "use basename of filename in output"),
	popt::option(options::accumulated, "accumulated", '\0',
		     "percentage field show accumulated count"),
	popt::option(options::reverse_sort, "reverse-sort", 'r',
		     "use reverse sort"),
	popt::option(options::global_percent, "global-percent", '\0',
		     "percentage are not relative to symbol count or image count but whole sample count"),
};

}  // anonymous namespace


void get_options(int argc, char const * argv[])
{
	using namespace options;

	vector<string> non_option_args;

	popt::parse_options(argc, argv, non_option_args);

	set_verbose(verbose);

	parse_cmdline parser;
	handle_non_options(parser, non_option_args);

	list<string> sample_files;

	// FIXME --session, why it's in profile spec ?
	create_file_list(sample_files, OP_SAMPLES_CURRENT_DIR, "*", true);
	for (list<string>::const_iterator it = sample_files.begin();
	     it != sample_files.end();
	     ++it) {
		cout << *it << endl;
	}
}
