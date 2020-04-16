#define BOOST_TEST_MODULE test_file_trie

#include <boost/test/included/unit_test.hpp>
#include "file_trie.h"

using namespace std;


/* Testing policy
 * ==============
 *
 * Try to do whitebox testing as it's easier when not all needs to be tested
 * because the used operations in one method are known while ensuring that an
 * object's state, its members, have the right values. But be sure to test not
 * only what the code does but especially what it should do. Otherwise this
 * serves only regression testing purpose, but I clearly want to have the tests
 * ensure me using this class is safe to a large degree.
 *
 * The general definition of whitebox testing is testing with knowing about the
 * code, which allows to use certain properties of it, whereas blackbox testing
 * does not allow to use the source code's properties. But both have to define
 * test cases that are not purely generated from the code as functionality must
 * be defined before writing the code.
 * In this regard, this is whitebox testing. */


BOOST_AUTO_TEST_CASE (test_basic_trie_functionality)
{
	FileTrie<int> t;

	BOOST_TEST (!(bool) t.find_file ("/test"));
	t.insert_file ("/test");
	BOOST_TEST ((bool) t.find_file ("/test"));

	t.insert_file ("/test/test2");
	t.insert_file ("/test2/test3");
	t.insert_file ("/test2/test4");

	t.insert_directory ("/test2/test5");

	t.insert_file ("/testdir/test6");
	BOOST_TEST (!(bool) t.find_directory ("/testdir"));

	t.insert_directory ("/testdir");
	BOOST_TEST ((bool) t.find_directory ("/testdir"));

	t.insert_file ("/testdir/testdir2/testdir3/testdir4/test7");

	BOOST_TEST ((bool) t.find_file ("/test"));
	BOOST_TEST (!(bool) t.find_file ("/test/test2"));
	BOOST_TEST ((bool) t.find_file ("/test2/test3"));
	BOOST_TEST (!(bool) t.find_directory ("/test2"));
	BOOST_TEST ((bool) t.find_directory ("/testdir"));
	BOOST_TEST ((bool) t.find_file ("/test2/test4"));
	BOOST_TEST (!(bool) t.find_file ("/testdir"));
	BOOST_TEST ((bool) t.find_directory ("/test2/test5"));

	BOOST_TEST (!(bool) t.find_file ("/test2/test5"));
	t.insert_file ("/test2/test5");
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
	auto& root_children = FileTrieTestAdaptor<int>::get_children (t);

	BOOST_TEST (root_children.size () == 3);

	auto ir1 = root_children.find ("test");
	auto ir2 = root_children.find ("test2");
	auto ir3 = root_children.find ("testdir");

	BOOST_TEST ((bool) (ir1 != root_children.end()));
	BOOST_TEST ((bool) (ir2 != root_children.end()));
	BOOST_TEST ((bool) (ir3 != root_children.end()));

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

	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (r1) == "test");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (r2) == "test2");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (r3) == "testdir");


	/* /test */
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children(r1).empty());


	/* /test2/ */
	auto& test2_children = FileTrieTestAdaptor<int>::get_children (r2);

	BOOST_TEST (test2_children.size() == 3);

	auto itest21 = test2_children.find ("test3");
	auto itest22 = test2_children.find ("test4");
	auto itest23 = test2_children.find ("test5");

	BOOST_TEST ((bool) (itest21 != test2_children.end()));
	BOOST_TEST ((bool) (itest22 != test2_children.end()));
	BOOST_TEST ((bool) (itest23 != test2_children.end()));

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
	auto& test5_children = FileTrieTestAdaptor<int>::get_children (test23);

	BOOST_TEST (test5_children.size() == 1);

	auto itest51 = test5_children.find ("");

	BOOST_TEST ((bool) (itest51 != test5_children.end()));

	auto& test51 = itest51->second;

	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (test51) == "");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (test51) == &test23);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (test51) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (test51).empty());

	BOOST_TEST (test51.get_path() == "/test2/test5");

	/* /testdir */
	auto& testdir_children = FileTrieTestAdaptor<int>::get_children (r3);

	BOOST_TEST (testdir_children.size() == 3);

	auto itestdir1 = testdir_children.find ("");
	auto itestdir2 = testdir_children.find ("test6");
	auto itestdir3 = testdir_children.find ("testdir2");

	BOOST_TEST ((bool) (itestdir1 != testdir_children.end()));
	BOOST_TEST ((bool) (itestdir2 != testdir_children.end()));
	BOOST_TEST ((bool) (itestdir3 != testdir_children.end()));

	auto& testdir1 = itestdir1->second;
	auto& testdir2 = itestdir2->second;
	auto& testdir3 = itestdir3->second;

	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (testdir1) == "");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (testdir1) == &r3);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (testdir1) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (testdir1).empty());

	BOOST_TEST (testdir1.get_path() == "/testdir");


	/* /testdir/test6 */
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (testdir2) == "test6");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (testdir2) == &r3);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (testdir2) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (testdir2).empty());

	BOOST_TEST (testdir2.get_path() == "/testdir/test6");


	/* /testdir/testdir2 */
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (testdir3) == "testdir2");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (testdir3) == &r3);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (testdir3) == false);
	BOOST_TEST (testdir3.get_path() == "/testdir/testdir2");

	auto& testdir2_children = FileTrieTestAdaptor<int>::get_children (testdir3);

	BOOST_TEST (testdir2_children.size() == 1);

	auto itestdir21 = testdir2_children.find ("testdir3");

	BOOST_TEST ((bool) (itestdir21 != testdir2_children.end()));

	auto &testdir21 = itestdir21->second;

	/* /testdir/testdir2/testdir3 */
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (testdir21) == "testdir3");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (testdir21) == &testdir3);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (testdir21) == false);
	BOOST_TEST (testdir21.get_path() == "/testdir/testdir2/testdir3");

	auto& testdir3_children = FileTrieTestAdaptor<int>::get_children (testdir21);

	BOOST_TEST (testdir3_children.size() == 1);

	auto itestdir31 = testdir3_children.find ("testdir4");
	BOOST_TEST ((bool) (itestdir31 != testdir3_children.end()));

	auto& testdir31 = itestdir31->second;


	/* /testdir/testdir2/testdir3/testdir4 */
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (testdir31) == "testdir4");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (testdir31) == &testdir21);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (testdir31) == false);
	BOOST_TEST (testdir31.get_path() == "/testdir/testdir2/testdir3/testdir4");

	auto& testdir4_children = FileTrieTestAdaptor<int>::get_children (testdir31);

	BOOST_TEST (testdir4_children.size() == 1);

	auto itestdir41 = testdir4_children.find ("test7");
	BOOST_TEST ((bool) (itestdir41 != testdir4_children.end()));

	auto& testdir41 = itestdir41->second;


	/* /testdir/testdir2/testdir3/testdir4/test7 */
	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (testdir41) == "test7");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (testdir41) == &testdir31);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (testdir41) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (testdir41).empty());
	BOOST_TEST (testdir41.get_path() == "/testdir/testdir2/testdir3/testdir4/test7");


	/* Remove nodes. Be aware that this creates dangling references. */
	BOOST_TEST (t.remove_element ("/testdir/testdir2/testdir3/testdir4/test7"));

	BOOST_TEST (testdir_children.size() == 2);
	BOOST_TEST ((bool) t.find_file ("/testdir/test6"));
	BOOST_TEST ((bool) t.find_directory ("/testdir"));

	/* Removing something non-existent must not change the trie. */
	BOOST_TEST (!t.remove_element ("/testdir/ "));
	BOOST_TEST (!t.remove_element ("/meow/waff"));

	BOOST_TEST (testdir_children.size() == 2);
	BOOST_TEST (root_children.size() == 3);
	BOOST_TEST (test2_children.size() == 3);
	BOOST_TEST (test5_children.size() == 1);

	BOOST_TEST ((bool) t.find_file ("/testdir/test6"));
	BOOST_TEST ((bool) t.find_directory ("/testdir"));
	BOOST_TEST ((bool) t.find_file ("/test2/test3"));
	BOOST_TEST ((bool) t.find_file ("/test2/test4"));
	BOOST_TEST ((bool) t.find_directory ("/test2/test5"));
	BOOST_TEST ((bool) t.find_file ("/test"));


	/* Finish by ensuring removing all remaining elements works correctly */
	BOOST_TEST (t.remove_element ("/testdir"));
	BOOST_TEST (testdir_children.size() == 1);

	BOOST_TEST (t.remove_element ("/testdir/test6"));
	BOOST_TEST (root_children.size() == 2);

	BOOST_TEST (t.remove_element ("/test"));
	BOOST_TEST (root_children.size() == 1);

	BOOST_TEST (t.remove_element ("/test2/test3"));
	BOOST_TEST (test2_children.size() == 2);

	BOOST_TEST (t.remove_element ("/test2/test4"));
	BOOST_TEST (test2_children.size() == 1);

	BOOST_TEST (t.remove_element ("/test2/test5"));
	BOOST_TEST (root_children.empty());
}


