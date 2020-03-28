#include "package_constraints.h"

using namespace std;


namespace PackageConstraints
{

Formula::~Formula()
{
}

shared_ptr<Formula> Formula::from_string (const string& s)
{
	shared_ptr<Formula> f;

	/* Strings must start and end with paranthesis. Moreover each formula is at
	 * least 6 characters long:
	 *   * (>s:a)
	 *   * (&()())
	 *   * (|()())
	 */
	if (s.size() < 6 || s.front() != '(' || s.back() != ')')
		return nullptr;

	if (s[1] == '&' || s[1] == '|')
	{
		char op = s[1];

		/* Left to parse: (...)(...) */
		if (s[2] != '(')
			return nullptr;

		/* Find matching closing bracket of first expression */
		size_t j = 2;
		int cnt = 1;

		while (cnt > 0 && j + 1 < s.size())
		{
			j++;

			if (s[j] == '(')
				cnt++;
			else if (s[j] == ')')
				cnt--;
		}

		if (cnt != 0)
			return nullptr;

		string s1 = s.substr (2, j - 1);

		/* Left to parse: (...) */
		size_t i = j + 1;

		if (i >= s.size() || s[i] != '(')
			return nullptr;

		/* Find matching closing bracket of second expression */
		j = i;
		cnt = 1;

		while (cnt > 0 && j + 1 < s.size())
		{
			j++;

			if (s[j] == '(')
				cnt++;
			else if (s[j] == ')')
				cnt--;
		}

		if (cnt != 0)
			return nullptr;

		/* This must not be the rightmos bracket which terminates the current
		 * formula ... */
		if (j == s.size() - 1)
			return nullptr;

		string s2 = s.substr(i, j - i + 1);


		/* Parse sub-formulas or interpret them as netural elements */
		shared_ptr<Formula> p1, p2;

		if (s1 != "()")
		{
			p1 = Formula::from_string(s1);
			if (!p1)
				return nullptr;
		}

		if (s2 != "()")
		{
			p2 = Formula::from_string(s2);
			if (!p2)
				return nullptr;
		}

		if (op == '&')
			return make_shared<And> (p1, p2);
		else
			return make_shared<Or> (p1, p2);
	}
	else
	{
		/* Find operation + type specifier */
		char is_sourcec;
		string ops;
		string vs;

		if (s[3] == ':')
		{
			ops = s.substr(1,1);
			is_sourcec = s[2];
			vs = s.substr(4, s.size() - 5);
		}
		else if (s[4] == ':')
		{
			ops = s.substr(1,2);
			is_sourcec = s[3];
			vs = s.substr(5, s.size() - 6);
		}

		bool is_source;

		if (is_sourcec == 's')
			is_source = true;
		else if (is_sourcec == 'b')
			is_source = false;
		else
			return nullptr;

		char op;

		if (ops == "==")
			op = PrimitivePredicate::TYPE_EQ;
		else if (ops == "!=")
			op = PrimitivePredicate::TYPE_NEQ;
		else if (ops == ">=")
			op = PrimitivePredicate::TYPE_GEQ;
		else if (ops == "<=")
			op = PrimitivePredicate::TYPE_LEQ;
		else if (ops == ">")
			op = PrimitivePredicate::TYPE_GT;
		else if (ops == "<")
			op = PrimitivePredicate::TYPE_LT;
		else
			return nullptr;


		/* Check there is a version left */
		if (vs.size() == 0)
			return nullptr;

		try {
			/* Return primitive predicate */
			return make_shared<PrimitivePredicate> (is_source, op, VersionNumber(vs));
		} catch (InvalidVersionNumberString&) {
			return nullptr;
		}
	}

	return f;
}


PrimitivePredicate::PrimitivePredicate(bool is_source, char type, const VersionNumber &v)
	: is_source(is_source), type(type), v(v)
{
}

PrimitivePredicate::~PrimitivePredicate()
{
}

bool PrimitivePredicate::fulfilled(const VersionNumber &sv, const VersionNumber &bv) const
{
	const VersionNumber &tv = is_source ? sv : bv;

	switch (type)
	{
		case TYPE_EQ:
			return tv == v;

		case TYPE_NEQ:
			return tv != v;

		case TYPE_GEQ:
			return tv >= v;

		case TYPE_LEQ:
			return tv <= v;

		case TYPE_GT:
			return tv > v;

		case TYPE_LT:
			return tv < v;

		default:
			/* Fail safe */
			return false;
	}
}

string PrimitivePredicate::to_string() const
{
	string op;

	switch (type)
	{
		case TYPE_EQ:
			op = "==";
			break;

		case TYPE_NEQ:
			op = "!=";
			break;

		case TYPE_GEQ:
			op = ">=";
			break;

		case TYPE_LEQ:
			op = "<=";
			break;

		case TYPE_GT:
			op = ">";
			break;

		case TYPE_LT:
			op = "<";
			break;

		default:
			op = "?";
			break;
	};

	string vt = is_source ? "s:" : "b:";

	return "(" + op + vt + v.to_string() + ")";
}


And::And(shared_ptr<const Formula> left, shared_ptr<const Formula> right)
	: left(left), right(right)
{
}

And::~And()
{
}

bool And::fulfilled(const VersionNumber &sv, const VersionNumber &bv) const
{
	return
		(left ? left->fulfilled(sv, bv) : true) &&
		(right ? right->fulfilled(sv, bv) : true);
}

string And::to_string() const
{
	return "(&" +
		(left ? left->to_string() : "()") +
		(right ? right->to_string() : "()") + ")";
}


Or::Or(shared_ptr<const Formula> left, shared_ptr<const Formula> right)
	: left(left), right(right)
{
}

Or::~Or()
{
}

bool Or::fulfilled(const VersionNumber &sv, const VersionNumber &bv) const
{
	return
		(left ? left->fulfilled(sv, bv) : false) ||
		(right ? right->fulfilled(sv, bv) : false);
}

string Or::to_string() const
{
	return "(|" +
		(left ? left->to_string() : "()") +
		(right ? right->to_string() : "()") + ")";
}

}
