/**
 * @file merge_spec.h
 * Encapsulation for merging and partitioning samples filename list
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <set>
#include <algorithm>

#include "merge_spec.h"
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
	merge_compare(bool merge_cpu, bool merge_lib, bool merge_tid,
		      bool merge_tgid, bool merge_unitmask);
	bool operator()(string const & lhs, string const & rhs) const;
private:
	split_sample_filename lhs;
	bool merge_cpu;
	bool merge_lib;
	bool merge_tid;
	bool merge_tgid;
	bool merge_unitmask;
};

merge_compare::merge_compare(bool merge_cpu_, bool merge_lib_, bool merge_tid_,
			     bool merge_tgid_, bool merge_unitmask_)
	:
	merge_cpu(merge_cpu_),
	merge_lib(merge_lib_),
	merge_tid(merge_tid_),
	merge_tgid(merge_tgid_),
	merge_unitmask(merge_unitmask_)
{
}

bool merge_compare::operator()(string const & lhs_, string const & rhs_) const
{
	split_sample_filename lhs = split_sample_file(lhs_);
	split_sample_filename rhs = split_sample_file(rhs_);

	if (merge_lib) {
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

	if (!merge_cpu && lhs.cpu != rhs.cpu)
		return lhs.cpu < rhs.cpu;

	if (!merge_tid && lhs.tid != rhs.tid)
		return lhs.tid < rhs.tid;

	if (!merge_tgid && lhs.tgid  != rhs.tgid)
		return lhs.tgid < rhs.tgid;

	return false;
}

// FIXME: use a temp struct instead of lots of params
//
// f(N*log(N)) N: filename.size()
list<list<string> > partition_files(list<string> const & filename,
				    bool merge_cpu, bool merge_lib,
				    bool merge_tid, bool merge_tgid,
				    bool merge_unitmask)
{
	list<list<string> > result;

	typedef multiset<string, merge_compare> spec_set;

	merge_compare compare(merge_cpu, merge_lib, merge_tid, merge_tgid,
			merge_unitmask);
	spec_set files(compare);
	copy(filename.begin(), filename.end(), inserter(files, files.begin()));

	spec_set::const_iterator it = files.begin();
	while (it != files.end()) {
		pair<spec_set::const_iterator, spec_set::const_iterator>
			p_it = files.equal_range(*it);

		list<string> temp;
		copy(p_it.first, p_it.second, back_inserter(temp));
		result.push_back(temp);

		it = p_it.second;
	}

	return result;
}
