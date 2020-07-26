/** Tests for the most basic functions of depres v2. */

#define BOOST_TEST_MODULE test_depres2_basics

#include <boost/test/included/unit_test.hpp>
#include <depres2.h>

using namespace std;
using namespace depres2;
namespace pc = PackageConstraints;


class SolverTestAdaptor : public Solver
{
public:
	SolverTestAdaptor(
			vector<shared_ptr<PackageVersion>> installed_packages,
			vector<selected_package_t> selected_packages,
			list_package_versions_t list_package_versions,
			get_package_version_t get_package_version)
		:
			Solver(installed_packages, selected_packages, list_package_versions, get_package_version)
	{
	}

	SolverTestAdaptor()
		: Solver(vector<shared_ptr<PackageVersion>>(),
				vector<selected_package_t>(),
				nullptr, nullptr)
	{
	}

	void push_error(string e)
	{
		errors.push_back(e);
	}

	void add_simple_test_node(string name, int arch)
	{
		G.insert(make_pair(make_pair(name, arch), IGNode(*this, make_pair(name, arch), true)));
	}

	installation_graph_t &access_G()
	{
		return G;
	}
};


/* Tests for functions of the `IGNode` class */
class TestpkgVersion : public PackageVersion
{
public:
	TestpkgVersion(string name, const VersionNumber& sv, const VersionNumber& bv)
		: PackageVersion(name, 0, sv, bv)
	{
	}

	bool is_installed() const override
	{
		return false;
	}

	std::vector<
			std::pair<std::pair<std::string, int>, std::shared_ptr<PackageConstraints::Formula>>
		> get_dependencies() const override
	{
		std::vector<
				std::pair<std::pair<std::string, int>, std::shared_ptr<PackageConstraints::Formula>>
			> deps;

		deps.push_back(make_pair(make_pair(name, 0), make_shared<pc::And>(nullptr, nullptr)));
		return deps;
	}
};

BOOST_AUTO_TEST_CASE( test_ig_node_set_dependencies_set )
{
	SolverTestAdaptor s;
	s.add_simple_test_node("testpkg", 0);
	
	s.access_G().find(make_pair("testpkg", 0))->second.chosen_version =
		make_shared<TestpkgVersion>("testpkg2", VersionNumber("1.0"), VersionNumber("1.1"));

	s.access_G().find(make_pair("testpkg", 0))->second.set_dependencies();

	BOOST_TEST(s.access_G().size() == 2);
	auto &v = s.access_G().find(make_pair("testpkg", 0))->second;
	auto &w = s.access_G().find(make_pair("testpkg2", 0))->second;

	BOOST_TEST(v.dependencies.size() == 1);
	BOOST_TEST(w.reverse_dependencies.size() == 1);

	BOOST_TEST(v.dependencies[0] == &w);
	BOOST_TEST(*w.reverse_dependencies.begin() == &v);
}

BOOST_AUTO_TEST_CASE( test_ig_node_set_dependencies_modify )
{
	SolverTestAdaptor s;
	s.add_simple_test_node("testpkg", 0);
	
	s.access_G().find(make_pair("testpkg", 0))->second.chosen_version =
		make_shared<TestpkgVersion>("testpkg2", VersionNumber("1.0"), VersionNumber("1.1"));

	s.access_G().find(make_pair("testpkg", 0))->second.set_dependencies();

	s.access_G().find(make_pair("testpkg", 0))->second.chosen_version =
		make_shared<TestpkgVersion>("testpkg3", VersionNumber("1.0"), VersionNumber("1.1"));

	s.access_G().find(make_pair("testpkg", 0))->second.set_dependencies();

	BOOST_TEST(s.access_G().size() == 3);
	auto &v = s.access_G().find(make_pair("testpkg", 0))->second;
	auto &w = s.access_G().find(make_pair("testpkg2", 0))->second;
	auto &u = s.access_G().find(make_pair("testpkg3", 0))->second;

	BOOST_TEST(v.dependencies.size() == 1);
	BOOST_TEST(w.reverse_dependencies.size() == 0);
	BOOST_TEST(u.reverse_dependencies.size() == 1);

	BOOST_TEST(v.dependencies[0] == &u);
	BOOST_TEST(*u.reverse_dependencies.begin() == &v);
}

BOOST_AUTO_TEST_CASE( test_ig_node_set_dependencies_clear )
{
	SolverTestAdaptor s;
	s.add_simple_test_node("testpkg", 0);
	
	s.access_G().find(make_pair("testpkg", 0))->second.chosen_version =
		make_shared<TestpkgVersion>("testpkg2", VersionNumber("1.0"), VersionNumber("1.1"));

	s.access_G().find(make_pair("testpkg", 0))->second.set_dependencies();

	s.access_G().find(make_pair("testpkg", 0))->second.chosen_version = nullptr;
	s.access_G().find(make_pair("testpkg", 0))->second.set_dependencies();

	BOOST_TEST(s.access_G().size() == 2);
	auto &v = s.access_G().find(make_pair("testpkg", 0))->second;
	auto &w = s.access_G().find(make_pair("testpkg2", 0))->second;

	BOOST_TEST(v.dependencies.size() == 0);
	BOOST_TEST(w.reverse_dependencies.size() == 0);
}


/* Tests for functions of the `Solver` class */
BOOST_AUTO_TEST_CASE( test_solver_retrieve_errors )
{
	SolverTestAdaptor s;

	BOOST_TEST( s.retrieve_errors().size() == 0 );

	s.push_error("test1");
	BOOST_TEST( s.retrieve_errors().size() == 1);

	s.push_error("test2");
	BOOST_TEST( s.retrieve_errors().size() == 2);
	BOOST_TEST( s.retrieve_errors()[0] == string("test1") );
	BOOST_TEST( s.retrieve_errors()[1] == string("test2") );
}

BOOST_AUTO_TEST_CASE( test_solver_get_G )
{
	SolverTestAdaptor s;

	BOOST_TEST( s.get_G().size() == 0);

	s.add_simple_test_node("test", 1);
	auto G = s.get_G();
	BOOST_TEST( G.size() == 1);
	BOOST_TEST( (bool) (G.find(make_pair("test", 1)) != G.end()) );
	BOOST_TEST( (bool) (G.find(make_pair("test", 1))->second.is_selected == true) );

	BOOST_TEST( s.get_G().size() == 0);
}
