add_executable (test_version_number
	test_version_number.cc
	../version_number.cc)

target_link_libraries (test_version_number ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
add_test (NAME test_version_number COMMAND test_version_number)


add_executable (test_package_constraints
	test_package_constraints.cc
	../package_constraints.cc
	../version_number.cc)

target_include_directories (test_package_constraints PRIVATE ${TINY_XML2_INCLUDE_DIRS})
target_link_libraries (test_package_constraints ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${TINY_XML2_LIBRARIES})
add_test (NAME test_package_constraints COMMAND test_package_constraints)


add_executable (test_package_version
	test_package_version.cc
	../package_version.cc
	../package_constraints.cc
	../version_number.cc)

target_include_directories (test_package_constraints PRIVATE ${TINY_XML2_INCLUDE_DIRS})
target_link_libraries (test_package_version ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${TINY_XML2_LIBRARIES})
add_test (NAME test_package_version COMMAND test_package_version)


add_executable (test_depres_common
	test_depres_common.cc
	../depres_common.cc
	../depres2.cc
	../package_constraints.cc
	../package_version.cc
	../version_number.cc)

target_include_directories (test_depres_common PRIVATE ${TINY_XML2_INCLUDE_DIRS})
target_link_libraries (test_depres_common ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${TINY_XML2_LIBRARIES})
add_test (NAME test_depres_common COMMAND test_depres_common)

add_executable (test_depres2_basics
	test_depres2_basics.cc
	../depres_common.cc
	../depres2.cc
	../package_constraints.cc
	../package_version.cc
	../version_number.cc)

target_include_directories (test_depres2_basics PRIVATE ${TINY_XML2_INCLUDE_DIRS})
target_link_libraries (test_depres2_basics ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${TINY_XML2_LIBRARIES})
add_test (NAME test_depres2_basics COMMAND test_depres2_basics)
