/**
 * @file profile_container.cpp
 * profile file container
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

#include "string_filter.h"

#include "op_header.h"
#include "profile.h"
#include "symbol_functors.h"
#include "profile_container.h"
#include "sample_container.h"
#include "symbol_container.h"

using namespace std;

namespace {

struct filename_by_samples {
	filename_by_samples(string filename_, double percent_)
		: filename(filename_), percent(percent_)
		{}

	bool operator<(filename_by_samples const & lhs) const {
		if (percent != lhs.percent)
			return percent > lhs.percent;
		return filename > lhs.filename;
	}

	string filename;
	// ratio of samples which belongs to this filename.
	double percent;
};

}


profile_container::profile_container(bool add_zero_samples_symbols_,
                                     bool debug_info_,
                                     bool need_details_)
	:
	symbols(new symbol_container),
	samples(new sample_container),
	total_count(0),
	debug_info(debug_info_),
	add_zero_samples_symbols(add_zero_samples_symbols_),
	need_details(need_details_)
{
}


profile_container::~profile_container()
{
}
 

// Post condition:
//  the symbols/samples are sorted by increasing vma.
//  the range of sample_entry inside each symbol entry are valid
//  the samples_by_file_loc member var is correctly setup.
void profile_container::add(profile_t const & profile,
                            op_bfd const & abfd, string const & app_name)
{
	string const image_name = abfd.get_filename();

	for (symbol_index_t i = 0; i < abfd.syms.size(); ++i) {

		u32 start, end;
		string filename;
		uint linenr;
		symbol_entry symb_entry;

		abfd.get_symbol_range(i, start, end);

		symb_entry.sample.count =
			profile.accumulate_samples(start, end);

		if (symb_entry.sample.count == 0 && !add_zero_samples_symbols)
			continue;

		symb_entry.size = end - start;

		total_count += symb_entry.sample.count;

		symb_entry.name = abfd.syms[i].name();

		if (debug_info && abfd.get_linenr(i, start, filename, linenr)) {
			symb_entry.sample.file_loc.filename = filename;
			symb_entry.sample.file_loc.linenr = linenr;
		} else {
			symb_entry.sample.file_loc.linenr = 0;
		}

		symb_entry.image_name = image_name;
		symb_entry.app_name = app_name;

		bfd_vma base_vma = abfd.syms[i].vma();

		symb_entry.sample.vma = abfd.sym_offset(i, start) + base_vma;

		symbol_entry const * symbol = symbols->insert(symb_entry);

		if (need_details) {
			add_samples(profile, abfd, i, start, end,
			            base_vma, symbol);
		}
	}
}


// FIXME: far too many args etc.
void
profile_container::add_samples(profile_t const & profile,
                               op_bfd const & abfd,
                               symbol_index_t sym_index,
                               u32 start, u32 end, bfd_vma base_vma,
                               symbol_entry const * symbol)
{

	for (u32 pos = start; pos < end ; ++pos) {
		string filename;
		sample_entry sample;
		uint linenr;

		sample.count = profile.accumulate_samples(pos);
		if (!sample.count)
			continue;

		if (debug_info && sym_index != nil_symbol_index &&
		    abfd.get_linenr(sym_index, pos, filename, linenr)) {
			sample.file_loc.filename = filename;
			sample.file_loc.linenr = linenr;
		} else {
			sample.file_loc.linenr = 0;
		}

		sample.vma = (sym_index != nil_symbol_index)
			? abfd.sym_offset(sym_index, pos) + base_vma
			: pos;

		samples->insert(symbol, sample);
	}
}


symbol_collection const
profile_container::select_symbols(symbol_choice & choice) const
{
	symbol_collection v;
	symbol_collection result;
	string app_name;

	double const threshold = choice.threshold / 100.0;

	symbols->get_symbols_by_count(v);

	symbol_collection::const_iterator it = v.begin();
	symbol_collection::const_iterator const end = v.end();
	for (; it < end; ++it) {
		if (choice.match_image
		    && (*it)->image_name != choice.image_name)
			continue;

		double const percent =
			op_ratio((*it)->sample.count, samples_count());

		if (percent >= threshold) {
			result.push_back(*it);

			if (app_name.empty()) {
				app_name = (*it)->app_name;
			} else if (app_name != (*it)->app_name) {
				choice.hints = column_flags(
					choice.hints | cf_multiple_apps);
			}

			/**
			 * It's theoretically possible that we get a
			 * symbol where its samples pass the border
			 * line, but the start is below it, but the
			 * the hint is only used for formatting
			 */
			if ((*it)->sample.vma & ~0xffffffffLLU) {
				choice.hints = column_flags(
					choice.hints | cf_64bit_vma);
			}
		}
	}

	return result;
}


vector<string> const profile_container::select_filename(double threshold) const
{
	vector<string> result;
	set<string> filename_set;

	threshold /= 100.0;

	// Trying to iterate on symbols to create the set of filenames which
	// contain sample does not work: a symbol can contain samples and this
	// symbol is in a source file that contain zero sample because only
	// inline function in this source file contains samples.
	sample_container::samples_iterator sit = samples->begin();
	sample_container::samples_iterator const send = samples->end();

	for (; sit != send; ++sit) {
		filename_set.insert(sit->second.file_loc.filename);
	}

	// Give a sort order on filename for the selected counter.
	vector<filename_by_samples> file_by_samples;

	set<string>::const_iterator it = filename_set.begin();
	set<string>::const_iterator const end = filename_set.end();
	for (; it != end; ++it) {
		unsigned int count = samples_count(*it);

		filename_by_samples f(*it, op_ratio(count, samples_count()));

		file_by_samples.push_back(f);
	}

	// now sort the file_by_samples entry.
	sort(file_by_samples.begin(), file_by_samples.end());

	vector<filename_by_samples>::const_iterator cit
		= file_by_samples.begin();
	vector<filename_by_samples>::const_iterator const cend
		= file_by_samples.end();

	for (; cit != cend; ++cit) {
		if (cit->percent >= threshold)
			result.push_back(cit->filename);
	}

	return result;
}


u32 profile_container::samples_count() const
{
	return total_count;
}


// Rest here are delegated to our private implementation.

symbol_entry const *
profile_container::find_symbol(string const & image_name, bfd_vma vma) const
{
	return symbols->find_by_vma(image_name, vma);
}


symbol_entry const *
profile_container::find_symbol(string const & filename, size_t linenr) const
{
	return symbols->find(filename, linenr);
}


symbol_collection const
profile_container::find_symbol(string const & name) const
{
	return symbols->find(name);
}


sample_entry const *
profile_container::find_sample(symbol_entry const * symbol, bfd_vma vma) const
{
	return samples->find_by_vma(symbol, vma);
}


unsigned int profile_container::samples_count(string const & filename) const
{
	return samples->accumulate_samples(filename);
}


unsigned int profile_container::samples_count(string const & filename,
				    size_t linenr) const
{
	return samples->accumulate_samples(filename, linenr);
}


sample_container::samples_iterator
profile_container::begin(symbol_entry const * symbol) const
{
	return samples->begin(symbol);
}


sample_container::samples_iterator
profile_container::end(symbol_entry const * symbol) const
{
	return samples->end(symbol);
}


sample_container::samples_iterator profile_container::begin() const
{
	return samples->begin();
}


sample_container::samples_iterator profile_container::end() const
{
	return samples->end();
}
