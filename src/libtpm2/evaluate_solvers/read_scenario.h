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

	std::vector<std::shared_ptr<Package>> universe;

	/* (Package, manually installed) */
	std::vector<std::pair<std::shared_ptr<Package>, bool>> installed;

	/* (name, arch, constraints) */
	std::vector<std::tuple<std::string, int, std::shared_ptr<PackageConstraints::Formula>>> selected;
};

std::shared_ptr<Scenario> read_scenario(std::string filename);

#endif /* __READ_SCENARIO_H */
