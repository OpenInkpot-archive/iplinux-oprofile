/**
 * @file op_header.cpp
 * various free function acting on a sample file header
 *
 * @remark Copyright 2004 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <iostream>
#include <cstdlib>

#include "odb_hash.h"
#include "op_cpu_type.h"
#include "op_print_event.h"
#include "op_file.h"
#include "op_header.h"

using namespace std;

void op_check_header(opd_header const & h1, opd_header const & h2)
{
	if (h1.mtime != h2.mtime) {
		cerr << "header timestamps are different ("
		     << h1.mtime << ", " << h2.mtime << ")\n";
		exit(EXIT_FAILURE);
	}

	if (h1.is_kernel != h2.is_kernel) {
		cerr << "header is_kernel flags are different\n";
		exit(EXIT_FAILURE);
	}

	if (h1.cpu_speed != h2.cpu_speed) {
		cerr << "header cpu speeds are different ("
		     << h1.cpu_speed << ", " << h2.cpu_speed << ")\n";
		exit(EXIT_FAILURE);
	}

	if (h1.separate_lib_samples != h2.separate_lib_samples) {
		cerr << "header separate_lib_samples are different ("
		     << h1.separate_lib_samples << ", " 
		     << h2.separate_lib_samples << ")\n";
		exit(EXIT_FAILURE);
	}

	if (h1.separate_kernel_samples != h2.separate_kernel_samples) {
		cerr << "header separate_kernel_samples are different ("
		     << h1.separate_kernel_samples << ", " 
		     << h2.separate_kernel_samples << ")\n";
		exit(EXIT_FAILURE);
	}
}


void check_mtime(string const & file, opd_header const & header)
{
	time_t const newmtime = op_get_mtime(file.c_str());

	if (newmtime == header.mtime)
	       return;

	// Files we couldn't get mtime of have zero mtime
	if (!header.mtime) {
		cerr << "warning: could not check that the binary file "
		     << file << "\nhas not been modified since "
			"the profile was taken. Results may be inaccurate.\n";
	} else {
		cerr << "warning: the last modified time of the binary file\n"
		     << file << "\ndoes not match that of the sample file.\n"
			"Either this is the wrong binary or the binary "
			"has been modified since the sample file was created.\n";
	}
}


opd_header read_header(string const & sample_filename)
{
	samples_odb_t samples_db;
	char * err_msg;

	int rc = odb_open(&samples_db, sample_filename.c_str(), ODB_RDONLY,
		sizeof(struct opd_header), &err_msg);

	if (rc != EXIT_SUCCESS) {
		cerr << err_msg << endl;
		free(err_msg);
		exit(EXIT_FAILURE);
	}

	opd_header head = *static_cast<opd_header *>(samples_db.base_memory);

	odb_close(&samples_db);

	return head;
}


ostream & operator<<(ostream & out, opd_header const & header)
{
	op_cpu cpu = static_cast<op_cpu>(header.cpu_type);

	out << "CPU type: " << op_get_cpu_type_str(cpu) << endl;

	out << "Estimated CPU speed was (MHz): " << header.cpu_speed << endl;

	op_print_event(out, cpu, header.ctr_event,
	               header.ctr_um, header.ctr_count);

	return out;
}
