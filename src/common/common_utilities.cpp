#include <cerrno>
#include <climits>
#include <cstdlib>
#include <system_error>
#include <filesystem>
#include "common_utilities.h"

extern "C" {
#include <unistd.h>
}

using namespace std;
namespace fs = std::filesystem;


string get_absolute_path (const string& path)
{
	string abs_path = fs::absolute(path);

	/* No we have an absolute path. Simplify it! */
	/* Do a few trials and throw a system_error if it doesn't work. */
	char *buf = nullptr;
	size_t buf_size = 1024;

	for (;;)
	{
		buf = (char*) malloc (buf_size * sizeof(char));
		if (!buf)
			throw system_error (error_code (errno, generic_category()));

		char *ret = realpath (abs_path.c_str(), buf);

		if (!ret)
		{
			free (buf);
			buf = nullptr;

			buf_size *= 2;

			if (buf_size >= 1000000)
				throw system_error (error_code(ENOENT, generic_category()));

			continue;
		}

		break;
	}


	/* One more copy ... such an interface shizzle! */
	string s(buf);

	free (buf);

	return s;
}


string convenient_readlink (const std::string& path)
{
	return convenient_readlink (path.c_str());
}


string convenient_readlink (const char *path)
{
	char *buf = nullptr;
	ssize_t buf_size = 1024;
	ssize_t ret;

	for (;;)
	{
		buf = (char*) malloc (buf_size);
		if (!buf)
			throw system_error (error_code (errno, generic_category()));

		ret = readlink (path, buf, buf_size);
		if (ret < 0)
		{
			free (buf);
			throw system_error (error_code (errno, generic_category()));
		}

		if (ret < buf_size)
			break;

		/* Do another round. */
		free (buf);
		buf = nullptr;

		buf_size *= 2;

		if (buf_size >= 10000000)
			throw system_error (error_code (ENOENT, generic_category()));
	}

	string s(buf, ret);
	free (buf);
	return s;
}


gp_exception::gp_exception (const string& msg)
	: msg(msg)
{
}

const char *gp_exception::what() const noexcept
{
	return msg.c_str();
}
