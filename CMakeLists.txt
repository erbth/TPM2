cmake_minimum_required(VERSION 3.1)

project(TPM2 CXX)

set(TPM2_VERSION_MAJOR 2)
set(TPM2_VERSION_MINOR 0)
set(TPM2_VERSION_PATCH 0)

# Options
option (STATIC_TPM2 "Statically link the program TPM2" OFF)

set(CMAKE_CXX_FLAGS "-std=gnu++17 -Wall")
set(CMAKE_CXX_FLAGS_Debug "-DDEBUG -O1 -gdwarf-2")
set(CMAKE_CXX_FLAGS_Release "-O3")

# Find required packages
find_package(PkgConfig)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(TINY_XML2 REQUIRED tinyxml2)
pkg_check_modules(ZLIB REQUIRED zlib)

if (WITH_TESTS)
	find_package (Boost COMPONENTS unit_test_framework REQUIRED)
endif ()


# Configure a config file
configure_file(tpm2_config.h.in tpm2_config.h)
include_directories(${CMAKE_BINARY_DIR})


# Enable testing if desired
if (WITH_TESTS)
	enable_testing()
endif ()

add_subdirectory(src)


# Packaging                                                                      
set (CPACK_SOURCE_VERSION_MAJOR ${TPM2_VERSION_MAJOR})                           
set (CPACK_SOURCE_VERSION_MINOR ${TPM2_VERSION_MINOR})                           
set (CPACK_SOURCE_VERSION_PATCH ${TPM2_VERSION_PATCH})                           
set (CPACK_SOURCE_GENERATOR "TGZ")                                               

set (CPACK_SOURCE_IGNORE_FILES "/build;/\.git;.*\.swp;mysession\.vim;/playground;"
	"/Documentation/.*\.aux;/Documentation/.*\.out;/Documentation/.*\.synctex.gz;"
	"/Documentation/.*\.toc;/Documentation/.*\.log;"
	"${CPACK_SOURCE_IGNORE_FILES}")

set (CPACK_SOURCE_PACKAGE_FILE_NAME "tpm2-${CPACK_SOURCE_VERSION_MAJOR}.${CPACK_SOURCE_VERSION_MINOR}.${CPACK_SOURCE_VERSION_PATCH}")
include(CPack)
