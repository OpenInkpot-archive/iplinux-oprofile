/**
 * @file derive_files.h
 * Command-line helper
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef DERIVE_FILES_H
#define DERIVE_FILES_H

#include <string>
#include <map>
#include <vector>

/**
 * container type used to store alternative location of binary image. We need a
 * multimap to warn against ambiguity between mutiple time found image name.
 * \sa add_to_alternate_filename(), check_image_name()
 */
typedef std::multimap<std::string, std::string> alt_filename_t;

/**
 * @param alternate_filename a container where all filename belonging to the
 * following path are stored
 * @param path_names a vector of path to consider
 *
 * add all file name below path_name recursively, to the the set of
 * alternative filename used to retrieve image name when a samples image name
 * directory is not accurate
 */
void add_to_alternate_filename(alt_filename_t & alternate_filename,
			       std::vector<std::string> const & path_names);

/**
 * @param alternate_filename container where all candidate filename are stored
 * @param image_name binary image name
 * @param samples_filename samples filename
 *
 * check than image_name belonging to samples_filename exist. If not it try to
 * retrieve it through the alternate_filename location. If we fail to retrieve
 * the file or if it is not readable we provide a warning and return an empty
 * string
 */
std::string check_image_name(alt_filename_t const & alternate_filename,
			     std::string const & image_name,
			     std::string const & samples_filename);

#endif /* ! DERIVE_FILES_H */
