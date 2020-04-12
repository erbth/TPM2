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


string simplify_path (const string& path)
{
	char last = 0;
	string new_path;

	for (auto c : path)
	{
		if (c != '/' || last != '/')
			new_path += c;

		last = c;
	}

	return new_path;
}


string sha1_to_string (const char sha1[])
{
	string s;
	s.reserve (60);

	for (int i = 0; i < 20; i++)
	{
		if (s.size() > 0)
			s += ':';

		char d1 = (sha1[i] >> 4) & 0xf;
		char d2 = sha1[i] & 0xf;

		d1 = d1 > 9 ? d1 - 10 + 'a' : d1 + '0';
		d2 = d2 > 9 ? d2 - 10 + 'a' : d2 + '0';

		s += d1;
		s += d2;
	}

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
