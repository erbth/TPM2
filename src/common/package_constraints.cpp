#include "package_constraints.h"

using namespace std;


namespace PackageConstraints
{

Formula::~Formula()
{
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
		(left ? left->fulfilled(sv, bv) : false) &&
		(right ? right->fulfilled(sv, bv) : false);
}

}
