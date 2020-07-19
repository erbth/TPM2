add_executable (tpm2_pack
	tpm2_pack.cpp
	pack.cpp
	../common/common_utilities.cpp
	../common/package_meta_data.cpp
	../common/architecture.cpp
	../common/dependencies.cpp
	../common/transport_form.cpp
	../common/message_digest.cpp
	../common/file_list.cpp
	)

target_include_directories (tpm2_pack PRIVATE ${TINY_XML2_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
target_link_libraries(tpm2_pack libtpm2)
target_link_libraries (tpm2_pack stdc++fs ${TINY_XML2_LIBRARIES} ${ZLIB_LIBRARIES})

install (TARGETS tpm2_pack DESTINATION bin)
