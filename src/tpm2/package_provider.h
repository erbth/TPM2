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


class ProvidedPackage
{
private:
	std::shared_ptr<PackageMetaData> mdata;

	/* Referring to the original transport form */
	TransportForm::TableOfContents toc;
	std::shared_ptr<TransportForm::ReadStream> rs;

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

	std::shared_ptr<PackageMetaData> get_mdata();

	std::shared_ptr<ManagedBuffer<char>> get_preinst();
	std::shared_ptr<ManagedBuffer<char>> get_configure();
	std::shared_ptr<ManagedBuffer<char>> get_unconfigure();
	std::shared_ptr<ManagedBuffer<char>> get_postrm();

	bool has_archive ();

	void clear_buffers();

	/* Keep care: This calls exec and does NOT close fds before! */
	void unpack_archive_to_directory(const std::string& dst);
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
