/**
 * @file name_storage.h
 * Storage of global names (filenames and symbols)
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef NAME_STORAGE_H
#define NAME_STORAGE_H

#include <string>
#include <map>

typedef int name_id;

/**
 * Holds shared names for filenames and symbol names.
 * Each ID identifies a unique string, and IDs  can be
 * shared across all users.
 */
class name_storage {

public:
	name_storage();

	/// allocate or re-use an ID for this name
	name_id create(std::string const & original);

	/// return the original name for the given ID
	std::string const & name(name_id id) const;

	/// return the demangled form of the given ID
	std::string const & demangle(name_id id) const;

	/// return the basename form of the given ID
	std::string const & basename(name_id id) const;

private:
	struct stored_name {
		stored_name(std::string const & n = std::string())
			: name(n) {}

		std::string name;
		mutable std::string name_processed;

		bool operator<(stored_name const & rhs) const {
			return name < rhs.name;
		}
	};

	typedef std::map<name_id, stored_name> name_map;

	typedef std::map<std::string, name_id> id_map;

	name_map names;

	id_map ids;
};

/// for images
extern name_storage image_names;

/// for debug filenames
extern name_storage debug_names;

/// for symbols
extern name_storage symbol_names;

#endif /* !NAME_STORAGE_H */
