/**
 * @file common_option.h
 * Declaration of entry point of pp tools, implementation file add common
 * options of pp tools
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef COMMON_OPTION_H
#define COMMON_OPTION_H

namespace options {
	extern bool demangle;
	// FIXME: rename to smart_demangle
	extern bool demangle_and_shrink;
	extern bool verbose;
};

typedef int (*pp_fct_run_t)(int argc, char const * argv[]);

/**
 * @param argc  command line number of argument
 * @param argv  command line argument pointer array
 * @param fct  functon to run to start this pp tool
 *
 * Provide a common entry to all pp tools, adding default options and
 * providing the necessary try catch clause
 */
int run_pp_tool(int argc, char const * argv[], pp_fct_run_t fct);

#endif /* !COMMON_OPTION_H */
