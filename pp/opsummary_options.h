/**
 * @file opsummary_options.h
 * Options for opsummary tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef OPSUMMARY_OPTIONS_H
#define OPSUMMARY_OPTIONS_H

#include "common_option.h"

namespace options {
}

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
void get_options(int argc, char const * argv[]);

#endif // OPSUMMARY_OPTIONS_H
