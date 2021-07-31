#include <algorithm>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <optional>
#include "package_provider.h"
#include "directory_repository.h"
#include "common_utilities.h"
#include "crypto_tools.h"

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
}

using namespace std;
namespace tf = TransportForm;


ProvidedPackage::ProvidedPackage (
		shared_ptr<PackageMetaData> mdata,
		const tf::TableOfContents* toc,
		shared_ptr<tf::ReadStream> rs,
		get_read_stream_t get_read_stream,
		shared_ptr<RepoIndex> index,
		bool disable_repo_digest_check)
	:
		PackageVersion(mdata->name, mdata->architecture, mdata->source_version, mdata->version),
		mdata(mdata),
		disable_repo_digest_check(disable_repo_digest_check),
		toc(toc ? optional<tf::TableOfContents>(*toc) : nullopt),
		rs(rs),
		get_read_stream(get_read_stream),
		index(index)
{
	if (!mdata)
		throw invalid_argument ("mdata nullptr");

	if (!(
			(rs && toc) ||
			(get_read_stream && disable_repo_digest_check) ||
			(get_read_stream && index)))
	{
		throw invalid_argument ("Invalid combination of info sources");
	}

}


void ProvidedPackage::ensure_read_stream()
{
	if (rs)
		return;

	if (!get_read_stream)
		throw invalid_argument ("Reopening read streams requires a read stream generator.");

	if (!index && !disable_repo_digest_check)
	{
		throw invalid_argument ("Reopening read streams requires an index or "
				"disabling repo digest checks.");
	}

	auto tmp_rs = get_read_stream();

	if (index)
	{
		/* Integrity check transport form */
		auto digest = index->get_digest(mdata->name, mdata->architecture, mdata->version);
		if (!digest)
			throw gp_exception("Digest for transport form is not in index given to ProvidedPackage");

		auto filename = tmp_rs->get_filename();
		int fd = open(filename.c_str(), O_RDONLY);
		if (fd < 0)
			throw system_error(error_code(errno, generic_category()));

		try
		{
			if (!verify_sha256_fd(fd, *digest))
			{
				throw gp_exception("SHA256 sum missmatch of package '" +
						mdata->name + "@" + Architecture::to_string(mdata->architecture) +
						":" + mdata->version.to_string() + "'");
			}
		}
		catch(...)
		{
			close(fd);
			throw;
		}

		close(fd);
	}

	rs = tmp_rs;
}


tf::TableOfContents& ProvidedPackage::ensure_toc()
{
	if (toc)
		return *toc;

	ensure_read_stream();

	rs->seek(0);
	toc = tf::TableOfContents::read_from_binary(*rs);
	return *toc;
}


/* PackageVersion interface */
bool ProvidedPackage::is_installed() const
{
	return false;
}

vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>>
ProvidedPackage::get_dependencies()
{
	vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>> deps;
	for (auto& dep : mdata->dependencies)
		deps.push_back(make_pair(dep.identifier, dep.version_formula));

	return deps;
}

vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>>
ProvidedPackage::get_pre_dependencies()
{
	vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>> deps;
	for (auto& dep : mdata->pre_dependencies)
		deps.push_back(make_pair(dep.identifier, dep.version_formula));

	return deps;
}

const vector<string> &ProvidedPackage::get_files()
{
	if (!file_paths_populated)
	{
		for (auto& file : *get_file_list())
		{
			if (file.type != FILE_TYPE_DIRECTORY)
				file_paths.push_back(file.path);
		}

		file_paths_populated = true;
	}

	return file_paths;
}

const vector<string> &ProvidedPackage::get_directories()
{
	if (!directory_paths_populated)
	{
		for (auto& file : *get_file_list())
		{
			if (file.type == FILE_TYPE_DIRECTORY)
				directory_paths.push_back(file.path);
		}

		directory_paths_populated = true;
	}

	return directory_paths;
}


shared_ptr<PackageMetaData> ProvidedPackage::get_mdata()
{
	return mdata;
}


shared_ptr<FileList> ProvidedPackage::get_file_list()
{
	if (!files)
	{
		if (index)
		{
			/* Get file list from index */
			files = index->get_file_list (mdata->name, mdata->architecture, mdata->version);
			if (!files)
				throw gp_exception("File list is not in index given to ProvidedPackage");
		}
		else
		{
			for (const auto& sec : ensure_toc().sections)
			{
				if (sec.type == tf::SEC_TYPE_FILE_INDEX)
				{
					ensure_read_stream();

					if (rs->tell () != sec.start)
						rs->seek (sec.start);

					files = tf::read_file_list (*rs, sec.size);
					break;
				}
			}
		}
	}

	if (!files)
		files = make_shared<FileList>();

	return files;
}


shared_ptr<vector<string>> ProvidedPackage::get_config_files()
{
	if (!config_files)
	{
		ensure_toc();
		for (const auto& sec : ensure_toc().sections)
		{
			if (sec.type == tf::SEC_TYPE_CONFIG_FILES)
			{
				ensure_read_stream();

				if (rs->tell() != sec.start)
					rs->seek (sec.start);

				config_files = tf::read_config_files (*rs, sec.size);
			}
		}

		if (!config_files)
			config_files = make_shared<vector<string>>();
	}

	sort(config_files->begin(), config_files->end());
	return config_files;
}


