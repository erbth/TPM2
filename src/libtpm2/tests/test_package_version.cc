#define BOOST_TEST_MODULE test_package_version

#include <boost/test/included/unit_test.hpp>
#include "package_version.h"

using namespace std;
namespace pc = PackageConstraints;

class TestPackageVersion : public PackageVersion
{
protected:
	vector<string> files, directories;

public:
	TestPackageVersion(const string &n, const int arch,
			const VersionNumber &sv, const VersionNumber &bv)
		: PackageVersion(n, arch, sv, bv)
	{
	}

	bool is_installed() const override
	{
		return false;
	}

	vector<pair<pair<string, int>, shared_ptr<pc::Formula>>> get_dependencies() override
	{
		vector<pair<pair<string, int>, shared_ptr<pc::Formula>>> v;
		v.push_back(make_pair(make_pair("test", 1), nullptr));
		return v;
	}

	vector<pair<pair<string, int>, shared_ptr<pc::Formula>>> get_pre_dependencies() override
	{
		vector<pair<pair<string, int>, shared_ptr<pc::Formula>>> v;
		v.push_back(make_pair(make_pair("test2", 1), nullptr));
		return v;
	}

	const vector<string> &get_files() override
	{
		return files;
	}

	const vector<string> &get_directories() override
	{
		return directories;
	}
};

ostream &operator<<(ostream &o, const TestPackageVersion &v)
{
	o << "TestPackageVersion(" << v.get_name() << ", " << v.get_architecture() <<
		", " << v.get_binary_version().to_string() << ")";

	return o;
}

typedef pair<string, int> pairtype;
BOOST_TEST_DONT_PRINT_LOG_VALUE( pairtype );

typedef pair<const char*, int> pairtype2;
BOOST_TEST_DONT_PRINT_LOG_VALUE( pairtype2 );


BOOST_AUTO_TEST_CASE( test_basic )
{
	TestPackageVersion v("test", 2, VersionNumber("1.1"), VersionNumber("1.0"));

	BOOST_TEST (v.is_installed() == false);
	BOOST_TEST (v.get_name() == "test");
	BOOST_TEST (v.get_architecture() == 2);
	BOOST_TEST (v.get_identifier() == make_pair(string("test"), 2));
	BOOST_TEST (v.get_source_version() == VersionNumber("1.1"));
	BOOST_TEST (v.get_binary_version() == VersionNumber("1.0"));

	auto deps = v.get_dependencies();
	BOOST_TEST (deps.size() == 1);

	auto dep = deps[0];
	BOOST_TEST (dep.first == make_pair(string("test"), 1));
	BOOST_TEST (dep.second == nullptr);

	auto pre_deps = v.get_pre_dependencies();
	BOOST_TEST (pre_deps.size() == 1);

	auto pre_dep = pre_deps[0];
	BOOST_TEST (pre_dep.first == make_pair(string("test2"), 1));
	BOOST_TEST (pre_dep.second == nullptr);
}

BOOST_AUTO_TEST_CASE( test_comparators )
{
	/* Difference in name */
	TestPackageVersion n1("p", 1, VersionNumber("1.1"), VersionNumber("1.0"));
	TestPackageVersion n2("p", 1, VersionNumber("1.1"), VersionNumber("1.0"));
	TestPackageVersion n3("q", 1, VersionNumber("1.1"), VersionNumber("1.0"));

	BOOST_TEST (n1 == n1);
	BOOST_TEST (n1 == n2);
	BOOST_TEST (!(n1 == n3));

	BOOST_TEST (!(n1 != n1));
	BOOST_TEST (!(n1 != n2));
	BOOST_TEST (n1 != n3);

	BOOST_TEST (!(n1 < n1));
	BOOST_TEST (!(n1 < n2));
	BOOST_TEST (n1 < n3);

	BOOST_TEST (!(n1 > n1));
	BOOST_TEST (!(n1 > n2));
	BOOST_TEST (n3 > n1);

	BOOST_TEST (n1 <= n1);
	BOOST_TEST (n1 <= n2);
	BOOST_TEST (n2 <= n1);
	BOOST_TEST (n1 <= n3);
	BOOST_TEST (!(n3 <= n1));

	BOOST_TEST (n1 >= n1);
	BOOST_TEST (n2 >= n1);
	BOOST_TEST (n1 >= n2);
	BOOST_TEST (n3 >= n1);
	BOOST_TEST (!(n1 >= n3));

	/* Difference in architecture */
	TestPackageVersion a1("p", 1, VersionNumber("1.1"), VersionNumber("1.0"));
	TestPackageVersion a2("p", 1, VersionNumber("1.1"), VersionNumber("1.0"));
	TestPackageVersion a3("p", 2, VersionNumber("1.1"), VersionNumber("1.0"));

	BOOST_TEST (a1 == a1);
	BOOST_TEST (a1 == a2);
	BOOST_TEST (!(a1 == a3));

	BOOST_TEST (!(a1 != a1));
	BOOST_TEST (!(a1 != a2));
	BOOST_TEST (a1 != a3);

	BOOST_TEST (!(a1 < a1));
	BOOST_TEST (!(a1 < a2));
	BOOST_TEST (a1 < a3);

	BOOST_TEST (!(a1 > a1));
	BOOST_TEST (!(a1 > a2));
	BOOST_TEST (a3 > a1);

	BOOST_TEST (a1 <= a1);
	BOOST_TEST (a1 <= a2);
	BOOST_TEST (a2 <= a1);
	BOOST_TEST (a1 <= a3);
	BOOST_TEST (!(a3 <= a1));

	BOOST_TEST (a1 >= a1);
	BOOST_TEST (a2 >= a1);
	BOOST_TEST (a1 >= a2);
	BOOST_TEST (a3 >= a1);
	BOOST_TEST (!(a1 >= a3));

	/* Difference in version number */
	TestPackageVersion v1("p", 1, VersionNumber("1.1"), VersionNumber("1.0"));
	TestPackageVersion v2("p", 1, VersionNumber("1.1"), VersionNumber("1.0"));
	TestPackageVersion v3("p", 1, VersionNumber("1.1"), VersionNumber("1.1"));

	BOOST_TEST (v1 == v1);
	BOOST_TEST (v1 == v2);
	BOOST_TEST (!(v1 == v3));

	BOOST_TEST (!(v1 != v1));
	BOOST_TEST (!(v1 != v2));
	BOOST_TEST (v1 != v3);

	BOOST_TEST (!(v1 < v1));
	BOOST_TEST (!(v1 < v2));
	BOOST_TEST (v1 < v3);

	BOOST_TEST (!(v1 > v1));
	BOOST_TEST (!(v1 > v2));
	BOOST_TEST (v3 > v1);

	BOOST_TEST (v1 <= v1);
	BOOST_TEST (v1 <= v2);
	BOOST_TEST (v2 <= v1);
	BOOST_TEST (v1 <= v3);
	BOOST_TEST (!(v3 <= v1));

	BOOST_TEST (v1 >= v1);
	BOOST_TEST (v2 >= v1);
	BOOST_TEST (v1 >= v2);
	BOOST_TEST (v3 >= v1);
	BOOST_TEST (!(v1 >= v3));
}
