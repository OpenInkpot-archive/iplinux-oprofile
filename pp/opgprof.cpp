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

#include "op_libiberty.h"
#include "op_fileio.h"
#include "string_filter.h"
#include "derive_files.h"
#include "file_manip.h"
#include "profile_container.h"
#include "partition_files.h"
#include "opgprof_options.h"
#include "cverb.h"

using namespace std;

namespace {

#define GMON_VERSION 1
#define GMON_TAG_TIME_HIST 0

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


void output_gprof(profile_container const & samples,
		  string gmon_filename, string image_name)
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

	op_bfd abfd(image_name, string_filter());

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

	sample_container::samples_iterator it  = samples.begin();
	sample_container::samples_iterator end = samples.end();
	for (; it != end ; ++it) {
		u32 pos = (it->second.vma - low_pc) / multiplier;
		u32 count = it->second.count;

		if (pos >= histsize) {
			cerr << "Bogus histogram bin " << pos
			     << ", larger than " << pos << " !\n";
			continue;
		}

		// FIXME: this doesn't actually cap to max value ?
		if (hist[pos] + count > (u16)-1) {
			cerr <<	"Warning: capping sample count by "
			     << hist[pos] + count - ((u16)-1) << endl;
		} else {
			hist[pos] += (u16)count;
		}
	}

	op_write_file(fp, hist, histsize * sizeof(u16));
	op_close_file(fp);

	free(hist);
}


string load_samples(partition_files const & files, profile_container & samples)
{
	// assert partition_files.nr_set() == 1
	string image_name;

	partition_files::filename_set const & file_set = files.set(0);

	partition_files::filename_set::const_iterator it;
	for (it = file_set.begin(); it != file_set.end(); ++it) {
		image_name = it->lib_image.empty() ? it->image : it->lib_image;

		cverb << "adding to samples container: " << it->image
		      << " " << it->lib_image << endl;

		// if the image files does not exist try to retrieve it
		image_name = check_image_name(options::alternate_filename,
					      image_name, it->sample_filename);

		// no need to warn if image_name is not readable
		// check_image_name() already do that
		if (op_file_readable(image_name)) {
			// FIXME: inefficient since we can have multiple
			// time the same binary file open bfd opened
			add_samples(samples, it->sample_filename,
				    image_name, image_name,
				    string_filter());
		}
	}

	return image_name;
}


int opgprof(int argc, char const * argv[])
{
	get_options(argc, argv);

	profile_container samples(false, osf_vma, true);

	string image_name = load_samples(*sample_file_partition, samples);

	output_gprof(samples, options::gmon_filename, image_name);

	return 0;
}


} // anonymous namespace


int main(int argc, char const * argv[])
{
	return run_pp_tool(argc, argv, opgprof);
}
