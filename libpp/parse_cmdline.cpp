/**
 * @file parse_cmdline.h
 * A tag:value as described by pp_interface parser
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include "parse_cmdline.h"
#include "string_manip.h"
#include "glob_filter.h"


using namespace std;


parse_cmdline::parse_cmdline()
	:
	set_p(false),
	file_spec_set_p(false)
{
	parse_table["sample-file"] = &parse_cmdline::parse_sample_file;
	parse_table["binary"] = &parse_cmdline::parse_binary;
	parse_table["session"] = &parse_cmdline::parse_session;
	parse_table["session-exclude"] =
		&parse_cmdline::parse_session_exclude;
	parse_table["image"] = &parse_cmdline::parse_image;
	parse_table["image-exclude"] = &parse_cmdline::parse_image_exclude;
	parse_table["lib-image"] = &parse_cmdline::parse_lib_image;
	parse_table["lib-image-exclude"] =
		&parse_cmdline::parse_lib_image_exclude;
	parse_table["event"] = &parse_cmdline::parse_event;
	parse_table["count"] = &parse_cmdline::parse_count;
	parse_table["unit-mask"] = &parse_cmdline::parse_unitmask;
	parse_table["tid"] = &parse_cmdline::parse_tid;
	parse_table["tgid"] = &parse_cmdline::parse_tgid;
	parse_table["cpu"] = &parse_cmdline::parse_cpu;
}


void parse_cmdline::set(string const & tag_value)
{
	string value;
	action_t action = get_handler(tag_value, value);
	if (!action) {
		throw invalid_argument("parse_cmdline::set(): not "
				       "a valid tag \"" + tag_value + "\"");
	}

	(this->*action)(value);
}

void parse_cmdline::set_image_or_lib_name(string const & str)
{
	set_p = true;
	image_or_lib_image.push_back(str);
}


bool parse_cmdline::is_valid_tag(string const & tag_value)
{
	string value;
	return get_handler(tag_value, value);
}


void parse_cmdline::validate()
{
	// 3.3 sample_file can be used only with binary
	// 3.4 binary can be used only with sample_file
	if (file_spec_set_p || !binary.empty()) {
		if (!is_empty()) {
			throw invalid_argument("Cannot specify sample-file: or binary: tag with another tag");
		}
	}

	// PP:3.5 no session given means use the current session.
	if (session.empty()) {
		session.push_back("current");
	}

	// PP:3.7 3.8 3.9 3.10: is it the right time to translate all filename
	// to absolute path ? (if yes do it after plugging this code in
	// oprofile)
}


vector<string> parse_cmdline::get_session() const
{
	return session;
}


vector<string> parse_cmdline::get_session_exclude() const
{
	return session_exclude;
}


void parse_cmdline::parse_sample_file(string const & str)
{
	file_spec_set_p = true;
	file_spec.set_sample_filename(str);
}


void parse_cmdline::parse_binary(string const & str)
{
	binary = str;
}


void parse_cmdline::parse_session(string const & str)
{
	set_p = true;
	separate_token(session, str, ',');
}


void parse_cmdline::parse_session_exclude(string const & str)
{
	set_p = true;
	separate_token(session_exclude, str, ',');
}


void parse_cmdline::parse_image(string const & str)
{
	set_p = true;
	separate_token(image, str, ',');
}


void parse_cmdline::parse_image_exclude(string const & str)
{
	set_p = true;
	separate_token(image_exclude, str, ',');
}


void parse_cmdline::parse_lib_image(string const & str)
{
	set_p = true;
	separate_token(lib_image, str, ',');
}


void parse_cmdline::parse_lib_image_exclude(string const & str)
{
	set_p = true;
	separate_token(lib_image_exclude, str, ',');
}


void parse_cmdline::parse_event(string const & str)
{
	set_p = true;
	event.set(str);
}


void parse_cmdline::parse_unitmask(string const & str)
{
	set_p = true;
	unitmask.set(str);
}


void parse_cmdline::parse_count(string const & str)
{
	set_p = true;
	count.set(str);
}


void parse_cmdline::parse_tid(string const & str)
{
	set_p = true;
	tid.set(str, false);
}


void parse_cmdline::parse_tgid(string const & str)
{
	set_p = true;
	tgid.set(str, false);
}


void parse_cmdline::parse_cpu(string const & str)
{
	set_p = true;
	cpu.set(str, false);
}


parse_cmdline::action_t
parse_cmdline::get_handler(string const & tag_value, string & value)
{
	string::size_type pos = tag_value.find_first_of(':');
	if (pos == string::npos) {
		return 0;
	}

	string tag(tag_value.substr(0, pos));
	value = tag_value.substr(pos + 1);

	parse_table_t::const_iterator it = parse_table.find(tag);
	if (it == parse_table.end()) {
		return 0;
	}

	return it->second;
}


bool parse_cmdline::match(string const & filename) const
{
	filename_spec spec(filename);

	// PP:3.3 if spec was defined through sample-file: match it directly
	if (file_spec_set_p) {
		return file_spec.match(spec, binary);
	}

	bool matched_by_image_or_lib_image = false;

	// PP:3.19
	if (!image_or_lib_image.empty()) {
		glob_filter f_1(image_or_lib_image, image_exclude);
		glob_filter f_2(image_or_lib_image, lib_image_exclude);
		if (f_1.match(spec.image) || f_2.match(spec.lib_image)) {
			matched_by_image_or_lib_image = true;
		}
	}

	if (!matched_by_image_or_lib_image) {
		// PP:3.7 3.8
		if (!image.empty()) {
			glob_filter filter(image, image_exclude);
			if (!filter.match(spec.image)) {
				return false;
			}
		} else if (!image_or_lib_image.empty()) {
			// image.empty() means match all except if user
			// specified image_or_lib_image
			return false;
		}

		// PP:3.9 3.10
		if (!lib_image.empty()) {
			glob_filter filter(lib_image, lib_image_exclude);
			if (!filter.match(spec.lib_image)) {
				return false;
			}
		} else if (image.empty() && !image_or_lib_image.empty()) {
			// lib_image empty means match all except if user
			// specified image_or_lib_image *or* we already
			// matched this spec through image
			return false;
		}
	}

	if (!event.match(spec.event)) {
		return false;
	}

	if (!count.match(spec.count)) {
		return false;
	}

	if (!unitmask.match(spec.unitmask)) {
		return false;
	}

	if (!cpu.match(spec.cpu)) {
		return false;
	}

	if (!tid.match(spec.tid)) {
		return false;
	}

	if (!tgid.match(spec.tgid)) {
		return false;
	}

	return true;
}


bool parse_cmdline::is_empty() const
{
	return !set_p;
}


/* TODO */
static bool substitute_alias(parse_cmdline & /*parser*/,
			     string const & /*arg*/)
{
	return false;
}


parse_cmdline handle_non_options(vector<string> const & args)
{
	parse_cmdline parser;

	for (size_t i = 0 ; i < args.size() ; ++i) {
		if (parser.is_valid_tag(args[i])) {
			parser.set(args[i]);
		} else if (!substitute_alias(parser, args[i])) {
			parser.set_image_or_lib_name(args[i]);
		}
	}

	parser.validate();

	return parser;
}
