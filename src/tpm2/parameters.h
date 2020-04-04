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
#include "tpm2_config.h"
#include "architecture.h"

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


struct RepositorySpecification
{
	/* Good for parsing etc. */
	static const char TYPE_INVALID = -1;
	static const char TYPE_DIR = 0;

	const char type;

	/* Depend on type:
	 * Dir:
	 *   param1:   location
	 */
	const std::string param1;

	RepositorySpecification (char type, const std::string& param1);

	bool operator==(const RepositorySpecification& o) const;
};


/* Most parameters are public to allow for easier access. */
struct Parameters
{
	std::string target = "/";

	int default_architecture = Architecture::invalid;
	std::vector<RepositorySpecification> repos;

	/* The operation to perform along with the packages on which it shall be
	 * performed. */
	enum operation_type operation = OPERATION_INVALID;
	std::vector<std::string> operation_packages;

	/* Methods */
	bool target_is_native() const;

	/* Read the parameters from environment variables. This should be executed
	 * before settings them manually from i.e. commandline arguments as the
	 * latter can usually override the environment. */
	void read_from_env();
};


bool read_config_file (std::shared_ptr<Parameters> params);

#endif /* __PARAMETERS_H */
