/** This file is part of the TSClient LEGACY Package Manager
 *
 * Compare installed files with the database */
#ifndef __COMPARE_SYSTEM_H
#define __COMPARE_SYSTEM_H

#include <memory>
#include "parameters.h"

/* @returns  true in on success (even if the system differs), false otherwise */
bool compare_system (std::shared_ptr<Parameters> params);

#endif /* __COMPARE_SYSTEM_H */
