#include <cstdio>
#include <cerrno>
#include <cstring>
#include <regex>
#include "common_utilities.h"
#include "standard_repo_index.h"
#include "package_meta_data.h"
#include "crypto_tools.h"
#include "transport_form.h"

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>
}

using namespace std;
namespace fs = std::filesystem;
namespace tf = TransportForm;


StandardRepoIndex::StandardRepoIndex(shared_ptr<Parameters> params,const fs::path& index_path)
	: params(params), index_path(index_path)
{
}

StandardRepoIndex::~StandardRepoIndex()
{
	if (fd_file_index >= 0)
		close(fd_file_index);
}


optional<tuple<string, ManagedBuffer<unsigned char>, unsigned, ssize_t>>
StandardRepoIndex::read_signature(FILE* f)
{
	/* Start searching at 16384 bits + 1000 bytes from the file's end */
	ssize_t max_search_size = 16384 / 8 + 1000;

	if (fseek(f, 0, SEEK_END) < 0)
		throw system_error(error_code(errno, generic_category()));

	ssize_t end = ftell(f);

	if (fseek(f, max((ssize_t) 0, end - max_search_size), SEEK_SET) < 0)
		throw system_error(error_code(errno, generic_category()));

	/* Find start of index */
	ManagedBuffer<char> buf(max_search_size);
	auto search_size = fread(buf.buf, 1, max_search_size, f);
	if (search_size <= 0 || ferror(f))
		throw system_error(error_code(feof(f) ? EIO : errno, generic_category()));

	char signature_start[] = "RSA Signature with key: ";
	for (size_t i = 0; i < search_size;)
	{
		/* Read line */
		auto start = i;
		while (buf.buf[i] != '\n' && i < search_size)
			i++;

		/* Compare line to signature start */
		if (i - start >= sizeof(signature_start) &&
				strncmp(buf.buf + start, signature_start, sizeof(signature_start) - 1) == 0)
		{
			ssize_t data_end = end - search_size + start - 1;
			if (data_end < 0)
				data_end = 0;

			string key_name(buf.buf + start + sizeof(signature_start) - 1,
					i - start - sizeof(signature_start) + 1);

			/* The remainder is the signature */
			ManagedBuffer<unsigned char> sig(search_size - i);
			i++;
			unsigned j = 0;

			while (i + 1 < search_size)
			{
				if (buf.buf[i] == '\n')
				{
					i++;;
					continue;
				}

				sig.buf[j++] = ascii_to_byte(buf.buf + i);
				i += 2;
			}

			return make_tuple(key_name, move(sig), j, data_end);
		}

		i++;
	}

	return nullopt;
}

EVP_PKEY* StandardRepoIndex::retrieve_key (const string& key_name)
{
	auto p = fs::path(params->target + TPM2_KEY_DIR) / (key_name + ".pub");

	if (!fs::is_regular_file(p))
		return nullptr;

	auto bio = BIO_new_file(p.c_str(), "r");
	if (!bio)
		return nullptr;

	RSA* rsa_key;
	rsa_key = PEM_read_bio_RSA_PUBKEY(
			bio, nullptr, nullptr, nullptr);

	BIO_free(bio);

	if (!rsa_key)
	{
		ERR_print_errors_fp(stdout);
		return nullptr;
	}

	EVP_PKEY* pkey = nullptr;
	pkey = EVP_PKEY_new();
	if (!pkey)
	{
		RSA_free(rsa_key);
		throw bad_alloc();
	}

	if (EVP_PKEY_assign_RSA(pkey, rsa_key) <= 0)
	{
		EVP_PKEY_free(pkey);
		RSA_free(rsa_key);
		throw gp_exception("EVP_PKEY_set1_RSA failed");
	}

	return pkey;
}

