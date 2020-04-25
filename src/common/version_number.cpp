#include "version_number.h"
#include <charconv>

using namespace std;


VersionNumber::VersionNumber(const string s)
{
	auto cstr = s.c_str();
	auto length = s.size();

	auto begin = cstr;
	bool active_int = false;

	for (auto cur = cstr; cur != cstr + length; cur++)
	{
		/* Differentiate between int and character components */
		if (*cur >= '0' && *cur <= '9')
		{
			/* Int component */
			active_int = true;
		}
		else if (*cur == '.')
		{
			/* Separator - convert current component */
			if (active_int)
			{
				/* Regular int component */
				unsigned u = 0;
				from_chars(begin, cur, u);

				components.push_back(VersionNumberComponent(u));
				active_int = false;
			}
			else
			{
				/* If no int component is active, the previous one was either a
				 * char component, a deliminator or the start of the string. The
				 * latter two are not allowed. */
				if (cur - 1 < cstr || *(cur - 1) == '.')
				{
					throw InvalidVersionNumberString(
							s, "Empty components are not allowed.");
				}
			}

			begin = cur + 1;
		}
		else
		{
			/* Convert to lowercase */
			char c = tolower(*cur);

			if (c >= 'a' && c <= 'z')
			{
				/* Character component */
				/* If there is an active int component, add it first. */
				if (active_int)
				{
					unsigned u = 0;
					from_chars(begin, cur, u);

					components.push_back(VersionNumberComponent(u));
					active_int = false;
				}

				/* Add char component */
				components.push_back(VersionNumberComponent(c));

				begin = cur + 1;
			}
			else
			{
				throw InvalidVersionNumberString(
						s,
						string("Invalid component letter '") + c + "'");
			}
		}
	}


	/* Convert a last int component if one is left. */
	if (active_int)
	{
		unsigned u = 0;
		from_chars(begin, cstr + length, u);

		components.push_back(VersionNumberComponent(u));
	}


	/* Check that the version number does not end with a dot */
	if (length > 0 && *(cstr + length - 1) == '.')
		throw InvalidVersionNumberString(s, "Empty components are not allowed.");


	/* Ensure that the version number contains at least one component. In fact
	 * that's always the case if the string was not empty. But this is more
	 * obvious. */
	if (components.size() == 0)
		throw InvalidVersionNumberString(
				s, "At least one component must be provided.");
}

VersionNumber::VersionNumber(const VersionNumber &o)
{
	components = o.components;
}


string VersionNumber::to_string() const
{
	string s;
	bool last_chr = false;

	for (const VersionNumberComponent &c : components)
	{
		if (s.size() > 0 && !(last_chr && c.is_chr))
			s += ".";

		if (c.is_chr)
		{
			s += c.val.c;
			last_chr = true;
		}
		else
		{
			s += std::to_string(c.val.u);
			last_chr = false;
		}
	}

	return s;
}


bool VersionNumber::operator==(const VersionNumber &o) const
{
	if (components.size() != o.components.size())
		return false;

	auto i1 = components.cbegin();
	auto i2 = o.components.cbegin();

	while (i1 != components.cend())
	{
		if (i1->is_chr != i2->is_chr)
			return false;

		if (i1->is_chr)
		{
			if (i1->val.c != i2->val.c)
				return false;
		}
		else
		{
			if (i1->val.u != i2->val.u)
				return false;
		}

		i1++;
		i2++;
	}

	return true;
}

bool VersionNumber::operator!=(const VersionNumber &o) const
{
	return !(*this == o);
}


bool VersionNumber::operator>=(const VersionNumber &o) const
{
	auto i1 = components.cbegin();
	auto i2 = o.components.cbegin();

	for (;;)
	{
		if (i2 == o.components.cend())
		{
			return true;
		}
		else if (i1 == components.cend())
		{
			if (i2 == o.components.cend())
				return true;
			else
				return false;
		}
		else if (i1->is_chr && !i2->is_chr)
		{
			return true;
		}
		else if (i2->is_chr && !i1->is_chr)
		{
			return false;
		}
		else if (i1->is_chr)
		{
			if (i1->val.c > i2->val.c)
				return true;
			else if (i1->val.c < i2->val.c)
				return false;
		}
		else
		{
			if (i1->val.u > i2->val.u)
				return true;
			else if (i1->val.u < i2->val.u)
				return false;
		}

		i1++;
		i2++;
	}
}

bool VersionNumber::operator<=(const VersionNumber &o) const
{
	return o >= *this;
}

bool VersionNumber::operator>(const VersionNumber &o) const
{
	return !(o >= *this);
}

bool VersionNumber::operator<(const VersionNumber &o) const
{
	return !(*this >= o);
}


/* To make the VersionNumber ostream printable */
std::ostream &operator<<(std::ostream &out, const VersionNumber &v)
{
	out << "VersionNumber(" << v.to_string() << ")";
	return out;
}


/******************************** Exceptions **********************************/
InvalidVersionNumberString::InvalidVersionNumberString(string s, string msg)
{
	this->msg = s + ": " + msg;
}

const char *InvalidVersionNumberString::what() const noexcept
{
	return msg.c_str();
}
