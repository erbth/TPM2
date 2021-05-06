add_executable (test_message_digest
	test_message_digest.cc
	../message_digest.cc)

target_link_libraries (test_message_digest ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test (NAME test_message_digest COMMAND test_message_digest)
