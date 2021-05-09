/** This file is part of the TSClient LEGACY Package Manager. */
#include <cmath>
#include "run_scenario.h"
#include "architecture.h"

using namespace std;
using namespace depres;

namespace pc = PackageConstraints;


/* Trimming a string. See e.g. https://stackoverflow.com/a/1798170 */
string trim(const string& s)
{
	auto start = s.find_first_not_of(" \t");
	auto stop = s.find_last_not_of(" \t");

	if (start == string::npos)
		return "";

	return s.substr(start, stop - start + 1);
}


PackageVersionAdaptor::PackageVersionAdaptor(std::shared_ptr<Scenario::Package> scenario_package)
	: PackageVersion(
			scenario_package->name, scenario_package->arch,
			scenario_package->sv, scenario_package->bv),
		scenario_package(scenario_package)
{
}

vector<pair<pair<string, int>, shared_ptr<pc::Formula>>> PackageVersionAdaptor::get_dependencies()
{
	vector<pair<pair<string, int>, shared_ptr<pc::Formula>>> deps;

	for (auto& scenario_dep : scenario_package->deps)
	{
		auto [name, arch, vc] = scenario_dep;
		deps.push_back(make_pair(make_pair(name, arch), vc));
	}

	return deps;
}

vector<pair<pair<string, int>, shared_ptr<pc::Formula>>> PackageVersionAdaptor::get_pre_dependencies()
{
	return vector<pair<pair<string, int>, shared_ptr<pc::Formula>>>();
}

const vector<string> &PackageVersionAdaptor::get_files()
{
	return scenario_package->files;
}

const vector<string> &PackageVersionAdaptor::get_directories()
{
	return scenario_package->directories;
}


/** Implementation of the ScenarioRunner class */
vector<VersionNumber> ScenarioRunner::list_package_versions (const string &name, int arch)
{
	vector<VersionNumber> versions;

	for (auto pkg : scenario->universe)
	{
		if (pkg->name == name && pkg->arch == arch)
			versions.push_back (pkg->bv);
	}

	return versions;
}

shared_ptr<PackageVersion> ScenarioRunner::get_package_version (
		const string &name, int arch, const VersionNumber &version)
{
	for (auto pkg : adapted_universe)
	{
		if (pkg->get_name() == name && pkg->get_architecture() == arch &&
				pkg->get_binary_version() == version)
		{
			return pkg;
		}
	}

	return nullptr;
}

void ScenarioRunner::set_solver(shared_ptr<SolverInterface> solver)
{
	this->solver = solver;
}

void ScenarioRunner::set_scenario(shared_ptr<Scenario> scenario)
{
	this->scenario = scenario;

	/* Create adapted universe */
	adapted_universe.clear();

	for (auto pkg : scenario->universe)
		adapted_universe.push_back(make_shared<ProvidedPackageVersionAdaptor>(pkg));

	/* Create adapted installed package list */
	adapted_installed_packages.clear();

	for (auto [pkg, manual] : scenario->installed)
		adapted_installed_packages.push_back(
				{make_shared<InstalledPackageVersionAdaptor>(pkg), !manual});

	/* Create adapted selected package list */
	adapted_selected_packages.clear();

	for (const auto &[name, arch, formula] : scenario->selected)
		adapted_selected_packages.push_back(make_pair(make_pair(name, arch), formula));
}

void ScenarioRunner::run()
{
	/* Set parameters */
	solver->set_parameters(
			adapted_installed_packages,
			adapted_selected_packages,
			bind(&ScenarioRunner::list_package_versions, this,
				placeholders::_1, placeholders::_2),
			bind(&ScenarioRunner::get_package_version, this,
				placeholders::_1, placeholders::_2, placeholders::_3));

	/* Run */
	solver_succeeded = solver->solve();

	/* Retrieve result */
	result.clear();

	if (solver_succeeded)
	{
		auto solver_G = move(solver->get_G());

		for (auto &[key, v] : solver_G)
		{
			if (!v->chosen_version)
			{
				solver_succeeded = false;
				solver_errors.push_back("IGNode (" + v->identifier.first + ", " +
						Architecture::to_string(v->identifier.second) + ") has no chosen version.");

				break;
			}

			result.push_back(v->chosen_version);
		}
	}

	solver_errors = solver->get_errors();
}

pair<double, vector<string>> ScenarioRunner::evaluate_result()
{
	vector<string> reasons;

	/* If the solver failed but should have not or failed with the wrong error
	 * message, the deviation is -inf. */
	if (!solver_succeeded)
	{
		if (scenario->desired_errors.size())
		{
			reasons.push_back("Original solver errors:");
			for (auto &error : solver_errors)
				reasons.push_back("  " + error);

			bool ok = true;

			/* Try to find desired errors in the solver's output */
			auto i = scenario->desired_errors.begin();
			for (auto& error : solver_errors)
			{
				if (i == scenario->desired_errors.end())
					break;

				if (trim(error).rfind(*i, 0) == 0)
					i++;
			}

			if (i != scenario->desired_errors.end())
			{
				ok = false;

				reasons.push_back("The following error message prefixes were "
						"not found in the solver's output (or in the wrong order):");

				for(; i != scenario->desired_errors.end(); i++)
					reasons.push_back("  " + *i);
			}

			if (ok)
				reasons.push_back("Solver failed with desired error messages.");

			return make_pair(ok ? 0. : -INFINITY, reasons);
		}
		else
		{
			reasons.push_back ("Solver failed but it should have not.");
			for (auto &error : solver_errors)
				reasons.push_back(error);

			return make_pair(-INFINITY, reasons);
		}
	}

	/* If the solver succeeded but should have failed, the deviation is likewise
	 * -inf. */
	if (scenario->desired_errors.size())
	{
		reasons.push_back("Solver succeeded but should have failed.");
		for (auto &error : solver_errors)
			reasons.push_back(error);

		return make_pair(-INFINITY, reasons);
	}

	/* Every package that is either missing or would not be required
	 * ('additional packages') counts one. If any package is missing, the
	 * deviation is negative. */
	double deviation = 0;
	bool missing = false;

	/* Find missing packages */
	for (auto pkg : scenario->desired)
	{
		bool found = false;

		for (auto result_pkg : result)
		{
			if (result_pkg->get_name() == pkg->name &&
					result_pkg->get_architecture() == pkg->arch &&
					result_pkg->get_binary_version() == pkg->bv)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			deviation += 1;
			missing = true;
			reasons.push_back("missing:    " + pkg->name + "@" +
					Architecture::to_string(pkg->arch) + ":" +
					pkg->bv.to_string());
		}
	}

	/* Find additional packages */
	for (auto result_pkg : result)
	{
		bool found = false;

		for (auto pkg : scenario->desired)
		{
			if (result_pkg->get_name() == pkg->name &&
					result_pkg->get_architecture() == pkg->arch &&
					result_pkg->get_binary_version() == pkg->bv)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			deviation += 1;
			reasons.push_back("additional: " + result_pkg->get_name() + "@" +
					Architecture::to_string(result_pkg->get_architecture()) + ":" +
					result_pkg->get_binary_version().to_string());
		}
	}

	if (missing)
		deviation *= -1;

	if (solver_errors.size())
	{
		reasons.push_back("However there are solver errors:");
		for (auto& error : solver_errors)
			reasons.push_back("  " + error);
	}

	return make_pair(deviation, reasons);
}
