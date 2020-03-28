/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module includes utilities to use in various places. */

#ifndef __UTILITY_H
#define __UTILITY_H

#include <memory>
#include <string>
#include "parameters.h"

void print_target(std::shared_ptr<Parameters> params, bool to_stderr = false);

/* Get the abslute path of a potentially relaive path.
 *
 * @param path The path to expand
 * @returns An absolute path to the specified target
 *
 * @throws std::system_error if the path cannot be resolved. */
std::string get_absolute_path(const std::string& path);

#endif /* __UTILITY_H */
