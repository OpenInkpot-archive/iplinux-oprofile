/**
 * @file opreport.cpp
 * Implement opreport utility
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <iostream>
#include <vector>
#include <algorithm>

#include "split_sample_filename.h"
#include "opreport_options.h"
#include "profile.h"
#include "partition_files.h"

using namespace std;

namespace {

struct files_count {
	size_t count;
	string filename;
};

struct compare_files_count {
	bool operator()(files_count const & lhs, files_count const & rhs) const;
};

bool compare_files_count::operator()(files_count const & lhs,
				     files_count const & rhs) const
{
	return options::reverse_sort 
		? lhs.count < rhs.count
		: rhs.count < lhs.count;
}

size_t counts(partition_files::filename_set const & files)
{
	size_t count = 0;

	partition_files::filename_set::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		profile_t samples(*it);

		count += samples.accumulate_samples(0, ~0);
	}

	return count;
}

void output_files_count(partition_files const & files)
{
	vector<files_count> set_file_count;

	for (size_t i = 0 ; i < files.nr_set(); ++i) {
		partition_files::filename_set const & file_set = files.set(i);

		files_count temp;
		temp.count = counts(file_set);
		temp.filename = *file_set.begin();

		set_file_count.push_back(temp);
	}

	sort(set_file_count.begin(), set_file_count.end(),
	     compare_files_count());

	vector<files_count>::const_iterator it;
	for (it = set_file_count.begin(); it != set_file_count.end(); ++it) {
		split_sample_filename sp = split_sample_file(it->filename);

		// FIXME: the way we must show or if we need to show lib_image
		// depends on --merge=lib --include-dependent

		// FIXME: do we need a new option in pp_interface like -k
		// option of op_time ? This option is not compatible with
		// --merge=lib and have no effect if !--include-dependent
		// this option will allow three way output, e.g. for /bin/bash:
		// show only /bin/bash count (!--include-dependent && !-k)
		// show only /bin/bash count but this count include shared
		// libs (--include-dependent && !-k)
		// show count for /bin/bash and sub-count for each shared
		// library (--include-dependent -k)
		cout << it->count << " " << sp.image << " " 
		     << sp.lib_image << endl;
	}
}

}  // anonymous namespace


int opreport(int argc, char const * argv[])
{
	get_options(argc, argv);

	if (options::symbols) {
		cerr << "N/A\n";
	} else {
		output_files_count(*sample_file_partition);
	}
	return 0;
}


int main(int argc, char const * argv[])
{
	run_pp_tool(argc, argv, opreport);
}
