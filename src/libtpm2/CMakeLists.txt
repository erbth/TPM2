add_library(libtpm2 STATIC
	architecture.cc
	depres_common.cc
	depres_factory.cc
	depres2.cc
	package_constraints.cc
	package_version.cc
	version_number.cc
)

set_target_properties(libtpm2 PROPERTIES OUTPUT_NAME libtpm2)

if (WITH_TESTS)
	add_subdirectory(evaluate_solvers)
	add_subdirectory(tests)
endif()
