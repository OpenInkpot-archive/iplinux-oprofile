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
#include "string_manip.h"

#include "opp_symbol.h"
#include "format_output.h"
#include "profile_container.h"
#include "sample_container.h"
#include "demangle_symbol.h"

using namespace std;


namespace format_output {

formatter::formatter(profile_container const & profile_)
	:
	flags(osf_none),
	profile(profile_),
	first_output(true),
	vma_64(false),
	need_details(false),
	need_header(true),
	long_filenames(false)
{
	total_count = profile.samples_count();
	total_count_details = profile.samples_count();
	cumulated_samples = 0;
	cumulated_percent = 0;
	cumulated_percent_details = 0;

	format_map[osf_vma] = field_description(9, "vma", &formatter::format_vma);
	format_map[osf_nr_samples] = field_description(9, "samples", &formatter::format_nr_samples);
	format_map[osf_nr_samples_cumulated] = field_description(14, "cum. samples", &formatter::format_nr_cumulated_samples);
	format_map[osf_percent] = field_description(12, "%", &formatter::format_percent);
	format_map[osf_percent_cumulated] = field_description(11, "cum. %", &formatter::format_cumulated_percent);
	format_map[osf_linenr_info] = field_description(28, "linenr info", &formatter::format_linenr_info);
	format_map[osf_image_name] = field_description(25, "image name", &formatter::format_image_name);
	format_map[osf_app_name] = field_description(25, "app name", &formatter::format_app_name);
	format_map[osf_symb_name] = field_description(30, "symbol name", &formatter::format_symb_name);
	format_map[osf_percent_details] = field_description(12, "%", &formatter::format_percent_details);
	format_map[osf_percent_cumulated_details] = field_description(10, "cum. %", &formatter::format_cumulated_percent_details);
}

 
void formatter::add_format(format_flags flag)
{
	flags = static_cast<format_flags>(flags | flag);
}


void formatter::output(ostream & out, symbol_entry const * symb)
{
	do_output(out, *symb, symb->sample, false);

	if (need_details) {
		output_details(out, symb);
	}
}


void formatter::output(ostream & out,
                       vector<symbol_entry const *> const & symbols,
                       bool reverse, bool vma_64_)
{
	vma_64 = vma_64_;

	if (reverse) {
		vector<symbol_entry const *>::const_reverse_iterator it;
		for (it = symbols.rbegin(); it != symbols.rend(); ++it) {
			output(out, *it);
		}
	} else {
		vector<symbol_entry const *>::const_iterator it;
		for (it = symbols.begin(); it != symbols.end(); ++it) {
			output(out, *it);
		}
	}
}


void formatter::show_details()
{
	need_details = true;
}


void formatter::hide_header()
{
	need_header = false;
}


void formatter::show_long_filenames()
{
	long_filenames = true;
}
 

/// describe each possible field of colummned output.
// FIXME: some field have header_name too long (> field_description::width)
// FIXME: use % of the screen width here. sum of % equal to 100, then calculate
// ratio between 100 and the selected % to grow non fixed field use also
// lib[n?]curses to get the console width (look info source) (so on add a fixed
// field flags)
size_t formatter::output_field(ostream & out, symbol_entry const & symbol,
				 sample_entry const & sample,
				 format_flags fl, size_t padding)
{
	out << string(padding, ' ');
	padding = 0;

	field_description const & field(format_map[fl]);
	string str = (this->*field.formatter)(field_datum(symbol, sample));
	out << str;

	padding = 1;	// at least one separator char
	if (str.length() < field.width)
		padding = field.width - str.length();

	return padding;
}

 
size_t formatter::output_header_field(ostream & out, format_flags fl,
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

		do_output(out, *symb, cur->second, true);
	}

	total_count = temp_total_count;
	cumulated_samples = temp_cumulated_samples;
	cumulated_percent = temp_cumulated_percent;
}

 
void formatter::do_output(ostream & out, symbol_entry const & symb,
			  sample_entry const & sample,
			  bool hide_immutable_field)
{
	output_header(out);

	size_t padding = 0;

	size_t temp_flag = flags;
	for (size_t i = 1 ; temp_flag != 0 ; i <<= 1) {
		format_flags fl = static_cast<format_flags>(i);
		if (flags & fl) {
			if (hide_immutable_field && (fl & osf_immutable_field)) {
				field_description const & field(format_map[fl]);
				padding += field.width;
			} else {
				padding = output_field(out, symb, sample, fl, padding);
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

	if (!need_header) {
		return;
	}

	size_t padding = 0;

	// now the remaining field
	size_t temp_flag = flags;
	for (size_t i = 1 ; temp_flag != 0 ; i <<= 1) {
		if ((temp_flag & i) != 0) {
			format_flags fl = static_cast<format_flags>(i);
			padding = output_header_field(out, fl, padding);
			temp_flag &= ~i;
		}
	}

	out << "\n";
}

 
string formatter::format_vma(field_datum const & f)
{
	ostringstream out;
	int width = vma_64 ? 16 : 8;

	out << hex << setw(width) << setfill('0') << f.sample.vma;

	return out.str();
}

 
string formatter::format_symb_name(field_datum const & f)
{
	if (f.symbol.name[0] == '?')
		return "(no symbol)";
	return demangle_symbol(f.symbol.name);
}

 
string formatter::format_image_name(field_datum const & f)
{
	return long_filenames
		? f.symbol.image_name
		: basename(f.symbol.image_name);
}

 
string formatter::format_app_name(field_datum const & f)
{
	return long_filenames
		? f.symbol.app_name
		: basename(f.symbol.app_name);
}

 
string formatter::format_linenr_info(field_datum const & f)
{
	ostringstream out;

	if (!f.sample.file_loc.filename.empty()) {
		string filename = long_filenames
			? f.sample.file_loc.filename
			: basename(f.sample.file_loc.filename);
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
	return format_double(ratio * 100, percent_int_width,
			     percent_fract_width);
}

 
string formatter::format_cumulated_percent(field_datum const & f)
{
	ostringstream out;
	cumulated_percent += f.sample.count;
	double ratio = op_ratio(cumulated_percent, total_count);
	return format_double(ratio * 100, percent_int_width,
			     percent_fract_width);
}

 
string formatter::format_percent_details(field_datum const & f)
{
	ostringstream out;
	double ratio = op_ratio(f.sample.count, total_count_details);
	return format_double(ratio * 100, percent_int_width,
			     percent_fract_width);
}

 
string formatter::format_cumulated_percent_details(field_datum const & f)
{
	ostringstream out;
	cumulated_percent_details += f.sample.count;
	double ratio = op_ratio(cumulated_percent_details,
				total_count_details);
	return format_double(ratio * 100, percent_int_width,
			     percent_fract_width);
}

}; // namespace format_output
