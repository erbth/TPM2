/** This file is part of the TSClient LEGACY Package Manager
 *
 * High level installation, reinstallation and upgrading of packages */
#ifndef __INSTALLATION_H
#define __INSTALLATION_H

#include <memory>
#include "parameters.h"

bool print_installation_graph(std::shared_ptr<Parameters> params);

bool install_packages(std::shared_ptr<Parameters> params);

#endif /* __INSTALLATION_H */
