/** This file is part of the TSClient LEGACY Package Manager
 *
 * A library for reading testing scenarios, which are used to evaluate
 * dependency solvers. This C++ module contains the implementation. */
#include <cstring>
#include <algorithm>
#include <optional>
#include "architecture.h"
#include "read_scenario.h"
#include "tinyxml2.h"

using namespace std;
using namespace tinyxml2;
namespace pc = PackageConstraints;


optional<tuple<string, int, shared_ptr<pc::Formula>>>
read_required_package(XMLElement* root)
{
	XMLElement* ce = root->FirstChildElement();

	const char* name = nullptr;
	int arch = Architecture::invalid;
	shared_ptr<pc::Formula> constr = make_shared<pc::And>(nullptr, nullptr);

	while (ce)
	{
		auto key = ce->Name();

		if (strcmp(key, "name") == 0)
		{
			if (name)
			{
				fprintf(stderr, "Required package has two names (second on line %d).\n",
						ce->GetLineNum());
				return nullopt;
			}

			const char* val = ce->GetText();
			if (!val || strlen(val) == 0)
			{
				fprintf(stderr, "Empty name of required package on line %d.\n",
						ce->GetLineNum());
				return nullopt;
			}

			name = val;
		}
		else if (strcmp(key, "arch") == 0)
		{
			if (arch != Architecture::invalid)
			{
				fprintf(stderr, "Required package has two architecture definitions (second on line %d).\n",
						ce->GetLineNum());
				return nullopt;
			}

			const char* val = ce->GetText();
			int tmp;

			if (val)
			{
				try
				{
					tmp = Architecture::from_string(val);
				}
				catch (InvalidArchitecture&)
				{
					val = nullptr;
				}
			}

			if (!val)
			{
				fprintf(stderr, "Invalid architecture of required package on line %d.\n",
						ce->GetLineNum());
				return nullopt;
			}

			arch = tmp;
		}
		else if (strcmp(key, "constr") == 0 || strcmp(key, "sconstr") == 0)
		{
			const char *stype = ce->Attribute("type");

			if (!stype)
			{
				fprintf(stderr, "Constraint on line %d has not type.\n",
						ce->GetLineNum());
				return nullopt;
			}

			/* Decode type */
			char type;

			if (strcmp(stype, "eq") == 0)
			{
				type = pc::PrimitivePredicate::TYPE_EQ;
			}
			else if (strcmp(stype, "neq") == 0)
			{
				type = pc::PrimitivePredicate::TYPE_NEQ;
			}
			else if (strcmp(stype, "geq") == 0)
			{
				type = pc::PrimitivePredicate::TYPE_GEQ;
			}
			else if (strcmp(stype, "leq") == 0)
			{
				type = pc::PrimitivePredicate::TYPE_LEQ;
			}
			else if (strcmp(stype, "gt") == 0)
			{
				type = pc::PrimitivePredicate::TYPE_GT;
			}
			else if (strcmp(stype, "lt") == 0)
			{
				type = pc::PrimitivePredicate::TYPE_LT;
			}
			else
			{
				fprintf(stderr, "Invalid constraint type \"%s\" on line %d.\n",
						stype, ce->GetLineNum());
				return nullopt;
			}

			/* Decode version number */
			auto tmp = ce->GetText();

			if (tmp)
			{
				try
				{
					VersionNumber v(tmp);

					/* Add predicate */
					constr = make_shared<pc::And>(
							constr,
							make_shared<pc::PrimitivePredicate>(key[0] == 's', type, move(v)));
				}
				catch (InvalidVersionNumberString& e)
				{
					fprintf(stderr, "%s\n", e.what());
					tmp = nullptr;
				}
			}

			if (!tmp)
			{
				fprintf(stderr, "Invalid version number on line %d.\n", ce->GetLineNum());
				return nullopt;
			}
		}
		else
		{
			fprintf(stderr, "Invalid element \"%s\" in required package on line %d.\n",
					key, ce->GetLineNum());
			return nullopt;
		}

		if (ce != root->LastChildElement())
			ce = ce->NextSiblingElement();
		else
			ce = nullptr;
	}

	if (!name || strlen(name) == 0)
	{
		fprintf(stderr, "Required package on line %d misses a name.\n",
				root->GetLineNum());
		return nullopt;
	}

	if (arch == Architecture::invalid)
	{
		fprintf(stderr, "Required package on line %d misses an architecture.\n",
				root->GetLineNum());
		return nullopt;
	}

	return make_tuple(string(name), arch, constr);
}

