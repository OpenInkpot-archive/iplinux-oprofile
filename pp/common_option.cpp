/**
 * @file common_option.cpp
 * Contains common options and implementation of entry point of pp tools
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <iostream>

#include "op_exception.h"
#include "popt_options.h"

#include "common_option.h"

using namespace std;

namespace options {
	bool verbose;
}

namespace {
popt::option options_array[] = {
	popt::option(options::verbose, "verbose", 'V',
		     "verbose output"),
};
}

int run_pp_tool(int argc, char const * argv[], pp_fct_run_t fct)
{
	try {
		return fct(argc, argv);
	}
	catch (op_runtime_error const & e) {
		cerr << "op_runtime_error:" << e.what() << endl;
		return 1;
	}
	catch (op_fatal_error const & e) {
		cerr << "op_fatal_error:" << e.what() << endl;
	}
	catch (op_exception const & e) {
		cerr << "op_exception:" << e.what() << endl;
	}
	catch (exception const & e) {
		cerr << "exception:" << e.what() << endl;
	}
	catch (...) {
		cerr << "unknown exception" << endl;
	}

	return EXIT_SUCCESS;
}
