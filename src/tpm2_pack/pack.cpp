#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <tinyxml2.h>
#include <utility>
#include "pack.h"
#include "common_utilities.h"
#include "package_meta_data.h"
#include "file_wrapper.h"
#include "transport_form.h"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
}

using namespace std;
using namespace tinyxml2;
namespace fs = std::filesystem;
namespace tf = TransportForm;


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

		tf.set_postrm (postrm->buf, s);
	}


	/* Archive the package's content and write it to the transport form file */
	fs::path destdir_path = dir / "destdir";

	size_t archive_size = 0;
	DynamicBuffer<uint8_t> archive;

	if (fs::is_directory (destdir_path))
	{
		if (!create_tar_archive (dir, archive, archive_size))
			return false;

		if (archive_size > 0)
			tf.set_archive (archive.buf, archive_size);
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
		ssize_t r = read (pread, dst.buf, 20480);

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
