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

#include <algorithm>

using namespace std;

namespace {


int compare_by(sort_options::sort_order order,
               symbol_entry const & lhs, symbol_entry const & rhs)
{
	switch (order) {
		case sort_options::sample:
			if (lhs.sample.count < rhs.sample.count)
				return 1;
			if (lhs.sample.count > rhs.sample.count)
				return -1;
			return 0;
		case sort_options::symbol:
			return lhs.name.compare(rhs.name);
		case sort_options::image:
			return lhs.image_name.compare(rhs.image_name);
		case sort_options::vma:
			if (lhs.sample.vma < rhs.sample.vma)
				return -1;
			if (lhs.sample.vma > rhs.sample.vma)
				return 1;
			return 0;
		case sort_options::debug: {
			file_location const & f1 = lhs.sample.file_loc;
			file_location const & f2 = rhs.sample.file_loc;
			int ret = f1.filename.compare(f2.filename);
			if (ret == 0)
				ret = f1.linenr - f2.linenr;
			return ret;
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

	bool operator()(symbol_entry const & lhs,
			symbol_entry const & rhs) const;
private:
	vector<sort_options::sort_order> const & compare_order;
	bool reverse_sort;
};


bool symbol_compare::operator()(symbol_entry const & lhs,
				symbol_entry const & rhs) const
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


void sort_options::sort_by(symbol_collection & syms, bool reverse_sort) const
{
	syms.sort(symbol_compare(sort, reverse_sort));
}


void sort_options::add_sort_option(std::string const & name)
{
	if (name == "vma") {
		sort.push_back(vma);
	} else if (name == "sample") {
		sort.push_back(sample);
	} else if (name == "symbol") {
		sort.push_back(symbol);
	} else if (name == "debug") {
		sort.push_back(debug);
	} else if (name == "image") {
		sort.push_back(image);
	} else {
		cerr << "unknown sort option: " << name << endl;
		exit(EXIT_FAILURE);
	}
}


void sort_options::add_sort_option(sort_options::sort_order order)
{
	sort.push_back(order);
}
