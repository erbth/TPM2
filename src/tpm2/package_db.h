/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module implements the package database. */

#ifndef __PACKAGE_DB
#define __PACKAGE_DB

#include <memory>
#include <string>
#include <sqlite3.h>
#include <exception>
#include "parameters.h"
#include "package_meta_data.h"


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


private:
	void begin();
	void rollback();
	void commit();

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