void StandardRepoIndex::check_signature(FILE* f, const string& key_name,
		const unsigned char* sig, size_t sig_len, size_t data_end)
{
	char buf[10240];

	/* Retrieve key */
	EVP_PKEY* key = retrieve_key(key_name);
	if (!key)
	{
		throw IndexAuthenticationFailed("Index '" + index_path.string() +
				"' could not be authenticated; key '" + key_name + "' not found'");
	}

	EVP_MD_CTX* md_ctx = nullptr;

	/* Check signature */
	try
	{
		md_ctx = EVP_MD_CTX_new();
		if (!md_ctx)
			throw bad_alloc();

		if (EVP_DigestVerifyInit(md_ctx, nullptr, EVP_sha256(), nullptr, key) <= 0)
			throw gp_exception("EVP_DigestSignInit failed");

		if (fseek(f, 0, SEEK_SET) < 0)
			throw system_error(error_code(errno, generic_category()));

		size_t read_total = 0;
		for (;;)
		{
			auto to_read = min(data_end - read_total, sizeof(buf));
			if (to_read <= 0)
				break;

			if (fread(buf, 1, to_read, f) != to_read)
				throw system_error(error_code(errno, generic_category()));

			read_total += to_read;

			if (EVP_DigestVerifyUpdate(md_ctx, buf, to_read) <= 0)
				throw gp_exception("EVP_DigestVerifyUpdate failed");
		}

		auto sig_res = EVP_DigestVerifyFinal(md_ctx, sig, sig_len);
		if (sig_res != 1)
		{
			if (sig_res == 0)
			{
				throw IndexAuthenticationFailed("Index '" + index_path.string() +
						"' has an invalid signature");
			}

			throw gp_exception("EVP_DigestVerifyFinal failed");
		}

		EVP_MD_CTX_free(md_ctx);
		EVP_PKEY_free(key);
	}
	catch (...)
	{
		if (md_ctx)
			EVP_MD_CTX_free(md_ctx);

		EVP_PKEY_free(key);
		throw;
	}
}