std::shared_ptr<Scenario::Package> read_package_description (
		std::shared_ptr<Scenario> scenario, XMLElement *root)
{
	XMLElement *elem = root->FirstChildElement();

	const char *name = nullptr;
	int arch = Architecture::invalid;
	optional<VersionNumber> version;

	while (elem)
	{
		auto n = elem->Name();

		if (strcmp(n, "name") == 0)
		{
			if (name)
			{
				fprintf(stderr, "Package description has two names (second on line %d).\n",
						elem->GetLineNum());
				return nullptr;
			}

			const char *val = elem->GetText();
			if (!val || strlen(val) == 0)
			{
				fprintf(stderr, "Empty name of package description on line %d.\n",
						elem->GetLineNum());
				return nullptr;
			}

			name = val;
		}
		else if (strcmp(n, "arch") == 0)
		{
			if (arch != Architecture::invalid)
			{
				fprintf (stderr, "Package description has two architecture definitions (second on line %d).\n",
						elem->GetLineNum());
				return nullptr;
			}

			const char *val = elem->GetText();
			int tmp;

			if (val)
			{
				try
				{
					tmp = Architecture::from_string(val);
				}
				catch (InvalidArchitecture&)
				{
					val = nullptr;
				}
			}

			if (!val)
			{
				fprintf (stderr, "Invalid architecture of package description on line %d.\n",
						elem->GetLineNum());
				return nullptr;
			}

			arch = tmp;
		}
		else if (strcmp(n, "version") == 0)
		{
			if (version)
			{
				fprintf (stderr, "Package description has two version numbers (second on line %d).\n",
						elem->GetLineNum());
				return nullptr;
			}

			auto tmp = elem->GetText();
			if (tmp)
			{
				try
				{
					version = VersionNumber(tmp);
				}
				catch (InvalidVersionNumberString& e)
				{
					fprintf(stderr, "Invalid version number on line %d: %s.\n",
							elem->GetLineNum(), e.what());
					return nullptr;
				}
			}
		}
		else
		{
			fprintf (stderr, "Invalid element \"%s\" in package description on line %d.\n",
					n, elem->GetLineNum());
			return nullptr;
		}

		if (elem != root->LastChildElement())
			elem = elem->NextSiblingElement();
		else
			elem = nullptr;
	}

	/* Test if all required information has been given */
	if (!name)
	{
		fprintf(stderr, "Package description on line %d misses a name.\n",
				root->GetLineNum());
		return nullptr;
	}
	
	if (arch == Architecture::invalid)
	{
		fprintf (stderr, "Package description on line %d misses an architecture.\n",
				root->GetLineNum());
		return nullptr;
	}
	
	if (!version)
	{
		fprintf (stderr, "Package description on line %d misses a version number.\n",
				root->GetLineNum());
		return nullptr;
	}

	/* Try to find the specified package in the universe. */
	auto i = find_if (scenario->universe.begin(), scenario->universe.end(),
			[name, arch, version](auto ppkg) {
				return ppkg->name == name && ppkg->arch == arch && ppkg->bv == *version;
			});

	if (i == scenario->universe.end())
	{
		fprintf (stderr, "A package with name \"%s\", defined on line %d, does not exist in the universe.\n",
				name, root->GetLineNum());
		return nullptr;
	}

	return *i;
}


