#include <cstring>
#include <stdexcept>
#include "package_meta_data.h"
#include "managed_buffer.h"
#include "common_utilities.h"

using namespace std;
using namespace tinyxml2;
namespace pc = PackageConstraints;


PackageMetaData::PackageMetaData(const string &name, const int architecture,
		const VersionNumber &version, const VersionNumber &source_version,
		char installation_reason, int state)
	:
		name(name), architecture(architecture),
		version(version), source_version(source_version),
		installation_reason(installation_reason), state(state)
{
}


void PackageMetaData::add_pre_dependency(const Dependency &d)
{
	pre_dependencies.dependencies.insert(d);
}


void PackageMetaData::add_dependency(const Dependency &d)
{
	dependencies.dependencies.insert(d);
}


unique_ptr<XMLDocument> PackageMetaData::to_xml() const
{
	auto doc = make_unique<XMLDocument>();

	/* Root node */
	XMLElement *root = doc->NewElement ("pkg");
	root->SetAttribute ("file_version", "2.0");

	doc->InsertFirstChild (root);

	/* Name, architecture, version and source_version */
	XMLElement *e = doc->NewElement ("name");
	e->SetText (name.c_str());

	XMLNode *ln = root->InsertFirstChild (e);

	e = doc->NewElement ("arch");
	e->SetText (Architecture::to_string (architecture).c_str());
	ln = root->InsertAfterChild (ln, e);

	e = doc->NewElement ("version");
	e->SetText (version.to_string().c_str());
	ln = root->InsertAfterChild (ln, e);

	e = doc->NewElement ("source_version");
	e->SetText (source_version.to_string().c_str());
	ln = root->InsertAfterChild (ln, e);


	/* Pre-dependencies */
	XMLElement *deps = doc->NewElement ("pre-dependencies");
	ln = root->InsertAfterChild (ln, deps);

	XMLNode *ln2 = nullptr;

	for (auto i = pre_dependencies.cbegin(); i != pre_dependencies.cend(); i++)
	{
		const Dependency& d = *i;

		XMLElement *dep = doc->NewElement ("dep");
		if (!ln2)
			ln2 = deps->InsertFirstChild (dep);
		else
			ln2 = deps->InsertAfterChild (ln2, dep);

		/* Name, architecture */
		XMLElement *e3 = doc->NewElement ("name");
		e3->SetText (d.get_name().c_str());
		XMLNode *ln3 = dep->InsertFirstChild (e3);

		e3 = doc->NewElement ("arch");
		e3->SetText (Architecture::to_string (d.get_architecture()).c_str());
		ln3 = dep->InsertAfterChild (ln3, e3);

		/* Version formula */
		if (d.version_formula)
			d.version_formula->to_xml (doc.get(), dep);
	}


	/* Dependencies */
	deps = doc->NewElement ("dependencies");
	ln = root->InsertAfterChild (ln, deps);

	ln2 = nullptr;

	for (auto i = dependencies.cbegin(); i != dependencies.cend(); i++)
	{
		const Dependency& d = *i;

		XMLElement *dep = doc->NewElement ("dep");
		if (!ln2)
			ln2 = deps->InsertFirstChild (dep);
		else
			ln2 = deps->InsertAfterChild (ln2, dep);

		/* Name, architecture */
		XMLElement *e3 = doc->NewElement ("name");
		e3->SetText (d.get_name().c_str());
		XMLNode *ln3 = dep->InsertFirstChild (e3);

		e3 = doc->NewElement ("arch");
		e3->SetText (Architecture::to_string (d.get_architecture()).c_str());
		ln3 = dep->InsertAfterChild (ln3, e3);

		/* Version formula */
		if (d.version_formula)
			d.version_formula->to_xml (doc.get(), dep);
	}


	/* Triggers */
	XMLElement *triggers = doc->NewElement ("triggers");
	ln = root->InsertAfterChild (ln, triggers);

	if (!interested_triggers || !activated_triggers)
	{
		throw gp_exception ("PackageMetaData::to_xml called without both "
				"metadata lists being present.");
	}

	ln2 = nullptr;
	for (const auto& t : *interested_triggers)
	{
		XMLElement *trigger = doc->NewElement ("interested");
		trigger->SetText (t.c_str());

		if (!ln2)
			ln2 = triggers->InsertFirstChild (trigger);
		else
			ln2 = triggers->InsertAfterChild (ln2, trigger);
	}

	for (const auto& t : *activated_triggers)
	{
		XMLElement *trigger = doc->NewElement ("activate");
		trigger->SetText (t.c_str());

		if (!ln2)
			ln2 = triggers->InsertFirstChild (trigger);
		else
			ln2 = triggers->InsertAfterChild (ln2, trigger);
	}

	return doc;
}