shared_ptr<ManagedBuffer<char>> ProvidedPackage::get_preinst()
{
	if (!preinst)
	{
		for (auto& sec : ensure_toc().sections)
		{
			if (sec.type == tf::SEC_TYPE_PREINST)
			{
				ensure_read_stream();

				if (rs->tell() != sec.start)
					rs->seek (sec.start);

				preinst = make_shared<ManagedBuffer<char>>(sec.size);
				rs->read (preinst->buf, sec.size);
				break;
			}
		}
	}

	return preinst;
}


shared_ptr<ManagedBuffer<char>> ProvidedPackage::get_configure()
{
	if (!configure)
	{
		for (auto& sec : ensure_toc().sections)
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


shared_ptr<ManagedBuffer<char>> ProvidedPackage::get_unconfigure()
{
	if (!unconfigure)
	{
		ensure_toc();
		for (auto& sec : ensure_toc().sections)
		{
			if (sec.type == tf::SEC_TYPE_UNCONFIGURE)
			{
				ensure_read_stream();

				if (rs->tell() != sec.start)
					rs->seek(sec.start);

				unconfigure = make_shared<ManagedBuffer<char>>(sec.size);
				rs->read (unconfigure->buf, sec.size);
				break;
			}
		}
	}

	return unconfigure;
}


shared_ptr<ManagedBuffer<char>> ProvidedPackage::get_postrm()
{
	if (!postrm)
	{
		for (auto& sec : ensure_toc().sections)
		{
			if (sec.type == tf::SEC_TYPE_POSTRM)
			{
				ensure_read_stream();

				if (rs->tell() != sec.start)
					rs->seek(sec.start);

				postrm = make_shared<ManagedBuffer<char>>(sec.size);
				rs->read (postrm->buf, sec.size);
				break;
			}
		}
	}

	return postrm;
}


bool ProvidedPackage::has_archive ()
{
	for (auto& sec : ensure_toc().sections)
	{
		if (sec.type == tf::SEC_TYPE_ARCHIVE)
		{
			return sec.size > 0;
		}
	}

	return false;
}


void ProvidedPackage::clear_buffers()
{
	rs = nullptr;
	files = nullptr;
	preinst = nullptr;
	configure = nullptr;
	unconfigure = nullptr;
	postrm = nullptr;
}


void ProvidedPackage::unpack_archive_to_directory(const string& dst, vector<string>* excluded_paths)
{
	size_t archive_size = 0;

	for (auto& sec : ensure_toc().sections)
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

	if (!archive_size)
		return;


	/* Construct a temporary file containing paths to exclude. */
	optional<TemporaryFile> exclude_file;
	if (excluded_paths && !excluded_paths->empty())
	{
		exclude_file.emplace ("tpm2-excl");

		for (auto& f : *excluded_paths)
			exclude_file->append_string ("." + f + "\n");

		exclude_file->close();
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
		/* This is an implicite copy, hence disable deleting the file on
		 * destruction (usually the destructor won't be run anyway). */
		if (exclude_file)
			exclude_file->set_unowned();

		/* In the child */
		ret = dup2 (pipefds[0], STDIN_FILENO);
		if (ret < 0)
		{
			fprintf (stderr, "Failed to replace stdin: %s.\n", strerror (errno));
			exit (1);
		}

		close (pipefds[0]);
		close (pipefds[1]);


		/* Exclude files */
		if (exclude_file)
		{
			execlp ("tar", "tar", "-xC", dst.c_str(),
					"-X", exclude_file->path().c_str(), nullptr);
		}
		else
		{
			execlp ("tar", "tar", "-xC", dst.c_str(), nullptr);
		}

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
						params, repo.param1, true));
		}
		else if (repo.type == RepositorySpecification::TYPE_DIR_ALLOW_UNSIGNED)
		{
			repositories.emplace_back(make_shared<DirectoryRepository>(
						params, repo.param1, false));
		}
	}
}


shared_ptr<PackageProvider> PackageProvider::create (shared_ptr<Parameters> params)
{
	return shared_ptr<PackageProvider> (new PackageProvider (params));
}


set<VersionNumber> PackageProvider::list_package_versions (const string& name, const int architecture)
{
	set<VersionNumber> vs;

	for (auto &r : repositories)
		vs.merge (r->list_package_versions (name, architecture));

	return vs;
}


class ReadStreamFactory
{
private:
	const string filename;

public:
	ReadStreamFactory(const string& filename)
		: filename(filename)
	{
	}

	shared_ptr<tf::ReadStream> operator()()
	{
		return make_shared<tf::GZReadStream>(filename);
	}
};

shared_ptr<ProvidedPackage> PackageProvider::get_package (const string& name,
		const int architecture, const VersionNumber& version)
{
	for (auto &r : repositories)
	{
		auto ret = r->get_package (name, architecture, version);

		if (ret)
		{
			auto [filename, index] = *ret;
			ReadStreamFactory rsf(filename);

			/* A read stream can only be opened if it does not have to be digest
			 * checked */
			if (!index && !r->digest_checking_required())
			{
				auto rs = rsf();
				auto rtf = tf::read_transport_form (*rs);

				return make_shared<ProvidedPackage>(
						rtf.mdata,
						&rtf.toc, rs,
						rsf,
						nullptr,
						true);
			}
			else if (index)
			{
				auto mdata = index->get_mdata(name, architecture, version);
				if (!mdata)
					throw gp_exception("Package not in index returned by repository");

				return make_shared<ProvidedPackage>(
						mdata,
						nullptr, nullptr,
						rsf,
						index,
						false);
			}
			else
			{
				throw gp_exception("Transport form '" + filename +
						"' requires digest checking but is not part of an index.");
			}
		}
	}

	return nullptr;
}
