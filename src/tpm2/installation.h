/** This file is part of the TSClient LEGACY Package Manager
 *
 * High level installation, reinstallation and upgrading of packages */
#ifndef __INSTALLATION_H
#define __INSTALLATION_H

#include <memory>
#include "parameters.h"
#include "package_meta_data.h"
#include "package_db.h"
#include "package_provider.h"
#include "stored_maintainer_scripts.h"

bool print_installation_graph(std::shared_ptr<Parameters> params);

bool install_packages(std::shared_ptr<Parameters> params);

bool ll_unpack_package (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<ProvidedPackage> pp);

/* Only on of @param pp and @param sms needs to be present. */
bool ll_configure_package (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		std::shared_ptr<ProvidedPackage> pp,
		std::shared_ptr<StoredMaintainerScripts> sms);

#endif /* __INSTALLATION_H */
