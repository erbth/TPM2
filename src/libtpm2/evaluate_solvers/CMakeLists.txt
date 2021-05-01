set(DEFAULT_SCENARIO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/scenarios")
configure_file(evaluate_solvers_config.h.in evaluate_solvers_config.h)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(solver_evaluation_tools
	read_scenario.cc
	run_scenario.cc
	evaluate_all_scenarios.cc)

target_include_directories(solver_evaluation_tools PRIVATE ${TINY_XML2_INCLUDE_DIRS})
target_link_libraries(solver_evaluation_tools libtpm2 ${TINY_XML2_LIBRARIES})


add_executable(print_scenario
	print_scenario.cc)

target_link_libraries(print_scenario solver_evaluation_tools)

add_executable(run_scenario
	run_scenario_cmd.cc)

target_link_libraries(run_scenario solver_evaluation_tools)

add_executable(evaluate_all_scenarios
	evaluate_all_scenarios_cmd.cc)

target_link_libraries(evaluate_all_scenarios solver_evaluation_tools)
