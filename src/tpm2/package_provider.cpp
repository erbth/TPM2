#include <cerrno>
#include <cstring>
#include <system_error>
#include "package_provider.h"
#include "directory_repository.h"
#include "common_utilities.h"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
}

using namespace std;
namespace tf = TransportForm;


ProvidedPackage::ProvidedPackage (
		shared_ptr<PackageMetaData> md,
		const tf::TableOfContents& toc,
		shared_ptr<tf::ReadStream> rs)
	: md(md), toc(toc), rs(rs)
{
	if (!md || !rs)
		throw invalid_argument ("md or rs nullptr");
}


void ProvidedPackage::ensure_read_stream()
{
	if (!rs)
	{
		throw invalid_argument ("Reopening read streams is not implemented yet.");
	}
}


shared_ptr<ManagedBuffer<char>> ProvidedPackage::get_configure()
{
	if (!configure)
	{
		for (auto& sec : toc.sections)
		{
			if (sec.type == tf::SEC_TYPE_CONFIGURE)
			{
				ensure_read_stream();

				if (rs->tell() != sec.start)
					rs->seek(sec.start);

				configure = make_shared<ManagedBuffer<char>>(sec.size);
				rs->read (configure->buf, sec.size);
				break;
			}
		}
	}

	return configure;
}


void ProvidedPackage::clear_buffers()
{
	rs = nullptr;
	configure = nullptr;
}


void ProvidedPackage::unpack_archive_to_directory(const string& dst)
{
	size_t archive_size = 0;

	for (auto& sec : toc.sections)
	{
		if (sec.type == tf::SEC_TYPE_ARCHIVE)
		{
			/* Seek to start of archive */
			ensure_read_stream();

			if (rs->tell() != sec.start)
				rs->seek (sec.start);

			archive_size = sec.size;
			break;
		}
	}


	/* Start Tar as subprocess */
	int pipefds[2];
	int ret = pipe (pipefds);
	if (ret < 0)
	{
		fprintf (stderr, "Failed to create pipe.\n");
		throw system_error (error_code (errno, generic_category()));
	}

	pid_t pid = fork ();
	if (pid < 0)
	{
		close (pipefds[0]);
		close (pipefds[1]);

		fprintf (stderr, "Failed to fork.\n");
		throw system_error (error_code (errno, generic_category()));
	}

	if (pid == 0)
	{
		/* In the child */
		ret = dup2 (STDIN_FILENO, pipefds[0]);
		if (ret < 0)
		{
			fprintf (stderr, "Failed to replace stdin: %s.\n", strerror (errno));
			exit (1);
		}

		close (pipefds[0]);
		close (pipefds[1]);

		execlp ("tar", "tar", "-xC", dst.c_str(), nullptr);

		fprintf (stderr, "Failed to exec tar: %s.\n", strerror (errno));
		exit (1);
	}


	/* Communicate */
	close (pipefds[0]);
	int write_end = pipefds[1];

	size_t cnt_total = 0;
	size_t to_read;
	ssize_t cnt_written;
	size_t pos;
	char buf[8192];

	try
	{
		for (;;)
		{
			to_read = MIN (8192, archive_size - cnt_total);
			rs->read (buf, to_read);

			for (pos = 0; pos < to_read; pos++)
			{
				cnt_written = write (write_end, buf + pos, to_read - pos);

				if (cnt_written <= 0)
				{
					fprintf (stderr, "Failed to write to pipe\n");
					throw system_error (error_code (errno, generic_category()));
				}

				pos += cnt_written;
			}

			cnt_total += to_read;

			if (cnt_total >= archive_size)
			{
				/* Safety guard */
				if (cnt_total > archive_size)
					throw gp_exception ("This must not have happened.");

				break;
			}
		}
	}
	catch (...)
	{
		close (write_end);
		throw;
	}

	close (write_end);


	/* Wait for Tar */
	int wstatus;

	if (waitpid (pid, &wstatus, 0) < 0)
	{
		fprintf (stderr, "Failed to wait for Tar to finish\n");
		throw system_error (error_code (errno, generic_category()));
	}

	int exit_code = WEXITSTATUS (wstatus);
	if (exit_code != 0)
		throw invalid_argument ("Tar returned abnormally: " + to_string (exit_code));
}


PackageProvider::PackageProvider (shared_ptr<Parameters> params)
	: params(params)
{
	/* Create repositories */
	for (auto& repo : params->repos)
	{
		if (repo.type == RepositorySpecification::TYPE_DIR)
		{
			repositories.emplace_back(make_shared<DirectoryRepository>(
						params->target + "/" + repo.param1));
		}
	}
}


set<VersionNumber> PackageProvider::list_package_versions (const string& name, const int architecture)
{
	set<VersionNumber> vs;

	for (auto &r : repositories)
		vs.merge (r->list_package_versions (name, architecture));

	return vs;
}


optional<ProvidedPackage> PackageProvider::get_package (const string& name,
		const int architecture, const VersionNumber& version)
{
	for (auto &r : repositories)
	{
		auto filename = r->get_package (name, architecture, version);

		if (filename)
		{
			auto rs = make_shared<tf::ReadStream>(*filename);

			auto rtf = tf::read_transport_form (*rs);

			return ProvidedPackage(rtf.mdata, rtf.toc, rs);
		}
	}

	return nullopt;
}