BOOST_AUTO_TEST_CASE (test_root_contained)
{
	FileTrie<int> t;

	t.insert_file ("/");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (t).empty());

	t.insert_directory ("/");

	auto& root_children = FileTrieTestAdaptor<int>::get_children (t);

	BOOST_TEST (root_children.size() == 1);

	auto ir1 = root_children.find ("");
	BOOST_TEST ((bool) (ir1 != root_children.end()));

	auto &r1 = ir1->second;

	BOOST_TEST (FileTrieTestAdaptor<int>::get_name (r1) == "");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_parent (r1) == nullptr);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_is_leaf (r1) == true);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_children (r1).empty ());

	BOOST_TEST (r1.get_path() == "/");


	/* Test if we can find it */
	BOOST_TEST ((bool) t.find_file ("/") == false);

	auto h = t.find_directory ("/");
	BOOST_TEST ((bool) h == true);

	BOOST_TEST (h->get_path() == "/");
	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (h) == &r1);


	/* Test if we can remove it */
	BOOST_TEST (t.remove_element ("/"));
	BOOST_TEST (root_children.empty());

	BOOST_TEST (t.remove_element ("/") == false);
}


BOOST_AUTO_TEST_CASE (test_data_initialize_and_kept)
{
	/* Just some fun using the new data structure */
	FileTrie<int> t;

	t.insert_directory ("/test/testdir");
	BOOST_TEST (t.find_directory ("/test/testdir")->data == 0);

	t.find_directory("/test/testdir")->data = 5;
	BOOST_TEST (t.find_directory ("/test/testdir")->data == 5);

	t.insert_file ("/test/testdir/test3");
	BOOST_TEST (t.find_file ("/test/testdir/test3")->data == 0);

	t.find_file ("/test/testdir/test3")->data = 6;
	BOOST_TEST (t.find_directory ("/test/testdir")->data == 5);
	BOOST_TEST (t.find_file ("/test/testdir/test3")->data == 6);


	/* Really test initialization */
	class A
	{
	public:
		int value;

		A() : value(10) {}
	};

	FileTrie<A> t2;

	t2.insert_directory ("/testdir");
	BOOST_TEST (t2.find_directory ("/testdir")->data.value == 10);
}


