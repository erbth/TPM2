#include "pkg_tools.h"
#include "utility.h"
#include "package_db.h"
#include "architecture.h"

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
	print_target (params);
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
