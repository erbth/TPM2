/** This file is part of the TSClient LEGACY Package Manager
 *
 * This source module contains a factory-like function for creating depres
 * solvers. Note that it actually is not a factory as it's just a function and
 * not a class ... The functions main purpose is to resolve solver names into
 * solver classes. */
#ifndef __DEPRES_FACTORY_H
#define __DEPRES_FACTORY_H

#include <memory>
#include <string>
#include "depres_common.h"

namespace depres
{
	std::shared_ptr<SolverInterface> create_solver(const std::string &solver_name);
}

#endif /* __DEPRES_FACTORY_H */
