/** A simple version number implementation
 *
 * A version number is composed of multiple positive int components and / or
 * character components.
 * 
 * And, 2.0 != 2.0.0. This is because it makes a difference if you say `Version
 * 2' or `Version 2.0' (the last one tends to sound more like a big `thing').
 * And 1.0 < 1.0.0 because it's nice to have a strict order (You'd said that,
 * too, wouldn't you? ;-)).
 * 
 * Appended letters like in 1.1.0h are mapped to an extra component (for
 * OpenSSL).  All appended letters represent one component, so 'ad' means a *
 * |{a..z}| + d = 1 * 26 + 4 = 30; like in ieee802.3ad. Case does not matter.
 * Characters are always bigger than numbers.
 *
 * The version number's alphabet is hence: {0, ..., 9} | {a, ... , z}. However
 * uppercase letters will be converted to lowercase during a version number's
 * construction, hence it is valid to use those, too. */

#ifndef __VERSION_NUMBER_H
#define __VERSION_NUMBER_H

#include <exception>
#include <string>
#include <vector>
#include <ostream>


struct VersionNumberComponent
{
	bool is_chr;
	union {
		char c;
		unsigned u;
	} val;

	VersionNumberComponent(unsigned u) : is_chr(false) { val.u = u; }
	VersionNumberComponent(char c) : is_chr(true) { val.c = c; }
};


class VersionNumber
{
protected:
	std::vector<VersionNumberComponent> components;

public:
	VersionNumber(const std::string);
	VersionNumber(const VersionNumber &);

	std::string to_string() const;

	bool operator==(const VersionNumber &o) const;
	bool operator!=(const VersionNumber &o) const;
	bool operator>=(const VersionNumber &o) const;
	bool operator<=(const VersionNumber &o) const;
	bool operator>(const VersionNumber &o) const;
	bool operator<(const VersionNumber &o) const;
};


class InvalidVersionNumberString : public std::exception
{
protected:
	std::string msg;

public:
	InvalidVersionNumberString(std::string s, std::string msg = "");
	const char* what() const noexcept override;
};


/* To make the VersionNumber ostream printable */
std::ostream &operator<<(std::ostream &out, const VersionNumber &v);

#endif /* __VERSION_NUMBER_H */
