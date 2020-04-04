#include <cstdio>
#include <regex>
#include "directory_repository.h"
#include "architecture.h"
#include "filesystem"

using namespace std;
namespace fs = std::filesystem;


DirectoryRepository::DirectoryRepository (const fs::path& location)
	: location(location)
{
}


DirectoryRepository::~DirectoryRepository ()
{
}


const map<string, vector<pair<VersionNumber,string>>>* DirectoryRepository::read_index (int arch)
{
	/* Do we have it already? */
	for (auto i = index_cache.begin(); i != index_cache.end(); i++)
	{
		if (i->first == arch)
		{
			/* Move to front */
			index_cache.splice (index_cache.begin(), index_cache, i);
			return &i->second;
		}
	}


	/* If not, read index */
	fs::path arch_location = location / Architecture::to_string (arch);

	if (fs::is_directory (arch_location))
	{
		auto& index = index_cache.emplace_front (
				arch,
				map<string, vector<pair<VersionNumber,string>>>()
			).second;

		const regex re("(.+)-([^-_]+)_" + Architecture::to_string (arch) + ".tpm2");

		for (auto& de : fs::directory_iterator(arch_location))
		{
			smatch m;

			string filename(de.path().filename().string());

			if (regex_match (filename, m, re))
			{
				try
				{
					const string& pkg_name = m[1].str();
					VersionNumber&& pkg_version(m[2].str());
					string&& pkg_location(arch_location / filename);

					auto i = index.find (pkg_name);

					if (i == index.end())
					{
						vector<pair<VersionNumber, string>> v;
						v.emplace_back (pkg_version, pkg_location);


						index.emplace (make_pair (string(pkg_name), move(v)));
					}
					else
					{
						i->second.emplace_back (make_pair (pkg_version, pkg_location));
					}
				}
				catch (InvalidVersionNumberString& e)
				{
					fprintf (stderr,
							"Invalid version number string in directory repo %s: %s (%s)\n",
							location.c_str(), m[1].str().c_str(), e.what());
				}
			}
		}

		return &index;
	}

	return nullptr;
}


set<VersionNumber> DirectoryRepository::list_package_versions (const string& name, const int architecture)
{
	set<VersionNumber> versions;

	auto index = read_index (architecture);

	if (index)
	{
		auto i = index->find (name);

		if (i != index->end())
		{
			for (const auto& p : i->second)
			{
				versions.insert (p.first);
			}
		}
	}

	return versions;
}


optional<string> DirectoryRepository::get_package (const string& name, const int architecture,
		const VersionNumber& version)
{
	auto index = read_index (architecture);

	if (index)
	{
		auto i = index->find (name);

		if (i != index->end())
		{
			for (const auto& p : i->second)
			{
				if (p.first == version)
					return location / Architecture::to_string (architecture) / p.second;
			}
		}
	}

	return nullopt;
}
