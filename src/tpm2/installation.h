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

/* Prototypes */
namespace depres {
	struct RemovalGraphBranch;
}

bool print_installation_graph(std::shared_ptr<Parameters> params);

/* @param upgrade  Upgrade the packages specified in @params (instruct the
 *                 solver to do so) or all packages if no packages are
 *                 specified. */
bool install_packages(std::shared_ptr<Parameters> params, bool upgrade);

bool set_installation_reason (char reason, std::shared_ptr<Parameters> params);

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
 * specified without change semantics.
 *
 * The package meta data of @param pp is not used as the package may already be
 * installed and have a different meta data associated with it. */
bool ll_run_preinst (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		std::shared_ptr<ProvidedPackage> pp,
		bool change,
		FileTrie<std::vector<PackageMetaData*>>* current_trie = nullptr);

/* The package meta data of @param pp is not used as the package may already be
 * installed and have a different meta data associated with it. */
bool ll_unpack (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		std::shared_ptr<ProvidedPackage> pp,
		bool change,
		FileTrie<std::vector<PackageMetaData*>>* current_trie);

/* Only on of @param pp and @param sms needs to be present. The package meta
 * data of @param is not used as the package is installed already and may have a
 * different meta data associated with it. */
bool ll_configure_package (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		std::shared_ptr<ProvidedPackage> pp,
		std::shared_ptr<StoredMaintainerScripts> sms,
		bool change);


bool ll_change_installation_reason (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		char reason);


/* This function ensured that the system is in an accepted state for package
 * installation. It prints errors directly to stderr. */
bool system_state_accepted_for_install (
		std::vector<std::shared_ptr<PackageMetaData>> installed_packages);


/***************************** Removing packages ******************************/
bool print_removal_graph (std::shared_ptr<Parameters> params, bool autoremove);
bool list_reverse_dependencies (std::shared_ptr<Parameters> params, bool transitive);

/* If autoremove is true, packages that are not required by manually installed
 * packages directly or indirectly after the specified packages were removed
 * will be removed, too. */
bool remove_packages (std::shared_ptr<Parameters> params, bool autoremove);

bool hl_remove_packages (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::vector<std::shared_ptr<PackageMetaData>>& installed_packages,
		depres::RemovalGraphBranch& g);

bool ll_unconfigure_package (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		StoredMaintainerScripts& sms,
		bool change);

/* If change is set to true, change semantics are applied. @param current_trie
 * must be a file trie, which must contain at least the files and directories of
 * the packages that superseed/replace the given one if change semantics are
 * used, and all directories of the packages that will finally be installed on
 * the system. However every element shall be represented by a file trie
 * directory to allow for migrating from files to directories and vice-versa.
 *
 * The vectors embedded into the file trie must contain all owners of a file
 * that fall under one of the two conditions stated above.
 *
 * The files of more packages may be added as well, but if files of the trie
 * contain references to packages that have been removed already, these files
 * won't be removed. Therefore, files and directories of removed packages should
 * not be in the trie. To maintain the current situation efficiently, the
 * function removes the package's references to files and directories from the
 * trie, and the files / directories if no references point to them anymore. */
bool ll_rm_files (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		bool change,
		FileTrie<std::vector<PackageMetaData*>>& current_trie);

bool ll_run_postrm (
		std::shared_ptr<Parameters> params,
		PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata,
		StoredMaintainerScripts& sms,
		bool change);


/*************************** Config file handling *****************************/
bool config_file_differs (std::shared_ptr<Parameters> params, const PackageDBFileEntry& file);


/******************************** Triggers ************************************/
void activate_package_triggers (std::shared_ptr<Parameters> params, PackageDB& pkgdb,
		std::shared_ptr<PackageMetaData> mdata);

/* Execute activated triggers. @returns true if successful */
bool execute_triggers (std::shared_ptr<Parameters> params, PackageDB& pkgdb);

#endif /* __INSTALLATION_H */
