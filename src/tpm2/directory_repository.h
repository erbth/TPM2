/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains the directory repository client implementation. */

#ifndef __DIRECTORY_REPOSITORY_H
#define __DIRECTORY_REPOSITORY_H

#include <filesystem>
#include <list>
#include <map>
#include <string>
#include <utility>
#include "repository.h"
#include "version_number.h"


class DirectoryRepository : public Repository
{
protected:
	const std::filesystem::path location;

	std::list<std::pair<int,std::map<std::string, std::vector<std::pair<VersionNumber,std::string>>>>> index_cache;

	/* @returns May be null */
	const std::map<std::string, std::vector<std::pair<VersionNumber,std::string>>>* read_index (int arch);

public:
	DirectoryRepository (const std::filesystem::path& location);
	virtual ~DirectoryRepository();


	std::set<VersionNumber> list_package_versions (
			const std::string& name, const int architecture) override;

	std::optional<std::string> get_package (const std::string& name,
			const int architecture, const VersionNumber& version) override;
};


#endif /* __DIRECTORY_REPOSITORY_H */
