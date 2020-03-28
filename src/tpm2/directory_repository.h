/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains the directory repository client implementation. */

#ifndef __DIRECTORY_REPOSITORY_H
#define __DIRECTORY_REPOSITORY_H

#include <filesystem>
#include <string>
#include "repository.h"


class DirectoryRepository : public Repository
{
protected:
	std::path location;

public:
	DirectoryRepository (const std::path& location);
	virtual ~DirectoryRepository();


	std::set<VersionNumber> list_package_versions (
			const std::string& name, const int architecture) override;

	std::optional<ProvidedPackage> get_package (const std::string& name,
			const int architecture, const VersionNumber& version) override;
};


#endif /* __DIRECTORY_REPOSITORY_H */
