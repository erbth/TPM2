add_executable(tpm2
	tpm2.cpp
	installation.cpp
	parameters.cpp
	utility.cpp
	depres.cpp
	package_db.cpp
	package_provider.cpp
	directory_repository.cpp
	stored_maintainer_scripts.cpp
	pkg_tools.cpp
	../common/package_constraints.cpp
	../common/dependencies.cpp
	../common/package_meta_data.cpp
	../common/architecture.cpp
	../common/version_number.cpp
	../common/common_utilities.cpp
	../common/transport_form.cpp
	../common/file_list.cpp
	../common/safe_console_input.cpp
	../common/message_digest.cpp
	)


target_include_directories(tpm2 PRIVATE ${SQLITE3_INCLUDE_DIRS} ${TINY_XML2_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})

if (STATIC_TPM2)
	target_link_libraries(tpm2 ${SQLITE3_STATIC_LIBRARIES} ${TINY_XML2_STATIC_LIBRARIES} ${ZLIB_STATIC_LIBRARIES} stdc++fs --static)
else ()
	target_link_libraries(tpm2 ${SQLITE3_LIBRARIES} ${TINY_XML2_LIBRARIES} ${ZLIB_LIBRARIES} stdc++fs)
endif ()
