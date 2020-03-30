#include "package_db.h"
#include <algorithm>
#include <filesystem>
#include <vector>
#include "version_number.h"

#include <cstdio>

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

	/* Create the directory in which the database should reside if it does not
	 * already exist. */
	path += "var/lib/tpm";
	filesystem::create_directories (path);

	path += "/status.db";


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


	/* Create the database schema if required. */
	try
	{
		ensure_schema ();
	}
	catch (...)
	{
		sqlite3_close_v2(pDb);
		throw;
	}
}


PackageDB::~PackageDB()
{
	if (pDb)
	{
		sqlite3_close_v2(pDb);
	}
}


void PackageDB::begin()
{
	sqlite3_stmt *pStmt = nullptr;

	try
	{
		int err = sqlite3_prepare_v2 (pDb, "begin;", -1, &pStmt, nullptr);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize(pStmt);

		throw;
	}

	sqlite3_finalize(pStmt);
}


void PackageDB::rollback()
{
	sqlite3_stmt *pStmt = nullptr;

	try
	{
		int err = sqlite3_prepare_v2 (pDb, "rollback;", -1, &pStmt, nullptr);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize(pStmt);

		throw;
	}

	sqlite3_finalize(pStmt);
}


void PackageDB::commit()
{
	sqlite3_stmt *pStmt = nullptr;

	try
	{
		int err = sqlite3_prepare_v2 (pDb, "commit;", -1, &pStmt, nullptr);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize(pStmt);

		throw;
	}

	sqlite3_finalize(pStmt);
}


