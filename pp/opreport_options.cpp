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

#include "op_config.h"
#include "file_manip.h"
#include "parse_cmdline.h"
#include "split_sample_filename.h"
#include "opreport_options.h"
#include "popt_options.h"
#include "file_manip.h"
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
	bool merge_cpu;
	bool merge_lib;
	bool merge_tid;
	bool merge_tgid;
	bool merge_unitmask;
	bool no_header;
	bool short_filename;
	bool accumulated;
	bool reverse_sort;
	bool global_percent;
}


namespace {

string threshold;
vector<string> merge;

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
void handle_merge_option()
{
	for (size_t i = 0; i < merge.size(); ++i) {
		if (merge[i] == "cpu") {
			options::merge_cpu = true;
		} else if (merge[i] == "tid") {
			options::merge_tid = true;
		} else if (merge[i] == "tgid") {
			options::merge_tgid = true;
			options::merge_tid = true;
		} else if (merge[i] == "lib") {
			options::merge_lib = true;
		} else if (merge[i] == "unitmask") {
			options::merge_unitmask = true;
		} else {
			cerr << "unknown merge options: " << merge[i] << endl;
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

struct unmergeable_profile {
	std::string event;
	std::string count;

	unmergeable_profile(std::string const & event_,
			    std::string const & count_)
		:
		event(event_),
		count(count_)
		{
		}

	bool operator<(unmergeable_profile const & rhs) const {
		return event < rhs.event ||
			(event == rhs.event && count < rhs.count);
	}
};

ostream & operator<<(ostream & out, unmergeable_profile const & lhs)
{
	out << lhs.event << " " << lhs.count;
	return out;
}

vector<unmergeable_profile> merge_profile(list<string> const & files)
{
	set<unmergeable_profile> spec_set;

	split_sample_filename model = split_sample_file(*files.begin());

	list<string>::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		split_sample_filename spec = split_sample_file(*it);
		spec_set.insert(unmergeable_profile(spec.event, spec.count));
	}

	vector<unmergeable_profile> result;
	copy(spec_set.begin(), spec_set.end(), back_inserter(result));

	return result;
}

}  // anonymous namespace


void get_options(int argc, char const * argv[])
{
	using namespace options;

	vector<string> non_option_args;

	popt::parse_options(argc, argv, non_option_args);

	set_verbose(verbose);

	handle_merge_option();

	parse_cmdline parser;
	handle_non_options(parser, non_option_args);

	list<string> sample_files = matching_sample_filename(parser);

	if (verbose) {
		cout << "Matched sample files:\n";
		copy(sample_files.begin(), sample_files.end(),
		     ostream_iterator<string>(cout, "\n"));
	}

	vector<unmergeable_profile>
		unmerged_profile = merge_profile(sample_files);
	if (unmerged_profile.size() > 1) {
		cerr << "incompatible profile specification:\n";
		copy(unmerged_profile.begin(), unmerged_profile.end(),
		     ostream_iterator<unmergeable_profile>(cerr, "\n"));
		exit(EXIT_FAILURE);
	}

	if (verbose) {
		cerr << "unmergeable profile specification:\n";
		copy(unmerged_profile.begin(), unmerged_profile.end(),
		     ostream_iterator<unmergeable_profile>(cout, "\n"));
	}

	// now the behavior depend on option and number of different
	// specification coming from the samples files.
}
