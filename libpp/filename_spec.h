/**
 * @file filename_spec.h
 * Container holding a sample filename splitted into its components
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef FILENAME_SPEC_H
#define FILENAME_SPEC_H

#include <unistd.h>
#include <string>

#include "generic_spec.h"


class parse_cmdline;

/**
 * a class to split and store components of a sample filename.
 */
class filename_spec
{
	friend class parse_cmdline;
public:
	/**
	 * @param filename  the samples filename
	 *
	 * build a filename_spec from a samples filename
	 */
	filename_spec(std::string const & filename);

	filename_spec();

	/**
	 * @param filename  a sample filename
	 *
	 * setup filename spec according to the samples filename. PP:3.19 to
	 * 3.25
	 */
	void set_sample_filename(std::string const & filename);

	/**
	 * @param rhs  right hand side of the match operator
	 * @param binary  if binary is non empty matching the binary name
	 *  or lib_name must use it rather the once in rhs
	 *
	 * return true if *this match rhs, matching is:
	 *  - image_name are identical
	 *  - lib_name are identical
	 *  - event_spec match
	 *
	 * This operation is not commutative. First part of PP:3.24
	 */
	bool match(filename_spec const & rhs,
		   std::string const & binary) const;
private:
	std::string image;
	std::string lib_image;
	std::string event;
	int count;
	unsigned int unit_mask;
	generic_spec<pid_t> tgid;
	generic_spec<pid_t> tid;
	generic_spec<int> cpu;
};


#endif /* !FILENAME_SPEC_H */
