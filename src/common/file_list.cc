#include <cerrno>
#include <cstring>
#include <filesystem>
#include <ios>
#include <system_error>
#include "file_list.h"
#include "message_digest.h"

extern "C" {
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
}

using namespace std;
namespace fs = std::filesystem;
namespace md = message_digest;


FileRecord::FileRecord()
{
}


FileRecord::FileRecord(uint8_t type, uint32_t uid, uint32_t gid, uint16_t mode,
		uint32_t size, uint8_t sha1_sum[], const string& path)
	:
		type(type), uid(uid), gid(gid), mode(mode), size(size), path(path)
{
	memcpy (this->sha1_sum, sha1_sum, sizeof (this->sha1_sum));
}


size_t FileRecord::binary_size() const
{
	return 1 + 4 + 4 + 2 + 4 + 20 + path.size() + 1;
}


void FileRecord::to_binary (uint8_t *buf) const
{
	*buf = type;
	*((uint32_t*) (buf + 0x01)) = htole32 (uid);
	*((uint32_t*) (buf + 0x05)) = htole32 (gid);
	*((uint16_t*) (buf + 0x09)) = htole16 (mode);
	*((uint32_t*) (buf + 0x0b)) = htole32 (size);
	memcpy (buf + 0x0f, sha1_sum, 20);
	strcpy ((char*) buf + 0x23, path.c_str());
	*(buf + 0x23 + path.size()) = 0;
}


void FileRecord::from_binary (const uint8_t *buf, size_t size, FileRecord& fr)
{
	if (size >= 0x24)
	{
		fr.type = *buf;
		fr.uid = le32toh (*((uint32_t*) (buf + 0x01)));
		fr.gid = le32toh (*((uint32_t*) (buf + 0x05)));
		fr.mode = le16toh (*((uint16_t*) (buf + 0x09)));
		fr.size = le32toh (*((uint32_t*) (buf + 0x0b)));

		memcpy (fr.sha1_sum, buf + 0xf, 20);
		fr.path = string ((const char*) (buf + 0x23), size - 0x24);
	}
}


bool FileRecord::operator< (const FileRecord& o) const
{
	return path < o.path;
}


bool FileRecord::non_existent_or_matches (const string& root, ostream *out) const
{
	const auto target_path = simplify_path (root + "/" + path);

	struct stat statbuf;

	int ret = lstat (target_path.c_str(), &statbuf);
	if (ret < 0)
	{
		if (errno == ENOENT || errno == ENOTDIR)
			return true;

		throw system_error (error_code (errno, generic_category()));
	}

	/* Compare attributes */
	/* Common attributes */
	if (uid != statbuf.st_uid)
	{
		if (out)
			*out << "UID differs: " << uid << " (pkg) != " << statbuf.st_uid << " (system)\n";
		return false;
	}
	
	if (gid != statbuf.st_gid)
	{
		if (out)
			*out << "GID differs: " << gid << " (pkg) != " << statbuf.st_gid << " (system)\n";
		return false;
	}
	
	if (mode != (statbuf.st_mode & 07777))
	{
		if (out)
		{
			*out << "Mode differs: " << oct << mode << " (pkg) != "
				<< (statbuf.st_mode & 07777) << dec << " (system)\n";
		}

		return false;
	}

	/* Type specific attributes (includes type ...) */
	switch (type)
	{
		case FILE_TYPE_REGULAR:
		{
			if (!S_ISREG (statbuf.st_mode))
			{
				if (out)
					*out << "Not a regular file\n";

				return false;
			}

			if (statbuf.st_size != size)
			{
				if (out)
					*out << "Size differs: " << size << " (pkg) != "
						<< statbuf.st_size << " (system)\n";

				return false;
			}

			char tmp[20];

			ret = md::sha1_file (target_path.c_str(), tmp);
			if (ret < 0)
				throw system_error (error_code (-ret, generic_category()));

			if (memcmp (tmp, sha1_sum, 20) != 0)
			{
				if (out)
				{
					*out << "SHA1 sum differs: " << sha1_to_string (sha1_sum) <<
						" (pkg) != " << sha1_to_string(tmp) << " (system)\n";
				}

				return false;
			}

			break;
		}

		case FILE_TYPE_DIRECTORY:
			if (!S_ISDIR (statbuf.st_mode))
			{
				if (out)
					*out << "Not a directory\n";

				return false;
			}

			break;

		case FILE_TYPE_LINK:
		{
			if (!S_ISLNK (statbuf.st_mode))
			{
				if (out)
					*out << "Not a link\n";

				return false;
			}

			auto lnk = convenient_readlink (target_path);

			if (lnk.size() != size)
			{
				if (out)
					*out << "Link target path length differs\n";

				return false;
			}

			char tmp[20];

			md::sha1_memory (lnk.c_str(), lnk.size(), tmp);

			if (memcmp (tmp, sha1_sum, 20) != 0)
			{
				if (out)
					*out << "Link target hash differs\n";

				return false;
			}

			break;
		}

		case FILE_TYPE_CHAR:
			if (!S_ISCHR (statbuf.st_mode))
			{
				if (out)
					*out << "Not a character device\n";

				return false;
			}

			break;

		case FILE_TYPE_BLOCK:
			if (!S_ISBLK (statbuf.st_mode))
			{
				if (out)
					*out << "Not a block device\n";

				return false;
			}

			break;

		case FILE_TYPE_SOCKET:
			if (!S_ISSOCK (statbuf.st_mode))
			{
				if (out)
					*out << "Not a socket\n";

				return false;
			}

			break;

		case FILE_TYPE_PIPE:
			if (!S_ISFIFO (statbuf.st_mode))
			{
				if (out)
					*out << "Not a fifo\n";

				return false;
			}

			break;

		default:
			/* Fail safe */
			throw gp_exception ("Invalid file type stored at FileRecord.");
	}

	return true;
}


FileRecord::~FileRecord()
{
}


DummyFileRecord::DummyFileRecord(const string& path)
{
	this->path = path;
}


void FileList::add_file (FileRecord&& file)
{
	files.emplace (move(file));
}


set<FileRecord>::const_iterator FileList::begin() const noexcept
{
	return files.cbegin();
}


set<FileRecord>::const_iterator FileList::end() const noexcept
{
	return files.cend();
}


set<FileRecord>::const_iterator FileList::find(const FileRecord& file) const
{
	return files.find(file);
}
