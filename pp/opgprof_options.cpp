/**
 * @file opgprof_options.cpp
 * Options for opgprof tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <vector>
#include <list>

#include "opgprof_options.h"
#include "popt_options.h"
#include "cverb.h"
#include "parse_cmdline.h"
#include "partition_files.h"

using namespace std;

scoped_ptr<partition_files> sample_file_partition;

namespace options {
	string gmon_filename = "gmon.out";
	alt_filename_t alternate_filename;
}


namespace {

vector<string> image_path;

popt::option options_array[] = {
	popt::option(options::gmon_filename, "output-filename", 'o',
		     "output filename, default to gmon.out if not specified",
		     "filename"),
	popt::option(image_path, "image-path", 'p',
		     "comma separated path to search missing binaries","path"),
};


// FIXME: pass merge_option as parameter and re-use in opreport_options.cpp
// *probably*
bool try_partition_file(parse_cmdline const & parser,  bool include_dependent)
{
	list<string> sample_files =
		select_sample_filename(parser, include_dependent);

	cverb << "Matched sample files: " << sample_files.size() << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb, "\n"));

	vector<unmergeable_profile>
		unmerged_profile = merge_profile(sample_files);

	cverb << "Unmergeable profile specification:\n";
	copy(unmerged_profile.begin(), unmerged_profile.end(),
	     ostream_iterator<unmergeable_profile>(cverb, "\n"));

	if (unmerged_profile.empty() && include_dependent == true) {
		cerr << "no samples files found: try running opcontrol --dump"
		     << endl;
		exit(EXIT_FAILURE);
	}

	if (unmerged_profile.size() > 1) {
		cerr << "too many unmergeable profile specification" << endl;
		cerr << "use event:xxxx and/or count:yyyyy to restrict "
		     << "samples files set considered\n" << endl;
		exit(EXIT_FAILURE);
	}

	// opgprof merge all by default
	merge_option merge_by;
	merge_by.merge_cpu = true;
	merge_by.merge_lib = true;
	merge_by.merge_tid = true;
	merge_by.merge_tgid = true;
	merge_by.merge_unitmask = true;

	sample_file_partition.reset(
		new partition_files(sample_files, merge_by));

	// FIXME: above check check would be suppressed
	if (sample_file_partition->nr_set() > 1) {
		cerr << "too many unmerged profile specification" << endl;
		cerr << "use event:xxxx and/or count:yyyyy to restrict "
		     << "samples files set considered\n" << endl;
		exit(EXIT_FAILURE);
	}

	return sample_file_partition->nr_set() == 1;
}

}  // anonymous namespace


void get_options(int argc, char const * argv[])
{
	using namespace options;

	vector<string> non_option_args;

	popt::parse_options(argc, argv, non_option_args);

	set_verbose(verbose);

	cverb << "output filename: " << options::gmon_filename << endl;

	add_to_alternate_filename(alternate_filename, image_path);

	parse_cmdline parser = handle_non_options(non_option_args);

	// we do a first try w/o include-dependent if it fails we include
	// dependent. First try should catch "opgrof /usr/bin/make" whilst
	// the second catch "opgprof /lib/libc-2.2.5.so"
	if (!try_partition_file(parser, false)) {
		try_partition_file(parser, true);
	}
}
