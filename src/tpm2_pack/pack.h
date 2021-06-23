/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module packs a package. */

#ifndef __PACK_H
#define __PACK_H

#include <filesystem>
#include <string>
#include <vector>
#include <regex>
#include "managed_buffer.h"

bool pack (const std::string& dir);

bool create_file_index (const std::filesystem::path& dir, DynamicBuffer<uint8_t>& dst, size_t& size);
bool create_config_files (const std::filesystem::path& dir, DynamicBuffer<uint8_t>& dst, size_t& size,
		const std::vector<std::regex>& patterns);

/* Be aware that this calls exec and does not close open fds. Make sure to
 * set the CLOEXEC flag on them! */
bool create_tar_archive (const std::string& dir, DynamicBuffer<uint8_t>& dst, size_t& size);

#endif /* __PACK_H */
