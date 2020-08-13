#include <cstdio>
#include "architecture.h"
#include "read_scenario.h"

using namespace std;

void print_usage()
{
	fprintf(stderr, "Usage: print_scenario <filename> universe | installed | selected | desired\n");
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		print_usage();
		return 1;
	}

	fprintf (stderr, "Reading scenario ...\n");
	auto scenario = read_scenario(argv[1]);
	if (!scenario)
	{
		fprintf(stderr, "Failed.\n");
		return 1;
	}

	fprintf (stderr, "done.\n");

	string action = argv[2];

	if (action == "universe")
	{
		printf("Universe:\n");

		for (auto& pkg : scenario->universe)
		{
			printf ("  %s@%s: [sv = %s, bv = %s]\n",
					pkg->name.c_str(),
					Architecture::to_string(pkg->arch).c_str(),
					pkg->sv.to_string().c_str(),
					pkg->bv.to_string().c_str());

			printf("    Files:\n");
			for (auto& file : pkg->files)
				printf("      %s\n", file.c_str());

			printf("    Directories:\n");
			for (auto& directory : pkg->directories)
				printf("      %s\n", directory.c_str());

			printf("    Dependencies:\n");
			for (auto& dep : pkg->deps)
			{
				auto [name, arch, constr] = dep;

				printf ("      %s@%s %s\n",
						name.c_str(),
						Architecture::to_string(arch).c_str(),
						constr->to_string().c_str());
			}

			printf("\n");
		}
	}
	else if (action == "installed")
	{
		printf("Installed:\n");

		for (auto& p : scenario->installed)
		{
			auto [pkg, manual] = p;

			printf("  %s@%s:%s (%s)\n",
					pkg->name.c_str(),
					Architecture::to_string(pkg->arch).c_str(),
					pkg->bv.to_string().c_str(),
					manual ? "manual" : "auto");
		}
	}
	else if (action == "selected")
	{
		printf("Selected:\n");

		for (auto& t : scenario->selected)
		{
			auto [name, arch, constr] = t;

			printf ("  %s@%s %s\n",
					name.c_str(),
					Architecture::to_string(arch).c_str(),
					constr->to_string().c_str());
		}
	}
	else if (action == "desired")
	{
		printf ("Desired:\n");

		for (auto& ppkg : scenario->desired)
		{
			printf ("  %s@%s:%s\n",
					ppkg->name.c_str(),
					Architecture::to_string(ppkg->arch).c_str(),
					ppkg->bv.to_string().c_str());
		}
	}
	else
	{
		print_usage();
		return 1;
	}
}
