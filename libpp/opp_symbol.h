/**
 * @file opp_symbol.h
 * Symbol containers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef OPP_SYMBOL_H
#define OPP_SYMBOL_H

#include "config.h"
#include "name_storage.h"

#include <string>
#include <iostream>

#include <bfd.h>

/// A simple container for a fileno:linenr location.
struct file_location {
	file_location() : filename(0), linenr(0) {}
	/// empty if not valid.
	name_id filename;
	/// 0 means invalid or code is generated internally by the compiler
	unsigned int linenr;

	bool operator<(file_location const & rhs) const {
		return filename < rhs.filename ||
			(filename == rhs.filename && linenr < rhs.linenr);
	}
};


/// associate vma address with a file location and a samples count
struct sample_entry {
	sample_entry() : vma(0), count(0) {}
	/// From where file location comes the samples
	file_location file_loc;
	/// From where virtual memory address comes the samples
	bfd_vma vma;
	/// the samples count
	unsigned int count;
};


/// associate a symbol with a file location, samples count and vma address
struct symbol_entry {
	symbol_entry() : image_name(0), app_name(0), name(0), size(0) {}
	/// which image this symbol belongs to
	name_id image_name;
	/// owning application name: identical to image name if profiling
	/// session did not separate samples for shared libs or if image_name
	// is not a shared lib
	name_id app_name;
	/// file location, vma and cumulated samples count for this symbol
	sample_entry sample;
	/// name of symbol
	name_id name;
	/// symbol size as calculated by op_bfd, start of symbol is sample.vma
	size_t size;
};

#endif /* !OPP_SYMBOL_H */
