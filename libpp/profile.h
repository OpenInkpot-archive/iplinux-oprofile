/**
 * @file profile.h
 * Encapsulation for samples files over all counter belonging to the
 * same binary image
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef PROFILE_H
#define PROFILE_H

#include <string>
#include <map>

#include "odb_hash.h"
#include "op_types.h"
#include "op_hw_config.h"
#include "utility.h"

class opd_header;

/** A class to store sample files over all counters */
class profile_t /*:*/  noncopyable {
public:
	/**
	 * profile_t - construct an profile_t object
	 * @param sample_file  sample file name
	 *
	 * store samples for one sample file, sample file header is sanitized.
	 *
	 * all error are fatal
	 */
	profile_t(std::string const & sample_file);

	~profile_t();
 
	/**
	 * check_mtime - check mtime of samples file against file
	 */
	void check_mtime(std::string const & file) const;

	/**
	 * accumulate_samples - lookup samples from a vma address
	 * @param vma index of the samples.
	 *
	 * return zero if no samples has been found
	 */
	unsigned int accumulate_samples(uint vma) const;

	/**
	 * accumulate_samples - lookup samples from a range of vma address
	 * @param counter where to accumulate the samples
	 * @param start start index of the samples.
	 * @param end end index of the samples.
	 *
	 * return zero if no samples has been found
	 */
	unsigned int accumulate_samples(uint start, uint end) const;

	/**
	 * output_header() - output counter setup
	 *
	 * output to stdout the cpu type, cpu speed
	 * and all counter description available
	 */
	void output_header() const;

	/// return the header of the first opened samples file
	opd_header const & first_header() const {
		return *file_header;
	}

	/**
	 * check_headers - check that the lhs and rhs headers are
	 * coherent (same size, same mtime etc.)
	 * @param headers the other _profile_t
	 *
	 * all errors are fatal
	 */
	void check_headers(profile_t const & headers) const;

	/**
	 * Set the start offset of the underlying samples files
	 * to non-zero (derived from the BFD) iff this contains
	 * the kernel or kernel module sample files.
	 */
	void set_start_offset(u32 start_offset);

private:
	std::string sample_filename;

	/// storage type for samples sorted by eip
	typedef std::map<odb_key_t, odb_value_t> ordered_samples_t;

	/// helper to build ordered samples by eip
	void build_ordered_samples(std::string const & filename);

	/// copy of the samples file header
	scoped_ptr<opd_header> file_header;

	/**
	 * Samples are stored in hash table, iterating over hash table don't
	 * provide any ordering, the above count() interface rely on samples
	 * ordered by eip. This map is only a temporary storage where samples
	 * are ordered by eip.
	 */
	ordered_samples_t ordered_samples;

	/**
	 * For the kernel and kernel modules, this value is non-zero and
	 * equal to the offset of the .text section. This is done because
	 * we use the information provided in /proc/ksyms, which only gives
	 * the mapped position of .text, and the symbol _text from
	 * vmlinux. This value is used to fix up the sample offsets
	 * for kernel code as a result of this difference (in user-space
	 * samples, the sample offset is from the start of the mapped
	 * file, as seen in /proc/pid/maps).
	 */
	u32 start_offset;
};

#endif /* !PROFILE_H */
