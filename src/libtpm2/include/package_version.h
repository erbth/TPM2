/** This file is part of the TSClient LEGACY Package Manager
 *
 * A (new) abstraction for package versions. */
#ifndef __PACKAGE_VERSION_H
#define __PACKAGE_VERSION_H

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <version_number.h>
#include "package_constraints.h"

/** Abstract base class */
class PackageVersion
{
protected:
	const std::string name;
	int architecture;
	const VersionNumber source_version, binary_version;

	PackageVersion(const std::string &n, const int architecture,
			const VersionNumber &sv, const VersionNumber &bv)
		: name(n), architecture(architecture), source_version(sv), binary_version(bv)
	{
	}

	virtual ~PackageVersion() = 0;

public:
	/* Tell if this package version is currently installed. Essentially this
	 * method is supposed to return true for `InstalledPackageVersion`s and
	 * false for `ProvidedPackageVersion`s. It avoids a dynamic cast. */
	virtual bool is_installed() const = 0;

	inline std::string get_name() const
	{
		return name;
	}

	inline int get_architecture() const
	{
		return architecture;
	}

	inline std::pair<std::string, int> get_identifier() const
	{
		return make_pair(name, architecture);
	}

	inline VersionNumber get_source_version() const
	{
		return source_version;
	}

	inline VersionNumber get_binary_version() const
	{
		return binary_version;
	}

	virtual std::vector<
			std::pair<std::pair<std::string, int>, std::shared_ptr<PackageConstraints::Formula>>
		> get_dependencies() = 0;

	virtual const std::vector<std::string> &get_files() = 0;
	virtual const std::vector<std::string> &get_directories() = 0;

	/* Package versions can be compared based on their name, architecture and
	 * binary version number. This means, among other things, that two package
	 * versions are equal if they have the same name, architecture and binary
	 * version number, no matter if they are the same object or even belong to
	 * the same subclass.  */
	bool operator==(const PackageVersion &o) const
	{
		return o.name == name && o.architecture == architecture &&
			o.binary_version == binary_version;
	}

	bool operator!=(const PackageVersion &o) const
	{
		return !(*this == o);
	}

	bool operator<(const PackageVersion &o) const
	{
		if (name < o.name)
			return true;
		else if (name > o.name)
			return false;

		if (architecture < o.architecture)
			return true;
		else if (architecture > o.architecture)
			return false;

		if (binary_version < o.binary_version)
			return true;

		return false;
	}

	bool operator<=(const PackageVersion &o) const
	{
		return *this == o || *this < o;
	}

	bool operator>(const PackageVersion &o) const
	{
		return o < *this;
	}

	bool operator>=(const PackageVersion &o) const
	{
		return o <= *this;
	}
};

#endif /* __PACKAGE_VERSION_H */
