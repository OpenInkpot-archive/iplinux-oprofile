/**
 * @file partition_files.h
 * Encapsulation for merging and partitioning samples filename list
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <set>
#include <algorithm>
#include <iterator>

#include "cverb.h"
#include "partition_files.h"
#include "split_sample_filename.h"

using namespace std;

unmergeable_profile::unmergeable_profile(std::string const & event_,
					 std::string const & count_)
	:
	event(event_),
	count(count_)
{
}

bool unmergeable_profile::operator<(unmergeable_profile const & rhs) const
{
	return event < rhs.event || (event == rhs.event && count < rhs.count);
}

ostream & operator<<(ostream & out, unmergeable_profile const & lhs)
{
	out << lhs.event << " " << lhs.count;
	return out;
}

vector<unmergeable_profile> merge_profile(list<string> const & files)
{
	set<unmergeable_profile> spec_set;

	split_sample_filename model = split_sample_file(*files.begin());

	list<string>::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		split_sample_filename spec = split_sample_file(*it);
		spec_set.insert(unmergeable_profile(spec.event, spec.count));
	}

	vector<unmergeable_profile> result;
	copy(spec_set.begin(), spec_set.end(), back_inserter(result));

	return result;
}

/**
 * merge_compare - comparator used to partition a set of samples filename
 * into equivalence class.  The equivalence relation equiv(a, b) is given by
 * !merge_compare(a, b) && !merge_compare(b, a)
 */
class merge_compare {
public:
	merge_compare(merge_option const & merge_by);
	bool operator()(string const & lhs, string const & rhs) const;
private:
	split_sample_filename lhs;
	merge_option merge_by;
};

merge_compare::merge_compare(merge_option const & merge_by_)
	:
	merge_by(merge_by_)
{
}

bool merge_compare::operator()(string const & lhs_, string const & rhs_) const
{
	split_sample_filename lhs = split_sample_file(lhs_);
	split_sample_filename rhs = split_sample_file(rhs_);

	if (merge_by.merge_lib) {
		if (lhs.lib_image != rhs.lib_image)
			return lhs.lib_image < rhs.lib_image;
		if (lhs.lib_image.empty() && lhs.image != rhs.image)
			return lhs.image < rhs.image;
	} else {
		if (lhs.lib_image != rhs.lib_image)
			return lhs.lib_image < rhs.lib_image;

		if (lhs.image != rhs.image)
			return lhs.image < rhs.image;
	}

	if (lhs.event != rhs.event)
		return lhs.event < rhs.event;

	if (lhs.count != rhs.count)
		return lhs.count < rhs.count;

	if (!merge_by.merge_cpu && lhs.cpu != rhs.cpu)
		return lhs.cpu < rhs.cpu;

	if (!merge_by.merge_tid && lhs.tid != rhs.tid)
		return lhs.tid < rhs.tid;

	if (!merge_by.merge_tgid && lhs.tgid != rhs.tgid)
		return lhs.tgid < rhs.tgid;

	if (!merge_by.merge_unitmask && lhs.unitmask != rhs.unitmask)
		return lhs.unitmask < rhs.unitmask;

	return false;
}

partition_files::partition_files(list<string> const & filename,
				 merge_option const & merge_by)
{
	typedef multiset<string, merge_compare> spec_set;

	merge_compare compare(merge_by);
	spec_set files(compare);
	copy(filename.begin(), filename.end(), inserter(files, files.begin()));

	spec_set::const_iterator it = files.begin();
	while (it != files.end()) {
		pair<spec_set::const_iterator, spec_set::const_iterator>
			p_it = files.equal_range(*it);

		filename_set temp;
		copy(p_it.first, p_it.second, back_inserter(temp));
		filenames.push_back(temp);

		it = p_it.second;
	}

	cverb << "Partition entries: " << nr_set() << endl;
	filename_partition::const_iterator cit;
	for (cit = filenames.begin(); cit != filenames.end(); ++cit) {
		cverb << "Partition entry:\n";
		copy(cit->begin(), cit->end(), 
		     ostream_iterator<string>(cverb, "\n"));
	}
}

size_t partition_files::nr_set() const
{
	return filenames.size();
}

partition_files::filename_set const & partition_files::set(size_t index) const
{
	filename_partition::const_iterator it = filenames.begin();
	// cast because parameter to advance must be signed, using unsigned
	// produce warning ... (FIXME: use a vector of list ?)
	advance(it, filename_partition::difference_type(index));

	return *it;
}
