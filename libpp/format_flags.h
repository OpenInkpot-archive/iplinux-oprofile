/**
 * @file format_flags.h
 * output options
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef FORMAT_FLAGS_H
#define FORMAT_FLAGS_H

/**
 * flags passed to the ctor of an output_symbol object. This also specify the
 * order of field output: lower enum tag ==> comes first in output order.
 * Note than changing value of enum is not safe. (why not ?)
 *
 * \sa formatter
 */
enum format_flags {
	osf_none = 0,
	/// a formatted memory address
	osf_vma = 1 << 0,
	/// number of samples
	osf_nr_samples = 1 << 1,
	/// number of samples accumulated
	osf_nr_samples_cumulated = 1 << 2,
	/// relative percentage of samples
	osf_percent = 1 << 3,
	/// relative percentage of samples accumulated
	osf_percent_cumulated = 1 << 4,
	/// output the (demangled) symbol name
	osf_symb_name = 1 << 5,
	/// output debug filename and line nr.
	osf_linenr_info = 1 << 6,
	/// output the image name for this line
	osf_image_name = 1 << 7,
	/**
	 * Output percentage for details, not relative
	 * to symbol but relative to the total nr of samples
	 */
	osf_percent_details = 1 << 8,
	/**
	 * Output percentage for details, not relative
	 * to symbol but relative to the total nr of samples,
	 * accumulated
	 */
	osf_percent_cumulated_details = 1 << 9,
	/// output owning application name
	osf_app_name = 1 << 10,

	/// These fields are considered immutable when showing details for one
	/// symbol, we show them only when outputting the symbol itself but
	/// we don't display them in the details output line
	osf_immutable_field = osf_symb_name + osf_image_name + osf_app_name
};


/**
 * Taking two symbol_entry input iterators return true if we need to format
 * VMA as 64 bits value else false.
 */
template <class input_iterator>
bool vma64_p(input_iterator first_symbol, input_iterator last_symbol)
{
	if (sizeof(bfd_vma) == 4)
		return false;

	// slow way, check all address
	for ( ; first_symbol != last_symbol ; ++first_symbol) {
		if ((*first_symbol)->sample.vma & ~0xffffffffLLU) {
			return true;
		}
	}

	return false;
}

#endif // FORMAT_FLAGS_H
