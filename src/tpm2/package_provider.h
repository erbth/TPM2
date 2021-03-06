/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains the package provider. */

#ifndef __PACKAGE_PROVIDER_H
#define __PACKAGE_PROVIDER_H

#include <memory>
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


class ProvidedPackage : public PackageVersion, public InstallationPackageVersion
{
private:
	std::shared_ptr<PackageMetaData> mdata;

	/* Referring to the original transport form */
	TransportForm::TableOfContents toc;
	std::shared_ptr<TransportForm::ReadStream> rs;

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

public:
	/* A constructor */
	ProvidedPackage(
			std::shared_ptr<PackageMetaData> mdata,
			const TransportForm::TableOfContents& toc,
			std::shared_ptr<TransportForm::ReadStream> rs);

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
