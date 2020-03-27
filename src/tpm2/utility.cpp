#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include "utility.h"

extern "C" {
#include <unistd.h>
}

using namespace std;


void print_target(shared_ptr<Parameters> params)
{
	if (params->target_is_native())
		printf ("Runtime system is native\n");
	else
		printf ("Runtime system is at \"%s\"\n", params->target.c_str());
}


string get_absolute_path (const string& path)
{
	/* Do a few trials and throw a system_error if it doesn't work. */
	char *buf = nullptr;
	size_t buf_size = 1024;

	for (;;)
	{
		buf = (char*) malloc(buf_size * sizeof(char));
		if (!buf)
			throw system_error (error_code(errno, generic_category()));

		if (!getcwd (buf, buf_size) || buf_size - strlen(buf) - 2 < path.size())
		{
			free (buf);
			buf = nullptr;

			buf_size *= 2;

			if (buf_size >= 1000000)
				throw system_error (error_code(ENOMEM, generic_category()));

			continue;
		}

		strcat (buf, "/");
		strcat (buf, path.c_str());

		/* No we have an absolute path. Simplify it! */
		char *oldbuf = buf;

		buf = (char*) malloc (buf_size * sizeof(char));
		if (!buf)
		{
			free (oldbuf);
			throw system_error (error_code (errno, generic_category()));
		}

		char *ret = realpath (oldbuf, buf);
		free (oldbuf);

		if (!ret)
		{
			free (buf);
			buf = nullptr;

			buf_size *= 2;

			if (buf_size >= 1000000)
				throw system_error (error_code(ENOMEM, generic_category()));

			continue;
		}

		break;
	}


	/* One more copy ... such an interface shizzle! */
	string s(buf);

	free (buf);

	return s;
}
