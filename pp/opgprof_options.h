/**
 * @file opgprof_options.h
 * Options for opgprof tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef OPGPROF_OPTIONS_H
#define OPGPROF_OPTIONS_H

#include <string>

#include "utility.h"
#include "derive_files.h"
#include "common_option.h"

class partition_files;

namespace options {
	extern alt_filename_t alternate_filename;
	extern std::string gmon_filename;
}

/**
 * a partition of sample filename to treat, each sub-list is a list of
 * sample to merge. filled by get_options()
 */
extern scoped_ptr<partition_files> sample_file_partition;

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
void get_options(int argc, char const * argv[]);

#endif // OPGPROF_OPTIONS_H
