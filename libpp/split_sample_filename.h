/**
 * @file sample_sample_filename.h
 * Container holding a sample filename splitted into its string components
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef SPLIT_SAMPLE_FILENAME_H
#define SPLIT_SAMPLE_FILENAME_H

#include <string>

/**
 * a convenience class to store result of split_filename
 */
struct split_sample_filename
{
	std::string base_dir;
	std::string image;
	std::string lib_image;
	std::string event;
	std::string count;
	std::string unitmask;
	std::string tgid;
	std::string tid;
	std::string cpu;
};


/**
 * split a sample filename
 * @param filename in: samples filename
 *
 * filename are splited in eight part, the lib_image is optionnal and can
 * be empty on successfull call. All other error are fatal. filenames
 * are encoded according to PP:3.19 to PP:3.25
 *
 * all error throw an std::invalid_argument exception
 *
 * return the splited filename
 */
split_sample_filename split_sample_file(std::string const & filename);

#endif /* !SPLIT_SAMPLE_FILENAME_H */
