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
#include "cverb.h"
#include "common_option.h"

using namespace std;

namespace options {
	bool verbose;
	extra_images extra_found_images;
}

namespace {

vector<string> image_path;

popt::option options_array[] = {
	popt::option(options::verbose, "verbose", 'V',
		     "verbose output"),
	popt::option(image_path, "image-path", 'p',
		     "comma-separated path to search missing binaries","path"),
};


vector<string> get_options(int argc, char const * argv[])
{
	vector<string> non_options;
	popt::parse_options(argc, argv, non_options);

	set_verbose(options::verbose);

	options::extra_found_images.populate(image_path);

	return non_options;
}

}


int run_pp_tool(int argc, char const * argv[], pp_fct_run_t fct)
{
	try {
		vector<string> non_options = get_options(argc, argv);

		return fct(non_options);
	}
	catch (op_runtime_error const & e) {
		cerr << argv[0] << " op_runtime_error:\n" << e.what();
	}
	catch (op_fatal_error const & e) {
		cerr << argv[0] << " op_fatal_error:\n" << e.what();
	}
	catch (op_exception const & e) {
		cerr << argv[0] << " op_exception:\n" << e.what();
	}
	catch (invalid_argument const & e) {
		cerr << argv[0] << " invalid_argument:\n" << e.what();
	}
	catch (exception const & e) {
		cerr << argv[0] << " exception:\n" << e.what() << endl;
	}
	catch (...) {
		cerr << argv[0] << " unknown exception" << endl;
	}

	return EXIT_FAILURE;
}
