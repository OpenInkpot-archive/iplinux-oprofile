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
#include <sstream>

#include "op_header.h"
#include "string_manip.h"
#include "file_manip.h"
#include "opreport_options.h"
#include "profile.h"
#include "partition_files.h"
#include "profile_container.h"
#include "format_output.h"

using namespace std;

namespace {

struct merged_file_count {
	merged_file_count() : count(0) {}
	size_t count;
	string image_name;
	string lib_image;
};

struct compare_merged_file_count {
	bool operator()(merged_file_count const & lhs,
			merged_file_count const & rhs) const;
};

struct files_count {
	files_count() : count(0) {}
	size_t count;
	string image_name;
	string lib_image;
	vector<merged_file_count> files;
};

struct compare_files_count {
	bool operator()(files_count const & lhs, files_count const & rhs) const;
};


bool compare_merged_file_count::operator()(merged_file_count const & lhs,
					   merged_file_count const & rhs) const
{
	return options::reverse_sort 
		? lhs.count < rhs.count
		: rhs.count < lhs.count;
}


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

	count.image_name = files.begin()->image;
	count.lib_image = files.begin()->lib_image;

	partition_files::filename_set::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		merged_file_count sub_count;

		sub_count.image_name = it->image;
		sub_count.lib_image  = it->lib_image;
		profile_t samples(it->sample_filename);
		sub_count.count = samples.accumulate_samples(0, ~0);

		count.count += sub_count.count;
		count.files.push_back(sub_count);
	}

	sort(count.files.begin(), count.files.end(),
	     compare_merged_file_count());

	return count;
}


// FIXME: intended to replace format_percent
string const format_double(double value, size_t int_width, size_t frac_width)
{
	ostringstream os;

	// os << fixed << value unsupported by gcc 2.95
	os.setf(ios::fixed, ios::floatfield);
	os << setw(int_width + frac_width + 1)
	   << setprecision(frac_width) << value;

	return os.str();
}

void output_header(partition_files const & files)
{
	if (files.nr_set()) {
		partition_files::filename_set const & file_set = files.set(0);
		profile_t profile(file_set.begin()->sample_filename);
		output_header(cout, profile.get_header());
	}
}


void output_counter(double total_count, size_t count)
{
	// FIXME: left or right, op_time was using left
	// left io manipulator doesn't exist in 2.95
//	cout.setf(ios::left, ios::adjustfield);
	cout << setw(9) << count << " ";
	double ratio = op_ratio(count, total_count);
	cout << format_double(ratio * 100, 3, 4) << " ";
}


string get_filename(string const & filename)
{
	return options::short_filename ? basename(filename) : filename;
}


void output_sub_count(files_count const & files, double total_count)
{
	for (size_t i = 0; i < files.files.size(); ++i) {
		merged_file_count const & count = files.files[i];

		cout << "\t";
		double tot_count = options::global_percent 
			? total_count : files.count;
		output_counter(tot_count, count.count);

		if (count.lib_image.empty())
			cout << " " << get_filename(count.image_name);
		else
			cout << " " << get_filename(count.lib_image);
		cout << endl;
	}
}


void output_files_count(partition_files const & files)
{
	vector<files_count> set_file_count;

	double total_count = 0;
	for (size_t i = 0 ; i < files.nr_set(); ++i) {
		partition_files::filename_set const & file_set = files.set(i);

		set_file_count.push_back(counts(file_set));

		total_count += set_file_count.back().count;
	}

	sort(set_file_count.begin(), set_file_count.end(),
	     compare_files_count());

	vector<files_count>::const_iterator it;
	for (it = set_file_count.begin(); it != set_file_count.end(); ++it) {
		output_counter(total_count, it->count);
		if (!options::merge_by.merge_lib) {
			cout << get_filename(it->image_name);
		} else {
			if (it->lib_image.empty())
				cout << get_filename(it->image_name);
			else
				cout << get_filename(it->lib_image);
		}
		cout << endl;
		if (options::include_dependent && !options::hide_dependent &&
		    !options::merge_by.merge_lib) {
			output_sub_count(*it, total_count);
		}
	}
}


void output_symbols_count(partition_files const & files)
{
	image_set images = sort_by_image(files, options::extra_found_images);

	profile_container samples(false, options::debug_info, options::details);

	image_set::const_iterator it;
	for (it = images.begin(); it != images.end(); ) {
		pair<image_set::const_iterator, image_set::const_iterator>
			p_it = images.equal_range(it->first);

		op_bfd abfd(p_it.first->first, options::symbol_filter);

		for (it = p_it.first;  it != p_it.second; ++it) {
			string app_name = it->second.image;
			if (options::merge_by.merge_lib) {
				app_name = it->first;
			}

			add_samples(samples, it->second.sample_filename,
				    abfd, app_name);
		}
	}

	vector<symbol_entry const *> symbols =
		samples.select_symbols(string(), options::threshold / 100.0, false);

	bool need_vma64 = vma64_p(symbols.begin(), symbols.end());

	format_output::formatter out(samples);

	if (options::details)
		out.show_details();
	if (options::short_filename)
		out.show_short_filename();
	if (!options::show_header)
		out.hide_header();

	// FIXME: we probably don't want to show application name if
	// we report samples about only one application
	outsymbflag flags = outsymbflag(osf_vma | osf_nr_samples | osf_percent | osf_symb_name | osf_app_name);

	if (options::include_dependent && !options::merge_by.merge_lib)
		flags = outsymbflag(flags | osf_image_name);

	if (options::debug_info)
		flags = outsymbflag(flags | osf_linenr_info);

	out.add_format(flags);

	out.output(cout, symbols, options::reverse_sort, need_vma64);
}


int opreport(vector<string> const & non_options)
{
	handle_options(non_options);

	output_header(*sample_file_partition);

	if (options::symbols) {
		output_symbols_count(*sample_file_partition);
	} else {
		output_files_count(*sample_file_partition);
	}
	return 0;
}

}  // anonymous namespace


int main(int argc, char const * argv[])
{
	return run_pp_tool(argc, argv, opreport);
}
