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

symbol_container::size_type symbol_container::size() const
{
	return symbols.size();
}


symbol_entry const * symbol_container::insert(symbol_entry const & symb)
{
	pair<symbols_t::iterator, bool> p = symbols.insert(symb);
	if (!p.second) {
		// safe: count is not used by sorting criteria
		symbol_entry * symbol = const_cast<symbol_entry*>(&*p.first);
		symbol->sample.count += symb.sample.count;
	}

	return &*p.first;
}


symbol_entry const *
symbol_container::find(string filename, size_t linenr) const
{
	build_by_loc();

	symbol_entry symbol;
	symbol.sample.file_loc.filename = filename;
	symbol.sample.file_loc.linenr = linenr;

	symbols_by_loc_t::const_iterator it =
		symbols_by_loc.find(&symbol);

	if (it != symbols_by_loc.end())
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


void symbol_container::build_by_loc() const
{
	if (!symbols_by_loc.empty())
		return;

	symbols_t::const_iterator cit = symbols.begin();
	symbols_t::const_iterator end = symbols.end();
	for (; cit != end; ++cit)
		symbols_by_loc.insert(&*cit);
}


symbol_entry const * symbol_container::find_by_vma(bfd_vma /*vma*/) const
{
	// can't work, symbol are no longer sorted by vma, anyway the interface
	// is no longer sufficient
#if 0
	symbol_entry value;

	value.sample.vma = vma;

	symbols_t::const_iterator it =
		lower_bound(symbols.begin(), symbols.end(),
			    value, less_sample_entry_by_vma());

	if (it != symbols.end() && it->sample.vma == vma)
		return &(*it);
#else
	cerr << "FIXME: symbol_container::find_by_vma()" << endl;
	exit(EXIT_FAILURE);
#endif

	return 0;
}


void symbol_container::get_symbols_by_count(
	profile_container::symbol_collection & v) const
{
	symbols_t::const_iterator cit = symbols.begin();
	symbols_t::const_iterator end = symbols.end();
	for (; cit != end; ++cit)
		v.push_back(&*cit);

	stable_sort(v.begin(), v.end(), less_symbol_entry_by_samples_nr());
}