BOOST_AUTO_TEST_CASE (test_handles)
{
	auto node = FileTrieTestAdaptor<int>::make_node();

	/* Two different constructors. */
	auto he = FileTrieTestAdaptor<int>::make_handle();
	auto hc = FileTrieTestAdaptor<int>::make_handle(&node);

	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (he) == nullptr);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (hc) == &node);

	/* Additional copy- and move constructors */
	auto he_c = he;
	auto hc_c = hc;

	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (he_c) == nullptr);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (hc_c) == &node);

	auto he_m = move(he_c);
	auto hc_m = move(hc_c);

	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (he_m) == nullptr);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (hc_m) == &node);

	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (he_c) == nullptr);
	BOOST_TEST (FileTrieTestAdaptor<int>::get_node_pointer (hc_c) == nullptr);

	/* Operators * and -> */
	node.data = 7;
	BOOST_TEST ((*hc).data == 7);
	BOOST_TEST (hc->data == 7);

	node.data = 8;
	BOOST_TEST ((*hc).data == 8);
	BOOST_TEST (hc->data == 8);

	/* Operator bool */
	BOOST_TEST ((bool) he == false);
	BOOST_TEST ((bool) hc == true);

	/* Operators == and != */
	BOOST_TEST ((bool) (he == he));
	BOOST_TEST (!(he != he));

	BOOST_TEST ((bool) (he != hc));
	BOOST_TEST (!(he == hc));

	auto node2 = FileTrieTestAdaptor<int>::make_node();
	auto  hc2 = FileTrieTestAdaptor<int>::make_handle (&node2);

	BOOST_TEST ((bool) (hc2 == hc2));
	BOOST_TEST (!(hc2 != hc2));

	BOOST_TEST ((bool) (hc != hc2));
	BOOST_TEST (!(hc == hc2));
}
