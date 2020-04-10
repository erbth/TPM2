/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module is about storing maintainer scripts. */
#ifndef __STORED_MAINTAINER_SCRIPTS_H
#define __STORED_MAINTAINER_SCRIPTS_H

#include <filesystem>
#include <memory>
#include "managed_buffer.h"
#include "parameters.h"
#include "package_meta_data.h"
#include "transport_form.h"

/* To store maintainer scripts of installed packages. The file structure of
 * these archives is very similar to the transport form; so it will use the TOC
 * and reader / writer code of that. */
class StoredMaintainerScripts
{
private:
	TransportForm::TableOfContents toc;

	std::shared_ptr<Parameters> params;
	std::shared_ptr<PackageMetaData> mdata;

	std::shared_ptr<ManagedBuffer<char>> preinst;
	std::shared_ptr<ManagedBuffer<char>> configure;
	std::shared_ptr<ManagedBuffer<char>> unconfigure;
	std::shared_ptr<ManagedBuffer<char>> postrm;

	std::unique_ptr<TransportForm::ReadStream> rs;

	std::filesystem::path get_path () const;

	void ensure_read_stream();


public:
	/* This constructor will read the sms from fs */
	StoredMaintainerScripts (
			std::shared_ptr<Parameters> params,
			std::shared_ptr<PackageMetaData> mdata);

	StoredMaintainerScripts (
			std::shared_ptr<Parameters> params,
			std::shared_ptr<PackageMetaData> mdata,
			std::shared_ptr<ManagedBuffer<char>> preinst,
			std::shared_ptr<ManagedBuffer<char>> configure,
			std::shared_ptr<ManagedBuffer<char>> unconfigure,
			std::shared_ptr<ManagedBuffer<char>> postrm);

	std::shared_ptr<ManagedBuffer<char>> get_preinst();
	std::shared_ptr<ManagedBuffer<char>> get_configure();
	std::shared_ptr<ManagedBuffer<char>> get_unconfigure();
	std::shared_ptr<ManagedBuffer<char>> get_postrm();

	void clear_buffers();

	void write() const;
};

#endif /* __STORED_MAINTAINER_SCRIPTS_H */
