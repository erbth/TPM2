include_directories("${CMAKE_SOURCE_DIR}/src/common")
include_directories("${CMAKE_SOURCE_DIR}/src/libtpm2/include")

add_subdirectory(tpm2)
add_subdirectory(tpm2_pack)
add_subdirectory(common)
add_subdirectory(libtpm2)
