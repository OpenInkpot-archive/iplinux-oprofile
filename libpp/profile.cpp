/**
 * @file profile.cpp
 * Encapsulation for samples files over all counter belonging to the
 * same binary image
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <unistd.h>

#include <iostream>
#include <string>

#include <cerrno>

#include "op_header.h"
#include "op_config.h"
#include "op_sample_file.h"

#include "profile.h"

using namespace std;

profile_t::profile_t(string const & sample_file)
	:
	sample_filename(sample_file),
	start_offset(0)
{
	if (access(sample_filename.c_str(), R_OK) == 0) {
		build_ordered_samples(sample_file);
	} else {
		cerr << "profile_t:profile_t() Opening " << sample_filename
		     <<  "failed." << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}
}


profile_t::~profile_t()
{
}


unsigned int profile_t::accumulate_samples(uint index) const
{
	return accumulate_samples(index, index + 1);
}


unsigned int profile_t::accumulate_samples(uint start, uint end) const
{
	unsigned int count = 0;

	ordered_samples_t::const_iterator first, last;
	// FIXME: this is wrong since we use accumulate_samples(0, ~0);
	// as special constant to get the whole sample count. Fix here or
	// caller ?
	first = ordered_samples.lower_bound(start - start_offset);
	last = ordered_samples.lower_bound(end - start_offset);
	for ( ; first != last ; ++first) {
		count += first->second;
	}

	return count;
}


void profile_t::set_start_offset(u32 start_offset_)
{
	if (!get_header().is_kernel)
		return;

	start_offset = start_offset_;
}


void profile_t::build_ordered_samples(string const & filename)
{
	samples_odb_t samples_db;
	char * err_msg;

	int rc = odb_open(&samples_db, filename.c_str(), ODB_RDONLY,
		sizeof(struct opd_header), &err_msg);

	if (rc != EXIT_SUCCESS) {
		cerr << err_msg << endl;
		free(err_msg);
		exit(EXIT_FAILURE);
	}

	opd_header const & head = *static_cast<opd_header *>(samples_db.base_memory);

	if (head.version != OPD_VERSION) {
		cerr << "oprofpp: samples files version mismatch, are you "
			"running a daemon and post-profile tools with version "
			"mismatch ?" << endl;
		exit(EXIT_FAILURE);
	}

	if (file_header.get()) {
		op_check_header(head, *file_header);
	}

	file_header.reset(new opd_header(head));

	odb_node_nr_t node_nr, pos;
	odb_node_t * node = odb_get_iterator(&samples_db, &node_nr);

	for (pos = 0; pos < node_nr; ++pos) {
		if (node[pos].key) {
			ordered_samples_t::value_type
				val(node[pos].key, node[pos].value);
			ordered_samples.insert(val);
		}
	}

	odb_close(&samples_db);
}
