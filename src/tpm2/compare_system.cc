#include <iostream>
#include <algorithm>
#include <filesystem>
#include <set>
#include <stack>
#include "compare_system.h"
#include "package_db.h"
#include "utility.h"
#include "message_digest.h"

extern "C"
{
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
}

using namespace std;
namespace fs = std::filesystem;
namespace md = message_digest;


void compare_file (shared_ptr<Parameters> params, const PackageDBFileEntry& fentry)
{
	const auto target_path = simplify_path (params->target + "/" + fentry.path);
	string prefix = fentry.path + ": ";

	struct stat statbuf;

	int ret = lstat (target_path.c_str(), &statbuf);
	if (ret < 0)
	{
		if (errno == ENOENT || errno == ENOTDIR)
		{
			cout << prefix << "Does not exist on system" << endl;
			return;
		}

		throw system_error (error_code (errno, generic_category()));
	}

	/* Type and checksum for regular files and links */
	switch (fentry.type)
	{
		case FILE_TYPE_REGULAR:
		{
			if (!S_ISREG (statbuf.st_mode))
			{
				cout << prefix <<  "Not a regular file" << endl;
				return;
			}

			char tmp[20];
			ret = md::sha1_file (target_path.c_str(), tmp);
			if (ret < 0)
				throw system_error (error_code (-ret, generic_category()));

			if (memcmp (tmp, fentry.sha1_sum, 20) != 0)
			{
				cout << prefix << "SHA1 sum differs: " << sha1_to_string (fentry.sha1_sum) <<
					" (db) != " << sha1_to_string(tmp) << " (system)" << endl;

				return;
			}

			break;
		}

		case FILE_TYPE_DIRECTORY:
			if (!S_ISDIR (statbuf.st_mode))
			{
					cout << prefix << "Not a directory" << endl;
					return;
			}

			break;

		case FILE_TYPE_LINK:
		{
			if (!S_ISLNK (statbuf.st_mode))
			{
				cout << prefix << "Not a link" << endl;
				return;
			}

			auto lnk = convenient_readlink (target_path);
			char tmp[20];

			md::sha1_memory (lnk.c_str(), lnk.size(), tmp);

			if (memcmp (tmp, fentry.sha1_sum, 20) != 0)
			{
				cout << prefix << "Link target hash differs" << endl;
				return;
			}

			break;
		}

		case FILE_TYPE_CHAR:
			if (!S_ISCHR (statbuf.st_mode))
			{
				cout << prefix << "Not a character device" << endl;
				return;
			}

			break;

		case FILE_TYPE_BLOCK:
			if (!S_ISBLK (statbuf.st_mode))
			{
				cout << prefix << "Not a block device" << endl;
				return;
			}

			break;

		case FILE_TYPE_SOCKET:
			if (!S_ISSOCK (statbuf.st_mode))
			{
				cout << prefix << "Not a socket" << endl;
				return;
			}

			break;

		case FILE_TYPE_PIPE:
			if (!S_ISFIFO (statbuf.st_mode))
			{
				cout << prefix << "Not a fifo" << endl;
				return;
			}

			break;

		default:
			/* Fail safe */
			throw gp_exception ("Invalid file type stored in DB file entry.");
	}
}


struct iter_stack_elem
{
	fs::path target_path;
	string rel_path;

	vector<fs::path> children;
	size_t pos = 0;

	iter_stack_elem(const fs::path& target_path, const string& rel_path)
		: target_path(target_path), rel_path(rel_path)
	{
		if (fs::is_directory(fs::symlink_status(target_path)))
		{
			for (const auto& child : fs::directory_iterator(target_path))
				children.push_back(child);

			sort (children.begin(), children.end());
		}
	}
};

bool compare_system (shared_ptr<Parameters> params)
{
	PackageDB pkgdb(params);
	set<string> file_set;

	/* Read all files from the database and compare the installed files to them
	 * */
	cout << "Comparing files in the database with the files on the system..." << endl;

	auto files_db = pkgdb.get_all_files_plain ();
	for (auto& fentry : files_db)
	{
		compare_file(params, fentry);
		file_set.insert(fentry.path);
	}

	/* Traverse the file system and identify files that are not in the db */
	cout << "\nSearching for files that are on the system but not in the database..." << endl;

	stack<iter_stack_elem> st;
	st.emplace(params->target, "/");

	while (st.size() > 0)
	{
		if (st.top().pos >= st.top().children.size())
		{
			st.pop();
			continue;
		}

		auto& path = st.top().children[st.top().pos++];
		iter_stack_elem file (path, simplify_path(st.top().rel_path + "/" + path.filename().string()));

		if (file_set.find(file.rel_path) == file_set.end())
		{
			cout << file.rel_path << endl;
			continue;
		}

		/* Recurse on directories */
		if (file.children.size() > 0)
			st.push(move(file));
	}

	return true;
}
