/** This file is part of the TSClient LEGACY Package Manager
 *
 * High level installation, reinstallation and upgrading of packages */

#include <cstdio>
#include <map>
#include "installation.h"
#include "utility.h"
#include "depres.h"
#include "package_db.h"

using namespace std;


bool print_installation_graph(shared_ptr<Parameters> params)
{
	print_target(params, true);

	/* Read the package database */
	PackageDB pkgdb(params);

	vector<shared_ptr<PackageMetaData>> installed_packages =
		pkgdb.get_packages_in_state (ALL_PKG_STATES);

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
	printf ("digraph \"Dependencies\" {\n");

	map<depres::InstallationGraphNode*, int> node_indeces;

	/* Assign each node an index */
	int i = 0;

	for (auto& p : r.g.V)
	{
		node_indeces[p.second.get()] = i++;
	}


	for (auto& p : r.g.V)
	{
		shared_ptr<depres::InstallationGraphNode> node = p.second;
		auto cv = node->chosen_version;

		if (!cv)
		{
			fprintf (stderr, "No version chosen for packages %s@%s.\n",
					node->name.c_str(),
					Architecture::to_string(node->architecture).c_str());

			return false;
		}

		printf ("    %d [label=\"%s@%s:%s\"];\n",
				node_indeces[node.get()],
				cv->name.c_str(),
				Architecture::to_string(cv->architecture).c_str(),
				cv->version.to_string().c_str());


		for (auto d : node->pre_dependencies)
		{
			printf ("    %d -> %d [style=dotted];\n",
					node_indeces[node.get()], node_indeces[d]);
		}

		for (auto d : node->dependencies)
		{
			printf ("    %d -> %d;\n",
					node_indeces[node.get()], node_indeces[d]);
		}
	}

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