bool read_universe(shared_ptr<Scenario> scenario, XMLElement* root)
{
	XMLElement* pkge = root->FirstChildElement();

	while (pkge)
	{
		if (strcmp(pkge->Name(), "pkg") != 0)
		{
			fprintf(stderr, "Invalid element \"%s\" in universe on line %d.\n",
					pkge->Name(), pkge->GetLineNum());

			return false;
		}


		XMLElement* ce = pkge->FirstChildElement();

		auto pkg = make_shared<Scenario::Package>();

		while (ce)
		{
			const char *n = ce->Name();

			if (strcmp(n, "name") == 0)
			{
				auto value = ce->GetText();

				if (!value || strlen(value) == 0)
				{
					fprintf(stderr, "Empty name of package on line %d.\n", ce->GetLineNum());
					return false;
				}

				pkg->name = value;
			}
			else if (strcmp(n, "arch") == 0)
			{
				auto value = ce->GetText();

				if (value)
				{
					try
					{
						pkg->arch = Architecture::from_string(value);
					}
					catch (InvalidArchitecture&)
					{
						value = nullptr;
					}
				}

				if (!value)
				{
					fprintf(stderr, "Invalid architecture of package on line %d.\n", ce->GetLineNum());
					return false;
				}
			}
			else if (strcmp(n, "source_version") == 0)
			{
				auto value = ce->GetText();

				if (value)
				{
					try
					{
						pkg->sv = VersionNumber(value);
					}
					catch (InvalidVersionNumberString& e)
					{
						fprintf(stderr, "%s\n", e.what());
						value = nullptr;
					}
				}

				if (!value)
				{
					fprintf(stderr, "Invalid source version of package on line %d.\n", ce->GetLineNum());
					return false;
				}
			}
			else if (strcmp(n, "binary_version") == 0)
			{
				auto value = ce->GetText();

				if (value)
				{
					try
					{
						pkg->bv = VersionNumber(value);
					}
					catch (InvalidVersionNumberString& e)
					{
						fprintf(stderr, "%s\n", e.what());
						value = nullptr;
					}
				}

				if (!value)
				{
					fprintf(stderr, "Invalid binary version of package on line %d.\n", ce->GetLineNum());
					return false;
				}
			}
			else if (strcmp(n, "files") == 0)
			{
				auto cf = ce->FirstChildElement();

				while (cf)
				{
					if (strcmp(cf->Name(), "f") == 0)
					{
						const char* f = cf->GetText();

						if (!f || strlen(f) == 0)
						{
							fprintf(stderr, "Empty file on line %d.\n", cf->GetLineNum());
							return false;
						}

						pkg->files.push_back(f);
					}
					else if (strcmp(cf->Name(), "d") == 0)
					{
						const char* d = cf->GetText();

						if (!d || strlen(d) == 0)
						{
							fprintf(stderr, "Empty directory on line %d.\n", cf->GetLineNum());
							return false;
						}

						pkg->directories.push_back(d);
					}
					else
					{
						fprintf(stderr, "Invalid element \"%s\" in files on line %d.\n",
								cf->Name(), cf->GetLineNum());
						return false;
					}

					if (cf != ce->LastChildElement())
						cf = cf->NextSiblingElement();
					else
						cf = nullptr;
				}
			}
			else if (strcmp(n, "deps") == 0)
			{
				auto cd = ce->FirstChildElement();

				while (cd)
				{
					if (strcmp(cd->Name(), "pkg") != 0)
					{
						fprintf(stderr, "Invalid element \"%s\" in a package's dependencies on line %d.\n",
								cd->Name(), cd->GetLineNum());
						return false;
					}

					auto t = read_required_package(cd);
					if (!t)
					{
						fprintf(stderr, "Failed to read a package's dependency.\n");
						return false;
					}

					pkg->deps.push_back(*t);

					if (cd != ce->LastChildElement())
						cd = cd->NextSiblingElement();
					else
						cd = nullptr;
				}
			}
			else
			{
				fprintf(stderr, "Invalied element \"%s\" in package on line %d.\n",
						n, ce->GetLineNum());

				return false;
			}

			if (ce != pkge->LastChildElement())
				ce = ce->NextSiblingElement();
			else
				ce = nullptr;
		}

		scenario->universe.push_back(pkg);


		if (pkge != root->LastChildElement())
			pkge = pkge->NextSiblingElement();
		else
			pkge = nullptr;
	}

	return true;
}

