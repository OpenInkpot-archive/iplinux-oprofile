/**
 * @file generic_spec.h
 * Container holding an item or a special "match all" item
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef GENERIC_SPEC_H
#define GENERIC_SPEC_H

#include <stdexcept>
#include <string>
#include <sstream>

template <class T> class comma_list;


/**
 * used to hold something like { int cpu_nr, bool is_all };
 * to store a sub part of a samples filename see PP:3.21.
 */
template <class T>
class generic_spec
{
public:
	/**
	 * build a default spec which match anything
	 */
	generic_spec();

	/// build a spec from a string, valid argument are "all"
	/// or a string convertible to T through istringtream(str) >> data
	/// conversion is strict, no space are allowed at begin or end of str
	void set(std::string const &);

	/// return true if rhs match this spec. Sub part of PP:3.24
	bool match(T const & rhs) const { return is_all || rhs == data; }

	/// return true if rhs match this spec. Sub part of PP:3.24
	bool match(generic_spec<T> const & rhs) const {
		return is_all || rhs.is_all || rhs.data == data;
	}

private:
	T data;
	bool is_all;
};


/**
 * convert str to a T through an istringstream but conversion is strict:
 * no space are allowed at begin or end of str.
 * throw invalid_argument if conversion fail.
 */
template <class T>
T strict_convert(std::string const & str)
{
	T value;

	std::istringstream in(str);
	// this doesn't work properly for 2.95/2.91 so with these compiler
	// " 33" is accepted as valid input, no big deal.
	in.unsetf(std::ios::skipws);
	in >> value;
	if (in.fail())
		throw std::invalid_argument("strict_convert<T>::set(\""+ str +"\")");
	// we can't check eof here, eof is reached at next read.
	char ch;
	in >> ch;
	if (!in.eof())
		throw std::invalid_argument("strict_convert<T>::set(\""+ str +"\")");

	return value;
}


template <class T>
generic_spec<T>::generic_spec()
	:
	data(T()),
	is_all(true)
{
}


template <class T>
void generic_spec<T>::set(std::string const & str)
{
	if (str == "all") {
		is_all = true;
		return;
	}

	is_all = false;
	data = strict_convert<T>(str);
}


/// An explicit specialization because generic_spec<string> doesn't want
/// a strict conversion but a simple copy
template <>
void generic_spec<std::string>::set(std::string const & str);

#endif /* !GENERIC_SPEC_H */
