/** This file is part of the TSClient LEGACY Package Manager
 *
 * High level installation, reinstallation and upgrading of packages */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include "installation.h"
#include "utility.h"
#include "depres.h"
#include "package_db.h"
#include "safe_console_input.h"
#include "package_provider.h"
#include "message_digest.h"

extern "C"
{
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
}

using namespace std;
namespace fs = std::filesystem;
namespace md = message_digest;


bool print_installation_graph(shared_ptr<Parameters> params)
{
	print_target(params, true);

	/* Read the package database */
	PackageDB pkgdb(params);

	vector<shared_ptr<PackageMetaData>> installed_packages =
		pkgdb.get_packages_in_state (ALL_PKG_STATES);

	/* Build the installation graph */
	vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>> new_packages;

	for (const auto& pkg : params->operation_packages)
	{
		auto res = parse_cmd_param (*params, pkg);
		if (!res.success)
		{
			fprintf (stderr, "Unknown package description: %s (%s)\n",
					res.pkg.c_str(), res.err.c_str());
			return false;
		}

		fprintf (stderr, "Additional package: %s@%s %s\n",
				res.name.c_str(),
				Architecture::to_string (res.arch).c_str(),
				res.vc ? res.vc->to_string().c_str() : " all versions");

		new_packages.emplace_back (make_pair(
					make_pair(res.name, res.arch), res.vc));
	}

	depres::ComputeInstallationGraphResult r =
		depres::compute_installation_graph(params, installed_packages, pkgdb,
				PackageProvider::create (params), new_packages, true);

	if (r.error)
	{
		fprintf (stderr, "Error: Failed to build the installation graph: %s\n",
				r.error_message.c_str());

		return false;
	}


	/* Print it. */
	printf ("digraph Dependencies {\n");

	/* Assign each node an index */
	int i = 0;

	for (auto& p : r.G)
	{
		p.second->algo_priv = i++;
	}


	for (auto& p : r.G)
	{
		shared_ptr<depres::IGNode> node = p.second;
		auto cv = node->chosen_version;
		auto instv = dynamic_pointer_cast<InstallationPackageVersion>(cv);
		auto mdata = instv->get_mdata();
		auto& iv = node->installed_version;

		if (!cv)
		{
			fprintf (stderr, "No version chosen for packages %s@%s.\n",
					mdata->name.c_str(),
					Architecture::to_string(mdata->architecture).c_str());

			return false;
		}

		string irs = node->installed_automatically ? "auto" : "manual";

		printf ("    %d [label=\"", (int) node->algo_priv);

		if (!iv)
		{
			printf ("Missing_pkg (%s, %s, %s, %s)",
					mdata->name.c_str(),
					Architecture::to_string(mdata->architecture).c_str(),
					irs.c_str(),
					cv->get_binary_version().to_string().c_str());
		}
		else if (*iv == *cv)
		{
			printf ("Present_pkg (%s, %s, %s, %s)",
					mdata->name.c_str(),
					Architecture::to_string(mdata->architecture).c_str(),
					irs.c_str(),
					cv->get_binary_version().to_string().c_str());
		}
		else
		{
			printf ("Wrong_pkg (%s, %s, %s, %s)",
					mdata->name.c_str(),
					Architecture::to_string(mdata->architecture).c_str(),
					irs.c_str(),
					cv->get_binary_version().to_string().c_str());
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
	if (!system_state_accepted_for_install (installed_packages))
		return false;

	/* Find the requested packages in a repo and resolve their dependencies
	 * recursively. I have the depres module for this task. */
	shared_ptr<PackageProvider> pprov = PackageProvider::create (params);

	vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>> new_packages;

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
		depres::compute_installation_graph (
				params, installed_packages, pkgdb, pprov, new_packages, true);

	if (comp_igraph_res.error)
	{
		fprintf (stderr, "Failed to build the installation graph: %s\n",
				comp_igraph_res.error_message.c_str());

		return false;
	}

	depres::installation_graph_t& igraph = comp_igraph_res.G;


	/* Determine an unpack and a configuration order */
	vector<depres::pkg_operation> unpack_order =
		depres::generate_installation_order_from_igraph (
			pkgdb,
			igraph,
			installed_packages,
			true);

	vector<depres::pkg_operation> configure_order =
		depres::generate_installation_order_from_igraph (
			pkgdb,
			igraph,
			installed_packages,
			false);


	if (params->verbose)
	{
		printf ("unpack order:\n");
		for (auto& op : unpack_order)
		{
			string name, op_name, version;

			switch (op.operation)
			{
				case depres::pkg_operation::INSTALL_NEW:
					name = op.ig_node->chosen_version->get_name();
					version = op.ig_node->chosen_version->get_binary_version().to_string();
					op_name = "install_new";
					break;

				case depres::pkg_operation::REMOVE:
					name = op.pkg->name;
					version = op.pkg->version.to_string();
					op_name = "remove";
					break;

				case depres::pkg_operation::CHANGE_REMOVE:
					name = op.pkg->name;
					version = op.pkg->version.to_string();
					op_name = "change_remove";
					break;

				case depres::pkg_operation::CHANGE_INSTALL:
					name = op.ig_node->chosen_version->get_name();
					version = op.ig_node->chosen_version->get_binary_version().to_string();
					op_name = "change_install";
					break;

				case depres::pkg_operation::REPLACE_REMOVE:
					name = op.pkg->name;
					version = op.pkg->version.to_string();
					op_name = "replace_remove";
					break;

				case depres::pkg_operation::REPLACE_INSTALL:
					name = op.ig_node->chosen_version->get_name();
					version = op.ig_node->chosen_version->get_binary_version().to_string();
					op_name = "replace_install";
					break;

				default:
					break;
			}

			printf ("  %s:%s - %s\n", name.c_str(), version.c_str(), op_name.c_str());
		}

		printf ("configure order:\n");
		for (auto& op : configure_order)
		{
			string name, op_name, version;

			switch (op.operation)
			{
				case depres::pkg_operation::INSTALL_NEW:
					name = op.ig_node->chosen_version->get_name();
					version = op.ig_node->chosen_version->get_binary_version().to_string();
					op_name = "install_new";
					break;

				case depres::pkg_operation::REMOVE:
					name = op.pkg->name;
					version = op.pkg->version.to_string();
					op_name = "remove";
					break;

				case depres::pkg_operation::CHANGE_REMOVE:
					name = op.pkg->name;
					version = op.pkg->version.to_string();
					op_name = "change_remove";
					break;

				case depres::pkg_operation::CHANGE_INSTALL:
					name = op.ig_node->chosen_version->get_name();
					version = op.ig_node->chosen_version->get_binary_version().to_string();
					op_name = "change_install";
					break;

				case depres::pkg_operation::REPLACE_REMOVE:
					name = op.pkg->name;
					version = op.pkg->version.to_string();
					op_name = "replace_remove";
					break;

				case depres::pkg_operation::REPLACE_INSTALL:
					name = op.ig_node->chosen_version->get_name();
					version = op.ig_node->chosen_version->get_binary_version().to_string();
					op_name = "replace_install";
					break;

				default:
					break;
			}

			printf ("  %s:%s - %s\n", name.c_str(), version.c_str(), op_name.c_str());
		}

		printf ("\n");
	}


	/* Set package states of new packages to wanted. They are invalid now. */
	for (auto& op : unpack_order)
	{
		if (op.ig_node)
		{
			auto mdata = dynamic_pointer_cast<InstallationPackageVersion>(
					op.ig_node->chosen_version)
				->get_mdata();

			if (mdata->state == PKG_STATE_INVALID)
				mdata->state = PKG_STATE_WANTED;
		}
	}


	/* See what operations we have got from drepres. */
	bool remove_pkgs = false;
	bool change_pkgs = false;

	for (auto& op : unpack_order)
	{
		switch (op.operation)
		{
			case depres::pkg_operation::INSTALL_NEW:
				break;

			case depres::pkg_operation::REMOVE:
				remove_pkgs = true;
				break;

			default:
				change_pkgs = true;
				break;
		}
	}

	/* Always ask for confirmation if operations will be performed. */
	if (unpack_order.size())
	{
		vector<pair<const string,const int>> to_remove;
		set<pair<const string,const int>> to_change;
		vector<pair<const string,const int>> to_install;

		for (auto& op : unpack_order)
		{
			switch (op.operation)
			{
				case depres::pkg_operation::INSTALL_NEW:
				case depres::pkg_operation::REPLACE_INSTALL:
					to_install.push_back (make_pair (
								op.ig_node->chosen_version->get_name(),
								op.ig_node->chosen_version->get_architecture()));
					break;

				case depres::pkg_operation::REMOVE:
				case depres::pkg_operation::REPLACE_REMOVE:
					to_remove.push_back (make_pair (
								op.pkg->name,
								op.pkg->architecture));
					break;

				case depres::pkg_operation::CHANGE_REMOVE:
					to_change.insert (make_pair (
								op.pkg->name,
								op.pkg->architecture));
					break;

				case depres::pkg_operation::CHANGE_INSTALL:
					to_change.insert (make_pair (
								op.ig_node->chosen_version->get_name(),
								op.ig_node->chosen_version->get_architecture()));
					break;

				default:
					break;
			}
		}

		/* Some packages' old version will be replaced by multiple new package
		 * versions including a new version of itself. Hence the new version
		 * will be CHANGE_INSTALLed and the old version be REPLACE_REMOVEd.
		 * Similar situation can happen with REPLACE_INSTALL. As this is only
		 * for user information, only list these packages under 'change'. */

		bool have_pkgs = false;
		for (auto& p : to_remove)
		{
			if (to_change.find(p) != to_change.end())
				continue;

			if (!have_pkgs)
			{
				printf ("These packages will be removed:\n");
				have_pkgs = true;
			}

			printf ("  %s@%s\n", p.first.c_str(), Architecture::to_string (p.second).c_str());
		}

		if (have_pkgs)
			printf ("\n");


		if (!to_change.empty())
		{
			printf ("These packages will be changed:\n");
			for (auto& p : to_change)
				printf ("  %s@%s\n", p.first.c_str(), Architecture::to_string (p.second).c_str());

			printf ("\n");
		}


		have_pkgs = false;
		for (auto& p : to_install)
		{
			if (to_change.find(p) != to_change.end())
				continue;

			if (!have_pkgs)
			{
				printf ("These packages will be installed:\n");
				have_pkgs = true;
			}

			printf ("  %s@%s\n", p.first.c_str(), Architecture::to_string (p.second).c_str());
		}

		if (have_pkgs)
			printf ("\n");


		if (!params->assume_yes)
		{
			printf ("Continue? ");
			auto c = safe_query_user_input ("Yn");
			if (c != 'y')
			{
				printf ("User aborted.\n");
				return false;
			}
		}
	}


	/* A pointer feels smoother here than an optional. */
	unique_ptr<FileTrie<vector<PackageMetaData*>>> current_trie;

	if (change_pkgs || remove_pkgs)
	{
		current_trie = make_unique<FileTrie<vector<PackageMetaData*>>>();

		/* Read files and build a file trie. */
		for (auto pkg : installed_packages)
		{
			for (auto& file : pkgdb.get_files (pkg))
			{
				auto h = current_trie->find_directory (file.path);

				if (!h)
				{
					current_trie->insert_directory (file.path);
					h = current_trie->find_directory (file.path);
				}

				/* Should not be too many and spares overhead of set */
				if (find (h->data.begin(), h->data.end(), pkg.get()) == h->data.end())
					h->data.push_back (pkg.get());
			}
		}
	}


	/* Fetch missing archives. This is useful if continuing after having aborted
	 * an installation. */
	for (auto op : unpack_order)
	{
		if (
				op.operation == depres::pkg_operation::INSTALL_NEW ||
				op.operation == depres::pkg_operation::CHANGE_INSTALL ||
				op.operation == depres::pkg_operation::REPLACE_INSTALL)
		{
			depres::IGNode* ig_node = op.ig_node;
			auto installation_package_version = dynamic_pointer_cast<InstallationPackageVersion>(
					ig_node->chosen_version);

			auto mdata = installation_package_version->get_mdata();

			/* Check for states that need archives */
			if (
					mdata->state == PKG_STATE_WANTED ||
					mdata->state == PKG_STATE_PREINST_BEGIN ||
					mdata->state == PKG_STATE_PREINST_CHANGE ||
					mdata->state == PKG_STATE_UNPACK_BEGIN ||
					mdata->state == PKG_STATE_UNPACK_CHANGE)
			{
				if (!dynamic_pointer_cast<ProvidedPackage>(installation_package_version))
				{
					auto installed_package_version =
						dynamic_pointer_cast<depres::InstalledPackageVersion>(installation_package_version);

					installed_package_version->provided_package = pprov->get_package (
							mdata->name, mdata->architecture, mdata->version);

					if (!installed_package_version->provided_package)
					{
						fprintf (stderr,
								"Could not fetch package %s@%s:%s\n"
								"    (currently installed but its change requires "
								"the package archive to be present).\n",
								mdata->name.c_str(),
								Architecture::to_string (mdata->architecture).c_str(),
								mdata->version.to_string().c_str());

						return false;
					}
				}
			}
		}
	}


	/* If required, low-level unconfigure the packages to remove / change /
	 * replace */
	if (remove_pkgs || change_pkgs)
	{
		printf ("Unconfiguring old packages.\n");

		for (auto op : configure_order)
		{
			if (
					op.operation != depres::pkg_operation::REMOVE &&
					op.operation != depres::pkg_operation::CHANGE_REMOVE &&
					op.operation != depres::pkg_operation::REPLACE_REMOVE)
			{
				continue;
			}

			shared_ptr<PackageMetaData> mdata = op.pkg;
			bool change = op.operation != depres::pkg_operation::REMOVE;

			if (
					mdata->state == PKG_STATE_CONFIGURED ||
					mdata->state == PKG_STATE_UNCONFIGURE_BEGIN ||
					mdata->state == PKG_STATE_UNCONFIGURE_CHANGE)
			{
				if (!params->target_is_native())
				{
					fprintf (stderr, "Cannot unconfigure packages because the target system is not native.\n");
					return false;
				}

				printf ("ll unconfiguring package %s@%s\n", mdata->name.c_str(),
						Architecture::to_string (mdata->architecture).c_str());

				/* Integrity protection */
				if (change && mdata->state == PKG_STATE_UNCONFIGURE_BEGIN)
				{
					fprintf (stderr, "Depres requested a change but the package is not in a change state.\n");
					return false;
				}
				else if (!change && mdata->state == PKG_STATE_UNCONFIGURE_CHANGE)
				{
					fprintf (stderr, "Depres did not request a change but the package is in a change state.\n");
					return false;
				}

				StoredMaintainerScripts sms(params, mdata);

				if (change)
				{
					if (!ll_unconfigure_package (params, pkgdb, mdata, sms, true))
					{
						return false;
					}
				}
				else
				{
					if (!ll_unconfigure_package (params, pkgdb, mdata, sms, false))
					{
						return false;
					}
				}
			}
		}
	}


	/* Low-level unpack the new packages and run their preinst scripts */
	printf ("Unpacking packages.\n");

	for (auto op : unpack_order)
	{
		if (
				op.operation != depres::pkg_operation::INSTALL_NEW &&
				op.operation != depres::pkg_operation::CHANGE_INSTALL &&
				op.operation != depres::pkg_operation::REPLACE_INSTALL)
		{
			continue;
		}

		depres::IGNode* ig_node = op.ig_node;
		auto installation_package_version = dynamic_pointer_cast<InstallationPackageVersion>(
				ig_node->chosen_version);

		shared_ptr<PackageMetaData> mdata = installation_package_version->get_mdata();
		shared_ptr<ProvidedPackage> pp = dynamic_pointer_cast<ProvidedPackage>(ig_node->chosen_version);
		if (!pp)
		{
			/* Must be an InstalledPackageVersion */
			pp = dynamic_pointer_cast<depres::InstalledPackageVersion>(ig_node->chosen_version)
				->provided_package;
		}

		bool change = op.operation != depres::pkg_operation::INSTALL_NEW;

		/* Potentially change installation reason */
		if (ig_node->installed_automatically &&
				mdata->installation_reason == INSTALLATION_REASON_MANUAL)
		{
			if (!ll_change_installation_reason (
						params,
						pkgdb,
						mdata,
						INSTALLATION_REASON_AUTO))
			{
				return false;
			}
		}
		else if (!ig_node->installed_automatically &&
				mdata->installation_reason == INSTALLATION_REASON_AUTO)
		{
			if (!ll_change_installation_reason (
						params,
						pkgdb,
						mdata,
						INSTALLATION_REASON_MANUAL))
			{
				return false;
			}
		}

		/* Perform ll operations of the installation procedure */
		if (
				mdata->state == PKG_STATE_WANTED ||
				mdata->state == PKG_STATE_PREINST_BEGIN ||
				mdata->state == PKG_STATE_UNPACK_BEGIN ||
				mdata->state == PKG_STATE_PREINST_CHANGE ||
				mdata->state == PKG_STATE_UNPACK_CHANGE)
		{
			printf ("ll unpacking package %s@%s\n", mdata->name.c_str(),
					Architecture::to_string (mdata->architecture).c_str());

			/* Set installation reason before storing a package in the db (this
			 * does not change the reason, only sets it if it has not been set -
			 * changing would have been performed before. */
			mdata->installation_reason = ig_node->installed_automatically ?
				INSTALLATION_REASON_AUTO : INSTALLATION_REASON_MANUAL;

			/* Integrity protection */
			if (change && mdata->state == PKG_STATE_PREINST_BEGIN)
			{
				fprintf (stderr, "Depres requested a change but the package is not in a change state.\n");
				return false;
			}
			else if (!change && mdata->state == PKG_STATE_PREINST_CHANGE)
			{
				fprintf (stderr, "Depres did not request a change but the package is in a change state.\n");
				return false;
			}

			/* ll preinst */
			if ((change && mdata->state == PKG_STATE_WANTED) ||
					(mdata->state == PKG_STATE_PREINST_CHANGE))
			{
				if (!ll_run_preinst (params, pkgdb, mdata, pp, true, current_trie.get()))
					return false;
			}
			else if (mdata->state == PKG_STATE_WANTED || mdata->state == PKG_STATE_PREINST_BEGIN)
			{
				if (!ll_run_preinst (params, pkgdb, mdata, pp, false, current_trie.get()))
					return false;
			}

			/* Integrity protection */
			if (change && mdata->state == PKG_STATE_UNPACK_BEGIN)
			{
				fprintf (stderr, "Depres requested a change but the package is not in a change state.\n");
				return false;
			}
			else if (!change && mdata->state == PKG_STATE_UNPACK_CHANGE)
			{
				fprintf (stderr, "Depres did not request a change but the package is in a change state.\n");
				return false;
			}

			/* ll unpack */
			if (mdata->state == PKG_STATE_UNPACK_CHANGE)
			{
				if (!ll_unpack (params, pkgdb, mdata, pp, true, current_trie.get()))
					return false;
			}
			else if (mdata->state == PKG_STATE_UNPACK_BEGIN)
			{
				if (!ll_unpack (params, pkgdb, mdata, pp, false, current_trie.get()))
					return false;
			}
					
		}
	}


	/* Low-level remove the files of old packages + run postrm. */
	if (remove_pkgs || change_pkgs)
	{
		printf ("Removing packages.\n");

		for (auto op : unpack_order)
		{
			if (
					op.operation != depres::pkg_operation::REMOVE &&
					op.operation != depres::pkg_operation::CHANGE_REMOVE &&
					op.operation != depres::pkg_operation::REPLACE_REMOVE)
			{
				continue;
			}

			shared_ptr<PackageMetaData> mdata = op.pkg;
			bool change = op.operation != depres::pkg_operation::REMOVE;

			if (
					mdata->state == PKG_STATE_RM_FILES_BEGIN ||
					mdata->state == PKG_STATE_POSTRM_BEGIN ||
					mdata->state == PKG_STATE_WAIT_NEW_UNPACKED ||
					mdata->state == PKG_STATE_RM_FILES_CHANGE ||
					mdata->state == PKG_STATE_POSTRM_CHANGE)
			{
				printf ("ll removing package %s@%s\n", mdata->name.c_str(),
						Architecture::to_string (mdata->architecture).c_str());

				/* Integrity protection */
				if (change && mdata->state == PKG_STATE_RM_FILES_BEGIN)
				{
					fprintf (stderr, "Depres requested a change but the package is not in a change state.\n");
					return false;
				}
				else if (!change && (
							mdata->state == PKG_STATE_WAIT_NEW_UNPACKED ||
							mdata->state == PKG_STATE_RM_FILES_CHANGE))
				{
					fprintf (stderr, "Depres did not request a change but the package is in a change state.\n");
					return false;
				}

				/* ll remove files */
				if (mdata->state == PKG_STATE_WAIT_NEW_UNPACKED || mdata->state == PKG_STATE_RM_FILES_CHANGE)
				{
					if (!ll_rm_files (params, pkgdb, mdata, true, *current_trie))
						return false;
				}
				else if (mdata->state == PKG_STATE_RM_FILES_BEGIN)
				{
					if (!ll_rm_files (params, pkgdb, mdata, false, *current_trie))
						return false;
				}

				/* Integrity protection */
				if (change && mdata->state == PKG_STATE_POSTRM_BEGIN)
				{
					fprintf (stderr, "Depres requested a change but the package is not in a change state.\n");
					return false;
				}
				else if (!change && mdata->state == PKG_STATE_POSTRM_CHANGE)
				{
					fprintf (stderr, "Depres did not request a change but the package is in a change state.\n");
					return false;
				}

				/* ll postrm */
				StoredMaintainerScripts sms(params, mdata);

				if (mdata->state == PKG_STATE_POSTRM_CHANGE)
				{
					if (!ll_run_postrm (params, pkgdb, mdata, sms, true))
					{
						return false;
					}
				}
				else if (mdata->state == PKG_STATE_POSTRM_BEGIN)
				{
					if (!ll_run_postrm (params, pkgdb, mdata, sms, false))
					{
						return false;
					}
				}
			}
		}
	}


	/* Low-level configure the new packages */
	if (params->target_is_native())
	{
		printf ("Configuring packages.\n");

		for (auto op : configure_order)
		{
			if (
					op.operation != depres::pkg_operation::INSTALL_NEW &&
					op.operation != depres::pkg_operation::CHANGE_INSTALL &&
					op.operation != depres::pkg_operation::REPLACE_INSTALL)
			{
				continue;
			}

			depres::IGNode* ig_node = op.ig_node;
			auto installation_package_version = dynamic_pointer_cast<InstallationPackageVersion>(
					ig_node->chosen_version);
			auto mdata = installation_package_version->get_mdata();
			bool change = op.operation != depres::pkg_operation::INSTALL_NEW;

			if (
					mdata->state == PKG_STATE_CONFIGURE_BEGIN ||
					mdata->state == PKG_STATE_WAIT_OLD_REMOVED ||
					mdata->state == PKG_STATE_CONFIGURE_CHANGE)
			{
				/* Get provided package and optionally stored maintainer
				 * scripts. */
				auto pp = dynamic_pointer_cast<ProvidedPackage>(ig_node->chosen_version);

				/* Note: Since the configuration happens after ll installation,
				 * sms will be present if the package version is an installed
				 * package. Hence it is more atomic to use the stored maintainer
				 * scripts of installed package versions, because newly provided
				 * package versions may have changed (if someone published a
				 * different archive with the same version). In fact, the
				 * InstalledPackageVersion will not have a ProvidedPackage in
				 * some of those cases (only if installation has been
				 * interrupted during preinst or unpack). */
				shared_ptr<StoredMaintainerScripts> sms;
				if (!pp)
					sms = make_shared<StoredMaintainerScripts>(params, mdata);

				printf ("ll configuring package %s@%s\n",
						mdata->name.c_str(),
						Architecture::to_string (mdata->architecture).c_str());

				/* Integrity protection */
				if (change && mdata->state == PKG_STATE_CONFIGURE_BEGIN)
				{
					fprintf (stderr, "Depres requested a change but the package is not in a change state.\n");
					return false;
				}
				else if (!change && (
							mdata->state == PKG_STATE_CONFIGURE_CHANGE ||
							mdata->state == PKG_STATE_WAIT_OLD_REMOVED))
				{
					fprintf (stderr, "Depres did not request a change but the package is in a change state.\n");
					return false;
				}

				if (
						mdata->state == PKG_STATE_WAIT_OLD_REMOVED ||
						mdata->state == PKG_STATE_CONFIGURE_CHANGE)
				{
					if (!ll_configure_package (params, pkgdb, mdata,
								pp, sms, true))
						return false;
				}
				else if (mdata->state == PKG_STATE_CONFIGURE_BEGIN)
				{
					if (!ll_configure_package (params, pkgdb, mdata,
								pp, sms, false))
						return false;
				}
			}
		}
	}
	else
	{
		printf ("Not configuring packages because the target is not native.\n");
	}

	return execute_triggers (params, pkgdb);
}


bool ll_run_preinst (
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata,
		shared_ptr<ProvidedPackage> pp,
		bool change,
		FileTrie<vector<PackageMetaData*>>* current_trie)
{
	if (!(
				(mdata->state == PKG_STATE_WANTED) ||
				(!change && mdata->state == PKG_STATE_PREINST_BEGIN) ||
				(change && mdata->state == PKG_STATE_PREINST_CHANGE)))
		throw gp_exception ("ll_run_preinst called with pkg in inacceptable state.");

	if (mdata->state == PKG_STATE_WANTED)
	{
		/* Look for existing files and eventually adopt them */
		printf_verbose_flush (params, "  Looking for existing files ...");

		try
		{
			auto files = pp->get_file_list();
			auto config_files = pp->get_config_files();

			bool first = true;

			for (const auto& file : *files)
			{
				stringstream ss;

				/* Augment file trie if one is specified and skip the files from old
				 * packages if change semantics are requested. */
				if (current_trie)
				{
					auto h = current_trie->find_directory (file.path);

					if (!h)
					{
						current_trie->insert_directory (file.path);
						h = current_trie->find_directory (file.path);
					}

					if (find (h->data.begin(), h->data.end(), mdata.get()) == h->data.end())
						h->data.push_back (mdata.get());

					/* Now this file will be registered for at least this package.
					 * If it belongs to other packages as well, these must be old
					 * packages (otherwise there would be a conflict). */
					if (change && h->data.size() > 1)
						continue;
				}

				/* Adoption semantics */
				if (!file.non_existent_or_matches (params->target, &ss))
				{
					/* Skip config files , since the config file logic will handle
					 * them later. */
					if (binary_search(config_files->begin(), config_files->end(), file.path))
						continue;

					if (first)
					{
						printf ("\n\n");
						first = false;
					}

					printf ("File \"%s\" differs from the one in the package: %s",
							file.path.c_str(), ss.str().c_str());

					if (!params->adopt_all)
					{
						printf ("Adopt it anyway? ");

						auto c = safe_query_user_input ("yN");
						if (c != 'y')
							throw gp_exception ("User aborted.");
					}

					printf ("Adopting \"%s\", which differs.\n", file.path.c_str());
				}
			}

			printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
		}
		catch (exception& e)
		{
			printf (COLOR_RED " failed" COLOR_NORMAL "\n");
			printf ("%s\n", e.what());
			return false;
		}


		/* Create DB tuples to make the operation transactional, store maintainer
		 * scripts, and store the list of config files. */
		printf_verbose_flush (params, "  Creating db tuples and storing maintainer scripts ...");

		try
		{
			pkgdb.begin();

			mdata->state = change ? PKG_STATE_PREINST_CHANGE : PKG_STATE_PREINST_BEGIN;

			pkgdb.update_or_create_package (mdata);
			pkgdb.set_dependencies (mdata);
			pkgdb.set_files (mdata, pp->get_file_list());
			pkgdb.set_config_files (mdata, pp->get_config_files());

			StoredMaintainerScripts sms (params, mdata,
					pp->get_preinst(),
					pp->get_configure(),
					pp->get_unconfigure(),
					pp->get_postrm());

			sms.write();

			pkgdb.commit();

			printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
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
	}


	/* Run preinst if available */
	try
	{
		printf_verbose_flush (params, "  Running preinst script ...");

		auto preinst = pp->get_preinst();
		if (preinst)
		{
			if (change)
				run_script (params, *preinst, "change");
			else
				run_script (params, *preinst);
		}

		mdata->state = change ? PKG_STATE_UNPACK_CHANGE : PKG_STATE_UNPACK_BEGIN;
		pkgdb.update_state (mdata);

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}


/* If change is true, current_trie must be provided. */
bool ll_unpack (
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata,
		shared_ptr<ProvidedPackage> pp,
		bool change,
		FileTrie<vector<PackageMetaData*>>* current_trie)
{
	if (!(
				(!change && mdata->state == PKG_STATE_UNPACK_BEGIN) ||
				(change && mdata->state == PKG_STATE_UNPACK_CHANGE)))
		throw gp_exception ("ll_unpack called with pkg in inacceptable state.");

	/* Unpack the archive */
	try
	{
		printf_verbose_flush (params, "  Unpacking the package's archive ...");

		if (pp->has_archive())
		{
			vector<string> excluded_paths;

			/* Check if config files have to be excluded. Skip the check if the
			 * package has no config files, because most packages probably don't
			 * have config files. */
			auto cfiles = pp->get_config_files();
			if (!cfiles->empty())
			{
				auto files = pp->get_file_list();

				for (auto& cfile : *cfiles)
				{
					bool handled = false;
					if (change)
					{
						/* Check if the config file was part of other packages
						 * or a version of this package before and if it has
						 * been changed/deleted. If so, do not overwrite it. If
						 * the file is not part of an installed package version,
						 * use the semantics for new config files below.
						 *
						 * Ignore config files which are not in the archive. */
						auto h = current_trie->find_directory (cfile);
						if (h)
						{
							for (auto other : h->data)
							{
								/* The trie's data can only contain two packages
								 * - the previous one and this one. */
								if (other == mdata.get())
									continue;

								auto file = pkgdb.get_file (other, cfile);
								if (!file)
								{
									throw gp_exception ("ll_unpack: Config file `" + cfile +
											"' is in file trie but not in the db anymore.");
								}

								/* Compare the file on the system to the
								 * information from the the database. If they
								 * differ, exclude the config file. */
								if (config_file_differs (params, *file))
								{
									printf ("    Config file `%s' was changed.\n", cfile.c_str());

									if (!params->adopt_all)
									{
										printf ("    Overwrite it with the packaged version? ");
										if (safe_query_user_input("yN") == 'n')
											excluded_paths.push_back(cfile);
									}
									else
									{
										printf ("    Overwriting because of '--adopt-all\n");
									}
								}

								handled = true;
								break;
							}
						}
					}

					if (!handled)
					{
						/* The file was not part of a previous package version.
						 *
						 * If the config file exists already and differs, ask
						 * the user if it should be overwritten or not. In case
						 * 'adopt_all' has been enabled, just overwrite it. */
						auto inew = files->find (DummyFileRecord(cfile));
						stringstream ss;

						/* Ignore config files not in the archive. */
						if (inew == files->end())
							continue;

						if (!inew->non_existent_or_matches (params->target, &ss))
						{
							printf ("    Config file `%s' already present and differs: %s",
									cfile.c_str(), ss.str().c_str());

							if (!params->adopt_all)
							{
								printf ("    Overwrite it with the packaged version? ");
								if (safe_query_user_input("yn") == 'n')
									excluded_paths.push_back(cfile);
							}
							else
							{
								printf ("    Overwriting because of '--adopt-all\n");
							}
						}
					}
				}
			}

			pp->unpack_archive_to_directory (params->target, &excluded_paths);
		}

		mdata->state = change ? PKG_STATE_WAIT_OLD_REMOVED : PKG_STATE_CONFIGURE_BEGIN;
		pkgdb.update_state (mdata);

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
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
		shared_ptr<StoredMaintainerScripts> sms,
		bool change)
{

	if (!pp && !sms)
		throw gp_exception ("ll_configure: Neither pp nor sms specified.");

	if (!(
				(!change && mdata->state == PKG_STATE_CONFIGURE_BEGIN) ||
				(change && mdata->state == PKG_STATE_WAIT_OLD_REMOVED) ||
				(change && mdata->state == PKG_STATE_CONFIGURE_CHANGE)))
		throw gp_exception ("ll_configure called with pkg in inacceptable state.");


	/* If in a waiting state, mark the package first. */
	if (mdata->state == PKG_STATE_WAIT_OLD_REMOVED)
	{
		try
		{
			printf_verbose_flush (params, "  Moving package from wait_old_removed ...");

			mdata->state = PKG_STATE_CONFIGURE_CHANGE;
			pkgdb.update_state (mdata);

			printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
		}
		catch (exception& e)
		{
			printf (COLOR_RED " failed" COLOR_NORMAL "\n");
			printf ("%s\n", e.what());
			return false;
		}
	}


	try
	{
		printf_verbose_flush (params, "  Running configure script ...");

		auto configure = pp ? pp->get_configure() : sms->get_configure();
		if (configure)
		{
			if (change)
				run_script (params, *configure, "change");
			else
				run_script (params, *configure);
		}

		activate_package_triggers (params, pkgdb, mdata);


		mdata->state = PKG_STATE_CONFIGURED;
		pkgdb.update_state (mdata);

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
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
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata,
		char reason)
{
	try
	{
		printf_verbose_flush (params, "  Changing installation reason ...");

		mdata->installation_reason = reason;
		pkgdb.update_installation_reason (mdata);

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}

	return true;
}


bool system_state_accepted_for_install (vector<shared_ptr<PackageMetaData>> installed_packages)
{
	bool errors_found = false;

	for (auto mdata : installed_packages)
	{
		if (
				mdata->state != PKG_STATE_WANTED &&
				mdata->state != PKG_STATE_PREINST_BEGIN &&
				mdata->state != PKG_STATE_UNPACK_BEGIN &&
				mdata->state != PKG_STATE_CONFIGURE_BEGIN &&
				mdata->state != PKG_STATE_CONFIGURED)
		{
			fprintf (stderr, "System is not in a clean state. Package "
					"%s@%s:%s is not in an accepted state.\n",
					mdata->name.c_str(),
					Architecture::to_string (mdata->architecture).c_str(),
					mdata->version.to_string().c_str());

			errors_found = true;
		}
	}

	return !errors_found;
}


/***************************** Removing packages ******************************/
bool print_removal_graph (shared_ptr<Parameters> params, bool autoremove)
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
	depres::RemovalGraphBranch g = depres::build_removal_graph (
			pkgdb.get_packages_in_state (ALL_PKG_STATES));

	/* If any packages are specified, reduce it to the branch to remove */
	if (!pkg_ids.empty() || autoremove)
		depres::reduce_to_branch_to_remove (g, pkg_ids, autoremove);

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


bool list_reverse_dependencies (shared_ptr<Parameters> params)
{
	print_target (params);

	/* If no packages are specifies, there's nothing to do. */
	if (params->operation_packages.empty())
		return true;

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


	auto all_packages = pkgdb.get_packages_in_state (ALL_PKG_STATES);

	/* Check that all queried packages are installed. */
	set<pair<string, int>> installed_pkg_ids;

	for (auto mdata : all_packages)
	{
		auto i = pkg_ids.find (make_pair (mdata->name, mdata->architecture));
		if (i != pkg_ids.end())
			installed_pkg_ids.insert (make_pair (mdata->name, mdata->architecture));
	}

	if (installed_pkg_ids.size() != pkg_ids.size())
	{
		for (auto& id : pkg_ids)
		{
			if (installed_pkg_ids.find (id) == installed_pkg_ids.end())
			{
				fprintf (stderr, "Package %s@%s is not installed.\n",
						id.first.c_str(),
						Architecture::to_string (id.second).c_str());
			}
		}

		return false;
	}


	/* Build the removal graph */
	depres::RemovalGraphBranch g = depres::build_removal_graph (all_packages);

	/* Reduce it to the branch to remove */
	depres::reduce_to_branch_to_remove (g, pkg_ids, false);

	/* Sort print the the packages from the removal graph. */
	vector<pair<string, int>> pkgs;

	for (auto node : g.V)
	{
		/* Don't include the packages for which the user asked to get the
		 * reverse dependencies. The removal graph will contain them, of course.
		 * */
		if (pkg_ids.find (make_pair (node->pkg->name, node->pkg->architecture)) == pkg_ids.end())
			pkgs.push_back (make_pair (node->pkg->name, node->pkg->architecture));
	}

	sort (pkgs.begin(), pkgs.end());
	
	for (auto& pkg : pkgs)
		printf ("%s@%s\n", pkg.first.c_str(), Architecture::to_string(pkg.second).c_str());

	return true;
}


bool remove_packages (std::shared_ptr<Parameters> params, bool autoremove)
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


	auto installed_packages = pkgdb.get_packages_in_state (ALL_PKG_STATES);

	/* Add packages that are in a state that belongs to removing them. */
	for (auto mdata : installed_packages)
	{
		if (
				mdata->state == PKG_STATE_UNCONFIGURE_BEGIN ||
				mdata->state == PKG_STATE_RM_FILES_BEGIN ||
				mdata->state == PKG_STATE_POSTRM_BEGIN
		   )
		{
			pkg_ids.insert (make_pair (mdata->name, mdata->architecture));
		}
	}

	/* Build the removal graph */
	depres::RemovalGraphBranch g = depres::build_removal_graph (installed_packages);
	depres::reduce_to_branch_to_remove (g, pkg_ids, autoremove);

	/* If nothing has to be removed, abort here. */
	if (g.V.empty())
		return true;

	/* Ask the user if all these packages shall be removed. */
	printf ("The following packages will be removed:\n");

	for (auto v : g.V)
	{
		printf ("  %s@%s:%s\n",
				v->pkg->name.c_str(),
				Architecture::to_string (v->pkg->architecture).c_str(),
				v->pkg->version.to_string().c_str());
	}

	if (!params->assume_yes)
	{
		printf ("\nContinue? ");
		if (safe_query_user_input ("Yn") != 'y')
		{
			printf ("User aborted.\n");
			return false;
		}
	}

	/* High-level remove the packages in the removal graph. */
	if (!hl_remove_packages (params, pkgdb, installed_packages, g))
		return false;

	return execute_triggers (params, pkgdb);
}


bool hl_remove_packages (
		shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		vector<shared_ptr<PackageMetaData>>& installed_packages,
		depres::RemovalGraphBranch& g)
{
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

	/* Build a trie of all directories that are currently installed on the
	 * system. */
	FileTrie<vector<PackageMetaData*>> current_trie;

	for (auto pkg : installed_packages)
	{
		for (auto& file : pkgdb.get_files (pkg))
		{
			if (file.type == FILE_TYPE_DIRECTORY)
			{
				auto h = current_trie.find_directory (file.path);

				if (!h)
				{
					current_trie.insert_directory (file.path);
					h = current_trie.find_directory (file.path);
				}

				/* Should not be too many and spares overhead of set */
				if (find (h->data.begin(), h->data.end(), pkg.get()) == h->data.end())
					h->data.push_back (pkg.get());
			}
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

			printf ("ll unconfiguring package %s@%s\n",
					rg_node->pkg->name.c_str(),
					Architecture::to_string (rg_node->pkg->architecture).c_str());

			if (!ll_unconfigure_package (params, pkgdb, rg_node->pkg,
					sms_map.find(rg_node->pkg)->second, false))
			{
				return false;
			}
		}
	}

	printf ("Removing packages.\n");
	for (auto rg_node : rmfiles_order)
	{
		printf ("ll removing package %s@%s\n",
				rg_node->pkg->name.c_str(),
				Architecture::to_string (rg_node->pkg->architecture).c_str());

		if (rg_node->pkg->state == PKG_STATE_RM_FILES_BEGIN)
		{
			if (!ll_rm_files (params, pkgdb, rg_node->pkg, false, current_trie))
				return false;
		}

		if (rg_node->pkg->state == PKG_STATE_POSTRM_BEGIN)
		{
			if (!ll_run_postrm (params, pkgdb, rg_node->pkg,
						sms_map.find(rg_node->pkg)->second, false))
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
		StoredMaintainerScripts& sms,
		bool change)
{
	if (!(
				mdata->state == PKG_STATE_CONFIGURED ||
				(!change && mdata->state == PKG_STATE_UNCONFIGURE_BEGIN) ||
				(change && mdata->state == PKG_STATE_UNCONFIGURE_CHANGE)))
		throw gp_exception ("ll_unconfigure_package called with pkg in inaccepted state.");

	try
	{
		printf_verbose_flush (params, "  Marking unconfiguration in db ...");

		mdata->state = change ? PKG_STATE_UNCONFIGURE_CHANGE : PKG_STATE_UNCONFIGURE_BEGIN;
		pkgdb.update_state (mdata);

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}


	try
	{
		printf_verbose_flush (params, "  Running unconfigure script ...");

		auto unconfigure = sms.get_unconfigure();
		if (unconfigure)
		{
			if (change)
				run_script (params, *unconfigure, "change");
			else
				run_script (params, *unconfigure);
		}

		activate_package_triggers (params, pkgdb, mdata);


		mdata->state = change ? PKG_STATE_WAIT_NEW_UNPACKED : PKG_STATE_RM_FILES_BEGIN;
		pkgdb.update_state (mdata);

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
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
		shared_ptr<PackageMetaData> mdata,
		bool change,
		FileTrie<vector<PackageMetaData*>>& current_trie)
{
	if (!(
				(!change && mdata->state == PKG_STATE_RM_FILES_BEGIN) ||
				(change && mdata->state == PKG_STATE_RM_FILES_CHANGE) ||
				(change && mdata->state == PKG_STATE_WAIT_NEW_UNPACKED)))
		throw gp_exception ("ll_rm_files called with pkg in inacceptable state.");


	/* If in a waiting state, mark the package first. */
	if (mdata->state == PKG_STATE_WAIT_NEW_UNPACKED)
	{
		try
		{
			printf_verbose_flush (params, "  Moving package from wait_new_unpacked ...");

			mdata->state = PKG_STATE_RM_FILES_CHANGE;
			pkgdb.update_state (mdata);

			printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
		}
		catch (exception& e)
		{
			printf (COLOR_RED " failed" COLOR_NORMAL "\n");
			printf ("%s\n", e.what());
			return false;
		}
	}


	try
	{
		printf_verbose_flush (params, "  Removing files ...");

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

		/* Remove references of this package to files in the file trie. If the
		 * files are references by other packages and change semantics are used,
		 * the file is removed from the list of files to delete. */
		auto fiter = files.begin();
		while (fiter != files.end())
		{
			auto h = current_trie.find_directory (fiter->path);
			if (h)
			{
				auto ref = find (h->data.begin(), h->data.end(), mdata.get());
				if (ref != h->data.end())
					h->data.erase (ref);

				if (h->data.empty())
					current_trie.remove_element (fiter->path);
				else if (change)
					files.erase (fiter++);
			}
			else
			{
				fiter++;
			}
		}

		/* Do the same thing for directories, but directories may always belong
		 * to multiple packages and hence must only be removed if no other
		 * packages reference them, even if semantics are not used. */
		auto diter = directories.begin();
		while (diter != directories.end())
		{
			auto h = current_trie.find_directory (diter->path);
			if (h)
			{
				auto ref = find (h->data.begin(), h->data.end(), mdata.get());
				if (ref != h->data.end())
					h->data.erase (ref);

				if (h->data.empty())
					current_trie.remove_element (diter->path);
				else
					directories.erase (diter++);
			}
			else
			{
				diter++;
			}
		}


		/* Makes sure that subdirectories come first. That is, the longer paths
		 * must come first and hence be considered 'less'. */
		directories.sort (
				[](const PackageDBFileEntry& a, const PackageDBFileEntry& b) {
					return a.path.size() > b.path.size();
				});

		/* Remove files */
		auto cfiles = pkgdb.get_config_files (mdata);

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
					/* Do not remove config files that have been changed. */
					if (binary_search (cfiles.begin(), cfiles.end(), f.path))
					{
						auto file = pkgdb.get_file(mdata.get(), f.path);
						if (file)
						{
							if (config_file_differs(params, *file))
							{
								printf ("    Not deleting changed config file `%s'.\n",
										f.path.c_str());

								continue;
							}
						}
						else
						{
							throw gp_exception ("ll_rm_files: Config file `" + f.path +
									"' is in file list but not in the db anymore.");
						}
					}
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

		activate_package_triggers (params, pkgdb, mdata);


		mdata->state = change ? PKG_STATE_POSTRM_CHANGE : PKG_STATE_POSTRM_BEGIN;
		pkgdb.update_state (mdata);

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
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
		StoredMaintainerScripts& sms,
		bool change)
{
	if (!(
				(!change && mdata->state == PKG_STATE_POSTRM_BEGIN) ||
				(change && mdata->state == PKG_STATE_POSTRM_CHANGE)))
		throw gp_exception ("ll_run_postrm called with pkg in inacceptable state.");

	try
	{
		printf_verbose_flush (params, "  Running postrm script ...");

		auto postrm = sms.get_postrm();
		if (postrm)
		{
			if (change)
				run_script (params, *postrm, "change");
			else
				run_script (params, *postrm);
		}

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
	}
	catch (exception& e)
	{
		printf (COLOR_RED " failed" COLOR_NORMAL "\n");
		printf ("%s\n", e.what());
		return false;
	}


	try
	{
		printf_verbose_flush (params, "  Removing db tuples and stored maintainer scripts ...");

		pkgdb.begin();

		StoredMaintainerScripts::delete_archive (params, mdata);
		pkgdb.delete_package (mdata);

		pkgdb.commit();

		printf_verbose (params, COLOR_GREEN " OK" COLOR_NORMAL "\n");
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


/* Set the installation reason of a group of packages to the specified value. */
bool set_installation_reason (char reason, std::shared_ptr<Parameters> params)
{
	PackageDB pkgdb (params);

	/* Interpret the supplied package specifications */
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

	/* Find the packages that match the given descriptions */
	vector<shared_ptr<PackageMetaData>> pkgs_to_change;
	for (auto mdata : pkgdb.get_packages_in_state (ALL_PKG_STATES))
	{
		auto i = pkg_ids.find (make_pair (mdata->name, mdata->architecture));
		if (i != pkg_ids.end())
		{
			if (mdata->installation_reason != reason)
				pkgs_to_change.push_back (mdata);

			pkg_ids.erase (i);
		}
	}

	if (!pkg_ids.empty())
	{
		for (auto& id : pkg_ids)
		{
			fprintf (stderr, "Package %s@%s is not installed.\n",
					id.first.c_str(),
					Architecture::to_string (id.second).c_str());
		}

		return false;
	}

	/* For each package that is installed, change the installation reason if
	 * required. */
	for (auto mdata : pkgs_to_change)
	{
		printf ("ll changing installation reason of package %s@%s\n",
				mdata->name.c_str(),
				Architecture::to_string (mdata->architecture).c_str());

		if (!ll_change_installation_reason (params, pkgdb, mdata, reason))
			return false;
	}

	return true;
}


/*************************** Config file handling *****************************/
bool config_file_differs (shared_ptr<Parameters> params, const PackageDBFileEntry& file)
{
	const auto target_path = simplify_path (params->target + "/" + file.path);
	struct stat statbuf;

	int ret = lstat (target_path.c_str(), &statbuf);
	if (ret < 0)
	{
		if (errno == ENOENT || errno == ENOTDIR)
			return true;

		throw system_error (error_code (errno, generic_category()));
	}

	/* Compare type */
	switch (file.type)
	{
		case FILE_TYPE_REGULAR:
		{
			if (!S_ISREG (statbuf.st_mode))
				return true;

			char tmp[20];
			ret = md::sha1_file (target_path.c_str(), tmp);
			if (ret < 0)
				throw system_error (error_code (-ret, generic_category()));

			if (memcmp (tmp, file.sha1_sum, 20) != 0)
				return true;

			break;
		}

		case FILE_TYPE_LINK:
		{
			if (!S_ISLNK (statbuf.st_mode))
				return true;

			auto lnk = convenient_readlink (target_path);

			char tmp[20];
			md::sha1_memory (lnk.c_str(), lnk.size(), tmp);

			if (memcmp (tmp, file.sha1_sum, 20) != 0)
				return true;

			break;
		}

		default:
			return false;
	}

	return false;
}


/******************************** Triggers ************************************/
void activate_package_triggers (shared_ptr<Parameters> params, PackageDB& pkgdb,
		shared_ptr<PackageMetaData> mdata)
{
	pkgdb.ensure_activating_triggers_read (mdata);
	for (const auto& trg : *mdata->activated_triggers)
	{
		pkgdb.activate_trigger (trg);
		printf_verbose (params, "\n  Activated trigger `%s'.", trg.c_str());
	}
}


bool execute_triggers (shared_ptr<Parameters> params, PackageDB& pkgdb)
{
	auto trgs = pkgdb.get_activated_triggers ();
	if (trgs.empty())
		return true;

	if (!params->target_is_native())
	{
		printf ("Not executing triggers because the target is not native.\n");
		return true;
	}

	std::map<tuple<string, int, VersionNumber>, optional<StoredMaintainerScripts>> cache;

	for (const auto& trg : trgs)
	{
		printf ("Executing trigger %s.\n", trg.c_str());

		/* Find packages interested in this trigger and run their configure
		 * scripts. */
		for (const auto& t : pkgdb.find_packages_interested_in_trigger(trg))
		{
			const auto& [name, arch, version] = t;

			auto ipkg = cache.find (t);
			if (ipkg == cache.end())
			{
				auto pkg = pkgdb.get_reduced_package(name, arch, version);
				if (!pkg)
				{
					fprintf (stderr, "WARNING: Package %s@%s:%s is interested in "
							"triggers but not in the db.\n",
							name.c_str(),
							Architecture::to_string(arch).c_str(),
							version.to_string().c_str());
				}

				if (pkg && pkg->state != PKG_STATE_CONFIGURED)
				{
					fprintf (stderr, "WARNING: Package %s@%s:%s is interested in "
							"triggers but not configured. Triggers will not be run for it.\n",
							name.c_str(),
							Architecture::to_string(arch).c_str(),
							version.to_string().c_str());

					pkg = nullptr;
				}

				cache.emplace (make_pair(t, StoredMaintainerScripts(params, pkg)));
				ipkg = cache.find (t);
			}

			if (ipkg->second)
			{
				auto configure = ipkg->second->get_configure();
				if (configure)
				{
					printf_verbose(params, "  Triggering %s@%s:%s...\n",
							name.c_str(), Architecture::to_string(arch).c_str(),
							version.to_string().c_str());

					run_script (params, *configure, "triggered", trg.c_str());
				}
			}
		}

		/* Clear the trigger */
		pkgdb.clear_trigger (trg);
	}

	return true;
}
