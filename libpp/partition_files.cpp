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
#include "file_manip.h"
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
	if (files.empty())
		return vector<unmergeable_profile>();

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
	bool operator()(split_sample_filename const & lhs,
			split_sample_filename const & rhs) const;
private:
	merge_option merge_by;
};


merge_compare::merge_compare(merge_option const & merge_by_)
	:
	merge_by(merge_by_)
{
}


bool merge_compare::operator()(split_sample_filename const & lhs,
			       split_sample_filename const & rhs) const
{
	if (merge_by.lib) {
		if (lhs.lib_image != rhs.lib_image)
			return lhs.lib_image < rhs.lib_image;
		if (lhs.lib_image.empty() && lhs.image != rhs.image)
			return lhs.image < rhs.image;
	} else {
		if (lhs.image != rhs.image)
			return lhs.image < rhs.image;
	}

	if (lhs.event != rhs.event)
		return lhs.event < rhs.event;

	if (lhs.count != rhs.count)
		return lhs.count < rhs.count;

	if (!merge_by.cpu && lhs.cpu != rhs.cpu)
		return lhs.cpu < rhs.cpu;

	if (!merge_by.tid && lhs.tid != rhs.tid)
		return lhs.tid < rhs.tid;

	if (!merge_by.tgid && lhs.tgid != rhs.tgid)
		return lhs.tgid < rhs.tgid;

	if (!merge_by.unitmask && lhs.unitmask != rhs.unitmask)
		return lhs.unitmask < rhs.unitmask;

	return false;
}


partition_files::partition_files(list<string> const & filename,
				 merge_option const & merge_by)
{
	typedef multiset<split_sample_filename, merge_compare> spec_set;

	merge_compare compare(merge_by);
	spec_set files(compare);

	list<string>::const_iterator cit;
	for (cit = filename.begin(); cit != filename.end(); ++cit)
		files.insert(split_sample_file(*cit));

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
	filename_partition::const_iterator fit;
	for (fit = filenames.begin(); fit != filenames.end(); ++fit) {
		cverb << "Partition entry:\n";
		copy(fit->begin(), fit->end(), 
		     ostream_iterator<split_sample_filename>(cverb, ""));
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


image_set sort_by_image(partition_files const & files,
			extra_images const & extra_images)
{
	image_set result;

	for (size_t i = 0 ; i < files.nr_set(); ++i) {
		partition_files::filename_set const & file_set = files.set(i);

		partition_files::filename_set::const_iterator it;
		for (it = file_set.begin(); it != file_set.end(); ++it) {
			string image_name = it->lib_image.empty() ?
				it->image : it->lib_image;

			// if the image files does not exist try to retrieve it
			image_name = find_image_path(extra_images,
				image_name, it->sample_filename);

			// no need to warn if image_name is not readable
			// check_image_name() already do that
			if (op_file_readable(image_name)) {
				image_set::value_type value(image_name, *it);
				result.insert(value);
			}
		}
	}

	return result;
}
