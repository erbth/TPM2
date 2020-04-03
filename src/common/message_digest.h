/** This file is part of the TSClient LEGACY Package Manager
 *
 * Common message digest calculation
 *
 * Contains a SHA1 implementation according to FIPS PUB 180-4.
 *
 * Error reporting through exceptions. */

#ifndef __MESSAGE_DIGEST_H
#define __MESSAGE_DIGEST_H

#include <cstdint>

namespace message_digest
{
	/* @param sum must be of size 20. */
	void sha1_memory (const char *data, size_t data_size, char *sum) noexcept;

	/* @param sum must be of size 20.
	 * @returns 0 on success, -errno if it could not open the file. */
	int sha1_file (const char *path, char *sum) noexcept;

	class sha1_ctx
	{
	private:
		char c_buf[64];
		uint32_t H[5] = {
			0x67452301,
			0xefcdab89,
			0x98badcfe,
			0x10325476,
			0xc3d2e1f0
		};

		uint64_t l = 0;
		unsigned c_fill = 0;


	public:
		void input_bytes(const char *data, size_t size) noexcept;
		void get_hash(char *hash) noexcept;
	};
};

#endif /* __MESSAGE_DIGEST_H */
