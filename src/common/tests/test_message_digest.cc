#define BOOST_TEST_MODULE test_message_digest

#include <boost/test/included/unit_test.hpp>
#include "message_digest.h"
#include <cstring>

using namespace std;
namespace md = message_digest;


BOOST_AUTO_TEST_CASE (test_nist_vectors)
{
	char sum1[20];
	char sum2[20];

	const char *msg1 = "abc";
	const char *msg2 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

	unsigned char ref1[20] = { 0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E, 0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D };
	unsigned char ref2[20] = { 0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E, 0xBA, 0xAE, 0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5, 0xE5, 0x46, 0x70, 0xF1 };

	md::sha1_memory (msg1, strlen (msg1), sum1);
	md::sha1_memory (msg2, strlen (msg2), sum2);

	BOOST_TEST (memcmp (sum1, ref1, 20) == 0);
	BOOST_TEST (memcmp (sum2, ref2, 20) == 0);
}