void StandardRepoIndex::read(const bool require_signature)
{
	if (index_read)
		return;

	char buf[10240];
	char* line_buffer = nullptr;

	FILE* f = fopen(index_path.c_str(), "rb");
	if (!f)
	{
		throw gp_exception("Could not open index at '" + index_path.string() +
				"': " + strerror_r(errno, buf, sizeof(buf)));
	}

	try
	{
		const char required_first_line[] = "tpm_repo_index 1.0\n";

		/* Check index version */
		ssize_t ret = fread(buf, 1, sizeof(required_first_line), f);
		if (ret != sizeof(required_first_line) && !feof(f))
			throw system_error(error_code(errno, generic_category()));

		buf[sizeof(required_first_line) - 1] = '\0';
		if (feof(f) || strcmp(buf, required_first_line) != 0)
			throw UnsupportedIndexVersion("Unsupported version (!= 1.0)");

		/* Read signature, if present */
		auto sig = read_signature(f);
		if (sig)
		{
			auto& [key_name, sig_bytes, sig_len, data_end] = *sig;
			check_signature (f, key_name, sig_bytes.buf, sig_len, data_end);
		}
		else if (require_signature)
		{
			throw IndexAuthenticationFailedNoSignature(
					"Index '" + index_path.string() +
					"' has no signature but a signature is required.");
		}

		/* Read package list */
		if (fseek(f, 0, SEEK_SET) < 0)
			throw system_error(error_code(errno, generic_category()));

		size_t line_buffer_size = 10240;
		line_buffer = (char*) malloc(10240);
		if (!line_buffer)
			throw bad_alloc();

		bool awaiting_digest = false;
		string file_list_name, file_list_digest;

		DynamicBuffer<char> pkg_buf(10240);
		size_t pkg_len = 0;

		for (size_t line_num = 0; ; line_num++)
		{
			/* Process lines (should be fast enough for the package list) */
			ret = getline(&line_buffer, &line_buffer_size, f);
			if (ret < 0)
			{
				if (feof(f))
				{
					if (pkg_len == 0)
					{
						break;
					}
					else
					{
						throw gp_exception("Index '" + index_path.string() +
								"': unexpected end of package list.");
					}
				}
				else
					throw system_error(error_code(errno, generic_category()));
			}

			/* Skip first line */
			if (line_num == 0)
				continue;

			/* File list description in second line */
			if (line_num == 1)
			{
				int i = 0;
				for (; i < ret && line_buffer[i] != ' '; i++);

				file_list_name = string(line_buffer, i);

				if (ret > i + 2)
					file_list_digest = string(line_buffer + i + 1, ret - i - 2);

				if (file_list_name.size() < 1 || file_list_digest.size() != 64)
				{
					throw gp_exception("Index '" + index_path.string() +
							"' has an invalid header.");
				}

				continue;
			}

			if (awaiting_digest)
			{
				/* Package digest */
				awaiting_digest = false;

				if (ret != 65)
				{
					throw gp_exception("Index '" + index_path.string() +
							"': invalid package digest length");
				}

				auto mdata = read_package_meta_data_from_xml(pkg_buf.buf, pkg_len);
				string digest(line_buffer, ret - 1);

				/* Insert package into index */
				if (arch == Architecture::invalid)
				{
					arch = mdata->architecture;
				}
				else if (arch != mdata->architecture)
				{
					throw gp_exception("Index '" + index_path.string() +
							"' contains packages for different architectures.");
				}

				if (packages.find(mdata->name) == packages.end())
				{
					packages.insert(mdata->name);
					package_versions.insert(make_pair(mdata->name, vector<VersionNumber>()));
				}

				if (package_data.find(make_pair(mdata->name, mdata->version)) != package_data.end())
				{
					throw gp_exception("Index '" + index_path.string() +
							"' contains package version '" + mdata->name + ":" +
							mdata->version.to_string() + "' multiple times.");
				}

				package_versions.find(mdata->name)->second.push_back(mdata->version);
				package_data.insert(make_pair(
						make_pair(mdata->name, mdata->version),
						make_tuple(mdata, digest, 0, 0)
				));

				pkg_len = 0;
			}
			else
			{
				/* End of package list if a signature is present */
				if (pkg_len == 0 && ret == 1 && line_buffer[0] == '\n')
					break;

				if (pkg_len == 0 && strncmp(line_buffer, "<pkg", 4) != 0)
					throw gp_exception("Index '" + index_path.string() +
							"': unexpected characters at start of package: '" +
							string(line_buffer) + "'");

				/* Package */
				pkg_buf.ensure_size(pkg_len + ret);

				memcpy (pkg_buf.buf + pkg_len, line_buffer, ret);
				pkg_len += ret;

				if (strcmp(line_buffer, "</pkg>\n") == 0)
					awaiting_digest = true;
			}
		}

		free(line_buffer);
		line_buffer = nullptr;

		fclose(f);
		f = nullptr;


		/* Check digest of file index */
		fd_file_index = open(
				(index_path.parent_path() / file_list_name).c_str(),
				O_RDONLY);

		if (fd_file_index < 0)
		{
			throw gp_exception("Index '" + index_path.string() +
					"': could not open file list '" + file_list_name + "': " +
					strerror_r(errno, buf, sizeof(buf)));
		}

		if (!verify_sha256_fd (fd_file_index, file_list_digest))
		{
			throw gp_exception("Index '" + index_path.string() +
					"': file list checksum missmatch");
		}

		/* Read file index's index */
		if (lseek(fd_file_index, 0, SEEK_SET) < 0)
			throw system_error(error_code(errno, generic_category()));

		bool eoi = false;
		ssize_t last_pos = 0;

		uint64_t last_addr = 0;
		string last_pkg;

		const regex re("(.+)@([^@:]+):([^@:]+)");

		while (!eoi)
		{
			ssize_t ret = ::read(fd_file_index, buf + last_pos, sizeof(buf) - last_pos);
			if (ret < 0)
				throw system_error(error_code(errno, generic_category()));

			if (ret == 0)
			{
				if (errno == EINTR)
				{
					continue;
				}
				else
				{
					throw gp_exception("Index '" + index_path.string() +
							"': unexpected end of file index");
				}
			}

			/* Process data */
			auto fill = last_pos + ret;
			ssize_t i = 0;

			/* Ensure that we always have 8 bytes of lookahead for the address
			 * at the end of each entry */
			while (!eoi)
			{
				/* Find entry delimiter */
				bool found = false;

				ssize_t j = i;
				for (; ; j++)
				{
					if (j + 8 >= fill)
						break;

					if (buf[j] == '\0')
					{
						found = true;
						break;
					}
				}

				if (!found)
					break;

				/* Entry size > 9 bytes -> package
				 * Entry size == 9 ('\0') -> end; read file size */
				ssize_t entry_size = j - i + 1 + 8;
				if (entry_size >= 9)
				{
					uint64_t new_addr;
					memcpy ((char*) &new_addr, buf + j + 1, 8);
					new_addr = le64toh(new_addr);

					if (last_addr > 0)
					{
						smatch m;
						if (!regex_match (last_pkg, m, re))
						{
							throw gp_exception("Index '" + index_path.string() +
									"': Invalid package specification '" + last_pkg +
									"' in file index");
						}

						string pkg_name = m[1];
						string pkg_arch_str = m[2];
						auto pkg_version = VersionNumber(m[3]);

						if (Architecture::to_string(arch) != pkg_arch_str)
						{
							throw gp_exception("Index '" + index_path.string() +
									"': Invalid architecture '" + pkg_arch_str +
									"' in file index");
						}

						auto i = package_data.find(make_pair(pkg_name, pkg_version));
						if (i == package_data.end())
						{
							throw gp_exception("Index '" + index_path.string() +
									"': Package '" + pkg_name + ":" + pkg_version.to_string() +
									"' in file index but not in package list.");
						}

						get<2>(i->second) = last_addr;
						get<3>(i->second) = new_addr - last_addr;
					}

					if (entry_size == 9)
					{
						eoi = true;
					}
					else
					{
						last_pkg = string(buf + i, entry_size - 9);
						last_addr = new_addr;
					}
				}
				else
				{
					throw gp_exception("Index '" + index_path.string() +
							"': Invalid entry size in file index");
				}

				i = j + 9;
			}

			/* Shift buffer left */
			memmove(buf, buf + i, fill - i);
			last_pos = fill - i;
		}

		/* Check that every package version was mentioned in the file index */
		for (auto& [k, v] : package_data)
		{
			auto& [mdata, digest, addr, size] = v;

			if (addr == 0)
			{
				throw gp_exception("Index '" + index_path.string() +
						"': Package version '" + mdata->name + ":" + mdata->version.to_string() +
						"' in package list but not in file index");
			}
		}
	}
	catch(...)
	{
		if (line_buffer)
			free(line_buffer);

		if (f)
			fclose(f);

		throw;
	}

	index_read = true;
}


