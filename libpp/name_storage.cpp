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
#include "string_manip.h"

using namespace std;

int name_storage::last_id;

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
		names[last_id] = name;
		ids[name] = last_id;
		return last_id++;
	}
	return cit->second;
}


bool name_storage::present(string const & name) const
{
	return ids.find(name) != ids.end();
}


std::string const & name_storage::name(name_id id) const
{
	return (names.find(id))->second.name;
}


std::string const & name_storage::demangle(name_id id) const
{
	stored_name const & n = names.find(id)->second;
	if (!n.name_processed.empty() || n.name.empty())
		return n.name_processed;

	if (n.name[0] != '?') {
		n.name_processed = demangle_symbol(n.name);
		return n.name_processed;
	}

	if (n.name.length() < 2 || n.name[1] != '?') {
		n.name_processed = "(no symbols)";
		return n.name_processed;
	}
	
	n.name_processed = "anonymous symbol from section ";
	n.name_processed += ltrim(n.name, "?");
	return n.name_processed;
}


std::string const & name_storage::basename(name_id id) const
{
	stored_name const & n = names.find(id)->second;
	if (n.name_processed.empty()) {
		n.name_processed = ::basename(n.name);
	}
	return n.name_processed;
}
