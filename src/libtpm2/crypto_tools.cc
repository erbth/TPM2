#include <exception>
#include "crypto_tools.h"
#include "common_utilities.h"

extern "C" {
#include <sys/types.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/crypto.h>
}

using namespace std;

bool verify_sha256_fd (int fd, std::string digest)
{
	if (digest.size() != 64)
		throw invalid_argument("digest must have 64 charecters in [0-9a-fA-F]");

	auto cstr = digest.c_str();
	for (int i = 0; i < 64; i++)
	{
		if (!(
				(cstr[i] >= '0' && cstr[i] <= '9') ||
				(cstr[i] >= 'a' && cstr[i] <= 'f') ||
				(cstr[i] >= 'A' && cstr[i] <= 'F')
			))
		{
			throw invalid_argument("digest must have 64 charecters in [0-9a-fA-F]");
		}
	}

	unsigned char bytes[32];

	for (int i = 0; i < 32; i++)
		bytes[i] = ascii_to_byte(cstr + i*2);

	return verify_sha256_fd (fd, bytes);
}

bool verify_sha256_fd (int fd, const unsigned char* digest)
{
	bool ret_val = false;
	unsigned char buf[max(10240, EVP_MAX_MD_SIZE)];

	if (lseek (fd, 0, SEEK_SET) < 0)
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
			ssize_t ret = read(fd, buf, sizeof(buf));
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

		if (size != 32)
			throw gp_exception("EVP_DigestFinal_ex returned wrong SHA256 digest size");

		ret_val = CRYPTO_memcmp(buf, digest, size) == 0;

		EVP_MD_CTX_free (md_ctx);
	}
	catch(...)
	{
		EVP_MD_CTX_free (md_ctx);
		throw;
	}

	return ret_val;
}
