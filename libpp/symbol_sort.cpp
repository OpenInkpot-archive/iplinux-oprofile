/**
 * @file symbol_sort.cpp
 * Sorting symbols
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "symbol_sort.h"
#include "opp_symbol.h"
#include "symbol_functors.h"
#include "file_manip.h"
#include "name_storage.h"

#include <algorithm>

using namespace std;

namespace {

bool long_filenames;

inline string const & get(name_id id)
{
	if (long_filenames)
		return name_store.name(id);
	return name_store.basename(id);
}


int compare_by(sort_options::sort_order order,
               symbol_entry const * lhs, symbol_entry const * rhs)
{
	switch (order) {
		case sort_options::sample:
			if (lhs->sample.count < rhs->sample.count)
				return 1;
			if (lhs->sample.count > rhs->sample.count)
				return -1;
			return 0;
		case sort_options::symbol:
			return name_store.demangle(lhs->name).compare(
				name_store.demangle(rhs->name));
		case sort_options::image: {
			return get(lhs->image_name).compare(
				get(rhs->image_name));
		}
		case sort_options::vma:
			if (lhs->sample.vma < rhs->sample.vma)
				return -1;
			if (lhs->sample.vma > rhs->sample.vma)
				return 1;
			return 0;
		case sort_options::debug: {
			file_location const & f1 = lhs->sample.file_loc;
			file_location const & f2 = rhs->sample.file_loc;
			int ret = get(f1.filename).compare(get(f2.filename));
			if (ret == 0)
				ret = f1.linenr - f2.linenr;
		}
		default:
			cerr << "compare_by(): unknown sort option: "
			     << order << endl;
			exit(EXIT_FAILURE);
	}

	return false;
}


struct symbol_compare {
	symbol_compare(vector<sort_options::sort_order> const & order,
	               bool reverse)
		: compare_order(order), reverse_sort(reverse) {}

	bool operator()(symbol_entry const * lhs,
			symbol_entry const * rhs) const;
private:
	vector<sort_options::sort_order> const & compare_order;
	bool reverse_sort;
};


bool symbol_compare::operator()(symbol_entry const * lhs,
				symbol_entry const * rhs) const
{
	for (size_t i = 0; i < compare_order.size(); ++i) {
		int ret = compare_by(compare_order[i], lhs, rhs);

		if (reverse_sort)
			ret = -ret;
		if (ret != 0)
			return ret < 0;
	}
	return true;
}

} // anonymous namespace


void sort_options::sort(symbol_collection & syms,
                        bool reverse_sort, bool lf) const
{
	long_filenames = lf;

	stable_sort(syms.begin(), syms.end(),
	            symbol_compare(options, reverse_sort));
}


void sort_options::add_sort_option(std::string const & name)
{
	if (name == "vma") {
		options.push_back(vma);
	} else if (name == "sample") {
		options.push_back(sample);
	} else if (name == "symbol") {
		options.push_back(symbol);
	} else if (name == "debug") {
		options.push_back(debug);
	} else if (name == "image") {
		options.push_back(image);
	} else {
		cerr << "unknown sort option: " << name << endl;
		exit(EXIT_FAILURE);
	}
}


void sort_options::add_sort_option(sort_options::sort_order order)
{
	options.push_back(order);
}
