/**
 * @file opgprof_options.cpp
 * Options for opgprof tool
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <vector>
#include <list>
#include <iterator>

#include "opgprof_options.h"
#include "popt_options.h"
#include "cverb.h"
#include "profile_spec.h"
#include "partition_files.h"

using namespace std;

scoped_ptr<partition_files> sample_file_partition;

namespace options {
	string gmon_filename = "gmon.out";

	// Ugly, for build only
	bool demangle;
	bool smart_demangle;
}


namespace {

popt::option options_array[] = {
	popt::option(options::gmon_filename, "output-filename", 'o',
	             "output filename, defaults to gmon.out if not specified",
	             "filename"),
};


// FIXME: pass merge_option as parameter and re-use in opreport_options.cpp
// *probably*
bool try_partition_file(profile_spec const & spec, bool exclude_dependent)
{
	list<string> sample_files = spec.generate_file_list(exclude_dependent);

	cverb << "Matched sample files: " << sample_files.size() << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb, "\n"));

	vector<unmergeable_profile>
		unmerged_profile = merge_profile(sample_files);

	cverb << "Unmergeable profile specification:\n";
	copy(unmerged_profile.begin(), unmerged_profile.end(),
	     ostream_iterator<unmergeable_profile>(cverb, "\n"));

	if (unmerged_profile.empty() && !exclude_dependent) {
		cerr << "No samples files found: profile specification too "
		     << "strict ?" << endl;
		exit(EXIT_FAILURE);
	}

	if (unmerged_profile.size() > 1) {
		cerr << "Too many unmergeable profile specifications." << endl;
		cerr << "Use event:xxxx and/or count:yyyyy to restrict "
		     << "sample files considered.\n" << endl;
		exit(EXIT_FAILURE);
	}

	// opgprof merge all by default
	merge_option merge_by;
	merge_by.cpu = true;
	merge_by.lib = true;
	merge_by.tid = true;
	merge_by.tgid = true;
	merge_by.unitmask = true;

	sample_file_partition.reset(
		new partition_files(sample_files, merge_by));

	// FIXME: above check should be suppressed
	if (sample_file_partition->nr_set() > 1) {
		// FIXME: we can do a lot better in telling the user the
		// *exact* problem: sample_file_partition::report()
		// or whatever
		cerr << "Too many unmerged profile specifications." << endl;
		cerr << "use event:xxxx and/or count:yyyyy to restrict "
		     << "samples files considered.\n" << endl;
		exit(EXIT_FAILURE);
	}

	return sample_file_partition->nr_set() == 1;
}

}  // anonymous namespace


void handle_options(vector<string> const & non_options)
{
	cverb << "output filename: " << options::gmon_filename << endl;

	profile_spec const spec =
		profile_spec::create(non_options, options::extra_found_images);

	// we do a first try with exclude-dependent if it fails we include
	// dependent. First try should catch "opgrof /usr/bin/make" whilst
	// the second catch "opgprof /lib/libc-2.2.5.so"
	if (!try_partition_file(spec, true)) {
		try_partition_file(spec, false);
	}
}
