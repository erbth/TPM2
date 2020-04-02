/** This file is part of the TSClient LEGACY Package Manager
 *
 * Common message digest calculation
 *
 * Error reporting through exceptions. */

#ifndef __MESSAGE_DIGEST_H
#define __MESSAGE_DIGEST_H

namespace message_digest
{
	void sha1_memory (const char *data, size_t data_size, const char *sum);

	void sha1_file (const char *path, const char *sum);
};

#endif /* __MESSAGE_DIGEST_H */
