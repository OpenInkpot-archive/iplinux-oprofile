/**
 * @file sample_container.h
 * Internal implementation of sample container
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef SAMPLE_CONTAINER_H
#define SAMPLE_CONTAINER_H

#include <vector>
#include <string>

/**
 * Arbitrary container of sample entries. Can return
 * number of samples for a file or line number and
 * return the particular sample information for a VMA.
 */
class sample_container {
public:
	/// container type
	typedef std::vector<sample_entry> samples_t;

	typedef samples_t::size_type size_type;

	sample_entry const & operator[](size_type index) const;

	/// return the number of separate sample entries stored
	size_type size() const;

	/// add a sample entry. Can only be done before any lookups
	void push_back(sample_entry const &);

	/// return nr of samples in the given filename
	unsigned int accumulate_samples(std::string const & filename) const;

	/// return nr of samples at the given line nr in the given file
	unsigned int accumulate_samples(std::string const & filename,
					size_t linenr) const;

	/// return the sample entry for the given VMA if any
	sample_entry const * find_by_vma(bfd_vma vma) const;

private:
	/// build the symbol by file-location cache
	void build_by_loc() const;

	/// main sample entry container
	samples_t samples;

	typedef std::multiset<sample_entry const *, less_by_file_loc>
		samples_by_loc_t;

	// must be declared after the vector to ensure a correct life-time.

	/**
	 * Sample entries by file location. Lazily built when necessary,
	 * so mutable.
	 */
	mutable samples_by_loc_t samples_by_loc;
};

#endif /* SAMPLE_CONTAINER_H */
