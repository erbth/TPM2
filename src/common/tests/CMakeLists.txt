add_executable (test_version_number
	test_version_number.cpp
	../version_number.cpp)

target_link_libraries (test_version_number ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test (NAME test_version_number COMMAND test_version_number)
