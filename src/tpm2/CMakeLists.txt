add_executable(tpm2
	tpm2.cc
	installation.cc
	parameters.cc
	utility.cc
	depres.cc
	package_db.cc
	package_provider.cc
	directory_repository.cc
	stored_maintainer_scripts.cc
	pkg_tools.cc
	installation_package_version.cc
	repo_tools.cc
	../common/dependencies.cc
	../common/package_meta_data.cc
	../common/transport_form.cc
	../common/file_list.cc
	../common/safe_console_input.cc
	../common/message_digest.cc
	)


target_include_directories(tpm2 PRIVATE
	${SQLITE3_INCLUDE_DIRS}
	${TINY_XML2_INCLUDE_DIRS}
	${ZLIB_INCLUDE_DIRS}
	${LIBCRYPTO_INCLUDE_DIRS})

target_link_libraries(tpm2 libtpm2)

if (STATIC_TPM2)
	target_link_libraries(tpm2
		${SQLITE3_STATIC_LIBRARIES}
		${TINY_XML2_STATIC_LIBRARIES}
		${ZLIB_STATIC_LIBRARIES}
		${LIBCRYPTO_STATIC_LIBRARIES}
		stdc++fs --static)

else ()
	target_link_libraries(tpm2
		${SQLITE3_LIBRARIES}
		${TINY_XML2_LIBRARIES}
		${ZLIB_LIBRARIES}
		${LIBCRYPTO_STATIC_LIBRARIES}
		stdc++fs)

endif ()

install (TARGETS tpm2 DESTINATION bin)
