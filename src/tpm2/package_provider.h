/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains the package provider. */

#ifndef __PACKAGE_PROVIDER_H
#define __PACKAGE_PROVIDER_H

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "parameters.h"
#include "version_number.h"
#include "package_meta_data.h"
#include "repository.h"
#include "transport_form.h"
#include "managed_buffer.h"
#include "file_list.h"
#include "package_version.h"
#include "installation_package_version.h"
#include "repo_index.h"


/** Trust model: ProvidedPackage checks a digest contained in an authenticated
 * index against the real archive.
 *
 * If ProvidedPackage is given a ReadStream, the ReadStream was authenticated
 * already (or authentication is bypassed intentionally). Otherwise the archive
 * is checked before it is opened, which requires and index to be present,
 * unless disable_repo_digest_check is set to true.
 *
 * An index given to ProvidedPackage must have been authenticated already (or
 * authentication of it bypassed intentionally).
 *
 * If an disable_repo_digest_check is false and no index is given, the read
 * stream cannot be (re-) opened. This situation corresponds to the case where
 * no index is present or the package is not in the index, in which case it
 * can't be authenticated through the index (signing of archives is a different
 * matter but does not help in authenticating repositories, hence in this case).
 *
 * Therefore if a package is not mentioned in an index,
 * disable_repo_digest_check should be true. */
class ProvidedPackage : public PackageVersion, public InstallationPackageVersion
{
public:
	/* A factory for read streams, but as a function */
	using get_read_stream_t = std::function<std::shared_ptr<TransportForm::ReadStream>()>;

private:
	std::shared_ptr<PackageMetaData> mdata;

	/* Index and signature checking */
	bool disable_repo_digest_check;

	/* Referring to the original transport form and potentially and index */
	std::optional<TransportForm::TableOfContents> toc;
	std::shared_ptr<TransportForm::ReadStream> rs;
	get_read_stream_t get_read_stream;
	std::shared_ptr<RepoIndex> index;

	std::shared_ptr<FileList> files;
	std::shared_ptr<std::vector<std::string>> config_files;

	std::vector<std::string> file_paths;
	std::vector<std::string> directory_paths;
	bool file_paths_populated = false;
	bool directory_paths_populated = false;

	std::shared_ptr<ManagedBuffer<char>> preinst;
	std::shared_ptr<ManagedBuffer<char>> configure;
	std::shared_ptr<ManagedBuffer<char>> unconfigure;
	std::shared_ptr<ManagedBuffer<char>> postrm;

	void ensure_read_stream();
	TransportForm::TableOfContents& ensure_toc();

public:
	/* A constructor
	 *
	 * At least one of the following argument combinations must be provided:
	 *   * mdata, rs, toc
	 *   * mdata, get_read_stream, disable_repo_digest_check = true
	 *   * mdata, get_read_stream, index
	 * */
	ProvidedPackage(
			std::shared_ptr<PackageMetaData> mdata,
			const TransportForm::TableOfContents* toc,
			std::shared_ptr<TransportForm::ReadStream> rs,
			get_read_stream_t get_read_stream,
			std::shared_ptr<RepoIndex> index,
			bool disable_repo_digest_check);

	/* PackageVersion interface */
	bool is_installed() const override;

	std::vector<std::pair<std::pair<std::string, int>, std::shared_ptr<const PackageConstraints::Formula>>>
		get_dependencies() override;

	std::vector<std::pair<std::pair<std::string, int>, std::shared_ptr<const PackageConstraints::Formula>>>
		get_pre_dependencies() override;

	/* Because FileList is a set, these vectors are sorted in ascending order.
	 * */
	const std::vector<std::string> &get_files() override;
	const std::vector<std::string> &get_directories() override;

	/* InstalledPackageVersion interface */
	std::shared_ptr<PackageMetaData> get_mdata() override;

	/* Original methods */
	/* This will always return a valid pointer. If the package has no files, the
	 * list is simply empty. */
	std::shared_ptr<FileList> get_file_list();

	/* This will always return a valid pointer. If the package has no files, the
	 * list is simply empty. Files are sorted ascendingly by path. It's best to
	 * first access get_file_list because the file index comes before the config
	 * files in the transport form and hence avoids a potentially expensive seek
	 * in a compressed archive. */
	std::shared_ptr<std::vector<std::string>> get_config_files();

	/* These do only return something other than nullptr if the corresponding
	 * maintainer script is present. */
	std::shared_ptr<ManagedBuffer<char>> get_preinst();
	std::shared_ptr<ManagedBuffer<char>> get_configure();
	std::shared_ptr<ManagedBuffer<char>> get_unconfigure();
	std::shared_ptr<ManagedBuffer<char>> get_postrm();

	bool has_archive ();

	void clear_buffers();

	/* Take care: This calls exec and does NOT close fds before! */
	void unpack_archive_to_directory(
			const std::string& dst,
			std::vector<std::string>* excluded_paths);
};


class PackageProvider
{
private:
	std::shared_ptr<Parameters> params;
	std::vector<std::shared_ptr<Repository>> repositories;

	/* Constructor */
	PackageProvider (std::shared_ptr<Parameters> params);

public:
	/* Creator */
	static std::shared_ptr<PackageProvider> create (std::shared_ptr<Parameters> params);

	std::set<VersionNumber> list_package_versions (const std::string& name, const int architecture);

	std::shared_ptr<ProvidedPackage> get_package (
			const std::string& name, const int architecture, const VersionNumber& version);
};


#endif /* __PACKAGE_PROVIDER_H */
