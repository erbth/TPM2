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
#include "pkg_tools.h"
#include "repo_tools.h"
#include "compare_system.h"

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
"\n"
"This is version two, which is entirely written in C++. It uses GNU Tar, zlib,\n"
"TinyXML2 and SQLite3 as package database.\n\n"

"Specifying packages: Each package description may look like name@arch>=s:version.\n"
"Each description is one parameter. The 's:' is optional and indicates source\n"
"package version in contrast to binary package version. @arch and >=version are\n"
"optional, one of them may be given alone, or both. Or none. arch can be amd64,\n"
"i386 or any other supported architecture. Instead of >= one may use <=, >, <,\n"
"!=, = or == where the latter two mean the same. Version number is a version\n"
"number in the format like verywhere else within TSClient LEGACY.\n\n"

"Parameters:\n"
"  --version               Print the program's version\n\n"

"  --target                Root of the managed system's filesystem\n\n"

"  --verbose               Enable verbose output\n\n"

"  --install               Install or uprade the specified packages\n\n"

"  --upgrade               If packages are specified, install or upgrade them\n"
"                          but use a stronger bias to upgrading them compared\n"
"                          to --install. If no packages are specified, try to\n"
"                          upgrade all installed packages.\n\n"

"  --adopt-all             Adopt all files without asking. This will also\n"
"                          overwrite config files that are part of a package to\n"
"                          be installed/changed and exist on the system already\n"
"                          with different content.\n\n"

"  --assume-yes            Do not ask for confirmation if the operation shall\n"
"                          be performed on the packages. However this does not\n"
"                          disable the prompts for adopting files.\n\n"

"  --list-available        Show the installed and available versions of a\n"
"                          package\n\n"

"  --show-version          Print a package's version number or `---' if it is\n"
"                          not installed\n\n"

"  --remove                Remove specified packages and their config files if\n"
"                          they were not modified\n\n"

"  --removal_graph         Prints the entire removal graph, which determines the\n"
"                          packages that will be removed when a particular one is\n"
"                          requested to be removed and in which order, if no\n"
"                          package is specified. If one is specified this prints\n"
"                          the particular branch that will be removed.\n\n"

"  --remove-unneeded       Remove all packages that were marked as automatically\n"
"                          installed and are not required by other packages that\n"
"                          are marked as manually installed. This argument may\n"
"                          also be given as an option to --remove and\n"
"                          --removal-graph.\n\n"

"  --list-installed        List all installed packages and their states\n\n"

"  --show-problems         Show all problems with the current installation\n"
"                          (i.e. halfly installed packages after an interruption\n"
"                          or missing dependencies)\n\n"

"  --installation-graph    Print the dependency graph in the dot format; If\n"
"                          packages are specified, they are added to the graph.\n\n"

"  --reverse-dependencies  List the packages that depend or pre-depend on the\n"
"                          specified packages directly or indirectly.\n\n"

"  --mark-manual           Mark the specified packages as manually installed\n\n"

"  --mark-auto             Mark the specified packages as automatically installed\n\n"

"  --compare-system        Compare the installed files with the files in the\n"
"                          database. The database only stores the file type, and\n"
"                          SHA1 checksum of regular files and target paths of\n"
"                          symlinks, hence only these paremeters are compared.\n\n\n"


"Repository tools:\n"
"  --create-index <dir> [<name>]  Create indeces for a repository's\n"
"                                 architectures. The root of the repository is\n"
"                          given as <dir> and the index is named <name> if a\n"
"                          name is given. If no name is given, the index is\n"
"                          called 'index'. If an index with that name exists\n"
"                          already, it is overwritten atomically.\n\n"

"  --sign <key>            Sign the index with the given RSA key (must be in PEM\n"
"                          format).\n\n"

"  --help                  Display this list of options\n\n"

"At least one operation must be specified.\n"
);

}


/* Commandline parser support class */
struct ParserState
{
	static const char NOT_SPECIFIED = 0;
	static const char AWAITING = 1;
	static const char SPECIFIED = 2;

	char target = NOT_SPECIFIED;
	char operation = NOT_SPECIFIED;
	char create_index_repo = NOT_SPECIFIED;
	char create_index_name = NOT_SPECIFIED;
	char sign = NOT_SPECIFIED;
};


