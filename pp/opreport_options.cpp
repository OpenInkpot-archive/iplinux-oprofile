/**
 * @file opreport_options.cpp
 * Options for opreport tool
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <vector>
#include <list>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <fstream>

#include "profile_spec.h"
#include "opreport_options.h"
#include "popt_options.h"
#include "file_manip.h"
#include "partition_files.h"
#include "cverb.h"

using namespace std;

scoped_ptr<partition_files> sample_file_partition;

namespace options {
	bool demangle = true;
	bool smart_demangle;
	bool symbols;
	bool debug_info;
	bool details;
	double threshold;
	bool include_dependent;
	bool hide_dependent;
	bool sort_by_vma;
	bool sort_by_sample;
	bool sort_by_symbol;
	bool sort_by_debug;
	bool sort_by_image;
	string_filter symbol_filter;
	merge_option merge_by;
	bool show_header = true;
	bool short_filename;
	bool accumulated;
	bool reverse_sort;
	bool global_percent;
}


namespace {

string threshold;
string outfile;
vector<string> mergespec;
vector<string> sort_by;
vector<string> exclude_symbols;
vector<string> include_symbols;

popt::option options_array[] = {
	popt::option(options::demangle, "demangle", 'd',
		     "demangle GNU C++ symbol names (default on)"),
	popt::option(options::demangle, "no-demangle", '\0',
		     "don't demangle GNU C++ symbol names"),
	popt::option(options::smart_demangle, "smart-demangle", 'D',
		     "demangle GNU C++ symbol names and shrink them"),
	popt::option(outfile, "output-file", 'o',
	             "output to the given filename", "file"),
	// PP:5
	popt::option(options::symbols, "symbols", 'l',
		     "list all symbols"),
	popt::option(options::debug_info, "debug-info", 'b',
		     "add source file and line number to output"),
	popt::option(options::details, "details", 'a',
		     "output detailed samples for each symbol"),
	popt::option(threshold, "threshold", 't',
		     "threshold of minimum value before a symbol is printed",
		     "count or percent"),
	popt::option(options::include_dependent, "include-dependent", 'n',
		     "include libs, modules etc."),
	popt::option(options::hide_dependent, "hide-dependent", 'h',
		     "include libs, modules in %-age count but hide them in output"),
	popt::option(sort_by, "sort", 's',
		     "sort by", "vma,sample,symbol,debug,image"),
	popt::option(exclude_symbols, "exclude-symbols", 'e',
		     "exclude these comma separated symbols", "symbols"),
	popt::option(include_symbols, "include-symbols", 'i',
		     "include these comma separated symbols", "symbols"),
	popt::option(mergespec, "merge", 'm',
		     "comma separated list", "cpu,pid,lib"),
	popt::option(options::show_header, "no-header", '\0',
		     "remove all header from output"),
	popt::option(options::short_filename, "short-filename", '\0',
		     "use basename of filename in output"),
	popt::option(options::accumulated, "accumulated", '\0',
		     "percentage field show accumulated count"),
	popt::option(options::reverse_sort, "reverse-sort", 'r',
		     "use reverse sort"),
	popt::option(options::global_percent, "global-percent", '\0',
		     "percentage are not relative to symbol count or image "
		     "count but total sample count"),
};


// FIXME: separate file if reused
void handle_sort_option()
{
	if (sort_by.empty()) {
		// PP:5.14 sort default to sample
		sort_by.push_back("sample");
	}

	vector<string>::const_iterator cit = sort_by.begin();
	vector<string>::const_iterator end = sort_by.end();

	for (; cit != end; ++cit) {
		if (*cit == "vma") {
			options::sort_by_vma = true;
		} else if (*cit == "sample") {
			options::sort_by_sample = true;
		} else if (*cit == "symbol") {
			options::sort_by_symbol = true;
		} else if (*cit == "debug") {
			options::sort_by_debug = true;
		} else if (*cit == "image") {
			options::sort_by_image = true;
		} else {
			cerr << "unknown sort option: " << *cit << endl;
			exit(EXIT_FAILURE);
		}
	}
}

// FIXME: separate file if reused
void handle_merge_option()
{
	vector<string>::const_iterator cit = mergespec.begin();
	vector<string>::const_iterator end = mergespec.end();

	for (; cit != end; ++cit) {
		if (*cit == "cpu") {
			options::merge_by.cpu = true;
		} else if (*cit == "tid") {
			options::merge_by.tid = true;
		} else if (*cit == "tgid") {
			// PP:5.21 tgid merge imply tid merging.
			options::merge_by.tgid = true;
			options::merge_by.tid = true;
		} else if (*cit == "lib") {
			options::merge_by.lib = true;
		} else if (*cit == "unitmask") {
			options::merge_by.unitmask = true;
		} else if (*cit == "all") {
			options::merge_by.cpu = true;
			options::merge_by.lib = true;
			options::merge_by.tid = true;
			options::merge_by.tgid = true;
			options::merge_by.unitmask = true;
		} else {
			cerr << "unknown merge option: " << *cit << endl;
			exit(EXIT_FAILURE);
		}
	}
}


// FIXME: separate file if reused
void handle_output_file()
{
	if (outfile.empty())
		return;

	static ofstream os(outfile.c_str());
	if (!os) {
		cerr << "Couldn't open \"" << outfile
		     << "\" for writing." << endl;
		exit(EXIT_FAILURE);
	}

	cout.rdbuf(os.rdbuf());
}

} // namespace anon


void handle_options(vector<string> const & non_options)
{
	using namespace options;

	if (options::details)
		options::symbols = true;

	if (!::threshold.empty())
		options::threshold = handle_threshold(::threshold);

	handle_sort_option();
	handle_merge_option();
	handle_output_file();

	options::symbol_filter = string_filter(include_symbols, exclude_symbols);

	profile_spec spec = profile_spec::create(non_options);

	list<string> sample_files = spec.generate_file_list(include_dependent);

	cverb << "Matched sample files: " << sample_files.size() << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb, "\n"));

	vector<unmergeable_profile>
		unmerged_profile = merge_profile(sample_files);

	cverb << "Unmergeable profile specification:\n";
	copy(unmerged_profile.begin(), unmerged_profile.end(),
	     ostream_iterator<unmergeable_profile>(cverb, "\n"));

	if (unmerged_profile.empty()) {
		cerr << "No samples files found: profile specification too "
		     << "strict ?" << endl;
		exit(EXIT_FAILURE);
	}

	if (unmerged_profile.size() > 1) {
		// quick and dirty check for now
		cerr << "Can't handle multiple counters." << endl;
		cerr << "use event:xxxx and/or count:yyyyy to restrict "
		     << "samples files set considered\n" << endl;
		exit(EXIT_FAILURE);
	}

	sample_file_partition.reset(
		new partition_files(sample_files, options::merge_by));
}
