#define BOOST_TEST_MODULE test_file_trie

#include <boost/test/included/unit_test.hpp>
#include "file_trie.h"

using namespace std;


BOOST_AUTO_TEST_CASE (test_all)
{
	FileTrie<int> t;

	BOOST_TEST (!(bool) t.find_file ("/test"));
	t.insert_file ("/test", 0);
	BOOST_TEST ((bool) t.find_file ("/test"));

	t.insert_file ("/test/test2", 0);
	t.insert_file ("/test2/test3", 0);
	t.insert_file ("/test2/test4", 0);

	t.insert_directory ("/test2/test5", 0);

	t.insert_file ("/testdir/test6", 0);
	BOOST_TEST (!(bool) t.find_directory ("/testdir"));

	t.insert_directory ("/testdir", 0);
	BOOST_TEST ((bool) t.find_directory ("/testdir"));

	t.insert_file ("/testdir/testdir2/testdir3/testdir4/test7", 0);

	BOOST_TEST ((bool) t.find_file ("/test"));
	BOOST_TEST (!(bool) t.find_file ("/test/test2"));
	BOOST_TEST ((bool) t.find_file ("/test2/test3"));
	BOOST_TEST (!(bool) t.find_directory ("/test2"));
	BOOST_TEST ((bool) t.find_directory ("/testdir"));
	BOOST_TEST ((bool) t.find_file ("/test2/test4"));
	BOOST_TEST (!(bool) t.find_file ("/testdir"));
	BOOST_TEST ((bool) t.find_directory ("/test2/test5"));

	BOOST_TEST (!(bool) t.find_file ("/test2/test5"));
	t.insert_file ("/test2/test5", 0);
	BOOST_TEST (!(bool) t.find_file ("/test2/test5"));

	BOOST_TEST ((bool) t.find_directory ("/test2/test5"));
	BOOST_TEST ((bool) t.find_file ("/testdir/test6"));

	BOOST_TEST ((bool) t.find_file ("/testdir/testdir2/testdir3/testdir4/test7"));
	BOOST_TEST (!(bool) t.find_file ("/testdir/testdir2/testdir3/testdir4/test7/"));
	BOOST_TEST (!(bool) t.find_file ("/testdir/testdir2/testdir3/testdir4/test8"));
	BOOST_TEST (!(bool) t.find_file ("/testdir/testdir2/testdir3/test8"));
	BOOST_TEST (!(bool) t.find_directory ("/testdir/testdir2/testdir3/test8"));
	BOOST_TEST (!(bool) t.find_directory ("/testdir/testdir2/testdir3"));


	/* Get path */
	BOOST_TEST (t.find_file ("/test")->get_path() == "/test");
	BOOST_TEST ((*t.find_file ("/testdir/test6")).get_path() == "/testdir/test6");
	BOOST_TEST (t.find_directory ("/testdir")->get_path() == "/testdir");
	BOOST_TEST (t.find_directory ("/test2/test5")->get_path() == "/test2/test5");
	BOOST_TEST (t.find_directory ("/test2/test5/")->get_path() == "/test2/test5");


	/* Whitebox testing */
	/* / */
	auto& root_nodes = FileTrieTestAdaptor<int>::get_children (t);

	BOOST_TEST (root_nodes.size () == 3);

	auto ir1 = root_nodes.find ("test");
	auto ir2 = root_nodes.find ("test2");
	auto ir3 = root_nodes.find ("testdir");

	BOOST_TEST ((bool) (ir1 != root_nodes.end()));
	BOOST_TEST ((bool) (ir2 != root_nodes.end()));
	BOOST_TEST ((bool) (ir3 != root_nodes.end()));

	auto& r1 = ir1->second;
	auto& r2 = ir2->second;
	auto& r3 = ir3->second;

	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (r1) == "test");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (r2) == "test2");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (r3) == "testdir");

	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (r1) == nullptr);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (r2) == nullptr);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (r3) == nullptr);

	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (r1) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (r2) == false);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (r3) == false);


	/* /test */
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children(r1).empty());


	/* /test2/ */
	auto& test2_nodes = FileTrieTestAdaptor<int>::get_children (r2);

	BOOST_TEST (test2_nodes.size() == 3);

	auto itest21 = test2_nodes.find ("test3");
	auto itest22 = test2_nodes.find ("test4");
	auto itest23 = test2_nodes.find ("test5");

	BOOST_TEST ((bool) (itest21 != test2_nodes.end()));
	BOOST_TEST ((bool) (itest22 != test2_nodes.end()));
	BOOST_TEST ((bool) (itest23 != test2_nodes.end()));

	auto& test21 = itest21->second;
	auto& test22 = itest22->second;
	auto& test23 = itest23->second;

	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (test21) == "test3");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (test22) == "test4");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (test23) == "test5");

	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (test21) == &r2);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (test22) == &r2);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (test23) == &r2);

	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (test21) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (test22) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (test23) == false);

	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (test21).empty());
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (test22).empty());


	/* /test2/test5 */
	auto& test5_nodes = FileTrieTestAdaptor<int>::get_children (test23);

	BOOST_TEST (test5_nodes.size() == 1);

	auto itest51 = test5_nodes.find ("");

	BOOST_TEST ((bool) (itest51 != test5_nodes.end()));

	auto& test51 = itest51->second;

	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (test51) == "");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (test51) == &test23);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (test51) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (test51).empty());

	BOOST_TEST (test51.get_path() == "/test2/test5");

	/* Todo: continue with checking /testdir, then removal, and finally the
	 * handles with their (in)equality. */
}
