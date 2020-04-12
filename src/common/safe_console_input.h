/* Safe to use console input functions (no traps with non-canonical mode and EOF
 * etc.) */
#ifndef __SAFE_CONSOLE_INPUT_H
#define __SAFE_CONSOLE_INPUT_H

#include <string>

char safe_query_user_input (const std::string& options);

#endif /* __SAFE_CONSOLE_INPUT_H */
