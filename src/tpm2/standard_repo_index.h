/** Standard repo index implementation */
#ifndef __STANDARD_REPO_INDEX_H
#define __STANDARD_REPO_INDEX_H

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include "repo_index.h"
#include "parameters.h"
#include "architecture.h"

extern "C" {
#include <openssl/evp.h>
}

class StandardRepoIndex : public RepoIndex
{
protected:
	const std::shared_ptr<Parameters> params;
	const std::filesystem::path index_path;
	bool index_read = false;

	int fd_file_index = -1;

	/* In-memory index */
	int arch = Architecture::invalid;

	std::set<std::string> packages;
	std::map<std::string, std::vector<VersionNumber>> package_versions;
	std::map<
		std::pair<std::string, VersionNumber>,
		std::tuple<std::shared_ptr<PackageMetaData>, std::string, uint64_t, uint64_t>
	> package_data;

	std::optional<std::tuple<std::string, ManagedBuffer<unsigned char>, unsigned, ssize_t>>
		read_signature(FILE* f);

	EVP_PKEY* retrieve_key (const std::string& key_name);

	void check_signature(FILE* f, const std::string& key_name,
			const unsigned char* sig, size_t sig_len, size_t data_end);

public:
	StandardRepoIndex(std::shared_ptr<Parameters> params, const std::filesystem::path& index_path);
	virtual ~StandardRepoIndex();

	/* Read the index from disk and check the its integrity and signature. This
	 * method must be called before any call to get_*.
	 *
	 * @param require_signature  If true, the image must be signed (with a valid
	 * 		signature). If false, an invalid signature will still throw an
	 * 		exception, but if no signature is present, nothing is done.
	 *
	 * In case authentication fails, an IndexAuthenticationFailed exception is
	 * thrown. */
	void read(const bool require_signature) override;

	/* Retrieving package information */
	std::vector<std::string> list_packages (const int pkg_arch) override;

	std::set<VersionNumber> list_package_versions (
			const std::string& pkg_name, const int pkg_arch) override;

	std::shared_ptr<PackageMetaData> get_mdata(
			const std::string& pkg_name,
			const int pkg_arch,
			const VersionNumber& pkg_version) override;

	/* SHA256 digest */
	std::optional<std::string> get_digest(
			const std::string& pkg_name,
			const int pkg_arch,
			const VersionNumber& pkg_version) override;

	std::shared_ptr<FileList> get_file_list(
			const std::string& pkg_name,
			const int pkg_arch,
			const VersionNumber& pkg_version) override;
};

#endif /* __STANDARD_REPO_INDEX_H */
