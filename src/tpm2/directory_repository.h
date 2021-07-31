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
#include <tuple>
#include "repository.h"
#include "version_number.h"
#include "parameters.h"


class DirectoryRepository : public Repository
{
protected:
	const std::shared_ptr<Parameters> params;
	const std::filesystem::path location;
	const bool require_signing;

	std::list<
		std::pair<
			int,
			std::map<std::string, std::vector<std::tuple<VersionNumber, std::string, std::shared_ptr<RepoIndex>>>>
		>
	> index_cache;

	/* @returns May be null */
	const std::map<std::string, std::vector<std::tuple<VersionNumber,std::string,std::shared_ptr<RepoIndex>>>>*
		read_index (int arch);

public:
	DirectoryRepository (std::shared_ptr<Parameters> params,
			const std::filesystem::path& location, const bool require_signing);

	virtual ~DirectoryRepository();


	std::set<VersionNumber> list_package_versions (
			const std::string& name, const int architecture) override;

	std::optional<std::pair<std::string, std::shared_ptr<RepoIndex>>> get_package (
			const std::string& name, const int architecture, const VersionNumber& version) override;

	bool digest_checking_required() override;
};


#endif /* __DIRECTORY_REPOSITORY_H */
