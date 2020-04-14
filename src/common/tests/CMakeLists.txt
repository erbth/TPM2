add_executable (test_version_number
	test_version_number.cpp
	../version_number.cpp)

target_link_libraries (test_version_number ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test (NAME test_version_number COMMAND test_version_number)


add_executable (test_package_constraints
	test_package_constraints.cpp
	../package_constraints.cpp
	../version_number.cpp)

target_include_directories (test_package_constraints PRIVATE ${TINY_XML2_INCLUDE_DIRS})
target_link_libraries (test_package_constraints ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${TINY_XML2_LIBRARIES})
add_test (NAME test_package_constraints COMMAND test_package_constraints)


add_executable (test_message_digest
	test_message_digest.cpp
	../message_digest.cpp)

target_link_libraries (test_message_digest ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test (NAME test_mesage_digest COMMAND test_message_digest)


add_executable (test_file_trie
	test_file_trie.cpp
	../common_utilities.cpp)

target_link_libraries (test_file_trie ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} stdc++fs)
add_test (NAME test_file_trie COMMAND test_file_trie)
