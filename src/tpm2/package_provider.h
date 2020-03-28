/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains the package provider. */

#ifndef __PACKAGE_PROVIDER_H
#define __PACKAGE_PROVIDER_H

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "parameters.h"
#include "version_number.h"
#include "package_meta_data.h"
#include "Repository.h"


struct ProvidedPackage
{
	std::shared_ptr<PackageMetaData> md;

	/* A constructor */
	ProvidedPackage(std::shared_ptr<PackageMetaData> md);
};


class PackageProvider
{
private:
	std::shared_ptr<Parameters> params;
	std::vector<Repository> repositories;

public:
	PackageProvider (std::shared_ptr<Parameters> params);

	std::set<VersionNumber> list_package_versions (const std::string& name, const int architecture);

	std::optional<ProvidedPackage> get_package (
			const std::string& name, const int architecture, const VersionNumber& version);
};


#endif /* __PACKAGE_PROVIDER_H */
