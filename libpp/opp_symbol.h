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

#include <string>
#include <iostream>

#include <bfd.h>

/// A simple container for a fileno:linenr location.
struct file_location {
	/// empty if not valid.
	std::string filename;
	/// 0 means invalid or code is generated internally by the compiler
	unsigned int linenr;

	bool operator<(file_location const & rhs) const {
		return filename < rhs.filename ||
			(filename == rhs.filename && linenr < rhs.linenr);
	}
};


/// associate vma address with a file location and a samples count
struct sample_entry {
	/// From where file location comes the samples
	file_location file_loc;
	/// From where virtual memory address comes the samples
	bfd_vma vma;
	/// the samples count
	unsigned int count;
};


/// associate a symbol with a file location, samples count and vma address
struct symbol_entry {
	/// which image this symbol belongs to
	std::string image_name;
	/// owning application name: identical to image name if profiling
	/// session did not separate samples for shared libs or if image_name
	// is not a shared lib
	std::string app_name;
	/// file location, vma and cumulated samples count for this symbol
	sample_entry sample;
	/// name of symbol
	std::string name;
	/// symbol size as calculated by op_bfd, start of symbol is sample.vma
	size_t size;
};

#endif /* !OPP_SYMBOL_H */
