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
#include <algorithm>
#include <set>
#include <iterator>

#include "op_config.h"
#include "file_manip.h"
#include "parse_cmdline.h"
#include "split_sample_filename.h"
#include "opreport_options.h"
#include "popt_options.h"
#include "file_manip.h"
#include "partition_files.h"
#include "cverb.h"

using namespace std;

scoped_ptr<partition_files> sample_file_partition;

namespace options {
	bool symbols;
	bool debug_info;
	bool details;
	double threshold;
	bool percent_threshold;
	bool include_dependent;
	bool hide_dependent;
	bool sort_by_vma;
	bool sort_by_sample;
	bool sort_by_symbol;
	bool sort_by_debug;
	bool sort_by_image;
	vector<string> ignore_symbols;
	vector<string> exclude_symbols;
	vector<string> image_path;
	merge_option merge_by;
	bool no_header;
	bool short_filename;
	bool accumulated;
	bool reverse_sort;
	bool global_percent;
}


namespace {

string threshold;
vector<string> merge;
vector<string> sort_by;

popt::option options_array[] = {
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
	popt::option(options::ignore_symbols, "ignore-symbols", 'i',
		     "ignore these comma separated symbols", "symbols"),
	popt::option(options::exclude_symbols, "exclude-symbols", 'e',
		     "exclude these comma separated symbols", "symbols"),
	popt::option(options::image_path, "image-path", 'p',
		     "comma separated path to search missing binaries","path"),
	popt::option(merge, "merge", 'm',
		     "comma separated list", "cpu,pid,lib"),
	popt::option(options::no_header, "no-header", '\0',
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
void handle_threshold()
{
	if (threshold.length()) {
		double value;
		istringstream ss(threshold);
		if (ss >> value) {
			options::threshold = value;
			char ch;
			if (ss >> ch && ch == '%') {
				options::percent_threshold = true;
			}
		}
	}

	cverb << options::threshold << (options::percent_threshold ? "%" : "")
	      << endl;;
}

// FIXME: separate file if reused
void handle_sort_option()
{
	if (sort_by.empty()) {
		// PP:5.14 sort default to sample
		sort_by.push_back("sample");
	}

	for (size_t i = 0; i < sort_by.size(); ++i) {
		if (sort_by[i] == "vma") {
			options::sort_by_vma = true;
		} else if (sort_by[i] == "sample") {
			options::sort_by_sample = true;
		} else if (sort_by[i] == "symbol") {
			options::sort_by_symbol = true;
		} else if (sort_by[i] == "debug") {
			options::sort_by_debug = true;
		} else if (sort_by[i] == "image") {
			options::sort_by_image = true;
		} else {
			cerr << "unknown sort option: " << sort_by[i] << endl;
			exit(EXIT_FAILURE);
		}
	}
}

// FIXME: separate file if reused
void handle_merge_option()
{
	for (size_t i = 0; i < merge.size(); ++i) {
		if (merge[i] == "cpu") {
			options::merge_by.merge_cpu = true;
		} else if (merge[i] == "tid") {
			options::merge_by.merge_tid = true;
		} else if (merge[i] == "tgid") {
			// PP:5.21 tgid merge imply tid merging.
			options::merge_by.merge_tgid = true;
			options::merge_by.merge_tid = true;
		} else if (merge[i] == "lib") {
			options::merge_by.merge_lib = true;
		} else if (merge[i] == "unitmask") {
			options::merge_by.merge_unitmask = true;
		} else if (merge[i] == "all") {
			options::merge_by.merge_cpu = true;
			options::merge_by.merge_lib = true;
			options::merge_by.merge_tid = true;
			options::merge_by.merge_tgid = true;
			options::merge_by.merge_unitmask = true;
		} else {
			cerr << "unknown merge option: " << merge[i] << endl;
			exit(EXIT_FAILURE);
		}
	}
}


// FIXME: separate file
vector<string> filter_session(vector<string> const & session,
			      vector<string> const & session_exclude)
{
	vector<string> result(session);

	if (result.empty()) {
		result.push_back("current");
	}

	for (size_t i = 0 ; i < session_exclude.size() ; ++i) {
		// FIXME: would we use fnmatch on each item, are we allowed
		// to --session=current* ?
		vector<string>::iterator it = find(result.begin(), 
						   result.end(),
						   session_exclude[i]);
		if (it != result.end()) {
			result.erase(it);
		}
	}

	return result;
}

bool valid_candidate(string const & filename, parse_cmdline const & parser)
{
	if (parser.match(filename)) {
		if (!options::include_dependent &&
		    filename.find("{dep}") != string::npos)
			return false;
		return true;
	}

	return false;
}

// FIXME: in a separate file
list<string> matching_sample_filename(parse_cmdline const & parser)
{
	set<string> unique_files;

	vector<string> session = filter_session(parser.get_session(),
						parser.get_session_exclude());

	for (size_t i = 0; i < session.size(); ++i) {
		if (session[i].empty())
			continue;

		string base_dir;
		if (session[i][0] != '.' && session[i][0] != '/')
			base_dir = OP_SAMPLES_DIR;
		base_dir += session[i];

		base_dir = relative_to_absolute_path(base_dir);

		list<string> files;
		create_file_list(files, base_dir, "*", true);

		list<string>::const_iterator it;
		for (it = files.begin(); it != files.end(); ++it) {
			if (valid_candidate(*it, parser)) {
				unique_files.insert(*it);
			}
		}
	}

	list<string> result;
	copy(unique_files.begin(), unique_files.end(), back_inserter(result));

	return result;
}

}  // anonymous namespace


void get_options(int argc, char const * argv[])
{
	using namespace options;

	vector<string> non_option_args;

	popt::parse_options(argc, argv, non_option_args);

	set_verbose(verbose);

	if (options::details)
		options::symbols = true;

	handle_threshold();

	handle_sort_option();

	handle_merge_option();

	parse_cmdline parser = handle_non_options(non_option_args);

	list<string> sample_files = matching_sample_filename(parser);

	cverb << "Matched sample files: " << sample_files.size() << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb, "\n"));

	// FIXME: crash here if we do opreport /usr/bin/doesntexist

	vector<unmergeable_profile>
		unmerged_profile = merge_profile(sample_files);

	cverb << "Unmergeable profile specification:\n";
	copy(unmerged_profile.begin(), unmerged_profile.end(),
	     ostream_iterator<unmergeable_profile>(cverb, "\n"));


	if (unmerged_profile.size() > 1) {
		// quick and dirty check for now
		cerr << "Can't handle multiple counter!" << endl;
		cerr << "use event:xxxx and/or count:yyyyy to restrict "
		     << "samples files set considered\n" << endl;
		exit(EXIT_FAILURE);
	}


	sample_file_partition.reset(
		new partition_files(sample_files, options::merge_by));
}
