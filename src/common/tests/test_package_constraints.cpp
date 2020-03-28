#define BOOST_TEST_MODULE test_package_constraints

#include <boost/test/included/unit_test.hpp>
#include "package_constraints.h"

using namespace std;
using namespace PackageConstraints;


BOOST_AUTO_TEST_CASE (test_primitive_predicate_fulfilled)
{
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_EQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.0")));
	BOOST_TEST (!PrimitivePredicate(false, PrimitivePredicate::TYPE_EQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("2.0")));

	BOOST_TEST (PrimitivePredicate(true, PrimitivePredicate::TYPE_EQ, VersionNumber("4.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.0")));
	BOOST_TEST (!PrimitivePredicate(true, PrimitivePredicate::TYPE_EQ, VersionNumber("4.0")).fulfilled(VersionNumber("3.0"), VersionNumber("2.0")));

	BOOST_TEST (!PrimitivePredicate(false, PrimitivePredicate::TYPE_NEQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.0")));
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_NEQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("2.0")));

	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_GEQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.0")));
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_GEQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.1")));
	BOOST_TEST (!PrimitivePredicate(false, PrimitivePredicate::TYPE_GEQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("0.9")));

	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_LEQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.0")));
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_LEQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("0.9")));
	BOOST_TEST (!PrimitivePredicate(false, PrimitivePredicate::TYPE_LEQ, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.1")));

	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_GT, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.1")));
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_GT, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.2")));
	BOOST_TEST (!PrimitivePredicate(false, PrimitivePredicate::TYPE_GT, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.0")));

	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_LT, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("0.9")));
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_LT, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("0.8")));
	BOOST_TEST (!PrimitivePredicate(false, PrimitivePredicate::TYPE_LT, VersionNumber("1.0")).fulfilled(VersionNumber("4.0"), VersionNumber("1.0")));
}


BOOST_AUTO_TEST_CASE (test_primitive_predicate_to_string)
{
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_EQ, VersionNumber("1.0")).to_string() == "(==b:1.0)");
	BOOST_TEST (PrimitivePredicate(true, PrimitivePredicate::TYPE_EQ, VersionNumber("1.0")).to_string() == "(==s:1.0)");
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_NEQ, VersionNumber("1.0")).to_string() == "(!=b:1.0)");
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_GEQ, VersionNumber("1.0")).to_string() == "(>=b:1.0)");
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_LEQ, VersionNumber("1.0")).to_string() == "(<=b:1.0)");
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_GT, VersionNumber("1.0")).to_string() == "(>b:1.0)");
	BOOST_TEST (PrimitivePredicate(false, PrimitivePredicate::TYPE_LT, VersionNumber("1.0")).to_string() == "(<b:1.0)");
}


BOOST_AUTO_TEST_CASE (test_and_fulfilled)
{
	auto p1 = make_shared<PrimitivePredicate> (false, (char) PrimitivePredicate::TYPE_GEQ, VersionNumber("1.0"));
	auto p2 = make_shared<PrimitivePredicate> (true, (char) PrimitivePredicate::TYPE_LT, VersionNumber("3.0"));
	And a(p1, p2);

	BOOST_TEST (a.fulfilled (VersionNumber("0.6"), VersionNumber("1.0")));
	BOOST_TEST (!a.fulfilled (VersionNumber("3.0"), VersionNumber("1.0")));
	BOOST_TEST (!a.fulfilled (VersionNumber("0.6"), VersionNumber("0.9")));
	BOOST_TEST (!a.fulfilled (VersionNumber("3.0"), VersionNumber("0.9")));

	BOOST_TEST (And(nullptr, nullptr).fulfilled (VersionNumber("0.6"), VersionNumber("1.0")));
	BOOST_TEST (And(p1, nullptr).fulfilled (VersionNumber("0.6"), VersionNumber("1.0")));
	BOOST_TEST (!And(p1, nullptr).fulfilled (VersionNumber("0.6"), VersionNumber("0.9")));
	BOOST_TEST (And(nullptr, p2).fulfilled (VersionNumber("0.6"), VersionNumber("1.0")));
	BOOST_TEST (!And(nullptr, p2).fulfilled (VersionNumber("3.0"), VersionNumber("1.0")));
}


BOOST_AUTO_TEST_CASE (test_and_to_string)
{
	auto p1 = make_shared<PrimitivePredicate> (false, (char) PrimitivePredicate::TYPE_GEQ, VersionNumber("1.0"));
	auto p2 = make_shared<PrimitivePredicate> (false, (char) PrimitivePredicate::TYPE_LT, VersionNumber("3.0"));

	BOOST_TEST (And(p1, p2).to_string() == "(&(>=b:1.0)(<b:3.0))");
	BOOST_TEST (And(p1, nullptr).to_string() == "(&(>=b:1.0)())");
	BOOST_TEST (And(nullptr, p2).to_string() == "(&()(<b:3.0))");
	BOOST_TEST (And(nullptr, nullptr).to_string() == "(&()())");
}


