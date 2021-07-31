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
#define COLOR_BRIGHT_YELLOW "\033[93m"

/* ASCII representing half byte to unsigned char (e.g. 'a' -> 0x0a) */
unsigned char ascii_to_half_byte(char c);

/* Two ASCII characters representing a byte to unsigned char (e.g. "ab" -> 0xab)
 * */
unsigned char ascii_to_byte(const char* cs);


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

/* Does not follow symlinks. If the path does not refer to a directory, the
 * bahavior is unspecified. The function does, what the
 * std::filesystem::directory_iterator does then ... */
bool directory_is_empty (const std::string& path);


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


/* Create a temporary file, which will be unlinked when the object is deleted.
 * All operatios (except those marked 'noexcept') may throw a system_error if
 * low-level io operations fail. */
class TemporaryFile
{
protected:
	int file_fd;
	std::string file_path;
	bool unowned = false;

public:
	/* @param name_prefix must be at lead 6 characters long. */
	TemporaryFile (const std::string& name_prefix);
	virtual ~TemporaryFile();

	TemporaryFile (const TemporaryFile&) = delete;
	TemporaryFile (TemporaryFile&&) = delete;

	TemporaryFile& operator= (const TemporaryFile&) = delete;
	TemporaryFile& operator= (TemporaryFile&&) = delete;

	/* Get the file's path */
	std::string path() const noexcept;

	/* Append a string to the file (opens it automatically if required). If the
	 * file has been closed already, throw a gp_exception. */
	void append_string (const std::string& s);

	/* Close the file if it is open */
	void close();

	/* Signal that the actual temporary file should not be owned by this
	 * `TemporaryFile` instance anymore. If called, the file won't be unlinked
	 * by the destructor. Useful after fork(). */
	void set_unowned();
};

#endif /* __COMMON_UTILITIES_H */
