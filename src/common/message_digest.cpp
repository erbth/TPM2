#include <cerrno>
#include <cstdio>
#include <cstring>
#include "message_digest.h"

extern "C" {
#include <endian.h>
}


inline unsigned min (unsigned a, unsigned b)
{
	return a < b ? a : b;
}


inline uint32_t rotl (uint32_t x, unsigned n)
{
	return (x << n) | (x >> (32 - n));
}


namespace message_digest
{


void sha1_memory (const char *data, size_t data_size, char *sum) noexcept
{
	sha1_ctx c;
	c.input_bytes (data, data_size);
	c.get_hash (sum);
}


int sha1_file (const char *path, char *sum) noexcept
{
	FILE *f = fopen (path, "rb");
	if (!f)
		return -errno;

	/* Just in case ... */
	sha1_ctx c;

	char buf[8192];
	size_t r;

	for (;;)
	{
		r = fread (buf, 1, 8192, f);
		c.input_bytes (buf, r);

		if (r == 0 || feof (f))
			break;
	}

	int err = 0;

	if (ferror(f) || !feof (f))
		err = -EIO;

	fclose (f);
	if (err != 0)
		return err;

	c.get_hash (sum);
	return 0;
}


void sha1_ctx::input_bytes (const char *data, size_t size) noexcept
{
	while (size > 0)
	{
		/* Attention, c_fill is expressed in bytes! */
		unsigned new_bytes = min (16 * 4 - c_fill, size);
		size -= new_bytes;

		memcpy (c_buf + c_fill, data, new_bytes);
		c_fill += new_bytes;
		data += new_bytes;

		l += 8 * new_bytes;


		if (c_fill == 16 * 4)
		{
			/* Hash a block */
			uint32_t W[80];

			unsigned i = 0;

			for (; i < 16; i++)
				W[i] = be32toh (((uint32_t*) c_buf)[i]);

			for (; i < 80; i++)
				W[i] = rotl (W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1);


			uint32_t a = H[0];
			uint32_t b = H[1];
			uint32_t c = H[2];
			uint32_t d = H[3];
			uint32_t e = H[4];


			const uint32_t K[4] = {
				0x5a827999,
				0x6ed9eba1,
				0x8f1bbcdc,
				0xca62c1d6
			};

			for (i = 0; i < 80; i++)
			{
				uint32_t f_t;

				if (i < 20)
				{
					f_t = (b & c) ^ (~b & d);
				}
				else if (i < 40 || i >= 60)
				{
					f_t = b ^ c ^ d;
				}
				else
				{
					f_t = (b & c) ^ (b & d) ^ (c & d);
				}

				uint32_t T = rotl (a, 5) + f_t + e + K[i / 20] + W[i];
				e = d;
				d = c;
				c = rotl (b, 30);
				b = a;
				a = T;
			}

			H[0] = a + H[0];
			H[1] = b + H[1];
			H[2] = c + H[2];
			H[3] = d + H[3];
			H[4] = e + H[4];

			c_fill = 0;
		}
	}
}


void sha1_ctx::get_hash (char *hash) noexcept
{
	/* Pad  + append length */
	unsigned char trailer[64 + 9] = { 0x80, 0 };

	unsigned trailer_length = 16 * 4 - c_fill;

	if (trailer_length < 9)
		trailer_length += 64;

	*((uint64_t*) (trailer + trailer_length - 8)) = htobe64(l);

	input_bytes ((char*) trailer, trailer_length);

	/* copy hash */
	for (unsigned i = 0; i < 5; i++)
		((uint32_t*) hash)[i] = htobe32 (H[i]);
}


};