bool read_installed(shared_ptr<Scenario> scenario, XMLElement* root)
{
	XMLElement *pkge = root->FirstChildElement();

	while (pkge)
	{
		if (strcmp(pkge->Name(), "pkg") != 0)
		{
			fprintf(stderr, "Invalid element \"%s\" in installed on line %d.\n",
					pkge->Name(), pkge->GetLineNum());
			return false;
		}


		XMLElement *ce = pkge->FirstChildElement();
		const char *name = nullptr;
		int arch = Architecture::invalid;
		optional<VersionNumber> version;
		optional<bool> manual;

		while (ce)
		{
			auto key = ce->Name();

			if (strcmp(key, "name") == 0)
			{
				if (name)
				{
					fprintf(stderr, "Installed package \"%s\" has multiple names (second on line %d).\n",
							name, ce->GetLineNum());
					return false;
				}

				auto val = ce->GetText();

				if (!val || strlen(val) == 0)
				{
					fprintf(stderr, "Empty name of installed package on line %d.\n",
							ce->GetLineNum());
					return false;
				}

				name = val;
			}
			else if (strcmp(key, "arch") == 0)
			{
				if (arch != Architecture::invalid)
				{
					fprintf(stderr, "Duplicate architecture of installed package on line %d.\n",
							ce->GetLineNum());
					return false;
				}

				auto val = ce->GetText();

				if (val)
				{
					try
					{
						arch = Architecture::from_string(val);
					}
					catch (InvalidArchitecture&)
					{
						val = nullptr;
					}
				}

				if (!val)
				{
					fprintf(stderr, "Invalid architecture of installed package on line %d.\n",
							ce->GetLineNum());
					return false;
				}
			}
			else if (strcmp(key, "version") == 0)
			{
				if (version)
				{
					fprintf(stderr, "Duplicate version number of installed package on line %d.\n",
							ce->GetLineNum());
					return false;
				}

				auto val = ce->GetText();

				if (val)
				{
					try
					{
						version = VersionNumber(val);
					}
					catch (InvalidVersionNumberString& e)
					{
						fprintf(stderr, "%s\n", e.what());
						val = nullptr;
					}
				}

				if (!val)
				{
					fprintf(stderr, "Invalid version number of installed package on line %d.\n",
							ce->GetLineNum());
					return false;
				}
			}
			else if (strcmp(key, "manual") == 0)
			{
				if (manual)
				{
					fprintf(stderr, "Duplicate manual-attribute of installed package on line %d.\n",
							ce->GetLineNum());
					return false;
				}

				auto val = ce->GetText();

				if (val)
				{
					if (strcmp(val, "true") == 0)
						manual = true;
					else if (strcmp(val, "false") == 0)
						manual = false;
					else
						val = nullptr;
				}

				if (!val)
				{
					fprintf(stderr, "Invalid manual-attribute of installed package on line %d "
							"(must be \"true\" or \"false\").\n",
							ce->GetLineNum());
					return false;
				}
			}
			else
			{
				fprintf(stderr, "Invalid element \"%s\" of installed package on line %d.\n",
						key, ce->GetLineNum());
				return false;
			}

			if (ce != pkge->LastChildElement())
				ce = ce->NextSiblingElement();
			else
				ce = nullptr;
		}

		/* Check if all attributes are present */
		if (!name)
		{
			fprintf(stderr, "No name of installed package on line %d defined.\n",
					pkge->GetLineNum());
			return false;
		}

		if (arch == Architecture::invalid)
		{
			fprintf(stderr, "No architecture of installed package on line %d defined.\n",
					pkge->GetLineNum());
			return false;
		}

		if (!version)
		{
			fprintf(stderr, "No version of installed package on line %d defined.\n",
					pkge->GetLineNum());
			return false;
		}

		if (!manual)
		{
			fprintf(stderr, "Installed package on line %d has no manual attribute.\n",
					pkge->GetLineNum());
			return false;
		}

		/* Try to find a matching package in the universe. */
		bool found = false;
		string str_name = name;

		for (auto& pkg : scenario->universe)
		{
			if (pkg->name == str_name && pkg->arch == arch && pkg->bv == version)
			{
				scenario->installed.push_back(make_pair(pkg, *manual));

				found = true;
				break;
			}
		}

		if (!found)
		{
			fprintf(stderr, "Installed package %s@%s:%s, defined on line %d, not found in universe.\n",
					name, Architecture::to_string(arch).c_str(),
					version->to_string().c_str(), pkge->GetLineNum());
			return false;
		}


		if (pkge != root->LastChildElement())
			pkge = pkge->NextSiblingElement();
		else
			pkge = nullptr;
	}

	return true;
}

