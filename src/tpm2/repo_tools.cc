/** Tools for repositories */
#include <algorithm>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <filesystem>
#include <vector>
#include "common_utilities.h"
#include "utility.h"
#include "transport_form.h"
#include "managed_buffer.h"
#include "repo_tools.h"

extern "C"
{
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
}

using namespace std;
using namespace tinyxml2;
namespace fs = std::filesystem;
namespace tf = TransportForm;

namespace repo_tools {


void write_string (int fd, const string& buf, const char* error_msg = nullptr)
{
	size_t written = 0;

	while (written < buf.size())
	{
		auto ret = write (fd, buf.c_str() + written, buf.size() - written);
		if (ret < 0)
		{
			if (error_msg)
				fprintf (stderr, "%s\n", error_msg);

			throw system_error (error_code (errno, generic_category()));
		}

		if (ret == 0)
			throw system_error (error_code (errno != 0 ? errno : EIO, generic_category()));

		written += ret;
	}
}


void create_index_arch (shared_ptr<Parameters> params, const fs::path& p,
		const string& name, RSA* signing_key = nullptr, const string& signing_key_name = string())
{
	char buf[max(10240, EVP_MAX_MD_SIZE)];
	int plist = -1;
	int findex = -1;

	try
	{
		/* Create package list file */
		fs::path plist_path = p / (name + ".index.new");
		plist = open (plist_path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);

		if (plist < 0)
		{
			fprintf (stderr, "Could not create '%s'.\n", plist_path.c_str());
			throw system_error(error_code(errno, generic_category()));
		}

		/* Create file index with a unique name */
		struct timespec tnow;
		struct tm tm;
		if (clock_gettime (CLOCK_REALTIME, &tnow) < 0)
			throw system_error(error_code(errno, generic_category()));

		strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", gmtime_r(&tnow.tv_sec, &tm));
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				"%06d.files", (int) tnow.tv_nsec / 1000);

		string findex_name(buf);
		fs::path findex_path = p / findex_name;
		findex = open (findex_path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);

		if (findex < 0)
		{
			fprintf (stderr, "Could not create '%s'.\n", findex_path.c_str());
			throw system_error(error_code(errno, generic_category()));
		}

		/* Write package list header */
		printf_verbose (params, "  Writing package list header...\n");

		write_string (plist, "tpm_repo_index 1.0\n" + findex_name + " ");

		auto findex_csum_pos = lseek(plist, 0, SEEK_CUR);
		if (findex_csum_pos < 0)
			throw system_error(error_code(errno, generic_category()));

		/* Write a dummy sha256 sum to fill space */
		write_string (plist, "0000000000000000000000000000000000000000000000000000000000000000\n");


		/* Write packages to the package list */
		vector<shared_ptr<PackageMetaData>> pkgs;

		for (auto& entry : fs::directory_iterator(p))
		{
			if (!entry.is_regular_file())
				continue;

			auto filename = entry.path().filename();
			if (filename.extension() != ".tpm2")
				continue;

			printf_verbose (params, "    Processing transport form %s...\n", filename.c_str());

			/* Read package meta data */
			{
				auto rs = make_shared<tf::GZReadStream>(entry.path());
				auto rtf = tf::read_transport_form (*rs);

				pkgs.push_back(rtf.mdata);

				/* Write package meta data */
				unique_ptr<XMLDocument> doc = rtf.mdata->to_xml();

				XMLPrinter printer;
				doc->Print (&printer);

				ssize_t written = 0;
				ssize_t to_write = printer.CStrSize() - 1;
				while (written < to_write)
				{
					auto ret = write (plist, printer.CStr() + written, to_write - written);
					if (ret < 0)
						throw system_error(error_code(errno, generic_category()));

					if (ret == 0)
						throw system_error(error_code(errno ? errno : EIO, generic_category()));

					written += ret;
				}

				if (to_write < 1 || printer.CStr()[to_write - 1] != '\n')
					write_string (plist, "\n");
			}

			/* Write package sha256 sum */
			FILE* f = nullptr;
			EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
			if (!md_ctx)
				throw bad_alloc();

			try
			{
				if (EVP_DigestInit_ex(md_ctx, EVP_sha256(), nullptr) <= 0)
					throw gp_exception ("EVP_DigestInit(SHA256) failed");

				f = fopen (entry.path().c_str(), "rb");
				if (!f)
					throw system_error(error_code(errno, generic_category()));

				for (;;)
				{
					ssize_t ret = fread(buf, 1, sizeof(buf), f);
					if (ret < 0)
						throw system_error(error_code(errno, generic_category()));

					if (ret == 0)
					{
						if (feof(f))
							break;
						else
							throw system_error(error_code(errno ? errno : EIO, generic_category()));
					}

					if (EVP_DigestUpdate(md_ctx, (unsigned char*) buf, ret) <= 0)
						throw gp_exception ("EVP_DigestUpdate failed");
				}

				fclose(f);
				f = nullptr;

				unsigned size;
				if (EVP_DigestFinal_ex(md_ctx, (unsigned char*) buf, &size) <= 0)
					throw gp_exception ("EVP_DigestFinal failed");

				string str_digest;
				char conv[10];

				for (unsigned i = 0; i < size; i++)
				{
					snprintf(conv, sizeof(conv), "%02x", (unsigned) ((unsigned char*) buf)[i]);
					conv[sizeof(conv) - 1] = '\0';
					str_digest += conv;
				}

				write_string (plist, str_digest + "\n");

				EVP_MD_CTX_free (md_ctx);
			}
			catch(...)
			{
				if (f)
					fclose(f);

				EVP_MD_CTX_free (md_ctx);
				throw;
			}
		}


		/* Write the file index's index */
		printf_verbose (params, "  Writing file index...\n");

		sort (pkgs.begin(), pkgs.end(), [](auto a, auto b) {
			return a->name < b->name;
		});

		vector<unsigned int> dir_positions(pkgs.size() + 1);
		vector<uint64_t> positions(pkgs.size() + 1);
		unsigned i = 0;

		for (auto pkg : pkgs)
		{
			write_string (findex, pkg->name + "@" +
					Architecture::to_string(pkg->architecture) + ":" +
					pkg->version.to_string());

			/* End of package descriptor */
			buf[0] = '\0';
			if (write(findex, buf, 1) != 1)
				throw system_error(error_code(errno ? errno : EIO, generic_category()));

			auto pos = lseek(findex, 0, SEEK_CUR);
			if (pos < 0)
				throw system_error(error_code(errno, generic_category()));

			dir_positions[i++] = pos;

			/* Address offset dummy */
			write_string (findex, "        ");
		}

		/* End of index */
		buf[0] = '\0';
		if (write(findex, buf, 1) != 1)
			throw system_error(error_code(errno ? errno : EIO, generic_category()));

		/* File size indication */
		{
			auto pos = lseek(findex, 0, SEEK_CUR);
			if (pos < 0)
				throw system_error(error_code(errno, generic_category()));

			dir_positions[i++] = pos;
		}

		/* Write dummy for file size (first position after file's end) */
		write_string (findex, "        ");

		/* Write the file index */
		i = 0;

		for (auto pkg : pkgs)
		{
			fs::path tfp = p / (pkg->name + "-" +
					pkg->version.to_string() + "_" +
					Architecture::to_string(pkg->architecture) + ".tpm2");

			auto rs = make_shared<tf::GZReadStream>(tfp);
			auto rtf = tf::read_transport_form (*rs);

			tf::TOCSection* ind_sec = nullptr;
			for (auto& sec : rtf.toc.sections)
			{
				if (sec.type == tf::SEC_TYPE_FILE_INDEX)
				{
					ind_sec = &sec;
					break;
				}
			}

			/* Save start position of potential file index (otherwise size will
			 * simply be 0) */
			uint64_t pos = lseek(findex, 0, SEEK_CUR);
			if (pos < 0)
				throw system_error(error_code(errno, generic_category()));

			positions[i++] = pos;

			if (ind_sec)
			{
				rs->seek(ind_sec->start);

				/* Copy file index from the transport form */
				ssize_t total_written = 0;
				while (total_written < ind_sec->size)
				{
					int to_process = ind_sec->size - total_written;
					if (to_process > (int) sizeof(buf))
						to_process = sizeof(buf);

					rs->read(buf, to_process);

					int written = 0;
					while (written < to_process)
					{
						auto ret = write (findex, buf + written, to_process - written);
						if (ret < 0)
							throw system_error(error_code(errno, generic_category()));

						if (ret == 0)
							throw system_error(error_code(errno ? errno : EIO, generic_category()));

						written += ret;
					}

					total_written += written;
				}
			}
		}

		/* File size */
		{
			uint64_t pos = lseek(findex, 0, SEEK_CUR);
			if (pos < 0)
				throw system_error(error_code(errno, generic_category()));

			positions[i++] = pos;
		}

		/* Update positions in the file index's index */
		for (i = 0; i < dir_positions.size(); i++)
		{
			if (lseek(findex, dir_positions[i], SEEK_SET) < 0)
				throw system_error(error_code(errno, generic_category()));

			uint64_t offset = htole64(positions[i]);
			if (write(findex, (char*) &offset, sizeof(offset)) != sizeof(offset))
				throw system_error(error_code(errno ? errno : EIO, generic_category()));
		}


		/* Update the file index's checksum in the package list */
		{
			if (lseek(findex, 0, SEEK_SET) < 0)
				throw system_error(error_code(errno, generic_category()));

			EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
			if (!md_ctx)
				throw bad_alloc();

			try
			{
				if (EVP_DigestInit_ex(md_ctx, EVP_sha256(), nullptr) <= 0)
					throw gp_exception ("EVP_DigestInit(SHA256) failed");

				for (;;)
				{
					ssize_t ret = read(findex, buf, sizeof(buf));
					if (ret < 0)
						throw system_error(error_code(errno, generic_category()));

					if (ret == 0)
					{
						if (errno == EINTR)
							continue;

						break;
					}

					if (EVP_DigestUpdate(md_ctx, (unsigned char*) buf, ret) <= 0)
						throw gp_exception ("EVP_DigestUpdate failed");
				}

				unsigned size;
				if (EVP_DigestFinal_ex(md_ctx, (unsigned char*) buf, &size) <= 0)
					throw gp_exception ("EVP_DigestFinal failed");

				string str_digest;
				char conv[10];

				for (unsigned i = 0; i < size; i++)
				{
					snprintf(conv, sizeof(conv), "%02x", (unsigned) ((unsigned char*) buf)[i]);
					conv[sizeof(conv) - 1] = '\0';
					str_digest += conv;
				}

				if (lseek(plist, findex_csum_pos, SEEK_SET) < 0)
					throw system_error(error_code(errno, generic_category()));

				write_string (plist, str_digest);

				EVP_MD_CTX_free (md_ctx);
			}
			catch(...)
			{
				EVP_MD_CTX_free (md_ctx);
				throw;
			}
		}


		/* Optionally sign the package list */
		if (signing_key)
		{
			printf_verbose(params, "  Signing package list...\n");

			EVP_PKEY* pkey = nullptr;
			EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
			if (!md_ctx)
				throw bad_alloc();

			try
			{
				pkey = EVP_PKEY_new();
				if (!pkey)
					throw bad_alloc();

				if (EVP_PKEY_set1_RSA(pkey, signing_key) <= 0)
					throw gp_exception("EVP_PKEY_set1_RSA failed");

				if (EVP_DigestSignInit(md_ctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0)
					throw gp_exception("EVP_DigestSignInit failed");

				/* Process package list */
				if (lseek(plist, 0, SEEK_SET) < 0)
					throw system_error(error_code(errno, generic_category()));

				for (;;)
				{
					auto ret = read(plist, buf, sizeof(buf));
					if (ret < 0)
						throw system_error(error_code(errno, generic_category()));

					if (ret == 0)
					{
						if (errno == EINTR)
							continue;
						else
							break;
					}

					if (EVP_DigestSignUpdate(md_ctx, buf, ret) <= 0)
						throw gp_exception("EVP_DigestSignUpdate failed");
				}

				size_t siglen;
				if (EVP_DigestSignFinal(md_ctx, nullptr, &siglen) <= 0)
					throw gp_exception("EVP_DigestSignFinal failed");

				ManagedBuffer<unsigned char> sig(siglen);
				if (EVP_DigestSignFinal(md_ctx, sig.buf, &siglen) <= 0)
					throw gp_exception("EVP_DigestSignFinal failed");

				/* Append signature */
				write_string(plist, "\nRSA Signature with key: " + signing_key_name + "\n");
				for (size_t i = 0; i < siglen;)
				{
					auto to_write = min((size_t) 36, siglen - i);

					for (unsigned j = 0; j < to_write; j++)
						snprintf(buf + j*2, 10, "%02x", (unsigned int) sig.buf[i++]);

					buf[to_write * 2] = '\n';
					buf[to_write * 2 + 1] = '\0';
					write_string(plist, buf);
				}

				EVP_PKEY_free(pkey);
				EVP_MD_CTX_free(md_ctx);
			}
			catch(...)
			{
				if (pkey)
					EVP_PKEY_free(pkey);

				EVP_MD_CTX_free(md_ctx);
				throw;
			}
		}


		close (plist);
		close (findex);
		plist = -1;
		findex = -1;

		/* Swap the new package list in */
		printf_verbose(params, "  moving the new index into place.\n");
		if (rename (plist_path.c_str(), (p / (name + ".index")).c_str()) < 0)
			throw system_error(error_code(errno, generic_category()));
	}
	catch(...)
	{
		if (plist >= 0)
			close (plist);

		if (findex >= 0)
			close (findex);

		throw;
	}
}


bool create_index (shared_ptr<Parameters> params)
{
	bool ret_val = false;
	RSA *signing_key = nullptr;

	/* List architecture directories and create indeces for them. */
	try
	{
		auto root = get_absolute_path (params->create_index_repo);

		printf ("Creating index '%s' in repository rooted at '%s'.\n",
				params->create_index_name.c_str(), root.c_str());

		/* Try to load signing key */
		try
		{
			if (params->sign.size() > 0)
			{
				auto bio = BIO_new_file(params->sign.c_str(), "r");
				if (!bio)
					throw system_error(error_code(errno, generic_category()));

				signing_key = PEM_read_bio_RSAPrivateKey(
						bio, nullptr, nullptr, nullptr);

				BIO_free(bio);

				if (!signing_key)
				{
					printf ("Failed to load signing key.\n");
					goto END;
				}
			}
		}
		catch (system_error& e)
		{
			printf ("Failed to load signing key: %s\n", e.what());
			goto END;
		}


		string signing_key_name = fs::path(params->sign).filename().stem();
		if (signing_key)
			printf("Index will be signed with key '%s'.\n", signing_key_name.c_str());
		else
			printf("Index will NOT be signed.\n");


		for (auto& arch_dir : fs::directory_iterator(root))
		{
			auto& p = arch_dir.path();
			auto arch = p.filename();

			try
			{
				Architecture::from_string (arch.c_str());
			}
			catch (InvalidArchitecture&)
			{
				printf ("Skipping invalid architecture subdirectory '%s'.\n", arch.c_str());
				continue;
			}

			printf ("Architecture %s.\n", arch.c_str());

			try
			{
				create_index_arch (params, p, params->create_index_name,
						signing_key, signing_key_name);
			}
			catch (exception& e)
			{
				fprintf (stderr, "Error: %s\n", e.what());
				goto END;
			}
		}
	}
	catch (system_error& e)
	{
		if (e.code().value() == ENOENT)
		{
			printf ("%s\n", e.what());
			return false;
		}

		if (signing_key)
			RSA_free(signing_key);

		throw;
	}
	catch(...)
	{
		if (signing_key)
			RSA_free(signing_key);

		throw;
	}

	ret_val = true;

END:
	if (signing_key)
		RSA_free(signing_key);

	return ret_val;
}

}
