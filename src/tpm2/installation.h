/** This file is part of the TSClient LEGACY Package Manager
 *
 * High level installation, reinstallation and upgrading of packages */
#ifndef __INSTALLATION_H
#define __INSTALLATION_H

#include <memory>
#include "parameters.h"
#include "package_meta_data.h"
#include "package_db.h"
#include "package_provider.h"
#include "stored_maintainer_scripts.h"
#include "file_trie.h"

bool print_installation_graph(std::shared_ptr<Parameters> params);

bool install_packages(std::shared_ptr<Parameters> params);

/* This function does not only run the package's preinst script, but also test
 * if its files are already present in the system and adopt them if required.
 * And it adds the package to the package database. If moreover @param
 * current_trie is specified, the packages files (including directories) are
 * added to it, if they are not contained already and do not belong to this
 * package. However all elements are added as directories to allow converting
 * from files to directories during upgrades and vice-versa.
 *
 * If @param change is true, change semantics are used. Then, @current_trie
 * needs to be specified and must contain at least the files of the packages to
 * superseed/replace. These will not be checked for existance and no adoption
 * will be used on them. In that case, the trie is augmented just like if it was
 * specified without change semantics. */
bool ll_run_preinst (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<ProvidedPackage> pp,
		bool change,
		FileTrie<std::vector<PackageMetaData*>>* current_trie = nullptr);

bool ll_unpack (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<ProvidedPackage> pp,
		bool change);

/* Only on of @param pp and @param sms needs to be present. */
bool ll_configure_package (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		std::shared_ptr<ProvidedPackage> pp,
		std::shared_ptr<StoredMaintainerScripts> sms,
		bool change);


bool ll_change_installation_reason (
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		char reason);


/***************************** Removing packages ******************************/
bool print_removal_graph (std::shared_ptr<Parameters> params);

bool remove_packages (std::shared_ptr<Parameters> params);

bool ll_unconfigure_package (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		StoredMaintainerScripts& sms,
		bool change);

/* If change is set to true, change semantics are applied. Then the user is
 * required to specify a file trie through @param current_trie, which must
 * contain at least the files and directories of the packages that
 * superseed/replace the given one. However every element shall be represented
 * by a file trie directory to allow for migrating from files to directories and
 * vice-versa. The vectors embedded into the file trie must contain all owners
 * of a file. */
bool ll_rm_files (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		bool change,
		FileTrie<std::vector<PackageMetaData*>>* current_trie = nullptr);

bool ll_run_postrm (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		StoredMaintainerScripts& sms,
		bool change);

#endif /* __INSTALLATION_H */
