/**
 * @file sample_container.cpp
 * Internal container for samples
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <set>
#include <numeric>
#include <algorithm>
#include <vector>

#include "opp_symbol.h"
#include "symbol_functors.h"
#include "sample_container.h"

using namespace std;

namespace {

inline unsigned int add_counts(unsigned int count, sample_entry const * s)
{
	return count + s->count;
}

} // namespace anon


sample_entry const & sample_container::operator[](sample_container::size_type index) const
{
	return samples[index];
}


sample_container::size_type sample_container::size() const
{
	return samples.size();
}


void sample_container::push_back(sample_entry const & sample)
{
	samples.push_back(sample);
}


unsigned int
sample_container::accumulate_samples(string const & filename) const
{
	build_by_loc();

	sample_entry lower, upper;

	lower.file_loc.filename = upper.file_loc.filename = filename;
	lower.file_loc.linenr = 0;
	upper.file_loc.linenr = INT_MAX;

	typedef samples_by_loc_t::const_iterator iterator;

	iterator it1 = samples_by_loc.lower_bound(&lower);
	iterator it2 = samples_by_loc.upper_bound(&upper);

	return accumulate(it1, it2, 0, add_counts);
}


sample_entry const * sample_container::find_by_vma(bfd_vma vma) const
{
	sample_entry value;

	value.vma = vma;

	samples_t::const_iterator it =
		lower_bound(samples.begin(), samples.end(), value,
			    less_sample_entry_by_vma());

	if (it != samples.end() && it->vma == vma)
		return &(*it);

	return 0;
}


unsigned int
sample_container::accumulate_samples(string const & filename,
		size_t linenr) const
{
	build_by_loc();

	sample_entry sample;

	sample.file_loc.filename = filename;
	sample.file_loc.linenr = linenr;

	typedef pair<samples_by_loc_t::const_iterator,
		samples_by_loc_t::const_iterator> it_pair;

	it_pair itp = samples_by_loc.equal_range(&sample);

	return accumulate(itp.first, itp.second, 0, add_counts);
}


void sample_container::build_by_loc() const
{
	if (!samples_by_loc.empty())
		return;

	samples_t::const_iterator cit = samples.begin();
	samples_t::const_iterator end = samples.end();
	for (; cit != end; ++cit)
		samples_by_loc.insert(&*cit);
}
