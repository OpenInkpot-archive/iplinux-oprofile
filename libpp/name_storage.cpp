/**
 * @file name_storage.cpp
 * Storage of global names (filenames and symbols)
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "name_storage.h"
#include "demangle_symbol.h"
#include "file_manip.h"

using namespace std;

namespace {

/// id for new allocations
int global_id = 0;

}

name_storage image_names;
name_storage debug_names;
name_storage symbol_names;

name_storage::name_storage()
{
}


name_id name_storage::create(string const & name)
{
	id_map::const_iterator cit = ids.find(name);
	if (cit == ids.end()) {
		names[global_id] = name;
		ids[name] = global_id;
		return global_id++;
	}
	return cit->second;
}


std::string const & name_storage::name(name_id id) const
{
	return (names.find(id))->second.name;
}


std::string const & name_storage::demangle(name_id id) const
{
	stored_name const & n = names.find(id)->second;
	if (!n.demangled) {
		if (n.name.length() && n.name[0] == '?')
			n.name_demangled = "(no symbol)";
		else
			n.name_demangled = demangle_symbol(n.name);
		n.demangled = true;
	}
	return n.name_demangled;
}


std::string const & name_storage::basename(name_id id) const
{
	stored_name const & n = names.find(id)->second;
	if (!n.basenamed) {
		n.name_basenamed = ::basename(n.name);
		n.basenamed = true;
	}
	return n.name_basenamed;
}
