#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <memory>
#include <optional>
#include <stack>
#include <tinyxml2.h>
#include <utility>
#include "pack.h"
#include "common_utilities.h"
#include "package_meta_data.h"
#include "file_wrapper.h"
#include "transport_form.h"
#include "message_digest.h"
#include "file_list.h"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
}

using namespace std;
using namespace tinyxml2;
namespace fs = std::filesystem;
namespace tf = TransportForm;
namespace md = message_digest;


bool pack (const string& _dir)
{
	/* Convert _dir to an absolute path and check if the directory exists. */
	fs::path dir = get_absolute_path (_dir);

	printf ("Packing the unpacked form of a package located at \"%s\"\n", dir.c_str());

	try
	{
		if (!fs::is_directory (dir))
		{
			fprintf (stderr, "No such directory.\n");
			return false;
		}
	}
	catch (fs::filesystem_error& e)
	{
		fprintf (stderr, "Cannot access directory: %s\n", e.what());
		return false;
	}


	/* Read desc.xml */
	shared_ptr<PackageMetaData> mdata;

	try
	{
		auto desc_path = dir / "desc.xml";

		FileWrapper f;

		int r = f.open (desc_path.c_str(), "r");
		if (r < 0)
		{
			fprintf (stderr, "Failed to open desc.xml: %s\n", strerror (-r));
			return false;
		}


		r = f.seek (0, SEEK_END);
		if (r < 0)
		{
			fprintf (stderr, "Failed to read desc.xml: %s\n", strerror (-r));
			return false;
		}

		long size = f.tell ();
		if (size < 0)
		{
			fprintf (stderr, "Failed to read desc.xml: %s\n", strerror ((int) -size));
			return false;
		}

		r = f.seek (0, SEEK_SET);
		if (r < 0)
		{
			fprintf (stderr, "Failed to read desc.xml: %s\n", strerror (-r));
			return false;
		}


		ManagedBuffer<char> buf(size);

		long cnt = f.read (buf.buf, size);

		if (cnt != size)
		{
			fprintf (stderr, "Failed to read desc.xml: (%ld / %ld read)\n", cnt, size);
			return false;
		}

		mdata = read_package_meta_data_from_xml (buf, size);
	}
	catch (invalid_package_meta_data_xml& e)
	{
		fprintf (stderr, "Error in desc.xml: %s\n", e.what());
		return false;
	}


	/* Print some information about the package */
	printf ("\n"
			"    Name:               %s\n"
			"    Architecture:       %s\n"
			"    Version:            %s\n"
			"    Source version:     %s\n\n",
			mdata->name.c_str(), Architecture::to_string(mdata->architecture).c_str(),
			mdata->version.to_string().c_str(), mdata->source_version.to_string().c_str());

	printf ("    Pre-dependencies:\n");
	for (auto& d : mdata->pre_dependencies)
	{
		printf ("      %s@%s\n",
				d.get_name().c_str(),
				Architecture::to_string(d.get_architecture()).c_str());
	}

	printf ("\n    Dependencies:\n");
	for (auto& d : mdata->dependencies)
	{
		printf ("      %s@%s\n",
				d.get_name().c_str(),
				Architecture::to_string(d.get_architecture()).c_str());
	}

	printf ("\n");


	/* Serialize XML DOM */
	tf::TransportForm tf;

	unique_ptr<XMLDocument> doc = mdata->to_xml();

	XMLPrinter printer;
	doc->Print (&printer);

	tf.set_desc (printer.CStr(), printer.CStrSize() - 1);


	/* Examine maintainer scripts */
	auto preinst_path = dir / "preinst";
	auto configure_path = dir / "configure";
	auto unconfigure_path = dir / "unconfigure";
	auto postrm_path = dir / "postrm";

	optional<ManagedBuffer<char>> preinst, configure, unconfigure, postrm;


	if (fs::is_regular_file (preinst_path))
	{
		ifstream i;

		i.open (preinst_path);
		if (!i.good())
		{
			fprintf (stderr, "Failed to open preinst: %s\n", strerror (errno));
			return false;
		}

		i.seekg (0, ifstream::end);
		if (!i.good())
		{
			fprintf (stderr, "Failed to seek to end of preinst: %s\n", strerror (errno));
			return false;
		}

		ssize_t s = i.tellg();
		if (s < 0)
		{
			fprintf (stderr, "Failed to get size of preinst: %s\n", strerror (errno));
			return false;
		}

		i.seekg (0, ifstream::beg);

		preinst.emplace (s);

		i.read (preinst->buf, s);

		if (i.gcount() != s)
		{
			fprintf (stderr, "Failed to read content of preinst.\n");
			return false;
		}

		printf ("    Have preinst\n");
		tf.set_preinst (preinst->buf, s);
	}


	if (fs::is_regular_file (configure_path))
	{
		ifstream i;

		i.open (configure_path);
		if (!i.good())
		{
			fprintf (stderr, "Failed to open configure: %s\n", strerror (errno));
			return false;
		}

		i.seekg (0, ifstream::end);
		if (!i.good())
		{
			fprintf (stderr, "Failed to seek to end of configure: %s\n", strerror (errno));
			return false;
		}

		ssize_t s = i.tellg();
		if (s < 0)
		{
			fprintf (stderr, "Failed to get size of configure: %s\n", strerror (errno));
			return false;
		}

		i.seekg (0, ifstream::beg);

		configure.emplace (s);

		i.read (configure->buf, s);

		if (i.gcount() != s)
		{
			fprintf (stderr, "Failed to read content of configure.\n");
			return false;
		}

		printf ("    Have configure\n");
		tf.set_configure (configure->buf, s);
	}


	if (fs::is_regular_file (unconfigure_path))
	{
		ifstream i;

		i.open (unconfigure_path);
		if (!i.good())
		{
			fprintf (stderr, "Failed to open unconfigure: %s\n", strerror (errno));
			return false;
		}

		i.seekg (0, ifstream::end);
		if (!i.good())
		{
			fprintf (stderr, "Failed to seek to end of unconfigure: %s\n", strerror (errno));
			return false;
		}

		ssize_t s = i.tellg();
		if (s < 0)
		{
			fprintf (stderr, "Failed to get size of unconfigure: %s\n", strerror (errno));
			return false;
		}

		i.seekg (0, ifstream::beg);

		unconfigure.emplace (s);

		i.read (unconfigure->buf, s);

		if (i.gcount() != s)
		{
			fprintf (stderr, "Failed to read content of unconfigure.\n");
			return false;
		}

		printf ("    Have unconfigure\n");
		tf.set_unconfigure (unconfigure->buf, s);
	}


	if (fs::is_regular_file (postrm_path))
	{
		ifstream i;

		i.open (postrm_path);
		if (!i.good())
		{
			fprintf (stderr, "Failed to open postrm: %s\n", strerror (errno));
			return false;
		}

		i.seekg (0, ifstream::end);
		if (!i.good())
		{
			fprintf (stderr, "Failed to seek to end of postrm: %s\n", strerror (errno));
			return false;
		}

		ssize_t s = i.tellg();
		if (s < 0)
		{
			fprintf (stderr, "Failed to get size of postrm: %s\n", strerror (errno));
			return false;
		}

		i.seekg (0, ifstream::beg);

		postrm.emplace (s);

		i.read (postrm->buf, s);

		if (i.gcount() != s)
		{
			fprintf (stderr, "Failed to read content of postrm.\n");
			return false;
		}

		printf ("    Have postrm\n");
		tf.set_postrm (postrm->buf, s);
	}


	/* Index and archive the files that will be part of the package */
	fs::path destdir_path = dir / "destdir";

	DynamicBuffer<uint8_t> file_index;
	size_t file_index_size = 0;

	DynamicBuffer<uint8_t> archive;
	size_t archive_size = 0;

	if (fs::is_directory (destdir_path))
	{
		if (!create_file_index (destdir_path, file_index, file_index_size))
			return false;

		if (!create_tar_archive (destdir_path, archive, archive_size))
			return false;

		if (archive_size > 0 && file_index_size > 0)
		{
			printf ("    Have archive\n");

			tf.set_file_index ((const char*) file_index.buf, file_index_size);
			tf.set_archive (archive.buf, archive_size);
		}
	}


	/* Write the package to a transport form file. */
	std::unique_ptr<tf::Writer> writer;

	try
	{
		writer = make_unique<tf::Writer> (tf::filename_from_mdata (*mdata));
	}
	catch (exception& e)
	{
		fprintf (stderr, "Failed to open transport form file: %s\n", e.what());
		return false;
	}


	int ret = tf.write (*writer);
	if (ret < 0)
	{
		fprintf (stderr, "Failed to write to transport form: %s\n", strerror (-ret));
		return false;
	}


	/* Optionally sign the file */

	return true;
}


