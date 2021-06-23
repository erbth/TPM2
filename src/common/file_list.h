/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains a file list that may be used as index. */
#ifndef __FILE_LIST_H
#define __FILE_LIST_H

#include <memory>
#include <ostream>
#include <set>
#include <string>
#include "common_utilities.h"
#include "package_meta_data.h"


/* Prototypes */
namespace TransportForm {
	class ReadStream;
}


struct FileRecord
{
	/* Stored attributes */
	uint8_t type = FILE_TYPE_REGULAR;
	uint32_t uid = 0, gid = 0;
	uint16_t mode = 0;
	uint32_t size = 0;
	char sha1_sum[20] = { 0 };
	std::string path;

	FileRecord();
	FileRecord (uint8_t type, uint32_t uid, uint32_t gid, uint16_t mode,
			uint32_t size, uint8_t sha1_sum[], const std::string& path);

	/* Serialization */
	size_t binary_size () const;
	void to_binary (uint8_t *buf) const;

	/* Size must be tight and must include the path's terminating null
	 * character. */
	static void from_binary (const uint8_t *buf, size_t size, FileRecord& fr);

	bool operator<(const FileRecord& o) const;


	/* Doing something with the information. @param root refers to the root
	 * filesystem in which the file should be checked. This requires as files
	 * are treated with absolute paths, but the system which shall be modified
	 * may not be the on located in the current root filesystem (non-native
	 * target). */
	/* @param out specifies an ostream where difference information should be
	 * printed to. This does not include the file path, it's assumed that the
	 * apllication outputs that itself.
	 * @returns true if the file does either not exist on the target filesystem
	 * or its attributes match with this FileRecord's. This may raise an
	 * std::exception or descendent class ... */
	bool non_existent_or_matches (const std::string& root, std::ostream *out = nullptr) const;

	virtual ~FileRecord();
};


/* A dummy file record that has only a path. It can be used to find files by
 * path in a FileList. */
struct DummyFileRecord : public FileRecord
{
	DummyFileRecord(const std::string& path);
};


class FileList
{
private:
	std::set<FileRecord> files;

public:
	/* Attention: File is moved. */
	void add_file (FileRecord&& file);

	std::set<FileRecord>::const_iterator begin() const noexcept;
	std::set<FileRecord>::const_iterator end() const noexcept;

	std::set<FileRecord>::const_iterator find(const FileRecord& file) const;
};

#endif /* __FILE_LIST_H */