void PackageDB::ensure_schema()
{
	/* Find all relations in the database to see its state. */
	sqlite3_stmt *pStmt = nullptr;
	vector<string> relations;

	begin();

	try
	{
		int err = sqlite3_prepare_v2 (
				pDb,
				"select name from SQLITE_MASTER where type='table';",
				-1,
				&pStmt,
				nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		for (;;)
		{
			err = sqlite3_step (pStmt);

			if (err == SQLITE_DONE)
				break;
			else if (err != SQLITE_ROW)
				throw sqlitedb_exception (err, pDb);

			/* Process this tuple */
			if (sqlite3_column_count (pStmt) != 1)
				throw PackageDBException ("Invalid column cound in SQLITE_MASTER");

			relations.push_back ((const char*) sqlite3_column_text (pStmt, 0));
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		rollback();
		throw;
	}


	/* Is there a table named schema_version? Remember we're still an
	 * transaction ... */
	if (find (relations.cbegin(), relations.cend(),
				"schema_version") != relations.cend())
	{
		/* Ensure this is the right version. */
		VersionNumber v("0");

		try
		{
			int err = sqlite3_prepare (
					pDb,
					"select version from schema_version;",
					-1,
					&pStmt,
					nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_step (pStmt);

			if (err == SQLITE_DONE)
				throw PackageDBException("schema_version is empty.");
			else if (err != SQLITE_ROW)
				throw sqlitedb_exception (err, pDb);

			if (sqlite3_column_count (pStmt) != 1)
				throw PackageDBException("Wrong column count from schema_version");

			v = VersionNumber ((const char*) sqlite3_column_text (pStmt, 0));

			sqlite3_finalize (pStmt);
			pStmt = nullptr;
		}
		catch (...)
		{
			if (pStmt)
				sqlite3_finalize (pStmt);

			rollback();
			throw;
		}

		/* Nothing left to do. */
		commit();

		if (v != VersionNumber("1.0"))
		{
			throw PackageDBException (
					"Unsupported PackageDB version: " + v.to_string());
		}
	}
	else
	{
		/* Ensure that the database is empty */
		if (relations.size() > 0)
		{
			rollback();
			throw PackageDBException ("Database not empty though it has no "
					"schema_version");
		}


		/* Instantiate a new schema */
		try
		{
			int err = sqlite3_exec (pDb,
					"create table schema_version (version varchar primary key);",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_exec (pDb,
					"create table packages ("
						"name varchar,"
						"architecture integer,"
						"version varchar,"
						"source_version varchar not null,"
						"state integer not null,"
						"installation_reason integer not null,"
						"primary key (name, architecture, version));",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_exec (pDb,
					"create table files ("
						"path varchar,"
						"pkg_name varchar,"
						"pkg_architecture integer,"
						"pkg_version varchar,"
						"type integer,"
						"digest varchar not null,"
						"primary key (path, pkg_name, pkg_architecture, pkg_version),"
						"foreign key (pkg_name, pkg_architecture, pkg_version)"
							"references packages (name, architecture, version));",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);


			err = sqlite3_exec (pDb,
					"create table pre_dependencies ("
						"pkg_name varchar,"
						"pkg_architecture integer,"
						"pkg_version varchar,"
						"name varchar,"
						"architecture integer,"
						"constraints varchar not null,"
						"primary key (pkg_name, pkg_architecture, pkg_version, name, architecture),"
						"foreign key (pkg_name, pkg_architecture, pkg_version)"
							"references packages (name, architecture, version));",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);


			err = sqlite3_exec (pDb,
					"create table dependencies ("
						"pkg_name varchar,"
						"pkg_architecture integer,"
						"pkg_version varchar,"
						"name varchar,"
						"architecture integer,"
						"constraints varchar not null,"
						"primary key (pkg_name, pkg_architecture, pkg_version, name, architecture),"
						"foreign key (pkg_name, pkg_architecture, pkg_version)"
							"references packages (name, architecture, version));",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);


			/* Set schema version */
			err = sqlite3_exec (pDb,
					"insert into schema_version (version) values ('1.0');",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			commit();
		}
		catch (...)
		{
			rollback();
			throw;
		}
	}
}


vector<shared_ptr<PackageMetaData>> PackageDB::get_packages_in_state(const int state)
{
	sqlite3_stmt *pStmt = nullptr;

	vector<shared_ptr<PackageMetaData>> pkgs;

	try
	{
		int err;
		
		if (state == ALL_PKG_STATES)
		{
			err = sqlite3_prepare_v2 (pDb,
					"select name, architecture, version, source_version, installation_reason, state "
					"from packages;",
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);
		}
		else
		{
			err = sqlite3_prepare_v2 (pDb,
					"select name, architecture, version, source_version, installation_reason, state "
					"from packages where state=?;",
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);


			err = sqlite3_bind_int (
					pStmt,
					1,
					state);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);
		}


		for (;;)
		{
			err = sqlite3_step (pStmt);

			if (err == SQLITE_DONE)
			{
				break;
			}
			else if (err != SQLITE_ROW)
			{
				throw sqlitedb_exception (err, pDb);
			}
			else
			{
				if (sqlite3_column_count (pStmt) != 6)
					throw PackageDBException ("Invalid column count in get_packages_in_state");

				string name = (const char*) sqlite3_column_text (pStmt, 0);
				int architecture = sqlite3_column_int (pStmt, 1);
				VersionNumber v((const char*) sqlite3_column_text (pStmt, 2));
				VersionNumber sv((const char*) sqlite3_column_text (pStmt, 3));
				char reason = (char) sqlite3_column_int (pStmt, 4);
				int state = sqlite3_column_int (pStmt, 5);

				auto pkg = make_shared<PackageMetaData> (
						name, architecture, v, sv, reason, state);


				/* Get the pre-dependencies of this package */
				sqlite3_stmt *pStmt2 = nullptr;
				auto vs = v.to_string();


				try
				{
					err = sqlite3_prepare_v2 (pDb,
							"select name, architecture, constraints "
							"from pre_dependencies "
							"where pkg_name = ? and "
								"pkg_architecture = ? "
								"and pkg_version = ?;",
							-1,
							&pStmt2,
							nullptr);

					if (err != SQLITE_OK)
						throw sqlitedb_exception (err, pDb);


					err = sqlite3_bind_text (
							pStmt2,
							1,
							name.c_str(),
							name.size(),
							SQLITE_STATIC);

					if (err != SQLITE_OK)
						throw sqlitedb_exception (err, pDb);


					err = sqlite3_bind_int (
							pStmt2,
							2,
							architecture);

					if (err != SQLITE_OK)
						throw sqlitedb_exception (err, pDb);


					err = sqlite3_bind_text (
							pStmt2,
							3,
							vs.c_str(),
							vs.size(),
							SQLITE_STATIC);

					if (err != SQLITE_OK)
						throw sqlitedb_exception (err, pDb);


					for (;;)
					{
						err = sqlite3_step (pStmt2);

						if (err == SQLITE_DONE)
						{
							break;
						}
						else if (err != SQLITE_ROW)
						{
							throw sqlitedb_exception (err, pDb);
						}
						else
						{
							if (sqlite3_column_count (pStmt2) != 3)
								throw PackageDBException ("Invalid column "
										"count while selecting pre-dependencies");


							auto constraints = PackageConstraints::Formula::from_string (
									(const char*) sqlite3_column_text (pStmt2, 2));

							if (!constraints)
								throw PackageDBException ("Invalid constraint string \"" +
										string ((const char*) sqlite3_column_text (pStmt2, 2)));


							pkg->add_pre_dependency (Dependency (
									(const char*) sqlite3_column_text (pStmt2, 0),
									sqlite3_column_int (pStmt2, 1),
									constraints));
						}
					}

					sqlite3_finalize (pStmt2);
					pStmt2 = nullptr;
				}
				catch (...)
				{
					if (pStmt2)
						sqlite3_finalize (pStmt2);

					throw;
				}


				/* Get the dependencies of this package */
				try
				{
					err = sqlite3_prepare_v2 (pDb,
							"select name, architecture, constraints "
							"from dependencies "
							"where pkg_name = ? and "
								"pkg_architecture = ? "
								"and pkg_version = ?;",
							-1,
							&pStmt2,
							nullptr);

					if (err != SQLITE_OK)
						throw sqlitedb_exception (err, pDb);


					err = sqlite3_bind_text (
							pStmt2,
							1,
							name.c_str(),
							name.size(),
							SQLITE_STATIC);

					if (err != SQLITE_OK)
						throw sqlitedb_exception (err, pDb);


					err = sqlite3_bind_int (
							pStmt2,
							2,
							architecture);

					if (err != SQLITE_OK)
						throw sqlitedb_exception (err, pDb);


					err = sqlite3_bind_text (
							pStmt2,
							3,
							vs.c_str(),
							vs.size(),
							SQLITE_STATIC);

					if (err != SQLITE_OK)
						throw sqlitedb_exception (err, pDb);


					for (;;)
					{
						err = sqlite3_step (pStmt2);

						if (err == SQLITE_DONE)
						{
							break;
						}
						else if (err != SQLITE_ROW)
						{
							throw sqlitedb_exception (err, pDb);
						}
						else
						{
							if (sqlite3_column_count (pStmt2) != 3)
								throw PackageDBException ("Invalid column "
										"count while selecting dependencies");


							auto constraints = PackageConstraints::Formula::from_string (
									(const char*) sqlite3_column_text (pStmt2, 2));

							if (!constraints)
								throw PackageDBException ("Invalid constraint string \"" +
										string((const char*) sqlite3_column_text (pStmt2, 2)));


							pkg->add_dependency (Dependency (
									(const char*) sqlite3_column_text (pStmt2, 0),
									sqlite3_column_int (pStmt2, 1),
									constraints));
						}
					}

					sqlite3_finalize (pStmt2);
					pStmt2 = nullptr;
				}
				catch (...)
				{
					if (pStmt2)
						sqlite3_finalize (pStmt2);

					throw;
				}

				/* Add the package to the list to return */
				pkgs.push_back (pkg);
			}
		}

		sqlite3_finalize (pStmt);
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	return pkgs;
}


/********************************* Exceptions *********************************/
PackageDBException::PackageDBException ()
{
}

PackageDBException::PackageDBException (const std::string& msg)
	: msg(msg)
{
}

PackageDBException::~PackageDBException ()
{
}

const char* PackageDBException::what() const noexcept
{
	return msg.c_str();
}


sqlitedb_exception::sqlitedb_exception(int err, sqlite3 *db)
{
	msg = string(sqlite3_errstr(err));

	if (db)
		msg += ", " + string(sqlite3_errmsg(db));
}


CannotOpenDB::CannotOpenDB (int err, const string& path)
	: sqlitedb_exception (err)
{
	msg = "Failed to open database file \"" + path + "\": " + msg;
}
