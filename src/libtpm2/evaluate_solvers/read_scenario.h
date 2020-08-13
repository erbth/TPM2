/** This file is part of the TSClient LEGACY Package Manager
 *
 * A library that reads scenarios for evaluating dependency solvers from a xml
 * file. */
#ifndef __READ_SCENARIO_H
#define __READ_SCENARIO_H

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "package_constraints.h"
#include "package_version.h"

struct Scenario
{
	struct Package
	{
		std::string name;
		int arch = 0;
		VersionNumber sv = VersionNumber("0");
		VersionNumber bv = VersionNumber("0");

		std::vector<std::string> files;
		std::vector<std::string> directories;

		/* (name, arch, constraints */
		std::vector<std::tuple<std::string, int, std::shared_ptr<PackageConstraints::Formula>>> deps;
	};

	std::string name;

	/* The package universe - a list of pointers to packages */
	std::vector<std::shared_ptr<Package>> universe;

	/* Installed packages - a list of tuples
	 * (pointer to package, manually installed) */
	std::vector<std::pair<std::shared_ptr<Package>, bool>> installed;

	/* Selected packages - a list of tuples (name, arch, constraints) */
	std::vector<std::tuple<std::string, int, std::shared_ptr<PackageConstraints::Formula>>> selected;

	/* The desired target configuration - a list of pointers to packages */
	std::vector<std::shared_ptr<Package>> desired;
};

std::shared_ptr<Scenario> read_scenario(std::string filename);

#endif /* __READ_SCENARIO_H */
