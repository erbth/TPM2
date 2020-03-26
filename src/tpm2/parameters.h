/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains the current runtime parameters, i.e. what operation
 * should be performed, where the filesystem root is and those things. Some of
 * the configuration is read from tpm2_config.h as static configuration, and the
 * parameters are overwritten by commandline arguments as needed. */
#ifndef __PARAMETERS_H
#define __PARAMETERS_H

#include <string>
#include <vector>

enum operation_type {
	OPERATION_INVALID,
	OPERATION_INSTALL,
	OPERATION_REINSTALL,
	OPERATION_POLICY,
	OPERATION_SHOW_VERSION,
	OPERATION_REMOVE,
	OPERATION_REMOVE_UNNEEDED,
	OPERATION_LIST_INSTALLED,
	OPERATION_SHOW_PROBLEMS,
	OPERATION_RECOVER,
	OPERATION_INSTALLATION_GRAPH,
	OPERATION_REVERSE_DEPENDENCIES,
	OPERATION_MARK_MANUAL,
	OPERATION_MARK_AUTO
};

/* Most parameters are public to allow for easier access. */
class Parameters
{
public:
	std::string target = "/";

	/* The operation to perform along with the packages on which it shall be
	 * performed. */
	enum operation_type operation = OPERATION_INVALID;
	std::vector<std::string> operation_packages;

	/* Methods */
	bool target_is_native() const;
};

#endif /* __PARAMETERS_H */
