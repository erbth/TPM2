#include <cerrno>
#include <climits>
#include <cstdlib>
#include <system_error>
#include <filesystem>
#include "common_utilities.h"

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
