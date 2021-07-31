/** Interface for repository indexes */
#ifndef __REPO_INDEX_H
#define __REPO_INDEX_H

#include <exception>
#include <memory>
#include <optional>
#include <set>
#include "file_list.h"
#include "package_meta_data.h"
#include "managed_buffer.h"

class RepoIndex
{
public:
	virtual ~RepoIndex() = 0;

	/* Read the index from disk and check the its integrity and signature. This
	 * method must be called before any call to get_*.
	 *
	 * @param require_signature  If true, the image must be signed (with a valid
	 * 		signature). If false, an invalid signature will still throw an
	 * 		exception, but if no signature is present, nothing is done.
	 *
	 * In case authentication fails, an IndexAuthenticationFailed exception is
	 * thrown. */
	virtual void read(bool require_signature) = 0;

	/* Retrieving package information */
	virtual std::vector<std::string> list_packages (const int pkg_arch) = 0;

	virtual std::set<VersionNumber> list_package_versions (
			const std::string& pkg_name, const int pkg_arch) = 0;

	virtual std::shared_ptr<PackageMetaData> get_mdata(
			const std::string& pkg_name,
			const int pkg_arch,
			const VersionNumber& pkg_version) = 0;

	/* SHA256 digest */
	virtual std::optional<std::string> get_digest(
			const std::string& pkg_name,
			const int pkg_arch,
			const VersionNumber& pkg_version) = 0;

	virtual std::shared_ptr<FileList> get_file_list(
			const std::string& pkg_name,
			const int pkg_arch,
			const VersionNumber& pkg_version) = 0;
};

class IndexAuthenticationFailed : public std::exception
{
protected:
	std::string msg;

public:
	IndexAuthenticationFailed(const std::string msg);
	const char* what() const noexcept override;
};

/* Reserved for indexes without signature if one is requested, DO NOT use if an
 * index has a signature which failed to verify due to any reason. */
class IndexAuthenticationFailedNoSignature : public IndexAuthenticationFailed
{
public:
	IndexAuthenticationFailedNoSignature(const std::string msg);
};

class UnsupportedIndexVersion : public std::exception
{
protected:
	std::string msg;

public:
	UnsupportedIndexVersion(const std::string msg);
	const char* what() const noexcept override;
};

#endif /* __REPO_INDEX_H */