std::vector<string> StandardRepoIndex::list_packages (const int pkg_arch)
{
	if (pkg_arch != arch)
		return vector<string>();

	return vector<string>(packages.cbegin(), packages.cend());
}

set<VersionNumber> StandardRepoIndex::list_package_versions (const string& pkg_name, const int pkg_arch)
{
	if (pkg_arch != arch)
		return set<VersionNumber>();

	auto i = package_versions.find(pkg_name);
	if (i == package_versions.end())
		return set<VersionNumber>();

	return set<VersionNumber>(i->second.cbegin(), i->second.cend());
}

shared_ptr<PackageMetaData> StandardRepoIndex::get_mdata(
		const string& pkg_name, const int pkg_arch, const VersionNumber& pkg_version)
{
	if (pkg_arch != arch)
		return nullptr;

	auto i = package_data.find(make_pair(pkg_name, pkg_version));
	if (i == package_data.end())
		return nullptr;

	return get<0>(i->second);
}

optional<string> StandardRepoIndex::get_digest(
		const string& pkg_name, const int pkg_arch, const VersionNumber& pkg_version)
{
	if (pkg_arch != arch)
		return nullopt;

	auto i = package_data.find(make_pair(pkg_name, pkg_version));
	if (i == package_data.end())
		return nullopt;

	return get<1>(i->second);
}

shared_ptr<FileList> StandardRepoIndex::get_file_list(
		const string& pkg_name, const int pkg_arch, const VersionNumber& pkg_version)
{
	if (pkg_arch != arch)
		return nullptr;

	auto i = package_data.find(make_pair(pkg_name, pkg_version));
	if (i == package_data.end())
		return nullptr;

	auto addr = get<2>(i->second);
	auto size = get<3>(i->second);

	auto rs = tf::FDReadStream(fd_file_index, false);
	rs.seek(addr);

	return tf::read_file_list(rs, size);
}
