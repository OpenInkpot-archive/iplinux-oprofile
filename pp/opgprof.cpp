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
#include <cstdio>

#include "op_header.h"
#include "profile.h"
#include "op_libiberty.h"
#include "op_fileio.h"
#include "string_filter.h"
#include "file_manip.h"
#include "profile_container.h"
#include "arrange_profiles.h"
#include "opgprof_options.h"
#include "cverb.h"
#include "ocg_hash.h"

using namespace std;

namespace {

#define GMON_VERSION 1
#define GMON_TAG_TIME_HIST 0
#define GMON_TAG_CG_ARC 1

struct gmon_hdr {
	char cookie[4];
	u32 version;
	u32 spare[3];
};


void op_write_vma(FILE * fp, op_bfd const & abfd, bfd_vma vma)
{
	// bfd vma write size is a per binary property not a bfd
	// configuration property
	switch (abfd.bfd_arch_bits_per_address()) {
		case 32:
			op_write_u32(fp, vma);
			break;
		case 64:
			op_write_u64(fp, vma);
			break;
		default:
			cerr << "oprofile: unknwon vma size for this binary\n";
			exit(EXIT_FAILURE);
	}
}


void get_vma_range(bfd_vma & min, bfd_vma & max,
                   profile_container const & samples)
{
	min = bfd_vma(-1);
	max = 0;

	sample_container::samples_iterator it  = samples.begin();
	sample_container::samples_iterator end = samples.end();
	for (; it != end ; ++it) {
		if (it->second.vma < min)
			min = it->second.vma;
		if (it->second.vma > max)
			max = it->second.vma;
	}

	if (min == bfd_vma(-1))
		min = 0;
	// we must return a range [min, max) not a range [min, max]
	if (max != 0)
		max += 1;
}


/**
 * @param abfd  bfd object
 * @param samples_files  profile container to act on
 * @param gap  a power of 2
 *
 * return true if all sample in samples_files are at least aligned on gap. This
 * function is used to get at runtime the right size of gprof bin size
 * reducing gmon.out on arch with fixed size instruction length
 *
 */
bool aligned_samples(profile_container const & samples, int gap)
{
	sample_container::samples_iterator it  = samples.begin();
	sample_container::samples_iterator end = samples.end();
	for (; it != end ; ++it) {
		if (it->second.vma % gap)
			return false;
	}

	return true;
}


void output_cg(FILE * fp, op_bfd const & abfd, samples_ocg_t const & cg_db)
{
	ocg_node_nr_t node_nr, pos;
	ocg_node_t * node = ocg_get_iterator(&cg_db, &node_nr);

	opd_header const & head = *static_cast<opd_header *>(cg_db.base_memory);
	bfd_vma offset = abfd.get_start_offset();
	if (!head.is_kernel)
		offset = 0;

	for (pos = 0; pos < node_nr; ++pos) {
		if (!node[pos].key)
			continue;

		bfd_vma from = node[pos].key >> 32;
		bfd_vma to = node[pos].key & 0xffffffff;

		from += offset;
		to += offset;
		op_write_u8(fp, GMON_TAG_CG_ARC);
		op_write_vma(fp, abfd, abfd.offset_to_pc(from));
		op_write_vma(fp, abfd, abfd.offset_to_pc(to));
		op_write_u32(fp, node[pos].value);
	}
}


void output_gprof(op_bfd const & abfd, profile_container const & samples,
                  samples_ocg_t const & cg_db, bool is_cg,
                  string const & gmon_filename)
{
	static gmon_hdr hdr = { { 'g', 'm', 'o', 'n' }, GMON_VERSION, {0,0,0,},};

	bfd_vma low_pc;
	bfd_vma high_pc;

	/* FIXME worth to try more multiplier ? is ia64 with its chunk of
	 * instructions can get sample inside a chunck or always at chunk
	 * boundary ? */
	int multiplier = 2;
	if (aligned_samples(samples, 4))
		multiplier = 8;

	cverb << "opgrof multiplier: " << multiplier << endl;

	get_vma_range(low_pc, high_pc, samples);

	cverb << "low_pc: " << hex << low_pc << " " << "high_pc: "
	      << high_pc << dec << endl;

	// round-down low_pc to ensure bin number is correct in the inner loop
	low_pc = (low_pc / multiplier) * multiplier;
	// round-up high_pc to ensure a correct histsize calculus
	high_pc = ((high_pc + multiplier - 1) / multiplier) * multiplier;

	cverb << "low_pc: " << hex << low_pc << " " << "high_pc: "
	      << high_pc << dec << endl;

	size_t histsize = (high_pc - low_pc) / multiplier;

	FILE * fp = op_open_file(gmon_filename.c_str(), "w");

	op_write_file(fp,&hdr, sizeof(gmon_hdr));
	op_write_u8(fp, GMON_TAG_TIME_HIST);

	op_write_vma(fp, abfd, low_pc);
	op_write_vma(fp, abfd, high_pc);
	/* size of histogram */
	op_write_u32(fp, histsize);
	/* profiling rate */
	op_write_u32(fp, 1);
	op_write_file(fp, "samples\0\0\0\0\0\0\0\0", 15);
	/* abbreviation */
	op_write_u8(fp, '1');

	u16 * hist = (u16*)xcalloc(histsize, sizeof(u16));

	profile_container::symbol_choice choice;
	choice.threshold = options::threshold;
	symbol_collection symbols = samples.select_symbols(choice);

	symbol_collection::const_iterator sit = symbols.begin();
	symbol_collection::const_iterator send = symbols.end();

	for (; sit != send; ++sit) {
		sample_container::samples_iterator it  = samples.begin(*sit);
		sample_container::samples_iterator end = samples.end(*sit);
		for (; it != end ; ++it) {
			u32 pos = (it->second.vma - low_pc) / multiplier;
			u32 count = it->second.counts[0];

			if (pos >= histsize) {
				cerr << "Bogus histogram bin " << pos
			     << ", larger than " << pos << " !\n";
				continue;
			}
	
			if (hist[pos] + count > (u16)-1) {
				hist[pos] = (u16)-1;
				cerr <<	"Warning: capping sample count by "
				     << hist[pos] + count - ((u16)-1) << endl;
			} else {
				hist[pos] += (u16)count;
			}
		}
	}

	op_write_file(fp, hist, histsize * sizeof(u16));

	if (is_cg)
		output_cg(fp, abfd, cg_db);

	op_close_file(fp);

	free(hist);
}


void load_samples(op_bfd const & abfd, list<string> const & files,
                  string const & image, profile_container & samples)
{
	list<string>::const_iterator it = files.begin();
	list<string>::const_iterator const end = files.end();

	for (; it != end; ++it) {

		profile_t profile;

		profile.add_sample_file(*it, abfd.get_start_offset());

		check_mtime(abfd.get_filename(), profile.get_header());

		samples.add(profile, abfd, image, 0);
	}
}


// FIXME: merging
bool load_cg(samples_ocg_t & cg_db, string const & filename)
{
	string::size_type prefixend = filename.find_last_of("/");
	string const base = filename.substr(0, prefixend + 1);
	string const end = filename.substr(prefixend + 1, string::npos);

	string const cg_file = base + "{cg}/" + end;

	int rc = ocg_open(&cg_db, cg_file.c_str(), OCG_RDONLY,
		sizeof(struct opd_header));

	return rc == EXIT_SUCCESS;
}


int opgprof(vector<string> const & non_options)
{
	handle_options(non_options);

	profile_container samples(false, true);

	// FIXME: symbol_filter would be allowed through option
	op_bfd abfd(profiles.image, string_filter());

	load_samples(abfd, profiles.files, profiles.image, samples);

	samples_ocg_t cg_db;

	bool const is_cg = load_cg(cg_db, *(profiles.files.begin()));

	output_gprof(abfd, samples, cg_db, is_cg, options::gmon_filename);

	if (is_cg)
		ocg_close(&cg_db);

	return 0;
}


} // anonymous namespace


int main(int argc, char const * argv[])
{
	return run_pp_tool(argc, argv, opgprof);
}
