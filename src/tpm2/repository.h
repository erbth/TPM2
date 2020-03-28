/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains an abstract interface for repositories */

#ifndef __REPOSITORY_H
#define __REPOSITORY_H

#include <memory>
#include <set>
#include <string>
#include <utility>
#include "version_number.h"


/* Prototypes */
class ProvidedPackage;


class Repository
{
public:
	virtual ~Repository() = 0 {};

	virtual std::set<VersionNumber> list_package_versions (
			const std::string &name, const int architecture) = 0;

	virtual std::optional<ProvidedPackage> get_package (const std::string& name,
			const int architecture, const VersionNumber& version) = 0;
};


#include "PackageProvider.h"

#endif /* __REPOSITORY_H */
