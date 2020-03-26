#include "package_db.h"

using namespace std;


PackageDB::PackageDB(shared_ptr<Parameters> params)
	: params(params)
{
	if (params->target == "")
	{
		path = "";
	}
	else
	{
		path = params->target;
		
		if (path.back() != '/')
			path += '/';
	}

	path += "var/lib/tpm/status.db";


	/* Open or create the SQLite3 database */
	int err = sqlite3_open_v2 (
			path.c_str(),
			&pDb,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
			nullptr);

	if (err != SQLITE_OK)
	{
		sqlite3_close(pDb);
		throw CannotOpenDB(err, path);
	}
}


PackageDB::~PackageDB()
{
	if (pDb)
	{
		sqlite3_close_v2(pDb);
	}
}


/********************************* Exceptions *********************************/
sqlitedb_exception::sqlitedb_exception(int err, sqlite3 *db)
{
	msg = string(sqlite3_errstr(err));

	if (db)
		msg += ", " + string(sqlite3_errmsg(db));
}

const char *sqlitedb_exception::what() const noexcept
{
	return msg.c_str();
}


CannotOpenDB::CannotOpenDB (int err, const string& path)
	: sqlitedb_exception (err)
{
	msg = "Failed to open database file \"" + path + "\": " + msg;
}
