#define BOOST_TEST_MODULE test_depres_common

#include <boost/test/included/unit_test.hpp>
#include "depres2.h"

using namespace std;
using namespace depres;
namespace pc = PackageConstraints;

class Depres2SolverTestAdaptor : public Depres2Solver
{
public:
	void push_error(string e)
	{
		errors.push_back(e);
	}

	void add_simple_test_node(string name, int arch)
	{
		G.insert(make_pair(make_pair(name, arch), IGNode(*this, make_pair(name, arch), true, false)));
	}

	installation_graph_t &access_G()
	{
		return G;
	}
};


class TestpkgVersion : public PackageVersion
{
protected:
	vector<string> files, directories;

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
		> get_dependencies() override
	{
		std::vector<
				std::pair<std::pair<std::string, int>, std::shared_ptr<PackageConstraints::Formula>>
			> deps;

		deps.push_back(make_pair(make_pair(name, 0), make_shared<pc::And>(nullptr, nullptr)));
		return deps;
	}

	std::vector<
			std::pair<std::pair<std::string, int>, std::shared_ptr<PackageConstraints::Formula>>
		> get_pre_dependencies() override
	{
		std::vector<
				std::pair<std::pair<std::string, int>, std::shared_ptr<PackageConstraints::Formula>>
			> deps;

		deps.push_back(make_pair(make_pair(name, 0), make_shared<pc::And>(nullptr, nullptr)));
		return deps;
	}

	const vector<string> &get_files()
	{
		return files;
	}

	const vector<string> &get_directories()
	{
		return directories;
	}
};

BOOST_AUTO_TEST_CASE( test_ig_node_set_dependencies_set )
{
	Depres2SolverTestAdaptor s;
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
	Depres2SolverTestAdaptor s;
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
	Depres2SolverTestAdaptor s;
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
