/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains various tools to extract information from the package
 * database or update it. These tools are intended to be used by the user and
 * therefore be exposed through the commandline interface. */
#ifndef __PKG_TOOLS_H
#define __PKG_TOOLS_H

#include <memory>
#include <string>
#include "parameters.h"

std::string pkg_state_to_string (int state);

bool list_installed_packages (std::shared_ptr<Parameters> params);

bool show_version (std::shared_ptr<Parameters> params);
bool list_available (std::shared_ptr<Parameters> params);

bool show_problems (std::shared_ptr<Parameters> params);

#endif /* __PKG_TOOLS_H */
