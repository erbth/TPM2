#include "package_meta_data.h"

using namespace std;


PackageMetaData::PackageMetaData(const string &name, const int architecture,
		const VersionNumber &version, const VersionNumber &source_version)
	:
		name(name), architecture(architecture),
		version(version), source_version(source_version)
{
}


void PackageMetaData::add_pre_dependency(const Dependency &d)
{
	pre_dependencies.dependencies.insert(d);
}


void PackageMetaData::add_dependency(const Dependency &d)
{
	dependencies.dependencies.insert(d);
}
