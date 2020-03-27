/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains classes that represent a package's meta data. That is
 * everything except maintainer scripts (I borrowed this term from Debian) and
 * the actual file archive. */

#ifndef __PACKAGE_META_DATA_H
#define __PACKAGE_META_DATA_H

#include <string>
#include "architecture.h"
#include "dependencies.h"
#include "version_number.h"


#define PKG_STATE_WANTED				0

/* Only for selecting packages */
#define ALL_PKG_STATES					1000


/* This class represents a package in memory. It shall be returned by the
 * package providing module from repositories, depres shall use it and finally
 * the install module can find information about the package here. */
struct PackageMetaData
{
	const std::string name;
	const int architecture;
	const VersionNumber version;
	const VersionNumber source_version;


	DependencyList pre_dependencies;
	DependencyList dependencies;

	int state;

	// FileList files;


	/* Methods */
	PackageMetaData(const std::string &name, const int architecture,
			const VersionNumber &version, const VersionNumber &source_version);


	/* Just a convenience method with straight forward implementation */
	void add_pre_dependency(const Dependency&);
	void add_dependency(const Dependency&);
};

#endif /* __PACKAGE_META_DATA_H */
