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
#include <iomanip>
#include <vector>
#include <algorithm>

#include "split_sample_filename.h"
#include "opreport_options.h"
#include "profile.h"
#include "partition_files.h"

using namespace std;

namespace {

struct merged_file_count {
	merged_file_count() : count(0) {}
	size_t count;
	string filename;
};

struct files_count {
	files_count() : count(0) {}
	size_t count;
	string image_name;
	string sample_filename;
	vector<merged_file_count> files;
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

files_count counts(partition_files::filename_set const & files)
{
	files_count count;

	count.sample_filename = *files.begin();
	split_sample_filename sp = split_sample_file(count.sample_filename);
	count.image_name = sp.image;

	partition_files::filename_set::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		merged_file_count sub_count;

		sub_count.filename = *it;
		profile_t samples(*it);
		sub_count.count = samples.accumulate_samples(0, ~0);

		count.count += sub_count.count;
		count.files.push_back(sub_count);		
	}

	return count;
}

void output_files_count(partition_files const & files)
{
	vector<files_count> set_file_count;

	for (size_t i = 0 ; i < files.nr_set(); ++i) {
		partition_files::filename_set const & file_set = files.set(i);

		set_file_count.push_back(counts(file_set));
	}

	sort(set_file_count.begin(), set_file_count.end(),
	     compare_files_count());

	vector<files_count>::const_iterator it;
	for (it = set_file_count.begin(); it != set_file_count.end(); ++it) {
		cout << setw(6) << it->count << " ";
		if (!options::merge_by.merge_lib) {
			cout << it->image_name;
		} else {
			split_sample_filename sp =
					split_sample_file(it->sample_filename);
			if (sp.lib_image.empty())
				cout << it->image_name;
			else
				cout << sp.lib_image;
		}
		cout << endl;
		if (!options::hide_dependent && !options::merge_by.merge_lib) {
			for (size_t i = 0; i < it->files.size(); ++i) {
				merged_file_count const & count = it->files[i];
				split_sample_filename sp =
				 split_sample_file(count.filename);
				cout << "\t" << setw(6) << count.count << " ";
				if (sp.lib_image.empty())
					cout << " " << sp.image;
				else
					cout << " " << sp.lib_image;
				cout << endl;
			}
		}
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
