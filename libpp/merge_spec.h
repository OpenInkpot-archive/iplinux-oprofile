/**
 * @file merge_spec.h
 * Encapsulation for merging and partitioning samples filename set
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef MERGE_SPEC_H
#define MERGE_SPEC_H

#include <string>
#include <iostream>
#include <vector>
#include <list>

/**
 * unmergeable profile specification are those with disctinct event/count
 *
 * FIXME: perhaps obsolete, prefer to use partition_filename()
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
 * FIXME: perhaps obsolete, prefer to use partition_filename()
 */
std::vector<unmergeable_profile> merge_profile(std::list<std::string> const & diles);

/// convienence function for debug/verbose
// FIXME: perhaps obsolete, prefer to use partition_filename()
std::ostream & operator<<(std::ostream & out, unmergeable_profile const & lhs);


/**
 * @param files a list of filename to partition
 * @param merge_cpu allow to merge regardless of cpu filename spec
 * @param merge_lib allow to merge regardless of lib filename spec
 * @param merge_tid allow to merge regardless of tid filename spec
 * @param merge_tgid allow to merge regardless of tgid filename spec
 * @param merge_unimask allow to merge regardless of unitmask filename spec
 *
 * return a list of list of filename, each sub-list is an equivalent class
 * of merged filename
 *
 * complexity: f(N*log(N)) N: files.size()
 */
std::list<std::list<std::string> >
partition_files(std::list<std::string> const & files,
		bool merge_cpu, bool merge_lib,
		bool merge_tid, bool merge_tgid,
		bool merge_unitmask);

#endif /* !MERGE_SPEC_H */