shared_ptr<PackageMetaData> read_package_meta_data_from_xml (
		const char* buf, size_t size)
{
	XMLDocument doc;

	if (doc.Parse (buf, size) != XML_SUCCESS)
	{
		throw invalid_package_meta_data_xml ("Could not parse xml: " +
				string(doc.ErrorStr()) + ".");
	}

	/* Read the document */
	XMLElement *root = doc.RootElement();

	if (!root || strcmp (root->Name(), "pkg") != 0)
		throw invalid_package_meta_data_xml ("No root element or root element not \"pkg\".");

	const char* file_version = root->Attribute("file_version");

	if (!file_version)
		throw invalid_package_meta_data_xml (
				"The root \"pkg\" element does not have a \"file_version\" attribute.");

	if (strcmp (file_version, "2.0") != 0)
		throw invalid_package_meta_data_xml ("Unsupported file version " + string(file_version));


	XMLElement *ce = root->FirstChildElement();
	const char *name = nullptr;
	int architecture = Architecture::invalid;
	optional<VersionNumber> version;
	optional<VersionNumber> source_version;

	shared_ptr<PackageMetaData> mdata;

	while (ce)
	{
		const char *n = ce->Name();

		if (strcmp (n, "name") == 0)
		{
			if (!name)
			{
				name = ce->GetText();
				if (!name || strlen(name) == 0)
				{
					throw invalid_package_meta_data_xml (
							"Invalid name section in line " +
							to_string(ce->GetLineNum()));
				}
			}
			else
				throw invalid_package_meta_data_xml ("Duplicate name section");
		}
		else if (strcmp (n, "arch") == 0)
		{
			if (architecture == Architecture::invalid)
			{
				auto tmp = ce->GetText();

				if (tmp)
				{
					try
					{
						architecture = Architecture::from_string (tmp);
					}
					catch (InvalidArchitecture&)
					{
						architecture = Architecture::invalid;
					}
				}

				if (!tmp || architecture == Architecture::invalid)
				{
					throw invalid_package_meta_data_xml (
							"Invalid architecture section in line " +
							to_string(ce->GetLineNum()));
				}
			}
			else
				throw invalid_package_meta_data_xml ("Duplicate architecture section");
		}
		else if (strcmp (n, "version") == 0)
		{
			if (!version)
			{
				auto tmp = ce->GetText();
				string err;

				if (tmp)
				{
					try
					{
						version = VersionNumber (tmp);
					}
					catch (InvalidVersionNumberString& e)
					{
						err = e.what();
					}
				}

				if (!tmp || !version)
				{
					throw invalid_package_meta_data_xml (
							"Invalid version section in line " +
							to_string(ce->GetLineNum()) + ": " + err);
				}
			}
			else
				throw invalid_package_meta_data_xml ("Duplicate version section");
		}
		else if (strcmp (n, "source_version") == 0)
		{
			if (!source_version)
			{
				auto tmp = ce->GetText();
				string err;

				if (tmp)
				{
					try
					{
						source_version = VersionNumber (tmp);
					}
					catch (InvalidVersionNumberString& e)
					{
						err = e.what();
					}
				}

				if (!tmp || !source_version)
				{
					throw invalid_package_meta_data_xml (
							"Invalid source_version section in line " +
							to_string(ce->GetLineNum()) + ": " + err);
				}
			}
			else
				throw invalid_package_meta_data_xml ("Duplicate source_version section");
		}
		else if (strcmp (n, "pre-dependencies") == 0 || strcmp (n, "dependencies") == 0)
		{
			if (!mdata)
				throw invalid_package_meta_data_xml ("dependency section before first four attributes");

			bool predeps = n[0] == 'p';

			/* Parse subsection */
			XMLElement *cdep = ce->FirstChildElement();

			while (cdep)
			{
				/* Each <dep> entry looks like this:
				 *   <name>...</name>
				 *   <arch>...</arch>
				 *   <constr type="{eq|neq|geq|leq|gt|lt}">...</constr>
				 *   <sconstr type="{eq|neq|geq|leq|gt|lt}">...</sconstr>
				 *     \vdots */
				const char *dep_name = nullptr;
				int dep_arch = Architecture::invalid;
				std::shared_ptr<pc::Formula> formula = make_shared<pc::And>(nullptr, nullptr);

				const char *n2 = cdep->Name();

				if (strcmp (n2, "dep") == 0)
				{
					XMLElement *cde = cdep->FirstChildElement();

					while (cde)
					{
						const char *n3 = cde->Name();


						if (strcmp (n3, "name") == 0)
						{
							if (!dep_name)
							{
								dep_name = cde->GetText();
								if (!dep_name || strlen (dep_name) == 0)
								{
									throw invalid_package_meta_data_xml (
											"Invalid dependency name in line " +
											to_string (cde->GetLineNum()));
								}
							}
							else
							{
								throw invalid_package_meta_data_xml (
										"Dependency \"" + string(dep_name) +
										"\" has multiple names, see line " +
										to_string(cde->GetLineNum()));
							}
						}
						else if (strcmp (n3, "arch") == 0)
						{
							if (dep_arch == Architecture::invalid)
							{
								auto tmp = cde->GetText();

								if (tmp)
								{
									try
									{
										dep_arch = Architecture::from_string (tmp);
									}
									catch (InvalidArchitecture&)
									{
										dep_arch = Architecture::invalid;
									}
								}

								if (!tmp || dep_arch == Architecture::invalid)
								{
									throw invalid_package_meta_data_xml (
											"Invalid dependency architecture in line " +
											to_string (cde->GetLineNum()));
								}
							}
							else
							{
								throw invalid_package_meta_data_xml (
										"A dependency has multiple architectures in line " +
										to_string (cde->GetLineNum()));
							}
						}
						else if (strcmp (n3, "constr") == 0 || strcmp (n3, "sconstr") == 0)
						{
							bool source = n3[0] == 's';

							const char *stype = cde->Attribute("type");

							if (!stype)
							{
								throw invalid_package_meta_data_xml (
										"No dependency constraint type specified in line " +
										to_string (cde->GetLineNum()));
							}

							/* Decode type */
							char type;

							if (strcmp (stype, "eq") == 0)
							{
								type = pc::PrimitivePredicate::TYPE_EQ;
							}
							else if (strcmp (stype, "neq") == 0)
							{
								type = pc::PrimitivePredicate::TYPE_NEQ;
							}
							else if (strcmp (stype, "geq") == 0)
							{
								type = pc::PrimitivePredicate::TYPE_GEQ;
							}
							else if (strcmp (stype, "leq") == 0)
							{
								type = pc::PrimitivePredicate::TYPE_LEQ;
							}
							else if (strcmp (stype, "gt") == 0)
							{
								type = pc::PrimitivePredicate::TYPE_GT;
							}
							else if (strcmp (stype, "lt") == 0)
							{
								type = pc::PrimitivePredicate::TYPE_LT;
							}
							else
							{
								throw invalid_package_meta_data_xml (
										"Invalid version constraint type in line " +
										to_string (cde->GetLineNum()));
							}

							/* Decode version number */
							auto tmp = cde->GetText();
							const char *err = nullptr;

							try
							{
								VersionNumber v(tmp);

								/* Add predicate */
								formula = make_shared<pc::And> (
										formula,
										make_shared<pc::PrimitivePredicate> (source, type, move(v)));
							}
							catch (InvalidVersionNumberString& e)
							{
								err = e.what();
							}

							if (!tmp || err)
							{
								throw invalid_package_meta_data_xml (
										"Invalid version constrint version number in line " +
										to_string (cde->GetLineNum()) + ": " +
										(err ? err : ""));
							}
						}
						else
						{
							throw invalid_package_meta_data_xml (
									"A dependency has an invalid attribute \"" +
									string(n3) + "\" in line " + to_string (cde->GetLineNum()));
						}


						/* Next dependency parameter */
						if (cde != cdep->LastChildElement())
						{
							cde = cde->NextSiblingElement();
						}
						else
						{
							cde = nullptr;

							/* This was the last attribute of this dependency.
							 * Check if it does not exist in the dependency list
							 * already and add it. */
							Dependency d (string(dep_name), dep_arch, formula);

							DependencyList *dl = predeps ? &(mdata->pre_dependencies) : &(mdata->dependencies);

							if (dl->dependencies.find (d) != dl->dependencies.end())
							{
								throw invalid_package_meta_data_xml (
										"Duplicate dependency specification for \"" +
										string(dep_name) + "@" + Architecture::to_string(dep_arch) + "\"");
							}

							dl->dependencies.insert (move(d));
						}
					}
				}
				else
				{
					throw invalid_package_meta_data_xml (
							"Invalid section \"" + string(n2) + "\" in section "
							"dependencies or pre-dependencies on line " + to_string(cdep->GetLineNum()));
				}

				/* Next dependency */
				if (cdep != ce->LastChildElement())
					cdep = cdep->NextSiblingElement();
				else
					cdep = nullptr;
			}
		}
		else if (strcmp (n, "triggers") == 0)
		{
			if (!mdata)
				throw invalid_package_meta_data_xml ("triggers section before first four attributes");

			/* Parse subsection */
			XMLElement *ctrg = ce->FirstChildElement();

			while (ctrg)
			{
				const char *n2 = ctrg->Name();

				bool interested = false;
				if (strcmp (n2, "interested") == 0)
				{
					interested = true;
				}
				else if (strcmp (n2, "activate") != 0)
				{
					throw invalid_package_meta_data_xml (
							"Invalid section \"" + string(n2) + "\" in section "
							"triggers on  line " + to_string (ctrg->GetLineNum()));
				}

				const char *trg_name = ctrg->GetText();
				if (!trg_name || strlen (trg_name) == 0)
				{
					throw invalid_package_meta_data_xml (
							"Invalid trigger name on line " +
							to_string (ctrg->GetLineNum()));
				}

				if (interested)
					mdata->interested_triggers->emplace_back (trg_name);
				else
					mdata->activated_triggers->emplace_back (trg_name);

				/* Next trigger reference */
				if (ctrg != ce->LastChildElement())
					ctrg = ctrg->NextSiblingElement();
				else
					ctrg = nullptr;
			}
		}
		else
		{
			throw invalid_package_meta_data_xml ("Unknown section \"" + string(n) +
					"\" in line " + to_string(ce->GetLineNum()));
		}


		/* See if you can construct the metadata object already. */
		if (!mdata && name && architecture != Architecture::invalid && version && source_version)
		{
			mdata = make_shared<PackageMetaData> (string(name), architecture,
					version.value(), source_version.value(),
					INSTALLATION_REASON_INVALID, PKG_STATE_INVALID);

			/* The triggers will be read (or determined that the package
			 * references none). */
			mdata->interested_triggers.emplace();
			mdata->activated_triggers.emplace();
		}

		/* Next child */
		if (ce != root->LastChildElement())
			ce = ce->NextSiblingElement();
		else
			ce = nullptr;
	}

	return mdata;
}


/********************************** Exceptions ********************************/
invalid_package_meta_data_xml::invalid_package_meta_data_xml (const std::string& msg)
	: msg(msg)
{
}


const char* invalid_package_meta_data_xml::what() const noexcept
{
	return msg.c_str();
}
