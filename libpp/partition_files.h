/**
 * @file partition_files.h
 * Encapsulation for merging and partitioning samples filename set
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef PARTITION_FILES_H
#define PARTITION_FILES_H

#include <string>
#include <iostream>
#include <vector>
#include <list>

/**
 * store merging options options used to partition samples filename
 */
struct merge_option {
	bool merge_cpu;
	bool merge_lib;
	bool merge_tid;
	bool merge_tgid;
	bool merge_unitmask;
};

/**
 * unmergeable profile specification are those with distinct event/count
 */
struct unmergeable_profile {
	std::string event;
	std::string count;
	unmergeable_profile(std::string const & event_, 
			    std::string const & count_);

	bool operator<(unmergeable_profile const & rhs) const;
};

/**
 * @param files files the file list from we extract unmergeable spec
 *
 * return a vector of unmergeable profile
 *
 */
std::vector<unmergeable_profile> merge_profile(std::list<std::string> const & files);

/// convenience function for debug/verbose
std::ostream & operator<<(std::ostream & out, unmergeable_profile const & lhs);


/// FIXME: doc
class partition_files {

public:
	typedef std::list<std::string> filename_set;

	/**
	 * @param files a list of filename to partition
	 * @param merge_cpu allow to merge regardless of cpu spec
	 * @param merge_lib allow to merge regardless of lib spec
	 * @param merge_tid allow to merge regardless of tid spec
	 * @param merge_tgid allow to merge regardless of tgid spec
	 * @param merge_unimask allow to merge regardless of unitmask spec
	 *
	 *
	 * complexity: f(N*log(N)) N: files.size()
	 */
	partition_files(std::list<std::string> const & files,
			merge_option const & merge_by);


	/**
	 * return the number of unmerged set of filename
	 */
	size_t nr_set() const;

	/*
	 * @param index filename set index
	 *
	 * return the filename set at position index
	 */
	filename_set const & set(size_t index) const;

private:
	typedef std::list<filename_set> filename_partition;
	filename_partition filenames;
};

#endif /* !PARTITION_FILES_H */