int _main(int argc, char** argv)
{
	auto params = make_shared<Parameters>();

	params->read_from_env();

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

			if (state.create_index_repo == state.AWAITING)
			{
				printf("--create-index must be followed by a path.\n");
				return 2;
			}

			if (state.sign == state.AWAITING)
			{
				printf("--sign must be followed by a path.\n");
				return 2;
			}

			if (state.create_index_name == state.AWAITING)
				state.create_index_name = state.NOT_SPECIFIED;

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
			else if (option == "verbose")
			{
				params->verbose = true;
			}
			else if (option == "adopt-all")
			{
				params->adopt_all = true;
			}
			else if (option == "assume-yes")
			{
				params->assume_yes = true;
			}
			else if (option == "install")
			{
				params->operation = OPERATION_INSTALL;
				state.operation = state.SPECIFIED;
			}
			else if (option == "upgrade")
			{
				params->operation = OPERATION_UPGRADE;
				state.operation = state.SPECIFIED;
			}
			else if (option == "list-available")
			{
				params->operation = OPERATION_LIST_AVAILABLE;
				state.operation = state.SPECIFIED;
			}
			else if (option == "show-version")
			{
				params->operation = OPERATION_SHOW_VERSION;
				state.operation = state.SPECIFIED;
			}
			else if (option == "remove")
			{
				params->operation = OPERATION_REMOVE;
				state.operation = state.SPECIFIED;
			}
			else if (option == "removal-graph")
			{
				params->operation = OPERATION_REMOVAL_GRAPH;
				state.operation = state.SPECIFIED;
			}
			else if (option == "remove-unneeded")
			{
				if (state.operation == state.SPECIFIED &&
						(params->operation == OPERATION_REMOVE ||
						 params->operation == OPERATION_REMOVAL_GRAPH)
					)
				{
					params->autoremove = true;
				}
				else
				{
					params->operation = OPERATION_REMOVE_UNNEEDED;
					state.operation = state.SPECIFIED;
				}
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
			else if (option == "compare-system")
			{
				params->operation = OPERATION_COMPARE_SYSTEM;
				state.operation = state.SPECIFIED;
			}
			else if (option == "create-index")
			{
				if (state.create_index_repo != state.NOT_SPECIFIED)
				{
					printf("--create-index may only be specified once.\n");
					return 2;
				}

				params->operation = OPERATION_CREATE_INDEX;
				state.operation = state.SPECIFIED;
				state.create_index_repo = state.AWAITING;
			}
			else if (option == "sign")
			{
				if (state.sign != state.NOT_SPECIFIED)
				{
					printf("--sign may only be specified once.\n");
					return 2;
				}

				state.sign = state.AWAITING;
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
			else if (state.create_index_repo == state.AWAITING)
			{
				params->create_index_repo = parameter;
				state.create_index_repo = state.SPECIFIED;
				state.create_index_name = state.AWAITING;
			}
			else if (state.create_index_name == state.AWAITING)
			{
				if (parameter.size() < 1)
				{
					printf("The index name must not be empty.\n");
					return 2;
				}

				params->create_index_name = parameter;
				state.create_index_name = state.SPECIFIED;
			}
			else if (state.sign == state.AWAITING)
			{
				params->sign = parameter;
				state.sign = state.SPECIFIED;
			}
			else
			{
				if (state.operation == state.SPECIFIED)
				{
					if (
							params->operation == OPERATION_INSTALL ||
							params->operation == OPERATION_UPGRADE ||
							params->operation == OPERATION_INSTALLATION_GRAPH ||
							params->operation == OPERATION_REMOVE ||
							params->operation == OPERATION_REMOVAL_GRAPH ||
							params->operation == OPERATION_MARK_MANUAL ||
							params->operation == OPERATION_MARK_AUTO ||
							params->operation == OPERATION_SHOW_VERSION ||
							params->operation == OPERATION_REVERSE_DEPENDENCIES ||
							params->operation == OPERATION_LIST_AVAILABLE
					   )
					{
						if (
								params->operation == OPERATION_SHOW_VERSION ||
								params->operation == OPERATION_LIST_AVAILABLE
							)
						{
							if (!params->operation_packages.empty())
							{
								printf ("This operation accepts only one package as argument.\n");
								return 2;
							}
						}

						params->operation_packages.push_back(parameter);
					}
					else
					{
						printf ("This operation does not accept packages as arguments.\n");
						return 2;
					}
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

	if (state.create_index_repo == state.AWAITING)
	{
		printf("--create-index must be followed by a path.\n");
		return 2;
	}

	if (state.create_index_name != state.AWAITING)
		state.create_index_name = state.NOT_SPECIFIED;

	if (state.sign == state.AWAITING)
	{
		printf("--sign must be followed by a path.\n");
		return 2;
	}

	if (state.operation != state.SPECIFIED)
	{
		printf ("Error: no operation specified\n");
		return 2;
	}

	/* Operations that don't require a config file */
	switch(params->operation)
	{
		case OPERATION_CREATE_INDEX:
			return repo_tools::create_index(params) ? 0 : 1;

		default:
			break;
	}

	/* Read the config file */
	if (!read_config_file (params))
		return 1;

	/* Perform the specified operation. */
	switch(params->operation)
	{
	case OPERATION_INSTALL:
		return install_packages(params, false) ? 0 : 1;

	case OPERATION_UPGRADE:
		return install_packages(params, true) ? 0 : 1;

	case OPERATION_INSTALLATION_GRAPH:
		return print_installation_graph(params) ? 0 : 1;

	case OPERATION_REMOVE:
		return remove_packages (params, params->autoremove) ? 0 : 1;

	case OPERATION_REMOVE_UNNEEDED:
		/* Just to be sure if the parser allowed to specify packages after
		 * autoremove. */
		params->operation_packages.clear();
		return remove_packages (params, true) ? 0 : 1;

	case OPERATION_REMOVAL_GRAPH:
		return print_removal_graph (params, params->autoremove) ? 0 : 1;

	case OPERATION_LIST_INSTALLED:
		return list_installed_packages(params) ? 0 : 1;

	case OPERATION_REVERSE_DEPENDENCIES:
		return list_reverse_dependencies(params) ? 0 : 1;

	case OPERATION_SHOW_VERSION:
		return show_version (params) ? 0 : 1;

	case OPERATION_LIST_AVAILABLE:
		return list_available (params) ? 0 : 1;

	case OPERATION_SHOW_PROBLEMS:
		return show_problems (params) ? 0 : 1;

	case OPERATION_MARK_MANUAL:
		return set_installation_reason (INSTALLATION_REASON_MANUAL, params) ? 0 : 1;

	case OPERATION_MARK_AUTO:
		return set_installation_reason (INSTALLATION_REASON_AUTO, params) ? 0 : 1;

	case OPERATION_COMPARE_SYSTEM:
		return compare_system (params) ? 0 : 1;

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
