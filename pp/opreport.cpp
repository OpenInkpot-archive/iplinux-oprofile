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

#include "opreport_options.h"
#include "profile.h"
#include "partition_files.h"

using namespace std;

namespace {

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
	vector<size_t> count;

	for (size_t i = 0 ; i < files.nr_set(); ++i) {
		count.push_back(counts(files.set(i)));
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
