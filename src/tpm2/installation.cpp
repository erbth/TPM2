/** This file is part of the TSClient LEGACY Package Manager
 *
 * High level installation, reinstallation and upgrading of packages */

#include <cstdio>
#include "installation.h"
#include "utility.h"
#include "depres.h"
#include "package_db.h"

using namespace std;


bool print_installation_graph(shared_ptr<Parameters> params)
{
	print_target(params);

	/* Read the package database */
	PackageDB pkgdb(params);

	vector<shared_ptr<PackageMetaData>> installed_packages;

	/* Build the installation graph */
	vector<pair<pair<const string, int>, shared_ptr<const PackageConstraints::Formula>>> new_packages;

	depres::ComputeInstallationGraphResult r =
		depres::compute_installation_graph(installed_packages, new_packages);

	switch (r.status)
	{
		case CIG_SUCCESS:
			break;


		case CIG_INVALID_CURRENT_CONFIG:
			printf ("Error: Failed to build the installation graph.\n");
			printf ("Invalid current package configuration: %s\n",
					r.error_message.c_str());

			return false;


		default:
			printf ("Error: Failed to build the installation graph.\n");
			return false;
	}


	/* Print it. */
	printf ("digraph \"installation graph\" {\n");
	printf ("}\n");

	return true;
}


bool install_packages(shared_ptr<Parameters> params)
{
	print_target(params);

	/* Find package in repo and resolve its dependencies, recursively. */

	/* Determine an installation order */

	/* Determine an configuration order */

	/* Low-level install the packages */

	/* Low-level configure the packages */

	return true;
}
