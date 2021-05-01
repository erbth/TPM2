add_executable (test_message_digest
	test_message_digest.cc
	../message_digest.cc)

target_link_libraries (test_message_digest ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test (NAME test_message_digest COMMAND test_message_digest)


add_executable (test_file_trie
	test_file_trie.cc
	../common_utilities.cc)

target_link_libraries (test_file_trie ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} stdc++fs)
add_test (NAME test_file_trie COMMAND test_file_trie)
