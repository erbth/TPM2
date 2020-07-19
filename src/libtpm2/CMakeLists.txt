add_library(libtpm2 STATIC
	version_number.cpp
	package_constraints.cpp
	depres2.cpp)

set_target_properties(libtpm2 PROPERTIES OUTPUT_NAME libtpm2)

if (WITH_TESTS)
	add_subdirectory(tests)
endif()
