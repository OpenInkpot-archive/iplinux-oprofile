/**
 * @file format_output.h
 * outputting format for symbol lists
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef FORMAT_OUTPUT_H
#define FORMAT_OUTPUT_H

#include "config.h"

#include <string>
#include <map>
#include <vector>
#include <iosfwd>

#include "outsymbflag.h"
#include "opp_symbol.h"

class profile_container;
class symbol_entry;

namespace format_output {
 
/// class to output in a columned format symbols and associated samples
class formatter {
public:
	/// build an output_symbol object, the profile_container life time
	/// object must be > of the life time of the output_symbol object.
	formatter(profile_container const & profile);

	/// convenience to add output options flags w/o worrying about cast
	void add_format(outsymbflag flag);

	/** output one symbol symb to out according to the output format
	 * specifier previously set by call(s) to add_format() */
	void output(std::ostream & out, symbol_entry const * symb,bool vma_64);
	/** output a vector of symbols to out according to the output format
	 * specifier previously set by call(s) to add_format() */
	void output(std::ostream & out,
		    std::vector<symbol_entry const *> const & v, bool reverse,
		    bool vma_64);

	/// set the output_details boolean
	void show_details();
	/// set the need_header boolean to false
	void hide_header();
	/// set the short_filename boolean
	void show_short_filename();
private:

	/// data passed for output
	struct field_datum {
		field_datum(std::string const & n, sample_entry const & s, bool vma_64_)
			: name(n), sample(s), vma_64(vma_64_) {}
		std::string const & name;
		sample_entry const & sample;
		bool vma_64;
	};
 
	/// format callback type
	typedef std::string (formatter::*fct_format)(field_datum const &);
 
	/** @name format functions.
	 * The set of formatting functions, used internally by output().
	 */
	//@{
	std::string format_vma(field_datum const &);
	std::string format_symb_name(field_datum const &);
	std::string format_image_name(field_datum const &);
	std::string format_app_name(field_datum const &);
	std::string format_linenr_info(field_datum const &);
	std::string format_nr_samples(field_datum const &);
	std::string format_nr_cumulated_samples(field_datum const &);
	std::string format_percent(field_datum const &);
	std::string format_cumulated_percent(field_datum const &);
	std::string format_percent_details(field_datum const &);
	std::string format_cumulated_percent_details(field_datum const &);
	//@}
 
	/// decribe one field of the colummned output.
	struct field_description {
		field_description() {}
		field_description(std::size_t w, std::string h, fct_format f)
			: width(w), header_name(h), formatter(f) {}
 
		std::size_t width;
		std::string header_name;
		fct_format formatter;
	};
 
	typedef std::map<outsymbflag, field_description> format_map_t;

	/// stores functors for doing actual formatting
	format_map_t format_map;
 
	/// actually do output
	void do_output(std::ostream & out, std::string const & name,
		      sample_entry const & sample, bool hide_immutable_field);
 
	/// output details for the symbol
	void output_details(std::ostream & out, symbol_entry const * symb);
 
	/// output table header
	void output_header(std::ostream & out);
 
	/// returns the nr of char needed to pad this field
	size_t output_field(std::ostream & out, std::string const & name,
			   sample_entry const & sample,
			   outsymbflag fl, size_t padding);
 
	/// returns the nr of char needed to pad this field
	size_t output_header_field(std::ostream & out, outsymbflag fl,
				 size_t padding);

	/// formatting flags set
	outsymbflag flags;
 
	/// container we work from
	profile_container const & profile;
 
	/// total sample count
	unsigned int total_count;
	/// samples so far
	unsigned int cumulated_samples;
	/// percentage so far
	unsigned int cumulated_percent;
	/// detailed total count
	unsigned int total_count_details;
	/// detailed percentage so far
	unsigned int cumulated_percent_details;
	/// used for outputting header
	bool first_output;
	/// true if we need to format as 64 bits quantities
	bool vma_64;
	/// true if we need to show details for each symbols
	bool need_details;
	/// true if we need to show header before the before the first output
	bool need_header;
	/// true if we use basename(filename) in output rather filename
	bool short_filename;
};

}; // namespace format_output 
 
#endif /* !FORMAT_OUTPUT_H */
