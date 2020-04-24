#include <algorithm>
#include <map>
#include <utility>
#include "pkg_tools.h"
#include "utility.h"
#include "package_db.h"
#include "architecture.h"
#include "package_provider.h"

using namespace std;


string pkg_state_to_string (int state)
{
	switch (state)
	{
		case PKG_STATE_INVALID:
			return "invalid";

		case PKG_STATE_WANTED:
			return "wanted";

		case PKG_STATE_PREINST_BEGIN:
			return "preinst_begin";

		case PKG_STATE_UNPACK_BEGIN:
			return "unpack_begin";

		case PKG_STATE_CONFIGURE_BEGIN:
			return "configure_begin";

		case PKG_STATE_CONFIGURED:
			return "configured";

		case PKG_STATE_UNCONFIGURE_BEGIN:
			return "unconfigure_begin";

		case PKG_STATE_RM_FILES_BEGIN:
			return "rm_files_begin";

		case PKG_STATE_POSTRM_BEGIN:
			return "postrm_begin";

		case PKG_STATE_UNCONFIGURE_CHANGE:
			return "unconfigure_change";

		case PKG_STATE_WAIT_NEW_UNPACKED:
			return "wait_new_unpacked";

		case PKG_STATE_RM_FILES_CHANGE:
			return "rm_files_change";

		case PKG_STATE_POSTRM_CHANGE:
			return "postrm_change";

		case PKG_STATE_PREINST_CHANGE:
			return "preinst_change";

		case PKG_STATE_UNPACK_CHANGE:
			return "unpack_change";

		case PKG_STATE_WAIT_OLD_REMOVED:
			return "wait_old_removed";

		case PKG_STATE_CONFIGURE_CHANGE:
			return "configure_change";

		default:
			return "???";
	};
}


bool list_installed_packages (shared_ptr<Parameters> params)
{
	print_target (params, true);
	PackageDB pkgdb (params);

	printf ("\n");

	auto all_pkgs = pkgdb.get_packages_in_state (ALL_PKG_STATES);

	/* Compute the size of different columns */
	unsigned c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;

	for (auto p : all_pkgs)
	{
		c1 = MAX (c1, p->name.size());
		c2 = MAX (c2, Architecture::to_string (p->architecture).size());
		c3 = MAX (c3, p->version.to_string().size());
		c4 = MAX (c4, installation_reason_to_string (p->installation_reason).size());
		c5 = MAX (c5, pkg_state_to_string (p->state).size());
	}

	for (auto p : all_pkgs)
	{
		printf ("%-*s @ %-*s : %-*s - %-*s / %-*s\n",
				(int) c1, p->name.c_str(),
				(int) c2, Architecture::to_string (p->architecture).c_str(),
				(int) c3, p->version.to_string().c_str(),
				(int) c4, installation_reason_to_string(p->installation_reason).c_str(),
				(int) c5, pkg_state_to_string (p->state).c_str());
	}

	return true;
}


bool show_version (shared_ptr<Parameters> params)
{
	print_target (params, true);
	if (params->operation_packages.empty())
	{
		fprintf (stderr, "No package specified.\n");
		return false;
	}

	auto res = parse_cmd_param (*params, params->operation_packages.front());
	if (!res.success)
	{
		fprintf (stderr, "Unknown package description: %s (%s)\n",
				res.pkg.c_str(), res.err.c_str());
		return false;
	}

	PackageDB pkgdb(params);

	auto all_packages = pkgdb.get_packages_in_state (ALL_PKG_STATES);

	for (auto mdata : all_packages)
	{
		if (mdata->name == res.name && mdata->architecture == res.arch)
		{
			printf ("%s\n", mdata->version.to_string().c_str());
			return true;
		}
	}

	printf ("---\n");
	return true;
}


bool list_available (shared_ptr<Parameters> params)
{
	print_target (params, true);
	if (params->operation_packages.empty())
	{
		fprintf (stderr, "No package specified.\n");
		return false;
	}

	auto res = parse_cmd_param (*params, params->operation_packages.front());
	if (!res.success)
	{
		fprintf (stderr, "Unknown package description: %s (%s)\n",
				res.pkg.c_str(), res.err.c_str());
		return false;
	}

	/* Search for available versions */
	auto pprov = PackageProvider::create (params);
	auto available_versions = pprov->list_package_versions (res.name, res.arch);

	/* Search for the installed version */
	PackageDB pkgdb(params);

	auto all_packages = pkgdb.get_packages_in_state (ALL_PKG_STATES);
	shared_ptr<PackageMetaData> installed_version;

	for (auto mdata : all_packages)
	{
		if (mdata->name == res.name && mdata->architecture == res.arch)
		{
			installed_version = mdata;
			break;
		}
	}

	if (installed_version)
	{
		printf ("Installed: %s (%s)\n",
				installed_version->version.to_string().c_str(),
				installed_version->source_version.to_string().c_str());
	}
	else
	{
		printf ("Installed: ---\n");
	}

	printf ("Available versions:\n");


	vector<shared_ptr<PackageMetaData>> available_mdata;
	for (auto v : available_versions)
	{
		auto pp = pprov->get_package (res.name, res.arch, v);
		if (!pp)
		{
			fprintf (stderr, "Failed to get package version %s.\n", v.to_string().c_str());
			continue;
		}

		available_mdata.push_back (pp->get_mdata());
	}

	sort (available_mdata.begin(), available_mdata.end(), [](
				shared_ptr<PackageMetaData> a, shared_ptr<PackageMetaData> b) {
					if (a->source_version > b->source_version)
						return true;

					return a->version > b->version;
				});

	for (auto mdata : available_mdata)
	{
		/* Filter those versions out that don't meet version constraints, if the
		 * user specified them. */
		if (res.vc)
		{
			if (!res.vc->fulfilled (mdata->source_version, mdata->version))
				continue;
		}

		printf ("    %s (%s)\n",
				mdata->version.to_string().c_str(),
				mdata->source_version.to_string().c_str());
	}

	return true;
}


