/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module packs a package. */

#ifndef __PACK_H
#define __PACK_H

#include <string>
#include "managed_buffer.h"

bool pack (const std::string& dir);

/* Be aware that this calls exec and does not close open fds. Make sure to
 * set the CLOEXEC flag on them! */
bool create_tar_archive (const std::string& dir, DynamicBuffer<uint8_t>& dst, size_t& size);

#endif /* __PACK_H */
