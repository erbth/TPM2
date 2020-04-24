/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module includes utilities to use in various places. */

#ifndef __UTILITY_H
#define __UTILITY_H

#include <filesystem>
#include <memory>
#include <string>
#include "parameters.h"
#include "common_utilities.h"
#include "managed_buffer.h"
#include "package_constraints.h"

void print_target(std::shared_ptr<Parameters> params, bool to_stderr = false);

/* throws a gp_exception or alike if something fails, in addition to the script
 * printing stuff itself. This actually does an execl call and hence works with
 * any executable, not only scripts. I just call it script referring to Debian's
 * maintainer scripts.
 *
 * Be warned that this does not close fds before exec. Use CLOEXEC for this. */
void run_script (std::shared_ptr<Parameters> params, ManagedBuffer<char>& script,
		const char *arg1 = nullptr, const char *arg2 = nullptr);

std::filesystem::path create_tmp_dir (std::shared_ptr<Parameters> params);

std::string installation_reason_to_string (char reason);


/* A function with result object class to parse package specifications supplied
 * by the user */
struct parse_cmd_param_result
{
	bool success;
	const std::string& pkg;
	std::string err;

	std::string name;
	int arch;
	std::shared_ptr<PackageConstraints::Formula> vc;

	parse_cmd_param_result (const std::string& pkg)
		: pkg(pkg)
	{}
};

parse_cmd_param_result parse_cmd_param (const Parameters& params, const std::string& pkg);

std::string pkg_state_to_string (int state);

#endif /* __UTILITY_H */
