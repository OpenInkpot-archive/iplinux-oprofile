/**
 * @file symbol_container.cpp
 * Internal container for symbols
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <string>
#include <algorithm>
#include <set>
#include <vector>

#include "symbol_functors.h"
#include "symbol_container.h"
#include "profile_container.h"

using namespace std;

symbol_index_t symbol_container::size() const
{
	return symbols.size();
}


void symbol_container::push_back(symbol_entry const & symbol)
{
	symbols.push_back(symbol);
}


symbol_entry const *
symbol_container::find(string filename, size_t linenr) const
{
	build_by_file_loc();

	symbol_entry symbol;
	symbol.sample.file_loc.filename = filename;
	symbol.sample.file_loc.linenr = linenr;

	set_symbol_by_file_loc::const_iterator it =
		symbol_entry_by_file_loc.find(&symbol);

	if (it != symbol_entry_by_file_loc.end())
		return *it;

	return 0;
}


vector<symbol_entry const *> symbol_container::find(string name) const
{
	vector<symbol_entry const *> v;

	symbols_t::const_iterator cit = symbols.begin();
	symbols_t::const_iterator end = symbols.end();

	for (; cit != end; ++cit) {
		if (cit->name == name)
			v.push_back(&*cit);
	}

	return v;
}


void symbol_container::build_by_file_loc() const
{
	if (!symbol_entry_by_file_loc.empty())
		return;

	symbols_t::const_iterator cit = symbols.begin();
	symbols_t::const_iterator end = symbols.end();
	for (; cit != end; ++cit)
		symbol_entry_by_file_loc.insert(&*cit);
}


symbol_entry const * symbol_container::find_by_vma(bfd_vma vma) const
{
	symbol_entry value;

	value.sample.vma = vma;

	vector<symbol_entry>::const_iterator it =
		lower_bound(symbols.begin(), symbols.end(),
			    value, less_sample_entry_by_vma());

	if (it != symbols.end() && it->sample.vma == vma)
		return &(*it);

	return 0;
}


void symbol_container::get_symbols_by_count(
	profile_container_t::symbol_collection & v) const
{
	symbols_t::const_iterator cit = symbols.begin();
	symbols_t::const_iterator end = symbols.end();
	for (; cit != end; ++cit)
		v.push_back(&*cit);

	stable_sort(v.begin(), v.end(), less_symbol_entry_by_samples_nr());
}
