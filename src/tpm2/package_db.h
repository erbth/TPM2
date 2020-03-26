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


class PackageDB
{
private:
	std::shared_ptr<Parameters> params;
	std::string path;

	sqlite3 *pDb = nullptr;

public:
	PackageDB(std::shared_ptr<Parameters> params);
	~PackageDB();
};


/********************************* Exceptions *********************************/
class sqlitedb_exception : public std::exception
{
protected:
	std::string msg;

public:
	sqlitedb_exception(int err, sqlite3 *db = nullptr);
	const char *what() const noexcept;
};


class CannotOpenDB : public sqlitedb_exception
{
public:
	CannotOpenDB (int err, const std::string &path);
};


#endif /* __PACKAGE_DB */
