/**
 * @file parse_cmdline.h
 * A tag:value as described by pp_interface parser
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef PARSE_CMDLINE_H
#define PARSE_CMDLINE_H

#include <map>
#include <vector>

#include "generic_spec.h"
#include "filename_spec.h"
#include "comma_list.h"


/**
 * command line parser for tag:value things.
 *
 *  @internal implemented through a map of string, pointer to function member
 *  indexed by tag_name.
 *
 * FIXME: this is either a singleton or a few free function with a bunch of
 * global variable, I prefer a singleton (forced to singleton by documentation
 * only)
 */
class parse_cmdline
{
public:
	parse_cmdline();

	/**
	 * @param tag_value  a "tag:value" to interpret, all error throw an
	 * invalid_argument exception.
	 */
	void set(std::string const & tag_value);

	/**
	 * @param str  a "tag:value"
	 *
	 * return true if tag is a valid tag
	 */
	bool is_valid_tag(std::string const &);

	/**
	 * must be called when parsing is finished to check constraint
	 * on argument stated in various place of PP:3
	 */
	void validate();

	/**
	 * getter allowing to create the sample filename candidate list
	 */
	std::vector<std::string> get_session() const;
	std::vector<std::string> get_session_exclude() const;

	/**
	 * @param filename  the filename to check
	 *
	 * return true if filename match the spec. PP:3.24 internal loop
	 */
	bool match(std::string const & filename) const;
private:
	/**
	 * implement tag parsing: PP:3.3 to 3.16
	 */
	void parse_sample_file(std::string const &);
	void parse_binary(std::string const &);
	void parse_session(std::string const &);
	void parse_session_exclude(std::string const &);
	void parse_image(std::string const &);
	void parse_image_exclude(std::string const &);
	void parse_lib_image(std::string const &);
	void parse_lib_image_exclude(std::string const &);
	void parse_event(std::string const &);
	void parse_count(std::string const &);
	void parse_unit_mask(std::string const &);
	void parse_tid(std::string const &);
	void parse_tgid(std::string const &);
	void parse_cpu(std::string const &);

	typedef void (parse_cmdline::*action_t)(std::string const &);
	typedef std::map<std::string, action_t> parse_table_t;
	parse_table_t parse_table;

	/**
	 * @param tag_value  input "tag:value" string
	 * @param value  if success return the value part of tag_value
	 * helper for set/is_valid_tag public interface
	 *
	 * return null if tag is not valid, else return the pointer to member
	 * function to apply and the value in value parameter
	 */
	action_t get_handler(std::string const & tag_value,
			     std::string & value);

	/**
	 * PP:3.3/3.4 constraint: tag sample-file and binary: cannot be used
	 * with any other tag
	 * 
	 * FIXME: mis-named ? is_set_except_sample_file_or_binary() ...
	 */
	bool is_empty() const;

	filename_spec file_spec;
	std::string binary;
	std::vector<std::string> session;
	std::vector<std::string> session_exclude;
	std::vector<std::string> image;
	std::vector<std::string> image_exclude;
	std::vector<std::string> lib_image;
	std::vector<std::string> lib_image_exclude;
	generic_spec<std::string> event;
	generic_spec<int> count;
	generic_spec<unsigned int> unit_mask;
	comma_list<pid_t> tid;
	comma_list<pid_t> tgid;
	comma_list<int> cpu;

	/// tree if any tag except sample-file: and binary: are seen
	bool set_p;
	/// true if samples-file: tag has been seen
	bool file_spec_set_p;
};


/**
 * @param args  a vector of non options strings
 *
 * return a parser_cmdline instance storing all valid tag:value contained in
 * args vector doing also alias substitution, non valid tag:value options are
 * considered as image:value
 *
 */
parse_cmdline handle_non_options(std::vector<std::string> const & args);

#endif /* !PARSE_CMDLINE_H */
