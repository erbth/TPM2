/** This file is part of the TSClientLEGACY Package Manager.
 *
 * This module contains the entrypoint for the program tpm2.
 */

#include <cstdio>
#include <memory>
#include <string>
#include "tpm2_config.h"
#include "installation.h"
#include "parameters.h"
#include "package_db.h"
#include "utility.h"

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
}


using namespace std;


/* Functions of the UI */
void print_version()
{
	printf ("TSClient LEGACY Package Manager version %d.%d.%d\n",
			TPM2_VERSION_MAJOR,
			TPM2_VERSION_MINOR,
			TPM2_VERSION_PATCH);
}

void print_help()
{
	print_version();

	printf(
"  --version               Print the program's version\n\n"

"  --target                Root of the managed system's filesystem\n\n"

"  --install               Install or uprade the specified packages\n\n"

"  --reinstall             Like install but reinstalls the specified packages\n"
"                          even if the same version is already installed\n\n"

"  --policy                Show the installed and available versions of name\n\n"

"  --show-version          Print a package's version number or `---' if it is\n"
"                          not installed\n\n"

"  --remove                Remove specified packages and their config files if\n"
"                          they were not modified\n\n"

"  --remove-unneeded       Remove all packages that were marked as automatically\n"
"                          installed and are not required by other packages that\n"
"                          are marked as manually installed\n\n"

"  --list-installed        List all installed packages\n\n"

"  --show-problems         Show all problems with the current installation\n"
"                          (i.e. halfly installed packages after an interruption\n"
"                          or missing dependencies)\n\n"

"  --recover               Recover from a dirty state by deleting all packages\n"
"                          that are in a dirty state (always possible due to\n"
"                          atomic write operations to status\n\n"

"  --installation-graph    Print the dependency graph in the dot format; If\n"
"                          packages are specified, they are added to the graph.\n\n"

"  --reverse-dependencies  List the List the packages that depend on the\n"
"                          specified package directly or indirectly\n\n"

"  --mark-manual           Mark the specified packages as manually installed\n\n"

"  --mark-auto             Mark the specified packages as automatically installed\n\n"

"  --help                  Display this list of options\n\n"
);

}


/* Commandline parser support library */
struct ParserState
{
	static const char NOT_SPECIFIED = 0;
	static const char AWAITING = 1;
	static const char SPECIFIED = 2;

	char target = NOT_SPECIFIED;
	char operation = NOT_SPECIFIED;
};


int _main(int argc, char** argv)
{
	auto params = make_shared<Parameters>();

	/* Set the umask to a defined value such that directories and files are
	 * created with right permissions. */
	umask (S_IWGRP | S_IWOTH);

	/* A simple DFA-like commandline parser */
	ParserState state;

	for (int i = 1; i < argc; i++)
	{
		string parameter = argv[i];

		if (parameter.rfind("--", 0) == 0)
		{
			auto option = parameter.substr(2);

			if (state.target == state.AWAITING)
			{
				printf("--target must be followed by a path.\n");
				return 2;
			}

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
			else if (option == "target")
			{
				if (state.target == state.NOT_SPECIFIED)
				{
					state.target = state.AWAITING;
				}
				else
				{
					printf("Only one target may be specified.\n");
					return 2;
				}
			}
			else if (option == "install")
			{
				params->operation = OPERATION_INSTALL;
				state.operation = state.SPECIFIED;
			}
			else if (option == "reinstall")
			{
				params->operation = OPERATION_REINSTALL;
				state.operation = state.SPECIFIED;
			}
			else if (option == "policy")
			{
				params->operation = OPERATION_POLICY;
				state.operation = state.SPECIFIED;
			}
			else if (option == "show-version")
			{
				params->operation = OPERATION_SHOW_VERSION;
				state.operation = state.SPECIFIED;
			}
			else if (option == "remove-unneeded")
			{
				params->operation = OPERATION_REMOVE_UNNEEDED;
				state.operation = state.SPECIFIED;
			}
			else if (option == "list-installed")
			{
				params->operation = OPERATION_LIST_INSTALLED;
				state.operation = state.SPECIFIED;
			}
			else if (option == "show-problems")
			{
				params->operation = OPERATION_SHOW_PROBLEMS;
				state.operation = state.SPECIFIED;
			}
			else if (option == "recover")
			{
				params->operation = OPERATION_RECOVER;
				state.operation = state.SPECIFIED;
			}
			else if (option == "installation-graph")
			{
				params->operation = OPERATION_INSTALLATION_GRAPH;
				state.operation = state.SPECIFIED;
			}
			else if (option == "reverse-dependencies")
			{
				params->operation = OPERATION_REVERSE_DEPENDENCIES;
				state.operation = state.SPECIFIED;
			}
			else if (option == "mark-manual")
			{
				params->operation = OPERATION_MARK_MANUAL;
				state.operation = state.SPECIFIED;
			}
			else if (option == "mark-auto")
			{
				params->operation = OPERATION_MARK_AUTO;
				state.operation = state.SPECIFIED;
			}
			else
			{
				printf("Invalid option --%s.\n", option.c_str());
				return 2;
			}
		}
		else
		{
			if (state.target == state.AWAITING)
			{
				params->target = get_absolute_path (parameter);
				state.target = state.SPECIFIED;
			}
			else
			{
				if (state.operation == state.SPECIFIED)
				{
					params->operation_packages.push_back(parameter);
				}
				else
				{
					printf("An operation must be specified before packages.\n");
					return 2;
				}
			}
		}
	}

	/* Verify that the state is valid */
	if (state.target == state.AWAITING)
	{
		printf("--target must be followerd by a path.\n");
		return 2;
	}

	if (state.operation != state.SPECIFIED)
	{
		printf ("Error: no operation specified\n");
		return 2;
	}

	/* Perform the specified operation. */
	switch(params->operation)
	{
	case OPERATION_INSTALL:
		return install_packages(params) ? 0 : 1;

	case OPERATION_INSTALLATION_GRAPH:
		return print_installation_graph(params) ? 0 : 1;

	default:
		printf ("Error: this operation is not yet implemented.\n");
		return 1;
	}
}


int main(int argc, char** argv)
{
	try {
		return _main(argc, argv);
	} catch (CannotOpenDB& e) {
		fprintf (stderr, "%s\n", e.what());
		return 1;
	} catch(char* p) {
		fprintf(stderr, "Critical internal error: %s\n", p);
		return 3;
	} catch (const exception& e) {
		fprintf (stderr, "Critical internal error: %s\n", e.what());
		return 3;
	} catch(...) {
		fprintf(stderr, "Critical internal error.\n");
		return 3;
	}
}
