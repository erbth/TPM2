/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module contains classes that represent a package's meta data. That is
 * everything except maintainer scripts (I borrowed this term from Debian) and
 * the actual file archive. */

#ifndef __PACKAGE_META_DATA_H
#define __PACKAGE_META_DATA_H

#include <exception>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <tinyxml2.h>
#include "architecture.h"
#include "dependencies.h"
#include "version_number.h"
#include "managed_buffer.h"


/* The _INVALID symbolic constants are good for parsing e.g. desc.xml. They
 * should not be stored in the db ... */
#define PKG_STATE_INVALID				0
#define PKG_STATE_WANTED				1
#define PKG_STATE_PREINST_BEGIN			2
#define PKG_STATE_UNPACK_BEGIN			3
#define PKG_STATE_CONFIGURE_BEGIN		4
#define PKG_STATE_CONFIGURED			5
#define PKG_STATE_UNCONFIGURE_BEGIN		6
#define PKG_STATE_RM_FILES_BEGIN		7
#define PKG_STATE_POSTRM_BEGIN			8
#define PKG_STATE_UNCONFIGURE_CHANGE	9
#define PKG_STATE_WAIT_NEW_UNPACKED		10
#define PKG_STATE_RM_FILES_CHANGE		11
#define PKG_STATE_POSTRM_CHANGE			12
#define PKG_STATE_PREINST_CHANGE		13
#define PKG_STATE_UNPACK_CHANGE			14
#define PKG_STATE_WAIT_OLD_REMOVED		15
#define PKG_STATE_CONFIGURE_CHANGE		16

/* Only for selecting packages */
#define ALL_PKG_STATES					1000


#define INSTALLATION_REASON_INVALID		0
#define INSTALLATION_REASON_AUTO		1
#define INSTALLATION_REASON_MANUAL		2


/* Different file types */
#define FILE_TYPE_REGULAR				0
#define FILE_TYPE_DIRECTORY				1
#define FILE_TYPE_LINK					2
#define FILE_TYPE_CHAR					3
#define FILE_TYPE_BLOCK					4
#define FILE_TYPE_SOCKET				5
#define FILE_TYPE_PIPE					6


/* This class represents a package in memory. It shall be returned by the
 * package providing module from repositories, depres shall use it and finally
 * the install module can find information about the package here. */
struct PackageMetaData
{
	const std::string name;
	const int architecture;
	const VersionNumber version;
	const VersionNumber source_version;


	DependencyList pre_dependencies;
	DependencyList dependencies;

	char installation_reason;
	int state;

	std::optional<std::vector<std::string>> interested_triggers;
	std::optional<std::vector<std::string>> activated_triggers;


	/* Private data to use by algorithms etc. Must not be relied on to be
	 * present after the function that uses them exits, however it is not
	 * altered by the graph's members. Only by third entities. */
	ssize_t algo_priv;


	/* Methods */
	PackageMetaData(const std::string &name, const int architecture,
			const VersionNumber &version, const VersionNumber &source_version,
			char installation_reason, int state);


	/* Just a convenience method with straight forward implementation */
	void add_pre_dependency(const Dependency&);
	void add_dependency(const Dependency&);


	/* Generate an XML DOM Document from it.
	 * Requires that trigger lists are set. */
	std::unique_ptr<tinyxml2::XMLDocument> to_xml() const;
};


/* Read the package metadata from an XML file represented by an istream. This
 * does always return a valid pointer (not nullptr) or throw an exception.
 *
 * This function cannot populate the installation_reason and the state. Hence
 * the corresponding invalid values are used. 
 *
 * @raises invalid_package_meta_data_xml */
std::shared_ptr<PackageMetaData> read_package_meta_data_from_xml (
		const ManagedBuffer<char>& buf, size_t size);


/***************************** Exceptions *************************************/
class invalid_package_meta_data_xml : public std::exception
{
protected:
	std::string msg;

public:
	invalid_package_meta_data_xml (const std::string& msg);

	const char* what() const noexcept override;
};

#endif /* __PACKAGE_META_DATA_H */