bool show_problems (shared_ptr<Parameters> params)
{
	print_target (params, true);
	PackageDB pkgdb (params);

	bool errors_found = false;

	auto all_packages = pkgdb.get_packages_in_state (ALL_PKG_STATES);

	/* List problems in problematic states */
	printf ("\nSearching for packages in invalid states ...\n");
	for (auto mdata : all_packages)
	{
		if (mdata->state == PKG_STATE_CONFIGURED)
		{
			continue;
		}
		else if (mdata->state == PKG_STATE_CONFIGURE_BEGIN)
		{
			printf ("warning: %s@%s:%s in pre-configured state %s\n",
					mdata->name.c_str(),
					Architecture::to_string (mdata->architecture).c_str(),
					mdata->version.to_string().c_str(),
					pkg_state_to_string (mdata->state).c_str());

			errors_found = true;
		}
		else
		{
			printf ("ERROR:   %s@%s:%s in unaccepting state %s\n",
					mdata->name.c_str(),
					Architecture::to_string (mdata->architecture).c_str(),
					mdata->version.to_string().c_str(),
					pkg_state_to_string (mdata->state).c_str());

			errors_found = true;
		}
	}

	/* List missing dependencies */
	printf ("\nLocating missing dependencies ...\n");

	map<tuple<string, int>, shared_ptr<PackageMetaData>> pkg_map;
	for (auto mdata : all_packages)
		pkg_map.insert (make_pair (make_pair (mdata->name, mdata->architecture), mdata));

	for (auto mdata : all_packages)
	{
		/* Look if all pre-dependencies are installed and fulfill potential
		 * version constraints. */
		for (const auto& dep : mdata->pre_dependencies)
		{
			auto i = pkg_map.find (dep.identifier);

			if (i == pkg_map.end())
			{
				printf ("Package %s@%s:%s pre-depends on non-present package %s@%s.\n",
						mdata->name.c_str(),
						Architecture::to_string (mdata->architecture).c_str(),
						mdata->version.to_string().c_str(),
						dep.get_name().c_str(),
						Architecture::to_string (dep.get_architecture()).c_str());

				errors_found = true;
			}
			else
			{
				if (dep.version_formula)
				{
					auto dep_mdata = i->second;

					if (!dep.version_formula->fulfilled (dep_mdata->source_version, dep_mdata->version))
					{
						printf ("Package %s@%s:%s pre-depends on package %s@%s "
								"but an unaccepted version is installed.\n",
								mdata->name.c_str(),
								Architecture::to_string (mdata->architecture).c_str(),
								mdata->version.to_string().c_str(),
								dep_mdata->name.c_str(),
								Architecture::to_string (dep_mdata->architecture).c_str());

						errors_found = true;
					}
				}
			}
		}

		/* Look if all dependencies are installed and fulfill potential version
		 * constraints. */
		for (const auto& dep : mdata->dependencies)
		{
			auto i = pkg_map.find (dep.identifier);

			if (i == pkg_map.end())
			{
				printf ("Package %s@%s:%s depends on non-present package %s@%s.\n",
						mdata->name.c_str(),
						Architecture::to_string (mdata->architecture).c_str(),
						mdata->version.to_string().c_str(),
						dep.get_name().c_str(),
						Architecture::to_string (dep.get_architecture()).c_str());

				errors_found = true;
			}
			else
			{
				if (dep.version_formula)
				{
					auto dep_mdata = i->second;

					if (!dep.version_formula->fulfilled (dep_mdata->source_version, dep_mdata->version))
					{
						printf ("Package %s@%s:%s depends on package %s@%s but "
								"an unaccepted version is installed.\n",
								mdata->name.c_str(),
								Architecture::to_string (mdata->architecture).c_str(),
								mdata->version.to_string().c_str(),
								dep_mdata->name.c_str(),
								Architecture::to_string (dep_mdata->architecture).c_str());

						errors_found = true;
					}
				}
			}
		}
	}

	return errors_found;
}
