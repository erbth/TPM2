/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module implements the package database.
 * In the context of the database `package' usually means a specific package
 * version. */

#ifndef __PACKAGE_DB
#define __PACKAGE_DB

#include <exception>
#include <list>
#include <vector>
#include <memory>
#include <string>
#include <tuple>
#include <optional>
#include <sqlite3.h>
#include "parameters.h"
#include "package_meta_data.h"
#include "file_list.h"
#include "version_number.h"


struct PackageDBFileEntry
{
	char type;
	std::string path;
	char sha1_sum[20] = { 0 };

	inline PackageDBFileEntry (char type, const std::string& path)
		: type(type), path(path)
	{ }
};


class PackageDB
{
private:
	std::shared_ptr<Parameters> params;
	std::string path;

	sqlite3 *pDb = nullptr;

public:
	/* This may create a new database if none is already present together with
	 * the directory it resides in. It does not applay specific permissions on
	 * the directories and the database file so the process's umask must be set
	 * accordingly. */
	PackageDB(std::shared_ptr<Parameters> params);
	~PackageDB();


	std::vector<std::shared_ptr<PackageMetaData>> get_packages_in_state(const int state);

	/* This updates or creates only the tuple in the packages relation, that is
	 * name, architecture, version, source version, state and installation
	 * reason. @returns true if the tuple was created, false if it was only
	 * updated. */
	bool update_or_create_package (std::shared_ptr<PackageMetaData> mdata);

	void update_state (std::shared_ptr<PackageMetaData> mdata);
	void update_installation_reason (std::shared_ptr<PackageMetaData> mdata);

	/* Sets the pre-dependencies and dependencies to the values given in the
	 * metadata object */
	void set_dependencies (std::shared_ptr<PackageMetaData> mdata);

	/* Neither parameter may be nullptr. The latter can, however, be an empty
	 * list. */
	void set_files (std::shared_ptr<PackageMetaData> mdata, std::shared_ptr<FileList> files);
	std::list<PackageDBFileEntry> get_files (std::shared_ptr<PackageMetaData> mdata);

	std::optional<PackageDBFileEntry> get_file (const PackageMetaData* mdata,
			const std::string& path);

	/* Neither parameter may be nullptr. The latter cann, howerver, be an empty
	 * vector. The retrieved file list is sorted by ascending pathname (with
	 * std::string::less). */
	void set_config_files (std::shared_ptr<PackageMetaData> mdata,
			std::shared_ptr<std::vector<std::string>> files);

	std::vector<std::string> get_config_files (std::shared_ptr<PackageMetaData> mdata);


	/* Delete a package version and all associated tuples. Does a lot, so it's
	 * better to call this from within a transaction. */
	void delete_package (std::shared_ptr<PackageMetaData> mdata);


	void begin();
	void rollback();
	void commit();

private:
	void ensure_schema();
};


/********************************* Exceptions *********************************/
class PackageDBException : public std::exception
{
protected:
	std::string msg;

public:
	PackageDBException ();
	PackageDBException (const std::string& msg);

	virtual ~PackageDBException();

	const char *what() const noexcept override;
};


class sqlitedb_exception : public PackageDBException
{
public:
	sqlitedb_exception(int err, sqlite3 *db = nullptr);
};


class CannotOpenDB : public sqlitedb_exception
{
public:
	CannotOpenDB (int err, const std::string &path);
};


#endif /* __PACKAGE_DB */
