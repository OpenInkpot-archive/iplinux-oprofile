/**
 * @file opreport_options.h
 * Options for opreport tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef OPREPORT_OPTIONS_H
#define OPREPORT_OPTIONS_H

#include <string>

#include "common_option.h"
#include "utility.h"

class partition_files;
class merge_option;

namespace options {
	extern bool symbols;
	extern bool reverse_sort;
	extern bool hide_dependent;
	extern bool include_dependent;
	extern merge_option merge_by;
	extern bool global_percent;
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

#endif // OPREPORT_OPTIONS_H
