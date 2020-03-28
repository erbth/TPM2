#include "package_provides.h"

using namespace std;


ProvidedPackage::ProvidedPackage (shared_ptr<PackageMetaData> md)
	: md(md)
{
}


PackageProvider::PackageProvider (shared_ptr<Parameters> params)
	: params(params)
{
}


set<VersionNumber> list_package_versions (const string& name, const int architecture)
{
	set<VersionNumber> vs;

	for (autor &r : repositories)
		vs.merge (r.list_package_versions (name, architecture);

	return vs;
}


optional<ProvidedPackage> get_package (const string& name,
		const int architecture, const VersionNumber& version)
{
	for (auto &r : repositories)
	{
		ProvidedPackage p = r.get_package (name, architecture, version);

		if (p)
			return p;
	}

	return nullopt;
}