bool read_selected(shared_ptr<Scenario> scenario, XMLElement* root)
{
	XMLElement *pkge = root->FirstChildElement();

	while (pkge)
	{
		if (strcmp(pkge->Name(), "pkg") != 0)
		{
			fprintf(stderr, "Invalid element \"%s\" in selected on line %d.\n",
					pkge->Name(), pkge->GetLineNum());
			return false;
		}


		auto t = read_required_package(pkge);
		if (!t)
		{
			fprintf(stderr, "Failed to read a selected package.\n");
			return false;
		}

		scenario->selected.push_back(*t);


		if (pkge != root->LastChildElement())
			pkge = pkge->NextSiblingElement();
		else
			pkge = nullptr;
	}

	return true;
}

bool read_desired(shared_ptr<Scenario> scenario, XMLElement *root)
{
	XMLElement *pkge = root->FirstChildElement();

	while (pkge)
	{
		if (strcmp(pkge->Name(), "pkg") != 0)
		{
			fprintf(stderr, "Invalid element \"%s\" in desired on line %d.\n",
					pkge->Name(), pkge->GetLineNum());
			return false;
		}

		auto ppkg = read_package_description(scenario, pkge);
		if (!ppkg)
		{
			fprintf (stderr, "Failed to read a desired package.\n");
			return false;
		}

		scenario->desired.push_back(ppkg);

		if (pkge != root->LastChildElement())
			pkge = pkge->NextSiblingElement();
		else
			pkge = nullptr;
	}

	return true;
}


shared_ptr<Scenario> read_scenario(string filename)
{
	XMLDocument doc;
	XMLError err = doc.LoadFile(filename.c_str());

	if (err != XML_SUCCESS)
	{
		fprintf(stderr, "Failed to load scenario: %s\n", doc.ErrorStr());
		return nullptr;
	}

	auto scenario = make_shared<Scenario>();

	/* Parse the XML file */
	XMLElement *root = doc.RootElement();
	if (!root || strcmp(root->Name(), "scenario") != 0)
	{
		fprintf(stderr, "No root element or root element not \"scenario\".");
		return nullptr;
	}

	const char* scenario_name = root->Attribute("name");

	if (!scenario_name)
	{
		fprintf(stderr, "The root element \"scenario\" has no \"name\" attribute.");
		return nullptr;
	}

	scenario->name = scenario_name;

	XMLElement *ce = root->FirstChildElement();

	while (ce)
	{
		const char *n = ce->Name();

		if (strcmp(n, "universe") == 0)
		{
			if (!read_universe(scenario, ce))
				return nullptr;
		}
		else if (strcmp(n, "installed") == 0)
		{
			if (!read_installed(scenario, ce))
				return nullptr;
		}
		else if (strcmp(n, "selected") == 0)
		{
			if (!read_selected(scenario, ce))
				return nullptr;
		}
		else if (strcmp(n, "desired") == 0)
		{
			if (!read_desired(scenario, ce))
				return nullptr;
		}
		else
		{
			fprintf(stderr, "Invalied element \"%s\" in scenario on line %d.\n",
					n, ce->GetLineNum());

			return nullptr;
		}

		if (ce != root->LastChildElement())
			ce = ce->NextSiblingElement();
		else
			ce = nullptr;
	}

	return scenario;
}
