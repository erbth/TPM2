/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module includes common utilities to use in various places that are not
 * connected to a program. */

#ifndef __COMMON_UTILITIES_H
#define __COMMON_UTILITIES_H

#include <exception>
#include <istream>
#include <string>


#define MIN(X,Y) (X < Y ? X : Y)
#define MAX(X,Y) (X > Y ? X : Y)

#define COLOR_NORMAL "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"

/* Get the abslute path of a potentially relaive path.
 *
 * @param path The path to expand
 * @returns An absolute path to the specified target
 *
 * @throws std::system_error if the path cannot be resolved. */
std::string get_absolute_path(const std::string& path);

std::string convenient_readlink (const std::string& path);
std::string convenient_readlink (const char *path);


/* Simplify a path, that is remove all double slashes. Compared to libc's
 * readlpath this does not access the filesystem, but likewise not expand
 * symbolic links. Trailing slashes are preserved. */
std::string simplify_path (const std::string& path);

std::string sha1_to_string (const char sha1[]);


/* An exception for general purposes, i.e. when things should not have happened
 * or to deliver a simple error message. */
class gp_exception : public std::exception
{
private:
	std::string msg;

public:
	gp_exception (const std::string& msg);
	const char *what() const noexcept override;
};

#endif /* __COMMON_UTILITIES_H */
