/**
 * @file outsymbflag.h
 * output options
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OUTSYMBFLAG_H
#define OUTSYMBFLAG_H

// FIXME

/**
 * flags passed to the ctor of an output_symbol object. This also specify the
 * order of field output: lower enum tag ==> comes first in output order
 * Note than changing value of enum is not safe.
 */
enum outsymbflag {
	osf_none = 0,
	osf_vma = 1 << 0,
	/// this four field can be repeated on output for each counter.
	osf_nr_samples = 1 << 1,
	osf_nr_samples_cumulated = 1 << 2,
	osf_percent = 1 << 3,
	osf_percent_cumulated = 1 << 4,

	osf_symb_name = 1 << 5,
	osf_linenr_info = 1 << 6,
	osf_image_name = 1 << 7,

	/// don't treat percent for details as relative to symbol but relative
	/// to the total nr of samples
	osf_percent_details = 1 << 8,
	osf_percent_cumulated_details = 1 << 9,

	osf_app_name = 1 << 10,		// owning application

	/// These fields are considered immutable when showing details for one
	/// symbol, we show them only when outputting the symbol itself but
	/// we avoid to display them during details output
	osf_immutable_field = osf_symb_name + osf_image_name + osf_app_name
};

#endif // OUTSYMBFLAG_H
