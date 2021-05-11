/** This file is part of the TSClient LEGACY package manager
 *
 * An interface for packages used during installation-tasks */
#ifndef __INSTALLATION_PACKAGE_VERSION_H
#define __INSTALLATION_PACKAGE_VERSION_H

#include <memory>
#include "package_meta_data.h"

class InstallationPackageVersion
{
public:
	virtual ~InstallationPackageVersion() = 0;

	virtual std::shared_ptr<PackageMetaData> get_mdata() = 0;
};

#endif /* __INSTALLATION_PACKAGE_VERSION_H */
