/** Cryptographic tools */
#ifndef __CRYPTO_TOOLS_H
#define __CRYPTO_TOOLS_H

#include <string>

/* Warning: Both functions modifiy the position within fd */
bool verify_sha256_fd (int fd, std::string digest);

/* digest is of size 32 bytes */
bool verify_sha256_fd (int fd, const unsigned char* digest);

#endif /* __CRYPTO_TOOLS_H */
