#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <tinyxml2.h>
#include "parameters.h"
#include "utility.h"

using namespace std;
using namespace tinyxml2;
namespace fs = std::filesystem;


RepositorySpecification::RepositorySpecification (char type, const string& param1)
	: type(type), param1(param1)
{
}


bool RepositorySpecification::operator==(const RepositorySpecification& o) const
{
	return type == o.type && param1 == o.param1;
}


bool Parameters::target_is_native() const
{
	return target == "/";
}


void Parameters::read_from_env()
{
	const char* v = secure_getenv ("TPM_TARGET");

	if (v)
		target = get_absolute_path(v);
}


bool read_config_file (shared_ptr<Parameters> params)
{
	XMLDocument doc;

	fs::path cfile(params->target + "/" + TPM2_CONFIG_FILE);

	if (fs::is_regular_file (cfile))
	{
		auto err = doc.LoadFile (cfile.c_str());

		if (err != XML_SUCCESS)
		{
			fprintf (stderr, "Failed to read config file: %s\n", doc.ErrorStr());
			return false;
		}
	}
	else
	{
		fprintf (stderr, "Config file %s not found.\n", cfile.c_str());
		return false;
	}


	XMLElement *root = doc.RootElement();
	if (!root || strcmp (root->Name(), "tpm") != 0)
	{
		fprintf (stderr, "Config file has not root element or it is not tpm.\n");
		return false;
	}

	const char *file_version = root->Attribute ("file_version");
	if (!file_version)
	{
		fprintf (stderr, "The config file has no version.\n");
		return false;
	}
	
	if (strcmp (file_version, "2.0") != 0)
	{
		fprintf (stderr, "The config file has unsupported file version \"%s\"\n.",
				file_version);

		return false;
	}

	XMLElement *ce = root->FirstChildElement();
	for (;;)
	{
		const char *n = ce->Name();

		if (strcmp (n, "default_arch") == 0)
		{
			if (params->default_architecture == Architecture::invalid)
			{
				const char *tmp = ce->GetText();

				if (tmp)
				{
					try
					{
						params->default_architecture =
							Architecture::from_string (tmp);
					}
					catch (InvalidArchitecture&)
					{
						params->default_architecture = Architecture::invalid;
					}
				}

				if (!tmp || params->default_architecture == Architecture::invalid)
				{
					fprintf (stderr,
							"Invalid default architecture specified in config file on line: %d\n",
							ce->GetLineNum());

					return false;
				}
			}
			else
			{
				fprintf (stderr,
						"Multiple default architectures specified in config file, last on line %d.\n",
						ce->GetLineNum());

				return false;
			}
		}
		else if (strcmp (n, "repo") == 0)
		{
			const char *tmp = ce->Attribute ("type");
			if (!tmp)
			{
				fprintf (stderr, "Repo in config file line %d lacks type.\n",
						ce->GetLineNum());

				return false;
			}

			if (strcmp (tmp, "dir") == 0)
			{
				const char *tmp2 = ce->GetText();

				if (!tmp2)
				{
					fprintf (stderr, "Directory repo in config file on line %d has not location.\n",
							ce->GetLineNum());

					return false;
				}

				RepositorySpecification r(RepositorySpecification::TYPE_DIR, tmp2);

				if (find (params->repos.cbegin(), params->repos.cend(), r) != params->repos.cend())
				{
					fprintf (stderr,
							"Directory repo \"%s\" specified multiple times in "
							"config file, last on line %d.\n",
							tmp2, ce->GetLineNum());

					return false;
				}

				params->repos.emplace_back (r);
			}
			else
			{
				fprintf (stderr, "Invalid repo type in config file on line %d: %s\n",
						ce->GetLineNum(), tmp);

				return false;
			}
		}
		else
		{
			fprintf (stderr,
					"Invalid element tag in config file on line %d: \"%s\"\n",
					ce->GetLineNum(), n);

			return false;
		}

		/* Next element */
		if (ce == root->LastChildElement())
			break;
		else
			ce = ce->NextSiblingElement();
	}


	/* Check that a default architecture has been set */
	if (params->default_architecture == Architecture::invalid)
	{
		fprintf (stderr, "No default architecture set.\n");
		return false;
	}

	return true;
}
