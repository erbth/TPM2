#define BOOST_TEST_MODULE test_version_number

#include <boost/test/included/unit_test.hpp>
#include "version_number.h"

using namespace std;


BOOST_AUTO_TEST_CASE (test_different_constructors)
{
	/* Different version number strings */
	BOOST_TEST (VersionNumber("1").to_string() == "1");
	BOOST_TEST (VersionNumber("1.0").to_string() == "1.0");
	BOOST_TEST (VersionNumber("1.0ad").to_string() == "1.0.ad");
	BOOST_TEST (VersionNumber("1.0.ad").to_string() == "1.0.ad");
	BOOST_TEST (VersionNumber("1.0.a.d").to_string() == "1.0.ad");
	BOOST_TEST (VersionNumber("a").to_string() == "a");
	BOOST_TEST (VersionNumber("1.a").to_string() == "1.a");
	BOOST_TEST (VersionNumber("1a").to_string() == "1.a");
	BOOST_TEST (VersionNumber("1a2").to_string() == "1.a.2");
	BOOST_TEST (VersionNumber("a2").to_string() == "a.2");

	/* Invalid version number strings */
	BOOST_CHECK_THROW (VersionNumber(""), InvalidVersionNumberString);
	BOOST_CHECK_THROW (VersionNumber("."), InvalidVersionNumberString);
	BOOST_CHECK_THROW (VersionNumber("1."), InvalidVersionNumberString);
	BOOST_CHECK_THROW (VersionNumber(".a"), InvalidVersionNumberString);
	BOOST_CHECK_THROW (VersionNumber("1..a"), InvalidVersionNumberString);

	/* Copy constructor */
	VersionNumber v("1.0");
	VersionNumber v2(v);

	BOOST_TEST (v2.to_string() == "1.0");
}


BOOST_AUTO_TEST_CASE (test_comparisons)
{
	/* Equality */
	BOOST_TEST (VersionNumber("0") == VersionNumber("0"));
	BOOST_TEST (VersionNumber("1.0") == VersionNumber("1.0"));
	BOOST_TEST (!(VersionNumber("1.0") == VersionNumber("1")));
	BOOST_TEST (VersionNumber("1.0a") == VersionNumber("1.0a"));
	BOOST_TEST (VersionNumber("1.0.a") == VersionNumber("1.0a"));

	/* Inequality */
	BOOST_TEST (!(VersionNumber("0") != VersionNumber("0")));
	BOOST_TEST (!(VersionNumber("1.0") != VersionNumber("1.0")));
	BOOST_TEST (VersionNumber("1.0") != VersionNumber("1"));
	BOOST_TEST (!(VersionNumber("1.0a") != VersionNumber("1.0a")));
	BOOST_TEST (!(VersionNumber("1.0.a") != VersionNumber("1.0a")));

	/* <= */
	BOOST_TEST (!(VersionNumber("1.0") <= VersionNumber("1")));
	BOOST_TEST (VersionNumber("1.0") <= VersionNumber("1.0"));
	BOOST_TEST (VersionNumber("1.0") <= VersionNumber("1.1"));
	BOOST_TEST (VersionNumber("1.0") <= VersionNumber("2"));

	BOOST_TEST (!(VersionNumber("1.0a") <= VersionNumber("1.0")));
	BOOST_TEST (VersionNumber("1.0a") <= VersionNumber("1.0a"));
	BOOST_TEST (VersionNumber("1.0a") <= VersionNumber("1.0b"));
	BOOST_TEST (VersionNumber("1.0a") <= VersionNumber("1.0ad"));
	BOOST_TEST (VersionNumber("1.0ad") <= VersionNumber("1.0da"));
	BOOST_TEST (VersionNumber("1.0a") <= VersionNumber("1.1"));

	/* >= */
	BOOST_TEST (!(VersionNumber("1") >= VersionNumber("1.0")));
	BOOST_TEST (VersionNumber("1.0") >= VersionNumber("1"));
	BOOST_TEST (VersionNumber("1.0") >= VersionNumber("1.0"));
	BOOST_TEST (!(VersionNumber("1.0") >= VersionNumber("1.1")));
	BOOST_TEST (!(VersionNumber("1.0") >= VersionNumber("2")));

	BOOST_TEST (VersionNumber("1.0a") >= VersionNumber("1.0"));
	BOOST_TEST (VersionNumber("1.0a") >= VersionNumber("1.0a"));
	BOOST_TEST (!(VersionNumber("1.0a") >= VersionNumber("1.0b")));
	BOOST_TEST (!(VersionNumber("1.0a") >= VersionNumber("1.0ad")));
	BOOST_TEST (!(VersionNumber("1.0ad") >= VersionNumber("1.0da")));
	BOOST_TEST (!(VersionNumber("1.0a") >= VersionNumber("1.1")));

	/* < */
	BOOST_TEST (!(VersionNumber("1.0") < VersionNumber("1")));
	BOOST_TEST (!(VersionNumber("1.0") < VersionNumber("1.0")));
	BOOST_TEST (VersionNumber("1.0") < VersionNumber("1.1"));
	BOOST_TEST (VersionNumber("1.0") < VersionNumber("2"));

	BOOST_TEST (!(VersionNumber("1.0a") < VersionNumber("1.0")));
	BOOST_TEST (!(VersionNumber("1.0a") < VersionNumber("1.0a")));
	BOOST_TEST (VersionNumber("1.0a") < VersionNumber("1.0b"));
	BOOST_TEST (VersionNumber("1.0a") < VersionNumber("1.0ad"));
	BOOST_TEST (VersionNumber("1.0ad") < VersionNumber("1.0da"));
	BOOST_TEST (VersionNumber("1.0a") < VersionNumber("1.1"));

	/* > */
	BOOST_TEST (!(VersionNumber("1") > VersionNumber("1.0")));
	BOOST_TEST (VersionNumber("1.0") > VersionNumber("1"));
	BOOST_TEST (!(VersionNumber("1.0") > VersionNumber("1.0")));
	BOOST_TEST (!(VersionNumber("1.0") > VersionNumber("1.1")));
	BOOST_TEST (!(VersionNumber("1.0") > VersionNumber("2")));

	BOOST_TEST (VersionNumber("1.0a") > VersionNumber("1.0"));
	BOOST_TEST (!(VersionNumber("1.0a") > VersionNumber("1.0a")));
	BOOST_TEST (!(VersionNumber("1.0a") > VersionNumber("1.0b")));
	BOOST_TEST (!(VersionNumber("1.0a") > VersionNumber("1.0ad")));
	BOOST_TEST (!(VersionNumber("1.0ad") > VersionNumber("1.0da")));
	BOOST_TEST (VersionNumber("1.0da") > VersionNumber("1.0ad"));
	BOOST_TEST (!(VersionNumber("1.0a") > VersionNumber("1.1")));
}
