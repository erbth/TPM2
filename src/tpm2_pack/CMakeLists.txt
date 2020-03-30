add_executable (tpm2_pack
	tpm2_pack.cpp
	pack.cpp
	../common/common_utilities.cpp
	../common/package_meta_data.cpp
	../common/architecture.cpp
	../common/version_number.cpp
	../common/dependencies.cpp
	../common/package_constraints.cpp
	../common/transport_form.cpp)

target_include_directories (tpm2_pack PRIVATE ${TINY_XML2_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
target_link_libraries (tpm2_pack stdc++fs ${TINY_XML2_LIBRARIES} ${ZLIB_LIBRARIES})
