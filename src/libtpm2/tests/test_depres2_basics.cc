/** Tests for the most basic functions of depres v2. */

#define BOOST_TEST_MODULE test_depres2_basics

#include <boost/test/included/unit_test.hpp>
#include "depres2.h"

using namespace std;
using namespace depres;


class Depres2SolverTestAdaptor : public Depres2Solver
{
public:
	void push_error(string e)
	{
		errors.push_back(e);
	}

	void add_simple_test_node(string name, int arch)
	{
		G.insert(make_pair(make_pair(name, arch), make_shared<Depres2IGNode>(*this, make_pair(name, arch), true, false)));
	}

	installation_graph_t &access_G()
	{
		return G;
	}
};


BOOST_AUTO_TEST_CASE( test_retrieve_errors )
{
	Depres2SolverTestAdaptor s;

	BOOST_TEST( s.get_errors().size() == 0 );

	s.push_error("test1");
	BOOST_TEST( s.get_errors().size() == 1);

	s.push_error("test2");
	BOOST_TEST( s.get_errors().size() == 2);
	BOOST_TEST( s.get_errors()[0] == string("test1") );
	BOOST_TEST( s.get_errors()[1] == string("test2") );
}

BOOST_AUTO_TEST_CASE( test_get_G )
{
	Depres2SolverTestAdaptor s;

	BOOST_TEST( s.get_G().size() == 0);

	s.add_simple_test_node("test", 1);
	auto G = s.get_G();
	BOOST_TEST( G.size() == 1);
	BOOST_TEST( (bool) (G.find(make_pair("test", 1)) != G.end()) );
	BOOST_TEST( (bool) (G.find(make_pair("test", 1))->second->is_selected == true) );

	BOOST_TEST( s.get_G().size() == 0);
}
