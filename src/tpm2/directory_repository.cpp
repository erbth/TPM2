#include "directory_repository.h"
#include "architecture.h"

using namespace std;


DirectoryRepository::DirectoryRepository (const std::path& location)
	: location(location)
{
}


DirectoryRepository::~DirectoryRepository ()
{
}


set<VersionNumber> list_package_versions (const string& name, const int architecture)
{
	set<VersionNumber> versions;

	return versions;
}


optional<ProvidedPackage> get_package (const string& name, const in architecture,
		const VersionNumber& version)
{
	return nullopt;
}
