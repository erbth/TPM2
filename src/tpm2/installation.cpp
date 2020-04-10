/** This file is part of the TSClient LEGACY Package Manager
 *
 * High level installation, reinstallation and upgrading of packages */

#include <cstdio>
#include <map>
#include <regex>
#include <optional>
#include "installation.h"
#include "utility.h"
#include "depres.h"
#include "package_db.h"

using namespace std;
namespace pc = PackageConstraints;


struct parse_cmd_param_result
{
	bool success;
	const string& pkg;
	string err;

	string name;
	int arch;
	shared_ptr<pc::Formula> vc;

	parse_cmd_param_result (const string& pkg)
		: pkg(pkg)
	{}
};

parse_cmd_param_result parse_cmd_param (const Parameters& params, const std::string& pkg)
{
	parse_cmd_param_result res (pkg);
	res.success = true;
	res.arch = params.default_architecture;

	/* May be of the form name@arch>=version */
	const regex pattern1(
			"([^<>!=@]+)[ \t]*(@(amd64|i386))?[ \t]*((>=|<=|>|<|=|==|!=)(s:)?([^<>!=@]+))?");

	smatch m1;
	if (regex_match (pkg, m1, pattern1))
	{
		res.name = m1[1].str();
		
		if (m1[4].matched)
		{
			auto op = m1[5];

			char type;

			if (op == ">=")
			{
				type = pc::PrimitivePredicate::TYPE_GEQ;
			}
			else if (op == "<=")
			{
				type = pc::PrimitivePredicate::TYPE_LEQ;
			}
			else if (op == ">")
			{
				type = pc::PrimitivePredicate::TYPE_GT;
			}
			else if (op == "<")
			{
				type = pc::PrimitivePredicate::TYPE_LT;
			}
			else if (op == "=" || op == "==")
			{
				type = pc::PrimitivePredicate::TYPE_EQ;
			}
			else
			{
				type = pc::PrimitivePredicate::TYPE_NEQ;
			}

			try
			{
				res.vc = make_shared<pc::PrimitivePredicate>(
						m1[6].matched, type, VersionNumber(m1[7].str()));
			}
			catch (InvalidVersionNumberString& e)
			{
				res.err = e.what();
				res.success = false;
				return res;
			}
		}

		if (m1[2].matched)
		{
			res.arch = Architecture::from_string (m1[3].str());
		}

		return res;
	}

	res.success = false;
	res.err = "Unknown format";
	return res;
}


bool print_installation_graph(shared_ptr<Parameters> params)
{
	print_target(params, true);

	/* Read the package database */
	PackageDB pkgdb(params);

	vector<shared_ptr<PackageMetaData>> installed_packages =
		pkgdb.get_packages_in_state (ALL_PKG_STATES);

	/* Build the installation graph */
	vector<pair<pair<const string, int>, shared_ptr<const PackageConstraints::Formula>>> new_packages;

	for (const auto& pkg : params->operation_packages)
	{
		auto res = parse_cmd_param (*params, pkg);
		if (!res.success)
		{
			fprintf (stderr, "Unknown package description: %s (%s)\n",
					res.pkg.c_str(), res.err.c_str());
			return false;
		}

		fprintf (stderr, "Additional package: %s @%s %s\n",
				res.name.c_str(),
				Architecture::to_string (res.arch).c_str(),
				res.vc ? res.vc->to_string().c_str() : " all versions");

		new_packages.emplace_back (make_pair(
					make_pair(res.name, res.arch), res.vc));
	}

	depres::ComputeInstallationGraphResult r =
		depres::compute_installation_graph(params, installed_packages, new_packages);

	if (r.status != depres::ComputeInstallationGraphResult::SUCCESS)
	{
		fprintf (stderr, "Error: Failed to build the installation graph: %s\n",
				r.error_message.c_str());

		return false;
	}


	/* Print it. */
	printf ("digraph Dependencies {\n");

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
		auto& iv = node->currently_installed_version;

		if (!cv)
		{
			fprintf (stderr, "No version chosen for packages %s@%s.\n",
					node->name.c_str(),
					Architecture::to_string(node->architecture).c_str());

			return false;
		}

		printf ("    %d [label=\"", node_indeces[node.get()]);

		if (!iv)
		{
			string irs;

			switch (cv->installation_reason)
			{
				case INSTALLATION_REASON_AUTO:
					irs = "auto";
					break;

				case INSTALLATION_REASON_MANUAL:
					irs = "manual";
					break;

				default:
					irs = "invalid";
					break;
			}

			printf ("Missing_pkg (%s, %s, %s, %s)",
					node->name.c_str(),
					Architecture::to_string(node->architecture).c_str(),
					irs.c_str(),
					cv->version.to_string().c_str());
		}

		printf ("\"];\n");


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

	/* Read the package database */
	PackageDB pkgdb(params);
	vector<shared_ptr<PackageMetaData>> installed_packages =
		pkgdb.get_packages_in_state (ALL_PKG_STATES);

	/* Ensure that the system is in a clean state */
	for (auto mdata : installed_packages)
	{
		if (mdata->state != PKG_STATE_CONFIGURED && mdata->state != PKG_STATE_CONFIGURE_BEGIN)
		{
			fprintf (stderr, "System is not in a clean state. Package "
					"%s@%s:%s is not in an accepted state.\n",
					mdata->name.c_str(),
					Architecture::to_string (mdata->architecture).c_str(),
					mdata->version.to_string().c_str());

			return false;
		}
	}

	/* Find packages in repo and resolve their dependencies, recursively. For
	 * this I have the depres module. */
	vector<pair<pair<const string, int>, shared_ptr<const PackageConstraints::Formula>>> new_packages;

	for (const auto& pkg : params->operation_packages)
	{
		auto res = parse_cmd_param (*params, pkg);
		if (!res.success)
		{
			fprintf (stderr, "Unknown package description: %s (%s)\n",
					res.pkg.c_str(), res.err.c_str());
			return false;
		}

		new_packages.emplace_back (make_pair (
					make_pair (res.name, res.arch), res.vc));
	}

	depres::ComputeInstallationGraphResult comp_igraph_res =
		depres::compute_installation_graph (params, installed_packages, new_packages);

	if (comp_igraph_res.status != depres::ComputeInstallationGraphResult::SUCCESS)
	{
		fprintf (stderr, "Failed to build the installation graph: %s\n",
				comp_igraph_res.error_message.c_str());

		return false;
	}

	depres::InstallationGraph& igraph = comp_igraph_res.g;


	/* Determine an unpack and a configuration order */
	list<depres::InstallationGraphNode*> unpack_order = depres::serialize_igraph (
			igraph, true);

	list<depres::InstallationGraphNode*> configuration_order = depres::serialize_igraph (
			igraph, false);


	/* Set package states of new packages to wanted. They are invalid now. */
	for (auto ig_node : unpack_order)
	{
		if (ig_node->chosen_version->state == PKG_STATE_INVALID)
			ig_node->chosen_version->state = PKG_STATE_WANTED;
	}

	/* Low-level unpack the packages */
	printf ("Unpacking packages.\n");

	for (auto ig_node : unpack_order)
	{
		if (ig_node->chosen_version->state == PKG_STATE_WANTED)
		{
			if (!ll_unpack_package (params, pkgdb, ig_node->provided_package))
				return false;
		}
	}

	/* Low-level configure the packages */
	if (params->target_is_native())
	{
		printf ("Configuring packages.\n");

		for (auto ig_node : configuration_order)
		{
			if (ig_node->chosen_version->state == PKG_STATE_CONFIGURE_BEGIN)
			{
				if (!ll_configure_package (params, pkgdb, ig_node->chosen_version,
							ig_node->provided_package, ig_node->sms))
					return false;
			}
		}
	}
	else
	{
		printf ("Not configuring packages because the target is not native.\n");
	}

	return true;
}


