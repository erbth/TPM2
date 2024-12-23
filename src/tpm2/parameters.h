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
	OPERATION_UPGRADE,
	OPERATION_LIST_AVAILABLE,
	OPERATION_SHOW_VERSION,
	OPERATION_REMOVE,
	OPERATION_REMOVAL_GRAPH,
	OPERATION_REMOVE_UNNEEDED,
	OPERATION_LIST_INSTALLED,
	OPERATION_SHOW_PROBLEMS,
	OPERATION_INSTALLATION_GRAPH,
	OPERATION_REVERSE_DEPENDENCIES,
	OPERATION_DIRECT_REVERSE_DEPENDENCIES,
	OPERATION_MARK_MANUAL,
	OPERATION_MARK_AUTO,
	OPERATION_COMPARE_SYSTEM,

	OPERATION_CREATE_INDEX
};


struct RepositorySpecification
{
	/* Good for parsing etc. */
	static const char TYPE_INVALID = -1;
	static const char TYPE_DIR = 0;
	static const char TYPE_DIR_ALLOW_UNSIGNED = 1;

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

	bool autoremove = false;

	/* Disable prompts */
	bool assume_yes = false;
	bool adopt_all = false;

	/* Verbose output */
	bool verbose = false;

	/* Depres2 debug log */
	bool depres2_debug_log = false;

	/* Parameters for repository tools */
	std::string create_index_repo;
	std::string create_index_name = "index";

	/* Key file name behind --sign */
	std::string sign;

	/* Methods */
	bool target_is_native() const;

	/* Read the parameters from environment variables. This should be executed
	 * before settings them manually from i.e. commandline arguments as the
	 * latter can usually override the environment. */
	void read_from_env();
};


bool read_config_file (std::shared_ptr<Parameters> params);

#endif /* __PARAMETERS_H */
