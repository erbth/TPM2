add_library(libtpm2 STATIC
	architecture.cpp
	depres2.cpp
	package_constraints.cpp
	version_number.cpp
)

set_target_properties(libtpm2 PROPERTIES OUTPUT_NAME libtpm2)

if (WITH_TESTS)
	add_subdirectory(evaluate_solvers)
	add_subdirectory(tests)
endif()
