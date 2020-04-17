/** This file is part of the TSClient LEGACY Package Manager
 *
 * High level installation, reinstallation and upgrading of packages */

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include "installation.h"
#include "utility.h"
#include "depres.h"
#include "package_db.h"
#include "safe_console_input.h"

using namespace std;
namespace pc = PackageConstraints;
namespace fs = std::filesystem;


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
		depres::compute_installation_graph(params, installed_packages, pkgdb, new_packages);

	if (r.status != depres::ComputeInstallationGraphResult::SUCCESS)
	{
		fprintf (stderr, "Error: Failed to build the installation graph: %s\n",
				r.error_message.c_str());

		return false;
	}


	/* Print it. */
	printf ("digraph Dependencies {\n");

	/* Assign each node an index */
	int i = 0;

	for (auto& p : r.g.V)
	{
		p.second->algo_priv = i++;
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

		string irs = installation_reason_to_string (cv->installation_reason);

		printf ("    %d [label=\"", (int) node->algo_priv);

		if (!iv)
		{
			printf ("Missing_pkg (%s, %s, %s, %s)",
					node->name.c_str(),
					Architecture::to_string(node->architecture).c_str(),
					irs.c_str(),
					cv->version.to_string().c_str());
		}
		else if (*iv == cv->version)
		{
			printf ("Present_pkg (%s, %s, %s, %s)",
					node->name.c_str(),
					Architecture::to_string(node->architecture).c_str(),
					irs.c_str(),
					cv->version.to_string().c_str());
		}
		else
		{
			printf ("Wrong_pkg (%s, %s, %s, %s)",
					node->name.c_str(),
					Architecture::to_string(node->architecture).c_str(),
					irs.c_str(),
					cv->version.to_string().c_str());
		}

		printf ("\"];\n");


		for (auto d : node->pre_dependencies)
		{
			printf ("    %d -> %d [style=dotted];\n",
					(int) node->algo_priv, (int) d->algo_priv);
		}

		for (auto d : node->dependencies)
		{
			printf ("    %d -> %d;\n",
					(int) node->algo_priv, (int) d->algo_priv);
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
		depres::compute_installation_graph (params, installed_packages, pkgdb, new_packages);

	if (comp_igraph_res.status != depres::ComputeInstallationGraphResult::SUCCESS)
	{
		fprintf (stderr, "Failed to build the installation graph: %s\n",
				comp_igraph_res.error_message.c_str());

		return false;
	}

	depres::InstallationGraph& igraph = comp_igraph_res.g;


	/* Check if package versions need to be changed. This is not supported yet.
	 * And depred does not remove installed packages yet, hence this check is
	 * enough to make sure that only new versions will be installed. */
	for (auto t : igraph.V)
	{
		auto ig_node = t.second;

		if (ig_node->currently_installed_version &&
				(*ig_node->currently_installed_version != ig_node->chosen_version->version))
		{
			fprintf (stderr, "Changing package versions is not supported yet.\n");
			fprintf (stderr, "%s@%s would be changed from %s to %s.\n",
					ig_node->name.c_str(),
					Architecture::to_string (ig_node->architecture).c_str(),
					ig_node->currently_installed_version->to_string().c_str(),
					ig_node->chosen_version->version.to_string().c_str());

			return false;
		}
	}


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
		else
		{
			/* Potentially change installation reason */
			if (ig_node->manual && ig_node->chosen_version->installation_reason
					!= INSTALLATION_REASON_MANUAL)
			{
				if (!ll_change_installation_reason (
							pkgdb,
							ig_node->chosen_version,
							INSTALLATION_REASON_MANUAL))
				{
					return false;
				}
			}
			else if (!ig_node->manual && ig_node->chosen_version->installation_reason
					== INSTALLATION_REASON_MANUAL)
			{
				if (!ll_change_installation_reason (
							pkgdb,
							ig_node->chosen_version,
							INSTALLATION_REASON_AUTO))
				{
					return false;
				}
			}
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


	/* Look for existing files and eventually adopt them */
	printf ("  Looking for existing files ...");
	fflush (stdout);

	try
	{
		auto files = pp->get_files();

		bool first = true;

		for (const auto& file : *files)
		{
			stringstream ss;

			if (!file.non_existent_or_matches (params->target, &ss))
			{
				if (first)
				{
					printf ("\n\n");
					first = false;
				}

				printf ("File \"%s\" differs from the one in the package: %sAdopt it anyway? ",
						file.path.c_str(), ss.str().c_str());

				auto c = safe_query_user_input ("yN");

				if (c != 'y')
					throw gp_exception ("User aborted.");

				printf ("Adopting \"%s\", which differs.\n", file.path.c_str());
			}
		}

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}


	/* Create DB tuples to make the operation transactional and store maintainer
	 * scripts. */
	printf ("  Creating db tuples and storing maintainer scripts ...");
	fflush (stdout);

	try
	{
		pkgdb.begin();

		mdata->state = PKG_STATE_PREINST_BEGIN;

		pkgdb.update_or_create_package (mdata);
		pkgdb.set_dependencies (mdata);
		pkgdb.set_files (mdata, pp->get_files());

		StoredMaintainerScripts sms (params, mdata,
				pp->get_preinst(),
				pp->get_configure(),
				pp->get_unconfigure(),
				pp->get_postrm());

		sms.write();

		pkgdb.commit();

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		pkgdb.rollback();
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
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

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
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

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
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

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}


bool ll_change_installation_reason (
		PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata,
		char reason)
{
	try
	{
		printf ("  Changing installation reason ...");
		fflush (stdout);

		mdata->installation_reason = reason;
		pkgdb.update_installation_reason (mdata);

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}


/***************************** Removing packages ******************************/
bool print_removal_graph (shared_ptr<Parameters> params)
{
	print_target (params, true);
	PackageDB pkgdb (params);

	/* Interpret the given package specifications */
	set<pair<string, int>> pkg_ids;

	for (const auto& pkg : params->operation_packages)
	{
		auto res = parse_cmd_param (*params, pkg);
		if (!res.success)
		{
			fprintf (stderr, "Unknown package description: %s (%s)\n",
					res.pkg.c_str(), res.err.c_str());
			return false;
		}

		pkg_ids.emplace (make_pair(move(res.name), res.arch));
	}

	/* Build the removal graph */
	depres::RemovalGraphBranch g = depres::build_removal_graph (pkgdb);

	/* If any packages are specified, reduce it to the branch to remove */
	if (!pkg_ids.empty())
		depres::reduce_to_branch_to_remove (g, pkg_ids);

	/* Print the removal graph */
	/* First, enumerate all nodes. */
	int i = 0;

	for (auto node : g.V)
		node->algo_priv = i++;

	printf ("digraph \"Removal Graph\" {\n");

	for (auto node : g.V)
	{
		printf ("    %d [label=\"(%s, %s)\"];\n",
				(int) node->algo_priv,
				node->pkg->name.c_str(),
				Architecture::to_string (node->pkg->architecture).c_str());

		/* Pre-provided packages */
		for (auto& p : node->pre_provided)
		{
			printf ("    %d -> %d [style=dotted];\n",
					(int) node->algo_priv, (int) p->algo_priv);
		}

		/* Provided packages */
		for (auto& p : node->provided)
		{
			printf ("    %d -> %d;\n",
					(int) node->algo_priv, (int) p->algo_priv);
		}
	}

	printf ("}\n");

	return true;
}


bool remove_packages (std::shared_ptr<Parameters> params)
{
	print_target (params, false);
	PackageDB pkgdb (params);

	/* Interpret the given package specifications */
	set<pair<string, int>> pkg_ids;

	for (const auto& pkg : params->operation_packages)
	{
		auto res = parse_cmd_param (*params, pkg);
		if (!res.success)
		{
			fprintf (stderr, "Unknown package description: %s (%s)\n",
					res.pkg.c_str(), res.err.c_str());
			return false;
		}

		pkg_ids.emplace (make_pair(move(res.name), res.arch));
	}

	/* Build the removal graph */
	depres::RemovalGraphBranch g = depres::build_removal_graph (pkgdb);
	depres::reduce_to_branch_to_remove (g, pkg_ids);

	/* Compute an unconfigure and rmfiles order */
	vector<depres::RemovalGraphNode*> unconfigure_order = depres::serialize_rgraph (g, false);
	vector<depres::RemovalGraphNode*> rmfiles_order = depres::serialize_rgraph (g, true);

	/* Check that all packages to be touched are in an accepted state. This is
	 * primarily for a good user experience. */
	for (auto rg_node : unconfigure_order)
	{
		int state = rg_node->pkg->state;

		if (state != PKG_STATE_CONFIGURED &&
				state != PKG_STATE_UNCONFIGURE_BEGIN &&
				state != PKG_STATE_RM_FILES_BEGIN &&
				state != PKG_STATE_POSTRM_BEGIN)
		{
			fprintf (stderr, "Package %s@%s is in a state that is not supported for removal.\n",
					rg_node->pkg->name.c_str(),
					Architecture::to_string (rg_node->pkg->architecture).c_str());

			return false;
		}
	}

	/* Load stored maintainer scripts for involved packages */
	map<std::shared_ptr<PackageMetaData>,StoredMaintainerScripts> sms_map;

	for (auto rg_node : unconfigure_order)
	{
	 	sms_map.emplace (make_pair (
					rg_node->pkg,
					StoredMaintainerScripts (params, rg_node->pkg)));
	}

	printf ("Unconfiguring packages.\n");
	for (auto rg_node : unconfigure_order)
	{
		if (rg_node->pkg->state == PKG_STATE_CONFIGURED ||
				rg_node->pkg->state == PKG_STATE_UNCONFIGURE_BEGIN)
		{
			if (!params->target_is_native())
			{
				fprintf (stderr, "Cannot unconfigure package %s@%s because the "
						"target system is not native.\n",
						rg_node->pkg->name.c_str(),
						Architecture::to_string (rg_node->pkg->architecture).c_str());

				return false;
			}

			printf (" unconfiguring package %s@%s\n",
					rg_node->pkg->name.c_str(),
					Architecture::to_string (rg_node->pkg->architecture).c_str());

			if (!ll_unconfigure_package (params, pkgdb, rg_node->pkg,
					sms_map.find(rg_node->pkg)->second))
			{
				return false;
			}
		}
	}

	printf ("Removing packages.\n");
	for (auto rg_node : rmfiles_order)
	{
		printf (" removing package %s@%s\n",
				rg_node->pkg->name.c_str(),
				Architecture::to_string (rg_node->pkg->architecture).c_str());

		if (rg_node->pkg->state == PKG_STATE_RM_FILES_BEGIN)
		{
			if (!ll_rm_files (params, pkgdb, rg_node->pkg))
				return false;
		}

		if (rg_node->pkg->state == PKG_STATE_POSTRM_BEGIN)
		{
			if (!ll_run_postrm (params, pkgdb, rg_node->pkg,
						sms_map.find(rg_node->pkg)->second))
			{
				return false;
			}
		}
	}

	return true;
}


bool ll_unconfigure_package (
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata,
		StoredMaintainerScripts& sms)
{
	if (mdata->state != PKG_STATE_CONFIGURED && mdata->state != PKG_STATE_UNCONFIGURE_BEGIN)
		throw gp_exception ("ll_unconfigure_package called with pkg in inaccepted state.");

	try
	{
		printf ("  Marking unconfiguration in db ...");
		fflush (stdout);

		mdata->state = PKG_STATE_UNCONFIGURE_BEGIN;
		pkgdb.update_state (mdata);

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}


	try
	{
		printf ("  Running unconfigure script ...");
		fflush (stdout);


		auto unconfigure = sms.get_unconfigure();
		if (unconfigure)
			run_script (params, *unconfigure);


		mdata->state = PKG_STATE_RM_FILES_BEGIN;
		pkgdb.update_state (mdata);

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}


bool ll_rm_files (
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata)
{
	if (mdata->state != PKG_STATE_RM_FILES_BEGIN)
		throw gp_exception ("ll_rm_files called with pkg in inacceptable state.");

	try
	{
		printf ("  Removing files ...");
		fflush (stdout);


		list<PackageDBFileEntry> files = pkgdb.get_files (mdata);
		list<PackageDBFileEntry> directories;

		auto iter = files.begin();
		while (iter != files.end())
		{
			if (iter->type == FILE_TYPE_DIRECTORY)
				directories.splice (directories.end(), files, iter++);
			else
				++iter;
		}

		/* Makes sure that subdirectories come first. That is, the longer paths
		 * must come first and hence be considered 'less'. */
		directories.sort (
				[](const PackageDBFileEntry& a, const PackageDBFileEntry& b) {
					return a.path.size() > b.path.size();
				});

		/* Remove files */
		for (auto& f : files)
		{
			auto s = fs::symlink_status (f.path);

			if (fs::exists (s))
			{
				/* Remove any type of file except directories, cause such a
				 * mismatch is (1) strange and (2) may lead to bigger data
				 * losses ... */
				if (!is_directory (s))
				{
					fs::remove (f.path);
				}
				else
				{
					throw gp_exception ("File \"" + f.path +
							"\" to be removed is a directory though it should not be one.");
				}
			}
		}

		/* Remove directories */
		for (auto& d : directories)
		{
			auto s = fs::symlink_status (d.path);

			if (fs::exists (s))
			{
				if (fs::is_directory (s))
				{
					if (directory_is_empty (d.path))
						fs::remove (d.path);
				}
				else
				{
					throw gp_exception ("File \"" + d.path +
							"\" to be removed is not a directory while it should be one.");
				}
			}
		}


		mdata->state = PKG_STATE_POSTRM_BEGIN;
		pkgdb.update_state (mdata);

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}


bool ll_run_postrm (
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata,
		StoredMaintainerScripts& sms)
{
	if (mdata->state != PKG_STATE_POSTRM_BEGIN)
		throw gp_exception ("ll_run_postrm called with pkg in inacceptable state.");

	try
	{
		printf ("  Running postrm script ...");
		fflush (stdout);


		auto postrm = sms.get_postrm();
		if (postrm)
			run_script (params, *postrm);

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}


	try
	{
		printf ("  Removing db tuples and stored maintainer scripts ...");
		fflush (stdout);

		pkgdb.begin();

		StoredMaintainerScripts::delete_archive (params, mdata);
		pkgdb.delete_package (mdata);

		pkgdb.commit();

		printf (COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		pkgdb.rollback();
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}
