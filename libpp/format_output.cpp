/**
 * @file format_output.cpp
 * outputting format for symbol lists
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <sstream>
#include <iomanip>

#include "file_manip.h"

#include "opp_symbol.h"
#include "format_output.h"
#include "profile_container.h"
#include "sample_container.h"
#include "demangle_symbol.h"

using namespace std;

namespace options {
	extern bool demangle;
}

struct output_option {
	char option;
	outsymbflag flag;
	string help_string;
};

static output_option const output_options[] = {
	{ 'v', osf_vma, "vma offset" },
	{ 's', osf_nr_samples, "nr samples" },
	{ 'S', osf_nr_samples_cumulated, "nr cumulated samples" },
	{ 'p', osf_percent, "nr percent samples" },
	{ 'P', osf_percent_cumulated, "nr cumulated percent samples" },
	{ 'q', osf_percent_details, "nr percent samples details" },
	{ 'Q', osf_percent_cumulated_details, "nr cumulated percent samples details" },
	{ 'n', osf_symb_name, "symbol name" },
	{ 'l', osf_linenr_info, "source file name and line nr" },
	{ 'i', osf_image_name, "image name" },
	{ 'e', osf_app_name, "owning application name" },
};

size_t const nr_output_option = sizeof(output_options) / sizeof(output_options[0]);

namespace {

output_option const * find_option(char ch)
{
	for (size_t i = 0 ; i < nr_output_option ; ++i) {
		if (ch == output_options[i].option) {
			return &output_options[i];
		}
	}

	return 0;
}

}

 
namespace format_output {

outsymbflag parse_format(string const & option)
{
	size_t flag = 0;
	for (size_t i = 0 ; i < option.length() ; ++i) {
		output_option const * opt = find_option(option[i]);
		if (!opt)
			return osf_none;
		flag |= opt->flag;
	}

	return static_cast<outsymbflag>(flag);
}


void show_help(ostream & out)
{
	out << "--output format string:\n";
	for (size_t i = 0 ; i < nr_output_option ; ++i) {
		out << output_options[i].option << "\t"
		    << output_options[i].help_string << endl;
	}
}


formatter::formatter(profile_container const & profile_)
	:
	flags(osf_none),
	profile(profile_),
	first_output(true),
	vma_64(false),
	need_details(false),
	need_header(false),
	short_filename(false)
{
	total_count = profile.samples_count();
	total_count_details = profile.samples_count();
	cumulated_samples = 0;
	cumulated_percent = 0;
	cumulated_percent_details = 0;

	format_map[osf_vma] = field_description(9, "vma", &formatter::format_vma);
	format_map[osf_nr_samples] = field_description(9, "samples", &formatter::format_nr_samples);
	format_map[osf_nr_samples_cumulated] = field_description(9, "cum. samples", &formatter::format_nr_cumulated_samples);
	format_map[osf_percent] = field_description(12, "%", &formatter::format_percent);
	format_map[osf_percent_cumulated] = field_description(10, "cum. %", &formatter::format_cumulated_percent);
	format_map[osf_symb_name] = field_description(24, "symbol name", &formatter::format_symb_name);
	format_map[osf_linenr_info] = field_description(28, "linenr info", &formatter::format_linenr_info);
	format_map[osf_image_name] = field_description(24, "image name", &formatter::format_image_name);
	format_map[osf_percent] = field_description(12, "%", &formatter::format_percent);
	format_map[osf_percent_cumulated] = field_description(10, "cum %", &formatter::format_cumulated_percent);
	format_map[osf_percent_details] = field_description(12, "%", &formatter::format_percent_details);
	format_map[osf_percent_cumulated_details] =field_description(10, "cum. %", &formatter::format_cumulated_percent_details);
	format_map[osf_app_name] = field_description(24, "app name", &formatter::format_app_name);
}

 
void formatter::add_format(outsymbflag flag)
{
	flags = static_cast<outsymbflag>(flags | flag);
}


void formatter::output(ostream & out, symbol_entry const * symb, bool vma_64_)
{
	vma_64 = vma_64_;
	do_output(out, symb->name, symb->sample, false);

	if (need_details) {
		output_details(out, symb);
	}
}


void formatter::output(ostream & out,
			  vector<symbol_entry const *> const & symbols,
			  bool reverse, bool vma_64_)
{
	if (reverse) {
		vector<symbol_entry const *>::const_reverse_iterator it;
		for (it = symbols.rbegin(); it != symbols.rend(); ++it) {
			output(out, *it, vma_64_);
		}
	} else {
		vector<symbol_entry const *>::const_iterator it;
		for (it = symbols.begin(); it != symbols.end(); ++it) {
			output(out, *it, vma_64_);
		}
	}
}


void formatter::show_details()
{
	need_details = true;
}


void formatter::show_header()
{
	need_header = true;
}

void formatter::show_short_filename()
{
	short_filename = true;
}
 

/// describe each possible field of colummned output.
// FIXME: some field have header_name too long (> field_description::width)
// FIXME: use % of the screen width here. sum of % equal to 100, then calculate
// ratio between 100 and the selected % to grow non fixed field use also
// lib[n?]curses to get the console width (look info source) (so on add a fixed
// field flags)
size_t formatter::output_field(ostream & out, string const & name,
				 sample_entry const & sample,
				 outsymbflag fl, size_t padding)
{
	out << string(padding, ' ');
	padding = 0;

	field_description const & field(format_map[fl]);
	string str = (this->*field.formatter)(field_datum(name, sample, vma_64));
	out << str;

	padding = 1;	// at least one separator char
	if (str.length() < field.width)
		padding = field.width - str.length();

	return padding;
}

 
size_t formatter::output_header_field(ostream & out, outsymbflag fl,
					size_t padding)
{
	out << string(padding, ' ');
	padding = 0;

	field_description const & field(format_map[fl]);
	out << field.header_name;

	padding = 1;	// at least one separator char
	if (field.header_name.length() < field.width)
		padding = field.width - field.header_name.length();

	return padding;
}
 

void formatter::output_details(ostream & out, symbol_entry const * symb)
{
	// We need to save the accumulated count and to restore it on
	// exit so global cumulation and detailed cumulation are separate
	u32 temp_total_count;
	u32 temp_cumulated_samples;
	u32 temp_cumulated_percent;

	temp_total_count = total_count;
	temp_cumulated_samples = cumulated_samples;
	temp_cumulated_percent = cumulated_percent;

	total_count = symb->sample.count;
	cumulated_percent_details -= symb->sample.count;
	cumulated_samples = 0;
	cumulated_percent = 0;

	sample_container::samples_iterator cur;
	for (cur = profile.begin(symb); cur != profile.end(symb); ++cur) {
		out << ' ';

		do_output(out, symb->name, cur->second, true);
	}

	total_count = temp_total_count;
	cumulated_samples = temp_cumulated_samples;
	cumulated_percent = temp_cumulated_percent;
}

 
void formatter::do_output(ostream & out, string const & name,
			  sample_entry const & sample,
			  bool hide_immutable_field)
{
	output_header(out);

	size_t padding = 0;

	size_t temp_flag = flags;
	for (size_t i = 1 ; temp_flag != 0 ; i <<= 1) {
		outsymbflag fl = static_cast<outsymbflag>(i);
		if (flags & fl) {
			if (hide_immutable_field && (fl & osf_immutable_field)) {
				field_description const & field(format_map[fl]);
				padding += field.width;
			} else {
				padding = output_field(out, name, sample, fl, padding);
			}
			temp_flag &= ~i;
		}
	}

	out << "\n";
}
 

void formatter::output_header(ostream & out)
{
	if (!first_output) {
		return;
	}

	first_output = false;

	if (need_header) {
		return;
	}

	size_t padding = 0;

	// now the remaining field
	size_t temp_flag = flags;
	for (size_t i = 1 ; temp_flag != 0 ; i <<= 1) {
		if ((temp_flag & i) != 0) {
			outsymbflag fl = static_cast<outsymbflag>(i);
			padding = output_header_field(out, fl, padding);
			temp_flag &= ~i;
		}
	}

	out << "\n";
}

 
string formatter::format_vma(field_datum const & f)
{
	ostringstream out;
	int width = f.vma_64 ? 16 : 8;

	out << hex << setw(width) << setfill('0') << f.sample.vma;

	return out.str();
}

 
string formatter::format_symb_name(field_datum const & f)
{
	if (f.name[0] == '?')
		return "(no symbol)";
	return options::demangle ? demangle_symbol(f.name) : f.name;
}

 
string formatter::format_image_name(field_datum const & f)
{
	return short_filename
		? basename(f.sample.file_loc.image_name)
		: f.sample.file_loc.image_name;
}

 
string formatter::format_app_name(field_datum const & f)
{
	return short_filename
		? basename(f.sample.file_loc.app_name)
		: f.sample.file_loc.app_name;
}

 
string formatter::format_linenr_info(field_datum const & f)
{
	ostringstream out;

	if (f.sample.file_loc.filename.length()) {
		string filename = short_filename
			? basename(f.sample.file_loc.filename)
			: f.sample.file_loc.filename;
		out << filename << ":" << f.sample.file_loc.linenr;
	} else {
		out << "(no location information)";
	}

	return out.str();
}

 
string formatter::format_nr_samples(field_datum const & f)
{
	ostringstream out;
	out << f.sample.count;
	return out.str();
}

 
string formatter::format_nr_cumulated_samples(field_datum const & f)
{
	ostringstream out;
	cumulated_samples += f.sample.count;
	out << cumulated_samples;
	return out.str();
}

 
string formatter::format_percent(field_datum const & f)
{
	ostringstream out;
	double ratio = op_ratio(f.sample.count, total_count);
	out << ratio * 100.0;
	return out.str();
}

 
string formatter::format_cumulated_percent(field_datum const & f)
{
	ostringstream out;
	cumulated_percent += f.sample.count;
	double ratio = op_ratio(cumulated_percent, total_count);
	out << ratio * 100.0;
	return out.str();
}

 
string formatter::format_percent_details(field_datum const & f)
{
	ostringstream out;
	double ratio = op_ratio(f.sample.count, total_count_details);
	out << ratio * 100.0;
	return out.str();
}

 
string formatter::format_cumulated_percent_details(field_datum const & f)
{
	ostringstream out;
	cumulated_percent_details += f.sample.count;
	double ratio = op_ratio(cumulated_percent_details,
				total_count_details);
	out << ratio * 100.0;
	return out.str();
}

}; // namespace format_output
