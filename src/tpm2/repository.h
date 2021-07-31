/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains an abstract interface for repositories */

#ifndef __REPOSITORY_H
#define __REPOSITORY_H

#include <memory>
#include <set>
#include <string>
#include <optional>
#include <utility>
#include "version_number.h"
#include "repo_index.h"


class Repository
{
public:
	virtual ~Repository() {};

	virtual std::set<VersionNumber> list_package_versions (
			const std::string &name, const int architecture) = 0;

	/** @returns  A path to the package's transport form and optionally a
	 * 		pointer to an index containing the package. */
	virtual std::optional<std::pair<std::string, std::shared_ptr<RepoIndex>>> get_package (
			const std::string& name, const int architecture, const VersionNumber& version) = 0;

	/** @returns  true if the repository requires that packages are
	 * digest-checked against checksums provided in an index. Actually this
	 * information could be encoded in returning an index or not (and only
	 * returning packages which are on an index), however it is easier to verify
	 * that digest checking is enforced if it is encoded in an additional
	 * parameter. */
	virtual bool digest_checking_required() = 0;
};

#endif /* __REPOSITORY_H */
