/**
 * @file symbol_container.h
 * Internal container for symbols
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef SYMBOL_CONTAINER_H
#define SYMBOL_CONTAINER_H

#include <vector>
#include <string>
#include <set>

#include "profile_container.h"

/**
 * An arbitrary container of symbols. Supports lookup
 * by name, by VMA, and by file location.
 *
 * Lookup by name or by VMA is O(n). Lookup by file location
 * is O(log(n)).
 */
class symbol_container {
public:
	/// container type
	typedef std::vector<symbol_entry> symbols_t;

	typedef symbols_t::size_type size_type;

	/// return the number of symbols stored
	size_type size() const;

	/**
	 * Add a symbol. Can only be done before any file-location
	 * based lookups, since the two lookup methods are not
	 * synchronised.
	 */
	void push_back(symbol_entry const &);

	/// find the symbol at the given filename and line number, if any
	symbol_entry const * find(std::string filename, size_t linenr) const;

	/// return all symbols of the given name
	std::vector<symbol_entry const *> find(std::string name) const;

	/// find the symbol with the given VMA if any
	symbol_entry const * find_by_vma(bfd_vma vma) const;

	/// populate the given container with all the symbols, sorted by count
	void get_symbols_by_count(profile_container::symbol_collection & v) const;

private:
	/// build the symbol by file-location cache
	void build_by_loc() const;

	/**
	 * The main container of symbols. Multiple symbols with the same
	 * name are allowed.
	 */
	symbols_t symbols;

	/**
	 * Differently-named symbol at same file location are allowed e.g.
	 * template instantiation.
	 */
	typedef std::multiset<symbol_entry const *, less_by_file_loc>
		symbols_by_loc_t;

	// must be declared after the vector to ensure a correct life-time.

	/**
	 * Symbols sorted by location order. Lazily built on request,
	 * so mutable.
	 */
	mutable symbols_by_loc_t symbols_by_loc;
};

#endif /* SYMBOL_CONTAINER_H */
