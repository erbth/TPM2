cmake_minimum_required(VERSION 3.1)

project(TPM2 CXX)

set(TPM2_VERSION_MAJOR 0)
set(TPM2_VERSION_MINOR 1)
set(TPM2_VERSION_PATCH 0)

set(CMAKE_CXX_FLAGS "-std=gnu++17 -Wall -gdwarf-2")
set(CMAKE_CXX_FLAGS_Debug "-DDEBUG")
set(CMAKE_CXX_FLAGS_Release "-O3")

# Find required packages
find_package(PkgConfig)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

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
