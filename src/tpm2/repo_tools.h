/** Tools for repositories */
#ifndef __REPO_TOOLS_H
#define __REPO_TOOLS_H

#include <memory>
#include "parameters.h"

namespace repo_tools {

bool create_index (std::shared_ptr<Parameters> params);

}

#endif /* __REPO_TOOLS_H */