bool create_file_index (const fs::path& dir, DynamicBuffer<uint8_t>& dst, size_t& size)
{
	struct path_component
	{
		fs::path location;
		fs::path virtual_path;
		DIR *dir;
	};

	bool success = true;
	stack<path_component> dirs;

	size = 0;

	/* Start with the root */
	{
		DIR *first_dir = opendir (dir.c_str());
		if (!first_dir)
		{
			fprintf (stderr, "Failed to index destdir: %s\n", strerror (errno));
			success = false;
		}

		dirs.push ({dir, "/", first_dir});
	}

	/* Traverse in DFS pre-order manner */
	while (success && dirs.size() > 0)
	{
		errno = 0;

		struct dirent *dent = readdir (dirs.top().dir);

		if (!dent && errno == 0)
		{
			/* Go up */
			closedir (dirs.top().dir);
			dirs.pop();
		}
		else if (!dent)
		{
			/* Error */
			fprintf (stderr, "Failed to open directory %s: %s\n",
					dirs.top().location.c_str(), strerror (errno));

			success = false;
		}
		else
		{
			/* Filter */
			if (strcmp (dent->d_name, ".") == 0 ||
					strcmp (dent->d_name, "..") == 0)
			{
				continue;
			}

			/* Index a file */
			fs::path elem_location = dirs.top().location / dent->d_name;
			fs::path elem_virtual_path = dirs.top().virtual_path / dent->d_name;

			struct stat statbuf;
			if (lstat (elem_location.c_str(), &statbuf) < 0)
			{
				fprintf (stderr, "Failed to stat %s: %s\n",
						elem_location.c_str(), strerror (errno));

				success = false;
				break;
			}

			if (S_ISDIR(statbuf.st_mode))
			{
				/* Index a directory: in order traversal + move down into it. */
				FileRecord rec;

				rec.type = FILE_TYPE_DIRECTORY;
				rec.uid = statbuf.st_uid;
				rec.gid = statbuf.st_gid;
				rec.mode = statbuf.st_mode & 07777;
				rec.size = 0;
				memset (rec.sha1_sum, 0, sizeof (rec.sha1_sum));
				rec.path = elem_virtual_path;

				dst.ensure_size (size + rec.binary_size());
				rec.to_binary (dst.buf + size);

				size += rec.binary_size();

				DIR *child = opendir (elem_location.c_str());
				if (!child)
				{
					fprintf (stderr, "Failed to open directory %s: %s\n",
							elem_location.c_str(), strerror (errno));

					success = false;
				}
				else
				{
					dirs.push ({elem_location, elem_virtual_path, child});
				}
			}
			else
			{
				FileRecord rec;

				switch (statbuf.st_mode & S_IFMT)
				{
					case S_IFSOCK:
						rec.type = FILE_TYPE_SOCKET;
						rec.size = 0;
						memset (rec.sha1_sum, 0, sizeof (rec.sha1_sum));
						break;

					case S_IFLNK:
					{
						rec.type = FILE_TYPE_LINK;

						string dst;

						try {
							dst = convenient_readlink (elem_location);
						} catch (exception& e) {
							fprintf (stderr, "Cannot read link %s: %s\n",
									elem_location.c_str(), e.what());

							success = false;
						}

						if (success)
						{
							rec.size = dst.size();
							md::sha1_memory (dst.c_str(), dst.size(), (char*) rec.sha1_sum);
						}

						break;
					}

					case S_IFREG:
					{
						rec.type = FILE_TYPE_REGULAR;

						int ret = md::sha1_file (elem_location.c_str(), (char*) rec.sha1_sum);
						if (ret < 0)
						{
							fprintf (stderr, "Cannot SHA1-hash file %s: %s\n",
									elem_location.c_str(), strerror (-ret));

							success = false;
						}

						if (success)
							rec.size = statbuf.st_size;

						break;
					}

					case S_IFBLK:
						rec.type = FILE_TYPE_BLOCK;
						rec.size = 0;
						memset (rec.sha1_sum, 0, sizeof (rec.sha1_sum));
						break;

					case S_IFCHR:
						rec.type = FILE_TYPE_CHAR;
						rec.size = 0;
						memset (rec.sha1_sum, 0, sizeof (rec.sha1_sum));
						break;

					case S_IFIFO:
						rec.type = FILE_TYPE_PIPE;
						rec.size = 0;
						memset (rec.sha1_sum, 0, sizeof (rec.sha1_sum));
						break;

					default:
						fprintf (stderr, "File %s has unknown type.\n",
								elem_location.c_str());

						success = false;
						break;
				}


				if (success)
				{
					rec.uid = statbuf.st_uid;
					rec.gid = statbuf.st_gid;
					rec.mode = statbuf.st_mode & 07777;
					rec.path = elem_virtual_path;

					dst.ensure_size (size + rec.binary_size());
					rec.to_binary (dst.buf + size);

					size += rec.binary_size();
					/* Index something else. */
				}
			}
		}
	}

	/* Clean up */
	while (dirs.size() > 0)
	{
		closedir (dirs.top().dir);
		dirs.pop();
	}

	if (!success)
		return false;

	return true;
}


