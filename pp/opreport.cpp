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

#include "string_manip.h"
#include "file_manip.h"
#include "split_sample_filename.h"
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
	string filename;
};

struct compare_merged_file_count {
	bool operator()(merged_file_count const & lhs,
			merged_file_count const & rhs) const;
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
	return options::short_filename
		? basename(filename)
		: filename;
}


void output_sub_count(files_count const & files, double total_count)
{
	for (size_t i = 0; i < files.files.size(); ++i) {
		merged_file_count const & count = files.files[i];

		cout << "\t";
		double tot_count = options::global_percent 
			? total_count : files.count;
		output_counter(tot_count, count.count);

		split_sample_filename sp = split_sample_file(count.filename);
		if (sp.lib_image.empty())
			cout << " " << get_filename(sp.image);
		else
			cout << " " << get_filename(sp.lib_image);
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
			split_sample_filename sp =
					split_sample_file(it->sample_filename);
			if (sp.lib_image.empty())
				cout << get_filename(it->image_name);
			else
				cout << get_filename(sp.lib_image);
		}
		cout << endl;
		if (!options::hide_dependent && !options::merge_by.merge_lib) {
			output_sub_count(*it, total_count);
		}
	}
}


void output_symbols_count(partition_files const & files)
{
	// FIXME: we probably don't want to show application name if
	// we report samples about only one application
	outsymbflag flags = outsymbflag(osf_vma | osf_nr_samples | osf_percent | osf_symb_name | osf_app_name);

	if (options::include_dependent && !options::merge_by.merge_lib)
		flags = outsymbflag(flags | osf_image_name);

	if (options::debug_info)
		flags = outsymbflag(flags | osf_linenr_info);

	profile_container samples(false, flags, options::details);

	for (size_t i = 0 ; i < files.nr_set(); ++i) {
		partition_files::filename_set const & file_set = files.set(i);

		partition_files::filename_set::const_iterator it;
		for (it = file_set.begin(); it != file_set.end(); ++it) {
			split_sample_filename sp = split_sample_file(*it);

			string app_name = sp.image;
			string image_name = sp.lib_image.empty() ?
				sp.image : sp.lib_image;

			if (options::merge_by.merge_lib) {
				app_name = image_name;
			}

			// FIXME
			// if the image files does not exist try to retrieve it
//			image_name = check_image_name(options::alternate_filename, image_name, samples_filename);
			if (op_file_readable(image_name)) {
				// FIXME: inneficient since we can have
				// multiple time the same binary file open bfd
				// openened
				add_samples(samples, *it, image_name, app_name,
					    options::exclude_symbols,
					    options::include_symbols);
			}
		}
	}

	vector<symbol_entry const *> symbols =
		samples.select_symbols(options::threshold / 100.0, false);

	bool need_vma64 = vma64_p(symbols.begin(), symbols.end());

	format_output::formatter out(samples);

	if (options::details)
		out.show_details();
	if (options::short_filename)
		out.show_short_filename();
	out.add_format(flags);

	out.output(cout, symbols, options::reverse_sort, need_vma64);
}

}  // anonymous namespace


int opreport(int argc, char const * argv[])
{
	get_options(argc, argv);

	if (options::symbols) {
		output_symbols_count(*sample_file_partition);
	} else {
		output_files_count(*sample_file_partition);
	}
	return 0;
}


int main(int argc, char const * argv[])
{
	run_pp_tool(argc, argv, opreport);
}
