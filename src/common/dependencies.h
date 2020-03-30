/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains implementations about the concept of dependencies. */

#ifndef __DEPENDENCIES_H
#define __DEPENDENCIES_H

#include <memory>
#include <set>
#include <string>
#include <utility>
#include "package_constraints.h"


struct Dependency
{
	/* A pair (name, architecture) */
	const std::pair<std::string, int> identifier;
	const std::shared_ptr<const PackageConstraints::Formula> version_formula;

	/* A constructor */
	Dependency(const std::string &name, const int architecture,
			const std::shared_ptr<const PackageConstraints::Formula> version_formula);

	const std::string& get_name() const;
	int get_architecture() const;

	/* To easily use this with sets. Dependencies are considered equal if their
	 * identifies match. For everything else it wouldn't make much sense to use
	 * a set ... */
	bool operator<(const Dependency&) const;
};


/* A dependency list actually owns the Dependencies. */
struct DependencyList
{
	std::set<Dependency> dependencies;

	std::set<Dependency>::iterator begin();
	std::set<Dependency>::iterator end();

	std::set<Dependency>::const_iterator cbegin() const;
	std::set<Dependency>::const_iterator cend() const;
};

#endif /* __DEPENDENCIES */
