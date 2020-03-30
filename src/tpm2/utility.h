/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module includes utilities to use in various places. */

#ifndef __UTILITY_H
#define __UTILITY_H

#include <memory>
#include <string>
#include "parameters.h"
#include "common_utilities.h"

void print_target(std::shared_ptr<Parameters> params, bool to_stderr = false);

#endif /* __UTILITY_H */
