/** This file is part of the TSClient LEGACY Package Manager.
 *
 * This module contains the entrypoint for the program tpm2_pack.
 */

#include <cstdio>
#include <string>
#include "tpm2_config.h"
#include "pack.h"

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}


using namespace std;


/* Functions of the UI */
void print_version()
{
	printf ("TSClient LEGACY Package Manager version %d.%d.%d - package creation program\n",
			TPM2_VERSION_MAJOR,
			TPM2_VERSION_MINOR,
			TPM2_VERSION_PATCH);
}

void print_help()
{
	print_version();

	printf(
"\n"
"This program creates the transport form of packages. The path to the root\n"
"directory of the unpacked from must always be present as unnamed argument, like\n"
"\n    tpm2_pack [options] <path>\n"
".\n"

"\nOptions:\n"
"  --version               Print the program's version\n\n"

"  --help                  Display this help\n\n"
);
}


/* Commandline parser support class */
struct ParserState
{
	static const char NOT_SPECIFIED = 0;
	static const char AWAITING = 1;
	static const char SPECIFIED = 2;

	char unpacked_dir_state = NOT_SPECIFIED;
	string unpacked_dir;
};


int _main (int argc, char** argv)
{
	/* Set the umask to a defined value such that directories and files are
	 * crated with right permissions. */
	umask (S_IWGRP | S_IWOTH);

	/* A simple DFA-like commandline parser */
	ParserState state;

	for (int i = 1; i < argc; i++)
	{
		string parameter = argv[i];

		if (parameter.rfind ("--", 0) == 0)
		{
			auto option = parameter.substr(2);

			if (option == "version")
			{
				print_version();
				return 0;
			}
			else if (option == "help")
			{
				print_help();
				return 0;
			}
			else
			{
				fprintf (stderr, "Invalid argument \"--%s\".\n", option.c_str());
				return 2;
			}
		}
		else
		{
			if (state.unpacked_dir_state == state.NOT_SPECIFIED)
			{
				state.unpacked_dir = parameter;
				state.unpacked_dir_state = state.SPECIFIED;
			}
			else
			{
				fprintf (stderr, "Only one unpacked directory maybe specified.\n");
				return 2;
			}
		}
	}


	/* Verify that the state is valid */
	if (state.unpacked_dir_state != state.SPECIFIED)
	{
		printf ("Specify a task to do.\n");
		return 2;
	}

	return pack (state.unpacked_dir) ? 0 : 1;
}


int main(int argc, char** argv)
{
	try {
		return _main(argc, argv);
	} catch (char* p) {
		fprintf (stderr, "Critical internal error: %s\n", p);
		return 3;
	} catch (const exception& e) {
		fprintf (stderr, "Critical internal error: %s\n", e.what());
		return 3;
	} catch (...) {
		fprintf (stderr, "Critical internal error.\n");
		return 3;
	}
}
