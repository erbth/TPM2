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
