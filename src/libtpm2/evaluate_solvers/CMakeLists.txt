add_library(read_scenario
	read_scenario.cpp)

target_include_directories(read_scenario PRIVATE ${TINY_XML2_INCLUDE_DIRS})
target_link_libraries(read_scenario libtpm2 ${TINY_XML2_LIBRARIES})


add_executable(print_scenario
	print_scenario.cpp)

target_link_libraries(print_scenario read_scenario)
