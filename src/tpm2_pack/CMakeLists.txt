add_executable (tpm2_pack
	tpm2_pack.cc
	pack.cc
	../common/common_utilities.cc
	../common/package_meta_data.cc
	../common/dependencies.cc
	../common/transport_form.cc
	../common/message_digest.cc
	../common/file_list.cc
	)

target_include_directories (tpm2_pack PRIVATE ${TINY_XML2_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
target_link_libraries(tpm2_pack libtpm2)
target_link_libraries (tpm2_pack stdc++fs ${TINY_XML2_LIBRARIES} ${ZLIB_LIBRARIES})

install (TARGETS tpm2_pack DESTINATION bin)