BOOST_AUTO_TEST_CASE (test_or_fulfilled)
{
	auto p1 = make_shared<PrimitivePredicate> (false, (char) PrimitivePredicate::TYPE_GEQ, VersionNumber("1.0"));
	auto p2 = make_shared<PrimitivePredicate> (true, (char) PrimitivePredicate::TYPE_LT, VersionNumber("3.0"));
	Or o(p1, p2);

	BOOST_TEST (o.fulfilled (VersionNumber("0.6"), VersionNumber("1.0")));
	BOOST_TEST (o.fulfilled (VersionNumber("3.0"), VersionNumber("1.0")));
	BOOST_TEST (o.fulfilled (VersionNumber("0.6"), VersionNumber("0.9")));
	BOOST_TEST (!o.fulfilled (VersionNumber("3.0"), VersionNumber("0.9")));

	BOOST_TEST (!Or(nullptr, nullptr).fulfilled (VersionNumber("0.6"), VersionNumber("1.0")));
	BOOST_TEST (Or(p1, nullptr).fulfilled (VersionNumber("0.6"), VersionNumber("1.0")));
	BOOST_TEST (!Or(p1, nullptr).fulfilled (VersionNumber("0.6"), VersionNumber("0.9")));
	BOOST_TEST (Or(nullptr, p2).fulfilled (VersionNumber("0.6"), VersionNumber("1.0")));
	BOOST_TEST (!Or(nullptr, p2).fulfilled (VersionNumber("3.0"), VersionNumber("1.0")));
}


BOOST_AUTO_TEST_CASE (test_or_to_string)
{
	auto p1 = make_shared<PrimitivePredicate> (false, (char) PrimitivePredicate::TYPE_GEQ, VersionNumber("1.0"));
	auto p2 = make_shared<PrimitivePredicate> (false, (char) PrimitivePredicate::TYPE_NEQ, VersionNumber("0.5"));

	BOOST_TEST (Or(p1, p2).to_string() == "(|(>=b:1.0)(!=b:0.5))");
	BOOST_TEST (Or(p1, nullptr).to_string() == "(|(>=b:1.0)())");
	BOOST_TEST (Or(nullptr, p2).to_string() == "(|()(!=b:0.5))");
	BOOST_TEST (Or(nullptr, nullptr).to_string() == "(|()())");
}


BOOST_AUTO_TEST_CASE (test_formula_from_string)
{
	/* A few corner cases */
	BOOST_TEST (Formula::from_string ("") == nullptr);
	BOOST_TEST (Formula::from_string ("(>>s:2.3") == nullptr);
	BOOST_TEST (Formula::from_string ("(>s:)") == nullptr);
	BOOST_TEST (Formula::from_string ("(>=s:)") == nullptr);
	BOOST_TEST (Formula::from_string ("(>=s:-)") == nullptr);
	BOOST_TEST (Formula::from_string ("(>>s:2)") == nullptr);
	BOOST_TEST (Formula::from_string ("(&()()") == nullptr);
	BOOST_TEST (Formula::from_string ("(&(>=b:1)()") == nullptr);

	/* Primitive preidactes */
	BOOST_TEST (Formula::from_string ("(==b:1.0)")->to_string() == "(==b:1.0)");
	BOOST_TEST (Formula::from_string ("(==s:1.0)")->to_string() == "(==s:1.0)");
	BOOST_TEST (Formula::from_string ("(!=b:1.0)")->to_string() == "(!=b:1.0)");
	BOOST_TEST (Formula::from_string ("(>=b:1.0)")->to_string() == "(>=b:1.0)");
	BOOST_TEST (Formula::from_string ("(<=b:1.0)")->to_string() == "(<=b:1.0)");
	BOOST_TEST (Formula::from_string ("(>b:1.0)")->to_string() == "(>b:1.0)");
	BOOST_TEST (Formula::from_string ("(<b:1.0)")->to_string() == "(<b:1.0)");

	/* And and or */
	BOOST_TEST (Formula::from_string ("(&(>=b:1)())")->to_string() == "(&(>=b:1)())");
	BOOST_TEST (Formula::from_string ("(|(>=b:1)())")->to_string() == "(|(>=b:1)())");
	BOOST_TEST (Formula::from_string ("(&()(>=b:1))")->to_string() == "(&()(>=b:1))");
	BOOST_TEST (Formula::from_string ("(&()())")->to_string() == "(&()())");
	BOOST_TEST (Formula::from_string ("(&(>=b:1)(>=s:1))")->to_string() == "(&(>=b:1)(>=s:1))");
	BOOST_TEST (Formula::from_string ("(&(>=b:1)(&(>=s:1)(!=s:2)))")->to_string() == "(&(>=b:1)(&(>=s:1)(!=s:2)))");
}
