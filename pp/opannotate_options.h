/**
 * @file opannotate_options.h
 * Options for opannotate tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef OPANNOTATE_OPTIONS_H
#define OPANNOTATE_OPTIONS_H

#include <string>

#include "common_option.h"

namespace options {
	extern bool demangle;
	extern bool smart_demangle;
}

/**
 * handle_options - process command line
 * @param non_options vector of non options string
 *
 * Process the arguments, fatally complaining on error.
 */
void handle_options(std::vector<std::string> const & non_options);

#endif // OPANNOTATE_OPTIONS_H