bool create_tar_archive (const std::string& dir, DynamicBuffer<uint8_t>& dst, size_t& size)
{
	int pipefds[2];

	if (pipe (pipefds) < 0)
	{
		fprintf (stderr, "Failed to create pipe: %s\n", strerror (errno));
		return false;
	}

	pid_t pid  = fork ();

	if (pid < 0)
	{
		fprintf (stderr, "Failed to fork: %s\n", strerror (errno));
		return false;
	}

	if (pid == 0)
	{
		/* In the child process */
		dup2 (pipefds[1], STDOUT_FILENO);
		close (pipefds[1]);
		close (pipefds[0]);

		execlp ("tar", "tar", "-cC", dir.c_str(), ".", (char*) nullptr);

		printf ("Failed to exec tar: %s\n", strerror (errno));

		exit(1);
	}


	/* In the parent process */
	int pread = pipefds[0];
	close (pipefds[1]);

	size = 0;
	for (;;)
	{
		dst.ensure_size (size + 20480);
		ssize_t r = read (pread, dst.buf + size, 20480);

		if (r < 0)
		{
			fprintf (stderr, "Failed to read from pipe: %s\n", strerror (errno));
			close (pread);
			break;
		}

		if (r == 0)
		{
			close (pread);
			break;
		}

		size += r;
	}


	/* Wait for child to exit */
	int status;

	if (waitpid (pid, &status, 0) < 0)
	{
		fprintf (stderr, "Failed to wait for child: %s\n", strerror (errno));
		return false;
	}

	int exit_code = WEXITSTATUS (status);
	if (exit_code != 0)
	{
		fprintf (stderr, "Tar encountered an error and returned exit code %d.\n", exit_code);
		return false;
	}

	return true;
}
