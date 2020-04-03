/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module includes common utilities to use in various places that are not
 * connected to a program. */

#ifndef __COMMON_UTILITIES_H
#define __COMMON_UTILITIES_H

#include <istream>
#include <string>

/* Get the abslute path of a potentially relaive path.
 *
 * @param path The path to expand
 * @returns An absolute path to the specified target
 *
 * @throws std::system_error if the path cannot be resolved. */
std::string get_absolute_path(const std::string& path);

std::string convenient_readlink (const std::string& path);
std::string convenient_readlink (const char *path);

#endif /* __COMMON_UTILITIES_H */