bool ll_unpack_package (
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		shared_ptr<ProvidedPackage> pp)
{
	auto mdata = pp->get_mdata();

	printf ("ll unpacking package %s@%s\n", mdata->name.c_str(),
			Architecture::to_string (mdata->architecture).c_str());

	/* Create DB tuple to make the operation transactional and store maintainer
	 * scripts. */
	printf ("  Creating db tuple and storing maintainer scripts ...");
	fflush (stdout);

	try
	{
		pkgdb.begin();

		mdata->state = PKG_STATE_PREINST_BEGIN;

		pkgdb.update_or_create_package (mdata);
		pkgdb.set_dependencies (mdata);

		StoredMaintainerScripts sms (params, mdata,
				pp->get_preinst(),
				pp->get_configure(),
				pp->get_unconfigure(),
				pp->get_postrm());

		sms.write();

		pkgdb.commit();

		printf (COLOR_GREEN " OK\n" COLOR_NORMAL);
	}
	catch (exception& e)
	{
		pkgdb.rollback();
		printf (COLOR_RED " failed\n" COLOR_NORMAL);
		printf ("%s\n", e.what());
		return false;
	}
	catch (...)
	{
		pkgdb.rollback();
		throw;
	}


	/* Run preinst if available */
	try
	{
		printf ("  Running preinst script ...");
		fflush (stdout);

		auto preinst = pp->get_preinst();
		if (preinst)
			run_script (params, *preinst);

		mdata->state = PKG_STATE_UNPACK_BEGIN;
		pkgdb.update_state (mdata);

		printf (COLOR_GREEN " OK\n" COLOR_NORMAL);
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed\n" COLOR_NORMAL);
		printf ("%s\n", e.what());
		return false;
	}


	/* Unpack the archive */
	try
	{
		printf ("  Unpacking the package's archive ...");
		fflush (stdout);

		if (pp->has_archive())
			pp->unpack_archive_to_directory (params->target);

		mdata->state = PKG_STATE_CONFIGURE_BEGIN;
		pkgdb.update_state (mdata);

		printf (COLOR_GREEN " OK\n" COLOR_NORMAL);
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed\n" COLOR_NORMAL);
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}


bool ll_configure_package (
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata,
		shared_ptr<ProvidedPackage> pp,
		shared_ptr<StoredMaintainerScripts> sms)
{
	printf ("ll configuring package %s@%s\n",
			mdata->name.c_str(), Architecture::to_string (mdata->architecture).c_str());

	if (!pp && !sms)
	{
		fprintf (stderr, "Internal error: Neither pp nor sms specified.");
		return false;
	}


	try
	{
		printf ("  Running configure script ...");
		fflush (stdout);


		auto configure = pp ? pp->get_configure() : sms->get_configure();
		if (configure)
			run_script (params, *configure);


		mdata->state = PKG_STATE_CONFIGURED;
		pkgdb.update_state (mdata);

		printf (COLOR_GREEN " OK\n" COLOR_NORMAL);
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed\n" COLOR_NORMAL);
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}
