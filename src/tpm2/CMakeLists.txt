add_executable(tpm2
	tpm2.cpp
	installation.cpp
	parameters.cpp
	utility.cpp
	depres.cpp
	package_db.cpp
	../common/package_constraints.cpp
	../common/dependencies.cpp
	../common/package_meta_data.cpp
	../common/architecture.cpp
	../common/version_number.cpp)


target_include_directories(tpm2 PRIVATE ${SQLITE3_INCLUDE_DIRS})
target_link_libraries(tpm2 ${SQLITE3_LIBRARIES} stdc++fs)
