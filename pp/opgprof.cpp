/**
 * @file opgprof.cpp
 * Implement opgprof utility
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <iostream>

#include "opgprof_options.h"

using namespace std;

namespace {

int opgprof(int argc, char const * argv[])
{
	get_options(argc, argv);

	cerr << "N/A\n";
	return 0;
}

} // anonymous namespace

int main(int argc, char const * argv[])
{
	run_pp_tool(argc, argv, opgprof);
}
