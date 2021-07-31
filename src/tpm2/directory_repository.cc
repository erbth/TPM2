#include <algorithm>
#include <cstdio>
#include <regex>
#include "directory_repository.h"
#include "architecture.h"
#include "filesystem"
#include "standard_repo_index.h"
#include "common_utilities.h"

using namespace std;
namespace fs = std::filesystem;


DirectoryRepository::DirectoryRepository (
		shared_ptr<Parameters> params, const fs::path& location, const bool require_signing)
	: params(params), location(params->target + "/" + location.string()), require_signing(require_signing)
{
}


DirectoryRepository::~DirectoryRepository ()
{
}


const map<string, vector<tuple<VersionNumber,string,shared_ptr<RepoIndex>>>>*
DirectoryRepository::read_index (int arch)
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
		auto& new_index_cache = index_cache.emplace_front (
				arch,
				map<string, vector<tuple<VersionNumber,string,shared_ptr<RepoIndex>>>>()
			).second;


		/* Read index files */
		for (auto& entry : fs::directory_iterator(arch_location))
		{
			if (entry.path().filename().extension() != ".index")
				continue;

			auto repo_index = make_shared<StandardRepoIndex>(params, entry.path());

			try
			{
				repo_index->read(require_signing);
			}
			catch (UnsupportedIndexVersion& e)
			{
				fprintf (stderr, "Skipping unsupported index '%s': %s.\n",
						entry.path().c_str(), e.what());

				continue;
			}
			catch (IndexAuthenticationFailedNoSignature&)
			{
				fprintf (stderr, COLOR_BRIGHT_YELLOW
						"WARNING: Index '%s' has no signature, but "
						"signatures are required for this repository. "
						"Ignoring index." COLOR_NORMAL "\n", entry.path().c_str());

				continue;
			}

			/* Add packages from index to the index cache if they are not
			 * present yet. */
			for (const auto& pkg_name : repo_index->list_packages(arch))
			{
				auto i = new_index_cache.find (pkg_name);
				if (i == new_index_cache.end())
				{
					new_index_cache.emplace (
							make_pair(
								pkg_name,
								vector<tuple<VersionNumber, string, shared_ptr<RepoIndex>>>()
							)
					);

					i = new_index_cache.find (pkg_name);
				}

				/* Insert missing versions */
				for (const auto& v : repo_index->list_package_versions(pkg_name, arch))
				{
					bool exists = false;

					for (const auto& [ev, ea, ei] : i->second)
					{
						if (ev == v)
						{
							exists = true;
							break;
						}
					}

					if (exists)
						continue;

					i->second.push_back(
							make_tuple(
								v,
								pkg_name + "-" + v.to_string() + "_" +
									Architecture::to_string(arch) + ".tpm2",
								repo_index
							)
					);
				}
			}
		}


		/* Read directory if signing is not require. */
		if (!require_signing)
		{
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

						auto i = new_index_cache.find (pkg_name);

						if (i == new_index_cache.end())
						{
							new_index_cache.emplace (
									make_pair(
										pkg_name,
										vector<tuple<VersionNumber, string, shared_ptr<RepoIndex>>>()
									)
							);

							i = new_index_cache.find (pkg_name);
						}

						/* Insert missing archives */
						bool exists = false;

						for (const auto& [ev, ea, ei] : i->second)
						{
							if (ev == pkg_version)
							{
								exists = true;
								break;
							}
						}

						if (!exists)
						{
							i->second.push_back(
									make_tuple(
										pkg_version,
										filename,
										nullptr
									)
							);
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
		}

		return &new_index_cache;
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
			for (const auto& t : i->second)
			{
				versions.insert (get<0>(t));
			}
		}
	}

	return versions;
}


optional<pair<string, shared_ptr<RepoIndex>>> DirectoryRepository::get_package (
		const string& name, const int architecture, const VersionNumber& version)
{
	auto index = read_index (architecture);

	if (index)
	{
		auto i = index->find (name);

		if (i != index->end())
		{ 
			for (const auto& [v, filename, index] : i->second)
			{
				if (v == version)
				{
					return make_pair(
							location / Architecture::to_string (architecture) / filename,
							index);
				}
			}
		}
	}

	return nullopt;
}


bool DirectoryRepository::digest_checking_required()
{
	return require_signing;
}
