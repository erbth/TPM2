#include "package_db.h"
#include <algorithm>
#include <filesystem>
#include <vector>

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

	/* If even WAL is too slow, I may consider using MEMORY at the cost of worse
	 * db integrity. */
	err = sqlite3_exec (pDb, "pragma journal_mode = WAL;", nullptr, nullptr, nullptr);
	if (err != SQLITE_OK)
	{
		sqlite3_close(pDb);
		throw sqlitedb_exception (err, pDb);
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
				throw PackageDBException ("Invalid column count in SQLITE_MASTER");

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


	/* Is there a table named schema_version? Remember we're still in a
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

		if (v != VersionNumber("1.2"))
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
						"type integer not null,"
						"digest blob not null,"
						"primary key (path, pkg_name, pkg_architecture, pkg_version),"
						"foreign key (pkg_name, pkg_architecture, pkg_version) "
							"references packages (name, architecture, version) "
							"on update cascade on delete cascade);",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_exec (pDb,
					"create table config_files ("
						"path varchar,"
						"pkg_name varchar,"
						"pkg_architecture integer,"
						"pkg_version varchar,"
						"primary key (path, pkg_name, pkg_architecture, pkg_version),"
						"foreign key (pkg_name, pkg_architecture, pkg_version) "
							"references packages (name, architecture, version) "
							"on update cascade on delete cascade);",
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
						"foreign key (pkg_name, pkg_architecture, pkg_version) "
							"references packages (name, architecture, version) "
							"on update cascade on delete cascade);",
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
						"foreign key (pkg_name, pkg_architecture, pkg_version) "
							"references packages (name, architecture, version) "
							"on update cascade on delete cascade);",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);


			/* Triggers */
			err = sqlite3_exec (pDb,
					"create table triggers_activate ("
						"pkg_name varchar,"
						"pkg_architecture integer,"
						"pkg_version varchar,"
						"trigger varchar,"
						"primary key (pkg_name, pkg_architecture, pkg_version, trigger),"
						"foreign key (pkg_name, pkg_architecture, pkg_version) "
							"references packages (name, architecture, version) "
							"on update cascade on delete cascade);",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_exec (pDb,
					"create table triggers_interest ("
						"pkg_name varchar,"
						"pkg_architecture integer,"
						"pkg_version varchar,"
						"trigger varchar,"
						"primary key (pkg_name, pkg_architecture, pkg_version, trigger),"
						"foreign key (pkg_name, pkg_architecture, pkg_version) "
							"references packages (name, architecture, version) "
							"on update cascade on delete cascade);",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_exec (pDb,
					"create index triggers_interest_index "
					"on triggers_interest ("
						"trigger);",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_exec (pDb,
					"create table triggers_activated ("
						"trigger varchar,"
						"primary key (trigger));",
					nullptr, nullptr, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);


			/* Set schema version */
			err = sqlite3_exec (pDb,
					"insert into schema_version (version) values ('1.2');",
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


shared_ptr<PackageMetaData> PackageDB::get_reduced_package (
		const string& name, const int architecture, const VersionNumber& version)
{
	shared_ptr<PackageMetaData> pkg;
	sqlite3_stmt *pStmt = nullptr;

	string version_string(version.to_string());

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"select name, architecture, version, source_version, installation_reason, state "
				"from packages "
				"where name = ?1 and architecture = ?2 and version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, name.c_str(), name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err == SQLITE_ROW)
		{
			if (sqlite3_column_count (pStmt) != 6)
				throw PackageDBException ("Invalid column gound in get_reduced_package");

			pkg = make_shared<PackageMetaData> (
					(const char*) sqlite3_column_text (pStmt, 0),
					sqlite3_column_int (pStmt, 1),
					VersionNumber ((const char*) sqlite3_column_text (pStmt, 2)),
					VersionNumber ((const char*) sqlite3_column_text (pStmt, 3)),
					(char) sqlite3_column_int (pStmt, 4),
					sqlite3_column_int (pStmt, 5));
		}
		else if (err != SQLITE_DONE)
		{
			throw sqlitedb_exception (err, pDb);
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	return pkg;
}


bool PackageDB::update_or_create_package (shared_ptr<PackageMetaData> mdata)
{
	sqlite3_stmt *pStmt = nullptr;
	bool created;

	try
	{
		int err = sqlite3_prepare_v2 (pDb,
				"select count(*) from packages p "
				"where p.name = ?1 and p.architecture = ?2 and p.version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, mdata->version.to_string().c_str(), -1, SQLITE_TRANSIENT);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_ROW)
			throw sqlitedb_exception (err, pDb);

		if (sqlite3_column_count (pStmt) != 1)
			throw PackageDBException ("Invalid columns count while determining if a package exists already.");

		int cnt = sqlite3_column_int (pStmt, 0);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		if (cnt == 0)
		{
			err = sqlite3_prepare_v2 (pDb,
					"insert into packages "
					"(name, architecture, version , source_version, state, installation_reason) "
					"values (?1, ?2, ?3, ?4, ?5, ?6);",
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 3, mdata->version.to_string().c_str(), -1, SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 4, mdata->source_version.to_string().c_str(), -1, SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 5, mdata->state);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 6, mdata->installation_reason);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_step (pStmt);
			if (err != SQLITE_DONE)
				throw sqlitedb_exception (err, pDb);

			sqlite3_finalize (pStmt);
			pStmt = nullptr;

			created = true;
		}
		else
		{
			err = sqlite3_prepare_v2 (pDb,
					"update packages "
					"set source_version = ?4, state = ?5, installation_reason = ?6 "
					"where name = ?1 and architecture = ?2 and version = ?3;",
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 3, mdata->version.to_string().c_str(), -1, SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 4, mdata->source_version.to_string().c_str(), -1, SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 5, mdata->state);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 6, mdata->installation_reason);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_step (pStmt);
			if (err != SQLITE_DONE)
				throw sqlitedb_exception (err, pDb);

			sqlite3_finalize (pStmt);
			pStmt = nullptr;

			created = false;
		}
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	set_interested_triggers (mdata);
	set_activating_triggers (mdata);

	return created;
}


void PackageDB::update_state (shared_ptr<PackageMetaData> mdata)
{
	sqlite3_stmt *pStmt = nullptr;
	const string& version_string = mdata->version.to_string();

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"update packages set state = ?4 "
				"where name = ?1 and architecture = ?2 and version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 4, mdata->state);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


void PackageDB::update_installation_reason (shared_ptr<PackageMetaData> mdata)
{
	sqlite3_stmt *pStmt = nullptr;
	const string& version_string = mdata->version.to_string();

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"update packages set installation_reason = ?4 "
				"where name = ?1 and architecture = ?2 and version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 4, mdata->installation_reason);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


void PackageDB::set_dependencies (shared_ptr<PackageMetaData> mdata)
{
	sqlite3_stmt *pStmt = nullptr;
	const string& version_string = mdata->version.to_string();

	const char *delete_statements[2] = {
		"delete from pre_dependencies "
		"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
		"delete from dependencies "
		"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;"
	};

	const char *insert_statements[2] = {
		"insert into pre_dependencies "
		"(pkg_name, pkg_architecture, pkg_version, name, architecture, constraints) "
		"values (?1, ?2, ?3, ?4, ?5, ?6);",
		"insert into dependencies "
		"(pkg_name, pkg_architecture, pkg_version, name, architecture, constraints) "
		"values (?1, ?2, ?3, ?4, ?5, ?6);"
	};

	try
	{
		for (unsigned char i = 0; i < 2; i++)
		{
			auto err = sqlite3_prepare_v2 (pDb,
					delete_statements[i],
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_step (pStmt);
			if (err != SQLITE_DONE)
				throw sqlitedb_exception (err, pDb);

			sqlite3_finalize (pStmt);
			pStmt = nullptr;


			const auto& deps = i ? mdata->dependencies : mdata->pre_dependencies;
			for (auto j = deps.cbegin(); j != deps.cend(); j++)
			{
				const auto& dep = *j;

				err = sqlite3_prepare_v2 (pDb,
						insert_statements[i],
						-1, &pStmt, nullptr);

				if (err != SQLITE_OK)
					throw sqlitedb_exception (err, pDb);

				err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
				if (err != SQLITE_OK)
					throw sqlitedb_exception (err, pDb);

				err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
				if (err != SQLITE_OK)
					throw sqlitedb_exception (err, pDb);

				err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
				if (err != SQLITE_OK)
					throw sqlitedb_exception (err, pDb);

				err = sqlite3_bind_text (pStmt, 4,
						dep.identifier.first.c_str(), dep.identifier.first.size(),
						SQLITE_STATIC);

				if (err != SQLITE_OK)
					throw sqlitedb_exception (err, pDb);

				err = sqlite3_bind_int (pStmt, 5, dep.identifier.second);
				if (err != SQLITE_OK)
					throw sqlitedb_exception (err, pDb);

				const string& constraints = dep.version_formula ? dep.version_formula->to_string() : string();

				/* Transient because I'm not sure if the string has to survive
				 * until sqlite3_finalize. */
				err = sqlite3_bind_text (pStmt, 6, constraints.c_str(), constraints.size(), SQLITE_TRANSIENT);
				if (err != SQLITE_OK)
					throw sqlitedb_exception (err, pDb);

				err = sqlite3_step (pStmt);
				if (err != SQLITE_DONE)
					throw sqlitedb_exception (err, pDb);

				sqlite3_finalize (pStmt);
				pStmt = nullptr;
			}
		}
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


void PackageDB::set_files (shared_ptr<PackageMetaData> mdata, shared_ptr<FileList> files)
{
	sqlite3_stmt *pStmt = nullptr;
	const string& version_string = mdata->version.to_string();

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"delete from files "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step  (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		for (const auto& file : *files)
		{
			err = sqlite3_prepare_v2 (pDb,
					"insert into files "
					"(path, pkg_name, pkg_architecture, pkg_version, type, digest) "
					"values (?1, ?2, ?3, ?4, ?5, ?6);",
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			/* Again, not sure if the path would have to survive into the
			 * potential catch block. */
			err = sqlite3_bind_text (pStmt, 1, file.path.c_str(), file.path.size(), SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 2, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 3, mdata->architecture);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 4, version_string.c_str(), version_string.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 5, file.type);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_blob (pStmt, 6, file.sha1_sum, 20, SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_step (pStmt);
			if (err != SQLITE_DONE)
				throw sqlitedb_exception (err, pDb);

			sqlite3_finalize (pStmt);
			pStmt = nullptr;
		}
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


list<PackageDBFileEntry> PackageDB::get_files (shared_ptr<PackageMetaData> mdata)
{
	list<PackageDBFileEntry> files;
	sqlite3_stmt *pStmt = nullptr;

	const std::string& version_string = mdata->version.to_string();

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"select path, type, digest from files "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);


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
				if (sqlite3_column_count (pStmt) != 3)
					throw PackageDBException ("Invalid column count while reading files.");

				files.emplace_back (
						sqlite3_column_int (pStmt, 1),
						(const char*) sqlite3_column_text (pStmt, 0));

				/* Read file digest */
				auto digest_size = sqlite3_column_bytes(pStmt, 2);
				if (digest_size != 0)
				{
					if (digest_size != 20)
						throw PackageDBException ("Invalid digest length while reading files.");

					memcpy (files.back().sha1_sum, sqlite3_column_blob (pStmt, 2), 20);
				}
			}
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	return files;
}


optional<PackageDBFileEntry> PackageDB::get_file (const PackageMetaData* mdata,
		const string& path)
{
	optional<PackageDBFileEntry> file;
	sqlite3_stmt *pStmt = nullptr;

	const std::string& version_string = mdata->version.to_string();

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"select type, digest from files "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3 and path = ?4;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 4, path.c_str(), path.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);


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
				if (sqlite3_column_count (pStmt) != 2)
					throw PackageDBException ("Invalid column count while reading file.");

				file.emplace (
						sqlite3_column_int (pStmt, 0),
						path);

				/* Read file digest */
				auto digest_size = sqlite3_column_bytes(pStmt, 1);
				if (digest_size != 0)
				{
					if (digest_size != 20)
						throw PackageDBException ("Invalid digest length while reading files.");

					memcpy (file->sha1_sum, sqlite3_column_blob (pStmt, 1), 20);
				}

				break;
			}
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	return file;
}


void PackageDB::set_config_files (shared_ptr<PackageMetaData> mdata, shared_ptr<vector<string>> files)
{
	sqlite3_stmt *pStmt = nullptr;
	const string& version_string = mdata->version.to_string();

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"delete from config_files "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step  (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		for (const auto& file : *files)
		{
			err = sqlite3_prepare_v2 (pDb,
					"insert into config_files "
					"(path, pkg_name, pkg_architecture, pkg_version) "
					"values (?1, ?2, ?3, ?4);",
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			/* Again, not sure if the path would have to survive into the
			 * potential catch block. */
			err = sqlite3_bind_text (pStmt, 1, file.c_str(), file.size(), SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 2, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 3, mdata->architecture);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 4, version_string.c_str(), version_string.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_step (pStmt);
			if (err != SQLITE_DONE)
				throw sqlitedb_exception (err, pDb);

			sqlite3_finalize (pStmt);
			pStmt = nullptr;
		}
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


vector<string> PackageDB::get_config_files(shared_ptr<PackageMetaData> mdata)
{
	vector<string> files;
	sqlite3_stmt *pStmt = nullptr;

	const std::string& version_string = mdata->version.to_string();

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"select path from files "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);


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
				if (sqlite3_column_count (pStmt) != 1)
					throw PackageDBException ("Invalid column count while reading config files.");

				files.emplace_back ((const char*) sqlite3_column_text (pStmt, 0));
			}
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	sort(files.begin(), files.end());
	return files;
}


vector<PackageDBFileEntry> PackageDB::get_all_files_plain ()
{
	vector<PackageDBFileEntry> files;
	sqlite3_stmt *pStmt = nullptr;

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"select path, type, digest from files order by path;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);


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
				if (sqlite3_column_count (pStmt) != 3)
					throw PackageDBException ("Invalid column count while reading files.");

				files.emplace_back (
						sqlite3_column_int (pStmt, 1),
						(const char*) sqlite3_column_text (pStmt, 0));

				/* Read file digest */
				auto digest_size = sqlite3_column_bytes(pStmt, 2);
				if (digest_size != 0)
				{
					if (digest_size != 20)
						throw PackageDBException ("Invalid digest length while reading files.");

					memcpy (files.back().sha1_sum, sqlite3_column_blob (pStmt, 2), 20);
				}
			}
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	return files;
}


void PackageDB::set_interested_triggers (shared_ptr<PackageMetaData> mdata)
{
	sqlite3_stmt *pStmt = nullptr;
	const string& version_string = mdata->version.to_string();

	if (!mdata->interested_triggers)
	{
		throw PackageDBException ("set_interested_triggers called with a mdata "
				"without an interested triggers list.");
	}

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"delete from triggers_interest "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step  (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		for (const auto& trigger : *mdata->interested_triggers)
		{
			err = sqlite3_prepare_v2 (pDb,
					"insert into triggers_interest "
					"(pkg_name, pkg_architecture, pkg_version, trigger) "
					"values (?1, ?2, ?3, ?4);",
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 4, trigger.c_str(), trigger.size(), SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_step (pStmt);
			if (err != SQLITE_DONE)
				throw sqlitedb_exception (err, pDb);

			sqlite3_finalize (pStmt);
			pStmt = nullptr;
		}
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


void PackageDB::set_activating_triggers (shared_ptr<PackageMetaData> mdata)
{
	sqlite3_stmt *pStmt = nullptr;
	const string& version_string = mdata->version.to_string();

	if (!mdata->activated_triggers)
	{
		throw PackageDBException ("set_activating_triggers called with a mdata "
				"without an activated triggers list.");
	}

	try
	{
		auto err = sqlite3_prepare_v2 (pDb,
				"delete from triggers_activate "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step  (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		for (const auto& trigger : *mdata->activated_triggers)
		{
			err = sqlite3_prepare_v2 (pDb,
					"insert into triggers_activate "
					"(pkg_name, pkg_architecture, pkg_version, trigger) "
					"values (?1, ?2, ?3, ?4);",
					-1, &pStmt, nullptr);

			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_bind_text (pStmt, 4, trigger.c_str(), trigger.size(), SQLITE_TRANSIENT);
			if (err != SQLITE_OK)
				throw sqlitedb_exception (err, pDb);

			err = sqlite3_step (pStmt);
			if (err != SQLITE_DONE)
				throw sqlitedb_exception (err, pDb);

			sqlite3_finalize (pStmt);
			pStmt = nullptr;
		}
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


void PackageDB::delete_package (shared_ptr<PackageMetaData> mdata)
{
	sqlite3_stmt *pStmt = nullptr;
	const string& version_string = mdata->version.to_string();

	try
	{
		/* Delete files */
		auto err = sqlite3_prepare_v2 (pDb,
				"delete from files "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		/* Delete config files */
		err = sqlite3_prepare_v2 (pDb,
				"delete from config_files "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		/* Delete dependencies */
		err = sqlite3_prepare_v2 (pDb,
				"delete from dependencies "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		/* Delete pre-dependencies */
		err = sqlite3_prepare_v2 (pDb,
				"delete from pre_dependencies "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		/* Delete refrences to triggers */
		err = sqlite3_prepare_v2 (pDb,
				"delete from triggers_activate "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;

		err = sqlite3_prepare_v2 (pDb,
				"delete from triggers_interest "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;


		/* Finally delete the package's main tuple. */
		err = sqlite3_prepare_v2 (pDb,
				"delete from packages "
				"where name = ?1 and architecture = ?2 and version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


void PackageDB::ensure_activating_triggers_read (shared_ptr<PackageMetaData> mdata)
{
	/* Only read activating triggers if they are not present yet */
	if (mdata->activated_triggers)
		return;

	vector<string> triggers;
	sqlite3_stmt *pStmt = nullptr;

	const std::string& version_string = mdata->version.to_string();

	try
	{
		mdata->activated_triggers.emplace();

		auto err = sqlite3_prepare_v2 (pDb,
				"select trigger from triggers_activate "
				"where pkg_name = ?1 and pkg_architecture = ?2 and pkg_version = ?3;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, mdata->name.c_str(), mdata->name.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_int (pStmt, 2, mdata->architecture);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 3, version_string.c_str(), version_string.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);


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
				if (sqlite3_column_count (pStmt) != 1)
					throw PackageDBException ("Invalid column count while reading activating triggers.");

				mdata->activated_triggers->emplace_back ((const char*) sqlite3_column_text (pStmt, 0));
			}
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		/* Transactionality */
		mdata->activated_triggers = nullopt;

		throw;
	}
}


void PackageDB::activate_trigger(const string& trigger)
{
	sqlite3_stmt *pStmt = nullptr;

	try
	{
		int err = sqlite3_prepare_v2 (pDb,
				"insert into triggers_activated "
				"(trigger) values (?1) "
				"on conflict do nothing;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, trigger.c_str(), trigger.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


vector<string> PackageDB::get_activated_triggers ()
{
	vector<string> triggers;
	sqlite3_stmt *pStmt = nullptr;

	try
	{
		int err = sqlite3_prepare_v2 (pDb,
				"select trigger from triggers_activated;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

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
				if (sqlite3_column_count (pStmt) != 1)
					throw PackageDBException ("Invalid column count while reading activated triggers.");

				triggers.emplace_back ((const char*) sqlite3_column_text (pStmt, 0));
			}
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	return triggers;
}


vector<tuple<string, int, VersionNumber>> PackageDB::find_packages_interested_in_trigger (
		const string& trigger)
{
	vector<tuple<string, int, VersionNumber>> pkgs;
	sqlite3_stmt *pStmt = nullptr;

	try
	{
		int err = sqlite3_prepare_v2 (pDb,
				"select pkg_name, pkg_architecture, pkg_version "
				"from triggers_interest "
				"where trigger = ?1;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, trigger.c_str(), trigger.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

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
				if (sqlite3_column_count (pStmt) != 3)
					throw PackageDBException (
							"Invalid column count while finding packages interested in a trigger.");

				pkgs.emplace_back (
						(const char*) sqlite3_column_text (pStmt, 0),
						sqlite3_column_int (pStmt, 1),
						VersionNumber ((const char*) sqlite3_column_text (pStmt, 2)));
			}
		}

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}

	return pkgs;
}


void PackageDB::clear_trigger (const string& trigger)
{
	sqlite3_stmt *pStmt = nullptr;

	try
	{
		int err = sqlite3_prepare_v2 (pDb,
				"delete from triggers_activated "
				"where trigger = ?1;",
				-1, &pStmt, nullptr);

		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_bind_text (pStmt, 1, trigger.c_str(), trigger.size(), SQLITE_STATIC);
		if (err != SQLITE_OK)
			throw sqlitedb_exception (err, pDb);

		err = sqlite3_step (pStmt);
		if (err != SQLITE_DONE)
			throw sqlitedb_exception (err, pDb);

		sqlite3_finalize (pStmt);
		pStmt = nullptr;
	}
	catch (...)
	{
		if (pStmt)
			sqlite3_finalize (pStmt);

		throw;
	}
}


/********************************* Exceptions *********************************/
PackageDBException::PackageDBException ()
{
}

PackageDBException::PackageDBException (const string& msg)
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
