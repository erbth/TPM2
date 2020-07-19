/** This file is part of the TSClient LEGACY Package Manager
 *
 * A (new) abstraction for package versions. */
#ifndef __PACKAGE_VERSION_H
#define __PACKAGE_VERSION_H

#include <string>
#include <utility>
#include <version_number.h>

/** Abstract base class */
class PackageVersion
{
protected:
	const std::string name;
	int architecture;
	const VersionNumber version;

	PackageVersion(const std::string &n, const int architecture, const VersionNumber &v)
		: name(n), architecture(architecture), version(v)
	{
	}

	virtual ~PackageVersion() = 0;

public:
	/* Tell if this package version is currently installed. Essentially this
	 * method is supposed to return true for `InstalledPackageVersion`s and
	 * false for `ProvidedPackageVersions`. It avoids a dynamic cast. */
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

	inline VersionNumber get_version_number() const
	{
		return version;
	}

	/* Package versions can be compared based on their name, architecture and
	 * version number. This means, among other things, that two package versions
	 * are equal if they have the same name, architecture and version number, no
	 * matter if they are the same object or even belong to the same subclass.
	 * */
	bool operator==(const PackageVersion &o) const
	{
		return o.name == name && o.architecture == architecture && o.version == version;
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

		if (version < o.version)
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

/* Every class needs a destructor even if it is pure virtual. */
PackageVersion::~PackageVersion()
{
}

#endif /* __PACKAGE_VERSION_H */
